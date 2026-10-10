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

} // namespace launcher
