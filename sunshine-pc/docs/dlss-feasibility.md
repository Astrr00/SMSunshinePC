# DLSS feasibility

## Verdict

**Not implemented.** DLSS cannot be wired into the port today. Three independent blockers, any one of which is enough:

1. **No frame data reaches DLSS in the form it needs.** NGX and Streamline want a colour input, a depth buffer, motion vectors and jitter offsets. The GX-emulated renderer produces none of motion vectors or jitter (Port.md §5 requires per-draw previous matrices, which are not tracked yet).
2. **The GPU-side handoff is not available.** Aurora renders through Dawn (WebGPU). Dawn's D3D12 backend only *imports* external D3D12 resources (`build/aurorapixel/_deps/dawn_prebuilt-src/include/dawn/native/D3D12Backend.h`: `GetD3D12Device`, `GetD3D12CommandQueue`, shared-texture import descriptors). It does not export a Dawn-created colour target as an `ID3D12Resource`. Feeding DLSS would mean a re-architecture of the present path to caller-owned D3D12 textures with manual synchronisation.
3. **No SDK in the repo, and the licence is restrictive.** No DLSS SDK headers (`sl.h`, `nvsdk_ngx`) exist in the tree. Only NVIDIA runtime DLLs are installed on the dev machine (`nvngx_dlss.dll`, `nvngx_dlssd.dll`, `nvngx_dlisr.dll`), which are not an SDK. DLSS also runs only on NVIDIA GPUs.

## Licence evidence

Source: NVIDIA RTX SDKs licence (`https://raw.githubusercontent.com/NVIDIA/DLSS/main/LICENSE.txt`, "NVIDIA RTX SDKs LICENSE" v. March 14, 2024, plus the DLSS supplement). Key terms:

- §1(c): distribution only as object code incorporated into an application.
- §2(a): the application must have material functionality beyond the SDK.
- §2(c): redistribution on terms at least as protective as NVIDIA's.
- §4(b): no stand-alone distribution of the SDK.
- §4(e): the SDK must not become subject to an open-source licence. This conflicts with distributing it alongside a GPL/MIT-style combined work and must be cleared with NVIDIA before any release.
- §4(d): no circumvention.
- Supplement §1: DLSS usable only on NVIDIA GPUs.
- Supplement §4: NVIDIA must be notified before a commercial release.
- Supplement §7.1(b): attribution and NVIDIA marks on splash, about and credits screens.

The Streamline repository (`https://github.com/NVIDIA-RTX/Streamline`) was not verified: its licence page returned 404 at the URL tried. Do not cite it as checked.

## What was built instead

An FSR1-style spatial upscaler, written from scratch (no AMD FidelityFX code copied):

- `SAMPLER_FSR1` resampler mode in Aurora's present chain (`extern/aurora/lib/webgpu/gpu.cpp`): `sample_easu` (edge-adaptive 4x4 gaussian gather) followed by `fs_rcas` (unsharp-mask sharpen) as a second pass through an intermediate target.
- Enum and API: `extern/aurora/include/aurora/aurora.h` (`SAMPLER_FSR1`, `aurora_set_upscale_sharpness`).
- Render scale uses Aurora's existing `VISetFrameBufferScale` (`extern/aurora/lib/dolphin/vi/vi.cpp`).
- Env configuration: `sunshine-pc/src/render/render_scale.cpp` (`SUNSHINE_UPSCALER=bilinear|area|fsr1`, `SUNSHINE_UPSCALE_SHARPNESS`, `SUNSHINE_RENDER_SCALE` 0.25–1.0), called from `main.cpp` after `aurora_initialize`.

Limits:
- When an RmlUI overlay is active, Aurora bypasses the resample pass (`extern/aurora/lib/aurora.cpp`, `end_frame()`), so the upscaler is not applied to that frame.
- RCAS has no local min/max halo limiter (`ponytail:` comment in `gpu.cpp`); halos are possible on high-contrast edges.

## Upgrade path to DLSS

1. Add per-draw motion vectors and jitter in the GX path (Port.md §5).
2. Move the present target to caller-owned D3D12 textures (or Vulkan with `ExportVulkanImage`, `VulkanBackend.h` l.175–179) with explicit barriers.
3. Obtain the SDK under the NVIDIA RTX SDKs licence and clear §4(e) and the supplement notification before shipping.
