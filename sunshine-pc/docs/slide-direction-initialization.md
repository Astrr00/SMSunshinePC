# Signed speed at the end of a slide

On PC, `TMario::slideProcess` now initializes `turn` from the signed difference between Mario's facing and his velocity bearing before the low-speed heading test.
When both horizontal velocity components are strictly between -0.1 and 0.1, that test skips the heading adjustment, but the routine still reads `turn` to choose the sign of `mForwardVel`.
Previously that read used an uninitialized local.
The correction retains forward/backward classification at tiny nonzero speeds without rotating Mario; it leaves acceleration, friction, thresholds, the normal heading chase and component cap unchanged.
Only the PC build changes.

## Evidence

A private `/O2` warning scan compiled 224 Player, Enemy, MoveBG, Map and Camera units into separate audit objects; all compiled successfully.
Clang identified the skipped assignment and unconditional sign read in `MarioRun.cpp`.
Recompiling the complete changed unit with the same warnings removes that diagnostic.
The other warning concerns an unproven null-parameter path in `hinokuri2.cpp`; it is not changed here.

The complete GMSJ01 routine at `0x8013B520` also has this inherited path.
Its tiny-velocity branch at `0x8013B690` jumps to `0x8013B804`, skipping the relative-angle assignment to `r31` at `0x8013B6A4`.
The final sign comparisons at `0x8013B89C` and `0x8013B8A4` still read that register; the prologue saves it but does not initialize it.
The PC fix uses the relative angle already used by the moving branch rather than assuming all tiny movement is forward.
This evidence concerns this local read; it does not explain the separate historical enemy-wrap watchdog.

## Isolated regression

`tools/slide_direction_test.py` extracts the complete production `slideProcess` body and compiles it at Strict's `/O2 /Ob1`.
The fixture supplies flat ground, zero slope acceleration and heading-chase speed, and standard-library math stand-ins.
It does not reproduce the full engine math, class layout, collision or `doSliding` stop-state logic.
The unchanged source fails five of 35 checks; the corrected source passes all 35.
Cases include forward/backward cardinal movement below the threshold, diagonals, exactly perpendicular facing, the threshold itself, ordinary speed, stationary motion and the original component cap.

Run from `sunshine-pc` with `LLVM_BIN` configured as for Strict:

```powershell
tools\slide_direction_test.cmd
```

An optional unchanged `MarioRun.cpp` path supplies the negative control.
Local outputs: `build/pa09-slide-before.log`, `build/pa09-slide-after.log` and `build/PlayabilityAudit_pa09/summary.json`.

- Strict reports `STRICT: 0 unresolved symbols`; all 68 port checks pass.
- `ninja baseline` followed by `ninja changes_all` retains all report totals, including 11,323 exact functions and 445 matching units.
- The changed PPC object is byte-identical to the fresh unchanged object: SHA256 `a9a9b2bc64958cf8e173a963aee7272af542621822374d8055185b29db2764d5`.
- Baseline-aware symbol validation reports zero new errors and the one inherited missing unused `braking` symbol; the unqualified check still reports that existing error.

## Native movement regression

`pa09_normal_slide` passes normal File B load and two ordinary run/B-dive/slide/stop/A-jump cycles in Delfino1/6: rc0/72 s, completion at tick1979.
Each cycle traces 29 ticks in `MARIO_STATUS_CATCH` (whose handler calls `doSliding`) and 19 in `MARIO_STATUS_JUMP`, followed by stationary waiting.
All 616 traced positions, velocity components and signed speeds are finite.
Health remains 8, no cheats are active, the crash log is empty and the earned six-Shine GCI remains byte-identical.
Its checksums pass; SHA256 is `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.

```powershell
python tools/route.py Strict tools/routes/pc_delfino_slide_unassisted.dev --name <unique> --appdata-from nw91_gelato_early_takeoff --seconds 300
```

The public route contains the accepted controller/observer commands; only a stale inherited comment was removed.
Local summary `build/pa09-summary.py` asserts both movement cycles, completion, finite motion, health, cheats/crash absence and unchanged save; output is `build/pa09-native-evidence.json`.
Strict executable SHA256: `8fa3ed6eeaebddd7fc03a4e6993a10faa29cad2c2a3e6810e018834581a25d62`.
Public route SHA256: `9482cc169bbde5dec80d9be343738b819e918f48447aa1d488bd9a1a300a45b4`.
No developer stage selection, teleport, direct aim, cheat, fixed seed, progress/resource write or movie/timing override is used.
Script completion is not native Close acceptance or a consecutive-repeatability claim.

The exact tiny-speed branch has not yet been observed during native ordinary play; its affected-path proof is the extracted production-routine test.
`doSliding` retains its original stop rules, including the surfaces that allow continued sliding below the ordinary stop threshold.
The mirror route remains deferred; broad playthrough and the historical enemy-wrap watchdog remain open.
