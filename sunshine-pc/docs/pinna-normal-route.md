# Normal Pinna milestone

Pinna entry, Mecha Bowser completion and its saved/cold-reloaded Shine remain **open**. The normal cannon prerequisite is ten distinct earned Shines plus the Ricco/Gelato unlocks. The following accepted controller-input prerequisites extend the existing six-Shine save; they do not certify Pinna combat.

## Earned prerequisites (2026-10-19)

Start from `nw91_gelato_early_takeoff`, the normal six-Shine File B documented in [normal-world-routes.md](normal-world-routes.md): IDs0/1/10/20/86/117, GCI SHA-256 `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.

1. `pc_delfino_crates_first_unassisted.dev`, accepted private `pn12_crate_first`: rc0/149.44s, ten crates broken with 7.35 seconds left, ordinary Shine94 collection/save/plaza return, health8, empty crash log and no active cheats. Latest File B write9 has seven Shines: IDs0/1/10/20/86/94/117. All seven GCI sector checksums pass; SHA-256 `fe7f0b046a47cbea85302ce00c41bfbf9805450014c87536db0796628927edcd`.
2. `pc_delfino_crates_second_unassisted.dev`, accepted private `pn14_crate_second`, seeded from that saved result: rc0/146.35s, ten crates broken with 10.59 seconds left, ordinary Shine95 collection/save/plaza return, health8, empty crash log and no active cheats. Latest File B write10 has eight Shines: IDs0/1/10/20/86/94/95/117. All seven GCI sector checksums pass; SHA-256 `cfcee4186e7e85c998e8fd1831cfcc69f4de588e7aa7db6cff04bc0f03c8105c`.

The second run cold-loads the first reward through normal menus. The next private roof trial `pn15_roof` independently cold-loads all eight Shines before its unverified traversal; that initial load is evidence for the saved second reward only.

The accepted Strict executable SHA-256 is `4bc903e7a254f9782f8e52d561dfc373a5dbc55eb2d74f2b81424ba1407b55af`, built from game source at `99dca5dd` with read-only route diagnostics. Strict reports zero unresolved symbols; 68 port checks, all 46 native parser cases (including five new bell cases) and 37 coaster-aim checks pass. Later documentation-only changes do not alter this binary.

Accepted private route hashes: first `2152f18d4f41fa3971b8c23c5a8b5262b6e80cd3ee54c7d89641bcd9b31b0adc`, second `58f550f92cacad22158d52a950b02ac0d227bf9464d0843ed0e44fea1681bf9a`. Public copies change only comments; executable controller commands match the accepted inputs.

## Reproduction

Run from `sunshine-pc`, choosing fresh output names:

```powershell
python tools/route.py Strict tools/routes/pc_delfino_crates_first_unassisted.dev --name <first> --appdata-from nw91_gelato_early_takeoff --seconds 480
python tools/route.py Strict tools/routes/pc_delfino_crates_second_unassisted.dev --name <second> --appdata-from <first> --seconds 360
```

The crate-room approach crosses the canal at x-2500 and enters the street opening at z-5500. A normal circle-stick spin jump is followed by facing the next crate before takeoff. Every layer of the second game's stacked columns receives its own jump/ground pound. Early braking before the reward prevents overshooting its pickup volume.

Earlier private failures remain preserved: canal/door geometry, insufficient jump height, spin-facing errors, collection overshoot and the second layout's unbroken lower layers. They timed out without a nonempty crash log. No undefined-behavior cause is asserted. These routes write only ordinary PAD inputs; conditions, actor lookup and logs observe state. No teleport, developer stage start, automatic boss aiming, cheat, save/progress edit or movie/timing override is used. Quarter-scale muted WARP runs are functional evidence, not image-quality, listening or physical-controller checks.
