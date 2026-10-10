#!/usr/bin/env python3
"""Apply or verify the Aurora patch series in patches/aurora on extern/aurora.

The series is built on upstream commit 50e5be3. Each run classifies extern/aurora:
  applied   every touched file matches the result of the series -> nothing to do (exit 0)
  pristine  every touched file matches upstream                 -> apply, or exit 2 with --check
  mixed     neither; files differ from both                     -> refuse (exit 1)

Usage: python tools/apply-aurora-patches.py [--check] [--aurora DIR] [--patches DIR]
"""
import argparse
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

UPSTREAM = "50e5be3"
ROOT = pathlib.Path(__file__).resolve().parent.parent


def git_blob(aurora, path):
    r = subprocess.run(["git", "-C", str(aurora), "cat-file", "blob", f"{UPSTREAM}:{path}"], capture_output=True)
    if r.returncode == 0:
        return r.stdout
    if subprocess.run(["git", "-C", str(aurora), "cat-file", "-e", f"{UPSTREAM}^{{commit}}"],
                      capture_output=True).returncode != 0:
        sys.exit(f"error: upstream {UPSTREAM} is not in {aurora}; fetch the aurora repo first")
    return None  # file does not exist upstream (added by the series)


def read(path):
    return path.read_bytes() if path.exists() else None


def expected_state(touched, patches, base):
    """Run the series in a scratch dir holding only upstream copies of touched files."""
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="aurora-series-"))
    try:
        for p, data in base.items():
            if data is not None:
                (tmp / p).parent.mkdir(parents=True, exist_ok=True)
                (tmp / p).write_bytes(data)
        for patch in patches:
            r = subprocess.run(["git", "apply", str(patch)], cwd=tmp, capture_output=True)
            if r.returncode != 0:
                sys.exit(f"error: {patch.name} does not apply to upstream {UPSTREAM}: "
                         f"{r.stderr.decode(errors='replace').strip()}")
        return {p: read(tmp / p) for p in touched}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="report state only; exit 2 if pristine")
    ap.add_argument("--aurora", default=str(ROOT / "extern" / "aurora"))
    ap.add_argument("--patches", default=str(ROOT / "patches" / "aurora"))
    args = ap.parse_args()

    aurora = pathlib.Path(args.aurora)
    patches = sorted(pathlib.Path(args.patches).glob("[0-9][0-9][0-9][0-9]-*.patch"))
    if not patches:
        sys.exit(f"error: no patches in {args.patches}")

    touched = []
    for patch in patches:
        for line in patch.read_bytes().decode("latin-1").splitlines():
            m = re.match(r"diff --git a/(\S+) b/(\S+)$", line)
            if m and m.group(1) not in touched:
                touched.append(m.group(1))

    base = {p: git_blob(aurora, p) for p in touched}
    current = {p: read(aurora / p) for p in touched}
    expected = expected_state(touched, patches, base)

    if all(current[p] == expected[p] for p in touched):
        print(f"applied: {len(patches)} patches, {len(touched)} files already match")
        return 0
    if all(current[p] == base[p] for p in touched):
        if args.check:
            print(f"pristine: {len(patches)} patches not applied to {aurora}")
            return 2
        for patch in patches:
            subprocess.run(["git", "-C", str(aurora), "apply", str(patch)], check=True)
        print(f"applied: {len(patches)} patches to {aurora}")
        return 0
    bad = [p for p in touched if current[p] not in (base[p], expected[p])]
    print(f"mixed: {len(bad)} files match neither upstream {UPSTREAM} nor the series; refusing:")
    for p in bad:
        print(f"  {p}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
