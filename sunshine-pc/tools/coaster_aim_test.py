"""Isolate the production coaster-camera query and FLUDD angle routine at /O2.

Uses the real coaster helper layout/getter, with inert camera/FLUDD dependencies.
An optional unchanged CameraMode.cpp path supplies the negative control.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
camera_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/Camera/CameraMode.cpp"
camera = camera_path.read_text(encoding="utf-8")
start = camera.index("bool CPolarSubCamera::isJetCoaster1stCamera() const")
query = camera[start:camera.index("\nbool CPolarSubCamera::isTalkCameraSpecifyMode", start)]
water = (ROOT.parent / "sms/src/Player/WaterGun.cpp").read_text(encoding="utf-8")
start = water.index("void TNozzleBase::calcGunAngle(")
angle = water[start:water.index("\nvoid TNozzleBase::movement(", start)]

fixture = r"""
#include <Camera/CameraJetCoaster.hpp>
#include <Camera/CameraMode.hpp>
#include <Player/MarioStatus.hpp>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

// No parameter loader or game boot: explicitly initialize each tested field.
TCameraJetCoaster::TCameraJetCoaster() = default;
struct CPolarSubCamera {
    int mMode;
    TCameraJetCoaster* unk2B8;
    struct { s16 mPitch; } mCurrentTarget;
    bool isJetCoaster1stCamera() const;
    bool isLButtonCamera() const { return mMode == CAMERA_MODE_L_NORMAL; }
};
struct TMarioGamePad { f32 mCompSPos[4] = {}; };
struct TMario { u32 mStatus = 0; TMarioGamePad* mGamePad; };
struct TWaterGun { TMario* mMario; };
struct TMarioControllerWork {};
template<class T> struct Param { T value; T get() const { return value; } };
struct TNozzleBase {
    struct {
        Param<f32> mRButtonMult{10000.0f};
        Param<s16> mLAngleBase{4096}, mLAngleMin{-8192}, mLAngleMax{8192};
        Param<f32> mLAngleChase{0.5f};
    } mEmitParams;
    TWaterGun* mFludd;
    s16 unk36E = 0;
    void calcGunAngle(const TMarioControllerWork&);
};
CPolarSubCamera* gpCamera;
TMario* gpMarioAddress;
""" + query + angle + r"""
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
    };
    TCameraJetCoaster coaster;
    coaster.unk0 = nullptr;
    CPolarSubCamera camera{CAMERA_MODE_JET_COASTER, &coaster, {6000}};
    gpCamera = &camera;
    TMarioGamePad pad;
    TMario mario{0, &pad}, other{0, &pad};
    gpMarioAddress = &mario;
    TWaterGun gun{&mario};
    TNozzleBase nozzle;
    nozzle.mFludd = &gun;
    for (u8 mode : {0, 1, 2, 3}) {
        coaster.mLButtonMode = mode;
        for (s16 offset : {0, 1, 256, -1}) {
            coaster.unk8 = offset;
            check(camera.isJetCoaster1stCamera() == bool(mode & 1),
                  "coaster query follows its mode bit, independent of angle bytes");
            nozzle.unk36E = 0;
            nozzle.calcGunAngle({});
            check(nozzle.unk36E == ((mode & 1) ? 6000 : -2048),
                  "FLUDD uses camera pitch only in coaster first-person mode");
        }
    }
    camera.unk2B8 = nullptr;
    check(!camera.isJetCoaster1stCamera(), "absent coaster helper stays false");
    camera.unk2B8 = &coaster;
    camera.mMode = CAMERA_MODE_FOLLOW;
    coaster.mLButtonMode = 1;
    coaster.unk8 = 1;
    check(!camera.isJetCoaster1stCamera(), "ordinary follow camera stays false");
    nozzle.unk36E = 0;
    nozzle.calcGunAngle({});
    check(nozzle.unk36E == -2048, "ordinary follow-camera aiming is unchanged");
    camera.mMode = CAMERA_MODE_L_NORMAL;
    camera.unk2B8 = nullptr;
    nozzle.unk36E = 0;
    nozzle.calcGunAngle({});
    check(nozzle.unk36E == 6000, "ordinary first-person camera retains pitch aiming");
    gun.mMario = &other;
    nozzle.unk36E = 0;
    nozzle.calcGunAngle({});
    check(nozzle.unk36E == -2048, "another actor's FLUDD does not copy player-camera pitch");
    std::printf("coaster mode offset=%zu, old byte offset=12\n", offsetof(TCameraJetCoaster, mLButtonMode));
    std::printf("%d coaster-aim checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/coaster-aim-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\coaster_aim_test.cmd")
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/w", "-Wno-register",
           "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude", "/I../sms/include",
           "/Iextern/aurora/include", str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
