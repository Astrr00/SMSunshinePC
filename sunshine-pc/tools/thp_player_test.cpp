// Offline check of the whole THP player on the host: THPPlayer.c, THPRead.c, THPVideoDecode.c, THPAudioDecode.c,
// THPDec.c, THPAudio.c running on the port's real OS layer (os_thread.cpp: baton-scheduled threads, message queues) and
// real VI retrace / AI code (av.cpp). Only the edges are replaced by test doubles: the DVD (stdio on the extracted movie),
// GX (THPDraw's three entry points just record which planes would be drawn), and JAudio's mix-callback hook (the test
// plays the AI pump and pulls the callback itself).
//   tools\thp_player_test.cmd [movie.thp] [retraces]
// It plays the movie the way MovieDirector does (Init, Open, CalcNeedMemory, SetBuffer, Prepare, Play) and runs a game loop
// of VIWaitForRetrace + THPPlayerDrawCurrentFrame + THPPlayerDrawDone, then checks
//   * every frame the player hands to the renderer is bit-identical to a direct THPVideoDecode of that frame
//     (endian swaps, 64-bit pointers, texture-set recycling),
//   * the displayed frame numbers advance at 29.97 fps (one per two retraces),
//   * the audio the mix callback delivers is the THPAudioDecode output of the frames in order, sample for sample.
#include <algorithm>
#include <malloc.h>
#include <csignal>
void installCrashHandler(); // src/crash.cpp
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <THPPlayer/THPDraw.h>
#include <THPPlayer/THPPlayer.h>
#include <dolphin/ai.h>
#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <dolphin/thp.h>
#include <dolphin/vi.h>
#include <JSystem/JAudio/JASystem/JASAiCtrl.hpp>

#define CHECK(c)                                                                                                       \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::fprintf(stderr, "thp_player_test: FAILED line %d: %s\n", __LINE__, #c);                               \
			std::exit(1);                                                                                              \
		}                                                                                                              \
	} while (0)

// ---- test doubles --------------------------------------------------------------------------------------------------

// DVD: the movie is a host file; the FILE* travels in the command block's userData.
extern "C" BOOL DVDOpen(char* fileName, DVDFileInfo* fi)
{
	FILE* f = std::fopen(fileName, "rb");
	if (!f)
		return FALSE;
	fi->cb.userData = f;
	return TRUE;
}
extern "C" BOOL DVDClose(DVDFileInfo* fi)
{
	std::fclose((FILE*)fi->cb.userData);
	return TRUE;
}
extern "C" s32 DVDReadPrio(DVDFileInfo* fi, void* addr, s32 length, s32 offset, s32)
{
	FILE* f = (FILE*)fi->cb.userData;
	if (std::fseek(f, offset, SEEK_SET) != 0)
		return -1;
	return (s32)std::fread(addr, 1, (size_t)length, f);
}
extern "C" s32 DVDCancel(DVDCommandBlock*) { return 0; }

// GX: THPDraw.c's draw call is replaced by a recorder
struct Shown {
	u8 *y = nullptr, *u = nullptr, *v = nullptr;
	int w = 0, h = 0;
} gShown;
extern "C" void THPGXRestore() {}
extern "C" void THPGXYuv2RgbSetup(GXRenderModeObj*) {}
extern "C" void THPGXYuv2RgbDraw(u8* y, u8* u, u8* v, s16, s16, s16 tw, s16 th, s16, s16) { gShown = { y, u, v, tw, th }; }
extern "C" void GXDrawDone() {}

extern "C" void LCEnable() {}
extern "C" void LCDisable() {}
extern "C" void DCFlushRange(void*, u32) {}
extern "C" void DCInvalidateRange(void*, u32) {}
extern "C" void DCZeroRange(void* p, u32 n) { std::memset(p, 0, n); }
extern "C" u32 VIGetTvFormat(void) { return VI_NTSC; }
extern "C" OSTime OSGetTime(void) { return 0; }
extern "C" void* OSGetArenaLo(void) { return nullptr; }
void sunshinePresentFrame() {}

static JASystem::Kernel::MixCallback gMix;
namespace JASystem::Kernel {
void registerMixCallback(MixCallback cb, u8) { gMix = cb; }
} // namespace JASystem::Kernel

// ---- the movie, read the way the player does ------------------------------------------------------------------------
static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0); // the OS layer can end the process without flushing
	installCrashHandler(); // symbolized stack on a fault (src/crash.cpp, needs the PDB)
	std::signal(SIGABRT, [](int) { __debugbreak(); });
	std::set_terminate([] {
		if (auto e = std::current_exception()) {
			try {
				std::rethrow_exception(e);
			} catch (const std::exception& x) {
				std::fprintf(stderr, "thp_player_test: terminate: exception %s\n", x.what());
			} catch (...) {
				std::fprintf(stderr, "thp_player_test: terminate: unknown exception\n");
			}
		} else {
			std::fprintf(stderr, "thp_player_test: terminate without exception\n");
		}
		std::abort();
	});
	const char* path = argc > 1 ? argv[1] : "sms/orig/GMSJ01/extracted/files/data/openingA.thp";
	int retraces     = argc > 2 ? std::atoi(argv[2]) : 150;

	FILE* f = std::fopen(path, "rb");
	CHECK(f);
	uint8_t hdr[0x60];
	CHECK(std::fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
	const uint32_t bufsize = be32(hdr + 8), numFrames = be32(hdr + 20), firstSize = be32(hdr + 24), movieOff = be32(hdr + 0x28);
	uint8_t ci[0x40];
	std::fseek(f, (long)be32(hdr + 0x20), SEEK_SET);
	CHECK(std::fread(ci, 1, sizeof(ci), f) == sizeof(ci));
	const uint32_t nComp = be32(ci);
	uint32_t xSize = 0, ySize = 0, tracks = 0;
	const uint8_t* rec = ci + 20;
	for (uint32_t i = 0; i < nComp; i++) {
		if (ci[4 + i] == 0) {
			xSize = be32(rec), ySize = be32(rec + 4);
			rec += 12;
		} else {
			tracks = be32(rec + 12);
			rec += 16;
		}
	}
	const size_t ySz = (size_t)xSize * ySize, cSz = ySz / 4;
	const int nRef   = retraces / 2 + 8; // frames the run can reach
	CHECK((uint32_t)nRef <= numFrames);

	// reference decode of the frames (direct entry points, before any player thread exists)
	CHECK(THPInit());
	std::vector<uint8_t> refYuv((size_t)nRef * (ySz + 2 * cSz));
	std::vector<int16_t> refPcm;
	{
		std::vector<uint8_t> frame(bufsize + 64), work(0x1000 + 64);
		long off = (long)movieOff;
		uint32_t sz = firstSize;
		for (int n = 0; n < nRef; n++) {
			std::fseek(f, off, SEEK_SET);
			CHECK(std::fread(frame.data(), 1, sz, f) == sz);
			const uint8_t* vdata = frame.data() + 8 + 4 * nComp;
			uint8_t* out         = refYuv.data() + (size_t)n * (ySz + 2 * cSz);
			const s32 err = THPVideoDecode((void*)vdata, out, out + ySz, out + ySz + cSz, work.data());
			if (err)
				std::fprintf(stderr, "thp_player_test: reference frame %d: THPVideoDecode error %d\n", n, err);
			CHECK(err == 0);
			if (tracks) {
				int16_t pcm[2 * 2048]; // up to audioMaxSamples (1078) stereo frames
				uint32_t s = THPAudioDecode((s16*)pcm, (u8*)(vdata + be32(frame.data() + 8)), 0);
				refPcm.insert(refPcm.end(), pcm, pcm + 2 * s);
			}
			off += sz;
			sz = be32(frame.data());
		}
	}
	std::fclose(f);
	std::printf("thp_player_test: %s  %ux%u, %u audio track(s); reference: %d frames, %zu audio samples\n", path, xSize, ySize,
	            tracks, nRef, refPcm.size() / 2);

	// ---- MovieDirector::rsetup ----
	CHECK(THPPlayerInit(0));
	CHECK(THPPlayerOpen(path, 0));
	u32 mem  = THPPlayerCalcNeedMemory();
	u8* base = (u8*)_aligned_malloc(mem, 32);
	CHECK(base && THPPlayerSetBuffer(base));
	std::printf("thp_player_test: player memory %u bytes\n", mem);
	auto t0 = std::chrono::steady_clock::now();
	CHECK(THPPlayerPrepare(0, 0, 0));
	std::printf("thp_player_test: prepared after %.1f ms\n",
	            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
	CHECK(THPPlayerGetState() == 1);
	THPVideoInfo vi;
	CHECK(THPPlayerGetVideoInfo(&vi) && vi.xSize == xSize && vi.ySize == ySize);
	CHECK(THPPlayerPlay());

	// ---- the game loop ----
	std::vector<s16> audio;
	std::vector<int> shownFrames;
	int firstShown = -1, lastFrame = -1, repeats = 0, nonMono = 0, badPixels = 0;
	double consumed = 0;
	for (int r = 1; r <= retraces; r++) {
		VIWaitForRetrace();
		gShown      = Shown();
		const s32 f = THPPlayerDrawCurrentFrame(nullptr, 0, 0, xSize, ySize);
		if (f >= 0) {
			CHECK(f < nRef && gShown.y && gShown.w == (int)xSize && gShown.h == (int)ySize);
			const uint8_t* ref = refYuv.data() + (size_t)f * (ySz + 2 * cSz);
			if (std::memcmp(gShown.y, ref, ySz) || std::memcmp(gShown.u, ref + ySz, cSz) || std::memcmp(gShown.v, ref + ySz + cSz, cSz))
				badPixels++;
			if (firstShown < 0)
				firstShown = r;
			if (f == lastFrame)
				repeats++;
			if (f < lastFrame)
				nonMono++;
			lastFrame = f;
			shownFrames.push_back(f);
		}
		THPPlayerDrawDone();
		// the AI pump: 32000 Hz against the 59.94 Hz retrace
		const int want = (int)(r * 32000.0 / 59.94 - consumed);
		consumed += want;
		if (gMix) {
			CHECK(want <= 560);
			if (s16* p = gMix(want))
				audio.insert(audio.end(), p, p + 2 * want);
		}
	}
	const int state = THPPlayerGetState();
	THPPlayerStop();
	THPPlayerClose();
	THPPlayerQuit();

	// ---- verdicts ----
	std::printf("thp_player_test: %zu frames drawn from retrace %d, last frame %d, repeated draws %d, backward steps %d, "
	            "frames whose planes differ from the direct decode: %d\n",
	            shownFrames.size(), firstShown, lastFrame, repeats, nonMono, badPixels);
	CHECK(badPixels == 0 && nonMono == 0);
	const double rate = lastFrame / ((retraces - firstShown) / 59.94);
	std::printf("thp_player_test: displayed %.2f frames/s (movie %.2f), state at end %d\n", rate, 30000.0 / 1001, state);
	CHECK(rate > 29.0 && rate < 31.0);

	if (tracks) {
		// MixAudio delivers silence until the first audio buffer is decoded; line the two streams up at their first sample
		size_t lead = 0, refLead = 0;
		while (lead < audio.size() && audio[lead] == 0)
			lead++;
		while (refLead < refPcm.size() && refPcm[refLead] == 0)
			refLead++;
		const size_t cmp = std::min(audio.size() - lead, refPcm.size() - refLead);
		size_t bad       = 0;
		for (size_t i = 0; i < cmp; i++)
			bad += audio[lead + i] != refPcm[refLead + i];
		std::printf("thp_player_test: audio %zu sample frames pulled, %zu of silence first; %zu compared with THPAudioDecode, %zu "
		            "differ\n",
		            audio.size() / 2, lead / 2, cmp / 2, bad);
		CHECK(bad == 0 && cmp > 20000);
	}
	std::printf("thp_player_test: ok\n");
	return 0;
}
