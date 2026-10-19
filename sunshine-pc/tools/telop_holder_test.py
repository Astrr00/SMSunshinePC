"""Compile the production news selector with real host holder offsets at /O2.

Models actor storage using real header offsets; no actor constructor, game boot,
rendering or message timing. An optional GCConsole2.cpp supplies the old control.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/GC2D/GCConsole2.cpp"
source = path.read_text(encoding="utf-8")
start = source.index("void TGCConsole2::checkChangeTelopArray()")
selector = source[start:source.index("\nbool TGCConsole2::processAppearJet(", start)]
start = source.index("static u32 scDolpicNewsDolpic0[]")
arrays = source[start:source.index("\n// rogue include", start)]
header = (ROOT.parent / "sms/include/Strategic/TakeActor.hpp").read_text(encoding="utf-8")
start = header.index("TTakeActor* getHolder()")
getter = header[start:header.index("\n", start)]

fixture = r"""
#include <Strategic/TakeActor.hpp>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace fixture {
// Layout-backed storage, without the real actor's vtable/constructor dependencies.
struct alignas(::TTakeActor) TTakeActor {
    u8 prefix[offsetof(::TTakeActor, mHolder)];
    TTakeActor* mHolder;
""" + getter + r"""
};
static_assert(offsetof(TTakeActor, mHolder) == offsetof(::TTakeActor, mHolder));
struct Director { u8 mMap=1, unk7D=1; } director;
Director* gpMarDirector=&director;
struct TFlagManager {
    static TFlagManager* smInstance;
    bool one=false, two=false, nozzle=false, unlock=false;
    int event=0, shines=0;
    bool getBool(u32 id) { return id==0x50001 ? one : id==0x50002 ? two : unlock; }
    bool getNozzleRight(int, int) { return nozzle; }
    int getFlag(u32 id) { return id==0x60003 ? event : shines; }
} flags;
TFlagManager* TFlagManager::smInstance=&flags;
struct TGCConsole2 {
    void* unkC4;
    u32* unk570=nullptr;
    u8 unk558=0;
    void checkChangeTelopArray();
};
""" + arrays + selector + r"""
}
int main() {
    int checks=0, failures=0;
    auto check=[&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n",name); }
    };
    using namespace fixture;
    std::printf("host holder=0x%zx, collision count=0x%zx, capacity=0x%zx; PPC read=0x68\n",
        offsetof(::TTakeActor,mHolder), offsetof(::THitActor,mColCount), offsetof(::THitActor,mColCapacity));
    static_assert(offsetof(::TTakeActor,mHolder) > 0x68+sizeof(u32));
    static_assert(offsetof(::THitActor,mColCount)==0x68 && offsetof(::THitActor,mColCapacity)==0x6A);
    fixture::TTakeActor peach{}, carrier{};
    TGCConsole2 console{&peach};
    // NPC initialization supports capacity0 (alone actor) or capacity2 (managed).
    struct Collision { u16 count, capacity; };
    for (const Collision c : {Collision{0,0}, {0,2}, {1,2}, {2,2}}) {
        std::memcpy(reinterpret_cast<u8*>(&peach)+offsetof(::THitActor,mColCount),&c,sizeof(c));
        for (bool carried : {false,true}) {
            peach.mHolder=carried ? &carrier : nullptr;
            console.unk570=scDolpicNewsDolpic0;
            console.unk558=7;
            console.checkChangeTelopArray();
            auto expected=carried ? scDolpicNewsDolpic1 : nullptr;
            check(console.unk570==expected, "Peach news follows holder, independent of unrelated host bytes");
            check(console.unk558==0, "changed news array resets its current item");
            console.unk558=5;
            console.checkChangeTelopArray();
            check(console.unk558==5, "unchanged news array preserves its current item");
        }
    }
    struct Scenario { u8 scenario; u32* expected; };
    for (const Scenario c : {Scenario{0,scDolpicNewsDolpic0}, {2,scDolpicNewsDolpic10},
                            {6,scDolpicNewsDolpic6}, {7,scDolpicNewsDolpic7},
                            {9,scDolpicNewsDolpic9}, {3,nullptr}}) {
        director.unk7D=c.scenario;
        console.checkChangeTelopArray();
        check(console.unk570==c.expected, "other Delfino scenario news stays unchanged");
    }
    director.unk7D=5;
    for (int mask=0; mask<4; ++mask) {
        flags.one=mask&1; flags.two=mask&2;
        u32* expected[]={scDolpicNewsDolpic5_1,scDolpicNewsDolpic5_2,
                         scDolpicNewsDolpic5_3,scDolpicNewsDolpic5_4};
        console.checkChangeTelopArray();
        check(console.unk570==expected[mask], "scenario5 flag combinations stay unchanged");
    }
    director.unk7D=8;
    flags={};
    console.checkChangeTelopArray();
    check(console.unk570==scDolpicNewsDolpic8_1, "scenario8 default news remains available");
    flags.shines=20;
    console.checkChangeTelopArray();
    check(console.unk570==scDolpicNewsDolpic8_7, "scenario8 Shine threshold remains intact");
    director.mMap=2;
    console.checkChangeTelopArray();
    check(console.unk570==nullptr, "other worlds have no Delfino news array");
    std::printf("%d telop-holder checks, %d failed\n",checks,failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/telop-holder-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\telop_holder_test.cmd")
subprocess.run([str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/w", "-Wno-register",
                "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude", "/I../sms/include",
                "/Iextern/aurora/include", str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"],
               check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
