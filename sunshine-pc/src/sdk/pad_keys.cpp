// Default keyboard bindings for controller port 0 (Aurora's PAD layer has none). Kept in its own translation unit because
// the game's include path shadows <dolphin/pad.h> with the console SDK header; the Aurora header is included by path.
#include "../../extern/aurora/include/dolphin/pad.h"

// SDL scancode values (SDL3 SDL_scancode.h; stable USB HID usage ids). Not included: this library has no SDL include path.
enum : int {
	SDL_SCANCODE_A = 4, SDL_SCANCODE_D = 7, SDL_SCANCODE_E = 8, SDL_SCANCODE_F = 9, SDL_SCANCODE_I = 12,
	SDL_SCANCODE_J = 13, SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15, SDL_SCANCODE_Q = 20, SDL_SCANCODE_S = 22,
	SDL_SCANCODE_W = 26, SDL_SCANCODE_Z = 29, SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_SPACE = 44,
	SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
	SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225,
};

#include <cstring>

// WASD = left stick, IJKL = C-stick, arrows = D-pad, Space/E/Q/F = A/B/X/Y, Enter = Start, Z = Z,
// Left Shift = L, Left Ctrl = R. The launcher's key-binding page edits these later (PADSetKeyButtonBinding).
void sunshineInstallKeyboardDefaults()
{
	PADDefaultKeyBindings keys = {};
	const PADKeyButtonBinding buttons[PAD_BUTTON_COUNT] = {
		{ SDL_SCANCODE_LEFT, PAD_BUTTON_LEFT }, { SDL_SCANCODE_RIGHT, PAD_BUTTON_RIGHT },
		{ SDL_SCANCODE_DOWN, PAD_BUTTON_DOWN }, { SDL_SCANCODE_UP, PAD_BUTTON_UP },
		{ SDL_SCANCODE_Z, PAD_TRIGGER_Z }, { SDL_SCANCODE_LCTRL, PAD_TRIGGER_R }, { SDL_SCANCODE_LSHIFT, PAD_TRIGGER_L },
		{ SDL_SCANCODE_SPACE, PAD_BUTTON_A }, { SDL_SCANCODE_E, PAD_BUTTON_B }, { SDL_SCANCODE_Q, PAD_BUTTON_X },
		{ SDL_SCANCODE_F, PAD_BUTTON_Y }, { SDL_SCANCODE_RETURN, PAD_BUTTON_START },
	};
	const PADKeyAxisBinding axes[PAD_AXIS_COUNT] = {
		{ SDL_SCANCODE_D, PAD_AXIS_LEFT_X_POS, 0 }, { SDL_SCANCODE_A, PAD_AXIS_LEFT_X_NEG, 0 },
		{ SDL_SCANCODE_W, PAD_AXIS_LEFT_Y_POS, 0 }, { SDL_SCANCODE_S, PAD_AXIS_LEFT_Y_NEG, 0 },
		{ SDL_SCANCODE_L, PAD_AXIS_RIGHT_X_POS, 0 }, { SDL_SCANCODE_J, PAD_AXIS_RIGHT_X_NEG, 0 },
		{ SDL_SCANCODE_I, PAD_AXIS_RIGHT_Y_POS, 0 }, { SDL_SCANCODE_K, PAD_AXIS_RIGHT_Y_NEG, 0 },
		{ SDL_SCANCODE_LSHIFT, PAD_AXIS_TRIGGER_L, 0 }, { SDL_SCANCODE_LCTRL, PAD_AXIS_TRIGGER_R, 0 },
	};
	std::memcpy(keys.buttons, buttons, sizeof(buttons));
	std::memcpy(keys.axes, axes, sizeof(axes));
	PADSetDefaultKeyBindings(0, &keys);
	// PADInit now, before applyRuntimeSettings sets the INI [keys] table: Aurora's first PADInit resets the key table to
	// these defaults, so the game's own first call (JUTGamePad) dropped the INI keys (tools/routes/99_key_rebind.dev).
	// Later PADInit calls return at once.
	PADInit();
	// The INI [keys] (applyRuntimeSettings sets every slot) are the only source of key bindings: this drops the port-0
	// keys PADInit just read from Aurora's keyboard_bindings.dat. The port never writes it (no PADSerializeMappings).
	PADRestoreDefaultKeyBindings(0);
	PADSetKeyboardActive(0, TRUE);
}

