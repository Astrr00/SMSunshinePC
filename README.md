# Super Mario Sunshine for Windows (port in progress)

A native Windows x86-64 port of Super Mario Sunshine (GameCube, GMSJ01). The game code is the decompilation in `sms/`, compiled with clang-cl against [Aurora](https://github.com/encounter/aurora), which replaces the Dolphin SDK layer (GX, VI, PAD, OS, DVD, CARD). No emulator, no PowerPC code.

## Status

Not playable end to end yet. Legend: done = observed in a run or check, partial = works with gaps or unverified, open = not working. % is the lead's estimate of how much of the item works, not a measurement. Updated with every milestone. Last measured: 2026-10-12.

**Decompilation progress (measured 2026-10-12, `sms/` full `ninja` + `changes_all` against a clean build of `d414ba0`):** 11 317 of 12 881 functions byte-identical = **87.9 %** (87.86); matched code **62.56 %** (by bytes); fuzzy match **98.64 %**; units complete **445 of 736 = 60.5 %**; data 87.5 %. DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` unchanged. About 1 560 functions are still not byte-identical (most of them between 98.5 % and 100 %). The numbers are measured from `report.json`; they are refreshed after every decomp round.

| Area | State | % | Evidence / gap |
| --- | --- | ---: | --- |
| Matching decomp build (SHA-1 `9f5a8caf…`) | done | 100 | `ninja` in `sms/` |
| Decomp logic correctness (non-matching functions) | partial | 55 | 11 317/12 881 functions bytegleich (62.56 % code, 98.64 % fuzzy), 445/736 units Matching; ledger `sunshine-pc/docs/decomp-verified.tsv` (2026-10-12, rows counted by status prefix; some functions have several rows under different name spellings): 1 977 `logic-verified` + 149 `matched` rows, 250 `partial` rows; rows written by agents were only partly audited, and the open list still holds several hundred game functions without a finished equivalence review (most blocked by header-inline codegen). Own code is allowed where byte-identical is not reachable if behaviour equals retail. Asm-proven bug fixes include inverted branches, wrong case sets, wrong members/offsets/constants/indices (e.g. CardLoad scale 1.5f, camera sub-stick axes, Kumokun velocity member, `TModelWaterManager::move` ground probe and state-2 plane store). Changes against the previous commit were compared per function with `changes_all`: 200 functions improved, 3 got worse (asm-proven behaviour fixes) |
| Strict link, 0 unresolved symbols | done | 100 | `tools/_strict.cmd` |
| Units compiled (clang-cl) | done | 100 | 583/583 built units, 150 excluded with reasons (`sunshine-pc/excluded-units.txt`, `docs/platform-notes.md`) |
| Boot: logo, intro movies | done | 100 | normal `--dvd` boot |
| Title screen | partial | 70 | logo, Shine Sprite, seagulls render, logic 30/s; area behind the logo black (retail background unverified) |
| Title → file select → new game → Delfino | partial | 75 | dev-assisted route `sunshine-pc/tools/routes/70_boss_until.dev` (via `tools/route.py`, env `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST` and `SUNSHINE_ALLOW_CPU_ADAPTER=0`, no `SUNSHINE_DEV_START`) goes logo → option map → card prompts → new game → airport0 → airport boss → Shine → Delfino (stage 1) → Delfino boss → shadow Mario → gate → Bianco (stage 2) → Shine → save; two consecutive runs `u01`/`u02` rc=0, 153 s/193 s wall time (not 30 Hz time: the retrace is stepped); all 13 runs of the 2026-10-12 seed matrix reached Delfino (stage 1/1); over all code states 22 of 41 logged runs passed the chase, 16 ended stuck at a shadow-Mario waiting node, 2 fell into the water. A script `TIMEOUT` exits with rc 3 (fixed 2026-10-12: `aurora_shutdown` runs from `atexit`; before, the static-destructor teardown fast-failed with `0xC0000409`). Without `SUNSHINE_ALLOW_CPU_ADAPTER=0` the game runs on the CPU adapter at about 21 ticks/s. A retail-input run at 30 Hz without dev switches does not exist (movies and loads alone exceed 330 s) |
| Bianco Hills Ep.1 | partial | 65 | reached in route 70 (`stage=2 scen=0`, Mario controllable, status `0x0000133F`); textured, HUD, Mario walks/jumps, camera follows; `bianco/0` ran stable for 100 s (normal and heap-check mode); an intermittent flat red/blue wedge appears in about 1 of 6 runs at frame 600, cause open |
| First Shine + save + reload | partial | 75 | airport Shine collected by walking to it (`shines=1`, Shine-get sequence runs), Bianco 100-coin Shine through the dev verb `shine 2` (`shines=2`), return to Delfino `stage=1 scen=5`, `.gci` written, `gci_check.py` PASS, switch-free reload `stage=1 scen=5 shines=2` (`u05`, route `61_reload.dev`, file block B); the Bianco Shine is not collected by real input: an offline floor model (`tools/bianco_path.py`, `map.col` + `scene.bin`) finds none of the three Bianco Shines reachable on foot from the start (nearest is the 100-coin Shine, 7 069 units away in XZ and 2 498 above the reachable height); nozzle/jump mechanics would be needed (model only, not verified in the game) |
| Boss fights (airport and Delfino GateKeeper) | partial | 60 | both bosses die from the water jet with the dev aim assist (`SUNSHINE_DEV_AIM_ASSIST=1`); the airport boss is fought on foot (`walkto` + short `hold L` cycles), the Delfino boss is reached on foot over three waypoints (no `tp`), `cheat infinite_health` still needed (the boss hurts Mario); without the assist no head hit |
| Delfino gate to Bianco | partial | 50 | route 70: shadow-Mario chase with the dev verb `chase` (reads the shadow Mario position, real stick/R input), talk, gate walk-in on foot, spray opens the JellyGate, select menu (two port bugs fixed: `OSJoinThread` result, `TSelectMenu` message index and `break`), `stage=2 scen=0` controllable; no `tp`/`tpem` in the route anymore. The chase fails in the current tree: 9 of 9 decided runs on 2026-10-12 end `TIMEOUT chase` with the shadow Mario waiting (state 0xC) at the node (3474,645,-641) and Mario stuck at (4088,303,-4450); `d414ba0` gave 2 pass, 3 fell, 0 stuck in 5 decided runs. The shadow Mario's node pick is deterministic (`mRandomFlag` 0 in `setting.prm`) and very sensitive to Mario's position. Cause (single-file bisect on a clean d414ba0 worktree, 2026-10-12): the round-7 fix in `sms/src/Map/MapCheck.cpp` (`checkFlag(0x4)` -> `checkFlag(0x8)` in the wall-triangle test inside `TMapCollisionData::checkWallList`, verified against retail asm `MapCheck.s` 0x801DE8C0-0x801DE8C8, `rlwinm. 28,28`) makes wall collision retail-faithful. Mario is now really blocked by a wall at (4088,-4450) and the dev verb `chase` steers in a straight line; the old wrong flag let him pass some walls. The fix belongs in the chase steering, not in the collision. A navigation-aware `chase` is being worked on |
| Goop / pixel metric | open | 15 | GPU occlusion gives 0/1 only; needs real fragment counts |
| Audio | partial | 50 | movies and title music play; gameplay output turns into noise after ~20 s |
| Particles | partial | 50 | JPA loads; some particle view objects not in the draw lists |
| Unlocked framerate | partial | 30 | logic 30/s, replay/interpolation code present; 60/120/144 not verified (host load, "presents 0/s" counter) |
| Widescreen | partial | 55 | Hor+ frame widths 1706x960 (16:9) and 2242x960 (21:9) observed, HUD at the edges in the 21:9 frame; 4:3-identical, world scale and camera-cut reset NOT verified |
| Launcher, settings | partial | 65 | FSR1/scale/cheat rows persist to the INI, launcher screenshot rendered; DLSS row wrap fix not visually verified |
| Cheats, mods | partial | 60 | cheats verified by log (lives/coins held at 99); water/health cheat and mod overlay effect not verified |
| Controller glyphs | partial | 65 | Xbox/PlayStation/Switch/GameCube styles load and change the water-button glyph (PNGs); PlayStation X asset is a square; keyboard style missing |
| FSR1 upscaling | partial | 65 | FSR1 pass observed in log at 429x322 → 1279x960 (`build/PortLauncherRun/fsr67_on.log`); quality vs native not compared |
| DLSS | blocked | 0 | needs motion vectors and a D3D12 handoff Dawn does not expose (`docs/dlss-feasibility.md`) |
| Release package | partial | 40 | packaging script exists, not run on the current tree |

Overall (mean of the rows): **61 %**.

Plan and order of work: [`ROADMAP.md`](ROADMAP.md).

## Legal

- No game data is in this repository. You need your own legally obtained copy of the GMSJ01 disc image. The game reads it at runtime (`--dvd <path>`).
- The port's own licence is not decided yet. Third-party licences are listed in `sunshine-pc/THIRD_PARTY_LICENSES.md` (to be pinned before release).

## Layout

- `sms/`: decompilation (nested repository, submodule after publishing). Changes to it stay behind `#if TARGET_PC`; the matching build must keep the reference SHA-1.
- `sunshine-pc/`: the port (CMake, `src/`, `include/`, tools, docs).
- `sunshine-pc/extern/aurora`: Aurora at upstream `50e5be3`, with the port's changes as patches in `sunshine-pc/patches/aurora/`.

## Build

Requirements:
- Windows 10/11 x86-64.
- Visual Studio 2022 (BuildTools or Community) with the MSVC x64 tools. `build.cmd` loads `vcvars64.bat` from there.
- clang-cl. Put it on `PATH`, or set `LLVM_BIN` to the folder that contains `clang-cl.exe`.
- CMake, Ninja, and Python 3 (`Python3` is required by `CMakeLists.txt`).

Steps:

    git clone --recurse-submodules https://github.com/Astrr00/SMSunshinePC.git
    cd SMSunshinePC
    python sunshine-pc/tools/apply-aurora-patches.py
    sunshine-pc\build.cmd

`apply-aurora-patches.py` applies the port's changes to the Aurora submodule. It is idempotent. The configure step warns if the series is not applied.

`build.cmd [target]` configures and builds the preset `windows-clang-relwithdebinfo`. The binary is written to `sunshine-pc/build/windows-clang-relwithdebinfo/` (from `binaryDir` in `sunshine-pc/CMakePresets.json`).

Run with your own disc image:

    sunshine-pc\build\windows-clang-relwithdebinfo\sunshine.exe --dvd "path\to\Super Mario Sunshine (Japan).rvz"

## Matching decompilation

`sms/` builds the original `mario.dol` with its own Python/Ninja system. Its SHA-1 must stay `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` (GMSJ01). Run `ninja` inside `sms/` for that check; the port's build does not touch it.

### Update 2026-10-12 (measured): first full chain logos -> Bianco -> Bianco Shine -> save -> reload

Route `sunshine-pc/tools/routes/60_acceptance.dev`, run twice in a row (`h01`, `h02`): rc=0, 144 s and 147 s wall time, no `overrun`, no crash, 13 PNGs each (airport0 `ap0.png`, Delfino `dolpic.png`, gate `gate.png`, Bianco `bianco.png`, Shine get `shine_get.png`/`shine_get2.png`). Stages seen in `run.log`: 15 (option map) -> 0/0 -> 0/1 -> 1/0 -> 1/1 -> 2/0 (Bianco, `dir.state=4`, Mario steerable, status `0x0C400201`) -> 1/5. `shines=2`, `.gci` PASS (`tools/gci_check.py`), reload with `tools/routes/61_reload.dev` (file block B, real time): `stage=1 scen=5 shines=2`, rc=0 (`h05`).
Command: `python tools/route.py Strict tools/routes/60_acceptance.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`.
**What is NOT retail input (the chain is dev-assisted, so the acceptance wording "no dev switches" is not met):** `SUNSHINE_DEV_STEP_RETRACE` (logic faster than 30 Hz; the 144 s is not 30 Hz wall time), `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST` (faces the head, sets nozzle pitch, forces the crouch; the Delfino spray gate), script verbs `cheat infinite_health/infinite_water`, `tp` (pit/boss/gate positions), `tpem` (shadow-Mario chase), `shine 2` (touches the Bianco 100-coin Shine). Real: airport boss killed with water via assist + short `hold L` cycles, airport Shine taken by walking to it and pressing A, Delfino GateKeeper killed with water (flag `0x10384` in the log), gate walk-in, Bianco load.
Bug found and fixed on the way: `JAIData::initSeqParameter` left `mSeqHandle` 0 on PC, so `JAISeqEntry::storeBuffer` stopped the SE master sequence and `processFrameWork` crashed at the airport exit (session log section 38).

## More

- [`sunshine-pc/docs/status.md`](sunshine-pc/docs/status.md): status and blockers
- [`sunshine-pc/docs/platform-notes.md`](sunshine-pc/docs/platform-notes.md): technical notes for the port
- [`sunshine-pc/patches/aurora/README.md`](sunshine-pc/patches/aurora/README.md): Aurora patch series
- [`sunshine-pc/docs/publishing.md`](sunshine-pc/docs/publishing.md): publishing runbook
- [`Port.md`](Port.md), [`Goal-Port.md`](Goal-Port.md): work orders

### Update 2026-10-12 (later): route 70, fewer dev crutches

`sunshine-pc/tools/routes/70_boss_until.dev` run twice in a row (`u01` 153 s, `u02` 193 s wall, rc=0, no `overrun`, no crash, 13 PNGs, `gci_check.py` PASS, reload `stage=1 scen=5 shines=2`). Remaining dev: `cheat infinite_health`, `chase` (position read, real input), `shine 2`, env `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST`. Gone: `tp`, `tpem`, `killboss`. Not met: a real Bianco Shine without `shine 2` (no measured floor path to any of the three Shines), 330 s at real 30 Hz, no dev switches. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 42.

### Update 2026-10-12 (decomp round 6): 11 282 / 12 881 functions byte-identical (87.6 %)

Measured after round 6: SHA-1 unchanged, 11 282 / 12 881 byte-identical (87.6 %), matched code 61.98 %, fuzzy 98.60 %, 445 / 736 units, strict link 0 unresolved, Aurora 11 patches, `TEMP-DEBUG` 0, route 70 smoke 1 of 2 complete (`p01` chase fall, the known flake). Details: `sunshine-pc/docs/session-2026-10-11b.md` section 52.

### Update 2026-10-12 (decomp round 5): 11 247 / 12 881 functions byte-identical (87.3 %)

Measured after round 5: SHA-1 unchanged, 11 247 / 12 881 byte-identical (87.3 %), matched code 61.48 %, fuzzy 98.27 %, 445 / 736 units, strict link 0 unresolved, Aurora 11 patches, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 50.

### Update 2026-10-12 (decomp round 4): 11 191 / 12 881 functions byte-identical (86.9 %)

Measured after round 4: SHA-1 unchanged, 11 191 / 12 881 byte-identical (86.9 %), matched code 60.69 %, fuzzy 98.25 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 49.

### Update 2026-10-12 (decomp round 3): 11 112 / 12 881 functions byte-identical (86.3 %)

Measured after round 3: SHA-1 unchanged, 11 112 / 12 881 functions byte-identical (86.3 %), matched code 59.67 %, fuzzy 98.23 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 47.

### Update 2026-10-12 (decomp pass): 11 020 / 12 881 functions byte-identical

After a 12-agent decomp pass (details in `sunshine-pc/docs/session-2026-10-11b.md` section 44), measured by the lead: DOL SHA-1 unchanged, matched functions 11 013 -> 11 020, units 445 / 736, fuzzy total 98.12 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Several real behaviour bugs fixed (Koopa nerves, BathtubKiller, bossgesso, ModelWaterManager, fenceMove, Kumokun, GCConsole2).

### Update 2026-10-12 (decomp round 2)

Gates: SHA-1 unchanged, 11 020 / 12 881 functions byte-identical, 445 / 736 units, fuzzy total 98.23 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Route 70 smoke after the edits: 3 of 6 runs complete; one chase fall, one rare crash in the shadow-Mario replay (`TEnemyMario::emReplayJumpToNearestNode`, run `v02`, cause open). Details: session log section 45.
