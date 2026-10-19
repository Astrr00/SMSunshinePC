#!/usr/bin/env python3
"""Generate separate playability and matching graphics for the README.

Usage: python sunshine-pc/tools/status_treemap.py [report.json] [status.svg]
The matching graphic is written beside status.svg as matching.svg.
Uses only the standard library; reads the matrix and build report, not game data.
"""
import csv
import json
import sys
import textwrap
from collections import Counter
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BG, PANEL = "#101b2a", "#19283a"
INK, MUTED = "#f3f7fb", "#b9cbdc"
GREEN, ORANGE, GOLD, GREY = "#5fd5b2", "#ffb181", "#f0d47b", "#42566d"
STATES = {"passed-bounded": GREEN, "open": ORANGE, "external-check-required": GOLD}


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


def rect(x, y, w, h, fill, radius=0, extra=""):
    return f'<rect x="{x:.2f}" y="{y:.2f}" width="{max(w, 0):.2f}" height="{max(h, 0):.2f}" rx="{radius}" fill="{fill}" {extra}/>'


def text(x, y, value, size=18, color=INK, weight=400):
    return f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{color}">{escape(str(value))}</text>'


def svg(title, description, height, parts):
    return '\n'.join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="{height}" viewBox="0 0 1000 {height}" role="img" aria-labelledby="title desc" font-family="Segoe UI,Arial,sans-serif">',
        f'<title id="title">{escape(title)}</title>',
        f'<desc id="desc">{escape(description)}</desc>',
        rect(0, 0, 1000, height, BG, 16), *parts, '</svg>', '',
    ])


def port_rows(path):
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    if not rows or any(not r.get("id") or not r.get("area") or r.get("status") not in STATES for r in rows):
        raise ValueError("Matrix needs nonempty id/area fields and a recognized status for every row")
    if len({r["id"] for r in rows}) != len(rows):
        raise ValueError("Duplicate matrix IDs would inflate the status counts")
    return rows


def port_graphic(rows):
    counts = Counter(r["status"] for r in rows)
    parts = [text(32, 42, "SUNSHINE / WINDOWS PORT", 14, GREEN, 700),
             text(32, 87, "Playability evidence", 36, weight=700),
             text(32, 118, "A complete, unassisted playthrough is still open.", 19, MUTED)]
    for i, (state, label) in enumerate(zip(STATES, ["Passed with bounded evidence", "Open acceptance checks", "External check required"])):
        x = 32 + i * 316
        parts += [rect(x, 146, 304, 116, PANEL, 12), text(x+20, 205, counts[state], 48, STATES[state], 700),
                  text(x+20, 239, label, 17, MUTED)]
    # Equal segments represent matrix rows, not effort or overall completion.
    cell = 936 / len(rows)
    for i, row in enumerate(sorted(rows, key=lambda r: list(STATES).index(r["status"]))):
        parts.append(f'<g><title>{escape(row["id"] + ": " + row["area"] + " — " + row["status"])}</title>'
                     + rect(32+i*cell, 282, cell-3, 10, STATES[row["status"]], 3) + '</g>')
    parts += [text(32, 322, f'{len(rows)} checks with unequal scope. This is not a completion percentage.', 17, MUTED),
              text(32, 369, "Still to verify", 23, weight=700)]
    pending = [r for r in rows if r["status"] == "open"]
    for i, row in enumerate(pending):
        x, y = 32 + (i % 2) * 480, 388 + (i // 2) * 86
        parts += [rect(x, y, 456, 74, PANEL, 10), text(x+16, y+23, row["id"], 12, ORANGE, 700)]
        for j, line in enumerate(textwrap.wrap(row["area"], 43)):
            parts.append(text(x+16, y+47+j*19, line, 17))
    y = 388 + ((len(pending)+1)//2)*86 + 5
    for row in rows:
        if row["status"] == "external-check-required":
            parts += [rect(32, y, 936, 62, PANEL, 10), text(48, y+25, row["id"] + " / EXTERNAL CHECK", 12, GOLD, 700),
                      text(48, y+48, row["area"], 18)]
            y += 76
    parts.append(text(32, y+24, "Source: playability-matrix.tsv · A pass certifies only its recorded test scope.", 16, MUTED))
    return svg("Windows port playability evidence", f'{counts["passed-bounded"]} of {len(rows)} checks passed with bounded evidence; '
               f'{counts["open"]} open and {counts["external-check-required"]} requiring an external check. Not overall completion.', y+48, parts)


def matching_graphic(report):
    totals = report["measures"]
    mf, tf = int(totals["matched_functions"]), int(totals["total_functions"])
    mc, tc = int(totals["matched_code"]), int(totals["total_code"])
    cu, tu = int(totals["complete_units"]), int(totals["total_units"])
    if not (0 <= mf <= tf and tf > 0 and 0 <= mc <= tc and tc > 0 and 0 <= cu <= tu and tu > 0):
        raise ValueError("Invalid matching report totals")
    parts = [text(32, 42, "SUNSHINE / GMSJ01 DECOMPILATION", 14, GREEN, 700),
             text(32, 87, "Matching the original binary", 36, weight=700),
             text(32, 118, "Byte matching measures reconstruction, not Windows playability.", 19, MUTED)]
    metrics = [(f'{100*mf/tf:.1f}%', "Byte-identical functions", f'{mf:,} / {tf:,} functions'),
               (f'{100*mc/tc:.2f}%', "Matched code bytes", f'{mc:,} / {tc:,} bytes'),
               (f'{cu} / {tu}', "Complete units", "Build report classification")]
    for i, (value, label, detail) in enumerate(metrics):
        x = 32+i*316
        parts += [rect(x, 146, 304, 126, PANEL, 12), text(x+20, 198, value, 40, GREEN, 700),
                  text(x+20, 232, label, 18), text(x+20, 255, detail, 15, MUTED)]
    parts += [rect(32, 298, 12, 12, GREEN, 2), text(52, 310, "Byte-identical", 16, MUTED),
              rect(190, 298, 12, 12, GREY, 2), text(210, 310, "Not byte-identical", 16, MUTED),
              text(540, 310, "Cell area = function count in a unit", 16, MUTED)]
    groups = {}
    for u in report["units"]:
        m = u["measures"]
        total, matched = int(m.get("total_functions", 0)), int(m.get("matched_functions", 0))
        if not 0 <= matched <= total:
            raise ValueError(f'Invalid function counts in {u["name"]}')
        names = u["name"].split("/")[1:]
        key = "/".join(names[:2]) if len(names)>2 and names[0]=="JSystem" else (names[0] if names else "main")
        groups.setdefault(key, []).append((total, matched, u["name"]))
    items = sorted(((sum(t for t, _, _ in units), key) for key, units in groups.items()), reverse=True)
    for key, x, y, w, h in squarify(items, 32, 334, 936, 420):
        units = sorted(groups[key], reverse=True)
        for (total, matched, name), ux, uy, uw, uh in squarify([(u[0], u) for u in units], x, y, w, h):
            iw, ih = max(uw-2, 0), max(uh-2, 0)
            parts.append(f'<g><title>{escape(name)}: {matched}/{total} byte-identical functions</title>'
                         + rect(ux+1, uy+1, iw, ih, GREY)
                         + rect(ux+1, uy+1, iw*matched/total, ih, GREEN) + '</g>')
        parts.append(rect(x, y, w, h, "none", extra=f'stroke="{BG}" stroke-width="4"'))
        if w >= 100 and h >= 45:
            name = key.removeprefix("JSystem/")
            limit = int((w-16)/8)
            name = name if len(name)<=limit else name[:limit-1]+"…"
            parts += [rect(x+4, y+4, min(w-8, len(name)*8+16), 25, BG, 4), text(round(x+12, 2), round(y+22, 2), name, 14, INK, 700)]
    parts += [text(32, 786, "Source: sms/build/GMSJ01/report.json · Open the SVG to inspect unit counts.", 16, MUTED),
              text(32, 812, "100% byte matching is not required for the Windows port.", 16, MUTED)]
    return svg("GMSJ01 matching progress", f'{mf:,} of {tf:,} functions byte-identical; {100*mc/tc:.2f}% of code bytes matched; '
               f'{cu} of {tu} units complete. This does not certify gameplay.', 840, parts)


def main():
    report = Path(sys.argv[1]) if len(sys.argv)>1 else ROOT / "sms/build/GMSJ01/report.json"
    out = Path(sys.argv[2]) if len(sys.argv)>2 else ROOT / "sunshine-pc/docs/img/status.svg"
    rows = port_rows(ROOT / "sunshine-pc/docs/playability-matrix.tsv")
    # Validate and render both before writing either artifact.
    graphics = [(out, port_graphic(rows)), (out.with_name("matching.svg"), matching_graphic(json.loads(report.read_text(encoding="utf-8"))))]
    if graphics[0][0] == graphics[1][0]:
        raise ValueError("The status output must not be named matching.svg")
    out.parent.mkdir(parents=True, exist_ok=True)
    for path, content in graphics:
        path.write_text(content, encoding="utf-8", newline="\n")
        print(f"wrote {path} ({len(content.encode('utf-8'))//1024} KiB)")
    print("Matrix: " + ", ".join(f"{state}={sum(r['status']==state for r in rows)}" for state in STATES))


if __name__ == "__main__":
    main()
