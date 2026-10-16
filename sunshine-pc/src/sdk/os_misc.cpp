// OS layer, part 2: reporting, reset, stopwatch, SRAM-backed modes, ROM font, error handlers and the PPC/hardware
// leftovers (decomp: OSError.c, OSReset.c, OSStopwatch.c, OSRtc.c, OSFont.c, OSResetSW.c, OSMemory.c, PPCArch.c).
// Threads, interrupts, mutexes and message queues are in os_thread.cpp.
#include <dolphin/base/PPCArch.h>
#include <dolphin/os.h>
#include "../settings.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" {

// Bus clock the time base is derived from (OS_TIMER_CLOCK = bus / 4); same constant as Aurora's OSGetTime().
u32 __OSBusClock = 162000000;

// ---- reporting ----

// The decomp prints with vprintf; so does this.
void OSReport(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	std::vfprintf(stdout, msg, args);
	va_end(args);
}

// The console prints a stack dump and halts; the host prints the message and aborts (no PPC stack to walk).
void OSPanic(const char* file, int line, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	std::vfprintf(stderr, msg, args);
	va_end(args);
	std::fprintf(stderr, " in \"%s\" on line %d.\n", file, line);
	std::fflush(nullptr);
	std::abort();
}

// ---- reset ----

// There is no IPL to return to: every reset kind (restart, hot reset, shutdown) ends the process with exit code 0.
// std::exit runs atexit handlers and static destructors on the calling thread; a launcher that must run
// aurora_shutdown() first should register it with std::atexit.
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu)
{
	std::fflush(nullptr);
	std::fprintf(stderr, "[exit] OSResetSystem reset=%d\n", reset);
	std::exit(0);
}

// The reset button does not exist on PC; the host window handles quitting. Never pressed.
BOOL OSGetResetSwitchState(void) { return FALSE; }

// ---- stopwatch (decomp OSStopwatch.c) ----

void OSInitStopwatch(OSStopwatch* sw, char* name)
{
	sw->name  = name;
	sw->total = 0;
	sw->hits  = 0;
	sw->min   = 0x00000000FFFFFFFF;
	sw->max   = 0;
}

void OSStartStopwatch(OSStopwatch* sw)
{
	sw->running = 1;
	sw->last    = OSGetTime();
}

void OSStopStopwatch(OSStopwatch* sw)
{
	if (sw->running != 0) {
		long long interval = OSGetTime() - sw->last;
		sw->total += interval;
		sw->running = 0;
		sw->hits++;
		if (sw->max < interval)
			sw->max = interval;
		if (interval < sw->min)
			sw->min = interval;
	}
}

long long OSCheckStopwatch(OSStopwatch* sw)
{
	long long total = sw->total;
	if (sw->running != 0)
		total += OSGetTime() - sw->last;
	return total;
}

void OSResetStopwatch(OSStopwatch* sw) { OSInitStopwatch(sw, sw->name); }

// ---- SRAM flags (decomp OSRtc.c): main persists the console sound bit; progressive mode stays process-local ----

static std::atomic<u32> sSoundMode{ OS_SOUND_MODE_STEREO }; // console default
static std::atomic<u32> sProgressiveMode{ 0 };

u32 OSGetSoundMode(void) { return sSoundMode; }
void OSSetSoundMode(u32 mode)
{
	mode &= 1;
	sSoundMode = mode;
	sunshine::settings().consoleSoundMode = static_cast<int>(mode);
}
u32 OSGetProgressiveMode(void) { return sProgressiveMode; }
void OSSetProgressiveMode(u32 mode) { sProgressiveMode = mode & 1; }

// ---- ROM font (decomp OSFont.c) ----
// The SDK decodes the font out of the console's IPL ROM, which is Nintendo's and not part of the disc image, so the
// host has no glyph data: OSInitFont fails like it does on a console whose ROM read returns no 'Yay0' header,
// and OSGetFontTexture/OSGetFontWidth report "no glyph" (null image, width 0) while still consuming one character.
// Only JUTRomFont uses it (the fallback font of the boot error screen); the game's own bfn font is unaffected.

// The console reports Shift-JIS fonts for Japanese hardware; the encoding follows the game's region.
u16 OSGetFontEncode(void)
{
#ifdef VERSION_GMSJ01
	return OS_FONT_ENCODE_SJIS;
#else
	return OS_FONT_ENCODE_ANSI;
#endif
}

BOOL OSInitFont(OSFontHeader* fontData) { return FALSE; }

// One character, two for a Shift-JIS lead byte (the SDK's string walking).
static char* nextChar(char* s)
{
	u8 c = static_cast<u8>(*s++);
	if (OSGetFontEncode() == OS_FONT_ENCODE_SJIS && ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) && *s)
		s++;
	return s;
}

char* OSGetFontTexture(char* string, void** image, s32* x, s32* y, s32* width)
{
	*image = nullptr;
	if (*string == 0)
		return string;
	*x = *y = 0;
	if (width)
		*width = 0;
	return nextChar(string);
}

char* OSGetFontWidth(char* string, s32* width)
{
	if (*string == 0)
		return string;
	if (width)
		*width = 0;
	return nextChar(string);
}

// ---- exceptions and hardware without a PC meaning ----

// Error handlers are stored but never invoked: there are no PPC exceptions on the host.
static OSErrorHandler sErrorTable[OS_ERROR_MAX + 1];

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler)
{
	if (error > OS_ERROR_MAX)
		return nullptr;
	OSErrorHandler old = sErrorTable[error];
	sErrorTable[error] = handler;
	return old;
}

// MEM protection registers (trap writes to a physical range) do not exist; nothing to program.
void OSProtectRange(u32 channel, void* address, u32 nBytes, u32 control) { }

// Fills the exception dump's FPR snapshot. The host FPU is not the PPC register file and the dump path never runs
// here, so the snapshot is zeroed rather than left as stale memory.
void OSFillFPUContext(OSContext* context)
{
	for (f64& f : context->fpr)
		f = 0;
	for (f64& f : context->psf)
		f = 0;
	context->fpscr_pad = 0;
	context->fpscr     = 0;
}

// sync: full memory barrier.
void PPCSync()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
}

} // extern "C"
