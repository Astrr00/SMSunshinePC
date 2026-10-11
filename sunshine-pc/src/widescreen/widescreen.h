// Widescreen render width for the PC port (Port.md section 5). The GameCube EFB is 640x448 (4:3). Widescreen widens
// the EFB (3D view, EFB copies, and the window, since Aurora's FIT policy follows the render size). 2D UI stays 640
// units wide and is centred in it. Vertical size and the 4:3 output are unchanged.
#pragma once

#include <cstdint>

namespace sunshine {
struct Settings;
}

// Reads settings.widescreen and settings.aspect (or SUNSHINE_ASPECT=16:9, a test override that also enables widescreen).
// windowWidth/windowHeight feed "auto". Call once at startup, before the first render mode is configured.
// Returns the logical window width to create: unchanged at 4:3, otherwise windowHeight * EFB width / 448.
uint32_t widescreenInit(const sunshine::Settings& s, uint32_t windowWidth, uint32_t windowHeight);

// EFB width in pixels (640 at 4:3, otherwise 448 * aspect rounded to even). Used by sms/src/System/Resolution.cpp.
uint32_t sunshineRenderWidth();
// Left edge of the centred 640-wide 4:3 UI column inside sunshineRenderWidth() (even; 0 at 4:3).
uint32_t sunshineUIOffsetX();

// Logical window width for windowHeight at the active EFB aspect; windowWidth unchanged at 4:3. Used by widescreenInit and
// by launcher::applyRuntimeSettings, which would otherwise reset the window to the saved 4:3 size.
uint32_t widescreenWindowWidth(uint32_t windowWidth, uint32_t windowHeight);

// Pure policy, exposed for tests: EFB width for a target aspect ratio (width / height).
uint32_t widescreenRenderWidthForAspect(double aspect);
