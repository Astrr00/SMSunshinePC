#include <JSystem/JAudio/JASystem/JASDSPBuf.hpp>
#include <JSystem/JAudio/JASystem/JASProbe.hpp>
#include <JSystem/JAudio/JASystem/JASDSPInterface.hpp>
#include <JSystem/JAudio/JASystem/JASCallback.hpp>
#include <JSystem/JAudio/JASystem/JASDSPChannel.hpp>
#include <JSystem/JAudio/JASystem/JASRate.hpp>
#include <JSystem/JAudio/JASystem/JASSystemHeap.hpp>
#include <JSystem/JAudio/JASystem/JASAudioThread.hpp>
#include <JSystem/dspproc.h>
#include <JSystem/dsptask.h>
#include <dolphin/os/OSCache.h>
#ifdef TARGET_PC
#include <JSystem/JAudio/JASystem/JASChGlobal.hpp>
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>

u32 sunshineAiRepeats(); // sunshine-pc/src/sdk/av.cpp: AI periods that played the previous block again
#endif

namespace JASystem {

namespace DSPBuf {
	static s16* dsp_buf[3];

	static u8 write_buffer      = 0;
	static u8 read_buffer       = 0;
	static u8 dspstatus         = 0;
	static int dac_sync_counter = 0;
#ifdef TARGET_PC
	// SUNSHINE_DEV_AUDIO_STATS=<seconds> (dev diagnostic; any value enables it, 0 or less means 10 s): pcStats prints
	// one stderr line per <seconds> of DAC time with these counters and the channel pools. The counters always run;
	// only the line is gated.
	static u32 pcReads     = 0; // one per mixed DAC frame
	static u32 pcUnderruns = 0; // mixDSP found the next buffer still being rendered: each channel half of the frame
	                            // got one constant (UNK2 below)
	static void pcStats();
#endif
} // namespace DSPBuf

void DSPBuf::updateDSP()
{
	++dac_sync_counter;
	Kernel::probeStart(3, "SFR-UPDATE");
	DSPInterface::invalChannelAll();
	Kernel::subframeCallback();
	TDSPChannel::updateAll();
	DSPReleaseHalt();
	Kernel::aiCallback();
	Kernel::probeFinish(3);
}

#ifdef TARGET_PC
void DSPBuf::pcStats()
{
	static const char* env = getenv("SUNSHINE_DEV_AUDIO_STATS");
	static u32 every       = 0;
	static OSTime start;
	static u32 lastUnderruns = 0, lastRepeats = 0;
	if (!env)
		return;
	if (every == 0) {
		const int seconds   = atoi(env) > 0 ? atoi(env) : 10;
		const double frames = double(seconds) * Kernel::getDacRate() / Kernel::getFrameSamples();
		every               = frames < 1.0 ? 1u : frames > 1e9 ? 1000000000u : u32(frames);
		start               = OSGetTime();
	}
	if (pcReads % every != 0)
		return;

	u32 logical[10];
	ChGlobal::countChannelsPC(logical);
	u32 status[3] = { 0, 0, 0 }, enabled = 0;
	for (u32 i = 0; TDSPChannel::DSPCH && i < 64; ++i) {
		const TDSPChannel& ch = TDSPChannel::DSPCH[i];
		if (ch.mStatus < 3)
			++status[ch.mStatus];
		if (ch.mDSPHandle && ch.mDSPHandle->enabled)
			++enabled;
	}
	// ai_repeat: AI periods in which the host pump played the previous block again (no new block programmed when the
	// pump read the address: the audio thread was late, or the pump woke late, src/sdk/av.cpp AiPump). dac= counts mixes, wall= OS time since the first mix: dac falls behind wall when DAC periods
	// are lost (message 0 is dropped by the non-blocking OSSendMessage when its 16 slots are full: os_thread.cpp:836-840,
	// sent from JASAudioThread.cpp:54).
	const u32 repeats = sunshineAiRepeats();
	fprintf(stderr,
	        "[audio-stats] dac=%.1fs wall=%.1fs dsp_underrun=%u (+%u) ai_repeat=%u (+%u)"
	        " | logical global l0-3/none %u/%u/%u/%u/%u lent %u/%u/%u/%u/%u"
	        " | dsp alloc/free/stopping %u/%u/%u enabled=%u\n",
	        pcReads * double(Kernel::getFrameSamples()) / Kernel::getDacRate(),
	        double(OSTicksToMilliseconds(OSGetTime() - start)) / 1000.0, pcUnderruns,
	        pcUnderruns - lastUnderruns, repeats, repeats - lastRepeats, logical[0], logical[1], logical[2],
	        logical[3], logical[4], logical[5], logical[6], logical[7], logical[8], logical[9], status[0],
	        status[1], status[2], enabled);
	lastUnderruns = pcUnderruns;
	lastRepeats   = repeats;
}
#endif

// asm 0x8005EA8C: mixDSP tail-calls process(UNK2) and returns its r3 unchanged.
s16* DSPBuf::mixDSP(s32 param) { return process(DSPBUF_EVENTS_UNK2); }

void DSPBuf::finishDSPFrame() { process(DSPBUF_EVENTS_UNK1); }

s16* DSPBuf::process(DSPBUF_EVENTS event)
{
	u32 samples = Kernel::getFrameSamples();
	switch (event) {
	case DSPBUF_EVENTS_UNK0: {
		write_buffer = 2;
		read_buffer  = 0;
		for (u32 i = 0; i < 3; ++i) {
			dsp_buf[i]
			    = (s16*)Kernel::allocFromSysDram(samples * 2 * sizeof(s16));
			for (u32 j = 0; j < 2 * samples; ++j)
				dsp_buf[i][j] = 0;
			DCStoreRange(dsp_buf[i], samples * 2 * sizeof(s16));
		}
		dspstatus = 0;
		break;
	}
	case DSPBUF_EVENTS_UNK1: {
		u8 nextWriteBuf  = write_buffer + 1;
		u32 frameSamples = Kernel::getFrameSamples();
		if (nextWriteBuf == 3)
			nextWriteBuf = 0;

		if (nextWriteBuf == read_buffer) {
			dspstatus = 0;
		} else {
			write_buffer = nextWriteBuf;
			AudioThread::setDSPSyncCount(Kernel::getSubFrames());
			Kernel::probeStart(7, "DSP-MAIN");
			DsyncFrame2(Kernel::getSubFrames(), PTR_U32(dsp_buf[write_buffer]),
			            PTR_U32(&dsp_buf[write_buffer][frameSamples]));
			dspstatus = 1;
			updateDSP();
		}
		break;
	}
	case DSPBUF_EVENTS_UNK2: {
		u8 nextReadBuf   = read_buffer + 1;
		u32 frameSamples = Kernel::getFrameSamples();
		if (nextReadBuf == 3)
			nextReadBuf = 0;
#ifdef TARGET_PC
		++pcReads;
#endif

		if (nextReadBuf == write_buffer) {
			u32 i;
#ifdef TARGET_PC
			++pcUnderruns;
#endif

			s16 var1 = dsp_buf[read_buffer][frameSamples / 2 - 1];
			s16 var2 = dsp_buf[read_buffer][frameSamples - 1];

			for (i = 0; i < frameSamples; ++i)
				dsp_buf[read_buffer][i] = var1;
			for (i = frameSamples; i < frameSamples * 2; ++i)
				dsp_buf[read_buffer][i] = var2;

			if (dspstatus == 0)
				finishDSPFrame();
		} else {
			read_buffer = nextReadBuf;
			DCInvalidateRange(dsp_buf[read_buffer],
			                  frameSamples * 2 * sizeof(s16));
			if (dspstatus == 0)
				finishDSPFrame();
		}
#ifdef TARGET_PC
		pcStats();
#endif

		return dsp_buf[read_buffer];
	}
	}

	return nullptr;
}

} // namespace JASystem
