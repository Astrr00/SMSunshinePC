"""Compile the production pickup expressions against the real host field offsets.

The fixture models object storage, not the surrounding gameplay or effects.
An optional unchanged source path provides a negative control.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/Player/MarioReceiveMsg.cpp"
source = source_path.read_text(encoding="utf-8")


def argument(pos):
    start, depth = pos + 1, 1
    while depth:
        pos += 1
        depth += (source[pos] == "(") - (source[pos] == ")")
    return source[start:pos]


item = source.index("case 0x20000005:")
outer_if = source.index("if (", item)
inner_if = source.index("if (", outer_if + 4)
predicate = argument(inner_if + 3)
shine = source.index("case 0x20000013:")
yaw = argument(source.index("DEG2SHORTANGLE(", shine) + len("DEG2SHORTANGLE"))

fixture = r"""
#include <MoveBG/MapObjItem2.hpp>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace fixture {
struct THitActor {};
// Real headers determine every tested member offset; no game constructor runs.
struct alignas(::TMushroom1up) TMushroom1up : THitActor {
    u8 prefix[offsetof(::TMushroom1up, mInitialRotation)];
    JGeometry::TVec3<f32> mInitialRotation;
    u8 gap[offsetof(::TMushroom1up, unk13A) - offsetof(::TMushroom1up, mInitialRotation)
           - sizeof(mInitialRotation)];
    s8 unk13A;
    int unk13C;
};
using TMapObjBase = TMushroom1up;
static_assert(offsetof(TMushroom1up, unk13A) == offsetof(::TMushroom1up, unk13A));
static_assert(offsetof(TMushroom1up, unk13C) == offsetof(::TMushroom1up, unk13C));
static_assert(offsetof(TMushroom1up, mInitialRotation) == offsetof(::TMapObjBase, mInitialRotation));
bool eligible(THitActor* sender) { return (
""" + predicate + r"""
); }
float initialYaw(THitActor* sender) { return (
""" + yaw + r"""
); }
}

int main() {
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
    };
    struct Case { s8 collected; int age; s8 oldFlag; int oldAge; bool expected; const char* name; };
    const Case cases[] = {
        {0,119,0,0,false,"1-up remains unavailable before tick120"},
        {0,120,0,0,true,"1-up becomes available at tick120"},
        {0,200,0,0,true,"mature stationary 1-up is collectible"},
        {1,0,0,0,true,"original nonzero-state condition is retained"},
        {0,0,1,0,false,"ride-position bytes cannot enable an early pickup"},
        {0,0,0,120,false,"ground-actor yaw bytes cannot enable an early pickup"},
        {0,120,0,119,true,"ground-actor yaw bytes cannot block a mature pickup"},
    };
    for (const auto& c : cases) {
        fixture::TMushroom1up item{};
        std::memcpy(reinterpret_cast<u8*>(&item) + 0x13A, &c.oldFlag, sizeof(c.oldFlag));
        std::memcpy(reinterpret_cast<u8*>(&item) + 0x13C, &c.oldAge, sizeof(c.oldAge));
        item.unk13A = c.collected;
        item.unk13C = c.age;
        check(fixture::eligible(&item) == c.expected, c.name);
    }
    for (float angle : {0.0f, 90.0f, -90.0f, 180.0f}) {
        fixture::TMushroom1up item{};
        const float oldGravity = 0.35f;
        std::memcpy(reinterpret_cast<u8*>(&item) + 0x11C, &oldGravity, sizeof(oldGravity));
        item.mInitialRotation.y = angle;
        check(fixture::initialYaw(&item) == angle, "Shine uses its initial yaw, not gravity");
    }
    std::printf("%d pickup-layout checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/pickup-layout-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\pickup_layout_test.cmd")
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/w", "-Wno-register",
           "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude", "/I../sms/include",
           "/Iextern/aurora/include", str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
