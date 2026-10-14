// Entry point of the PC port. Replaces sms/src/main.cpp (excluded in excluded-units.txt): Aurora brings up the
// window, GX and the disc image, then the decompiled game runs its own initialize/proc/finalize.
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <sunshine_config.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include "render/render_scale.h"
#include "widescreen/widescreen.h"
#include "launcher/launcher.hpp"
#include "settings.h"
#include "saves/saves.hpp"

#include <System/Application.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include "cheats/cheats.h"
#include "mods/mods_loader.h"

// ABI guard: the game's PADStatus (sms/include/dolphin/pad.h, TARGET_PC) must match Aurora's, which PADRead() fills.
#include <dolphin/pad.h>
#include <dolphin/vi.h> // VIWaitForRetrace: in-game menu pacing
static_assert(sizeof(PADStatus) == 16, "PADStatus layout differs from Aurora's");

// BSS object defined by the decomp's main.cpp.
TApplication gpApplication;

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int)
{
	// Aurora logs every unhandled XF/BP register write at debug level (thousands per frame); SUNSHINE_LOG_DEBUG=1 restores them.
	static const bool debugLogs = std::getenv("SUNSHINE_LOG_DEBUG") != nullptr;
	if (level == LOG_DEBUG && !debugLogs)
		return;
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
void sunshineHeartbeat();   // crash.cpp: watchdog heartbeat, once per presented frame
void sunshineWatchdogStart(); // crash.cpp
void sunshineInstallKeyboardDefaults(); // sdk/pad_keys.cpp

// Developer switch: SUNSHINE_DEV_SKIP_MOVIES=1 ends every THP movie as soon as it starts playing (the retail game only
// allows skipping movies that were already seen), to reach the scenes behind them quickly during boot testing.
bool sunshineDevSkipMovies()
{
	static const bool skip = std::getenv("SUNSHINE_DEV_SKIP_MOVIES") != nullptr;
	return skip;
}

// Present the open frame, pump window events, open the next frame.
static void presentAndPump()
{
	aurora_end_frame();
	for (;;) {
		for (const AuroraEvent* e = aurora_update(); e != nullptr && e->type != AURORA_NONE; ++e) {
			if (e->type == AURORA_EXIT) {
				std::fprintf(stderr, "[exit] AURORA_EXIT\n");
				std::exit(0);
			}
			if (e->type == AURORA_CONTROLLER_ADDED) {
				// A pad Aurora adds after applyRuntimeSettings (any pad at a --dvd boot: no pump before it; hot-plug) would
				// keep Aurora's own 24 % dead zones (tools/routes/99_stick_deadzone.dev, SUNSHINE_DEV_VPAD=late). Only for the
				// port-0 pad, so a second pad does not reset port 0's zones.
				launcher::controllerAdded(e->controller);
			}
		}
		sunshineHeartbeat(); // minimized or paused windows keep pumping events: not a hang
		if (aurora_begin_frame()) // false while minimized or paused: keep pumping events
			break;
	}
}

// Called by TVideo::waitForRetrace (JDRVideo.cpp) once per game frame, right before VIFlush: present what the game
// drew, pump window events, open the next frame. Aurora has no XFB, so the game's XFB handoff cannot mark frames.
// This is also the single seam where a later phase decouples rendering from the 30 Hz game logic.
void sunshinePresentFrame()
{
	launcher::menuPoll(); // in-game menu (UI-4): opens on a request from the last pump or a dev token, so this present shows it
	modsPresent(); // mods/mods_loader.cpp
	presentAndPump();
	// Menu open: the game is frozen here. Logic, the devscript and the game's PAD reads wait; each frame draws only the
	// menu (opaque, no game draws). VIWaitForRetrace paces it and lets the other game threads run.
	// shortcut: no sunshineReplayFrame under the menu. Before the replay abort fix it aborted here in Aurora's FIFO worker
	// (frame index buffer full, recording.cpp push), and a control without the menu aborted the same way (replay60,
	// fps_limit=60, dolpic/0, a49732c). Upgrade: replay under the menu after a run proves it (circle shadows: see the
	// replay bullets in docs/platform-notes.md, frame-rate pacing).
	while (launcher::menuOpen()) {
		presentAndPump();
		VIWaitForRetrace();
		launcher::menuPoll(); // closed: the caller draws into the open frame as usual
	}
}

// Called from sms MarDirectorDirect.cpp once per direct() tick (30 Hz), after the last logic sub-step's movement().
void sunshineLogicTick()
{
	cheatsTick();
	modsLogicTick();
}

int main(int argc, char* argv[])
{
	installCrashHandler();
	// Settings load before Aurora starts: window and VSync options are part of Aurora's startup config.
	const std::filesystem::path settingsFile = sunshine::settingsPath(std::filesystem::path(argv[0]).parent_path());
	sunshine::loadSettings(sunshine::settings(), settingsFile);
	const sunshine::Settings& s = sunshine::settings();
	// Dev hooks for tiling several test windows: SUNSHINE_WINDOW_POS=x,y and SUNSHINE_WINDOW_SIZE=w,h override the saved placement.
	int posX = 0;
	int posY = 0;
	// remember_window_size=0: open at the default size; the saved size stays in the INI.
	const sunshine::Settings def = sunshine::defaultSettings();
	int width = static_cast<int>(s.rememberWindowSize ? s.windowWidth : def.windowWidth);
	int height = static_cast<int>(s.rememberWindowSize ? s.windowHeight : def.windowHeight);
	if (const char* pos = std::getenv("SUNSHINE_WINDOW_POS")) {
		std::sscanf(pos, "%d,%d", &posX, &posY);
	}
	if (const char* size = std::getenv("SUNSHINE_WINDOW_SIZE")) {
		std::sscanf(size, "%d,%d", &width, &height);
	}
	width = static_cast<int>(widescreenInit(s, static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
	// GameCube main RAM backs the OS arena the JKR heaps are carved from; ARAM backs JKRAram.
	// SUNSHINE_ALLOW_CPU_ADAPTER=1: accept the software adapter (WARP). Without it a GPU-less host falls back to Aurora's
	// Null backend, which draws nothing (white screenshots, 0 presents). "0" or empty keeps it off.
	const char* cpuEnv = std::getenv("SUNSHINE_ALLOW_CPU_ADAPTER");
	const AuroraConfig config = { .appName = "Sunshine", .vsync = s.vsync, .startFullscreen = s.fullscreen,
		                          .allowCpuAdapter = cpuEnv != nullptr && *cpuEnv != '\0' && *cpuEnv != '0',
		                          .windowPosX = posX, .windowPosY = posY,
		                          .windowWidth = static_cast<uint32_t>(width),
		                          .windowHeight = static_cast<uint32_t>(height), .logCallback = &log_callback,
		                          .mem1Size = kSunshineMem1Size, .mem2Size = kSunshineMem2Size };
	aurora_initialize(argc, argv, &config);
	// One ordered teardown for every way out (return from main, std::exit from the devscript, OSResetSystem, AURORA_EXIT):
	// Aurora's pipeline worker is a static std::thread that only aurora_shutdown() joins. A std::exit without it runs the
	// static destructors with the thread still joinable: std::terminate, exit code 0xC0000409.
	std::atexit([] { aurora_shutdown(); });
	sunshineApplyRenderSettings();

	const char* disc = findDvdArg(argc, argv);
	if (disc == nullptr) {
		// No --dvd: the launcher chooses and validates the disc. Quit or closing the window exits.
		if (!launcher::run(settingsFile)) {
			return 0;
		}
	} else if (!aurora_dvd_open(disc)) {
		std::fprintf(stderr, "usage: sunshine --dvd <path to disc image (iso/rvz/wia/...)>\n");
		return 1;
	}

	// Cheats and mods read files next to the exe and register disc overlays, so they need the disc open.
	const std::string exeDir = std::filesystem::path(argv[0]).parent_path().string();
	// The launcher path loaded the registry already: it holds the one-shot cheats picked there (actions, not saved to the
	// INI), and cheatsTick runs them once in the first stage after the title. A reload would drop them. --dvd and
	// skip_launcher start with an empty registry.
	if (cheatCount() == 0) {
		cheatsLoad(exeDir); // cheats/cheats_game.cpp
	}
	modsLoad(exeDir, s.mods); // mods/mods_loader.cpp

	sunshineInstallKeyboardDefaults(); // sdk/pad_keys.cpp
	launcher::devVpadAttach(); // dev hook, SUNSHINE_DEV_VPAD only: virtual gamepad as player 0 before the dead zones apply
	launcher::applyRuntimeSettings(); // launcher/launcher.cpp: PAD bindings, dead zones, master volume, window
	launcher::menuInstall(settingsFile); // launcher/launcher.cpp: R+Start / F1 in-game menu layer
	savesApplyDirectory(s, exeDir); // saves/save_directory.cpp: [saves] directory, before CARDInit in initialize()

	// The console's startup code (excluded, __start) runs OSInit before main; it sets up the arena JKRHeap draws from.
	sunshineWatchdogStart(); // after the launcher: its own loop is interactive and may idle for any time
	OSInit();
	while (!aurora_begin_frame()) { // GX commands issued during TApplication::initialize need an open frame
		aurora_update();
	}
	launcher::applyPadDeadZones(); // the loop above drops Aurora's events: a pad added there (--dvd) would keep 24 %
	gpApplication.initialize();
	gpApplication.proc();
	gpApplication.finalize();
	modsShutdown(); // mods/mods_loader.cpp: calls sunshine_mod_shutdown and frees the DLLs

	return 0;
}
