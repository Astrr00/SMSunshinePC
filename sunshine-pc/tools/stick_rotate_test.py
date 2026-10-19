"""Compile the current stick-rotation routine in isolation at Strict's /O2 /Ob1.

The fixture supplies only the two fields read by this routine, not a game engine.
An optional source path permits a negative control against an unchanged source.
"""

from pathlib import Path
import os
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "sms/src/Player/MarioMove.cpp"
source = SOURCE.read_text(encoding="utf-8")
start = source.index("int TMario::checkStickRotate(int* outDir)")
end = source.index("\nvoid TMario::checkStickSmash()", start)
body = source[start:end]

fixture = r"""
#include <cstdio>
using s16 = short;
using u8 = unsigned char;
using f32 = float;
using BOOL = int;
struct TMario {
    s16* unk530;
    u8 unk534;
    __declspec(noinline) int checkStickRotate(int*);
};
""" + body + r"""
int main() {
    s16 increasing[] = {-30000, -16000, 0, 16000, 30000};
    s16 decreasing[] = {30000, 16000, 0, -16000, -30000};
    s16 straight[] = {0, 0, 0, 0, 0};
    s16 partial[] = {-30000, -16000, 0, 0, 0};
    s16 boundaries[] = {-24577, -24576, -8191, 8192, 0};
    s16 missing[] = {-24576, -8192, 8192, 24576, 0};
    struct Case { s16* angles; u8 count; int spin; int direction; const char* name; };
    Case cases[] = {
        {increasing, 5, 1, 2, "increasing full rotation"},
        {straight, 5, 0, 4, "straight after rotation"},
        {decreasing, 5, 1, 3, "decreasing full rotation"},
        {partial, 5, 0, 4, "three quadrants after rotation"},
        {increasing, 5, 1, 2, "rotation before empty history"},
        {straight, 0, 0, 4, "empty history after rotation"},
        {decreasing, 5, 1, 3, "rotation before single sample"},
        {straight, 1, 0, 4, "single sample after rotation"},
        {boundaries, 5, 1, 2, "original quadrant boundaries"},
        {missing, 5, 0, 4, "boundary samples miss two quadrants"},
        {increasing, 5, 1, 2, "rotation before short history"},
        {increasing, 4, 0, 4, "last sample is only the comparison endpoint"},
    };
    constexpr int count = sizeof(cases) / sizeof(cases[0]);
    int results[count] = {}, directions[count] = {};
    TMario mario{};
    // No intervening library calls: successive queries reuse the same stack.
    for (int i = 0; i < count; ++i) {
        mario.unk530 = cases[i].angles;
        mario.unk534 = cases[i].count;
        results[i] = mario.checkStickRotate(&directions[i]);
    }
    int failures = 0;
    for (int i = 0; i < count; ++i) {
        if (results[i] != cases[i].spin || directions[i] != cases[i].direction) {
            ++failures;
            std::printf("FAIL: %s: spin=%d direction=%d\n",
                        cases[i].name, results[i], directions[i]);
        }
    }
    std::printf("%d stick-rotation checks, %d failed\n", count, failures);
    return failures ? 1 : 0;
}
"""

build = ROOT / "build/stick-rotate-test"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "fixture.cpp"
cpp.write_bytes(fixture.encode("utf-8"))
compiler = Path(os.environ.get("LLVM_BIN", "")) / "clang-cl.exe"
if not compiler.is_file():
    compiler = shutil.which("clang-cl")
    if not compiler:
        raise SystemExit("clang-cl not found; set LLVM_BIN and run tools\\stick_rotate_test.cmd")
command = [str(compiler), "/nologo", "/O2", "/Ob1", "/EHsc", "/DTARGET_PC=1",
           str(cpp), f"/Fo:{build / 'fixture.obj'}", f"/Fe:{build / 'fixture.exe'}"]
subprocess.run(command, check=True, cwd=ROOT)
raise SystemExit(subprocess.run([str(build / "fixture.exe")], cwd=ROOT).returncode)
