"""Syntax-only MSVC probe over every decomp source. Run via probe-units.cmd (needs the MSVC env).

Prints how many units parse and the most common errors; writes tools/probe-results.json.
"""
import collections
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", ".."))
sms = os.path.join(root, "sms")
aurora_inc = os.path.join(root, "sunshine-pc", "extern", "aurora", "include")
use_aurora = "--aurora" in sys.argv

cfg = open(os.path.join(sms, "configure.py"), encoding="utf8").read()
units = [p for _, p in re.findall(r'Object\((\w+),\s*"([^"]+)"', cfg)]
units = [u for u in units if u.endswith((".cpp", ".c")) and os.path.exists(os.path.join(sms, "src", u))]
rules = [ln.split("|", 1)[0].strip() for ln in open(os.path.join(root, "sunshine-pc", "excluded-units.txt"), encoding="utf8")
         if ln.strip() and not ln.startswith("#")]
excluded = [u for u in units if any(u.startswith(r) for r in rules)]
units = [u for u in units if u not in excluded]
print(f"excluded by rule: {len(excluded)}; probing: {len(units)}")

inc = [os.path.join(sms, "include"), os.path.join(sms, "include", "dolphin"),
       os.path.join(sms, "include", "PowerPC_EABI_Support")]
# Lowest priority: decomp headers win; only files the decomp lacks (dolphin/ppc_math.h) come from Aurora.
inc.append(aurora_inc)
# CLANG_CL=<path to clang-cl.exe> switches to clang-cl (accepts C99 compound literals in C++, like Dusklight's clang-cl presets).
cc = os.environ.get("CLANG_CL")
base = [cc or "cl", "/nologo", "/c", "/Zs", "/Zc:__cplusplus", "/utf-8", "/W0", "/DTARGET_PC=1",
        "/DVERSION_GMSJ01=1", "/DNDEBUG", "/FI" + os.path.join(root, "sunshine-pc", "include", "pc_compat.h"),
        "/I" + os.path.join(root, "sunshine-pc", "include")] + [f"/I{i}" for i in inc]
if cc:
    base += ["-Wno-everything", "-Wno-multichar", "-Wno-trigraphs"]
err_re = re.compile(r"^(.*?): error (C\d+): (.*)$|^(.*?\(\d+,\d+\)): error: (.*)$", re.M)


def parse(out):
    res = []
    for a, c, m, b, m2 in err_re.findall(out):
        res.append((c, m, a) if c else ("clang", m2, b))
    return res


def run(u):
    cxx = u.endswith(".cpp") or u.startswith("THPPlayer/")  # THPPlayer is built -lang=c++
    std = ["/TP", "/std:c++20"] if cxx else ["/std:c11"]
    r = subprocess.run(base + std + [os.path.join(sms, "src", u)], capture_output=True, text=True, cwd=sms)
    return u, r.returncode, parse(r.stdout + r.stderr)


with cf.ThreadPoolExecutor(12) as ex:
    results = list(ex.map(run, units))

print("parse clean:", sum(r[1] == 0 for r in results), "/", len(results))
codes = collections.Counter()
sites = collections.Counter()
for _, _, errs in results:
    for c, m, loc in errs:
        codes[(c, m[:80])] += 1
        sites[(c, loc)] += 1
for (c, m), n in codes.most_common(15):
    print(n, c, m)
print("top error sites (units affected):")
for (c, loc), n in sites.most_common(12):
    print(n, c, os.path.relpath(loc.split("(")[0], root) + "(" + loc.split("(", 1)[-1])

by, tot = collections.Counter(), collections.Counter()
for u, rc, _ in results:
    k = "/".join(u.split("/")[:2]) if u.startswith(("JSystem", "dolphin")) else u.split("/")[0]
    tot[k] += 1
    by[k] += rc == 0
for k in sorted(tot, key=lambda k: -tot[k])[:25]:
    print(f"{k:34}{by[k]:>4}/{tot[k]}")

json.dump([{"unit": u, "rc": rc, "errors": errs} for u, rc, errs in results],
          open(os.path.join(here, "probe-results.json"), "w"), indent=1)
