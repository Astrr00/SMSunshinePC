# Port status audit

Scope: Port.md phases 1-8 and §5-§6 features, plus the Goal-Port §6 acceptance criteria. Read-only audit by PortAudit. No game runs, no builds, no edits outside this file.

Legend: **Done** = verified by observed artifact. **Partial** = some code or verification exists, gaps remain. **Not started** / **Open** = no working implementation or known blocker. `[INFERENCE]` = not observed directly.

## 0. Lead update (later than this audit; supersedes the rows it contradicts)

**Session paused at the user's request (2026-10-16).** Read [session-2026-10-16.md](session-2026-10-16.md) for the complete retained goal, source/binary identities, private evidence, all outstanding tasks and exact restart instructions. All owned native tests/observers and agents are stopped; desktop control is released. No full-port completion is claimed.

### Update 2026-10-16: gameplay-first goal and current-source baseline

The retained user goal is a fully playable native Windows port, including normal new-save progression through credits/postgame and all original content; development is paused, not completed.
100% byte-identical PowerPC matching is no longer the primary goal or a release prerequisite.
The [playability matrix](playability-matrix.tsv) assigns concrete early blockers, reproduction paths, owners and acceptance conditions.
Historical developer-assisted achievements below do not certify unassisted progression.

The four already pending pane/boss source-form candidates are preserved in `154d9f5`; no further codegen-only matching work is prioritized.
Native runtime source `16d1c13` passes `verify --build --matching --boot-secs 45`: 7 gates, 63 host checks, zero unresolved symbols, 45-second boot and the unchanged reference SHA. Clean-Aurora core source `0570182` builds in 1282 steps/407.70 seconds; its independent checkout fast-forwards to the launcher source and rebuilds four steps/6.69 seconds using its own earlier objects and documented dependency-content caches, never Main game objects/binary. Packaging-only `11a1304` passes its focused regression in both checkouts without changing or rebuilding native code.
`ninja changes_all` reports fuzzy-only improvements; the complete set of 11 323 exact function identities and sizes is preserved.
Matched code remains 2 249 348 of 3 590 088 bytes; matched data remains 560 095 of 640 331 bytes.
No new byte-identical function, code-byte total or data-byte total is claimed.

The user's immediate native-launcher request is implemented in `16d1c13`, inspired by [chasem-dev/sms-launcher](https://github.com/chasem-dev/sms-launcher) while keeping C++/RmlUi/SDL, GMSJ01 validation and existing settings. Original coastal art, a gold Choose/Play action, real readiness/error states, six grouped panes and Cancel-first confirmation are actually rendered. Native picker/Cancel, invalid `&` path, valid image and disappeared owned hardlink, cold volume75%, 640×480/UI150% focus scrolling, normal Nintendo Presents startup and native Close0 pass in isolated profiles. Input replays and dialog AX are not physical mouse/gamepad/hearing acceptance; foreground-control approval timed out, no grant was assumed and no takeover occurred.

A subsequent packaging-only correction adds missing supported WIA/GCZ extensions to the existing staged-folder/ZIP exclusion policy. The focused stdlib regression passes; actual CLI-main packaging with real DLL/licence/cache resolution creates a clean 21.1 MiB internal ZIP, then refuses synthetic WIA/GCZ contamination with no ZIP left. The temporary mirror is removed. RelWithDebInfo is explicitly not Release/licence/clean-PC approval; native runtime source/binary and matching scope are unchanged.
Pinned-source licence closure is also corrected: 20 copied texts retain full FreeType FTL/BDF/PCF, RmlUi containers and Dawn/nod/DXC dependencies instead of summary-only notices. The final actual isolated package CLI exits0 with134 files/11 DLLs and source/stage/ZIP/checksum equality; its internal ZIP SHA256 is `820a92e4c828e310fa68f8aa16893b86407638bcd712578ea305d5c44e188e53`, then temporary output is removed. Native game code/flags/binary remain unchanged. Own-code/artwork licensing, Microsoft entitlement/compliance and exact upstream signed-DXIL provenance remain unresolved; a matching SDK executable body is not a distribution grant.
Final source sealing normalizes only 22 copied nod-notice CRLF sequences to LF, preserving all notice text. Actual package-main is re-exercised on those final Main files without a build/game run; 134 files/11 DLLs/20 notices and source/stage/ZIP/checksum checks pass. Final internal ZIP SHA256 `d33d55e14351ee2b12d70129a1056a9d88c1b1fcfc77223fdf3d461577f8e131` supersedes the earlier private checksum above. Temporary output is removed, metadata remains in `build/SessionHandoff_20261016/notice-package-seal.json`, and all legal/Release gates stay open.
Normal lifecycle calibration `pg9_pause_save_cal_20261016` exits0 in211.46 seconds: genuine File B load, pause/B resume, original Save, normal post-save title return and same-process reload. `pg9_scene_cycle_cal_20261016` also exits0 in393.60 seconds after actual Bianco STORY2, original pause ExitArea, Plaza Save, title return and reload. Both diagnostic-exit calibrations preserve IDs0/86/save3 and seven checksums.
The subsequent `pg9_lifecycle_30min_20261016` completes three ordinary Bianco/ExitArea/Plaza Save/title/reload cycles, script end17298 with diagnostic exit disabled, and remains alive for1874.686 seconds before exact owned native AX Close. Actual surface and window absence are observed, process exits0/AURORA_EXIT. The card has exactly IDs0/86/save5, seven valid checksums and SHA256 `4f258d289914783426e0daeba81d8154b895d1872bcbde6a530b37f2a185ba8a`; all earned flags/20 other saved integers and native INI are unchanged from the original two-Shine profile.
A new process, `pg9_lifecycle_cold_20261016`, executes original Options/normal File B loading to live Delfino1/5 and native Close0/AURORA_EXIT. The card, all persistent flags/21 saved integers and native INI remain byte-identical to the long-run result. This proves cold persistence, not new Shine collection.
The long run uses contended quarter-scale WARP; no original-speed, image-quality, listening or hardware inference. Its exit reports 2 audio underruns/4 overflow drops/0 device failures; pending buffer maps and Device destroyed are logged at normal teardown, not suppressed. Separate fresh-profile death run1 actually shows health8 to0/lives3 to2/Director7 at8700 and original restart to0/0/health8 at8790, but its boss-center walk times out and exits rc3. Run2 remains on a cleaned patch and does not die; corrected perimeter run3 is interrupted before its hazard section, then closes natively0 at the user's stop. The revised replay is not accepted; all three private runs remain preserved.


Current core fixes distinguish desired/height-panned/smoothed camera positions, restore the shared Walker `candidateY <= ground+0.05` landing predicate, and retain the native console Mono bit through orderly exit.
Corrected core `38338b4` passes the full ordinary earned-file route through Delfino/Shadow/natural Bianco entry/mission0/event0/save/return: `pg7_main_normal_bianco_20261016`, rc0/936.15 seconds, live Delfino1/5 at20042/end20102. The card has exactly IDs0/86/save2, all seven checksums valid, SHA-256 `ef990d01385c306f7182c2690f1b40833159e0908614eb3bc9e008616599ec22`. This is quarter-WARP functional proof before the UI-only change, not a performance or physical-input result.
Main Auto-resolution `pg6_main_camera_auto_20261016` passes descent/jump/settle in 121.55 seconds; actual and requested target Y agree at 2922.7 for Mario Y2762.7. This is diagnostic scene control, not normal unlocking.
Actual original Options and native Close pass on the preceding two-fix Main snapshot in `pg6_main_options_mono_20261016` (64 seconds); cold original Options, normal earned File B load and native Close pass in `pg6_main_options_cold_load_20261016` (151 seconds, including observer interaction).
The original Mono/Rumble-off UI and live Delfino surface are captured through native computer helpers. Precisely earned IDs 0/86, save count 2, all persistent flags/21 saved integers and native preferences survive; only option offsets 0x1888/0x203C/0x203E change from the pre-options card. Cold GCI/INI remain byte-identical.
Script `end` is diagnostic `std::_Exit`, not an orderly-exit proof. The above native Close uses the exact owned titlebar control and observes exit 0/AURORA_EXIT; no original saves or unrelated windows are touched.
The three-fix Main carrier SHA-256 is `47c88ea6e637499868f213458cef87ef68c333e37e8633e11700fe7fa189111a`. Ordinary earned File B reaches actual STORY2, exercises LEFT/STORY1/RIGHT/STORY2/A, and enters controllable raw 2/1 at tick 2767 (`pg7_bianco_episode2_entry_20261016`, exit 0, 138.40 seconds, end 3127).
That run preserves GCI SHA-256 `b16fc7f868f8a63dace6ca99a72cb47c2d7c1e767d71a8cdf3640f148f796c94`, every earned flag/integer and the native INI. It proves next-episode selection/entry, not Petey victory/reward.
Walker private before/after evidence restores live airborne fall and landing instead of an above-floor snap/death. The subsequent diagnostic arena run reaches real HP3 to0, collects reward1, saves and returns at7940; harness rc124/331s occurs during its final post-return wait, so no passing full-route/native-Close claim. Its diagnostic File A has only ID1; genuine File B IDs0/86 remain unchanged, not a three-Shine checkpoint. Normal windmill/intro connection and whole-function camera/Walker PPC/FP equivalence remain open; both ledger rows stay partial, with no exact-byte promotion.
The source-authenticated geometry discriminator executes three diagnostic runs, including opt-in unfilled allocations with allocation logging disabled. All render normal world geometry; first-two pollution-draw producer/FIFO/actual staging position, index and referenced immediate footprint-matrix payloads agree. The inherited indexed matrix binding is not referenced by those draws. A separate uninstrumented current carrier also renders normally in one calloc and three fresh unfilled runs, with all four later PNGs byte-identical; parent binds actual carrier and all eight frame600/later image hashes. No current bad-run witness or responsible uninitialized read is established, so global zero filling remains a mitigation, not a completed fix.
No typed Main process reads are made under an adjacent unauthenticated PDB. The previous two-fix Main PE has no CodeView directory; source-consistent private diagnostic carriers authenticate actual PE/PDB/live GUIDs separately.

The ordinary next-episode bankjump route on unchanged native carrier `447ae21f...e136e0` reaches the raised windmill bank, then the watchdog ends it after live tick4410. Actual Main PE/COFF bytes uniquely authenticate `TSpineEnemy::zigzagToCurPathNode` calling the distance-wrap `MsWrap<float>` at enemy.cpp:520. One authenticated debug and two unchanged-carrier mechanism runs do not manifest the hang; they instead hit the same900-tick ramp-walk timeout. Original float locals were not retained and both observers return7 without capture, so actual inputs/upstream cause remain unknown. Partial raised-ramp ascent and OIL_SLIP are observed; no actual wall/edge contact is captured. No product fix, guard, bypass or timeout change is claimed. See the preserved private handoff-index and the session handoff for exact inputs/poses/byte proofs.


The earlier isolated `FreshTakeover_3c10554` checkout fast-forwards to `154d9f5` and independently rebuilds Strict.
Its Aurora check confirms 18 patches and 45 already-matching files; third-party source downloads/caches include the main `build/Strict/_deps` tree.
This is an isolated project-source build, not a fresh dependency download or clean Windows machine.
Canonical integrated source `2702e9b` subsequently builds from a fresh detached `pg2-final-2702e9b` checkout and a clean Aurora `50e5be3` clone with all 18 tracked patches applied.
The 15 explicit dependency-content overrides reuse main `build/Strict/_deps` directories, including prebuilt Dawn/SDL3/nod packages; no existing game objects or binary are reused.
The first isolated Strict build passes in 193.73 seconds with zero unresolved symbols.
After removal of the temporary configuration helpers, the unchanged cached end-state build also passes in 7.07 seconds.
Private `build/PortGoal_20261016/canonical-isolated.json` records the source, caches and actual matching PE/PDB identity for read-only investigation.
This remains a development-host build, not clean-PC, real-GPU or full-gameplay acceptance.
The subsequent independent `pg3-final-08b8c30` checkout also builds from clean Aurora `50e5be3` plus 18 patches and the same 15 explicit dependency-content overrides, including those prebuilt packages.
The initial1282-step build uses2compiler jobs and takes434.37seconds; after removal of own configuration helpers, the final house Strict rebuilds1269steps in162.84seconds and reports0unresolved.
This second run is not reported as a cache hit or measured speedup. Final actual PE/PDB GUID1A81F619-1B4D-52C7-4C4C-44205044422E/age1 agrees; the previous intermediate GUID is not used for typed observation.
The normal earned-save Delfino boss route is now verified on canonical production source `2702e9b`: private `pg3_gate_earnedload_20261016` exits0 after431seconds.
Ordinary fileB load restores Shine0x56; boss HP3 reaches0 at10428, health6/water8230, then the game naturally reaches controllable stage1/scenario1 at11523 with one Shine.
Maintained `tools/routes/pc_gatekeeper_unassisted.dev` uses only controller inputs, washed footing and source-backed throat windows; the documented16-tick shortcut must be resampled if native input pacing changes.
Private game-thread observation proves the grounded live unheld `TResetFruit` contact while a pursuit waypoint lies inside its130-unit envelope; no physics defect is inferred.
The observer-free pursuit candidate then downs Shadow after3065/5000ticks at3945, waterCt0/downWait1/doing0xF, and ordinary A closes the natural dialogue after65ticks.
Its diagnostic `dolpic/1` start does not prove normal unlocking: at4310 Shadow is naturally gateward doing0x11, but the gate remains flags0x2/noCollision1.
Integrated source `08b8c30` passes Main Strict, the actual native clear-corridor/missing-cell/100-unit-drop self-check and the final7-gate aggregate; no temporary observer remains.
Generate the private grid with `python tools/nav/col_grid.py dolpic1 build/PortGoal_20261016/dolpic1-corrected.nav 200 750`, then set `SUNSHINE_DEV_NAV` to its absolute path; no grid or game assets are published.
The lead's earlier entry-only `pg4_normal_bianco_20261016` exits0 after751.93seconds on final isolated source08b8c30, using an owned normally earned airport profile; checkpointa02a111 records its bounded route.
It restores the earned Shine by ordinary fileB input, defeats Delfino at10438, downs Shadow after3065/5000ticks at15056, closes dialogue through ordinary A after65ticks, and naturally draws/opens the gate.
Bianco2/0 is controllable at18634; real walk/jump reaches(-7306,3161,17362), and the route ends18964 with shines1/health8/water10000.
All seven GCI checksums pass and the card remains byte-identical to the earned airport source; no new Bianco Shine is claimed in this connected run.
The run uses no devstart, cheats, aim assist, seed, direct state/resource writes, movie skipping or step-retrace. Quarter scale and concurrent host activity make it functional evidence, not quality/performance acceptance.
`BiancoFirstShine` first proved generic boss death, natural terrain/reward flight, event 0 collection and normal save/return in canonical `2702e9b` diagnostic `bianco/0` (exit 0, 325 seconds). That component alone did not prove normal unlocking.
The subsequent connected mission `pg5_normal_bianco_shine_20261016` passes with exit 0 in 853.08 seconds on the same immutable compiled source `08b8c30`, with no new engine/game code.
Ordinary inputs reach Bianco death animation at tick 20522, natural event 0 appearance at 20911, collection at 21160 and controllable Delfino 1/5 with two Shines at 21663; the route ends at 21723.
Independent complete GCI decoding finds precisely IDs 0/86, save count 2 and no 100-coin reward. Compared with the earned airport baseline, no flag is lost and the only changed saved integer is save count 1 to 2.
New-process `pg5_normal_bianco_reload_20261016` passes ordinary File B input with exit 0 in 147.48 seconds, reaching controllable Delfino 1/5 with two Shines at tick 4192.
Both standard `gci_check.py` runs pass all seven sectors. Whole GCI SHA-256 is `bd42d75d65d3e4a1c19cfad3a005ac71bf789ff24ef30de94acb36aa8bec5fa0`; every persistent flag, all 21 saved integers and the option sector remain byte-identical across cold load.
Actual script end is `std::_Exit` and bypasses orderly application callbacks; do not infer native sound-mode persistence or normal Close from that exit 0.
Independent workers now correct confirmed camera target/desired-position member errors and reproduce/fix the volatile native console Mono bit. Their candidate normal-Close/non-default settings proofs are not borrowed into this unchanged-source mission certificate.
The seven-gate/63-check and isolated Strict evidence above is reused for unchanged compiled source `08b8c30`; no duplicate build or new matching gain is claimed for this route/documentation-only wave.
No retail source, matching mark or ledger promotion changes in this replay-tool wave. The complete 11,323 exact identity/size set was checked again, with zero functions lost or added.
Execution now reuses the canonical immutable compiler artifacts for private single-translation-unit candidates and copied normally earned profiles for local route checks.
The current host exposes 6 logical processors and about 16 GiB RAM; the existing maximum of 3 native build/run owners is retained rather than adding competing workers.
Complete publication gates remain per completed source wave; documentation-only changes do not trigger another unchanged game build.

The actual native intro window is observed with `computer` screenshot and accessibility helpers.
`build/PortGoal_20261016/native-ui.png` and `native-ui.log` retain the private evidence; the adapter is D3D12 Microsoft Basic Render Driver (CPU/WARP).
APPDATA and USERPROFILE both point into the owned test profile; logs show zero active cheats/mods and the isolated card path.
The native Close button removes the window and the process returns 0.
Intro rendering and this normal-close case do not prove complete gameplay, listening, a physical controller or a hardware GPU.

### Update 2026-10-16: unassisted airport checkpoint, cold load and replay-tool repairs

`pc_airport_unassisted.dev` uses ordinary virtual-controller input only, with an empty owned profile and no aim assist, cheats, developer stage start, teleportation, direct aim/progress writes, movie skip or step-retrace.
On main source `154d9f5`, WARP run `pg1_airport_unassisted_20261016` returns 0 after 598 seconds.
The airport boss reaches HP 0/dead 1 at tick 10080; health 7 and water 8552 remain.
The genuinely earned Shine leads through normal transitions to live Delfino 1/0, one Shine and director state 4, at tick 17532.
An actual native computer capture also observes the running movie surface; this is not physical-controller or audible-output verification.

The newly earned GCI independently decodes to sole Shine `0x56`, save count 1 and stored Shine count 1, with no blue-coin flags.
All seven sector checksums pass.
A new process loading a copy of that owned test profile through `86_accept_reload.dev` returns 0 after 320 seconds: live Delfino 1/0, one Shine, director state 4 and camera demo 0 at tick 9290.
The pre-reload snapshot, original test card and copied/reloaded card remain byte-identical; original user saves are untouched.
Private `build/PortGoal_20261016/airport-save-before.json` and `airport-cold-reload.json` record the bounded evidence.
This does not certify later mission/nozzle unlocks, non-default settings or repeated lifecycle cases.

The native replay helper `FaceTo` now compensates the actual PADClamp/Mario deadzones instead of losing its minor stick axis.
`pc_faceto_deadzone.dev` passes on main at the unchanged 3-degree tolerance: error -0.9 degrees after 14 ticks.
Its isolated candidate also passes; the linked controller harness covers 3600 bearings but retains a source-equivalent Mario arithmetic tail, not a whole-controller certificate.
The integrated helper passes all 7 verification gates/63 host checks; the reference SHA and existing exact-match set/totals do not change.
An actual native computer screenshot observes its Delfino scene; developer scene selection remains diagnostic, not normal reachability.

`tools/nav/col_grid.py` now samples each triangle plane at the cell centre and excludes downward ceilings.
Both maintained `.col`-to-`.nav` regressions fail against the old main generator and pass after the correction.
The real `dolpic1` conversion produces 99,506 cells in height band 200-750.
The independent native pursuit using this grid still times out after 5000 chase ticks, rc 3, with Shadow Mario's water counter 64 to 59.
Static ramp connectivity is repaired; live dynamic-fruit contact and controller-bearing explanations are not yet proven.
The retail-correct wall flag `0x8` remains unchanged; no replay-index fallback is added.

The [content inventory](content-coverage.tsv) now contains 4714 technical-metadata rows: 154 ordered scenarios, 120 source-identified Shine identities and 240 observed persistent blue-coin identities.
`content_inventory.py --self-check` and the full actual-data generation pass after independent review; all cited decoder/semantic inputs are among 440 provenance hashes.
Repeated archives retain scenario order; 44 unresolved resource references and 144 unresolved object/script records remain visible.
Every runtime status stays open: presence is not collectibility, mission completion, connected progression or a 120/240 playthrough.
No game assets, localized text, scripts or saves are emitted.

Remaining early blockers are Delfino GateKeeper, Shadow Mario and normal Bianco progression.
WARP really clears reachable floor, but the attempted stance slides back into goop; no physics defect is established.
Digital R maps to analog pressure in the native adapter, so the private "R pre-aim without spray" and always-R windowed bot assumptions are invalid and are not integrated.
A typed brake repetition is interrupted by an ordinary enemy before reaching A; it does not establish the pre-jump state or explain the observed drift.
Separately, the first Ricco/Gesso smoke hits a WARP presentation-progress watchdog before developer scene selection, not inside Gesso.
The old broad zero-allocation colour-screen mitigation still has no identified root cause; private draw/allocation diagnostics remain separate from accepted production code.
Null, WARP and real-GPU acceptance remain distinct.

### Update 2026-10-16: native Ricco table contract and diagnostic display quality

The actual Ricco loader failure is a shared producer/consumer contract violation, not a missing model to ignore.
`TSmallEnemyManager::createModelData` supplies one scalar entry to `createModelDataArrayBase`, which scans until a null model name.
Private before/after instrumentation observes 54 entries before the fix and exactly one intended entry after an explicit native-only null terminator.
The old run reaches a garbage model-name pointer and faults in `sprintf`; the corrected real managers load successfully.
The original PowerPC producer remains unchanged.

All own temporary model/boss probes and the invalid Delfino fight policy are removed from the isolated checkout.
The resulting isolated Strict rebuild reports zero unresolved symbols.
On unmodified main, `pc_ricco_scene.dev` reaches live Ricco 3/0, director state 4/camera demo 0, walks to the first target, jumps and returns using ordinary virtual-pad input.
The recorded clean WARP run returns 0 after 43 seconds, including a diagnostic idle hold.
A separate visible WARP run finishes the maintained route at tick 535 and exits 0 through the native Close button.
The private Gesso probe also confirms `performInContainer` executes its CalcAnim path with tentacle state 9; this is not a complete boss fight or normal Ricco unlocking.

The user's pixelation observation is caused by the route runner's diagnostic `SUNSHINE_RENDER_SCALE=0.25`, relative to the native 640x480 EFB, not the window.
Production settings already default to `Resolution scale: Auto`; they are not changed.
`SUNSHINE_RENDER_SCALE=` removes the route override and uses the fresh profile's Auto/window resolution.
The actual native Auto capture shows substantially sharper HUD, edges and crane geometry; its maintained input route also completes at tick 535 and normal Close exits 0.
Private captures are `build/PortGoal_20261016/ricco-native-ui.png` and `ricco-native-auto-ui.png`.
This is D3D12 Microsoft Basic Render Driver/CPU evidence, not hardware-GPU performance, HD assets or full graphical correctness.

Before the following integration, the same real scene logs expose two implementation gaps: format 1/2-bit ADPCM is silenced, and the original shine-volume data are not loaded on PC.
The original shine-volume draw condition is map 1; a warning while in Ricco does not prove that Ricco needs this effect.
Independent agents implement the host mixer and game-rendering translation unit; the lead retains shared headers, integration and final gates.

The historical J3D envelope method's absent native matrix writes are separately proven using actual compiled before/after methods and the real matrix/vertex consumer.
The earlier method leaves all 12 poisoned coefficients unchanged and produces consumer point `(5,-7,9)`; the already-fixed method produces `(7.5,-10.5,30.5)`.
The existing load harness now retains this deterministic regression and supplies the real Aurora configuration context required by its standalone OS address checks.
This does not certify the uncaptured historical flat-colour frame's cause or justify removing global zero-filled allocation.
The production allocator and JSystem implementation bodies remain untouched.
The rebuilt main harness also passes the actual corpus: 698 models, 23 material tables, 1244 BCK, 138 BTK, 56 BRK, 161 BTP and 11 BPK files, with zero failures.
It retains `/FORCE:UNRESOLVED` for unused game/graphics references; the exercised real methods resolve, but this is not the separate production Strict-link gate.

### Update 2026-10-16: real AFC2 voices and original map1 shine volume

The host mixer now decodes format-1/AFC2 frames: one header and four payload bytes contain 16 MSB-first signed 2-bit residuals.
Actual retail DSP instructions at `0x07EF..0x080E` and `0x081E..0x0825` establish the residual gain `2^(scale+2)` and the existing Q11 predictor/saturated-history behavior.
The existing AFC4 cache, source progression, loop restoration and ARAM checks are reused; no exported decoder API, compiler option or third-party dependency is added.
An independent review identifies one history-reset test gap; the maintained regression now starts with a nonzero predictor, dirty history and unsaturated zero-history expected samples.
Main `dsp_test.cmd` passes all 10 mixer sections and the mailbox protocol case, including the new reset case and the old 200272 identical AFC4 reference samples.
Private additional checks cover 152 real format-1 assets and 256 synthetic headers: 3338783 source samples and 6692320 stereo-channel output samples match the equivalent AFC4-reference expansion.
That reference comparison is not physical DSP execution.

The unmodified integrated native Ricco route returns 0 after 39 seconds and completes at tick 535.
Existing voice diagnostics show actual `kind=afc2/type=0x5` source decoding with positive RMS, rather than merely an absent warning.
These muted diagnostic runs do not prove final bus gain, audio-device output or human listening.

`drawShineShadowVolume` now reuses the original map1-only draw body on PC.
Existing `DVDGetDOLLocation` supplies the borrowed executable; only its native declaration and the Windows system `bcrypt` link are needed.
The loader validates GMSJ01 revision 0, the full reference DOL SHA1, bounded big-endian section mapping and display-list/index bounds.
It owns a static copy of the original BE bytes, binds the exact position extent as BE, and aborts explicitly on invalid data.
The 0x10CC-byte blob contains a 0x760-byte display list and 402 signed-16 XYZ positions; nothing from the original blob is embedded or published.
An independent source review finds no issue; compile-time boundary/endian checks and a private actual-DOL/malformed-data helper harness pass.

Main Strict builds the integrated source with zero unresolved symbols.
Actual WARP run `pg2_shine_volume_live_20261016` logs the loaded retail sphere, reaches map1/scenario0 and passes the unchanged FaceTo tolerance at tick 924, error -0.9 degrees.
The native computer capture observes the actual Auto-resolution surface and the native Close button exits 0 after 158 seconds.
Private `build/PortGoal_20261016/shine-volume-integrated-ui.png` retains the surface evidence.
Developer scene selection is still diagnostic, not normal progression; no retail-image equivalence, hardware GPU or full graphical/audio acceptance is claimed.




### Update 2026-10-16: additional compiled helper audits and collision-square fix

Two further independent agents complete bounded current-object `getGravityDir` and `calcBathtub` reviews.
`getGravityDir`'s 143 retail/current instructions preserve ordered operands, fused graph and returned defined FP status,
assuming the same PPC estimate and ABI-conforming trigonometric implementations.
No helper/header change is justified there; the earlier numerical-only ledger explanation is replaced.

Source [`d33c1ef`](https://github.com/Astrr00/SMSunshinePC/commit/d33c1ef) restores retail's unconditional `innerR*innerR`
in `calcBathtub` with one reused plain local. A conditional multiplication had omitted sticky inexact/underflow flags.
The real production method is called in temporary WARP fixtures with exact, inexact and underflow square inputs:
before inexact/underflow flags are `0/0,0/0,0/0`; after they are `0/0,1/0,1/1`.
All three calls retain count 1 and accumulated position `(0,1,0)`; each fixture saves/restores the native FP environment.
Both before/after runs finish in 37 seconds. This is native-host evidence plus a compiled-PPC proof, not physical Gekko execution.

The complete corrected helper review finds no further reachable operand, NaN, branch, write or final-status mismatch.
Its outer rim body is unreachable under unchanged valid inputs; no dead-code cleanup is made.
After fixture/include removal, final WARP route 98 passes (rc 0, 37 s), with visible water/arena.
Full final-source verification: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, zero unresolved, unchanged SHA-1, WARP boot 45 s.
The isolated checkout fast-forwards to `d33c1ef` and independently rebuilds Strict.
Normalized source streams for `perform` (1 240), `getGravityDir` (143) and `inv_sqrt` (13) are unchanged by this helper fix.
`calcBathtub` score rises `95.36346 -> 95.55992`; `perform` retains the earlier correction's `93.910194`.
Full fuzzy total is 98.66029%; byte-identical totals and ledger prefixes remain unchanged.
Final `bath_square_final85_20261016` passes on Null (503 s, tick 22280, Bianco 2/0, one Shine).
All 681 ready status ticks retain JPA statistics; none overlap the 61 unavailable-scene ticks.
The isolated seven-block GCI passes independent checksums; fresh-process `bath_square_final86_20261016`
reloads it successfully (Null, 17 s, tick 1675, Delfino 1/0, one Shine).
Progression uses step-retrace, skipped movies, aim assist and health/water cheats;
reload retains step-retrace/movie skip but no dev start, aim assist, scripted cheat or teleport.

These helper certificates assume production-valid ABI objects, unchanged inputs, IEEE mode, masked exception enables,
the same PPC estimate/math implementations and no concurrent/MMIO mutation.
They do not certify intermediate FPSCR scheduling, enabled-trap context, unspecified NI behavior or universal host FP.
Whole-`perform` remains partial for final integration across initialized and initialization paths; listening and hardware GPU remain open.

### Update 2026-10-16: bath-water spawn NaN and FP-status correction

Source [`3d095fa`](https://github.com/Astrr00/SMSunshinePC/commit/3d095fa) fixes two independently reviewed `perform` spawn defects.
Precomputing scalar world offsets before the center setter restores retail's three NaN operand priorities.
The existing `get_float(0.0f, 10.0f)` restores multiplication followed by exact addition, clearing velocity `FI/FR`.
Actual emitted-instruction replay checks 18 NaN payload/sign/signaling cases (old object differs in 12);
two exact finite rounding witnesses prove the old status gap. These are architecture-model checks, not PPC hardware results.
The existing shared X/Z zero product analytically preserves values and accumulated invalid exceptions;
no added setter, wrapper, volatile access, dependency or compiler change is needed.

Null and D3D12 WARP read-only probes each observe two successful spawn-position calculations and capacity rejection:
active count and capacity are both 0. Independent LCG decoding confirms velocity bits `0x40324864`/`0x40D5B7A6`;
the manager advances exactly two draws between observations.
After removing probes, WARP `bath_perform_spawn_final_20261016` passes (rc 0, 29 s, tick 812) with visible arena/HUD.
Dev start, diagnostic platform travel, real analog-R/L attacks, health/water cheats, step-retrace and entry movie skip remain explicit.
No new accepted nonzero-capacity drop, unassisted fight or native PPC FPSCR measurement is claimed.

Full verification: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, Strict zero unresolved, unchanged SHA-1 and WARP boot 45 s.
The existing isolated checkout fast-forwards to `3d095fa` and independently rebuilds Strict with cached dependencies.
Full report: 11 323/12 881 byte-identical, 445/736 complete units; no byte-identical function lost.
`perform` score falls `94.09952 -> 93.910194` (data-value wrapper `93.89401`) for the assembly-proven semantic correction.
Ledger prefixes remain 1 976 logic-verified / 150 matched / 252 partial, excluding the same three malformed legacy rows.
Whole-function transitive equivalence remains partial pending actual compiled `getGravityDir`/`calcBathtub` reviews.
Proof assumes identical masked-exception PPC controls, valid ABI memory and no concurrent/MMIO mutation;
enabled-trap context, universal host-x64 FP behavior, listening and hardware GPU remain unverified.

### Update 2026-10-16: corrected `setLookDir` compiled FP review

Two independent read-only reviews and a lead-run canonical instruction comparison close the corrected helper's PPC review.
Retail `0x801FE23C-0x801FE418` and current MWCC source each contain 120 instructions; all rows agree after
explicit FPR permutation, caller-relative ABI-stack offsets, local branch targets and relocation-data normalization.
The actual weak `inv_sqrt` emitted into the current BathWater source object also agrees directly with all 13
retail JPAEmitter instructions/constants, including `frsp` and fused `fnmsubs`. No score-only certification is used.
This establishes output and FP-operation equivalence for valid ABI memory and identical non-trapping PPC controls,
not identical trap-handler register/PC/stack context or universal host-x64 bit/exception behavior.
The old pre-fix BR20 claim remains false historically; this new review certifies the corrected function.

Real production-header smoke passes 37 invariant checks in an `/O2` build and the current unoptimized Strict-style
host configuration: axial/skew orientation, handedness, zero/epsilon direction, parallel input aliasing,
cross-product cutoff, NaN propagation class and translation preservation. These are invariants, not a retail-bit oracle.
One-ULP-above nominal near-parallel input can still collapse: normalized Z is approximately `0.99999994`,
so the guard must be evaluated on the rounded cross magnitude, not the nominal up-vector component.
The smoke uses a robust twice-cutoff bracket for its nondegenerate-basis assertion; no production fix or test repinning.
The throwaway smoke source is removed.

WARP `lookdir_fp_corona_20261016` passes route 98 (rc 0, 36 s, tick 1024) with visible water geometry.
`verify --build --matching --boot-secs 45`: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, zero unresolved,
unchanged reference SHA-1 and a successful 45-second WARP boot. Compiled game source is unchanged,
so the existing isolated-checkout build proof is reused; no new fresh-clone run or matching gain is claimed.
Ledger prefixes are now 1 976 logic-verified, 150 matched, 252 partial; three malformed legacy rows remain excluded.
Unconditional bath-water `perform` FP-state equivalence, universal host FP behavior, human listening and hardware GPU remain open.

### Update 2026-10-15: natural ending and persisted completion

Two independent read-only reviews trace the real MovieDirector/CardSave/FlagManager contract.
Local THP headers measure epilogue at 4301 frames, credits at 3000, and either final movie at 360,
all at 29.9699993 fps. The correct sequence is 14 -> 15 -> 16 for this save, not all four alternatives.
The 120-Shine branch selects movie 17 and was not manufactured or exercised.
Independent `thp_ref` decoding confirms that the earlier six-second blue-sky image is genuine film content.

The first passive run, `corona_ending_natural_20261015`, exits 0 after 304 seconds at tick 9502;
it captures visible epilogue/credits and earns the completion Shine, but stops just after movie 16 starts.
It is not evidence of a completed save. The complete run is `corona_ending_save_20261015`:
417 seconds, rc 0, tick 13167. Temporary read-only boundary probes observe THP state 3, not error state 5,
for films 14, 15 and 16; film 9 also ends naturally on the subsequent return to the option map.
Screenshots show epilogue content, scrolling credits and the normal "THE END" scene.
No buttons or developer movie-skip switch advance the three ending films.

Dev-start selects file A without loading a bookmark; copying a card does not select its file B.
The copied baseline has a genuinely empty A. LEFT, release, A selects Yes in the default-No endsave prompt.
Observed save progress is 0x15 -> 0x16 -> 0x17 -> 0x01, with card status READY and no timestamp-protection bypass.
Independent `gci_check.py` passes all seven sectors. New A sector 2 has write count 1, save count 1,
magic 4, one preview/live-bitmap Shine and completion `sector[0x6E] & 0x80`.
The GCI completion byte is at `0x40AE`: the stream starts at sector+4 and writes a 0x5C-byte header before
the bitmap at sector+0x60. File B's two sectors and the original baseline file remain byte-identical.

All three source probes are removed before the fresh-process reload.
`corona_ending_reload_fixed_20261015` loads A by ordinary file-select movement and head bumps,
with no dev-start, card-test, aim-assist, cheats or developer movie skipping: rc 0 after 147 seconds,
tick 4456, live Delfino 1/2, one Shine and an explicit `flag=0x10077` condition met at tick 4396.
This proves the earned final Shine survives disk serialization and real game loading.

The first reload attempt times out at 651 seconds on `thp=0` despite live Plaza gameplay.
`TMarDirector::thpInit()` opens `/data/ex128x144_q0.thp` with looping enabled, so global THP-idle is not a
gameplay-readiness invariant. Replacing it with a passive gameplay wait proves A, but a general first-time
reload can also need normal confirmation after load movies. Route 86 now uses existing A-input confirmation
until the live stage-1 gameplay owner, Mario, director state 4 and a free camera are ready.
This reload input may advance previously watched load movies; it does not alter the natural-ending proof above.
The passive-only B attempt times out at 451 seconds before gameplay. The final general route passes on WARP:
`corona_reload_b_pad_20261015`, rc 0 after 317 seconds, tick 10009, Delfino 1/0, one Shine,
no dev-start/card-test/aim-assist/cheat/movie-skip switches. Real A confirmation finishes at tick 9889.
Final `verify --build --matching --boot-secs 45`: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks,
zero unresolved symbols, unchanged matching SHA-1 and a successful 45-second WARP boot.
The compiled game source remains `b1392e5`, so its previous isolated-checkout build proof is reused;
no new fresh-clone build is claimed for this route/documentation-only change.
Game source and decomp ledger remain unchanged; only reload synchronization and documentation change.
Fight aids remain diagnostic platform travel, health/water cheats and step-retrace.
Unassisted combat/navigation, a full-game playthrough, the 120-Shine ending, sound listening, hardware GPU,
forced races and same-frame retail depth timing remain unverified.

### Update 2026-10-15: Corona five-platform ending progression

Two independent read-only reviews compare the critical Mario landing, fragile/hard forwarding and bathtub ending
branches against retail assembly. No divergence is found in these sections; this is not a new whole-unit ledger claim.
Real damage is message 3 from a state-3 L ground-pound landing on a fragile `b` piece. Hard `c` pieces intentionally
downgrade that message to 1. The ending checks four already-broken grips before accepting the fifth distinct hit;
`demo=1` and the movie handoff are stronger evidence than requiring a final count of five.

Runtime proof uses WARP, isolated saves, dev start, step-retrace, health/water cheats and diagnostic platform travel.
`corona_five_pounds_20261015` observes an ordinary message-1 hit without progress; its fixed high-altitude sequence
misses other platforms, loses a life and ends with only one broken grip. Exit code 0 is not victory.
`corona_rocket_pound_20261015` proves an actual analog-R launch and L attack without supplied attack altitude.
The first five-rocket timing probe reaches four broken grips, not the ending.
After calibrating the initial landing/charge timing, `corona_five_rockets_b_20261015` records five distinct
message-3 landings at grip angles -144, -72, 72, 144 and 0 degrees; the pre-hit count advances 0, 1, 2, 3, 4.
The final hit sets `demo=1`, takes Mario and transitions to stage 15/0 with THP state 2 (rc 0, tick 1824).
No direct message injection, damage, nerve or victory-state mutation supplies that result.

After removing both source probes, `corona_finale_uninstrumented_20261015` repeats the transition with movie skipping
disabled: its explicit stage-15/THP-2 condition passes at tick 1612, and the run exits 0 at tick 1852.
The early movie screenshot shows blue sky and letterboxing; complete epilogue content, credits and ending save are
not verified. Teleports only handle travel; every damaging rocket launch follows a neutral landing on the platform.
The final game source is unchanged from `b1392e5`; no bug fix or matching gain is claimed.
`verify --build --matching --boot-secs 45`: 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, zero unresolved,
unchanged reference SHA-1 and 45-second WARP boot. The existing isolated-checkout build proof for this same game
source remains applicable. Temporary diagnostic code/routes were removed; logs and game-derived images stay local.
Unassisted navigation/combat, complete credits/save flow, sound listening, hardware GPU, forced races and
same-frame retail depth synchronization remain open.

### Update 2026-10-15: Corona GPU depth reaches the height field

The previous scene-entry smoke did not produce CPU depth. A temporary probe on `cb28330` observes 900
renders at stock width 84 with all-zero depth bytes and heights. Aurora 0018 now converts actual depth
to Z24X8 and stages canonical 4×4/two-plane bytes through the existing guarded readback transport.
Bath water registers lazily after stage setup, uses a native-sized single-sample offscreen pass, initializes
valid far-depth tiles and decodes their big-endian bytes explicitly.
An additional assembly-proven decomp bug reversed the second `setLookDir` cross product; the corrected
right-handed view preserves backface culling and restores visible waves.
WARP runs `corona_depth_fixed_half_20261015` and `corona_depth_fixed_quarter_20261015` complete at
ticks 1023/1024. Four captured fields contain 28 224 heights, all equal to independent canonical depth decoding;
known cap depths 0xC00000 and 0xE00000 match the world-to-depth calibration. Results change over time.
The R8 regression `corona_depth_goop_r8_20261015` completes at tick 879: the sprayed point is clean and an
unsprayed point remains dirty. The new real geometry host test fails with the old cross order.
Measured readback latency remains 2–4 render copies, not same-frame retail synchronization.
Ledger prefixes now count 1 975 logic-verified, 150 matched, 253 partial: the old `setLookDir` equivalence claim
was false; its corrected algorithm is reviewed but post-fix compiled FP equivalence remains unverified.
Final source `b1392e5`: full verification 7 PASS, 0 FAIL, 1 optional SKIP; 63 host checks, zero-unresolved Strict
link, unchanged matching SHA-1 and 45-second WARP boot. A separate isolated checkout applies patch 0018
on the earlier 17-patch tree and completes 481 build steps, including affected header users, with zero unresolved symbols.
Optional GX FIFO tests are blocked by a vendored googletest warning-as-error, not reported as passing.
Uninstrumented final-source routes pass: WARP Corona `corona_depth_final98_20261015` (37 s, tick 1023);
status-enabled Null `corona_depth_final85_20261015` (471 s, tick 22436, Bianco 2/0, one Shine,
seven-block GCI checksum PASS); reload `corona_depth_final86_20261015` (16 s, tick 1674, Delfino 1/0,
Shines 0 -> 1). Scene-ready JPA samples remain live; none overlap unavailable-scene samples.
Full Corona fight, audible sound, hardware GPU, forced map/lifetime races and exact same-frame timing remain open.

### Update 2026-10-15: Corona static Z24X8 blocker

Two independent source investigations traced the format-22/4×4/one-mip failure to bath-water's valid static
`clear_z_TX[64]` object (`GX_TF_Z24X8`), not to a malformed archive BTI. Its logged pointer was not symbolized;
the exact tuple, unique constructor and correction's observed effect support that ownership.
Aurora patch 0017 now computes 64-byte 4×4 tiles and uses the existing RGBA8 tile decoder for static Z24X8 input.
The dev-start stage resolver also accepts an exact archive basename (`coronaBoss.arc`), without test-only hooks.
Before: Null aborted at tick 109 on unknown format 22. After: Null stage 60/0 completes at tick 705 (15 ready
status samples); WARP completes at tick 375 and captures the Corona boss/HUD in a real rendered frame.
No format coercion or suppressed error was used. Strict zero-unresolved link, unchanged reference SHA-1,
62 headless checks and a 45-second WARP boot pass; the patch-series checker sees 17 applied patches.
`verify --build --matching`: 7 PASS, 0 FAIL, 1 optional SKIP, including the 17-patch tracking gate.
The separate fresh checkout fast-forwards to `21e2b60`, applies patch 0017 and rebuilds Application.cpp,
both Aurora texture files and the Strict executable with zero unresolved symbols.
Permanent route `tools/routes/98_corona_depth.dev` passes on WARP (rc 0, tick 393, visible boss frame).
No depth-copy CPU readback, water height accuracy, complete boss fight, audible sound or hardware-GPU result is claimed.

### Update 2026-10-15: published lifetime fix, bath-water sound and EventWatcher

The verified lifetime fix and previous initializer review are published in `3c10554`; issue #12 is closed.
A fresh local clone of that commit builds with unresolved symbols disabled, using cached dependency source packages
and separately checked-out Aurora `50e5be3` with all 16 tracked patches; project objects were rebuilt.

The next two delegated tasks found/corrected bath-water sound being gated to every fourth move tick and confirmed
both EventWatcher warp and Appear8 whole-function reviews.
The rebuilt matching object contains exactly the intended sound-branch target change.
Finite spawn-FP/signed-zero/RNG gaps are analytically closed and CRT f32 edge assertions pass; unconditional
NaN-payload/FP-state equivalence remains partial.
Appear8 was already byte-identical; its stale partial ledger row is now matched, not a new matching gain.
Ledger prefixes: 1 976 logic-verified, 150 matched, 252 partial; three legacy malformed rows excluded.
Full matching totals remain 11 323/12 881 byte-identical, reference SHA-1 unchanged.
`verify --build --matching`: 7 PASS, 0 FAIL, 1 SKIP (optional probe-units); strict zero unresolved, 62 headless checks,
45-second WARP boot without fatal exception or WebGPU error.
The final `cd552f5` source rebuilds and links in the isolated fresh checkout with zero unresolved symbols.
Status-enabled Null route `bath_sound_fix85_20261015` completes at tick 17790 (368 s wall):
Bianco 2/0, one Shine, live particle status; its seven-block GCI checksum checker passes.
The route does not reach Corona, so this is a progression/save regression check, not an audible sound check.

An isolated pre-fix Corona dev-start smoke reaches stage 60/0 at tick 109, then aborts on unknown texture format 22,
before any sound-cadence observation. Temporary exact-archive-name and logging hooks were removed and are not shipped.
This runtime blocker is tracked in [issue #13](https://github.com/Astrr00/SMSunshinePC/issues/13).
No audible Corona or hardware-GPU result is claimed; detailed branch/FP evidence is in `decomp-rounds.md`.

### Update 2026-10-15: takeover, scene lifetime and initializer review

Last source work was `bf1ea15` (bath-water first-pair collision correction), followed by the `88d7c5d` status refresh.
Two independent tasks were delegated: TransitionLifetime fixed the diagnostic lifecycle error; BathInitializer
completed the renewed whole-function initializer review without changing its source.
`initializeIfYet_` is now `logic-verified`; `perform` spawn-position FP equivalence and Corona runtime comparison remain open.
Ledger status prefixes now count 1 976 `logic-verified`, 149 `matched` and 253 `partial`, excluding three legacy malformed rows.

The lifecycle fix in `src/devscript/devscript.cpp` gates scene-object readers on the active gameplay director and its
existing `unk260` post-setup marker; this covers status logging, scene-object route conditions and aim assist.
It preserves transition-safe tick/stage/movie status and resumes particle statistics in each live scene.
The first status-enabled route-85 run, `takeover_lifetime85_20261015`, survives the former airport crash and reaches
Delfino 1/1 with one Shine; it ends at the 330-second wall limit (rc 124), not at route completion.
The extended run, `takeover_lifetime85_long_20261015`, completes with status enabled: rc 0 after 364 s wall
(650 s limit), tick 18210, Bianco 2/0 and one Shine; independent GCI checking passes all seven block checksums.
Route 86 (`takeover_lifetime86_20261015`) reloads that card: rc 0 after 17 s, tick 1675, Delfino 1/0, Shines 0 -> 1.
Assert-based checks confirm live JPA statistics in airport, Delfino and Bianco and no JPA traversal on the 61 sampled
unavailable-scene ticks of the completed run.
Both routes use Null, step-retrace, skipped movies and aim assist; they do not prove switch-free or hardware rendering.

Lead checks: strict link with zero unresolved symbols, 62 headless checks with zero failures,
matching DOL SHA-1 unchanged and `ninja changes_all` empty; rebuilt-binary WARP boot stays alive for 45 seconds
without a fatal exception or WebGPU error.
The initial verification invocation could not find `clang-cl`; setting `LLVM_BIN` to the already installed compiler
made the strict and headless checks pass without installing anything.
See `decomp-rounds.md` for the initializer branch/FP evidence and diagnostic ownership chain.

### Update 2026-10-15: matching refresh and bath-water reserve-drop correction

Source `bf1ea15`; full `sms/` build and `changes_all`: 11 323/12 881 byte-identical functions (87.90467 %),
62.6544 % matched code, 98.66045 % fuzzy, 87.469604 % data, 445/736 complete units.
The DOL SHA-1 remains `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.
Six more functions match than in the 2026-10-12 full report.

The targeted audit confirms `TTailRubber::adjust` (its two ledger spellings are consolidated).
Bath-water `calcWaterModel` now enters its first collision pair before checking the inner bound, matching both retail
call sites; all 500 slots are initialized, while stock wave parameters activate 160 drops with stride 1.
This restores an interaction with reserve slot 160.
`perform` improves 93.91747 -> 94.09952 %; `initializeIfYet_` drops 94.67607 -> 94.61472 % for the same required logic fix.
No byte-identical function was lost.
Their whole-function statuses remain partial: spawn-position FP boundaries and initializer re-review are open.

Observed validation: strict Windows link with zero unresolved symbols, 62 headless checks with zero failures,
45-second WARP title boot with no fatal exception or WebGPU error.
A fresh clone of `bf1ea15` also builds and links with unresolved symbols disabled, reusing cached dependency sources
and packages; Aurora was checked out separately and the tracked patch series applied.
No Corona fight comparison was performed.
Route 85 regression: `bath_audit85_nostatus_20261015` on Null, step-retrace, skipped movies and aim assist, automatic
status output and screenshot monitor off: rc 0 after 230 s wall, Bianco stage 2/0, one Shine, GCI checker PASS.
With automatic status output on, the first run crashed in `logJpa` at `devscript.cpp:539` during airport scene teardown;
this diagnostic failure remains open and is distinct from the passing route without periodic output.
The pre-fix bath-water control also faults at that transition (invalid read, rc 0xc0000005, 120 s wall; no control debug
symbols). Tracked in [issue #12](https://github.com/Astrr00/SMSunshinePC/issues/12).
Ledger counts by status prefix: 1 975 logic-verified, 149 matched, 254 partial; three malformed legacy rows excluded.
Details: [`decomp-rounds.md`](decomp-rounds.md).

The following undated lead notes and dated updates describe earlier observations; use README and ROADMAP for the
current combined feature state.

Observed by the lead with `AURORA_SCREENSHOT` and `SUNSHINE_DEV_STATUS` logs (dev start `SUNSHINE_DEV_START=bianco/0`):

- Strict link: 0 unresolved symbols. Matching DOL SHA-1 unchanged. TEMP-DEBUG grep: 0.
- Bianco Hills renders textured; HUD (Shines, blue coins, coins, lives) renders; Mario survives WARP_OUT, walks, jumps, camera follows; director reaches state 4.
- NOT verified / broken: title screen render and the retail flow title -> file select -> new game -> Delfino -> Bianco; first Shine (only a dev hook exists, it finds no TShine because the Bianco Ep.1 Shine is event-spawned after the boss) and saving; goop/graffiti (pixel metric is visibility-only 0/1, so `PollutionCount` magnitudes are wrong); title/gameplay BGM (title music ends after ~20 s, gameplay has broadband noise); THP audio channel order (decoder agrees with its own model but matches ffmpeg only with L/R swapped: likely swapped); replay/interpolation at >30 fps in gameplay (gate `mState == STATE_UNK4` only just reachable); widescreen visuals; FSR1; launcher click paths; cheat effects; mods in-game. DLSS: feasibility document only.
- Known decomp gaps: none empty any more; `PacketUtil::ShapePacketCallBackFunc` (75.7 %, partial review) and `TGCConsole2::perform` (69.5 %) are the largest open game functions on the gameplay path; open list in `decomp-verified.tsv` / `decomp_queue.py status`. `EmitterViewObj`/`EmitterIndirectViewObj` resolve to null in the perform lists (particles likely not drawn).
- `probe-units`: clang-cl 624/733 clean (all 584 built units); MSVC `cl /Zs` 565/733 (66 built units fail on MWCC-specific syntax; reasons in platform-notes).

### Lead update 2026-10-11 (supersedes the next section where they conflict; details in `session-2026-10-11.md`)

Measured: `ninja -k 0 -j4` exit 0, DOL SHA-1 `9f5a8caf...` OK, `changes_all` no regression, strict link 0 unresolved,
`apply-aurora-patches.py --check` "applied: 8 patches", 11 013 / 12 881 functions bytegleich (58.57 % code, 97.98 % fuzzy),
445 / 736 units Matching, ledger 1 366 `logic-verified` / 64 `matched` / 541 `partial`.

- A3 update: a condition-based route (`tools/route.py`, `tools/routes/07_airport.dev`, verbs `waitfor`/`mash`/`walkto` with
  `timeout`, conditions `stage scen dirstate movie flag cardmode cardprog`) reaches **airport0 (stage 0, scen 0)** from the
  logo with no start switches: stage 15 is the option map with the file blocks (x 840/1080/1320, z -1000); card prompts
  (`TCardLoad` progress 14 -> 17 -> 19) are dismissed with A; bumping block B (progress 27) starts the new game (41);
  the opening movie runs ~1900 ticks; airport0 is ready at tick ~5300-5490. This supersedes "not reproducible"
  (the earlier non-determinism came from tick-timed scripts).
- Not reached: `MapObjStartDemo` (-6630,500,1600). The straight-line `walkto` wanders and Mario falls from y 500 to y 300 near
  x -500..-960. Delfino, Bianco gate, Shine, `.gci` save: not attempted.
- Crash root cause (asm-proven): `JPADrawExecDirectionalCross::exec` and `JPADrawExecRotDirectionalCross::exec` called
  `GXBegin(GX_QUADS, ..., 4)` but wrote 8 vertices; retail passes 8 (asm 0x8007C2C4 / 0x8007C864). Aurora's FIFO parser then
  read vertex data as commands ("draw vertex data overrun", fatal). Fixed in `JPADrawVisitor.cpp`; the full-length
  confirmation run is still open. Aurora patch 0008 adds the diagnostics that found it.
- Decomp agents (Opus 5.5 medium directing six Haiku 5.5 xhigh workers): +170 `logic-verified`; asm-proven fixes in
  `MarioMove.cpp` (`setStatusToRunning`, gameplay-visible), `Koopa.cpp`, `hamukuri.cpp`, `GCConsole2.cpp`, `bossManta.cpp`,
  `bosspakkun.cpp`. Only these diffs were checked by the lead against retail asm; ledger rows are not audited.
- Observed but unexplained: option-map and title screenshots are pure white PNGs.

### Update 2026-10-11 (session b, measured; see `session-2026-10-11b.md` sections 10-35)

Gates: DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, 11 013 / 12 881 functions, 445 / 736 units, strict link 0 unresolved, Aurora series 11 patches. Fixed in the decomp/port: `TEMario::perform` control flow (shadow Mario froze), `TSelectDir` `OSJoinThread` result, `TSelectMenu` stage-name index and `break`, `TMarDirector::mConsole` init (PC), Aurora alpha-compare shader (patch 0011). Reached with dev assists only: Delfino gate chain to Bianco Ep.1 (controllable), both GateKeepers killed by the water jet (dev aim assist), Shine collect via dev verb, `.gci` PASS, switch-free reload `shines=1`. Not reached: a switch-free route logos to Bianco under 330 s (the whole flow needs about 20 000 logic ticks at 30 Hz), the bosses and the real Shine without dev assists, two clean full runs.

### Latest lead update (end of the agent session; supersedes everything else in this file where they conflict, including the A3/R02/R03 rows)

Measured by the lead at round 7 (commit 513e37a, base d414ba0): `ninja -k 0 -j4` in `sms/` rc 0, DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` unchanged, strict link 0 unresolved, `apply-aurora-patches.py --check` "applied: 11 patches", 11 317 / 12 881 functions byte-identical (87.858 %; matched code 62.56448 %, fuzzy 98.637474 %, matched data 87.469604 %), 445 / 736 units Matching. Ledger (fact sheet; rows, not unique functions): 1 977 `logic-verified`, 149 `matched`, 250 `partial`. Previous entry: 11 012 / 12 881 (58.56 % code), ledger 1 196 / 66 / 614 with no stated basis; not reconciled here.

- A3 correction (round 7): `airport0` -> Delfino -> Bianco is reached in route 70 (stage 0/1 at logic t=46 s, 1/0 at t=284 s, `u01`/`u02` 2/0 at t=590/715 s) and in the `60_acceptance` runs, both with dev aids (`SUNSHINE_DEV_STEP_RETRACE`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST`, `SUNSHINE_ALLOW_CPU_ADAPTER=0`). The earlier "reached once, not reproducible" note is superseded. The airport0 exit is inferred to be `MapObjStartDemo` at (-6630,500,1600) (`sms/src/MoveBG/MapObjTown.cpp:353-364`, trailing s32=2 -> `fireStreamingMovie(2)` -> `setNextStage(0x101)`; airport0's `map.col` has no 0x200/0x300 collision planes) [INFERENCE, not re-checked in round 7]. The old next step (condition-based `waitfor` verb; pad-clock WIP uncommitted) is not re-checked here. `TMarDirector::unk261` is never written in retail either (uninitialised there); its readers take no branch on 0.
- R02/R03: `bianco/0` and `dolpic/0` ran stable for 100 s each (normal and heap-check mode). Check-mode `0xc0000374` was a data race in the debug quarantine (fixed with a mutex in `src/sdk/jkr_new_delete.cpp`, committed in `92390cb`). The earlier normal-mode crashes were not reproduced; cause unproven.
- B3: an intermittent flat red/blue wedge in Bianco frames (about 1 of 6 runs at frame 600) also occurs with widescreen off and FSR off; cause open (the command stream differs run to run before the renderer; seeds do not pin it).
- Phase 4/5/6/7 items: see the README table. Verified by log/PNG: FSR1 pass, cheats lives/coins, glyph style change, Hor+ frame widths. Not verified: 4:3 identity, interpolation at 60/120/144, mod overlay effect, water cheat.
- Process note: `tools/reap.py --loop` kills `sunshine.exe` older than 6 min or older than `src/sdk/av.cpp`; rebuild after av.cpp edits. Use `tools/runlock.py` to serialise game runs.

## 1. Verified measurements (this audit)

| Check | Result | Source |
| --- | --- | --- |
| Matching `sms/` build SHA-1 | **Match.** `sms/build/GMSJ01/mario.dol` (4,114,624 bytes) = `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, equal to `sms/config/GMSJ01/build.sha1` | `hashlib` on file, `sms/config/GMSJ01/build.sha1` |
| Decomp match (`report.json`) | 10,960/12,881 functions (game code 6,945/8,806), 57.9% code matched, fuzzy 97.9%; 954 game functions with whole-function asm review; 973 game functions open | `sms/build/GMSJ01/report.json`, `decomp-verified.tsv` |
| Git-tracked game data | 0 data files (`.dol .szs .arc .rvz .iso .aaf .thp .bin .jpa .bti`) in `sms` (1,569 tracked files), `sunshine-pc` (86) or root (89). `sms/orig/GMSJ01/` extraction and RVZ exist on disk, untracked | `git ls-files` |
| Link state | Lead reports strict link with 0 unresolved. Not re-verified. `docs/unresolved.md` is an older 171-symbol snapshot | `docs/unresolved.md` (stale) |
| Empty-body functions in port layer | 6 `{ }` bodies. All intentional: `dummy_handler`/`Hx_Warning` match retail `blr`; `OSProtectRange` has no PC equivalent; `gx_extra.cpp` break-point no-ops; `OSFillFPUContext` zeroes the dump snapshot. No hidden stubs found | `sunshine-pc/src/sdk/*`, `launcher.cpp` (false positive: constructor) |
| `TEMP-DEBUG` markers left | **0** in `sms/src` and `sunshine-pc/src` now. `platform-notes.md` line 465 names one in `Application.cpp`; it appears removed | grep |
| Excluded/blocked lists | `excluded-units.txt`: 26 prefix rules (151 units per notes). `blocked-units.txt`: **empty** (only header). Notes say 3 blocked (Koopa, bossManta, hanasambo) | files |
| Stray root artifacts | `sunshine-pc/buildaurora-gxtestsbuild-probe.out` and `sunshine-pc/crash.log` exist. Likely scratch | glob |

### Conflicts I could not resolve (flagged, not picked)
- **Decomp %**: Port.md says ~41% code / 8,585 functions. platform-notes says 47.44% code / 9,160 functions. `report.json` now says 54.1% / 10,699. Use `report.json` as the source of truth.
- **Unit counts**: 579 compiled / 151 excluded / 3 blocked (notes) versus 733 total (Goal-Port). `report.json` lists 736 units, and my prefix match against `excluded-units.txt` could not reproduce the 151 (unit name format differs from `sms/src` paths). `probe-units` is needed to settle it.
- **Blocked**: notes list 3; file now lists 0. Likely already removed, `files.cmake` needs confirming.

## 2. Goal-Port §6 acceptance

| # | Criterion | State | Evidence / gap |
| --- | --- | --- | --- |
| A1 | `probe-units` 733/733 or justified exclusions | **Partial** | 151 exclusions with reasons (`excluded-units.txt`). Count not reproducible from `report.json` here. Re-run `probe-units` |
| A2 | No empty/stub source in port build; missing-function list empty or justified | **Open** | `unresolved.md`: 157 class/method + 10 game-global symbols from empty or unfinished decomp sources (Enemy bosses/NPCs, `SMS_EmitRipple*`, `FeetInvCalc`, `j3dDefault*`). Not justified per-symbol |
| A3 | `sunshine.exe` links with 0 unresolved and boots image -> title -> file select -> Delfino -> Bianco Ep1 with Shine and saving | **Partial** | (superseded in part: see 'Latest lead update' at the top) Link 0 unresolved (measured by lead at round 7, `tools\_strict.cmd` STRICT: 0 unresolved). Boot to title works but washed out. Stage 0 `airport0` (scen 0) reached by pad input with no skip switches in a PortFlow run (`build/PortFlow/watch.log` 1237-1270: `dir.state=4`, billboard dismissed by A, status 0x10001308 -> 0x0C400201). Delfino and Bianco reached only with dev aids: route 70 stage 1/0 at logic t=284 s, stage 2/0 at t=590 s (`u01`) and t=715 s (`u02`). Delfino (`dolpic0`) was blocked by R03 (crash at 12-19 s) in an earlier run; route 70 runs `u01`/`u02` (rc=0, no crash) and round-7 chase runs reached Delfino. `delfino/0` reached gameplay (fps 144 run). `bianco/0` crashes in `TNpcParts::TNpcParts`. Shine, saving, file select flow unverified |
| A4 | Matching `sms/` build passes SHA-1 | **Done** | Verified above |
| A5 | Phases 4-8 done or documented with blocker | **Partial** | Phases 4-8 not done. Blockers documented in `platform-notes.md` and `dlss-feasibility.md` |

## 3. Requirement table (Port.md phases 1-8, §5-§6), ordered by criticality for "fully playable from disc image"

Order: P0 blocks playing the game from the disc image. P1 is required to ship. P2 is polish and extra.

| ID | Req | Phase | State | Evidence | Gaps |
| --- | --- | --- | --- | --- | --- |
| R01 | Title screen renders correctly | 2 | **Open** | Sun emblem only in black box at top-left (~40%×57%). Platform-notes line 404 onward | Root cause not fixed. `GXCopyDisp` is an empty stub (`extern/aurora/lib/dolphin/gx/GXFrameBuffer.cpp:206`). Retail clears the EFB after display copy; Aurora ignores it. Hypothesis unproven |
| R02 | Stage loads: bianco, ricco, mamma, sirena | 3 | **Open** | Earlier crash `TNpcParts::TNpcParts` at `sms/src/NPC/NpcParts.cpp:56`. Not reproduced in 3 normal and 2 `SUNSHINE_CHECK_HEAP=1` `bianco/0` runs at 100 s (`build/StageCrash/bianco-*.watch.log`) | Root cause unproven. Zero-clear loop is in bounds. Only bianco run; ricco, mamma, sirena not run. Owner: StageBianco |
| R03 | Stage load: dolpic (Delfino) without memory corruption | 3 | **Open** | Check mode `0xc0000374`: fixed. Cause was a race in the debug quarantine (`src/sdk/jkr_new_delete.cpp`), now mutex-guarded. Normal mode `0xc0000409` at 12-19 s not reproduced in 4 normal runs at 100 s (`build/StageCrash/dolpic-*.watch.log`) | Normal-mode cause unproven. Quarantine is not active in normal mode, so the mutex does not explain it. [INFERENCE] may be the WGSL shader error (platform-notes ~515), not heap damage |
| R04 | Empty/unfinished decomp sources filled (Enemy bosses, NPCs, fences, rails, Rocket, Sand, Talk2D2, Guide, Select shine, etc.) | 1, 3, 4 | **Open** | 157 symbols + 10 globals (`unresolved.md`) | Full list is in `unresolved.md`. Boss Hanachan nerves, BW nerves (`TNerveBW*`), Manta, Sand, Select shine manager, Talk2D2 all still empty |
| R05 | `__fres` (bit-exact reciprocal estimate) | 2, 3 | **Open** | Undefined symbol, blocks `JPABaseField::calcFieldFadeScale` (68/284 particle resources) and `JPAVecToRotaMtx` (185/284) | Every particle emitter hits it. No emitter runs |
| R06 | Global `JKR_NEW`/`JKR_DELETE` allocator | 2 | **Partial** | `src/sdk/jkr_new_delete.cpp` exists. Global operator new/delete only as placement overloads under `TARGET_PC` | Goal-Port B.4 not done (`JKR_NEW` path). OOM aborts |
| R07 | Pointer truncation (`JSUConvertOffsetToPtr`, `(uintptr_t) < 0xC0000000`) | 2 | **Open** | Goal-Port B.3 open (platform-notes) | Not done |
| R08 | Game logic at 30 Hz | 3 | **Done** | Log: `[framerate] logic 30/s, extra presents 0/s, fpsLimit 0`, 56 samples over 60 s title run (platform-notes) | Only title and delfino verified |
| R09 | Unlocked framerate, pacing window (30/60/120/144/240/unlimited) | 5 | **Partial** | `src/framerate/`, `sunshinePaceWindow`. Gate open at delfino/0 fps 144 (stack dump only) | Per-second window count not logged. 60/120/240 not measured. Logic 30 ± 0.1 Hz in gameplay not verified. Step 2 interpolation not done |
| R10 | Widescreen and Hor+, 4:3 UI anchoring (part B) | 5 | **Not started** | Designed only. Setting persisted (`launcher`) but no effect | No code |
| R11 | Movies (intro, THP) on screen | 3, 4 | **Partial** | THP decoder max error 0 vs reference (offline) | On-screen picture not verified at boot |
| R12 | Shine sequence, saving (memory card) | 3 | **Partial** | `src/sdk` card path; `CardSave` crash fixed via dev skip (`gDevStartSkipPreStageMovie`) | Save and load not verified in-game. Shine flow not verified |
| R13 | Full playability: all worlds, bosses, Yoshi, nozzles, Corona Mtn, credits, music, saving | 4 | **Not started** / unverified | Not reached beyond delfino stage load | Bosses and Yoshi depend on R04. Music depends on DSP (R20) |
| R14 | Audio: DSP host (ADPCM, PCM, loops, streams, osc waveforms 0/1/3/4/7/10, resampler, FIR/IIR, pan) | 3, 4 | **Partial** | `src/sdk/dsp_glue.cpp`, `dsp_mixer.cpp`. `dsp_test.cmd` | Not implemented: 2-bit ADPCM, other osc waveforms, per-channel delay, dist low-pass, surround buses 8/9, Dolby. Runtime order unverified |
| R15 | Float contraction and paired-single `ps_madd` (`fmaf`) policy | 2, 3 | **Open** | Undecided (platform-notes). `OSf32tos8` NaN behavior unverified | Affects match and gameplay numerics |
| R16 | File formats swapped on host (RARC, BTI, BFN, BLO/J2D, BMG, ResNTAB, J3D, JAudio AAF/BNK/WSYS/BARC, JPA, THP) | 2 | **Partial** | Offline tests (`j3d-test`, `jpa_test`, `thp_test`). `BCSV_U32/U16` read-only swap fix in `ToolData.hpp` | Runtime-verified only where the stage ran. `ms_m_spinos.jpa` disc one-byte shift defect unhandled |
| R17 | Frame pacing / `sunshineReplayFrame` with skeletal animation | 5 | **Partial** | Replay masks `CUE_MOVE | CUE_CALC_ANIM | CUE_DRAW`. Skeletal animation not interpolated | Visual check of interpolated frames not done |
| R18 | Logic tick hook: cheats, mods | 6 | **Partial** | Hook `sms/src/System/MarDirectorDirect.cpp` → `sunshineLogicTick()`. `port_tests.cmd` 51 checks expected with the game-code host checks (`tests/sms_host_checks.cpp`; 52 observed by RefDecompFix, then the hit-check table replica check was dropped). `[cheats] 4 cheats loaded`, example mods load, SEH isolation proven | In-game cheat effects not verified (health, water, lives, nozzles). `cheats.user.json` and overlays not verified. Instant-death paths bypass `decHP` guard (`MarioMove.cpp` ~1948, ~2247, `MarioRun.cpp` ~978). `gpMarioOriginal` may dangle |
| R19 | Launcher: image select (ISO/GCM/RVZ/WIA/CISO/GCZ), hash detection, remember path, `--dvd` | 6 | **Partial** | Pages rendered (video, audio, controls, interface, cheats). `--dvd` path works. Disc check: `aurora_dvd_open` + `GMSJ01` | Browse, Start, invalid-image rejection, clicks not verified. Hash-based region/revision detection not implemented (only ID check). Cheats tab placeholder. No gamepad navigation |
| R20 | Launcher graphics (res, window mode, monitor, VSync, FPS, internal scale, MSAA, aniso, aspect, HUD scale, D3D12/Vulkan) | 6 | **Partial** | `settings.ini` round trip ok (`settings_test`). Window mode, size, VSync (next start), render scale, FPS presets (0/30/60/120/144) applied | No MSAA, aniso, aspect, HUD scale, monitor, D3D12/Vulkan choice. FPS and scale are cycling presets, not sliders |
| R21 | Controls: keyboard + SDL3 gamepad, profiles, analog trigger half-press, deadzones, sensitivity, camera inversion | 6 | **Partial** | Keyboard defaults (WASD, IJKL, arrows, Space/E/Q/F, Enter, Z, LShift, LCtrl). Stick dead zones applied via `applyDeadZones` | Trigger zones at Aurora defaults. No profiles, sensitivity, inversion. Gamepad pad mapping unverified |
| R22 | Button prompts (GC/Xbox/PS/Switch/keyboard, BMG and textures) | 6 | **Partial** | 90 PNGs `assets/glyphs/{xbox,playstation,switch,gamecube,keyboard}` (keyboard added 2026-10-15, original art). Style setting persists and applies (`glyphsSetStyle`). `[glyph] <style>/{a,b,x} 128x128` logged for all four styles (`build/PortLauncherRun/glyph_<style>.log`). Frames show the style change at the water-button slot (xbox blue X, playstation pink square). `playstation/x.png` is a square (□), not a cross (✕) | Only A, B, X, Start hooked (`JUTResource.cpp`). Y, Z, L, R, D-pad, C-stick, stick glyphs not hooked. BMG prompt text not done. `guide_tx_b` visual check pending. No keyboard style. `[glyph] <style>/start` not in the four style logs; seen once in `build/PortLauncher/shot.log` |
| R23 | Audio settings: volumes, device | 6 | **Partial** | Master volume applied. Volume presets 100/75/50/25/0 | Per-channel volume and device selection not done (`launcher.cpp:86-90`) |
| R24 | Mods: folder, enable/disable, load order, SZS/RARC path replacement, Dolphin `tex1_` packs, conflicts | 6 | **Partial** | Scan `<exe>/mods/*/mod.json`, `mod.dll`, ABI 1.0, load order, overlay merge, SEH. Loader log measured: `[mods] 1 overlay file(s) active` in 10 mod-on runs; `no mods folder` in 3 mod-off runs (`build/PortLauncherRun/*.log`) | Overlay effect not verified: no `overlay in use` / `overlay replaced` line in any run; station text not reached at bianco. `tex1_` packs not present. Conflict reporting not present. Enable/disable UI not verified |
| R25 | Cheats: native toggles, explain Gecko/AR unsupported | 6 | **Partial** | `assets/cheats.json` (4 cheats), fixed C++ effect table | Effects in-game unverified. Gecko/AR message not checked |
| R26 | Storage `%APPDATA%\<project>`, TOML/JSON | 6 | **Partial** | Settings at `%APPDATA%\Sunshine\settings.ini` (fallback exe dir). Config is INI, not TOML/JSON | Spec mismatch: Port.md says TOML/JSON. Document or change |
| R27 | DLSS | 7 | **Not started** | Not implemented. Blockers: no motion vectors/jitter in GX path, Dawn D3D12 import-only, no DLSS SDK headers (`docs/dlss-feasibility.md`) | Phase 7 blocked, documented |
| R28 | FSR1 upscaler (extra, not requested) | — | **Partial (extra)** | `SUNSHINE_UPSCALER=fsr1`, `render_scale.cpp`, `gpu.cpp`. Packaged exe runs 20 s, WGSL compile not confirmed | Not applied to RmlUI frames with a backdrop filter (RmlUI overlay frames are resampled). In-game frames measured on WARP in T2 and after Aurora patch 0015 (`platform-notes.md`, T2 section). Not requested by Port.md; decide keep or drop |
| R29 | Release package, installation-free, no protected data | 8 | **Partial** | `tools/package.py` → `dist/sunshine-0.1.0/` (+ zip, sha256, 20.3 MiB). Hard-fails on game-image extensions. No game data tracked (verified above) | Licence texts not pinned (Dawn, nod, SQLite, DXC). VC redist EULA unchecked. Port licence undecided. `release-checklist.md` still names `build/windows-clang-release` |
| R30 | Packaged run from clean dir, `--dvd` | 8 | **Partial** | 20 s from clean dir, process stays alive, log empty | No screenshot or in-game verification |
| R31 | Dev switches and test harness documented and removed from release | 8 | **Partial** | `SUNSHINE_DEV_START`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_CHECK_HEAP`, `SUNSHINE_HANG_DUMP`, `SUNSHINE_WINDOW_*`, `SUNSHINE_DEV_LAUNCHER_PAGE`, `SUNSHINE_DEV_AUDIO_STATS` | Need decision: keep as dev-only or gate out of release |

## 4. Historical blockers from the original audit (superseded)

1. **Title render (R01)**: `GXCopyDisp` stub and EFB clear. Blocks visual correctness of every screen.
2. **Stage crashes (R02, R03)**: NpcParts ctor and dolpic heap writer. Blocks Bianco Ep1 and most worlds.
3. **Empty decomp source (R04)**: bosses, NPCs, fences, sand, talk, guide. Blocks boss and stage-object logic listed in `unresolved.md`.
4. **`__fres` (R05)**: blocks every particle emitter. No effects in game.
5. **Audio gaps (R14)**: missing ADPCM and osc waveforms blocks music correctness.
6. **Unverified gameplay**: nothing past delfino load is verified in-game (shine, save, bosses, credits).

## 5. Historical task list (superseded; not the current queue)

The numbered list below is retained as audit history, not an instruction to redo completed work.
Use the [current playability matrix](playability-matrix.tsv) for owners, completed bounded proofs, active progression blockers and outstanding acceptance criteria.

1. Fix `GXCopyDisp`: snapshot EFB into display source before any clear, and present that source (R01). Owner: GxCopy2 (coordinate; they own `lib/gx/*`).
2. Gate title-frame logs on copy-2 count, check background quad vs viewport and `drain()` (R01).
3. Verify copy-2 source rect (1280x960) vs logical 640x448 with aspect offset (0,32) (R01).
4. `TNpcParts::TNpcParts` crash at `NpcParts.cpp:56` for bianco/ricco/mamma/sirena (R02): not reproduced in 5 bianco runs. Prove the cause or retire it. Owner: StageBianco.
5. R03: check-mode quarantine race fixed. Prove or retire the normal-mode `0xc0000409` at 12-19 s. Owner: HeapHunt.
6. Run `probe-units` once and reconcile 151 excluded vs `report.json` 736 units. Update `excluded-units.txt` and `blocked-units.txt` if needed (A1).
7. Re-run `tools/gen-files.py` and confirm `files.cmake` matches blocked list (now empty) (A3).
8. Implement `__fres` with bit-exact reciprocal estimate (R05). Unblocks particle emitters.
9. Implement global `JKR_NEW`/`JKR_DELETE` allocator path (R06).
10. Fix pointer truncation: `JSUConvertOffsetToPtr` and `(uintptr_t) < 0xC0000000` (R07).
11. Fill empty bodies in order: Boss Hanachan (`TNerveBossHanachan*`), then `TNerveBW*`, Manta, Sand, Talk2D2, Guide (R04). Map `TNerveBW*` to its game object first (name not confirmed in this audit).
12. Add per-symbol justification list for the 157 + 10 undefined (A2), or fill them.
13. Decide float-contraction policy and `ps_madd` as `fmaf`. Verify `OSf32tos8` NaN (R15).
14. Implement missing DSP features in priority order: 2-bit ADPCM, then other osc waveforms (R14).
15. Verify in-game: title → file select → Delfino → Bianco Ep1 with Shine and save (A3, R12). Use `SUNSHINE_DEV_START` and `computer` screenshots.
16. Verify cheats effects in-game (health, water, lives, nozzles) and fix instant-death guard gaps (R18).
17. Verify launcher Browse, Start, invalid-image rejection, clicks (R19). Add gamepad navigation for launcher.
18. Hook remaining button glyphs (Y, Z, L, R, D-pad, C-stick, stick) and verify `guide_tx_b` (R22).
19. Settle the Port.md spec gaps: TOML/JSON storage, hash-based region detection, MSAA/aniso/aspect/monitor (R20, R26).
20. Clean up release: pin licence texts, check VC redist EULA, update `release-checklist.md` to `build/ReleaseDlss`, decide on `SUNSHINE_DEV_*` in release, remove stray root `buildaurora-gxtestsbuild-probe.out` and `crash.log` (R29, R31).

Deferred (not in the 20): widescreen part B (R10), framerate step 2 interpolation (R17), DLSS (R27, blocked by SDK and motion vectors).

## 6. Historical checks from the original audit (superseded)
- `probe-units` (resolve 151/736 count and the blocked list).
- Strict link and `_link.log` to confirm 0 unresolved and refresh `unresolved.md`.
- SHA-1 check of `sms/build/GMSJ01/mario.dol` against `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` (matches now).
- Title and delfino runs with `computer` screenshots (not yet done in-game).

### Update 2026-10-12 (measured): first full chain logos -> Bianco -> Bianco Shine -> save -> reload

Route `sunshine-pc/tools/routes/60_acceptance.dev`, run twice in a row (`h01`, `h02`): rc=0, 144 s and 147 s wall time, no `overrun`, no crash, 13 PNGs each (airport0 `ap0.png`, Delfino `dolpic.png`, gate `gate.png`, Bianco `bianco.png`, Shine get `shine_get.png`/`shine_get2.png`). Stages seen in `run.log`: 15 (option map) -> 0/0 -> 0/1 -> 1/0 -> 1/1 -> 2/0 (Bianco, `dir.state=4`, Mario steerable, status `0x0C400201`) -> 1/5. `shines=2`, `.gci` PASS (`tools/gci_check.py`), reload with `tools/routes/61_reload.dev` (file block B, real time): `stage=1 scen=5 shines=2`, rc=0 (`h05`).
Command: `python tools/route.py Strict tools/routes/60_acceptance.dev --name <n> --seconds 330 SUNSHINE_DEV_STEP_RETRACE=1 SUNSHINE_DEV_SKIP_MOVIES=1 SUNSHINE_DEV_AIM_ASSIST=1 SUNSHINE_ALLOW_CPU_ADAPTER=0`.
**What is NOT retail input (the chain is dev-assisted, so the acceptance wording "no dev switches" is not met):** `SUNSHINE_DEV_STEP_RETRACE` (logic faster than 30 Hz; the 144 s is not 30 Hz wall time), `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST` (faces the head, sets nozzle pitch, forces the crouch; the Delfino spray gate), script verbs `cheat infinite_health/infinite_water`, `tp` (pit/boss/gate positions), `tpem` (shadow-Mario chase), `shine 2` (touches the Bianco 100-coin Shine). Real: airport boss killed with water via assist + short `hold L` cycles, airport Shine taken by walking to it and pressing A, Delfino GateKeeper killed with water (flag `0x10384` in the log), gate walk-in, Bianco load.
Bug found and fixed on the way: `JAIData::initSeqParameter` left `mSeqHandle` 0 on PC, so `JAISeqEntry::storeBuffer` stopped the SE master sequence and `processFrameWork` crashed at the airport exit (session log section 38).

### Update 2026-10-12 (later): route 70, fewer dev crutches

`sunshine-pc/tools/routes/70_boss_until.dev` run twice in a row (`u01` 153 s, `u02` 193 s wall, rc=0, no `overrun`, no crash, 13 PNGs, `gci_check.py` PASS, reload `stage=1 scen=5 shines=2`). Remaining dev: `cheat infinite_health`, `chase` (position read, real input), `shine 2`, env `SUNSHINE_DEV_STEP_RETRACE`/`SKIP_MOVIES`/`AIM_ASSIST`. Gone: `tp`, `tpem`, `killboss`. Not met: a real Bianco Shine without `shine 2` (no measured floor path to any of the three Shines), 330 s at real 30 Hz, no dev switches. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 42.

### Update 2026-10-12 (decomp pass): 11 020 / 12 881 functions byte-identical

After a 12-agent decomp pass (details in `sunshine-pc/docs/session-2026-10-11b.md` section 44), measured by the lead: DOL SHA-1 unchanged, matched functions 11 013 -> 11 020, units 445 / 736, fuzzy total 98.12 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Several real behaviour bugs fixed (Koopa nerves, BathtubKiller, bossgesso, ModelWaterManager, fenceMove, Kumokun, GCConsole2).

### Update 2026-10-12 (decomp round 2)

Gates: SHA-1 unchanged, 11 020 / 12 881 functions byte-identical, 445 / 736 units, fuzzy total 98.23 %, strict link 0 unresolved, `TEMP-DEBUG` 0. Route 70 smoke after the edits: 3 of 6 runs complete; one chase fall, one rare crash in the shadow-Mario replay (`TEnemyMario::emReplayJumpToNearestNode`, run `v02`, cause open). Details: session log section 45.

### Update 2026-10-12 (decomp round 3): 11 112 / 12 881 functions byte-identical (86.3 %)

Measured after round 3: SHA-1 unchanged, 11 112 / 12 881 functions byte-identical (86.3 %), matched code 59.67 %, fuzzy 98.23 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke rc=0. Details: `sunshine-pc/docs/session-2026-10-11b.md` section 47.

### Update 2026-10-12 (decomp round 4): 11 191 / 12 881 functions byte-identical (86.9 %)

Measured: SHA-1 unchanged, 11 191 / 12 881 byte-identical (86.9 %), matched code 60.69 %, fuzzy 98.25 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: session log section 49.

### Update 2026-10-12 (decomp round 5): 11 247 / 12 881 functions byte-identical (87.3 %)

Measured: SHA-1 unchanged, 11 247 / 12 881 byte-identical (87.3 %), matched code 61.48 %, fuzzy 98.27 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0, route 70 smoke 2 of 2 rc=0. Details: session log section 50.

### Update 2026-10-12 (decomp round 6): 11 282 / 12 881 functions byte-identical (87.6 %)

Measured: SHA-1 unchanged, 11 282 / 12 881 byte-identical (87.6 %), matched code 61.98 %, fuzzy 98.60 %, 445 / 736 units, strict link 0 unresolved, `TEMP-DEBUG` 0; route 70 smoke 1 of 2 complete (chase flake). Details: session log section 52.
