# Release checklist (Windows)

Output: `sunshine-pc/dist/sunshine-<version>.zip` plus `.sha256`. Installation-free. No game data.

## 1. Preconditions

- [ ] `sunshine-pc/assets/` exists and holds the generated glyph assets. `package.py` fails without it.
- [ ] `sms/` is unmodified for the release build (SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625` for the matching build).
- [ ] `THIRD_PARTY_LICENSES.md` and `licenses/` are present (CMake install requires them).
- [ ] `CMakeLists.txt` `project(sunshine VERSION x.y.z)` is the version to ship.
- [ ] A recent LLVM/clang-cl is installed (llvm-readobj is needed for the DLL closure).

## 2. Configure Release

```
cmake -S sunshine-pc -B sunshine-pc/build/windows-clang-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
```

The `CMakePresets.json` has no release preset; the command above is the release configure.

## 3. Build

```
cmake --build sunshine-pc/build/windows-clang-release --target sunshine
```

## 4. Package

```
python sunshine-pc/tools/package.py --build-dir sunshine-pc/build/windows-clang-release --version x.y.z --llvm-bin "C:/llvm-dl/t/clang+llvm-23.1.3-x86_64-pc-windows-msvc/bin"
```

Internal build with symbols (keep the PDB out of the public zip):

```
python sunshine-pc/tools/package.py --build-dir sunshine-pc/build/windows-clang-release --version x.y.z --llvm-bin "<LLVM bin>" --with-pdb
```

Extra VC runtime location, if the default Visual Studio redist path is not found:

```
... --redist-dir "<VS>/VC/Redist/MSVC/<ver>/x64/Microsoft.VC143.CRT"
```

The script prints each shipped DLL with its source path and the zip SHA-256. It fails on a missing DLL, a missing
licence text, or any game-image file (`.rvz .iso .dol .szs .gcm .wbfs .ciso .nkit`).

## 5. Verify the artifact

- [ ] `dist/sunshine-x.y.z.zip.sha256` matches `certutil -hashfile dist/sunshine-x.y.z.zip SHA256`.
- [ ] Unzip into an empty folder outside the repo.
- [ ] `sunshine.exe --dvd "<your image>"` starts from that folder (use your own legally obtained GMSJ01 image).
- [ ] No `.pdb` in the zip unless it is an internal build.
- [ ] `README.md`, `LICENSE`, `THIRD_PARTY_LICENSES.md` and `licenses/` are present.
- [ ] No game data in the folder or zip (`tar -tf` / Explorer search for the forbidden extensions).

## 6. Publish

- [ ] Upload the zip and `.sha256` only. Do not upload disc images, PDBs (except as a separate symbols archive),
      or `build/`.
- [ ] Keep the release notes free of game data and download links to game data.
- [ ] Re-check the licence items in `THIRD_PARTY_LICENSES.md` open notes before publishing (Visual C++ redist terms,
      the port's own licence).

## Verified 2026-10-14 (ReleasePkg rebuild + pipeline cache seed)

- Release configure/build in `build/RelPkg` (`CMAKE_BUILD_TYPE=Release`, `SUNSHINE_ALLOW_UNRESOLVED=OFF`): rc 0, 0 `LNK2019/LNK2001/LNK1120` hits, `sunshine.exe` 11 233 280 bytes, `POST_BUILD` materialises `initial_pipeline_cache.db` (484 rows, 2 007 040 bytes) via `tools/pipeline_cache_seed.py`.
- `python tools/package.py --build-dir build/RelPkg --llvm-bin <LLVM bin>`: `dist/sunshine-0.1.0.zip` (21 252 707 bytes = 20.3 MiB, SHA-256 `cebd582ba708e18f0bb2e7640b9b271d8d5ca92a10a5ec6ce05a3c50e64305a5`), ships `sunshine.exe`, `initial_pipeline_cache.db`, 9 imported DLLs + `dxcompiler.dll`/`dxil.dll` (0 missing imports verified by `llvm-readobj` walk on the unpacked folder), `assets/`, and 16 licence files; 0 game data files.
- Unpacked smoke test in `%TEMP%\sunshine-pkg2\sunshine-0.1.0` with a private `APPDATA` (`SUNSHINE_ALLOW_CPU_ADAPTER=1` on this GPU-less VM): launcher renders (`launcher.png`); `--dvd` run logs `Seeded pipeline cache from '.../initial_pipeline_cache.db' (484 rows merged, 0 rows skipped)`, reaches the title screen at `logic 30/s`, 0 fatal exceptions, `crash.log` 0 bytes.

## Verified 2026-10-13 (lead + ReleasePkg worker)

- Release configure/build in a new tile: `python tools\locked.py cmd /c tools\build-alt.cmd ReleasePkg` style build of the `sunshine` target with `SUNSHINE_ALLOW_UNRESOLVED=OFF`: rc 0, 94 s with sccache, `sunshine.exe` 11 104 768 bytes, 0 `LNK2019/LNK2001/LNK1120` hits. `TracyClient.lib` is an 8.7 KB stub, the exe holds no Tracy strings, so no Tracy licence row is shipped.
- `cmake --install build/ReleasePkg --prefix <dir>`: 92 files, runtime DLLs now installed through `install(FILES $<TARGET_RUNTIME_DLLS:sunshine>)` plus `dxcompiler.dll`/`dxil.dll` (loaded by Dawn at run time, not in the import table). The install does NOT contain the VC runtime: the release path is `python tools/package.py --build-dir build/ReleasePkg --llvm-bin <LLVM bin> --redist-dir <...>\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT`, which adds it (zip `dist/sunshine-0.1.0.zip`, 20.2 MiB, 110 entries, `dist/` is git-ignored).
- Zip content check: no `.iso/.rvz/.szs/.arc/.dol/.bmd/.thp/.bin`, no `sms/`, assets complete (launcher rml/rcss/theme.rcss, fonts, glyphs, `cheats.json`), 16 licence files, every linked component (Aurora, SDL3, Dawn, DXC, nod, libpng, zlib, freetype, fmt, imgui, rmlui, sqlite3, xxhash, zstd, abseil, Lato) has a shipped text.
- Smoke of the UNPACKED zip from a different working directory with a temp `APPDATA`: launcher (WARP) shows the launcher page (screenshot written); `--dvd` with the owner's own disc image: 60 s run reaches `stage=15` (title), no fatal exception, ended by `SUNSHINE_DEV_MAX_SECONDS` (rc `0xDEAD0002`). Observed: the launcher run ignores `SUNSHINE_DEV_MAX_SECONDS` and had to be killed by the harness after 100 s.
- Not verified: a clean PC without Visual Studio (the VC runtime DLLs come from the zip), the Windows installer/CPack generator, code signing.
