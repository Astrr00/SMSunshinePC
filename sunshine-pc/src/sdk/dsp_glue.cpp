// Host DSP, part 1: the CPU-visible DSP interface of JAudio1. Replaces the decomp's JSystem/dspproc.c, dsptask.c,
// osdsp.c and osdsp_task.c (excluded from the build, they poke __DSPRegs) with a DSP running on a host thread.
//
// THE PROTOCOL (as the original glue + microcode speak it, derived from those four files and JASAudioThread.cpp):
//   CPU -> DSP  DsetupTable    command 0x81xxxxxx: voice count, voice block array, resampler table, ADPCM table, FX lines.
//                              The DSP answers with the mail 0xF355 << 16 | 0x8100, which JASAudioThread::syncDSP turns
//                              into DspFinishWork(0x8100): that pops the completion callback DsetupTable waits for.
//               DsyncFrame     command 0x82xxxxxx: subframe count, two output buffers, master level. No answer.
//               DSPReleaseHalt 2 words = bit map of enabled voices (bit 63 = voice 0): "render the next subframe".
//  DSP -> CPU   after every subframe the DSP interrupts the CPU with the mail 0xF355FF00 | subframes left; the handler
//               (JASAudioThread::syncDSP, registered with DspBoot) turns it into message 1 of the audio thread, which
//               updates the voices for the next subframe and calls DSPReleaseHalt again, 7 times per frame, and starts
//               the next frame after the last one (JASDSPBuf.cpp). The audio thread paces the DSP: it only asks for a
//               new frame when the AI DMA consumed one (JASDSPBuf.cpp dspstatus/read_buffer), so the DSP needs no clock.
//
// WHAT THIS FILE DOES NOT EMULATE: the hardware mailbox words themselves and the DSP task scheduler of osdsp_task.c
// (task switching with CARDUnlock's DSP program; nothing here loads it). The original posts 0xDCD10004 first and lets
// __DSPHandler call the request callback; here the DSP thread posts only the second mail and calls the callback itself,
// with interrupts disabled like an interrupt handler. The boot/handshake (0xDCD10000) collapses into "the thread runs".
// Commands are typed C++ values, not 32-bit words: buffer addresses stay full width (see dspproc.h TARGET_PC).
#include "av.hpp"
#include "dsp_mixer.hpp"

#include <JSystem/JAudio/JASystem/JASDSPInterface.hpp>
#include <JSystem/dspproc.h>
#include <JSystem/dsptask.h>
#include <dolphin/ar.h>
#include <dolphin/os.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

extern "C" void* ARGetStorageAddress(void); // Aurora extension (AR.cpp): host base of ARAM
u64 DSP_CreateMap();                        // JASDSPInterface.cpp

namespace {

struct Command {
	enum Kind { Setup, Frame, Halt } kind;
	u32 count        = 0;       // Setup: voices; Frame: subframes
	uintptr_t a = 0, b = 0, c = 0, d = 0; // Setup: voices, resample table, ADPCM table, FX lines; Frame: a = right, b = left
	u16 level        = 0;       // Frame: master level (4.12)
	u64 map          = 0;       // Halt: enabled voices
	u32 reply        = 0;       // Setup: low half of the answer mail
};

// SUNSHINE_DSP_TRACE=1 logs the protocol (one line per command and per interrupt) to stderr.
bool traceOn()
{
	static const bool on = std::getenv("SUNSHINE_DSP_TRACE") != nullptr;
	return on;
}
#define DSP_TRACE(...)                                                                                                 \
	do {                                                                                                               \
		if (traceOn()) {                                                                                               \
			std::fprintf(stderr, "[dsp] " __VA_ARGS__);                                                                \
			std::fputc('\n', stderr);                                                                                  \
			std::fflush(stderr);                                                                                       \
		}                                                                                                              \
	} while (0)

std::mutex g_qm; // leaf lock: never held while taking the interrupt lock
std::condition_variable g_qcv;
std::deque<Command> g_queue;
std::atomic<bool> g_running { false }; // the DSP booted (Dsp_Running_Check of the original)
std::atomic<bool> g_stopping { false };
void (*g_requestCb)(void*) = nullptr; // DspBoot's argument: the DSP's interrupt handler on the CPU side
u16 g_mixerLevel           = 0x4000;  // dspproc.c DSP_MIXERLEVEL

// ---- completion callbacks of commands (dsptask.c taskwork ring) -------------------------------------------------------
struct TaskWork {
	u16 command;
	void (*callback)(u16);
} g_taskwork[16];
u32 g_taskRead, g_taskWrite;

// Called with interrupts disabled (the DSP's interrupt handler also runs under that lock).
u32 startWork(u32 command, void (*callback)(u16))
{
	const u32 next = g_taskWrite + 1;
	if ((next & 0xf) == g_taskRead)
		return 0;
	g_taskwork[g_taskWrite] = { static_cast<u16>(command >> 16), callback };
	g_taskWrite             = next & 0xf;
	return next;
}

void send(const Command& c, u32 commandWord = 0, void (*callback)(u16) = nullptr)
{
	while (!g_running) // DSPSendCommands2 waits for the DSP handshake
		std::this_thread::yield();
	BOOL enabled = OSDisableInterrupts();
	if (callback && startWork(commandWord, callback) == 0)
		std::fprintf(stderr, "[dsp] command completion ring is full; the callback of command 0x%08x is lost\n",
		             static_cast<unsigned>(commandWord));
	{
		std::lock_guard<std::mutex> lk(g_qm);
		g_queue.push_back(c);
	}
	g_qcv.notify_one();
	OSRestoreInterrupts(enabled);
}

// ---- the DSP thread ----------------------------------------------------------------------------------------------------

// The DSP's interrupt: mail in the mailbox, then the CPU's request callback under the interrupt lock.
void interrupt(u32 mailLow16)
{
	DSP_TRACE("interrupt: mail 0x%04x", static_cast<unsigned>(mailLow16));
	av::DspPostMailFromDsp((static_cast<u32>(JASystem::DSPInterface::JAS_DSP_PREFIX) << 16) | mailLow16);
	BOOL enabled = OSDisableInterrupts();
	if (g_requestCb)
		g_requestCb(nullptr);
	OSRestoreInterrupts(enabled);
}

struct FrameState {
	bool active = false;
	u32 index = 0, count = 0;
	s16* right = nullptr; // first buffer of DsyncFrame: Kernel::vframeWork interleaves the second buffer first (left)
	s16* left  = nullptr;
	u16 level  = 0;
} g_frame;

hostdsp::Mixer g_mixer;

void bindAram()
{
	static const u8* bound = nullptr;
	const u8* base         = static_cast<const u8*>(ARGetStorageAddress());
	if (base != bound) {
		bound = base;
		g_mixer.setAram(base, base ? ARGetSize() : 0);
	}
}

void handle(const Command& c)
{
	DSP_TRACE("command %d (count %u, map %016llx)", static_cast<int>(c.kind), c.count,
	          static_cast<unsigned long long>(c.map));
	switch (c.kind) {
	case Command::Setup: {
		hostdsp::Mixer::Config cfg;
		cfg.voices     = reinterpret_cast<JASystem::DSPInterface::DSPBuffer*>(c.a);
		cfg.resTable   = reinterpret_cast<const u32*>(c.b);
		cfg.adpcmTable = reinterpret_cast<const u32*>(c.c);
		cfg.fx         = reinterpret_cast<JASystem::DSPInterface::FXBuffer*>(c.d);
		if (c.count != hostdsp::kVoiceCount)
			std::fprintf(stderr, "[dsp] DsetupTable with %u voices (the mixer handles %d)\n", c.count,
			             hostdsp::kVoiceCount);
		g_mixer.configure(cfg);
		g_frame = FrameState();
		bindAram();
		interrupt(c.reply);
		break;
	}
	case Command::Frame:
		if (g_frame.active)
			std::fprintf(stderr, "[dsp] frame command while %u subframes of the previous frame are open\n",
			             g_frame.count - g_frame.index);
		g_frame = { true, 0, c.count, reinterpret_cast<s16*>(c.a), reinterpret_cast<s16*>(c.b), c.level };
		break;
	case Command::Halt: {
		if (!g_frame.active) {
			static bool warned;
			if (!warned) {
				warned = true;
				std::fprintf(stderr, "[dsp] DSPReleaseHalt without an open frame (ignored)\n");
			}
			break;
		}
		bindAram();
		s16 left[hostdsp::kSubframeSamples], right[hostdsp::kSubframeSamples];
		g_mixer.render(c.map, g_frame.level, left, right);
		const size_t off = static_cast<size_t>(g_frame.index) * hostdsp::kSubframeSamples;
		std::memcpy(g_frame.left + off, left, sizeof(left));
		std::memcpy(g_frame.right + off, right, sizeof(right));
		const u32 remaining = g_frame.count - ++g_frame.index;
		if (remaining == 0)
			g_frame.active = false;
		interrupt(0xFF00 | (remaining & 0xFF));
		break;
	}
	}
}

void dspMain()
{
	av::BoostThreadPriority();
	DSP_TRACE("booted");
	g_running = true; // booted: from here DSPSendCommands2 of the original would proceed
	for (;;) {
		Command c;
		{
			std::unique_lock<std::mutex> lk(g_qm);
			g_qcv.wait(lk, [] { return g_stopping || !g_queue.empty(); });
			if (g_stopping)
				return;
			c = g_queue.front();
			g_queue.pop_front();
		}
		handle(c);
	}
}

volatile BOOL g_setupPending;
void setupCallback(u16) { g_setupPending = 0; }

} // namespace

// =====================================================================================================================
// dsptask.c
// =====================================================================================================================

void DspBoot(void (*requestCallback)(void*))
{
	static std::once_flag once;
	DSP_TRACE("DspBoot");
	std::call_once(once, [requestCallback] {
		g_requestCb = requestCallback;
		std::atexit([] {
			g_stopping = true;
			g_qcv.notify_all();
		});
		std::thread(dspMain).detach(); // ponytail: not joined; teardown order of the OS layer is not ours to fix
	});
}

void DspFinishWork(u16 command)
{
	if (command != g_taskwork[g_taskRead].command)
		return;
	if (g_taskwork[g_taskRead].callback)
		g_taskwork[g_taskRead].callback(static_cast<u16>(g_taskRead));
	g_taskRead = (g_taskRead + 1) & 0xf;
}

// =====================================================================================================================
// dspproc.c
// =====================================================================================================================

void DsetMixerLevel(float level)
{
	const float v = 4096.0f * level;
	g_mixerLevel  = v <= 0.0f ? 0 : v >= 65535.0f ? 0xFFFF : static_cast<u16>(v);
}

void DsetupTable(u32 count, uintptr_t voices, uintptr_t resTable, uintptr_t adpcmTable, uintptr_t fx)
{
	Command c { Command::Setup };
	c.count = count;
	c.a = voices, c.b = resTable, c.c = adpcmTable, c.d = fx;
	const u32 word = (count & 0xFFFF) | 0x81000000;
	c.reply        = word >> 16;
	g_setupPending = 1;
	send(c, word, setupCallback);
	while (g_setupPending)
		std::this_thread::yield();
}

void DsyncFrame(u32 subframes, uintptr_t firstBuffer, uintptr_t secondBuffer)
{
	Command c { Command::Frame };
	c.count = subframes & 0xFF;
	c.a     = firstBuffer;
	c.b     = secondBuffer;
	c.level = g_mixerLevel;
	send(c);
}

void DSPReleaseHalt()
{
	Command c { Command::Halt };
	c.map = DSP_CreateMap();
	send(c);
}

// osdsp_task.c: the original defers the command while another DSP task has the DSP (CARDUnlock); nothing else
// runs on the host DSP.
void DsyncFrame2(u32 subframes, uintptr_t firstBuffer, uintptr_t secondBuffer)
{
	DsyncFrame(subframes, firstBuffer, secondBuffer);
}
