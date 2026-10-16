#!/usr/bin/env python3
"""Build the installation-free Windows release: dist/sunshine-<version>.zip (+ .sha256).

Run after a Release build (docs/release-checklist.md). Stdlib only. Ships sunshine.exe, the DLLs it needs at runtime
(found by walking the import tables with llvm-readobj), assets/, licence texts and a README. Never ships game data:
the build fails if any game-image extension ends up in the staged folder or in the zip.

  python tools/package.py --build-dir build/windows-clang-release --llvm-bin "<LLVM bin dir>"
"""
import argparse
import glob
import hashlib
import os
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # sunshine-pc
SMS_LICENSE = os.path.join(ROOT, "..", "sms", "LICENSE")  # decompilation licence (CC0 1.0)
SYSTEM32 = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
FORBIDDEN_EXT = (".rvz", ".iso", ".dol", ".szs", ".gcm", ".wbfs", ".ciso", ".nkit", ".wia", ".gcz")
EXTRA_DLLS = ("dxcompiler.dll", "dxil.dll")  # Dawn LoadLibrary()s these; they are in no import table
CRT_PREFIXES = ("vcruntime140", "msvcp140", "concrt140", "vcomp140")  # redistributable, not in System32 on a clean PC

# (file name in licenses/, base, path). base "build": under the build dir; "root": under sunshine-pc.
LICENSES = [
    ("aurora-LICENSE", "root", "extern/aurora/LICENSE"),
    ("SDL3-LICENSE.txt", "build", "_deps/sdl3_prebuilt-src/licenses/SDL3/LICENSE.txt"),
    ("libpng-LICENSE", "build", "_deps/png-src/LICENSE"),
    ("zlib-LICENSE.md", "build", "_deps/zlib-src/LICENSE.md"),
    ("freetype-LICENSE.TXT", "build", "_deps/freetype-src/LICENSE.TXT"),
    ("rmlui-LICENSE.txt", "build", "_deps/rmlui-src/LICENSE.txt"),
    ("imgui-LICENSE.txt", "build", "_deps/imgui-src/LICENSE.txt"),
    ("fmt-LICENSE", "build", "_deps/fmt-src/LICENSE"),
    ("xxhash-LICENSE", "build", "_deps/xxhash-src/LICENSE"),
    ("zstd-LICENSE", "build", "_deps/zstd-src/LICENSE"),
    ("abseil-LICENSE", "build", "_deps/abseil-cpp-src/LICENSE"),
    ("dawn-LICENSE.txt", "root", "licenses/dawn-LICENSE.txt"),
    ("nod-LICENSE-MIT.txt", "root", "licenses/nod-LICENSE-MIT.txt"),
    ("sqlite3-PUBLIC-DOMAIN.txt", "root", "licenses/sqlite3-PUBLIC-DOMAIN.txt"),
    ("dxc-LICENSE.txt", "root", "licenses/dxc-LICENSE.txt"),
    ("Lato-OFL-LICENSE.txt", "root", "assets/launcher/LICENSE-Lato.txt"),
]

README = """Sunshine {version} - Super Mario Sunshine (GMSJ01) port for Windows

NO GAME DATA IS INCLUDED. Sunshine does not contain, download or decrypt Super Mario
Sunshine. Supply your own legally obtained GameCube disc image of GMSJ01 (NTSC-J).

No installation: keep this folder together and run

    sunshine.exe --dvd "C:\\path\\to\\your\\image"

The image format is whatever the disc reader accepts (iso/rvz/wia/...). Without --dvd,
sunshine.exe opens the launcher.

Requirements: Windows 10/11 x64 and a GPU with a current driver.

Contents:
  sunshine.exe               game and port
  initial_pipeline_cache.db  bundled GX pipeline cache seed
  *.dll                      runtime libraries (see THIRD_PARTY_LICENSES.md)
  assets/                    port assets
  LICENSE                    licence of the decompilation (CC0 1.0)
  THIRD_PARTY_LICENSES.md    third-party components and their licences
  licenses/                  full licence texts

Not included: game data, NVIDIA DLSS SDK files.
"""


def read_version():
    with open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8") as f:
        m = re.search(r"project\(\s*sunshine\s+VERSION\s+(\d+(?:\.\d+)+)", f.read())
    if not m:
        sys.exit("error: no 'project(sunshine VERSION x.y.z)' in CMakeLists.txt")
    return m.group(1)


def find_llvm_readobj(llvm_bin):
    explicit = llvm_bin or os.environ.get("LLVM_BIN")
    if explicit:
        path = os.path.join(explicit, "llvm-readobj.exe")
        if not os.path.isfile(path):
            sys.exit(f"error: {path} not found (--llvm-bin / LLVM_BIN)")
        return path
    path = shutil.which("llvm-readobj")
    if not path:
        sys.exit("error: llvm-readobj not found; pass --llvm-bin <clang-cl bin dir>")
    return path


def imports_of(readobj, pe):
    out = subprocess.run([readobj, "--coff-imports", pe], capture_output=True, text=True, check=True).stdout
    return re.findall(r"^  Name: (\S+)\s*$", out, re.M)


def resolve_crt(names, explicit):
    if not names:
        return {}
    dirs = list(explicit)
    for pf in {os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles")} - {None}:
        # ponytail: lexical sort picks the newest toolset only while version digits align; pin with --redist-dir.
        dirs += sorted(glob.glob(os.path.join(pf, "Microsoft Visual Studio", "*", "*", "VC", "Redist", "MSVC", "*",
                                              "x64", "Microsoft.VC*.CRT")), reverse=True)
    for d in dirs:
        if all(os.path.isfile(os.path.join(d, n)) for n in names):
            return {n: os.path.join(d, n) for n in names}
    sys.exit(f"error: VC runtime {sorted(names)} not found; pass --redist-dir <...\\VC\\Redist\\MSVC\\<v>\\x64\\Microsoft.VCxxx.CRT>")


def runtime_dlls(exe, build, readobj, redist):
    """Transitive import closure of sunshine.exe: build-dir DLLs are shipped, VC runtime comes from the redist dir,
    everything else must be a Windows system DLL (System32) or an API set."""
    shipped, crt, seen = {}, set(), set()
    pending = [exe]
    for n in EXTRA_DLLS:
        p = os.path.join(build, n)
        if not os.path.isfile(p):
            sys.exit(f"error: {p} missing (Dawn loads it at runtime)")
        shipped[n] = p
        pending.append(p)
    while pending:
        pe = pending.pop()
        for name in imports_of(readobj, pe):
            low = name.lower()
            if low in seen or low.startswith(("api-ms-win-", "ext-ms-")):
                continue
            seen.add(low)
            local = os.path.join(build, name)
            if os.path.isfile(local):
                shipped[name] = local
                pending.append(local)
            elif low.startswith(CRT_PREFIXES):
                crt.add(name)
            elif not os.path.isfile(os.path.join(SYSTEM32, name)):
                sys.exit(f"error: {name} (needed by {os.path.basename(pe)}) is neither in the build dir nor in System32")
    shipped.update(resolve_crt(crt, redist))
    return shipped


def banned(paths):
    return [p for p in paths if p.lower().endswith(FORBIDDEN_EXT)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", default=os.path.join(ROOT, "build", "windows-clang-release"))
    ap.add_argument("--version", help="default: VERSION in CMakeLists.txt")
    ap.add_argument("--llvm-bin", help="dir with llvm-readobj.exe (default: $LLVM_BIN, then PATH)")
    ap.add_argument("--redist-dir", action="append", default=[],
                    help="dir with the x64 VC runtime DLLs (Microsoft.VCxxx.CRT); repeatable")
    ap.add_argument("--with-pdb", action="store_true", help="also ship sunshine.pdb (default: not shipped)")
    args = ap.parse_args()

    build = os.path.abspath(args.build_dir)
    exe = os.path.join(build, "sunshine.exe")
    if not os.path.isfile(exe):
        sys.exit(f"error: {exe} missing; build the sunshine target first")
    version = args.version or read_version()
    if not re.fullmatch(r"\d+(\.\d+)+", version):
        sys.exit(f"error: bad version {version!r}")
    cache = os.path.join(build, "CMakeCache.txt")
    m = re.search(r"^CMAKE_BUILD_TYPE:\w+=(.*)$", open(cache, encoding="utf-8").read(), re.M) if os.path.isfile(cache) else None
    if not m or m.group(1).strip() != "Release":
        print(f"warning: build type is {m.group(1) if m else 'unknown'}, not Release", file=sys.stderr)
    assets = os.path.join(ROOT, "assets")
    if not os.path.isdir(assets):
        sys.exit(f"error: {assets} missing; generate the port assets before packaging")
    for path in (SMS_LICENSE, os.path.join(ROOT, "THIRD_PARTY_LICENSES.md")):
        if not os.path.isfile(path):
            sys.exit(f"error: {path} missing")

    dlls = runtime_dlls(exe, build, find_llvm_readobj(args.llvm_bin), args.redist_dir)

    name = f"sunshine-{version}"
    dist = os.path.join(ROOT, "dist")
    stage = os.path.join(dist, name)
    zip_path = os.path.join(dist, name + ".zip")
    shutil.rmtree(stage, ignore_errors=True)
    for stale in (zip_path, zip_path + ".sha256"):
        if os.path.exists(stale):
            os.remove(stale)
    os.makedirs(os.path.join(stage, "licenses"))

    shutil.copy2(exe, stage)
    if args.with_pdb:
        shutil.copy2(os.path.join(build, "sunshine.pdb"), stage)
    seed_db = os.path.join(build, "initial_pipeline_cache.db")
    if not os.path.isfile(seed_db):
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "pipeline_cache_seed.py"), seed_db], check=True)
    shutil.copy2(seed_db, os.path.join(stage, "initial_pipeline_cache.db"))
    for dll_name, src in sorted(dlls.items()):
        shutil.copy2(src, os.path.join(stage, dll_name))
    shutil.copytree(assets, os.path.join(stage, "assets"))
    shutil.copy2(SMS_LICENSE, os.path.join(stage, "LICENSE"))
    shutil.copy2(os.path.join(ROOT, "THIRD_PARTY_LICENSES.md"), stage)
    for dest, base, rel in LICENSES:
        src = os.path.join(ROOT if base == "root" else build, rel)
        if not os.path.isfile(src):
            sys.exit(f"error: licence text {src} missing")
        shutil.copy2(src, os.path.join(stage, "licenses", dest))
    with open(os.path.join(stage, "README.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(README.format(version=version))

    staged = [os.path.relpath(os.path.join(dp, f), stage) for dp, _, fs in os.walk(stage) for f in fs]
    if banned(staged):
        sys.exit(f"error: game image files in package: {banned(staged)}")

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for dp, _, fs in os.walk(stage):
            for f in sorted(fs):
                full = os.path.join(dp, f)
                zf.write(full, os.path.relpath(full, dist).replace(os.sep, "/"))
    with zipfile.ZipFile(zip_path) as zf:
        if banned(zf.namelist()):
            os.remove(zip_path)
            sys.exit("error: game image files in zip")
    digest = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
    with open(zip_path + ".sha256", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"{digest}  {name}.zip\n")

    for dll_name, src in sorted(dlls.items()):
        print(f"dll  {dll_name:24} <- {src}")
    print(f"zip  {zip_path}  ({os.path.getsize(zip_path) / 2**20:.1f} MiB)")
    print(f"sha256 {digest}")


if __name__ == "__main__":
    main()
