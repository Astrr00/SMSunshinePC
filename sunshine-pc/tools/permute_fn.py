"""Run decomp-permuter on one function of an sms unit (GC mwcc 1.2.5, Windows).

usage: python permute_fn.py <unit> <mangled_sym> [--seconds 900] [--jobs 4]

Needs <permuter>/nonmatchings/<sym>/base.c (hand-written C-syntax function, see
platform-notes.md) and optional defs.h (#define aliases for typedef stubs).
Member functions: base.c is a free function whose first argument is the class pointer.
Writes prelude.h, compile.sh, settings.toml, target.o; then runs permuter.py.
Windowless + BELOW_NORMAL priority; the tree is killed after --seconds.
"""

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SMS = ROOT / "sms"
PERMUTER = ROOT.parent / "decomp-permuter"
FLAGS = 0x08000000 | 0x4000  # CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS
GIT_DIRS = ["C:/Program Files/Git/bin", "C:/Program Files/Git/usr/bin"]


def unit_entry(name):
    for u in json.loads((SMS / "objdiff.json").read_text(encoding="utf-8"))["units"]:
        if u["name"] == name:
            return u
    sys.exit(f"unit not found: {name}")


def mwcc_command(base_path):
    line = subprocess.check_output(
        ["ninja", "-t", "commands", base_path], cwd=SMS, text=True, creationflags=FLAGS
    ).strip().splitlines()[-1]
    toks = shlex.split(line.replace("\\", "/"))
    wrapper, compiler = toks[0], toks[1]
    rest = toks[toks.index("-nodefaults"):]
    flags, i = [], 0
    while i < len(rest):
        if rest[i] == "-MMD":
            i += 1
        elif rest[i] in ("-c", "-o"):
            i += 2
        else:
            flags.append(rest[i])
            i += 1
    return wrapper, compiler, flags


def write_compile_sh(d, wrapper, compiler, flags):
    q = " ".join(shlex.quote(f) for f in flags)
    (d / "compile.sh").write_text(
        f"""#!/usr/bin/env bash
# permuter compile step: compile.sh <candidate.c> -o <out.o>
# drops `typedef int X;` stubs (names come from defs.h), prepends the unit's includes.
set -e
D="$(cd "$(dirname "$0")" && pwd)"
FULL="$(mktemp --suffix=.cpp "$D/_full.XXXXXX")"  # per-process: permuter runs -j workers in parallel
trap 'rm -f "$FULL"' EXIT
{{ cat "$D/prelude.h" "$D/defs.h"; sed -E '/^typedef int [A-Za-z_0-9]+;$/d' "$(cygpath -u "$1")"; }} > "$FULL"
OUT="$(cygpath -wa "$3")"
FULL_W="$(cygpath -w "$FULL")"
cd "{SMS.as_posix()}"
"{wrapper}" "{compiler}" {q} -c "$FULL_W" -o "$OUT"
""",
        encoding="utf-8", newline="\n",
    )


def objdump_fn(args):
    """objdump_command hook for the permuter. target.o holds the whole unit: keep only <sym>'s block.
    Candidates come from a base.c free function, whose mangled name differs for members
    (changeStatusToGo__FP11TFenceWater vs changeStatusToGo__11TFenceWaterFv): keep the blocks named
    <base>__ and drop the unit's other functions (__sinit__, dtors)."""
    sym, objdump, rest = args[0], args[1], args[2:]
    base, is_target = sym.split("__")[0], Path(rest[-1]).name == "target.o"
    out = subprocess.run([objdump, *rest], capture_output=True, text=True, encoding="utf-8",
                         errors="replace", check=True, creationflags=FLAGS).stdout
    keep, on = [], False
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.*)>:$", line)
        if m:
            on = m.group(1) == sym if is_target else m.group(1).startswith(base + "__")
        if on:
            keep.append(line)
    print("\n".join(keep))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("unit")
    ap.add_argument("sym")
    ap.add_argument("--seconds", type=int, default=900)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--prepare-only", action="store_true")
    a = ap.parse_args()

    d = PERMUTER / "nonmatchings" / a.sym.split("__")[0]  # folder = readable name; func_name stays mangled
    if not (d / "base.c").is_file():
        sys.exit(f"missing {d / 'base.c'}")
    u = unit_entry(a.unit)

    src = (SMS / u["metadata"]["source_path"]).read_text(encoding="utf-8", errors="replace")
    (d / "prelude.h").write_text(
        "\n".join(l for l in src.splitlines() if l.startswith("#include")) + "\n",
        encoding="utf-8",
    )
    if not (d / "defs.h").is_file():
        (d / "defs.h").write_text("", encoding="utf-8")
    shutil.copyfile(SMS / u["target_path"], d / "target.o")
    wrapper, compiler, flags = mwcc_command(u["base_path"])
    write_compile_sh(d, wrapper, compiler, flags)
    objdump = (SMS / "build/binutils/powerpc-eabi-objdump.exe").as_posix()
    py, me = Path(sys.executable).as_posix(), Path(__file__).resolve().as_posix()
    (d / "settings.toml").write_text(
        f'name = "{a.sym}"\n'
        f'func_name = "{a.sym.split("__")[0]}"\n'  # C name in base.c
        'compiler_type = "base"\n'
        f"objdump_command = '\"{py}\" \"{me}\" --objdump {a.sym} \"{objdump}\" -dr -EB -mpowerpc -M broadway'\n",
        encoding="utf-8",
    )
    if a.prepare_only:
        return

    # permuter uses shutil.which("bash"); System32 bash is the WSL shim, so put Git Bash first.
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(GIT_DIRS) + os.pathsep + env["PATH"]
    log = d / "run.log"
    with open(log, "w", encoding="utf-8") as out:
        p = subprocess.Popen(
            [sys.executable, "permuter.py", str(d), "-j", str(a.jobs), "--stop-on-zero"],
            cwd=PERMUTER,
            stdout=out,
            stderr=subprocess.STDOUT,
            env=env,
            creationflags=FLAGS,
        )
        try:
            p.wait(timeout=a.seconds)
        except subprocess.TimeoutExpired:
            subprocess.run(
                ["taskkill", "/PID", str(p.pid), "/T", "/F"],
                capture_output=True,
                creationflags=FLAGS,
            )
            p.wait()
    print(f"log: {log}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--objdump":
        objdump_fn(sys.argv[2:])
    else:
        main()
