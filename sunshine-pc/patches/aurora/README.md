# Aurora patch series

`extern/aurora` is upstream [encounter/aurora](https://github.com/encounter/aurora) at `50e5be3`, plus the port's changes. The changes are stored here as eighteen patches so the submodule can be pinned to upstream and the changes re-applied.

Series base: `50e5be3`. Applying all eighteen to a clean checkout gives the tree the port builds against (verified byte-for-byte, see below).

## Apply

    python tools/apply-aurora-patches.py          # apply, or no-op if already applied
    python tools/apply-aurora-patches.py --check  # report only

States the tool reports:

| state | meaning | exit |
|---|---|---|
| applied | every touched file already matches the series result | 0 |
| pristine | every touched file matches upstream `50e5be3`; series is applied | 0 (2 with `--check`) |
| mixed | some files match neither (e.g. partly reverted); nothing is changed | 1 |

The tool needs the `50e5be3` commit in the aurora repo (`git -C extern/aurora cat-file -e 50e5be3^{commit}`). The configure step of `CMakeLists.txt` runs `--check` and warns when the series is not applied.

## Patches

Applied in filename order. Counts are `+`/`-` lines in the patch text. The total is 87 file entries over 48 distinct files, +2327 / -216 (recounted with 0020).

| file | topic | files | +/- |
|---|---|---|---|
| `0001-gx-color-peek-and-pixel-metric.patch` | `GXPeekARGB`, pixel metric (`GXReadPixMetric`/`GXClearPixMetric`, `GXSetDrawSync`), occlusion queries, tests | 18 | +674 / -84 |
| `0002-gx-fifo-implicit-primitive-close.patch` | implicit close of a still-open primitive (`GXVert.cpp`); `fifo::drain()` calls `__GXCloseOpenPrimitive` | 2 | +32 / -16 |
| `0003-gx-tev-alpha-r8-compare.patch` | TEV alpha `R8_GT`/`R8_EQ` mapped to RGB8 variants | 1 | +6 / -0 |
| `0004-card-base-path.patch` | `CARDSetBasePath` directory used by `CARDInit` | 1 | +8 / -9 |
| `0005-fsr1-render-scale.patch` | FSR1 upscale (EASU + RCAS) and sharpness API | 4 | +137 / -22 |
| `0006-pass-dump-and-screenshot.patch` | `AURORA_DUMP_PASSES` pass dump with per-draw descriptions (`TitleRender` fields in `command_processor.cpp`); frame screenshot with `AURORA_SCREENSHOT_EVERY=<n>:<file.png>` live monitor mode and `aurora_screenshot_next(path)`; `CopySrc` usage flags on render/conv textures (topic by inference, see below) | 12 | +705 / -2 |
| `0007-texture-side-channel.patch` | `GX_AURORA_SET_TEX_DATA` / `SET_TLUT_DATA` / `SET_TEX_TLUT`: texture host pointers and TLUT state passed from `GDTexture.cpp` to the GX command stream | 4 | +76 / -3 |
| `0008-gx-fifo-desync-diagnostics.patch` | on a FIFO "draw vertex data overrun" the parser prints the last 8 parsed draws (vertex layout snapshot) and, from a 8192-entry ring of GXBegin calls, the GXBegin neighbours of the failing stream position with their caller address; diagnostics only, no behaviour change | 4 | +112 / -0 |
| `0009-gx-fifo-overrun-diagnostics-ring.patch` | on a FIFO overrun the parser also prints the last 64 FIFO commands (opcode@position) and a 256-byte hex dump before the failing draw; diagnostics only (found the J3DShape slot-tail bug, see `sunshine-pc/docs/session-2026-10-11b.md`) | 1 | see patch |
| `0010-gx-corrupt-texobj-diagnostics.patch` | `GXLoadTexObj` prints caller and module base for a texture object larger than 4096 px; `resolve_static_texture` logs implausible static textures before the allocation; diagnostics only (found a corrupt `JUTTexture` in a `J2DPicture`, see `sunshine-pc/docs/session-2026-10-11b.md`) | 2 | see patch |
| `0011-gx-alpha-compare-scalar.patch` | `tev_alpha_op` maps the GR16/BGR24 compares to the scalar RGB8 compare (alpha operands are `f32`; `.rg`/`.rgb` made the shader fail and aborted the process once the shadow Mario was drawn); see `sunshine-pc/docs/session-2026-10-11b.md` |
| `0012-pipeline-cache-read-error-no-use-after-abort.patch` | `load_pipeline_cache_entries` returns right after `pipeline_cache_abort()` when reading the rows fails; before, it called `sqlite3_reset`/`sqlite3_clear_bindings` on the finalized (null) load statement, and a corrupt `pipeline_cache.db` ("database disk image is malformed") crashed `aurora_initialize` with 0xc0000005; see `sunshine-pc/docs/platform-notes.md`, "Pipeline cache per run; corrupt cache" | 1 | +2 / -1 |
| `0013-gx-copy-readback.patch` | `GXSetCopyReadback(dest, w, h)`: an exact 1:1 `GX_CTF_R8` copy into a registered `dest` is read back (texture-to-buffer copy after the conversion, async map in `depth_peek::after_submit`, one readback in flight per dest) and written into `dest` as GX I8 tiles on the game thread inside the next `GXCopyTex(dest)`, like a GameCube EFB copy writes main RAM; registering destroys the old copy texture of `dest`, `dest == NULL` drops all registrations; one log line on the first apply (bytes changed against the CPU copy, latency) and one after 300 applies (latency range). GX draws inside offscreen passes (`GXCreateFrameBuffer`) compile their pipeline blocking instead of being skipped (a skipped pollution base pass at stage start wiped the goop maps). Used by the pollution maps (`MarDirectorInitECT.cpp`, TARGET_PC), so `isPolluted` sees sprayed goop | 12 | +299 / -8 |
| `0014-gx-copy-readback-off-null-backend.patch` | `register_copy_readback` ignores every registration when the adapter is Dawn's Null backend (`SUNSHINE_ALLOW_CPU_ADAPTER=0` without a GPU) and logs once "Copy readback off: Null backend": Null renders nothing, so its R8 copies are empty and the readback of 0013 wiped the goop maps (airport nonzero 6306 -> 0), which stalled the Delfino boss in route 85; the maps keep their load state as before 0013. Other backends unchanged | 1 | +9 / -0 |
| `0015-fsr1-sharp-easu-rcas-limiter.patch` | FSR1 rework (own implementation): EASU maps output pixel centres like the bilinear sampler (`pos * scale - 0.5`; before `(pos - 0.5) * scale`, a 1.5 px shift at 4x), Lanczos-2 weight rotated to the 2x2 luma edge and stretched along it up to 1.8x instead of the 4x4 Gaussian, result clamped to the min/max of the nearest 2x2 texels (no ringing); RCAS (`k = 0.25 * sharpness`, unchanged) limited to the min/max of its 4-neighbour cross (no halos); default sharpness 0.8 (k 0.2; before 0.2, k 0.05), `aurora.h` doc comment updated; `SUNSHINE_UPSCALE_SHARPNESS` keeps its scale (1 = k 0.25). Bilinear and area paths unchanged; measurements in `sunshine-pc/docs/platform-notes.md` (T2 section) | 2 | +38 / -20 |
| `0016-gx-scissor-12bit-fields.patch` | scissor BP registers 0x20/0x21 use 12-bit coordinate fields (hardware: 11) in `GXSetScissor`, `GXGetScissor` and the `bp_scissor` decode, values masked to 12 bits: with the +342 offset an 11-bit field ends at a right edge of 1705, so a widescreen EFB of 1708 px (32:9) encoded rt 2049, decoded as 1, the scissor came out empty and every frame was black; now right edges up to 3753 work (from code; run at W 1708 only), all values below 2048 encode as before; see `sunshine-pc/docs/platform-notes.md`, "Widescreen pixel checks" | 3 | +17 / -12 |
| `0017-gx-static-z24x8.patch` | size and decode static `GX_TF_Z24X8` depth textures using the existing 64-byte 4×4 tile decoder; required by bath-water's `clear_z_TX` at Corona stage 60/0 | 2 | +2 / -0 |
| `0018-gx-z24x8-copy-readback.patch` | genuine Z24X8 depth conversion and canonical 4×4/two-plane CPU readback through the existing generation-guarded transport; preserves R8 layout, records copy formats, rejects unsupported depth conversion; constexpr packing checks include width 84 and padded rows | 8 | +127 / -34 |
| `0019-thread-skip-single-core-cache-domain.patch` | `pin_shared_cache` pins main, FIFO worker and render worker only to a cache domain of at least 4 processors; a VM reporting one L3 per vCPU pinned all three to one core and starved the game thread (70 to 180 ms baton wake latency, audio dropouts every few seconds) | 1 | +4 / -1 |
| `0020-host-wait-hooks.patch` | `aurora_set_host_wait_hooks` and `thread::HostWaitScope`: the embedding application is called around the blocking waits on Aurora's workers (FIFO drain, blocking pipeline compile, DVD command wait) so the port can let its audio thread run meanwhile; audio dropouts at scene loads | 7 | +34 / -0 |

Patch 0001 also adds `lib/gfx/pixel_metric.{cpp,hpp}` and `lib/gfx/depth_peek.{cpp,hpp}`; patch 0006 adds `lib/gfx/pass_dump.{cpp,hpp}` and `lib/gfx/screenshot.{cpp,hpp}`.

Patch 0008 was produced with `python tools/make_aurora_patch.py 0008-gx-fifo-desync-diagnostics.patch <files>` (diff of the working tree against the result of the earlier patches). To symbolise a caller address: `llvm-symbolizer --obj=build/<tree>/sunshine.exe --relative-address <address - module base>`.

Each patch applies on top of the previous one. Topic boundaries are per line, so several files hold lines from more than one patch (e.g. `lib/aurora.cpp` in 0005 and 0006; `lib/gx/command_processor.cpp` in 0001, 0006, and 0007). The `CopySrc` flags in `lib/gfx/texture.cpp` are assigned to 0006 because they sit with the pass dump; their exact purpose was not traced in the code.

## Verification (run when the series was regenerated)

Current 0018 source `b1392e5` (2026-10-15): checker reports 18 applied/tracked patches over 45 files.
Full verification: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks and 45-second WARP boot pass.
The isolated checkout applies 0018 and rebuilds/links the affected sources successfully.
Real WARP depth copies reach the CPU mesh at render scales 0.5/0.25; R8 goop cleaning also passes.

Earlier 0017 snapshot (2026-10-15): `tools/apply-aurora-patches.py --check` reports 17 applied patches over
44 files, and `verify --build --matching --boot-secs 45` reports 7 PASS, 0 FAIL, 1 optional SKIP.
An isolated fresh checkout updated to `21e2b60` applied 0017 separately and rebuilt both texture sources
and the Strict executable with zero unresolved symbols. Earlier checks below describe older series snapshots.

- `git archive 50e5be3` into a scratch dir, then `git apply --check` and `git apply` of 0001..0007 in order: all seven apply cleanly (0008 was added later and is checked by `tools/apply-aurora-patches.py --check`, "applied: 8 patches").
- `diff -r -x .git` between that scratch tree and `extern/aurora`: no differences (exit 0).
- Tool on a scratch clone reset to `50e5be3`: `--check` exit 2 (pristine); apply exit 0; `--check` exit 0 and reapply exit 0 (applied, 33 files match).
- `tools/aurora-patches.txt` regexes: 15 of 15 match the worktree (same check as the `aurora-patches` step of `verify.py`; that step was not run as a whole).

Intermediate series stages were not built; the current 18-patch final tree builds and links as recorded above.

## Regenerating

The generator is a scratch script outside the repo. It assigns each changed line to a topic by file and regex, writes stage-by-stage diffs from upstream, and asserts the last stage equals the worktree. The patches in this directory are the artifact; keep the topic split when updating. To update after changing `extern/aurora`, regenerate against `50e5be3`, then rerun the verification above.
