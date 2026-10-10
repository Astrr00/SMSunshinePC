#!/usr/bin/env python3
"""Driver for tools/spc_bind_test.cpp (headless check of the EventWatcher SPCB host bindings).

Usage: python tools\\spc_bind_test.py <build name> [scene.szs ...]
  Default scene: sms/orig/GMSJ01/extracted/files/data/scene/dolpic0.szs. Build first with spc_bind_test.cmd <build name>.

Reads the 88 PTR_U32 bind lines of sms/src/System/EventWatcher.cpp (script name -> evFn) and the argument count of
each evFn (its verifyArgNum(N, &arg_num)), writes them as the table the native test reads, runs the test on every
scene and checks the output:
  - every table name a scene's .sb references is bound (no UNBOUND), and its handle is in 1..512;
  - every bound callback address lies inside this executable's image (a u32-truncated pointer would not);
  - the same name resolves to the same address in every scene;
  - the native test exits cleanly.
Callback calls with zero-valued arguments are counted per name; faults are listed, not hidden (they come from null game
globals in a headless process, so they are reported, not counted as binding failures).
Exit status 0 when all binding checks pass, 1 otherwise.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "sms", "src", "System", "EventWatcher.cpp")
SCENE_DIR = os.path.join(ROOT, "sms", "orig", "GMSJ01", "extracted", "files", "data", "scene")
DEFAULT_SCENES = [os.path.join(SCENE_DIR, "dolpic0.szs")]
HANDLE_MAX = 512


def parse_bindings(text):
    """Return [(script name, evFn, argc, has_verify)] for the PTR_U32 bind block."""
    out = []
    for name, fn in re.findall(r'bindSystemDataToSymbol\("(\w+)",\s*PTR_U32\(&(\w+)\)\)', text):
        m = re.search(r"static void\s+%s\(" % re.escape(fn), text)
        if not m:
            raise SystemExit("callback %s not defined in EventWatcher.cpp" % fn)
        body = text[m.end():text.find("\n}\n", m.end())]
        v = re.search(r"verifyArgNum\((\d+),\s*&arg_num\)", body)
        out.append((name, fn, int(v.group(1)) if v else 0, v is not None))
    return out


def run(build, scenes):
    exe = os.path.join(os.environ.get("TEMP", tempfile.gettempdir()), "sunshine_spc_bind_test", "spc_bind_test.exe")
    if not os.path.exists(exe):
        raise SystemExit("missing %s: run tools\\spc_bind_test.cmd %s first" % (exe, build))
    with open(SRC, encoding="utf-8", errors="replace") as f:
        text = f.read()
    binds = parse_bindings(text)
    if len(binds) != 88:
        raise SystemExit("expected 88 bind lines, found %d" % len(binds))
    no_verify = [fn for _, fn, _, has in binds if not has]
    table = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False)
    for name, _, argc, _ in binds:
        table.write("%s %d\n" % (name, argc))
    table.close()

    failures = []
    addr_by_name = {}
    faults = {}
    ok_calls = 0
    for scene in scenes:
        p = subprocess.run([exe, scene, table.name], capture_output=True, text=True)
        label = os.path.basename(scene)
        if p.returncode != 0:
            failures.append("%s: native test exited %d" % (label, p.returncode))
            continue
        image = None
        bound, unbound = set(), []
        for line in p.stdout.splitlines():
            parts = line.split()
            if not parts:
                continue
            kind = parts[0]
            if kind == "IMAGE":
                image = (int(parts[1], 16), int(parts[2], 16))
            elif kind == "UNBOUND":
                unbound.append("%s %s" % (parts[2], parts[3]))
            elif kind == "BOUND":
                _, _, name, _, handle, addr, in_img = parts
                handle, addr = int(handle), int(addr, 16)
                bound.add(name)
                if not (1 <= handle <= HANDLE_MAX):
                    failures.append("%s %s: handle %d out of range" % (label, name, handle))
                if in_img != "1" or image is None or not (image[0] <= addr < image[1]):
                    failures.append("%s %s: address %x outside image" % (label, name, addr))
                prev = addr_by_name.setdefault(name, addr)
                if prev != addr:
                    failures.append("%s: %s resolves to %x, earlier %x" % (label, name, addr, prev))
            elif kind == "CALL":
                if parts[3] == "ok":
                    ok_calls += 1
                else:
                    faults.setdefault(parts[2], set()).add(" ".join(parts[4:]))
        for u in unbound:
            failures.append("%s: unbound %s" % (label, u))
        print("%s: referenced %d of 88 names, bound %d, unbound %d"
              % (label, len(bound) + len(unbound), len(bound), len(unbound)))
    os.unlink(table.name)

    print("callbacks ok: %d" % ok_calls)
    if faults:
        print("callbacks faulting with zero args (game-state globals are null headless):")
        for name in sorted(faults):
            print("  %s %s" % (name, ",".join(sorted(faults[name]))))
    if no_verify:
        print("no verifyArgNum in: %s" % ", ".join(no_verify))
    if failures:
        print("FAIL")
        for f in failures:
            print("  " + f)
        return 1
    print("PASS: %d names resolved, all handles in image" % len(addr_by_name))
    return 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    return run(argv[1], argv[2:] or DEFAULT_SCENES)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
