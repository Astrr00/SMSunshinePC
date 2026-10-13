# UI plan: Sunshine launcher and in-game menu toward Dusklight polish

Companion to `ui-gap-dusklight.md`. Row IDs (A1 to H8) refer to that matrix. This document is a plan only. No code, asset, or INI change is made by it. Dusklight facts cite its repo (`TwilitRealm/dusklight`, `main`) or site. Sunshine facts cite this repo. `[INFERENCE]` marks an unverified assumption that the package must confirm first.

## Project rules (apply to every package)

- Nothing under `sms/` except inside `#if TARGET_PC`.
- No game data in the repo: no disc images, extracted assets, or hashes unless the owner approves them.
- Tests write saves only to temp directories.
- Game runs go through the run lock (`tools/runlock.py`, used by `tools/route.py`).
- Build output goes under `sunshine-pc/build/`. `sunshine-pc/.gitignore:1` ignores `build/`.
- Each shipped font or sound needs a license entry in `sunshine-pc/THIRD_PARTY_LICENSES.md`. Dusklight's font licenses were not verified, so its fonts are not copied.

## Verified API facts used here

- Aurora input events (`sunshine-pc/extern/aurora/include/aurora/input.hpp:109-117`): `InputEvent::ButtonChanged { SDL_GamepadButton button; bool pressed; }` and `InputEvent::AxisChanged { SDL_GamepadAxis axis; float value; }`. Axis range: sticks [-1, 1], triggers [0, 1].
- Launcher input layer: `kRmlUiLayerPriority = 100` (`input.hpp:185`), registered at `sunshine-pc/src/launcher/launcher.cpp:218-224` with `capturesAll`.
- Launcher `onInput` (`launcher.cpp:295-305`) handles only `InputEvent::KeyChanged`. Gamepad events are ignored.
- UI scale: `aurora::rmlui::set_ui_scale(float)` and `get_ui_scale()` (`sunshine-pc/extern/aurora/include/aurora/rmlui.hpp:32-33`). 0 means window scale. Values are clamped to [0.25, 4.0] (`extern/aurora/lib/rmlui.cpp:453`). Sunshine's `src/` does not call either function.
- `aurora::rmlui::InputType` has only `Text` and `Number` (`rmlui.hpp:19-22`). Aurora gives no keyboard-versus-gamepad hint, so the glyph source must be tracked by Sunshine itself (UI-7).
- Sunshine's `src/` and `extern/aurora/lib` contain no HTTP or socket client code. The matches for `http|WinHTTP|libcurl|curl_|socket` are URL comments only.

## Dependencies

| Package | Depends on |
|---|---|
| UI-1 | none |
| UI-2 | UI-1 |
| UI-3 | UI-1 |
| UI-4 | UI-1, UI-3 |
| UI-5 | UI-2 |
| UI-6 | UI-1 |
| UI-7 | UI-3, UI-4 |
| UI-8 | UI-2 |
| UI-9 | UI-1, UI-3 |
| UI-10 | UI-1, UI-3 |
| UI-11 | UI-3 |
| UI-12 | UI-5 or UI-8 (settings row); owner decision on HTTP client |
| UI-13 | UI-1 (launcher part); UI-4 (in-game part) |
| UI-14 | UI-1, UI-4 |
| UI-15 | UI-4, UI-6 |
| UI-16 | UI-1, UI-8 |

Suggested order: UI-1; then UI-2 and UI-3 in parallel; then UI-4, UI-5, UI-6, UI-8, UI-9, UI-10, UI-11; then UI-7, UI-12, UI-13, UI-14, UI-15, UI-16.

**Status (2026-10-12).** UI-1: implemented (`assets/launcher/theme.rcss`, `:focus` and `:active` states); screenshots `build/UiDev/ui1-*.png` at 1280×960 and 640×480. UI-2: implemented (`ui_scale`, `remember_window_size`, aspect row); `settings_test` covers the keys. UI-3: implemented (D-pad, left stick, A, B, Start; `handlePad` in `launcher.cpp`); focus path verified from the pad log (`build/UiDev/ui4/probe.log`). Not verified: the in-launcher aspect round trip and the `remember_window_size` relaunch were not run on the current build (UI-2 is therefore partly verified: parser/writer by `settings_test`, scale by screenshots at 75/100/150 %); no physical gamepad was tested (synthetic events through `onInput` only); the disc step is excluded. The same pad log shows A on Back, B, Start and the left stick reaching the intended controls.

**Status (2026-10-13, UiPages).** UI-9 (Cheats pane): implemented; `cheat_toggle` run writes `[cheats] infinite_health=1` to the INI and `cheat_relaunch` INI keeps it (relaunch apply not confirmed: that run's log is empty). UI-13 (Quit confirm): implemented; `quit_cancel` returns focus to `quit`, `quit_ok` runs A on `confirm-yes` and ends with `Device lost`; Escape/B and the pad restriction to the two dialog buttons are verified by code read only (`launcher.cpp:351, 532, 418-420`). UI-5: not done; no on-screen FPS overlay exists in `src/framerate` (grep), so the FPS row is not added; widescreen label not screenshot-verified; "Open data folder" not added. UI-10: not started (time box exceeded; loader disable change and `[mods]` INI section still open).

## Common acceptance standard

1. **Screenshots** at 1280×960 (default `windowWidth`/`windowHeight`, `src/settings.h`; applied at `src/main.cpp:101-102`) and at 640×480 (smallest preset, `src/launcher/launcher.cpp:83-86`). Store them under `sunshine-pc/build/`. No baseline exists in the repo: `docs/platform-notes.md:461` describes launcher screenshots, but none are present. The first run of UI-1 creates the baseline for owner review.
2. **Controller walkthrough log.** Each button press, the focused element id, and the resulting pane are recorded. Until UI-11, the disc choice uses the SDL file dialog (`launcher.cpp:395-396`). The log marks that step as a keyboard and mouse exception.
3. **INI round trip.** For each new key, change it in the UI, quit, relaunch, and compare the displayed value and the file. The parser and writer are covered without Aurora by `tools/settings_test.cpp` (`CMakeLists.txt:85-86`). Real path: `%APPDATA%\Sunshine\settings.ini`, with the exe directory as fallback (`docs/platform-notes.md:456`).
4. **Existing checks.** `tools/port_tests.cmd` is claimed to run 38 checks with 0 failed (`docs/platform-notes.md:461`). Run it after the packages land. This study did not run it.

## Packages

Each package lists goal, files, depends, closes (gap rows), size per the rubric in `ui-gap-dusklight.md`, and acceptance.

### Foundation

**UI-1 Theme tokens and fonts** (M)
- Goal: replace the hard-coded colors in `launcher.rcss` with a theme sheet using token names modeled on Dusklight's `res/rml/theme.rcss` (custom properties on `body`, for example `--color-accent`, `--font-size-md`, `--radius-control`). Sunshine chooses the values. Add `:focus` and `:active` states to buttons. Keep `LatoLatin`; the `font-family: Lato` pitfall (`docs/platform-notes.md:458`) stays documented.
- Files: `assets/launcher/launcher.rcss`, `assets/launcher/launcher.rml:4` (theme link), new `assets/launcher/theme.rcss`, `src/launcher/launcher.cpp:207-208` (font load, unchanged).
- Closes: H5, H6 (partial).
- Acceptance: the launcher renders with the theme sheet at both screenshot sizes. Every button shows a focus ring. Layout is unchanged except color. `[INFERENCE]` custom-property `var()` works with the pinned RmlUi commit (`sunshine-pc/extern/aurora/extern/CMakeLists.txt`, `f00a0fee38391b2f927114e11bea18dc0a7dba1e`). Confirm on the first build.
- Font decision for owner: keep Lato, or adopt another font with a recorded license. Dusklight's `res/` license was not read; its fonts are not used by this plan.

**UI-2 Scaling and window options** (M)
- Goal: add a UI scale setting that calls `aurora::rmlui::set_ui_scale`, default 0 (window scale). It stays separate from the render resolution scale (`launcher.rml:36`). Add a "remember window size" toggle (C2) and an aspect row (C3) for the existing `aspect` key (`src/settings.h:24`, `src/settings.cpp:85-87`).
- Files: `src/settings.h`, `src/settings.cpp` (new keys; section chosen by owner), `src/launcher/launcher.cpp` (click handlers near 365-376), `assets/launcher/launcher.rml:32-41`, `src/main.cpp:101-102` (initial size when "remember" is off).
- Closes: C2, C3, C5 (partial).
- Acceptance: UI scale 0.75, 1.0, and 1.5 change launcher text size and leave game render size unchanged. The 640×480 layout has no clipped buttons. INI round trip passes. `settings_test` covers the new keys.

**UI-3 Launcher controller navigation** (M)
- Goal: a gamepad drives the launcher. D-pad and left stick move focus between buttons in the active pane. A activates. B goes back one level. Start starts the game. Implement in `onInput` (`launcher.cpp:295-305`): handle `ButtonChanged` (pressed edges only) and `AxisChanged` (stick with a neutral band, reference `kAxisNeutral = 0.25`, `input.hpp:188`). Button identifiers are `SDL_GamepadButton` values, not `PAD_*` names. Keep `capturesAll` (`launcher.cpp:171`, `222`). Keyboard rebinding stays unchanged until UI-7.
- Files: `src/launcher/launcher.cpp`, `assets/launcher/launcher.rcss` (`:focus`), `assets/launcher/launcher.rml` (focusable markup if needed).
- Closes: H4.
- Acceptance: the walkthrough log shows boot → Settings → Video → change window mode → Back → Start, pad only. The disc step is excluded and marked. Focus never becomes empty while the launcher is open. Keyboard still works for rebinding.
- `[INFERENCE]` RmlUi focus calls (`Element::Focus`, `Element::Blur`, `tabindex`) exist in the pinned RmlUi. Confirm while implementing.

### Pages

**UI-4 In-game menu shell** (L)
- Goal: open the menu in game with R+Start (the chord Dusklight uses, `src/dusk/ui/input.cpp:74-78`), F1 on keyboard (Dusklight: `ui.cpp:646`), and a bindable "Open menu" action (UI-7). Render through the same RmlUi path as the launcher. Tabs: Settings, Mods (UI-10), Cheats (UI-9), Reset and Quit (UI-13). The game pauses while the menu is open.
- Files: new module under `src/` (for example `src/ingame_menu/`), `src/main.cpp` (pump near 78-79), SMS pause flow under `sms/` inside `#if TARGET_PC`. The `sms/` pause flow has not been audited. Conflict risk: `[INFERENCE]`. Read it before editing.
- Closes: A1, A2 (partial), A5 (partial).
- Acceptance: menu opens and closes by pad chord and F1, and no input leaks to the game while it is open (walkthrough log). Pause state matches the game's pause behavior. The `#if TARGET_PC` guard keeps non-port builds unchanged.

**UI-5 Video pane** (S-M)
- Goal: add FPS overlay (C6; `src/framerate/framerate.cpp` not yet read) and make widescreen honest. Either implement the render path (owner decision) or relabel the toggle as not implemented (`docs/platform-notes.md:457` says persisted only). Add "Open data folder" (C10).
- Files: `assets/launcher/launcher.rml:32-41`, `src/launcher/launcher.cpp:465-478`, `src/settings.*`.
- Closes: C4 (decision), C6, C10.
- Acceptance: each new row round trips through the INI. The widescreen row either changes a real render output (screenshot diff) or shows "not implemented" in the UI.

**UI-6 Audio pane** (M)
- Goal: output device selection (D2) and music volume (D4). Menu sounds toggle (D5) follows UI-15. Surround (D3) is deferred: `sdk/dsp_mixer.cpp:315-316, 329, 342` are "not implemented" warnings. Keep the existing volume presets (`launcher.cpp:469-470`).
- Files: `assets/launcher/launcher.rml:43-45`, `src/launcher/launcher.cpp`, `src/settings.*`, Aurora audio device API (not yet located; `[INFERENCE]`).
- Closes: D2, D4.
- Acceptance: device change applies at runtime or the UI states that a restart is needed. Music volume maps to a real bus; if SMS has no separate BGM bus (`[INFERENCE]`), D4 is re-scoped to master volume.

**UI-7 Input mapping with glyphs** (L)
- Goal: split controls into pages matching Dusklight's `Page` enum (`src/dusk/ui/controller_config.hpp`: Controller, Buttons, Triggers, Sticks, Rumble, Actions). Gamepad capture replaces the keyboard-only capture (`launcher.cpp:296-304`), with a neutral-wait before the next input (Dusklight: `mSuppressNavigationUntilNeutral`). Use `ButtonChanged` and `AxisChanged`, and the capture pattern in `extern/aurora/lib/input/capture.cpp`. Add trigger dead zones (E3). Add rumble test (E4; Aurora rumble API `[INFERENCE]`). Add an Actions page with "Open menu" (E5). Complete glyphs (E6, `ROADMAP.md` row 66): Y, Z, L, R, stick, and D-pad prompts; a keyboard glyph style. Track the last-used device in Sunshine, because Aurora gives no hint (see facts).
- Before changing styles: reconcile the two `GlyphStyle` enums (`src/settings.h:12` and `src/glyphs/glyphs.h:8`). Glyph art: no `generic/` folder exists (`assets/glyphs/` has xbox, playstation, switch, gamecube3). Decide whether Generic reuses Xbox art or ships new art, and record its license.
- Files: `assets/launcher/launcher.rml:47-79`, `src/launcher/launcher.cpp:296-304, 475-478`, `src/glyphs/*`, `src/settings.*`.
- Closes: E1, E2, E3, E4, E5, E6.
- Acceptance: all 20 `bind-*` rows (`launcher.rml:53-72`) and both triggers are rebindable by pad. Escape cancels a capture. Every bound button has a glyph in all five styles. `settings_test` covers the keys.

**UI-8 Interface pane** (S-M)
- Goal: "Skip main menu" setting (B2). `--dvd <file>` already skips the launcher (`docs/platform-notes.md:455`). Advanced-settings gate (F4) tied to the R31 decision in `docs/status.md:118` (keep dev-only, or gate out of release). Menu scaling mode (C5) uses UI-2's scale. Dev overlays (H7) are not in scope.
- Files: `assets/launcher/launcher.rml:77-79`, `src/settings.*`, `src/launcher/launcher.cpp`.
- Closes: B2, F1, F4.
- Acceptance: skip-menu setting round trips and is honored on the next launch. Advanced rows are hidden in release builds if the owner chooses the gate.

**UI-9 Cheats pane** (M)
- Goal: replace the placeholder "Cheats are not wired into the launcher yet." (`launcher.rml:81-83`) with a toggle list from the cheats backend (`src/cheats/cheats.h`, `assets/cheats.json`, loaded at `src/main.cpp:138`). Toggle state persists to the INI.
- Files: `assets/launcher/launcher.rml:81-83`, `src/launcher/launcher.cpp`, `src/cheats/*`, `src/settings.*`.
- Closes: G1.
- Acceptance: each toggle round trips. Applied cheats are confirmed by log, the same method as `ROADMAP.md` row 67. Health and mod overlay are not claimed until verified.

**UI-10 Mods list** (M launcher; L in-game browser deferred)
- Goal: list installed mods from `src/mods/mods_loader.cpp` and `mod_manifest.cpp`, showing the manifest fields from `docs/mods.md:23-27` (`sunshine_api`, `name`, `version`, `author`, `description`). Enable and disable state persists to the INI. Loader support for disable is not verified (`[INFERENCE]`).
- Dusklight reference (not copied): its `mod.json` defines `id`, `name`, `version`, `author`, `description`, `icon`, `banner` (`docs/modding.md`). Dusklight's mod manager displays icon and banner. Sunshine's field set is its own.
- Files: `assets/launcher/launcher.rml`, `src/launcher/launcher.cpp`, `src/mods/*`.
- Closes: G2 (launcher list).
- Acceptance: the list matches the mods folder contents. A mod disabled in the UI is not loaded after restart. The online browser (G3) and mod updates are deferred to an owner decision.

**UI-11 Disc picker and hash verification** (M-L)
- Goal: replace the SDL file dialog (`launcher.cpp:395-396, 421-424`) with a launcher file browser that the pad can drive (B5). Add a recent-discs list in the INI. Add hash verification (B3): today only the GMSJ01 ID check runs (`launcher.cpp:502-506`, `docs/platform-notes.md:455`). Dusklight reference: `src/dusk/ui/prelaunch.cpp` `DiscVerificationTask`, and `DiscVerificationState { Unknown, Success, HashMismatch }` (`src/dusk/settings.h:39-43`).
- Hash table: stores hashes only, and only if the owner approves. No game data goes into the repo. Hash on a background thread so the UI stays responsive.
- Files: `src/launcher/launcher.cpp:395-396, 421-424, 502-506`, new browser code, `src/settings.*`.
- Closes: B3, B5.
- Acceptance: the browser navigates and selects with the pad. Hash comparison is tested with a generated temp file and its computed hash, never a disc. A mismatch shows a message distinct from the existing ID error. Frame time stays flat during the hash (log).

### Polish

**UI-12 Update checker** (L)
- Goal: opt-in setting (G4). Help text as Dusklight words it: "Checks GitHub releases for a new Dusklight version on startup. No personal information is transmitted or collected." (`src/dusk/ui/settings.cpp:1590`, observed). Sunshine's wording is its own. The endpoint is `[INFERENCE]`: GitHub releases for the Sunshine repo. HTTP client: none exists. Options are WinHTTP (Windows-only port) or a dependency. Owner decides. A new dependency for a few lines is not justified.
- Files: new module under `src/`, UI row in UI-5 or UI-8 pane, `src/settings.*`.
- Closes: G4.
- Acceptance: with the setting off, no network call is made (log or blocked-socket test). With it on, a stubbed release response in a test (not the live API) shows an "update available" toast. Offline start does not fail. The request carries no identifier headers.

**UI-13 Confirmations** (M)
- Goal: launcher Quit asks for confirmation (today it quits immediately, `launcher.cpp:425-427`). In-game Reset and Quit use a modal with title, body, Cancel, and a confirm action (Dusklight pattern: `menu_bar.cpp:96-137`, body "Unsaved progress will be lost."). Default focus on Cancel `[INFERENCE]`: convention.
- Files: `assets/launcher/launcher.rml`, `src/launcher/launcher.cpp`, in-game menu module from UI-4.
- Closes: A3.
- Acceptance: Cancel leaves state unchanged. Confirm performs the action. Keyboard and pad both reach both buttons.

**UI-14 Transitions** (S-M)
- Goal: page and modal transitions in the theme (UI-1). The in-game bar hides after `Transitionend` (Dusklight `menu_bar.cpp:60-62`).
- Files: theme sheet from UI-1, in-game menu module from UI-4.
- Closes: A5.
- Acceptance: duration is a theme token. No input is lost during a transition. Screenshot comparison is done with transitions finished.

**UI-15 Sounds** (M)
- Goal: open, close, confirm, and cancel sounds. Dusklight plays menu open and close through its game audio API (`src/dusk/ui/document.cpp:215-223`, `mDoAud_seStartMenu`, `kSoundMenuOpen`, `kSoundMenuClose`; window close at `menu_bar.cpp:96-137`). Sunshine has no sound code outside the Audio pane (negative check). Sound source: SMS's own effects through `sms/` under `#if TARGET_PC`, or a small asset set with a recorded license.
- Files: in-game menu module, `src/launcher/launcher.cpp`, `src/settings.*`, sound assets if used.
- Closes: A4, D5.
- Acceptance: the toggle round trips. With it off, no effect plays. Any shipped sound has a license entry in `THIRD_PARTY_LICENSES.md`.

**UI-16 Languages** (L)
- Goal: F2, game-language selection. `[INFERENCE]` GMSJ01 is Japanese only (`launcher.rml:9` "Japanese version"); the disc's language options need confirming. F3, launcher string localization: move inline strings in `launcher.rml` into a string table with keys.
- Files: `assets/launcher/launcher.rml`, new string table, `src/settings.*`.
- Closes: F2, F3.
- Acceptance: every visible string has a key. A missing key fails loudly in dev builds and falls back to English in release builds. Owner confirms whether language selection is in scope at all.

## Backlog (not scheduled)

Rows in `ui-gap-dusklight.md` not listed in a package above are backlog. They include Discord Rich Presence (G5, external SDK), speedrun timer integration (G6), the online mod browser (G3), surround audio (D3), drag-and-drop install (B4), and the touch-control and achievement rows (H8, G7), which do not apply to this port. Dev overlays (H7) are excluded by default.

## Owner decisions needed

1. Font: keep Lato, or adopt another font with a recorded license (UI-1).
2. Hash table: approve storing GMSJ01 hashes in the repo (UI-11).
3. Update checker: enable it, choose the HTTP client, confirm the endpoint (UI-12).
4. Dev and advanced settings: keep as dev-only or gate out of release (`docs/status.md:118`, R31; UI-8).
5. Widescreen: implement the render path, or relabel as not implemented (UI-5).
6. Language scope (UI-16).
7. Surround audio and Discord Rich Presence: schedule or leave in backlog.
8. `GlyphStyle` duplicate (`src/settings.h:12`, `src/glyphs/glyphs.h:8`): choose the canonical enum before UI-7.

## Verification (run once after packages land, not in this study)

- `tools/settings_test` (`CMakeLists.txt:85-86`).
- `tools/port_tests.cmd` (`docs/platform-notes.md:461`).
- Route or walkthrough runs under the run lock, with the screenshots and logs in the acceptance standard.
- Open item: `docs/platform-notes.md:461` claims launcher screenshots exist. None are in `sunshine-pc/docs/img/` (only `bianco0_ep1.png`), and `build/` does not hold them. Regenerate during UI-1.
