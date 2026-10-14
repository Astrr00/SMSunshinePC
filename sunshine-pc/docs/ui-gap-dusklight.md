# UI gap study: Dusklight vs. Sunshine

Goal: Sunshine's UI (launcher and, later, in-game menu) reaches the polish level of Dusklight, the Twilight Princess PC port by TwilitRealm (CC0). This document lists what Dusklight ships, what Sunshine has today, and how big each gap is. Work packages are in `ui-plan.md`.

## Sources and evidence

- Dusklight: `https://github.com/TwilitRealm/dusklight`, branch `main`, tree sha `34a144a493b7f57adeb4034d382317c826eb5c8d`. Paths below are repo-relative. Site: `https://twilitrealm.dev/`. License: CC0 1.0 (`LICENSE.md`).
- Sunshine: paths are repo-relative to `sunshine-pc/` unless marked otherwise.
- Tags: a claim without a tag is observed in the cited file or URL. `[INFERENCE]` marks an unobserved conclusion. "Not read" means the file exists in the listing but its contents were not read.

## Gap rubric

- **S** (small): text, row, or config change in existing `assets/launcher/launcher.rml`, `launcher.rcss`, or `src/settings.*`, reusing existing code paths.
- **M** (medium): a new pane, module, or widget that reuses existing Aurora/RmlUi and INI paths.
- **L** (large): a new subsystem. Examples: in-game menu shell, gamepad rebinding with a glyph pipeline, mod manager UI, network update checker, localization, external SDK integration (Discord).
- **State** is relative to today's Sunshine code. *Present*: works end to end. *Partial*: some of it exists. *Missing*: no code path found. The grep scope used for "missing" is listed in the notes.
- The rating is the effort in Sunshine's code. It is not a measure of Dusklight's effort.

## Notes: differences between Dusklight and SMS

- **Pad:** Dusklight's menu chord uses the GameCube pad names `PAD_TRIGGER_R` and `PAD_BUTTON_START` (`src/dusk/ui/input.cpp:74`, `78`). SMS uses the same pad names (`sunshine-pc/src/launcher/launcher.cpp:55-60`, `65-71`). The chord therefore needs no mapping.
- **Version:** Sunshine accepts only GMSJ01 (NTSC-J) (`sunshine-pc/docs/platform-notes.md:455`). Dusklight v2.0 reports "expanded version support" (`https://twilitrealm.dev/posts/2026-09-18-dusklight-v2-0-released/`).
- **Z-targeting:** SMS has none. Dusklight features that depend on it are not applicable. None were audited.
- **Platforms:** Dusklight also ships Android, iOS/iPadOS, and macOS (`https://twilitrealm.dev/install/`). Touch overlays (`res/rml/touch_controls*.rcss`) are out of scope for a Windows-only port.

## How Dusklight builds it

1. **Document UI on RmlUi.** Player-facing UI is built from RmlUi documents: `Rml::String`, `Rml::Element`, `listen(Rml::EventId::Keydown, ...)` in `src/dusk/ui/controller_config.cpp`. Dear ImGui is in `src/dusk/imgui/`; `ImGuiConfig.hpp` defines `ImGuiCheckbox`, `ImGuiSliderFloat`, `ImGuiSliderInt`, `ImGuiMenuItem`. Each helper writes a `ConfigVar<T>` and calls `save()`. Whether ImGui is dev-only is `[INFERENCE]`.
2. **One theme file with tokens.** `res/rml/theme.rcss` defines font families (Fira Sans, Fira Sans Condensed, Noto Mono, Material Symbols Rounded, Alegreya SC), a `dp`-based type scale (11dp to 32dp), spacing (2dp to 32dp), radii, colors (text, accent, border, surface, interactive, success, info, warning, error), and button states (`--button-background-hover`, `-selected`, `-active`). `res/rml/` holds 18 `.rcss` sheets (contents listing; `window`, `popup`, `popover`, `tabbing`, `prelaunch`, `mods`, `mod_browser`, `overlay`, `tuner`, `touch_controls*`, `command_console`, `logs`, `saves`, `controls`, `global`, `mod_common`, `theme`).
3. **Font loading.** `src/dusk/ui/ui.cpp:39-40` (`load_font`) calls `Rml::LoadFontFace(resource_path(...), fallback)`. `ui.cpp:106-112` loads FiraSans Regular (as fallback), FiraSans Bold, FiraSansCondensed Regular and Bold, AlegreyaSC Regular and Bold, MaterialSymbolsRounded, NotoMono. Inter Regular and Bold appear in the `res/` listing but are not in that load list (`[INFERENCE]`: unused, or loaded elsewhere).
4. **Menu open and nav commands.** `src/dusk/ui/document.cpp:215-223` (`Document::handle_nav_command`): `NavCommand::Menu` plays the open/close sound (`mDoAud_seStartMenu`, `kSoundMenuOpen`/`kSoundMenuClose`) and calls `toggle()`. Keyboard F1 maps to the menu at `src/dusk/ui/ui.cpp:646`.
5. **Gamepad mapping.** `src/dusk/ui/input.cpp`: R+Start chord (`is_menu_chord_part` 74, `has_menu_chord_part_held` 78, `is_menu_chord` 144, `consume_menu_chord` 723, `update_menu_chord_release` 746, `should_defer_menu_chord_part` 754). A bound menu button maps to F1 (215-218, 231-234, 339-341). Back/Select maps to F1 when the menu is not bound to that port (230-232, 343-348). South maps to Return (212-213). Navigation is driven by `nav_group.cpp`, `nav_types.cpp`, and `tabbing.rcss` (files exist; contents not read).
6. **Menu shell.** `src/dusk/ui/menu_bar.cpp`: the bar hides after `Rml::EventId::Transitionend` (60-62). Tabs (`build_tabs`, 74): Settings (75); Warp and Editor (77-78), gated by `enableAdvancedSettings` (76, dev-only); Achievements (86), gated to vanilla or speedrun mode (81-83); Mods (87-88) via `mods::svc::ui_mod_menu_tabs()`. Reset (96) and Quit (133) are both Modal confirmations ("Unsaved progress will be lost."; Cancel plus a second action).
7. **Settings pages.** `src/dusk/ui/settings.cpp` (about 1704 lines, grep-level read): Video (678: fullscreen 686-693 via `VISetWindowFullscreen`, VSync 699, Lock 4:3 705-707, FPS overlay 726-765, remember window size 772, `uiScale` 788, max frame rate 872). Audio (1098: output mode 1098-1126 via `audio::Reinitialize`, master volume 1132-1146, music 1157, reverb 1178, menu sounds 1184). Gameplay (1203), Quality of Life (1267), Speedrunning (1308, LiveSplit 1327-1331), Cheats (1350), Interface (1463), Tools (1675, "Link" 1679). Keys: Free Camera and sensitivity (1014-1021), Gyro Aim (1035), Mouse/Touch in Menus (1080), Turbo Key (1090). Rows also cover Skip Dusklight Main Menu (1577), Check for Dusklight Updates (1590), Discord Rich Presence (1597), Advanced Settings (1610), Input Viewer (1620), Menu Scaling Mode (1632), Recording Mode (1670-1672), Open Data Folder (540-541), Language (580), Mirror Mode (221-222).
8. **Config persistence.** `src/dusk/config.hpp`: `ConfigImplBase` with `loadFromJson`, `loadFromArg` (launch args), and `Register(ConfigVar&, onChange)` for runtime callbacks. Settings are JSON (`[INFERENCE]` on file layout; the JSON path is observed).
9. **Controller config.** `src/dusk/ui/controller_config.hpp` declares `enum class Page { Controller, Buttons, Triggers, Sticks, Rumble, Actions }`. `controller_config.cpp` has pending-binding capture (`poll_pending_binding`, `finish_pending_binding`, `cancel_pending_binding`, `pending_input_neutral`), `mSuppressNavigationUntilNeutral`, device name via `current_controller_name(port)`, `config::ClearAllActionBindings(port)`, and `PADSerializeMappings()`. Rumble test: `mRumbleTestActive`, `stop_rumble_test`. Action list includes "Open Dusklight Menu" (`src/dusk/action_bindings.cpp:25`).
10. **Glyphs.** `src/dusk/ui/icon_provider.cpp` (899 lines): `switch(control)` at 811, 832, 850.
11. **Prelaunch and disc check.** `src/dusk/ui/prelaunch.cpp` (1204 lines): `DiscVerificationTask`, message "Disc verification failed...", `try_apply_mirrored_layout` (933, called 1025), state fields such as `configuredDiscValidation` and `activeDiscInfo`. `src/dusk/settings.h:39-43`: `DiscVerificationState { Unknown, Success, HashMismatch }`. `src/dusk/iso_validate.hpp/.cpp` exist (not read). Prelaunch uses `res/prelaunch-bg.png` and `res/rml/prelaunch.rcss` (contents not read).
12. **Language.** `src/dusk/language.hpp`: `available_languages(const iso::DiscInfo&)`, `language_name(GameLanguage)`, `msg_folder()`. `src/dusk/settings.h:30-37`: `GameLanguage` from `OS_LANGUAGE_*`. Whether UI strings are localized is not verified.
13. **Mod UI API.** `docs/modding.md:243`: `dusklight.ui` provides mod panels, controls, lists, windows, dialogs, styles, menu tabs, toasts, and clipboard access. Lines 1011-1015: `register_styles(scope, rcss, &handle)` and `register_styles_file`. Scopes: `UI_SCOPE_PRELAUNCH`, `UI_SCOPE_WINDOW`, `UI_SCOPE_MENU_BAR`, `UI_SCOPE_OVERLAY`, `UI_SCOPE_TOUCH_CONTROLS`, `UI_SCOPE_GRAPHICS_TUNER`. Mod UI sources (names only; contents not read): `src/dusk/ui/mods_window.cpp`, `mod_browser.cpp`, `online_mods.cpp`, `mod_updates.cpp`, `mod_texture_provider.cpp`, `remote_texture_provider.cpp`.
14. **Update checker.** Help text at `src/dusk/ui/settings.cpp:1590`: "Checks GitHub releases for a new Dusklight version on startup. No personal information is transmitted or collected." The endpoint code was not located (`[INFERENCE]`: GitHub releases API).
15. **Platform base.** `README.md` (raw): D3D12, Vulkan 1.1+, or Metal required; D3D11 and OpenGL ES best-effort. Credits Aurora. Sunshine also uses Aurora (`sunshine-pc/CMakeLists.txt:25-26`).
16. **Shipped features (v2.0 post, `https://twilitrealm.dev/posts/2026-09-18-dusklight-v2-0-released/`).** Mod system; official randomizer; expanded version support; "menus carefully reworked with variable delta time"; 5.1 and 7.1 surround. The post also mentions an in-game mod browser with in-game updates and a bundled randomizer manager. Re-check the post before quoting those two.

## What Sunshine builds on

1. **Aurora with RmlUi.** `sunshine-pc/CMakeLists.txt:25-26`: `AURORA_ENABLE_RMLUI ON`, `AURORA_ENABLE_THP OFF`. `sunshine-pc/extern/aurora/CMakeLists.txt:14` declares the option. `sunshine-pc/extern/aurora/extern/CMakeLists.txt` pins RmlUi at commit `f00a0fee38391b2f927114e11bea18dc0a7dba1e` (grep hit at about 332-335) and Dear ImGui `v1.91.9b-docking` (about 225-233). Sunshine's own `src/` has no ImGui (0 hits for `imgui|dear`).
2. **Launcher.** `src/launcher/launcher.cpp` (568 lines) and `launcher.hpp`. `open()` (199-226): `aurora::rmlui::is_initialized()` and `get_context()`; `Rml::LoadFontFace("assets/launcher/LatoLatin-Regular.ttf")` and `-Bold.ttf` (207-208); `LoadDocument("assets/launcher/launcher.rml")` (211); input layer registration (218-224) with `capturesAll` (171) so the launcher captures every input source.
3. **Input in the launcher.** `onInput` (295-305): only `KeyChanged` is handled. Key capture for rebinding is keyboard-only. Escape cancels (296-304). No gamepad focus or navigation.
4. **Markup.** `assets/launcher/launcher.rml` (89 lines): main page 7-19 (Choose image, Start, Settings, Quit); settings tabs 22-30 (Video, Audio, Controls, Interface, Cheats); Video 32-41 (window mode, window size, VSync, resolution scale, FSR 1, DLSS span at 38 "unavailable (docs/dlss-feasibility.md)", FPS limit, widescreen); Audio 43-45 (master volume); Controls 47-75 (stick and C-stick dead zones 49-50, 20 `bind-*` buttons 53-72, reset 74); Interface 77-79 (glyph style); Cheats 81-83 (placeholder: "Cheats are not wired into the launcher yet.").
5. **Stylesheet.** `assets/launcher/launcher.rcss` (84 lines): colors hard-coded (background `#1b1d2a`, accent `#d8a21c`, text `#f2efe6`); `font-family: LatoLatin` (line 2); `button:hover` only (58-60); no `:focus` or `:active`; a `button.tab.active` rule (72-75). No shared theme file.
6. **Fonts.** `assets/launcher/LatoLatin-Regular.ttf`, `LatoLatin-Bold.ttf`, license `LICENSE-Lato.txt`; `sunshine-pc/THIRD_PARTY_LICENSES.md:18` lists Lato under SIL OFL. Aurora registers the faces as `LatoLatin`; `font-family: Lato` renders no text (`sunshine-pc/docs/platform-notes.md:458`). No icon font.
7. **Settings.** `src/settings.h`: `windowWidth = 1280`, `windowHeight = 960`; `aspect` string (line 24) accepts `4:3, 16:10, 21:9, 32:9 or auto` (`src/settings.cpp:85-87`); `GlyphStyle { Generic=0, Xbox, PlayStation, Switch, GameCube }` (line 12). `src/glyphs/glyphs.h:8` declares a second `GlyphStyle` (uint8_t); reconcile before use. INI: `glyph_style=` in `[interface]` (`platform-notes.md:461`). Round-trip test `tools/settings_test.cpp` (built per `CMakeLists.txt:85-86`), reported ok for Switch and GameCube glyph names (`platform-notes.md:461`).
8. **Runtime apply.** `launcher::applyRuntimeSettings()` (120-137) pushes PAD key and axis bindings (`PADSetKeyButtonBindings` 123-124, `PADSetKeyAxisBindings` 125), dead zones (`applyDeadZones` 128-137; trigger zones stay at Aurora defaults per the comment there). Called from `main.cpp:142`. Settings file loaded before `aurora_initialize`: window mode, size, VSync (`platform-notes.md:456`).
9. **Disc open.** `chooseImage()` (395-396) uses `SDL_ShowOpenFileDialog` with filter "Disc images" (native dialog, not controller-navigable). `openDisc` (502-506) calls `aurora_dvd_open`; error "Could not open this file as a disc image." GMSJ01 check: `platform-notes.md:455`.
10. **Quit.** `launcher.cpp:425-427`: sets `finished_ = true` with no confirmation.
11. **Runtime cheats and mods.** `main.cpp:138` `cheatsLoad(exeDir)`; `main.cpp:139` `modsLoad(exeDir)`. Backend: `src/cheats/cheats.h`, `cheats_core.cpp/.h`, `cheats_game.cpp`, `assets/cheats.json`. Mods: `src/mods/mods_loader.cpp`, `mod_manifest.cpp`; `docs/mods.md`. No mod UI.
12. **Glyph assets.** `assets/glyphs/{xbox,playstation,switch,gamecube3}/*.png` (source tree; the same folders appear in `build/Strict/assets/glyphs/`). No `generic/` folder.
13. **Dev hooks.** `SUNSHINE_DEV_LAUNCHER_PAGE=settings[/video|audio|controls|interface|cheats]` (`launcher.cpp:228-229`; `platform-notes.md:460`). `SUNSHINE_WINDOW_SIZE=w,h` (`platform-notes.md:460`). `docs/status.md:118` lists these as R31 with "need decision: keep as dev-only or gate out of release".
14. **Known Sunshine-only items.** FSR 1 toggle (`launcher.rml:37`, `launcher.cpp:467-468`); resolution scale presets 0/50/67/77/100/150/200/300 (render scale, not UI scale). Widescreen is persisted but not implemented (`platform-notes.md:457`).

## Matrix

Columns: Dusklight feature (where, how) | Sunshine today (file, state) | Gap | Notes.

### A. In-game menu shell and entry

| # | Dusklight feature (where, how) | Sunshine today (file, state) | Gap | Notes |
|---|---|---|---|---|
| A1 | Menu opener: R+Start chord, F1, bound menu button (`input.cpp:74-78, 144, 215-218, 230-232`; `ui.cpp:646`; `document.cpp:215-223`) | Missing. `main.cpp:78-79` only pumps events while paused or minimized | L | Chord uses the same PAD names as SMS. SMS's own pause/menu flow in `sms/` was not audited (`[INFERENCE]`: conflict risk). Needs a `#if TARGET_PC` hook. |
| A2 | Tab shell: Settings, Mods, Cheats (`menu_bar.cpp:74-88`) | Missing | L | Warp/Editor are dev-only (76-78). Achievements (81-86) have no SMS backend. |
| A3 | Reset and Quit confirmation Modal (`menu_bar.cpp:96-137`) | Missing in game. Launcher Quit is immediate (`launcher.cpp:425-427`) | M | Needs a modal widget in RmlUi. Dusklight `modal.cpp` exists (not read). |
| A4 | Menu open/close sounds (`document.cpp:215-223`; `menu_bar.cpp:96-137` `kSoundWindowClose`) | Partial. Launcher: four synthetic tones (UI-15 launcher half, runs `ui15fix2_*`, not heard). In-game menu silent (waits for UI-4b) | M | Synthetic tones, no asset (lead decision); SMS's own effects not used. No game data may ship in the repo. |
| A5 | Menu transitions: bar hides after `Transitionend` (`menu_bar.cpp:60-62`); v2.0 "variable delta time" menus | Partial: launcher pages, panes and the confirm page fade in (UI-14A, `theme.rcss`, `setDisplay`); in-game pane switches fade, opening the menu does not; no in-game close after `Transitionend` (UI-14 part B) | M | Launcher-only transitions are S once the theme (UI-1) exists. |

### B. Launcher and disc flow

| # | Dusklight feature | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| B1 | Prelaunch screen: background image, logo, styled layout (`prelaunch.cpp`, `res/prelaunch-bg.png`, `res/rml/prelaunch.rcss`) | Partial. Layout and buttons present (`launcher.rml:7-19`); no background or logo | M | Background is game-adjacent art; check license before adding. |
| B2 | Skip main menu setting (`settings.cpp:1577`) | Done (UI-8). `[launcher] skip_launcher` row on the Interface pane starts `last_image`; Shift at start or a bad disc shows the launcher. `--dvd <file>` still skips it | S | Unlike Dusklight the key is not cleared on a bad disc. |
| B3 | Disc verification with hash (`prelaunch.cpp` `DiscVerificationTask`; `settings.h:39-43`) | Missing. Only the GMSJ01 ID check (`platform-notes.md:455`). Grep `md5\|sha1\|sha256\|crc32\|hash\|checksum` in `src/`: 0 hits | M | Needs a known-hash table (no game data in the repo) and a background task for multi-GB reads (`[INFERENCE]`). |
| B4 | Drag-and-drop install (`drop_install_modal.*`, `package_row.*`; not read) | Missing. Grep `drag\|dropfile\|DROP_FILE`: 0 hits | M | `[INFERENCE]` needs SDL/Aurora drop events (not verified). |
| B5 | Disc picker, controller-operable | Partial. Native `SDL_ShowOpenFileDialog` (`launcher.cpp:395-396`, `421-424`) cannot be driven by gamepad | M | Sunshine-specific gap. Needs an in-launcher file list or a recent-discs list. |

### C. Settings: Video and display

| # | Dusklight feature (`ui/settings.cpp`) | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| C1 | Fullscreen, VSync (686-699) | Present. Window mode and VSync rows (`launcher.rml:33-35`); VSync applies at next start (`platform-notes.md:456`) | — | |
| C2 | Remember window size (772) | Partial. Four size presets 640×480, 960×720, 1280×960, 1600×1200 (`launcher.cpp:83-86`); size persisted to INI (`platform-notes.md:456`); no toggle | S | Dusklight has a toggle; Sunshine always remembers. |
| C3 | Lock 4:3 aspect (705-707) | Partial. `aspect` key with 4:3, 16:10, 21:9, 32:9, auto (`settings.h:24`; `settings.cpp:85-87`); no launcher row | S | |
| C4 | Unchained graphics / widescreen (Dusklight v2.0 post) | Partial. Widescreen toggle (`launcher.rml:40`, `launcher.cpp:465-466`) is persisted but not implemented (`platform-notes.md:457`) | M | Needs render-path work (`[INFERENCE]`). |
| C5 | Menu Scaling Mode (1632) and `uiScale` (788) | Missing. Resolution scale (`launcher.rml:36`) is render scale, not UI scale. Launcher CSS uses px (`launcher.rcss:3, 18`) | M | Dusklight theme uses `dp`. Sunshine RmlUi dp ratio not set (`[INFERENCE]`). |
| C6 | FPS overlay (726-765), max frame rate (872) | Partial. FPS limit row (`launcher.rml:39`; `settings().fpsLimit`, `platform-notes.md:456`). Overlay not verified. `src/framerate/framerate.cpp` exists (not read) | S | |
| C7 | Shader compilation overlay (about 1585) | Missing | M | `[INFERENCE]` needs Aurora shader-compile hook (not verified). |
| C8 | Input viewer (1620) | Missing | M | Needs pad state readout. `[INFERENCE]` |
| C9 | Recording mode: hides HUD and music (1670-1672) | Missing | M | Game-side HUD toggle under `#if TARGET_PC` (`[INFERENCE]`). |
| C10 | Open Data Folder (540-541) | Missing | S | `[INFERENCE]` shell open of the save folder. |

### D. Settings: Audio

| # | Dusklight feature (`ui/settings.cpp`) | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| D1 | Master volume (1132-1146) | Present. Row `launcher.rml:44`; presets 100/75/50/25/0 (`launcher.cpp:469-470`) | — | |
| D2 | Output mode, reinit on change (1098-1126) | Missing | M | `[INFERENCE]` needs Aurora audio device API (not verified). |
| D3 | Surround 5.1/7.1 (v2.0 post) | Missing. `sdk/dsp_mixer.cpp:315-316, 329, 342` are "not implemented" warnings | M | Mixer path, not UI, is the main cost. |
| D4 | Music volume (1157) | Missing. Only master volume exists | M | Depends on whether SMS has a separate BGM bus (`[INFERENCE]`). |
| D5 | Menu sounds toggle (1184) | Done for the launcher (`[audio] menu_sounds`, Audio pane's "Menu sounds" button; UI-15 launcher half) | S | Depends on A4 (launcher part done; in-game part waits for UI-4b). |

### E. Controls, glyphs, and rebinding

| # | Dusklight feature (`ui/controller_config.*`, `icon_provider.cpp`) | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| E1 | Controller pages: Controller, Buttons, Triggers, Sticks, Rumble, Actions (`enum class Page`) | Partial. 20 keyboard binds (`launcher.rml:53-72`) and stick dead-zone rows (49-50) | M (capture) / L (all pages) | Sunshine's pages are flat, not tabbed. |
| E2 | Pending capture: gamepad or key, neutral-wait before next input (`mPendingActionBinding`, `mSuppressNavigationUntilNeutral`) | Partial. Keyboard-only capture; Escape cancels (`launcher.cpp:296-304`) | M | Gamepad capture needs Aurora pad events in the launcher's input layer (`launcher.cpp:218-224`). |
| E3 | Dead zones | Present for stick and C-stick (`launcher.cpp:471-474`; `applyDeadZones` 128-137). Trigger zones not exposed (`launcher.cpp` comment in `applyDeadZones`) | S | Adds one row. |
| E4 | Rumble test (`Page::Rumble`, `mRumbleTestActive`) | Missing (grep `rumble`: none) | M | `[INFERENCE]` needs Aurora rumble API (not verified). |
| E5 | Actions page (`action_bindings.cpp`; "Open Dusklight Menu" at line 25) | Missing. Only the PAD names in `kKeyNames` (`launcher.cpp:76-79`) | M | Depends on A1 for the menu action. |
| E6 | Glyph style and per-control icons (`icon_provider.cpp:811, 832, 850`) | Partial. Five styles (`settings.h:12`), cycle button (`launcher.cpp:475-476`), mapping (543-554; Generic and Xbox fall back to Xbox). `ROADMAP.md` row 66 "PART": Y/Z/L/R/stick/dpad prompts missing; keyboard style missing | M | Two `GlyphStyle` enums exist (`settings.h:12`, `glyphs/glyphs.h:8`). |
| E7 | Reset to defaults (`ClearAllActionBindings`) | Present. `launcher.rml:74`; `launcher.cpp:477-478` | — | |
| E8 | Mouse/touch in menus, turbo key, gyro aim, free camera (`settings.cpp:1014-1035, 1080, 1090`) | Missing | M to L | Gameplay input; game-side in `sms/` (`[INFERENCE]`). |

### F. Interface, language, and game options

| # | Dusklight feature (`ui/settings.cpp`) | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| F1 | Interface tab (1463) | Done (UI-8). Button glyphs, UI scale (UI-2), Skip launcher | S | Menu scaling mode (C5, game menus) needs `sms/` and stays with C4. |
| F2 | Game language choice from disc (`language.hpp`; `settings.cpp:580`) | Missing | L | `[INFERENCE]` GMSJ01 is Japanese-only; `launcher.rml:9` says "Japanese version". Availability per disc is Dusklight's `available_languages`. |
| F3 | Launcher UI localization (`language.hpp` `msg_folder()`) | Missing. All strings are inline in `launcher.rml` | L | Dusklight UI localization not verified. |
| F4 | Advanced settings gate (1610) | Closed (UI-8): not needed until a dev row exists; dev hooks stay env-only | S | Only needed if dev rows are exposed. Related to R31 in `docs/status.md:118`. |

### G. Cheats, mods, updates, integrations

| # | Dusklight feature | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| G1 | Cheats tab (`settings.cpp:1350`) | Partial. Placeholder (`launcher.rml:81-83`). Backend `src/cheats/` and `assets/cheats.json`, loaded at `main.cpp:138` | M | `ROADMAP.md` row 67: cheats verified by log; overlay not verified. |
| G2 | Mods tab and browser (`menu_bar.cpp:87-88`; `modding.md:243`) | Partial. Runtime loader only (`main.cpp:139`; `src/mods/`). `docs/mods.md` | M (launcher list) / L (in-game browser) | |
| G3 | Online mod browser, mod updates (`online_mods.cpp`, `mod_updates.cpp`; v2.0 post) | Missing | L | Source names only; contents not read. |
| G4 | Update checker: GitHub releases, opt-in text (`settings.cpp:1590`) | Missing. Grep `update` in `src/`: only `aurora_update()` and `updatePriority` | L | Network code plus opt-in UI. Mechanism is `[INFERENCE]`. |
| G5 | Discord Rich Presence (`settings.cpp:1597`) | Missing | L | External SDK integration. |
| G6 | Speedrunning and LiveSplit (1308, 1327-1331) | Missing | M | `[INFERENCE]` LiveSplit needs its own socket protocol. |
| G7 | Achievements (`menu_bar.cpp:81-86`) | Missing | N/A | `[INFERENCE]` no SMS achievement backend found. |
| G8 | Randomizer manager (v2.0 post) | Missing | N/A | TP-specific. |

### H. Widgets, feedback, and navigation

| # | Dusklight feature | Sunshine today | Gap | Notes |
|---|---|---|---|---|
| H1 | Toasts (`modding.md:243`; `res/rml/popup.rcss`) | Missing. Single `.status` line (`launcher.rcss:35-39`) | M | |
| H2 | Popovers, context menus, dropdowns, number and bool buttons (`popover.rcss`, `context_menu.cpp`, `dropdown_button.cpp`, `number_button.cpp`, `bool_button.cpp`; contents not read) | Missing. Cycle-on-click buttons (`launcher.cpp:465-478`) | M | Dropdown is S-M once H4 exists. |
| H3 | Windows, panes, tabs (`window.rcss`, `tabbing.rcss`) | Partial. Tab buttons (`launcher.rml:25-29`); `.tab` styles (`launcher.rcss:68-75`) | S (tabs) / M (panes) | |
| H4 | Keyboard and gamepad focus navigation (`nav_group.cpp`, `nav_types.cpp`, `input.cpp:212-234, 343-348`) | Missing. Launcher captures all input (`launcher.cpp:221-224`); `onInput` handles keys only (295-305). No `:focus` (`launcher.rcss`) | M (launcher) / L (in game) | Biggest controller blocker. Covered by UI-3. |
| H5 | Theme tokens (`res/rml/theme.rcss`) | Missing. Colors hard-coded in `launcher.rcss` | S (launcher) / M (shared) | Covered by UI-1. |
| H6 | Font stack and icon font (`ui.cpp:106-112`) | Partial. Lato only (`launcher.cpp:207-208`); family-name pitfall (`platform-notes.md:458`); no icon font | S | |
| H7 | Dev overlays: logs window, command console, graphics tuner, editor (`logs_window.cpp`, `command_console`, `tuner.rcss`, `editor.cpp`) | Missing player-facing. `src/devscript/` exists (not read) | M (only if wanted) | Dev tools; not in player scope by default. |
| H8 | Touch controls (`res/rml/touch_controls*.rcss`) | N/A | N/A | Windows-only port. |

## Open items and unverified claims

- **Beobachtet, but unlocated:** the path of a Dusklight RML document that links `res/rml/theme.rcss`, `mod_common.rcss`, `tabbing.rcss`, and `popup.rcss` in its `<head>`. The lines were seen in an earlier session; the file path was not recorded. Re-read before citing.
- **Vermutet (not verified):** the update-check endpoint and mechanism (`ui/settings.cpp:1590` shows only the help text); how Dusklight calls `Rml::LoadFontFace` for the Inter files; the RmlUi and ImGui pins in Dusklight's build; the implementation behind the Modal widget; whether ImGui is dev-only; "Mirror Mode" meaning (`settings.cpp:221-222`).
- **Sunshine claim without a repo artifact:** `platform-notes.md:461` says launcher pages were "rendered (screenshots)". The repo has no launcher screenshot: `sunshine-pc/docs/img/` holds only `bianco0_ep1.png` (a game HUD), and no `build/*Launcher*` directory exists. No baseline exists for UI comparison.
- **Grep scope:** "Missing" entries for Aurora internals (gamepad rumble, shader-compile hooks, audio output devices, drop events) were not confirmed against Aurora sources. Those are marked `[INFERENCE]`.
- **Missing docs:** `sunshine-pc/README.md` and `sunshine-pc/ROADMAP.md` do not exist; the root `README.md` and `ROADMAP.md` are the references.
