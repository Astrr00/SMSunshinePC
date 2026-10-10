# Aurora patch series

`extern/aurora` is upstream [encounter/aurora](https://github.com/encounter/aurora) at `50e5be3`, plus the port's changes. The changes are stored here as seven patches so the submodule can be pinned to upstream and the changes re-applied.

Series base: `50e5be3`. Applying all seven to a clean checkout gives the tree the port builds against (verified byte-for-byte, see below).

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

Applied in filename order. Counts are `+`/`-` lines in the patch text. The total is 42 file entries over 33 distinct files, +1552 / -136.

| file | topic | files | +/- |
|---|---|---|---|
| `0001-gx-color-peek-and-pixel-metric.patch` | `GXPeekARGB`, pixel metric (`GXReadPixMetric`/`GXClearPixMetric`, `GXSetDrawSync`), occlusion queries, tests | 18 | +674 / -84 |
| `0002-gx-fifo-implicit-primitive-close.patch` | implicit close of a still-open primitive (`GXVert.cpp`); `fifo::drain()` calls `__GXCloseOpenPrimitive` | 2 | +32 / -16 |
| `0003-gx-tev-alpha-r8-compare.patch` | TEV alpha `R8_GT`/`R8_EQ` mapped to RGB8 variants | 1 | +6 / -0 |
| `0004-card-base-path.patch` | `CARDSetBasePath` directory used by `CARDInit` | 1 | +8 / -9 |
| `0005-fsr1-render-scale.patch` | FSR1 upscale (EASU + RCAS) and sharpness API | 4 | +137 / -22 |
| `0006-pass-dump-and-screenshot.patch` | `AURORA_DUMP_PASSES` pass dump and frame screenshot; `CopySrc` usage flags on render/conv textures (topic by inference, see below) | 12 | +619 / -2 |
| `0007-texture-side-channel.patch` | `GX_AURORA_SET_TEX_DATA` / `SET_TLUT_DATA` / `SET_TEX_TLUT`: texture host pointers and TLUT state passed from `GDTexture.cpp` to the GX command stream | 4 | +76 / -3 |

Patch 0001 also adds `lib/gfx/pixel_metric.{cpp,hpp}` and `lib/gfx/depth_peek.{cpp,hpp}`; patch 0006 adds `lib/gfx/pass_dump.{cpp,hpp}` and `lib/gfx/screenshot.{cpp,hpp}`.

Each patch applies on top of the previous one. Topic boundaries are per line, so several files hold lines from more than one patch (e.g. `lib/aurora.cpp` in 0005 and 0006; `lib/gx/command_processor.cpp` in 0001, 0006, and 0007). The `CopySrc` flags in `lib/gfx/texture.cpp` are assigned to 0006 because they sit with the pass dump; their exact purpose was not traced in the code.

## Verification (run when the series was regenerated)

- `git archive 50e5be3` into a scratch dir, then `git apply --check` and `git apply` of 0001..0007 in order: all seven apply cleanly.
- `diff -r -x .git` between that scratch tree and `extern/aurora`: no differences (exit 0).
- Tool on a scratch clone reset to `50e5be3`: `--check` exit 2 (pristine); apply exit 0; `--check` exit 0 and reapply exit 0 (applied, 33 files match).
- `tools/aurora-patches.txt` regexes: 15 of 15 match the worktree (same check as the `aurora-patches` step of `verify.py`; that step was not run as a whole).

Not verified: compiling any stage. No build was run against the series, so intermediate stages and the final tree are unbuilt.

## Regenerating

The generator is a scratch script outside the repo. It assigns each changed line to a topic by file and regex, writes stage-by-stage diffs from upstream, and asserts the last stage equals the worktree. The patches in this directory are the artifact; keep the topic split when updating. To update after changing `extern/aurora`, regenerate against `50e5be3`, then rerun the verification above.
