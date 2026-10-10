# Port status audit

Scope: Port.md phases 1-8 and §5-§6 features, plus the Goal-Port §6 acceptance criteria. Read-only audit by PortAudit. No game runs, no builds, no edits outside this file.

Legend: **Done** = verified by observed artifact. **Partial** = some code or verification exists, gaps remain. **Not started** / **Open** = no working implementation or known blocker. `[INFERENCE]` = not observed directly.

## 0. Lead update (later than this audit; supersedes the rows it contradicts)

Observed by the lead with `AURORA_SCREENSHOT` and `SUNSHINE_DEV_STATUS` logs (dev start `SUNSHINE_DEV_START=bianco/0`):

- Strict link: 0 unresolved symbols. Matching DOL SHA-1 unchanged. TEMP-DEBUG grep: 0.
- Bianco Hills renders textured; HUD (Shines, blue coins, coins, lives) renders; Mario survives WARP_OUT, walks, jumps, camera follows; director reaches state 4.
- NOT verified / broken: title screen render and the retail flow title -> file select -> new game -> Delfino -> Bianco; first Shine (only a dev hook exists, it finds no TShine because the Bianco Ep.1 Shine is event-spawned after the boss) and saving; goop/graffiti (pixel metric is visibility-only 0/1, so `PollutionCount` magnitudes are wrong); title/gameplay BGM (title music ends after ~20 s, gameplay has broadband noise); THP audio channel order (decoder agrees with its own model but matches ffmpeg only with L/R swapped: likely swapped); replay/interpolation at >30 fps in gameplay (gate `mState == STATE_UNK4` only just reachable); widescreen visuals; FSR1; launcher click paths; cheat effects; mods in-game. DLSS: feasibility document only.
- Known decomp gaps: `PacketUtil::ShapePacketCallBackFunc` (empty stub), `ConsoleStr::processGo`/`startCloseWipe` (placeholders), `EmitterViewObj`/`EmitterIndirectViewObj` resolve to null in the perform lists (particles likely not drawn), several Audit items listed in platform-notes.
- `probe-units`: clang-cl 624/733 clean (all 584 built units); MSVC `cl /Zs` 565/733 (66 built units fail on MWCC-specific syntax; reasons in platform-notes).

## 1. Verified measurements (this audit)

| Check | Result | Source |
| --- | --- | --- |
| Matching `sms/` build SHA-1 | **Match.** `sms/build/GMSJ01/mario.dol` (4,114,624 bytes) = `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, equal to `sms/config/GMSJ01/build.sha1` | `hashlib` on file, `sms/config/GMSJ01/build.sha1` |
| Decomp match (`report.json`) | 83.1% functions (10,699/12,881), 54.1% code matched, 20.2% code complete, 415/736 units complete, fuzzy 96.3% | `sms/build/GMSJ01/report.json` |
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
| A3 | `sunshine.exe` links with 0 unresolved and boots image → title → file select → Delfino → Bianco Ep1 with Shine and saving | **Partial** | Link 0 unresolved (lead, not re-verified). Boot to title works but washed out. Only `delfino/0` reached gameplay (fps 144 run). `bianco/0` crashes in `TNpcParts::TNpcParts`. Shine, saving, file select flow unverified |
| A4 | Matching `sms/` build passes SHA-1 | **Done** | Verified above |
| A5 | Phases 4-8 done or documented with blocker | **Partial** | Phases 4-8 not done. Blockers documented in `platform-notes.md` and `dlss-feasibility.md` |

## 3. Requirement table (Port.md phases 1-8, §5-§6), ordered by criticality for "fully playable from disc image"

Order: P0 blocks playing the game from the disc image. P1 is required to ship. P2 is polish and extra.

| ID | Req | Phase | State | Evidence | Gaps |
| --- | --- | --- | --- | --- | --- |
| R01 | Title screen renders correctly | 2 | **Open** | Sun emblem only in black box at top-left (~40%×57%). Platform-notes line 404 onward | Root cause not fixed. `GXCopyDisp` is an empty stub (`extern/aurora/lib/dolphin/gx/GXFrameBuffer.cpp:206`). Retail clears the EFB after display copy; Aurora ignores it. Hypothesis unproven |
| R02 | Stage loads: bianco, ricco, mamma, sirena | 3 | **Open** | Crash `TNpcParts::TNpcParts` at `sms/src/NPC/NpcParts.cpp:56` | Not fixed (StageBianco owns the fix) |
| R03 | Stage load: dolpic (Delfino) without memory corruption | 3 | **Open** | Crash on read of `000002AC5FFC0008`. Heap-corruption writer unidentified. Crash at 12-19 s with `0xc0000409`, or `0xc0000374` with `SUNSHINE_CHECK_HEAP=1` | Probes negative (footer, live sweep, page heap). Next probe: never release freed operator-new blocks. Not built |
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
| R18 | Logic tick hook: cheats, mods | 6 | **Partial** | Hook `sms/src/System/MarDirectorDirect.cpp` → `sunshineLogicTick()`. `port_tests.cmd` 38 checks, 0 failed. `[cheats] 4 cheats loaded`, example mods load, SEH isolation proven | In-game cheat effects not verified (health, water, lives, nozzles). `cheats.user.json` and overlays not verified. Instant-death paths bypass `decHP` guard (`MarioMove.cpp` ~1948, ~2247, `MarioRun.cpp` ~978). `gpMarioOriginal` may dangle |
| R19 | Launcher: image select (ISO/GCM/RVZ/WIA/CISO/GCZ), hash detection, remember path, `--dvd` | 6 | **Partial** | Pages rendered (video, audio, controls, interface, cheats). `--dvd` path works. Disc check: `aurora_dvd_open` + `GMSJ01` | Browse, Start, invalid-image rejection, clicks not verified. Hash-based region/revision detection not implemented (only ID check). Cheats tab placeholder. No gamepad navigation |
| R20 | Launcher graphics (res, window mode, monitor, VSync, FPS, internal scale, MSAA, aniso, aspect, HUD scale, D3D12/Vulkan) | 6 | **Partial** | `settings.ini` round trip ok (`settings_test`). Window mode, size, VSync (next start), render scale, FPS presets (0/30/60/120/144) applied | No MSAA, aniso, aspect, HUD scale, monitor, D3D12/Vulkan choice. FPS and scale are cycling presets, not sliders |
| R21 | Controls: keyboard + SDL3 gamepad, profiles, analog trigger half-press, deadzones, sensitivity, camera inversion | 6 | **Partial** | Keyboard defaults (WASD, IJKL, arrows, Space/E/Q/F, Enter, Z, LShift, LCtrl). Stick dead zones applied via `applyDeadZones` | Trigger zones at Aurora defaults. No profiles, sensitivity, inversion. Gamepad pad mapping unverified |
| R22 | Button prompts (GC/Xbox/PS/Switch/keyboard, BMG and textures) | 6 | **Partial** | 72 PNGs `assets/glyphs/{xbox,playstation,switch,gamecube}`. Style setting persists and applies (`glyphsSetStyle`) | Only A, B, X, Start hooked (`JUTResource.cpp`). Y, Z, L, R, D-pad, C-stick, stick glyphs not hooked. BMG prompt text not done. `guide_tx_b` visual check pending |
| R23 | Audio settings: volumes, device | 6 | **Partial** | Master volume applied. Volume presets 100/75/50/25/0 | Per-channel volume and device selection not done (`launcher.cpp:86-90`) |
| R24 | Mods: folder, enable/disable, load order, SZS/RARC path replacement, Dolphin `tex1_` packs, conflicts | 6 | **Partial** | Scan `<exe>/mods/*/mod.json`, `mod.dll`, ABI 1.0, load order, overlay merge, SEH. Verified in 30 s run | SZS/RARC path replacement and `tex1_` packs not present. Conflict reporting not present. Enable/disable UI not verified |
| R25 | Cheats: native toggles, explain Gecko/AR unsupported | 6 | **Partial** | `assets/cheats.json` (4 cheats), fixed C++ effect table | Effects in-game unverified. Gecko/AR message not checked |
| R26 | Storage `%APPDATA%\<project>`, TOML/JSON | 6 | **Partial** | Settings at `%APPDATA%\Sunshine\settings.ini` (fallback exe dir). Config is INI, not TOML/JSON | Spec mismatch: Port.md says TOML/JSON. Document or change |
| R27 | DLSS | 7 | **Not started** | Not implemented. Blockers: no motion vectors/jitter in GX path, Dawn D3D12 import-only, no DLSS SDK headers (`docs/dlss-feasibility.md`) | Phase 7 blocked, documented |
| R28 | FSR1 upscaler (extra, not requested) | — | **Partial (extra)** | `SUNSHINE_UPSCALER=fsr1`, `render_scale.cpp`, `gpu.cpp`. Packaged exe runs 20 s, WGSL compile not confirmed | Not applied when RmlUI overlay active. Not requested by Port.md; decide keep or drop |
| R29 | Release package, installation-free, no protected data | 8 | **Partial** | `tools/package.py` → `dist/sunshine-0.1.0/` (+ zip, sha256, 20.3 MiB). Hard-fails on game-image extensions. No game data tracked (verified above) | Licence texts not pinned (Dawn, nod, SQLite, DXC). VC redist EULA unchecked. Port licence undecided. `release-checklist.md` still names `build/windows-clang-release` |
| R30 | Packaged run from clean dir, `--dvd` | 8 | **Partial** | 20 s from clean dir, process stays alive, log empty | No screenshot or in-game verification |
| R31 | Dev switches and test harness documented and removed from release | 8 | **Partial** | `SUNSHINE_DEV_START`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_CHECK_HEAP`, `SUNSHINE_HANG_DUMP`, `SUNSHINE_WINDOW_*`, `SUNSHINE_DEV_LAUNCHER_PAGE` | Need decision: keep as dev-only or gate out of release |

## 4. Blockers that gate "fully playable" (summary)

1. **Title render (R01)**: `GXCopyDisp` stub and EFB clear. Blocks visual correctness of every screen.
2. **Stage crashes (R02, R03)**: NpcParts ctor and dolpic heap writer. Blocks Bianco Ep1 and most worlds.
3. **Empty decomp source (R04)**: bosses, NPCs, fences, sand, talk, guide. Blocks boss and stage-object logic listed in `unresolved.md`.
4. **`__fres` (R05)**: blocks every particle emitter. No effects in game.
5. **Audio gaps (R14)**: missing ADPCM and osc waveforms blocks music correctness.
6. **Unverified gameplay**: nothing past delfino load is verified in-game (shine, save, bosses, credits).

## 5. Next 20 tasks (priority order)

1. Fix `GXCopyDisp`: snapshot EFB into display source before any clear, and present that source (R01). Owner: GxCopy2 (coordinate; they own `lib/gx/*`).
2. Gate title-frame logs on copy-2 count, check background quad vs viewport and `drain()` (R01).
3. Verify copy-2 source rect (1280x960) vs logical 640x448 with aspect offset (0,32) (R01).
4. Fix `TNpcParts::TNpcParts` crash at `NpcParts.cpp:56` for bianco/ricco/mamma/sirena (R02). Owner: StageBianco.
5. Implement heap-writer probe: `SUNSHINE_CHECK_HEAP=1` never releases freed operator-new blocks (R03). Owner: HeapHunt.
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

## 6. Checks the main agent should run
- `probe-units` (resolve 151/736 count and the blocked list).
- Strict link and `_link.log` to confirm 0 unresolved and refresh `unresolved.md`.
- SHA-1 check of `sms/build/GMSJ01/mario.dol` against `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` (matches now).
- Title and delfino runs with `computer` screenshots (not yet done in-game).
