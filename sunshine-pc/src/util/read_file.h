// Whole-file read for the port's own loaders (cheats, mods). Uses the Win32 file API: std::ifstream/rdbuf
// hung the startup thread in this port's CRT (see docs/platform-notes.md).
#pragma once

#include <filesystem>
#include <string>

// Reads the whole file into out. False when the file cannot be opened or read; out is then empty.
bool readWholeFile(const std::filesystem::path& path, std::string& out);
