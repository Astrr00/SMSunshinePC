#include "widescreen.h"

#include "../settings.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr uint32_t kGameWidth = 640; // GameCube 4:3 EFB and UI width
// 4:3 output shows the 448-line EFB over 480 display lines (Aurora stretches 448 -> 480), so one EFB pixel is
// 480/448 taller than wide. The wide EFB keeps that pixel shape: W = 480 * aspect, window = height * W / 480.
// With 448 here the 3D view came out 480/448 = 1.071x wider than at 4:3 (WideCheck, platform-notes.md).
constexpr uint32_t kDisplayHeight = 480;
uint32_t g_renderWidth = kGameWidth;
} // namespace

uint32_t widescreenRenderWidthForAspect(double aspect)
{
	const double wide = kDisplayHeight * aspect;
	// Within 8 px of 4:3 (e.g. aspect=auto with a 1281-wide window) stay on the 640 path: no UI offset, unchanged window.
	if (wide < kGameWidth + 8)
		return kGameWidth;
	const uint32_t w = static_cast<uint32_t>(wide + 0.5);
	// Multiple of 4: TEfbCtrl aligns copy rects to 2 px, and the 640 UI column needs an even offset on both sides
	// ((W - 640) / 2 even), otherwise it sits 1 px off centre (854 gave 106 px left, 108 px right).
	return (w + 2) & ~3u;
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
