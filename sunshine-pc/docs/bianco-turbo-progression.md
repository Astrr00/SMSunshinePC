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

## Open acceptance

Episode5 completion and saved Shine4, normal Episode6 Turbo acquisition/use, saved Turbo right and the final fresh-process reload remain pending. A route draft, one knockdown or resource placement is not their PASS. The broader playability matrix remains17 passed-bounded / 8 open / 0 external-check-required. Mirrors remain deferred; no release is published.

Private logs, screenshots, disc data and GCI files are not distributed. Native routes are muted and use the CPU adapter at render scale0.25; they do not certify audio or full rendering quality.
