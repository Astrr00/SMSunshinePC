# Shadow Mario replay-node boundary

The owner reprioritized shared playability defects over further Gelato mirror-route tuning on 2026-10-18.
This change bounds the ordinary replay node selection to the rows actually loaded from `linkdata.bin`.
It does not certify a full playthrough or reproduce every intermittent historical failure.

## Evidence

The supported disc's `dolpic1` scene has nine `mariomodoki` graph nodes and seven ordinary replay-link rows.
Nodes 7 and 8 are used separately by `emWaitingToInviteMario` and the gate replay.
`emReplayJumpToNearestNode` previously searched all nine nodes and used the resulting index directly in the seven-row `mReplayLinks` array.
The original PPC routine also has this unrestricted lookup; this is a PC boundary correction, not a claimed decompilation mismatch or an optimization diagnosis.

The retained `build/Route/v02/crash.log` identifies `TGraphNode::getPoint` called from the deterministic candidate lookup in `emReplayJumpToNearestNode`.
The old null-rail check protects the initial current node, not this candidate lookup.
At the last retained full enemy snapshot, `(2220,300,1798)`, the actual disc graph selects node 7 without the limit and node 6 with the seven-row limit.
That demonstrates an out-of-range replay-row selection consistent with the historical crash.
The snapshot precedes the exception and contains no fault-time index register, so the exact `v02` cause is not claimed proven.

A static check of `bianco6`, `ricco6`, `mamma6`, `monte6` and `mare6` finds equal graph/replay counts of 9/9, 11/11, 14/14, 12/12 and 9/9.
The inspected `pinnaParco6` and `sirena6` archives contain neither this graph nor a replay-link table; archive suffixes alone do not identify their Shadow Mario mission.
These are metadata checks, not normal-reachability or combat certificates.

## Change

The PC loader retains the replay-row count already read from the file.
Only the normal replay-jump selection passes it to the shared nearest-node search.
Other graph callers keep their full range by default, including the explicit gate-event paths.
Selection still uses the original nearest-distance and excluded-flag rules; it neither skips a tick nor fabricates a replay or input.
All changes to game structures and lookup arguments are under `TARGET_PC`.
The historical null-rail check is unchanged and is not presented as the fix.

## Validation

- `python tools\locked.py cmd /c tools\_strict.cmd`: `STRICT: 0 unresolved symbols`.
- `tools\port_tests.cmd`: 68 checks, 0 failed.
- `tools\replay_node_test.cmd`: six checks, 0 failed, compiling the real graph implementation at `/O2`.
  The synthetic fixture reproduces the unbounded index outside the replay rows, checks the bounded candidate, preserves the gate node for other callers, and tests empty, oversized and flag-filtered ranges.
  Unused map/spline dependencies abort if invoked; the fixture does not simulate their gameplay.
- `ninja baseline` then `ninja changes_all`: all 11,323 exact function identities and sizes, code/data totals and 445 matching units retained.
  Both changed PPC objects are byte-identical to fresh builds of source `9fec2e9`.
  `mario.dol` retains SHA1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.
- Symbol order: `graph` passes with existing unused-size warnings.
  `enemyMario` has zero new symbol errors against a fresh base object; its existing missing unused `TPathNode::getPoint` and seven unused-size warnings remain visible.

The first connected controller trial, `pa01_shadow_replay_bounds`, fails before reaching Shadow Mario: rc3/488 s, empty crash log.
Mario dies during the prerequisite Gatekeeper fight; the continuing approach then times out.
That failure neither exercises the corrected replay selection nor establishes a native engine crash.
The second trial, `pa02_shadow_replay_bounds`, uses ordinary START to skip the title attract movie; no developer movie override is enabled.
It passes normal File B load, the Gatekeeper fight, natural Delfino1/1 transition, Shadow Mario defeat and the original dialogue: rc0/537 s, script completion at tick15221, empty crash log and no active cheats.
The chase takes 3065 ticks and dialogue completion 65 ticks.
Mario ends with health8, three lives and the original one Shine; minimum health is 1 during the Gatekeeper prerequisite, so this is not a damage-safety or broad repeatability claim.
The old null-rail tick-skip does not fire.
This component does not enter Bianco or collect/save a new Shine, and script exit is not native Close acceptance.

The accepted public `tools/routes/pc_shadow_connected_unassisted.dev` has exactly the same operative command sequence as the private accepted script.
Run from `sunshine-pc` in PowerShell, with the existing locally generated corrected Delfino navigation grid:

```powershell
python tools/route.py Strict tools/routes/pc_shadow_connected_unassisted.dev --name <unique> --appdata-from pg1_airport_reload_20261016 --seconds 1000 "SUNSHINE_DEV_NAV=$PWD\build\PortGoal_20261016\dolpic1-corrected.nav"
```

Native executable SHA256: `30678140929e45d9a627d908b5ef503a031dd8ff95b8e2a2a5508f253e6de792`.
Public route SHA256: `1cbd376a82f13cf2638d0f5e9efbc41bd72aa59730af43cf261ab64913529876`.
All 41 native parser cases pass on this executable.
No developer stage selection, teleportation, direct aim, cheats, fixed seed, progress/resource writes or movie/timing override is used.

The separate enemy wrap-loop watchdog, broader content completion and Gelato mirror/Wiggler rewards remain open.
Matrix statuses remain 16/25 passed-bounded.
