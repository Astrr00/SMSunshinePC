// Windows host services for the AI/VI port layer (see av.hpp): high-resolution sleep, memory probing and the audio
// output device. Deliberately includes no decomp header (<windows.h> clashes with them).
#include "av.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>

#pragma comment(lib, "ole32.lib")

namespace av {

void SleepUntil(std::chrono::steady_clock::time_point t)
{
	using namespace std::chrono;
	// One timer per calling thread, never closed (the threads live as long as the process).
	thread_local HANDLE timer = [] {
		HANDLE h = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		// ponytail: before Windows 10 1803 there is no high-resolution timer; the plain one wakes with the system
		// tick (~15 ms jitter). Fix by raising the tick (timeBeginPeriod) if older Windows ever matters.
		return h ? h : CreateWaitableTimerW(nullptr, FALSE, nullptr);
	}();

	const auto remaining = t - steady_clock::now();
	if (remaining <= steady_clock::duration::zero())
		return;
	LARGE_INTEGER due; // negative = relative, in 100 ns units
	due.QuadPart = -(duration_cast<nanoseconds>(remaining).count() / 100);
	if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
		WaitForSingleObject(timer, INFINITE);
	else
		std::this_thread::sleep_until(t);
}

void BoostThreadPriority() { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST); }

bool IsReadable(const void* p, size_t bytes)
{
	const char* cur       = static_cast<const char*>(p);
	const char* const end = cur + bytes;
	while (cur < end) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery(cur, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT
		    || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
			return false;
		cur = static_cast<const char*>(mbi.BaseAddress) + mbi.RegionSize;
	}
	return true;
}

namespace {

constexpr REFERENCE_TIME kDeviceBuffer = 2'000'000; // 200 ms, in 100 ns units: capacity only, the fill is limited below

enum OpenState { kIdle = 0, kOpening, kDone, kFailed };

struct Device {
	IAudioClient* client       = nullptr;
	IAudioRenderClient* render = nullptr;
	std::atomic<UINT32> hz { 0 };   // 0 = closed; the opening thread sets it last
	std::atomic<int> open { kIdle }; // state of the helper thread that opens the stream
	std::chrono::steady_clock::time_point retryAt {};
	bool warned = false;
};
Device g_dev;

// Blocks that never reached the device, counted for the exit summary. Written by the pump thread only.
struct Stats {
	uint64_t notUp     = 0; // dropped while the device was closed, opening, or waiting for its retry
	uint64_t underruns = 0; // device ran dry: 40 ms of silence queued before the block
	uint64_t overflows = 0; // more than 100 ms queued: block dropped
	uint64_t fails     = 0; // device lost or write failed (see Fail)
} g_stats;

void summaryAtExit()
{
	std::fprintf(stderr,
	             "[av] audio: %llu blocks dropped while the device was not up, %llu underruns, %llu overflow drops, "
	             "%llu device failures\n",
	             static_cast<unsigned long long>(g_stats.notUp), static_cast<unsigned long long>(g_stats.underruns),
	             static_cast<unsigned long long>(g_stats.overflows), static_cast<unsigned long long>(g_stats.fails));
}

template <class T> void Release(T*& p)
{
	if (p) {
		p->Release();
		p = nullptr;
	}
}

void Close()
{
	if (g_dev.client)
		g_dev.client->Stop();
	Release(g_dev.render);
	Release(g_dev.client);
	g_dev.hz = 0;
}

// Queues `frames` frames of silence.
bool Silence(UINT32 frames)
{
	BYTE* dst;
	return SUCCEEDED(g_dev.render->GetBuffer(frames, &dst))
	       && SUCCEEDED(g_dev.render->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT));
}

// Shared-mode stream at `hz`, 16-bit stereo. AUTOCONVERTPCM lets Windows resample to whatever the endpoint mixes at.
bool Open(UINT32 hz)
{
	// S_FALSE and RPC_E_CHANGED_MODE both leave COM usable on this thread.
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	WAVEFORMATEX fmt    = {};
	fmt.wFormatTag      = WAVE_FORMAT_PCM;
	fmt.nChannels       = 2;
	fmt.nSamplesPerSec  = hz;
	fmt.wBitsPerSample  = 16;
	fmt.nBlockAlign     = 4;
	fmt.nAvgBytesPerSec = hz * 4;

	IMMDeviceEnumerator* enumerator = nullptr;
	IMMDevice* device               = nullptr;
	const bool ok
	    = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
	                                 __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))
	      && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))
	      && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
	                                    reinterpret_cast<void**>(&g_dev.client)))
	      && SUCCEEDED(g_dev.client->Initialize(
	          AUDCLNT_SHAREMODE_SHARED,
	          AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, kDeviceBuffer, 0, &fmt,
	          nullptr))
	      && SUCCEEDED(g_dev.client->GetService(__uuidof(IAudioRenderClient),
	                                            reinterpret_cast<void**>(&g_dev.render)))
	      && Silence(hz / 25) // 40 ms of lead, so the device does not run dry between two pushes
	      && SUCCEEDED(g_dev.client->Start());
	Release(device);
	Release(enumerator);
	if (!ok)
		Close();
	else
		g_dev.hz = hz;
	return ok;
}

// Device missing or lost: close it and keep quiet for a second before the next attempt.
void Fail(std::chrono::steady_clock::time_point now, const char* what)
{
	++g_stats.fails;
	Close();
	g_dev.retryAt = now + std::chrono::seconds(1);
	if (!g_dev.warned) {
		g_dev.warned = true;
		std::fprintf(stderr, "[av] audio output unavailable (%s); retrying every second, audio is dropped meanwhile\n",
		             what);
	}
}

// Opens the stream on a helper thread. Inside the game process the first open has been seen to sit in the audio
// service's RPC for many seconds (standalone it takes ~20 ms); the AI pump must keep its DMA cadence regardless, so it
// drops audio until the open returns. Logs once when the first open completes.
void OpenAsync(UINT32 hz)
{
	g_dev.open = kOpening;
	std::thread([hz] {
		const auto t0 = std::chrono::steady_clock::now();
		const bool ok = Open(hz);
		static bool reported;
		if (!reported) {
			reported = true;
			std::fprintf(stderr, "[av] audio device open %s after %lld ms\n", ok ? "completed" : "failed",
			             static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
			                                        std::chrono::steady_clock::now() - t0)
			                                        .count()));
		}
		g_dev.open = ok ? kDone : kFailed;
	}).detach();
}

} // namespace

void AudioWrite(const int16_t* stereo, uint32_t frames, double hz)
{
	static const bool summaryRegistered = (std::atexit(summaryAtExit), true);
	(void)summaryRegistered;
	const UINT32 rate = static_cast<UINT32>(hz + 0.5);
	const auto now    = std::chrono::steady_clock::now();

	if (g_dev.open == kDone) { // the helper thread finished: hz is set when it succeeded
		g_dev.open   = kIdle;
		g_dev.warned = false;
	}
	static thread_local const bool comReady = [] { // the pump uses the MTA objects the helper created
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		return true;
	}();
	(void)comReady;
	if (g_dev.hz != rate) { // first call, rate change, or the device was lost
		++g_stats.notUp;
		const int state = g_dev.open;
		if (state == kOpening) // the open can block for seconds inside the audio service; never stall the pump on it
			return;
		if (state == kFailed) {
			g_dev.open = kIdle;
			Fail(now, "open failed");
			return;
		}
		Close();
		if (now < g_dev.retryAt)
			return;
		OpenAsync(rate);
		return; // audio is dropped until the device is up
	}

	UINT32 padding;
	if (FAILED(g_dev.client->GetCurrentPadding(&padding))) {
		Fail(now, "GetCurrentPadding failed");
		return;
	}
	if (padding == 0)
		g_stats.underruns++;
	// Device ran dry: restore the lead before queueing more.
	if (padding == 0 && !Silence(rate / 25)) {
		Fail(now, "write failed");
		return;
	}
	// ponytail: the producer (wall clock) and the device clock drift apart by ~100 ppm. Over 100 ms queued we drop a
	// block (and on underrun re-add lead above); both are a short click every few minutes. Upgrade: resample slightly.
	if (padding + frames > rate / 10) {
		g_stats.overflows++;
		return;
	}

	BYTE* dst;
	if (FAILED(g_dev.render->GetBuffer(frames, &dst))) {
		Fail(now, "write failed");
		return;
	}
	std::memcpy(dst, stereo, static_cast<size_t>(frames) * 4);
	g_dev.render->ReleaseBuffer(frames, 0);
}

} // namespace av
