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
