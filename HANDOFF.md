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
- Optional: sccache (`_strict.cmd`/`build-alt.cmd` use it only with `SUNSHINE_SCCACHE=1`: cache hits across checkouts leave ninja deps on the other checkout's headers), Dolphin (not usable, see below).

## 3. Restore (automated)

Run `sunshine-pc/tools/handoff-setup.ps1` from the bundle root (it clones the repo from `repo.bundle`, extracts the toolchain, installs Python wheels, copies the game data, applies the WIP patch, runs the first gates). Manual equivalent:

1. `git clone repo.bundle "Sunshine Decomp"`; `cd "Sunshine Decomp"`; `git checkout main`; `git submodule update --init` (needs the network; or copy the bundled `aurora-worktree/` over `sunshine-pc/extern/aurora`, then `python sunshine-pc/tools/apply-aurora-patches.py --check` must say `applied: 13 patches`).
2. Extract `toolchain/llvm.tar.xz`; `setx LLVM_BIN <folder>\bin`.
3. `pip install -r sunshine-pc/tools/requirements.txt` (online).
4. Copy the game data folder to `sms/orig/GMSJ01/` (keep the layout: `Super Mario Sunshine (Japan).rvz`, `extracted/`, `files/`, `sys/`).
5. Copy `toolchain/sms-build-tools/*` to `sms/build/tools/` and `toolchain/sms-build-compilers/` to `sms/build/compilers/` (otherwise `configure.py` downloads dtk/objdiff/sjiswrap/compilers).
6. Optional repos next to the project (sibling folders): `decomp-permuter/` (used by `sunshine-pc/tools/permute_fn.py`; path `..\decomp-permuter` relative to the repo's parent) and `sms-upstream/` (doldecomp/sms upstream bodies for reference, commit 20b801cb).
7. Optional: `git apply wip-uncommitted.patch` (see section 7).
8. `sms/build/triage.json` + `decomp_queue.json` (bundle `state/`) are needed by `decomp_queue.py`; the setup copies them. To regenerate triage after a full `ninja`: `python sunshine-pc/tools/decomp_triage.py` (writes `sms/build/triage.json`).
9. Set `origin` to `https://github.com/Astrr00/SMSunshinePC.git` (the setup does this; a bundle clone points at the bundle file).
10. sccache is optional (`winget install Mozilla.sccache`); `_strict.cmd` and `build-alt.cmd` find it and use it only with `SUNSHINE_SCCACHE=1` (a hit from another checkout replays its `/showIncludes`, so header edits would not rebuild those objects).

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
python tools\apply-aurora-patches.py --check       # "applied: 13 patches"
```
`python sunshine-pc/tools/verify.py --matching` (from the repo root; `sunshine-pc\tools\verify.cmd --matching` loads the MSVC environment first) runs `ninja -k 0` in `sms/` + the DOL SHA-1 check, the TEMP-DEBUG grep, the Aurora series check (`--check` count vs the tracked patches, clean `patches/aurora`), the line-ending check of the changed files, the headless tests and the boot smoke (one PASS/FAIL/SKIP line each, exit 1 on any FAIL). If ninja runs `download_tool.py`, re-run `configure.py` with the relative tool paths of the worktree procedure below. Until the line-ending step is proven, also check the touched files with `git ls-files --eol`. The strict link stays manual (`--build` rebuilds `build/strict` in the tree it runs in).
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
- Scripted runs: `python tools/route.py <tree> <script.dev> --name <n> --seconds 330` from `sunshine-pc` (wraps the rules above: invisible, muted, temp APPDATA and USERPROFILE, run lock, own PID only). Scripts live in `tools/routes/`; verbs and conditions are listed in `docs/session-2026-10-11.md` section 2.1. Run `tools\_strict.cmd` from PowerShell (Git Bash turns `cmd /c` into `cmd C:\`), and start long runs with the harness's background mode, not `( ... &)`. After a killed run release a stale lock with `python tools/runlock.py release lead`. Launcher runs: add `--no-dvd` (same rules, the exe starts without `--dvd <disc>`, so the launcher runs or `[launcher] skip_launcher=1` starts `last_image`); pre-write `build/Route/<name>/appdata/Sunshine/settings.ini` to set the launcher state, and `--appdata-from` to reuse a run's INI.
- Route 70 (`tools/routes/70_boss_until.dev`, from `sunshine-pc`) needs `SUNSHINE_ALLOW_CPU_ADAPTER=0` in its command: `python tools/route.py Strict tools/routes/70_boss_until.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`. Without it the game takes the CPU adapter: about 21 logic ticks/s; runs r8a and an earlier one ended rc=124 at logic t=219-235 s and looked like a hang. With it: about 70 ticks/s (r8b: 21180 ticks in 298 s); u01/u02 about 120 ticks/s. `[status]` `t` is logic time (tick/30), not wall time.
- A devscript TIMEOUT exits with rc 3 (a failure). Until 2026-10-12 it returned 0xC0000409: `std::exit` skipped `aurora_shutdown()`, so Aurora's static pipeline-worker `std::thread` was still joinable at the static destructors (`std::terminate`). `main.cpp` now registers `aurora_shutdown` with `atexit` (see `platform-notes.md`, "Exit after a devscript TIMEOUT").
- Dev switches (`SUNSHINE_DEV_START=bianco/0`, `SUNSHINE_DEV_SCRIPT`, `SUNSHINE_DEV_STATUS`, `SUNSHINE_DEV_SEED`, `SUNSHINE_DEV_MAX_SECONDS`, `SUNSHINE_CHECK_HEAP`, `SUNSHINE_DEV_HIT_LOG`, `AURORA_SCREENSHOT*`) are for diagnosis; the acceptance flow (B1) must run without start/skip switches. `SUNSHINE_DEV_HIT_LOG=1` adds a `[-spray]` line (cumulative GateKeeper head/body spray hits from `gDbgGKHeadSprayHits`/`gDbgGKBodySprayHits` in `sms/src/Enemy/gatekeeper.cpp`, TARGET_PC only) to each `logBoss` line; routes 80-82 need it.
- Aurora is a submodule pinned at 50e5be3 plus the patch series `sunshine-pc/patches/aurora/0001-0013`; never leave unrecorded edits in `extern/aurora` (`apply-aurora-patches.py --check` refuses "mixed" trees).
- Dolphin as a reference emulator is blocked (apploader stall, popups); do not run it.

Aurora note: the old PC's aurora checkout reported HEAD 94431fd (a local commit, not on a known remote branch), but `apply-aurora-patches.py --check` says its files equal upstream 50e5be3 + patches 0001-0007 (0008, the FIFO diagnostics patch, was added on 2026-10-11). On a new PC `git submodule update --init` checks out 50e5be3; then run `python sunshine-pc/tools/apply-aurora-patches.py` (the setup script does) and expect `applied: 13 patches` (the series is 0001-0013). `aurora-worktree/` in the bundle is the already-patched fallback.

## 7. State at handoff and next steps (2026-10-13, HEAD `6a552da`)

Start here. Details per topic are in the `### Update 2026-10-13 ...` sections at the end of this file, in `ROADMAP.md`
(Phase 1 and Phase 2 sections) and in `sunshine-pc/docs/platform-notes.md`. Agent reports and patches are not in git
(session-local): first session in `C:/Users/Administrator/Documents/Projekte/handoff-2026-10-13/` (62 files), second
session in `C:/Users/Administrator/Documents/Projekte/lead-2026-10-13b/` (`<Agent>_report.md`, `*.patch`, `TODO.md`,
UI plans in `uiplans/`).

**State (observed):** origin/main `6a552da`. Landed since `a49732c`: `14f65b1` verify.py line-endings step,
`26b6a78` Aurora 0014 (copy readback off on the Null backend), `7665296` + `bf86825` UI-4a in-game menu, `3d224da` +
`bac9d7e` frame-rate replay crash, `f0c4bc6` T1 boot matrix, `c86ed12` routes 85/72 robustness, `080e4fc` + `20c5992`
GoopCount (counters from the CPU map), `3a61348` DSP overload voice steal skipped on PC, `243b386` master volume at the
AI output, `b67b438` + `44581e8` AudioRef docs, `b22eec0` replay circle shadows, `6a552da` T4 launcher chain. Gates:
as recorded in each commit message (agents' clone builds). Aurora series: 14 patches (0001-0014). Branch `agent/docs3`
(not on origin yet): `ced1fdb` T2 pixel diff, `a205e79` empty `SUNSHINE_RENDER_SCALE` no longer masks the INI scale,
and the docs commit that follows them.

- **Phase 1: DONE under user-accepted dev aids** (runs accA1/acc86a and accB1/acc86b at `846d81f`; ROADMAP Phase 1).
  Dev aids accepted by the user: `SUNSHINE_DEV_AIM_ASSIST`, `chase tp=1`, `cheat infinite_health`, env
  `SUNSHINE_ALLOW_CPU_ADAPTER=0`. User rule: all dev cheats are allowed in test runs; switch them off only for damage
  tests. The route 85 regression after `cfe7373` is fixed (`26b6a78`, `c86ed12`; item 4 below).
- **Phase 2: exit met (2026-10-13), except the human listening check (PENDING, owner).** As in Phase 1 this holds under
  dev aids, not retail input: dev start, scripted walk/spray/jump input with `cheat infinite_health` and `cheat
  infinite_water`, WARP only (this machine has no hardware GPU). Done items (ROADMAP Phase 2):
  1. **10-minute soaks.** Bianco (`bf86825`, clone-chase, bianco/0, WARP, page heap `SUNSHINE_CHECK_HEAP=2`,
     `SoakBianco_report.md`): rc 0, 647.2 game s, 0 fatal/overrun/TIMEOUT, audio 0 clips, 10 s RMS median -24.7 dBFS,
     dips mostly in the slowest stretch on the shared host ([INFERENCE] host stalls). Delfino Plaza soakD1 (`20c5992`,
     main tree, dolpic/0, WARP, route `lead-2026-10-13b/soak_walk_spray49.dev`): rc 0, 646.8 game s, 0 fatal, audio
     median -25.1 dBFS, 0 clips, silent only at boot and stage load. Cleaning was not exercised in the soaks.
  2. **Goop cleans in game logic (`cfe7373`, Aurora 0013), on a rendering adapter.** Each pollution layer pass renders
     offscreen at the map's size and the R8 copy is read back into `mPollutionMap`, 2-4 frames late. On the Null
     backend the readback is off since `26b6a78` (Aurora 0014; Null read back empty copies), so the maps keep their
     load state there. Counters from the CPU map (`080e4fc`, `20c5992`, `sms/src/Map/PollutionCount.cpp`): route 96
     (`96_goop_count_bianco.dev`, WARP) raises the Bianco sink building when its goop is cleaned
     (`GoopCountLand_report.md`). Not verified: hardware GPU, other goop and boss stages, fpsLimit != 30, window resize.
  3. **Audio.** Clipping fixed (`93a9a1c`, `AudioFix_report.md`), output matrix and effect lines (`a49732c`,
     `AudioMatrix_report.md`), THP L/R (`45015f6`), DSP overload voice steal skipped on PC (`3a61348`) and master
     volume at the AI output (`243b386`), docs `44581e8` (`AudioRef_report.md`). The title BGM stop at about 21.5 s is
     retail: the title jingle is one 11 s sequence without a loop (`AudioRest_report.md`). **PENDING (owner):** a person
     listens to the WAV excerpts (`listen_bianco_music.wav`, `listen_bianco_spray.wav`). Not verified: no LLE or
     console capture; ramp shape and the auto-mixer send law are approximations.
  4. **Route 85 regression after `cfe7373`: fixed.** `26b6a78` (Aurora 0014) brings route 85/86 back on Null (gn85a,
     gn85b, gn86a); `c86ed12` (ramp waypoints, `waitfor camDemo=0` + `faceto` before the gate sprays) fixes the
     post-chase fragility (gclN1/gclN2 Null, gclW1 WARP, gclR86 pass). A seeded stepped Null run on `20c5992` showed
     no divergence vs the baseline in 100 status lines; RefDecompFix also ran route 85 (rc 0, shines=1, on its
     unlanded branch; `RefDecompFix_report.md`). Not ruled out: exe dependence of the post-chase failures (note in
     `20c5992`).
  5. **Logic rate below 30/s: explained, no code change** (`LogicRate2_report.md`). route.py's screenshot monitor
     (`AURORA_SCREENSHOT_EVERY=60:live.png`) is the main cost; with it off WARP holds 30/s at idle (at or above 30/s in
     92.4 % and 93.3 % of samples vs 54.5 % and 54.0 % with it); vsync off (Mailbox) did not raise the rate;
     [INFERENCE] WARP vs Null is not the cause (8 idle runs). Timing runs set `AURORA_SCREENSHOT_EVERY=` (empty); since
     `ced1fdb` route.py removes an empty `KEY=` from the game's environment, and Aurora treats unset as off too
     (`extern/aurora/lib/gfx/screenshot.cpp:58-60`). Not run: a loaded run (host contention half tested).
  6. **Exit-time heap crash: fixed (`32f3941`).** The muted AI pump buffer is `thread_local` (`std::exit` freed the
     static under the detached pump); forced interleaving 4 of 4 faults before, 0 of 4 after; 18 of 18 natural TIMEOUT
     runs clean (`HeapCrash2_report.md`). Still open: host threads are not joined at exit; window close,
     `OSResetSystem` and launcher quit not run.
  7. **Frame-rate replay:** the abort at `fpsLimit > 30` is fixed (`3d224da`, `bac9d7e`), circle shadows are drawn
     once per frame (`b22eec0`); details in `sunshine-pc/docs/platform-notes.md`.
- **Next: Phase 3** (ToggleMatrix `ToggleMatrix.md`: T1, T2 and T4 done). In this order: T3 Pacing + Fullscreen
  (timing runs, so on an idle machine with `AURORA_SCREENSHOT_EVERY=` empty); T5 InputHooks; FSR1 follow-ups (the fix
  landed as Aurora patch 0015, measured on WARP in one scene; real GPU, 1:1 frames and a native reference open); UI
  track A: UI-4b, UI-14B, UI-15 in-game half (after UI-4b), UI-7, UI-16 (plans in `lead-2026-10-13b/uiplans/`). Owner
  decisions needed first: UI-11 hash table, UI-12 network. Owner: listen to the audio excerpts (the last Phase 2
  check). Known gap of T4: "Unlock all nozzles" with a loaded save NOT verified, and a game over before the first save
  returns the nozzles to 0x00000 (code read).
- UI packages: done UI-1..3, 4a (in-game menu: R+Start or F1 opens the settings page over the frozen game), 5, 6, 8,
  9, 10, 13; UI-15 launcher half (menu sounds, synthetic tones, runs `ui15fix2_*`); open UI-4b, UI-7, 11, 12, 14-16 (UI-15 in-game half after UI-4b). Music volume (D4) is folded into the master volume row (the BGM level
  lives in `sms/src/MSound/MSoundBGM.cpp:8`, `MSBgm::smMainVolume`).
- Decomp: 16 units / 34 functions open (hard leftovers: koopajr 9, GCConsole2 7, elecNokonoko 3, ...), all claims free.
  Ledger cleaned (`40999a3`).
- Main tree at `6a552da`: `git status` at the start of the Docs3 session showed no uncommitted tracked changes and
  11 untracked scratch routes in `sunshine-pc/tools/routes/` (76-82, 90, 94).

**Not done / known gaps:** Delfino GateKeeper cannot be killed with pad input only (Mario stands on goop in OIL_RUN and
cannot squat; three attempts failed, see platform-notes; not retried since the goop logic changed), so `AIM_ASSIST`
stays. Bianco flat-colour screen is mitigated (zeroed plain `new`), root object unknown. Route 86 sees `thp=0`
thousands of ticks before the status lines do (unexplained). Switch-free retail flow from the logos without dev aids
and a Bianco Shine are not proven.

**How the lead worked (keep this; rules approved by the owner on 2026-10-13):**
- The lead only plans, reviews, lands and pushes; every task goes to a fresh agent with a 45-75 min time box, facts in
  the brief, observed vs inferred separated. Agents that already reported tend to repeat their old report instead of
  a new task: give new tasks to fresh agents. The omp advisor is no longer used.
- Game/build agents work in clones: `C:/Users/Administrator/Documents/Projekte/fresh-gate-2`, `clone-chase` and
  `clone-3` (being set up). Each clone has a `lead` remote, so agents can fetch commits the lead has not pushed yet.
- Agents commit locally on a branch `agent/<name>` in their clone and never push; the lead fetches from the clone,
  cherry-picks onto main and pushes. `git add` with explicit paths only. Never `git checkout`/`reset`/`stash`/`clean`
  in the main repo (one agent did once, nothing was lost).
- Locks are per clone: runs go through the run lock (`tools/route.py`), builds through `tools/locked.py`, inside one
  clone. The lead serialises timing runs across clones; timing runs set `AURORA_SCREENSHOT_EVERY=` (empty).
- Before every push (working rule 1): `python sunshine-pc/tools/verify.py --matching` from the repo root (TEMP-DEBUG
  grep, Aurora series count and clean `patches/aurora`, matching DOL SHA-1, line endings of the changed files); until
  its line-ending step is proven, also `git ls-files --eol` on the touched files; the strict link
  (`python tools\locked.py cmd /c tools\_strict.cmd` in a clone with the exact tree); and the fresh-clone build passes.
- Before pushing a game-behaviour change: route 85 stepped on the Null backend (`python tools/route.py Strict
  tools/routes/85_accept_no_shine2.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1
  SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`) plus one WARP run of the affected spot (route.py's default
  adapter, `SUNSHINE_ALLOW_CPU_ADAPTER=1`; a full route 85 on WARP took 646 s wall in rg85w, so raise `--seconds`).
  Current baseline: route 85 passes on Null and WARP since `26b6a78` + `c86ed12` (gclN1/gclN2, gclW1; seeded runs
  with `SUNSHINE_DEV_SEED=1` compare state lines with `lead-2026-10-13b/gcl_diverge.py`). The `cfe7373` regression
  was found only after the push.

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

`tools/routes/85_accept_no_shine2.dev`: route 72 without `shine 2`, ends at Bianco controllable (stage 2/0, `bianco_end.png`, no Bianco Shine). Run `a85a` (stepped command): rc=0, 203 s, `stage=2 scen=0`, `shines=1` (the airport Shine [INFERENCE]), `.gci` PASS. Route 85 has no reload step; the reload is covered by `tools/routes/86_accept_reload.dev` (commit `a6ae867`, ROADMAP Phase 1, blocker c): `python tools/route.py Strict tools/routes/86_accept_reload.dev --name <n> --appdata-from <route-85 run>`; run `fg85reload` rc 0, after load `stage=1 shines=1`, reloaded card byte-identical, `gci_check` PASS (still with `SUNSHINE_DEV_AIM_ASSIST` and the stepped env). Not accepted as the Phase-1 candidate yet. Dev aids left: `SUNSHINE_DEV_AIM_ASSIST`, `cheat infinite_health`, `chase tp=1`.

### Update 2026-10-13: health crutch analysis

`cheat infinite_health` compensates goop damage while the bot stands still at the Delfino spray spot (see ROADMAP update `2026-10-13 (why cheat infinite_health is needed)`). Route 77 (`tools/routes/77_boss_no_health_cheat.dev`) is route 72 without the cheat: it dies at tick 10620 and times out. Diagnostics `SUNSHINE_DEV_HPLOG=1` (env-gated) exist in the working tree in `sms/src/Player/MarioCollision.cpp`, `MarioMove.cpp`, `MarioReceiveMsg.cpp` and reference `gDbgGKCurBck` from the gatekeeper diagnostics: commit them together or not at all.

### Update 2026-10-13 (later): flat-colour Bianco screen mitigated, cause open; boss killed with pad input only

Wedge (observed, route 92 = `tools/routes/92_bianco_wedge_probe.dev`: `SUNSHINE_DEV_START=bianco/0`, no input, screenshot `w600.png` at logic tick 600; Strict build, WARP adapter): anomalous images (dark blue or purple screen, red wedge) are found with a script that takes the share of the dominant 16-level colour bucket of a 160x120 downscale: normal 0.059 (every normal image falls in the same bucket), anomalous 0.256-0.924.

| variant (env `SUNSHINE_DEV_HEAP_FILL`, temporary probe, removed again) | runs | anomalous |
| --- | --- | --- |
| none (CRT heap contents as they come) | 14 (6 + 8, two sessions) | 8 (2 + 6) |
| fill every plain-new block with 0x00 / 0xAA / 0xFF / 0x7F | 6 each, 24 total | 0 |
| `touch` (read one byte per cache line, contents unchanged) | 8 | 3 |
| production build, plain `new` zeroed (`sunshine-pc/src/sdk/jkr_new_delete.cpp`, commit 7bbcaee) | 12 | 0 |

Observed: any deterministic fill removes the anomaly, so what decides is the content of a plain-new block, not allocation timing alone (touch-only still shows 3 of 8). The `[status-cam]` lines of anomalous and normal runs are identical (target, up, fov, near, far, `[status-cam2]` pos), so the camera state that is logged is not the carrier. Not identified: which object reads uninitialised memory (INFERENCE: a field that no decomp constructor writes; retail arena memory is not documented here). The fix is a port-level zero fill; size-range bisection of the fill (to find the class) is the next step if the object matters.

Other fixes of this round (observed): `GetAtanTable` index clamp under `TARGET_PC` (`sms/src/MarioUtil/MathUtil.cpp`, commit 9d1f0ca): route 91 had crashed once at tick 662 with a NaN-derived index in the camera; 3 of 3 later runs complete; matching build unchanged.

Real-input boss fight (observed): `fightboss timeout=N` in `sunshine-pc/src/devscript/devscript.cpp` plays the airport GateKeeper fight with `setVirtualPad` only (crouch held, R spray and stick Y -127 in the head windows bck 0x3/0x11/0xF with head y > 700, stick X toward the head, sleep phase bck 0xA/0x4 body spray with stick Y following the nozzle angle). Route `tools/routes/76b_airport_boss_fight.dev` (prefix of route 54, cheats `infinite_health` and `infinite_water` still on): boss dead in 3 of 3 runs of an agent in a fresh clone and 1 of 1 run in the main tree (`bb76b_0531`): hp 3 -> 2 at tick 2525-2527, 2 -> 1 by 2840, 1 -> 0 in the window 3374-3440, verb done at tick 3586-3589, 929-930 ticks after the bot started. The log field `headHits` sums increases of `unk154` and is not a hit count (`unk154` is reset by `CUE_MOVE`); the kill evidence is `hp`. Not done: Delfino GateKeeper with pad input (it still uses `SUNSHINE_DEV_AIM_ASSIST`), the health and water cheats, the real-time (30 Hz) run of this route.

### Update 2026-10-13 (Phase 1 acceptance)

Acceptance chain (observed by agents Accept and Reload86RT; HEAD `846d81f` plus the route-86 fix; Strict exe 09:11:43; clone `fresh-gate-2`; 0 unresolved): route 85 (`tools/routes/85_accept_no_shine2.dev`, route 72 without `shine 2`, ends at Bianco Ep.1 stage 2 scen 0, no Bianco Shine), then route 86 (`tools/routes/86_accept_reload.dev`, `--appdata-from` the route-85 run) reloading the route-85 card. Real time: no `SUNSHINE_DEV_STEP_RETRACE`, no `SUNSHINE_DEV_SKIP_MOVIES`. Phase 1 is marked DONE under dev aids by user decision via Main (2026-10-13); the exit wording "no dev switch" is met only by that decision. The section "route 85 (no `shine 2`)" above is superseded for the acceptance chain. Details: ROADMAP.md Phase 1.

| Run | Route | rc | Wall | Logic 30/s samples | Result |
| --- | --- | ---: | ---: | ---: | --- |
| accA1 | 85 | 0 | 1171 s | 940/1156 (81.3 %) | stage 2 scen 0 (Bianco Ep.1), shines=1, gci_check PASS, 0 overrun, 0 fatal |
| accB1 | 85 | 0 | 1001 s | 840/994 (84.5 %) | stage 2 scen 0 (Bianco Ep.1), shines=1, gci_check PASS, 0 overrun, 0 fatal |
| acc86a | 86 from accA1 | 0 | - | 261/312 (83.7 %) | shines 0->1 at tick 3420 after load, 0 fatal/overrun |
| acc86b | 86 from accB1 | 0 | - | 237/311 (76.2 %) | shines 0->1 at tick 3390, 0 fatal/overrun |

Logic 30/s in 76-85 % of one-second samples (76.2 % for acc86b). No threshold is defined.

Commands (`--seconds <S>`):

```
python tools/route.py Strict tools/routes/85_accept_no_shine2.dev --name <n> --seconds <S> SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0
python tools/route.py Strict tools/routes/86_accept_reload.dev --name <n2> --appdata-from <n> --seconds <S> SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0
```

`--seconds` for accA1/accB1 was not recorded. The default is 330 (`tools/route.py` line 36) and the walls were 1001-1171 s, so S was above 1171 `[INFERENCE]`.

Dev aids (user decision via Main, 2026-10-13; none is retail input; cheats are switched off only for damage tests):
- `SUNSHINE_DEV_AIM_ASSIST=1`: faces the head, sets nozzle pitch, forces the crouch (Delfino spray gate).
- `SUNSHINE_ALLOW_CPU_ADAPTER=0`: keeps the game off the CPU adapter (about 21 ticks/s without it).
- `chase timeout=4000 tp=1` (route 85 line 333): teleports Mario behind the shadow Mario and sprays; counts DONE for the chase item.
- `cheat infinite_health` (route 85 line 305).
- `cheat infinite_water` and `godmode`: accepted; named in the run record; not in the route 85, 86 or 72 files.

Open items:
- Route 86: `waitfor thp=0` was met at tick ~6564, but status samples show thp=2 until ~8670. Not explained. Before the fix, runs accA2/accB2 pressed A during the Delfino load movie (thp=2) and failed with rc 3.
- Route 86 line 1 says "Stepped env as fg85"; stale, not edited. Route 85 header lines 5-6 still describe the step-retrace chain; not edited.
- Reload count: acc86a/acc86b record `shines 0->1 after load`; the route-85 section above records `fg85reload` as `stage=1 shines=1`. Not reconciled; check `build/Route/acc86a/run.log`.
- Exit fatal after a TIMEOUT run: fixed in `32f3941`; see platform-notes "Exit fatal after a TIMEOUT run (fixed in
  `32f3941`)".
- Delfino GateKeeper without aim assist: see platform-notes "Delfino GateKeeper without aim assist (open, 2026-10-13)".
- walkto stall (route 72 line 292): cause not found; see ROADMAP Phase 1, background (b).
- accA1 ran its first minutes with two orphan `cygpath.exe` processes (load 54 %); killed afterwards. Measurement caveat.
- Uncommitted at writing: `sunshine-pc/tools/routes/86_accept_reload.dev` (the fix) and `sunshine-pc/src/devscript/devscript.cpp` (4 changed lines; content not inspected).
