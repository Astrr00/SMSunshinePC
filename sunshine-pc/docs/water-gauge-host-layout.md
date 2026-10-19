# FLUDD full-tank notification on the host

The PC path in `TGCConsole2::loadAfter` now initializes its previous-water cache (`unk28`) through `nozzle->mEmitParams.mAmountMax.get()`.
The old load at fixed PPC offset `0xCC` reads the high word of the `mEmitPow` parameter's name pointer on Windows.
The actual capacity value is at host offset `0x1A0`; that unrelated name pointer starts at `0xC8`.
Both offsets come from the real `TNozzleBase`, `TEmitParams` and `TParamRT` declarations compiled for the host.

## Observable effect and scope

`updateWaterGaugeFill` requests sound `0x4807` when current water reaches capacity and differs from the cached previous value.
Initializing that cache from unrelated parameter metadata can therefore request the refill-complete sound on the first update of an already-full tank.
The same helper already obtains the correct capacity through the typed parameter for the visible fill ratio.
This correction fixes cache initialization and its false notification; it does not change tank capacity, water consumption or the gauge's ratio calculation.
Subsequent updates still replace the cache with current water and retain the original sound gate.

The GMSJ01 `loadAfter` at `0x8020EE40` loads the nozzle through `getCurrentNozzle() const`, reads `0xCC` at `0x8021015C` and stores it into console offset `0x28` at `0x80210164`.
The gauge helper's capacity read and full-tank sound condition confirm what that field represents.
The complete unchanged `loadAfter` diff was reviewed; it retains existing stack/register differences from retail.
Using the typed getter on PPC changes MWCC's stack allocation, so only PC uses the new expression; the original PPC path is preserved.
No guard, gameplay bypass or runtime fallback is added.

## Regression checks

`tools/water_gauge_test.py` extracts the production PC initialization expression and the complete `updateWaterGaugeFill` body.
It compiles them at `/O2 /Ob1` with the actual nozzle/parameter declarations, real parameter construction and real `get`/`set` accessors.
Parameter registration/loading, panes, surrounding player/FLUDD objects and sound output are inert fixture dependencies.
The fixture records sound requests; it does not render a HUD or verify audible playback.

Twenty-five checks cover host layout, capacities 2100/10000/12000, startup with a full tank, partial/empty fill and alpha, a real refill, repeated full updates and sound-gate suppression.
The old source fails six checks: wrong initial cache and false initial sound request at each capacity.
After the initial-update checks, the sound counter is reset to test later transitions independently.
The corrected source passes all 25; the subsequent gauge and refill behavior is unchanged.
Run from `sunshine-pc` with `LLVM_BIN` configured as for Strict:

```powershell
tools\water_gauge_test.cmd
```

An optional unchanged `GCConsole2.cpp` path supplies the negative control.
Local outputs: `build/pa10-gauge-before.log` and `build/pa10-gauge-after.log`.
No disc data is embedded in the fixture.

- Strict reports `STRICT: 0 unresolved symbols`; all 68 port checks pass.
- `ninja baseline` followed by `ninja changes_all` retains all report totals, including 11,323 exact functions and 445 matching units.
- The final PPC object is byte-identical to the fresh unchanged object: SHA256 `f1ff463d3716dd0a1f32b80886fe2c95ffb77196ecaf9bfbdff6b00651241f9e`.
- Symbol validation passes with the existing four unused-size warnings and no errors.

## Native controller regression

`pa10b_water_gauge` passes ordinary File B load, stationary spraying, walking into the sea and R-button refilling: rc0/65 s, completion at tick1803.
The 440 read-only trace ticks show water10000 to9816 to10000 and finite position/velocity/signed-speed values.
Mario remains in Delfino1/6 with health8 and no active cheats; the crash log is empty.
Full, partial and refilled gauge screenshots are saved; the partial and refilled images were visually checked.
This is a consumption/refill regression, not a native audio comparison against the old executable.

The initial `pa10_water_gauge` attempt (rc0/61 s) consumed water but stopped at9816 because it only waited in the sea.
The original `getSuckRate` depends on R-trigger pressure; the accepted route adds ordinary R input and waits after the final screenshot.
Both attempts remain in the private logs.

The earned six-Shine GCI remains byte-identical and all checksums pass: SHA256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.
The public route retains the accepted controller/observer commands, removing only a stale inherited comment:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_water_gauge_unassisted.dev --name <unique> --appdata-from nw91_gelato_early_takeoff --seconds 300
```

Local `build/pa10-summary.py` asserts completion, water consumption/refill, finite motion, health, cheats/crash absence, three screenshots and unchanged save; output is `build/pa10-native-evidence.json`.
Strict executable SHA256: `cd44acd7501fd39a452b9b421257f898016f102cd9950e5582ceff52a0cb1420`.
Public route SHA256: `7ced71a712f697f9c3eaf489528df9a12e6c525e34053b3904d7cb9e0305ac22`.
No developer stage selection, teleport, direct aim, cheat, fixed seed, progress/resource write or movie/timing override is used.
Script completion is not native Close acceptance or a consecutive-repeatability claim.

Native audible startup/refill behavior remains unverified; the affected-path proof records the production helper's sound requests in isolation.
The mirror route remains deferred; normal later-world completion and the historical enemy-wrap watchdog remain open.
