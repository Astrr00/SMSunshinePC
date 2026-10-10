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
