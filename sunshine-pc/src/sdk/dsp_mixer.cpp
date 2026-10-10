// Host DSP, part 2: the HLE mixer. See dsp_mixer.hpp for what the CPU side defines. Per voice and subframe:
//   source (ARAM wave / stream ring / oscillator) -> 4-tap polyphase resampler -> FIR/IIR -> 6 mixer channels
//   -> bus accumulators -> effect lines -> master level -> two s16 output buffers.
//
// Things the decomp cannot tell (the microcode is not part of it) are marked "approximation" below; they run silently.
// Things that are not implemented log once to stderr when a game hits them: "[dsp] ...".
#include "dsp_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace hostdsp {

using JASystem::DSPInterface::DSPBuffer;
using JASystem::DSPInterface::FXBuffer;

namespace {

constexpr u16 kBusAddr[kBusCount] = { 0x0D00, 0x0D60, 0x0DC0, 0x0E20, 0x0E80, 0x0EE0,
	                                  0x0CA0, 0x0F40, 0x0FA0, 0x0B00, 0x09A0 };
constexpr u16 kBusFxLeft          = 0x0DC0; // default mixer config 0x352 (bus 3) and 0x412 (bus 4): the effect sends
constexpr u16 kBusFxRight         = 0x0E20;

constexpr u16 kTypeDirectPcm = 0x21; // JAIGFrameStream Play_DirectPCM
constexpr u16 kTypeAfc4      = 9;    // DSPBuffer::setWaveInfo COMP_BLOCKBYTES: bytes per 16-sample frame / sample width
constexpr u16 kTypeAfc2      = 5;
constexpr u16 kTypePcm8      = 8;
constexpr u16 kTypePcm16     = 0x10;

// Offsets into DSPRES_FILTER (words): 64 phases x 4 taps of s16, then 64-sample oscillator tables.
constexpr int kResTapWords = 128;
constexpr int kOscSine     = 128; // word index, 64 samples = 32 words
constexpr int kOscTriangle = 160;

#define DSP_WARN_ONCE(...)                                                                                             \
	do {                                                                                                               \
		static bool warned_;                                                                                           \
		if (!warned_) {                                                                                                \
			warned_ = true;                                                                                            \
			std::fprintf(stderr, "[dsp] " __VA_ARGS__);                                                                \
			std::fputc('\n', stderr);                                                                                  \
		}                                                                                                              \
	} while (0)

inline s16 sat16(s64 v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : static_cast<s16>(v); }

inline uintptr_t addrOf(const void* p) { return reinterpret_cast<uintptr_t>(p); }

inline u32 posOf(const DSPBuffer& v) { return static_cast<u32>(v.currentPosition); }

// Sample `idx` of a 64-sample table stored as 32 words, high half first.
inline s16 tableSample(const u32* table, int idx)
{
	const u32 w = table[idx >> 1];
	return static_cast<s16>((idx & 1) ? w : w >> 16);
}

// Oscillator waveform sample (JASBankMgr::noteOnOsc: instrument 0xF0 + n selects waveform n, 64 samples per cycle).
// Sine and triangle come from DSPRES_FILTER; the others are synthesized. Amplitudes of the synthesized ones follow
// Dusklight's RenderOscChannel (approximation).
s16 oscSample(const u32* res, u16 type, u32 idx)
{
	idx &= 63;
	switch (type) {
	case 0: return idx < 32 ? 16383 : -16383;              // square 50 %
	case 1: return static_cast<s16>(idx << 10);            // saw
	case 3: return idx < 16 ? 16383 : -16383;              // square 25 %
	case 4: return tableSample(res + kOscTriangle, idx);   // triangle
	case 7:
	case 10: return tableSample(res + kOscSine, idx);      // sine (10 = variable-step sine, approximated as sine)
	default:
		DSP_WARN_ONCE("oscillator waveform %u is not implemented (silence)", static_cast<unsigned>(type));
		return 0;
	}
}

} // namespace

void Mixer::configure(const Config& c)
{
	cfg_ = c;
	std::memset(state_, 0, sizeof(state_));
	std::memset(fxState_, 0, sizeof(fxState_));
}

int Mixer::busSlot(u16 id) const
{
	for (int i = 0; i < kBusCount; ++i)
		if (kBusAddr[i] == id)
			return i;
	return -1;
}

Mixer::Kind Mixer::kindOf(const DSPBuffer& v) const
{
	if (v.samplesSourceType == kTypeDirectPcm)
		return Kind::DirectPcm;
	if (v.baseAddress == nullptr) // DSPBuffer::setOscInfo clears the address; ARAM starts at 0x4000
		return Kind::Osc;
	switch (v.samplesSourceType) {
	case kTypeAfc4: return Kind::Afc4;
	case kTypeAfc2: return Kind::Afc2;
	case kTypePcm8: return Kind::Pcm8;
	case kTypePcm16: return Kind::Pcm16;
	default: return Kind::Unknown;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Source
// ---------------------------------------------------------------------------------------------------------------------

s16 Mixer::endSource(DSPBuffer& v, VoiceState& st)
{
	if (!st.ended) {
		st.ended    = true;
		v.endReached = 1;
	}
	if (st.flushed < 255)
		++st.flushed;
	return 0;
}

// Handles the end of a sample: wraps to the loop start (restoring the ADPCM history the wave file stores for it) or
// ends the source. Returns false when the source ended.
bool Mixer::endOrLoop(DSPBuffer& v, VoiceState& st, u32& pos, bool adpcm)
{
	if (pos < v.endPosition)
		return true;
	if (!v.isLooping || static_cast<u32>(addrOf(v.loopAddress)) >= v.endPosition) {
		endSource(v, st);
		return false;
	}
	pos = static_cast<u32>(addrOf(v.loopAddress)); // loop start, in samples
	if (adpcm) {
		v.afcRemainingSamples[15]       = static_cast<u16>(v.loopYN1);
		v.afcRemainingSamples[14]       = static_cast<u16>(v.loopYN2);
		v.afcRemainingDecodedSamples    = 16; // decoded block consumed: decode the one holding the loop start next
	}
	return true;
}

// Next source sample of the voice (advances the position fields).
s16 Mixer::fetch(DSPBuffer& v, VoiceState& st, Kind kind)
{
	if (st.ended)
		return endSource(v, st);

	const u8* aram = cfg_.aram;
	const u32 base = static_cast<u32>(addrOf(v.baseAddress));
	u32 pos        = posOf(v);

	switch (kind) {
	case Kind::Osc:
		v.currentPosition = static_cast<int>(pos + 1);
		return oscSample(cfg_.resTable, v.samplesSourceType, pos);

	case Kind::DirectPcm: {
		// Stream ring buffer in main RAM: 16.16 positions, wraps at endPosition, remainingLength counts samples left.
		if (v.remainingLength == 0)
			return endSource(v, st);
		const s16 s = reinterpret_cast<const s16*>(addrOf(v.baseAddress))[pos >> 16];
		pos += 0x10000;
		if (pos >= v.endPosition)
			pos -= v.endPosition;
		v.currentPosition = static_cast<int>(pos);
		--v.remainingLength;
		return s;
	}

	case Kind::Afc4: {
		if (!endOrLoop(v, st, pos, true))
			return 0;
		if (v.afcRemainingDecodedSamples >= 16) {
			const size_t off = static_cast<size_t>(base) + static_cast<size_t>(pos / 16) * 9;
			if (!aram || off + 9 > cfg_.aramSize) {
				DSP_WARN_ONCE("ADPCM voice reads outside ARAM (address 0x%x); voice stopped", static_cast<unsigned>(off));
				return endSource(v, st);
			}
			s16 y1 = static_cast<s16>(v.afcRemainingSamples[15]);
			s16 y2 = static_cast<s16>(v.afcRemainingSamples[14]);
			s16 out[16];
			adpcm4Frame(aram + off, cfg_.adpcmTable, out, y1, y2);
			for (int i = 0; i < 16; ++i)
				v.afcRemainingSamples[i] = static_cast<u16>(out[i]);
			v.afcRemainingDecodedSamples = static_cast<s16>(pos % 16); // a loop start inside a frame skips its head
		}
		const s16 s       = static_cast<s16>(v.afcRemainingSamples[v.afcRemainingDecodedSamples++]);
		v.currentPosition = static_cast<int>(pos + 1);
		return s;
	}

	case Kind::Pcm8:
	case Kind::Pcm16: {
		if (!endOrLoop(v, st, pos, false))
			return 0;
		const bool wide   = kind == Kind::Pcm16;
		const size_t off  = static_cast<size_t>(base) + (wide ? static_cast<size_t>(pos) * 2 : pos);
		if (!aram || off + (wide ? 2 : 1) > cfg_.aramSize) {
			DSP_WARN_ONCE("PCM voice reads outside ARAM (address 0x%x); voice stopped", static_cast<unsigned>(off));
			return endSource(v, st);
		}
		v.currentPosition = static_cast<int>(pos + 1);
		return wide ? static_cast<s16>((aram[off] << 8) | aram[off + 1]) : static_cast<s16>(aram[off] << 8);
	}

	case Kind::Afc2:
		DSP_WARN_ONCE("2-bit ADPCM (wave format 1) is not implemented (silence)");
		return endSource(v, st);

	case Kind::Unknown:
	default:
		DSP_WARN_ONCE("voice with source type 0x%x is not implemented (silence)", static_cast<unsigned>(v.samplesSourceType));
		return endSource(v, st);
	}
}

// A voice starts (DSPBuffer::playStart sets resetVpb). The resampler window holds the last 4 source samples x[k-1..k+2]
// around the output position k; priming it with 3 samples makes the first output sample x[0] (x[-1] = 0).
void Mixer::resetVoice(DSPBuffer& v, VoiceState& st, Kind kind)
{
	v.resetVpb = 0;
	st         = VoiceState();
	if (kind == Kind::Afc4) { // the CPU already did this (setWaveInfo); the DSP must not trust a reused block
		std::memset(v.afcRemainingSamples, 0, sizeof(v.afcRemainingSamples));
		v.afcRemainingDecodedSamples = 16;
	}
	std::memset(v.resampleBuffer, 0, sizeof(v.resampleBuffer));
	for (int i = 0; i < 3; ++i) {
		v.resampleBuffer[0] = v.resampleBuffer[1];
		v.resampleBuffer[1] = v.resampleBuffer[2];
		v.resampleBuffer[2] = v.resampleBuffer[3];
		v.resampleBuffer[3] = fetch(v, st, kind);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Voice
// ---------------------------------------------------------------------------------------------------------------------

void Mixer::filterVoice(DSPBuffer& v, s32* mono)
{
	// filterMode (DSPBuffer::setFilterMode): bit 5 = IIR on, bits 0-4 = FIR on (nonzero). The microcode is not part of
	// the decomp: FIR is 8 taps at unit spacing with 1.15 coefficients, IIR is Dusklight's DuskDsp.cpp biquad
	// out[n] = (b1 in[n-1] + b2 in[n-2] + a1 out[n-1] + a2 out[n-2]) >> 15 with biquadFilterCoeffs = {b1, b2, a1, a2}.
	// (approximation; the distance low-pass lowPassCoeff is reported below when a game uses it)
	if (v.filterMode & 0x1f) {
		s16* hist = reinterpret_cast<s16*>(v.variableFirHistory); // x[n-1] .. x[n-7]
		for (int i = 0; i < kSubframeSamples; ++i) {
			const s32 x = mono[i];
			s32 acc     = x * v.variableFirCoeffs[0];
			for (int k = 1; k < 8; ++k)
				acc += hist[k - 1] * v.variableFirCoeffs[k];
			for (int k = 6; k > 0; --k)
				hist[k] = hist[k - 1];
			hist[0] = clamp16(x);
			mono[i] = clamp16(acc >> 15);
		}
	}
	if (v.filterMode & 0x20) {
		s16* h = v.biquadHistory; // in[n-1], in[n-2], out[n-1], out[n-2]
		for (int i = 0; i < kSubframeSamples; ++i) {
			const s32 in  = mono[i];
			const s32 out = clamp16((v.biquadFilterCoeffs[0] * h[0] + v.biquadFilterCoeffs[1] * h[1]
			                         + v.biquadFilterCoeffs[2] * h[2] + v.biquadFilterCoeffs[3] * h[3])
			                        >> 15);
			h[1]    = h[0];
			h[0]    = clamp16(in);
			h[3]    = h[2];
			h[2]    = static_cast<s16>(out);
			mono[i] = out;
		}
	}
	if (v.lowPassCoeff != 0)
		DSP_WARN_ONCE("distance low-pass filter (setDistFilter) is not implemented");
}

// Adds the voice to its buses. Each mixer channel ramps linearly from its current to its target volume over the
// subframe (1.15 fixed point) and the target becomes the current volume, as DSPBuffer::setMixerVolume expects.
void Mixer::mixVoice(DSPBuffer& v, const s32* mono)
{
	auto addToBus = [&](int slot, s32 from, s32 to) {
		if (slot < 0 || (from == 0 && to == 0))
			return;
		s32* bus       = bus_[slot];
		const s64 span = static_cast<s64>(to - from) << 16;
		for (int i = 0; i < kSubframeSamples; ++i) { // exact at the end: the last sample is at `to`
			const s64 gain = (static_cast<s64>(from) << 16) + span * (i + 1) / kSubframeSamples;
			bus[i] += static_cast<s32>((mono[i] * gain) >> 31);
		}
	};

	if (v.useDolbyVolume) {
		// Auto mixer (the sequence selected mixer config 0xFFFF): the DSP derives the buses itself from volume, pan,
		// effect send and surround. approximation: equal-power pan between the two outputs, the effect send feeds the
		// default send buses 0x0DC0/0x0E20 with the same law; surround (dolbyVoicePosition low byte) is reported.
		const s32 from = static_cast<s16>(v.dolbyVolumeCurrent);
		const s32 to   = v.endRequested ? 0 : static_cast<s16>(v.dolbyVolumeTarget);
		const double p = std::min(127, v.dolbyVoicePosition >> 8) / 127.0;
		const double f = std::min(127, v.dolbyReverbFactor >> 8) / 127.0;
		const double hp = 1.5707963267948966;
		const double gl = std::cos(p * hp), gr = std::sin(p * hp), gf = std::sin(f * hp);
		auto scaled = [](s32 x, double g) { return static_cast<s32>(x * g); };
		addToBus(0, scaled(from, gl), scaled(to, gl));
		addToBus(1, scaled(from, gr), scaled(to, gr));
		addToBus(busSlot(kBusFxLeft), scaled(from, gl * gf), scaled(to, gl * gf));
		addToBus(busSlot(kBusFxRight), scaled(from, gr * gf), scaled(to, gr * gf));
		if ((v.dolbyVoicePosition & 0xFF) != 0)
			DSP_WARN_ONCE("surround (dolby) position of the auto mixer is not implemented");
		v.dolbyVolumeCurrent = static_cast<u16>(to);
		return;
	}

	for (int k = 0; k < 6; ++k) {
		auto& c        = v.mixChannels[k];
		const s32 from = static_cast<s16>(c.currentVolume);
		const s32 to   = v.endRequested ? 0 : static_cast<s16>(c.targetVolume);
		if (c.id != 0)
			addToBus(busSlot(c.id), from, to);
		c.currentVolume = static_cast<u16>(to);
		if ((c.id == 0x0F40 || c.id == 0x0FA0) && (from != 0 || to != 0))
			DSP_WARN_ONCE("voice send to the surround buses 8/9 (Dolby output mode) is not implemented (dropped)");
		// Per-mixer-channel delay (the high byte of `level`, kept in step with samplesToKeepCount by
		// setMixerInitDelayMax) is not implemented.
		if ((c.level >> 8) != 0 && v.samplesToKeepCount != 0)
			DSP_WARN_ONCE("per-mixer-channel delay (level/samplesToKeepCount) is not implemented");
	}
}

void Mixer::renderVoice(int index, DSPBuffer& v)
{
	VoiceState& st  = state_[index];
	const Kind kind = kindOf(v);
	if (v.resetVpb)
		resetVoice(v, st, kind);

	// Source through the 4-tap polyphase resampler: 64 phases of 4 s16 taps (1.15), over the window x[k-1..k+2].
	s32 mono[kSubframeSamples];
	if (v.useConstantSample) {
		// Paused (DSPBuffer::setPauseFlag): hold one value and do not advance. The CPU resets constantSample to 0 at
		// playStart; the DSP latches the last output sample when the pause starts so that it does not click
		// (approximation: the microcode is not part of the decomp).
		if (!st.paused) {
			st.paused         = true;
			v.constantSample = st.last;
		}
		const s32 c = v.constantSample;
		for (int i = 0; i < kSubframeSamples; ++i)
			mono[i] = c;
	} else {
		st.paused = false;
		s16* w            = v.resampleBuffer;
		const u32 stepFix = static_cast<u32>(v.resamplingRatio) << 4; // 4.12 -> 16.16
		u32 frac          = static_cast<u16>(v.currentPosFrac);
		for (int i = 0; i < kSubframeSamples; ++i) {
			const u32* t  = cfg_.resTable + (frac >> 10) * 2;
			const s32 acc = w[0] * static_cast<s16>(t[0] >> 16) + w[1] * static_cast<s16>(t[0])
			                + w[2] * static_cast<s16>(t[1] >> 16) + w[3] * static_cast<s16>(t[1]);
			mono[i] = clamp16(acc >> 15);
			frac += stepFix;
			for (u32 n = frac >> 16; n != 0; --n) {
				w[0] = w[1];
				w[1] = w[2];
				w[2] = w[3];
				w[3] = fetch(v, st, kind);
			}
			frac &= 0xFFFF;
		}
		v.currentPosFrac = static_cast<s16>(frac);
		st.last          = static_cast<s16>(mono[kSubframeSamples - 1]);
		v.samplesBeforeLoop = v.endPosition > posOf(v) ? v.endPosition - posOf(v) : 0;
	}

	filterVoice(v, mono);
	mixVoice(v, mono);

	// The voice is over when the end of its source has left the resampler window, or when the CPU asked for the end
	// (DSPBuffer::forceStop path: the volumes above ramped to 0). The CPU then answers `done` (replyFinishRequest).
	if ((st.ended && st.flushed >= 4) || v.endRequested)
		v.done = 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Effect lines and output
// ---------------------------------------------------------------------------------------------------------------------

// FXBuffer (JASDSPInterface.cpp setFXLine): unk0 = mode (0 = off), unk2 = delay in subframes (the buffer holds
// unk2 * 80 s16), unk4 = delay buffer in main RAM, unk8 = source bus, unkA = gain on the source side, unkC = destination
// bus, unkE = gain on the destination side, unk10[8] = FIR taps (1.15). The microcode is not part of the decomp;
// approximation: the line stores the source bus (times unkA) into the delay buffer and adds the block written unk2
// subframes ago, run through the 8-tap FIR and times unkE, to the destination bus. The gains are 4.12 (the game's own
// scene data holds 4096 and 2047 = 1.0 and 0.5). That data (mSound.aaf, 2 scenes) reads the main buses 0x0D00/0x0D60 and
// delays them by 48 subframes (120 ms) into bus 0x0F40 (bus 8): surround material that the stereo output stage ignores.
void Mixer::runFx()
{
	for (int line = 0; line < kFxLineCount; ++line) {
		FXBuffer& f  = cfg_.fx[line];
		FxState& st  = fxState_[line];
		const int sm = busSlot(f.unk8);
		const int dm = busSlot(f.unkC);
		if (f.unk0 == 0 || f.unk4 == nullptr || f.unk2 == 0 || sm < 0 || dm < 0)
			continue;
		if (st.buffer != f.unk4 || st.blocks != f.unk2) { // setFXLine zeroes the buffer; start over
			st.buffer = f.unk4;
			st.blocks = f.unk2;
			st.write  = 0;
		}
		s16* block = f.unk4 + static_cast<size_t>(st.write) * kSubframeSamples; // oldest block = the one to overwrite
		bool anyTap = false;
		for (int k = 0; k < 8; ++k)
			anyTap |= f.unk10[k] != 0;
		s32 dst[kSubframeSamples];
		for (int i = 0; i < kSubframeSamples; ++i) { // delayed signal through the FIR (all-zero taps: no filter)
			s32 acc = block[i] * 0x7FFF;
			if (anyTap) {
				acc = 0;
				for (int k = 0; k < 8 && i - k >= 0; ++k)
					acc += block[i - k] * f.unk10[k];
			}
			dst[i] = (acc >> 15) * f.unkE >> 12;
		}
		for (int i = 0; i < kSubframeSamples; ++i) {
			const s32 in = clamp16(bus_[sm][i]);
			block[i]     = clamp16(in * f.unkA >> 12);
			bus_[dm][i] += dst[i];
		}
		st.write = (st.write + 1) % st.blocks;
	}
}

void Mixer::render(u64 voiceMap, u16 masterLevel, s16* left, s16* right)
{
	std::memset(bus_, 0, sizeof(bus_));
	std::memset(left, 0, kSubframeSamples * sizeof(s16));
	std::memset(right, 0, kSubframeSamples * sizeof(s16));
	if (!cfg_.voices || !cfg_.resTable || !cfg_.adpcmTable)
		return;
	for (int i = 0; i < kVoiceCount; ++i)
		if (((voiceMap >> (63 - i)) & 1) && !cfg_.voices[i].done)
			renderVoice(i, cfg_.voices[i]);
	if (cfg_.fx)
		runFx();

	// Output stage: master level (4.12), saturate. Only the two main buses reach the output; the surround buses 0x0F40/
	// 0x0FA0 (bus 8/9) are where the effect lines write and where surround-mode voices send, and the Dolby encode the
	// microcode would do with them is not implemented (see mixVoice).
	for (int i = 0; i < kSubframeSamples; ++i) {
		left[i]  = sat16((static_cast<s64>(bus_[0][i]) * masterLevel) >> 12);
		right[i] = sat16((static_cast<s64>(bus_[1][i]) * masterLevel) >> 12);
	}
}

} // namespace hostdsp
