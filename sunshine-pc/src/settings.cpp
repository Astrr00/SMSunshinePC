#include "settings.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace sunshine {
namespace {

// Keys are the PAD names the launcher maps onto PADKeyButtonBinding / PADKeyAxisBinding. "L" and "R" bind both the
// trigger button and the trigger axis. Defaults match src/sdk/pad_keys.cpp.
struct KeyDefault {
	const char* name;
	int scancode;
};
constexpr KeyDefault kDefaultKeys[] = {
	{ "A", 44 }, { "B", 8 }, { "X", 20 }, { "Y", 9 }, { "START", 40 }, { "Z", 29 }, { "L", 225 }, { "R", 224 },
	{ "DPAD_UP", 82 }, { "DPAD_DOWN", 81 }, { "DPAD_LEFT", 80 }, { "DPAD_RIGHT", 79 },
	{ "STICK_UP", 26 }, { "STICK_DOWN", 22 }, { "STICK_LEFT", 4 }, { "STICK_RIGHT", 7 },
	{ "CSTICK_UP", 12 }, { "CSTICK_DOWN", 14 }, { "CSTICK_LEFT", 13 }, { "CSTICK_RIGHT", 15 },
};

using Ini = std::map<std::string, std::string>; // "section.key" -> raw value

std::string_view trim(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
		s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
		s.remove_suffix(1);
	return s;
}

Ini readIni(std::string_view text)
{
	Ini ini;
	std::string section;
	size_t pos = 0;
	while (pos < text.size()) {
		size_t end = text.find('\n', pos);
		if (end == std::string_view::npos)
			end = text.size();
		std::string_view line = trim(text.substr(pos, end - pos));
		pos = end + 1;
		if (line.empty() || line.front() == '#' || line.front() == ';')
			continue;
		if (line.front() == '[' && line.back() == ']') {
			section = std::string(trim(line.substr(1, line.size() - 2)));
			continue;
		}
		const size_t eq = line.find('=');
		if (eq == std::string_view::npos)
			continue;
		ini[section + "." + std::string(trim(line.substr(0, eq)))] = std::string(trim(line.substr(eq + 1)));
	}
	return ini;
}

int readInt(const Ini& ini, const std::string& name, int fallback, int lo, int hi)
{
	const auto it = ini.find(name);
	if (it == ini.end())
		return fallback;
	const std::string& s = it->second;
	int v = 0;
	const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
	if (ec != std::errc{} || ptr != s.data() + s.size())
		return fallback;
	return std::clamp(v, lo, hi);
}

bool readBool(const Ini& ini, const std::string& name, bool fallback)
{
	return readInt(ini, name, fallback ? 1 : 0, 0, 1) != 0;
}

std::string readString(const Ini& ini, const std::string& name, const std::string& fallback)
{
	const auto it = ini.find(name);
	return it == ini.end() ? fallback : it->second;
}

bool isValidAspect(const std::string& s)
{
	return s == "4:3" || s == "16:9" || s == "16:10" || s == "21:9" || s == "32:9" || s == "auto";
}

const char* glyphName(GlyphStyle style)
{
	switch (style) {
	case GlyphStyle::Xbox:        return "xbox";
	case GlyphStyle::PlayStation: return "playstation";
	case GlyphStyle::Nintendo:    return "nintendo";
	case GlyphStyle::Generic:     break;
	}
	return "generic";
}

GlyphStyle parseGlyph(const std::string& s)
{
	if (s == "xbox")
		return GlyphStyle::Xbox;
	if (s == "playstation")
		return GlyphStyle::PlayStation;
	if (s == "nintendo")
		return GlyphStyle::Nintendo;
	return GlyphStyle::Generic;
}

} // namespace

Settings& settings()
{
	static Settings instance = defaultSettings();
	return instance;
}

Settings defaultSettings()
{
	Settings s;
	for (const KeyDefault& k : kDefaultKeys)
		s.keys[k.name] = k.scancode;
	return s;
}

std::filesystem::path settingsPath(const std::filesystem::path& exeDir)
{
	if (const char* appData = std::getenv("APPDATA"); appData && *appData)
		return std::filesystem::path(appData) / "Sunshine" / "settings.ini";
	return exeDir / "settings.ini";
}

std::filesystem::path resolveSaveDirectory(const Settings& in, const std::filesystem::path& exeDir)
{
	if (!in.saveDirectory.empty())
		return in.saveDirectory;
	if (const char* appData = std::getenv("APPDATA"); appData && *appData)
		return std::filesystem::path(appData) / "Sunshine" / "saves";
	return exeDir / "saves";
}

Settings parseSettings(std::string_view text)
{
	const Ini ini = readIni(text);
	const Settings def = defaultSettings();
	Settings s;
	s.fullscreen = readBool(ini, "video.fullscreen", def.fullscreen);
	s.windowWidth = readInt(ini, "video.width", def.windowWidth, 320, 7680);
	s.windowHeight = readInt(ini, "video.height", def.windowHeight, 240, 4320);
	s.vsync = readBool(ini, "video.vsync", def.vsync);
	s.resolutionScalePct = readInt(ini, "video.resolution_scale", def.resolutionScalePct, 0, 400);
	s.fpsLimit = readInt(ini, "video.fps_limit", def.fpsLimit, 0, 1000);
	s.widescreen = readBool(ini, "video.widescreen", def.widescreen);
	s.aspect = readString(ini, "video.aspect", def.aspect);
	if (!isValidAspect(s.aspect))
		s.aspect = def.aspect;
	s.masterVolumePct = readInt(ini, "audio.master_volume", def.masterVolumePct, 0, 100);
	s.stickDeadZonePct = readInt(ini, "controls.stick_deadzone", def.stickDeadZonePct, 0, 50);
	s.substickDeadZonePct = readInt(ini, "controls.substick_deadzone", def.substickDeadZonePct, 0, 50);
	s.keys = def.keys;
	for (auto& [name, scancode] : s.keys)
		scancode = readInt(ini, "keys." + name, scancode, 0, 511);
	s.glyphStyle = parseGlyph(readString(ini, "interface.glyph_style", glyphName(def.glyphStyle)));
	s.saveDirectory = readString(ini, "saves.directory", def.saveDirectory);
	s.lastImagePath = readString(ini, "launcher.last_image", def.lastImagePath);
	return s;
}

std::string serializeSettings(const Settings& in)
{
	std::ostringstream out;
	out << "# Sunshine settings. Written by the launcher; edit with care.\n";
	out << "[video]\n";
	out << "fullscreen=" << (in.fullscreen ? 1 : 0) << '\n';
	out << "width=" << in.windowWidth << '\n';
	out << "height=" << in.windowHeight << '\n';
	out << "vsync=" << (in.vsync ? 1 : 0) << '\n';
	out << "resolution_scale=" << in.resolutionScalePct << '\n';
	out << "fps_limit=" << in.fpsLimit << '\n';
	out << "widescreen=" << (in.widescreen ? 1 : 0) << '\n';
	out << "aspect=" << in.aspect << '\n';
	out << "[audio]\n";
	out << "master_volume=" << in.masterVolumePct << '\n';
	out << "[controls]\n";
	out << "stick_deadzone=" << in.stickDeadZonePct << '\n';
	out << "substick_deadzone=" << in.substickDeadZonePct << '\n';
	out << "[keys]\n";
	for (const auto& [name, scancode] : in.keys)
		out << name << '=' << scancode << '\n';
	out << "[interface]\n";
	out << "glyph_style=" << glyphName(in.glyphStyle) << '\n';
	out << "[saves]\n";
	out << "directory=" << in.saveDirectory << '\n';
	out << "[launcher]\n";
	out << "last_image=" << in.lastImagePath << '\n';
	return out.str();
}

bool loadSettings(Settings& out, const std::filesystem::path& file)
{
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		out = defaultSettings();
		return false;
	}
	std::ostringstream text;
	text << in.rdbuf();
	out = parseSettings(text.str());
	return true;
}

bool saveSettings(const Settings& in, const std::filesystem::path& file)
{
	std::error_code ec;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), ec);
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	out << serializeSettings(in);
	return static_cast<bool>(out);
}

} // namespace sunshine
