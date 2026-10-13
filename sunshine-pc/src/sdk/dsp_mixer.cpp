// Host DSP, part 2: the HLE mixer. See dsp_mixer.hpp for what the CPU side defines. Per voice and subframe:
//   source (ARAM wave / stream ring / oscillator) -> 4-tap polyphase resampler -> FIR/IIR -> 6 mixer channels
//   -> bus accumulators (+ effect-line returns) -> output matrix -> master level -> two s16 output buffers.
//
// Things the decomp cannot tell (the microcode is not part of it) are marked "approximation" below; they run silently.
// Things that are not implemented log once to stderr when a game hits them: "[dsp] ...".
#include "dsp_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hostdsp {

using JASystem::DSPInterface::DSPBuffer;
using JASystem::DSPInterface::FXBuffer;

// Launcher master volume, percent 0..100 (src/settings.h audio.master_volume).
int gOutputVolumePct = 100;

// DIAG (temporary; remove when the audio investigation closes): SUNSHINE_DSP_VOICE_LOG=1 prints the RMS and zero
// crossings of every voice once per second of output (400 subframes).
struct VoiceDiag {
	double sq = 0;
	long n    = 0;
	long zc   = 0;
	s32 last  = 0;
};
VoiceDiag gDiag[kVoiceCount];
long gDiagSubframes = 0;
const bool gDiagOn  = std::getenv("SUNSHINE_DSP_VOICE_LOG") != nullptr;
long gDiagClip = 0; // DIAG: output samples saturated by sat16, per second
s64 gDiagPeak  = 0; // DIAG: largest |bus| sample, per second
// DIAG (temporary): SUNSHINE_DSP_SOLO=<voice> renders only that voice, so its output can be judged alone.
const int gDiagSolo = std::getenv("SUNSHINE_DSP_SOLO") != nullptr ? std::atoi(std::getenv("SUNSHINE_DSP_SOLO")) : -1;

namespace {

// Slot 2 + k is effect line k's send bus (0x0DC0 + 0x60 k, jdsp[] 0x0172). 0x0C00/0x0C50 are only reached by the
// auto mixer's back effect sends (IRAM table 0x0dd6).
constexpr u16 kBusAddr[kBusCount] = { 0x0D00, 0x0D60, 0x0DC0, 0x0E20, 0x0E80, 0x0EE0, 0x0CA0,
	                                  0x0F40, 0x0FA0, 0x0B00, 0x09A0, 0x0C00, 0x0C50 };
constexpr int slotOf(u16 id)
{
	for (int i = 0; i < kBusCount; ++i)
		if (kBusAddr[i] == id)
			return i;
	return -1;
}
constexpr int kSlotSend0        = slotOf(0x0DC0); // + line
constexpr int kSlotRevLeft      = slotOf(0x0E80); // send bus of line 2
constexpr int kSlotRevRight     = slotOf(0x0EE0); // send bus of line 3
constexpr int kSlotBackLeft     = slotOf(0x0F40);
constexpr int kSlotBackRight    = slotOf(0x0CA0);
constexpr int kSlotCentre       = slotOf(0x09A0);
constexpr int kSlotMinusRight   = slotOf(0x0FA0);
constexpr int kSlotRevBackLeft  = slotOf(0x0C00);
constexpr int kSlotRevBackRight = slotOf(0x0C50);
// The auto mixer's 8 destinations (IRAM table 0x0dd6): dry FL FR BL BR, then the same quadrants as effect sends.
constexpr u16 kAutoDest[8] = { 0x0D00, 0x0D60, 0x0F40, 0x0CA0, 0x0E80, 0x0EE0, 0x0C00, 0x0C50 };

// Q15 volumes of the ucode's output matrix (0x00ef-0x0113, 0x03f4-0x042b).
constexpr s16 kQ15One   = 0x7FFF;
constexpr s16 kQ15Cross = static_cast<s16>(0xB820); // -0.5615 (-5.0 dB)
constexpr s16 kQ15Mid   = 0x5A82;                   // 0.7071
constexpr s16 kQ15Minus = static_cast<s16>(0x8000); // -1.0

constexpr u16 kTypeDirectPcm = 0x21; // JAIGFrameStream Play_DirectPCM
constexpr u16 kTypeAfc4      = 9;    // DSPBuffer::setWaveInfo COMP_BLOCKBYTES: bytes per 16-sample frame / sample width
constexpr u16 kTypeAfc2      = 5;
constexpr u16 kTypePcm8      = 8;
constexpr u16 kTypePcm16     = 0x10;

// Offsets into DSPRES_FILTER (words): 64 phases x 4 taps of s16, then 64-sample oscillator tables.
constexpr int kResTapWords = 128;
constexpr int kOscSine     = 128; // word index, 64 samples = 32 words
constexpr int kOscTriangle = 160;
constexpr int kDolbySine   = 256; // word index, 128 samples: quarter sine, sine[i] = 32768 sin(i pi/256) within 1

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

// The ucode's fixed buffer mixes (jdsp[] 0x00d8 add, 0x00e7 copy, FIR 0x0bd7) multiply in M2 (product doubled), round
// the product at bit 16 half to even (MULMVZ/MOVPZ) and store 16 bits in SET16 mode: they wrap, they do not saturate.
inline s32 roundQ15(s64 prod2) { return static_cast<s32>(((prod2 & 0x10000) ? prod2 + 0x8000 : prod2 + 0x7FFF) >> 16); }
inline s32 wrap16(s64 v) { return static_cast<s16>(static_cast<u16>(v)); }

void mixQ15(s32* dst, const s32* src, int n, s16 vol, bool copy = false)
{
	for (int i = 0; i < n; ++i)
		dst[i] = wrap16((copy ? 0 : dst[i]) + roundQ15(2 * static_cast<s64>(src[i]) * vol));
}

// 0x0bd7: 8-tap FIR in place over an effect line's 88-sample window, tap 7 on the newest sample.
void firQ15(s32* u, const s16* taps)
{
	for (int i = 0; i < kSubframeSamples; ++i) {
		s64 acc = 0;
		for (int j = 0; j < 8; ++j)
			acc += static_cast<s64>(u[i + j]) * taps[j];
		u[i] = wrap16(roundQ15(2 * acc));
	}
}

inline uintptr_t addrOf(const void* p) { return reinterpret_cast<uintptr_t>(p); }

inline u32 posOf(const DSPBuffer& v) { return static_cast<u32>(v.currentPosition); }

// Sample `idx` of a table of s16 stored two per word, high half first.
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
	std::memset(bus_, 0, sizeof(bus_)); // the back buses carry into the next subframe (render)
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
// subframe and the target becomes the current volume, as DSPBuffer::setMixerVolume expects. The add is
// bus += (vol * src) >> 16, so 0x7FFF is 0.5: the microcode's voice mixer (jdsp[] 0x0d0a, both callers 0x03b2 and
// 0x0db6) multiplies in M0, i.e. without the DSP's product doubling.
void Mixer::mixVoice(DSPBuffer& v, const s32* mono)
{
	auto addToBus = [&](int slot, s32 from, s32 to) {
		if (slot < 0 || (from == 0 && to == 0))
			return;
		s32* bus       = bus_[slot];
		const s64 span = static_cast<s64>(to - from) << 16;
		for (int i = 0; i < kSubframeSamples; ++i) { // exact at the end: the last sample is at `to`
			const s64 gain = (static_cast<s64>(from) << 16) + span * (i + 1) / kSubframeSamples;
			bus[i] += static_cast<s32>((mono[i] * gain) >> 32);
		}
	};

	if (v.useDolbyVolume) {
		// Auto mixer (the sequence selected mixer config 0xFFFF): the DSP derives the buses itself from volume, pan X
		// (dolbyVoicePosition high byte), surround Y (low byte) and effect send. Quadrant law of the microcode (jdsp[]
		// 0x0d6b-0x0d70, as in Dolphin's Zelda HLE): quadrant = sine[X^0x7F or X] * sine[Y^0x7F or Y] >> 16 from
		// DSPRES_FILTER (FL, FR, BL, BR), times the s16 volume >> 16, then the voice mixer above: a hard-panned voice at
		// 0x7FFF adds 0x1FFE / 65536 = 0.125 of its signal. The CPU does not clamp this volume (JASChannel.cpp: volume *
		// 32767.5). The 8 destinations are IRAM table 0x0dd6 (kAutoDest): the back quadrants go to 0x0F40/0x0CA0 and the
		// effect sends to 0x0E80/0x0EE0/0x0C00/0x0C50. approximation: the effect send is that HLE's (dry *
		// dolbyReverbFactor) >> 16, while the ucode (0x0d7f-0x0d85) scales it by the current volume once more, so its send
		// falls faster below full volume.
		const int x = (v.dolbyVoicePosition >> 8) & 0x7F, y = v.dolbyVoicePosition & 0x7F;
		const u32* sine = cfg_.resTable + kDolbySine;
		auto m16        = [](s32 a, s32 b) { return (a * b) >> 16; };
		const s32 left = tableSample(sine, x ^ 0x7F), right = tableSample(sine, x);
		const s32 front = tableSample(sine, y ^ 0x7F), back = tableSample(sine, y);
		const s32 q[4]  = { m16(left, front), m16(right, front), m16(left, back), m16(right, back) };
		const s32 from  = static_cast<s16>(v.dolbyVolumeCurrent);
		const s32 to    = v.endRequested ? 0 : static_cast<s16>(v.dolbyVolumeTarget);
		const s32 rf    = static_cast<s16>(v.dolbyReverbFactor);
		for (int k = 0; k < 4; ++k) {
			addToBus(slotOf(kAutoDest[k]), m16(q[k], from), m16(q[k], to));
			addToBus(slotOf(kAutoDest[4 + k]), m16(m16(q[k], from), rf), m16(m16(q[k], to), rf));
		}
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
	if (gDiagOn)
		for (int i = 0; i < kSubframeSamples; ++i) {
			VoiceDiag& d = gDiag[index];
			d.sq += static_cast<double>(mono[i]) * mono[i];
			d.n++;
			if ((mono[i] < 0) != (d.last < 0))
				d.zc++;
			d.last = mono[i];
		}

	// The voice is over when the end of its source has left the resampler window, or when the CPU asked for the end
	// (DSPBuffer::forceStop path: the volumes above ramped to 0). The CPU then answers `done` (replyFinishRequest).
	if ((st.ended && st.flushed >= 4) || v.endRequested)
		v.done = 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Effect lines and output
// ---------------------------------------------------------------------------------------------------------------------

// FXBuffer (JASDSPInterface.cpp setFXLine) is the ucode's 16-word effect block (jdsp[] 0x0172, copied to 0x0390):
// unk0 = on (bit 0: FIR before the returns, bit 1: after), unk2 = delay in subframes (the buffer holds unk2 * 80 s16),
// unk4 = delay buffer, unk8/unkA = destination 1 and its Q15 volume, unkC/unkE = destination 2 and its Q15 volume
// (0x014b-0x0163), unk10[8] = FIR taps (Q15). Line k reads its own send bus 0x0DC0 + 0x60 k: the block written unk2
// subframes ago, behind the last 8 samples of the previous block (0x0190, 0x012d). That window, filtered, is added to
// both destinations and becomes the send bus, so the voices' sends add to it and fxStore writes it back: the line feeds
// back through its FIR. The game's scenes (mSound.aaf) return to a main bus and to 0x0F40.
void Mixer::fxReturn(const s32* revBackLeft, const s32* revBackRight)
{
	for (int line = 0; line < kFxLineCount; ++line) {
		FXBuffer& f = cfg_.fx[line];
		FxState& st = fxState_[line];
		st.active   = f.unk0 != 0 && f.unk4 != nullptr && f.unk2 != 0;
		if (!st.active)
			continue;
		if (st.buffer != f.unk4 || st.blocks != f.unk2) { // setFXLine zeroes the buffer; start over
			st        = FxState();
			st.buffer = f.unk4;
			st.blocks = f.unk2;
			st.active = true;
		}
		s32 u[kSubframeSamples + 8];
		const s16* block = st.buffer + static_cast<size_t>(st.write) * kSubframeSamples;
		for (int i = 0; i < 8; ++i)
			u[i] = st.hist[i];
		for (int i = 0; i < kSubframeSamples; ++i)
			u[8 + i] = block[i];
		for (int i = 0; i < 8; ++i)
			st.hist[i] = static_cast<s16>(u[kSubframeSamples + i]);
		if (f.unk0 & 1)
			firQ15(u, f.unk10);
		const u16 dest[2] = { f.unk8, f.unkC };
		const s16 vol[2]  = { f.unkA, f.unkE };
		for (int d = 0; d < 2; ++d) {
			const int slot = busSlot(dest[d]);
			if (dest[d] != 0 && slot < 0)
				DSP_WARN_ONCE("effect line returns to unknown buffer 0x%x (dropped)", static_cast<unsigned>(dest[d]));
			if (slot >= 0)
				mixQ15(bus_[slot], u, kSubframeSamples, vol[d]);
		}
		if (f.unk0 & 2)
			firQ15(u, f.unk10);
		std::copy(u, u + kSubframeSamples, bus_[kSlotSend0 + line]);
	}
	// 0x01a6-0x01c8: the effect back buses feed lines 2/3 like the output matrix: 0x0C00 one subframe late, the second
	// half of 0x0C50 40 samples late (its first half follows in fxStore).
	mixQ15(bus_[kSlotRevLeft], revBackLeft, kSubframeSamples, kQ15One);
	mixQ15(bus_[kSlotRevRight], revBackLeft, kSubframeSamples, kQ15Cross);
	mixQ15(bus_[kSlotRevLeft], revBackRight + 40, 40, kQ15Cross);
	mixQ15(bus_[kSlotRevRight], revBackRight + 40, 40, kQ15One);
}

// End of the subframe (0x01f0-0x0202, then 0x0204 -> 0x01d6): first half of 0x0C50 into the second half of lines 2/3,
// then every line stores its send bus as the newest block.
void Mixer::fxStore()
{
	mixQ15(bus_[kSlotRevLeft] + 40, bus_[kSlotRevBackRight], 40, kQ15Cross);
	mixQ15(bus_[kSlotRevRight] + 40, bus_[kSlotRevBackRight], 40, kQ15One);
	for (int line = 0; line < kFxLineCount; ++line) {
		FxState& st = fxState_[line];
		if (!st.active)
			continue;
		s16* block = st.buffer + static_cast<size_t>(st.write) * kSubframeSamples;
		for (int i = 0; i < kSubframeSamples; ++i)
			block[i] = static_cast<s16>(bus_[kSlotSend0 + line][i]);
		st.write = (st.write + 1) % st.blocks;
	}
}

void Mixer::render(u64 voiceMap, u16 masterLevel, s16* left, s16* right)
{
	s32 last[4][kSubframeSamples]; // the previous subframe's back buses, read before they are cleared (0x0115, 0x01ca)
	const int lastSlot[4] = { kSlotBackLeft, kSlotBackRight, kSlotRevBackLeft, kSlotRevBackRight };
	for (int k = 0; k < 4; ++k)
		std::memcpy(last[k], bus_[lastSlot[k]], sizeof(last[k]));
	std::memset(bus_, 0, sizeof(bus_));
	std::memset(left, 0, kSubframeSamples * sizeof(s16));
	std::memset(right, 0, kSubframeSamples * sizeof(s16));
	if (!cfg_.voices || !cfg_.resTable || !cfg_.adpcmTable)
		return;

	// Output matrix, start of the subframe (0x00ef-0x0113): the main buses start as the previous subframe's back left
	// (L 1.0, R -0.5615) plus the second half of its back right (L -0.5615, R 1.0): back left is 80, back right 40
	// samples late.
	s32* L = bus_[0];
	s32* R = bus_[1];
	mixQ15(L, last[0], kSubframeSamples, kQ15One, true);
	mixQ15(R, last[0], kSubframeSamples, kQ15Cross, true);
	mixQ15(L, last[1] + 40, 40, kQ15Cross);
	mixQ15(R, last[1] + 40, 40, kQ15One);
	if (cfg_.fx)
		fxReturn(last[2], last[3]);
	for (int i = 0; i < kVoiceCount; ++i)
		if (((voiceMap >> (63 - i)) & 1) && !cfg_.voices[i].done && (gDiagSolo < 0 || i == gDiagSolo))
			renderVoice(i, cfg_.voices[i]);
	for (auto& bus : bus_) // the voice adds saturate (SET40, 0x0386 and 0x0d44); one clamp after all voices stands in
		for (s32& s : bus)
			s = clamp16(s);

	// Output matrix, after the voices (0x03f4-0x0416, 0x0424): centre 0x09A0 at 0.7071 into both, the first half of
	// back right into the second half, and 0x0FA0 subtracted from the right bus only.
	mixQ15(L, bus_[kSlotCentre], kSubframeSamples, kQ15Mid);
	mixQ15(R, bus_[kSlotCentre], kSubframeSamples, kQ15Mid);
	mixQ15(L + 40, bus_[kSlotBackRight], 40, kQ15Cross);
	mixQ15(R + 40, bus_[kSlotBackRight], 40, kQ15One);
	mixQ15(R, bus_[kSlotMinusRight], kSubframeSamples, kQ15Minus);

	// Output stage (0x0d22): master level (4.12), saturate.
	const s64 level = (static_cast<s64>(masterLevel) * gOutputVolumePct) / 100;
	for (int i = 0; i < kSubframeSamples; ++i) {
		const s64 l = (static_cast<s64>(bus_[0][i]) * level) >> 12;
		const s64 r = (static_cast<s64>(bus_[1][i]) * level) >> 12;
		left[i]     = sat16(l);
		right[i]    = sat16(r);
		if (gDiagOn) { // DIAG
			gDiagClip += (l != left[i]) + (r != right[i]);
			gDiagPeak = std::max({gDiagPeak, std::abs(static_cast<s64>(bus_[0][i])), std::abs(static_cast<s64>(bus_[1][i]))});
		}
	}
	if (cfg_.fx)
		fxStore();
	if (gDiagOn && ++gDiagSubframes % 400 == 0) {
		for (int i = 0; i < kVoiceCount; ++i) {
			VoiceDiag& d = gDiag[i];
			if (d.n == 0)
				continue;
			const DSPBuffer& v = cfg_.voices[i];
			const double rms   = std::sqrt(d.sq / static_cast<double>(d.n));
			double gain        = 0;
			unsigned bus       = 0;
			for (const auto& c : v.mixChannels)
				if (c.id) {
					gain += static_cast<s16>(c.targetVolume) / 32768.0;
					bus = bus ? bus : c.id;
				}
			if (v.useDolbyVolume)
				gain = static_cast<s16>(v.dolbyVolumeTarget) / 32768.0;
			const Kind k = kindOf(v);
			const char* kn = k == Kind::Osc ? "osc" : k == Kind::DirectPcm ? "dpcm" : k == Kind::Afc4 ? "afc4"
			                 : k == Kind::Afc2 ? "afc2" : k == Kind::Pcm8 ? "pcm8" : k == Kind::Pcm16 ? "pcm16" : "unk";
			std::fprintf(stderr,
			             "[dsp-voice] t=%lds v=%d kind=%s type=0x%x base=0x%llx pos=%d end=%u loop=%d la=%llu ended=%d req=%d "
			             "ratio=%u flt=0x%x iir=%d,%d,%d,%d lp=%d bus=0x%x gain=%.3f rms=%.0f post=%.0f zc=%ld\n",
			             gDiagSubframes / 400, i, kn, static_cast<unsigned>(v.samplesSourceType),
			             static_cast<unsigned long long>(addrOf(v.baseAddress)), v.currentPosition,
			             static_cast<unsigned>(v.endPosition), v.isLooping ? 1 : 0,
			             static_cast<unsigned long long>(addrOf(v.loopAddress)), state_[i].ended ? 1 : 0,
			             v.endRequested ? 1 : 0, static_cast<unsigned>(v.resamplingRatio),
			             static_cast<unsigned>(static_cast<u16>(v.filterMode)), v.biquadFilterCoeffs[0],
			             v.biquadFilterCoeffs[1], v.biquadFilterCoeffs[2], v.biquadFilterCoeffs[3], v.lowPassCoeff,
			             bus, gain, rms, rms * gain, d.zc);
			d = VoiceDiag{};
		}
		std::fprintf(stderr, "[dsp-out] t=%lds level=%lld clip=%ld peak=%lld\n", gDiagSubframes / 400,
		             static_cast<long long>(level), gDiagClip, static_cast<long long>(gDiagPeak));
		gDiagClip = 0;
		gDiagPeak = 0;
	}
}

void setOutputVolumePct(int pct)
{
	gOutputVolumePct = std::clamp(pct, 0, 100);
}

} // namespace hostdsp
