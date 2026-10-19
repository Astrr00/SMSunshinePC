"""Run the complete production Mario viewport caller at /O2 /Ob1 with ASan.

An optional MarioDraw.cpp snapshot supplies the negative control. The fixture
extracts the unchanged Aurora viewport/projection writers and projector, uses
real vector/matrix declarations, and stubs the subsequent render setup. It
checks stack safety and projected coordinates, not rendered occlusion results.
"""
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SMS = ROOT.parent / "sms"
AURORA = ROOT / "extern/aurora"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", nargs="?", type=Path, default=SMS / "src/Player/MarioDraw.cpp")
args = parser.parse_args()


def function(path, signature):
    source = path.read_text(encoding="utf-8")
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


body = function(args.source, "void TMario::boxDrawPrepare(")
writers = "\n".join(function(AURORA / "lib/dolphin/gx/GXGet.cpp", signature)
                    for signature in ("void GXGetViewportv(", "void GXGetProjectionv("))
projector = function(AURORA / "lib/dolphin/gx/GXTransform.cpp", "void GXProject(")
fixture = r"""
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <JSystem/JGeometry/JGVec3.hpp>
"""
# Use the native GX declarations; the original console GXProject prototype
# lacks the const qualifiers used by Aurora's actual implementation.
for header in ("GXGet.h", "GXTransform.h"):
    fixture += f'#include "{(AURORA / "include/dolphin/gx" / header).as_posix()}"\n'
fixture += r"""
#define CHECK(condition, ...) do { if (!(condition)) std::abort(); } while (0)
struct FixtureGXState {
    f32 vpLeft, vpTop, vpWd, vpHt, vpNearz, vpFarz;
    GXProjectionType projType;
    f32 projMtx[6];
};
static FixtureGXState state;
static FixtureGXState* __gx = &state;
// Keep the actual caller/writer boundary observable under /O2 and ASan.
extern "C" __declspec(noinline) void GXGetViewportv(f32*);
extern "C" __declspec(noinline) void GXGetProjectionv(f32*);
extern "C" __declspec(noinline) void GXProject(f32, f32, f32, const f32[3][4],
                                             const f32*, const f32*, f32*, f32*, f32*);
""" + writers + "\n" + projector + r"""
struct TMario {
    JGeometry::TVec3<f32> mPosition;
    JGeometry::TVec3<f32> mMarioScreenPos;
    __declspec(noinline) void boxDrawPrepare(MtxPtr);
};
"""
# Rendering starts after projection and has no output examined by this fixture.
# Leave every production local declaration and writer/projector call intact.
for name in sorted(set(re.findall(r"\b(?:GX|MTX)\w+(?=\()", body))
                   - {"GXGetProjectionv", "GXGetViewportv", "GXProject"}):
    fixture += f"#undef {name}\n#define {name}(...) ((void)0)\n"
fixture += body + r"""

int main() {
    static_assert(sizeof(f32) == 4 && sizeof(Mtx) == 48);
    int checks = 0, failures = 0;
    auto check = [&](f32 actual, f32 expected, const char* component) {
        ++checks;
        if (!std::isfinite(actual) || std::fabs(actual - expected) > 0.0001f) {
            ++failures;
            std::printf("FAIL: %s: got %g expected %g\n", component, actual, expected);
        }
    };
    struct Viewport { f32 left, top, width, height, nearZ, farZ; };
    const Viewport viewports[] = {
        {0, 0, 640, 480, 0, 1},
        {10, 20, 800, 600, 0.2f, 0.75f},
        {-30, 15, 320, 240, 0.125f, 0.625f},
        {0, 0, 640, 480, 0.875f, 0.25f},
    };
    struct Case {
        GXProjectionType projection;
        f32 x, y, z;
        f32 matrixX, matrixY, matrixZ;
        f32 normalizedX, normalizedY, normalizedDepth;
    };
    const Case cases[] = {
        {GX_ORTHOGRAPHIC, 0, -80, 0, 0, 0, 0, 0.5f, 0.5f, 0},
        {GX_ORTHOGRAPHIC, 0.25f, -79.5f, 0.5f, 0, 0, 0, 0.625f, 0.25f, 0.5f},
        {GX_ORTHOGRAPHIC, -1, -79.75f, -0.5f, 1, -0.25f, 1, 0.5f, 0.5f, 0.5f},
        {GX_PERSPECTIVE, 0, -80, -2, 0, 0, 0, 0.5f, 0.5f, -1},
        {GX_PERSPECTIVE, 0.5f, -79.5f, -2, 0, 0, 0, 0.625f, 0.375f, -1},
    };
    for (const auto& vp : viewports) {
        state.vpLeft = vp.left;
        state.vpTop = vp.top;
        state.vpWd = vp.width;
        state.vpHt = vp.height;
        state.vpNearz = vp.nearZ;
        state.vpFarz = vp.farZ;
        // Unit orthographic or symmetric perspective coefficients.
        for (int i = 0; i < 6; ++i) state.projMtx[i] = i % 2 == 0 ? 1.f : 0.f;
        for (const auto& c : cases) {
            state.projType = c.projection;
            TMario mario;
            mario.mPosition.set(c.x, c.y, c.z);
            mario.mMarioScreenPos.set(-999.f, -999.f, -999.f);
            Mtx matrix = {{1, 0, 0, c.matrixX}, {0, 1, 0, c.matrixY}, {0, 0, 1, c.matrixZ}};
            mario.boxDrawPrepare(matrix);
            check(mario.mMarioScreenPos.x, vp.left + c.normalizedX * vp.width, "projected X");
            check(mario.mMarioScreenPos.y, vp.top + c.normalizedY * vp.height, "projected Y (+80 world offset)");
            check(mario.mMarioScreenPos.z, vp.farZ + c.normalizedDepth * (vp.farZ - vp.nearZ),
                  "projected Z (sixth viewport float)");
        }
    }
    std::printf("%d Mario viewport checks, %d failed; ASan enabled\n", checks, failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/mario-viewport-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_text(fixture, encoding="utf-8")
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    found = shutil.which("clang-cl")
    if not found:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\mario_viewport_test.cmd")
    compiler = Path(found)
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/Zi", "/fsanitize=address",
           "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude", f"/I{SMS / 'include'}",
           f"/I{AURORA / 'include'}", "-Wno-unknown-pragmas", "-Wno-register", str(cpp),
           f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}", "/link", "/DEBUG"]
print(f"Testing production caller from {args.source.resolve()}", flush=True)
subprocess.run(command, check=True, cwd=ROOT)
env = os.environ.copy()
runtime_dirs = list((compiler.parent.parent / "lib/clang").glob("*/lib/windows"))
env["PATH"] = os.pathsep.join(str(path) for path in runtime_dirs) + os.pathsep + env.get("PATH", "")
symbolizer = compiler.parent / "llvm-symbolizer.exe"
if symbolizer.is_file():
    env["ASAN_SYMBOLIZER_PATH"] = str(symbolizer)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT, env=env).returncode)
