# Goal: Super Mario Sunshine runs natively on Windows (Dusklight architecture)

## Current user goal (2026-10-16)

Complete the native Windows port through an unassisted new-save playthrough, credits and postgame, all original content including 120 Shines and 240 blue coins, reliable cold save/load, and an independently built and packaged Windows application.
100% byte-identical PowerPC matching is not a prerequisite; preserve existing matches and fix demonstrated gameplay behavior instead of pursuing register/stack cosmetics.
The current order, evidence limits and owners are in `ROADMAP.md` and `sunshine-pc/docs/playability-matrix.tsv`.
Preserve existing optional features; installer, signing, DLSS and new convenience work do not block the core port.
Human listening, physical-controller/GPU checks, a suitable clean Windows machine and licence decisions remain explicit external prerequisites when unavailable.

The starting measurements, original package order and acceptance records below are historical.
They do not supersede the current unassisted full-game criteria or turn developer-assisted routes into a completed playthrough.
Architecture and compatibility rules in `Port.md` still apply unless superseded by the current user goal.

## 1. Starting state (measured, verify against the files before starting work)

- `sunshine-pc/` exists: Aurora submodule `50e5be3`, CMake presets, `build.cmd` (MSVC 19.44, Ninja). `sunshine.exe` builds 647/647 and opens an Aurora window. No game code runs yet.
- `sunshine-pc/tools/probe-units.cmd` checks all 733 decomp sources via `cl /Zs` without MSL include paths. Status: **404 / 733 parse** (start: 73). Result per unit in `tools/probe-results.json`.
- Remaining error classes (count across all units): MSL headers such as `printf.h` included directly (164), `__declspec(weak)` (156), inline `asm` (139), `__attribute__((aligned))` (88 + 84), `(Type){...}` casts (106), non-constant expressions (84), `__start` (53).
- 52 source files in `sms/src` are empty (621,444 code bytes, 2,068 functions, 17.3% of the code), mainly Enemy: BossHanachan*, Kukku, TabePuku, BathtubPeach, Kazekun, plus Animal/Bird, BeeHive.
- Time base (from source, not verified at runtime): `TApplication::gameLoop()` calls `startRendering()`, `mDirector->direct()`, `endRendering()`. `TDisplay` waits 2 retraces per frame, that is 30 logic frames per second. Logic and drawing run in the same pass.
- The `sms` working tree contains foreign, uncommitted changes (among others `Koopa.cpp`, `hanasambo.cpp`, `PROGRESS.md`). Do not touch, do not stage, do not reset.

## 2. Decisions (do not renegotiate)

- **Architecture:** Source port like Dusklight, no emulator, no PowerPC recompilation. Aurora replaces GX/VI/PAD/OS/DVD/CARD. No custom renderer.
- **CRT instead of MSL:** The port uses the MSVC CRT. MSL include paths are not included. Where decomp code includes MSL headers directly, either switch to standard headers under `#if TARGET_PC` or put a small shim in `sunshine-pc/include/`. No rebuild of the complete MSL.
- **Delimitation:** Every change in `sms/` is under `#if TARGET_PC` (original code stays in place). UB fixes under `#if AVOID_UB`. Host replacements for Gekko intrinsics belong in `sunshine-pc/include/pc_compat.h`, not in `sms/`.
- **Float precision:** MSL constants stay `float`. `frsqrte` comes from Aurora `ppc_math.h`. No replacement with `sqrt` or `1.0f/sqrtf`.
- **Inline `asm`:** Replace with a C/C++ equivalent that yields the same float and integer results. Where Aurora already provides the function (Mtx, Vec, OS, THP, DVD, CARD, PAD, VI, GX, AI), exclude the decomp unit from the port build instead of rewriting it.
- **Matching build:** `sms/` keeps building with its toolchain and passes the reference check. After changes to shared headers in `sms/`, verify the affected units with the matching build (`python configure.py`, `ninja`, SHA-1 of the DOL). No reference hash is changed.
- **No sham solutions:** No stubs, no `return 0`, no empty body to hide a link or parse error. Missing functionality stays visibly marked as missing (link error or `#error`).
- **No game data and no PowerPC code** in the repository or in the exe. Game data comes at runtime from the image under `sms/orig/GMSJ01/`.
- **Git:** No `reset --hard`, no `clean`, no stash, no force pushes, no push without approval. Stage only your own files. `sunshine-pc/build/` stays ignored.

## 3. Work packages in this order

### A. Parse all units (count in `probe-units` to 733 / 733 or documented exclusions)
1. Work through error classes by number of affected units, each at the root (shared header or `pc_compat.h`), not per unit. After each step run `tools\probe-units.cmd` and note the count.
2. Keep units the port deliberately does not build (MetroTRK, `__start`, runtime, debug consoles, everything Aurora replaces) in an exclusion list in `sunshine-pc/`, with a justification per entry. No silent omission.
3. After each completed error class, update the notes in `sunshine-pc/docs/platform-notes.md` (table of parse counts).

### B. Actually compile and link
1. Derive `files.cmake` (or equivalent) from `sms/configure.py`, do not maintain by hand. Put the script in `sunshine-pc/tools/`.
2. Build statically per subsystem (JSystem, game code), link against Aurora. Output unresolved symbols as a list and sort by cause: missing in the port, replaced by Aurora, empty source file.
3. Fix 64-bit errors: structures that overlay file data directly or relocate 32-bit pointers in files (J3D/BMD/BDL, BCK/BTK/BRK, BLO, JDrama `scene.bin`, BMG, JPA, COL, RAIL). Document per format how it is loaded (byte swap on load, host structure separate).
4. Switch `operator new`/`delete` to the heap hierarchy as in Dusklight (`JKR_NEW`/`JKR_DELETE`).

### C. Boot to the first rendered frame with real decomp code
1. `sunshine-pc/src/main.cpp` calls `gpApplication.initialize()`, `proc()`, `finalize()`. Remove `OSProtectRange` with fixed addresses and other GC-specific calls in `initialize()` under `TARGET_PC`.
2. Pass the image to Aurora DVD via `--dvd <path>`. Boot to the Nintendo logo and title screen.
3. Smoke test: start the exe, check screen and log, locate the crash site with a debugger. A crash is only fixed once the run gets further afterwards.

### D. Empty sources (parallel to B and C, with subagents, each unit its own files)
1. Order by reachability in the game flow: boot, title, Delfino Plaza, Bianco Hills Episode 1 first, bosses and late worlds after.
2. For each function, read target address, symbol and assembly from `sms/config/GMSJ01` and `sms/build`, reconstruct per the rules from `Goal.md` §6. Functionally equivalent code is sufficient for the port. Set the status in the decomp to `NonMatching` or `Equivalent` only if proven.
3. Subagents must not modify the same headers. Shared header changes go through an integration owner.

### E. Then phases 4 to 8 from `Port.md`
Full playability, widescreen and free framerate (logic stays at original rate, rendering interpolates), launcher with settings, mods, cheats, button prompts, DLSS (check feasibility under Dawn first), release package.

## 4. Verification (measure every number yourself, take nothing over)

- Parse state: `sunshine-pc\tools\probe-units.cmd`.
- Port build: `sunshine-pc\build.cmd`, then smoke run of the exe.
- Matching build: build with `ninja` in the `sms` folder, compare reference SHA-1.
- Runtime: reference is Dolphin with the same image. Same input sequence, compare position, timers, camera. Explicitly mark tests not run as such. A screenshot is not proof of game logic.

## 5. Dealing with obstacles

- After three attempts without new insight: record the finding and discarded hypotheses in `platform-notes.md`, continue with a different work package.
- Missing input (file, path, hardware): name exactly what is missing and what was tried. Meanwhile do everything that works without this input.
- Before a non-trivial approach run `stateroot memory recall "failed approach <topic>"`. Record failures there.

## 6. Acceptance (everything must hold)

1. `probe-units` reports 733 / 733 or every exception is in the exclusion list with a justification.
2. There is no empty or stub source in the port build. The list of still-missing functions is empty or each one is justified as a blocker.
3. `sunshine.exe` links without unresolved symbols and boots from the image to the title screen, then through file select to Delfino Plaza and Bianco Hills Episode 1 including Shine sequence and saving.
4. The matching build of `sms/` still passes the reference check.
5. Phases 4 to 8 from `Port.md` are fulfilled or documented with a concrete blocker.

## 7. Reporting

Tersely in English, after every work block: changed, checks run with numbers, blockers, next step. Separate observed from assumed. No invented measurements, test results or commit IDs.

Start with package A, error class "MSL headers included directly" (164 errors). Then `__declspec(weak)`, then `asm`.

## 8. Acceptance evidence (measured by the lead; fresh clone of commit 06668e5, 2026-10-13)

Procedure (new folder next to the repo): `git clone`, `git submodule update --init` (Aurora 50e5be3), `apply-aurora-patches.py` ("applied: 11 patches", `--check`: "36 files already match"), `pip install -r tools/requirements.txt`, game data copied to `sms/orig/GMSJ01/`, `configure.py`, `ninja -k 0 -j4` in `sms/` (rc 0, 31 s wall time; dtk, sjiswrap and objdiff were downloaded by the build), `tools\locked.py cmd /c tools\_strict.cmd` (rc 0, 238 s, "STRICT: 0 unresolved symbols"), then route 72 (`tools/route.py Strict tools/routes/72_boss_tpchase.dev`, dev crutches, 137 s wall time): rc 0, `chase met` after 421 ticks, last status `stage=1 scen=5 shines=2`. The fresh clone found two defects, fixed in 7286e30 and 06668e5: `tools/locked.py` failed when `sunshine-pc/build` did not exist, and `tools/_strict.cmd` was git-ignored by the rule `tools/_*`.

1. Parse state: met. `probe-units` with clang-cl (`CLANG_CL`, `tools\probe-units.cmd`): 733 units, 623 parse clean, 110 fail, 0 of the failing units are outside `sunshine-pc/excluded-units.txt` (149 units listed with 27 rules, 40 of them parse anyway). The older documents say 624; today's count is 623. Both numbers come from `tools/probe-results-clang-cl.json` and disagree by one unit; re-run `probe-units` before quoting either one (open, see ROADMAP Phase 4).
2. No empty or stub source: strict link has 0 unresolved symbols (no missing function). Checked by text match against `sms/build/triage.json` (non-100 % functions only): 709 one-line empty bodies in the game directories, none of them has a score below 100 %, so retail has a trivial body there too. 113 `UNIMPLEMENTED` / "unused stub" lines in 21 `JSystem` files (`JKRAram*`, `JKRArchive*`, `JKRDvd*`, ...) sit in units that match retail 100 % (`report.json`). Functions whose names do not match textually are not covered by this check. Port-side gaps that are not missing symbols: `sunshine-pc/src/sdk/dsp_mixer.cpp` logs once and renders silence for 2-bit ADPCM (wave format 1), some oscillator waveforms, other voice source types, the distance low-pass filter, Dolby surround and per-channel mixer delay (justified as an audio-fidelity blocker, listed in `docs/platform-notes.md`).
3. Boot to title, file select, Delfino Plaza, Bianco Hills Ep.1, Shine sequence, saving: reached only with dev crutches (`cheat infinite_health`, `chase tp=1`, `shine 2`, `SUNSHINE_DEV_STEP_RETRACE`, `SUNSHINE_DEV_SKIP_MOVIES`, `SUNSHINE_DEV_AIM_ASSIST`); not met without them. Details and the open list: ROADMAP.md Phase 1 and the update `2026-10-12 (route 72)`.
4. Matching build: met. DOL SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` in the fresh clone; 11 317 / 12 881 functions byte-identical, 445 / 736 units.
5. Phases 4 to 8 of `Port.md`: not fulfilled. README.md lists every item with its state; DLSS is blocked (`docs/dlss-feasibility.md`); the UI target (Dusklight level) has a plan (`docs/ui-plan.md`), packages UI-1 to UI-3 started.
