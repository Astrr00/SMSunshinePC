#include "widescreen.h"

#include "../settings.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr uint32_t kGameWidth = 640; // GameCube 4:3 EFB and UI width
// 4:3 output shows the 448-line EFB over 480 display lines (Aurora stretches 448 -> 480), so one EFB pixel is
// 480/448 taller than wide. The wide EFB keeps that pixel shape: W = 480 * aspect, window = height * W / 480.
// With 448 here the 3D view and the HUD came out 480/448 = 1.071x wider than at 4:3 (WideCheck, platform-notes.md).
constexpr uint32_t kDisplayHeight = 480;
uint32_t g_renderWidth = kGameWidth;
} // namespace

uint32_t widescreenRenderWidthForAspect(double aspect)
{
	const double wide = kDisplayHeight * aspect;
	const uint32_t w = wide > kGameWidth ? static_cast<uint32_t>(wide + 0.5) : kGameWidth;
	return (w + 1) & ~1u; // even: TEfbCtrl aligns copy rects to 2 px
}

uint32_t widescreenInit(const sunshine::Settings& s, uint32_t windowWidth, uint32_t windowHeight)
{
	bool on = s.widescreen;
	std::string aspect = s.aspect;
	if (const char* env = std::getenv("SUNSHINE_ASPECT"); env && *env) {
		on = true;
		aspect = env;
	}

	g_renderWidth = kGameWidth;
	if (on) {
		double ratio = 4.0 / 3.0;
		unsigned aw = 0, ah = 0;
		if (aspect == "auto") {
			if (windowHeight)
				ratio = static_cast<double>(windowWidth) / windowHeight;
		} else if (std::sscanf(aspect.c_str(), "%u:%u", &aw, &ah) == 2 && aw && ah) {
			ratio = static_cast<double>(aw) / ah;
		}
		g_renderWidth = widescreenRenderWidthForAspect(ratio);
	}
	std::printf("widescreen: %s aspect=%s render width %u\n", on ? "on" : "off", aspect.c_str(), g_renderWidth);
	// The logical window keeps the display aspect W/480 (EFB W x 448 stretched to 480 lines, as at 4:3).
	return widescreenWindowWidth(windowWidth, windowHeight);
}

uint32_t sunshineRenderWidth() { return g_renderWidth; }

uint32_t sunshineUIOffsetX() { return ((g_renderWidth - kGameWidth) / 2) & ~1u; }

uint32_t widescreenWindowWidth(uint32_t windowWidth, uint32_t windowHeight)
{
	if (g_renderWidth == kGameWidth)
		return windowWidth;
	return ((windowHeight * g_renderWidth) / kDisplayHeight + 1) & ~1u;
}
