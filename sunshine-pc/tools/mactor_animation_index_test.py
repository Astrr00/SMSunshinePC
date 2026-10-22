"""Check the real animation-index getter and the production blend-stop caller.

Optional argument: an old MActorData.hpp snapshot for the ASan negative control.
The fixture stubs frame/blend bookkeeping; it does not load or render a model.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SMS = ROOT.parent / "sms"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("header", nargs="?", type=Path, default=SMS / "include/M3DUtil/MActorData.hpp")
args = parser.parse_args()


def function(path, signature):
    source = path.read_text(encoding="utf-8")
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r"""
#include <cstdio>
#include <climits>
#include <cstdint>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
""" + f'#include "{args.header.resolve().as_posix()}"\n'
fixture += function(SMS / "src/M3DUtil/MActorData.cpp", "MActorAnmDataBase::MActorAnmDataBase(")
fixture += r"""
struct FrameStub {
    float frame = 12.f;
    int inits = 0;
    float getFrame() const { return frame; }
    void init(short) { ++inits; }
    void setAttribute(unsigned char) {}
    void setRate(float) {}
};
struct BlendStub {
    J3DAnmBase* old = nullptr;
    J3DAnmBase* next = nullptr;
    float oldFrame = 0.f;
    int keeps = 0, sets = 0;
    void keepCurAnm(J3DAnmBase* value, float frame) {
        old = value; oldFrame = frame; ++keeps;
    }
    void setNewAnm(J3DAnmBase* value) { next = value; ++sets; }
};
static float SMSGetAnmFrameRate() { return 1.f; }
enum { MACTOR_MTX_CALC_MOTION_BLEND = 2 };
struct MActorAnmBck {
    int unk0 = -1;
    FrameStub unk4;
    J3DAnmBase* unk24 = nullptr;
    BlendStub* unk34;
    MActorAnmDataEach<J3DAnmBase>* data;
    int type = MACTOR_MTX_CALC_MOTION_BLEND;
    int getMtxCalcType() const { return type; }
    MActorAnmDataEach<J3DAnmBase>* getData() { return data; }
    __declspec(noinline) void setAnmFromIndex(int, u16*);
"""
fixture += function(SMS / "include/M3DUtil/MActorAnm.hpp", "bool setFrameCtrl(")
fixture += "\n};\n"
fixture += function(SMS / "src/M3DUtil/MActorAnm.cpp", "void MActorAnmBck::setAnmFromIndex(")
fixture += r"""
int main() {
    int checks = 0, failed = 0;
    auto check = [&](bool ok, const char* label) {
        ++checks;
        if (!ok) { ++failed; std::printf("FAIL: %s\n", label); }
    };
    MActorAnmDataEach<J3DAnmBase> data(2);
    data.mAnimations = new J3DAnmBase*[2];
    // Identity tokens are never dereferenced: every blend call below disables
    // animation, so the real frame-controller setter returns before reading it.
    auto first = reinterpret_cast<J3DAnmBase*>(uintptr_t(0x10000));
    auto last = reinterpret_cast<J3DAnmBase*>(uintptr_t(0x20000));
    data.mAnimations[0] = first;
    data.mAnimations[1] = last;
    BlendStub blend;
    blend.next = first;
    MActorAnmBck actor;
    actor.data = &data;
    actor.unk34 = &blend;
    actor.unk0 = 0;
    actor.setAnmFromIndex(-1, nullptr);
    check(actor.unk0 == -1, "disabled index retained");
    check(blend.old == first && blend.keeps == 1, "old animation retained");
    check(blend.oldFrame == 12.f, "old frame retained");
    check(blend.next == nullptr && blend.sets == 1, "disabled blend gets null");
    check(actor.unk4.inits == 0, "disable does not reset frame controller");
    actor.setAnmFromIndex(-1, nullptr);
    check(blend.keeps == 1 && blend.sets == 2, "repeated disable skips old lookup");
    actor.type = 0;
    actor.setAnmFromIndex(-1, nullptr);
    check(blend.sets == 2, "ordinary disable does not touch blend state");
    check(data.getAnmPtr(0) == first, "first animation");
    check(data.getAnmPtr(1) == last, "last animation");
    check(data.getAnmPtr(2) == nullptr, "end boundary");
    check(data.getAnmPtr(INT_MAX) == nullptr, "large positive index");
    check(data.getAnmPtr(-1) == nullptr, "disable sentinel");
    check(data.getAnmPtr(-2) == nullptr, "other negative index");
    check(data.getAnmPtr(INT_MIN) == nullptr, "minimum signed index");
    delete[] data.mAnimations;
    delete[] data.mAnmNames;
    delete[] data.mAnmKeyCodes;
    MActorAnmDataEach<J3DAnmBase> empty(0);
    empty.mAnimations = nullptr;
    check(empty.getAnmPtr(0) == nullptr, "empty table");
    check(empty.getAnmPtr(-1) == nullptr, "empty disable sentinel");
    delete[] empty.mAnmNames;
    delete[] empty.mAnmKeyCodes;
    std::printf("%d animation-index checks, %d failed; ASan enabled\n", checks, failed);
    return failed ? 1 : 0;
}
"""
build = ROOT / "build/mactor-animation-index-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_text(fixture, encoding="utf-8")
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    found = shutil.which("clang-cl")
    if not found:
        raise SystemExit("clang-cl not found; run tools\\mactor_animation_index_test.cmd")
    compiler = Path(found)
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/Zi",
           "/fsanitize=address", "/DTARGET_PC=1", "/FIinclude/pc_compat.h", "/Iinclude",
           f"/I{SMS / 'include'}", f"/I{ROOT / 'extern/aurora/include'}",
           "-Wno-register", "-Wno-unknown-pragmas", str(cpp),
           f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}", "/link", "/DEBUG"]
print(f"Testing animation indices from {args.header.resolve()}", flush=True)
subprocess.run(command, check=True, cwd=ROOT)
env = os.environ.copy()
runtime_dirs = list((compiler.parent.parent / "lib/clang").glob("*/lib/windows"))
env["PATH"] = os.pathsep.join(map(str, runtime_dirs)) + os.pathsep + env.get("PATH", "")
symbolizer = compiler.parent / "llvm-symbolizer.exe"
if symbolizer.is_file():
    env["ASAN_SYMBOLIZER_PATH"] = str(symbolizer)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT, env=env).returncode)
