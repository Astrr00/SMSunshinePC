# Normal Pinna milestone

Normal Pinna entry through the cannon, beach, park, director dialogue and coaster intro is verified. Mecha Bowser completion and its saved/cold-reloaded Shine remain **open**. The normal cannon prerequisite is ten distinct earned Shines plus the Ricco/Gelato unlocks. The following accepted controller-input prerequisites extend the existing six-Shine save; they do not certify Pinna combat. The accepted prerequisite save now contains ten earned Shines, including the normally collected hundred-coin reward107.

## Earned prerequisites (2026-10-19)

Start from `nw91_gelato_early_takeoff`, the normal six-Shine File B documented in [normal-world-routes.md](normal-world-routes.md): IDs0/1/10/20/86/117, GCI SHA-256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.

1. `pc_delfino_crates_first_unassisted.dev`, accepted private `pn12_crate_first`: rc0/149.44s, ten crates broken with 7.35 seconds left, ordinary Shine94 collection/save/plaza return, health8, empty crash log and no active cheats. Latest File B write9 has seven Shines: IDs0/1/10/20/86/94/117. All seven GCI sector checksums pass; SHA-256 `fe7f0b046a47cbea85302ce00c41bfbf9805450014c87536db0796628927edcd`.
2. `pc_delfino_crates_second_unassisted.dev`, accepted private `pn14_crate_second`, seeded from that saved result: rc0/146.35s, ten crates broken with 10.59 seconds left, ordinary Shine95 collection/save/plaza return, health8, empty crash log and no active cheats. Latest File B write10 has eight Shines: IDs0/1/10/20/86/94/95/117. All seven GCI sector checksums pass; SHA-256 `cfcee4186e7e85c998e8fd1831cfcc69f4de588e7aa7db6cff04bc0f03c8105c`.

3. `pc_delfino_chuckster_unassisted.dev`, accepted private `pn24_chuckster` from `pn20_blue_main`: rc0/155.39s, ordinary paid rooftop Pianta throw, Shine98 collection/save/plaza return, health8, empty crash log and no active cheats. The original nine yellow sewer coins fund the one-coin payment (9 to8); the NPC controls the throw. Latest File B write13 has nine Shines: IDs0/1/10/20/86/94/95/98/117. All seven sector checksums pass; GCI SHA-256 `acf58dfc4fe2d9994950a308e00f1ca41f0dc8fa9ae01445a764f6b1196c57a9`. Accepted private route SHA-256 `cce701f07d522a3effc29df4246ae1982c855bb74b563b205715a7d56ee2741f`; public commands match exactly, with comments changed.

4. `pc_delfino_100coins_unassisted.dev`, accepted private `pn42_coins_observed` from `pn24_chuckster`: rc0/771.96s,104 ordinary coins, first observed boat boarding, island landing, Shine107 collection/save and normal plaza return. `done` at tick21965, health8, empty crash log and no active cheats. Latest File B sector3/write14 has ten Shines: IDs0/1/10/20/86/94/95/98/107/117. All seven sector checksums pass; GCI SHA-256 `d7f5ea5640a49d942f78aad98a60eafcc0bcc1bb9e839d3beee0bd7462064241`. Accepted private route SHA-256 `85898888bb46177b84a3f428070052b17b9f13fdf413bb7c997b66842767dcc2`; public executable commands are identical. The fresh-process normal-menu prefix of `pn40_pinna_combat` restores the ten-Shine File B and enters Delfino1/7; its later Pinna traversal is a separate result.

The initial normal-menu section of `pn25_coins` independently cold-loads all nine Shines, including flag0x10062, with the same GCI hash. Its later coin/boat traversal is a separate, unverified result.

The second run cold-loads the first reward through normal menus. The next private roof trial `pn15_roof` independently cold-loads all eight Shines before its unverified traversal; that initial load is evidence for the saved second reward only.

The crate, sewer-blue and paid-throw runs use Strict executable SHA-256 `4bc903e7a254f9782f8e52d561dfc373a5dbc55eb2d74f2b81424ba1407b55af`, built from game source at `99dca5dd` with read-only route diagnostics. Strict reports zero unresolved symbols; 68 port checks, all 46 native parser cases (including five new bell cases) and 37 coaster-aim checks pass. This hash identifies those earlier prerequisite runs; later read-only diagnostics use the separate build below.

Accepted private route hashes: first `2152f18d4f41fa3971b8c23c5a8b5262b6e80cd3ee54c7d89641bcd9b31b0adc`, second `58f550f92cacad22158d52a950b02ac0d227bf9464d0843ed0e44fea1681bf9a`. Public copies change only comments; executable controller commands match the accepted inputs.

## Reproduction

Run from `sunshine-pc`, choosing fresh output names:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_crates_first_unassisted.dev --name <first> --appdata-from nw91_gelato_early_takeoff --seconds 480
python tools/route.py Strict tools/routes/pc_delfino_crates_second_unassisted.dev --name <second> --appdata-from <first> --seconds 360
```

The crate-room approach crosses the canal at x-2500 and enters the street opening at z-5500. A normal circle-stick spin jump is followed by facing the next crate before takeoff. Every layer of the second game's stacked columns receives its own jump/ground pound. Early braking before the reward prevents overshooting its pickup volume.

Earlier private failures remain preserved: canal/door geometry, insufficient jump height, spin-facing errors, collection overshoot and the second layout's unbroken lower layers. They timed out without a nonempty crash log. No undefined-behavior cause is asserted. These routes write only ordinary PAD inputs; conditions, actor lookup and logs observe state. No teleport, developer stage start, automatic boss aiming, cheat, save/progress edit or movie/timing override is used. Quarter-scale muted WARP runs are functional evidence, not image-quality, listening or physical-controller checks.

## Bounded sewer blue-coin route

`pc_delfino_sewer_blues_unassisted.dev` is accepted as private `pn20_blue_main` from `pn14_crate_second`: rc0/162.40s, flags0x10089/0x10088 through ordinary collection/save prompts, normal sewer-cover exit, health8, empty crash log and no active cheats. The two blue coins do not increase the Shine count. Latest File B write12 retains all eight Shines; all seven sector checksums pass, GCI SHA-256 `b5af285dbdab4015840ebec0b61d8e5e78d052ae247328c256f1db0dfad4425a`. Accepted private route SHA-256 `8488a532492b500d59343de65a9a2fd8cd67956a822855a76f398af1b786923b`. Its public executable commands are identical; comments differ. The initial normal-menu section of private `pn21_chuckster` independently cold-loads both blue flags and the unchanged eight-Shine GCI before its later, unsuccessful NPC-talk attempt. That subsequent failure does not certify the roof reward.

```powershell
python tools/route.py Strict tools/routes/pc_delfino_sewer_blues_unassisted.dev --name <blues> --appdata-from <second> --seconds 600
```

## Paid tower throw

The original Aloha Pianta stands near(-3621,2272,-3734), fixed yaw124. His ordinary talk script takes one yellow coin and schedules the original throw toward the tower window and Shine98. This uses the game's NPC interaction; the route never sets a throw vector or actor velocity.

The controller route reuses the west sewer's original spring floor and a Hover flight to the east eave. Three neutral frames before a fresh L press let the original Hover-fall transition finish, so ground pound actually brakes the flight. A second normal jump/Hover reaches the crate-house roof. Primary nozzle is selected before a wide approach around the Pianta's collision cylinder; a brief planted spray brakes Mario, and ordinary B/A starts and accepts the paid talk.

Earlier direct-window and talk attempts remain preserved as `pn15_roof`, `pn16_roof`, `pn18_roof_stop`, `pn19_roof`, `pn21_chuckster`, `pn22_chuckster` and `pn23_chuckster`. They establish flight/stop behavior or fail their later checks; they are not Shine-route passes. Source and actual logs explain the sloped-roof momentum, collider separation, three-dimensional talk cone, WAIT-state requirement and window-lintel contact. No game-code correction or undefined-behavior claim is made.

```powershell
python tools/route.py Strict tools/routes/pc_delfino_chuckster_unassisted.dev --name <throw> --appdata-from <blues> --seconds 600
```

## Boat approach component

The original hundred-coin Shine camera pauses the boat movement list. Consequently, a fixed departure schedule from the shorter boat probe does not preserve its phase after the complete coin route. Private `pn38_coins_boat` reaches104 coins but misses the boat repeatedly; its approximately644-tick retries nearly alias the boat circuit. It was closed normally after872.18s, with health8 and an empty crash log. Despite rc0, it has no `done` marker and is **not a passing route**. No Shine107 save is established by that run.

Read-only `boatdist<=N` and `boatz>=N`/`boatz<=N` conditions now observe the nearest live typed Delfino boat. Distance is horizontal distance from Mario; Z is the signed boat-minus-Mario coordinate. Missing actors and other scenes always fail the conditions. The route faces and settles the camera on shore, then waits for an approaching boat within1500 units and relative Z900..1250 before applying the ordinary circle-stick/jump/Hover sequence. Neither boat nor Mario state is changed by these observations.

Private `pn41_boat_observed`, seeded from `pn24_chuckster`, passes the first observed boarding attempt and the subsequent normal island landing: rc0/278.49s, `done` at tick7838, health8, floor801.9, empty crash log and no active cheats. This component does not collect a Shine. Route SHA-256: `f97495976b41f231703b6786497b44bd24d9a6290ceb77db5aa598120b2557d7`. Its final spin presses A with the fifth small nonzero stick input; a neutral A input would clear the original angle history before jump selection.

This component uses Strict executable SHA-256 `a24e73a61055538b2cade5011747a1bf54ace0c4135f4b796ecf4079576ddaa8`: zero unresolved symbols and68 port checks pass. All95 native parser cases pass on this executable:23 new boat cases plus the72 existing cases, in separate targeted invocations. The coaster observations expose only rail frame, carried-rocket presence and boss state. The native calibration and remaining combat limit are recorded below.

Reproduce the complete hundred-coin reward after the paid-throw save:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_100coins_unassisted.dev --name <hundred> --appdata-from <throw> --seconds 1300
```

## Normal Pinna entry and first combat calibration

Private `pn40_pinna_combat` starts a fresh process from `pn42_coins_observed` and confirms the ten-Shine File B in Delfino1/7. Ordinary movement triggers the original kidnapping movie/cannon, episode selector, Pinna beach5/0 and park13/0. Movie7 and the original director dialogue lead to coaster58/1. The route observes the complete intro camera before testing individual controller axes. Measured rail progression is2 BCK frames per PAD tick; first-person mode is enabled after the intro, and the initial rocket is acquired normally. Full positive-X input changes yaw by-924 per PAD tick; negative-X changes it by+920. No Y-button mode toggle is needed.

The first combat candidate exits3 after288.49s at its required two-broken-parts check, with health8 and an empty crash log. Both normal R launches occur, but boss state remains0/partsHP1. Its original second trajectory passes above the boss. The first measured ray reproduces an original static-ground intersection at triangle7076 near rail1315: BG0x400 is passable for Mario but remains solid for an enemy rocket. This explains a valid collision within the observed disappearance interval; the exact native kill target was not logged. No engine defect or undefined-behavior cause is asserted.

The corrected private candidate uses fixed controller pitches computed from the authored chest, FLUDD and nozzle transforms, including the roll-dependent FLUDD base tilt omitted from the earlier estimate. Mecha Bowser hits, reward pickup, saving and the final cold reload still require native acceptance.
