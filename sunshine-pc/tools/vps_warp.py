"""GPU-less host (VPS): install a newer WARP software rasteriser next to sunshine.exe.

  python tools/vps_warp.py <tree> [<tree> ...]      e.g. Strict

The Windows-inbox WARP crashes (0xc0000005 inside the driver while creating a GX pipeline) in the dolpic0 stage; the
Microsoft.Direct3D.WARP NuGet package (1.0.20) renders it. The DLL is downloaded from nuget.org into
build/<tree>/d3d10warp.dll (build/ is untracked; nothing binary is committed). Run games with
SUNSHINE_ALLOW_CPU_ADAPTER=1 (tools/route.py sets it by default) so Aurora accepts the CPU adapter.
"""
import io
import pathlib
import sys
import urllib.request
import zipfile

VERSION = "1.0.20"
URL = f"https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.warp/{VERSION}/microsoft.direct3d.warp.{VERSION}.nupkg"
MEMBER = "build/native/bin/x64/d3d10warp.dll"
ROOT = pathlib.Path(__file__).resolve().parent.parent


def main():
    trees = sys.argv[1:]
    if not trees:
        sys.exit(__doc__)
    with urllib.request.urlopen(URL, timeout=120) as r:
        pkg = zipfile.ZipFile(io.BytesIO(r.read()))
    dll = pkg.read(MEMBER)
    for tree in trees:
        dst = ROOT / "build" / tree / "d3d10warp.dll"
        if not dst.parent.is_dir():
            sys.exit(f"no build tree {dst.parent}")
        dst.write_bytes(dll)
        print(f"wrote {dst} ({len(dll)} bytes, WARP {VERSION})")


if __name__ == "__main__":
    main()
