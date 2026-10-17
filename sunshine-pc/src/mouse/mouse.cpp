// Mouse play for controller port 0. The keyboard reaches the game through Aurora's PAD layer; the mouse is folded into the
// same status here, once per logic pad read (JUTGamePad::read, before PADClamp):
//   mouse motion   while L is held (crouch and aim): the nozzle pitch (main stick Y, up = aim up); no camera look
//   left button    R, fully pressed (spray)
//   right button   L, fully pressed (crouch and aim: Mario stands still, the stick pitches the nozzle)
//   middle button  Z (change nozzle)
// The cursor is locked to the window while a stage is running and no menu is open. [controls] mouse=0 turns all of this off.
#include <dolphin/pad.h>

#include <SDL3/SDL.h>
#include <aurora/input.hpp>

#include <algorithm>

#include "Player/Mario.hpp"
#include "settings.h"

extern "C" int PADIsInputCaptured(u32 port); // Aurora; the decomp copy of <dolphin/pad.h> lacks it

namespace {

constexpr float kAimGain = 6.f; // main-stick units per mouse pixel per logic tick (30 per second)

bool wanted() { return sunshine::settings().mouseControls && gpMarioOriginal != nullptr; }

aurora::input::PointerMode pointerMode(void* /*userdata*/)
{
	return wanted() ? aurora::input::PointerMode::Relative : aurora::input::PointerMode::None;
}

s8 clampStick(float v) { return static_cast<s8>(std::clamp(v, -127.f, 127.f)); }

} // namespace

void sunshineMouseInstall()
{
	static aurora::input::LayerId layer = aurora::input::kInvalidLayerId;
	if (layer != aurora::input::kInvalidLayerId) return;
	aurora::input::LayerDescriptor desc{};
	desc.label = "sunshine.mouse";
	desc.priority = aurora::input::kGameLayerPriority; // below the launcher and the in-game menu, which show the cursor
	desc.onEvent = [](const aurora::input::InputEvent&, void*) { return aurora::input::EventResult::Pass; }; // register_layer needs one
	desc.pointerMode = &pointerMode;
	layer = aurora::input::register_layer(desc);
}

void sunshineMouseTick(PADStatus* status)
{
	float dx = 0.f, dy = 0.f;
	SDL_GetRelativeMouseState(&dx, &dy); // always drained, so the first active tick does not jump
	SDL_Window* window = SDL_GetKeyboardFocus();
	if (!wanted() || window == nullptr || !SDL_GetWindowRelativeMouseMode(window) || PADIsInputCaptured(0) || status[0].err != 0) return;

	const SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
	if (buttons & SDL_BUTTON_LMASK) {
		status[0].triggerRight = 255;
		status[0].button |= PAD_TRIGGER_R;
	}
	if (buttons & SDL_BUTTON_RMASK) {
		status[0].triggerLeft = 255;
		status[0].button |= PAD_TRIGGER_L;
	}
	if (buttons & SDL_BUTTON_MMASK) status[0].button |= PAD_TRIGGER_Z;

	const float gain = sunshine::settings().mouseSensitivityPct / 100.f;
	if (status[0].triggerLeft > 0 || (status[0].button & PAD_TRIGGER_L)) // aiming: mouse up raises the nozzle
		status[0].stickY = clampStick(status[0].stickY - dy * kAimGain * gain);
}
