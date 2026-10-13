// Host DSP, part 2: the HLE mixer. It does what JAudio1's DSP microcode (not part of the decomp) does with the voice
// parameter blocks: decode, resample, filter and mix the voices of one 80-sample subframe into two output buffers.
// It knows nothing about mailboxes or threads (dsp_glue.cpp), so tools/dsp_test.cpp links it standalone.
//
// WHAT THE CPU SIDE DEFINES (derived from sms/src/JSystem/JAudio/JASystem/*, nothing here is guessed unless marked
// "approximation"):
//  * A voice is a JASystem::DSPInterface::DSPBuffer, 64 of them, plus 4 FXBuffer effect lines. DsetupTable hands the
//    block arrays, the resampler/oscillator table (DSPRES_FILTER) and the ADPCM coefficient table (DSPADPCM_FILTER).
//  * One frame = gSubFrames (7) subframes of 80 samples at the 32 kHz DAC rate (JASRate.cpp). DSPReleaseHalt carries
//    the bit map of enabled voices (bit 63 = voice 0) for the next subframe.
//  * Sample data lives in ARAM (DSPBuffer::baseAddress is an ARAM byte address, loopAddress/endPosition/sampleCount
//    are sample indices, see DSPBuffer::setWaveInfo). ARAM is host memory (Aurora's AR), big-endian as on the disc.
//    Stream voices (samplesSourceType 0x21, JAIGFrameStream Play_DirectPCM) instead read host-endian s16 from a
//    main-RAM ring buffer: baseAddress is then a real host pointer and positions are 16.16 fixed point.
//  * resamplingRatio is 4.12 fixed point of source samples per output sample (0x1000 = 1.0).
//  * Mixer channel volumes are s16, added as (vol * src) >> 16 (Mixer::mixVoice; JAIGFrameStream plays at 0x7FFF and
//    uses 0x5A7E = 0.707 * 0x7FFF for an equal-power centre pan). A mixer channel routes the voice to the bus whose DSP
//    RAM address is `id` (0 = off).
//  * The master level of the frame command (DsetMixerLevel) is 4.12 fixed point (4096.0f * level).
//
// Bus layout (JASDSPInterface.cpp connect_table/SEND_TABLE): DSP RAM address -> bus. 0x0D00 is the left output, 0x0D60
// the right output (stream voice 0 puts the left decoded channel on mixer 0 = bus 1 = 0x0D00, and Kernel::vframeWork
// interleaves the second DsyncFrame buffer first).
#pragma once

#include <JSystem/JAudio/JASystem/JASDSPInterface.hpp>
#include <dolphin/types.h>

#include <cstddef>
#include <cstdint>

namespace hostdsp {

inline constexpr int kSubframeSamples = 80;
inline constexpr int kVoiceCount      = 64;
inline constexpr int kFxLineCount     = 4;
inline constexpr int kBusCount        = 13;

inline constexpr u16 kBusLeft  = 0x0D00;
inline constexpr u16 kBusRight = 0x0D60;

// ---------------------------------------------------------------------------------------------------------------------
// DSP-ADPCM (AFC 4-bit): 9-byte frame = header (scale << 4 | coefficient index) + 16 nibbles, high nibble first,
// -> 16 samples. The coefficient table is DSPADPCM_FILTER: word i = (coef for y[n-1]) << 16 | (coef for y[n-2]) & 0xFFFF.
// y1/y2 are the two previous output samples and are updated. Same arithmetic as Dusklight's Adpcm.cpp (CC0) and as the
// CPU-side stream decoder JAIGFrameStream.cpp __DecodeADPCM; the result is saturated to 16 bits like the DSP store.
// ---------------------------------------------------------------------------------------------------------------------
inline s16 clamp16(s32 v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : static_cast<s16>(v); }

inline void adpcm4Frame(const u8* frame, const u32* coefTable, s16 out[16], s16& y1, s16& y2)
{
	const u32 pair = coefTable[frame[0] & 0xF];
	const s32 c1   = static_cast<s16>(pair >> 16); // applies to y[n-1]
	const s32 c2   = static_cast<s16>(pair);       // applies to y[n-2]
	const int sh   = frame[0] >> 4;
	for (int i = 0; i < 16; ++i) {
		const u8 byte = frame[1 + i / 2];
		s32 nib       = (i & 1) ? (byte & 0xF) : (byte >> 4);
		nib           = static_cast<s32>(static_cast<u32>(nib) << 28) >> 28; // sign-extend 4 bits
		const s16 s   = clamp16(static_cast<s32>(static_cast<u32>(nib) << sh) + ((c1 * y1 + c2 * y2) >> 11));
		out[i]        = s;
		y2            = y1;
		y1            = s;
	}
}

class Mixer {
public:
	struct Config {
		JASystem::DSPInterface::DSPBuffer* voices = nullptr; // kVoiceCount blocks (DsetupTable param 2)
		JASystem::DSPInterface::FXBuffer* fx      = nullptr; // kFxLineCount lines (DsetupTable param 5)
		const u32* resTable                       = nullptr; // DSPRES_FILTER (DsetupTable param 3)
		const u32* adpcmTable                     = nullptr; // DSPADPCM_FILTER (DsetupTable param 4)
		const u8* aram                            = nullptr; // host base of ARAM
		size_t aramSize                           = 0;
	};

	void configure(const Config& c);
	void setAram(const u8* base, size_t size)
	{
		cfg_.aram     = base;
		cfg_.aramSize = size;
	}

	// Renders one subframe: the effect-line returns, the voices whose bit is set in `voiceMap` (bit 63 = voice 0), the
	// output matrix and the output stage, then the effect lines store their send buses. Writes kSubframeSamples saturated
	// s16 samples to `left` and `right`.
	// Voices write their state back into their blocks (positions, histories, volumes, `done`), as the DSP does.
	void render(u64 voiceMap, u16 masterLevel, s16* left, s16* right);

private:
	enum class Kind { Unknown, Osc, Afc4, Afc2, Pcm8, Pcm16, DirectPcm };

	struct VoiceState { // what the DSP keeps outside the parameter block
		bool ended    = false; // a non-looping source ran dry
		u8 flushed    = 0;     // zero samples fed to the resampler since then
		bool paused   = false; // useConstantSample was set (constantSample latched)
		s16 last      = 0;     // last resampled sample of the previous subframe
	};

	struct FxState { // delay-line position of an effect line (the block has no field for it) and the ucode's FIR window
		s16* buffer = nullptr;
		u32 blocks  = 0;
		u32 write   = 0;
		s16 hist[8] = {}; // last 8 samples of the previous block read (send bus words 80-87)
		bool active = false;
	};

	Kind kindOf(const JASystem::DSPInterface::DSPBuffer& v) const;
	void resetVoice(JASystem::DSPInterface::DSPBuffer& v, VoiceState& st, Kind kind);
	s16 fetch(JASystem::DSPInterface::DSPBuffer& v, VoiceState& st, Kind kind);
	bool endOrLoop(JASystem::DSPInterface::DSPBuffer& v, VoiceState& st, u32& pos, bool adpcm);
	s16 endSource(JASystem::DSPInterface::DSPBuffer& v, VoiceState& st);
	void renderVoice(int index, JASystem::DSPInterface::DSPBuffer& v);
	void filterVoice(JASystem::DSPInterface::DSPBuffer& v, s32* mono);
	void mixVoice(JASystem::DSPInterface::DSPBuffer& v, const s32* mono);
	void fxReturn(const s32* revBackLeft, const s32* revBackRight);
	void fxStore();
	int busSlot(u16 id) const;

	Config cfg_;
	VoiceState state_[kVoiceCount];
	FxState fxState_[kFxLineCount];
	s32 bus_[kBusCount][kSubframeSamples];
};

// Launcher master volume, percent 0..100. Scales the game's master level in Mixer::render.
void setOutputVolumePct(int pct);

} // namespace hostdsp
