# Roadmap

Goal: a fully playable native Windows x86-64 port of Super Mario Sunshine (JP, GMSJ01), built from `sms/` on Aurora.
Acceptance is unassisted play. Dev aids (`tp`, `SUNSHINE_DEV_START`, `SKIP_MOVIES`, godmode) are for diagnosis only.
PAL/US and byte-identical PowerPC matching are tracked separately and do not gate this roadmap.
No game data belongs in this repository.

Source of truth: [playability matrix](sunshine-pc/docs/playability-matrix.tsv) (16/25 rows passed-bounded, 2026-10-17).
Evidence and history: [`status.md`](sunshine-pc/docs/status.md), [`roadmap-history.md`](sunshine-pc/docs/roadmap-history.md) (the old per-phase roadmap and dated updates), [`release-checklist.md`](sunshine-pc/docs/release-checklist.md).

## Where we are

- Done: boot, launcher, new game, airport, Delfino, Shadow Mario, Bianco + boss, save and cold reload, Ricco loading, input (keyboard + DualSense), 60 fps timing (Strict build now `/O2`), widescreen incl. `aspect=auto`, Alt+Enter fullscreen.
- Owner played the first two Shines: picture, sound and gamepad fine.
- A draft pre-release v0.1.0 and `dist/sunshine-0.1.0.zip` exist; nothing is published.
- `/O2` sweep (`tools/o2_sweep.py`, 2026-10-17): Ricco, Shadow, airport (needs 700 s) and the cold Petey route pass. Gatekeeper now passes twice consecutively (`gk14`, `gk15`, each rc0/429 s), using only ordinary controller inputs from the earned airport profile. Approach defense, waking and frame-based throat attacks are verified through the natural Delfino scenario transition.

## Open work, in order

1. **Later worlds** (PC-CONTENT, PC-CORE). The 44 absent resource slots are statically unreferenced by normal transitions, including the Delfino/Pinna selectors (`docs/content-unresolved.md`). Aurora 0021 fixes three renderer aborts; the PC archive lookup now rejects the invalid parent sentinel that crashed Pianta episode 2. Targeted WARP probes pass, including Gelato's complete NPC/camera dialogue. The full 91-scenario diagnostic sweep is running on their common build (`python tools/world_sweep.py <unique-tag>`, private `build/WorldSweep/wl06/summary.json`); finish it and investigate remaining failures. Normal mission/boss completion and collectibility remain open. Recommended model: GPT-6.1-Sol, high effort; scripted.
2. **Owner playtest** (PC-RUN). Play a new save to the final boss, credits and post-ending; send logs (`play_logged.cmd`, `collect_logs.cmd`). Scripted runs only for milestones: one entry and boss per world, Corona.
3. **Open evidence rows** (PC-GRAPHICS, PC-AUDIO, PC-STABILITY). Certified by the playtest and a 30-minute run with transitions, repeated save/load, death/restart and native quit.
4. **Release** (PC-PACKAGE, PC-CLEANPC, PC-PUBLISH). Owner decides the Microsoft redistributable and `dxil.dll` provenance; a clean Windows 10/11 machine runs the real zip; then publish v0.1.0 as a pre-release, only on an explicit owner yes.

## Resume

- Build: `python tools\locked.py cmd /c tools\_strict.cmd` (PowerShell, in `sunshine-pc`) must print `STRICT: 0 unresolved symbols`; then `tools\port_tests.cmd`.
- Aurora: `python tools/apply-aurora-patches.py --check` must report `applied` (21 patches, 49 files on upstream `50e5be3`).
- Routes: `python tools/route.py Strict <script> --name <n> --seconds <s> [--appdata-from <run>] KEY=VAL`; output in `build/Route/<n>/run.log`. Kept earned profiles: `pg1_airport_reload_20261016` (gatekeeper), `pg11_ramp_spray24k` (cold Petey), `o2s_b_pc_airport_unassisted`.
- Gatekeeper regression only: `python tools/o2_sweep.py <unique-tag> gatekeeper`. Keep route.py's CPU-adapter fallback enabled on GPU-less hosts: Aurora Null cannot wash the CPU pollution map. PASS requires exit 0, `done at tick` and an absent or empty crash.log. Local checks: `python tools/o2_sweep_test.py` and `python tools/devscript_guard_test.py` (the latter needs Strict and the local disc). No `/Od` comparison or UB conclusion is established.
- Later-world diagnostics: `python tools/world_sweep.py <unique-tag>` (all 91 present scenario rows) or append raw stages / `stage/scenario` selectors, e.g. `4/1 8/0 8/1`. Each owned WARP run has a 90-second wall limit; results and executable SHA256 are saved after each run in `build/WorldSweep/<tag>/summary.json`. Checks: `python tools/world_sweep_test.py`, `python tools/content_reach_test.py`; actual resource audit: `python tools/content_reach.py`.
- Disk: check free space on `C:` before long route batches (it hit 0 bytes on 2026-10-17). Delete old `build/Route/pg*`/`o2s_*` runs only with owner approval; keep the profiles above. Approved generated-build/cache cleanup recovered 15.44 GiB; about 18.3 GiB remained after the Gatekeeper confirmations.
- Release: `build/pg17-release/build.cmd`, then `python tools/package.py --build-dir build/windows-clang-release --llvm-bin <llvm>/bin` (see `release-checklist.md`).

## Rules

- Push each verified wave; rerun `python tools/status_treemap.py` when matrix statuses change; no status table in the README.
- Do not hide a hang with a guard or route bypass; find the cause.
- Preserve private work, original saves and unrelated changes; never publish game or private artifacts.
- New optional features must not delay the core port.
