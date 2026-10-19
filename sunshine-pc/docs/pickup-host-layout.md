# Pickup fields on the 64-bit host

`TMario::receiveMessage` now reads 1-up state/age and Shine initial yaw through typed game-class members.
The old code added GameCube byte offsets to the sender, despite its larger host layout.
This could reject mature 1-ups, accept an early pickup based on unrelated bytes, and orient the Shine collection pose using gravity.
The original age threshold, state condition, healing, water refill, life increment and Shine sequence remain unchanged.

## Layout and original-code evidence

The supported game registry identifies actor types `0x20000005`, `0x20000006` and `0x20000007` as `mushroom1up`, `mushroom1upR` and `mushroom1upX`.
Their class is `TMushroom1up`, whose control routine increments `unk13C` and whose pickup routine updates `unk13A`.
Actor type `0x20000013` is the Shine, not a mushroom.

| Read | GameCube offset | Host member offset | Previous host read |
| --- | --- | --- | --- |
| 1-up state, `TMushroom1up::unk13A` | `0x13A` | `0x1AA` | A byte within `mRidePos.z` |
| 1-up age, `TMushroom1up::unk13C` | `0x13C` | `0x1AC` | `mGroundActorYaw` bytes interpreted as an integer |
| Shine initial yaw, `TMapObjBase::mInitialRotation.y` | `0x11C` | `0x184` | `mGravity` |

These host offsets are measured by compiling the real headers with the Strict compiler and `TARGET_PC`.
The actual Strict object confirms reads at `0x1AA`, `0x1AC` and `0x184` in the three corrected paths.
`TLiveActor` initializes the unrelated riding position and ground-actor yaw to zero, so they cannot serve as a mushroom's elapsed-age counter on static ground.
GMSJ01 `TMario::receiveMessage` reads the original state/age at `0x80162430` and `0x8016243C`, and the original yaw at `0x801629AC`.
Typed access selects the same fields on PPC and their actual locations on Windows.
This is a host layout correction, not a claim about compiler optimization or the unrelated historical wrap-loop watchdog.

## Checks

`tools/pickup_layout_test.py` extracts the two production load expressions and compiles them at `/O2 /Ob1`.
Its storage fixture takes every relevant member offset from the actual game headers, with static assertions checking the layout.
It does not construct the game actors or simulate collision, effects, the life counter or the save flow.
The unchanged `c9d5803` source fails ten of eleven cases; the corrected source passes all eleven.
Cases cover age119/120/200, the original nonzero-state condition, unrelated old-offset bytes, and four initial Shine angles.

Run from `sunshine-pc`, with `LLVM_BIN` set as for Strict:

```powershell
tools\pickup_layout_test.cmd
```

An optional unchanged source path provides the negative control.
Local controls and results are retained as `build/pa05-original-MarioReceiveMsg.cpp`, `build/pa05-pickup-before.log` and `build/pa05-pickup-after.log`.
No disc data is embedded in the test.

- Strict reports `STRICT: 0 unresolved symbols`; 68 port checks pass.
- `ninja baseline` and `ninja changes_all` retain identical report totals, including 11,323 exact functions and 445 matching units.
- All allocated PPC sections retain identical bytes, including the entire `.text` section SHA256 `77a11b02aa32d5c02c9236938b9a46efc483b883db1f31fd364d17d6a1119c67`.
  All 456 relocation destinations, types and addends remain identical; anonymous constant labels are renumbered by the extra header declarations.
  The complete object file is therefore not claimed byte-identical.
- Changed-unit symbol validation passes with no new or inherited errors.

## Native evidence

The first normal controller probe, `pa05_oneup_before`, completes rc0/94 s with an empty crash log, but misses the sewer cover and stays above ground.
It neither reaches a mushroom nor reproduces its old rejection in the game.
`pa06_pickup_shine` passes the complete normal Gelato sandcastle mission from the earned five-Shine `nw66_gelato_hover_entry` profile: rc0/263 s, completion at tick7466, health8 at the end/min7, no active cheats and an empty crash log.
The trace records 364 ticks of Shine pickup state `0x1302`, ticks6833..7196, all facing `0.0` degrees.
That matches the original Shine's initial yaw in `mam_ex0` scene record `0x6F35`.
Normal save and Delfino1/6 return complete; all seven GCI block checksums pass.
File B sector3/write8 contains IDs0/1/10/20/86/117, GCI SHA256 `9d319d154ed97cf0313d71c20525d3467dbb09b5ce472a1002b2c7b2e7b9a79f`.

Its private script adds only read-only trace output to the public route's identical controller commands.
The local assertion summary `build/pa06-pickup-summary.py` checks completion, pickup yaw, cheats/crash absence and command equivalence; output is `build/pa06-pickup-evidence.json`.
Run the public regression from `sunshine-pc`:

```powershell
python tools/route.py Strict tools/routes/pc_gelato_sandcastle_unassisted.dev --name <unique> --appdata-from nw66_gelato_hover_entry --seconds 1200
```

Strict executable SHA256: `184430bf9a5e2b87665580e3c3159b4cee9543fb439a7d200656bc2ec419ca67`.
No developer stage selection, teleportation, direct aim, cheat, fixed seed, progress/resource write or movie/timing override is used.
One accepted full route does not establish consecutive repeatability or native Close acceptance.

`pa07_oneup_after` passes the corrected normal sewer approach from the earned six-Shine `nw91_gelato_early_takeoff` profile: rc0/95 s, completion at tick2689, health8 throughout, no active cheats and an empty crash log.
Mario enters the beach manhole with an ordinary ground pound, reaches the fixed `Mushroom1upX` near `(8095,-1300,-577)` and gains one life, 3 to 4.
The saved GCI remains byte-identical to its source, SHA256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.
This verifies one normal stationary mushroom pickup; it does not certify every spawned/fleeing variant or a saved extra life.
The exact native pre-fix rejection is not reproduced; its counterexample is the production-expression/layout fixture.

The public route has the same operative commands as the accepted private script:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_oneup_unassisted.dev --name <unique> --appdata-from nw91_gelato_early_takeoff --seconds 300
```

The local assertion summary `build/pa07-oneup-summary.py` checks completion, the 3-to-4 life transition, health, cheats/crash absence and the unchanged save; output is `build/pa07-oneup-evidence.json`.
Public route SHA256: `10ad104957412168313d05688e3a523ec77b709a25c9418b828f0cc3dc33db7c`.

The full playthrough, broad 1-up coverage, the historical enemy wrap-loop watchdog and Gelato mirror/Wiggler completion remain open.
