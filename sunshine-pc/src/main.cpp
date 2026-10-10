// Entry point of the PC port. Replaces sms/src/main.cpp (excluded in excluded-units.txt): Aurora brings up the
// window, GX and the disc image, then the decompiled game runs its own initialize/proc/finalize.
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>
#include <aurora/main.h>

#include <System/Application.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// BSS object defined by the decomp's main.cpp.
TApplication gpApplication;

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int)
{
	std::fprintf(level >= LOG_ERROR ? stderr : stdout, "[%d %s] %s\n", (int)level, module, message);
	if (level == LOG_FATAL)
		std::abort();
}

static const char* findDvdArg(int argc, char** argv)
{
	for (int i = 1; i + 1 < argc; i++) {
		if (std::strcmp(argv[i], "--dvd") == 0)
			return argv[i + 1];
	}
	return nullptr;
}

void installCrashHandler(); // crash.cpp

// Called by VISetNextFrameBuffer (sdk/av.cpp), i.e. once per game frame at the point where the console hands the
// finished XFB to the video hardware: present what the game drew, pump window events, open the next frame.
// This is also the single seam where a later phase decouples rendering from the 30 Hz game logic.
void sunshinePresentFrame()
{
	aurora_end_frame();
	for (;;) {
		for (const AuroraEvent* e = aurora_update(); e != nullptr && e->type != AURORA_NONE; ++e) {
			if (e->type == AURORA_EXIT)
				std::exit(0);
		}
		if (aurora_begin_frame()) // false while minimized or paused: keep pumping events
			break;
	}
}

int main(int argc, char* argv[])
{
	installCrashHandler();
	// GameCube main RAM (24 MiB) backs the OS arena the JKR heaps are carved from; ARAM (16 MiB) backs JKRAram.
	const AuroraConfig config = { .appName = "Sunshine", .logCallback = &log_callback, .mem1Size = 24u * 1024 * 1024,
		                          .mem2Size = 16u * 1024 * 1024 };
	aurora_initialize(argc, argv, &config);

	const char* disc = findDvdArg(argc, argv);
	if (disc == nullptr || !aurora_dvd_open(disc)) {
		std::fprintf(stderr, "usage: sunshine --dvd <path to disc image (iso/rvz/wia/...)>\n");
		aurora_shutdown();
		return 1;
	}

	// The console's startup code (excluded, __start) runs OSInit before main; it sets up the arena JKRHeap draws from.
	OSInit();
	while (!aurora_begin_frame()) { // GX commands issued during TApplication::initialize need an open frame
		aurora_update();
	}
	gpApplication.initialize();
	gpApplication.proc();
	gpApplication.finalize();

	aurora_shutdown();
	return 0;
}
