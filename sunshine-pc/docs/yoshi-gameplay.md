# Normal Yoshi gameplay milestone

## Accepted scope

`ys16_inland` passes one normal-input Yoshi functional run in Pinna Episode4: feed the egg, mount, ride, eat with the tongue, refill/change colour, flutter, spray juice and dismount.
The implementation already existed; this wave adds read-only observations, controller steering toward revealed fruit, a route and bounded evidence. No original game code or game state is patched.

The normally earned `p525_full` profile contains fifteen Shines, including Pinna4 Shine33.
Normal File B loading and the Delfino cannon reach the episode menu; two ordinary left presses select Episode4 (`stage=5 scen=3`, `pinnaBeach2`).
Its beach egg at `(7800,163,5700)` requires Shine33. The permanent Delfino unlock is separate.
The route reveals banana and pineapple with normal squat-spray stick sweeps, stops when fruit appears, follows it and carries the requested fruit with B.
Rolling fruit is sprayed inland; stopping R cannot remove drops already in flight.
Pickup stops at radius 140, outside body separation and inside normal pickup reach. Feeding stops at radius 200 before the separate mount jump.

## Recorded result

- Run: `ys16_inland`, rc0, 205.36 seconds, done at script tick 5687; empty crash log; no active cheats.
- Egg wanted banana (`0x40000394`); hatch/unmounted state 6 at tick 5243 and mounted state 8 by tick 5280, colour 3.
- Mounted B is followed by pineapple consumption: colour 3 to 1, juice 20764 at tick 5340 to 21300 at tick 5350, and the revealed pineapple disappears. This production outcome proves tongue/eating/refill even though the short tongue cycle ends between snapshots (`tongue=0`).
- Riding moves over 500 horizontal units. Flutter state1 is recorded at tick 5460. X dismounts to state 6 at tick 5687.
- `R trig=0,255` uses the original Yoshi nozzle 3. Visual review of `ys_juice_start.png` shows the orange juice jet and its ground impact during the 60-tick trigger hold; nozzle selection or lifetime drain alone would not prove emission.
- Health ends/minimum 6. Two logged ordinary hits come from actor `0x1000001A` at `(7300,161,5800)` during approach; this is not a no-damage route. Episode4 also contains these enemies, despite its five sleeping turtles.
- The card is byte-identical to the earned seed: File B sector4/write19, fifteen Shine IDs0/1/10/20/30/31/32/33/34/86/94/95/98/107/117; all seven sector checksums pass. No persistent progress changes, so this functional milestone does not need another save/cold cycle.

SHA256:

```text
Strict executable 5e02d74530ebd1d0d1cc5c5599a125f9f86a48f62cbcec486462ed14a8de9dbd
Route             33622f45deac29ad5e7704c69fd3278c84fbdd538954d830ae07423bb9639bb7
GCI               d2881420875902009086f4ca53a6a9659cf58a6841bb00a62ad169ca51a6db9a
```

Private artifacts remain under `build/Route/ys16_inland/`: `run.log`, `crash.log`, screenshots and the copied profile. The frozen route and compact result are `build/ys16_inland.dev` and `build/ys16_inland-result.json`; they contain no acceptance claim for other episodes.
Preflight: `STRICT: 0 unresolved symbols`, 74 port checks with 0 failures and 27 focused parser/unready/missing-target cases passed. Independent code review found no blocker; the native run supplies the positive gameplay evidence.

## Permanent Delfino unlock, save and cold reload

`sr51_yoshi_saved_seed1` starts from the same earned fifteen-Shine File B and completes the permanent Delfino Yoshi event with normal controller inputs: defeat Shadow Mario, dismiss the dialogue, carry the requested banana to the egg, hatch and mount Yoshi, then use **Pause > Save > Yes > Continue**. It passes with rc0 in 126 seconds, finishes at tick 3514, keeps health at 7 or 8, records no active cheat and has an empty crash log. Mounting sets `0x1038F`; the final post-save assertion observes it again in controllable Delfino 1/8.

The route uses `SUNSHINE_DEV_SEED=1` only to make the ordinary Shadow Mario chase reproducible. It does not start a stage, aim at the target, teleport, change progress or emit non-controller gameplay actions.

The saved card changes from SHA-256 `d2881420875902009086f4ca53a6a9659cf58a6841bb00a62ad169ca51a6db9a` to `097ed898069ce949067c0867230a31971723917d2f2dc38aa19c3c63db6fb9c8`; the active File B advances from sector write counts 18/19 to 20/21. `gci_check.py` passes all seven checksums.

`sr52_yoshi_cold` starts a separate process from that output and loads File B through the normal title and file-select flow. It passes with rc0 in 50 seconds, finishes at tick 1393, reaches controllable Delfino 1/8 with fifteen Shines and asserts `0x1038F`. Its GCI is byte-identical to the saved source, its crash log is empty and no cheat is active. This closes the permanent Delfino Yoshi unlock/save/cold prerequisite; it does not certify every Yoshi mission or interaction.

## Reproduce

From `sunshine-pc`, with the private normally earned seed available:

```powershell
Get-PSDrive C
python tools\locked.py cmd /c tools\_strict.cmd
.\tools\port_tests.cmd
python tools/route.py Strict tools/routes/pc_yoshi_pinna_abilities.dev --name <unique> --appdata-from p525_full --seconds 300 SUNSHINE_DEV_HPLOG=1
python tools/gci_check.py "build/Route/<unique>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_delfino_yoshi_unlock_unassisted.dev --name <unlock> --appdata-from p525_full --seconds 300 SUNSHINE_DEV_HPLOG=1 SUNSHINE_DEV_SEED=1
python tools/gci_check.py "build/Route/<unlock>/appdata/Sunshine/saves/JAP/Card A/01-GMSJ-super_mario_sunshine.gci"
python tools/route.py Strict tools/routes/pc_delfino_yoshi_unlock_reload.dev --name <cold> --appdata-from <unlock> --seconds 150
```

The script contains no developer start, teleport, direct aiming, cheats, guard, bypass or save edit. Success requires the recorded feeding/mount/refill/flutter/spray/dismount outcomes; rc0 alone is insufficient.

## Read-only route support

`yoshistate=0..8` reads the current Yoshi state after scene setup has joined.
`eggfruit=0x40000390..0x40000394` reads a living egg's request; decimal IDs are accepted.
`livefruit=` observes a living `TResetFruit`. `walkto fruit=` and `faceto fruit=` update a waypoint for the existing camera-relative PAD steering; no actor/facing/camera state is written.
`TCoverFruit` and fruit more than 500 vertical units from Mario are excluded. Missing targets fail explicitly, and actor pointers are found afresh each frame.
This helper is limited to ground-fruit routes. Conditions remain false before scene readiness.
Logs include Yoshi state/type/lifetime juice/flutter/tongue/mouth, unlock flag, eggs, living fruit, linked hidden-fruit reveal state and the actual nozzle emission basis.
The FLUDD water count refills separately while riding; lifetime drain by itself is also not proof of a juice jet.

## Calibration and limits

Delfino probes `ys01_egg`--`ys03_early` were stopped when Shadow Mario waited on a roof outside the ground route's 3D trigger range. They do not prove a missing unlock implementation.
`ys04_pinna` exposed the seed's default Episode6 selection. Later fruit probes established ordinary water reveals and exposed high-fruit targeting, damaging approach positions, collision-limited pickup and water pushing rolling fruit toward the sea.
The height filter corrects the new helper's high-banana false target (`ys08_follow`). No optimization defect or UB cause is established.
`ys13_episode4` reveals both fruits but its radius 85 pickup orbits at about 92 separation and times out.
`ys14_pickup` completes hatch, mount, eating/refill, flutter and dismount once with health 8; its digital-only R command does not prove juice emission.
`ys15_juice` times out after residual spray rolls pineapple into the sea. `ys16_inland` is the accepted complete functional run, not a repeatability certificate.

The native accepted Pinna branch feeds banana and eats pineapple. Other egg requests are parser/source checked, not certified by that run. The separate Delfino route feeds the requested banana and proves the permanent unlock/save/cold path.
This does not certify all Yoshi missions, fruit variants, juice enemy/platform/goop interactions, underwater dissolution, physical-controller behaviour or audible output. Muted WARP testing and script exit do not replace those checks or normal-quit/lifecycle acceptance.
