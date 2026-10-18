# Normal connected world routes

Later-world diagnostic load coverage is recorded separately in [world-load-coverage.tsv](world-load-coverage.tsv).
These controller trials must earn their prerequisites through normal play. No dev start, teleport, direct boss aim, gameplay cheats, movie skip, state writes or timing overrides are accepted.

## Starting progress

The preserved `pg11_ramp_spray24k` and `pg15_cold_o2` profiles contain a third Shine from an `infinite_health` diagnostic run.
Their successful cold reloads prove persistence of that checkpoint, not an unassisted Petey victory or a valid prerequisite for normal Ricco unlocking.

The clean two-Shine profile was recovered from `build/BiancoNormalPath_20261016/run1/appdata`, matching the original source-card metadata exactly.
The owned copy is `build/Route/nw_earned_two_20261018/appdata`; its GCI SHA256 is `b16fc7f868f8a63dace6ca99a72cb47c2d7c1e767d71a8cdf3640f148f796c94`.
All seven GCI checks pass. The latest File B sector has Shine IDs 0 and 86, write count 2. Originals remain unchanged.

## Petey trial evidence (2026-10-18)

`nw03_petey_clean` loads File B through normal menus and movies, enters Bianco STORY2 through the unlocked Delfino portal, climbs the windmill and follows the original roof-fall transition to raw stage55.
Normal controller attacks reduce Petey from HP3 to HP0. The decisive log is `goto round ... petey hp=0 weak=0 health=5 tick=20805`; the Shine appears visibly.
The route then incorrectly restarts mouth targeting after an enemy-defense interruption. It is stopped without collecting the Shine, saving or returning to Delfino.
This proves the connected approach and boss defeat in one trial, but is **not** a complete route PASS or a three-Shine checkpoint. The crash log is empty and logged active-cheat counts stay zero.

Commit `364bb32` adds bounded `goto LABEL if ... max=N` controller-script branching, so a completed boss phase can lead to collection and an already-filled belly can lead to the ground pound.
It writes no gameplay state. Strict reports 0 unresolved symbols, all 64 host checks pass, and eight native parser cases pass, including taken/fallthrough conditions and the existing `unless` syntax.
Its Strict executable SHA256 is `5773950950931a98ef750c2b60067d9813f25c7991483ee998d32d190c9e3d60`.

`nw04_petey_clean` tests this revised script but loses all health on the polluted ramp before reaching Petey. Normal death returns to Delfino with two Shines.
The obsolete script continues Bianco coordinates in Delfino; the owned process is stopped. The crash log is empty. This is a navigation/survival failure, not evidence of an engine hang or optimization-related UB.
Shorter washes and hover-only landings do not stabilize the slope. The final candidate repeats ordinary A presses while steering and spraying, using `walkto hop=24`; no game physics or state is changed. The existing defense also uses the original oil-slip A jump to clear momentum.

## Complete earned third-Shine route

`nw11_petey_clean` passes: rc0, 483 seconds, `done at tick=13525`, empty crash log, active-cheat count zero throughout.
It loads the exact clean two-Shine seed, enters Bianco STORY2 normally, climbs the ramp/bridge/windmill, follows the roof-fall intro, defeats Petey, collects flag 0x10001, confirms the normal save prompt and returns to controllable Delfino 1/5 with three Shines.
The public route [pc_bianco_petey_unassisted.dev](../tools/routes/pc_bianco_petey_unassisted.dev) has all 412 tested controller commands unchanged; only obsolete private comments were removed.
The accepted route executable SHA256 is `c6fdc906041360d194b56eb8c392f29ac6e794cd5ade8446b5e60bc4bbed985c`.

Independent GCI verification passes every check. The latest File B sector is 4, write count 3, Shine IDs `[0, 1, 86]`.
Saved GCI SHA256: `f90a95a1b571817a264abaf05000a92365885eac75d7be7fe4cb3340f44296fc`; the clean source GCI remains unchanged.
`nw12_cold_three` loads this saved File B in a fresh process through ordinary menus/movies and observes three Shines plus flag 0x10001 in controllable Delfino 1/5: rc0, 48 seconds, `done at tick=1333`, empty crash log, no active cheats.
This separately verifies the earned prerequisite on the final helper build, SHA256 `1780d3d4b7d5b3bc5ff9bb6744cb6a2c298ed792c2b13ece939f77ebe27271ae`.

The final helper parses hop periods as bounded integers; zero, fractions, NaN and overflow are rejected. Strict has 0 unresolved symbols, all 64 host checks and all 13 native parser cases pass.
The Gatekeeper observer now selects the nearest named Bianco/Ricco/Mamma actor, retaining defeated actors through their event; Ricco/Mamma animation observations read that actor instead of the last global debug writer. The cold route observes the actual Ricco actor at (-10511,300,1953). Combat with that observer is the next live check; original Bianco animation observation keeps its existing behavior.

Reproduce in `sunshine-pc`:

```powershell
python tools/route.py Strict tools/routes/pc_bianco_petey_unassisted.dev --name <unique> --appdata-from nw_earned_two_20261018 --seconds 1200
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_bianco_petey_reload.dev --name <cold-unique> --appdata-from <unique> --seconds 240
```

The accepted normal route is one bounded pass; broad repeatability and later-world connected entry/completion remain open. Script `_Exit` does not certify native Close. Matrix statuses remain unchanged.


## Ricco Gatekeeper milestone, entry still open

`nw13_ricco_wake` follows the normal earned-three-Shine cold load, crosses the west boardwalk with A/hover inputs and wakes the Ricco Gatekeeper: rc0/81 seconds, empty crash log, health 8.
`nw19_ricco_entry` then defeats both original three-HP phases using normal controller inputs on the same final helper build used by the cold-three-Shine check above.
Phase one reaches HP0 at tick3208, resets naturally to HP3, and phase two reaches HP0 at tick5104; the boss is flagged dead at tick5704 and the original event sets flag 0x10386.
The route retains health 3 and reaches the rebuilt house, but its final portal entry wait fails: the overall trial is rc3/274 seconds, **not** a full entry PASS. Its crash log is empty.
The saved GCI is unchanged from the earned-three-Shine source, so this run does not certify a persisted Ricco unlock. No unlocked checkpoint may be inferred from its live flag.

The boss continually registers its pollution stamp while active. Ricco uses scale 1.2, compared with the Bianco gatekeeper's 0.8; copying the close Bianco stance leaves Mario in recurring goop.
The tested controller sequence wakes nearby, then retreats to a dry outer stance for the throat shots; frame-window observations now follow the Ricco actor rather than the distant Mamma actor.
`SUNSHINE_DEV_HIT_LOG=1` only reports counters; active gameplay-cheat counts remain zero. No direct aim or gameplay-state writes are used.

A read-only Ricco portal snapshot now records its flags, state, open timer/ratio, center joint, Mario's local position and the actual entry box.
The native `nw20_ricco_entry` logs finite observations with destination 1 and center approximately (-9947.3,612.1,2002.9), then fails after falling into the water while re-waking; its crash log is empty.
Strict has 0 unresolved symbols, all 64 host checks and all 13 native parser cases pass after adding the snapshot.
Portal-observer executable SHA256: `716b82f10d82317fc5b88393264d991f3a59e0cdd1f2bd93df4f8864cafdeed3`.

The next candidate waits for the original sleep animation before moving in, uses a safer wake point, attempts normal Pause > Save > Yes > Continue, and adjusts the portal spray/jump inputs.
Normal Ricco selector/entry, persisted unlock and Blooper completion remain open. None of these controller failures establishes an engine hang or optimization-related UB.


## Controller replay corrections

The steering helper now uses the camera input yaw consumed by `TMario::controllerWork` (`getUnk258()`), rather than deriving its basis from the rendered camera target.
Its pure input conversion also compensates PADClamp's 15-unit per-axis deadzone. Host fixtures apply the game-side yaw equation independently across six camera angles and six world directions, including wraparound and a neutral arrival case.

`nw23_ricco_entry` exposes a separate controller stall after entering the water: the enemy-defense helper treats any large distance above the seabed as an airborne braking jump and repeatedly emits neutral-stick R without advancing the route timeout.
The helper now requires Mario's actual jumping flag as well as height above the floor; a compile-time assertion binds that flag to the game enum.
The regression uses the actual jump and swim statuses from the replay. `nw24_ricco_entry` subsequently exits rc3 at the one-tick ground-height check after a failed return, with an empty crash log, rather than remaining in the swimming wait. This corrects the controller driver, not an engine hang.

Strict has 0 unresolved symbols; all 67 host checks and all 13 native parser cases pass. The Ricco route is still being refined; a full entry or persisted unlock is not claimed by these helper tests.
Controller-helper executable SHA256: `650f1bebfae64f25131b44e9f8fd53b447c1646216cbf0bc058f3e5447883db4`.
