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
The next candidate checks the expected scene at ramp retry points and trades shorter wash bursts for earlier ordinary FLUDD defense.

Normal third-Shine collection, save/cold reload, Ricco unlocking and later-world connected entry/completion remain open. Matrix statuses are unchanged.
