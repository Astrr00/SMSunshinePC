// Opt-in PCM dump for audio verification: SUNSHINE_AUDIO_DUMP=<file.wav> writes every stereo block that the AI pump
// hands to the device (JAudio/DSP output and THP movie audio, both mixed into the DAC buffer) as 16-bit stereo WAV.
// ponytail: one file, no rotation; the size fields are patched every 3 s and at exit, so a crash loses at most 3 s of
// header; a block at a different rate than the first one is skipped (the rate is fixed in the header).
#include "av.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

std::mutex g_m;
FILE* g_f        = nullptr;
bool g_tried     = false;
uint32_t g_rate  = 0;
uint64_t g_frames  = 0;
uint64_t g_patched = 0;

void putLe(uint8_t* p, uint32_t v, int bytes)
{
	for (int i = 0; i < bytes; ++i)
		p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// Writes the RIFF and data sizes for the frames so far and leaves the file position at the end. Caller holds g_m.
void patchLocked()
{
	uint8_t b[4];
	const uint32_t data = static_cast<uint32_t>(g_frames * 4);
	std::fseek(g_f, 4, SEEK_SET);
	putLe(b, 36 + data, 4);
	std::fwrite(b, 1, 4, g_f);
	std::fseek(g_f, 40, SEEK_SET);
	putLe(b, data, 4);
	std::fwrite(b, 1, 4, g_f);
	std::fseek(g_f, 0, SEEK_END);
	std::fflush(g_f);
	g_patched = g_frames;
}

void closeAtExit()
{
	std::lock_guard<std::mutex> lk(g_m);
	if (!g_f)
		return;
	patchLocked();
	std::fclose(g_f);
	g_f = nullptr;
	std::fprintf(stderr, "[av] audio dump: %llu stereo frames at %u Hz\n", static_cast<unsigned long long>(g_frames),
	             static_cast<unsigned>(g_rate));
}

void openLocked(double hz)
{
	const char* path = std::getenv("SUNSHINE_AUDIO_DUMP");
	g_tried          = true;
	g_f              = std::fopen(path, "wb");
	if (!g_f) {
		std::fprintf(stderr, "[av] SUNSHINE_AUDIO_DUMP: cannot create %s\n", path);
		return;
	}
	g_rate = static_cast<uint32_t>(hz + 0.5); // the rounding AudioWrite opens the device with
	uint8_t h[44];
	std::memset(h, 0, sizeof(h));
	std::memcpy(h, "RIFF", 4);
	std::memcpy(h + 8, "WAVE", 4);
	std::memcpy(h + 12, "fmt ", 4);
	putLe(h + 16, 16, 4);            // fmt chunk size
	putLe(h + 20, 1, 2);             // PCM
	putLe(h + 22, 2, 2);             // stereo
	putLe(h + 24, g_rate, 4);        // sample rate
	putLe(h + 28, g_rate * 4, 4);    // byte rate
	putLe(h + 32, 4, 2);             // block align
	putLe(h + 34, 16, 2);            // bits per sample
	std::memcpy(h + 36, "data", 4);
	std::fwrite(h, 1, sizeof(h), g_f);
	std::atexit(closeAtExit);
	std::fprintf(stderr, "[av] audio dump to %s (%u Hz stereo s16)\n", path, static_cast<unsigned>(g_rate));
}

} // namespace

namespace av {

void DumpWrite(const int16_t* stereo, uint32_t frames, double hz)
{
	static const bool on = std::getenv("SUNSHINE_AUDIO_DUMP") != nullptr;
	if (!on)
		return;
	std::lock_guard<std::mutex> lk(g_m);
	if (!g_tried)
		openLocked(hz);
	if (!g_f)
		return;
	if (static_cast<uint32_t>(hz + 0.5) != g_rate) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			std::fprintf(stderr, "[av] audio dump: block at %.1f Hz skipped (dump rate %u)\n", hz,
			             static_cast<unsigned>(g_rate));
		}
		return;
	}
	std::fwrite(stereo, 4, frames, g_f);
	std::fflush(g_f); // every block: the process may be killed (TerminateProcess skips atexit)
	g_frames += frames;
	if (g_frames - g_patched >= 3ull * g_rate)
		patchLocked();
}

} // namespace av
