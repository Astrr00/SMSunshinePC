"""Compile the complete production slideProcess body at Strict's /O2 /Ob1.

The fixture supplies flat ground, zero slope acceleration and angle-adjust speed.
It tests the body in isolation, not the engine's collision or stop-state logic.
An optional unchanged MarioRun.cpp path supplies the negative control.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/Player/MarioRun.cpp"
source = source_path.read_text(encoding="utf-8")
start = source.index("void TMario::slideProcess(f32 baseAcc, f32 friction)")
body = source[start:source.index("\nBOOL TMario::doSliding(", start)]

fixture = r"""
#include <cmath>
#include <cstdio>
#include <initializer_list>
using f32 = float;
using s16 = short;
struct Vec { f32 x = 0, y = 0, z = 0; };
struct TBGCheckData {
    Vec normal{0,1,0};
    const Vec& getNormal() const { return normal; }
};
f32 MsSqrtf(f32 x) { return std::sqrt(x); }
s16 matan(f32 z, f32 x) {
    return static_cast<s16>(static_cast<int>(std::atan2(x,z) * (32768.0 / 3.141592653589793)));
}
f32 JMASSin(s16 angle) { return std::sin(angle * (3.141592653589793 / 32768.0)); }
f32 JMASCos(s16 angle) { return std::cos(angle * (3.141592653589793 / 32768.0)); }
struct TMario {
    const TBGCheckData* mGroundPlane;
    s16 mSlopeAngle = 0;
    struct { s16 y = 0; } mFaceAngle;
    f32 mSlideVelX = 0, mSlideVelZ = 0;
    s16 unk9E = 0;
    Vec mVel;
    f32 mForwardVel = 0;
    int turnCalls = 0;
    void getSlopeSlideAccele(f32* up, f32* down) { *up = *down = 0; }
    f32 getChangeAngleSpeed() { ++turnCalls; return 0; }
    __declspec(noinline) void slideProcess(f32, f32);
};
""" + body + r"""
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
    };
    TBGCheckData ground;
    struct Case { f32 x,z; s16 facing; bool backwards; };
    const Case cases[] = {
        {0,0.05f,0,false}, {0,0.05f,-32768,true},
        {0,-0.05f,0,true}, {0,-0.05f,-32768,false},
        {0.05f,0,16384,false}, {0.05f,0,-16384,true},
        {-0.05f,0,-16384,false}, {-0.05f,0,16384,true},
        {0.09f,0.09f,0,false}, {0.09f,0.09f,-32768,true},
        {0,0.05f,16384,false}, {0,0.05f,-16384,false},
        {0,0.1f,0,false}, {0,0.1f,-32768,true},
        {0,10,0,false}, {0,10,-32768,true},
    };
    for (const auto& c : cases) {
        TMario mario;
        mario.mGroundPlane = &ground;
        mario.mSlideVelX = c.x;
        mario.mSlideVelZ = c.z;
        mario.mFaceAngle.y = c.facing;
        mario.slideProcess(0,1);
        const f32 speed = std::sqrt(c.x*c.x+c.z*c.z);
        const f32 expected = c.backwards ? -speed : speed;
        check(std::fabs(mario.mForwardVel-expected)<0.000001f,
              "signed speed follows relative facing, including the low-speed branch");
        check(mario.mFaceAngle.y == c.facing && mario.mVel.x == c.x && mario.mVel.z == c.z && mario.mVel.y == 0,
              "direction classification does not alter facing or velocity components");
    }
    TMario zero;
    zero.mGroundPlane = &ground;
    zero.mFaceAngle.y = -32768;
    zero.slideProcess(0,1);
    check(zero.mForwardVel == 0 && std::isfinite(zero.mForwardVel), "stationary velocity remains finite zero");
    check(zero.mFaceAngle.y == -32768 && zero.turnCalls == 0, "stationary heading is not rotated");
    TMario capped;
    capped.mGroundPlane = &ground;
    capped.mSlideVelZ = 200;
    capped.slideProcess(0,1);
    check(capped.mSlideVelZ == 100 && capped.mForwardVel == 200, "original component cap and reported magnitude are retained");
    std::printf("%d slide-direction checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/slide-direction-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\slide_direction_test.cmd")
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/DTARGET_PC=1",
           str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
