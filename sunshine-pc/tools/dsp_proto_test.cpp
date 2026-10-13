// Offline check of the DSP protocol (src/sdk/dsp_glue.cpp) over the port's real OS layer and mailbox (os_thread.cpp, av.cpp):
// a small stand-in for JAudio's CPU side (JASAudioThread.cpp syncDSP, JASDSPBuf.cpp frame loop) boots the DSP, sets it up,
// and runs frames: DsyncFrame2, then 7 x (DSPReleaseHalt, wait for the interrupt message). Checks that DsetupTable returns
// through the DspFinishWork completion path, that every subframe raises exactly one 0xF355FFxx mail that reaches the
// request callback with interrupts disabled, and that the frame buffers hold the mixed voice afterwards.
//   tools\dsp_test.cmd builds and runs this together with dsp_test.cpp.
#include "dsp_mixer.hpp"

#include <JSystem/JAudio/JASystem/JASDSPInterface.hpp>
#include <JSystem/dspproc.h>
#include <JSystem/dsptask.h>
#include <dolphin/dsp.h>
#include <dolphin/os.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace JASystem::DSPInterface {
extern u32 DSPADPCM_FILTER[];
extern u32 DSPRES_FILTER[];
u16 JAS_DSP_PREFIX = 0xF355;
} // namespace JASystem::DSPInterface

using namespace JASystem::DSPInterface;

#define CHECK(c)                                                                                                       \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::fprintf(stderr, "dsp_proto_test: FAILED line %d: %s\n", __LINE__, #c);                                \
			std::abort();                                                                                              \
		}                                                                                                              \
	} while (0)

// ---- stubs for what Aurora and the JAudio sources provide in the real build ----
extern "C" OSTime OSGetTime(void) { return 0; }
extern "C" void* OSGetArenaLo(void) { return nullptr; }
void sunshinePresentFrame() {}

static std::vector<u8> gAram(1 << 20);
extern "C" void* ARGetStorageAddress(void) { return gAram.data(); }
extern "C" u32 ARGetSize(void) { return static_cast<u32>(gAram.size()); }

static DSPBuffer gVoices[hostdsp::kVoiceCount];
static FXBuffer gFx[hostdsp::kFxLineCount];

u64 DSP_CreateMap() // JASDSPInterface.cpp
{
	u64 map = 0;
	for (int i = 0; i < 64; ++i) {
		map <<= 1;
		if (gVoices[i].enabled)
			map |= 1;
	}
	return map;
}

// ---- JASAudioThread.cpp: the request callback and the audio thread's message queue ----
static OSMessageQueue gMq;
static OSMessage gMsgs[16];
static std::atomic<int> gRequestMails { 0 };
static std::atomic<int> gLastLow { 0 };

static void syncDSP(void*)
{
	while (!DSPCheckMailFromDSP())
		;
	const u32 mail = DSPReadMailFromDSP();
	if (mail >> 16 != JAS_DSP_PREFIX)
		return;
	switch (mail & 0xFF00) {
	case 0xFF00:
		++gRequestMails;
		gLastLow = static_cast<int>(mail & 0xFF);
		OSSendMessage(&gMq, reinterpret_cast<void*>(1), 0);
		break;
	default:
		DspFinishWork(mail & 0xFFFF);
		break;
	}
}

static s16 gBufRight[3][560], gBufLeft[3][560];

int main()
{
	std::thread([] {
		std::this_thread::sleep_for(std::chrono::seconds(60));
		std::fprintf(stderr, "dsp_proto_test: TIMEOUT (deadlock)\n");
		std::abort();
	}).detach();

	OSInitMessageQueue(&gMq, gMsgs, 16);

	// one PCM16 voice: constant 12000, left bus at full volume, right bus at half
	for (int i = 0; i < 4000; ++i) {
		gAram[0x4000 + i * 2]     = static_cast<u8>(12000 >> 8);
		gAram[0x4000 + i * 2 + 1] = static_cast<u8>(12000 & 0xFF);
	}
	DSPBuffer& v        = gVoices[3];
	v.baseAddress       = reinterpret_cast<s16*>(static_cast<uintptr_t>(0x4000));
	v.samplesSourceType = 0x10;
	v.sampleCount = v.endPosition = 4000;
	v.mixChannels[0]              = { hostdsp::kBusLeft, 0x7FFF, 0x7FFF, 0 };
	v.mixChannels[1]              = { hostdsp::kBusRight, 0x3FFF, 0x3FFF, 0 };
	v.resamplingRatio             = 0x1000;
	v.resetVpb                    = 1;
	v.enabled                     = 1;

	DspBoot(syncDSP);
	DsetupTable(64, reinterpret_cast<uintptr_t>(gVoices), reinterpret_cast<uintptr_t>(DSPRES_FILTER),
	            reinterpret_cast<uintptr_t>(DSPADPCM_FILTER), reinterpret_cast<uintptr_t>(gFx));
	std::fprintf(stderr, "dsp_proto_test: setup returned\n");
	CHECK(gRequestMails == 0); // the setup answer went through DspFinishWork, not the frame path

	DsetMixerLevel(1.0f);
	for (int frame = 0; frame < 3; ++frame) {
		int intcount = 7; // JASAudioThread::intcount
		DsyncFrame2(7, reinterpret_cast<uintptr_t>(gBufRight[frame]), reinterpret_cast<uintptr_t>(gBufLeft[frame]));
		DSPReleaseHalt();
		for (int sub = 1;; ++sub) {
			OSMessage m;
			OSReceiveMessage(&gMq, &m, OS_MESSAGE_BLOCK);
			CHECK(m == reinterpret_cast<OSMessage>(1));
			CHECK(gLastLow == 7 - sub);
			if (--intcount == 0)
				break;
			DSPReleaseHalt();
		}
		OSMessage extra;
		CHECK(OSReceiveMessage(&gMq, &extra, OS_MESSAGE_NOBLOCK) == 0); // exactly one message per subframe
	}
	CHECK(gRequestMails == 21);
	CHECK(static_cast<u32>(v.currentPosition) == 3 + 3 * 560); // 3 primed samples + one per output sample at ratio 1.0

	// DC through the resampler is the same constant everywhere past the first sample; right = left * 0x3FFF / 0x7FFF
	for (int f = 0; f < 3; ++f)
		for (int i = (f ? 0 : 1); i < 560; ++i) {
			CHECK(gBufLeft[f][i] == gBufLeft[0][100]);
			CHECK(gBufRight[f][i] == gBufRight[0][100]);
		}
	CHECK(gBufLeft[0][100] > 5900 && gBufLeft[0][100] < 6100); // 12000 * DC gain of the taps (32779 / 32768) * 0x7FFF >> 16
	CHECK(std::abs(gBufRight[0][100] * 2 - gBufLeft[0][100]) <= 2);

	// a voice that left the map is silent; so is a frame without voices
	v.enabled = 0;
	int intcount = 7;
	DsyncFrame2(7, reinterpret_cast<uintptr_t>(gBufRight[0]), reinterpret_cast<uintptr_t>(gBufLeft[0]));
	DSPReleaseHalt();
	for (;;) {
		OSMessage m;
		OSReceiveMessage(&gMq, &m, OS_MESSAGE_BLOCK);
		if (--intcount == 0)
			break;
		DSPReleaseHalt();
	}
	for (int i = 0; i < 560; ++i)
		CHECK(gBufLeft[0][i] == 0 && gBufRight[0][i] == 0);
	std::fprintf(stderr, "dsp_proto_test: ok\n");
	return 0;
}
