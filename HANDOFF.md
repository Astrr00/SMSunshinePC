# Handoff: Super Mario Sunshine (GMSJ01) decomp + native Windows port

Everything needed to continue on another Windows PC. Read this first, then `sunshine-pc/docs/session-2026-10-11.md` (detailed record of the last session), `README.md` (status table), `ROADMAP.md` (phases, working rules), `Port.md` (port design), `sunshine-pc/docs/status.md` (latest measured state, section "Latest lead update").

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

1. `git clone repo.bundle "Sunshine Decomp"`; `cd "Sunshine Decomp"`; `git checkout main`; `git submodule update --init` (needs the network; or copy the bundled `aurora-worktree/` over `sunshine-pc/extern/aurora`, then `python sunshine-pc/tools/apply-aurora-patches.py --check` must say `applied: 11 patches`).
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
# per-function score drops: worktree procedure below (never `ninja baseline && ninja changes_all`)
cd ..\sunshine-pc
set LLVM_BIN=<llvm>\bin
python tools\locked.py cmd /c tools\_strict.cmd    # strict link: 0 unresolved symbols
python tools\apply-aurora-patches.py --check       # "applied: 11 patches"
```
Expected at handoff: 11 317 / 12 881 functions byte-identical, 445 / 736 units Matching, 62.56 % code, 98.64 % fuzzy.

### Baseline for changes_all (corrected 2026-10-12)

`ninja baseline` regenerates `build\GMSJ01\baseline.json` from the current tree on every run (`sms/build.ninja`, target always). `ninja baseline && ninja changes_all` therefore always reports no change. Build the baseline in a clean worktree instead:

1. `git worktree add --detach <path> d414ba0` (the clean base commit; use the previous round's commit as base next time).
2. `cmd /c mklink /J` junctions from the worktree to the main tree for `sms/orig`, `sms/build/tools`, `sms/build/compilers`, `sms/build/binutils`. The worktree's own `sms/build` stays a real directory.
3. In the worktree's `sms/`: `python configure.py --dtk build/tools/dtk.exe --objdiff build/tools/objdiff-cli.exe --sjiswrap build/tools/sjiswrap.exe --compilers build/compilers --binutils build/binutils`. Relative tool paths only: an absolute path containing a space ("Sunshine Decomp") lands unquoted in `build.ninja` and breaks argparse.
4. `ninja -k 0 -j3` (742 steps, about 27 s; check the DOL SHA-1).
5. `ninja baseline` in the worktree only. Copy its `build/GMSJ01/baseline.json` to the main tree's `sms/build/GMSJ01/`.
6. In the main tree run `ninja changes_all` alone.
7. Remove the junctions with `cmd /c rmdir` (no `/S`), then `git worktree remove`.

`decomp_triage.py --build <unit>` overwrites that unit's `.o` under `sms/build/GMSJ01/src`; `report.json` is not regenerated until the next full `ninja`.

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
- Scripted runs: `python tools/route.py <tree> <script.dev> --name <n> --seconds 330` from `sunshine-pc` (wraps the rules above: invisible, muted, temp APPDATA, run lock, own PID only). Scripts live in `tools/routes/`; verbs and conditions are listed in `docs/session-2026-10-11.md` section 2.1. Run `tools\_strict.cmd` from PowerShell (Git Bash turns `cmd /c` into `cmd C:\`), and start long runs with the harness's background mode, not `( ... &)`. After a killed run release a stale lock with `python tools/runlock.py release lead`.
- Route 70 (`tools/routes/70_boss_until.dev`, from `sunshine-pc`) needs `SUNSHINE_ALLOW_CPU_ADAPTER=0` in its command: `python tools/route.py Strict tools/routes/70_boss_until.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`. Without it the game takes the CPU adapter: about 21 logic ticks/s; runs r8a and an earlier one ended rc=124 at logic t=219-235 s and looked like a hang. With it: about 70 ticks/s (r8b: 21180 ticks in 298 s); u01/u02 about 120 ticks/s. `[status]` `t` is logic time (tick/30), not wall time.
- A devscript TIMEOUT exits with rc 3 (a failure). Until 2026-10-12 it returned 0xC0000409: `std::exit` skipped `aurora_shutdown()`, so Aurora's static pipeline-worker `std::thread` was still joinable at the static destructors (`std::terminate`). `main.cpp` now registers `aurora_shutdown` with `atexit` (see `platform-notes.md`, "Exit after a devscript TIMEOUT").
- Dev switches (`SUNSHINE_DEV_START=bianco/0`, `SUNSHINE_DEV_SCRIPT`, `SUNSHINE_DEV_STATUS`, `SUNSHINE_DEV_SEED`, `SUNSHINE_DEV_MAX_SECONDS`, `SUNSHINE_CHECK_HEAP`, `AURORA_SCREENSHOT*`) are for diagnosis; the acceptance flow (B1) must run without start/skip switches.
- Aurora is a submodule pinned at 50e5be3 plus the patch series `sunshine-pc/patches/aurora/0001-0011`; never leave unrecorded edits in `extern/aurora` (`apply-aurora-patches.py --check` refuses "mixed" trees).
- Dolphin as a reference emulator is blocked (apploader stall, popups); do not run it.

Aurora note: the old PC's aurora checkout reported HEAD 94431fd (a local commit, not on a known remote branch), but `apply-aurora-patches.py --check` says its files equal upstream 50e5be3 + patches 0001-0007 (0008, the FIFO diagnostics patch, was added on 2026-10-11). On a new PC `git submodule update --init` checks out 50e5be3; then run `python sunshine-pc/tools/apply-aurora-patches.py` (the setup script does) and expect `applied: 11 patches` (the series is 0001-0011). `aurora-worktree/` in the bundle is the already-patched fallback.

## 7. State at handoff and next steps

Measured state: see `README.md` table and `sunshine-pc/docs/session-2026-10-11.md` (gates, observed vs inferred, decomp
changes, crash analysis). Highlights (2026-10-11):

- Gates: SHA-1 ok, 11 013 / 12 881 functions, 445 / 736 units, strict link 0 unresolved, Aurora series 11 patches.
- **The retail flow is now scripted by conditions and reproducible up to airport0**: from `sunshine-pc`, after
  `tools\_strict.cmd`: `python tools/route.py Strict tools/routes/07_airport.dev --name <n> --seconds 330`.
  Title (option map, stage 15) -> card prompts (`cardprog=19`) -> bump file block B -> new game -> opening movie -> airport0
  (stage 0, scen 0). No dev start switches. The pad-clock rework (script clock = logic pad reads) is committed.
- Open at the end: walking from the airport0 spawn (819,500,1246) to `MapObjStartDemo` (-6630,500,1600) did not arrive
  (path wanders, Mario falls from y 500 to 300 near x -500..-960). A GX FIFO crash at ~204 s (tick 6120) was traced to two
  decomp `GXBegin` counts (4 instead of 8) and fixed; the post-fix run was cut short at 196 s, so **the fix is not yet confirmed**.

Next steps in order:
1. Re-run route 07 for 330 s; confirm no `overrun` in `run.log` (patch 0008 prints the offending caller otherwise).
2. B1: reach `MapObjStartDemo` (waypoints along the airport0 walkable path), `waitfor` Delfino (`dolpic0`), Bianco gate
   (`gateopen.sb` sets flag 0x10384 when the GateKeeper is dead: 3 nozzle hits in vulnerable windows, `gatekeeper.cpp`),
   then B2: collect a real Shine, save `.gci` (check with `tools/gci_check.py`), reload. Each step with log and PNG.
3. B3: white option-map/title screenshots, intermittent flat red/blue wedge in Bianco frames (~1 in 6 runs at frame 600),
   audio noise after ~20 s, particles.
4. C: verify widescreen (4:3 identity needs deterministic capture; route 07 is now deterministic enough to try), interpolation
   at 60/120/144, mod overlay effect, water cheat, glyph asset (PlayStation X is a square), DLSS stays a documented blocker.
5. Decomp: audit a sample of the 170 new `logic-verified` rows (see the caveats in the session log), fix
   `TBathWaterManager::throwMario`, re-check `TBossEel::perform`, continue protocol v3 on the remaining open functions,
   flip fully matching units to `Matching` one at a time with the SHA-1 check.

## 8. Repo hygiene notes

- `sunshine-pc/build/` is huge (tens of GB of experiment trees) and untracked; start fresh on the new PC.
- Untracked scratch at the repo root (`artifact_*.txt`, `_mr.json`, `crash.log`, `$env`, `sms/_e*.py`, `sms/_*.py`) is leftover and can be deleted.
- Stateroot (`.stateroot/`, `AGENTS.md` StateRoot block) is optional project memory. `stateroot handoff write` failed on the old PC (CLI error 'system cannot find the path'), so no CLI-written handoff exists; `state/stateroot/` is a raw reference copy (never edit `.stateroot` by hand). Continue from this file, `status.md` and the git log instead.

## Phase-1 acceptance status (2026-10-11, measured)

Reached with dev switches (diagnosis only): Delfino gate opens and warps, select menu works, Bianco Ep.1 `stage=2 scen=0` with a controllable Mario (`sunshine-pc/tools/routes/33_gate_wash.dev`, `docs/img/bianco0_ep1.png`). Also reached: a real Shine collect path (dev verb `shine 2` on the Bianco 100-coin shine) -> `shines=1`, `.gci` written and `gci_check.py` PASS (`s08`), and a switch-free reload of that save to `stage=1 scen=5 shines=1 dirstate=4` (`s15`, `tools/routes/38_reload_probe.dev`). Not reached: the no-switch route to Bianco (< 330 s), real GateKeeper kill, real Shine in Bianco, save/load in Bianco, two clean full runs. Details and what was tried: `sunshine-pc/docs/session-2026-10-11b.md` sections 10-16. Update (later in the same session): both GateKeepers die from the water jet alone with the dev switch `SUNSHINE_DEV_AIM_ASSIST=1` (airport run `a01`; Delfino run `a17`: hp 3 to 0, `flag 0x10384` met, `tools/routes/50_delfino_aim_assist.dev`); without it the head is not hit (squat: L = side-walk, R-only loses the crouch; stick BACK raises the nozzle; Delfino needs hits inside frames 50..160 of bck 0x12). The 330 s target cannot be met at retail timing: logic is locked to the 30 Hz retrace (`fps_limit` only adds replay frames), the measured pieces are logos to airport1 tutorial about 290 s (route 12, no dev switches), airport boss about 30 s game time, Delfino boss about 110 s (a17: 3741 ticks / 127 s), shadow-Mario chase and gate about 2000+ ticks, select menu and Bianco load another 30-60 s: roughly 550-650 s in total, not measured as one run. Still missing: a switch-free route, the real Shine without the dev verb, two clean full runs. Next: hit the airport boss head without `killboss`; real shadow-Mario chase; Bianco Shine (try the ramp / 100-coin shines, `tpshine` takes positional floats `tpshine x y z` (a `key=value` form parses as 0 0 0; not tried with real offsets)).

## 9. Legal

The disc image and extracted game files are the owner's own copy and must never be committed or published. The handoff bundle contains them only for local transfer; delete the `game-data/` folder from any shared medium.

### Update 2026-10-12 (measured): first full chain logos -> Bianco -> Bianco Shine -> save -> reload

Route `sunshine-pc/tools/routes/60_acceptance.dev`, run twice in a row (`h01`, `h02`): rc=0, 144 s and 147 s wall time, no `overrun`, no crash, 13 PNGs each (airport0 `ap0.png`, Delfino `dolpic.png`, gate `gate.png`, Bianco `bianco.png`, Shine get `shine_get.png`/`shine_get2.png`). Stages seen in `run.log`: 15 (option map) -> 0/0 -> 0/1 -> 1/0 -> 1/1 -> 2/0 (Bianco, `dir.state=4`, Mario steerable, status `0x0C400201`) -> 1/5. `shines=2`, `.gci` PASS (`tools/gci_check.py`), reload with `tools/routes/61_reload.dev` (file block B, real time): `stage=1 scen=5 shines=2`, rc=0 (`h05`).
Command: `python tools/route.py Strict tools/routes/60_acceptance.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`.
**What is NOT retail input (the chain is dev-assisted, so the acceptance wording "no dev switches" is not met):** `SUNSHINE_DEV_STEP_RETRACE` (logic faster than 30 Hz; the 144 s is not 30 Hz wall time), `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST` (faces the head, sets nozzle pitch, forces the crouch; the Delfino spray gate), script verbs `cheat infinite_health/infinite_water`, `tp` (pit/boss/gate positions), `tpem` (shadow-Mario chase), `shine 2` (touches the Bianco 100-coin Shine). Real: airport boss killed with water via assist + short `hold L` cycles, airport Shine taken by walking to it and pressing A, Delfino GateKeeper killed with water (flag `0x10384` in the log), gate walk-in, Bianco load.
Bug found and fixed on the way: `JAIData::initSeqParameter` left `mSeqHandle` 0 on PC, so `JAISeqEntry::storeBuffer` stopped the SE master sequence and `processFrameWork` crashed at the airport exit (session log section 38).

### Update 2026-10-12 (later): route 70, fewer dev crutches

`sunshine-pc/tools/routes/70_boss_until.dev` run twice in a row (`u01` 153 s, `u02` 193 s wall, rc=0, no `overrun`, no crash, 13 PNGs, `gci_check.py` PASS, reload `stage=1 scen=5 shines=2`). Remaining dev: `cheat infinite_health`, `chase` (position read, real input), `shine 2`, env `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST`. Gone: `tp`, `tpem`, `killboss`. Not met: a real Bianco Shine without `shine 2` (no measured floor path to any of the three Shines), 330 s at real 30 Hz, no dev switches. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 42.

### Update 2026-10-12 (decomp pass): 11 020 / 12 881 functions byte-identical

After a 12-agent decomp pass (details in `sunshine-pc/docs/session-2026-10-11b.md` section 44), measured by the lead: DOL SHA-1 unchanged, matched functions 11 013 -> 11 020, units 445 / 736, fuzzy total 98.12 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Several real behaviour bugs fixed (Koopa nerves, BathtubKiller, bossgesso, ModelWaterManager, fenceMove, Kumokun, GCConsole2).

### Update 2026-10-12 (decomp round 2)

Gates: SHA-1 unchanged, 11 020 / 12 881 functions byte-identical, 445 / 736 units, fuzzy total 98.23 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Route 70 smoke after the edits: 3 of 6 runs complete; one chase fall, one rare crash in the shadow-Mario replay (`TEnemyMario::emReplayJumpToNearestNode`, run `v02`, cause open). Details: session log section 45.

### Update 2026-10-12 (decomp round 3): 11 112 / 12 881 functions byte-identical (86.3 %)

Measured after round 3: SHA-1 unchanged, 11 112 / 12 881 functions byte-identical (86.3 %), matched code 59.67 %, fuzzy 98.23 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 47.

### Update 2026-10-12 (decomp round 4): 11 191 / 12 881 functions byte-identical (86.9 %)

Measured: SHA-1 unchanged, 11 191 / 12 881 byte-identical (86.9 %), matched code 60.69 %, fuzzy 98.25 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: session log section 49.

### Update 2026-10-12 (decomp round 5): 11 247 / 12 881 functions byte-identical (87.3 %)

Measured: SHA-1 unchanged, 11 247 / 12 881 byte-identical (87.3 %), matched code 61.48 %, fuzzy 98.27 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: session log section 50.

### Update 2026-10-12 (decomp round 6): 11 282 / 12 881 functions byte-identical (87.6 %)

Measured: SHA-1 unchanged, 11 282 / 12 881 byte-identical (87.6 %), matched code 61.98 %, fuzzy 98.60 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0; route 70 smoke 1 of 2 complete (chase flake). Details: session log section 52.

### Update 2026-10-12 (decomp round 7 committed): 11 317 / 12 881 functions byte-identical (87.9 %)

Measured: SHA-1 unchanged, 11 317 / 12 881 byte-identical (87.858 %), matched code 62.56 %, fuzzy 98.64 %, matched data 87.47 %, 445 / 736 units, `ninja -k 0 -j4` rc 0, strict link 0 unresolved, Aurora `applied: 11 patches`, `TEMP-DEBUG` 0. Commits 513e37a (round 7) and d66592c (docs) are on origin/main; base d414ba0 (11 282). Details: session log section 53.

Open:

- Chase (Delfino shadow Mario): see the update `2026-10-12 (route 72)` below. The walking `chase` of route 70 fails 9 of 9; route 72 (`chase tp=1`) passes 2 of 2.
- Null-rail guard (`enemyMario.cpp` ~L979-986): static analysis says the guard never fires; the crash read of run v02 is unexplained. Hypothesis only.
- Exit after devscript TIMEOUT: fixed (rc 3 observed). `OSResetSystem` and `AURORA_EXIT` also end with `std::exit(0)` and now run `aurora_shutdown` through `atexit`; verified only for a devscript TIMEOUT (rc 3) and a normal script end (rc 0).
- Ledger rows (updated by round 8): 1983 logic-verified, 149 matched, 255 partial (rows, not unique functions). 141 rows differ by more than 0.5 points from current scores; 41 rows unmapped.
- Hunk audits still unverified: `SelectDir.cpp`, `JPAField.cpp` loadFieldBlock, `CardLoad.cpp` setCenteredSize, `GCConsole2`, `cameralib.cpp` middle part, `bossgesso`, `MapEventMare`/`MapEventSink`.
- Follow-ups to a finished agent are lost after its context compaction; start a fresh agent with the full context.

### Update 2026-10-12 (route 72): chase crutch `tp=1`, working Delfino-to-Bianco chain again

Observed: route `sunshine-pc/tools/routes/72_boss_tpchase.dev` (route 70 with `chase timeout=4000 tp=1`), command as for route 70 (see section 4, `SUNSHINE_ALLOW_CPU_ADAPTER=0` required). Two consecutive runs: `t72g` (150 s wall) and `t72h` (191 s wall), rc=0, `overrun` 0, `fatal exception` 0, 13 PNGs each. Stages: 0/0, 15/0, 0/0, 0/1, 1/0 (logic t=284 s), 1/1, 2/255, 2/0 (Bianco, t=550 s / 726 s), 1/5; `shines=2`. `chase tp=1` met after 421 ticks in both runs (8 teleport cycles, water counter 64 -> 0). `tools/gci_check.py` on the `t72h` card: PASS. Reload with `tools/routes/61_reload.dev` on a copy of that appdata: rc=0, `stage=1 scen=5 shines=2`.

Use route 72 as the regression route (decomp rounds, port changes) until the walking chase works. It re-introduces a teleport crutch (route 70 had none): every 60 ticks Mario is put 250 units behind the shadow Mario (100 units when the shadow is down) and sprays for 45 ticks. Reason: with the retail-faithful wall test (`MapCheck.cpp` `checkFlag(0x8)`, asm 0x801DE8C0-0x801DE8C8, never revert) the walking `chase` fails 9 of 9; a floor-grid planner passed 0 of 6. The chase ends with 64 water hits on the shadow (`mWaterCtMax` 64, `TEnemyMario::hitWater`), then `doing=0xF` and the talk window, not at a graph node. The shadow flees from Mario along its replay links (ring B-C-D-E-F-G-B, `linkdata.bin`, 7 nodes; rail graph `mariomodoki` has 9; read by an agent, not re-decoded by me). A future walking chase has to stay within spray range of a shadow that runs at Mario's speed.

Open items for Phase 1: walking chase, `cheat infinite_health` (the Delfino boss hurts a standing Mario: health 4 -> 0 at the spray spot in an earlier run), `shine 2`, `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST`, 30 Hz real-time evidence, two clean runs in a row without dev switches.

### Update 2026-10-12 (decomp round 8): three real fixes, gates green

Measured by the lead: SHA-1 unchanged, 11 317 / 12 881 functions byte-identical (87.86 %, unchanged), matched code 62.56 %, fuzzy 98.6378 %, 445 / 736 units, `ninja -k 0 -j4` rc 0, `changes_all` against `d414ba0`: 227 rows, 3 drops, all decided (`BathWaterManager`, `TKumokun::checkOnMovingWall`). Strict link 0 unresolved. Route 72: `r8a`, `g8b` rc=0. Use a fresh `--name` for every `route.py` run: it reuses `build/Route/<name>/appdata`, and an old save changes the flow (run `r8b` skipped the airport and failed `mash ... until stage=0`).

Fixes (all asm-proven): `TKumokun::checkOnMovingFloor`/`checkOnMovingRoof` out-pointer is a stack temp, not `param_2` (asm 0x80332090, 0x80331EE8); `TEnemyMario::emReplayJumpToNearestNode` candidate points share the `nextPoint` stack slot (asm 0x80255B5C, 0x80255B74, 0x80255FAC). Retail has no bounds check on `replayLinks[nodeIndex]` (the source matches), so the run `v02` null-rail read is most likely an unchecked `nodeIndex` outside `[0, unk8)`; not proven. Ledger: 1 983 logic-verified, 149 matched, 255 partial.

Open: about 33 functions in `decomp_queue.py status` (`koopajr`, `GCConsole2`, `KoopaNeckCallBack`, `calcNrm`, `wireRolling`, `TModelWaterManager::move` partial, singles). The decomp workers are slow on big functions (20-40 min each); give one function per worker, tell them to read asm with `read <file>:raw:<from>-<to>` in chunks of about 150 lines.

### Update 2026-10-13: fresh-clone gate passed (commit 06668e5)

Measured by the lead in a new folder: clone, `git submodule update --init`, `apply-aurora-patches.py` (11 patches), pip, game data copied, `configure.py`, `ninja -k 0 -j4` in `sms/` (rc 0, 31 s, SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`), `tools\locked.py cmd /c tools\_strict.cmd` (rc 0, 238 s, 0 unresolved), route 72 (rc 0, 137 s, `chase met` after 421 ticks, `stage=1 scen=5 shines=2`). The section 3 steps work as written; `handoff-setup.ps1` itself was not run. Two defects found and fixed: `tools/locked.py` needed an existing `sunshine-pc/build`; `tools/_strict.cmd` was git-ignored (rule `tools/_*`), so the gate script of section 4 was missing from clones. `probe-units` (clang-cl, `CLANG_CL`): 733 units, 623 clean, 110 failing, all 110 in `excluded-units.txt`. Evidence for `Goal-Port.md` section 6: `Goal-Port.md` section 8.

### Update 2026-10-13: route 72 in real time

Observed (details: ROADMAP update `2026-10-13 (route 72 in real time, 30 Hz)`): without `SUNSHINE_DEV_STEP_RETRACE` and `SUNSHINE_DEV_SKIP_MOVIES`, route 72 passed once (`rt2`, rc=0, 1010 s wall, logic 30/s in 835 of 1001 one-second samples, 31 in 135, stage 2/0 at logic t=970 s) and failed once (`rt1`, `walkto` stood still at (3692,704,-2716), cause not found). `walkto` now recovers from a stall (sideways steering plus a jump after 120 ticks under 30 units; `tools/routes/74_walk_stall_test.dev`). Command for the real-time run: `python tools/route.py Strict tools/routes/72_boss_tpchase.dev --name <n> --seconds 1800 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0` (use a new `--name` every time; start it as a background job; takes about 17 minutes). Do not run other CPU-heavy work during a timing run.

### Update 2026-10-13: route 85 (no `shine 2`)

`tools/routes/85_accept_no_shine2.dev`: route 72 without `shine 2`, ends at Bianco controllable. Run `a85a` (stepped command): rc=0, 203 s, `stage=2 scen=0`, `shines=1`, `.gci` PASS. Use this route as the Phase-1 acceptance candidate; crutches left: `SUNSHINE_DEV_AIM_ASSIST`, `cheat infinite_health`, `chase tp=1`.
