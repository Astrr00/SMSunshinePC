"""Rank non-matching sms functions by logic risk (objdiff per unit).

Risk = diffs that change behaviour, not register allocation:
op mismatch / insert / delete / replace, immediate mismatches outside r1
stack offsets, and relocation (call/data symbol) mismatches. Local-static
renames (init$123 vs init$localstatic1$...) are counted as `local`, not in
the score: retail names carry no owner, so a wrong nerve singleton also
lands there. Score 0 is a ranking, not proof of correctness; only an exact
match or an asm-equivalence review is.
Known false positive: string-pool offsets (addi rX, rY, imm) show as imm.

  python sunshine-pc/tools/decomp_triage.py [unit-substring]   all units -> sms/build/triage.json
  python sunshine-pc/tools/decomp_triage.py --build <unit>      recompile one unit with mwcc, print its rows
  python sunshine-pc/tools/decomp_triage.py --diff <unit> <sym> [ctx]  differing rows only: retail | ours (ctx rows around)
"""
import json, os, subprocess, sys, collections
from concurrent.futures import ThreadPoolExecutor

SMS = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "sms"))
OBJDIFF = os.path.join(SMS, "build", "tools", "objdiff-cli.exe")
NOWIN = 0x08000000


def norm(name):
    return name.split("$")[0] + "$" if "$" in name else name


def risk(ins, rins, lsyms, rsyms):
    r = collections.Counter()
    for i, ri in zip(ins, rins):
        k = i.get("diff_kind")
        if not k:
            continue
        if k != "DIFF_ARG_MISMATCH":
            r[k.removeprefix("DIFF_").lower()] += 1
            continue
        parts = [p["arg"] for p in i["instruction"].get("parts", []) if "arg" in p]
        stack = "r1" in [p.get("opaque") for p in parts]
        for a, d in zip(parts, i.get("arg_diff", [])):
            if "diff_index" not in d:
                continue
            if "opaque" in a:
                r["reg"] += 1
            elif "reloc" in a or "branch_dest" in a:
                lt = i["instruction"].get("relocation", {}).get("target_symbol")
                rt = ri.get("instruction", {}).get("relocation", {}).get("target_symbol")
                same = lt is not None and rt is not None and \
                    norm(lsyms[lt]["name"]) == norm(rsyms[rt]["name"])
                r["local" if same else "reloc"] += 1
            elif not stack:
                r["imm"] += 1
            else:
                r["stack"] += 1
    return r


def unit(name):
    out = subprocess.run([OBJDIFF, "diff", "-p", SMS, "-u", name, "-o", "-", "--format", "json"],
                         capture_output=True, text=True, creationflags=NOWIN)
    if out.returncode:
        return []
    j = json.loads(out.stdout)
    ls, rs = j["left"]["symbols"], j.get("right", {}).get("symbols", [])
    res = []
    for s in ls:
        if s.get("kind") != "SYMBOL_FUNCTION":
            continue
        m = s.get("match_percent")
        if m is not None and m >= 100:
            continue
        t = s.get("target_symbol") if rs else None
        r = risk(s.get("instructions", []), rs[t].get("instructions", []) if t is not None else [], ls, rs)
        score = sum(v for k, v in r.items() if k not in ("reg", "stack", "local"))
        res.append(dict(unit=name, fn=s.get("demangled_name", s["name"]), sym=s["name"],
                        size=int(s.get("size", 0)), match=m, score=score if m is not None else 999,
                        diffs=dict(r)))
    return res


def build(name):
    p = json.load(open(os.path.join(SMS, "objdiff.json")))
    base = next(u["base_path"] for u in p["units"] if u["name"] == name)
    cmd = subprocess.run(["ninja", "-t", "commands", base], cwd=SMS, capture_output=True,
                         text=True, creationflags=NOWIN).stdout.strip().splitlines()[-1]
    c = subprocess.run(cmd, cwd=SMS, shell=True, capture_output=True, text=True, creationflags=NOWIN)
    print(c.stdout[-3000:], c.stderr[-3000:], f"mwcc exit={c.returncode}")
    if c.returncode:
        sys.exit(1)
    rows = unit(name)
    for f in sorted(rows, key=lambda f: -f["score"]):
        print(f["score"], f["match"], f["fn"], f["diffs"])
    print(f"nonmatching={len(rows)} risky={sum(1 for f in rows if f['score'])}")

def diff(name, sym, ctx=0):
    out = subprocess.run([OBJDIFF, "diff", "-p", SMS, "-u", name, "-o", "-", "--format", "json"],
                         capture_output=True, text=True, creationflags=NOWIN)
    j = json.loads(out.stdout)
    ls, rs = j["left"]["symbols"], j.get("right", {}).get("symbols", [])
    fns = [s for s in ls if s.get("kind") == "SYMBOL_FUNCTION"]
    hits = [s for s in fns if s["name"] == sym] or [s for s in fns if sym in s.get("demangled_name", "")]
    if len(hits) != 1:
        print(f"{len(hits)} matches for {sym!r}:", *[s["name"] for s in hits[:10]])
        return
    s = hits[0]
    L = s.get("instructions", [])
    R = rs[s["target_symbol"]].get("instructions", []) if s.get("target_symbol") is not None else []
    fmt = lambda i: i.get("instruction", {}).get("formatted", "")
    marks = [i for i, (a, b) in enumerate(zip(L, R)) if a.get("diff_kind") or b.get("diff_kind")]
    show = sorted({k for i in marks for k in range(max(0, i - ctx), min(len(L), i + ctx + 1))})
    print(f"{s.get('match_percent')}% rows={len(L)} diff_rows={len(marks)}")
    last = -2
    for i in show:
        if i != last + 1:
            print("  ...")
        k = (L[i].get("diff_kind") or R[i].get("diff_kind") or "").removeprefix("DIFF_")[:8]
        print(f"{i:4} {k:8} {fmt(L[i]):40} | {fmt(R[i])}")
        last = i



def main():
    if sys.argv[1:2] == ["--build"]:
        return build(sys.argv[2])
    if sys.argv[1:2] == ["--diff"]:
        return diff(sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 0)
    rep = json.load(open(os.path.join(SMS, "build", "GMSJ01", "report.json")))
    flt = sys.argv[1] if len(sys.argv) > 1 else ""
    names = [u["name"] for u in rep["units"] if flt in u["name"]
             and any(f.get("fuzzy_match_percent", 0) < 100 for f in u.get("functions", []))]
    with ThreadPoolExecutor(6) as ex:
        rows = [f for fs in ex.map(unit, names) for f in fs]
    rows.sort(key=lambda f: -f["score"])
    if not flt:
        json.dump(rows, open(os.path.join(SMS, "build", "triage.json"), "w"), indent=1)
    risky = [f for f in rows if f["score"]]
    print(f"units={len(names)} nonmatching={len(rows)} risky={len(risky)} "
          f"risky_bytes={sum(f['size'] for f in risky)} regalloc_only={len(rows) - len(risky)}")
    for f in risky[:15]:
        print(f["score"], f["match"], f["unit"], f["fn"], f["diffs"])


if __name__ == "__main__":
    main()
