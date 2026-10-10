// Offline driver for the decomp's THP decoder on the host (sms/src/dolphin/thp/THPDec.c, THPAudio.c, TARGET_PC code).
//   tools\thp_test.cmd [movie.thp] [frames]
// Decodes the first N frames of a real movie through the real decoder entry points (THPInit, THPVideoDecode,
// THPAudioDecode) and dumps what they produce:
//   dec_video.bin   per frame: Y, U, V planes exactly as the decoder writes them (GX I8 tile order, 8x4 tiles)
//   dec_audio.pcm   per frame: THPAudioDecode(flag 0) output, interleaved s16 stereo, host byte order
// tools\thp_ref.py compares both with independent references (a float32/fma model of the console's assembly, ffmpeg).
// The movie container is parsed here the way THPPlayerOpen / VideoDecode / AudioDecode do it (big-endian words).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dolphin/thp.h> // the decomp's header: THPFileInfo for the work-area budget check

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

// The decoder zeroes its work area with DCZeroRange (Aurora's OSCache.cpp: a memset); the test links without Aurora.
extern "C" void DCZeroRange(void* p, uint32_t n) { std::memset(p, 0, n); }

#define CHECK(c)                                                                                                       \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::fprintf(stderr, "thp_test: FAILED line %d: %s\n", __LINE__, #c);                                      \
			std::exit(1);                                                                                              \
		}                                                                                                              \
	} while (0)

int main(int argc, char** argv)
{
	const char* path = argc > 1 ? argv[1] : "sms/orig/GMSJ01/extracted/files/data/openingA.thp";
	int nFrames      = argc > 2 ? std::atoi(argv[2]) : 8;
	const char* out  = argc > 3 ? argv[3] : ".";
	int start        = argc > 4 ? std::atoi(argv[4]) : 0;

	FILE* f = std::fopen(path, "rb");
	CHECK(f);
	uint8_t hdr[0x60];
	CHECK(std::fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
	CHECK(std::memcmp(hdr, "THP\0", 4) == 0 && be32(hdr + 4) == 0x11000);
	const uint32_t bufsize = be32(hdr + 8), numFrames = be32(hdr + 20), firstSize = be32(hdr + 24);
	const uint32_t compOff = be32(hdr + 0x20), movieOff = be32(hdr + 0x28);
	std::vector<uint8_t> ci(0x40);
	std::fseek(f, (long)compOff, SEEK_SET);
	CHECK(std::fread(ci.data(), 1, ci.size(), f) == ci.size());
	const uint32_t nComp = be32(ci.data());
	uint32_t xSize = 0, ySize = 0, tracks = 0;
	const uint8_t* rec = ci.data() + 4 + 16;
	for (uint32_t i = 0; i < nComp; i++) {
		if (ci[4 + i] == 0) {
			xSize = be32(rec), ySize = be32(rec + 4);
			rec += 12;
		} else {
			tracks = be32(rec + 12);
			rec += 16;
		}
	}
	std::printf("thp_test: %s  %ux%u, %u frames, %u audio track(s), bufsize %u\n", path, xSize, ySize, numFrames, tracks, bufsize);
	CHECK(xSize % 16 == 0 && ySize % 16 == 0 && nFrames <= (int)numFrames);

	// THPPlayerCalcNeedMemory gives the work area 0x1000 bytes (the decoder places THPFileInfo and the six MCU buffers in it)
	const size_t workBytes = 0x1000;
	std::printf("thp_test: sizeof(THPFileInfo) = %zu (host), work area budget %zu: needs %zu\n", sizeof(THPFileInfo), workBytes,
	            32 + sizeof(THPFileInfo) + 32 + 6 * 64 * sizeof(THPCoeff));
	CHECK(32 + sizeof(THPFileInfo) + 32 + 6 * 64 * sizeof(THPCoeff) <= workBytes);

	const size_t ySz = (size_t)xSize * ySize, cSz = ySz / 4;
	std::vector<uint8_t> frame(bufsize + 64), work(workBytes + 4096, 0xA5);
	uint8_t* workp = work.data(); // guard bytes behind the budget catch an overrun
	std::vector<uint8_t> yuv(ySz + 2 * cSz);
	std::vector<int16_t> pcm(2 * 2048);

	CHECK(THPInit() == TRUE);
	FILE* fv = std::fopen((std::string(out) + "/dec_video.bin").c_str(), "wb");
	FILE* fa = std::fopen((std::string(out) + "/dec_audio.pcm").c_str(), "wb");
	CHECK(fv && fa);

	long off     = (long)movieOff;
	uint32_t sz  = firstSize;
	for (int n = 0; n < start; n++) { // each frame starts with the size of the next one
		uint8_t w[4];
		std::fseek(f, off, SEEK_SET);
		CHECK(std::fread(w, 1, 4, f) == 4);
		off += sz;
		sz = be32(w);
	}
	double msSum = 0;
	for (int n = 0; n < nFrames; n++) {
		CHECK(sz <= bufsize);
		std::fseek(f, off, SEEK_SET);
		CHECK(std::fread(frame.data(), 1, sz, f) == sz);
		const uint32_t nextSize = be32(frame.data());
		const uint8_t* sizes    = frame.data() + 8;
		const uint8_t* vdata    = frame.data() + 8 + 4 * nComp;
		const uint8_t* adata    = vdata + be32(sizes);

		std::memset(yuv.data(), 0xCC, yuv.size());
		auto t0 = std::chrono::steady_clock::now();
		int err = THPVideoDecode((void*)vdata, yuv.data(), yuv.data() + ySz, yuv.data() + ySz + cSz, workp);
		auto t1 = std::chrono::steady_clock::now();
		msSum += std::chrono::duration<double, std::milli>(t1 - t0).count();
		if (err) {
			std::fprintf(stderr, "thp_test: frame %d: THPVideoDecode error %d\n", n, err);
			return 1;
		}
		for (size_t i = workBytes; i < work.size(); i++)
			CHECK(work[i] == 0xA5);
		std::fwrite(yuv.data(), 1, yuv.size(), fv);

		if (tracks) {
			uint32_t samples = THPAudioDecode((s16*)pcm.data(), (u8*)adata, 0);
			CHECK(samples > 0 && samples <= 2048);
			std::fwrite(pcm.data(), 4, samples, fa);
		}
		off += sz;
		sz = nextSize;
	}
	std::fclose(fv);
	std::fclose(fa);
	std::fclose(f);
	std::printf("thp_test: decoded %d frames, %.2f ms/frame video\n", nFrames, msSum / nFrames);
	return 0;
}
