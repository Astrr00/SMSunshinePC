// Launcher shown when sunshine.exe starts without --dvd: disc image choice, validation and settings UI.
#pragma once

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

// In-game menu (UI-4): the launcher's settings page over the frozen game. R+Start (pad) or F1 opens it in gameplay;
// Start, B, Escape, F1 and Back close it. menuInstall registers its input layer once; main's present seam calls
// menuPoll once per presented frame and, while menuOpen() is true, keeps presenting frames that draw only the menu
// (no replay of the game frame under it, see sunshinePresentFrame in main.cpp).
void menuInstall(const std::filesystem::path& settingsFile);
void menuPoll();
bool menuOpen();
// Dev hook (devscript verb `menu`): comma-separated tokens run one per 30 menuPoll calls. chord (synthetic R + Start into
// the menu layer), f1 and key:<SDL scancode name>:down|up (pushed SDL key events, routed like real ones), and the
// launcher's pad tokens (up, down, left, right, a, b, start, lstick-*) while the menu is open.
void menuDevQueue(const char* tokens);

} // namespace launcher
