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
  python sunshine-pc/tools/decomp_triage.py --branchscan [unit...]   conditional-branch sense/cr flips after an identical compare -> sms/build/branch_inversions.tsv
"""
import json, os, re, subprocess, sys, collections, struct
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


def objdiff_syms(name):
    """objdiff (ours, retail) symbol lists for one unit; None if its object is missing or fails."""
    out = subprocess.run([OBJDIFF, "diff", "-p", SMS, "-u", name, "-o", "-", "--format", "json"],
                         capture_output=True, text=True, creationflags=NOWIN)
    if out.returncode:
        return None
    j = json.loads(out.stdout)
    return j["left"]["symbols"], j.get("right", {}).get("symbols", [])


def unit(name):
    loaded = objdiff_syms(name)
    if loaded is None:
        return []
    ls, rs = loaded
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

def lookup(name, sym):
    loaded = objdiff_syms(name)
    if loaded is None:
        print(f"objdiff failed for {name!r} (object missing or unbuildable)")
        return None
    ls, rs = loaded
    fns = [s for s in ls if s.get("kind") == "SYMBOL_FUNCTION"]
    hits = [s for s in fns if s["name"] == sym] or [s for s in fns if sym in s.get("demangled_name", "")]
    if len(hits) != 1:
        print(f"{len(hits)} matches for {sym!r}:", *[s["name"] for s in hits[:10]])
        return None
    s = hits[0]
    L = s.get("instructions", [])
    R = rs[s["target_symbol"]].get("instructions", []) if s.get("target_symbol") is not None else []
    return s, L, R, ls, rs


FUSED = {"fmadds", "fmsubs", "fnmadds", "fnmsubs", "fmadd", "fmsub", "fnmadd", "fnmsub"}


def check(name, sym):
    """Mechanical gate for a logic-verified claim: same calls in order, same fused/unfused FP ops,
    same non-move opcode histogram. Prints OK or each mismatch; every mismatch must be explained."""
    r = lookup(name, sym)
    if not r:
        return
    s, L, R, ls, rs = r
    ins = lambda rows: [i["instruction"] for i in rows if i.get("instruction")]
    li, ri = ins(L), ins(R)
    mn = lambda i: i.get("formatted", "").split(" ")[0]

    def calls(rows, syms):
        out = []
        for i in rows:
            m = mn(i)
            if m in ("bl", "b") and "relocation" in i:
                out.append(norm(syms[i["relocation"]["target_symbol"]]["name"]))
            elif m in ("bctrl", "bctr"):
                out.append(m)
        return out

    def vslots(rows):
        return [i["formatted"].split(",")[1].strip() for i in rows
                if i.get("formatted", "").startswith("lwz r12, ") and i["formatted"].endswith("(r12)")]

    moves = {"mr", "fmr", "li", "lis", "nop"}
    stack = lambda i: "(r1)" in i.get("formatted", "")
    hist = lambda rows: collections.Counter(mn(i) for i in rows if mn(i) not in moves and not stack(i))
    problems = []
    cl, cr = calls(li, ls), calls(ri, rs)
    if cl != cr:
        problems.append(f"calls differ:\n  retail {cl}\n  ours   {cr}")
    if vslots(li) != vslots(ri):
        problems.append(f"vtable slots differ: retail {vslots(li)} ours {vslots(ri)}")
    hl, hr = hist(li), hist(ri)
    fl = sum(v for k, v in hl.items() if k in FUSED)
    fr = sum(v for k, v in hr.items() if k in FUSED)
    if fl != fr:
        problems.append(f"fused FP ops differ: retail {fl} ours {fr} (rounding differs)")
    d = {k: hl[k] - hr[k] for k in set(hl) | set(hr) if hl[k] != hr[k]}
    if d:
        problems.append(f"opcode histogram (retail-ours, excl. moves/stack): {dict(sorted(d.items()))}")
    cvl, cvr = consts(name, li, ls, "target_path"), consts(name, ri, rs, "base_path")
    if cvl is not None and cvr is not None and sorted(cvl) != sorted(cvr):
        problems.append(f"pool constants differ (as multisets):\n  retail {sorted(cvl)}\n  ours   {sorted(cvr)}")
    print(f"{s['name']} {s.get('match_percent')}%:", "OK" if not problems else "")
    for p in problems:
        print(" -", p)
    print("note: branch sense/targets (which code sits on the taken vs fallthrough path) still need a manual check")


_elf_cache = {}


def consts(name, rows, syms, which):
    """Values of literal-pool / small-data constants loaded by these rows (lfs/lfd/lwz/lha… via @NNN symbols)."""
    try:
        from elftools.elf.elffile import ELFFile
    except ImportError:
        return None
    p = json.load(open(os.path.join(SMS, "objdiff.json")))
    path = os.path.join(SMS, next(u[which] for u in p["units"] if u["name"] == name))
    if path not in _elf_cache:
        f = ELFFile(open(path, "rb"))
        tab = {}
        for sym in f.get_section_by_name(".symtab").iter_symbols():
            if sym["st_shndx"] not in ("SHN_UNDEF", "SHN_ABS", "SHN_COMMON"):
                tab[sym.name] = (f.get_section(sym["st_shndx"]), sym["st_value"], sym["st_size"])
        _elf_cache[path] = tab
    tab = _elf_cache[path]
    out = []
    for i in rows:
        m = i.get("formatted", "").split(" ")[0]
        if "relocation" not in i or m not in ("lfs", "lfd", "lwz", "lhz", "lha", "lbz", "lfsu"):
            continue
        n = syms[i["relocation"]["target_symbol"]]["name"]
        if n not in tab or not (n.startswith("@") or n.startswith("lbl_")):
            continue
        sec, off, size = tab[n]
        width = 8 if m == "lfd" else 4 if m in ("lfs", "lfsu", "lwz") else 2 if m in ("lhz", "lha") else 1
        data = sec.data()[off:off + width] if sec.header["sh_type"] != "SHT_NOBITS" else b"\0" * width
        if m in ("lfs", "lfsu"):
            val = f"{struct.unpack('>f', data)[0]!r}f"
        elif m == "lfd":
            val = repr(struct.unpack('>d', data)[0])
        else:
            val = "0x" + data.hex()
        out.append(val)
    return out


def diff(name, sym, ctx=0):
    r = lookup(name, sym)
    if not r:
        return
    s, L, R, ls, rs = r
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



GAME_SKIP = ("JSystem", "PowerPC_EABI_Support", "THPPlayer", "TRK_MINNOW_DOLPHIN", "dolphin", "OdemuExi2")
COND_BR = re.compile(r"b(?:lt|le|eq|ge|gt|ne|so|ns|dnz|dz)[tf]?l?(?:lr|ctr|r)?[+-]?")


def mnem(row):
    ins = row.get("instruction")
    if not ins:
        return None
    return next((p["opcode"]["mnemonic"] for p in ins.get("parts", []) if "opcode" in p), None)


def crf(row):
    m = re.search(r"\bcr(\d)\b", row["instruction"]["formatted"])
    return m.group(0) if m else "cr0"


def text(row):
    """Row text for equality: formatted, with the branch target dropped so layout-shifted targets still compare equal."""
    ins = row.get("instruction")
    if not ins:
        return None
    return ins["formatted"].rsplit(" ", 1)[0] if "branch_dest" in ins else ins["formatted"]


def prev_cmp(rows, i):
    """Nearest compare above row i within its block (cr-logic and other non-branch rows skipped), else None."""
    for j in range(i - 1, max(-1, i - 9), -1):
        m = mnem(rows[j])
        if m is None:
            continue
        if m.startswith(("cmp", "fcmp")):
            return j
        if m.startswith("b"):
            return None
    return None


def byaddr(rows):
    return {r["instruction"]["address"]: j for j, r in enumerate(rows) if r.get("instruction", {}).get("address")}


def succ(rows, amap, j):
    """Text of the row at the address after rows[j] (by address, so alignment placeholders don't shift it)."""
    a = rows[j]["instruction"].get("address")
    k = amap.get(str(int(a) + 4)) if a else None
    return None if k is None else text(rows[k])


def target(rows, amap, j):
    k = amap.get(rows[j]["instruction"].get("branch_dest"))
    return None if k is None else text(rows[k])


def scan_fn(L, R):
    """Rows where ours and retail are both conditional branches with different mnemonic or cr field,
    after an identical compare. Class: same-layout (fallthrough row equal in both = likely real inversion),
    swapped-blocks (ours' fallthrough is retail's taken target = legal block reorder), other."""
    aL, aR = byaddr(L), byaddr(R)
    out = []
    for i in range(min(len(L), len(R))):
        ml, mr = mnem(L[i]), mnem(R[i])
        if not (ml and mr and COND_BR.fullmatch(ml) and COND_BR.fullmatch(mr)):
            continue
        if ml == mr and crf(L[i]) == crf(R[i]):
            continue
        cl, cr = prev_cmp(L, i), prev_cmp(R, i)
        if cl is None or cr is None or text(L[cl]) != text(R[cr]):
            continue
        ftl, ftr, tgt = succ(L, aL, i), succ(R, aR, i), target(R, aR, i)
        if ftl and ftl == ftr:
            cls = "same-layout"
        elif ftl and ftl == tgt:
            cls = "swapped-blocks"
        else:
            cls = "other"
        out.append((i, R[i]["instruction"]["formatted"], L[i]["instruction"]["formatted"], cls))
    return out


def scan_unit(name):
    loaded = objdiff_syms(name)
    if loaded is None:
        return None
    ls, rs = loaded
    hits, nfn = [], 0
    for s in ls:
        m = s.get("match_percent")
        if s.get("kind") != "SYMBOL_FUNCTION" or s.get("target_symbol") is None or (m is not None and m >= 100):
            continue
        nfn += 1
        L = s.get("instructions", [])
        R = rs[s["target_symbol"]].get("instructions", [])
        for i, rt, ou, cls in scan_fn(L, R):
            hits.append((name, s.get("demangled_name", s["name"]), i, rt, ou, cls))
    return hits, nfn


def branchscan(units):
    if not units:
        triage = json.load(open(os.path.join(SMS, "build", "triage.json")))
        units = sorted({f["unit"] for f in triage if f["unit"].split("/")[1] not in GAME_SKIP})
    with ThreadPoolExecutor(6) as ex:
        res = list(ex.map(scan_unit, units))
    skipped = [u for u, r in zip(units, res) if r is None]
    if skipped:
        print("skipped (no object):", *skipped)
    hits = [h for r in res if r for h in r[0]]
    nfn = sum(r[1] for r in res if r)
    path = os.path.join(SMS, "build", "branch_inversions.tsv")
    with open(path, "w") as fh:
        for u, fn, i, rt, ou, cls in hits:
            fh.write(f"{u}\t{fn}\t{i}\t{rt}\t{ou}\t{cls}\n")
    c = collections.Counter(h[5] for h in hits)
    print(f"units={len(units) - len(skipped)} functions={nfn} hits={len(hits)} same-layout={c['same-layout']} "
          f"swapped-blocks={c['swapped-blocks']} other={c['other']} -> {path}")



def main():
    if sys.argv[1:2] == ["--build"]:
        return build(sys.argv[2])
    if sys.argv[1:2] == ["--diff"]:
        return diff(sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 0)
    if sys.argv[1:2] == ["--check"]:
        return check(sys.argv[2], sys.argv[3])
    if sys.argv[1:2] == ["--branchscan"]:
        return branchscan(sys.argv[2:])
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
