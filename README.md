# Super Mario Sunshine for Windows (port in progress)

A native Windows x86-64 port of Super Mario Sunshine (GameCube, GMSJ01). The game code is the decompilation in `sms/`, compiled with clang-cl against [Aurora](https://github.com/encounter/aurora), which replaces the Dolphin SDK layer (GX, VI, PAD, OS, DVD, CARD). No emulator, no PowerPC code.

## Status

Not playable end to end yet. Legend: done = observed in a run or check, partial = works with gaps or unverified, open = not working. % is the lead's estimate of how much of the item works, not a measurement. Updated with every milestone. Last measured: 2026-10-11.

| Area | State | % | Evidence / gap |
| --- | --- | ---: | --- |
| Matching decomp build (SHA-1 `9f5a8caf…`) | done | 100 | `ninja` in `sms/` |
| Decomp logic correctness (non-matching functions) | partial | 55 | 11 013/12 881 functions bytegleich (58.57 % code, 97.98 % fuzzy), 445/736 units Matching; ledger `sunshine-pc/docs/decomp-verified.tsv`: 1 366 `logic-verified` + 64 `matched` rows, 541 `partial` (2026-10-11; the 170 new rows came from agents and were only sampled by diff, not audited); the open list still holds several hundred game functions without a finished equivalence review (most blocked by header-inline codegen). Own code is allowed where byte-identical is not reachable if behaviour equals retail. Asm-proven bug fixes include inverted branches, wrong case sets, wrong members/offsets/constants/indices (e.g. CardLoad scale 1.5f, camera sub-stick axes, Kumokun velocity member). 30/30 sampled rows confirmed by independent audit, 4 wrong hunks found and fixed |
| Strict link, 0 unresolved symbols | done | 100 | `tools/_strict.cmd` |
| Units compiled (clang-cl) | done | 100 | 583/583 built units, 150 excluded with reasons (`sunshine-pc/excluded-units.txt`, `docs/platform-notes.md`) |
| Boot: logo, intro movies | done | 100 | normal `--dvd` boot |
| Title screen | partial | 70 | logo, Shine Sprite, seagulls render, logic 30/s; area behind the logo black (retail background unverified) |
| Title → file select → new game → Delfino | partial | 50 | condition-based route `sunshine-pc/tools/routes/07_airport.dev` (via `tools/route.py`, no start switches) goes logo → option map (stage 15, file blocks) → card prompts → bump file block → new game → opening movie → **airport0 (stage 0, scen 0), reached in every complete run of this session (6/6)**; the walk to `MapObjStartDemo` (-6630,500,1600) did not arrive yet and Delfino/Bianco are not reached; a GX FIFO crash at ~204 s was traced to two `GXBegin` counts in the decomp (JPA cross particles) and fixed, post-fix run not yet complete (`docs/session-2026-10-11.md`) |
| Bianco Hills Ep.1 (dev start) | partial | 60 | textured, HUD, Mario walks/jumps, camera follows; `bianco/0` and `dolpic/0` ran stable for 100 s each (normal and heap-check mode, `build/StageCrash/`); an intermittent flat red/blue wedge appears in about 1 of 6 runs at frame 600 (also with widescreen off and FSR off), cause open (`build/ArtifactHunt/`) |
| First Shine + save + reload | open | 10 | tpshine finds the Shine via the shine manager (eventId 42, `build/ShineSave2-2/watch.log:73`); Mario falls to the ground, no collect, no `.gci` yet |
| Goop / pixel metric | open | 15 | GPU occlusion gives 0/1 only; needs real fragment counts |
| Audio | partial | 50 | movies and title music play; gameplay output turns into noise after ~20 s |
| Particles | partial | 50 | JPA loads; some particle view objects not in the draw lists |
| Unlocked framerate | partial | 30 | logic 30/s, replay/interpolation code present; 60/120/144 not verified (host load, "presents 0/s" counter) |
| Widescreen | partial | 55 | Hor+ frame widths 1706x960 (16:9) and 2242x960 (21:9) observed, HUD at the edges in the 21:9 frame; 4:3-identical, world scale and camera-cut reset NOT verified |
| Launcher, settings | partial | 65 | FSR1/scale/cheat rows persist to the INI, launcher screenshot rendered; DLSS row wrap fix not visually verified |
| Cheats, mods | partial | 60 | cheats verified by log (lives/coins held at 99); water/health cheat and mod overlay effect not verified |
| Controller glyphs | partial | 65 | Xbox/PlayStation/Switch/GameCube styles load and change the water-button glyph (PNGs); PlayStation X asset is a square; keyboard style missing |
| FSR1 upscaling | partial | 65 | FSR1 pass observed in log at 429x322 → 1279x960 (`build/PortLauncherRun/fsr67_on.log`); quality vs native not compared |
| DLSS | blocked | 0 | needs motion vectors and a D3D12 handoff Dawn does not expose (`docs/dlss-feasibility.md`) |
| Release package | partial | 40 | packaging script exists, not run on the current tree |

Overall (mean of the rows): **56 %**.

Plan and order of work: [`ROADMAP.md`](ROADMAP.md).

## Legal

- No game data is in this repository. You need your own legally obtained copy of the GMSJ01 disc image. The game reads it at runtime (`--dvd <path>`).
- The port's own licence is not decided yet. Third-party licences are listed in `sunshine-pc/THIRD_PARTY_LICENSES.md` (to be pinned before release).

## Layout

- `sms/`: decompilation (nested repository, submodule after publishing). Changes to it stay behind `#if TARGET_PC`; the matching build must keep the reference SHA-1.
- `sunshine-pc/`: the port (CMake, `src/`, `include/`, tools, docs).
- `sunshine-pc/extern/aurora`: Aurora at upstream `50e5be3`, with the port's changes as patches in `sunshine-pc/patches/aurora/`.

## Build

Requirements:
- Windows 10/11 x86-64.
- Visual Studio 2022 (BuildTools or Community) with the MSVC x64 tools. `build.cmd` loads `vcvars64.bat` from there.
- clang-cl. Put it on `PATH`, or set `LLVM_BIN` to the folder that contains `clang-cl.exe`.
- CMake, Ninja, and Python 3 (`Python3` is required by `CMakeLists.txt`).

Steps:

    git clone --recurse-submodules https://github.com/Astrr00/SMSunshinePC.git
    cd SMSunshinePC
    python sunshine-pc/tools/apply-aurora-patches.py
    sunshine-pc\build.cmd

`apply-aurora-patches.py` applies the port's changes to the Aurora submodule. It is idempotent. The configure step warns if the series is not applied.

`build.cmd [target]` configures and builds the preset `windows-clang-relwithdebinfo`. The binary is written to `sunshine-pc/build/windows-clang-relwithdebinfo/` (from `binaryDir` in `sunshine-pc/CMakePresets.json`).

Run with your own disc image:

    sunshine-pc\build\windows-clang-relwithdebinfo\sunshine.exe --dvd "path\to\Super Mario Sunshine (Japan).rvz"

## Matching decompilation

`sms/` builds the original `mario.dol` with its own Python/Ninja system. Its SHA-1 must stay `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` (GMSJ01). Run `ninja` inside `sms/` for that check; the port's build does not touch it.

## More

- [`sunshine-pc/docs/status.md`](sunshine-pc/docs/status.md): status and blockers
- [`sunshine-pc/docs/platform-notes.md`](sunshine-pc/docs/platform-notes.md): technical notes for the port
- [`sunshine-pc/patches/aurora/README.md`](sunshine-pc/patches/aurora/README.md): Aurora patch series
- [`sunshine-pc/docs/publishing.md`](sunshine-pc/docs/publishing.md): publishing runbook
- [`Port.md`](Port.md), [`Goal-Port.md`](Goal-Port.md): work orders
