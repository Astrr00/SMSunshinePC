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
rules = {}
for ln in open(os.path.join(root, "sunshine-pc", "excluded-units.txt"), encoding="utf8"):
    if ln.strip() and not ln.startswith("#"):
        prefix, _, reason = ln.partition("|")
        rules[prefix.strip()] = reason.strip()


def why(u):
    return next((r for p, r in rules.items() if u.startswith(p)), None)


excluded = {u: why(u) for u in units if why(u) is not None}
print(f"probing all {len(units)} units; {len(excluded)} of them are listed in excluded-units.txt")

inc = [os.path.join(sms, "include"), os.path.join(sms, "include", "dolphin"),
       os.path.join(root, "sunshine-pc", "src", "glyphs"), os.path.join(sms, "include", "PowerPC_EABI_Support")]
# Lowest priority: decomp headers win; only files the decomp lacks (dolphin/ppc_math.h) come from Aurora.
inc.append(aurora_inc)
# CLANG_CL=<path to clang-cl.exe> switches to clang-cl (accepts C99 compound literals in C++, like Dusklight's clang-cl presets).
cc = os.environ.get("CLANG_CL")
base = [cc or "cl", "/nologo", "/c", "/Zs", "/Zc:__cplusplus", "/utf-8", "/W0", "/DTARGET_PC=1",
        "/DVERSION_GMSJ01=1", "/DNDEBUG", "/FI" + os.path.join(root, "sunshine-pc", "include", "pc_compat.h"),
        "/I" + os.path.join(root, "sunshine-pc", "include")] + [f"/I{i}" for i in inc]
if cc:
    base += ["-Wno-everything", "-Wno-multichar", "-Wno-trigraphs"]
# --wflag=<name>: report every -W<name> warning (needs clang-cl); result goes to tools/warn-<name>.txt.
# --ptrtrunc is shorthand for --wflag=pointer-to-int-cast and writes tools/ptr-trunc.txt.
wflag = next((a[8:] for a in sys.argv if a.startswith("--wflag=")), None) or (
    "pointer-to-int-cast" if "--ptrtrunc" in sys.argv else None)
ptrtrunc = wflag is not None
if wflag and cc:
    base = [a for a in base if a != "/W0"] + [f"-W{wflag}", "-Wno-error"]  # /W0 would mute every warning
warn_re = re.compile(r"^(.*?\(\d+,\d+\)): warning: (.*)$", re.M)  # only the requested flag is enabled
err_re = re.compile(r"^(.*?): error (C\d+): (.*)$|^(.*?\(\d+,\d+\)): error: (.*)$", re.M)


def parse(out):
    res = []
    for a, c, m, b, m2 in err_re.findall(out):
        res.append((c, m, a) if c else ("clang", m2, b))
    return res
warnings = []


def run(u):
    cxx = u.endswith(".cpp") or u.startswith("THPPlayer/")  # THPPlayer is built -lang=c++
    std = ["/TP", "/std:c++20"] if cxx else ["/std:c11"]
    r = subprocess.run(base + std + [os.path.join(sms, "src", u)], capture_output=True, text=True, cwd=sms)
    if ptrtrunc:
        warnings.extend(warn_re.findall(r.stdout + r.stderr))
    return u, r.returncode, parse(r.stdout + r.stderr)


with cf.ThreadPoolExecutor(12) as ex:
    results = list(ex.map(run, units))

print("parse clean:", sum(r[1] == 0 for r in results), "/", len(results))
print("listed as not built, parse clean:", sum(rc == 0 for u, rc, _ in results if u in excluded), "/", len(excluded))
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

out = "probe-results-clang-cl.json" if cc else "probe-results.json"
json.dump([{"unit": u, "rc": rc, "errors": errs, **({"excluded": excluded[u]} if u in excluded else {})}
           for u, rc, errs in results], open(os.path.join(here, out), "w"), indent=1)

if ptrtrunc:
    uniq = sorted(set(warnings))
    with open(os.path.join(here, "ptr-trunc.txt" if wflag == "pointer-to-int-cast" else f"warn-{wflag}.txt"), "w", encoding="utf8") as f:
        for loc, msg in uniq:
            f.write(f"{os.path.relpath(loc.split('(')[0], root)}({loc.split('(', 1)[-1]}: {msg}\n")
    per = collections.Counter(os.path.relpath(loc.split("(")[0], root).split(os.sep)[2] if len(os.path.relpath(loc.split("(")[0], root).split(os.sep)) > 2 else "?" for loc, _ in uniq)
    print(f"pointer-to-32-bit casts: {len(uniq)} distinct sites in {len({l.split('(')[0] for l, _ in uniq})} files")
    for k, n in per.most_common(12):
        print(f"  {n:4} {k}")
