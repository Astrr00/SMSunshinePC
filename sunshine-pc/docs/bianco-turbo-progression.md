# Normal Bianco4/5 progression and Turbo verification

## Starting card and scope

The connected normal progression starts from `rt44_rocket_collect`: sixteen earned Shines, permanent Yoshi and the saved Bianco Rocket right. File B sector4/write23 has GCI SHA-256 `0a135d2b96c71776dee804f6eed0a8d0c8c7dfbf9d78407acc6fe243164618d2`. The necessary next rewards are Bianco Shine3 (Episode4 red coins) and Shine4 (Episode5 Petey rematch). Episode6 removes the six original stakes before the valid Turbo box. No developer starts, teleports, direct boss aiming, cheats or save/progress edits are used.

## Completed Episode4 milestone

`bt17_bianco4_full` passes normal entry, all eight red coins, the original windmill-top Shine3, save confirmation and controllable Delfino return: rc0 / 315 seconds, done tick8861. Minimum health6, no active cheats and an empty crash log. The published `tools/routes/pc_bianco_redcoins_unassisted.dev` is byte-identical to private `build/bt17-frozen.dev`; script SHA-256 `ec85307b5c280b2396955a341e338e32bb9935e36bf0affef05951829841e929`.

Independent GCI decoding shows File B sector3/write24 and seventeen Shine identities: `0,1,2,3,10,20,30,31,32,33,34,86,94,95,98,107,117`. The Yoshi/Rocket rights remain set and all seven sector checksums pass. GCI SHA-256 `6e3e1e78a846106ab106645f985344b3fe4ab3ac07a21113fb39771a0ea3b27b`. Subsequent fresh-process Episode5 attempts normally load that File B, assert Shine3 and retain this byte-identical GCI, establishing cold persistence without a redundant standalone replay. Episode4 Strict SHA-256 `91e9007ab0a9af18164d804230cadfbc356aa3fe3e2dad5126b1095332487200`.

From `sunshine-pc`:

```powershell
python tools/route.py Strict tools/routes/pc_bianco_redcoins_unassisted.dev --name <redcoins> --appdata-from rt44_rocket_collect --seconds 600
```

## Episode5 calibration and defensive-input correction

Native controller inputs establish normal Episode5 entry, washed roof Rocket pickup, a Rocket climb to the high Poink area, charged Poink shots that wake sleeping Petey, a return to the lower field and ordinary upward spray that knocks the flying boss down. Read-only observations report the original Petey weak point/animation/position, Poink attachment/scale/position and the selected defensive target. Temporary live-process collision inspection only reads memory; it does not change game state or controller decisions during a shot.

Calibration failures include goop/enemy damage, missed landing/shot timing, START ignored during airborne actions and a ground approach that outlasts Petey's short mouth window. They do not establish an original-game defect or undefined behavior. Canceled runs carry an explicit private reason; an empty crash log is normal.

`bt30_bianco5_phase` establishes a separate automation defect: `nearestSmallEnemy` selects an attached `TPopo` as a defensive spray target. The guard suspends route line194 and continues spraying while the Poink remains attached at scale2.5. Original `TPopo::checkTrigger` absorbs that spray and releases the charged projectile only after the trigger is released. This is a controller-selection loop, not a game hang.

Both defensive callers (`guard` and `clear`) now exclude `TPopo`. Normal hostiles retain the existing live/visible/height/range filters. In the first160 timestamped defense observations of the corrected `bt31_bianco5_defense` run, seventeen show an attached Poink with no defensive target; ordinary actor types `0x10000003` (21 observations) and `0x10000004` (one) remain eligible. The route also keeps Rocket selected until descending away from the Poink area, because an attached Poink would absorb spray aimed at a real threat too. No original game code changes.

The optional `SUNSHINE_DEV_SCRIPT_CHUNKS` directory accepts numbered controller scripts during calibration, replacing the unfinished input program when a new number is available; `SUNSHINE_DEV_EXIT_ON_END=0` keeps the session alive after END. Normal START inputs pause/resume the game. Both focused checks pass: disabled by default, and ordered active replacement without skipping missing numbers. Replacement changes only controller-program bookkeeping, not actor or save state. These are ordinary controller continuations, not save states.

The current Strict reports zero unresolved symbols; all74 port checks pass. Strict SHA-256 `e07fb4ee12416b026e0cd43c52a625637fb8ec7d502154bbced78f137f75f4a2`.

## Stationary defense stance and trajectory

The opt-in stationary guard needs an ordinary R-click aiming stance before analog spray. `TMario::canSquat` refuses that transition while FLUDD is already emitting; the controller now releases analog R until Mario enters the stance. Its previous downward pitch also aimed a flat-floor jet into the ground before a nearby slug. The input feedback now holds a slight upward nozzle pitch instead. Only virtual controller inputs change; no gameplay state is written.

In `bt36_bianco5_stance`, live controller chunk2 resolves two nearby `TNameKuri` without damage: health6 remains unchanged and water falls from9162 to9120. Read-only inspection identifies both enemies as `TNerveNKFollowMario` before the section and `TNerveSmallEnemyDie` afterwards. This is a bounded controller-defense check, not a Petey completion or a full-route PASS. The earlier `bt34_bianco5_roof` stationary defense exhausted its remaining tank and ended in normal enemy damage/death; no crash or original-game UB is inferred.

The corrected Strict has zero unresolved symbols and all74 port checks pass. Strict SHA-256 `ea8c486f8a6b25ee640e79e874c6b569864be746b0a38db966074f0cd3bcc049`.

## Partial-trigger walking inputs

`walkto ... spray=1 pressure=64` now expresses an ordinary light R-trigger press while steering. `pressure` accepts decimal raw PAD values0..255 and defaults to255, preserving existing routes. It changes only the virtual trigger; Hover/Rocket sequences can retain their full trigger input.

Strict links with zero unresolved symbols, all74 port checks pass, and six focused parser cases cover accepted limits, negative/out-of-range values, fractional/empty values and overflow. In `bt37_bianco5_partial_r`, the connected normal approach reaches the Poink plateau at tick5478 with health7 and water6286; the subsequent charged shot wakes Petey. This validates partial-trigger steering, not Episode5 completion. Strict SHA-256 `2beda12192b1ff36b5915bcea8b785ad03223bf2c22040664e31ec47035312f2`.

## Rematch controller components

The ordinary-input continuations in `bt41_bianco5_battle` establish two complete mouth-fill/navel-pound components: chunks22/23 reduce Petey from3 to2 HP while Mario remains at3 HP, and chunks28/29 reduce Petey from2 to1 HP while Mario remains at2 HP. Both fills reach250/250. Waiting for `weak=3 facing=60` before the flying shot avoids a subsequent approach from the wrong side. A later continuous movement/shot section also knocks Petey down normally.

These are component observations, not Episode5 completion. The low-health session subsequently ends in normal enemy damage/death before the final pound. Its card still has seventeen Shines and its crash log is empty. Petey's original swing-distance parameter is600; the death observation places Mario about760 units from Petey and a jumping slug42 units from Mario, so a Petey swing is not an established explanation. The next controller calibration preserves more health on the cliff return and checks the initial mouth hit separately. No game-code defect or undefined behavior is inferred.

## Completed Episode5 milestone

`bt46_bianco5_battle` normally loads the seventeen-Shine `bt17_bianco4_full` card, enters Episode5, collects the roof Rocket and a normal healing blue coin, wakes sleeping Petey with a charged Poink and returns to the village. The completed fight uses the northern spray position near `(-3037,3545)` and a separate eastern mouth approach near `(-2050,4780)`. Three full250/250 mouth fills in controller chunks10/16/22 and navel pounds in11/17/23 reduce Petey3→2→1→0; Mario stays at3 HP throughout those three attack chains. The narrower eastern flying-shot approach was unreliable and is not the accepted sequence.

The reward moves to its original authored position `(-5300,2800,-1000)`. Ordinary movement, Hover and a normal jump collect Shine4; normal save confirmation returns to controllable Delfino1/8. Chunk30 ends at tick54875. File B sector3/write26 contains eighteen Shine identities: `0,1,2,3,4,10,20,30,31,32,33,34,86,94,95,98,107,117`; GCI SHA-256 `99a8af03a336d61e443c1b63a443fd1ba1207595e8336b596358c1bf0c616fb1`. All seven sector checksums pass, the crash log is empty and no cheats are active. Reward travel drops Mario to1 HP before the Shine; this is not a damage-free approach claim.

The fresh `bt47_turbo6_entry` process normally loads File B, asserts Shine4 and the previous saved rights, and enters Episode6 (`stage2/scen5`) with18 Shines and8 HP. Its GCI is byte-identical to the saved Episode5 card, establishing cold persistence. Both use Strict SHA-256 `2beda12192b1ff36b5915bcea8b785ad03223bf2c22040664e31ec47035312f2`, whose build reports zero unresolved symbols and passes all74 port checks.

Episode5 is a completed segmented controller-input session, with ordinary START pauses and read-only checkpoints. The process was deliberately stopped after the verified saved Delfino return. It is not an unattended full-route rc0 or repeatability claim. The frozen private controller files and hashes are recorded in `build/bt46-proof/summary.json`; no game actor or progress state was written by the driver.

## Completed Episode6 Turbo milestone

The fresh `bt47_turbo6_entry` process continues from the saved eighteen-Shine card. Normal jumps/Hover board the original rotten leaf boats, and ordinary partial R spray propels them through the eastern channel into the northern bay. Two boat transfers are exercised; the second overshoots into poisoned water, followed by an ordinary surface jump/Hover recovery onto the next leaf. Health8 falls to6 during that recovery. A real spin jump (`status0x00000895`, y2909 at tick42949) and Hover reach the original y2800 cave shore with health6. The six Episode4/5 entrance stakes are absent in Episode6; no collision or episode gate is bypassed.

An ordinary jump opens the valid Turbo box at `(9198.1,2800,-23109.9)`. Walking into the thrown item selects nozzle5, and normal confirmation saves the Bianco Turbo right `0x10369`. File B sector4/write27 retains the same eighteen Shines, Rocket and Yoshi, adds Turbo, and passes all seven sector checksums. GCI SHA-256 `808befd1a9acacdc5115262c10ab8ce41d803334a15590e2e0bb72aedde7e52e`.

Controller chunk14 faces open water and holds full analog R for90 ticks. Trace observes charge state0 followed by emitting state1, water falling from2100 to2014, and peak forward speed85.6 at tick47504 with selected nozzle5 (`status0x0000088C`). Mario moves from about `(9224,2800,-22738)` to `(9228,2293.5,-21105)`; water refills upon reaching the lake. The input section ends paused with health5 and an empty crash log. This proves one bounded charge/use, including movement and fuel consumption; it does not certify every Turbo collision, door, race or global Delfino nozzle unlock. The trace's `trig` fields are nozzle trigger state/timer, not raw PAD trigger values.

`bt48_cold_turbo` runs the published `tools/routes/pc_bianco_turbo_reload.dev` in a separate process with default exit-on-END. It normally loads File B, asserts all eighteen Shine identities, the previous world rights, Yoshi and both Bianco nozzle rights, and reaches controllable Delfino1/8: rc0 /51 seconds, done tick1393, health8, cheats inactive and an empty crash log. Its GCI is byte-identical to the Turbo save and all seven sector checksums pass. Strict SHA-256 remains `2beda12192b1ff36b5915bcea8b785ad03223bf2c22040664e31ec47035312f2`; no original game-code change is needed for this evidence wave.

From `sunshine-pc`, the final cold check is reproducible with an existing earned profile:

```powershell
python tools/route.py Strict tools/routes/pc_bianco_turbo_reload.dev --name <cold> --appdata-from bt47_turbo6_entry --seconds 240
```

The Episode6 traversal is a segmented normal-controller session with read-only checkpoints and ordinary pauses, not an unattended full-route rc0 or repeatability claim. Private controller hashes and the compact use summary are frozen in `build/bt47-proof/summary.json`. The final cold script is a standalone rc0 check.

## Remaining scope

Bianco4/5 and the bounded Rocket/Turbo acquisition/use/save/cold milestones are complete. Episode6's secret-stage Shine, later missions, global Delfino nozzle unlocks and the remaining playability gates stay open. The broader playability matrix remains17 passed-bounded /8 open /0 external-check-required. Mirrors remain deferred; no release is published.

Private logs, screenshots, disc data and GCI files are not distributed. Native routes are muted and use the CPU adapter at render scale0.25; they do not certify audio or full rendering quality.
