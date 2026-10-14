// Launcher shown when sunshine.exe starts without --dvd: disc image choice, validation and settings UI.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace launcher {

// Opens the disc image and checks it is Super Mario Sunshine NTSC-J (GMSJ01). On success the disc stays open.
bool openDisc(const std::string& path, std::string& error);

// Runs the launcher until Start (returns true, the disc is open) or the window closes (returns false).
// `settingsFile` is where edits are saved. Requires aurora_initialize to have run.
bool run(const std::filesystem::path& settingsFile);

// Pushes the persisted settings into the running game: PAD bindings and dead zones, master volume,
// window mode and size, resolution scale. Call after the PAD defaults are installed.
void applyRuntimeSettings();
// The [controls] dead zones alone, onto the pad on port 0 (no pad: nothing). main calls it once after its startup pump,
// which drops Aurora's events.
void applyPadDeadZones();
// main calls it on AURORA_CONTROLLER_ADDED with the event's SDL joystick instance: applyPadDeadZones only when that pad
// is the one on port 0, so adding a second pad does not reset port 0's zones.
void controllerAdded(std::uint32_t instance);

// In-game menu (UI-4): the launcher's settings page over the frozen game. R+Start (pad) or F1 opens it in gameplay;
// Start, B, Escape, F1 and Back close it. menuInstall registers its input layer once; main's present seam calls
// menuPoll once per presented frame and, while menuOpen() is true, keeps presenting frames that draw only the menu
// (no replay of the game frame under it, see sunshinePresentFrame in main.cpp).
void menuInstall(const std::filesystem::path& settingsFile);
void menuPoll();
bool menuOpen();
// Dev hook (devscript verb `menu`): comma-separated tokens run one per 30 menuPoll calls. chord (synthetic R + Start into
// the menu layer), f1 and key:<SDL scancode name>:down|up (pushed SDL key events, routed like real ones), and the
// launcher's pad tokens (up, down, left, right, a, b, start, lstick-*) while the menu is open. vstick:<x>:<y> (-127..127)
// sets the left stick of the SUNSHINE_DEV_VPAD gamepad, menu open or not.
void menuDevQueue(const char* tokens);
// Dev hook, only with SUNSHINE_DEV_VPAD=1 or late (no-op otherwise): attaches an SDL virtual gamepad and pumps Aurora
// until it is player 0. main calls it before applyRuntimeSettings, so the [controls] dead zones land on that pad like on
// a real one.
// SUNSHINE_DEV_VPAD=late skips the pump: the game's first pump adds the pad (like any pad at a --dvd boot, or hot-plug).
void devVpadAttach();

} // namespace launcher
