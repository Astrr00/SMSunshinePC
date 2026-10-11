# Roadmap

Goal: a native Windows x86-64 port of Super Mario Sunshine (GMSJ01) built from the matching decomp (`sms/`), with Aurora replacing the GameCube SDK (`sunshine-pc/`). Launcher, mods, settings, unlocked framerate, widescreen, playable end to end. No game data in this repository.

Status legend: DONE = observed artifact, PART = partly working or unverified, OPEN = not working.
Last updated from observed state (see `sunshine-pc/docs/status.md` section 0 for evidence).

## Working rules (apply to every phase)

1. **Gates before any push**: matching DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` unchanged; strict link with 0 unresolved symbols; `TEMP-DEBUG` grep empty; fresh-clone build passes.
2. **Short tasks**: every agent task is one concrete deliverable time-boxed to ~30 min (edit→build→run→evidence at least every 15 min, one line per cycle in `sunshine-pc/build/progress.log`); it then reports and the next step becomes a new task.
3. **Concurrency**: at most 3 agents that build the port or run the game. Decomp-only agents (single-unit mwcc + objdiff, ~1 s per compile) may run up to ~13 at once while CPU load stays low (16 threads; check load before adding), ~45 min per task, one progress.log line per finished function. Each agent owns whole units; shared headers through one owner. Multi-file Aurora work happens in a private copy (`-DSUNSHINE_AURORA_DIR=<copy>`) and lands in one step. Timing measurements (fps, logic rate, latency) are taken only while no other agent runs.
4. **Evidence**: a feature counts as done only with a log excerpt or screenshot. Dev hooks are named as such.
5. **Decomp changes**: `#ifdef TARGET_PC` for port behaviour; asm-proven decomp bugs are fixed unguarded only in NonMatching units, with an `// asm 0x<addr>` comment.
6. **Saves**: tests use a temp save directory, never the user's `%APPDATA%\Sunshine\saves`.

## Phase 0: Decomp correctness (decomp phase paused at user request; resume with the worker protocol v3)

Runtime bugs keep coming from NonMatching functions (wrong masks, swapped branches, empty bodies with `TARGET_PC` stubs). `sunshine-pc/tools/decomp_triage.py` ranks every non-100 % function by behaviour-changing diffs (op/insert/delete/immediate/reloc; register allocation ignored).

| Item | State | Next |
| --- | --- | --- |
| Game-code functions open (not 100 % and no whole-function review) | PART (about 750 open in 124 units; ledger 1196 logic-verified + 66 matched, 614 partial; all code 11012/12881 bytegleich) | reviewers per unit list; most remaining functions are blocked by header-inline codegen (TVec3/TUtil/MsClamp/getMActor/TExPane setters): behaviour-equal own code is accepted |
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
| Retail flow: logo, movies, title, file select, new game, Delfino, Bianco | PART (file select + slot pick work; `airport0` reached once by pad script, not reproducible; no airport0 exit observed) | event-driven pad script (wait for stage/director/flag conditions; THP movies advance on their own clock), then walk to `MapObjStartDemo` (-6630,500,1600) -> Delfino -> GateKeeper (3 nozzle hits) -> Bianco |
| Shine collected by the real path, Shine-get sequence, save to `.gci`, reload | OPEN | needs the Bianco entry; dev-start Shine path not yet collecting |
| Particles (EmitterViewObj null in perform lists), Mario shape packet callback | OPEN | ParticlesNull |
| Missing decomp pieces: `ConsoleStr::processGo/startCloseWipe`, `ShapePacketCallBackFunc` | OPEN | from asm, as the flow needs them |

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
