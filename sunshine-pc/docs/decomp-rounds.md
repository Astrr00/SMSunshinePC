# Decomp residue rounds (2026-10-15)

Work log for GitHub issue #10 (decomp tracking): agent rounds that raise the match of NonMatching units without changing
behaviour. All numbers are `python sunshine-pc/tools/decomp_triage.py --build <unit>` output, reproduced by the lead on
the main tree after landing. Gates after every landing: strict link 0 unresolved, `verify --matching` PASS with DOL SHA-1
`9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, boot smoke alive 45 s. Behaviour equality of each edit was checked by hand
(same operations, same order of side effects); no ledger rows were written by these rounds.

## Results

| Round | Model, effort | Unit | Result |
| --- | --- | --- | --- |
| 1 | Haiku 5.5 | `GC2D/CardLoad` | `selectFunction` 96.80 -> 96.97 (pad 0xa0 -> 0x98, frame 0x498 = retail) |
| 1 | Haiku 5.5 | `Map/BathWaterManager`, `Enemy/fireWanwan` | no gain; ledger rows to re-review (see Open) |
| 2 | Haiku 5.5 | `Enemy/bosswanwan` | `TBWBinder::bind` 95.50 -> 95.62 |
| 2 | Haiku 5.5 | `Enemy/bossgesso` | no gain |
| A/B | Sonnet 5.5 high | `Enemy/bossgesso` | 9 functions up: `lenFromToeToMario` 92.37 -> 99.51, `changeAttackMode` 98.10 -> 99.95, `stopIfRoll` 96.48 -> 99.87, five nerve `execute` to 99.8-99.9 |
| 3 | Sonnet 5.5 high | `System/EventWatcher` | `evIsNearActors` 96.06 -> 100, `evWarpFrontToMario` 84.79 -> 94.95; nonmatching 32 -> 31 |
| 3 | Sonnet 5.5 high | `Enemy/bosstelesa` | `Spit::execute` 97.37 -> 100, Body/Tongue `receiveMessage` 95.0 -> 100, ten nerves up; nonmatching 25 -> 22 |
| 3 | Sonnet 5.5 high | `Enemy/hamukuri` | `forceRoll` 90.23 -> 99.66, `generateBody` 96.36 |
| 4 | Sonnet 5.5 medium | `Enemy/bosspakkun` | `calcBellyScale` 96.15 -> 100; nonmatching 21 -> 20 |
| 4 | Sonnet 5.5 medium | `MoveBG/MapObjMamma` | `TLeanMirror::control` 98.33 -> 100, `TSandCastle::expanded` 90.48 -> 99.67, six more up; nonmatching 25 -> 24 |
| 4 | Sonnet 5.5 medium | `Enemy/bossManta` | `updateAttractor` 97.64 -> 99.08; nonmatching 21 -> 20 |
| 5 | Sonnet 5.5 high | `Enemy/bosseel` | `TOilBall::moveObject` 89.76 -> 99.76, `TBossEel::perform` 98.15 -> 98.85 |
| 5 | Sonnet 5.5 high | `MoveBG/MapObjMare` | `TMuddyBoat::bind` 90.77 -> 92.00 (all stack offsets = retail, frame 0x270) |
| 5 | Sonnet 5.5 high | `MoveBG/MapObjBall` | stopped by the owner before a result; nothing landed |

Haiku improved 2 functions by under 0.2 points in two rounds; Sonnet 5.5 improved 9 functions on the unit where Haiku
found nothing. Medium effort gave 1-9 functions per unit, high gave more on the first unit; the sample is small.

## Techniques that worked

1. Named locals (`dx*dx + dy*dy + dz*dz`) fuse to `fmadds` where `length()`/`dot()` do not; named scale components or a
   `cap` copy of a clamp limit change register shape the same way.
2. Retail's inlined copy of a method can have another compare shape than its out-of-line copy: a file-static inline
   helper with that shape at the inlined call sites (bossgesso `changeAllTentacleStateInl`, set {3,4,6} unchanged).
3. A pad (`char trash[N]`, unused local) declared after the local that must sit higher; pads declared first only grow
   the frame.
4. A shared inline fixed once through a named local from a CSE'd pointer fixes many nerve functions (bosstelesa
   `setBossTelesaBck`).
5. A discarded expression inside a static inline helper gives retail's out-of-line `sub` or ctor call.
6. A helper that returns a reference to a by-value parameter mimics retail; keep a by-value version under
   `#ifdef TARGET_PC` because clang-cl drops the scale on the reference-return (hamukuri `scaledVec`).
7. `JGeometry::TUtil<f32>::one() * x` keeps a multiply that `1.0f * x` folds away.
8. Operand order of commutative float adds (`frame + speed`) changes the register order.
9. `#pragma dont_inline on/off` around a static helper forces an out-of-line call where retail has one.

## Open (from the reports)

- Ledger re-review completed on 2026-10-15: `TTailRubber::adjust` confirmed; `TBathWaterManager::perform` downgraded
  to `partial (equivalence gaps found)`; see the audit below.
- `bossgesso`: `TBossGesso::perform` frame 0x2c8 vs 0x218, `TBGBinder::bind` 86.96 (out-of-line `TVec3::sub` per call
  site), `changeAttackMode` frame 0xa8 vs 0x90.
- `EventWatcher`: `evWarpFrontToMario` retail re-reads the angle with `lhz`; `dispatchBuiltin` function-pointer register.
- `MapObjMamma`: `TMammaMirrorMapOperator::perform` 91.26, `TSandCastle::waitBeforeExplode` 91.78 (flag copy slot).
- `bossManta`: `updateMantaEscape` 90.05 (x in memory, y/z in registers), `spawn` 96.51 (retail adds an unassigned `f26`,
  probably a caller artifact, not proven).
- `bosseel`: `perform` retail calls `TVector::begin` out of line inside an inlined helper; `collideToMario` FPR groups.
- `MapObjMare`: `bind` register/schedule rows (about 56); `touchWater`, `TCogwheel::draw` residue.
- Permuter: `permute_fn.py` fails on functions whose base.c pycparser cannot parse (`TUtil<f32>::sqrt`, `0.0f` literal
  expressions, reference locals); nothing was run through it in these rounds.
- `decomp_queue.py claim` needs `sms/build/triage.json` (a full `decomp_triage.py` run); agents skipped the claim.

## Ledger audit (2026-10-15)

Rebuilt both units with `decomp_triage.py --build`, ran `--check`, `--branchscan` and `--immscan`, and read both complete
function diffs with `decomp-diff.py --no-collapse` against their source, inlined helpers and retail assembly.
This initial audit changed only documentation; the source correction and validation are recorded in the follow-up below.

- `TTailRubber::adjust()` (94.12251 %): `logic-verified` confirmed.
  Interior-node loop, head and tail updates preserve all arithmetic, operand order and stores; there are no calls.
  Every referenced local stack word maps from retail offset `s` to `s - 0x18`; the frame gap is 0xc0 vs 0xa8.
  The twelve additional loads reload position xyz and the bound rate once per inlined body, without intervening writes.
  The old DFire (mangled name) and BR1 (demangled name) rows are consolidated under the full demangled function name.
- `TBathWaterManager::perform(unsigned long, JDrama::TGraphics*)` (93.91747 %): downgraded from `logic-verified` to
  `partial (equivalence gaps found)`.
  In the inlined `calcWaterModel`, retail falls from 0x801F8C64 directly into the first pair calculation at 0x801F8C68;
  it tests the inner-loop bound only at 0x801F8E70, after advancing the second drop pointer.
  Our inner `for` checks the bound before its first calculation.
  For example, one active drop and stride 1 cause retail to read a second drop slot while ours skips the pair entirely.
  This is a control-flow difference; whether that slot is allocated/initialised, and the gameplay effect, remain unproven.
  Do not restore the verification status from a matching score or a clean branch scan alone.
  Also pending: final spawn-position FP boundary equivalence (15 vs 13 fused ops, constant up-vector folded in our build).
  The old factor-10 random-velocity correction is still present.
  Correct frame sizes: retail 0x418, ours 0x480; the original open note had these reversed.

No runtime validation was performed by the initial audit.

## Bath-water reserve-drop correction (2026-10-15, `bf1ea15`)

Retail reserves 500 drops (`TBathWaterManager` constructor, 0x801FC000-0x801FC02C).
`initializeIfYet_` sets the active count from `numDrops`, then resets every allocated slot up to the capacity of 500
(0x801FACF0-0x801FADA8), including positions, velocity, collision boxes and age.
The first pair beyond the active count therefore accesses initialized reserve state for the stock wave settings.
Read-only parsing of the local disc's `data/params.szs` gives wave `numDrops=160`, `intersects=1`, overflow `0/0`,
and attack `0/0`; `data/scene/coronaBoss.szs` contains no bath-water parameter overrides.
No game data is included in the change.

Changed the shared inner loop to `do/while`, retaining the outer bound and stride update.
Both retail call sites enter the first pair before testing the inner bound: `perform` at 0x801F8C64-0x801F8C68,
and `initializeIfYet_` at 0x801FB108-0x801FB10C.
With 160 active drops and stride 1, this restores the last active drop's interaction with reserve slot 160.
The rebuilt objects were checked against both retail entry sequences, including an assert-based check.
`changes_all` reports only the two users of the shared helper: `perform` 93.91747 -> 94.09952,
`initializeIfYet_` 94.67607 -> 94.61472 (the small score drop is accepted for the same asm-proven logic correction).
No byte-identical function was lost.

Validation: full matching build and reference SHA-1 PASS, strict Windows link with zero unresolved symbols PASS,
62 headless checks PASS, 45-second title boot on WARP PASS (no fatal exception or WebGPU error).
Fresh local clone of `bf1ea15` with a separately checked-out Aurora submodule and its 16 tracked patches builds and
links with `SUNSHINE_ALLOW_UNRESOLVED=OFF`; dependency sources and prebuilt packages were reused from the local cache.
Full report: 11 323 / 12 881 byte-identical functions (87.90467 %), 62.6544 % code bytes, 98.66045 % fuzzy,
87.469604 % data, 445 / 736 complete units.
These checks do not prove the Corona fight's visible behaviour.
`perform` remains partial for the spawn-position FP boundaries; the old `initializeIfYet_` whole-function claim is also
withheld pending renewed review, since its prior review missed the same loop.

Route regression: `bath_audit85_nostatus_20261015` runs route 85 on Null with `SUNSHINE_DEV_STEP_RETRACE=1`,
`SUNSHINE_DEV_SKIP_MOVIES=1`, `SUNSHINE_DEV_AIM_ASSIST=1`, `SUNSHINE_ALLOW_CPU_ADAPTER=0`,
`SUNSHINE_DEV_STATUS=` and `AURORA_SCREENSHOT_EVERY=` (empty values remove those variables).
Result: rc 0 after 230 s wall, script completes at tick 22434, stage 2/0, one Shine; `gci_check.py` PASS on the temporary card.
The first run, `bath_audit85_20261015`, kept the default automatic status logger and crashed after 50 s, during the
airport Shine-to-next-stage transition: `logJpa` at `devscript.cpp:539` reads address `FFFFFFFFFFFFFFFF` (0xc0000005).
The passing rerun does not resolve that diagnostic crash; it only demonstrates route completion without periodic status output.
Control `bath_control85_ready_20261015` rebuilt the fresh checkout with bath-water source byte-identical to `35e0090`:
it also exits 0xc0000005 reading `FFFFFFFFFFFFFFFF` during the same airport transition, after 120 s wall, last periodic
tick 8520. The control lacks debug symbols, so its exact faulting object remains unidentified.
The transition failure exists with pre-fix bath-water source too; tracked in
[issue #12](https://github.com/Astrr00/SMSunshinePC/issues/12).

## Process notes

- Agents work in clones (`clone-3`, `clone-chase`, `fresh-gate-2`) on `agent/<name>` branches and never push; the lead
  cherry-picks after reading the diff, rebuilds the unit, runs strict link and `verify --matching`, then pushes.
- Boot smoke needs `SUNSHINE_ALLOW_CPU_ADAPTER=1` in a fresh shell (WARP); without it the run fails with "No supported
  adapters".
- A stopped agent leaves its partial edit in the clone; the next brief restores that one file first.
- Decompiled-function totals were refreshed by full `ninja` + `changes_all` on 2026-10-15; the residue rounds and
  reserve-drop correction are now reflected in `report.json`, README and the regenerated status treemap.
