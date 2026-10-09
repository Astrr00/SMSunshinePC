# Work order: Native Windows port of Super Mario Sunshine in the style of Dusklight

## 1. Goal

Build a native Windows 10/11 x86-64 port of Super Mario Sunshine modeled on Dusklight (TwilitRealm/dusklight, the Twilight Princess PC port).
The game must be playable from start to finish, with unlocked framerate and true widescreen.

Dusklight is **not** a static recompilation and not an emulator, but a reimplementation based on the decompilation:
The decompiled C/C++ game code is compiled natively for the PC, and the Dolphin SDK layer (GX, VI, PAD, OS, DVD, AI/DSP, CARD) is replaced by a host implementation.
For this, Dusklight uses [Aurora](https://github.com/encounter/aurora) (GX → WebGPU/Dawn with D3D12/Vulkan/Metal) and reads the game data at runtime directly from the user's disc image (ISO/GCM, RVZ, WIA, CISO, GCZ).
Exactly this architecture is required here. No PowerPC recompilation, no interpreter, no embedded emulator runtime.

Before starting, read Dusklight's README, `docs/building.md`, `docs/code-conventions.md`, `CMakeLists.txt`, `CMakePresets.json` and the Aurora integration.
Adopt their proven solutions instead of inventing parallel concepts. Check the license of every adopted component and document project, commit and license.

## 2. Starting point in the workspace

- Decompilation: `sms/` (fork `Astr00/sms`, check the branch in Git), target version `GMSJ01` (NTSC-J), reference SHA-1 of the DOL in `sms/config/GMSJ01/build.sha1`.
- Further configuration: `sms/config/GMSP01/`. Symbols and splits: `sms/config/<Version>/symbols.txt`, `splits.txt`.
- Disc image: `sms/orig/GMSJ01/Super Mario Sunshine (Japan).rvz`, extracted files under `sms/orig/GMSJ01/`.
- Progress according to `sms/PROGRESS.md` (re-measure before starting work, do not take over): approx. 41% of code bytes matched, approx. 19% linked, approx. 8,585 / 12,881 functions matched.
- The ongoing decompilation work order is in `Goal.md`. This port is a separate work order and must not break the matching build.

**Central blocker:** A Dusklight-style port needs the entire game code as compilable C/C++ source.
Twilight Princess was largely decompiled when Dusklight started; Super Mario Sunshine is not.
For the port, functionally equivalent code (`NonMatching`/`Equivalent`) is sufficient; byte-exact matching is not a prerequisite.
Assembly fallbacks and original objects, on the other hand, are unusable for the port because they are PowerPC code.

## 3. Repository structure and conventions

- Create the port as its own project next to `sms/` (e.g. `sunshine-pc/`), with the decompilation as a Git submodule or fork branch – like Dusklight relative to `zeldaret/tp`. Decide based on the existing Git structure and document the choice.
- Build system of the port: CMake + Ninja with presets (`windows-msvc-*`, `windows-clang-*`), as in Dusklight. The Python/Ninja matching build system of the decompilation stays unchanged.
- **Always** delimit changes to the original code with `#if TARGET_PC`, leave the original code in place. UB fixes with `#if AVOID_UB`.
- Resolve global `operator new`/`delete` overloads into the heap hierarchy (JKRHeap) via macros (`JKR_NEW`, `JKR_DELETE` or similar), as in Dusklight.
- After every change to decompilation sources: the matching build (`ninja` in `sms/`) must still pass the reference check. This is part of the port's CI.
- Commit fixes that also benefit the decompilation separately in `sms/`.
- No copyrighted assets or binary data in the repository. Game data comes exclusively from the user's image.

## 4. Core technical problems

Work through these items with findings from the code, not with assumptions:

1. **Code completeness:** Determine from `report.json`/`objdiff` all units that still use assembly or original objects. Prioritize by reachability in the game flow (boot → title → Delfino Plaza → first episode). Implement functionally equivalent C/C++ code; validate against Dolphin (see §8).
2. **64-bit and endianness:** GameCube is 32-bit big-endian. Check every place where file data is interpreted directly as a structure or where offsets are relocated in place into pointers (J3D models/BMD/BDL, BCK/BTK/BRK animations, J2D layouts/BLO, JUTResFont/BFN, BMG texts, JPA particles, JDrama scene graph from `scene.bin`, PRM/parameter files, collision/COL, paths/RAIL, JAudio banks, THP). Adopt how Dusklight/Aurora solved this (byte swapping on load, separate host structures). No `reinterpret_cast` hacks with 4-byte pointer fields.
3. **SDK replacement:** GX/VI/PAD/OS via Aurora; DVD via Aurora/nod directly from the image; ARAM as host memory; CARD as a memory card file in the user profile with GCI import/export (compatible with Dolphin saves); OSThread/interrupts/alarms matched to host execution.
4. **Audio:** SMS uses JAudio with DSP microcode. Check how Dusklight solves JAudio output and whether it is transferable to SMS's older JAudio version. Music (AW/sequences), sound effects, streams (ADP/AST) and THP video audio must work.
5. **EFB copies and effects:** Capture effect at the Shine, screen water, heat haze, shadows, reflections, goop/graffiti (pollution texture), blur in menus. These must scale correctly at every resolution and aspect ratio.
6. **Undefined behavior:** Correct uninitialized variables, aliasing, overflows and pointer comparisons that happen to work on the GameCube under `AVOID_UB`, without changing game behavior.

## 5. Gameplay extensions

### Unlocked framerate
- Determine the actual logic rate (VI retrace, `JDrama` display/director loop, time base). Game logic, physics, collision and timers keep running at the original rate.
- Rendering runs decoupled (unlimited or capped at 30/60/120/144/165/240 Hz) with interpolation between two logic frames: camera, object matrices, skeletal animation, particles, HUD animations, water surface.
- No global speed factor on timers. Stable identifiers per object for interpolation; teleports and scene changes without interpolation artifacts (reset on jumps).
- Byproduct: motion vectors from previous and current matrix per draw – prerequisite for DLSS.

### Widescreen
- Aspect ratios: 4:3, 16:9, 16:10, 21:9, 32:9 and automatic by window.
- Correct the projection, adapt frustum culling and clipping of the game camera to the wider field of view (no pop-in at the edges).
- Anchor the HUD (Shine, coin, water gauge, life gauge, text boxes, menus) to screen edges instead of stretching.
- Real-time cutscenes in widescreen; THP videos in the original aspect ratio with pillarbox.

## 6. Launcher and settings

A single executable. A launcher appears at startup (Dear ImGui via Aurora, no separate framework), then the game starts in the same process. All settings are additionally reachable in-game via an overlay menu.

- **Game data:** Select image (ISO/GCM, RVZ, WIA, CISO, GCZ), detect region and revision by hash and allow only supported versions; clear error message for an unknown or modified image. Remember the last path. Command line `--dvd <path>` as in Dusklight.
- **Graphics:** Resolution, window mode (windowed, borderless fullscreen, exclusive fullscreen), monitor, VSync, FPS limit, internal resolution scaling, MSAA, anisotropic filtering, aspect ratio, HUD scaling, graphics backend (D3D12/Vulkan).
- **DLSS:** Super Resolution and DLAA via NVIDIA Streamline. Prerequisites: depth, motion vectors (§5), camera jitter, access to native D3D12/Vulkan resources under Dawn. Check feasibility under Aurora first; if resource access is not possible, document the concrete blocker instead of a dummy. Show the option as disabled on unsupported hardware.
- **Controls:** Keyboard and controller (SDL3 via Aurora) freely bindable, multiple profiles. Preserve analog triggers: R half pressed = spraying while running, fully pressed = standing and aiming. For keyboard, a half-press key. Analog stick deadzones and sensitivity, camera inversion.
- **Button prompts:** GameCube, Xbox, PlayStation, Nintendo Switch, keyboard. Automatic by most recently used device, manually overridable. Applies to menus and game texts: Replace button graphics in textures/layouts and button codes in BMG texts to match the profile.
- **Audio:** Master, music, effects volume, output device.
- **Mods:** Mod folder in the user profile, list with enable/disable and load order. Asset mods replace files of the disc file system by path (also files inside SZS/RARC archives). Load texture packs in Dolphin format (`tex1_<w>x<h>_<hash>_<fmt>`). Show conflicts in the launcher.
- **Cheats:** Built-in, toggleable cheats as native code (e.g. infinite water, infinite lives, moon jump, all nozzles, level/episode select, Yoshi everywhere, Shine counter). Gecko/Action Replay codes do not work in a source port (different memory addresses, no PowerPC code); do not fake it, explain in the launcher.
- **Storage location:** Settings, saves, mods and logs under `%APPDATA%\<project name>`; settings as a readable file (TOML or JSON, prefer Dusklight's format).

## 7. Phases and acceptance criteria

Each phase ends with a built, launched and verified state. No phase counts as done through code alone.

1. **Inventory:** Measure the decompilation state, prioritize missing units by game flow, analyze Dusklight/Aurora, document architecture decisions.
2. **Foundation:** CMake project, Aurora integrated, image is read, boot to the first rendered frame (logo/title) with real decompiled code.
3. **Code completeness:** All units needed for the port as C/C++; game flow title → file select → Delfino Plaza → Bianco Hills Episode 1 including Shine sequence and saving.
4. **Full playability:** All worlds, episodes, secret levels without F.L.U.D.D., bosses, Yoshi, all nozzles, Corona Mountain, credits, music, videos, saving/loading.
5. **Widescreen and unlocked framerate** per §5, verified at 30, 60, 120, 144, 240 Hz and unlimited.
6. **Launcher, settings, controls, button prompts, mods, cheats** per §6.
7. **DLSS** per §6.
8. **Release build:** Installation-free package (exe + runtime libraries), instructions "provide your own image", no protected data.

## 8. Validation

- The reference is Dolphin with the same image. Compare scenarios side by side: movement and jump distances, F.L.U.D.D. physics, water and goop, collisions, enemy behavior, camera, timers (e.g. red coins, races), audio synchronization.
- Check logic rate: results must not differ between 30 Hz and 240 Hz rendering (same input sequence → same game state).
- Check widescreen for culling gaps, HUD position, EFB effects at 1080p, 1440p, 4K, 21:9 and 32:9.
- Saves: create new, load, import from and export to Dolphin; use test copies.
- Explicitly mark tests not performed as not performed. A screenshot is not proof of correct game logic.

## 9. Prohibitions

- No stubs, empty success paths or `return 0` placeholders that hide missing game code. Missing functionality stays visibly marked as missing.
- No embedding of PowerPC code, DOL parts or game assets in the executable.
- No changes to reference hashes or diff configurations of the decompilation.
- No silent gameplay changes. Bugfixes and convenience features only as a switchable option, default = original behavior.
- No `git reset --hard`, no blanket `git clean`, no force pushes, no push without approval.

## 10. Reporting

Report tersely in English. After every work block: changed, checks run, measured progress (missing units, playable area), blockers, next concrete step.
Separate observations from assumptions. Do not invent test results, measurements or commit IDs.

## 11. Start now

Measure the current decompilation state, analyze Dusklight and Aurora, create the port project and bring the boot to the first rendered frame.
