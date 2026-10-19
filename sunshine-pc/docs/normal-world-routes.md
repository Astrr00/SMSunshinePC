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

## Normal crane transfer and Gooper Blooper arena entry

`nw50_blooper_awake` passes the connected approach through the original container-arm event: rc0/224 seconds, `done at tick=6307`, empty crash log, health8 and active-cheat counts zero.
The accepted `nw50` command sequence ended with the awakened boss; the public route has since been extended by the full completion below.
It preserves the central crossing, waits for the real crane state2/cargoY<=-600, boards the moving cargo and rides it above MarioY1350 before jumping to the high quay.
The cargo observations and Gesso HP2/mode6/held0 checks are exercised natively. Navigation around the crane support retains the existing controller-only stall recovery; no collision or game-state bypass is added.

A neutral 30-tick interval after L lets Mario leave squat/holding-pump before B. In `nw50`, the one-tick B press already takes the tentacle at tick5803; the following forward-B wait sees held1 immediately.
Backward B releases at tick5868 after 65 ticks and triggers the original streaming-movie/transition to raw59/scenario0.
Normal control resumes at tick6187; the final boss has HP2/mode0, Mario is at (10500,1500,2400), and the original beak position is about (12351.1,1464.1,2233.5).
The extra cargo-to-quay waypoint in failed `nw48` leads over the edge; it is absent from the accepted route. Earlier immediate B attempts remain in squat and time out normally. All crash logs are empty.

Gesso's read-only snapshot now also reports its original face-ink alpha; the initial value is230. No targeting, collision, actor state or gameplay resource is changed.
Strict reports 0 unresolved symbols and all 68 port checks pass after this logging-only change. The earlier 25 native parser cases cover the unchanged observation parser.
Executable SHA256: `5aebf349eedf4c4b4018cf56edad023a24947854190c55b90a1bf2216f9bb6b8`.
The accepted run GCI is still byte-identical to the earned-unlock source (`74c6589a03d19c7d82e18e8b8f28671b8dcfc320f86cec264efa8b1efba6e8f2`); this approach creates no saved Shine.

```powershell
python tools/route.py Strict tools/routes/pc_ricco_blooper_unassisted.dev --name <unique> --appdata-from nw27_ricco_entry --seconds 1000
```

This entry-only result did not certify boss defeat or a saved Shine. The following completion supersedes that boundary; broad repeatability remains open and matrix statuses stay at 16/25.

## Complete normal Gooper Blooper defeat, Shine save and cold reload

`nw55_blooper_complete` passes the complete connected Ricco episode1 mission on Strict `/O2`: rc0/310 seconds, `done at tick=8716`, empty crash log, health8 and active-cheat counts zero.
It starts from the unchanged earned Ricco-unlock `nw27_ricco_entry` profile, loads File B through ordinary menus/movies, crosses the central platform, rides the crane, pulls the container tentacle and follows the original raw3/0 -> raw59/0 arena transition.
The public [pc_ricco_blooper_unassisted.dev](../tools/routes/pc_ricco_blooper_unassisted.dev) now includes the tested fight, collection, normal save confirmation and controllable Delfino1/5 return; the earlier `nw50` awakening-only boundary is superseded by this result.

The original JP `/enemy/bossgesso.prm` supplies a beak-damage length of 800, an automatic-release limit of 1100, and a dirty-face limit of 500; the constructor's default damage length 600 is not the loaded value.
The read-only `gessopull=1` observation requires a held beak and compares its actual root distance with the loaded original damage threshold. It only ends an ordinary B hold; two neutral ticks then release B through the game's normal pulling handler.
The strict boolean parser rejects values outside 0/1 and fractional values; the Gesso snapshot reports actual pull/required distances.
The successful sequence washes from the safer (11750,2230) stance for 80 ticks, releases L/R for 30 ticks, approaches (12150,2230), takes with B and pulls backward until that threshold.
Both pulls reach the observed release condition after 66 ticks: HP2 -> 1 at tick6706, then HP1 -> 0 at tick7199. A 240-tick interval follows the first cork-damage/pollution sequence; the second wash clears the original renewed face ink.

`nw51` waits for automatic release and is interrupted by an arm. `nw52` releases before the loaded 800 threshold. `nw53` takes damage while washing too close and cannot grab during invincibility. `nw54` still reaches the arm hit before its fixed-duration release.
All have empty crash logs. The accepted route shortens the wash from 180 to 80 ticks, takes the beak farther out and observes the real damage threshold; no game physics, collision, actor HP, resources or progress state is changed.
No teleport, direct aim, gameplay cheat, dev start, resource/state write, seed, movie skip or timing override is used. `SUNSHINE_DEV_HPLOG=1` is read-only telemetry.

The original death/reward camera runs, the route collects Shine-ID10 (flag0x1000a) at tick8153, then confirms the normal save and returns with four Shines at tick8656.
Independent GCI validation passes every sector checksum: latest File B sector4/write5 has exactly IDs `[0, 1, 10, 86]`.
Saved GCI SHA256: `c227fb7200e0e7a30b7a5f253a66eaba3028f1b37ba70bb57f53d357d2772b53`.
The source GCI remains byte-identical (`74c6589a03d19c7d82e18e8b8f28671b8dcfc320f86cec264efa8b1efba6e8f2`).

`nw56_cold_blooper` independently passes a fresh ordinary menu/movie load: rc0/49 seconds, `done at tick=1333`, four Shines, flags0x10001/0x10386/0x1000a, health8, normal Delfino1/5 control, empty crash log and no active cheats.
Public cold proof: [pc_ricco_blooper_reload.dev](../tools/routes/pc_ricco_blooper_reload.dev). Both accepted runs use executable SHA256 `b18fed7651cc60b9998e7e027d78a44859e67bcfd7744e9aa54282437d78f599`.
Strict reports 0 unresolved symbols; all 68 port checks and all 28 native parser cases pass on this build.

```powershell
python tools/route.py Strict tools/routes/pc_ricco_blooper_unassisted.dev --name <unique> --appdata-from nw27_ricco_entry --seconds 1500 SUNSHINE_DEV_HPLOG=1
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_ricco_blooper_reload.dev --name <cold-unique> --appdata-from <unique> --seconds 240
```

This certifies one complete normal Ricco/Gooper mission, its saved fourth Shine and an independent cold reload. Broad repeatability, hardware playthrough and the other later worlds' normal mission completions remain open.
The 91 diagnostic loading rows retain their own recorded executable hashes; matrix statuses remain 16/25 and are not recertified by this route.


## Normal fifth-Shine prerequisite for Gelato

Original `MarDirectorSetupObjects.cpp` enables the Mamma/Gelato Gatekeeper event at five Shines, after Ricco's three-Shine threshold. The clean Gooper profile has four; no progress flag or save is edited to open the next world.
`nw64_fifth_shine` passes from unchanged `nw55_blooper_complete` through the normal File B menu and movies: rc0/153 seconds, done at tick4311, health8, no active cheats and an empty crash log.
[pc_delfino_beach_shine.dev](../tools/routes/pc_delfino_beach_shine.dev) washes the original beach drawing at (9024,-5097) with ordinary hover spray. Its normal reward camera carries Shine117 to the offshore pillar at (17380.5,839.9,-9340.3).
The route swims to the low pillar, uses hover/ledge climb, jumps across two pillars, walks under the reward and collects it with A. The original save confirmation returns Mario to controllable Delfino1/5 with five Shines.

The loaded `/mario/hhover.prm` brake is0.985, rather than the constructor default0.95. Neutral flight alone overshoots the middle pillar; a short ordinary backward-stick phase followed by neutral hover lands on it. This changes only controller input.
`nw57` times out against a plaza building; `nw58` reveals the reward but tries to collect it at the drawing. `nw59` and `nw61` were explicitly stopped prototypes. `nw60`/`nw62` overshoot the middle pillar; `nw63` lands both but jumps about120 units short of the Shine. None has a nonempty crash log. The accepted route moves under the reward before jumping; no engine behavior is bypassed.

GCI header and all sector checksums pass. Latest File B sector3/write6 contains exactly IDs `[0, 1, 10, 86, 117]`; SHA256 `c1832412d7e7ed55175c528dfd39eb137e17500e0dbf78971de00a35bf88ed02`.
The unchanged executable SHA256 is `b18fed7651cc60b9998e7e027d78a44859e67bcfd7744e9aa54282437d78f599`. The fresh normal-menu/movie load in `nw65_gelato_entry` independently observes all five Shines and flag0x10075 before its approach trial. This is a saved prerequisite, not Gelato entry or mission completion.

```powershell
python tools/route.py Strict tools/routes/pc_delfino_beach_shine.dev --name <unique> --appdata-from nw55_blooper_complete --seconds 800
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
```


## Normal Gelato unlock, save and episode1 entry

`nw66_gelato_hover_entry` passes from the clean five-Shine `nw64_fifth_shine` seed: rc0/249 seconds, done at tick7041, empty crash log and no active cheats.
[pc_gelato_entry_unassisted.dev](../tools/routes/pc_gelato_entry_unassisted.dev) follows ordinary menus/movies, swims around the plaza houses and uses a hover jump over the beach wall into the raised plaza. The earlier `nw65` walking-only approach times out at that wall with no crash.
The normal Mamma Gatekeeper at (9990,330,-7590) takes three throat hits in each of its two original phases. The reused controller defense and animation-frame aiming defeat both; Mario has health3 at the saved unlock, so this is not a damage-free or broad repeatability certificate.
Flag0x10387 becomes true at tick5332. The route waits for the real finish camera, uses Pause > Save > Yes > Continue, sprays the normal west-facing JellyGate, jumps into it and chooses episode1 through the selector. Normal raw4/scenario0 control starts at tick6921; entry restores health8.

The saved File B copy passes the header and every sector checksum. Latest sector4/write7 still has exactly Shine IDs `[0, 1, 10, 86, 117]`; its additional native world-unlock state produces GCI SHA256 `efaa86da54279feec25d4f1af77b3482064c081029dfaee50f6ef8b21e9fbda6`.
The original five-Shine source remains unchanged. Executable SHA256 is `b18fed7651cc60b9998e7e027d78a44859e67bcfd7744e9aa54282437d78f599`; a repeated Strict build reports 0 unresolved symbols and all68 port checks pass (`nw66-strict.log`, `nw66-tests.log`).

```powershell
python tools/route.py Strict tools/routes/pc_gelato_entry_unassisted.dev --name <unique> --appdata-from nw64_fifth_shine --seconds 1200 SUNSHINE_DEV_HIT_LOG=1
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
```

This certifies normal Gelato entry and a saved entrance unlock, not a Gelato mission reward or Wiggler defeat. Those remain open. The independent cold run `nw67_gelato_castle` observes saved flag0x10387 and re-enters its normal portal from Delfino1/6 with health8. The normal next Delfino scenario is6 after both Ricco and Mamma unlock flags are saved; future cold routes must allow that original progression.


## Normal Gelato sandcastle entry and middle approach

`nw77_gelato_ledge_jump` passes a bounded approach from the unchanged saved-unlock `nw66_gelato_hover_entry` profile: rc0/212 seconds, done at tick6035, health8, empty crash log and zero active-cheat counts.
[pc_gelato_sandcastle_approach.dev](../tools/routes/pc_gelato_sandcastle_approach.dev) loads File B normally, follows the saved portal through the episode1 selector, waters the original sandcastle sprout and enters raw32/0 through its live entrance. The normal Shadow Mario movie takes FLUDD; no dev start or gameplay-state write is used.
The route crosses the first sand-block corridor, lands on the wooden block and climbs the second corridor to the stable (about6995,4150,200) middle platform.

The first wide gaps need running momentum and an initial twelve-tick A hold. Neutral ground waits brake Mario before takeoff. For the rising corridor, `jump=12 hop=8` keeps the initial normal jump and then repeats ordinary A presses; this chooses the faster original ledge pull-up before a crumbling block drops too far.
L is the normal ground-pound button; Z opens the guide map. Early prototypes using Z as a brake were route failures, not crashes. No jump physics or extra input option is changed in the accepted build.

The saved GCI remains byte-identical to the source: latest File B sector4/write7, IDs `[0, 1, 10, 86, 117]`, SHA256 `efaa86da54279feec25d4f1af77b3482064c081029dfaee50f6ef8b21e9fbda6`; the header and every sector checksum pass.
Accepted executable SHA256: `2cb29905a29c3186735cadd65520a53c4e32d9c1f78daeafd6c5b73b5b3b80c5`. Strict reports 0 unresolved symbols and all68 port checks pass (`nw74-strict.log`, `nw74-tests.log`).

```powershell
python tools/route.py Strict tools/routes/pc_gelato_sandcastle_approach.dev --name <unique> --appdata-from nw66_gelato_hover_entry --seconds 1000
```

This certifies the connected secret entrance and its lower/middle approach only. The upper crossing, tower, Shine20 reward/save/cold reload and Gelato's boss milestone remain open; matrix statuses stay16/25.


## Normal Gelato sandcastle reward, save and cold reload

`nw91_gelato_early_takeoff` completes the first Gelato mission from the earned five-Shine saved-unlock profile `nw66_gelato_hover_entry`: rc0/263 seconds, done at tick7469, health8 on return to Delfino1/6, an empty crash log and zero active-cheat counts. It enters the normal portal, selects episode1, waters the original sandcastle trigger, enters raw32/0 through its entrance and plays the original FLUDD-theft sequence. All traversal uses ordinary stick, A and L inputs; no position, aim, progress, resource, save or timing override is used.

The connected curved rows are walked on the ground so Mario can turn. Running jumps use `jump=12 hop=8`, including the normal A ledge pull-up. L stops air drift over the stable NormalBlock and WoodBlock landings. After the final sand ledge, one straight run-up step supplies a full held-A jump early enough to meet the last wall while descending; the original `hangonCheck` rejects upward ledge grabs. Earlier attempts show wall clashes, slow ledge climbs and falling sand blocks, with empty crash logs; they do not establish an engine hang or UB.

Shine-ID20 is collected, the normal save dialog completes and the route returns to Delfino. Latest File B is sector3/write8 with exactly IDs0/1/10/20/86/117. All GCI checksums pass; SHA256 is `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`. The five-Shine source remains byte-identical (`efaa86da54279feec25d4f1af77b3482064c081029dfaee50f6ef8b21e9fbda6`).

`nw92_cold_gelato_six` independently cold-loads File B through the normal menus and verifies the saved Gelato unlock, Shine20 flag0x10014 and all earlier earned milestone flags: rc0/50 seconds, done at tick1363, six Shines, health8, zero active cheats and an empty crash log. Its GCI remains byte-identical to `nw91`.

Both runs use Strict executable SHA256 `2cb29905a29c3186735cadd65520a53c4e32d9c1f78daeafd6c5b73b5b3b80c5`; the associated Strict build reports 0 unresolved symbols and all68 port checks pass (`nw74-strict.log`, `nw74-tests.log`). The public route command sequences exactly match the native-tested scripts; only comments differ.

```powershell
python tools/route.py Strict tools/routes/pc_gelato_sandcastle_unassisted.dev --name <unique> --appdata-from nw66_gelato_hover_entry --seconds 1200
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_gelato_sandcastle_reload.dev --name <cold-unique> --appdata-from <unique> --seconds 240
```

This certifies one complete normal first-mission pass plus its independent persisted-reward reload, not consecutive full-route repeatability. The mirror mission (Shine21), Wiggler (Shine22) and their saved/cold proofs remain open. Matrix statuses stay16/25.
