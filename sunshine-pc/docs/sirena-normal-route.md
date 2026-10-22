# Normal Sirena milestone

Normal pipe unlocking and entry are recorded. The complete Manta encounter, Shine40 save and a nineteen-Shine cold reload remain **open**.

## Earned entry

The starting profile is `bt47_turbo6_entry`, the eighteen-Shine card documented in [Bianco/Turbo progression](bianco-turbo-progression.md). It retains Rocket, Turbo and Yoshi rights. Its Shine IDs are 0/1/2/3/4/10/20/30/31/32/33/34/86/94/95/98/107/117.

Private `sm03_pipe_saved` preserves the result of normal Yoshi feeding, mounting, roof traversal and eating the Sirena pipe pineapple. An ordinary Pause Save records flag0x1038B. This is a copy of the earned save after that menu operation, with no progress or card edits.

- Latest File B: sector3/write28; the same eighteen Shines and nozzle/Yoshi rights.
- GCI SHA-256: `fda013ce271f8c28ecc09784259cf3fa9656958bb259f5458b0bb6a54854dde6`.
- All seven sector checksums pass.

Subsequent fresh-process sessions load that card through normal File B menus, traverse the roofs to the open pipe, select Sirena Episode1 and talk to the hotel owner. The original Manta introduction sets flag0x50007. Those entry prefixes are bounded evidence; later deaths or controller-script failures do not turn their complete sessions into passes.

`tools/routes/pc_sirena_entry_unassisted.dev` preserves this entry component. It requires the already earned pipe-open card; it does not automate the preceding Yoshi unlock. Its File B assertions retain all eighteen Shine identities, the previous world rights, Yoshi, Rocket, Turbo and flag0x1038B. It ends after the normal Sirena introduction, before combat completion.

From `sunshine-pc`, with the private earned profile available:

```powershell
python tools/route.py Strict tools/routes/pc_sirena_entry_unassisted.dev --name <entry> --appdata-from sm03_pipe_saved --seconds 600
```

## Bounded combat and recovery observations

The segmented ordinary-controller session `sm17_manta_air` defeats the original generation0 Manta and one of its two generation1 descendants. Read-only checkpoints then show one generation1 and three generation2 actors alive. That is partial combat, not victory. Earlier calibration `sm13_sirena_ground` reaches one generation3 plus75 generation4 actors, but also dies before completion.

Normal recovery is observed: the eight placed western shelter coins are collected, including a four-coin recovery from4 to8 HP. Spraying the original hidden coin46 at approximately `(-7268.6,307.5,737.2)` and collecting its drop raises1 to2 HP. A normal stamp opens the authored red1-Up box; its moving mushroom is not collected, so no successful 1-Up recovery is claimed.

The last `sm17` attempt ends in ordinary damage/death and a subsequent pause-condition timeout (wrapper rc3). Its crash log is empty. The GCI remains byte-identical to `sm03_pipe_saved`, with the original eighteen IDs and seven valid sector checksums. There is no Shine40 save or nineteen-Shine cold proof.

Controller calibration must distinguish aiming camera mode7 from ordinary mode0: the main stick aims in the former and moves Mario in the latter. Read the actual camera mode rather than inferring it from a Y press or camera position. Entering an uncleared electrical patch and waiting to aim can consume several HP before any useful shot.

Read-only investigation also rejects an apparent stale-enemy explanation: the previous scene's Manta spine timer and position remain fixed while current actors advance, and its manager is absent from the current Conductor lists. Readable old heap objects do not establish active enemies or a game-code defect.

## Build and evidence limits

Combat Strict executable SHA-256: `2beda12192b1ff36b5915bcea8b785ad03223bf2c22040664e31ec47035312f2`, from source at `dec4d91`. No game-code change is established by these combat calibrations.

The final rebuild reports `STRICT: 0 unresolved symbols`; all 74 port checks pass. Its executable SHA-256 is `e670f752182b1079f08a4dfd3f69d44e0f8739eb6b5556c945b82d1cd674d1a2`. The standalone public entry component passes as `sm18_sirena_entry_check` (rc0/206s, done tick5763), reaching Sirena Episode1 after the original introduction with health8 and no active cheats. The crash log is empty. Its eighteen-Shine GCI remains byte-identical to `sm03_pipe_saved`, and all seven sector checksums pass. This fresh-process entry check does not complete combat or establish a new Shine save.

The scripts use ordinary controller inputs. Conditions, logs, original collision geometry and process-memory observations only read game state. There is no developer stage start, teleport, automatic boss aiming, cheat or progress edit. Muted quarter-scale runs do not certify audio or final image quality.

Private failed approaches include ordinary combat deaths, a malformed condition and pause-resume timeouts. Their crash logs are empty. No undefined-behavior cause or completion pass is asserted.

## Remaining acceptance

Defeat the original Manta encounter, collect Shine40 normally, save through the normal menu, and independently cold-load the nineteen-Shine card. Retain the previous eighteen IDs, flag0x1038B and the earned nozzle/Yoshi rights; check all card sectors and compare the saved card before and after the cold reload.

## High starting ledge probe (not passed)

Two controller-only probes on 2026-10-22 test downward spray from the initial NPC platform rather than walking onto the arena floor. Original `sirena0` collision data shows a ramp in front of this position, with surfaces descending from 1000 to 900; the starting platform is not isolated from the Manta's approach.

- `sm21_manta_ledge` reaches four G0 hits. Mario initially remains at y=1000 with health 8, then contact throws him north from approximately z=-2373 to -3526 and leaves health 7. The subsequent aiming recovery fails during another damage state: health 6, camera mode 0 and a pitch-condition timeout (rc3). G0 does not split.
- `sm22_manta_ledge` immediately follows the same normal entry with a fixed southern aim and an uninterrupted 600-tick spray block. It reaches only one G0 hit, leaves health 1 and throws Mario far from the starting position, to approximately `(-70.8,1124.7,-7956.6)`. The read-only checkpoint still has one live G0 and no G1. The paused process is then stopped externally; script completion does not mean combat success.

Both crash logs are empty, no cheats are active, and both GCI files remain byte-identical to the earned eighteen-Shine pipe-open card above. Both attempts use the recorded `e670f752...674d1a2` Strict executable without game-code changes. The shared fifteen-minute budget includes entry and observation; calibration stops after the second attempt, after 11m43s overall. Do not repeat this stationary fixed-angle approach as a safe ledge strategy. These failures do not prove every possible ledge tactic impossible or establish a game-code/undefined-behavior defect. The complete Sirena milestone remains open.
