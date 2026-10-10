// Presentation pacing (Port.md phase 5). The game logic keeps its 30 Hz NTSC schedule: TVideo::waitForRetrace(2) waits
// for two retraces per logic tick. With settings().fpsLimit above 30 (or unlimited), the interval between two logic ticks
// is filled with extra presents of the current game frame, re-drawn without advancing logic (sunshineReplayFrame,
// sms/src/System/MarDirectorDirect.cpp).
// ponytail: fixed slot grid from the cap, not aligned to vblank; ceiling: present jitter of the sleep granularity.
// Upgrade: align slots to the display's vblank when the renderer exposes it.
#include <dolphin/vi.h>
#include <chrono>
#include <cstdio>

#include "../sdk/av.hpp"
#include "../settings.h"

void sunshinePresentFrame();      // src/main.cpp
bool sunshineCanReplay();         // sms/src/System/MarDirectorDirect.cpp
void sunshineReplayFrame();       // sms/src/System/MarDirectorDirect.cpp
int64_t VIGetLastRetraceNs(u32* count); // src/sdk/av.cpp

namespace {
constexpr int kLogicRate = 30; // logic ticks per second (two retraces each)
using Clock              = std::chrono::steady_clock;
}

// Called once per logic tick from TVideo::waitForRetrace(), before its own pre-wait (sms/src/JSystem/JDrama/JDRVideo.cpp).
// Presents the frame drawn by TMarDirector::direct(), then replays on the cap's slot grid until the caller's retrace
// (nextRetraceIndex) is due. The caller's own present and wait take over, so logic stays locked to the retrace count.
void sunshinePaceWindow(u32 nextRetraceIndex)
{
	static int ticks = 0;
	static int presents = 0;
	static auto logStart = Clock::now();

	const auto now = Clock::now();
	++ticks;
	if (now - logStart >= std::chrono::seconds(1)) {
		std::printf("[framerate] logic %d/s, window presents %d/s, fpsLimit %d\n", ticks, presents,
		            sunshine::settings().fpsLimit);
		std::fflush(stdout);
		ticks = 0;
		presents = 0;
		logStart = now;
	}

	const int cap = sunshine::settings().fpsLimit; // 0 = unlimited (no slot sleep)
	if ((cap > 0 && cap <= kLogicRate) || !sunshineCanReplay())
		return;

	// Retraces are periodic: the caller's retrace is due at lastRetrace + (nextRetraceIndex - count) periods.
	u32 count          = 0;
	const int64_t last = VIGetLastRetraceNs(&count);
	const int64_t deadline = last + (int64_t)(nextRetraceIndex - count) * 16'683'333;
	const int64_t margin   = 1'000'000; // stop 1 ms early so the caller's present is not late
	const auto slot = cap > 0 ? std::chrono::nanoseconds(1'000'000'000 / cap) : std::chrono::nanoseconds(0);
	auto toNs       = [](Clock::time_point p) {
		return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(p.time_since_epoch()).count();
	};
	auto t = Clock::now();
	while (toNs(t) + margin < deadline) {
		av::SleepUntil(t);
		sunshinePresentFrame();
		++presents;
		sunshineReplayFrame();
		t += slot;
		if (t < Clock::now()) // fell behind (slow replay): resync, do not burst to catch up
			t = Clock::now();
	}
}
