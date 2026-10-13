// Offline checks for the host DSP (src/sdk/dsp_mixer.*): standalone, no game, no Aurora.
//   tools\dsp_test.cmd            (generates the ADPCM fixture with tools\dsp_ref.py first)
// Exits 0 and prints "dsp_test: ok" when every check passes; aborts with the failing line otherwise.
//
//  1. ADPCM: the decoder is compared sample for sample with tools/dsp_ref.py, an independent transcription of the format
//     (real frames from the disc's wave banks + random frames that exercise the saturation).
//  2. Mixer on synthetic voices built the way JASDSPInterface.cpp's DSPBuffer methods build them (the helpers below mirror
//     allocInit/setWaveInfo/playStart/setMixerInitVolume/setBusConnect): PCM16/PCM8/ADPCM sources, pitch, loops, end of
//     sample, volume ramp, panning buses, master level, effect line, stream ring buffer, oscillator, pause, forced stop.
//     Expected values come from a direct (random access) formulation of the resampler, not from the mixer's window code.
#include "dsp_mixer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace JASystem::DSPInterface {
extern u32 DSPADPCM_FILTER[];
extern u32 DSPRES_FILTER[];
} // namespace JASystem::DSPInterface

using namespace JASystem::DSPInterface;
using hostdsp::kSubframeSamples;

#define CHECK(c)                                                                                                       \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::fprintf(stderr, "dsp_test: FAILED line %d: %s\n", __LINE__, #c);                                      \
			std::abort();                                                                                              \
		}                                                                                                              \
	} while (0)

static int gSection;
static void section(const char* name) { std::fprintf(stderr, "dsp_test: %d %s\n", ++gSection, name); }

// ---------------------------------------------------------------------------------------------------------------------
// The CPU side of a voice (JASDSPInterface.cpp), reduced to what the mixer reads
// ---------------------------------------------------------------------------------------------------------------------

static const u32 kAramBase = 0x4000;

struct Rig {
	std::vector<u8> aram = std::vector<u8>(1 << 20);
	std::vector<DSPBuffer> voices = std::vector<DSPBuffer>(hostdsp::kVoiceCount);
	std::vector<FXBuffer> fx      = std::vector<FXBuffer>(hostdsp::kFxLineCount);
	hostdsp::Mixer mixer;

	Rig()
	{
		std::memset(voices.data(), 0, voices.size() * sizeof(DSPBuffer));
		std::memset(fx.data(), 0, fx.size() * sizeof(FXBuffer));
		hostdsp::Mixer::Config c;
		c.voices     = voices.data();
		c.fx         = fx.data();
		c.resTable   = DSPRES_FILTER;
		c.adpcmTable = DSPADPCM_FILTER;
		c.aram       = aram.data();
		c.aramSize   = aram.size();
		mixer.configure(c);
	}

	// Renders `n` subframes of the given voices; appends to left/right.
	void run(u64 map, int n, std::vector<s16>& left, std::vector<s16>& right, u16 master = 0x1000)
	{
		for (int i = 0; i < n; ++i) {
			s16 l[kSubframeSamples], r[kSubframeSamples];
			mixer.render(map, master, l, r);
			left.insert(left.end(), l, l + kSubframeSamples);
			right.insert(right.end(), r, r + kSubframeSamples);
		}
	}
};

static u64 bit(int voice) { return 1ull << (63 - voice); }

static void allocInit(DSPBuffer& d) // DSPBuffer::allocInit + initFilter
{
	d.useConstantSample = 0;
	d.done              = 0;
	d.endRequested      = 0;
	d.enabled           = 0;
	d.useDolbyVolume    = 0;
	for (int i = 0; i < 8; ++i)
		d.variableFirCoeffs[i] = 0;
	d.variableFirCoeffs[0] = 0x7FFF;
	for (int i = 0; i < 4; ++i)
		d.biquadFilterCoeffs[i] = 0;
	d.biquadFilterCoeffs[0] = 0x7FFF;
	d.lowPassCoeff          = 0;
}

// DSPBuffer::setWaveInfo: format 0 = ADPCM4, 2 = PCM8, 3 = PCM16
static void setWave(DSPBuffer& d, int format, u32 aramAddr, int sampleCount, int loop = 0, int loopStart = 0, int loopEnd = 0,
                    s16 last = 0, s16 penult = 0)
{
	static const u8 blockSamples[8] = { 0x10, 0x10, 0x01, 0x01, 0x01, 0x10, 0x10, 0x01 };
	static const u8 blockBytes[8]   = { 0x09, 0x05, 0x08, 0x10, 0x01, 0x01, 0x01, 0x01 };
	d.baseAddress                   = reinterpret_cast<s16*>(static_cast<uintptr_t>(aramAddr));
	d.afcRemainingDecodedSamples    = blockSamples[format];
	d.samplesSourceType             = blockBytes[format];
	d.sampleCount                   = sampleCount;
	d.isLooping                     = loop;
	if (loop) {
		d.loopAddress = reinterpret_cast<s16*>(static_cast<uintptr_t>(loopStart));
		d.endPosition = loopEnd;
		d.loopYN1     = last;
		d.loopYN2     = penult;
	} else {
		d.endPosition = sampleCount;
	}
	for (int i = 0; i < 16; ++i)
		d.afcRemainingSamples[i] = 0;
}

static void playStart(DSPBuffer& d, u16 pitch) // DSPBuffer::playStart + setPitch
{
	d.currentPosition = 0;
	d.currentPosFrac  = 0;
	d.resetVpb        = 1;
	d.constantSample  = 0;
	for (int i = 0; i < 4; ++i) {
		d.resampleBuffer[i] = 0;
		d.biquadHistory[i]  = 0;
	}
	for (int i = 0; i < 20; ++i)
		d.variableFirHistory[i] = 0;
	d.resamplingRatio = pitch;
	d.enabled         = 1;
}

static void mixInit(DSPBuffer& d, int ch, u16 bus, s16 vol) // setBusConnect + setMixerInitVolume
{
	d.mixChannels[ch].id            = bus;
	d.mixChannels[ch].currentVolume = vol;
	d.mixChannels[ch].targetVolume  = vol;
}

static void putBe16(Rig& r, u32 addr, s16 v)
{
	r.aram[addr]     = static_cast<u8>(static_cast<u16>(v) >> 8);
	r.aram[addr + 1] = static_cast<u8>(v);
}

// ---------------------------------------------------------------------------------------------------------------------
// Reference: the resampler as a direct formula over the whole source signal x[] (zero outside)
// ---------------------------------------------------------------------------------------------------------------------

static s16 sat(s64 v) { return static_cast<s16>(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

static std::vector<s16> refResample(const std::vector<s16>& x, u32 ratio, int count)
{
	auto at = [&](s64 i) -> s32 { return i < 0 || i >= static_cast<s64>(x.size()) ? 0 : x[i]; };
	std::vector<s16> y;
	u64 pos = 0; // 16.16 source position
	for (int n = 0; n < count; ++n) {
		const s64 k    = static_cast<s64>(pos >> 16);
		const u32 ph   = static_cast<u32>((pos & 0xFFFF) >> 10);
		const u32 a    = DSPRES_FILTER[ph * 2], b = DSPRES_FILTER[ph * 2 + 1];
		const s64 acc  = at(k - 1) * static_cast<s16>(a >> 16) + at(k) * static_cast<s16>(a) + at(k + 1) * static_cast<s16>(b >> 16)
		                 + at(k + 2) * static_cast<s16>(b);
		y.push_back(sat(acc >> 15));
		pos += static_cast<u64>(ratio) << 4;
	}
	return y;
}

static s16 gain(s16 v, s32 vol) { return sat((static_cast<s64>(v) * vol) >> 15); } // 1.15 stages (FIR, effect line)
static s16 mix(s16 v, s32 vol) { return sat((static_cast<s64>(v) * vol) >> 16); } // voice -> bus (ucode mixer, M0)

static std::vector<s16> pseudoSignal(int n, unsigned seed)
{
	std::vector<s16> x(n);
	unsigned s = seed;
	for (int i = 0; i < n; ++i) {
		s    = s * 1664525u + 1013904223u;
		x[i] = static_cast<s16>((s >> 16) & 0xFFFF) / 2 + static_cast<s16>(std::sin(i * 0.05) * 8000);
	}
	return x;
}

static bool sameSamples(const std::vector<s16>& got, const std::vector<s16>& want, size_t from, size_t count, int tol = 0)
{
	for (size_t i = 0; i < count; ++i) {
		const int d = got[from + i] - want[i];
		if (d > tol || d < -tol) {
			std::fprintf(stderr, "  mismatch at %zu: got %d, want %d\n", from + i, got[from + i], want[i]);
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------

static void testAdpcmFixture(const char* path)
{
	section("ADPCM decoder against tools/dsp_ref.py");
	std::FILE* f = std::fopen(path, "rb");
	CHECK(f);
	char magic[4];
	u32 chunks;
	CHECK(std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "DSPT", 4) == 0);
	CHECK(std::fread(&chunks, 4, 1, f) == 1);
	u32 realChunks = 0;
	size_t frames  = 0;
	for (u32 c = 0; c < chunks; ++c) {
		u32 kind, n;
		CHECK(std::fread(&kind, 4, 1, f) == 1 && std::fread(&n, 4, 1, f) == 1);
		std::vector<u8> data(static_cast<size_t>(n) * 9);
		std::vector<s16> want(static_cast<size_t>(n) * 16);
		CHECK(std::fread(data.data(), 1, data.size(), f) == data.size());
		CHECK(std::fread(want.data(), 2, want.size(), f) == want.size());
		s16 y1 = 0, y2 = 0;
		for (u32 i = 0; i < n; ++i) {
			s16 out[16];
			hostdsp::adpcm4Frame(&data[i * 9], DSPADPCM_FILTER, out, y1, y2);
			for (int k = 0; k < 16; ++k)
				if (out[k] != want[i * 16 + k]) {
					std::fprintf(stderr, "  chunk %u frame %u sample %d: got %d, want %d\n", c, i, k, out[k], want[i * 16 + k]);
					CHECK(false);
				}
		}
		realChunks += kind;
		frames += n;
	}
	std::fclose(f);
	std::fprintf(stderr, "  %u chunks (%u from the disc), %zu frames, %zu samples identical\n", chunks, realChunks, frames, frames * 16);
	CHECK(realChunks > 0 || std::getenv("DSP_TEST_NO_DISC"));
}

static void testPcm16Pitch()
{
	section("PCM16 source, resampler at several pitches, saturated output and end of sample");
	const std::vector<s16> x = pseudoSignal(900, 7);
	for (u32 ratio : { 0x1000u, 0x2000u, 0x0800u, 0x1800u, 0x0A5Du, 0x7FFFu }) {
		Rig r;
		for (size_t i = 0; i < x.size(); ++i)
			putBe16(r, kAramBase + static_cast<u32>(i) * 2, x[i]);
		DSPBuffer& v = r.voices[5];
		allocInit(v);
		setWave(v, 3, kAramBase, static_cast<int>(x.size()));
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		mixInit(v, 1, hostdsp::kBusRight, 0x3FFF);
		playStart(v, static_cast<u16>(ratio));
		std::vector<s16> l, rr;
		r.run(bit(5), 4, l, rr);
		std::vector<s16> ref = refResample(x, ratio, 4 * kSubframeSamples);
		std::vector<s16> wantL, wantR;
		for (s16 s : ref) {
			wantL.push_back(mix(s, 0x7FFF));
			wantR.push_back(mix(s, 0x3FFF));
		}
		// the reference runs on past the end of the sample; the mixer stops as soon as the data ended, but its output
		// up to the flush of the resampler window must agree sample for sample
		const size_t valid = std::min<size_t>(ref.size(), static_cast<size_t>(static_cast<double>(x.size()) * 4096.0 / ratio));
		CHECK(valid > 2 * kSubframeSamples || ratio == 0x7FFF);
		CHECK(sameSamples(l, wantL, 0, valid > 3 ? valid - 3 : 0));
		CHECK(sameSamples(rr, wantR, 0, valid > 3 ? valid - 3 : 0));
	}
	// end of sample: `done` is set once the window ran dry and later subframes are silent
	Rig r;
	for (size_t i = 0; i < 100; ++i)
		putBe16(r, kAramBase + static_cast<u32>(i) * 2, 1000);
	DSPBuffer& v = r.voices[0];
	allocInit(v);
	setWave(v, 3, kAramBase, 100);
	mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
	playStart(v, 0x1000);
	std::vector<s16> l, rr;
	r.run(bit(0), 1, l, rr);
	CHECK(v.done == 0);
	r.run(bit(0), 1, l, rr);
	CHECK(v.endReached == 1 && v.done == 1);
	CHECK(l[kSubframeSamples + 40] == 0);
	std::vector<s16> l2, r2;
	r.run(bit(0), 1, l2, r2); // a voice that is done is not rendered any more
	for (s16 s : l2)
		CHECK(s == 0);
}

static void testPcm8AndLoop()
{
	section("PCM8 source, loop with restart address");
	{
		Rig r;
		std::vector<s16> x;
		for (int i = 0; i < 300; ++i) {
			const s8 b = static_cast<s8>((i * 37) % 200 - 100);
			r.aram[kAramBase + i] = static_cast<u8>(b);
			x.push_back(static_cast<s16>(b * 256));
		}
		DSPBuffer& v = r.voices[1];
		allocInit(v);
		setWave(v, 2, kAramBase, 300);
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, 0x1000);
		std::vector<s16> l, rr;
		r.run(bit(1), 2, l, rr);
		std::vector<s16> ref = refResample(x, 0x1000, 2 * kSubframeSamples);
		for (s16& s : ref)
			s = mix(s, 0x7FFF);
		CHECK(sameSamples(l, ref, 0, 2 * kSubframeSamples));
	}
	{
		Rig r;
		const std::vector<s16> x = pseudoSignal(100, 3);
		for (size_t i = 0; i < x.size(); ++i)
			putBe16(r, kAramBase + static_cast<u32>(i) * 2, x[i]);
		DSPBuffer& v = r.voices[2];
		allocInit(v);
		setWave(v, 3, kAramBase, 100, 1, 20, 80);
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, 0x1000);
		std::vector<s16> l, rr;
		r.run(bit(2), 4, l, rr);
		std::vector<s16> seq(x.begin(), x.begin() + 80); // 0..79 once, then 20..79 forever
		for (int k = 0; k < 20; ++k)
			seq.insert(seq.end(), x.begin() + 20, x.begin() + 80);
		std::vector<s16> ref = refResample(seq, 0x1000, 4 * kSubframeSamples);
		for (s16& s : ref)
			s = mix(s, 0x7FFF);
		CHECK(sameSamples(l, ref, 0, 4 * kSubframeSamples));
		CHECK(v.done == 0);
		CHECK(v.samplesBeforeLoop <= 60);
	}
}

static void testAdpcmVoice(const char* path)
{
	section("ADPCM voice through the mixer (decode, loop with history restore, 16-sample frame edges)");
	std::FILE* f = std::fopen(path, "rb");
	CHECK(f);
	char magic[4];
	u32 chunks, kind, n;
	CHECK(std::fread(magic, 1, 4, f) == 4 && std::fread(&chunks, 4, 1, f) == 1);
	CHECK(std::fread(&kind, 4, 1, f) == 1 && std::fread(&n, 4, 1, f) == 1);
	n = std::min<u32>(n, 60);
	std::vector<u8> data(n * 9);
	std::vector<s16> pcm(n * 16);
	CHECK(std::fread(data.data(), 1, data.size(), f) == data.size());
	std::fclose(f);
	{ // decoded with the library function: the first section proved it equal to the Python reference
		s16 y1 = 0, y2 = 0;
		for (u32 i = 0; i < n; ++i)
			hostdsp::adpcm4Frame(&data[i * 9], DSPADPCM_FILTER, &pcm[i * 16], y1, y2);
	}
	for (u32 ratio : { 0x1000u, 0x13A7u }) {
		Rig r;
		std::memcpy(&r.aram[kAramBase], data.data(), data.size());
		DSPBuffer& v = r.voices[7];
		allocInit(v);
		setWave(v, 0, kAramBase, static_cast<int>(n * 16));
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, static_cast<u16>(ratio));
		std::vector<s16> l, rr;
		r.run(bit(7), 3, l, rr);
		std::vector<s16> ref = refResample(pcm, ratio, 3 * kSubframeSamples);
		for (s16& s : ref)
			s = mix(s, 0x7FFF);
		CHECK(sameSamples(l, ref, 0, 3 * kSubframeSamples));
	}
	// loop frames [16, 48) with the history the wave file stores for the loop start
	{
		const int ls = 16, le = 48;
		Rig r;
		std::memcpy(&r.aram[kAramBase], data.data(), data.size());
		DSPBuffer& v = r.voices[8];
		allocInit(v);
		setWave(v, 0, kAramBase, static_cast<int>(n * 16), 1, ls, le, pcm[ls - 1], pcm[ls - 2]);
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, 0x1000);
		std::vector<s16> l, rr;
		r.run(bit(8), 6, l, rr);
		std::vector<s16> seq(pcm.begin(), pcm.begin() + le);
		for (int k = 0; k < 30; ++k)
			seq.insert(seq.end(), pcm.begin() + ls, pcm.begin() + le);
		std::vector<s16> ref = refResample(seq, 0x1000, 6 * kSubframeSamples);
		for (s16& s : ref)
			s = mix(s, 0x7FFF);
		CHECK(sameSamples(l, ref, 0, 6 * kSubframeSamples));
	}
}

static void testMixerStage()
{
	section("volume ramp, buses, master level, forced stop, pause");
	const std::vector<s16> x(2000, 12000); // constant: the signal level is the filter's DC gain
	auto build = [&](Rig& r, int vi, s16 volL, s16 volR) -> DSPBuffer& {
		for (size_t i = 0; i < x.size(); ++i)
			putBe16(r, kAramBase + static_cast<u32>(i) * 2, x[i]);
		DSPBuffer& v = r.voices[vi];
		allocInit(v);
		setWave(v, 3, kAramBase, static_cast<int>(x.size()));
		mixInit(v, 0, hostdsp::kBusLeft, volL);
		mixInit(v, 1, hostdsp::kBusRight, volR);
		playStart(v, 0x1000);
		return v;
	};
	const s16 dc = refResample(x, 0x1000, 50).back(); // steady-state output of the resampler for DC

	{ // ramp 0 -> 0x4000 (current/target as DSPBuffer::setMixerVolume leaves them), then steady
		Rig r;
		DSPBuffer& v = build(r, 3, 0, 0);
		v.mixChannels[0].targetVolume = 0x4000;
		std::vector<s16> l, rr;
		r.run(bit(3), 2, l, rr);
		CHECK(v.mixChannels[0].currentVolume == 0x4000);
		CHECK(l[kSubframeSamples + 79] == mix(dc, 0x4000) && l[kSubframeSamples + 10] == mix(dc, 0x4000));
		for (int i = 1; i < kSubframeSamples; ++i)
			CHECK(l[i] >= l[i - 1]);
		CHECK(l[0] > 0 && l[0] < 200 && l[79] > mix(dc, 0x4000) - 200);
		const int mid = l[39];
		CHECK(std::abs(mid - mix(dc, 0x4000) / 2) < 300); // linear
		for (s16 s : rr)
			CHECK(s == 0);
	}
	{ // two buses and the master level (4.12)
		Rig r;
		build(r, 4, 0x7FFF, 0x3FFF);
		std::vector<s16> l, rr, l2, r2;
		r.run(bit(4), 1, l, rr);
		CHECK(l[30] == mix(dc, 0x7FFF) && rr[30] == mix(dc, 0x3FFF));
		Rig q;
		build(q, 4, 0x7FFF, 0x3FFF);
		q.run(bit(4), 1, l2, r2, 0x2000); // 2.0
		CHECK(l2[30] == sat(static_cast<s64>(mix(dc, 0x7FFF)) * 2) && r2[30] == sat(static_cast<s64>(mix(dc, 0x3FFF)) * 2));
		l2.clear(), r2.clear();
		Rig z;
		build(z, 4, 0x7FFF, 0);
		z.run(bit(4), 1, l2, r2, 0);
		CHECK(l2[30] == 0);
	}
	{ // several voices add; saturation at the output
		Rig r;
		for (int vi = 0; vi < 6; ++vi) // 0.5 each: six voices saturate
			build(r, vi, 0x7FFF, 0);
		std::vector<s16> l, rr;
		r.run(bit(0) | bit(1) | bit(2) | bit(3) | bit(4) | bit(5), 1, l, rr);
		CHECK(l[30] == sat(6 * static_cast<s64>(mix(dc, 0x7FFF))));
		CHECK(l[30] == 32767);
	}
	{ // forced stop (endRequested): ramp to 0, then `done`; a voice outside the map is not rendered
		Rig r;
		DSPBuffer& v = build(r, 6, 0x7FFF, 0);
		std::vector<s16> l, rr;
		r.run(bit(6), 1, l, rr);
		CHECK(v.done == 0);
		v.endRequested = 1;
		r.run(bit(6), 1, l, rr);
		CHECK(v.done == 1);
		CHECK(l[kSubframeSamples + 79] == 0 && l[kSubframeSamples + 0] > 0);
		std::vector<s16> l2, r2;
		Rig q;
		DSPBuffer& w = build(q, 9, 0x7FFF, 0);
		q.run(0, 2, l2, r2);
		CHECK(w.currentPosition == 0 && l2[30] == 0);
	}
	{ // pause (useConstantSample): the position stays, the sample at the output position is held
		Rig r;
		DSPBuffer& v = build(r, 10, 0x7FFF, 0);
		std::vector<s16> l, rr;
		r.run(bit(10), 1, l, rr);
		const int pos = v.currentPosition;
		v.useConstantSample = 1;
		r.run(bit(10), 2, l, rr);
		CHECK(v.currentPosition == pos);
		CHECK(v.constantSample == dc); // latched at the pause: the constant signal
		for (int i = 0; i < 2 * kSubframeSamples; ++i)
			CHECK(l[kSubframeSamples + i] == mix(dc, 0x7FFF));
		v.useConstantSample = 0;
		r.run(bit(10), 1, l, rr);
		CHECK(v.currentPosition > pos && l[3 * kSubframeSamples + 30] == mix(dc, 0x7FFF));
	}
}

static void testAutoMixer()
{
	section("auto mixer (useDolbyVolume): integer quadrant law, hard pan right and its mirror");
	const std::vector<s16> x(2000, 12000);
	const s16 dc = refResample(x, 0x1000, 50).back();
	auto sine    = [](int i) -> s32 { // DSPRES_FILTER words 256-319, high half first
		const u32 w = DSPRES_FILTER[256 + i / 2];
		return static_cast<s16>((i & 1) ? w : w >> 16);
	};
	const s32 hard = ((sine(0x7F) * sine(0x7F)) >> 16) * 0x7FFF >> 16; // quadrant X=0x7F/Y=0, times the volume
	CHECK(sine(0) == 0 && sine(0x40) == 23170 && sine(0x7F) == 32765 && hard == 0x1FFE);
	for (int pan : { 0x7F, 0 }) {
		Rig r;
		for (size_t i = 0; i < x.size(); ++i)
			putBe16(r, kAramBase + static_cast<u32>(i) * 2, x[i]);
		DSPBuffer& v = r.voices[5];
		allocInit(v);
		setWave(v, 3, kAramBase, static_cast<int>(x.size()));
		playStart(v, 0x1000);
		v.useDolbyVolume     = 1; // DSPBuffer::setAutoMixer(0x7FFF, pan, 0, 0, ...), already at its target
		v.dolbyVoicePosition = static_cast<u16>(pan << 8);
		v.dolbyReverbFactor  = 0;
		v.dolbyVolumeCurrent = 0x7FFF;
		v.dolbyVolumeTarget  = 0x7FFF;
		std::vector<s16> l, rr;
		r.run(bit(5), 2, l, rr);
		const std::vector<s16>& on  = pan ? rr : l;
		const std::vector<s16>& off = pan ? l : rr;
		CHECK(on[30] == mix(dc, hard) && on[kSubframeSamples + 79] == mix(dc, hard)); // 0.125 of the source
		for (s16 s : off)
			CHECK(s == 0);
		CHECK(v.dolbyVolumeCurrent == 0x7FFF);
	}
}

static void testFxLine()
{
	section("effect line: delay of 2 subframes from bus 0x0DC0 to the left output");
	Rig r;
	const std::vector<s16> x(4000, 10000);
	for (size_t i = 0; i < x.size(); ++i)
		putBe16(r, kAramBase + static_cast<u32>(i) * 2, x[i]);
	DSPBuffer& v = r.voices[0];
	allocInit(v);
	setWave(v, 3, kAramBase, static_cast<int>(x.size()));
	mixInit(v, 0, 0x0DC0, 0x7FFF); // only the send bus: the dry path is silent
	playStart(v, 0x1000);
	std::vector<s16> delayBuf(2 * kSubframeSamples, 0x5555);
	FXBuffer& f = r.fx[0];
	f.unk0      = 1;
	f.unk2      = 2;
	f.unk4      = delayBuf.data();
	f.unk8      = 0x0DC0;
	f.unkA      = 0x1000; // 4.12: 1.0
	f.unkC      = hostdsp::kBusLeft;
	f.unkE      = 0x1000;
	std::fill(delayBuf.begin(), delayBuf.end(), 0); // setFXLine zeroes the buffer
	std::vector<s16> l, rr;
	r.run(bit(0), 5, l, rr);
	const s16 dc = refResample(x, 0x1000, 50).back();
	CHECK(l[10] == 0 && l[kSubframeSamples + 70] == 0);               // nothing yet in subframes 0 and 1
	// then the delayed send: voice volume 1.15 and the (untapped) FIR; the line gains are 4.12 = 1.0
	CHECK(std::abs(l[2 * kSubframeSamples + 40] - gain(mix(dc, 0x7FFF), 0x7FFF)) <= 3);
	CHECK(std::abs(l[4 * kSubframeSamples + 40] - l[2 * kSubframeSamples + 40]) <= 1);
	CHECK(rr[2 * kSubframeSamples + 40] == 0);
}

static void testStreamAndOsc()
{
	section("stream voice (type 0x21, 16.16 ring buffer in main RAM) and oscillator voice");
	{
		Rig r;
		std::vector<s16> ring = pseudoSignal(1000, 11);
		DSPBuffer& v          = r.voices[12];
		allocInit(v);
		v.baseAddress       = ring.data();
		v.loopAddress       = ring.data();
		v.isLooping         = 0;
		v.samplesSourceType = 0x21;
		v.remainingLength   = 1500;
		v.endPosition       = 1000u << 16;
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, 0x1000);
		std::vector<s16> l, rr;
		r.run(bit(12), 3, l, rr);
		std::vector<s16> seq;
		for (int k = 0; k < 3; ++k)
			seq.insert(seq.end(), ring.begin(), ring.end());
		std::vector<s16> ref = refResample(seq, 0x1000, 3 * kSubframeSamples);
		for (s16& s : ref)
			s = mix(s, 0x7FFF);
		CHECK(sameSamples(l, ref, 0, 3 * kSubframeSamples));
		const u32 fetched = 3 + 3 * kSubframeSamples; // window priming + one per output sample at ratio 1.0
		CHECK(static_cast<u32>(v.currentPosition) >> 16 == fetched % 1000);
		CHECK(v.samplesBeforeLoop >> 16 == 1000 - fetched % 1000);
		CHECK(v.remainingLength == 1500 - fetched);
		r.run(bit(12), 17, l, rr); // 1500 samples in total: the stream ends, the voice reports done
		CHECK(v.done == 1 && v.endReached == 1);
	}
	{
		Rig r;
		DSPBuffer& v = r.voices[13];
		allocInit(v);
		v.baseAddress       = nullptr; // DSPBuffer::setOscInfo
		v.samplesSourceType = 7;       // sine
		mixInit(v, 0, hostdsp::kBusLeft, 0x7FFF);
		playStart(v, 0x1000); // 64-sample table at one sample per output: period 64
		std::vector<s16> l, rr;
		r.run(bit(13), 2, l, rr);
		for (int i = 1; i + 64 < 2 * kSubframeSamples; ++i) // sample 0 still sees the primed zero before the table
			CHECK(l[i] == l[i + 64]);
		s16 peak = 0;
		for (s16 s : l)
			peak = std::max<s16>(peak, s);
		CHECK(peak > 14000 && peak < 16384); // table peak near full scale, 0x7FFF = 0.5
		CHECK(v.done == 0);
	}
}

int main(int argc, char** argv)
{
	CHECK(argc == 2); // fixture written by tools/dsp_ref.py
	testAdpcmFixture(argv[1]);
	testPcm16Pitch();
	testPcm8AndLoop();
	testAdpcmVoice(argv[1]);
	testMixerStage();
	testAutoMixer();
	testFxLine();
	testStreamAndOsc();
	std::fprintf(stderr, "dsp_test: ok\n");
	return 0;
}
