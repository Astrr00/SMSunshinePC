# README history

Moved out of `README.md` on 2026-10-17. Dated progress notes, kept verbatim; the live state is in
[`status.md`](status.md) and the [playability matrix](playability-matrix.tsv).

## Old README status prose (2026-10-16)

**Development resumed at the user's request (2026-10-16).** The [complete session handoff](sunshine-pc/docs/session-2026-10-16.md) remains the preserved stop-state record.
Normal Bianco/Petey progression and ordinary death/restart work have resumed; the intermittent enemy watchdog remains separate from ramp-input failures.
All full-port requirements and the external clean-Windows, hardware and licensing prerequisites remain open.


**Native launcher (`16d1c13`):** an ocean-themed home inspired by [chasem-dev/sms-launcher](https://github.com/chasem-dev/sms-launcher), with original coastal art, a large Choose disc image/Play action, real disc readiness and six grouped settings pages. The existing C++/RmlUi stack remains; no Electron or upstream/Nintendo images are added. Native picker, invalid/valid/stale images, cold preferences, high-DPI focus scrolling and normal game startup are observed through application input replays and native AX; physical mouse/gamepad acceptance remains separate.

**Current verified native runtime `16d1c13` (2026-10-16):** full verification passes 7 gates and 63 host checks, Strict zero unresolved and a 45-second boot. The independent source checkout also builds the launcher after fast-forward. Packaging-only `11a1304` preserves these native sources/binaries and passes its focused regression in both checkouts.
Aurora remains `50e5be3` plus 18 tracked patches. The isolated core builds in 1282 steps/407.70 seconds; its later launcher-only rebuild takes four steps/6.69 seconds. Fifteen dependency-content directories, including prebuilt Dawn/SDL3/nod, are reused; no Main game objects or binary are copied. These are not fresh-download, clean-PC or speedup claims.
Native computer capture observes Auto-resolution original Options and live Delfino; ordinary native window Close exits 0 and a new process preserves Mono, Rumble-off and the genuinely earned two-Shine File B.
These checks do not prove an unassisted playthrough, physical-controller/audio/hardware-GPU acceptance or a clean-PC package.

**New bounded gameplay evidence (2026-10-16):** a fresh input-only file earns the airport Shine. Its legitimate saved continuation now defeats Delfino and Shadow Mario, enters Bianco normally, defeats the real first mission boss, collects event 0 and saves/returns to Delfino 1/5 with two Shines.
Corrected core `38338b4` completes `pc_early_bianco_unassisted.dev` in 936.15 seconds, end tick 20102, with precisely earned IDs 0/86, save count 2 and all seven GCI checksums valid. The preceding snapshot's cold-load/native-Close evidence remains separate. No gameplay aids, devstart, seed, direct state writes, movie skipping or timing overrides count as normal progression; concurrent WARP runs are not timing benchmarks.
The [content inventory](sunshine-pc/docs/content-coverage.tsv) derives 120 Shine identities and 240 observed persistent blue-coin identities. Generated runtime rows remain open; selected executed paths are recorded in the playability matrix, not treated as all-content proof.
Ricco's native SmallEnemy model-table overread is fixed: diagnostic scene loading, ordinary walk/jump/return and normal window close pass on WARP. Developer scene selection is not normal unlocking or boss-victory proof.
For visual checks use launcher `Resolution scale: Auto`, or remove the route tool's `SUNSHINE_RENDER_SCALE=0.25` override with `SUNSHINE_RENDER_SCALE=`. Auto/window rendering is visibly sharper; the diagnostic quarter-scale image is not representative of normal display quality.
Format-1/2-bit ADPCM now decodes real game voices instead of ending them as silence. The original map1 shine-shadow volume loads its validated geometry from the user's supported DOL; no game bytes are embedded.
Known remaining risks include later normal progression, full shared-system coverage, an early WARP watchdog, unverified full-game audio/graphics and the unidentified cause behind broad zero-allocation mitigation.
The camera now keeps desired, height-panned and smoothed positions distinct. The shared Walker clamp no longer snaps falling enemies down from above the floor. Private diagnostic Petey combat reaches HP3 to0, reward1 and save/return, but its harness ends rc124 during the final wait; normal windmill/intro connection remains unproven.
Original Options now retains the missing native console sound bit through ordinary native Close. Script `end` still uses `std::_Exit` and bypasses that orderly write; abrupt termination, repeated lifecycle and human/device acceptance remain separate.
Normal earned File B also reaches the actual Bianco STORY2 selector and controllable raw scene 2/1 on the three-fix candidate (exit 0, 138.40 seconds). The card and native preferences remain byte-identical. This proves selection/entry, not Petey's reward or the whole game.

**Next progress blocker:** the ordinary next-episode route reaches the windmill bank. The repeatable ramp timeouts are goop-slope slides (`OIL_SLOPE`, `MarioMove.cpp` `checkGraffitoSlip`); stand-and-spray input now climbs to z -1039 / y 3924 of the roughly 4096-high ramp head, the top is not reached yet. Separately, one earlier run hit a native no-frame watchdog. Actual PE/COFF bytes uniquely bind the caller to `TSpineEnemy::zigzagToCurPathNode`, inside its `MsWrap<float>` call; the actual float inputs and upstream cause are still under investigation. No wrap guard, route bypass or unauthenticated PDB field read is used.
Normal scene-cycle calibration passes: earned File B enters real Bianco STORY2, uses original pause ExitArea, saves in Delfino, returns to title and reloads (exit 0, 393.60 seconds).
A following actual **31m 14.686s wall-time** run completes three such scene/save/title/load cycles and exits 0 through the exact native window Close. A fresh process loads its File B and also closes natively with exit 0. Exactly IDs 0/86/save5, all seven GCI checksums, all persistent flags/21 saved integers and native settings survive that new process; only save count increases from 2 to 5 across the three normal saves.
This is contended quarter-WARP functional stability, not original-speed, image-quality, listening or hardware acceptance. A separate fresh-profile airport replay now completes ordinary death and restart by input only (exit 0, 416 seconds): health 8 to 0, lives 3 to 2, original restart at stage 0/scenario 0 with health 8, and the stick moves Mario afterwards. Its script exit is diagnostic, not a native Close. The next-episode enemy watchdog remains unresolved.



## Old packaging notes

Packaging safety also rejects supported WIA/GCZ images, including mixed-case names, from both the staged folder and ZIP. `python sunshine-pc/tools/package_test.py` passes; an actual isolated packaging smoke refuses synthetic contamination and leaves no ZIP. This is not Release/licence/clean-PC approval.
Pinned-source notices are also corrected: actual packaging now copies 20 licence texts, including the FreeType FTL/drivers and RmlUi containers, with complete pinned Dawn/nod/DXC dependency notices. The isolated actual CLI passes source/stage/ZIP/checksum checks (134 files, 11 DLLs); its temporary internal ZIP is removed. Own-code/artwork licensing, Microsoft redistribution compliance and exact signed-DXIL provenance remain unresolved; no Release approval is inferred.




## Dated updates

### Update 2026-10-12 (measured): first full chain logos -> Bianco -> Bianco Shine -> save -> reload

Route `sunshine-pc/tools/routes/60_acceptance.dev`, run twice in a row (`h01`, `h02`): rc=0, 144 s and 147 s wall time, no `overrun`, no crash, 13 PNGs each (airport0 `ap0.png`, Delfino `dolpic.png`, gate `gate.png`, Bianco `bianco.png`, Shine get `shine_get.png`/`shine_get2.png`). Stages seen in `run.log`: 15 (option map) -> 0/0 -> 0/1 -> 1/0 -> 1/1 -> 2/0 (Bianco, `dir.state=4`, Mario steerable, status `0x0C400201`) -> 1/5. `shines=2`, `.gci` PASS (`tools/gci_check.py`), reload with `tools/routes/61_reload.dev` (file block B, real time): `stage=1 scen=5 shines=2`, rc=0 (`h05`).
Command: `python tools/route.py Strict tools/routes/60_acceptance.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`.
**What is NOT retail input (the chain is dev-assisted, so the acceptance wording "no dev switches" is not met):** `SUNSHINE_DEV_STEP_RETRACE` (logic faster than 30 Hz; the 144 s is not 30 Hz wall time), `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST` (faces the head, sets nozzle pitch, forces the crouch; the Delfino spray gate), script verbs `cheat infinite_health/infinite_water`, `tp` (pit/boss/gate positions), `tpem` (shadow-Mario chase), `shine 2` (touches the Bianco 100-coin Shine). Real: airport boss killed with water via assist + short `hold L` cycles, airport Shine taken by walking to it and pressing A, Delfino GateKeeper killed with water (flag `0x10384` in the log), gate walk-in, Bianco load.
Bug found and fixed on the way: `JAIData::initSeqParameter` left `mSeqHandle` 0 on PC, so `JAISeqEntry::storeBuffer` stopped the SE master sequence and `processFrameWork` crashed at the airport exit (session log section 38).

### Update 2026-10-16: collision-square exceptions and compiled helper reviews

Two additional independent agents complete the bounded `getGravityDir`/`calcBathtub` compiled-PPC reviews.
`getGravityDir` needs no correction: all 143 instructions preserve the live operand graph and returned FP status,
assuming the same PPC estimate and math-library implementations.
Source [`d33c1ef`](https://github.com/Astrr00/SMSunshinePC/commit/d33c1ef) fixes `calcBathtub`'s conditionally skipped inner-radius square.
A plain reused local restores retail's unconditional operation; no volatile access or extra wrapper is needed.
Real production-method fixtures reproduce missing inexact/underflow flags before and restore them after;
the exact control, count and accumulated position remain unchanged.
Final probe-free WARP Corona route 98 passes (37 s); full verification passes 7 gates and 63 host checks.
The isolated checkout also rebuilds `d33c1ef` with zero unresolved symbols.
Status-enabled Null progression reaches Bianco with one Shine; independent seven-block GCI validation and real fresh-process reload pass.
All 681 ready-scene status ticks include JPA statistics, with no overlap with 61 unavailable-scene ticks.
These progression/reload regressions use documented dev aids, not an unassisted acceptance claim.
These bounded IEEE/masked-exception proofs do not certify physical Gekko execution, host-wide FP equivalence or whole-`perform` integration.
Next: compose the corrected helpers and both initialization paths into a final whole-`perform` FP review.

### Update 2026-10-16: bath-water spawn FP corrections

Source [`3d095fa`](https://github.com/Astrr00/SMSunshinePC/commit/3d095fa) fixes two independently reviewed spawn gaps.
Three pre-center scalar calculations preserve retail's NaN operand priority without an extra helper call.
The existing `get_float(0.0f, 10.0f)` wrapper restores multiplication followed by an exact addition,
clearing `FI/FR` as retail does even when drop capacity rejects the spawn.
Actual emitted-PPC instruction replay passes 18 NaN payload/sign/signaling cases; the previous object differs in 12.
Two exact finite rounding witnesses establish the velocity status gap; these are architecture-model checks, not PPC hardware execution.
Null and WARP probes observe two successful position calculations and capacity rejection (`active=0`, `capacity=0`);
their velocities match independent LCG decoding bit-for-bit. Final probe-free WARP combat smoke passes (29 s, tick 812).
Full verification passes 7 gates and 63 host checks; the isolated checkout of `3d095fa` rebuilds with zero unresolved symbols.
The shared X/Z zero product needs no new wrapper; whole-function transitive FP equivalence remains partial.
Next: compiled `getGravityDir` and `calcBathtub` reviews under identical masked-exception PPC controls.

### Update 2026-10-16: compiled `setLookDir` FP review

Two independent reviews plus an instruction-level check close the corrected helper's PPC review.
All 120 helper rows agree after explicit register, ABI-stack, constant and local-branch normalization;
the actual compiled bath-water `inv_sqrt` also agrees with all 13 retail rows and constants.
Fused operation order, epsilon branches, signed-zero behavior, input-read/store ordering and translation preservation agree.
The ledger returns this corrected function to `logic-verified`; no game source or matching percentage changes.
This proof assumes valid ABI memory, identical PPC FP controls and non-trapping execution.
It does not certify physical trap-handler context or universal x64 rounding, NaN-payload and exception-state behavior.
Real production-header smoke passes 37 invariant checks in both optimized and Strict-style host builds.
WARP Corona route 98 passes (36 s, tick 1024); full verification passes 7 gates, 63 host checks and the 45-second boot.
That helper-only milestone left unconditional bath-water `perform` FP-state equivalence open; the later spawn correction is above.

### Update 2026-10-15: natural ending, final save and real reload

Two independent reviews map the film/save contract and the card bitmap.
`corona_ending_save_20261015` completes epilogue (14), credits (15) and the normal ending (16)
with observed natural THP state 3 at each boundary, without movie skipping or buttons during those films.
Real LEFT/A confirmation saves the final Shine to the previously empty, isolated file A; the run returns to
the option map (rc 0, tick 13167). Independent validation passes every GCI checksum and finds the completion bit
in the newest valid sector; the existing file B and original baseline card are unchanged.
After removing all source probes, `corona_ending_reload_fixed_20261015` starts a new process and loads A
by normal PAD input (rc 0, tick 4456, Delfino 1/2, one Shine, explicit `flag=0x10077` assertion).
The initial reload probe timed out because it waited for global THP state 0: Plaza deliberately loops a TV movie.
Reload synchronization instead confirms prompts with real PAD input until the live gameplay director is ready.
The general slot-B route also passes on WARP after this fix (rc 0, tick 10009, Delfino 1/0, one Shine).
Final verification: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, zero unresolved, unchanged matching SHA-1
and a 45-second WARP boot. The existing isolated-checkout proof applies to the unchanged compiled game source.
This is not a switch-free fight or a full-game playthrough; the 120-Shine ending, human listening and hardware GPU remain open.

### Update 2026-10-15: Corona five-platform progression

Two independent retail-assembly reviews confirm the weak/hard collision distinction and fifth-hit ending predicate.
A normal ground-pound produces message 1 without breaking a platform; five real rocket launches and L attacks
produce message 3 on distinct fragile pieces, counts 0 through 4, then `demo=1` and the movie transition.
The uninstrumented executable repeats that transition with movie skipping disabled
(`corona_finale_uninstrumented_20261015`, rc 0, stage 15/0 with THP state 2, tick 1852).
The captured early movie frame shows blue sky and letterboxing, not proof of the complete epilogue or credits.
These are diagnostic runs: teleports supply platform travel, cheats supply health/water, and step-retrace is enabled.
Each rocket attack starts after landing; no supplied attack altitude, direct damage, forced nerve or victory flag.
The earlier fixed-timing probes missed platforms or landed ordinary attacks despite rc 0; they do not count as wins.
No game-logic fix was needed. Temporary source probes/routes were removed; the original source passes
7 verification gates, 63 host checks and the 45-second WARP boot, with unchanged matching SHA-1.
Unassisted combat/navigation, credits/save completion, sound listening and a hardware GPU remain open.

### Update 2026-10-15: Corona water depth and reflected view basis

Before this correction, 900 water renders leave the CPU depth buffer and generated heights all zero.
Aurora patch 0018 adds genuine Z24X8 conversion and guarded canonical tile readback; bath water now uses
an unscaled single-sample prepass, valid initial far-depth tiles and explicit big-endian decoding.
Assembly review also found a real `setLookDir` bug: `X cross Z` reflected the view; retail uses `Z cross X`.
Backface culling remains enabled after that root fix. WARP now renders visible waves.
Independent decoding verifies 28 224 generated heights at scales 0.5 and 0.25, including expected cap depths
0.75 and 0.875; the existing R8 goop-cleaning route also passes.
Readback deliberately retains the port's previous-copy transport (observed 2–4-copy latency).
The host regression fails before the cross fix; the final FP instruction-equivalence claim remains partial.
At `b1392e5`, full verification passes: 7 PASS, 0 FAIL, 1 optional SKIP; Strict links with zero unresolved,
63 host checks pass, matching SHA-1 is unchanged, and the 45-second WARP boot passes.
The separate isolated checkout applies all 18 Aurora patches and rebuilds/links the affected header users successfully.
The final uninstrumented executable also passes Corona route 98 on WARP (37 s, tick 1023), status-enabled
route 85 on Null (471 s, Bianco 2/0, one Shine, seven-block GCI PASS) and route-86 reload
(16 s, Delfino 1/0, Shines 0 -> 1). These are dev-assisted runs, not a complete Corona fight.

### Update 2026-10-15: Corona static depth texture

The stage-60/0 format-22 abort was a valid, code-defined `GX_TF_Z24X8` 4×4 depth texture,
not a broken archive texture. Aurora patch 0017 sizes and decodes its 64-byte tiles.
`SUNSHINE_DEV_START=coronaBoss/0` now resolves the exact archive basename.
The pre-fix Null run aborted at tick 109; the patched Null probe completes at tick 705.
The WARP probe completes at tick 375 and captures a visible Corona boss and HUD frame.
Regression route `tools/routes/98_corona_depth.dev` also reaches live gameplay and captures a WARP boss frame
(rc 0, tick 393). Full verification: 7 PASS, 0 FAIL, 1 optional SKIP; the updated isolated fresh checkout
builds and links the current source with all 17 Aurora patches.
This proves scene entry and rendering, not correct depth readback, fight completion or audible bath-water cadence.

### Update 2026-10-15: bath-water sound cadence and EventWatcher reviews

Bath-water sound was incorrectly gated to every fourth move tick; the sound block now follows retail on every move cue.
The rebuilt matching object changes exactly one branch target; its fuzzy score remains unchanged, so no matching gain is claimed.
Finite spawn-FP/RNG equivalence is reviewed; NaN-payload/FP-state equivalence remains partial.
EventWatcher warp review is confirmed, and the already byte-identical Appear8 function now has a corrected ledger row.
Matching SHA-1, strict zero-unresolved link, 62 checks and a 45-second WARP boot pass.
The final `cd552f5` source also passes a separate clean-source-checkout Strict rebuild with zero unresolved symbols.
Status-enabled route `bath_sound_fix85_20261015` completes on Null (368 s wall, tick 17790,
Bianco 2/0, one Shine); live particle status remains available and the seven-block GCI checker passes.
This route does not enter Corona and cannot prove the bath-water sound audibly.
The earlier diagnostic fix is published as [`3c10554`](https://github.com/Astrr00/SMSunshinePC/commit/3c10554);
[issue #12](https://github.com/Astrr00/SMSunshinePC/issues/12) is closed, including a fresh-checkout build gate.
A pre-fix Corona dev-start probe reaches stage 60/0 but aborts on texture format 22 before sound observation
([issue #13](https://github.com/Astrr00/SMSunshinePC/issues/13)); no audible Corona comparison is claimed.

### Update 2026-10-15: matching refresh and bath-water audit

Full report refreshed: 11 323/12 881 functions byte-identical, up six from the previous full measurement.
Bath-water collision processing now includes the first reserve-drop pair, as retail does, in both simulation call sites.
Matching SHA-1, strict link, 62 headless checks and 45-second WARP title boot pass; no Corona fight comparison was run.
A fresh checkout also builds and links with unresolved symbols disabled, using cached dependency sources and packages.
Route 85 also passes on Null with step-retrace, skipped movies, aim assist and automatic status logging disabled:
Bianco stage 2/0, one Shine saved, GCI checker PASS (230 s wall).
The earlier automatic particle-status crash ([issue #12](https://github.com/Astrr00/SMSunshinePC/issues/12)) is corrected
by checking scene ownership and post-setup readiness before diagnostic reads; published in `3c10554`.
Status-enabled route 85 now passes on Null (364 s, 650 s limit, Bianco 2/0, one Shine, GCI PASS), followed by route-86
reload (17 s, Delfino 1/0, Shines 0 -> 1). The first 330-second attempt timed out without crashing.
Renewed whole-function review confirms `initializeIfYet_`; `perform` spawn-position FP review and Corona comparison
remain open. [`decomp-rounds.md`](sunshine-pc/docs/decomp-rounds.md) records the evidence and limits.

### Update 2026-10-12 (later): route 70, fewer dev crutches

`sunshine-pc/tools/routes/70_boss_until.dev` run twice in a row (`u01` 153 s, `u02` 193 s wall, rc=0, no `overrun`, no crash, 13 PNGs, `gci_check.py` PASS, reload `stage=1 scen=5 shines=2`). Remaining dev: `cheat infinite_health`, `chase` (position read, real input), `shine 2`, env `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST`. Gone: `tp`, `tpem`, `killboss`. Not met: a real Bianco Shine without `shine 2` (no measured floor path to any of the three Shines), 330 s at real 30 Hz, no dev switches. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 42.

### Update 2026-10-12 (decomp round 6): 11 282 / 12 881 functions byte-identical (87.6 %)

Measured after round 6: SHA-1 unchanged, 11 282 / 12 881 byte-identical (87.6 %), matched code 61.98 %, fuzzy 98.60 %, 445 / 736 units, strict link 0 unresolved, Aurora 11 patches, `TEMP-DEBUG` 0, route 70 smoke 1 of 2 complete (`p01` chase fall, the known flake). Details: `sunshine-pc/docs/session-2026-10-11b.md` section 52.

### Update 2026-10-12 (decomp round 5): 11 247 / 12 881 functions byte-identical (87.3 %)

Measured after round 5: SHA-1 unchanged, 11 247 / 12 881 byte-identical (87.3 %), matched code 61.48 %, fuzzy 98.27 %, 445 / 736 units, strict link 0 unresolved, Aurora 11 patches, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 50.

### Update 2026-10-12 (decomp round 4): 11 191 / 12 881 functions byte-identical (86.9 %)

Measured after round 4: SHA-1 unchanged, 11 191 / 12 881 byte-identical (86.9 %), matched code 60.69 %, fuzzy 98.25 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 49.

### Update 2026-10-12 (decomp round 3): 11 112 / 12 881 functions byte-identical (86.3 %)

Measured after round 3: SHA-1 unchanged, 11 112 / 12 881 functions byte-identical (86.3 %), matched code 59.67 %, fuzzy 98.23 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 47.

### Update 2026-10-12 (decomp pass): 11 020 / 12 881 functions byte-identical

After a 12-agent decomp pass (details in `sunshine-pc/docs/session-2026-10-11b.md` section 44), measured by the lead: DOL SHA-1 unchanged, matched functions 11 013 -> 11 020, units 445 / 736, fuzzy total 98.12 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Several real behaviour bugs fixed (Koopa nerves, BathtubKiller, bossgesso, ModelWaterManager, fenceMove, Kumokun, GCConsole2).

### Update 2026-10-12 (decomp round 2)

Gates: SHA-1 unchanged, 11 020 / 12 881 functions byte-identical, 445 / 736 units, fuzzy total 98.23 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Route 70 smoke after the edits: 3 of 6 runs complete; one chase fall, one rare crash in the shadow-Mario replay (`TEnemyMario::emReplayJumpToNearestNode`, run `v02`, cause open). Details: session log section 45.
