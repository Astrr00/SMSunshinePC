// Persistent user settings. Plain data plus a small INI reader/writer. No Aurora or SDL dependency, so
// tools/settings_test.cpp builds it on its own. The launcher edits this; runtime code reads sunshine::settings().
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace sunshine {

enum class GlyphStyle : int { Generic = 0, Xbox, PlayStation, Nintendo };

struct Settings {
	// [video]
	bool fullscreen = false;
	int windowWidth = 1280;
	int windowHeight = 960;
	bool vsync = true;          // applied at the next start: Aurora only takes vsync at initialization
	int resolutionScalePct = 0; // 0 = Auto (swapchain size), else percent of the native EFB size
	int fpsLimit = 0;           // 0 = unlimited. FrameRate owns the cap implementation.
	bool widescreen = false;    // widescreen render width, see src/widescreen/widescreen.h
	std::string aspect = "16:9"; // 4:3, 16:9, 16:10, 21:9, 32:9 or auto (window aspect at startup)
	// [audio]
	int masterVolumePct = 100;  // 0..100
	// [controls]
	int stickDeadZonePct = 10;     // 0..50, percent of full stick travel
	int substickDeadZonePct = 10;  // 0..50
	std::map<std::string, int> keys; // PAD input name (see defaultKeys) -> SDL scancode
	// [interface]
	GlyphStyle glyphStyle = GlyphStyle::Generic; // applied via glyphsSetStyle() (glyphs/glyphs.h); Generic shows the Xbox set
	// [saves]
	std::string saveDirectory; // empty = default; see resolveSaveDirectory()
	// [launcher]
	std::string lastImagePath;
};

Settings& settings(); // process-wide instance, loaded by main before Aurora starts
Settings defaultSettings();
std::filesystem::path settingsPath(const std::filesystem::path& exeDir); // %APPDATA%\Sunshine\settings.ini, else exeDir
std::filesystem::path resolveSaveDirectory(const Settings& in, const std::filesystem::path& exeDir); // [saves] directory, else %APPDATA%\Sunshine\saves

// Missing file: returns false and leaves `out` at defaults.
bool loadSettings(Settings& out, const std::filesystem::path& file);
bool saveSettings(const Settings& in, const std::filesystem::path& file);
std::string serializeSettings(const Settings& in);
// Unknown keys are ignored; invalid or out-of-range values fall back to the default or are clamped.
Settings parseSettings(std::string_view text);

} // namespace sunshine
