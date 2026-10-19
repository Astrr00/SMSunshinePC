"""Test the production water-cache load and complete gauge-update body at /O2.

Uses the real nozzle/parameter declarations and constructor with inert parameter
registration/loading, pane and sound dependencies; no rendered HUD or audio.
An optional unchanged GCConsole2.cpp path supplies the negative control.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/GC2D/GCConsole2.cpp"
source = path.read_text(encoding="utf-8")
start = source.index("static inline void updateWaterGaugeFill(")
body = source[start:source.index("\n// fabricated", start)]
load = source.index("void TGCConsole2::loadAfter()")
start = source.index("unk28", source.index("TNozzleBase* nozzle", load))
expression = source[source.index("=", start)+1:source.index(";", start)].strip()

fixture = r"""
#include <Player/NozzleBase.hpp>
#include <cstddef>
#include <cstdio>

// Keep the real parameter construction/layout; omit registration and file I/O.
TBaseParam::TBaseParam(TParams*, u16 code, const char* paramName)
    : keyCode(code), name(paramName), next(nullptr) {}
u16 JDrama::TNameRef::calcKeyCode(const char*) { return 0; }
template<class T> void TParamT<T>::load(JSUMemoryInputStream&) {}
TNozzleBase::TNozzleBase(const char*, const char* prm, TWaterGun* fludd)
    : mEmitParams(prm), mFludd(fludd) {}
void TNozzleBase::init() {}
void TNozzleBase::movement(const TMarioControllerWork&) {}
void TNozzleBase::emitCommon(int, TWaterEmitInfo*) {}
void TNozzleBase::emit(int) {}
void TNozzleBase::animation(int) {}

namespace fixture {
struct Pane {
    u8 mAlpha = 0;
    void hide() {}
    void show() {}
    bool isInterpolatorAtZero() const { return false; }
    Pane* getPane() { return this; }
};
struct TWaterGun {
    enum { Spray=0, Rocket=1, Underwater=2, Hover=4, Turbo=5 };
    ::TNozzleBase* nozzle;
    s32 mCurrentWater;
    int mCurrentNozzle = Spray;
    ::TNozzleBase* getCurrentNozzle() const { return nozzle; }
};
struct TYoshi { s32 unkC=10, unk8=20; };
struct TMario { TWaterGun* mWaterGun; TYoshi* mYoshi=nullptr; };
TMario* gpMarioOriginal;
bool isMountedYoshi(TMario* mario) { return mario->mYoshi != nullptr; }
struct TGCConsole2 {
    Pane pane;
    Pane *unk2F8=&pane, *unk274=&pane, *unk288=&pane;
    Pane *unk278[3]={&pane,&pane,&pane}, *unk28C[4]={&pane,&pane,&pane,&pane};
    Pane* unk2A0[1]={&pane};
    bool unk45=false;
    int unk310=TWaterGun::Spray;
    u32 unk28=0;
    f32 unk2B8=0;
};
int soundCount=0;
struct Sound { bool allowed=true; bool gateCheck(u32 id) { return allowed && id==0x4807; } } sound;
Sound* SMSGetMSound() { return &sound; }
namespace MSoundSESystem { struct MSoundSE {
    static void startSoundSystemSE(u32 id, int, void*, int) {
        if (id==0x4807) ++soundCount;
    }
}; }
u32 initialWater(::TNozzleBase* nozzle) { return (
""" + expression + r"""
); }
""" + body + r"""
}

int main() {
    int checks=0, failures=0;
    auto check=[&](bool ok, const char* message) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
    };
    const size_t capacity = offsetof(TNozzleBase,mEmitParams)
        + offsetof(TNozzleBase::TEmitParams,mAmountMax) + offsetof(TParamRT<s32>,value);
    const size_t unrelated = offsetof(TNozzleBase,mEmitParams)
        + offsetof(TNozzleBase::TEmitParams,mEmitPow) + offsetof(TParamRT<f32>,name);
    std::printf("host capacity=0x%zx, emit-power name=0x%zx (PPC load=0xCC)\n",capacity,unrelated);
    static_assert(sizeof(void*)==8);
    check(capacity != 0xCC && unrelated+4 == 0xCC, "old load selects high word of unrelated name pointer");
    for (s32 maximum : {2100,10000,12000}) {
        TNozzleBase nozzle("fixture",nullptr,nullptr);
        nozzle.mEmitParams.mAmountMax.set(maximum);
        // This metadata must not affect the water-cache initialization.
        nozzle.mEmitParams.mEmitPow.name=nullptr;
        fixture::TWaterGun gun{&nozzle,maximum};
        fixture::TMario mario{&gun};
        fixture::gpMarioOriginal=&mario;
        fixture::TGCConsole2 console;
        console.unk28=fixture::initialWater(&nozzle);
        fixture::soundCount=0;
        fixture::sound.allowed=true;
        check(console.unk28 == maximum, "initial cache uses actual nozzle capacity");
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==0 && console.unk2B8==1 && console.pane.mAlpha==255,
              "already-full tank must not request a refill chime on first update");
        fixture::soundCount=0; // Test subsequent transitions independently of startup.
        gun.mCurrentWater=maximum/2;
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==0 && console.unk2B8==0.5f && console.pane.mAlpha==0x50,
              "partial tank retains original gauge fill without a chime");
        gun.mCurrentWater=maximum;
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==1 && console.unk2B8==1 && console.pane.mAlpha==255,
              "real refill to capacity requests one chime");
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==1, "remaining full does not repeat the chime");
        gun.mCurrentWater=0;
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==1 && console.unk2B8==0 && console.pane.mAlpha==0,
              "empty tank retains zero fill and no chime");
        fixture::sound.allowed=false;
        gun.mCurrentWater=maximum;
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==1 && console.unk28==maximum, "sound gate still suppresses refill request");
        fixture::sound.allowed=true;
        fixture::updateWaterGaugeFill(&console);
        check(fixture::soundCount==1, "suppressed chime is not replayed after the transition");
    }
    std::printf("%d water-gauge checks, %d failed\n",checks,failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/water-gauge-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\water_gauge_test.cmd")
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/w", "-Wno-register",
           "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude", "/I../sms/include",
           "/Iextern/aurora/include", str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
