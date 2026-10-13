# Mods

A mod is a folder `mods/<id>/` next to `sunshine.exe`. It can contain a DLL (code), data overlays, or both.
The C ABI is in `include/sunshine_mod_api.h` (version 1.0). The loader is `src/mods/`.

## Layout

```
mods/<id>/
  mod.json   manifest (required)
  mod.dll    code (optional; a data-only mod has none)
  files/     overlays: files/<path> replaces the DVD file "/" + <path>
```

Discovery: `modsLoad` scans `mods/*/mod.json`. A folder without `mod.json` is skipped with a warning.
The log is `sunshine_mods.log` next to the exe, and the same lines go to stdout.

## Manifest (`mod.json`)

| Field | Required | Rules |
|-------|----------|-------|
| `id` | yes | Non-empty, `[A-Za-z0-9_-]` only. Identifies the mod in logs and in overlay precedence. |
| `sunshine_api` | yes | `"major.minor"`, e.g. `"1.0"`. |
| `name` | no | Display name. Defaults to `id`. |
| `version` | no | String. |
| `author` | no | String. |
| `description` | no | String. |
| `load_order` | no | Integer. Default `0`. Lower loads first; ties are broken by `id`. |

Compatibility: the host major must equal the mod's major. The mod's minor must not be newer than the host's.
A mod that fails the check is disabled and logged.

## Enable and disable

- `[mods]` in `settings.ini`, also set by the launcher's Mods pane: `<id>=0` disables a mod, `<id>=1` or no key enables it.
- The loader does not load a disabled id and logs `[mods] <id> disabled by settings`.

## Overlays

- Each file under `files/` (recursively) becomes an overlay for the DVD path `"/" + relative path`, with `\` turned into `/`.
- Overlays are merged into Aurora's DVD file system (`aurora_dvd_overlay_files`, `extern/aurora/lib/dolphin/dvd/fst.cpp`).
  The game sees them through the normal `DVDOpen`, `DVDFastOpen`, and path lookup calls. The disc is not modified.
- **Case-insensitive.** `/Data/Message.szs` and `/data/message.szs` are the same entry. Paths are lowercased for lookup.
- **Size.** An overlay's size is the size of its host file. Directory listings include overlays.
  Files over 4 GiB, bad paths, and paths that clash with a directory are rejected. Each rejection is logged as a WARN.
- **`.szs`.** Overlays are plain bytes. `.szs` is Yaz0-compressed RARC, and the game decodes it itself. An overlay must be a complete, valid `.szs`.
  Same-length patches keep the RARC offsets valid. Changing the length means rewriting the RARC headers, which the example does not do.
- **Precedence.** Across mods, sort by `load_order`, then `id`. The last mod wins for each case-insensitive DVD path.
  Each replacement is logged as `[mods] overlay replaced ...`.

## Unload

- Data-only mods are never disabled.
- A DLL mod that faults in a callback is disabled. Its overlays are removed, and the remaining live mods are re-merged
  (`applyOverlays(false)` in `disableMod`). Its files then vanish from the DVD view. Files that are already open keep reading the host file.
- Ceiling: each disable rebuilds the whole FST. This is fine for a rare fault, not for a per-frame path.
- Ceiling: the overlay name and source deques only grow. They are not freed on each rebuild, because open handles still point into them.

## Safety

- A crash (SEH) inside a mod callback disables that mod and is logged. The game keeps running.
  C++ exceptions are not caught; a mod must not let them escape its exports.
- All callbacks run on the game thread. Mario handles are valid only inside the callback that obtained them.
- Overlays are read-only in the DVD layer. The mod cannot write to the disc.

## Host ABI 1.0 (summary)

Exports the mod must have: `sunshine_mod_api_version` (required) and `sunshine_mod_init` (required, `0` = ok).
Optional: `sunshine_mod_shutdown`, `sunshine_mod_on_logic_tick` (30 Hz), `sunshine_mod_on_stage_load`, `sunshine_mod_on_present`.

Host table `SunshineHostApi`: `log`, `flag_get`/`flag_set`, `cheat_get`/`cheat_set`, `mario`, `mario_health`/`mario_set_health`, `mario_water`/`mario_set_water`.
There is no DVD API for mods. Overlays are the data path.

## Example: `overlay-demo`

`mods-example/overlay-demo/` is a data-only mod. It replaces the station name in `data/message.szs`.
The archive is not committed. It is built from your own disc extract:

```
python tools/mod_pack.py                  # reads sms/orig/GMSJ01/extracted/files, writes mods-example/overlay-demo/files/data/message.szs
python tools/mod_pack.py --src <dir> --out <mod dir>
```

`*.szs` is git-ignored. The patch is same-length Shift-JIS (`ドルピックタウン駅` → `スーパーマリオ駅前`). The output
is Yaz0 literal-only: valid, but larger than the input.

To try it, copy the folder to `<exe dir>/mods/overlay-demo/`. Remove the folder to return to the stock file.
CMake does not copy data-only mods; the copy is manual.

Example DLL mods: `hello` logs the logic tick count every 30 ticks; `faulty` faults on purpose to test isolation.
With `-DSUNSHINE_BUILD_EXAMPLE_MODS=ON` they build to `<exe dir>/mod-examples/<id>/`, which is not on the discovery
path. Copy a folder into `<exe dir>/mods/` to enable it.

## Verified

- Loader, measured: with `mods/overlay-demo` present the log shows `[mods] 1 overlay file(s) active` and `[mods] 0 DLL mod(s) active` (10 mod-on runs in `build/PortLauncherRun/*.log`). Without a `mods/` folder: `[mods] no mods folder (...), 0 mods loaded` (3 runs, e.g. `bilinear67_nomod.log`).
- Overlay effect in-game: NOT verified. No `[mods] overlay in use` and no `[mods] overlay replaced` line appears in any mod-on run (`src/mods/mods_loader.cpp:170,248`). The bianco stage (`SUNSHINE_DEV_START=bianco/0`) shows `ビアンコヒルズ`, not the replaced station name. Delfino was not run in this session.
- Carried over from an earlier session, not re-run: `mod_pack.py` output round-trips through `tools/discfmt.py` (only `/station.bmg` differs).
- Not exercised: unload of a faulted DLL mod.
- Disable switch: with `hello=0` in `[mods]`, a disc start logs `[mods] hello disabled by settings` (`build/UiDev/ui_pages3/mods_disc`, run before the 04:46:20 rebuild). `mods_disc_on` had no INI and no skip line. Not checked: a DLL mod that runs when disabled and enabled.
