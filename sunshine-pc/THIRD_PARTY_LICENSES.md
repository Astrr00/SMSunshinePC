# Third-party licences

Sunshine is built from the decompilation in `sms/` plus the components below. Full licence texts ship in `licenses/`
of the release zip. Versions marked "pinned" are fixed in the CMake build; other versions are whatever the
`_deps/` checkout contains at build time.

| Component | Used for | Version | Licence | File in `licenses/` |
|---|---|---|---|---|
| Aurora | GameCube SDK replacement | HEAD `94431fd6` (pinned) | MIT | `aurora-LICENSE` |
| Dawn / Tint (`webgpu_dawn.dll`) | WebGPU renderer | `v20260807.225922` (pinned) | BSD-3-Clause | `dawn-LICENSE.txt` |
| DirectX Shader Compiler (`dxcompiler.dll`, `dxil.dll`) | Shader compilation | from Dawn prebuilt | NCSA / LLVM Release License (plus Miniz notice) | `dxc-LICENSE.txt` |
| SDL3 (`SDL3.dll`) | Window, input, audio | `3.4.10` (pinned) | Zlib | `SDL3-LICENSE.txt` |
| nod (`nod.dll`) | Disc image reading | `v2.0.0-alpha.12` (pinned) | Apache-2.0 OR MIT; shipped under MIT | `nod-LICENSE-MIT.txt` |
| libpng (`libpng16.dll`) | PNG loading | pinned in CMake | libpng License | `libpng-LICENSE` |
| zlib (`zlib1.dll`) | Compression | pinned in CMake | zlib License | `zlib-LICENSE.md` |
| FreeType | Font rasterisation | pinned in CMake | FreeType License (FTL) | `freetype-LICENSE.TXT` |
| RmlUi | UI layout | pinned in CMake | MIT | `rmlui-LICENSE.txt` |
| Lato (`assets/launcher/LatoLatin-*.ttf`) | Launcher UI font | as shipped in RmlUi samples | SIL OFL 1.1 | `Lato-OFL-LICENSE.txt` |
| Dear ImGui | Debug UI | pinned in CMake | MIT | `imgui-LICENSE.txt` |
| {fmt} | Formatting | pinned in CMake | MIT (with exception) | `fmt-LICENSE` |
| Tracy | Profiling client (inactive in release) | pinned in CMake | BSD-3-Clause | `tracy-LICENSE` |
| xxHash | Hashing | `v0.8.3` (pinned) | BSD-2-Clause | `xxhash-LICENSE` |
| zstd | Compression | pinned in CMake | BSD-3-Clause / GPLv2 dual; BSD elected | `zstd-LICENSE` |
| Abseil | C++ utilities | `20240722.0` (pinned) | Apache-2.0 | `abseil-LICENSE` |
| SQLite | Embedded database | `3.51.3` | Public domain | `sqlite3-PUBLIC-DOMAIN.txt` |
| Visual C++ runtime (`MSVCP140*.dll`, `VCRUNTIME140*.dll`) | C++ runtime | from Visual Studio 2022 redist | Microsoft redistributable terms | not shipped as a text file, see below |
| Decompilation (`sms/`) | Game code | repository | CC0 1.0 Universal | `LICENSE` (top level of the zip) |

## Notes

- **Upstream texts, not pinned commits.** The Dawn, nod, SQLite and DXC texts in `sunshine-pc/licenses/` were copied
  from the upstream default branches (SQLite from the amalgamation header). They are the licences of those projects
  at the time of copying; compare them against the pinned revisions before a release.
- **nod.** Upstream offers Apache-2.0 OR MIT. The release elects MIT. The Apache-2.0 text is not shipped.
- **Dawn.** The Apache-2.0 part of the Dawn repository covers generator templates and Android tooling, which are not
  part of the shipped binary.
- **Visual C++ runtime.** `MSVCP140*.dll` and `VCRUNTIME140*.dll` are copied from the Visual Studio 2022 redistributable
  folder into the release. Redistribution is governed by the Microsoft Visual Studio redistributable terms, which
  are not reproduced here. [UNVERIFIED: exact EULA text and version-specific terms]
- **NVIDIA DLSS SDK.** Not included. No DLSS code, DLL or model is shipped.
- **Game data.** Not included. The user supplies their own legally obtained GMSJ01 disc image via `sunshine.exe --dvd`.
  No download links or decryption tools are shipped.
- **Sunshine's own code.** Licence of the port's own code is undecided. The decompilation in `sms/` is CC0 1.0.
