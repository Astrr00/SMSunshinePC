#include <vector>
// Port-side AI (audio interface), VI (video interface) and DSP mailbox for the decomp's SDK API.
//
// Hardware events that the original SDK receives as interrupts (AI DMA block done, VI vertical retrace) come from two
// host threads here. Both run their callbacks under OSDisableInterrupts(), the port's global interrupt lock, exactly as
// an interrupt handler would, and wake game threads through OSWakeupThread.
//
//  * AI: a pump thread paces the DMA at the GameCube DAC rate, hands every block the game queued with AIInitDMA to the
//    Windows audio device (av_win.cpp) and raises the "DMA block done" callback. The audio the game produces (JAudio's
//    vframeWork mixing into its three DAC buffers) therefore reaches the speakers; the DSP that produces the mix is a
//    separate component (JSystem/dsp*.c, not part of this file).
//  * VI: a retrace thread counts retraces; see WaitForNextRetrace() for the clock.
#include "av.hpp"

#include <dolphin/ai.h>
#include <dolphin/ar.h>
#include <dolphin/dsp.h>
#include <dolphin/os.h>
#include <dolphin/vi.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

// Set at process exit so the host threads stop touching the OS layer while its statics are destroyed.
// ponytail: not joined; a game callback stuck at exit would otherwise hang the process. Join if teardown order bites.
std::atomic<bool> g_stopping { false };

void StartHostThread(void (*body)())
{
	static std::once_flag hook;
	std::call_once(hook, [] { std::atexit([] { g_stopping = true; }); });
	std::thread(body).detach();
}

// An interrupt handler on the console runs with interrupts masked; the port's equivalent is the global interrupt lock.
struct InterruptLock {
	BOOL enabled = OSDisableInterrupts();
	~InterruptLock() { OSRestoreInterrupts(enabled); }
};

// ---------------------------------------------------------------------------------------------------------------------
// AI
// ---------------------------------------------------------------------------------------------------------------------

// Everything below is guarded by the interrupt lock. Mirrors the registers the original SDK programs.
struct {
	AIDCallback dmaCallback = nullptr;
	u32 dmaAddr             = 0; // see ResolveDmaAddress
	u32 dmaLength           = 0; // bytes; 0 = nothing programmed
	u32 dspRate             = AI_SAMPLERATE_48KHZ; // register reset value; AIInit selects 32 kHz
	// Disc audio stream (the drive's ADPCM stream mixed in by the AI). There is no stream source on PC (Aurora's DVD
	// layer rejects audio streaming), so these are recorded for the pump to apply once one exists, and the sample
	// counter never advances.
	u32 streamPlay   = AI_STREAM_STOP;
	u32 streamRate   = AI_SAMPLERATE_48KHZ;
	u8 streamVolLeft = 0, streamVolRight = 0;
	u32 streamSamples = 0;
	bool initialized  = false;
} ai;

// The AI DAC clocks (the console's 48.043 kHz crystal-derived rate, not exactly 48000). 32 kHz mode is 2/3 of it;
// JAudio's own constant for it is gDacRate = 32028.5.
constexpr double kHz32 = 32028.5;
constexpr double kHz48 = 48042.75;

// The decomp hands the DMA buffer to AIInitDMA as a 32-bit value (`AIInitDMA(u32(dac[2]), ...)` in JASAiCtrl.cpp), which
// drops the upper half of a 64-bit host pointer. Recover it as the address with these low 32 bits that lies nearest to
// the OS arena, which holds the game's heap (a 24 MB-class block, so well within +-2 GB of any pointer in it).
// The real fix is passing a uintptr_t (Aurora's own AIInitDMA prototype does); then this function goes away.
const s16* ResolveDmaAddress(u32 low)
{
	const uintptr_t arena = reinterpret_cast<uintptr_t>(OSGetArenaLo());
	if (arena == 0)
		return reinterpret_cast<const s16*>(static_cast<uintptr_t>(low));
	const auto delta = static_cast<int32_t>(low - static_cast<u32>(arena)); // wraps modulo 2^32, signed distance
	return reinterpret_cast<const s16*>(arena + static_cast<intptr_t>(delta));
}

void PlayBlock(u32 addr, u32 bytes, double hz)
{
	static bool warned = false;
	const s16* src     = ResolveDmaAddress(addr);
	if (!av::IsReadable(src, bytes)) {
		if (!warned) {
			warned = true;
			std::fprintf(stderr, "[av] AIInitDMA buffer 0x%08lx (resolved %p, %lu bytes) is not readable memory; "
			                     "the block is dropped\n",
			             static_cast<unsigned long>(addr), static_cast<const void*>(src),
			             static_cast<unsigned long>(bytes));
		}
		return;
	}
	av::DumpWrite(src, bytes / 4, hz);
	// SUNSHINE_MUTE=1: the device gets silence (same timing, so the AI clock is unchanged); the dump above keeps the mix.
	static const bool mute = std::getenv("SUNSHINE_MUTE") != nullptr || std::getenv("SUNSHINE_DEV_MAX_SECONDS") != nullptr; // dev harness runs are always silent
	if (mute) {
		static std::vector<s16> silence;
		silence.assign(bytes / 2, 0);
		av::AudioWrite(silence.data(), bytes / 4, hz);
		return;
	}
	av::AudioWrite(src, bytes / 4, hz); // 16-bit stereo, 4 bytes per frame
}

void AiPump()
{
	using namespace std::chrono;
	av::BoostThreadPriority();

	bool playing = false; // false until the first block started: no block has ended yet, so no interrupt is due
	auto next    = Clock::now();
	while (!g_stopping) {
		u32 addr, bytes, rate;
		{
			InterruptLock lock;
			addr  = ai.dmaAddr;
			bytes = ai.dmaLength & ~3u;
			rate  = ai.dspRate;
		}
		if (bytes == 0) { // nothing programmed: the hardware has nothing to play and raises no interrupt
			next = Clock::now() + milliseconds(1);
			av::SleepUntil(next);
			continue;
		}

		// Block boundary. The hardware reloads its DMA registers first and raises the interrupt for the block that
		// just ended afterwards, so the block starting now is the one programmed *before* the callback; what the
		// callback programs plays one block later. JAudio triple-buffers on exactly that assumption.
		const double hz = rate == AI_SAMPLERATE_32KHZ ? kHz32 : kHz48;
		PlayBlock(addr, bytes, hz);
		if (playing) {
			InterruptLock lock;
			if (ai.dmaCallback)
				ai.dmaCallback();
		}
		playing = true;

		next += nanoseconds(std::llround(bytes / 4 / hz * 1e9));
		if (next + milliseconds(100) < Clock::now()) // stalled (debugger, suspend): resync, do not fire a callback burst
			next = Clock::now();
		av::SleepUntil(next);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// VI
// ---------------------------------------------------------------------------------------------------------------------

std::atomic<u32> g_retraceCount { 0 }; // changed only by the retrace thread, under the interrupt lock
OSThreadQueue g_retraceQueue;          // threads in VIWaitForRetrace
VIRetraceCallback g_postRetraceCb = nullptr; // interrupt lock
long long g_lastRetraceNs = 0; // steady-clock ns of the last retrace; interrupt lock
// SUNSHINE_DEV_STEP_RETRACE=1 (dev harness): the retrace fires only once a game thread waits for it (WaitForNextRetrace).
std::atomic<u32> g_waitFor { 0xFFFFFFFFu }; // retrace count the last VIWaitForRetrace caller waits on; interrupt lock

// Recorded for the presentation phase. Aurora has no XFB (its GXCopyDisp is empty); the frame reaches the window through
// aurora_end_frame(), which the port drives elsewhere, so nothing shows these on screen.
std::atomic<void*> g_nextFrameBuffer { nullptr };
std::atomic<BOOL> g_black { TRUE }; // the video output starts blanked

// ===== RETRACE SOURCE =====
// The ONLY place that decides when a vertical retrace happens. Blocks until the next one is due; called by the retrace
// thread alone. Today: wall clock at the NTSC field rate (60/1.001 = 59.94 Hz), which gives the game its 30 logic frames
// per second (TVideo::waitForRetrace(2)). To decouple rendering from game logic, replace the body, e.g. block on the
// renderer's present / the display's vsync instead.
void WaitForNextRetrace()
{
	static const bool stepped = std::getenv("SUNSHINE_DEV_STEP_RETRACE") != nullptr;
	if (stepped) { // dev harness: fire a retrace only once a game thread waits for it, so none is skipped
		while (g_waitFor.load() != g_retraceCount.load() && !g_stopping)
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		return;
	}
	using namespace std::chrono;
	constexpr nanoseconds kPeriod(16'683'333); // 1001 / 60000 s
	static auto next = Clock::now();
	next += kPeriod;
	if (next + milliseconds(100) < Clock::now()) // stalled: resync
		next = Clock::now();
	av::SleepUntil(next);
}

// What the retrace interrupt does (see __VIRetraceHandler in sms/src/dolphin/vi/vi.c): count, post-callback, wake waiters.
void RetraceInterrupt()
{
	InterruptLock lock;
	const u32 count = g_retraceCount.fetch_add(1) + 1;
	g_lastRetraceNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
	if (g_postRetraceCb)
		g_postRetraceCb(count);
	OSWakeupThread(&g_retraceQueue);
}

void RetraceThread()
{
	av::BoostThreadPriority();
	while (!g_stopping) {
		WaitForNextRetrace();
		RetraceInterrupt();
	}
}

// The retrace clock starts with the first VI call that needs it.
void EnsureRetraceThread()
{
	static std::once_flag once;
	std::call_once(once, [] {
		OSInitThreadQueue(&g_retraceQueue);
		StartHostThread(RetraceThread);
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// DSP mailbox
// ---------------------------------------------------------------------------------------------------------------------

// DSP -> CPU mailbox. Bit 31 is the hardware's "mail valid" flag: set when the DSP writes a mail, cleared when the CPU
// reads it. No DSP exists yet, so nothing posts; the mailbox is truthfully empty until av::DspPostMailFromDsp is called.
std::atomic<u32> g_dspMail { 0 };

} // namespace

namespace av {

void DspPostMailFromDsp(uint32_t mail) { g_dspMail.store(mail | 0x80000000u); }

} // namespace av

// =====================================================================================================================
// AI API (sms/include/dolphin/ai.h)
// =====================================================================================================================

void AIInit(u8* stack)
{
	// `stack` is the callback stack of the console's interrupt handler; callbacks run on the pump thread's own stack.
	(void)stack;
	InterruptLock lock;
	if (ai.initialized)
		return;
	ai.streamVolLeft = ai.streamVolRight = 0;
	ai.streamRate                        = AI_SAMPLERATE_48KHZ;
	ai.streamSamples                     = 0;
	ai.dspRate                           = AI_SAMPLERATE_32KHZ;
	ai.dmaCallback                       = nullptr;
	ai.initialized                       = true;
}

AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
	InterruptLock lock;
	AIDCallback old = ai.dmaCallback;
	ai.dmaCallback  = callback;
	return old;
}

void AIInitDMA(u32 start_addr, u32 length)
{
	InterruptLock lock;
	ai.dmaAddr   = start_addr;
	ai.dmaLength = length;
}

// Starts the pump (once). The DMA then runs for good: the game never stops it (AIStopDMA is not used).
void AIStartDMA(void)
{
	static std::once_flag once;
	std::call_once(once, [] { StartHostThread(AiPump); });
}

void AISetDSPSampleRate(u32 rate)
{
	InterruptLock lock;
	ai.dspRate = rate == AI_SAMPLERATE_32KHZ ? AI_SAMPLERATE_32KHZ : AI_SAMPLERATE_48KHZ;
}

u32 AIGetDSPSampleRate(void)
{
	InterruptLock lock;
	return ai.dspRate;
}

void AIResetStreamSampleCount(void)
{
	InterruptLock lock;
	ai.streamSamples = 0;
}

void AISetStreamPlayState(u32 state)
{
	InterruptLock lock;
	ai.streamPlay = state & 1;
}

// As on the console, only the 48 kHz disc stream rate is accepted; other values are ignored.
void AISetStreamSampleRate(u32 rate)
{
	InterruptLock lock;
	if (rate == AI_SAMPLERATE_48KHZ)
		ai.streamRate = rate;
}

void AISetStreamVolLeft(u8 vol)
{
	InterruptLock lock;
	ai.streamVolLeft = vol;
}

void AISetStreamVolRight(u8 vol)
{
	InterruptLock lock;
	ai.streamVolRight = vol;
}

// =====================================================================================================================
// ARAM base (sms/include/dolphin/ar.h). Aurora provides the rest of AR/ARQ; this one query it leaves out.
// =====================================================================================================================

// First ARAM address available to the application (the original SDK returns the same constant; Aurora's ARInit starts
// its allocator at 0x4000 too).
u32 ARGetBaseAddress(void) { return 0x4000; }

// =====================================================================================================================
// DSP mailbox API (sms/include/dolphin/dsp.h)
// =====================================================================================================================

u32 DSPCheckMailFromDSP(void) { return g_dspMail.load() >> 31; }

// Returns the mail including its valid bit, then clears the bit (reading the low half does that on the hardware).
u32 DSPReadMailFromDSP(void) { return g_dspMail.fetch_and(0x7FFFFFFFu); }

// =====================================================================================================================
// VI API (sms/include/dolphin/vi/vifuncs.h)
// =====================================================================================================================

void VIWaitForRetrace(void)
{
	EnsureRetraceThread();
	// Game threads must wait through the OS layer (not a host wait) so lower-priority game threads keep running.
	BOOL enabled    = OSDisableInterrupts();
	const u32 count = g_retraceCount;
	g_waitFor       = count;
	do {
		OSSleepThread(&g_retraceQueue);
	} while (count == g_retraceCount);
	OSRestoreInterrupts(enabled);
}

u32 VIGetRetraceCount(void)
{
	EnsureRetraceThread();
	return g_retraceCount;
}

// Steady-clock time (ns) of retrace number *count, read under the interrupt lock so count and time match.
// Used by the presentation pacer (src/framerate/); retraces come every 16'683'333 ns (see WaitForNextRetrace).
int64_t VIGetLastRetraceNs(u32* count)
{
	EnsureRetraceThread();
	BOOL enabled = OSDisableInterrupts();
	*count       = g_retraceCount;
	const int64_t t = g_lastRetraceNs;
	OSRestoreInterrupts(enabled);
	return t;
}

// Interlaced video alternates between the two fields on every retrace; this is the field the next retrace will show.
// (The console also folds in the display's Y offset parity, which the port's fixed 0 does not have.)
u32 VIGetNextField(void)
{
	EnsureRetraceThread();
	return (g_retraceCount + 1) & 1;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
	EnsureRetraceThread();
	InterruptLock lock;
	VIRetraceCallback old = g_postRetraceCb;
	g_postRetraceCb       = cb;
	return old;
}

// The game hands over an XFB pointer only when it flips buffers (never with a single XFB), and Aurora has no XFB.
// Frames are therefore presented once per game frame by TVideo::waitForRetrace (JDRVideo.cpp) through
// sunshinePresentFrame() in main.cpp, not here.
void VISetNextFrameBuffer(void* fb)
{
	g_nextFrameBuffer = fb;
}

void VISetBlack(BOOL black) { g_black = black; }

// Bit 0 of the DTV status register is the component-cable (progressive scan capable) sense line. A PC has no console
// video cable, so it is reported as absent: the game then never offers its progressive-scan prompt (GCLogoDir).
u32 VIGetDTVStatus(void) { return 0; }

// SUNSHINE_DEV_SEED=<n> (dev harness): fixed srand() seed so runs reproduce; unset keeps the boot-tick seed.
u32 sunshineRandSeed(u32 fallback)
{
	const char* seed = std::getenv("SUNSHINE_DEV_SEED");
	return seed != nullptr ? (u32)std::strtoul(seed, nullptr, 0) : fallback;
}
