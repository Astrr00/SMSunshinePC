#include "render_scale.h"

#include <aurora/aurora.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Implemented in extern/aurora/lib/dolphin/vi/vi.cpp. Declared here because the decomp's sms/include/dolphin/vi.h
// does not expose it and Aurora's header would clash with that include path.
extern "C" void VISetFrameBufferScale(float scale);

static bool readFloatEnv(const char* name, float& out)
{
	const char* text = std::getenv(name);
	if (text == nullptr || *text == '\0')
		return false;
	char* end = nullptr;
	const float value = std::strtof(text, &end);
	if (end == text) {
		std::fprintf(stderr, "[render] ignoring %s=%s (not a number)\n", name, text);
		return false;
	}
	out = value;
	return true;
}

void sunshineApplyRenderSettings()
{
	if (const char* upscaler = std::getenv("SUNSHINE_UPSCALER")) {
		if (std::strcmp(upscaler, "fsr1") == 0)
			aurora_set_resampler(SAMPLER_FSR1);
		else if (std::strcmp(upscaler, "area") == 0)
			aurora_set_resampler(SAMPLER_AREA);
		else if (std::strcmp(upscaler, "bilinear") == 0)
			aurora_set_resampler(SAMPLER_BILINEAR);
		else
			std::fprintf(stderr, "[render] unknown SUNSHINE_UPSCALER=%s (bilinear|area|fsr1)\n", upscaler);
	}

	float sharpness = 0.f;
	if (readFloatEnv("SUNSHINE_UPSCALE_SHARPNESS", sharpness))
		aurora_set_upscale_sharpness(sharpness);

	// Internal framebuffer size as a fraction of the 640x480 EFB frame, not of the window: Aurora's window.cpp
	// (get_window_size) scales vi::configured_fb_size() and widens it to the window aspect. Unset or empty: the INI
	// resolution_scale decides (launcher.cpp applyRuntimeSettings); scale 0 (Auto) renders at the window's pixel size.
	float scale = 0.f;
	if (readFloatEnv("SUNSHINE_RENDER_SCALE", scale)) {
		if (scale < 0.25f || scale > 1.f)
			std::fprintf(stderr, "[render] ignoring SUNSHINE_RENDER_SCALE=%g (range 0.25-1.0)\n", scale);
		else
			VISetFrameBufferScale(scale);
	}
}

void sunshineApplyUpscaler(bool fsr1)
{
	if (std::getenv("SUNSHINE_UPSCALER") != nullptr)
		return;
	aurora_set_resampler(fsr1 ? SAMPLER_FSR1 : SAMPLER_BILINEAR);
	std::printf("[render] upscaler=%s\n", fsr1 ? "fsr1" : "bilinear");
}
