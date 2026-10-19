# Stick-rotation history initialization

The PC stick-rotation query now clears its four local quadrant flags before examining the current history.
This prevents a previous query's stack values from satisfying quadrants absent from the current input.
The shared query is used by ordinary spin jumps, airborne spin conversion and FLUDD rotation.
No controller assistance, state override or hang bypass is involved.

## Evidence

`TMario::checkStickRotate` in `sms/src/Player/MarioMove.cpp` writes `q[n] = 1` only for visited quadrants, then reads all four flags.
Previously, no path initialized unvisited flags.
The original GMSJ01 routine at `0x80130758` also omits initialization: conditional stores at `0x801307D4`, `0x801307F4`, `0x8013080C` and `0x8013082C` precede unconditional loads starting at `0x80130864`.
This is a PC correction of an inherited uninitialized read, not a decompilation mismatch or evidence about any unrelated optimization failure.

`tools/stick_rotate_test.py` extracts the current method body verbatim and compiles it with clang-cl at `/O2 /Ob1` and `TARGET_PC=1`.
Its isolated fixture provides only the two fields read by that method and prevents inlining.
Successive calls have no intervening library calls, allowing the same stack slots to be reused.
After a complete rotation, the unchanged source incorrectly accepts straight input, three quadrants, empty history, one sample, incomplete boundary coverage and a short history.
The unchanged source fails six of twelve checks; the corrected source passes all twelve.
Both rotation directions, original quadrant boundaries and the original exclusion of the final comparison endpoint remain covered.
The fixture tests the real method body, not the surrounding game, class layout, controller driver or call-site stack layout.

Run from `sunshine-pc`, with `LLVM_BIN` set as for Strict:

```powershell
tools\stick_rotate_test.cmd
```

An optional source path runs the same fixture against an unchanged source for a negative control.
The retained local control is `build/pa03-original-MarioMove.cpp` from commit `035bf58`.
Its output is `build/pa03-stick-before.log`; corrected output is `build/pa03-stick-after.log`.
No disc data is required or embedded in this test.

## Build validation

- Strict reports `STRICT: 0 unresolved symbols`; `tools/port_tests.cmd` reports 68 checks, 0 failed.
- `ninja baseline` followed by `ninja changes_all` retains identical report totals, including 11,323 exact functions and 445 matching units.
- The changed PPC object remains byte-identical, SHA256 `99b171d2b1154cc1710a0482e3273c56baa86714ace5c92d71c4f6367038cd7d`.
  Initialization is compiled only under `TARGET_PC`.
- Symbol-order validation against the unchanged object reports zero new errors and one inherited missing UNUSED symbol, `getLRLevel__6TMarioFUc`, with 17 existing UNUSED size warnings.
- Disassembly of the actual Strict `MarioMove.cpp.obj` confirms four zero stores before the history loop.

## Bounded native movement regression

`pa04_stick_rotation_fast` passes normal File B load from the earned `nw91_gelato_early_takeoff` profile, both spin directions and three ordinary jumps: rc0/73 s, script completion at tick2013, health8 throughout and an empty crash log.
The per-tick trace records left spin `0x895` for 49 ticks beginning at1399 and right spin `0x896` for 49 ticks beginning at1642.
After each spin, neutral-history A produces an ordinary jump `0x02000880` for 19 ticks, beginning at1533 and1776.
A straight running jump also retains the ordinary state for 19 ticks beginning at1910.
No spin state occurs in those three normal-jump windows.
The saved six-Shine GCI remains byte-identical to its earned source, SHA256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.

The first movement probe, `pa03_stick_rotation`, also completes without a crash (rc0/69 s), but its slower nine-point input circles produce ordinary jumps, so it does not certify positive spin detection.
Both attempts remain retained; there is no consecutive-route repeatability claim.
The before/after false-positive proof is the isolated routine test, not a native reproduction of the old call-site stack state.
Native validation uses normal menus, cardinal stick inputs and A, with read-only trace output.
There is no developer stage selection, teleportation, direct aim, cheat, fixed seed, progress/resource write or movie/timing override.
Script completion is not a native Close or full playthrough certificate.

The public route has the same operative commands as the accepted private script:

```powershell
python tools/route.py Strict tools/routes/pc_stick_rotation_unassisted.dev --name <unique> --appdata-from nw91_gelato_early_takeoff --seconds 300
```

The local assertion summary `build/pa04-stick-summary.py` checks all five jump windows, completion, empty crash log, unchanged save, absence of active cheats and public/private route equivalence.
Its output is `build/pa04-stick-evidence.json`.
Strict executable SHA256: `a0f12ae871ce33cb01a1a39e4a44e8c6940904f9dad7d9133ab9ea103eaa9f03`.
Public route SHA256: `b5fbd628a00ec59d8ca118bb0d18466b830bd220b315206d24d8986060ac767f`.

Broad playthrough acceptance, the historical enemy wrap-loop watchdog and Gelato mirror/Wiggler completion remain open.
