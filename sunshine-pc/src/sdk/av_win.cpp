// Windows host services for the AI/VI port layer (see av.hpp): high-resolution sleep, memory probing and the audio
// output device. Deliberately includes no decomp header (<windows.h> clashes with them).
#include "av.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <cstdio>
#include <cstring>
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

struct Device {
	IAudioClient* client       = nullptr;
	IAudioRenderClient* render = nullptr;
	UINT32 hz                  = 0; // 0 = closed
	std::chrono::steady_clock::time_point retryAt {};
	bool comReady = false;
	bool warned   = false;
};
Device g_dev;

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
	if (!g_dev.comReady) {
		// S_FALSE and RPC_E_CHANGED_MODE both leave COM usable on this thread.
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		g_dev.comReady = true;
	}

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
	Close();
	g_dev.retryAt = now + std::chrono::seconds(1);
	if (!g_dev.warned) {
		g_dev.warned = true;
		std::fprintf(stderr, "[av] audio output unavailable (%s); retrying every second, audio is dropped meanwhile\n",
		             what);
	}
}

} // namespace

void AudioWrite(const int16_t* stereo, uint32_t frames, double hz)
{
	const UINT32 rate = static_cast<UINT32>(hz + 0.5);
	const auto now    = std::chrono::steady_clock::now();

	if (g_dev.hz != rate) { // first call, rate change, or the device was lost
		Close();
		if (now < g_dev.retryAt)
			return;
		if (!Open(rate)) {
			Fail(now, "open failed");
			return;
		}
		g_dev.warned = false;
	}

	UINT32 padding;
	if (FAILED(g_dev.client->GetCurrentPadding(&padding))) {
		Fail(now, "GetCurrentPadding failed");
		return;
	}
	// Device ran dry: restore the lead before queueing more.
	if (padding == 0 && !Silence(rate / 25)) {
		Fail(now, "write failed");
		return;
	}
	// ponytail: the producer (wall clock) and the device clock drift apart by ~100 ppm. Over 100 ms queued we drop a
	// block (and on underrun re-add lead above); both are a short click every few minutes. Upgrade: resample slightly.
	if (padding + frames > rate / 10)
		return;

	BYTE* dst;
	if (FAILED(g_dev.render->GetBuffer(frames, &dst))) {
		Fail(now, "write failed");
		return;
	}
	std::memcpy(dst, stereo, static_cast<size_t>(frames) * 4);
	g_dev.render->ReleaseBuffer(frames, 0);
}

} // namespace av
