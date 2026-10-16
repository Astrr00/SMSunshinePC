# Third-party licences

Sunshine is built from the decompilation in `sms/` plus the components below. Full licence texts ship in `licenses/`
of the release zip, with the embedded-header notices reproduced below. The pinned-input check describes the frozen
`build/Strict` carrier and its actual dependency inputs; another build, system package or archive requires a new check.

| Component | Used for | Version | Licence | File in `licenses/` |
|---|---|---|---|---|
| Aurora | GameCube SDK replacement | upstream `50e5be3` (submodule pin) + 18 port patches (`patches/aurora`) | MIT | `aurora-LICENSE` |
| Dawn / Tint (`webgpu_dawn.dll`) | WebGPU renderer | `v20260807.225922`, source `1155e0ed531126f33a1279afa029349651ca1c93` | BSD-3-Clause plus dependency notices | `dawn-LICENSE.txt`, `abseil-LICENSE` |
| DirectX Shader Compiler (`dxcompiler.dll`) | Shader compilation | Dawn DEPS pin `13730886e6a9019e4e0823746470f3ab75341d6b` | NCSA / LLVM Release License plus upstream third-party notices | `dxc-LICENSE.txt` |
| DirectX validator (`dxil.dll`) | Shader validation | file version `1.8.2502.11`; executable content matches installed Windows SDK `10.0.26100.0` copies, signing bytes differ | Microsoft SDK terms; exact signed-file provenance and distributor compliance unresolved | see below |
| SDL3 (`SDL3.dll`) | Window, input, audio | `3.4.10` (pinned) | Zlib | `SDL3-LICENSE.txt` |
| nod (`nod.dll`) | Disc image reading | `v2.0.0-alpha.12`, source `ebd80cac99b48a323200d84365e07a58cc27412d` | Apache-2.0 OR MIT; MIT elected, plus Rust/compression/dependency notices | `nod-LICENSE-MIT.txt` |
| libpng (`libpng16.dll`) | PNG loading | `v1.6.58` (pinned) | libpng License | `libpng-LICENSE` |
| zlib (`zlib1.dll`) | Compression | zlib-ng `2.3.3` (pinned) | zlib License | `zlib-LICENSE.md` |
| FreeType | Font rasterisation | `2.14.3` (pinned archive) | FreeType License (FTL), plus driver/hash notices | `freetype-LICENSE.TXT`, `freetype-FTL.TXT`, `freetype-BDF-notices.txt`, `freetype-PCF-notices.txt`; hash notice below |
| RmlUi | UI layout | `f00a0fee38391b2f927114e11bea18dc0a7dba1e` (pinned) | MIT, including robin_hood/itlib containers | `rmlui-LICENSE.txt`, `rmlui-container-LICENSE.txt`; flat_set notice below |
| Lato (`assets/launcher/LatoLatin-*.ttf`) | Launcher UI font | exact copies from that RmlUi snapshot | SIL OFL 1.1 | `Lato-OFL-LICENSE.txt` |
| Dear ImGui | Debug UI | `v1.91.9b-docking` (pinned) | MIT | `imgui-LICENSE.txt` |
| {fmt} | Formatting | `12.1.0` (pinned) | MIT (with exception) | `fmt-LICENSE` |
| xxHash | Hashing | `v0.8.3` (pinned) | BSD-2-Clause | `xxhash-LICENSE` |
| magic_enum | Aurora enum reflection | `0.7.2`, embedded in Aurora `50e5be3` | MIT | full embedded notice below |
| zstd | Compression | `1.5.7` (pinned archive) | BSD-3-Clause / GPLv2 dual; BSD elected | `zstd-LICENSE` |
| Abseil | C++ utilities | `20240722.0` (pinned) | Apache-2.0 | `abseil-LICENSE` |
| SQLite | Embedded database | `3.51.3` | Public domain | `sqlite3-PUBLIC-DOMAIN.txt` |
| Visual C++ runtime (`MSVCP140*.dll`, `VCRUNTIME140*.dll`) | C++ runtime | from Visual Studio 2022 redist | Microsoft redistributable terms | not shipped as a text file, see below |
| Decompilation (`sms/`) | Game code | repository | CC0 1.0 Universal | `LICENSE` (top level of the zip) |

## Notes

- **Pinned sources checked.** The Dawn BSD block and nod MIT block match the configured source revisions, not just
  the default branches. SQLite's blessing matches the actual `3.51.3` amalgamation from the hash-pinned archive.
  The former DXC text was only a summary; `dxc-LICENSE.txt` now retains the complete pinned `LICENSE.TXT` and
  `ThirdPartyNotices.txt`. Those upstream files identify their own build/test-only sections; including the files
  does not assert those test components ship.
- **Dawn dependencies.** The pinned release recipe and native CMake include/link references bring in SPIRV-Tools
  `5b5a25231a3543559b4f2cb27fe725a08b76b5d0`, SPIRV-Headers `942fe4b988359a0750b79f0ae7ed735994d3147d`,
  Vulkan-Headers `f9973cd97e6f3584707e7ef1c425e336f1b92a5b`, Vulkan-Utility-Libraries
  `6673ca41d73d36c3c838451e66dd30dd1ec46a97` and the embedded RenderDoc API header. Their source notices are
  retained in `dawn-LICENSE.txt`. Dawn's Abseil `df548c50b2cda67158364d3d23c63043881b391d` has byte-identical
  licence text to the separately packaged `abseil-LICENSE`. No RenderDoc DLL is bundled. The Dawn root licence's
  Apache section applies to `generator/templates/art/*` and `tools/android/webgpu/*`, not to all Dawn code.
- **nod dependencies.** The pinned CMake release recipe enables all four statically vendored compression backends
  and threading. The matching release's vendored-crates archive and `Cargo.lock` supply their source notices,
  including the separate bzip2, liblzma, stock-zlib, zstd, encoding_rs/WHATWG and xxhash-rust terms. The existing MIT
  choice applies to nod itself, not automatically to these dependencies. The notice file also retains the matching
  Rust `1.98.1` standard-library attributions: `nod.dll` embeds compiler source ID
  `48a229ceaefd4985c50990b14116b6d856af0985`. Macro/generated-code notices are retained; unrelated nodtool,
  tests and optional Python/OpenSSL dependencies are not inferred to ship.
- **FreeType.** This software is based in part on the work of the FreeType Team (https://freetype.org).
  The previous `LICENSE.TXT` was a licence-selection overview, not the FTL itself; the package now also copies
  `docs/FTL.TXT` and the compiled BDF/PCF driver notices. The compiled hash implementation's MIT notice is below.
  HarfBuzz is disabled in the inspected configuration.
- **RmlUi.** Third-party containers are enabled in Strict; their notices are packaged. Its pinned flat_set header
  has the additional notice below. `rmlui_debugger.lib` occurs on the link line, but there are no debugger API calls
  in the port/Aurora UI sources and neither embedded Courier Prime font's 1024-byte signature occurs in the carrier;
  a font notice is not added merely because the archive was built.
- **Tracy / GoogleTest.** Fetching or listing an archive on a link line is not evidence its code ships.
  Strict has `TRACY_ENABLE=OFF`; Aurora's Tracy calls are guarded, and no Tracy strings were found in the carrier.
  GoogleTest is linked by Aurora test executables, not by `sunshine.exe`. No profiling/test binary is packaged.
- **Windows SDK validator.** Dawn's pinned release workflow copies `dxil.dll` from `${WindowsSdkDir}`.
  The actual DLL SHA256 is `fda4dde92222ddb8564c3173ce48faa56f7e1c7b46b0b67dada1182ee07a4123`,
  file version `1.8.2502.11`. It is **not byte-identical** to official Microsoft DXC `v1.8.2502`
  `dxc_2025_02_20.zip:bin/x64/dxil.dll` (version `1.8.2502.8`), the checked `v1.8.2505`/`v1.8.2505.1`/
  `v1.9.2607` release members, or either installed SDK `10.0.26100.0` x64 copy.
  It does have **identical non-signing PE content** to both installed
  `Windows Kits/10/bin/10.0.26100.0/x64/dxil.dll` and `Windows Kits/10/Redist/D3D/x64/dxil.dll`:
  removing only the 4-byte PE checksum, 8-byte security-directory entry and certificate table gives
  SHA256 `afea7c16de218c35550734fb89388c6519012b955d37f0a417bb125b59027c1f` for all three.
  This identifies matching SDK executable content, not the origin of the exact upstream signed file.
  The matching installed SDK's legal references are `Licenses/10.0.26100.0/sdk_license.rtf`
  (SHA256 `0f4a26ac9dc50066f8a1bfeaaf3f092d1b9e4791df5487b9f3c31e7c3dc4d7f5`) and
  `sdk_third_party_notices.rtf` (SHA256 `ebb6640a8246b39b70322c5cec9aad7a71e86ea7b86c9c604d3a7f3ff1b30e89`).
  The EULA's section 2.a references the [Microsoft SDK redistribution list](https://learn.microsoft.com/en-us/legal/windows-sdk/redist),
  which names `Redist/D3D/x64/dxil.dll` for Classic Windows applications, conditional on a valid licence and all terms.
  [UNRESOLVED: exact upstream signed-file/SDK provenance and distributor entitlement/compliance; neither matching
  content nor the referenced conditional terms constitutes redistribution approval or acceptance of an EULA.]
- **Visual C++ runtime.** The packaging exercise resolves the runtime from Visual Studio 2022 BuildTools
  `VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT`. Redistribution still requires the applicable Microsoft terms
  and the distributor's entitlement; finding/copying a redist folder does not establish permission.
  [UNVERIFIED: exact EULA text, version-specific terms and redistribution approval.]
- **NVIDIA DLSS SDK.** Not included. No DLSS code, DLL or model is shipped.
- **Game data.** Not included. The user supplies their own legally obtained GMSJ01 disc image via `sunshine.exe --dvd`.
  No download links or decryption tools are shipped.
- **Sunshine's own code.** Licence of the port's own code is undecided. The decompilation in `sms/` is CC0 1.0.
- **Port artwork.** `tools/gen_glyphs.py` describes the controller glyphs as original generic geometry, not copied
  vendor artwork. The port's session provenance also establishes `assets/launcher/ocean.png` as original procedural
  Pillow artwork created for this port, not upstream/Nintendo artwork or copied pixels (adding commit `16d1c13`,
  SHA256 `a8e17ed0a80250f43cacb8a338163a530014068503b085cc569939532cfbd118`).
  No separate third-party artwork licence is inferred. Choosing a licence for the port's own code/artwork and
  establishing the distributor's rights remain owner decisions; this source/package check does not approve a release.

## Pinned-input receipt (2026-10-16)

The carrier SHA256 is `447ae21f0520f8027b172a692821c607bd142904c53332bd69850d28d0e136e0`.
The retained Dawn prebuilt archive SHA256 is
`42a7a295c87b2e8a36312e2f84a5823aac63325e8e1d271bc7ced6f83bfb075f`;
the nod prebuilt is `c9dfecf8e52066a5222f3cb1cad18d0740c958ce65bf60cb84e780a3856ed881`;
the actual SDL3 provider archive is `encounter/sdl3-build` `v3.4.10`,
SHA256 `5a641c2355b4cf3e710662f1b29d1255448ca55d9f34f1c62cd29668eaa3aabb`.
Their DLL bytes match the corresponding Strict DLLs. Dawn and nod archives contain no licence texts themselves.
The [Dawn release](https://github.com/encounter/dawn/releases/tag/v20260807.225922) binds its source SHA;
the [nod tag](https://github.com/encounter/nod/tree/ebd80cac99b48a323200d84365e07a58cc27412d) binds its source.

The original mapping contains **16**, not 15, source entries. For build-sourced rows below, source text bytes
match the named member in the retained FetchContent archive; all configured archive hashes that are present match.
Aurora's licence matches the submodule pin; its 18 tracked patches do not replace that licence.
Supplemental notices in the Dawn/nod/DXC files retain their own pinned source labels.

| Component | Exact inspected source | SHA256 of upstream text / result |
|---|---|---|
| Aurora | `50e5be30ec12cc72768bc564bc016b55e82be868:LICENSE` | `40c03cee04cda570d1029b85b6c2ee7ba78431ab1a754b9d912091d73b8ecb18` |
| SDL3 | prebuilt `licenses/SDL3/LICENSE.txt` | `1c040b8271b37e5076359f8fd54240e371114112924d2df81ef87c7d6a1dfdfd` |
| libpng | `v1.6.58/LICENSE` | `bdb0a645ea18c60507d0368379b1ac5474b92255fcc2d115e07486a7672ba526` |
| zlib-ng | `2.3.3/LICENSE.md` | `6c9f0d975b41afaa34d22f55bb8986ce69e5cb7ad327cb2b28820cd425edf5ee` |
| FreeType | `2.14.3/LICENSE.TXT` | `bd36c8b474855fa294c2ec5c184544478ef3720aad37d65a6296a4f264fd2d3b` |
| RmlUi | `f00a0fee38391b2f927114e11bea18dc0a7dba1e/LICENSE.txt` | `64c049f65ec810b805457dc1f24745002203f919ea5a16b6fcd7ca5f45ea298e` |
| Dear ImGui | `v1.91.9b-docking/LICENSE.txt` | `c80c5789748d955c4a650562baa0d750494e2c7128c2ca66aaebe2e3c4e198bf` |
| {fmt} | `12.1.0/LICENSE` | `07580f2a3b35709ce703d523f447b242f6dfec7582a8c0df102c7fa2849375f8` |
| xxHash | `v0.8.3/LICENSE` | `6ffedbc0f7878612d2b23589f1ff2ab15633e1df7963a5d9fc750ec5500c7e7a` |
| zstd | `1.5.7/LICENSE` | `7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8` |
| Abseil | `20240722.0/LICENSE` | `c79a7fea0e3cac04cd43f20e7b648e5a0ff8fa5344e644b0ee09ca1162b62747` |
| Dawn | `1155e0ed531126f33a1279afa029349651ca1c93:LICENSE`, BSD block | original packaged block matches; dependencies added separately |
| nod | `ebd80cac99b48a323200d84365e07a58cc27412d:LICENSE-MIT` | `544ffe9befaefa6b59ab1fdde58789069ba7c1b65432bdea273a16b2117909b7`; original block matches |
| SQLite | `sqlite-amalgamation-3510300.zip`, `sqlite3.c:35-40` | blessing matches; archive SHA256 `acb1e6f5d832484bf6d32b681e858c38add8b2acdfd42ac5df24b8afb46552b4` |
| DXC | `13730886e6a9019e4e0823746470f3ab75341d6b:LICENSE.TXT` and `ThirdPartyNotices.txt` | `27a49e35d1da96eba18fba54bc882667ff0ff8c0254f16f2b6e165d605ba7df8`; `19512a5d0a015ef16d167272c164da49a604614a786206212a80ad486ed0be6d` |
| Lato | pinned RmlUi `Samples/assets/LICENSE.txt`, both shipped fonts | licence bytes match, SHA256 `c97278bb336ab0fb2d3bb7ba3f303d638224d6dd1edc6e6fcdd4ba28b8dbf2a6`; font bytes also match |

## Embedded-header notices

### magic_enum 0.7.2

From Aurora `50e5be3`, `include/magic_enum.hpp:12-30`; included by compiled Aurora core/GX sources.

```text
// Copyright (c) 2019 - 2021 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
```

### FreeType hash implementation

From the pinned `freetype-2.14.3.tar.gz`, `src/base/fthash.c:9-31` (also in
`include/freetype/internal/fthash.h`).

```text
/*
 * Copyright 2000 Computing Research Labs, New Mexico State University
 * Copyright 2001-2015
 *   Francesco Zappa Nardelli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
```

### RmlUi's itlib flat_set

From pinned RmlUi `f00a0fee38391b2f927114e11bea18dc0a7dba1e`,
`Include/RmlUi/Core/Containers/itlib/flat_set.hpp:6-26`.
The container licence file separately retains the robin_hood and flat_map notices.

```text
// MIT License:
// Copyright(c) 2021-2023 Borislav Stanimirov
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files(the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and / or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions :
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

