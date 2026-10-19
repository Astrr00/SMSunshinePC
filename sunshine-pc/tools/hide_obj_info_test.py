"""Test the production hide-info helper with real JSU streams at /O2 /Ob1.

An optional MapObjLib.cpp snapshot supplies the negative control. Pattern-filled
locals make the old missing-float-field failure deterministic. This isolates
record decoding; it does not instantiate actors or load a scene.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SMS = ROOT.parent / "sms"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", nargs="?", type=Path, default=SMS / "src/MoveBG/MapObjLib.cpp")
args = parser.parse_args()
source = args.source.read_text(encoding="utf-8")
start = source.index("void TMapObjBase::loadHideObjInfo(")
opening = source.index("{", start)
depth = 1
end = opening + 1
while depth:
    if source[end] == "{":
        depth += 1
    elif source[end] == "}":
        depth -= 1
    end += 1
body = source[start:end]

fixture = r"""
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
// Compile the unchanged stream implementations, including their real EOF and
// big-endian typed-read behavior, rather than a test-local stream substitute.
"""
for name in ("JSUInputStream.cpp", "JSUOutputStream.cpp", "JSUMemoryStream.cpp"):
    fixture += f'#include "{(SMS / "src/JSystem/JSupport" / name).as_posix()}"\n'
fixture += r"""
struct TMapObjBase {
    __declspec(noinline) static void loadHideObjInfo(JSUMemoryInputStream&, s32*, f32*, f32*, s32*);
};
""" + body + r"""

static void appendWord(std::vector<u8>& out, u32 word) {
    out.push_back(static_cast<u8>(word >> 24));
    out.push_back(static_cast<u8>(word >> 16));
    out.push_back(static_cast<u8>(word >> 8));
    out.push_back(static_cast<u8>(word));
}
static void appendFloat(std::vector<u8>& out, f32 value) {
    u32 word;
    std::memcpy(&word, &value, sizeof(word));
    appendWord(out, word);
}
int main() {
    static_assert(sizeof(s32) == 4 && sizeof(f32) == 4);
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name, const char* field) {
        ++checks;
        if (!ok) {
            ++failures;
            std::printf("FAIL: %s: %s\n", name, field);
        }
    };
    struct Case {
        const char* name;
        int fields;
        s32 event;
        f32 speed, ySpeed;
        s32 timer;
        f32 expectedSpeed, expectedY;
        s32 expectedTimer;
        bool trailingWord = false;
    };
    const Case cases[] = {
        {"empty steam tail", 0, 117, 50, 12, 3, 0, 0, 1200},
        {"EOF after event", 1, 117, 50, 12, 3, 0, 0, 1200},
        // The same float local is reused: a failed second float read retains
        // the first float, matching the real stream's existing behavior.
        {"EOF after speed", 2, 117, 50, 12, 3, 3, 50, 1200},
        {"EOF after negative Y", 3, 117, 50, -1, 3, 3, 20, 1200},
        {"complete positive timer", 4, 117, 50, 12, 3, 3, 12, 30},
        {"complete negative Y and timer", 4, 95, -50, -1, -3, -3, 20, 1200},
        {"complete zero timer", 4, 0, 0, 0, 0, 0, 0, 1200},
        {"complete negative event retained", 4, -1, 25, 7, 120, 1.5f, 7, 1200},
        {"following word is untouched", 4, 94, 100, 18, 10, 6, 18, 100, true},
    };
    for (const auto& c : cases) {
        std::vector<u8> bytes;
        if (c.fields >= 1) appendWord(bytes, static_cast<u32>(c.event));
        if (c.fields >= 2) appendFloat(bytes, c.speed);
        if (c.fields >= 3) appendFloat(bytes, c.ySpeed);
        if (c.fields >= 4) appendWord(bytes, static_cast<u32>(c.timer));
        if (c.trailingWord) appendWord(bytes, 0x12345678);
        JSUMemoryInputStream stream(bytes.empty() ? nullptr : bytes.data(), static_cast<s32>(bytes.size()));
        s32 event = -1, timer = -999;
        f32 speed = -999, ySpeed = -999;
        TMapObjBase::loadHideObjInfo(stream, &event, &speed, &ySpeed, &timer);
        check(event == (c.fields ? c.event : -1), c.name, "event ID / failed-read preset");
        check(std::isfinite(speed) && std::fabs(speed - c.expectedSpeed) < 0.00001f, c.name, "scaled speed");
        check(std::isfinite(ySpeed) && ySpeed == c.expectedY, c.name, "Y speed / negative fallback");
        check(timer == c.expectedTimer, c.name, "timer scaling / fallback");
        check(stream.getPosition() == c.fields * 4, c.name, "consumed whole fields only");
        check(stream.isGood() == (c.fields == 4), c.name, "EOF state");
        if (c.trailingWord)
            check(stream.readU32() == 0x12345678, c.name, "following word value");
        std::printf("CASE %s: event=%ld speed=%g y=%g timer=%ld\n", c.name, event, speed, ySpeed, timer);
    }
    std::printf("%d hide-info checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
"""
build = ROOT / "build/hide-obj-info-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_text(fixture, encoding="utf-8")
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\hide_obj_info_test.cmd")
command = [str(compiler), "/nologo", "/std:c++20", "/O2", "/Ob1", "/EHsc", "/DTARGET_PC=1",
           "-ftrivial-auto-var-init=pattern", f"/I{SMS / 'include'}", str(cpp),
           f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
print(f"Testing production helper from {args.source.resolve()}", flush=True)
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
