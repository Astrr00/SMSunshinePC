"""Shared unit queue for decomp agents (claim a whole unit, work it, release it).

  python sunshine-pc/tools/decomp_queue.py claim <agent>            -> prints the next free unit (or NONE)
  python sunshine-pc/tools/decomp_queue.py release <agent> <unit> <note>
  python sunshine-pc/tools/decomp_queue.py hold <agent> <unit>...   (lead: reserve units for a running agent)
  python sunshine-pc/tools/decomp_queue.py free <agent>             (lead: drop all claims of an agent)
  python sunshine-pc/tools/decomp_queue.py status

Order: game-path dirs first, then Enemy, then Animal; inside a tier the units with the most open
functions (non-100 %, not logic-verified in docs/decomp-verified.tsv) come first. Claims older than
2 h expire. State: sms/build/decomp_queue.json (lock: sms/build/decomp_queue.lock).
"""
import json, os, sys, time

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
STATE = os.path.join(ROOT, "sms", "build", "decomp_queue.json")
LOCK = STATE.replace(".json", ".lock")
PATH = ["System", "Player", "Map", "Camera", "NPC", "GC2D", "MoveBG", "Strategic", "MarioUtil", "MSound", "M3DUtil"]
TIER = {d: 0 for d in PATH} | {"Enemy": 1, "Animal": 2}


def locked(fn):
    for _ in range(200):
        try:
            fd = os.open(LOCK, os.O_CREAT | os.O_EXCL)
            break
        except FileExistsError:
            if time.time() - os.path.getmtime(LOCK) > 30:
                os.remove(LOCK)
            time.sleep(0.1)
    else:
        sys.exit("queue lock busy")
    try:
        st = json.load(open(STATE)) if os.path.exists(STATE) else {"claims": {}, "done": []}
        res = fn(st)
        json.dump(st, open(STATE, "w"), indent=1)
        return res
    finally:
        os.close(fd)
        os.remove(LOCK)


def open_units():
    rows = json.load(open(os.path.join(ROOT, "sms", "build", "triage.json")))
    led = set()
    for l in open(os.path.join(ROOT, "sunshine-pc", "docs", "decomp-verified.tsv"), encoding="utf8"):
        c = l.rstrip("\n").split("\t")
        if len(c) >= 4 and c[3].startswith("logic-verified"):
            led.add((c[0].removeprefix("mario/"), c[1].split("(")[0].strip()))
    count = {}
    for f in rows:
        d = f["unit"].split("/")[1]
        if d in TIER and f["match"] is not None and (f["unit"].removeprefix("mario/"), f["fn"].split("(")[0].strip()) not in led:
            count[f["unit"]] = count.get(f["unit"], 0) + 1
    return sorted(count, key=lambda u: (TIER[u.split("/")[1]], -count[u])), count


def claim(agent, unit=None, quick=False):
    """claim <agent>            next open unit (tier order)
    claim <agent> <unit>      that specific unit (CLAIMED_BY <agent> if taken)
    claim <agent> --quick     unit with most >=99 % non-100 functions (fast pad wins)"""
    order, count = open_units()
    if quick:
        rows = json.load(open(os.path.join(ROOT, "sms", "build", "triage.json")))
        near = {}
        for r in rows:
            if r["match"] is not None and r["match"] >= 99 and r["unit"] in count:
                near[r["unit"]] = near.get(r["unit"], 0) + 1
        order = sorted((u for u in order if u in near), key=lambda u: -near[u])

    def f(st):
        now = time.time()
        st["claims"] = {u: c for u, c in st["claims"].items() if now - c["t"] < 7200}
        if unit:
            u = unit if unit.startswith("mario/") else "mario/" + unit
            if u in st["claims"] and st["claims"][u]["agent"] != agent:
                return f"CLAIMED_BY {st['claims'][u]['agent']}"
            st["claims"][u] = {"agent": agent, "t": now}
            return f"{u}\t{count.get(u, 0)} open functions"
        recent = {d["unit"] for d in st["done"] if now - d["t"] < 1200}
        for u in order:
            if u not in st["claims"] and u not in recent:
                st["claims"][u] = {"agent": agent, "t": now}
                return f"{u}\t{count[u]} open functions"
        return "NONE"
    print(locked(f))


def release(agent, unit, note):
    def f(st):
        st["claims"].pop(unit, None)
        st["done"].append({"unit": unit, "agent": agent, "t": time.time(), "note": note})
    locked(f)
    print("released", unit)


def hold(agent, units):
    units = [u if u.startswith("mario/") else "mario/" + u for u in units]
    res = []

    def f(st):
        now = time.time()
        st["claims"] = {u: c for u, c in st["claims"].items() if now - c["t"] < 7200}
        for u in units:
            c = st["claims"].get(u)
            if c and c["agent"] != agent:
                res.append(f"CLAIMED_BY {c['agent']} {u}")
            else:
                st["claims"][u] = {"agent": agent, "t": now}
                res.append(f"held {u}")
    locked(f)
    print("\n".join(res))


def free(agent):
    def f(st):
        st["claims"] = {u: c for u, c in st["claims"].items() if c["agent"] != agent}
    locked(f)


def status():
    st = json.load(open(STATE)) if os.path.exists(STATE) else {"claims": {}, "done": []}
    order, count = open_units()
    out = [f"open units {len(order)}, open functions {sum(count.values())}, claimed {len(st['claims'])}, done {len(st['done'])}"]
    for u, c in st["claims"].items():
        out.append(f"  {c['agent']} {u} {(time.time() - c['t']) / 60:.0f} min")
    sys.stdout.write("\n".join(out) + "\n")



def ledger(unit, fn, pct, status_, agent, evidence):
    """Insert or update one row of docs/decomp-verified.tsv under the queue lock.
    Gate: `logic-verified` needs `branches mapped` (or `no branches`) in the evidence, else it is stored as
    `partial (pending review)`. pct >= 100 is stored as `matched`."""
    try:
        if float(pct) >= 99.9999:
            status_ = "matched"
    except ValueError:
        pass
    ev = evidence.lower()
    if status_.startswith("logic-verified") and not ("branches mapped" in ev or "no branches" in ev or "0 branches" in ev):
        print("WARNING: no 'branches mapped' in evidence -> stored as partial (pending review); add the branch trace and re-ledger")
        status_ = "partial (pending review)"
    path = os.path.join(ROOT, "sunshine-pc", "docs", "decomp-verified.tsv")
    key = lambda u, f: (u.removeprefix("mario/"), f.split("(")[0].strip())

    def f(_st):
        lines = open(path, encoding="utf8").read().splitlines()
        row = "\t".join([unit.removeprefix("mario/"), fn, pct, status_, agent, evidence])
        for i, l in enumerate(lines):
            c = l.split("\t")
            if not l.startswith("#") and len(c) >= 2 and key(c[0], c[1]) == key(unit, fn):
                lines[i] = row
                break
        else:
            lines.append(row)
        open(path, "w", encoding="utf8", newline="\n").write("\n".join(lines) + "\n")
    locked(f)
    print("ledger", unit, fn, status_)


if __name__ == "__main__":
    a = sys.argv[1:]
    {"claim": lambda: claim(a[1], None if len(a) < 3 or a[2] == "--quick" else a[2], "--quick" in a[2:]),
     "release": lambda: release(a[1], a[2], " ".join(a[3:])),
     "hold": lambda: hold(a[1], a[2:]), "free": lambda: free(a[1]), "status": status,
     "ledger": lambda: ledger(a[1], a[2], a[3], a[4], a[5], " ".join(a[6:]))}[a[0]]()
