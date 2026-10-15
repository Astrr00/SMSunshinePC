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

- Ledger rows to re-review: `TTailRubber::adjust` (frame 0xc0 retail vs 0xa8, row says logic-verified) and
  `TBathWaterManager::perform` (frame 0x480 vs 0x418).
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

## Process notes

- Agents work in clones (`clone-3`, `clone-chase`, `fresh-gate-2`) on `agent/<name>` branches and never push; the lead
  cherry-picks after reading the diff, rebuilds the unit, runs strict link and `verify --matching`, then pushes.
- Boot smoke needs `SUNSHINE_ALLOW_CPU_ADAPTER=1` in a fresh shell (WARP); without it the run fails with "No supported
  adapters".
- A stopped agent leaves its partial edit in the clone; the next brief restores that one file first.
- Decompiled-function totals in README (87.9 %) come from the last full `ninja` + `changes_all` (2026-10-12); the
  rounds above are not yet reflected in `report.json` or the status treemap until that is rerun.
