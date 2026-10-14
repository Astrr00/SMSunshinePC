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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "../settings.h"
#include "../glyphs/glyphs.h"
#include "../render/render_scale.h"
#include "../widescreen/widescreen.h"
#include "../cheats/cheats.h"
#include "../mods/mods_loader.h"
#include "../sdk/av.hpp"
// Aurora headers by path: the game's include directories shadow <dolphin/...> (see sdk/pad_keys.cpp).
#include "../../extern/aurora/include/dolphin/dvd.h"
#include "../../extern/aurora/include/dolphin/pad.h"

// Aurora VI entry points (extern/aurora/lib/dolphin/vi). Declared locally as render/render_scale.cpp does.
extern "C" {
void VISetWindowFullscreen(bool fullscreen);
void VISetWindowSize(uint32_t width, uint32_t height);
void VISetFrameBufferScale(float scale);
}

bool sunshineCanReplay();           // sms/src/System/MarDirectorDirect.cpp: gameplay (director state 4), a frame can be redrawn
u32 sunshineLogicTickCount();      // src/framerate/framerate.cpp

namespace launcher {
namespace {

using sunshine::Settings;
using sunshine::settings;

// Name shown for the saved output device; an endpoint that is not active shows the Windows default (the fallback the
// audio output takes, sdk/av_win.cpp).
std::string outputDeviceName(const std::string& id) {
	for (const av::AudioDevice& d : av::AudioOutputDevices())
		if (d.id == id)
			return d.name;
	return "Windows default";
}

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
constexpr const char* kPanes[] = { "video", "audio", "controls", "interface", "cheats", "mods" };
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

// Cheat toggles from [cheats]. Absent ids keep the JSON default. Needs cheatsLoad first (it resets the registry).
void applyCheats(const Settings& s) {
	for (int i = 0; i < cheatCount(); ++i) {
		CheatInfo info{};
		if (cheatAt(i, info)) {
			const auto it = s.cheats.find(info.id);
			if (!info.oneShot && it != s.cheats.end()) {
				cheatSet(info.id, it->second);
			}
		}
	}
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

// Lato is registered once per process: the launcher and every in-game menu open share it.
bool loadFonts() {
	static bool loaded = false;
	if (!loaded) {
		loaded = Rml::LoadFontFace("assets/launcher/LatoLatin-Regular.ttf") &&
		         Rml::LoadFontFace("assets/launcher/LatoLatin-Bold.ttf");
	}
	return loaded;
}

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
	// inGame: the in-game menu (menuPoll), over the frozen game; otherwise the launcher before the game starts.
	explicit Launcher(std::filesystem::path settingsFile, bool inGame = false)
	    : file_(std::move(settingsFile)), inGame_(inGame) {}

	bool open() {
		if (!aurora::rmlui::is_initialized()) {
			return false;
		}
		ctx_ = aurora::rmlui::get_context();
		if (ctx_ == nullptr) {
			return false;
		}
		applyUiScale(settings()); // main calls launcher::run before applyRuntimeSettings(), so apply the saved scale here
		if (!loadFonts()) {
			return false;
		}
		doc_ = ctx_->LoadDocument("assets/launcher/launcher.rml");
		if (doc_ == nullptr) {
			return false;
		}
		doc_->AddEventListener("click", this);
		doc_->AddEventListener(Rml::EventId::Transitionend, this); // page and pane fade-in (theme.rcss)
		// Dev log of each finished fade, for runs driven by the pad hook or a devscript (in-game menu tokens).
		logTransitions_ = std::getenv("SUNSHINE_DEV_PAD_EVENTS") != nullptr || std::getenv("SUNSHINE_DEV_SCRIPT") != nullptr;
		doc_->Show();
		if (inGame_) {
			// In game the cheat registry is live (loaded by the launcher before the game, or by main.cpp when it was
			// empty; cheatsLoad would reset it), the disc stays open (no validate), the menu layer (menuInstall)
			// routes input here, and only the settings page is shown.
			const char* base = SDL_GetBasePath();
			buildCheatList();
			buildModList(base != nullptr ? base : ".");
			setText("back", "Resume");
			showPage(true);
			showPane("video");
			ensureFocus();
			refresh();
			return true;
		}
		// Cheats are listed from assets/cheats.json next to the exe. main keeps this registry (main.cpp loads only
		// when it is empty, cheatCount() == 0), so the one-shots picked here reach the game.
		// main.cpp uses argv[0]'s parent instead of SDL_GetBasePath(); they match when argv[0] is absolute [INFERENCE].
		const char* base = SDL_GetBasePath();
		cheatsLoad(base != nullptr ? base : ".");
		applyCheats(settings());
		buildCheatList();
		buildModList(base != nullptr ? base : ".");

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
			doc_->RemoveEventListener(Rml::EventId::Transitionend, this);
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
	// In game: why the menu should close ("b", "start", "escape", "f1", "back", "a"); empty while it stays open.
	const std::string& closeRequest() const { return closeWhy_; }
	std::string focusId() {
		Rml::Element* focused = focusedButton();
		return focused != nullptr ? focused->GetId() : "none";
	}

	void ProcessEvent(Rml::Event& event) override {
		Rml::Element* target = event.GetTargetElement();
		if (event.GetId() == Rml::EventId::Transitionend) { // not an action; the dev log only
			if (logTransitions_ && target != nullptr) {
				std::printf("[%s] transition end %s\n", inGame_ ? "menu" : "launcher", target->GetId().c_str());
				std::fflush(stdout);
			}
			return;
		}
		if (target != nullptr) {
			onAction(target->GetId());
		}
	}

	aurora::input::EventResult onInput(const aurora::input::InputEvent& event) {
		if (const auto* key = event.payload.get_if<aurora::input::InputEvent::KeyChanged>()) {
			if (!capture_.empty() && key->pressed && !key->repeat) {
				// Escape cancels; F1 cancels too: the in-game menu layer consumes F1 in gameplay, so the game never sees it.
				if (key->scancode != SDL_SCANCODE_ESCAPE && key->scancode != SDL_SCANCODE_F1) {
					settings().keys[capture_] = key->scancode;
					changed();
				}
				capture_.clear();
				refresh();
				return aurora::input::EventResult::Consume;
			}
				if (key->pressed && !key->repeat && key->scancode == SDL_SCANCODE_ESCAPE && confirmShown()) {
					closeConfirm();
					return aurora::input::EventResult::Consume;
				}
			if (inGame_ && key->pressed && !key->repeat && !confirmShown() &&
			    (key->scancode == SDL_SCANCODE_ESCAPE || key->scancode == SDL_SCANCODE_F1)) {
				closeWhy_ = key->scancode == SDL_SCANCODE_F1 ? "f1" : "escape";
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
			case SDL_GAMEPAD_BUTTON_START:
				if (!confirmShown()) {
					if (inGame_) {
						closeWhy_ = "start"; // in game Start resumes; it never reaches startGame
					} else {
						onAction("start");
					}
				}
				break;
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

	// Compares box edges, not centres: a wide button beside a narrow one must not lose to a centred one further away.
	static float perpendicularGap(Rml::Element* a, Rml::Element* b, int dx) {
		const bool sideways = dx != 0;
		const float a0 = sideways ? a->GetAbsoluteTop() : a->GetAbsoluteLeft();
		const float a1 = a0 + (sideways ? a->GetOffsetHeight() : a->GetOffsetWidth());
		const float b0 = sideways ? b->GetAbsoluteTop() : b->GetAbsoluteLeft();
		const float b1 = b0 + (sideways ? b->GetOffsetHeight() : b->GetOffsetWidth());
		return std::max(0.f, std::max(a0, b0) - std::min(a1, b1));
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
			const float across = perpendicularGap(from, el, dx);
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
				leaveSettings("a");
				return;
			}
			el->Click(); // runs the click listener, i.e. onAction(id)
			ensureFocus();
		}
	}

	// Back to the main page (B, or A on the Back button); focus lands on Settings, the button that opened it.
	// In game there is no main page: Back (labelled Resume) closes the menu.
	void leaveSettings(const char* why) {
		if (inGame_) {
			closeWhy_ = why;
			return;
		}
		onAction("back");
		ctx_->Update();
		if (Rml::Element* opener = doc_->GetElementById("open-settings")) {
			opener->Focus(true);
		}
	}

	// B on the settings page; B on the confirm page cancels it.
	void goBack() {
		if (confirmShown()) {
			closeConfirm();
			return;
		}
		if (!settingsShown()) {
			return;
		}
		leaveSettings("b");
	}

	bool confirmShown() {
		Rml::Element* page = doc_->GetElementById("page-confirm");
		return page != nullptr && page->IsVisible();
	}

	// Replaces the open page with the confirm page. The action runs only on OK; Cancel and B return to the page it came from.
	void askConfirm(const std::string& action, const char* message, const char* yes) {
		confirmAction_ = action;
		confirmReturnSettings_ = settingsShown();
		setText("confirm-text", message);
		setText("confirm-yes", yes);
		setDisplay("page-main", false);
		setDisplay("page-settings", false);
		setDisplay("page-confirm", true);
		ctx_->Update();
		if (Rml::Element* cancel = doc_->GetElementById("confirm-cancel")) {
			focusTarget(cancel); // Cancel has the default focus
		}
	}

	// Closes the confirm page; focus goes back to the button that opened it.
	void closeConfirm() {
		confirmAction_.clear();
		setDisplay("page-confirm", false);
		showPage(confirmReturnSettings_);
		ctx_->Update();
		if (Rml::Element* opener = doc_->GetElementById(confirmReturnSettings_ ? "reset-keys" : "quit")) {
			focusTarget(opener);
		}
	}

	void confirmYes() {
		if (confirmAction_ == "quit") {
			finished_ = true;
			return;
		}
		const std::string action = confirmAction_;
		closeConfirm();
		if (action == "reset-keys") {
			settings().keys = sunshine::defaultSettings().keys;
			capture_.clear();
			changed();
		}
	}

public:
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
			std::printf("[%s] pad %s: unknown token\n", inGame_ ? "menu" : "launcher", token.c_str());
			return;
		}
		std::printf("[%s] pad %s page=%s focus=%s\n", inGame_ ? "menu" : "launcher", token.c_str(),
		            settingsShown() ? "settings" : "main", focusId().c_str());
		std::fflush(stdout); // dev harness: SUNSHINE_DEV_MAX_SECONDS ends the process with TerminateProcess, which drops buffered stdout
	}

private:
	void setText(const char* id, const std::string& text) {
		if (Rml::Element* el = doc_->GetElementById(id)) {
			el->SetInnerRML(text);
		}
	}

	// Pages and panes fade in when shown: removing "hidden" starts the opacity transition (theme.rcss). Hiding is
	// instant. Pad targets follow display (IsVisible), not opacity, so a fading page takes input at once.
	void setDisplay(const std::string& id, bool shown) {
		if (Rml::Element* el = doc_->GetElementById(id)) {
			el->SetProperty("display", shown ? "block" : "none");
			el->SetClass("hidden", !shown);
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

	// Cheat rows are built once: rebuilding would drop the focused button. Ids are cheat-<index>.
	void buildCheatList() {
		std::string rows;
		for (int i = 0; i < cheatCount(); ++i) {
			rows += "<div class=\"row\"><button id=\"cheat-" + std::to_string(i) + "\"></button></div>";
		}
		if (cheatCount() == 0) {
			rows = "<p class=\"hint\">No cheats found: assets/cheats.json is missing or invalid.</p>";
		}
		setText("cheats-list", rows);
	}

	void refreshCheats() {
		for (int i = 0; i < cheatCount(); ++i) {
			CheatInfo info{};
			if (cheatAt(i, info)) {
				const std::string label = escapeRml(info.name) + ": " + (cheatEnabled(info.id) ? "On" : "Off") +
				                          (info.oneShot ? " (runs once when turned on)" : "");
				setText(("cheat-" + std::to_string(i)).c_str(), label);
			}
		}
	}

	// Mod rows are built once, like the cheat rows. Ids are mod-<index>; mods_ holds them in load order.
	void buildModList(const std::string& exeDir) {
		mods_ = scanModManifests(exeDir);
		mods::sortLoadOrder(mods_);
		std::string rows;
		for (size_t i = 0; i < mods_.size(); ++i) {
			const mods::Manifest& m = mods_[i];
			std::string meta = "API " + std::to_string(m.api.major) + "." + std::to_string(m.api.minor);
			if (!m.author.empty()) meta += " | by " + escapeRml(m.author);
			if (!m.description.empty()) meta += " | " + escapeRml(m.description);
			rows += "<div class=\"row\"><button id=\"mod-" + std::to_string(i) + "\"></button><p class=\"hint\">" + meta + "</p></div>";
		}
		if (mods_.empty()) {
			rows = "<p class=\"hint\">No mods found: each mod is a folder in mods/ with a mod.json.</p>";
		}
		setText("mods-list", rows);
	}

	void refreshMods() {
		const auto& switches = settings().mods;
		for (size_t i = 0; i < mods_.size(); ++i) {
			const auto it = switches.find(mods_[i].id);
			const bool on = it == switches.end() || it->second; // absent = on
			const std::string label = escapeRml(mods_[i].name) + " " + escapeRml(mods_[i].version) + ": " + (on ? "On" : "Off") +
			                          (inGame_ ? " (next start)" : ""); // mods load once, when the game starts
			setText(("mod-" + std::to_string(i)).c_str(), label);
		}
	}

	void persist() {
		if (sunshine::saveSettings(settings(), file_)) {
			setText("settings-status", "Saved to " + escapeRml(file_.string()));
		} else {
			setText("settings-status", "Could not write " + escapeRml(file_.string()));
		}
	}

	// Opens the folder that holds settings.ini. SUNSHINE_DEV_NO_SHELL logs the path instead (tests run without a shell).
	void openDataFolder() {
		const std::filesystem::path dir = file_.parent_path();
		if (std::getenv("SUNSHINE_DEV_NO_SHELL") != nullptr) {
			std::printf("[launcher] open data folder: %s\n", dir.string().c_str());
			return;
		}
		if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
			setText("settings-status", "Could not open " + escapeRml(dir.string()));
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
		setText("widescreen", std::string("Widescreen: ") + (s.widescreen ? "On" : "Off") + " (" + s.aspect + "), applies at next start");
		setText("aspect", "Aspect ratio: " + s.aspect + " (applies at next start)");
		setText("remember-window-size", std::string("Remember window size: ") + (s.rememberWindowSize ? "On" : "Off") +
		                                   (s.rememberWindowSize ? "" : " (opens at " + std::to_string(def.windowWidth) + " x " + std::to_string(def.windowHeight) + ")"));
		setText("fsr1", std::string("FSR 1 upscale: ") + (s.fsr1 ? "On" : "Off"));
		setText("volume", "Master volume: " + std::to_string(s.masterVolumePct) + "%");
		setText("output-device", "Output device: " + outputDeviceName(s.outputDevice));
		setText("stick-deadzone", "Stick dead zone: " + std::to_string(s.stickDeadZonePct) + "%");
		setText("substick-deadzone", "C-stick dead zone: " + std::to_string(s.substickDeadZonePct) + "%");
		setText("glyph-style", std::string("Button glyphs: ") + kGlyphNames[static_cast<int>(s.glyphStyle)]);
		setText("ui-scale", std::string("UI scale: ") + (s.uiScalePct == 0 ? "Window scale" : std::to_string(s.uiScalePct) + "%"));
		setText("skip-launcher", std::string("Skip launcher: ") +
		                             (s.skipLauncher ? "On (starts the last disc; hold Shift while starting to show this page)" : "Off"));
		for (const char* name : kKeyNames) {
			const std::string id = std::string("bind-") + name;
			const std::string label = capture_ == name ? "press a key..." : keyName(keyOf(s, name));
			setText(id.c_str(), std::string(name) + ": " + label);
		}
		refreshCheats();
		refreshMods();
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
			askConfirm(id, "Quit Super Mario Sunshine?", "Quit");
			return;
		}
		if (id == "confirm-yes") {
			confirmYes();
			return;
		}
		if (id == "confirm-cancel") {
			closeConfirm();
			return;
		}
		if (id == "open-settings") {
			showPage(true);
			return;
		}
		if (id == "back") {
			if (inGame_) {
				closeWhy_ = "back"; // mouse click on Resume
				return;
			}
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

		if (id.rfind("cheat-", 0) == 0) {
			CheatInfo info{};
			if (cheatAt(std::atoi(id.c_str() + 6), info)) {
				const bool on = !cheatEnabled(info.id);
				cheatSet(info.id, on);
				if (!info.oneShot) s.cheats[info.id] = on; // one-shot cheats are actions: not saved
				changed();
			}
			return;
		}

		if (id.rfind("mod-", 0) == 0) {
			const auto i = static_cast<size_t>(std::atoi(id.c_str() + 4));
			if (i < mods_.size()) {
				const std::string mid = mods_[i].id;
				const auto it = s.mods.find(mid);
				s.mods[mid] = it != s.mods.end() && !it->second; // absent = on, so the first press turns it off
				changed();
			}
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
		} else if (id == "output-device") {
			const std::vector<av::AudioDevice> devices = av::AudioOutputDevices();
			size_t at = 0;
			for (size_t i = 0; i < devices.size(); ++i)
				if (devices[i].id == s.outputDevice)
					at = i;
			s.outputDevice = devices[(at + 1) % devices.size()].id;
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
		} else if (id == "open-data") {
			openDataFolder();
			return;
		} else if (id == "ui-scale") {
			s.uiScalePct = nextPreset(kUiScalePresets, s.uiScalePct);
		} else if (id == "skip-launcher") {
			s.skipLauncher = !s.skipLauncher;
		} else if (id == "reset-keys") {
			askConfirm(id, "Restore the default key bindings?", "Restore");
			return;
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
	std::string confirmAction_;          // action waiting for OK on the confirm page
	bool confirmReturnSettings_ = false; // the page the confirm page replaced
	static constexpr int kPadStepFrames = 30; // frames between synthetic gamepad tokens (dev hook)
	bool stickHeld_[4] = {};                  // left stick: up, down, left, right; one step per crossing of the neutral band
	std::vector<std::string> padScript_;      // SUNSHINE_DEV_PAD_EVENTS tokens
	bool logTransitions_ = false;             // dev: print "transition end <id>" (open())
	size_t padNext_ = 0;
	int padWait_ = 0;
	bool finished_ = false;
	bool started_ = false;
	bool inGame_ = false; // in-game menu: settings page only; Start/B/Escape/F1/Resume set closeWhy_
	std::string closeWhy_;
	std::vector<mods::Manifest> mods_; // Mods pane rows in load order; button ids are mod-<index>
};

aurora::input::EventResult onInputThunk(const aurora::input::InputEvent& event, void* userdata) {
	return static_cast<Launcher*>(userdata)->onInput(event);
}

// In-game menu (UI-4). The input layer is permanent (menuInstall); the document exists only while the menu is open.
// The layer callback only records requests: menuPoll opens and closes the document outside the router.
std::filesystem::path gMenuFile;
std::optional<Launcher> gMenu;
aurora::input::LayerId gMenuLayer = aurora::input::kInvalidLayerId;
const char* gMenuRequest = nullptr; // "chord" or "f1": open at the next menuPoll
bool gMenuRHeld = false;            // R trigger axis >= 0.5, or the native button mapped to PAD R is down
int gMenuFrames = 0;                // frames presented while open
u32 gMenuLogicAtOpen = 0;
std::deque<std::string> gMenuDev; // menuDevQueue tokens
int gMenuDevWait = 0;
constexpr int kMenuDevStepFrames = 30;
SDL_Joystick* gDevVpad = nullptr; // SUNSHINE_DEV_VPAD: SDL virtual gamepad (devVpadAttach), moved by the vstick token

// Open or opening: every source is blocked for the layers below, so the game's pad layer drops held input.
bool menuCaptures(const aurora::input::InputSource& /*source*/, void* /*userdata*/) {
	return gMenu.has_value() || gMenuRequest != nullptr;
}

aurora::input::PointerMode menuPointer(void* /*userdata*/) {
	return gMenu ? aurora::input::PointerMode::Visible : aurora::input::PointerMode::None;
}

// Pads with a digital R report it as a button; Aurora's mapping names the native button behind PAD R.
bool isMappedR(SDL_GamepadButton button) {
	u32 count = 0;
	const PADButtonMapping* map = PADGetButtonMappings(0, &count);
	for (u32 i = 0; map != nullptr && i < count; ++i) {
		if (map[i].padButton == PAD_TRIGGER_R && map[i].nativeButton == static_cast<u32>(button)) {
			return true;
		}
	}
	return false;
}

aurora::input::EventResult menuLayerEvent(const aurora::input::InputEvent& event, void* /*userdata*/) {
	using aurora::input::EventResult;
	using aurora::input::InputEvent;
	bool startPress = false;
	bool f1Press = false;
	if (const auto* axis = event.payload.get_if<InputEvent::AxisChanged>()) {
		if (axis->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
			gMenuRHeld = axis->value >= 0.5f;
		}
	} else if (const auto* button = event.payload.get_if<InputEvent::ButtonChanged>()) {
		if (isMappedR(button->button)) {
			gMenuRHeld = button->pressed;
		}
		startPress = button->pressed && button->button == SDL_GAMEPAD_BUTTON_START;
	} else if (const auto* key = event.payload.get_if<InputEvent::KeyChanged>()) {
		f1Press = key->pressed && !key->repeat && key->scancode == SDL_SCANCODE_F1;
	} else if (event.payload.is<InputEvent::Cancelled>() && event.source.kind == aurora::input::InputSource::Kind::Controller) {
		gMenuRHeld = false;
	}
	if (gMenu) {
		return gMenu->onInput(event);
	}
	if (gMenuRequest != nullptr) {
		return EventResult::Consume; // opens at the next menuPoll
	}
	const bool chord = startPress && gMenuRHeld;
	if (!chord && !f1Press) {
		return EventResult::Pass;
	}
	if (!sunshineCanReplay()) {
		std::printf("[menu] ignored (not in gameplay)\n");
		std::fflush(stdout);
		return EventResult::Pass;
	}
	gMenuRequest = chord ? "chord" : "f1";
	return EventResult::Consume; // the router does not pass the chord's Start on to aurora.pad (game pause: TMarDirector::updateGameMode)
}

void pushKey(SDL_Scancode scancode, bool pressed) {
	SDL_Event ev{};
	ev.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	ev.key.timestamp = SDL_GetTicksNS();
	ev.key.windowID = SDL_GetWindowID(aurora_get_window());
	ev.key.scancode = scancode;
	ev.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, true);
	ev.key.down = pressed;
	SDL_PushEvent(&ev);
}

// One dev token (menuDevQueue). chord goes into the menu layer's callback; keys go through SDL and the real router.
void runMenuToken(const std::string& token) {
	using aurora::input::InputEvent;
	if (token == "chord") {
		InputEvent event{};
		event.source.kind = aurora::input::InputSource::Kind::Controller;
		event.source.origin = aurora::input::InputSource::Origin::Synthetic;
		event.payload = InputEvent::AxisChanged{ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1.f };
		menuLayerEvent(event, nullptr);
		event.payload = InputEvent::ButtonChanged{ SDL_GAMEPAD_BUTTON_START, true };
		const bool consumed = menuLayerEvent(event, nullptr) == aurora::input::EventResult::Consume;
		event.payload = InputEvent::ButtonChanged{ SDL_GAMEPAD_BUTTON_START, false };
		menuLayerEvent(event, nullptr);
		event.payload = InputEvent::AxisChanged{ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.f };
		menuLayerEvent(event, nullptr);
		std::printf("[menu] dev chord: start %s\n", consumed ? "consumed" : "passed");
	} else if (token == "f1") {
		pushKey(SDL_SCANCODE_F1, true);
		pushKey(SDL_SCANCODE_F1, false);
		std::printf("[menu] dev f1 pushed\n");
	} else if (token.rfind("key:", 0) == 0) { // key:<SDL scancode name>:down|up
		const size_t colon = token.rfind(':');
		const std::string name = colon > 4 ? token.substr(4, colon - 4) : "";
		const std::string edge = token.substr(colon + 1);
		const SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
		if (scancode == SDL_SCANCODE_UNKNOWN || (edge != "down" && edge != "up")) {
			std::printf("[menu] dev %s: unknown key token\n", token.c_str());
		} else {
			pushKey(scancode, edge == "down");
			std::printf("[menu] dev %s pushed\n", token.c_str());
		}
	} else if (token.rfind("vstick:", 0) == 0) { // vstick:<x>:<y>, -127..127, GameCube orientation (y > 0 = up)
		int x = 0;
		int y = 0;
		const PADDeadZones* zones = PADGetDeadZones(0);
		if (gDevVpad == nullptr || std::sscanf(token.c_str() + 7, "%d:%d", &x, &y) != 2) {
			std::printf("[menu] dev %s: no SUNSHINE_DEV_VPAD gamepad or bad token\n", token.c_str());
		} else {
			x = std::clamp(x, -127, 127);
			y = std::clamp(y, -127, 127);
			// SDL's left Y axis grows downwards; Aurora's default mapping turns SDL -Y into PAD up.
			SDL_SetJoystickVirtualAxis(gDevVpad, SDL_GAMEPAD_AXIS_LEFTX, static_cast<Sint16>(x * 32767 / 127));
			SDL_SetJoystickVirtualAxis(gDevVpad, SDL_GAMEPAD_AXIS_LEFTY, static_cast<Sint16>(-y * 32767 / 127));
			std::printf("[menu] dev %s set (port0 pad=%d use=%d stickDeadZone=%d substickDeadZone=%d)\n", token.c_str(),
			            zones != nullptr ? 1 : 0, zones != nullptr && zones->useDeadzones ? 1 : 0,
			            zones != nullptr ? zones->stickDeadZone : -1, zones != nullptr ? zones->substickDeadZone : -1);
		}
	} else if (gMenu) {
		gMenu->runPadToken(token);
	} else {
		std::printf("[menu] dev %s: menu closed, pad token dropped\n", token.c_str());
	}
	std::fflush(stdout);
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
	// [launcher] skip_launcher: start the last disc like --dvd does, without a frame. Shift held at start, or a disc that
	// does not open, shows the launcher instead (its open() validates last_image again and shows the error).
	const Settings& s = settings();
	if (s.skipLauncher && !s.lastImagePath.empty()) {
		std::string error;
		if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
			std::printf("[launcher] skip: Shift held, showing the launcher\n");
		} else if (openDisc(s.lastImagePath, error)) {
			std::printf("[launcher] skip: starting %s\n", s.lastImagePath.c_str());
			std::fflush(stdout);
			return true;
		} else {
			std::printf("[launcher] skip: %s Showing the launcher.\n", error.c_str());
		}
		std::fflush(stdout);
	}
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

void applyPadDeadZones() { applyDeadZones(settings()); }

void controllerAdded(std::uint32_t instance) {
	// Port 0 -> controller index -> SDL gamepad -> joystick instance (public PAD API; Aurora keys its controllers by that
	// instance, and AURORA_CONTROLLER_ADDED carries it). A pad on another port leaves port 0's zones alone.
	const auto index = PADGetIndexForPort(0);
	SDL_Gamepad* pad = index >= 0 ? PADGetSDLGamepadForIndex(static_cast<u32>(index)) : nullptr;
	if (pad != nullptr && SDL_GetGamepadID(pad) == instance) {
		applyPadDeadZones();
	}
}

void applyRuntimeSettings() {
	const Settings& s = settings();
	applyPadBindings(s);
	applyDeadZones(s);
	applyUiScale(s);
	applyCheats(s);
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
	av::AudioSetVolumePct(s.masterVolumePct);
	av::AudioSetDevice(s.outputDevice);
	// Window calls only when the values differ from the last applied ones: every settings change (a cheat toggled in the
	// in-game menu too) runs this, and VISetWindowSize un-maximizes (SDL_RestoreWindow) and resizes the window.
	static std::optional<bool> appliedFullscreen;
	static std::optional<std::pair<uint32_t, uint32_t>> appliedSize;
	if (appliedFullscreen != s.fullscreen) {
		appliedFullscreen = s.fullscreen;
		VISetWindowFullscreen(s.fullscreen);
		std::printf("[launcher] window: fullscreen=%d\n", s.fullscreen ? 1 : 0);
	}
	// remember_window_size=0: the window always opens at the default size; the saved size is kept for the next time it is on.
	const sunshine::Settings def = sunshine::defaultSettings();
	const auto winW = static_cast<uint32_t>(s.rememberWindowSize ? s.windowWidth : def.windowWidth);
	const auto winH = static_cast<uint32_t>(s.rememberWindowSize ? s.windowHeight : def.windowHeight);
	const std::pair<uint32_t, uint32_t> size{ widescreenWindowWidth(winW, winH), winH };
	if (appliedSize != size) {
		appliedSize = size;
		VISetWindowSize(size.first, size.second);
		std::printf("[launcher] window: size %ux%u\n", size.first, size.second);
	}
	std::fflush(stdout);
	// SUNSHINE_RENDER_SCALE (render/render_scale.cpp) overrides the saved scale; an empty value counts as unset there and here.
	const char* envScale = std::getenv("SUNSHINE_RENDER_SCALE");
	if (envScale == nullptr || *envScale == '\0') {
		VISetFrameBufferScale(static_cast<float>(s.resolutionScalePct) / 100.f);
	}
	sunshineApplyUpscaler(s.fsr1); // render/render_scale.cpp: FSR 1 or bilinear, unless SUNSHINE_UPSCALER is set
}

void menuInstall(const std::filesystem::path& settingsFile) {
	gMenuFile = settingsFile;
	if (gMenuLayer != aurora::input::kInvalidLayerId) {
		return;
	}
	aurora::input::LayerDescriptor desc{};
	desc.label = "sunshine.ingame-menu";
	desc.priority = aurora::input::kRmlUiLayerPriority; // above aurora.pad (kGameLayerPriority)
	desc.onEvent = &menuLayerEvent;
	desc.capturesSource = &menuCaptures;
	desc.pointerMode = &menuPointer;
	gMenuLayer = aurora::input::register_layer(desc);
}

void menuPoll() {
	if (!gMenuDev.empty() && ++gMenuDevWait >= kMenuDevStepFrames) {
		gMenuDevWait = 0;
		const std::string token = gMenuDev.front();
		gMenuDev.pop_front();
		runMenuToken(token);
	}
	if (gMenu) {
		++gMenuFrames;
		const std::string why = gMenu->closeRequest();
		if (!why.empty()) {
			const std::string focus = gMenu->focusId();
			gMenu->close();
			gMenu.reset();
			aurora::input::reconcile(); // the game's pad layer regains its sources; keys still held stay hidden from it
			std::printf("[menu] close (%s) frames=%d logic=%lu->%lu focus=%s\n", why.c_str(), gMenuFrames,
			            gMenuLogicAtOpen, sunshineLogicTickCount(), focus.c_str());
			std::fflush(stdout);
		}
		return;
	}
	if (gMenuRequest == nullptr) {
		return;
	}
	const char* why = gMenuRequest;
	gMenuRequest = nullptr;
	if (!sunshineCanReplay()) { // gameplay ended between the request and this poll: drop it, give the sources back
		aurora::input::reconcile();
		std::printf("[menu] ignored (not in gameplay)\n");
		std::fflush(stdout);
		return;
	}
	gMenu.emplace(gMenuFile, true);
	if (!gMenu->open()) {
		gMenu->close();
		gMenu.reset();
		aurora::input::reconcile();
		std::fprintf(stderr, "[menu] could not load assets/launcher\n");
		return;
	}
	for (u32 port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
		PADControlMotor(port, PAD_MOTOR_STOP); // a rumble running when the game froze would never be stopped by it
	}
	gMenuFrames = 0;
	gMenuLogicAtOpen = sunshineLogicTickCount();
	aurora::input::reconcile(); // capture: the game's pad layer gets Cancelled and drops held buttons
	std::printf("[menu] open (%s) logic=%lu focus=%s\n", why, gMenuLogicAtOpen, gMenu->focusId().c_str());
	std::fflush(stdout);
}

bool menuOpen() { return gMenu.has_value(); }

void devVpadAttach() {
	const char* env = std::getenv("SUNSHINE_DEV_VPAD");
	const bool late = env != nullptr && std::strcmp(env, "late") == 0;
	if (env == nullptr || (std::strcmp(env, "1") != 0 && !late)) {
		return; // only 1 or late: any other value (0, false, no, ...) leaves the hook off
	}
	// Route windows have no focus, so SDL drops joystick input. Aurora's setter sets the same SDL hint and keeps its
	// g_config.allowJoystickBackgroundEvents in sync.
	aurora_set_background_input(true);
	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
	desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
	desc.name = "Sunshine dev vpad";
	const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
	gDevVpad = id != 0 ? SDL_OpenJoystick(id) : nullptr;
	if (gDevVpad != nullptr) { // triggers at rest
		SDL_SetJoystickVirtualAxis(gDevVpad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
		SDL_SetJoystickVirtualAxis(gDevVpad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
	}
	// Aurora adds a controller on SDL_EVENT_GAMEPAD_ADDED; pump until it is player 0, so applyDeadZones finds it.
	// SUNSHINE_DEV_VPAD=late: no pump; the game's first pump adds it, like a pad plugged in at a --dvd boot or later.
	int pumps = 0;
	while (!late && gDevVpad != nullptr && PADGetDeadZones(0) == nullptr && pumps < 60) {
		aurora_update();
		++pumps;
	}
	std::printf("[devvpad] id=%u open=%d port0=%d pumps=%d\n", static_cast<unsigned>(id), gDevVpad != nullptr ? 1 : 0,
	            PADGetDeadZones(0) != nullptr ? 1 : 0, pumps);
	std::fflush(stdout);
}

void menuDevQueue(const char* tokens) {
	if (gMenuDev.empty()) {
		gMenuDevWait = kMenuDevStepFrames - 1; // the first token runs at the next menuPoll
	}
	std::string list = tokens != nullptr ? tokens : "";
	size_t begin = 0;
	while (begin < list.size()) {
		size_t end = list.find(',', begin);
		if (end == std::string::npos) {
			end = list.size();
		}
		if (end > begin) {
			gMenuDev.push_back(list.substr(begin, end - begin));
		}
		begin = end + 1;
	}
}

} // namespace launcher
