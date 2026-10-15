// Round-trip and clamping checks for src/settings.cpp. Standalone: no Aurora, no SDL.
#undef NDEBUG  // the checks must run in release builds too
#include "../src/settings.h"

#include <cassert>
#include <cstdio>
#include <filesystem>

using namespace sunshine;

int main()
{
	// Defaults survive serialize -> parse unchanged.
	const Settings def = defaultSettings();
	const std::string defText = serializeSettings(def);
	assert(serializeSettings(parseSettings(defText)) == defText);
	assert(!def.skipLauncher && defText.find("skip_launcher=0") != std::string::npos);

	// Non-default values round-trip.
	Settings s = defaultSettings();
	s.fullscreen = true;
	s.windowWidth = 1920;
	s.windowHeight = 1080;
	s.vsync = false;
	s.resolutionScalePct = 200;
	s.fpsLimit = 60;
	s.widescreen = true;
	s.aspect = "21:9";
	s.rememberWindowSize = false;
	s.uiScalePct = 75;
	s.masterVolumePct = 35;
	s.outputDevice = "{0.0.0.00000000}.{8d1a4c4e-6c0f-4f63-9d6e-2b7a1e5c3f10}";
	s.stickDeadZonePct = 25;
	s.substickDeadZonePct = 0;
	s.keys["A"] = 26;
	s.glyphStyle = GlyphStyle::PlayStation;
	s.lastImagePath = "C:/games/Super Mario Sunshine (Japan).rvz";
	s.skipLauncher = true;
	s.menuSounds = false;
	const std::string text = serializeSettings(s);
	const Settings back = parseSettings(text);
	assert(back.fullscreen && back.windowWidth == 1920 && back.windowHeight == 1080 && !back.vsync);
	assert(back.resolutionScalePct == 200 && back.fpsLimit == 60 && back.widescreen && back.aspect == "21:9");
	assert(back.masterVolumePct == 35 && back.stickDeadZonePct == 25 && back.substickDeadZonePct == 0);
	assert(back.outputDevice == s.outputDevice);
	assert(back.keys.at("A") == 26 && back.glyphStyle == GlyphStyle::PlayStation);
	assert(!back.rememberWindowSize && back.uiScalePct == 75);
	assert(back.lastImagePath == s.lastImagePath && back.skipLauncher);
	assert(!back.menuSounds && defText.find("menu_sounds=1") != std::string::npos);
	assert(parseSettings("").menuSounds && !parseSettings("[audio]\nmenu_sounds=0\n").menuSounds);
	assert(serializeSettings(back) == text);
	for (GlyphStyle g : {GlyphStyle::Switch, GlyphStyle::GameCube, GlyphStyle::Keyboard, GlyphStyle::Generic}) {
		Settings gs;
		gs.glyphStyle = g;
		assert(parseSettings(serializeSettings(gs)).glyphStyle == g);
	}
	{ // the INI name is the one the glyph loader's style directory and the launcher row use
		Settings ks;
		ks.glyphStyle = GlyphStyle::Keyboard;
		assert(serializeSettings(ks).find("glyph_style=keyboard\n") != std::string::npos);
		assert(parseSettings("[interface]\nglyph_style=keyboard\n").glyphStyle == GlyphStyle::Keyboard);
	}

	// Out-of-range values are clamped; garbage and unknown keys fall back or are ignored.
	const Settings odd = parseSettings("[video]\nwidth=99999\nheight=abc\nfps_limit=-4\nmystery=1\n"
	                                   "[audio]\nmaster_volume=250\n[controls]\nstick_deadzone=90\n");
	assert(odd.windowWidth == 7680 && odd.windowHeight == def.windowHeight && odd.fpsLimit == 0);
	assert(odd.masterVolumePct == 100 && odd.stickDeadZonePct == 50);
	assert(parseSettings("[video]\naspect=5:4\n").aspect == def.aspect);
	assert(parseSettings("[interface]\nui_scale=999\n").uiScalePct == 400);
	assert(parseSettings("[interface]\nui_scale=-5\n").uiScalePct == 0);
	assert(parseSettings("[interface]\nui_scale=abc\n").uiScalePct == def.uiScalePct);
	assert(parseSettings("[video]\nremember_window_size=7\n").rememberWindowSize);
	for (const char* a : { "4:3", "16:9", "16:10", "21:9", "32:9", "auto" }) {
		Settings as;
		as.aspect = a;
		assert(parseSettings(serializeSettings(as)).aspect == a);
	}

	// Cheat toggles: explicit on/off per id; invalid values are skipped.
	Settings ch = defaultSettings(); // keys need defaults, or the round trip adds them and the texts differ
	ch.cheats["infinite_health"] = true;
	ch.cheats["unlock_nozzles"] = false;
	const std::string chText = serializeSettings(ch);
	const Settings chBack = parseSettings(chText);
	assert(chBack.cheats.size() == 2 && chBack.cheats.at("infinite_health") && !chBack.cheats.at("unlock_nozzles"));
	assert(serializeSettings(chBack) == chText);
	assert(parseSettings("[cheats]\ninfinite_coins=abc\ninfinite_lives=1\n").cheats.size() == 1);

	// Mod toggles: "off" is stored explicitly; absent ids stay on, invalid values are skipped.
	Settings md = defaultSettings(); // keys need defaults, or the round trip adds them and the texts differ
	md.mods["hello"] = false;
	const std::string mdText = serializeSettings(md);
	const Settings mdBack = parseSettings(mdText);
	assert(mdBack.mods.size() == 1 && !mdBack.mods.at("hello"));
	assert(serializeSettings(mdBack) == mdText);
	const Settings mdOdd = parseSettings("[mods]\nhello=0\nfaulty=1\nbad=abc\n"); // non-numeric is skipped; a number is clamped like every bool key
	assert(mdOdd.mods.at("hello") == false && mdOdd.mods.at("faulty") == true && mdOdd.mods.count("bad") == 0);

	// File round trip through saveSettings / loadSettings.
	const std::filesystem::path file = std::filesystem::temp_directory_path() / "sunshine_settings_test.ini";
	assert(saveSettings(s, file));
	Settings fromFile;
	assert(loadSettings(fromFile, file));
	assert(serializeSettings(fromFile) == text);
	std::error_code ec;
	std::filesystem::remove(file, ec);

	// Missing file leaves defaults and reports false.
	Settings missing;
	missing.masterVolumePct = 7;
	assert(!loadSettings(missing, file));
	assert(missing.masterVolumePct == def.masterVolumePct);

	std::printf("settings_test: ok\n");
	return 0;
}
