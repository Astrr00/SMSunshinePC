#!/usr/bin/env python3
"""Operand audit of NonMatching units (GitHub issue #2).

For every unit of a decomp checkout that is not fully matched, runs one-shot
objdiff-cli diffs (target = build obj, base = build src, paths from objdiff.json),
then counts instructions whose operands differ in an immediate, an offset, a
relocation or a branch target. Pure register-allocation differences (both sides
are registers) are ignored, as are opcode changes (DIFF_REPLACE) and inserted or
deleted instructions. Units are ranked by hit count.

Read-only for the decomp tree: objdiff writes its JSON into a temp directory that
is removed afterwards. Prints only unit/function names and counts.

Usage:
  python nm_operand_audit.py --sms-dir "<decomp root>/sms" [--top 10] [--jobs 4]
                             [--tsv <file>]

Requires objdiff-cli (default <sms-dir>/build/tools/objdiff-cli.exe) and
<sms-dir>/build/GMSJ01/report.json. Python 3 standard library only.
"""

import argparse
import collections
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import tempfile


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def arg_kind(arg):
    """objdiff arg objects have exactly one key: opaque, signed, unsigned, reloc, branch_dest."""
    if not isinstance(arg, dict) or not arg:
        return "missing"
    return next(iter(arg))


def arg_list(row):
    instr = row.get("instruction") or {}
    return [p["arg"] for p in instr.get("parts", []) if "arg" in p]


def classify_row(lrow, rrow):
    """Return the operand categories of the mismatched args of one diff row.

    An empty list means the row differs only in register allocation.
    """
    cats = []
    la = arg_list(lrow)
    ra = arg_list(rrow)
    for k, entry in enumerate(lrow.get("arg_diff") or []):
        if not (isinstance(entry, dict) and "diff_index" in entry):
            continue
        lk = arg_kind(la[k]) if k < len(la) else "missing"
        rk = arg_kind(ra[k]) if k < len(ra) else "missing"
        if lk == "opaque" and rk == "opaque":
            continue  # register allocation
        if "reloc" in (lk, rk):
            cats.append("reloc")
        elif "branch_dest" in (lk, rk):
            cats.append("branch")
        else:
            cats.append("imm")
    return cats


def analyse_diff(doc):
    """Count operand hits per function for one objdiff JSON document."""
    left = doc["left"]["symbols"]
    right = doc["right"]["symbols"]
    funcs = collections.Counter()
    cats = collections.Counter()
    regalloc_rows = 0
    opcode_rows = 0
    for sym in left:
        lrows = sym.get("instructions") or []
        if not lrows:
            continue
        t = sym.get("target_symbol")
        if t is None or t >= len(right):
            continue
        rrows = right[t].get("instructions") or []
        name = sym.get("demangled_name") or sym.get("name") or "?"
        for i, lrow in enumerate(lrows):
            kind = lrow.get("diff_kind")
            if kind == "DIFF_REPLACE":
                opcode_rows += 1
                continue
            if kind != "DIFF_ARG_MISMATCH" or i >= len(rrows):
                continue
            c = classify_row(lrow, rrows[i])
            if c:
                funcs[name] += 1
                cats.update(c)
            else:
                regalloc_rows += 1
    return {"funcs": funcs, "cats": cats,
            "regalloc": regalloc_rows, "opcode": opcode_rows}


def is_nonmatching(unit):
    measures = unit.get("measures") or {}
    if float(measures.get("fuzzy_match_percent", 0.0)) < 100.0:
        return True
    return any(float(f.get("fuzzy_match_percent", 0.0)) < 100.0
               for f in unit.get("functions") or [])


def run_objdiff(objdiff, sms_dir, tmpdir, idx, target, base):
    out = os.path.join(tmpdir, "diff_%d.json" % idx)
    cmd = [objdiff, "--no-color", "diff", "-1", target, "-2", base,
           "-o", out, "--format", "json"]
    proc = subprocess.run(cmd, cwd=sms_dir, capture_output=True, text=True,
                          errors="replace")
    if proc.returncode != 0 or not os.path.exists(out):
        tail = (proc.stderr or proc.stdout or "").strip().splitlines()
        return None, (tail[-1] if tail else "exit %d" % proc.returncode)
    try:
        doc = load_json(out)
    finally:
        os.remove(out)
    return analyse_diff(doc), None


def short(name, width=64):
    return name if len(name) <= width else name[:width - 3] + "..."


def md_table(headers, rows):
    lines = ["| " + " | ".join(headers) + " |",
             "|" + "|".join("---" for _ in headers) + "|"]
    for row in rows:
        lines.append("| " + " | ".join(str(c) for c in row) + " |")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--sms-dir", required=True,
                    help="the sms/ directory of the decomp checkout")
    ap.add_argument("--report", default=None,
                    help="report.json (default <sms-dir>/build/GMSJ01/report.json)")
    ap.add_argument("--objdiff", default=None,
                    help="objdiff-cli (default <sms-dir>/build/tools/objdiff-cli.exe)")
    ap.add_argument("--top", type=int, default=10)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--tsv", default=None,
                    help="also write every function with hits as TSV (unit, function, hits)")
    args = ap.parse_args()

    sms = os.path.abspath(args.sms_dir)
    report_path = args.report or os.path.join(sms, "build", "GMSJ01", "report.json")
    objdiff = args.objdiff or os.path.join(sms, "build", "tools", "objdiff-cli.exe")
    for p in (report_path, objdiff, os.path.join(sms, "objdiff.json")):
        if not os.path.exists(p):
            sys.exit("missing: %s" % p)

    report = load_json(report_path)
    objdiff_units = {u["name"]: u for u in load_json(os.path.join(sms, "objdiff.json"))["units"]}

    nm_units = [u for u in report["units"] if is_nonmatching(u)]
    jobs = []
    skipped = []
    for u in nm_units:
        ou = objdiff_units.get(u["name"])
        if ou is None:
            skipped.append((u["name"], "not in objdiff.json"))
            continue
        jobs.append((u["name"], ou["target_path"], ou["base_path"]))

    tmpdir = tempfile.mkdtemp(prefix="nmaudit-")
    results = {}
    failed = []
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            futs = {pool.submit(run_objdiff, objdiff, sms, tmpdir, i, tgt, base): name
                    for i, (name, tgt, base) in enumerate(jobs)}
            for fut in concurrent.futures.as_completed(futs):
                name = futs[fut]
                try:
                    res, err = fut.result()
                except Exception as exc:  # keep going, report at the end
                    res, err = None, str(exc)
                if res is None:
                    failed.append((name, err))
                else:
                    results[name] = res
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

    unit_hits = {}
    cats = collections.Counter()
    regalloc = opcode = 0
    for name, res in results.items():
        cats.update(res["cats"])
        regalloc += res["regalloc"]
        opcode += res["opcode"]
        total = sum(res["funcs"].values())
        if total:
            unit_hits[name] = (total, res["funcs"])

    ranked = sorted(unit_hits.items(), key=lambda kv: (-kv[1][0], kv[0]))

    print("NonMatching units: %d (diffed %d, failed %d, skipped %d)"
          % (len(nm_units), len(results), len(failed), len(skipped)))
    print("Units with operand hits: %d" % len(ranked))
    print("Operand hits (instructions): %d  [imm/offset %d, reloc %d, branch %d]"
          % (sum(v[0] for v in unit_hits.values()), cats["imm"], cats["reloc"], cats["branch"]))
    print("Register-only mismatches (ignored): %d" % regalloc)
    print("Opcode changes (ignored): %d" % opcode)
    print()
    print("Top %d NonMatching units by operand hits:" % args.top)
    rows = []
    for rank, (name, (total, funcs)) in enumerate(ranked[:args.top], 1):
        fn, fn_hits = funcs.most_common(1)[0]
        rows.append((rank, name, short(fn), fn_hits, total))
    print(md_table(["#", "Unit", "Top function", "Fn hits", "Unit hits"], rows))

    if args.tsv:
        with open(args.tsv, "w", encoding="utf-8", newline="\n") as f:
            f.write("unit\tfunction\thits\n")
            for name, (total, funcs) in ranked:
                for fn, hits in funcs.most_common():
                    f.write("%s\t%s\t%d\n" % (name, fn, hits))
        print("TSV written: %s" % args.tsv)

    if failed:
        print("\nFailed diffs (first 5):", file=sys.stderr)
        for name, err in failed[:5]:
            print("  %s: %s" % (name, err), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
