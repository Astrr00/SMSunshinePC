# Normal Rocket and Turbo milestones

## Scope

This milestone uses the normally earned File B, normal episode selection and ordinary controller inputs. No stage start, teleport, direct aiming, cheat, progress edit or save edit is used. The route driver observes game state to steer the controller. No original game code is changed.

## Earned prerequisite

`rt40_secret_plainjumps` starts from `sr51_yoshi_saved_seed1`, loads File B normally, selects Bianco Episode3, enters the Hillside Cave through its ordinary portal and completes the FLUDD-less secret (`stage=47`, `coro_ex1`). Spin jumps cross the early wooden steps and the two fixed field tiles. A spin/dive transfer reaches the broad wooden beam; ordinary running jumps cross the final moving star platforms. The original Shine2 reward is collected and saved before returning to controllable Delfino1/8.

Result: rc0 / 273 seconds, done at tick7766; sixteen Shines, health end8/minimum7, no active cheats and an empty crash log. All seven GCI checksums pass. File B advances to sector3/write22 and gains Shine ID2 while retaining the earlier fifteen Shines and permanent Yoshi unlock. Saved GCI SHA-256: `e1a276d7d02417791e6a0b7ae2e2e8ad553ffde4d41b5dee6bd70472d1f724e8`.

The accepted script is `tools/routes/pc_bianco_hillside_secret_unassisted.dev`, frozen privately as `build/rt40-secret-frozen.dev`. Calibration runs `rt31` through `rt39` ended at normal platform misses, failed landing assertions or timeouts. Their cards remain unchanged and their crash logs are empty. They do not establish undefined behavior or a missing game-code implementation.

## Rocket acquisition, save and use

`rt44_rocket_collect` cold-loads the earned prerequisite, asserts Shine2 and enters Bianco Episode4 through the normal portal and selector. The east hillside path, spin jump, wall kick and Hover transfer reach the valid Rocket box at `(12077,3409,6533)`. An ordinary descending jump opens it; Mario then walks to the thrown pickup at approximately `(12077,3389,6921)`. The original nozzle save prompt is accepted. A full analog R charge launches Mario from y3388 above y4600, followed by an ordinary landing.

Result: rc0 / 185 seconds, done at tick5221; health8, no active cheats and an empty crash log. Trace records selected nozzle1, the charge, upward movement to a recorded maximum y6047.7 and return to ground. All seven GCI checksums pass; File B advances to sector4/write23, retains the sixteen Shines and Yoshi, and gains the Bianco Rocket right (`0x10368`). GCI SHA-256: `0a135d2b96c71776dee804f6eed0a8d0c8c7dfbf9d78407acc6fe243164618d2`.

The accepted script is `tools/routes/pc_bianco_rocket_turbo_unassisted.dev`, frozen privately as `build/rt44-rocket-frozen.dev`. Earlier `rt41`/`rt42` miss the pickup; `rt43_rocket_bounce` observes the opened box and live item without collecting it. These are route calibration results, not demonstrated game-code failures. The added devscript output only reads the current nozzle and box/item state.

## Turbo gate: normal Episode6 prerequisite

Bianco Episode4 contains a valid Turbo box at `(9198.1,2800,-23109.9)`, but six original `Pile` actors block the cave entrance. Their centers run from x8700 to9700 at z-22900, y2700. The original `pile.col` spans y0..500 and their Y scale is2, giving tops at y3700. Episode4 (`bianco3`) and Episode5 (`bianco4`) each contain16 Pile placements; Episode6 (`bianco5`) contains10, with those six entrance stakes removed. A valid box does not establish normal reachability in Episode4. The [external location description](https://www.mariowiki.com/Bianco_Hills#Nozzles) also describes the box as visible in Episodes4–8 and accessible in Episodes6–8; scene data and native collisions provide the local evidence.

The necessary Episode4 red-coin Shine3 and Episode5 Petey-rematch Shine4 are now normally earned, saved and cold loaded. `bt47_turbo6_entry` completes normal Episode6 leaf-boat travel, spin-jump/Hover shore access, Turbo pickup and save; a full R use reaches speed85.6 with nozzle5 and consumes water. `bt48_cold_turbo` independently reloads the eighteen-Shine card and both nozzle rights (rc0/51 seconds, seven valid checksums, byte-identical GCI). [Controller sequence, hashes and explicit limits](bianco-turbo-progression.md#completed-episode6-turbo-milestone). Do not repeat the blocked Episode4 approach, bypass the stakes or edit progress. These are necessary shared-mechanic prerequisites, not a requirement to automate every Shine. Global Delfino nozzle unlocks remain outside this milestone.

Calibration remains explicit: `rt45_turbo_acquire` hits the island wall; `rt46_turbo_east` takes the open eastern water line and lands at the shore, but bounces on the Pianta. `rt47_turbo_hover_box` and `rt48_turbo_spin_box` time out at excessive route altitude assertions; the latter records a spin jump and Hover at y3403. `rt49_turbo_box_land` hits the stakes (minimum health7), `rt50_turbo_stakes_east` has insufficient idle-Hover momentum, and `rt51_turbo_running_hover` meets the cave's east wall. All retain the accepted Rocket card and have empty crash logs. The final diagnostic is frozen privately as `build/rt51-turbo-episode4-blocked.dev`. None of those Episode4 calibrations is an accepted Turbo acquisition/use result, a demonstrated game-code defect or evidence of undefined behavior.

## Build and route identity

The prerequisite run uses Strict SHA-256 `1682b34aa987b98b05cc6a4fc59f455026f027914139c3a4e2e9cc17bf66ec5c`. The Rocket run uses Strict SHA-256 `482251a8bf3ff4a462b825169c2d9502cd7347832a640ffd518002faa3d0562b`, with the additional read-only box/item output. Both report `STRICT: 0 unresolved symbols` and pass all 74 port checks. The game sources and gameplay behavior are unchanged.

The published prerequisite and Rocket scripts have normalized LF line endings and corrected header comments; their non-comment controller/observation commands are checked identical to the frozen accepted scripts. Published SHA-256:

- Hillside: `8bbfb60edea9b51cabfab8fd6e037268f5b9d85755a06bd8ea697152b9b4cc9b`.
- Rocket: `99337756e1296a22528eb29c870f9cb58bd76778247ba8a8d2f20570f3360650`.

## Independent cold reload

`rt52_cold_rocket` starts a fresh process on the final Strict build, normally loads File B, and asserts all sixteen Shine identities, prior world unlocks, permanent Yoshi and Bianco Rocket right. It returns to controllable Delfino1/8 (rc0 / 50 seconds, done tick1393), health8, no active cheats and an empty crash log. The GCI is byte-identical to `rt44_rocket_collect`; all seven sector checksums pass. Final Strict SHA-256: `ce365ea2156232d5f72591f582071ccf96cbe55b5d3038b879165c6e7811bf59`; Strict reports zero unresolved symbols and all74 port checks pass. The only rebuild difference after the Rocket run is source line-ending normalization; the gameplay code is unchanged.

## Reproduce

Run from `sunshine-pc`, with a new output name each time:

```powershell
python tools/route.py Strict tools/routes/pc_bianco_hillside_secret_unassisted.dev --name <hillside> --appdata-from sr51_yoshi_saved_seed1 --seconds 900
python tools/route.py Strict tools/routes/pc_bianco_rocket_turbo_unassisted.dev --name <rocket> --appdata-from <hillside> --seconds 600
python tools/route.py Strict tools/routes/pc_bianco_rocket_reload.dev --name <cold> --appdata-from <rocket> --seconds 240
```

Private profiles, screenshots, disc data and GCI files are not distributed. Each accepted milestone is one successful bounded run; this is not consecutive-repeatability proof, every nozzle interaction, global Delfino nozzle unlocking or a complete playthrough. Native runs are muted and use the CPU adapter at render scale0.25; they do not certify audio or full rendering quality.

The later Turbo cold check uses `tools/routes/pc_bianco_turbo_reload.dev` with `--appdata-from bt47_turbo6_entry`. It passes on Strict SHA-256 `2beda12192b1ff36b5915bcea8b785ad03223bf2c22040664e31ec47035312f2`, whose existing build reports zero unresolved symbols and passes all74 port checks. The earlier Rocket reproduction commands remain the evidence for their original sixteen-Shine profile.
