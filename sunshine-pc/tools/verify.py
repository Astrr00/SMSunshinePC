"""Ordered verification steps for the Sunshine PC port. Run via tools/verify.cmd.

Usage: python tools/verify.py [--build] [--matching] [--full] [--boot-secs 45] [--tile 2800,520]

Default (cheap, no compiles): TEMP-DEBUG grep, Aurora patch presence (+ `apply-aurora-patches.py --check` count vs the
tracked series, clean patches/aurora), matching SHA of mario.dol, line endings of the files changed since origin/main
(fallback @{u}, HEAD~1) incl. staged/unstaged edits, headless tests, boot smoke on build/strict/sunshine.exe (SKIP when
it is not there).
  --build     also (re)build build/strict via tools/_strict.cmd and count unresolved symbols (LNK2019).
  --matching  lead only: runs `ninja -k 0` in sms/ first (the only place plain ninja is allowed). Needs MSVC env.
  --full      also runs probe-units over the decomp sources (slow; writes tools/probe-results.json).
Each step prints PASS / FAIL / SKIP with timing. Exit 1 if any step FAILs.
"""
import argparse
import hashlib
import os
import re
import subprocess
import sys
import time

here = os.path.dirname(os.path.abspath(__file__))
pc = os.path.abspath(os.path.join(here, ".."))
root = os.path.abspath(os.path.join(pc, ".."))
sms = os.path.join(root, "sms")
logdir = os.path.join(pc, "build", "verify")
disc = os.path.join(sms, "orig", "GMSJ01", "Super Mario Sunshine (Japan).rvz")
strict_exe = os.path.join(pc, "build", "strict", "sunshine.exe")

TEMP_DEBUG_ROOTS = [os.path.join(pc, "src"), os.path.join(pc, "include"), os.path.join(pc, "extern", "aurora", "lib"),
                    os.path.join(pc, "extern", "aurora", "include"), os.path.join(sms, "src")]
SRC_EXT = (".c", ".cc", ".cpp", ".h", ".hpp", ".inl")
FATAL = "*** fatal exception"

results = []  # (name, status, seconds, detail)


def step(name):
    def wrap(fn):
        def run(*a, **kw):
            t0 = time.time()
            try:
                status, detail = fn(*a, **kw)
            except Exception as e:  # a crashing step is a FAIL, not a silent skip
                status, detail = "FAIL", f"exception: {e}"
            dt = time.time() - t0
            results.append((name, status, dt, detail))
            print(f"{status:4}  {name:<20} {dt:6.1f}s  {detail}", flush=True)
            return status
        return run
    return wrap


def run_cmd(args, cwd, log_name):
    """Runs a command, writes its output to build/verify/<log_name>.log, returns (exit code, output text)."""
    os.makedirs(logdir, exist_ok=True)
    p = subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors="replace")
    out = p.stdout + p.stderr
    with open(os.path.join(logdir, log_name + ".log"), "w", encoding="utf8") as f:
        f.write(out)
    return p.returncode, out


def last_line(text):
    lines = [ln for ln in text.splitlines() if ln.strip()]
    return lines[-1].strip() if lines else "(no output)"


def git(*args):
    """git output (bytes) run in the repo root, or None on a non-zero exit."""
    p = subprocess.run(["git", *args], cwd=root, capture_output=True)
    return p.stdout if p.returncode == 0 else None


def eol_class(data):
    """LF / CRLF / mixed; None for a missing or binary (NUL byte) file or one without line breaks."""
    if not data or b"\0" in data:
        return None
    crlf = data.count(b"\r\n")
    lf = data.count(b"\n") - crlf
    return "mixed" if crlf and lf else "CRLF" if crlf else "LF" if lf else None


@step("temp-debug")
def temp_debug():
    hits = []
    for top in TEMP_DEBUG_ROOTS:
        for dp, dn, fn in os.walk(top):
            dn[:] = [d for d in dn if d not in ("build", ".git")]
            for name in fn:
                if not name.endswith(SRC_EXT):
                    continue
                path = os.path.join(dp, name)
                with open(path, encoding="utf8", errors="replace") as f:
                    for n, line in enumerate(f, 1):
                        if "TEMP-DEBUG" in line:
                            hits.append(f"{os.path.relpath(path, root)}:{n}")
    if hits:
        return "FAIL", f"{len(hits)} marker(s): " + ", ".join(hits[:5]) + (" ..." if len(hits) > 5 else "")
    return "PASS", "none"


@step("aurora-patches")
def aurora_patches():
    manifest = os.path.join(here, "aurora-patches.txt")
    aurora = os.path.join(pc, "extern", "aurora")
    missing = []
    count = 0
    with open(manifest, encoding="utf8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            name, rel, regexes = [part.strip() for part in line.split("|", 2)]
            count += 1
            path = os.path.join(aurora, rel)
            if not os.path.exists(path):
                missing.append(f"{name} (file {rel} missing)")
                continue
            text = open(path, encoding="utf8", errors="replace").read()
            for rx in regexes.split(" && "):
                if not re.search(rx, text, re.M):
                    missing.append(f"{name} (no match: {rx})")
    # the series itself: what --check applies must be what git tracks, with nothing uncommitted in patches/aurora
    rc, out = run_cmd([sys.executable, os.path.join(here, "apply-aurora-patches.py"), "--check"], pc, "aurora-check")
    m = re.search(r"(\d+) patches", out)
    tracked = [p for p in (git("ls-files", "sunshine-pc/patches/aurora") or b"").decode().splitlines()
               if re.fullmatch(r"\d{4}-.*\.patch", os.path.basename(p))]
    dirty = (git("status", "--porcelain", "--", "sunshine-pc/patches/aurora") or b"").decode().splitlines()
    if rc != 0 or not m:
        missing.append(f"--check exit {rc}: {last_line(out)}")
    elif int(m.group(1)) != len(tracked):
        missing.append(f"--check counts {m.group(1)} patches, git tracks {len(tracked)}")
    if dirty:
        missing.append("patches/aurora not clean: " + ", ".join(ln.strip() for ln in dirty[:3]))
    if missing:
        return "FAIL", "; ".join(missing)
    return "PASS", f"{count} patch(es) present; {last_line(out)}; {len(tracked)} tracked, clean"


@step("matching-sha")
def matching_sha(run_ninja):
    if run_ninja:
        t0 = time.time()
        rc, out = run_cmd(["ninja", "-k", "0"], sms, "sms-ninja")
        if re.search(r"download_tool\.py|\] TOOL ", out):
            return "FAIL", ("ninja ran download_tool.py (configure.py ran without the local tool paths); in sms/ re-run "
                            "`python configure.py --dtk build/tools/dtk.exe --objdiff build/tools/objdiff-cli.exe "
                            "--sjiswrap build/tools/sjiswrap.exe --compilers build/compilers --binutils build/binutils` "
                            "(relative paths: an absolute path with a space breaks argparse)")
        if rc != 0:
            return "FAIL", f"ninja -k 0 exit {rc}: {last_line(out)}"
        detail_pre = f"ninja -k 0 ok in {time.time() - t0:.0f}s; "
    else:
        detail_pre = ""
    dol = os.path.join(sms, "build", "GMSJ01", "mario.dol")
    sha_file = os.path.join(sms, "config", "GMSJ01", "build.sha1")
    if not os.path.exists(dol):
        return "SKIP", detail_pre + "mario.dol not built (lead runs --matching)"
    expected = open(sha_file, encoding="utf8").read().split()[0].lower()
    actual = hashlib.sha1(open(dol, "rb").read()).hexdigest()
    if actual != expected:
        return "FAIL", detail_pre + f"sha1 {actual} != {expected}"
    return "PASS", detail_pre + actual


@step("line-endings")
def line_endings():
    ref = next((r for r in ("origin/main", "@{u}", "HEAD~1") if git("rev-parse", "--verify", "--quiet", r)), None)
    if ref is None:
        return "SKIP", "no base commit (origin/main, @{u}, HEAD~1)"
    base = git("merge-base", ref, "HEAD").decode().strip()
    names = set()
    for args in ([base, "HEAD"], ["--cached", base], [base]):  # pushed commits, staged, unstaged
        names.update(n.decode("utf8") for n in (git("diff", "--name-only", "-z", *args) or b"").split(b"\0") if n)
    autocrlf = (git("config", "core.autocrlf") or b"").strip() in (b"true", b"input")
    bad = []
    for path in sorted(names):
        attrs = (git("check-attr", "text", "eol", "--", path) or b"").decode("utf8", "replace")
        normalised = autocrlf or re.search(r": (text: (set|auto)|eol: (lf|crlf))$", attrs, re.M) is not None
        was = eol_class(git("cat-file", "blob", f"{base}:{path}"))
        versions = [("HEAD", git("cat-file", "blob", f"HEAD:{path}")), ("index", git("cat-file", "blob", f":{path}"))]
        work = os.path.join(root, path)
        if not normalised and os.path.isfile(work):  # text/eol attributes or core.autocrlf: `git add` converts it
            versions.append(("work", open(work, "rb").read()))
        for where, data in versions:
            now = eol_class(data)
            if now == "mixed" or (was in ("LF", "CRLF") and now and now != was) or \
                    (was is None and now == "CRLF" and not normalised):
                bad.append(f"{path} ({where} {was or 'new'} -> {now})")
                break
    if bad:
        return "FAIL", f"{len(bad)} file(s): " + ", ".join(bad[:5]) + (" ..." if len(bad) > 5 else "")
    return "PASS", f"{len(names)} changed path(s) since {ref} ({base[:7]}), no EOL flip, mixed or new CRLF file"


@step("strict-link")
def strict_link(build):
    if not build:
        return "SKIP", "not requested (--build)"
    rc, out = run_cmd(["cmd", "/c", os.path.join(pc, "tools", "_strict.cmd")], pc, "strict-link")
    unresolved = sorted(set(re.findall(r"LNK2019: unresolved external symbol (\S+)", out)))
    if rc != 0 or unresolved:
        shown = ", ".join(unresolved[:3]) + (" ..." if len(unresolved) > 3 else "")
        return "FAIL", f"exit {rc}, {len(unresolved)} unresolved: {shown}"
    return "PASS", "link ok, 0 unresolved"


@step("headless-tests")
def headless_tests():
    rc, out = run_cmd(["cmd", "/c", os.path.join(pc, "tools", "port_tests.cmd")], pc, "port-tests")
    if rc != 0:
        return "FAIL", f"exit {rc}: {last_line(out)}"
    # port_tests.cmd may end with a harmless vswhere "not recognized" line; report the summary instead.
    summary = [ln.strip() for ln in out.splitlines() if " checks, " in ln]
    return "PASS", summary[-1] if summary else last_line(out)


@step("probe-units")
def probe_units(full):
    if not full:
        return "SKIP", "not requested (--full)"
    rc, out = run_cmd(["cmd", "/c", os.path.join(pc, "tools", "probe-units.cmd")], pc, "probe-units")
    if rc != 0:
        return "FAIL", f"exit {rc}: {last_line(out)}"
    return "PASS", last_line(out)


@step("boot-smoke")
def boot_smoke(secs, tile):
    if not os.path.exists(strict_exe):
        return "SKIP", "build/strict/sunshine.exe missing (use --build)"
    if not os.path.exists(disc):
        return "SKIP", "disc image missing (sms/orig/GMSJ01)"
    os.makedirs(logdir, exist_ok=True)
    bld = os.path.dirname(strict_exe)
    log_path = os.path.join(logdir, "boot.log")
    env = dict(os.environ, SUNSHINE_WINDOW_POS=tile, SUNSHINE_WINDOW_SIZE="640,480")
    with open(log_path, "w", encoding="utf8") as log:
        p = subprocess.Popen([strict_exe, "--dvd", disc], cwd=bld, env=env, stdout=log, stderr=subprocess.STDOUT)
        t0 = time.time()
        while time.time() - t0 < secs and p.poll() is None:
            time.sleep(0.25)
        alive = p.poll() is None
        if alive:
            p.kill()  # own PID only
            p.wait(timeout=10)
    text = open(log_path, encoding="utf8", errors="replace").read()
    err = next((ln.strip() for ln in text.splitlines() if re.search(r"error|fatal", ln, re.I)), "")
    if FATAL in text:
        return "FAIL", "fatal exception in boot.log: " + err[:200]
    if not alive:
        return "FAIL", f"exited after {time.time() - t0:.1f}s, rc {hex(p.returncode & 0xFFFFFFFF)}; first error: {err[:200] or 'none in log'}"
    if "WebGPU error" in text:
        return "FAIL", "alive but WebGPU errors logged: " + err[:200]
    return "PASS", f"alive {secs:.0f}s, no fatal exception or WebGPU error"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", action="store_true", help="rebuild build/strict and check unresolved symbols")
    ap.add_argument("--matching", action="store_true", help="lead only: ninja -k 0 in sms/ before the SHA check")
    ap.add_argument("--full", action="store_true", help="also run probe-units (slow)")
    ap.add_argument("--boot-secs", type=float, default=45.0)
    ap.add_argument("--tile", default="2800,520", help="window position for the boot smoke")
    args = ap.parse_args()

    t0 = time.time()
    temp_debug()
    aurora_patches()
    matching_sha(args.matching)
    line_endings()
    strict_link(args.build)
    headless_tests()
    probe_units(args.full)
    boot_smoke(args.boot_secs, args.tile)

    counts = {s: sum(1 for r in results if r[1] == s) for s in ("PASS", "FAIL", "SKIP")}
    verdict = "FAIL" if counts["FAIL"] else "PASS"
    print(f"summary: {counts['PASS']} PASS, {counts['FAIL']} FAIL, {counts['SKIP']} SKIP "
          f"in {time.time() - t0:.1f}s -> {verdict}")
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
