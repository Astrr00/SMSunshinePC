// Presentation pacing (Port.md phase 5). The game logic keeps its 30 Hz NTSC schedule: TVideo::waitForRetrace(2) waits
// for two retraces per logic tick. With settings().fpsLimit above 30 (or unlimited), the interval between two logic ticks
// is filled with extra presents of the current game frame, re-drawn without advancing logic (sunshineReplayFrame,
// sms/src/System/MarDirectorDirect.cpp).
// ponytail: fixed slot grid from the cap, not aligned to vblank; ceiling: present jitter of the sleep granularity.
// Upgrade: align slots to the display's vblank when the renderer exposes it.
#include <dolphin/vi.h>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cstdio>

#include "../sdk/av.hpp"
#include "../settings.h"

void sunshinePresentFrame();      // src/main.cpp
void sunshineHeartbeat();         // src/crash.cpp: watchdog counts logic ticks
bool sunshineCanReplay();         // sms/src/System/MarDirectorDirect.cpp
void sunshineReplayFrame();       // sms/src/System/MarDirectorDirect.cpp
int64_t VIGetLastRetraceNs(u32* count); // src/sdk/av.cpp

namespace {
constexpr int kLogicRate = 30; // logic ticks per second (two retraces each)
constexpr float kLogicPeriod = 1.0f / kLogicRate;
using Clock              = std::chrono::steady_clock;

// Interpolation state (J3DModel::interpBlend): g_alpha runs 0..1 over the window's replays after a logic tick and keeps
// the last replay's value into the next direct()'s draw lists, until sunshineInterpTick resets it.
bool g_interp    = false;
float g_alpha    = 0.0f;
u32 g_step       = 0; // interpolation step: one per TMarDirector::direct() tick (sunshineInterpTick)
u32 g_logicTicks = 0; // one per waitForRetrace, in every director (logs: devscript, launcher)
u32 g_blends     = 0;
Clock::time_point g_tickStart;

// Replay cost (sunshinePaceWindow budget and log): decaying peaks in ns, plus per-second max, sum and the count of
// ticks whose window had no room for a slot.
int64_t g_presentPeakNs = 0, g_replayPeakNs = 0;
int64_t g_presentMaxNs = 0, g_replayMaxNs = 0, g_presentSumNs = 0, g_replaySumNs = 0;
int g_skips = 0;

void notePeak(int64_t& peak, int64_t& max, int64_t& sum, int64_t ns)
{
	peak = std::max(ns, peak - peak / 16);
	max  = std::max(max, ns);
	sum += ns;
}
}

// J3DModel draws blend the draw matrices of the last two logic ticks with sunshineInterpAlpha(); -1 = no blend.
// The blend position runs 0..1 over the replays after a tick, so the picture lags the logic by one tick (33 ms).
bool sunshineInterpEnabled()
{
	const int cap = sunshine::settings().fpsLimit;
	return cap == 0 || cap > kLogicRate;
}
float sunshineInterpAlpha() { return g_interp ? g_alpha : -1.0f; }
u32 sunshineInterpStep() { return g_step; }
u32 sunshineLogicTickCount() { return g_logicTicks; }
void sunshineInterpNoteBlend() { ++g_blends; }

// Called by TMarDirector::direct() (sms/src/System/MarDirectorDirect.cpp) once per tick, right before its entry stage:
// after the draw lists and the logic sub-steps (sunshineLogicTick runs once per tick, after the last sub-step's movement).
// The step opens there, not in sunshinePaceWindow: direct()'s entry stage records the new logic state as cur (prev = the
// last tick) and the window's replays blend that same pair. With the step opened in the window, the replays' first
// viewCalc moved cur (already this tick) into prev too, so every replay drew the logic state unblended.
// The draw lists at the start of the next direct() still run in the window's step with the last replay's alpha, so the
// models they view-calc themselves (mirror scene: TMirrorActor) draw the same blend as the replayed frame they show.
void sunshineInterpTick()
{
	++g_step;
	g_alpha = 0.0f; // the entry stage of this tick shows the previous logic state until a replay blends
}

// Called once per logic tick from TVideo::waitForRetrace(), before its own pre-wait (sms/src/JSystem/JDrama/JDRVideo.cpp).
// Presents the frame drawn by TMarDirector::direct(), then replays on the cap's slot grid until the caller's retrace
// (nextRetraceIndex) is due. The caller's own present and wait take over, so logic stays locked to the retrace count.
void sunshinePaceWindow(u32 nextRetraceIndex)
{
	static int ticks = 0;
	static int presents = 0;
	static auto logStart = Clock::now();

	sunshineHeartbeat();
	const auto now = Clock::now();
	++ticks;
	++g_logicTicks;
	if (now - logStart >= std::chrono::seconds(1)) {
		// The replay fields come last, so readers of the first four keep working (ms: avg/max per slot).
		const double n = presents > 0 ? presents : 1;
		std::printf("[framerate] logic %d/s, window presents %d/s, blended draws %u/s, fpsLimit %d, present ms %.1f/%.1f, "
		            "replay ms %.1f/%.1f, no-slot ticks %d\n",
		            ticks, presents, g_blends, sunshine::settings().fpsLimit, g_presentSumNs / n / 1e6,
		            g_presentMaxNs / 1e6, g_replaySumNs / n / 1e6, g_replayMaxNs / 1e6, g_skips);
		std::fflush(stdout);
		ticks = 0;
		g_blends = 0;
		presents = 0;
		g_presentMaxNs = g_replayMaxNs = g_presentSumNs = g_replaySumNs = 0;
		g_skips = 0;
		logStart = now;
	}

	const int cap = sunshine::settings().fpsLimit; // 0 = unlimited (no slot sleep)
	g_tickStart   = now;
	g_alpha       = 0.0f;
	g_interp      = (cap == 0 || cap > kLogicRate) && sunshineCanReplay();
	if (!g_interp)
		return;

	// Retraces are periodic: the caller's retrace is due at lastRetrace + (nextRetraceIndex - count) periods.
	u32 count          = 0;
	const int64_t last = VIGetLastRetraceNs(&count);
	// Signed distance in retraces, clamped: the window may never exceed two retraces, whatever the counters say
	// (an unbounded window starved game logic forever on the title stage).
	const int64_t ahead    = std::clamp<int64_t>((int32_t)(nextRetraceIndex - count), 0, 2);
	const int64_t deadline = last + ahead * 16'683'333;
	const auto windowEnd   = Clock::now() + std::chrono::nanoseconds(2 * 16'683'333);
	const int64_t margin   = 1'000'000; // stop 1 ms early so the caller's present is not late
	const auto slot = cap > 0 ? std::chrono::nanoseconds(1'000'000'000 / cap) : std::chrono::nanoseconds(0);
	auto toNs       = [](Clock::time_point p) {
		return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(p.time_since_epoch()).count();
	};
	// Cost budget: a slot runs a present and a replay, and the caller presents once more before its retrace. A slot that
	// ends after the deadline makes TVideo::waitForRetrace miss its retrace, and the tick slips by whole retraces (owner
	// RTX 3070 Ti, Delfino, fps_limit=60: logic 15/s). So a slot starts only if its decaying peak cost and the caller's
	// present still fit; when none fits, the tick keeps the single present of fps 30.
	// The peaks decay per tick as well, so a tick without slots retries after a cost spike (half after about 44 ticks).
	g_presentPeakNs -= g_presentPeakNs / 64;
	g_replayPeakNs -= g_replayPeakNs / 64;
	auto ns = [](Clock::duration d) { return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); };
	auto t  = Clock::now();
	int slots = 0;
	while (toNs(t) + margin < deadline && Clock::now() < windowEnd) {
		if (toNs(std::max(t, Clock::now())) + g_presentPeakNs * 2 + g_replayPeakNs + margin >= deadline)
			break;
		av::SleepUntil(t);
		const auto p0 = Clock::now();
		sunshinePresentFrame();
		const auto p1 = Clock::now();
		++presents;
		++slots;
		g_alpha = std::clamp(std::chrono::duration<float>(p1 - g_tickStart).count() / kLogicPeriod, 0.0f, 1.0f);
		sunshineReplayFrame();
		const auto p2 = Clock::now();
		notePeak(g_presentPeakNs, g_presentMaxNs, g_presentSumNs, ns(p1 - p0));
		notePeak(g_replayPeakNs, g_replayMaxNs, g_replaySumNs, ns(p2 - p1));
		t += slot;
		if (t < Clock::now()) // fell behind (slow replay): resync, do not burst to catch up
			t = Clock::now();
	}
	if (slots == 0)
		++g_skips;
	// Diagnosis: SUNSHINE_DEV_REPLAY_PROFILE forces one (late) replay per tick so its phase costs get measured on slow
	// adapters too; never on in normal play.
	static const bool forceReplay = std::getenv("SUNSHINE_DEV_REPLAY_PROFILE") != nullptr;
	if (slots == 0 && forceReplay)
		sunshineReplayFrame();
}
