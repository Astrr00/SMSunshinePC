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
- `/O2` sweep (`tools/o2_sweep.py`, 2026-10-17): Ricco, Shadow, airport (needs 700 s) and the cold Petey route pass. The gatekeeper route fails, see step 1.

## Open work, in order

1. **Gatekeeper route on `/O2`** (PC-GATEKEEPER, PC-CORE). `tools/routes/pc_gatekeeper_unassisted.dev` stops at line 41 (`hold L trig=0,255 until bck=0`, the sleeping boss never wakes) in 3 of 4 runs; camera yaw and Mario's spot differ per run, so the spray misses. The throat steps now wait on `bckframe>=60` (bf30624) instead of 16 ticks, untested past line 41. Make the line-41 aim robust, then rerun `python tools/o2_sweep.py <tag> gatekeeper` twice. No `/Od` baseline exists to compare. Opus 5.5 high, ~1-2 h.
2. **Later worlds** (PC-CONTENT, PC-CORE). Load each world through dev start, find crashes and unreachable content; 44 unresolved resource references remain (`docs/content-unresolved.md`). Opus 5.5 high, scripted.
3. **Owner playtest** (PC-RUN). Play a new save to the final boss, credits and post-ending; send logs (`play_logged.cmd`, `collect_logs.cmd`). Scripted runs only for milestones: one entry and boss per world, Corona.
4. **Open evidence rows** (PC-GRAPHICS, PC-AUDIO, PC-STABILITY). Certified by the playtest and a 30-minute run with transitions, repeated save/load, death/restart and native quit.
5. **Release** (PC-PACKAGE, PC-CLEANPC, PC-PUBLISH). Owner decides the Microsoft redistributable and `dxil.dll` provenance; a clean Windows 10/11 machine runs the real zip; then publish v0.1.0 as a pre-release, only on an explicit owner yes.

## Resume

- Build: `python tools\locked.py cmd /c tools\_strict.cmd` (PowerShell, in `sunshine-pc`) must print `STRICT: 0 unresolved symbols`; then `tools\port_tests.cmd`.
- Aurora: `python tools/apply-aurora-patches.py --check` must report `applied` (20 patches on upstream `50e5be3`).
- Routes: `python tools/route.py Strict <script> --name <n> --seconds <s> [--appdata-from <run>] KEY=VAL`; output in `build/Route/<n>/run.log`. Kept earned profiles: `pg1_airport_reload_20261016` (gatekeeper), `pg11_ramp_spray24k` (cold Petey), `o2s_b_pc_airport_unassisted`.
- Disk: route runs fill `C:` (it hit 0 bytes on 2026-10-17); delete old `build/Route/pg*`/`o2s_*` runs, keep the profiles above.
- Release: `build/pg17-release/build.cmd`, then `python tools/package.py --build-dir build/windows-clang-release --llvm-bin <llvm>/bin` (see `release-checklist.md`).

## Rules

- Push each verified wave; rerun `python tools/status_treemap.py` when matrix statuses change; no status table in the README.
- Do not hide a hang with a guard or route bypass; find the cause.
- Preserve private work, original saves and unrelated changes; never publish game or private artifacts.
- New optional features must not delay the core port.
