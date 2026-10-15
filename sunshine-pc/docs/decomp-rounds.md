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

## Bath-water initializer re-review (2026-10-15, takeover)

BathInitializer reviewed the complete retail `initializeIfYet_` range `0x801FAC00-0x801FB8B8`, its source and embedded
`initialize`, `calcWaterModel` and drop helpers using the complete `decomp-diff.py --no-collapse` output.
No additional source correction was justified; the initializer ledger entry is restored to `logic-verified`.
This supersedes the pending initializer review above, not the separate `perform` spawn-position FP gap.

| Retail range | Checked path |
| --- | --- |
| `0x801FAC48-0x801FAC98` | Already initialized return; bathtub name lookup, null check and readiness field |
| `0x801FACDC-0x801FAE00` | Two actors; active count from parameters; reset all 500 allocated drops; hit-actor arguments and flags |
| `0x801FAE04-0x801FAFF0` | Both gravity paths; position integration, box clearing and bathtub collision; active-end pretests |
| `0x801FAFF4-0x801FB0CC` | `count*30 > activeCount`; average, square root and upper clamp; zero-height alternative |
| `0x801FB0D0-0x801FB324` | Intersection gate; unconditional first pair; stride reload, distance comparison and b-before-a box writes |
| `0x801FB328-0x801FB6A0` | Normal/lowered threshold, replenishment gates, reset or swap-delete and replacement-drop integration |
| `0x801FB6A4-0x801FB778` | Positive-lifetime gate, increment-before-compare, `age > lifeTime`; local end shrinks each iteration even without deletion |
| `0x801FB77C-0x801FB870` | At most one replenishment or excess-count clamp; 200 steps per actor; publish manager pointer after both actors |

Used FP dependencies retain all three `fnmsubs` boundaries (`0x801FB098`, `0x801FB184`, `0x801FB340`) and division order.
The two square-root sequences preserve `frsqrte`, `frsp`, multiply and correction rounding.
Retail's extra `half*normalY` multiply (`0x801FB1DC` / `0x801FB1F0`) only feeds a temporary overwritten before any reader;
the current object computes the same used value once.
The extra radius reload sees unchanged parameters across the checked helpers.
Constants and the Japanese lookup name are byte-identical in the object comparison.
The frame differs (`0x280` retail, `0x2b8` current), but live stack objects remain separate:
gravity `0x148/0x154` maps to `0x180/0x18c`, count `0x178` to `0x198`, accumulator `0x17c` to `0x19c`,
pair delta `0x160` to `0x1a8`, pair normal `0x16c` to `0x1b8`, and return objects
`0x188/0x194/0x1a0/0x1ac` to `0x1c4/0x1d0/0x1dc/0x1e8`.
Complete `getGravityDir` and `calcBathtub` diffs were also reviewed; used arithmetic, branches and writes agree.

Lead validation: `ninja -k 0` retains the reference DOL SHA-1; `ninja changes_all` reports no changes.
The full `report.json` retains `94.61472` for the initializer.
The direct per-object comparison reported `94.59019`; this score difference is not a source change or a matching gain.
No Corona fight comparison was run; modified drop-count/stride parameters remain subject to retail's unchecked capacity.

## Devscript scene lifetime correction (2026-10-15, takeover)

TransitionLifetime traced issue #12 to scene-heap ownership, not a corrupt-node condition needing a null check.
`MarDirectorLoadResource.cpp` allocates particle managers in the scene heap.
`TMarDirector::~TMarDirector` clears `gpEmitterManager4D2` and `gpMarDirector`, but the old 3D manager and several other
scene globals remain non-null when `Application.cpp` frees the scene heap.
The following PAD read invokes devscript before the next director's `direct()` has joined the setup thread.
Even the new director pointer alone is insufficient: its constructor publishes it before setup completes.

`devscript.cpp::sceneReady()` now requires gameplay state, the application's active director, and `unk260 == 1`.
That existing marker is set only after the setup-thread join and `setupObjects()`.
Status logging, scene-object route conditions and aim assist use this shared boundary before dereferencing scene data.
Live JPA traversal is unchanged; unavailable scenes still report THP state, tick and stage/scenario.
No retail director-state special case, pointer-address heuristic or Aurora change was added.

Lead validation: Strict link has zero unresolved symbols and all 62 headless checks pass.
The first route run, `takeover_lifetime85_20261015`, uses Null, step-retrace, skipped movies, aim assist,
`SUNSHINE_DEV_STATUS=1` and no periodic screenshot monitor.
It survives the airport Shine teardown and reaches Delfino 1/1 with one Shine; the 330-second wall limit stops it at
tick 15510 while starting the gate waypoint at route line 347 (rc 124).
An assert-based log check verifies JPA statistics in airport 0/1, Delfino 1/0 and 1/1, with no JPA traversal on ticks
marked `(scene not ready)` and no fatal exception.
This proves the corrected transition path, not completion of the full acceptance route within 330 seconds.

The extended fresh-profile run, `takeover_lifetime85_long_20261015`, keeps the same environment and raises only
the wall limit to 650 seconds: rc 0 after 364 seconds, done at tick 18210, Bianco 2/0, director state 4, one Shine.
`gci_check.py` independently validates the temporary card and all seven block checksums.
An assert-based check confirms that every sampled live-scene status retains JPA statistics, including airport 0/1,
Delfino 1/0 and Bianco 2/0, and excludes JPA traversal on all 61 sampled unavailable-scene ticks.
There is no fatal exception or route TIMEOUT in the completed run.
Route 86 with that card (`takeover_lifetime86_20261015`) returns rc 0 after 17 seconds, tick 1675:
the pre-load option-map count is zero and the loaded, controllable Delfino scene 1/0 has one Shine.
The historical expectation of scenario 5 is not part of the save-count contract.
The rebuilt binary also passes a separate 45-second WARP boot smoke with isolated APPDATA/USERPROFILE,
no fatal exception and no WebGPU error.
Evidence lives under `build/Route/takeover_lifetime*`, `build/takeover-strict.log`, `build/takeover-headless.log`
and `build/verify/boot.log`; no user save directory was used.
No fresh-clone or hardware-GPU check was repeated, and no remote issue state was changed.

## Bath-water sound cadence and FP closure (2026-10-15, continuation)

BathSpawnFP reviewed the complete `perform` range `0x801F887C-0x801F9BC8` and found a sound-control-flow defect.
Retail's non-quartertick path branches at `0x801F8920` through the spawn test to sound at `0x801F9AD0`.
Source incorrectly put sound inside the quartertick physics/hit block.
For an initialized manager, `CUE_MOVE`, old counter zero and positive amplitude, retail attempts sound while old source
does not.
The sound block now sits outside the quartertick block, still inside `CUE_MOVE`.
Spawn remains correctly gated: `(counter & 7) == 4` implies `(counter & 3) == 0`.

The rebuilt complete object differs by exactly one instruction: source `.text+0x150` now branches to sound `0x12fc`
instead of draw `0x1370`.
The fast path initializes every register it uses; live stack objects retain the reviewed injective mapping,
including sound position `retail 0x328 -> source 0x38c`, frames `0x418 / 0x480`.

The previous finite spawn-FP and random-wrapper gaps are closed analytically:
- All used fused arithmetic boundaries agree; the two extra retail X/Z fusions multiply exact zero by finite radius.
- Y retains `fma(1, radius, axis.y)`; final center additions retain finite rounding and signed-zero behavior.
- The folded up setter only writes `(0,1,0)` and has no hidden effect.
- Random conversion uses exact dyadic values from the same LCG state; range/velocity wrappers and RNG consumption agree.
- Lead CRT `fmaf` versus separate f32 assertions pass for zeros, signed zeros, normal/subnormal extrema and RNG
  endpoint/cancellation samples.

The ledger deliberately remains partial for unconditional NaN-payload/FP-state equivalence:
the final center additions swap operands (`retail 0x801F99C0/C4/C8`, source `.text+0x11e8/ec/f0`).
No remaining finite FMA-rounding mismatch was found; no assertion of NaN-payload bit identity is made.

Matching report remains `94.09952`, with `changes_all` empty: the corrected branch operand scores as a mismatch both
before and after, despite the important semantic change.
The wrapper uses `functionRelocDiffs=data_value` and reports `94.083336`; the full report uses default comparison.
Both inspect the same objects, not stale or alternate paths.
Lead validation: full matching/reference SHA-1 PASS, Strict zero unresolved, 62 headless checks PASS,
45-second WARP boot PASS; logs in `build/bath-fp-verification.log` and `build/verify/`.
The final `cd552f5` source also passes a separate isolated fresh-checkout Strict rebuild with zero unresolved symbols.
Status-enabled Null route `bath_sound_fix85_20261015` finishes at tick 17790 (368 s wall) in Bianco 2/0
with one Shine; 531 ready-scene JPA status samples have no overlap with 61 not-ready status samples.
The saved seven-block GCI passes independent block checksum validation.
This route does not enter Corona, so it checks progression, scene diagnostics and persistence, not audible sound.

Native Corona smoke was attempted against the before-fix `3c10554` fresh-checkout build.
The real scenario is stage 60/0, archive `coronaBoss.arc`; the normal dev-start prefix resolver cannot select a name
without the trailing digit it expects.
An isolated clone temporarily accepted an exact archive basename and logged sound-block entry; neither hook is shipped.
Run `bath_sound_before_20261015` reaches stage 60/0 at tick 109, then aborts on `convert_texture: unknown texture format 22`
before any sound observation (`0xC0000409`, about two seconds).
This pre-fix failure is tracked in [issue #13](https://github.com/Astrr00/SMSunshinePC/issues/13).
Both temporary source hooks and the throwaway route were removed.
No audible cadence or Corona-fight comparison is claimed.

## EventWatcher whole-function reviews (2026-10-15, continuation)

EventWarpAudit reviewed complete `evWarpFrontToMario` and `evAppear8RedCoinsAndTimer` plus their helpers.
No source change was justified.
Warp's retail yaw reload has no intervening aliasing store or call; register masking gives the same table index.
For `actor == Mario`, face yaw (`+0x96`) is distinct from rotation (`+0x34`), and position is copied before actor writes.
The whole-function ledger stays `logic-verified`; its stale score is corrected to the full report's `94.98582`
(wrapper `94.950356`).
Appear8 is already byte-identical: all 122 instructions, eight-coin loop, particle arguments and timer/push paths agree.
Its stale `partial` row becomes `matched` at 100%; this is a ledger correction, not an additional matched function.

## Corona static depth texture (2026-10-15, issue #13)

Two independent read-only investigations traced the pre-fix `fmt=22 4x4 mips=1 sourceBytes=0` log to
`GX_TF_Z24X8` (`0x16`), not an unknown disc/BTI format. The mesh renderer's constructor creates a
4×4, no-mipmap `GXTexObj` from its 64-byte static `clear_z_TX` array in `BathWaterManager.cpp:1027-1038`;
the prerender loads that object at line 1111. The logged host pointer was not symbolized, so its
identity remains an inference from the unique constructor tuple and the successful post-fix probe.
Aurora's `texture_source_size` returned zero for depth format 22; `convert_texture` then fatally
rejected it. Patch 0017 adds that format to the existing 64-byte tiled RGBA8-sized/decode paths,
preserving its GX format and full texture source bytes. The static array is not an archive resource.

The port dev-start stage resolver now accepts an exact `.arc` basename in addition to numbered
stage prefixes, so `SUNSHINE_DEV_START=coronaBoss/0` selects retail stage 60/0 without a probe-only hook.
Pre-fix `bath_sound_before_20261015` aborted at tick 109 on Null (`0xC0000409`).
Post-fix `corona_z24x8_after_20261015` completes on Null (rc 0, 14 s, tick 705;
15 stage-ready status samples); `corona_z24x8_warp_shot_20261015` completes on WARP
(rc 0, 15 s, tick 375; four ready status samples). Its `corona.png` visibly shows
Corona's boss, arena and HUD. The permanent `tools/routes/98_corona_depth.dev` checks a live stage-60/0 director
and captures a later boss frame; its WARP run `corona_depth_regression_20261015` completes (rc 0, 16 s, tick 393).
Neither run reports unknown textures, fatal exceptions, WebGPU validation errors or a script timeout.
The tests use isolated saves and dev aids
(step-retrace, movie skips, muted audio); they do not demonstrate a completed boss fight,
CPU depth-copy readback/height-field accuracy, audible bath-water cadence or hardware GPU behavior.
The DOL SHA-1 is unchanged and Strict links with zero unresolved symbols; see the latest
`build/verify/` logs for matching, 62 headless checks and the 45-second WARP boot.
Full `verify --build --matching --boot-secs 45` at `21e2b60`: 7 PASS, 0 FAIL, 1 optional SKIP;
Aurora checker confirms 17 tracked patches on 44 files, strict zero unresolved, 62 headless checks PASS.
The existing isolated fresh checkout fast-forwards to this commit, applies patch 0017 separately, then
rebuilds Application.cpp, both Aurora texture files and the Strict executable with zero unresolved symbols.

## Corona GPU depth and reflected basis (2026-10-15, issue #14)

`cb28330` with a temporary read-only probe runs Corona on WARP for 900 mesh renders at stock width 84,
but all CPU depth bytes and generated heights stay zero (`corona_depth_before_20261015`, rc 0, tick 1024).
Two independent audits find missing Z24X8 conversion/readback and a native-endian consumer of raw GX bytes.
Aurora 0018 reuses the R8 transport: actual depth becomes RGBA `(HH,MM,LL,0)`, then canonical 4×4
two-plane GX tiles. Registered copies retain generation/buffer/format guards; callbacks stage bytes,
and only the game thread applies a previously completed result. R8 packing is unchanged.
Compile-time packer checks cover 0x123456, zero/full depth, tile boundaries, width 4/84 and padded GPU rows.
Bath water registers on first prerender after stage reset, re-registers only on width change,
initializes valid far-depth tiles, uses an unscaled single-sample offscreen prepass and decodes big-endian bytes.
Square projection plus N×N viewport reproduces retail's top-left N×N mapping; no world-to-texel scaling change.

The first real readback still returned only 0xFFFFFF. Pass dumps prove 160 sphere draws plus a cap,
active depth writes and a correctly preserved offscreen depth attachment, ruling out missing transport.
A temporary cull-off control produces varied depths. Retail assembly `0x801FE320-0x801FE33C` then proves
`setLookDir` must form row 1 as `Z cross unnormalized X`, not `X cross Z`.
The old header reflected the view (determinant -1) and reversed cap winding; the one-line root fix
restores determinant +1 with backface culling enabled. Its only production caller is bath water.
The old BR20 ledger claim was false and is downgraded to partial pending corrected compiled FP review.
Whole source/retail algorithm review covers direction normalize/negate, both cross products,
independent epsilon guards/normalizations, row stores and unchanged translation.
Matching score falls from about 97.57 to 97.48333 despite fixing behavior; no byte-identical function is lost.
Full matching SHA-1 remains `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.

Real GPU/consumer proof: WARP `corona_depth_fixed_half_20261015` (scale 0.5, rc 0, 36 s, tick 1023)
and `corona_depth_fixed_quarter_20261015` (scale 0.25, rc 0, 39 s, tick 1024).
Captured fields at render counts 120/600 contain 28 224 heights. Independent canonical tile decoding
agrees exactly with every generated local Y, including `[x][y]` orientation and f32 scaling.
Fields vary over time; the half-scale captures contain 1 411/1 437 distinct depths.
Known cap depths are exactly 0xC00000 and 0xE00000 (0.75/0.875 for center Y=4800, R=3000,
view eye Y=13800 and far=12000); untouched background is 0xFFFFFF. Waves are visible in `depth.png`.
The existing transport reports 300 applies with 2–4-render-copy lag, not same-copy GameCube synchronization.
The permanent host geometry test calls real `setLookDir`; old cross order fails (63 checks, one failure).
R8 regression `corona_depth_goop_r8_20261015` completes (rc 0, 34 s, tick 879): applied bytes,
sprayed point `d0=0/0`, separate dirty point `d300=65/1`; no clear-all pollution regression.

The optional Aurora `gx_fifo_tests` target cannot build with this toolchain: vendored googletest
`gtest-printers.h:528` fails `-Werror,-Wcharacter-conversion` on char8_t to char32_t.
No dependency warning policy was changed to bypass that failure; the GX suite was not run.
Final source `b1392e5` passes `verify --build --matching --boot-secs 45`: 7 PASS, 0 FAIL, 1 optional SKIP;
63 host checks, unchanged SHA-1, Strict zero unresolved, 18 tracked patches on 45 Aurora files, 45-second WARP boot.
The isolated checkout fast-forwards to this source, applies 0018 on top of the earlier 17 patches,
and completes 481 build steps (affected header users, Aurora and Strict executable) with zero unresolved symbols.
Temporary pixel/height dump and geometry logging, plus the cull-off discriminator, are not shipped.
Uninstrumented `b1392e5` regression chain: WARP route 98 `corona_depth_final98_20261015`
(rc 0, 37 s, tick 1023, visible waves); Null status-enabled route 85 `corona_depth_final85_20261015`
(rc 0, 471 s, tick 22436, Bianco 2/0, one Shine); independent seven-block GCI checker PASS.
Route 86 `corona_depth_final86_20261015` loads that run's isolated card (rc 0, 16 s, tick 1674,
Delfino 1/0, Shines 0 -> 1). Route 85 has 686 live JPA status samples and 61 not-ready samples
with no tick overlap. No temporary probe text, script timeout, fatal exception or WebGPU error appears.
Full fight, audible sound, hardware GPU, forced registration/map races and same-frame timing remain unverified.

## Process notes

- Agents work in clones (`clone-3`, `clone-chase`, `fresh-gate-2`) on `agent/<name>` branches and never push; the lead
  cherry-picks after reading the diff, rebuilds the unit, runs strict link and `verify --matching`, then pushes.
- Boot smoke needs `SUNSHINE_ALLOW_CPU_ADAPTER=1` in a fresh shell (WARP); without it the run fails with "No supported
  adapters".
- A stopped agent leaves its partial edit in the clone; the next brief restores that one file first.
- Decompiled-function totals were refreshed by full `ninja` + `changes_all` on 2026-10-15; the residue rounds and
  reserve-drop correction are now reflected in `report.json`, README and the regenerated status treemap.
