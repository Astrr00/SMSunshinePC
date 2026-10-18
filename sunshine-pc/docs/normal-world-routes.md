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


## Earlier Ricco Gatekeeper trials (superseded by the complete route below)

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

## Complete normal Ricco unlocking, save and entry

`nw27_ricco_entry` passes on Strict `/O2`: rc0/275 seconds, `done at tick=7813`, empty crash log and active-cheat counts zero.
It starts from the unchanged earned-three-Shine `nw11_petey_clean` profile, loads File B through ordinary menus/movies, crosses the west boardwalk and defeats both Ricco Gatekeeper phases.
One-second observations record HP3 -> 2 -> 1 -> 0 at ticks570/3300/3540/3780, the natural reset to HP3 at tick3930, then HP2/1/0 at ticks4050/4170/5790 and dead=1 at tick5940.
The original event sets flag0x10386. Mario retains health7 before the save/entry sequence.

The route chooses normal Pause > Save > Yes > Continue, opens the portal by ordinary L/analog-R spray with vertical stick movement, jumps inside its real entry box, reaches raw3/scenario255 and confirms episode1 at raw3/scenario0 with normal control.
The public [pc_ricco_entry_unassisted.dev](../tools/routes/pc_ricco_entry_unassisted.dev) retains all 182 tested commands unchanged.
No teleport, direct aim, gameplay cheat, dev start, state/resource write, seed, movie skip or timing override is used. `SUNSHINE_DEV_HIT_LOG=1` is read-only telemetry.

Independent GCI verification passes: latest File B sector3, write4, earned Shine IDs `[0, 1, 86]`.
Saved GCI SHA256: `74c6589a03d19c7d82e18e8b8f28671b8dcfc320f86cec264efa8b1efba6e8f2`.
The earned-three-Shine source remains byte-identical (`f90a95a1b571817a264abaf05000a92365885eac75d7be7fe4cb3340f148f796c94`).
`nw28_cold_ricco` separately passes a fresh normal menu/movie load: rc0/49 seconds, `done at tick=1333`, three Shines, flags0x10001 and0x10386, controllable Delfino1/5, crash0 and no active cheats.
Public cold route: [pc_ricco_unlock_reload.dev](../tools/routes/pc_ricco_unlock_reload.dev).
Both accepted runs use executable SHA256 `d7f75220d83621f638ecaebbf973a2188a9b07d65194f65ff762c859a8c9477c`.

The replay's small-turn conversion now compensates the second stick deadzone in Mario's controller work; an independent game-side conversion fixture covers 360 headings within three degrees.
The opt-in `guard stationary=1` defends dry flat ground with full-R and normal stick rotation instead of walking toward nearby slugs at a narrow dock edge. Existing walking guards remain the default; oil-slide escape still uses A.
Read-only signed-int16 `pitch>=` / `pitch<=` conditions bracket the actual nozzle angle through L/stick inputs before throat shots, avoiding accumulated fixed-duration pitch drift.
These are controller-driver corrections, not game physics/state changes or engine-hang bypasses.
Strict reports 0 unresolved symbols, all 68 host checks and all 17 native parser cases pass, including invalid stationary/pitch tokens.

Reproduce:

```powershell
python tools/route.py Strict tools/routes/pc_ricco_entry_unassisted.dev --name <unique> --appdata-from nw11_petey_clean --seconds 1000 SUNSHINE_DEV_HIT_LOG=1
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_ricco_unlock_reload.dev --name <cold-unique> --appdata-from <unique> --seconds 240
```

This certifies one complete connected Ricco unlocking/entry and an independent persisted-unlock reload. Broad repeatability, the Ricco mission boss Gooper Blooper, other worlds' normal mission completion, hardware rendering and native Close remain open.
The earlier 91-world load certificate and Bianco Gatekeeper pair remain tied to their recorded executable hashes; they are not silently recertified by this controller change. Matrix statuses stay unchanged at 16/25.

## Normal harbor approach and Gooper Blooper observations

`nw32_blooper_approach` passes the first Ricco harbor approach component: rc0/120 seconds, `done at tick=3364`, empty crash log, health8 and active-cheat counts zero.
It loads the earned Ricco-unlock File B from `nw27_ricco_entry`, enters episode1 through the normal portal/selector, takes the upper quay at (-13000,1000)/(-11100,1100), then jumps and hovers onto the first container.
The final controllable position is (-9411.3,630.3,2356.6). The public [pc_ricco_harbor_approach.dev](../tools/routes/pc_ricco_harbor_approach.dev) retains every tested controller command unchanged.
The source and trial GCI both retain SHA256 `74c6589a03d19c7d82e18e8b8f28671b8dcfc320f86cec264efa8b1efba6e8f2`; no progress or resource file is altered to construct this approach.

The earlier `nw29` trial spends its entire height wait below an unreachable 600-unit target after launching from the lower, 150-high quay.
`nw30` cannot walk through the 150-unit curb to its intended 300-high launch point. `nw31` reaches that point via the upper quay, but uses 50 hover ticks waiting for height before horizontal travel and falls short of the container.
The passing route moves toward the container immediately after the jump, retaining the hover burst for the crossing. All three failed trials have empty crash logs; these are bounded controller navigation failures, not engine hangs or optimization-related UB evidence.

A read-only `logGesso` snapshot follows the actual scene manager name and reports HP, attack mode, dead flag, beak/tentacle positions and held status.
Native observations show Gooper Blooper at (12600,1500,2200), HP2/mode6, with tentacle0 in state9 and its grip near (11035.4,1492.7,2440.0). Inactive beak/other tentacle positions are zero during the original container phase.
The snapshot adds no targeting action or gameplay-state write. Strict has 0 unresolved symbols; all 68 host checks pass.
Accepted executable SHA256: `7e38ce5474889c0f9b36ef58c2793862132fa6570e98f10595a04c7182c75c60`.

```powershell
python tools/route.py Strict tools/routes/pc_ricco_harbor_approach.dev --name <unique> --appdata-from nw27_ricco_entry --seconds 480
```

This is one bounded approach pass. The remaining harbor traversal, container-arm pull, Gooper Blooper defeat/reward/save and broad repeatability remain open; it does not certify the Ricco mission's completion.

## Normal crossing to the central harbor platform

`nw46_blooper_central` passes on Strict `/O2`: rc0/167 seconds, `done at tick=4683`, empty crash log, health8 and active-cheat counts zero.
The public [pc_ricco_central_unassisted.dev](../tools/routes/pc_ricco_central_unassisted.dev) preserves the tested commands, including the ordinary cold File B load and Ricco episode1 entry from `nw27_ricco_entry`.
Mario swims through the clear western channel, jumps/hovers from (750,2750), releases R before the platform wall and uses normal A presses to climb its edge.
The trace records the original ledge/climb states `0x3800034B` and `0x3000054F` at ticks4605/4610; the final position is (3237.5,400.0,3001.7), with normal control.
Both source and trial GCI retain SHA256 `74c6589a03d19c7d82e18e8b8f28671b8dcfc320f86cec264efa8b1efba6e8f2`.
Executable SHA256: `e4f268915c12a2c294a76bf7ae61e35682a3b646f2ccbcfe4cb8115b9c24666e`.

The failed small-ramp trials expose a solid steel pillar above its slippery base; (1100,3400) is inside that pillar, not a free landing target.
Slope jumps use the original `MARIO_STATUS_MISS_JUMP`, which cannot start hover. The alternate open-water attempt `nw44` loses health in polluted water and dies normally; its owned process was stopped, not counted as a PASS or crash.
The accepted crossing avoids both obstacles with ordinary controller inputs. No game physics, collision, resources, progress flags or health are changed.

Read-only replay observations now include the original Ricco crane state/cargo Y, Gooper HP/mode and whether Mario holds an object.
Their strict parser rejects invalid state ranges, nondecimal heights and signed-int16 overflow. Strict has 0 unresolved symbols, all 68 port checks and all 25 native parser cases pass.
These observations do not target actors or change controller inputs by themselves.

```powershell
python tools/route.py Strict tools/routes/pc_ricco_central_unassisted.dev --name <unique> --appdata-from nw27_ricco_entry --seconds 720
```

This is one bounded central-platform crossing. The crane transfer, container-arm pull, boss defeat, Shine collection/save and repeatability remain open; matrix statuses stay at 16/25.
