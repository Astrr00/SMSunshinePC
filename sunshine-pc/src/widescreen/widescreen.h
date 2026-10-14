// Widescreen render width for the PC port (Port.md section 5). The GameCube EFB is 640x448 (4:3). Widescreen widens
// the EFB (3D view, EFB copies) to W = 480 * aspect and the window to the same display aspect W/480: Aurora scales the
// EFB to the window in X and Y separately (window FIT is never enabled, aurora lib/window.cpp), so 448 lines fill 480
// as at 4:3. 2D UI stays 640 units wide and is centred in it. Vertical size and the 4:3 output are unchanged.
#pragma once

#include <cstdint>

namespace sunshine {
struct Settings;
}

// Reads settings.widescreen and settings.aspect (or SUNSHINE_ASPECT=16:9, a test override that also enables widescreen).
// windowWidth/windowHeight feed "auto". Call once at startup, before the first render mode is configured.
// Returns the logical window width to create: unchanged at 4:3, otherwise windowHeight * EFB width / 480.
uint32_t widescreenInit(const sunshine::Settings& s, uint32_t windowWidth, uint32_t windowHeight);

// EFB width in pixels (640 while 480 * aspect < 648, otherwise 480 * aspect rounded to a multiple of 4, at most 3752; the EFB stays 448 high).
// Used by sms/src/System/Resolution.cpp.
uint32_t sunshineRenderWidth();
// Left edge of the centred 640-wide 4:3 UI column inside sunshineRenderWidth() (even; 0 at 4:3).
uint32_t sunshineUIOffsetX();

// Logical window width for windowHeight at the active EFB aspect; windowWidth unchanged at 4:3. Used by widescreenInit and
// by launcher::applyRuntimeSettings, which would otherwise reset the window to the saved 4:3 size.
uint32_t widescreenWindowWidth(uint32_t windowWidth, uint32_t windowHeight);

// Pure policy, exposed for tests: EFB width for a target aspect ratio (width / height).
uint32_t widescreenRenderWidthForAspect(double aspect);
