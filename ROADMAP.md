# Roadmap

Goal: a native Windows x86-64 port of Super Mario Sunshine (GMSJ01) built from the matching decomp (`sms/`), with Aurora replacing the GameCube SDK (`sunshine-pc/`). Launcher, mods, settings, unlocked framerate, widescreen, playable end to end. No game data in this repository.

Status legend: DONE = observed artifact, PART = partly working or unverified, OPEN = not working.
Last updated from observed state (see `sunshine-pc/docs/status.md` section 0 for evidence).

## Working rules (apply to every phase)

1. **Gates before any push**: matching DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` unchanged; strict link with 0 unresolved symbols; `TEMP-DEBUG` grep empty; fresh-clone build passes.
2. **Short tasks**: every agent task is one concrete deliverable time-boxed to ~30 min (edit→build→run→evidence at least every 15 min, one line per cycle in `sunshine-pc/build/progress.log`); it then reports and the next step becomes a new task.
3. **Concurrency**: at most 3 agents that build the port or run the game. Decomp-only agents (single-unit mwcc + objdiff, ~1 s per compile) may run up to ~13 at once on the old 16-thread PC; the current PC has 6 logical processors (`os.cpu_count()` 6, measured 2026-10-12, load already 100 % with two build/run agents), so run at most ~4 at once there and only while the load stays below about 60 % (lead's policy); check load before adding, ~45 min per task, one progress.log line per finished function. Each agent owns whole units; shared headers through one owner. Multi-file Aurora work happens in a private copy (`-DSUNSHINE_AURORA_DIR=<copy>`) and lands in one step. Timing measurements (fps, logic rate, latency) are taken only while no other agent runs.
4. **Evidence**: a feature counts as done only with a log excerpt or screenshot. Dev hooks are named as such.
5. **Decomp changes**: `#ifdef TARGET_PC` for port behaviour; asm-proven decomp bugs are fixed unguarded only in NonMatching units, with an `// asm 0x<addr>` comment.
6. **Saves**: tests use a temp save directory, never the user's `%APPDATA%\Sunshine\saves`.

## Phase 0: Decomp correctness (decomp phase paused at user request; resume with the worker protocol v3)

Runtime bugs keep coming from NonMatching functions (wrong masks, swapped branches, empty bodies with `TARGET_PC` stubs). `sunshine-pc/tools/decomp_triage.py` ranks every non-100 % function by behaviour-changing diffs (op/insert/delete/immediate/reloc; register allocation ignored).

| Item | State | Next |
| --- | --- | --- |
| Game-code functions open (not 100 % and no whole-function review) | PART (several hundred open in about 120 units; ledger 1366 logic-verified + 64 matched, 541 partial as of 2026-10-11, the +170 from agents not yet audited; all code 11013/12881 bytegleich) | reviewers per unit list; most remaining functions are blocked by header-inline codegen (TVec3/TUtil/MsClamp/getMActor/TExPane setters): behaviour-equal own code is accepted |
| Empty decomp bodies with `TARGET_PC` stubs | PART (40 found and resolved: removed when no retail symbol, otherwise reconstructed) | re-scan each wave |
| Fully matching units flipped to `Matching` in `configure.py` | PART (439 → 445 units) | lead, per-unit SHA-1 check each wave |
| Ledger quality | PART (audit 1: 10 % wrong; audit 2: 0/25 wrong; audit 3: 30/30 sampled rows confirmed, 4 wrong source hunks found and fixed; one auditor finding (KillerManager layout) was a false positive) | `--check` + `branches mapped` gate in `decomp_queue.py ledger`; periodic sampling audits |

Exit criterion: every function the port builds is an exact match (preferred) or, where codegen cannot be matched, uses code of our own (helpers, restructured control flow, header edits are allowed since the user's later rule) whose behaviour equals retail, proven by a whole-function asm-equivalence review recorded as `logic-verified` in `sunshine-pc/docs/decomp-verified.tsv` (the triage score only ranks the work); no `TARGET_PC` stub over an empty body.
Scope limit (`sms/AGENTS.md`): game code is the target; shared and JSystem headers may now be edited when it helps (rebuild every includer, Matching units stay 100 %, SHA-1 unchanged). MSL, MetroTRK, THPPlayer and the dolphin SDK stay untouched; port fixes there stay under `TARGET_PC`.

## Phase 1: Boot to gameplay (core)

| Item | State | Next |
| --- | --- | --- |
| Strict link, no stubs, matching SHA-1 | DONE | keep as gate |
| Bianco Hills renders (geometry, textures, HUD), Mario controllable, camera follows | DONE (dev start) | none |
| Title screen renders correctly | OPEN | TitleRender: copy-with-clear ordering, TEV/palette checks via pass dumps |
| Retail flow: logo, movies, title, file select, new game, Delfino, Bianco | PART (condition-based route reaches `airport0` reproducibly without start switches: `tools/routes/07_airport.dev`; route 72 (route 70 with the `chase tp=1` crutch; route 70's walking chase fails) with dev aids (`SUNSHINE_ALLOW_CPU_ADAPTER=0`) reaches stage 1/0 at logic t=284 s and stage 2/0 at t=550 s (t72g) / t=726 s (t72h); stage names not checked here; walk to `MapObjStartDemo` status unclear against this timeline) | waypoints along the airport0 path to `MapObjStartDemo` (-6630,500,1600) -> Delfino -> GateKeeper (3 nozzle hits) -> Bianco; confirm the FIFO fix (`GXBegin` count in JPA cross particles) over a full 330 s run |
| Shine collected by the real path, Shine-get sequence, save to `.gci`, reload | PART (dev verb `shine 2` on the Bianco 100-coin shine: `shines=1`, `.gci` PASS, switch-free reload `shines=1`; not from a switch-free flow) | boss kills need the dev aim assist; route without dev switches from the logos not built (sessions 2026-10-11b sections 22-35); Bianco floor model (`tools/bianco_path.py`, not verified in game) finds no Shine reachable on foot, jumps and nozzle not modelled |
| Particles (EmitterViewObj null in perform lists), Mario shape packet callback | OPEN | ParticlesNull |
| Missing decomp pieces: `ConsoleStr::processGo/startCloseWipe`, `ShapePacketCallBackFunc` | DONE (all three exist; measured 2026-10-12 with `decomp_triage.py --build`: `TConsoleStr::processGo` 96.76 %, `startCloseWipe` 97.94 %, `ShapePacketCallBackFunc` 91.34 %; ledger rows `logic-verified` by agent review, not audited by the lead; under `TARGET_PC` the callback uses Aurora GX setters instead of FIFO writes) | none |

Exit criterion: normal start (no dev switch) reaches Bianco Ep.1 controllable, a real Shine is collected, the save file round-trips.

## Phase 2: Correctness of gameplay systems

| Item | State | Next |
| --- | --- | --- |
| Asm-vs-source audit of NonMatching units | PART (System, parts of Player reviewed; 17 logic bugs fixed) | automated operand-diff over objdiff output (immediates, offsets, relocs), rank units, then review only hits |
| Goop/graffiti: real pixel counts (Aurora metric is 0/1 only) | OPEN | GoopCount: fragment counter in a private Aurora copy, exact-count self-test |
| Audio: gameplay noise from ~20 s, title BGM ends ~20 s, THP channel order | OPEN | find the noisy voice (per-voice log), decide legitimate title end |
| Pixel peek/readback (GXPeekARGB) | PART | verify with the goop self-test |

Exit criterion: Bianco and Delfino playable for 10 minutes without crash, goop cleans, BGM and SFX audible.

## Phase 3: Presentation features (Port.md phases 4-8)

| Item | State | Next |
| --- | --- | --- |
| Launcher: ROM select, settings pages | PART (pages rendered; FSR1/scale/cheat rows persist to the INI; DLSS row wraps onto the FPS button, fix not visually verified) | verify Browse/Start/keybinds; launcher cheats tab is a placeholder; no aspect/audio/language selector |
| Settings: resolution, VSync, fullscreen, FPS limit | PART | runtime apply check |
| Widescreen (Hor+, UI anchoring, 4:3 bit-exact) | PART (16:9 1706x960 and 21:9 2242x960 observed, HUD at edges in 21:9; 4:3 identity, world scale, direct HUD draws NOT verified) | deterministic capture (same-config runs differ: RNG seed from boot tick, nondeterministic startup), then pixel compare |
| Unlocked framerate (replay), interpolation | PART (code present; only a cap-60 A/B on a loaded host: logic 8-18/s, so cap not delivered; 120/144 and camera-cut reset not run) | measure on an idle machine; check the 'presents 0/s' counter |
| FSR1 upscale | PART (FSR1 pass observed in the log at 429x322 -> 1279x960; PNG clean) | compare against native at the same frame (framing is nondeterministic) |
| DLSS | DECIDED: infeasible without motion vectors and a GPU handoff (`docs/dlss-feasibility.md`) | launcher shows it as unavailable |
| Controller glyph sets (Xbox, PlayStation, Switch, GameCube) | PART (all four styles load; Xbox/PlayStation frames differ at the water-button slot; PlayStation X asset is a square; Y/Z/L/R/stick/dpad prompts absent from the game archive) | fix asset; keyboard style missing |
| Cheats and mods (DLL mods, data overlay) | PART (cheats verified by log: lives/coins held at 99; water/health cheat and mod overlay effect NOT verified) | in-game effects, defaults all OFF assertion |
| UI at Dusklight level (target set 2026-10-12): launcher and in-game menu with controller navigation, settings pages, glyph-aware rebinding, cheats/mods pages, disc picker with verification, update check, confirmations, localization | OPEN (gap study done: `sunshine-pc/docs/ui-gap-dusklight.md`, plan UI-1 to UI-16 in `sunshine-pc/docs/ui-plan.md`; same stack as Dusklight: RmlUi on Aurora; today the launcher has no gamepad navigation, no in-game menu, a cheats placeholder, hard-coded colours without focus state) | UI-1 theme tokens and fonts, UI-2 scaling, UI-3 launcher controller navigation, then UI-4 in-game menu shell; owner decisions listed in the plan (font, hash table, HTTP client, widescreen, dev gate, languages) |

Exit criterion: every toggle in the launcher has an observed effect in game.

## Phase 4: Release and publishing

| Item | State | Next |
| --- | --- | --- |
| Published on GitHub (`Astrr00/SMSunshinePC`, single repo, sms flattened, Aurora patch series) | DONE | keep gates green |
| Release package (exe, assets, licenses, no game data) | PART | install/CPack run, unpacked smoke test |
| `probe-units`: clang-cl 624/733 (584/584 built), MSVC `cl` 565/733 | DONE (documented) | optional: fix the 66 built-unit `cl` failures |
| Docs: status, platform notes, verification, mods | PART | update after each phase |

## Order of work

1. Restore a compiling shared tree (Aurora private-copy policy), then Phase 1 items in parallel with at most 3 agents: TitleRender, ShineSave, ParticlesNull. GoopCount works in its private copy.
2. After Phase 1 exit: Phase 2 audit automation and audio.
3. After Phase 2 exit: Phase 3 verification passes, one agent per feature, measurements serialized on an idle machine.
4. Phase 4 release package and a final fresh-clone check; update `Goal-Port.md` acceptance with evidence.

## Out of scope for now

DLSS, Wii/other regions (GMSP01 assets are not wired), netplay, mod store.

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

Measured: SHA-1 unchanged, 11 317 / 12 881 byte-identical (87.858 %), matched code 62.56 %, fuzzy 98.64 %, matched data 87.47 %, 445 / 736 units, `ninja -k 0 -j4` rc 0, strict link 0 unresolved, Aurora `applied: 11 patches`, `TEMP-DEBUG` 0. Commits 513e37a (round 7) and d66592c (docs) are on origin/main. Against the clean d414ba0 baseline: 225 changes, 200 improved, 3 worse (`TKumokun::checkOnMovingWall`, `TBathWaterManager::perform`, one not named).

Route 70 needs `SUNSHINE_ALLOW_CPU_ADAPTER=0`: about 70 logic ticks/s instead of about 21 on the CPU adapter. `[status]` `t` is logic time. A devscript TIMEOUT exits with rc 3 since the `atexit(aurora_shutdown)` fix (before: 0xC0000409 from the static pipeline thread at teardown).

Chase (Delfino shadow Mario): superseded by the update `2026-10-12 (route 72)` below. Short version: 9 of 9 walking-`chase` runs on the round-7 tree end in `TIMEOUT chase`; cause `MapCheck.cpp` `checkFlag(0x8)` (asm-backed, stays); route 72 uses the crutch `chase tp=1` and passes.

Bianco Shines, floor model (`tools/bianco_path.py`, not verified in game): no Shine reachable on foot in any variant tested. Jumps, nozzle and enemies are not modelled.

Open: null-rail guard hypothesis (`enemyMario.cpp`), a walking chase, several hunk audits still unverified.

Details: session log section 53.

### Update 2026-10-12 (route 72): chase crutch `tp=1`, working Delfino-to-Bianco chain again

Observed: route `sunshine-pc/tools/routes/72_boss_tpchase.dev` (route 70 with `chase timeout=4000 tp=1`), Strict tree built from the committed source, env `SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`. Two consecutive runs: `t72g` (150 s wall) and `t72h` (191 s wall), rc=0, `overrun` 0, `fatal exception` 0, 13 PNGs each. Stages: 0/0, 15/0, 0/0, 0/1, 1/0 (logic t=284 s), 1/1, 2/255, 2/0 (Bianco, t=550 s / 726 s), 1/5; `shines=2`. `chase tp=1` met after 421 ticks in both runs (8 teleport cycles, water counter 64 -> 0, shadow `doing=0xF`, talk opens). `tools/gci_check.py` on the `t72h` card: PASS. Reload of that card with `tools/routes/61_reload.dev` (copied appdata, run `t72h_rl`, `SUNSHINE_ALLOW_CPU_ADAPTER=0`): rc=0, `stage=1 scen=5 shines=2`.

What the crutch does (dev only, not retail input): every 60 ticks Mario is put 250 units behind the shadow Mario along its travel direction (100 units once the shadow is down) and the R trigger is held for 45 of the 60 ticks. Why (observed): the walking `chase` of route 70 fails 9 of 9 with the retail-faithful wall test; a floor-grid planner (BFS to a goal radius around the shadow, `tools/nav/col_grid.py` grids) passed 0 of 6 runs (the shadow left its node when Mario came within 1000 units; afterwards Mario's distance to it grew again, per the agent's log reading); route 63's `tpem` (Mario 250 units south of the shadow) landed 39 of 64 hits in 9 bursts and failed `waitfor talk=1`. What ends the chase is 64 water hits on the shadow (`mWaterCtMax` 64, `TEnemyMario::hitWater`, `enemyMario.cpp` about lines 1484-1530), then `doing=0xF` and the talk window; reaching a node does not end it.

Data facts (from reading the data and the source, by an agent; I did not repeat the decoding): the Pad-0 shadow uses the rail graph `mariomodoki` (`scene.ral`, 9 nodes, only node 2 has a flag, 0x1, no node has 0x40) and the replay links in `/map/map/pad/linkdata.bin` (7 nodes A-G: ring B-C-D-E-F-G-B plus a stem A-B). The link table has 7 entries and the rail graph 9 nodes: `mReplayLinks[nodeIndex]` would read past its allocation if the shadow's nearest node were 7 or 8 (not observed; the retail data has the same sizes). Pick rule with `mRandomFlag` 0 (read from `setting.prm` by an earlier agent): the shadow takes the link whose direction has the smallest dot product with the direction to Mario, i.e. it flees.

Not met: a walking chase (open), `cheat infinite_health`, `shine 2`, the three env switches. My earlier statement that a node with flag 0x40 is the chase goal was wrong for this graph.
