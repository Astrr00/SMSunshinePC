#!/usr/bin/env python3
"""Render the README status treemap (docs/img/status.svg).

Left: decomp functions per library/unit from the objdiff report (green = byte-identical share).
Right: port rows from the README status table (colour by percent).
Usage (repo root): python sunshine-pc/tools/status_treemap.py [report.json] [out.svg]
Stdlib only; reads no game data.
"""
import json, re, sys
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "sms/build/GMSJ01/report.json"
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "sunshine-pc/docs/img/status.svg"
BG, CELL_GAP = "#0d1117", 1.0
GREEN, GREY = "#2da44e", "#6e7681"


def squarify(items, x, y, w, h):
    """items: [(weight, payload)] sorted desc. Returns [(payload, x, y, w, h)]."""
    out, items = [], [i for i in items if i[0] > 0]
    total = sum(i[0] for i in items)
    if not items or w <= 0 or h <= 0:
        return out
    scale = w * h / total
    items = [(i[0] * scale, i[1]) for i in items]
    while items:
        side = min(w, h)
        row, rest = [items[0]], items[1:]

        def worst(r):
            s = sum(a for a, _ in r)
            return max(max(side * side * a / (s * s), s * s / (side * side * a)) for a, _ in r)

        while rest and worst(row + [rest[0]]) <= worst(row):
            row.append(rest.pop(0))
        s = sum(a for a, _ in row)
        if w >= h:  # column on the left
            cw = s / h
            cy = y
            for a, p in row:
                ch = a / cw
                out.append((p, x, cy, cw, ch))
                cy += ch
            x, w = x + cw, w - cw
        else:  # row on top
            rh = s / w
            cx = x
            for a, p in row:
                cwid = a / rh
                out.append((p, cx, y, cwid, rh))
                cx += cwid
            y, h = y + rh, h - rh
        items = rest
    return out


def rect(x, y, w, h, fill, extra=""):
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{max(w,0):.1f}" height="{max(h,0):.1f}" fill="{fill}"{extra}/>'


def label(x, y, w, h, text, size=11, color="#fff", bold=False):
    n = int((w - 6) / (size * 0.6))
    if n < 3 or h < size + 4:
        return ""
    t = text if len(text) <= n else text[: n - 1] + "…"
    wt = ' font-weight="700"' if bold else ""
    return (f'<text x="{x+4:.1f}" y="{y+size+2:.1f}" font-size="{size}"{wt} fill="{color}" '
            f'stroke="#000" stroke-opacity=".55" stroke-width="3" paint-order="stroke">{escape(t)}</text>')


def decomp_panel(x0, y0, w, h):
    rep = json.load(open(REPORT))
    groups = {}
    for u in rep["units"]:
        m = u["measures"]
        parts = u["name"].split("/")[1:]  # drop "mario"
        key = "/".join(parts[:2]) if parts and parts[0] == "JSystem" and len(parts) > 2 else (parts[0] if parts else "main")
        groups.setdefault(key, []).append((m.get("total_functions", 0), m.get("matched_functions", 0), parts[-1]))
    tot = rep["measures"]
    s = []
    glist = sorted(((sum(t for t, _, _ in v), k) for k, v in groups.items()), reverse=True)
    for (gx, gy, gw, gh), k in [((a, b, c, d), p) for p, a, b, c, d in squarify(glist, x0, y0, w, h)]:
        units = sorted(groups[k], reverse=True)
        for (t, mt, name), ux, uy, uw, uh in [(p, a, b, c, d) for p, a, b, c, d in squarify([(u[0], u) for u in units], gx, gy, gw, gh)]:
            frac = mt / t if t else 1.0
            s.append(rect(ux + CELL_GAP, uy + CELL_GAP, uw - 2 * CELL_GAP, uh - 2 * CELL_GAP, GREY))
            s.append(rect(ux + CELL_GAP, uy + CELL_GAP, (uw - 2 * CELL_GAP) * frac, uh - 2 * CELL_GAP, GREEN))
        s.append(rect(gx, gy, gw, gh, "none", ' stroke="#0d1117" stroke-width="3"'))
        s.append(label(gx, gy, gw, gh, k, 15, "#fff", True))
    return "".join(s), tot["matched_functions"], tot["total_functions"]


def port_rows():
    rows = []
    for line in (ROOT / "README.md").read_text(encoding="utf-8").splitlines():
        c = [x.strip() for x in line.strip().strip("|").split("|")]
        if len(c) >= 4 and re.fullmatch(r"\d+", c[2]) and c[1] in ("done", "partial", "open", "blocked"):
            rows.append((c[0], c[1], int(c[2])))
    return rows


def port_color(state, pct):
    if state == "blocked":
        return "#8b949e"
    if pct >= 100:
        return GREEN
    # partial/open: red-orange (0) to yellow-green (99)
    r = int(200 - pct * 1.0)
    g = int(80 + pct * 1.2)
    return f"rgb({r},{g},50)"


def port_panel(x0, y0, w, h):
    rows = port_rows()
    items = [(1.0, r) for r in rows]
    s = []
    for (name, state, pct), x, y, cw, ch in squarify(items, x0, y0, w, h):
        s.append(rect(x + 1, y + 1, cw - 2, ch - 2, port_color(state, pct)))
        words, lines, cur = re.split(r"[,(]", name)[0].split(), [], ""
        per = max(int((cw - 10) / 9.0), 4)
        for wd in words:
            if cur and len(cur) + 1 + len(wd) > per:
                lines.append(cur)
                cur = wd
            else:
                cur = (cur + " " + wd).strip()
        lines.append(cur)
        for i, ln in enumerate(lines[:3]):
            s.append(label(x + 1, y + 1 + i * 17, cw - 2, ch - 2, ln, 15, "#fff", True))
        s.append(label(x + 1, y + 1 + min(len(lines), 3) * 17, cw - 2, ch - 2, f"{pct} %", 20, "#fff"))
    mean = sum(p for _, _, p in rows) / max(len(rows), 1)
    return "".join(s), mean, len(rows)


def main():
    W, H, pw = 1000, 1060, 980
    dh, ph = 540, 400
    dp, mf, tf = decomp_panel(10, 70, pw, dh)
    py = 70 + dh + 50
    pp, mean, n = port_panel(10, py, pw, ph)
    svg = (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
        f'font-family="Segoe UI,Helvetica,Arial,sans-serif">{rect(0,0,W,H,BG)}'
        f'<text x="10" y="28" font-size="22" font-weight="700" fill="#fff">Status</text>'
        f'<text x="10" y="54" font-size="16" fill="#c9d1d9">Decompiled functions: {100*mf/tf:.1f}% ({mf}/{tf}), cell = unit, green = byte-identical share</text>'
        f'<text x="10" y="{py-14}" font-size="16" fill="#c9d1d9">Port rows: mean {mean:.0f}% ({n} rows), colour = percent</text>'
        f"{dp}{pp}</svg>"
    )
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(svg, encoding="utf-8", newline="\n")
    print(f"wrote {OUT} ({len(svg)//1024} KiB): decomp {mf}/{tf}, port mean {mean:.1f} over {n} rows")


if __name__ == "__main__":
    main()
