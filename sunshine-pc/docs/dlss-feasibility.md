# DLSS feasibility

## Verdict

**Not implemented, and not available in this build.** The launcher shows DLSS as a static "unavailable" row with this reason (`assets/launcher/launcher.rml`, id `dlss`). It is not a toggle.

Three blockers. The first two are on the port side and are independent of NVIDIA:

1. **Frame data does not exist yet.** NGX and Streamline need colour, depth, per-pixel motion vectors and jitter offsets. The GX-emulated renderer produces none of motion vectors or jitter. Port.md §5 requires per-draw previous matrices, which are not tracked.
2. **The GPU handoff from Dawn is not available on the default backend.** Aurora renders through Dawn (WebGPU).
   - **D3D12 backend: no export.** `dawn/native/D3D12Backend.h` offers `GetD3D12Device`, `GetD3D12CommandQueue`, `SharedTextureMemoryD3D12ResourceDescriptor` and `SharedBufferMemoryD3D12ResourceDescriptor`. The shared-texture descriptors import an external `ID3D12Resource` into Dawn. Nothing exports a Dawn-created texture as `ID3D12Resource`.
   - **Vulkan backend: export exists.** `dawn/native/VulkanBackend.h` has `ExportVulkanImage(WGPUTexture, ExternalImageExportInfoVk*)`, so a Vulkan-backed device could hand colour/depth to NGX's Vulkan path. Aurora does not select the Vulkan backend on Windows.
   - **Import direction is possible on D3D12.** The port could allocate its own D3D12 colour, depth and motion-vector resources, import them into Dawn, render into them, and pass the same resources to NGX. That needs explicit synchronisation (shared fence) and a present-path rewrite. No such path exists in Aurora today.
3. **No SDK in the repo, and the licence is restrictive.** No DLSS SDK headers (`sl.h`, `nvsdk_ngx`) are in the tree. Only NVIDIA runtime DLLs are installed on the dev machine (`nvngx_dlss.dll`, `nvngx_dlssd.dll`, `nvngx_dlisr.dll`), which are not an SDK. DLSS runs only on NVIDIA GPUs.

Blocker 1 alone makes DLSS a no-op: NGX would receive no motion vectors. Blocker 2 is a real Dawn limitation on D3D12 and needs either the Vulkan backend or a caller-owned D3D12 present path. Forking Dawn is not required for either; the Vulkan export is already in the shipped header.

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
- Not verified visually in game. The title-screen render is still being fixed by another agent.

## Upgrade path to DLSS

1. Add per-draw motion vectors and jitter in the GX path (Port.md §5).
2. Pick one GPU handoff:
   - (a) Vulkan backend for Dawn, then `ExportVulkanImage` for colour, depth and motion vectors into NGX's Vulkan path; or
   - (b) caller-owned D3D12 textures imported with `SharedTextureMemoryD3D12ResourceDescriptor`, with a shared fence for synchronisation.
3. Obtain the SDK under the NVIDIA RTX SDKs licence and clear §4(e) and the supplement notification before shipping.
4. Only then replace the launcher's `dlss` row with a real toggle.
