# Focused gameplay audit (2026-10-21)

This bounded audit of commit `40afd9bbf69fae00c9ecf5646bd4e732a903840e` establishes no new supported game-code defect.
No gameplay correction, speculative initializer or hang bypass is applied.
PC-CORE remains open; the [ROADMAP](../../ROADMAP.md) retains the remaining gameplay and release gates.

## Fresh checks

- A syntax-only compiler pass over all 72 GC2D/System compilation units completes with exit code 0 for every unit and four uninitialized-value warnings.
  It removes the compilation database's `/W0` suppression before enabling the selected diagnostics.
  A deliberately uninitialized canary emits the expected warning with the same settings.
  This is not a new Strict link or gameplay test.
- A further 74 Player/Camera/Strategic units complete with exit code 0 and eight uninitialized-value warnings under the corrected settings.
  Compiler warnings are leads requiring caller checks, not proven gameplay defects.
- A metadata scan of all 107 original scene archives checks collision vertex counts and the spacing between successive groups' triangle-index arrays.
  Every successive index array is contiguous, so the single-start-pointer implementation in `TMapCollisionBase::updateCheckData()` has no supported-data counterexample in this scan.
  The only non-root `/map/map.col` collision resource exceeding 350 vertices is `mare_ex0.szs:/map/map/map.col`, with 520 vertices.
- A separate bounded loader-tail scan of 107 archives finds 235 direct-name records with all expected scalar fields and no decode errors: `MapObjStartDemo` 4, `MapObjChangeStage` 87, `MapObjWaterSpray` 106, `RedCoinSwitch` 13, `RollBlock` 5, `MapObjSwitch` 9, `JumpMushroom` 10 and `FluffManager` 1.
  It does not cover loader aliases such as `Puncher`, `WarpArea` or colored RollBlock variants, or establish complete actor-loader coverage.

## Rejected candidates

**Wire-trap cooldown.** `TWireTrap` does not explicitly initialize `mCoolTimer` in its constructor or loader.
The movement nerves read it before a collision assigns 30; contact checks also read it.
However, the production PC factory uses ordinary `new`, and `src/sdk/jkr_new_delete.cpp` zero-fills ordinary allocations with `calloc`.
The check-heap and page-heap paths also supply zeroed storage.
Consequently this field starts at 0 on the supported allocation path.
A deliberately dirty placement-new fixture would not demonstrate a production failure.
The garbage-allocation diagnostic override is outside this acceptance claim.
The separate water-power field is protected by the initialized water timer.
Two readers independently checked the cooldown's callers and writes; the allocator check rejects the candidate.

**Collision scratch buffer.** The 520-vertex Noki secret resource does not establish an overflow of the 350-vertex transformation buffer.
`TMapStaticObj::initMapCollision()` requests static collision.
`TMapCollisionStatic::setUp()` selects flag 4 for at least 350 vertices, and `TMapCollisionBase::initCheckData()` then uses the decoded resource vertices directly instead of the stack scratch buffer.
No oversized moving-collision resource was identified by this scan.

**Other host-layout leads.** Inspected raw nozzle, console-holder and thread-result accesses already have PC-specific handling.
`TMessageLoader::getMessageEntry()` narrows its bounds check to 16 bits, but the inspected Talk/HUD callers mask their IDs and the subtitle caller is bounded.
No supported oversized-ID caller was established.
The apparently empty rail-camera mode query also returns false in the original PPC assembly; its body is not evidence of an omitted port implementation.

**Warning triage.** The Talk2D2 timer value is assigned by the surrounding cases 0/1/6; fruit/count values are read only when the same initializing case also sets a non-null basket.
Mario's pushing animation returns before using unset wall angles, and Yoshi reads its fruit selection only after a recognized fruit case.
The running-animation and splash-effect blends cover finite values; no supported NaN source is established.
The water-particle ground pointer and height are assigned together, so the non-null-pointer condition protects the height read.
Mario's landing-argument and jump-dispatch defaults remain caller-reachability questions rather than demonstrated failures.
The event-sound selector lacks a default, but a decode of 151 scripts in the common archive and 107 scene archives finds only seven `startEventSE` calls, all with constant argument 0 or 1 and no decode errors.
Thus that warning has no original-script counterexample in the inspected roots `/common/sp` and `/scene/map/sp`.

## Evidence limits

Private compiler summaries are `build/ga01-warning-audit.{log,json}` and `build/ga04-warning-audit.{log,json}`.
The warning canary is recorded in `build/ga03-warning-canary.log`; collision summaries are `build/ga02-collision-inventory.{log,json}`.
The bounded loader summary is `build/ga01-loader-audit.json` (57.5 seconds).
Script-call evidence is `build/ga01-eventse-calls.json` (56.8 seconds).
The corrected compiler passes took 17.0 and 19.2 seconds; the collision metadata scan took 58.2 seconds.
The latter reads archives offline and does not run 107 game scenarios.
The original `/O2` sweep, 91-scenario load batch and completed Shine routes were not repeated.
No new native gameplay, visible trap behavior, audio or full-content acceptance is claimed.
No original game data or private logs are committed.
Matrix counts remain 17 passed-bounded / 8 open / 0 external; graphics regeneration is unnecessary because their inputs do not change.
Mirrors remain deferred and release v0.1.0 remains draft.
