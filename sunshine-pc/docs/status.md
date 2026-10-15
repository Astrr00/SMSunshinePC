# Port status audit

Scope: Port.md phases 1-8 and §5-§6 features, plus the Goal-Port §6 acceptance criteria. Read-only audit by PortAudit. No game runs, no builds, no edits outside this file.

Legend: **Done** = verified by observed artifact. **Partial** = some code or verification exists, gaps remain. **Not started** / **Open** = no working implementation or known blocker. `[INFERENCE]` = not observed directly.

## 0. Lead update (later than this audit; supersedes the rows it contradicts)

### Update 2026-10-15: published lifetime fix, bath-water sound and EventWatcher

The verified lifetime fix and previous initializer review are published in `3c10554`; issue #12 is closed.
A fresh local clone of that commit builds with unresolved symbols disabled, using cached dependency source packages
and separately checked-out Aurora `50e5be3` with all 16 tracked patches; project objects were rebuilt.

The next two delegated tasks found/corrected bath-water sound being gated to every fourth move tick and confirmed
both EventWatcher warp and Appear8 whole-function reviews.
The rebuilt matching object contains exactly the intended sound-branch target change.
Finite spawn-FP/signed-zero/RNG gaps are analytically closed and CRT f32 edge assertions pass; unconditional
NaN-payload/FP-state equivalence remains partial.
Appear8 was already byte-identical; its stale partial ledger row is now matched, not a new matching gain.
Ledger prefixes: 1 976 logic-verified, 150 matched, 252 partial; three legacy malformed rows excluded.
Full matching totals remain 11 323/12 881 byte-identical, reference SHA-1 unchanged.
`verify --build --matching`: 7 PASS, 0 FAIL, 1 SKIP (optional probe-units); strict zero unresolved, 62 headless checks,
45-second WARP boot without fatal exception or WebGPU error.

An isolated pre-fix Corona dev-start smoke reaches stage 60/0 at tick 109, then aborts on unknown texture format 22,
before any sound-cadence observation. Temporary exact-archive-name and logging hooks were removed and are not shipped.
This runtime blocker is tracked in [issue #13](https://github.com/Astrr00/SMSunshinePC/issues/13).
No audible Corona or hardware-GPU result is claimed; detailed branch/FP evidence is in `decomp-rounds.md`.

### Update 2026-10-15: takeover, scene lifetime and initializer review

Last source work was `bf1ea15` (bath-water first-pair collision correction), followed by the `88d7c5d` status refresh.
Two independent tasks were delegated: TransitionLifetime fixed the diagnostic lifecycle error; BathInitializer
completed the renewed whole-function initializer review without changing its source.
`initializeIfYet_` is now `logic-verified`; `perform` spawn-position FP equivalence and Corona runtime comparison remain open.
Ledger status prefixes now count 1 976 `logic-verified`, 149 `matched` and 253 `partial`, excluding three legacy malformed rows.

The lifecycle fix in `src/devscript/devscript.cpp` gates scene-object readers on the active gameplay director and its
existing `unk260` post-setup marker; this covers status logging, scene-object route conditions and aim assist.
It preserves transition-safe tick/stage/movie status and resumes particle statistics in each live scene.
The first status-enabled route-85 run, `takeover_lifetime85_20261015`, survives the former airport crash and reaches
Delfino 1/1 with one Shine; it ends at the 330-second wall limit (rc 124), not at route completion.
The extended run, `takeover_lifetime85_long_20261015`, completes with status enabled: rc 0 after 364 s wall
(650 s limit), tick 18210, Bianco 2/0 and one Shine; independent GCI checking passes all seven block checksums.
Route 86 (`takeover_lifetime86_20261015`) reloads that card: rc 0 after 17 s, tick 1675, Delfino 1/0, Shines 0 -> 1.
Assert-based checks confirm live JPA statistics in airport, Delfino and Bianco and no JPA traversal on the 61 sampled
unavailable-scene ticks of the completed run.
Both routes use Null, step-retrace, skipped movies and aim assist; they do not prove switch-free or hardware rendering.

Lead checks: strict link with zero unresolved symbols, 62 headless checks with zero failures,
matching DOL SHA-1 unchanged and `ninja changes_all` empty; rebuilt-binary WARP boot stays alive for 45 seconds
without a fatal exception or WebGPU error.
The initial verification invocation could not find `clang-cl`; setting `LLVM_BIN` to the already installed compiler
made the strict and headless checks pass without installing anything.
See `decomp-rounds.md` for the initializer branch/FP evidence and diagnostic ownership chain.

### Update 2026-10-15: matching refresh and bath-water reserve-drop correction

Source `bf1ea15`; full `sms/` build and `changes_all`: 11 323/12 881 byte-identical functions (87.90467 %),
62.6544 % matched code, 98.66045 % fuzzy, 87.469604 % data, 445/736 complete units.
The DOL SHA-1 remains `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.
Six more functions match than in the 2026-10-12 full report.

The targeted audit confirms `TTailRubber::adjust` (its two ledger spellings are consolidated).
Bath-water `calcWaterModel` now enters its first collision pair before checking the inner bound, matching both retail
call sites; all 500 slots are initialized, while stock wave parameters activate 160 drops with stride 1.
This restores an interaction with reserve slot 160.
`perform` improves 93.91747 -> 94.09952 %; `initializeIfYet_` drops 94.67607 -> 94.61472 % for the same required logic fix.
No byte-identical function was lost.
Their whole-function statuses remain partial: spawn-position FP boundaries and initializer re-review are open.

Observed validation: strict Windows link with zero unresolved symbols, 62 headless checks with zero failures,
45-second WARP title boot with no fatal exception or WebGPU error.
A fresh clone of `bf1ea15` also builds and links with unresolved symbols disabled, reusing cached dependency sources
and packages; Aurora was checked out separately and the tracked patch series applied.
No Corona fight comparison was performed.
Route 85 regression: `bath_audit85_nostatus_20261015` on Null, step-retrace, skipped movies and aim assist, automatic
status output and screenshot monitor off: rc 0 after 230 s wall, Bianco stage 2/0, one Shine, GCI checker PASS.
With automatic status output on, the first run crashed in `logJpa` at `devscript.cpp:539` during airport scene teardown;
this diagnostic failure remains open and is distinct from the passing route without periodic output.
The pre-fix bath-water control also faults at that transition (invalid read, rc 0xc0000005, 120 s wall; no control debug
symbols). Tracked in [issue #12](https://github.com/Astrr00/SMSunshinePC/issues/12).
Ledger counts by status prefix: 1 975 logic-verified, 149 matched, 254 partial; three malformed legacy rows excluded.
Details: [`decomp-rounds.md`](decomp-rounds.md).

The following undated lead notes and dated updates describe earlier observations; use README and ROADMAP for the
current combined feature state.

Observed by the lead with `AURORA_SCREENSHOT` and `SUNSHINE_DEV_STATUS` logs (dev start `SUNSHINE_DEV_START=bianco/0`):

- Strict link: 0 unresolved symbols. Matching DOL SHA-1 unchanged. TEMP-DEBUG grep: 0.
- Bianco Hills renders textured; HUD (Shines, blue coins, coins, lives) renders; Mario survives WARP_OUT, walks, jumps, camera follows; director reaches state 4.
- NOT verified / broken: title screen render and the retail flow title -> file select -> new game -> Delfino -> Bianco; first Shine (only a dev hook exists, it finds no TShine because the Bianco Ep.1 Shine is event-spawned after the boss) and saving; goop/graffiti (pixel metric is visibility-only 0/1, so `PollutionCount` magnitudes are wrong); title/gameplay BGM (title music ends after ~20 s, gameplay has broadband noise); THP audio channel order (decoder agrees with its own model but matches ffmpeg only with L/R swapped: likely swapped); replay/interpolation at >30 fps in gameplay (gate `mState == STATE_UNK4` only just reachable); widescreen visuals; FSR1; launcher click paths; cheat effects; mods in-game. DLSS: feasibility document only.
- Known decomp gaps: none empty any more; `PacketUtil::ShapePacketCallBackFunc` (75.7 %, partial review) and `TGCConsole2::perform` (69.5 %) are the largest open game functions on the gameplay path; open list in `decomp-verified.tsv` / `decomp_queue.py status`. `EmitterViewObj`/`EmitterIndirectViewObj` resolve to null in the perform lists (particles likely not drawn).
- `probe-units`: clang-cl 624/733 clean (all 584 built units); MSVC `cl /Zs` 565/733 (66 built units fail on MWCC-specific syntax; reasons in platform-notes).

### Lead update 2026-10-11 (supersedes the next section where they conflict; details in `session-2026-10-11.md`)

Measured: `ninja -k 0 -j4` exit 0, DOL SHA-1 `9f5a8caf...` OK, `changes_all` no regression, strict link 0 unresolved,
`apply-aurora-patches.py --check` "applied: 8 patches", 11 013 / 12 881 functions bytegleich (58.57 % code, 97.98 % fuzzy),
445 / 736 units Matching, ledger 1 366 `logic-verified` / 64 `matched` / 541 `partial`.

- A3 update: a condition-based route (`tools/route.py`, `tools/routes/07_airport.dev`, verbs `waitfor`/`mash`/`walkto` with
  `timeout`, conditions `stage scen dirstate movie flag cardmode cardprog`) reaches **airport0 (stage 0, scen 0)** from the
  logo with no start switches: stage 15 is the option map with the file blocks (x 840/1080/1320, z -1000); card prompts
  (`TCardLoad` progress 14 -> 17 -> 19) are dismissed with A; bumping block B (progress 27) starts the new game (41);
  the opening movie runs ~1900 ticks; airport0 is ready at tick ~5300-5490. This supersedes "not reproducible"
  (the earlier non-determinism came from tick-timed scripts).
- Not reached: `MapObjStartDemo` (-6630,500,1600). The straight-line `walkto` wanders and Mario falls from y 500 to y 300 near
  x -500..-960. Delfino, Bianco gate, Shine, `.gci` save: not attempted.
- Crash root cause (asm-proven): `JPADrawExecDirectionalCross::exec` and `JPADrawExecRotDirectionalCross::exec` called
  `GXBegin(GX_QUADS, ..., 4)` but wrote 8 vertices; retail passes 8 (asm 0x8007C2C4 / 0x8007C864). Aurora's FIFO parser then
  read vertex data as commands ("draw vertex data overrun", fatal). Fixed in `JPADrawVisitor.cpp`; the full-length
  confirmation run is still open. Aurora patch 0008 adds the diagnostics that found it.
- Decomp agents (Opus 5.5 medium directing six Haiku 5.5 xhigh workers): +170 `logic-verified`; asm-proven fixes in
  `MarioMove.cpp` (`setStatusToRunning`, gameplay-visible), `Koopa.cpp`, `hamukuri.cpp`, `GCConsole2.cpp`, `bossManta.cpp`,
  `bosspakkun.cpp`. Only these diffs were checked by the lead against retail asm; ledger rows are not audited.
- Observed but unexplained: option-map and title screenshots are pure white PNGs.

### Update 2026-10-11 (session b, measured; see `session-2026-10-11b.md` sections 10-35)

Gates: DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, 11 013 / 12 881 functions, 445 / 736 units, strict link 0 unresolved, Aurora series 11 patches. Fixed in the decomp/port: `TEMario::perform` control flow (shadow Mario froze), `TSelectDir` `OSJoinThread` result, `TSelectMenu` stage-name index and `break`, `TMarDirector::mConsole` init (PC), Aurora alpha-compare shader (patch 0011). Reached with dev assists only: Delfino gate chain to Bianco Ep.1 (controllable), both GateKeepers killed by the water jet (dev aim assist), Shine collect via dev verb, `.gci` PASS, switch-free reload `shines=1`. Not reached: a switch-free route logos to Bianco under 330 s (the whole flow needs about 20 000 logic ticks at 30 Hz), the bosses and the real Shine without dev assists, two clean full runs.

### Latest lead update (end of the agent session; supersedes everything else in this file where they conflict, including the A3/R02/R03 rows)

Measured by the lead at round 7 (commit 513e37a, base d414ba0): `ninja -k 0 -j4` in `sms/` rc 0, DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` unchanged, strict link 0 unresolved, `apply-aurora-patches.py --check` "applied: 11 patches", 11 317 / 12 881 functions byte-identical (87.858 %; matched code 62.56448 %, fuzzy 98.637474 %, matched data 87.469604 %), 445 / 736 units Matching. Ledger (fact sheet; rows, not unique functions): 1 977 `logic-verified`, 149 `matched`, 250 `partial`. Previous entry: 11 012 / 12 881 (58.56 % code), ledger 1 196 / 66 / 614 with no stated basis; not reconciled here.

- A3 correction (round 7): `airport0` -> Delfino -> Bianco is reached in route 70 (stage 0/1 at logic t=46 s, 1/0 at t=284 s, `u01`/`u02` 2/0 at t=590/715 s) and in the `60_acceptance` runs, both with dev aids (`SUNSHINE_DEV_STEP_RETRACE`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST`, `SUNSHINE_ALLOW_CPU_ADAPTER=0`). The earlier "reached once, not reproducible" note is superseded. The airport0 exit is inferred to be `MapObjStartDemo` at (-6630,500,1600) (`sms/src/MoveBG/MapObjTown.cpp:353-364`, trailing s32=2 -> `fireStreamingMovie(2)` -> `setNextStage(0x101)`; airport0's `map.col` has no 0x200/0x300 collision planes) [INFERENCE, not re-checked in round 7]. The old next step (condition-based `waitfor` verb; pad-clock WIP uncommitted) is not re-checked here. `TMarDirector::unk261` is never written in retail either (uninitialised there); its readers take no branch on 0.
- R02/R03: `bianco/0` and `dolpic/0` ran stable for 100 s each (normal and heap-check mode). Check-mode `0xc0000374` was a data race in the debug quarantine (fixed with a mutex in `src/sdk/jkr_new_delete.cpp`, committed in `92390cb`). The earlier normal-mode crashes were not reproduced; cause unproven.
- B3: an intermittent flat red/blue wedge in Bianco frames (about 1 of 6 runs at frame 600) also occurs with widescreen off and FSR off; cause open (the command stream differs run to run before the renderer; seeds do not pin it).
- Phase 4/5/6/7 items: see the README table. Verified by log/PNG: FSR1 pass, cheats lives/coins, glyph style change, Hor+ frame widths. Not verified: 4:3 identity, interpolation at 60/120/144, mod overlay effect, water cheat.
- Process note: `tools/reap.py --loop` kills `sunshine.exe` older than 6 min or older than `src/sdk/av.cpp`; rebuild after av.cpp edits. Use `tools/runlock.py` to serialise game runs.

## 1. Verified measurements (this audit)

| Check | Result | Source |
| --- | --- | --- |
| Matching `sms/` build SHA-1 | **Match.** `sms/build/GMSJ01/mario.dol` (4,114,624 bytes) = `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, equal to `sms/config/GMSJ01/build.sha1` | `hashlib` on file, `sms/config/GMSJ01/build.sha1` |
| Decomp match (`report.json`) | 10,960/12,881 functions (game code 6,945/8,806), 57.9% code matched, fuzzy 97.9%; 954 game functions with whole-function asm review; 973 game functions open | `sms/build/GMSJ01/report.json`, `decomp-verified.tsv` |
| Git-tracked game data | 0 data files (`.dol .szs .arc .rvz .iso .aaf .thp .bin .jpa .bti`) in `sms` (1,569 tracked files), `sunshine-pc` (86) or root (89). `sms/orig/GMSJ01/` extraction and RVZ exist on disk, untracked | `git ls-files` |
| Link state | Lead reports strict link with 0 unresolved. Not re-verified. `docs/unresolved.md` is an older 171-symbol snapshot | `docs/unresolved.md` (stale) |
| Empty-body functions in port layer | 6 `{ }` bodies. All intentional: `dummy_handler`/`Hx_Warning` match retail `blr`; `OSProtectRange` has no PC equivalent; `gx_extra.cpp` break-point no-ops; `OSFillFPUContext` zeroes the dump snapshot. No hidden stubs found | `sunshine-pc/src/sdk/*`, `launcher.cpp` (false positive: constructor) |
| `TEMP-DEBUG` markers left | **0** in `sms/src` and `sunshine-pc/src` now. `platform-notes.md` line 465 names one in `Application.cpp`; it appears removed | grep |
| Excluded/blocked lists | `excluded-units.txt`: 26 prefix rules (151 units per notes). `blocked-units.txt`: **empty** (only header). Notes say 3 blocked (Koopa, bossManta, hanasambo) | files |
| Stray root artifacts | `sunshine-pc/buildaurora-gxtestsbuild-probe.out` and `sunshine-pc/crash.log` exist. Likely scratch | glob |

### Conflicts I could not resolve (flagged, not picked)
- **Decomp %**: Port.md says ~41% code / 8,585 functions. platform-notes says 47.44% code / 9,160 functions. `report.json` now says 54.1% / 10,699. Use `report.json` as the source of truth.
- **Unit counts**: 579 compiled / 151 excluded / 3 blocked (notes) versus 733 total (Goal-Port). `report.json` lists 736 units, and my prefix match against `excluded-units.txt` could not reproduce the 151 (unit name format differs from `sms/src` paths). `probe-units` is needed to settle it.
- **Blocked**: notes list 3; file now lists 0. Likely already removed, `files.cmake` needs confirming.

## 2. Goal-Port §6 acceptance

| # | Criterion | State | Evidence / gap |
| --- | --- | --- | --- |
| A1 | `probe-units` 733/733 or justified exclusions | **Partial** | 151 exclusions with reasons (`excluded-units.txt`). Count not reproducible from `report.json` here. Re-run `probe-units` |
| A2 | No empty/stub source in port build; missing-function list empty or justified | **Open** | `unresolved.md`: 157 class/method + 10 game-global symbols from empty or unfinished decomp sources (Enemy bosses/NPCs, `SMS_EmitRipple*`, `FeetInvCalc`, `j3dDefault*`). Not justified per-symbol |
| A3 | `sunshine.exe` links with 0 unresolved and boots image -> title -> file select -> Delfino -> Bianco Ep1 with Shine and saving | **Partial** | (superseded in part: see 'Latest lead update' at the top) Link 0 unresolved (measured by lead at round 7, `tools\_strict.cmd` STRICT: 0 unresolved). Boot to title works but washed out. Stage 0 `airport0` (scen 0) reached by pad input with no skip switches in a PortFlow run (`build/PortFlow/watch.log` 1237-1270: `dir.state=4`, billboard dismissed by A, status 0x10001308 -> 0x0C400201). Delfino and Bianco reached only with dev aids: route 70 stage 1/0 at logic t=284 s, stage 2/0 at t=590 s (`u01`) and t=715 s (`u02`). Delfino (`dolpic0`) was blocked by R03 (crash at 12-19 s) in an earlier run; route 70 runs `u01`/`u02` (rc=0, no crash) and round-7 chase runs reached Delfino. `delfino/0` reached gameplay (fps 144 run). `bianco/0` crashes in `TNpcParts::TNpcParts`. Shine, saving, file select flow unverified |
| A4 | Matching `sms/` build passes SHA-1 | **Done** | Verified above |
| A5 | Phases 4-8 done or documented with blocker | **Partial** | Phases 4-8 not done. Blockers documented in `platform-notes.md` and `dlss-feasibility.md` |

## 3. Requirement table (Port.md phases 1-8, §5-§6), ordered by criticality for "fully playable from disc image"

Order: P0 blocks playing the game from the disc image. P1 is required to ship. P2 is polish and extra.

| ID | Req | Phase | State | Evidence | Gaps |
| --- | --- | --- | --- | --- | --- |
| R01 | Title screen renders correctly | 2 | **Open** | Sun emblem only in black box at top-left (~40%×57%). Platform-notes line 404 onward | Root cause not fixed. `GXCopyDisp` is an empty stub (`extern/aurora/lib/dolphin/gx/GXFrameBuffer.cpp:206`). Retail clears the EFB after display copy; Aurora ignores it. Hypothesis unproven |
| R02 | Stage loads: bianco, ricco, mamma, sirena | 3 | **Open** | Earlier crash `TNpcParts::TNpcParts` at `sms/src/NPC/NpcParts.cpp:56`. Not reproduced in 3 normal and 2 `SUNSHINE_CHECK_HEAP=1` `bianco/0` runs at 100 s (`build/StageCrash/bianco-*.watch.log`) | Root cause unproven. Zero-clear loop is in bounds. Only bianco run; ricco, mamma, sirena not run. Owner: StageBianco |
| R03 | Stage load: dolpic (Delfino) without memory corruption | 3 | **Open** | Check mode `0xc0000374`: fixed. Cause was a race in the debug quarantine (`src/sdk/jkr_new_delete.cpp`), now mutex-guarded. Normal mode `0xc0000409` at 12-19 s not reproduced in 4 normal runs at 100 s (`build/StageCrash/dolpic-*.watch.log`) | Normal-mode cause unproven. Quarantine is not active in normal mode, so the mutex does not explain it. [INFERENCE] may be the WGSL shader error (platform-notes ~515), not heap damage |
| R04 | Empty/unfinished decomp sources filled (Enemy bosses, NPCs, fences, rails, Rocket, Sand, Talk2D2, Guide, Select shine, etc.) | 1, 3, 4 | **Open** | 157 symbols + 10 globals (`unresolved.md`) | Full list is in `unresolved.md`. Boss Hanachan nerves, BW nerves (`TNerveBW*`), Manta, Sand, Select shine manager, Talk2D2 all still empty |
| R05 | `__fres` (bit-exact reciprocal estimate) | 2, 3 | **Open** | Undefined symbol, blocks `JPABaseField::calcFieldFadeScale` (68/284 particle resources) and `JPAVecToRotaMtx` (185/284) | Every particle emitter hits it. No emitter runs |
| R06 | Global `JKR_NEW`/`JKR_DELETE` allocator | 2 | **Partial** | `src/sdk/jkr_new_delete.cpp` exists. Global operator new/delete only as placement overloads under `TARGET_PC` | Goal-Port B.4 not done (`JKR_NEW` path). OOM aborts |
| R07 | Pointer truncation (`JSUConvertOffsetToPtr`, `(uintptr_t) < 0xC0000000`) | 2 | **Open** | Goal-Port B.3 open (platform-notes) | Not done |
| R08 | Game logic at 30 Hz | 3 | **Done** | Log: `[framerate] logic 30/s, extra presents 0/s, fpsLimit 0`, 56 samples over 60 s title run (platform-notes) | Only title and delfino verified |
| R09 | Unlocked framerate, pacing window (30/60/120/144/240/unlimited) | 5 | **Partial** | `src/framerate/`, `sunshinePaceWindow`. Gate open at delfino/0 fps 144 (stack dump only) | Per-second window count not logged. 60/120/240 not measured. Logic 30 ± 0.1 Hz in gameplay not verified. Step 2 interpolation not done |
| R10 | Widescreen and Hor+, 4:3 UI anchoring (part B) | 5 | **Not started** | Designed only. Setting persisted (`launcher`) but no effect | No code |
| R11 | Movies (intro, THP) on screen | 3, 4 | **Partial** | THP decoder max error 0 vs reference (offline) | On-screen picture not verified at boot |
| R12 | Shine sequence, saving (memory card) | 3 | **Partial** | `src/sdk` card path; `CardSave` crash fixed via dev skip (`gDevStartSkipPreStageMovie`) | Save and load not verified in-game. Shine flow not verified |
| R13 | Full playability: all worlds, bosses, Yoshi, nozzles, Corona Mtn, credits, music, saving | 4 | **Not started** / unverified | Not reached beyond delfino stage load | Bosses and Yoshi depend on R04. Music depends on DSP (R20) |
| R14 | Audio: DSP host (ADPCM, PCM, loops, streams, osc waveforms 0/1/3/4/7/10, resampler, FIR/IIR, pan) | 3, 4 | **Partial** | `src/sdk/dsp_glue.cpp`, `dsp_mixer.cpp`. `dsp_test.cmd` | Not implemented: 2-bit ADPCM, other osc waveforms, per-channel delay, dist low-pass, surround buses 8/9, Dolby. Runtime order unverified |
| R15 | Float contraction and paired-single `ps_madd` (`fmaf`) policy | 2, 3 | **Open** | Undecided (platform-notes). `OSf32tos8` NaN behavior unverified | Affects match and gameplay numerics |
| R16 | File formats swapped on host (RARC, BTI, BFN, BLO/J2D, BMG, ResNTAB, J3D, JAudio AAF/BNK/WSYS/BARC, JPA, THP) | 2 | **Partial** | Offline tests (`j3d-test`, `jpa_test`, `thp_test`). `BCSV_U32/U16` read-only swap fix in `ToolData.hpp` | Runtime-verified only where the stage ran. `ms_m_spinos.jpa` disc one-byte shift defect unhandled |
| R17 | Frame pacing / `sunshineReplayFrame` with skeletal animation | 5 | **Partial** | Replay masks `CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW`. Skeletal animation not interpolated | Visual check of interpolated frames not done |
| R18 | Logic tick hook: cheats, mods | 6 | **Partial** | Hook `sms/src/System/MarDirectorDirect.cpp` → `sunshineLogicTick()`. `port_tests.cmd` 51 checks expected with the game-code host checks (`tests/sms_host_checks.cpp`; 52 observed by RefDecompFix, then the hit-check table replica check was dropped). `[cheats] 4 cheats loaded`, example mods load, SEH isolation proven | In-game cheat effects not verified (health, water, lives, nozzles). `cheats.user.json` and overlays not verified. Instant-death paths bypass `decHP` guard (`MarioMove.cpp` ~1948, ~2247, `MarioRun.cpp` ~978). `gpMarioOriginal` may dangle |
| R19 | Launcher: image select (ISO/GCM/RVZ/WIA/CISO/GCZ), hash detection, remember path, `--dvd` | 6 | **Partial** | Pages rendered (video, audio, controls, interface, cheats). `--dvd` path works. Disc check: `aurora_dvd_open` + `GMSJ01` | Browse, Start, invalid-image rejection, clicks not verified. Hash-based region/revision detection not implemented (only ID check). Cheats tab placeholder. No gamepad navigation |
| R20 | Launcher graphics (res, window mode, monitor, VSync, FPS, internal scale, MSAA, aniso, aspect, HUD scale, D3D12/Vulkan) | 6 | **Partial** | `settings.ini` round trip ok (`settings_test`). Window mode, size, VSync (next start), render scale, FPS presets (0/30/60/120/144) applied | No MSAA, aniso, aspect, HUD scale, monitor, D3D12/Vulkan choice. FPS and scale are cycling presets, not sliders |
| R21 | Controls: keyboard + SDL3 gamepad, profiles, analog trigger half-press, deadzones, sensitivity, camera inversion | 6 | **Partial** | Keyboard defaults (WASD, IJKL, arrows, Space/E/Q/F, Enter, Z, LShift, LCtrl). Stick dead zones applied via `applyDeadZones` | Trigger zones at Aurora defaults. No profiles, sensitivity, inversion. Gamepad pad mapping unverified |
| R22 | Button prompts (GC/Xbox/PS/Switch/keyboard, BMG and textures) | 6 | **Partial** | 90 PNGs `assets/glyphs/{xbox,playstation,switch,gamecube,keyboard}` (keyboard added 2026-10-15, original art). Style setting persists and applies (`glyphsSetStyle`). `[glyph] <style>/{a,b,x} 128x128` logged for all four styles (`build/PortLauncherRun/glyph_<style>.log`). Frames show the style change at the water-button slot (xbox blue X, playstation pink square). `playstation/x.png` is a square (□), not a cross (✕) | Only A, B, X, Start hooked (`JUTResource.cpp`). Y, Z, L, R, D-pad, C-stick, stick glyphs not hooked. BMG prompt text not done. `guide_tx_b` visual check pending. No keyboard style. `[glyph] <style>/start` not in the four style logs; seen once in `build/PortLauncher/shot.log` |
| R23 | Audio settings: volumes, device | 6 | **Partial** | Master volume applied. Volume presets 100/75/50/25/0 | Per-channel volume and device selection not done (`launcher.cpp:86-90`) |
| R24 | Mods: folder, enable/disable, load order, SZS/RARC path replacement, Dolphin `tex1_` packs, conflicts | 6 | **Partial** | Scan `<exe>/mods/*/mod.json`, `mod.dll`, ABI 1.0, load order, overlay merge, SEH. Loader log measured: `[mods] 1 overlay file(s) active` in 10 mod-on runs; `no mods folder` in 3 mod-off runs (`build/PortLauncherRun/*.log`) | Overlay effect not verified: no `overlay in use` / `overlay replaced` line in any run; station text not reached at bianco. `tex1_` packs not present. Conflict reporting not present. Enable/disable UI not verified |
| R25 | Cheats: native toggles, explain Gecko/AR unsupported | 6 | **Partial** | `assets/cheats.json` (4 cheats), fixed C++ effect table | Effects in-game unverified. Gecko/AR message not checked |
| R26 | Storage `%APPDATA%\<project>`, TOML/JSON | 6 | **Partial** | Settings at `%APPDATA%\Sunshine\settings.ini` (fallback exe dir). Config is INI, not TOML/JSON | Spec mismatch: Port.md says TOML/JSON. Document or change |
| R27 | DLSS | 7 | **Not started** | Not implemented. Blockers: no motion vectors/jitter in GX path, Dawn D3D12 import-only, no DLSS SDK headers (`docs/dlss-feasibility.md`) | Phase 7 blocked, documented |
| R28 | FSR1 upscaler (extra, not requested) | — | **Partial (extra)** | `SUNSHINE_UPSCALER=fsr1`, `render_scale.cpp`, `gpu.cpp`. Packaged exe runs 20 s, WGSL compile not confirmed | Not applied to RmlUI frames with a backdrop filter (RmlUI overlay frames are resampled). In-game frames measured on WARP in T2 and after Aurora patch 0015 (`platform-notes.md`, T2 section). Not requested by Port.md; decide keep or drop |
| R29 | Release package, installation-free, no protected data | 8 | **Partial** | `tools/package.py` → `dist/sunshine-0.1.0/` (+ zip, sha256, 20.3 MiB). Hard-fails on game-image extensions. No game data tracked (verified above) | Licence texts not pinned (Dawn, nod, SQLite, DXC). VC redist EULA unchecked. Port licence undecided. `release-checklist.md` still names `build/windows-clang-release` |
| R30 | Packaged run from clean dir, `--dvd` | 8 | **Partial** | 20 s from clean dir, process stays alive, log empty | No screenshot or in-game verification |
| R31 | Dev switches and test harness documented and removed from release | 8 | **Partial** | `SUNSHINE_DEV_START`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_CHECK_HEAP`, `SUNSHINE_HANG_DUMP`, `SUNSHINE_WINDOW_*`, `SUNSHINE_DEV_LAUNCHER_PAGE`, `SUNSHINE_DEV_AUDIO_STATS` | Need decision: keep as dev-only or gate out of release |

## 4. Blockers that gate "fully playable" (summary)

1. **Title render (R01)**: `GXCopyDisp` stub and EFB clear. Blocks visual correctness of every screen.
2. **Stage crashes (R02, R03)**: NpcParts ctor and dolpic heap writer. Blocks Bianco Ep1 and most worlds.
3. **Empty decomp source (R04)**: bosses, NPCs, fences, sand, talk, guide. Blocks boss and stage-object logic listed in `unresolved.md`.
4. **`__fres` (R05)**: blocks every particle emitter. No effects in game.
5. **Audio gaps (R14)**: missing ADPCM and osc waveforms blocks music correctness.
6. **Unverified gameplay**: nothing past delfino load is verified in-game (shine, save, bosses, credits).

## 5. Next 20 tasks (priority order)

1. Fix `GXCopyDisp`: snapshot EFB into display source before any clear, and present that source (R01). Owner: GxCopy2 (coordinate; they own `lib/gx/*`).
2. Gate title-frame logs on copy-2 count, check background quad vs viewport and `drain()` (R01).
3. Verify copy-2 source rect (1280x960) vs logical 640x448 with aspect offset (0,32) (R01).
4. `TNpcParts::TNpcParts` crash at `NpcParts.cpp:56` for bianco/ricco/mamma/sirena (R02): not reproduced in 5 bianco runs. Prove the cause or retire it. Owner: StageBianco.
5. R03: check-mode quarantine race fixed. Prove or retire the normal-mode `0xc0000409` at 12-19 s. Owner: HeapHunt.
6. Run `probe-units` once and reconcile 151 excluded vs `report.json` 736 units. Update `excluded-units.txt` and `blocked-units.txt` if needed (A1).
7. Re-run `tools/gen-files.py` and confirm `files.cmake` matches blocked list (now empty) (A3).
8. Implement `__fres` with bit-exact reciprocal estimate (R05). Unblocks particle emitters.
9. Implement global `JKR_NEW`/`JKR_DELETE` allocator path (R06).
10. Fix pointer truncation: `JSUConvertOffsetToPtr` and `(uintptr_t) < 0xC0000000` (R07).
11. Fill empty bodies in order: Boss Hanachan (`TNerveBossHanachan*`), then `TNerveBW*`, Manta, Sand, Talk2D2, Guide (R04). Map `TNerveBW*` to its game object first (name not confirmed in this audit).
12. Add per-symbol justification list for the 157 + 10 undefined (A2), or fill them.
13. Decide float-contraction policy and `ps_madd` as `fmaf`. Verify `OSf32tos8` NaN (R15).
14. Implement missing DSP features in priority order: 2-bit ADPCM, then other osc waveforms (R14).
15. Verify in-game: title → file select → Delfino → Bianco Ep1 with Shine and save (A3, R12). Use `SUNSHINE_DEV_START` and `computer` screenshots.
16. Verify cheats effects in-game (health, water, lives, nozzles) and fix instant-death guard gaps (R18).
17. Verify launcher Browse, Start, invalid-image rejection, clicks (R19). Add gamepad navigation for launcher.
18. Hook remaining button glyphs (Y, Z, L, R, D-pad, C-stick, stick) and verify `guide_tx_b` (R22).
19. Settle the Port.md spec gaps: TOML/JSON storage, hash-based region detection, MSAA/aniso/aspect/monitor (R20, R26).
20. Clean up release: pin licence texts, check VC redist EULA, update `release-checklist.md` to `build/ReleaseDlss`, decide on `SUNSHINE_DEV_*` in release, remove stray root `buildaurora-gxtestsbuild-probe.out` and `crash.log` (R29, R31).

Deferred (not in the 20): widescreen part B (R10), framerate step 2 interpolation (R17), DLSS (R27, blocked by SDK and motion vectors).

## 6. Checks the main agent should run
- `probe-units` (resolve 151/736 count and the blocked list).
- Strict link and `_link.log` to confirm 0 unresolved and refresh `unresolved.md`.
- SHA-1 check of `sms/build/GMSJ01/mario.dol` against `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` (matches now).
- Title and delfino runs with `computer` screenshots (not yet done in-game).

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
