# Normal Pinna milestone

Latest bounded component: [normal Episode4 entry and first-turtle defeat](pinna-four-first-turtle.md) pass once from the earned thirteen-Shine save. Four turtles, all flower revivals, Shine33 save and fresh-process reload remain open; Episode4 is not accepted as complete.

Pinna Episodes1–3 now pass normal completion, saved rewards and independent fresh-process reloads: Mecha Bowser/Shine30, Cannon and block secret/Shine31, and pirate-ship red coins/Shine32. The latest connected earned save contains thirteen Shines. Episode4 onward remains open. The normal cannon prerequisite is ten distinct earned Shines plus the Ricco/Gelato unlocks. The following accepted controller-input prerequisites extend the existing six-Shine save; they do not certify Pinna combat. The accepted prerequisite save now contains ten earned Shines, including the normally collected hundred-coin reward107.

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

The corrected private candidate uses fixed controller pitches computed from the authored chest, FLUDD and nozzle transforms, including the roll-dependent FLUDD base tilt omitted from the earlier estimate. The corrected candidate is accepted by the complete native result below.

## Complete normal Mecha Bowser milestone

`pc_pinna_mecha_unassisted.dev`, accepted private `pn45_pinna_combat` from the earned ten-Shine `pn42_coins_observed` save, passes in 469.67s: rc0, `done` at tick13211, all four boss parts broken, original victory flag/movie8, Parco13/7 reward, ordinary Shine30 collection/save and return to Delfino1/8. Health ends8 after the plaza reset, with combat minimum6; crash.log is empty and no active cheats are logged. Four fresh R launches use fixed ordinary stick inputs and read-only rail/loaded/state checks. No developer stage start, teleportation, direct boss aiming, progress/save edit or movie/timing override is used.

The controller pitches are -640/-5760/-2880/0 at authored rail windows1200/1375/2100/2550. Native boss state progresses0 to4; the last part finishes at tick8443/rail2733.5 and the original defeat flag is observed at tick8490. The normal reward route crosses the shallow pool rim with a jump and collects the original Shine at(-2000,799.694,-12202.711).

Latest File B sector4/write15 contains eleven earned Shines: IDs0/1/10/20/30/86/94/95/98/107/117. All seven sector checksums pass. GCI SHA-256: `f430dcb08a8b745c2e340de231510c920c53946216b68fe77232d61832e85213`. Accepted frozen route SHA-256: `caae0905b0af68a909b48a61c1b4ca0d980c0d8b3969e7d47cae2ad6e414466c`. Executable SHA-256: `a24e73a61055538b2cade5011747a1bf54ace0c4135f4b796ecf4079576ddaa8`. Public executable commands match the actual accepted frozen route; comments differ.

`pc_pinna_mecha_reload.dev`, accepted private `pn44_cold_pinna`, starts a separate process from that saved output and passes in 51.35s: rc0, `done` at tick1363, normal File B menus, Delfino1/8, all eleven Shine flags plus Ricco/Gelato/cannon progression flags. Health8, empty crash log and no active cheats. The GCI is byte-identical to the saved result above, with all seven checksums valid. Accepted frozen cold-route SHA-256: `c92779fe005ea486351f2bae61106e7ad8b974a579cb011a1ba5765b83615be7`.

```powershell
python tools/route.py Strict tools/routes/pc_pinna_mecha_unassisted.dev --name <pinna> --appdata-from <hundred> --seconds 1600
python tools/gci_check.py "build/Route/<pinna>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_pinna_mecha_reload.dev --name <cold> --appdata-from <pinna> --seconds 240
```

This certifies one complete controller-input Episode1 pass plus its saved-reward cold proof. It does not certify consecutive full-route repeatability, other Pinna episodes, later-world completion, image quality or audible playback on physical hardware. The broader PC-CORE/PC-CONTENT rows remain open; matrix counts remain17 passed-bounded,8 open and0 external-check-required. The deferred mirror route and draft release are unchanged.

## Complete normal Cannon and block-secret milestone

`pc_pinna_cannon_unassisted.dev`, accepted private `si25_pinna2_full` from earned eleven-Shine `pn45_pinna_combat`, passes in410.67s: rc0, `done` at tick11534, normal Episode2 selection, Cannon HP3 to0 and original flag0x5000c, secret entry50/0, all block/spring transfers, Shine31 collection, original card/save flow and natural return to Delfino1/8. Combat minimum health is3; the secret resets it normally and final health is8. Crash log is empty and no active cheats are logged. Only controller inputs are emitted; no stage start, teleport, direct boss aim, actor/progress/save edit or timing/movie override is used.

Fresh Bob-ombs are sprayed, picked up with bounded B pulses and thrown normally. The secret route waits on fixed surfaces, walks touching timed blocks, crosses two600-unit gaps with running jumps and uses all four original spring pads. Read-only hidden-block countdowns synchronize the upper departures. The final L corner through(13080,3720) saves approximately541units and passes both original-collision footprint checks and native traversal. Final takeoff is reached at10799 and fixed landing at10840. Height/floor guards observe the route; they do not establish a grounded Mario status.

Earlier `si24_pinna2_full` is **not a pass**: Cube14 disappears at10666 while Mario is123units short of its edge, then Cube15 briefly catches him before disappearing at10716. An earlier departure would enter the still-hidden first block; the supported shorter corner corrects the late crossing. `si15` is only a component pass: its unintended first `end` skipped the secret tail. Failed local evidence remains preserved. No game-code defect or undefined-behavior cause is asserted.

File B sector3/write16 has twelve earned Shines: IDs0/1/10/20/30/31/86/94/95/98/107/117. All seven sector checksums pass. GCI SHA-256: `1f99d219bda24b4dabb8ba4e555bd7c9038cdc7d5567f5b4fc1027c6dda2afb8`. Accepted frozen full-route SHA-256: `75d69e4f490ed17f6e08f7835a00195b6d099e3ccae9ec2bf1580f66c3c4af94`. Public executable commands are identical; comments and line endings differ. The parent GCI is the accepted Episode1 save documented above.

`pc_pinna_cannon_reload.dev`, accepted private `si26_pinna2_cold`, starts a separate process from that saved output and passes in50.25s: rc0, `done` at tick1363, normal File B menus, Delfino1/8, all twelve Shine flags and prior progression checks, health8, empty crash log and no active cheats. The GCI is byte-identical to the saved result above with all seven checksums valid. Accepted frozen/public cold-route SHA-256: `67c0255c1484878b68b1fcec1ae9bcf8da8f7ea41a355f5025174f159e9c72ff`.

Both use Strict executable SHA-256 `e5c1252ece36959d80fe964df20e2b37efe9c0827bff8979b11ed5fc82880478`: **STRICT: 0 unresolved symbols**,68 port checks and42 focused native parser cases pass (23 countdown,17 bomb/filter, invalid held state and existing observations). The new typed observations require the joined current scene, perform fresh lookups and retain no scene pointers. `clear bombs=1` filters existing PAD steering/spray targets; default0 preserves previous behavior. `bombready` observes a candidate; the route separately checks actual holding. Additional Tama telemetry is diagnostic and certifies no Episode4 play.

The observer headers follow the standard-library headers. Game headers define global `EOF=1`; reversing that order changed the compiled stream EOF value and hung developer-script parsing. A compile-time assertion now requires `std::char_traits<char>::eof()==-1`: the bad-order control fails it, while the corrected native parser triplet passes. No original game source or asset is changed.

```powershell
python tools/route.py Strict tools/routes/pc_pinna_cannon_unassisted.dev --name <pinna2> --appdata-from pn45_pinna_combat --seconds 1000
python tools/gci_check.py "build/Route/<pinna2>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_pinna_cannon_reload.dev --name <cold> --appdata-from <pinna2> --seconds 240
```

This certifies one complete Episode2 controller-input pass and its independent saved-reward cold proof. Consecutive full-route repeatability, Pinna Episode4 onward, later worlds, full image/audio quality and the full playthrough remain open. Episode3 is recorded below. Matrix states remain17/8/0; PC-CORE/PC-CONTENT remain open, mirrors deferred and v0.1.0 a draft.

## Complete normal pirate-ship red-coin milestone

Accepted `p317_full` starts a fresh process from the earned twelve-Shine `si25_pinna2_full` profile. Normal File B menus, Delfino cannon, Episode3 selector and the beach gate lead to park13/1. Ordinary movement, jumping/Hover and ship-angle gates collect the ship and west-cage coins; normal green-panel flips and roof crawling collect the remaining coins. The lower bridge uses two top-side ground pounds; the accepted crossing has no electrical-damage samples. The actual east-cage wall jump reaches the3200panel, a near-edge jump catches the3500ceiling, and a normal Hover transfer reaches its TOP. Eight red coins trigger the original reward camera, followed by Shine32 collection, the original card flow and return to Delfino1/8.

Full route: **rc0 / 334.85s**, final tick9275; all eight temporary red coins, saved flag0x10020, thirteen Shines, empty crash log and no active cheats. Minimum observed health is5; this is not a damage-free run. File B sector4/write17 contains exactly IDs0/1/10/20/30/31/32/86/94/95/98/107/117. All seven GCI sector checksums pass. Saved GCI SHA-256: `99f868abf960e0eba6d68b0e6fc4248947ba075f8d5b75dbaface967a96545e4`.

Independent `p318_cold` starts after that process ends, using only its saved appdata and the normal File B menus. **rc0 / 50.23s**, final tick1363; Delfino1/8, health8, thirteen native Shines and every prior/new Shine flag pass. Its GCI is byte-identical to the saved result, with all seven checksums valid; crash log is empty and no cheats are active.

Both accepted runs use `/O2 /Ob1` Strict executable SHA-256 `7c8ec93285ee025a85f233570c996b0bda0fa15a494991cacc1d0c0308335e19`: **STRICT: 0 unresolved symbols**,68 port checks and four focused renderer cases pass. The unchanged observation sources previously passed33 focused native parser cases (21 ship/red-coin cases, ten Mario-status cases, existing observations and wood-wait bounds). The full route and cold proof additionally exercise those observations on this final executable. No original game source or asset is changed.

Public full route SHA-256: `e8d131c76e13402f68217ffb520543980b9216fb8fae22374bf4417ec35798b2`. Accepted private frozen route SHA-256: `d5b903f08b82b442deef9b40adb5a851bdc71e3e79d7cc3a8ec421f60747a4b3`. Only comments/blank lines differ; all291 commands are identical, with command-list SHA-256 `220db005ccc19c474e958cc53fe0491c2165e61c0a083d99ef56b9a77042b518`. Public/frozen cold route SHA-256: `7150dc7965442ab96f6a275f7bec490a6c5b60e25949fab10ae443a5b226dddc`.

Failure controls retained privately: `p303_full` rejects an unreachable ship-rate band; `p304_full` misses the second ship with a stale target; `p305_full` expires during a still-progressing Hover flight; `p306_full`/`p307_full` expose the west-wall approach and fence-versus-roof distinction; `p308_full` jumps before braking and misses the floor flip. `p309_full` proves the real renderer abort during normal electrical damage. [Renderer correction, identical-input control and limits](gx-logic-invert.md). `p310_inv_control` survives that same hit but falls; it is not a full-route pass. `p311_full` reaches an impassable cage wall from the TOP; `p312_full` shows a ground pound outside the second patrol's collision radius. Corrected approach targets account for measured braking drift. `p313_full` actually reaches3200 and rejects an obsolete intermediate-height assumption; `p314_full` launches too early to catch the next ceiling. `p315_full` proves the TOP landing on the rising net at3613, beyond an obsolete flat-walkway bound. `p316_full` collects all eight coins but its last waypoint expires during the original reward camera; `p317_full` allows that camera to finish normally. No camera skip, invulnerability change, timing override or save edit is used.

```powershell
python tools/route.py Strict tools/routes/pc_pinna_pirate_unassisted.dev --name <pinna3> --appdata-from si25_pinna2_full --seconds 1600
python tools/gci_check.py "build/Route/<pinna3>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_pinna_pirate_reload.dev --name <cold> --appdata-from <pinna3> --seconds 240
```

This certifies one complete Episode3 controller-input pass and its independent saved-reward cold proof. Consecutive full-route repeatability, Episode4 onward, later worlds, full image/audio quality and the connected full playthrough remain open. Matrix states stay17/8/0; PC-CORE/PC-CONTENT remain open, mirrors deferred and v0.1.0 a draft.
