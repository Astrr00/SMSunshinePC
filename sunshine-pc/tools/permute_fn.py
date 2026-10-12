"""Run decomp-permuter on one function of an sms unit (GC mwcc 1.2.5, Windows).

usage (from sunshine-pc): python tools/permute_fn.py <unit> <mangled_sym> [--seconds 900] [--jobs 4]
                          [--scope prefix|unit|includes] [--nth N] [--regen] [--prepare-only]

base.c is generated automatically from the unit's .cpp (permute_extract.py): the function body is
extracted, `Cls::f(args)` becomes the free function `f(Cls* this_, args)`, C++ names (`A::B`, `T<..>`)
become #define aliases (defs.h). compile.sh wraps every candidate back into C++ (`Cls::f`, `this`) and
compiles it with the unit's real mwcc flags. A hand-edited base.c is kept unless --regen is given.
--scope: prefix = unit source above the function (default, keeps earlier inline candidates),
unit = whole unit with the function replaced (slowest, most faithful), includes = only the #includes.
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

sys.path.insert(0, str(Path(__file__).resolve().parent))
import permute_extract as px

ROOT = Path(__file__).resolve().parents[2]
SMS = ROOT / "sms"
PERMUTER = next(
    (p for p in (ROOT.parent / "decomp-permuter", ROOT.parent / "SunshineHandoff/extra-repos/decomp-permuter")
     if p.is_dir()),
    ROOT.parent / "decomp-permuter",
)
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


def write_compile_sh(d, wrapper, compiler, flags, py, me):
    q = " ".join(shlex.quote(f) for f in flags)
    (d / "compile.sh").write_text(
        f"""#!/usr/bin/env bash
# permuter compile step: python permute_fn.py wraps the candidate (typedef stubs dropped, C -> C++ member) 
set -e
D="$(cd "$(dirname "$0")" && pwd)"
FULL="$(mktemp --suffix=.cpp "$D/_full.XXXXXX")"  # per-process: permuter runs -j workers in parallel
trap 'rm -f "$FULL"' EXIT
"{py}" "{me}" --wrap "$(cygpath -w "$D")" "$(cygpath -w "$1")" > "$FULL"
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
    names = re.findall(r"(?m)^[0-9a-f]+ <(.*)>:$", out)
    want = (lambda n: n == sym) if sym in names else (lambda n: n.startswith(base + "__"))
    keep, on = [], False
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.*)>:$", line)
        if m:
            on = want(m.group(1))
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
    ap.add_argument("--scope", choices=["prefix", "unit", "includes"], default="prefix")
    ap.add_argument("--nth", type=int, default=0, help="which definition if the name is overloaded")
    ap.add_argument("--regen", action="store_true", help="regenerate base.c (overwrites hand edits)")
    a = ap.parse_args()

    name = a.sym.split("__")[0]
    d = PERMUTER / "nonmatchings" / re.sub(r"\W+", "_", a.sym)[:80]
    d.mkdir(parents=True, exist_ok=True)
    u = unit_entry(a.unit)
    srcpath = SMS / u["metadata"]["source_path"]
    src = srcpath.read_text(encoding="utf-8", errors="replace")

    cls, const, hit = px.find_function(src, a.sym, a.nth)
    if a.regen or not (d / "base.c").is_file():
        defs = px.make_base_c(src, cls, const, hit, name, d, PERMUTER)
        (d / "defs.h").write_text("".join(f"#define {k} {v}\n" for k, v in defs.items()), encoding="utf-8")
    (d / "meta.json").write_text(json.dumps({"name": name, "cls": cls, "const": const}), encoding="utf-8")
    _, o, c = hit
    hstart = px.header_start(src, o)
    if a.scope == "includes":
        head, tail = "\n".join(l for l in src.splitlines() if l.startswith("#include")) + "\n", ""
    else:
        head, tail = src[:hstart], (src[c:] if a.scope == "unit" else "")
    (d / "head.h").write_text(head, encoding="utf-8")
    (d / "tail.h").write_text(tail, encoding="utf-8")
    shutil.copyfile(SMS / u["target_path"], d / "target.o")
    py, me = Path(sys.executable).as_posix(), Path(__file__).resolve().as_posix()
    wrapper, compiler, flags = mwcc_command(u["base_path"])
    write_compile_sh(d, wrapper, compiler, flags + ["-i", srcpath.parent.relative_to(SMS).as_posix()], py, me)
    objdump = (SMS / "build/binutils/powerpc-eabi-objdump.exe").as_posix()
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
    for f in d.glob("_full.*"):  # temp files of workers killed mid-compile
        f.unlink(missing_ok=True)
    print(f"log: {log}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--objdump":
        objdump_fn(sys.argv[2:])
    elif len(sys.argv) > 1 and sys.argv[1] == "--wrap":
        px.wrap(Path(sys.argv[2]), sys.argv[3])
    else:
        main()
