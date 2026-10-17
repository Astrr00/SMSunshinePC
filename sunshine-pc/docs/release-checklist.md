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
Run the small packaging-safety regression before creating an archive:

```
python sunshine-pc/tools/package_test.py
```


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
licence text, or any game-image file (`.rvz .iso .dol .szs .gcm .wbfs .ciso .nkit .wia .gcz`).

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
- [ ] Re-check the licence items in `THIRD_PARTY_LICENSES.md` before publishing: own port-code/artwork licence (MIT, decided),
      Microsoft CRT/SDK entitlement/compliance and exact upstream signed-DXIL provenance. Matching executable
      content or a successful package copy is not legal approval.

## Package exclusion regression (2026-10-16)

The shared staged-folder/ZIP filter previously missed WIA and GCZ, although the launcher supports both. Both extensions are now blocked case-insensitively. The stdlib regression covers the observed pre-fix escape among legitimate port artwork/licences and passes on the corrected filter.

An actual `package.py` main-path smoke uses the current Strict binary and real PE/DLL/licence/cache resolution with copied port assets in an isolated temporary root. Clean packaging creates a 21.1 MiB ZIP; adding synthetic `accidental.WIA`/`accidental.gcz` fixtures makes the command refuse the package and leave no ZIP. No real game image or original save is used, and all temporary output is removed.

The build correctly warns that it is RelWithDebInfo, not Release. This is packaging-safety evidence, not current Release, redistribution, full-game or clean-PC approval. Those gates remain open.

## Pinned notices and normal lifecycle evidence (2026-10-16)

The actual frozen Strict inputs are traced to configured source/archive pins. The package now carries20 licence texts, including complete FreeType FTL/BDF/PCF and RmlUi containers, plus pinned Dawn/nod/DXC dependency notices. Actual isolated `package.py` main exits0 with134 files/11 DLLs; every notice matches its source, every ZIP member matches staging, forbidden extensions are absent and the checksum sidecar matches. The final internal ZIP SHA256 is `820a92e4c828e310fa68f8aa16893b86407638bcd712578ea305d5c44e188e53`; all temporary output is removed. RelWithDebInfo is not Release, and no archive is approved for publication.

At the final source seal, only 22 copied nod-notice CRLF sequences are normalized to repository LF; notice wording is unchanged. Actual package-main is rerun on the final Main files, with the same 134 files/11 DLLs/20 notices and all source/stage/ZIP/checksum checks passing. The final internal ZIP SHA256 `d33d55e14351ee2b12d70129a1056a9d88c1b1fcfc77223fdf3d461577f8e131` supersedes the preceding private checksum. Temporary output is removed. No native build/game run or Release approval is added.

The recommended release path remains `tools/package.py`. A bare `cmake --install`/CPack output does not provide that tool's complete dependency-notice/VC-runtime closure or exclusion gates; it is not an approved substitute.

An actual31m14.686s ordinary run completes three Bianco/ExitArea/Plaza Save/title/load cycles and native Close0. A fresh process loads the same two-Shine File B and closes natively0; GCI SHA256 `4f258d289914783426e0daeba81d8154b895d1872bcbde6a530b37f2a185ba8a`, exact IDs0/86/save5, seven checksums, persistent flags/21 saved integers and INI survive cold load. Real death/restart, next-episode progression, all-content/hardware/clean-PC and legal release gates remain open. Contended quarter-WARP is functional evidence, not a speed or listening certificate.


## Verified 2026-10-14 (ReleasePkg rebuild + pipeline cache seed)

- Release configure/build in `build/RelPkg` (`CMAKE_BUILD_TYPE=Release`, `SUNSHINE_ALLOW_UNRESOLVED=OFF`): rc 0, 0 `LNK2019/LNK2001/LNK1120` hits, `sunshine.exe` 11 233 280 bytes, `POST_BUILD` materialises `initial_pipeline_cache.db` (484 rows, 2 007 040 bytes) via `tools/pipeline_cache_seed.py`.
- `python tools/package.py --build-dir build/RelPkg --llvm-bin <LLVM bin>`: `dist/sunshine-0.1.0.zip` (21 252 707 bytes = 20.3 MiB, SHA-256 `754064342c734e94559b29bb99ebe087a9f5b94edf98c188a5fd7980b96c797f`), ships `sunshine.exe`, `initial_pipeline_cache.db`, 9 imported DLLs + `dxcompiler.dll`/`dxil.dll` (0 missing imports verified by `llvm-readobj` walk on the unpacked folder), `assets/`, and 16 licence files; 0 game data files.
- Unpacked smoke test in `%TEMP%\sunshine-pkg2\sunshine-0.1.0` with a private `APPDATA` (`SUNSHINE_ALLOW_CPU_ADAPTER=1` on this GPU-less VM): launcher renders (`launcher.png`); `--dvd` run logs `Seeded pipeline cache from '.../initial_pipeline_cache.db' (484 rows merged, 0 rows skipped)`, reaches the title screen at `logic 30/s`, 0 fatal exceptions, `crash.log` 0 bytes.

## Verified 2026-10-13 (lead + ReleasePkg worker)

- Release configure/build in a new tile: `python tools\locked.py cmd /c tools\build-alt.cmd ReleasePkg` style build of the `sunshine` target with `SUNSHINE_ALLOW_UNRESOLVED=OFF`: rc 0, 94 s with sccache, `sunshine.exe` 11 104 768 bytes, 0 `LNK2019/LNK2001/LNK1120` hits. `TracyClient.lib` is an 8.7 KB stub, the exe holds no Tracy strings, so no Tracy licence row is shipped.
- `cmake --install build/ReleasePkg --prefix <dir>`: 92 files, runtime DLLs now installed through `install(FILES $<TARGET_RUNTIME_DLLS:sunshine>)` plus `dxcompiler.dll`/`dxil.dll` (loaded by Dawn at run time, not in the import table). The install does NOT contain the VC runtime: the release path is `python tools/package.py --build-dir build/ReleasePkg --llvm-bin <LLVM bin> --redist-dir <...>\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT`, which adds it (zip `dist/sunshine-0.1.0.zip`, 20.2 MiB, 110 entries, `dist/` is git-ignored).
- Zip content check: no `.iso/.rvz/.szs/.arc/.dol/.bmd/.thp/.bin`, no `sms/`, assets complete (launcher rml/rcss/theme.rcss, fonts, glyphs, `cheats.json`), 16 licence files, every linked component (Aurora, SDL3, Dawn, DXC, nod, libpng, zlib, freetype, fmt, imgui, rmlui, sqlite3, xxhash, zstd, abseil, Lato) has a shipped text.
- Smoke of the UNPACKED zip from a different working directory with a temp `APPDATA`: launcher (WARP) shows the launcher page (screenshot written); `--dvd` with the owner's own disc image: 60 s run reaches `stage=15` (title), no fatal exception, ended by `SUNSHINE_DEV_MAX_SECONDS` (rc `0xDEAD0002`). Observed: the launcher run ignores `SUNSHINE_DEV_MAX_SECONDS` and had to be killed by the harness after 100 s.
- Not verified: a clean PC without Visual Studio (the VC runtime DLLs come from the zip), the Windows installer/CPack generator, code signing.

## First release attempt (2026-10-17)

- Fresh `windows-clang-release` build (Release, `/O2`), `package.py --version 0.1.0` -> `dist/sunshine-0.1.0.zip` (20.6 MiB, SHA-256 `1cdbfd466a2033415210728fc371b29517e8f7b42846e35a03e31d2633ff8888`), 11 DLLs, 0 forbidden files, `package_test.py` OK.
- Unpacked smoke from another cwd with a temp `APPDATA`: pipeline cache seeded (484 rows), 75 s at 30 logic/s, ended by `SUNSHINE_DEV_MAX_SECONDS`.
- GitHub: draft pre-release `v0.1.0` with the zip and `.sha256` created; not published. Publishing waits for the owner's decision on the Microsoft redistributable/DXIL items in `THIRD_PARTY_LICENSES.md`.
