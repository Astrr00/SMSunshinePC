# Handoff: Super Mario Sunshine (GMSJ01) decomp + native Windows port

Everything needed to continue on another Windows PC. Read this first, then `README.md` (status table), `ROADMAP.md` (phases, working rules), `Port.md` (port design), `sunshine-pc/docs/status.md` (latest measured state, section "Latest lead update").

## 1. What this repo is

| Part | Path | What |
| --- | --- | --- |
| Decomp | `sms/` | Matching decompilation of GMSJ01 (mwcc 1.2.5 via `sms/configure.py` + ninja). Reproduces `mario.dol` (SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`). |
| Port | `sunshine-pc/` | Native Windows source port: the same decomp sources compiled with clang-cl, platform layer on Aurora (GX/VI/PAD/OS/DVD/CARD/AI), launcher, widescreen, framerate, cheats, mods, glyphs, FSR1. |
| Tools | `sunshine-pc/tools/` | Triage/review queue (`decomp_triage.py`, `decomp_queue.py`), permuter driver, run lock, boot/watch/screenshot helpers. |
| Docs | `sunshine-pc/docs/` | `status.md`, `platform-notes.md`, `decomp-verified.tsv` (equivalence-review ledger), `decomp-worker-protocol.md` (agent protocol v3), release checklist. |

No game data and no PowerPC code are tracked in git. The disc image lives under `sms/orig/GMSJ01/` (untracked); the handoff bundle carries your own extracted copy separately (legal note in section 9).

## 2. Requirements on the new PC

- Windows 10/11 x64, ~60 GB free disk (port build trees are large), 16+ GB RAM, 8+ threads recommended.
- Python 3.12+ (developed on 3.14.7), `pip install -r sunshine-pc/tools/requirements.txt` (the new PC has internet access, so PyPI is used; `wheelhouse/` is an optional offline fallback).
- Visual Studio 2022 Build Tools (C++ workload, Windows SDK) — NOT bundled (licence). `winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`.
- CMake >= 3.28 and Ninja (`pip install ninja` covers Ninja; CMake 4.4 was used).
- LLVM/clang-cl 23.1.3 (bundled as `toolchain/llvm.tar.xz`; extract and set `LLVM_BIN` to its `bin` folder).
- Git.
- Internet on the first port configure: aurora's CMake FetchContent downloads abseil, xxhash, fmt, zlib-ng, libpng, freetype (twilitrealm.dev), SDL3, dawn etc.
- Optional: sccache (build-alt.cmd uses it via winget path), Dolphin (not usable, see below).

## 3. Restore (automated)

Run `sunshine-pc/tools/handoff-setup.ps1` from the bundle root (it clones the repo from `repo.bundle`, extracts the toolchain, installs Python wheels, copies the game data, applies the WIP patch, runs the first gates). Manual equivalent:

1. `git clone repo.bundle "Sunshine Decomp"`; `cd "Sunshine Decomp"`; `git checkout main`; `git submodule update --init` (needs the network; or copy the bundled `aurora-worktree/` over `sunshine-pc/extern/aurora`, then `python sunshine-pc/tools/apply-aurora-patches.py --check` must say `applied: 7 patches`).
2. Extract `toolchain/llvm.tar.xz`; `setx LLVM_BIN <folder>\bin`.
3. `pip install -r sunshine-pc/tools/requirements.txt` (online).
4. Copy the game data folder to `sms/orig/GMSJ01/` (keep the layout: `Super Mario Sunshine (Japan).rvz`, `extracted/`, `files/`, `sys/`).
5. Copy `toolchain/sms-build-tools/*` to `sms/build/tools/` and `toolchain/sms-build-compilers/` to `sms/build/compilers/` (otherwise `configure.py` downloads dtk/objdiff/sjiswrap/compilers).
6. Optional repos next to the project (sibling folders): `decomp-permuter/` (used by `sunshine-pc/tools/permute_fn.py`; path `..\decomp-permuter` relative to the repo's parent) and `sms-upstream/` (doldecomp/sms upstream bodies for reference, commit 20b801cb).
7. Optional: `git apply wip-uncommitted.patch` (see section 7).
8. `sms/build/triage.json` + `decomp_queue.json` (bundle `state/`) are needed by `decomp_queue.py`; the setup copies them. To regenerate triage after a full `ninja`: `python sunshine-pc/tools/decomp_triage.py` (writes `sms/build/triage.json`).
9. Set `origin` to `https://github.com/Astrr00/SMSunshinePC.git` (the setup does this; a bundle clone points at the bundle file).
10. sccache is optional (`winget install Mozilla.sccache`); `build-alt.cmd` finds it if present.

## 4. Gates (run these to confirm the new PC reproduces the state)

```
cd sms
python configure.py            # once
ninja -k 0 -j4                 # full matching build
certutil -hashfile build\GMSJ01\mario.dol SHA1    # must be 9f5a8caf56f5356aeac9d3ed28bf8de976a03625
ninja baseline && ninja changes_all                # per-function score drops (none allowed)
cd ..\sunshine-pc
set LLVM_BIN=<llvm>\bin
python tools\locked.py cmd /c tools\_strict.cmd    # strict link: 0 unresolved symbols
python tools\apply-aurora-patches.py --check       # "applied: 7 patches"
```
Expected at handoff: 11 012 / 12 881 functions byte-identical, 445 / 736 units Matching, 58.56 % code, 97.98 % fuzzy.

## 5. Decomp workflow (what the agents did; works for people too)

- Rule (user, latest): byte-identical where possible; otherwise own code is fine if behaviour equals retail, proven from the retail asm and recorded as `logic-verified` in `sunshine-pc/docs/decomp-verified.tsv`. Units marked `Matching` stay 100 %; DOL SHA-1 stays. Asm-proven source fixes carry `// asm 0x<addr>`. Shared headers may be edited (rebuild every includer, no drop in Matching units).
- Tools: `python sunshine-pc/tools/decomp_triage.py --build <unit>` (single-unit mwcc + per-function %), `--diff <unit> <sym>`, `--check <unit> <sym>` (call order, vtable slots, fused FP, opcode histogram, pool constants), `--branchscan [unit]`, `--immscan [unit]`. m2c: `python -m m2c.main --target ppc-mwcc-c++ -f <mangled> sms/build/GMSJ01/asm/<Dir>/<Unit>.s`.
- Queue (multi-agent): `python sunshine-pc/tools/decomp_queue.py claim|hold|release|free|status|ledger ...` (state `sms/build/decomp_queue.json`; the ledger gate downgrades `logic-verified` rows lacking `branches mapped`). Full agent protocol: `sunshine-pc/docs/decomp-worker-protocol.md` (v3).
- Bug classes that were real (look for them first): inverted branches mislabelled as codegen, wrong switch case sets (differing binary-search pivots), wrong member/field offsets, `this` vs parameter base, swapped call arguments/matrix indices, wrong constants (pool values), wrong vtable slot, misplaced braces (a `b` landing in a different block), signed vs unsigned compares.
- Known limit: functions where retail calls header-inline helpers out of line (JGeometry TVec3 dot/scale/sub, TUtil sqrt/mod, MsClamp, getMActor, TExPane setters, TSpineBase::getLatestNerve ...). No header mechanism found (see `platform-notes.md` "out-of-line header helpers"); accept behaviour-equal own code.
- Open work: ~750 game functions without a finished equivalence review (list = `decomp_queue.py status` and `sms/build/triage.json`), 614 `partial` ledger rows.

## 6. Port workflow and pitfalls

- Build: `sunshine-pc\tools\build-alt.cmd <Name>` builds into `sunshine-pc\build\<Name>` (needs `LLVM_BIN`; global build lock via `tools/locked.py`). Strict link check: `tools\_strict.cmd`.
- Run the game ONLY via `sunshine-pc/tools/watch.py` / `shot.py` (invisible, silent: `CREATE_NO_WINDOW|BELOW_NORMAL`, temp APPDATA; never touch `%APPDATA%\Sunshine\saves`). The game needs the disc: `--dvd <rvz/iso path>`.
- `tools/reap.py --loop` (if running) kills `sunshine.exe` that is older than 6 min or older than `src/sdk/av.cpp`: rebuild after av.cpp edits, keep runs < 6 min, check `build/reap.log` when a process dies. Serialise game runs with `python sunshine-pc/tools/runlock.py acquire <name>` / `release` (owner-only release, `touch` heartbeat, log in `build/runlock.log`). Kill only your own PID.
- Dev switches (`SUNSHINE_DEV_START=bianco/0`, `SUNSHINE_DEV_SCRIPT`, `SUNSHINE_DEV_STATUS`, `SUNSHINE_DEV_SEED`, `SUNSHINE_DEV_MAX_SECONDS`, `SUNSHINE_CHECK_HEAP`, `AURORA_SCREENSHOT*`) are for diagnosis; the acceptance flow (B1) must run without start/skip switches.
- Aurora is a submodule pinned at 50e5be3 plus the patch series `sunshine-pc/patches/aurora/0001-0007`; never leave unrecorded edits in `extern/aurora` (`apply-aurora-patches.py --check` refuses "mixed" trees).
- Dolphin as a reference emulator is blocked (apploader stall, popups); do not run it.

## 7. State at handoff and next steps

Measured state: see `README.md` table and `sunshine-pc/docs/status.md` ("Latest lead update"). Last pushed commit: see `git log -1` (docs commit `e36f87c` at handoff time, code wave `a3a6709`).

Uncommitted work in progress at handoff (in `wip-uncommitted.patch`, NOT in git): the pad-clock rework — `sms/src/JSystem/JUtility/JUTGamePad.cpp` (script clock in `JUTGamePad::read()` before `PADRead`), `sunshine-pc/src/devscript/devscript.cpp` (+111 lines, `[padclock]` counter), `sunshine-pc/src/main.cpp`. It builds but airport0 is not reached with it yet (scripts are tick-timed while THP movies advance on their own clock, suspected).

Next steps in order:
1. B1: make the dev pad script condition-based: `waitfor stage=<n> scen=<n>`, `waitfor mario`, `waitfor dir.state=<n>`, `waitfor movie=0` with a required `timeout=N` that fails the run. Then drive: title -> file select -> new game -> opening -> airport0 -> walk to `MapObjStartDemo` (-6630,500,1600; `MapObjTown.cpp:353-364`, `fireStreamingMovie(2)` -> `setNextStage(0x101)`) -> Delfino (dolpic0) -> Bianco gate: `gateopen.sb` sets flag 0x10384 when the Bianco GateKeeper is dead (3 nozzle hits in vulnerable windows, `gatekeeper.cpp`). `bianco/0` and `dolpic/0` run stable 100 s on dev start.
2. B2: collect a real Shine (hit-check fixes landed), Shine sequence, save `.gci` (check with `tools/gci_check.py`), reload.
3. B3: intermittent flat red/blue wedge in Bianco frames (~1 in 6 runs at frame 600, also without widescreen/FSR; command stream differs run to run before the renderer — see `build/ArtifactHunt` notes in `status.md`), title background, audio noise after ~20 s, particles.
4. C: verify widescreen (4:3 identity needs deterministic capture), interpolation at 60/120/144, mod overlay effect, water cheat, glyph asset (PlayStation X is a square), DLSS stays a documented blocker.
5. Decomp: continue equivalence review (protocol v3), re-run `--branchscan/--immscan`, flip fully matching units to `Matching` one at a time with the SHA-1 check.

## 8. Repo hygiene notes

- `sunshine-pc/build/` is huge (tens of GB of experiment trees) and untracked; start fresh on the new PC.
- Untracked scratch at the repo root (`artifact_*.txt`, `_mr.json`, `crash.log`, `$env`, `sms/_e*.py`, `sms/_*.py`) is leftover and can be deleted.
- Stateroot (`.stateroot/`, `AGENTS.md` StateRoot block) is optional project memory. `stateroot handoff write` failed on the old PC (CLI error 'system cannot find the path'), so no CLI-written handoff exists; `state/stateroot/` is a raw reference copy (never edit `.stateroot` by hand). Continue from this file, `status.md` and the git log instead.

## 9. Legal

The disc image and extracted game files are the owner's own copy and must never be committed or published. The handoff bundle contains them only for local transfer; delete the `game-data/` folder from any shared medium.
