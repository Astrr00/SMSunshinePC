# Super Mario Sunshine for Windows (port in progress)

A native Windows x86-64 port of Super Mario Sunshine (GameCube, GMSJ01). The game code is the decompilation in `sms/`, compiled with clang-cl against [Aurora](https://github.com/encounter/aurora), which replaces the Dolphin SDK layer (GX, VI, PAD, OS, DVD, CARD). No emulator, no PowerPC code.

**Not playable yet.** See [`sunshine-pc/docs/status.md`](sunshine-pc/docs/status.md) for the requirement table and open blockers (for example the title screen rendering and the stage-load crash).

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
