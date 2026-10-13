// Launcher: disc image choice, validation and the settings UI, shown when sunshine.exe starts without --dvd.
// Page flow follows Dusklight's launcher (CC0 1.0, github.com/TwilitRealm/dusklight): one RmlUi document, an input
// layer that owns input while it is open, and settings in one INI file. Layout lives in assets/launcher/launcher.rml.
#include "launcher.hpp"

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>
#include <aurora/input.hpp>
#include <aurora/rmlui.hpp>
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../settings.h"
#include "../glyphs/glyphs.h"
#include "../render/render_scale.h"
#include "../widescreen/widescreen.h"
// Aurora headers by path: the game's include directories shadow <dolphin/...> (see sdk/pad_keys.cpp).
#include "../../extern/aurora/include/dolphin/dvd.h"
#include "../../extern/aurora/include/dolphin/pad.h"

// Aurora VI entry points (extern/aurora/lib/dolphin/vi). Declared locally as render/render_scale.cpp does.
extern "C" {
void VISetWindowFullscreen(bool fullscreen);
void VISetWindowSize(uint32_t width, uint32_t height);
void VISetFrameBufferScale(float scale);
}

// Master volume lives in sdk/dsp_mixer.cpp.
namespace hostdsp {
void setOutputVolumePct(int pct);
}

namespace launcher {
namespace {

using sunshine::Settings;
using sunshine::settings;

// PAD bindings are keyed by the settings names. The tables mirror the layout of sdk/pad_keys.cpp.
struct ButtonSlot {
	const char* key;
	PADButton button;
};
constexpr ButtonSlot kButtonSlots[] = {
	{ "A", PAD_BUTTON_A },       { "B", PAD_BUTTON_B },         { "X", PAD_BUTTON_X },
	{ "Y", PAD_BUTTON_Y },       { "START", PAD_BUTTON_START }, { "Z", PAD_TRIGGER_Z },
	{ "L", PAD_TRIGGER_L },      { "R", PAD_TRIGGER_R },        { "DPAD_UP", PAD_BUTTON_UP },
	{ "DPAD_DOWN", PAD_BUTTON_DOWN }, { "DPAD_LEFT", PAD_BUTTON_LEFT }, { "DPAD_RIGHT", PAD_BUTTON_RIGHT },
};
struct AxisSlot {
	const char* key;
	PADAxis axis;
};
constexpr AxisSlot kAxisSlots[] = {
	{ "STICK_UP", PAD_AXIS_LEFT_Y_POS },   { "STICK_DOWN", PAD_AXIS_LEFT_Y_NEG },
	{ "STICK_LEFT", PAD_AXIS_LEFT_X_NEG }, { "STICK_RIGHT", PAD_AXIS_LEFT_X_POS },
	{ "CSTICK_UP", PAD_AXIS_RIGHT_Y_POS }, { "CSTICK_DOWN", PAD_AXIS_RIGHT_Y_NEG },
	{ "CSTICK_LEFT", PAD_AXIS_RIGHT_X_NEG }, { "CSTICK_RIGHT", PAD_AXIS_RIGHT_X_POS },
	{ "L", PAD_AXIS_TRIGGER_L },           { "R", PAD_AXIS_TRIGGER_R },
};
static_assert(std::size(kButtonSlots) == PAD_BUTTON_COUNT);
static_assert(std::size(kAxisSlots) == PAD_AXIS_COUNT);

// Rebindable inputs in UI order. "L" and "R" also drive the trigger axes (see kAxisSlots).
constexpr const char* kKeyNames[] = {
	"A", "B", "X", "Y", "START", "Z", "L", "R", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
	"STICK_UP", "STICK_DOWN", "STICK_LEFT", "STICK_RIGHT", "CSTICK_UP", "CSTICK_DOWN", "CSTICK_LEFT", "CSTICK_RIGHT",
};
constexpr const char* kPanes[] = { "video", "audio", "controls", "interface", "cheats" };
constexpr const char* kGlyphNames[] = { "Generic", "Xbox", "PlayStation", "Switch", "GameCube" };

struct WindowSize {
	int width;
	int height;
};
constexpr WindowSize kWindowSizes[] = { { 640, 480 }, { 960, 720 }, { 1280, 960 }, { 1600, 1200 } };
constexpr int kScalePresets[] = { 0, 50, 67, 77, 100, 150, 200, 300 }; // 0 = Auto. Below 100 renders a smaller internal frame; FSR 1 (video.fsr1) upscales it.
// ponytail: preset cycling instead of sliders. Values outside a preset (hand-edited INI) cycle back to the first.
constexpr int kFpsPresets[] = { 0, 30, 60, 120, 144 };
constexpr int kUiScalePresets[] = { 0, 75, 100, 150 }; // 0 = window scale (aurora::rmlui::set_ui_scale)
constexpr int kVolumePresets[] = { 100, 75, 50, 25, 0 };
constexpr int kDeadZonePresets[] = { 0, 5, 10, 20, 30 };

constexpr const char* kImageFilter = "iso;rvz;wia;gcz;ciso;wbfs";

template <size_t N>
int nextPreset(const int (&presets)[N], int current) {
	for (size_t i = 0; i < N; ++i) {
		if (presets[i] == current) {
			return presets[(i + 1) % N];
		}
	}
	return presets[0];
}

int keyOf(const Settings& s, const char* name) {
	const auto it = s.keys.find(name);
	if (it != s.keys.end()) {
		return it->second;
	}
	return sunshine::defaultSettings().keys.at(name);
}

void applyPadBindings(const Settings& s) {
	PADKeyButtonBinding buttons[PAD_BUTTON_COUNT];
	PADKeyAxisBinding axes[PAD_AXIS_COUNT];
	for (size_t i = 0; i < std::size(kButtonSlots); ++i) {
		buttons[i] = { keyOf(s, kButtonSlots[i].key), kButtonSlots[i].button };
	}
	for (size_t i = 0; i < std::size(kAxisSlots); ++i) {
		axes[i] = { keyOf(s, kAxisSlots[i].key), kAxisSlots[i].axis, 0 };
	}
	PADSetKeyButtonBindings(0, buttons);
	PADSetKeyAxisBindings(0, axes);
}

void applyUiScale(const Settings& s) {
	aurora::rmlui::set_ui_scale(static_cast<float>(s.uiScalePct) / 100.f); // 0 = window scale
}

void applyDeadZones(const Settings& s) {
	// ponytail: trigger activation zones stay at Aurora's defaults; only stick dead zones are exposed.
	PADDeadZones* zones = PADGetDeadZones(0);
	if (zones == nullptr) {
		return; // no pad bound to player 0 (keyboard only): Aurora returns null, nothing to tune
	}
	zones->useDeadzones = true;
	zones->stickDeadZone = static_cast<uint16_t>(s.stickDeadZonePct * 32767 / 100);
	zones->substickDeadZone = static_cast<uint16_t>(s.substickDeadZonePct * 32767 / 100);
}

// Shared by the launcher and the --dvd path. On success the disc stays open for the game.
std::string escapeRml(std::string_view text) {
	std::string out;
	for (const char c : text) {
		switch (c) {
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		default: out += c; break;
		}
	}
	return out;
}

std::string keyName(int scancode) {
	const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
	return (name != nullptr && *name != '\0') ? escapeRml(name) : "Unknown";
}

// File dialog callback runs on a dialog thread: hand the result to the UI thread under a lock.
std::mutex gDialogMutex;
std::optional<std::string> gDialogPick;

void onDialogResult(void* /*userdata*/, const char* const* filelist, int /*filter*/) {
	if (filelist == nullptr || filelist[0] == nullptr) {
		return; // error or cancelled
	}
	std::lock_guard lock(gDialogMutex);
	gDialogPick = filelist[0];
}

// Keeps the launcher modal: while it is open, every input source is captured.
aurora::input::EventResult onInputThunk(const aurora::input::InputEvent& event, void* userdata);
bool capturesAll(const aurora::input::InputSource& /*source*/, void* /*userdata*/) { return true; }

// Like main's sunshinePresentFrame, but reports window close.
bool pumpEvents() {
	bool alive = true;
	for (const AuroraEvent* e = aurora_update(); e != nullptr && e->type != AURORA_NONE; ++e) {
		if (e->type == AURORA_EXIT) {
			alive = false;
		}
	}
	return alive;
}

bool presentFrame() {
	aurora_end_frame();
	bool alive = pumpEvents();
	while (!aurora_begin_frame()) {
		if (!pumpEvents()) {
			alive = false;
		}
	}
	return alive;
}

class Launcher final : public Rml::EventListener {
public:
	explicit Launcher(std::filesystem::path settingsFile) : file_(std::move(settingsFile)) {}

	bool open() {
		if (!aurora::rmlui::is_initialized()) {
			return false;
		}
		ctx_ = aurora::rmlui::get_context();
		if (ctx_ == nullptr) {
			return false;
		}
		applyUiScale(settings()); // main calls launcher::run before applyRuntimeSettings(), so apply the saved scale here
		if (!Rml::LoadFontFace("assets/launcher/LatoLatin-Regular.ttf") ||
		    !Rml::LoadFontFace("assets/launcher/LatoLatin-Bold.ttf")) {
			return false;
		}
		doc_ = ctx_->LoadDocument("assets/launcher/launcher.rml");
		if (doc_ == nullptr) {
			return false;
		}
		doc_->AddEventListener("click", this);
		doc_->Show();

		aurora::input::LayerDescriptor desc{};
		desc.label = "sunshine.launcher";
		desc.priority = aurora::input::kRmlUiLayerPriority;
		desc.onEvent = &onInputThunk;
		desc.capturesSource = &capturesAll;
		desc.userdata = this;
		layer_ = aurora::input::register_layer(desc);

		showPage(false);
		showPane("video");
		// Dev hook for screenshots: SUNSHINE_DEV_LAUNCHER_PAGE=settings or settings/<pane> opens that page at start.
		if (const char* dev = std::getenv("SUNSHINE_DEV_LAUNCHER_PAGE")) {
			const std::string spec = dev;
			if (spec.rfind("settings", 0) == 0) {
				showPage(true);
				if (spec.size() > 9) {
					showPane(spec.substr(9));
				}
			}
		}
		const std::string& last = settings().lastImagePath;
		if (!last.empty()) {
			imagePath_ = last;
			validate();
		}
		// Dev hook for tests: SUNSHINE_DEV_LAUNCHER_ACTIONS=id,id,... runs those actions in order (e.g. fsr1,scale,start).
		if (const char* actions = std::getenv("SUNSHINE_DEV_LAUNCHER_ACTIONS")) {
			std::string list = actions;
			size_t begin = 0;
			while (begin < list.size()) {
				size_t end = list.find(',', begin);
				if (end == std::string::npos)
					end = list.size();
				const std::string id = list.substr(begin, end - begin);
				std::printf("[launcher] action %s\n", id.c_str());
				onAction(id);
				begin = end + 1;
			}
		}
		// Dev hook for tests: SUNSHINE_DEV_PAD_EVENTS=token,... replays synthetic gamepad tokens (see runPadToken).
		if (const char* pad = std::getenv("SUNSHINE_DEV_PAD_EVENTS")) {
			std::string list = pad;
			size_t begin = 0;
			while (begin < list.size()) {
				size_t end = list.find(',', begin);
				if (end == std::string::npos)
					end = list.size();
				padScript_.push_back(list.substr(begin, end - begin));
				begin = end + 1;
			}
		}
		ensureFocus();
		refresh();
		return true;
	}

	void close() {
		if (layer_ != aurora::input::kInvalidLayerId) {
			aurora::input::unregister_layer(layer_);
			layer_ = aurora::input::kInvalidLayerId;
		}
		if (doc_ != nullptr) {
			doc_->RemoveEventListener("click", this);
			ctx_->UnloadDocument(doc_);
			doc_ = nullptr;
		}
	}

	void poll() {
		std::optional<std::string> pick;
		{
			std::lock_guard lock(gDialogMutex);
			pick.swap(gDialogPick);
		}
		if (pick) {
			imagePath_ = *pick;
			validate();
		}
		// Dev hook: one synthetic gamepad token every kPadStepFrames frames (poll runs once per frame).
		if (padNext_ < padScript_.size() && ++padWait_ >= kPadStepFrames) {
			padWait_ = 0;
			runPadToken(padScript_[padNext_++]);
		}
	}

	bool finished() const { return finished_; }
	bool started() const { return started_; }

	void ProcessEvent(Rml::Event& event) override {
		Rml::Element* target = event.GetTargetElement();
		if (target != nullptr) {
			onAction(target->GetId());
		}
	}

	aurora::input::EventResult onInput(const aurora::input::InputEvent& event) {
		if (const auto* key = event.payload.get_if<aurora::input::InputEvent::KeyChanged>()) {
			if (!capture_.empty() && key->pressed && !key->repeat) {
				if (key->scancode != SDL_SCANCODE_ESCAPE) {
					settings().keys[capture_] = key->scancode;
					changed();
				}
				capture_.clear();
				refresh();
				return aurora::input::EventResult::Consume;
			}
		}
		if (handlePad(event)) {
			return aurora::input::EventResult::Consume;
		}
		aurora::rmlui::process_input(event);
		return aurora::input::EventResult::Consume;
	}

private:
	// Gamepad input (UI-3). Layer events arrive in onInput; returns true for gamepad events, which are always consumed.
	bool handlePad(const aurora::input::InputEvent& event) {
		if (const auto* button = event.payload.get_if<aurora::input::InputEvent::ButtonChanged>()) {
			if (!button->pressed) {
				return true;
			}
			if (!capture_.empty()) { // a rebind waits for a key; B cancels it
				if (button->button == SDL_GAMEPAD_BUTTON_EAST) {
					capture_.clear();
					refresh();
				}
				return true;
			}
			// No visible focus (startup, or a mouse page change hid it): A and the d-pad only place focus; B and Start act at once.
			if (button->button != SDL_GAMEPAD_BUTTON_START && button->button != SDL_GAMEPAD_BUTTON_EAST && !ensureFocus()) {
				return true;
			}
			switch (button->button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: navigate(0, -1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: navigate(0, 1); break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: navigate(-1, 0); break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: navigate(1, 0); break;
			case SDL_GAMEPAD_BUTTON_SOUTH: activateFocused(); break;
			case SDL_GAMEPAD_BUTTON_EAST: goBack(); break;
			case SDL_GAMEPAD_BUTTON_START: onAction("start"); break;
			default: break;
			}
			return true;
		}
		if (const auto* axis = event.payload.get_if<aurora::input::InputEvent::AxisChanged>()) {
			if (capture_.empty() && (axis->axis == SDL_GAMEPAD_AXIS_LEFTX || axis->axis == SDL_GAMEPAD_AXIS_LEFTY)) {
				const float v = axis->value;
				const float neutral = aurora::input::kAxisNeutral;
				if (axis->axis == SDL_GAMEPAD_AXIS_LEFTY) {
					stickEdge(stickHeld_[0], v < -neutral, 0, -1); // up
					stickEdge(stickHeld_[1], v > neutral, 0, 1);   // down
				} else {
					stickEdge(stickHeld_[2], v < -neutral, -1, 0); // left
					stickEdge(stickHeld_[3], v > neutral, 1, 0);   // right
				}
			}
			return true;
		}
		return false;
	}

	void stickEdge(bool& held, bool active, int dx, int dy) {
		if (active && !held) {
			navigate(dx, dy);
		}
		held = active;
	}

	// Visible buttons of the open page and pane, in document order. Hidden pages and panes are skipped.
	std::vector<Rml::Element*> padTargets() {
		Rml::ElementList all;
		doc_->GetElementsByTagName(all, "button");
		std::vector<Rml::Element*> out;
		for (Rml::Element* el : all) {
			if (el->IsVisible(true)) {
				out.push_back(el);
			}
		}
		return out;
	}

	// The focused element when it is a visible button, else nullptr.
	Rml::Element* focusedButton() {
		Rml::Element* active = ctx_->GetFocusElement();
		for (Rml::Element* el : padTargets()) {
			if (el == active) {
				return el;
			}
		}
		return nullptr;
	}

	// Focus a button and scroll its nearest scroll container so the button is fully visible. RmlUi Focus does not scroll.
	static void focusTarget(Rml::Element* el) {
		el->Focus(true);
		el->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest,
		                                              Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest));
	}

	// Pad input needs a visible focus. Returns true when a visible button already has it; otherwise focuses the first one.
	bool ensureFocus() {
		ctx_->Update(); // visibility depends on the current layout
		if (focusedButton() != nullptr) {
			return true;
		}
		const std::vector<Rml::Element*> targets = padTargets();
		if (!targets.empty()) {
			focusTarget(targets.front());
		}
		return false;
	}

	bool settingsShown() {
		Rml::Element* page = doc_->GetElementById("page-settings");
		return page != nullptr && page->IsVisible();
	}

	struct Point {
		float x;
		float y;
	};
	static Point centerOf(Rml::Element* el) {
		return { el->GetAbsoluteLeft() + el->GetOffsetWidth() * 0.5f, el->GetAbsoluteTop() + el->GetOffsetHeight() * 0.5f };
	}

	// Moves focus to the nearest visible button in direction (dx, dy); y grows downward. Stays put at the edge.
	void navigate(int dx, int dy) {
		ctx_->Update(); // geometry below needs the layout of the current frame
		const std::vector<Rml::Element*> targets = padTargets();
		if (targets.empty()) {
			return;
		}
		Rml::Element* from = focusedButton();
		if (from == nullptr) { // nothing focused yet: start at the first control in document order
			focusTarget(targets.front());
			return;
		}
		const Point f = centerOf(from);
		Rml::Element* best = nullptr;
		float bestScore = 0.f;
		for (Rml::Element* el : targets) {
			if (el == from) {
				continue;
			}
			const Point p = centerOf(el);
			const float ahead = (p.x - f.x) * dx + (p.y - f.y) * dy;
			if (ahead <= 0.f) {
				continue;
			}
			const float across = std::abs((p.x - f.x) * dy) + std::abs((p.y - f.y) * dx);
			const float score = ahead + 2.f * across;
			if (best == nullptr || score < bestScore) {
				best = el;
				bestScore = score;
			}
		}
		if (best != nullptr) {
			focusTarget(best);
		}
	}

	void activateFocused() {
		if (Rml::Element* el = focusedButton()) {
			if (el->GetId() == "back") { // A on Back does what B does
				leaveSettings();
				return;
			}
			el->Click(); // runs the click listener, i.e. onAction(id)
			ensureFocus();
		}
	}

	// Back to the main page (B, or A on the Back button); focus lands on Settings, the button that opened it.
	void leaveSettings() {
		onAction("back");
		ctx_->Update();
		if (Rml::Element* opener = doc_->GetElementById("open-settings")) {
			opener->Focus(true);
		}
	}

	// B on the settings page.
	void goBack() {
		if (!settingsShown()) {
			return;
		}
		leaveSettings();
	}

	// Dev hook: runs one synthetic gamepad token through onInput, so it takes the same path as a real pad event.
	// Tokens: up|down|left|right (D-pad), a|b|start (buttons), lstick-up|lstick-down|lstick-left|lstick-right (left stick).
	void runPadToken(const std::string& token) {
		using aurora::input::InputEvent;
		const auto send = [this](InputEvent::Payload payload) {
			InputEvent event{};
			event.source.kind = aurora::input::InputSource::Kind::Controller;
			event.source.origin = aurora::input::InputSource::Origin::Synthetic;
			event.payload = std::move(payload);
			onInput(event);
		};
		const auto press = [&](SDL_GamepadButton button) {
			send(InputEvent::ButtonChanged{ button, true });
			send(InputEvent::ButtonChanged{ button, false });
		};
		const auto flick = [&](SDL_GamepadAxis axis, float value) {
			send(InputEvent::AxisChanged{ axis, value });
			send(InputEvent::AxisChanged{ axis, 0.f });
		};
		if (token == "up") {
			press(SDL_GAMEPAD_BUTTON_DPAD_UP);
		} else if (token == "down") {
			press(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
		} else if (token == "left") {
			press(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
		} else if (token == "right") {
			press(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
		} else if (token == "a") {
			press(SDL_GAMEPAD_BUTTON_SOUTH);
		} else if (token == "b") {
			press(SDL_GAMEPAD_BUTTON_EAST);
		} else if (token == "start") {
			press(SDL_GAMEPAD_BUTTON_START);
		} else if (token == "lstick-up") {
			flick(SDL_GAMEPAD_AXIS_LEFTY, -1.f);
		} else if (token == "lstick-down") {
			flick(SDL_GAMEPAD_AXIS_LEFTY, 1.f);
		} else if (token == "lstick-left") {
			flick(SDL_GAMEPAD_AXIS_LEFTX, -1.f);
		} else if (token == "lstick-right") {
			flick(SDL_GAMEPAD_AXIS_LEFTX, 1.f);
		} else {
			std::printf("[launcher] pad %s: unknown token\n", token.c_str());
			return;
		}
		Rml::Element* focused = focusedButton();
		std::printf("[launcher] pad %s page=%s focus=%s\n", token.c_str(), settingsShown() ? "settings" : "main",
		            focused != nullptr ? focused->GetId().c_str() : "none");
		std::fflush(stdout); // dev harness: SUNSHINE_DEV_MAX_SECONDS ends the process with TerminateProcess, which drops buffered stdout
	}

	void setText(const char* id, const std::string& text) {
		if (Rml::Element* el = doc_->GetElementById(id)) {
			el->SetInnerRML(text);
		}
	}

	void setDisplay(const std::string& id, bool shown) {
		if (Rml::Element* el = doc_->GetElementById(id)) {
			el->SetProperty("display", shown ? "block" : "none");
		}
	}

	void setActive(const std::string& id, bool active) {
		if (Rml::Element* el = doc_->GetElementById(id)) {
			el->SetClass("active", active);
		}
	}

	void showPage(bool settingsPage) {
		setDisplay("page-main", !settingsPage);
		setDisplay("page-settings", settingsPage);
	}

	void showPane(const std::string& name) {
		for (const char* pane : kPanes) {
			const std::string key = pane;
			setDisplay("pane-" + key, key == name);
			setActive("tab-" + key, key == name);
		}
	}

	void persist() {
		if (sunshine::saveSettings(settings(), file_)) {
			setText("settings-status", "Saved to " + escapeRml(file_.string()));
		} else {
			setText("settings-status", "Could not write " + escapeRml(file_.string()));
		}
	}

	// Applies, saves and redraws after a settings change.
	void changed() {
		applyRuntimeSettings();
		persist();
		refresh();
	}

	void refresh() {
		const Settings& s = settings();
		const sunshine::Settings def = sunshine::defaultSettings();
		int width = s.windowWidth;
		int height = s.windowHeight;

		setText("image-path", imagePath_.empty() ? "No image selected" : escapeRml(imagePath_));
		setText("window-mode", std::string("Window mode: ") + (s.fullscreen ? "Fullscreen" : "Windowed"));
		setText("window-size", "Window size: " + std::to_string(width) + " x " + std::to_string(height));
		setText("vsync", std::string("VSync: ") + (s.vsync ? "On" : "Off") + " (applies at next start)");
		setText("scale", std::string("Resolution scale: ") +
		                     (s.resolutionScalePct == 0 ? "Auto" : std::to_string(s.resolutionScalePct) + "%"));
		setText("fps", std::string("FPS limit: ") + (s.fpsLimit == 0 ? "Unlimited" : std::to_string(s.fpsLimit)));
		setText("widescreen", std::string("Widescreen: ") + (s.widescreen ? "On" : "Off") + " (" + s.aspect + ")");
		setText("aspect", "Aspect ratio: " + s.aspect + " (applies at next start)");
		setText("remember-window-size", std::string("Remember window size: ") + (s.rememberWindowSize ? "On" : "Off") +
		                                   (s.rememberWindowSize ? "" : " (opens at " + std::to_string(def.windowWidth) + " x " + std::to_string(def.windowHeight) + ")"));
		setText("fsr1", std::string("FSR 1 upscale: ") + (s.fsr1 ? "On" : "Off"));
		setText("volume", "Master volume: " + std::to_string(s.masterVolumePct) + "%");
		setText("stick-deadzone", "Stick dead zone: " + std::to_string(s.stickDeadZonePct) + "%");
		setText("substick-deadzone", "C-stick dead zone: " + std::to_string(s.substickDeadZonePct) + "%");
		setText("glyph-style", std::string("Button glyphs: ") + kGlyphNames[static_cast<int>(s.glyphStyle)]);
		setText("ui-scale", std::string("UI scale: ") + (s.uiScalePct == 0 ? "Window scale" : std::to_string(s.uiScalePct) + "%"));
		for (const char* name : kKeyNames) {
			const std::string id = std::string("bind-") + name;
			const std::string label = capture_ == name ? "press a key..." : keyName(keyOf(s, name));
			setText(id.c_str(), std::string(name) + ": " + label);
		}
	}

	// Reads the picked file's header; leaves the disc closed (Start reopens it).
	void validate() {
		std::string error;
		if (!launcher::openDisc(imagePath_, error)) {
			setText("image-status", escapeRml(error));
			return;
		}
		aurora_dvd_close();
		setText("image-status", "Valid: Super Mario Sunshine (GMSJ01, NTSC-J).");
	}

	void chooseImage() {
		static const SDL_DialogFileFilter filters[] = { { "Disc images", kImageFilter } };
		SDL_ShowOpenFileDialog(&onDialogResult, nullptr, aurora_get_window(), filters, 1, nullptr, false);
	}

	void startGame() {
		if (imagePath_.empty()) {
			setText("image-status", "Choose a disc image first.");
			return;
		}
		std::string error;
		if (!launcher::openDisc(imagePath_, error)) {
			setText("image-status", escapeRml(error));
			return;
		}
		settings().lastImagePath = imagePath_;
		persist();
		started_ = true;
		finished_ = true;
	}

	void onAction(const std::string& id) {
		Settings& s = settings();
		if (id == "browse") {
			chooseImage();
			return;
		}
		if (id == "start") {
			startGame();
			return;
		}
		if (id == "quit") {
			finished_ = true;
			return;
		}
		if (id == "open-settings") {
			showPage(true);
			return;
		}
		if (id == "back") {
			showPage(false);
			return;
		}
		if (id.rfind("tab-", 0) == 0) {
			showPane(id.substr(4));
			return;
		}
		if (id.rfind("bind-", 0) == 0) {
			capture_ = id.substr(5);
			refresh();
			return;
		}

		if (id == "window-mode") {
			s.fullscreen = !s.fullscreen;
		} else if (id == "window-size") {
			size_t i = 0;
			for (; i < std::size(kWindowSizes); ++i) {
				if (kWindowSizes[i].width == s.windowWidth && kWindowSizes[i].height == s.windowHeight) {
					break;
				}
			}
			const WindowSize next = kWindowSizes[(i + 1) % std::size(kWindowSizes)];
			s.windowWidth = next.width;
			s.windowHeight = next.height;
		} else if (id == "vsync") {
			s.vsync = !s.vsync;
		} else if (id == "scale") {
			s.resolutionScalePct = nextPreset(kScalePresets, s.resolutionScalePct);
		} else if (id == "fps") {
			s.fpsLimit = nextPreset(kFpsPresets, s.fpsLimit);
		} else if (id == "widescreen") {
			s.widescreen = !s.widescreen;
		} else if (id == "fsr1") {
			s.fsr1 = !s.fsr1;
		} else if (id == "volume") {
			s.masterVolumePct = nextPreset(kVolumePresets, s.masterVolumePct);
		} else if (id == "stick-deadzone") {
			s.stickDeadZonePct = nextPreset(kDeadZonePresets, s.stickDeadZonePct);
		} else if (id == "substick-deadzone") {
			s.substickDeadZonePct = nextPreset(kDeadZonePresets, s.substickDeadZonePct);
		} else if (id == "glyph-style") {
			s.glyphStyle = static_cast<sunshine::GlyphStyle>((static_cast<int>(s.glyphStyle) + 1) % 5);
		} else if (id == "remember-window-size") {
			s.rememberWindowSize = !s.rememberWindowSize;
		} else if (id == "aspect") {
			static constexpr const char* kAspects[] = { "16:9", "4:3", "16:10", "21:9", "32:9", "auto" };
			size_t i = 0;
			while (i < std::size(kAspects) && s.aspect != kAspects[i]) {
				++i;
			}
			s.aspect = kAspects[(i + 1) % std::size(kAspects)];
		} else if (id == "ui-scale") {
			s.uiScalePct = nextPreset(kUiScalePresets, s.uiScalePct);
		} else if (id == "reset-keys") {
			s.keys = sunshine::defaultSettings().keys;
			capture_.clear();
		} else {
			return;
		}
		changed();
	}

	std::filesystem::path file_;
	Rml::Context* ctx_ = nullptr;
	Rml::ElementDocument* doc_ = nullptr;
	aurora::input::LayerId layer_ = aurora::input::kInvalidLayerId;
	std::string imagePath_;
	std::string capture_; // settings key waiting for the next key press
	static constexpr int kPadStepFrames = 30; // frames between synthetic gamepad tokens (dev hook)
	bool stickHeld_[4] = {};                  // left stick: up, down, left, right; one step per crossing of the neutral band
	std::vector<std::string> padScript_;      // SUNSHINE_DEV_PAD_EVENTS tokens
	size_t padNext_ = 0;
	int padWait_ = 0;
	bool finished_ = false;
	bool started_ = false;
};

aurora::input::EventResult onInputThunk(const aurora::input::InputEvent& event, void* userdata) {
	return static_cast<Launcher*>(userdata)->onInput(event);
}

} // namespace

bool openDisc(const std::string& path, std::string& error) {
	if (!aurora_dvd_open(path.c_str())) {
		error = "Could not open this file as a disc image.";
		return false;
	}
	const DVDDiskID* id = DVDGetCurrentDiskID();
	// Game code "GMSJ" with region 'J' is NTSC-J; company "01" is Nintendo.
	if (std::memcmp(id->gameName, "GMSJ", 4) != 0 || std::memcmp(id->company, "01", 2) != 0) {
		aurora_dvd_close();
		error = "Not Super Mario Sunshine (GMSJ01, NTSC-J).";
		return false;
	}
	return true;
}

bool run(const std::filesystem::path& settingsFile) {
	Launcher ui(settingsFile);
	if (!ui.open()) {
		std::fprintf(stderr, "launcher: could not load assets/launcher (the assets folder must sit next to sunshine.exe)\n");
		return false;
	}
	// Opens the frame the first draw needs. The game's begin_frame in main runs after the launcher closes its frame.
	while (!aurora_begin_frame()) {
		aurora_update();
	}
	bool alive = true;
	while (alive && !ui.finished()) {
		ui.poll();
		alive = presentFrame();
		SDL_Delay(1); // without vsync, presentFrame returns at once; do not spin a core
	}
	ui.close();
	aurora_end_frame();
	return alive && ui.started();
}

void applyRuntimeSettings() {
	const Settings& s = settings();
	applyPadBindings(s);
	applyDeadZones(s);
	applyUiScale(s);
	// Generic has no art set of its own: it shows the Xbox set, which glyphs.cpp loads by default.
	switch (s.glyphStyle) {
	case sunshine::GlyphStyle::PlayStation:
		glyphsSetStyle(::GlyphStyle::PlayStation);
		break;
	case sunshine::GlyphStyle::Switch:
		glyphsSetStyle(::GlyphStyle::Switch);
		break;
	case sunshine::GlyphStyle::GameCube:
		glyphsSetStyle(::GlyphStyle::GameCube);
		break;
	default:
		glyphsSetStyle(::GlyphStyle::Xbox);
		break;
	}
	hostdsp::setOutputVolumePct(s.masterVolumePct);
	VISetWindowFullscreen(s.fullscreen);
	// remember_window_size=0: the window always opens at the default size; the saved size is kept for the next time it is on.
	const sunshine::Settings def = sunshine::defaultSettings();
	const auto winW = static_cast<uint32_t>(s.rememberWindowSize ? s.windowWidth : def.windowWidth);
	const auto winH = static_cast<uint32_t>(s.rememberWindowSize ? s.windowHeight : def.windowHeight);
	VISetWindowSize(widescreenWindowWidth(winW, winH), winH);
	// SUNSHINE_RENDER_SCALE (render/render_scale.cpp) overrides the saved scale.
	if (std::getenv("SUNSHINE_RENDER_SCALE") == nullptr) {
		VISetFrameBufferScale(static_cast<float>(s.resolutionScalePct) / 100.f);
	}
	sunshineApplyUpscaler(s.fsr1); // render/render_scale.cpp: FSR 1 or bilinear, unless SUNSHINE_UPSCALER is set
}

} // namespace launcher
