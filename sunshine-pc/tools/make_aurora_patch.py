"""Write the diff between the Aurora series result (patches 0001..N-1) and the working tree as a new patch.

  python tools/make_aurora_patch.py <patch file name> <file relative to extern/aurora> [...]

Run it after editing extern/aurora; the new patch is the last one of the series. Needs commit 50e5be3 in the aurora repo.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
AU = ROOT / "extern" / "aurora"


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    name, files = sys.argv[1], sys.argv[2:]
    pdir = ROOT / "patches" / "aurora"
    out_path = pdir / name
    patches = [p for p in sorted(pdir.glob("0*.patch")) if p.name != name]
    tmp = pathlib.Path(tempfile.mkdtemp())
    try:
        touched = set(files)
        for p in patches:
            for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
                m = re.match(r"^diff --git a/(\S+) b/(\S+)", line)
                if m:
                    touched.add(m.group(2))
        for f in touched:
            r = subprocess.run(["git", "-C", str(AU), "cat-file", "blob", "50e5be3:" + f], capture_output=True)
            if r.returncode == 0:
                (tmp / f).parent.mkdir(parents=True, exist_ok=True)
                (tmp / f).write_bytes(r.stdout)
        for p in patches:
            r = subprocess.run(["git", "apply", str(p)], cwd=tmp, capture_output=True)
            if r.returncode != 0:
                sys.exit(f"{p.name} does not apply: {r.stderr.decode(errors='replace')}")
        out = []
        for f in files:
            old = tmp / f
            new = AU / f
            if not old.exists():
                old = pathlib.Path("/dev/null")
            r = subprocess.run(["git", "diff", "--no-index", "--no-color", "--", str(old), str(new)],
                               capture_output=True, cwd=tmp)
            text = r.stdout.decode("utf-8")
            # git prints the absolute paths of both sides: keep the hunks and write our own header
            hunks = text[text.index("@@"):] if "@@" in text else ""
            if hunks:
                text = "\n".join([f"diff --git a/{f} b/{f}", f"--- a/{f}", f"+++ b/{f}", ""]) + hunks
            else:
                text = ""
            out.append(text)
        out_path.write_text("".join(out), encoding="utf-8", newline="\n")
        print(out_path, len("".join(out).splitlines()), "lines")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


main()
