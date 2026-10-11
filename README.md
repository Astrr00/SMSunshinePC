# Super Mario Sunshine for Windows (port in progress)

A native Windows x86-64 port of Super Mario Sunshine (GameCube, GMSJ01). The game code is the decompilation in `sms/`, compiled with clang-cl against [Aurora](https://github.com/encounter/aurora), which replaces the Dolphin SDK layer (GX, VI, PAD, OS, DVD, CARD). No emulator, no PowerPC code.

## Status

Not playable end to end yet. Legend: done = observed in a run or check, partial = works with gaps or unverified, open = not working. % is the lead's estimate of how much of the item works, not a measurement. Updated with every milestone.

| Area | State | % | Evidence / gap |
| --- | --- | ---: | --- |
| Matching decomp build (SHA-1 `9f5a8caf…`) | done | 100 | `ninja` in `sms/` |
| Decomp logic correctness (non-matching functions) | partial | 48 | 10 960/12 881 functions bytegleich; game code: 6 945/8 806 bytegleich, 954 whole-function asm reviews (`sunshine-pc/docs/decomp-verified.tsv`), 973 game functions still open; asm-proven logic fixes include inverted branches (`barWait`, Amenbo, bossgesso), wrong switch case sets (`TMario::receiveMessage`, `initUnique`), wrong members/constants (motor params, splash radius), misplaced braces (`TSelectMenu::initData`); scanners `--branchscan`/`--immscan` in `decomp_triage.py` |
| Strict link, 0 unresolved symbols | done | 100 | `tools/_strict.cmd` |
| Units compiled (clang-cl) | done | 100 | 583/583 built units, 150 excluded with reasons (`sunshine-pc/excluded-units.txt`, `docs/platform-notes.md`) |
| Boot: logo, intro movies | done | 100 | normal `--dvd` boot |
| Title screen | partial | 70 | logo, Shine Sprite, seagulls render, logic 30/s; area behind the logo black (retail background unverified) |
| Title → file select → new game → Delfino | partial | 35 | select and slot panel run clean (`sunshine-pc/build/FlowFileSelect/s3.png`; flow6 to tick 3146, no fatal); CardLoad crashes fixed (titleDraw loop 13→11, asm 0x8022D16C; selectFunction unk2A4 loop 4→3, asm 0x80226050/0x802273B0); play of slot A hides card UI (`s6.png`) but stage stays 15; slot A still reads NEW after File created; new game/Delfino pending |
| Bianco Hills Ep.1 (dev start) | partial | 60 | textured, HUD, Mario walks/jumps, camera follows; flat untextured polygon at the screen edge |
| First Shine + save + reload | open | 10 | tpshine finds the Shine via the shine manager (eventId 42, `build/ShineSave2-2/watch.log:73`); Mario falls to the ground, no collect, no `.gci` yet |
| Goop / pixel metric | open | 15 | GPU occlusion gives 0/1 only; needs real fragment counts |
| Audio | partial | 50 | movies and title music play; gameplay output turns into noise after ~20 s |
| Particles | partial | 50 | JPA loads; some particle view objects not in the draw lists |
| Unlocked framerate | partial | 30 | replay works on the title but halves logic speed; default is retail 30 fps; interpolation not started |
| Widescreen | partial | 60 | implemented, visuals not verified |
| Launcher, settings | partial | 60 | pages render; clicks/runtime effects unverified |
| Cheats, mods | partial | 60 | 38 headless checks pass; in-game effects unverified |
| Controller glyphs | partial | 60 | assets + hook, unverified in game |
| FSR1 upscaling | partial | 50 | implemented, unverified |
| DLSS | blocked | 0 | needs motion vectors and a D3D12 handoff Dawn does not expose (`docs/dlss-feasibility.md`) |
| Release package | partial | 40 | packaging script exists, not run on the current tree |

Overall (mean of the rows): **55 %**.

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
