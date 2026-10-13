# Verification

How to check the port without guessing. Everything runs from `sunshine-pc/`.

## 1. Ordered checks: `tools\verify.cmd`

```
tools\verify.cmd [--build] [--matching] [--full] [--boot-secs 45] [--tile 2800,520]
```

Runs these steps in order and prints one line each (`PASS` / `FAIL` / `SKIP`, seconds, detail), then a summary. Exit code 1 if any step FAILs.

| Step | What it checks | Default |
|---|---|---|
| temp-debug | No `TEMP-DEBUG` markers in `sunshine-pc/src`, `include`, `extern/aurora/{lib,include}`, `sms/src` (manual gate: `git grep -n TEMP-DEBUG -- sms/src sms/include sunshine-pc/src sunshine-pc/include` prints nothing; bare `git grep TEMP-DEBUG` always hits docs and `tools/verify.py`) | always |
| aurora-patches | Each patch in `tools/aurora-patches.txt` (name, file, regex) is present | always |
| matching-sha | `sms/build/GMSJ01/mario.dol` hashes to `sms/config/GMSJ01/build.sha1` | always (SKIP if the dol is not built) |
| strict-link | `tools\_strict.cmd` builds `build/strict`; 0 unresolved `LNK2019` | `--build` |
| headless-tests | `tools\port_tests.cmd` | always |
| probe-units | Syntax-only probe over decomp sources | `--full` (slow; rewrites `tools/probe-results.json`) |
| boot-smoke | `build/strict/sunshine.exe --dvd <rvz>` stays alive for `--boot-secs`, no `*** fatal exception` in the log | always (SKIP if the exe is missing) |

Notes:
- `--matching` runs `ninja -k 0` in `sms/` first. Lead only. Nobody else runs plain ninja in `sms/`.
- Boot smoke uses its own window tile (`--tile`) and kills only the process it started. Its log is `build/verify/boot.log`; other step logs sit next to it.
- Step logs go to `build/verify/`.

## 2. Golden frames: `AURORA_SCREENSHOT`

Aurora can save one presented frame as a PNG. The variable is read once at startup:

```
set AURORA_SCREENSHOT=<frame>:<file.png>
```

- `<frame>` uses the same numbering as `AURORA_DUMP_PASSES` (`gfx::current_frame()` before the frame counter advances).
- The PNG is the final output surface after the blit, the RmlUi overlay and ImGui, so it matches what the player sees. It is re-rendered offscreen, so the swapchain is not changed.
- Other pixel formats than BGRA/RGBA are skipped with a warning.
- Without the variable nothing is captured and there is no cost beyond one check per frame.

## 3. Compare frames: `tools/compare_png.py`

```
python tools\compare_png.py golden.png candidate.png [--max-mad 2.0] [--min-ssim 0.98]
```

Prints the mean absolute difference (0-255, RGB) and a global SSIM-style score on luma. Exit 0 = PASS, 1 = FAIL, 2 = unreadable or different size. Needs PIL and numpy.

Typical flow: capture a golden from a known-good build at a fixed frame and scene, capture the candidate the same way, then compare. Pick a frame after the scene has settled.

## 4. Aurora patch manifest: `tools/aurora-patches.txt`

One line per patch: `name | path (under extern/aurora) | regex[ && regex...]`. The check fails when a regex has no match. Update the regex when the code moves; do not weaken the check.
