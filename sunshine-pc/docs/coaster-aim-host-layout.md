# Coaster first-person aiming on the host

`CPolarSubCamera::isJetCoaster1stCamera` now uses the existing `TCameraJetCoaster::isLButtonMode()` getter.
The previous fixed offset `0xC` reads the low byte of the coaster's `unk8` angle on Windows, not its first-person mode flag.
The flag is at host offset `0x10` because the preceding parameter pointer occupies eight bytes.
Camera rendering and Y-button toggling already use the typed getter, so this correction makes FLUDD agree with the active view.

## Gameplay effect and evidence

`TNozzleBase::calcGunAngle` copies the player-camera pitch only when ordinary first-person mode or this coaster query is true.
Otherwise it applies its ordinary nozzle-angle target and chase rule.
With coaster first-person enabled and `unk8 = 0`, the old host query returns false and FLUDD follows the fallback angle instead of the aimed pitch.
With first-person disabled and `unk8 = 1`, the old query incorrectly selects camera pitch.
The real coaster constructor initializes first-person mode to 1 and `unk8` to 0, matching the first counterexample.

The GMSJ01 query at `0x80363344` checks mode `0x2E`, a non-null helper and bit0 of its byte at `0xC`.
That byte is the correct flag in the PPC layout.
The changed Strict object instead loads the flag at `0x10`, retaining the mode/null/bit checks.
This is a host field-layout correction; no aiming assistance, camera override or hang bypass is introduced.

## Regression checks

`tools/coaster_aim_test.py` extracts the production query and the entire production `TNozzleBase::calcGunAngle` routine.
It compiles both at `/O2 /Ob1`, using the actual coaster helper declaration and getter.
The fixture supplies inert surrounding camera/FLUDD dependencies and explicitly initializes the tested helper fields, bypassing its parameter-loading constructor.
It tests these two routines, not a running coaster scene or the complete game-class layouts.

The unchanged `a10fa0d` source fails 16 of 37 checks; corrected source passes all 37.
The matrix covers mode bytes 0/1/2/3 and angle values 0/1/256/-1, with expected nozzle pitch 6000 in first-person and fallback -2048 otherwise.
Null helper, ordinary follow/first-person cameras and another actor's FLUDD also retain their original behavior.
Run from `sunshine-pc`, with `LLVM_BIN` set as for Strict:

```powershell
tools\coaster_aim_test.cmd
```

An optional unchanged `CameraMode.cpp` path supplies the negative control.
Local outputs are `build/pa08-aim-before.log` and `build/pa08-aim-after.log`.
No disc data is embedded in the fixture.

- Strict reports `STRICT: 0 unresolved symbols`; 68 port checks pass.
- `ninja baseline` then `ninja changes_all` retains identical report totals, including 11,323 exact functions and 445 matching units.
- The changed PPC object is byte-identical to the fresh unchanged object, including all 447 relocation destinations.
- Changed-unit symbol validation passes without errors.
- The actual Strict query disassembly confirms the byte load at `0x10`.

## Native scope

`pa08_normal_aim` completes normal File B load and ordinary Y/R/L/stick inputs in Delfino1/6: rc0/60 s, completion at tick1635, health8 throughout, no active cheats and an empty crash log.
The log observes normal camera modes 0 and 7, and FLUDD consumes water from 10000 to 9520 during the inputs.
The earned six-Shine GCI remains byte-identical, SHA256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.
This is a regression smoke outside the coaster, not a native reproduction of its aiming defect.

The public route has exactly the accepted private controller/observer commands:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_aim_unassisted.dev --name <unique> --appdata-from nw91_gelato_early_takeoff --seconds 300
```

Local summary `build/pa08-summary.py` checks completion, normal camera modes, water use, health, cheats/crash absence and unchanged save; output is `build/pa08-native-evidence.json`.
Strict executable SHA256: `056075ce77d26593c77aa8fcb71d81e25a718ad791fe5102072575a000ddff43`.
PPC object SHA256: `bc000131f02a7ec063301287ea64fd85890948180e17c1f95c3f6d29b25fca7a`.
Public route SHA256: `5fc6ae9d83b5902ba376577455fd500ba8ebbb1260f79f5fde323a8bec5f47bd`.
No developer stage selection, teleportation, direct aim, cheat, fixed seed, progress/resource write or movie/timing override is used.
Script completion is not native Close acceptance or a consecutive-repeatability claim.

The original isolated change did not certify normal Pinna access or combat. On2026-10-19, `pn45_pinna_combat` establishes normal cannon/beach/park/director/coaster entry, first-person aiming, four real rocket hits, Mecha Bowser defeat and Shine30 save (rc0/469.67s, health8 end/min6, crash0/no cheats). `pn44_cold_pinna` independently reloads all eleven earned Shines and the cannon unlock (rc0/51.35s), with unchanged valid GCI. Exact build/route/save hashes and controller commands: [normal Pinna milestone](pinna-normal-route.md). This is one affected-path completion and cold proof, not coverage of every coaster mission or a native old-source control.
The mirror route remains deferred; the broader playthrough and historical enemy wrap watchdog remain open.
