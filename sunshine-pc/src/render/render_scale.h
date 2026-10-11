// Render scale and upscaler selection for the PC port, configured from the environment at startup.
#pragma once

// Applies SUNSHINE_UPSCALER, SUNSHINE_UPSCALE_SHARPNESS and SUNSHINE_RENDER_SCALE. Call after aurora_initialize().
void sunshineApplyRenderSettings();
// Sets the present resampler from the launcher's FSR 1 setting (bilinear when off). SUNSHINE_UPSCALER overrides it.
void sunshineApplyUpscaler(bool fsr1);
