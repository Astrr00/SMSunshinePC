#!/usr/bin/env python3
"""Offline floor-walk check: can Mario walk to each Shine of a scene, without dev verbs?

Usage (repo root):
  python sunshine-pc/tools/bianco_path.py data/scene/bianco0.szs [--shine N] [--step 20] [--pick 50] [--max-states 400000]

Reads /map/map.col and /map/scene.bin in memory only; writes nothing.
Model (file:line refers to sms/src; everything marked Vermutet is not measured in game):
  ground  MapCheck.cpp:249-290: a triangle is ground if mMinY <= y, XZ inside (tol -1), plane <= y+78.
          Order MapMakeList.cpp:111-123 (mMinY descending); first hit wins.
  start   MarioAutodemo.cpp:293-305: no マリオセット位置 node -> position kept; probe at y+25.
  step    probe at y+30 per neighbour point (MarioPhysics.cpp:150).
  walk    normal.y >= 0.34202015 (MarioInit.cpp:500). Mario-through types: MapData.hpp:146-159.
  walls   (height, radius): (30, 25) MarioPhysics.cpp:143-144; (60, 50) MarioMove.cpp:1672.
Not modelled: jumps, ledges, enemies, shine pick-up radius, camera, walk speed, wall push-out.
"""
import argparse
import heapq
import math
import os
import struct
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, 'nav')]
from discfmt import load_szs  # noqa: E402
import col_grid  # noqa: E402

WALK_MIN_NY = 0.34202015
GROUND_REACH = 78.0
START_UP = 25.0
WALK_UP = 30.0
BASE_PROBES = ((30.0, 25.0), (60.0, 50.0))
STEP_PROBES = ((60.0, 50.0),)  # sensitivity: steps up to ~60 allowed
MARIO_THROUGH = {0x100, 0x101, 0x102, 0x103, 0x201, 0x203, 0x400, 0x8400, 0x800, 0x801}
FLOOR_HOLE = {0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x600, 0x800}  # water/pool, OOB, death: no standing (Vermutet)
BUCKET = 64.0
NEIGH = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1))
WAYPOINT_RADIUS = 30
FINAL_RADIUS = 50
TICKS_PER_UNIT = 0.3  # Vermutet: walk speed not measured
TICKS_SLACK = 60
SEG_SAMPLE = 10.0


def _cell(x, z):
    return (math.floor(x / BUCKET), math.floor(z / BUCKET))


def _rd_str(d, p):
    (n,) = struct.unpack('>H', d[p:p + 2])
    return d[p + 2:p + 2 + n], p + 2 + n


def scan_leaves(d, typ):
    """Leaves of one type, found by their type string. Node header: size u32, key u16, type (len u16 + bytes).
    A tree walk is not used: IdxGroup at 26576 holds an extra u32 before its first child, so the walk cannot parse it."""
    key = struct.pack('>H', len(typ)) + typ.encode('ascii')
    out, i = [], d.find(key)
    while i != -1:
        p = i - 6
        if p >= 0:
            (size,) = struct.unpack('>I', d[p:p + 4])
            if 0 < size and p + size <= len(d):
                _, q = _rd_str(d, p + 6)
                name, q = _rd_str(d, q + 2)
                out.append((typ, name.decode('shift_jis', 'replace'), struct.unpack('>3f', d[q:q + 12])))
        i = d.find(key, i + 1)
    return out


def classify(bg, ny):
    if ny >= WALK_MIN_NY:
        if bg in FLOOR_HOLE:
            return 'hole'
        return None if bg in MARIO_THROUGH else 'floor'
    if ny > -WALK_MIN_NY:
        return None if bg in MARIO_THROUGH else 'wall'
    return None  # ceilings / roofs


class Tri:
    def __init__(self, idx, A, B, C, kind, bg):
        self.idx, self.A, self.B, self.C, self.kind, self.bg = idx, A, B, C, kind, bg
        self.nx, self.ny, self.nz = col_grid.normal(A, B, C)
        self.pd = -(self.nx * A[0] + self.ny * A[1] + self.nz * A[2])
        ys = (A[1], B[1], C[1])
        self.minY, self.maxY = min(ys), max(ys)
        xs, zs = (A[0], B[0], C[0]), (A[2], B[2], C[2])
        self.box = (min(xs), max(xs), min(zs), max(zs))

    def y_at(self, x, z):
        return -(self.nx * x + self.nz * z + self.pd) / self.ny


def engine_inside(t, x, z):
    """Same test as checkGroundList (MapCheck.cpp:267-281), tolerance -1."""
    A, B, C = t.A, t.B, t.C
    if (A[2] - z) * (B[0] - A[0]) - (A[0] - x) * (B[2] - A[2]) < -1.0:
        return False
    if (B[2] - z) * (C[0] - B[0]) - (B[0] - x) * (C[2] - B[2]) < -1.0:
        return False
    if (C[2] - z) * (A[0] - C[0]) - (C[0] - x) * (A[2] - C[2]) < -1.0:
        return False
    return True


def _seg_dist(px, pz, ax, az, bx, bz):
    dx, dz = bx - ax, bz - az
    l2 = dx * dx + dz * dz
    t = 0.0 if l2 == 0.0 else max(0.0, min(1.0, ((px - ax) * dx + (pz - az) * dz) / l2))
    return math.hypot(ax + t * dx - px, az + t * dz - pz)


def planar_dist(t, x, z):
    A, B, C = t.A, t.B, t.C
    def s(ax, az, bx, bz):
        return (bx - ax) * (z - az) - (bz - az) * (x - ax)
    d1, d2, d3 = s(A[0], A[2], B[0], B[2]), s(B[0], B[2], C[0], C[2]), s(C[0], C[2], A[0], A[2])
    if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)):
        return 0.0
    return min(_seg_dist(x, z, A[0], A[2], B[0], B[2]),
               _seg_dist(x, z, B[0], B[2], C[0], C[2]),
               _seg_dist(x, z, C[0], C[2], A[0], A[2]))


class World:
    def __init__(self, tris, probes):
        self.tris = tris
        self.probes = probes
        floors = sorted((t for t in tris if t.kind in ('floor', 'hole')), key=lambda t: (-t.minY, -t.maxY))
        walls = [t for t in tris if t.kind == 'wall']
        self.fb = self._index(floors)
        self.wb = self._index(walls)

    @staticmethod
    def _index(items):
        grid = {}
        for t in items:
            x0, x1, z0, z1 = t.box
            for ix in range(math.floor(x0 / BUCKET), math.floor(x1 / BUCKET) + 1):
                for iz in range(math.floor(z0 / BUCKET), math.floor(z1 / BUCKET) + 1):
                    grid.setdefault((ix, iz), []).append(t)
        return grid

    def probe(self, x, y, z):
        for t in self.fb.get(_cell(x, z), ()):
            if t.minY > y or not engine_inside(t, x, z):
                continue
            h = t.y_at(x, z)
            if y - (h - GROUND_REACH) < 0.0:
                continue
            return t, h
        return None, None

    def blocked(self, x, z, feet):
        cands = self.wb.get(_cell(x, z), ())
        for h, r in self.probes:
            yy = feet + h
            for w in cands:
                if w.minY <= yy <= w.maxY and planar_dist(w, x, z) < r:
                    return True
        return False


def search(w, start, step, max_states):
    sx, sy, sz = start
    ix, iz = round(sx / step), round(sz / step)
    t0, _ = w.probe(ix * step, sy + START_UP, iz * step)
    if t0 is None or t0.kind != 'floor':
        return None
    k0 = (ix, iz, t0.idx)
    dist, par = {k0: 0.0}, {k0: None}
    pq = [(0.0, k0)]
    truncated = False
    while pq:
        d, k = heapq.heappop(pq)
        if d > dist[k]:
            continue
        if len(dist) > max_states:
            truncated = True
            break
        ix, iz, fid = k
        x, z = ix * step, iz * step
        yf = w.tris[fid].y_at(x, z)
        for dx, dz in NEIGH:
            jx, jz = ix + dx, iz + dz
            qx, qz = jx * step, jz * step
            g, hq = w.probe(qx, yf + WALK_UP, qz)
            if g is None or g.kind != 'floor':
                continue
            if w.blocked(qx, qz, hq) or w.blocked((x + qx) / 2, (z + qz) / 2, yf):
                continue
            nk = (jx, jz, g.idx)
            nd = d + step * math.hypot(dx, dz)
            if nd < dist.get(nk, math.inf):
                dist[nk], par[nk] = nd, k
                heapq.heappush(pq, (nd, nk))
    return dist, par, truncated


def route(w, res, k, step):
    _, par, _ = res
    keys = []
    while k is not None:
        keys.append(k)
        k = par[k]
    keys.reverse()
    pts = []
    for kx, kz, f in keys:
        x, z = kx * step, kz * step
        pts.append((x, w.tris[f].y_at(x, z), z))
    return pts


def seg_ok(w, a, b):
    ax, ay, az = a
    bx, _, bz = b
    n = max(1, int(math.hypot(bx - ax, bz - az) // SEG_SAMPLE))
    y = ay
    for i in range(1, n + 1):
        x, z = ax + (bx - ax) * i / n, az + (bz - az) * i / n
        g, h = w.probe(x, y + WALK_UP, z)
        if g is None or g.kind != 'floor' or w.blocked(x, z, h):
            return False
        y = h
    return True


def simplify(w, pts):
    out, i = [pts[0]], 0
    while i < len(pts) - 1:
        j = len(pts) - 1
        while j > i + 1 and not seg_ok(w, pts[i], pts[j]):
            j -= 1
        out.append(pts[j])
        i = j
    return out


def walk_lines(wps):
    lines, total = [], 0.0
    for i in range(1, len(wps)):
        (ax, _, az), (bx, _, bz) = wps[i - 1], wps[i]
        seg = math.hypot(bx - ax, bz - az)
        total += seg
        r = FINAL_RADIUS if i == len(wps) - 1 else WAYPOINT_RADIUS
        ticks = math.ceil(seg * TICKS_PER_UNIT) + TICKS_SLACK
        lines.append(f'walkto x={bx:.0f} z={bz:.0f} radius={r} timeout={ticks}')
    return lines, total


def evaluate(w, res, sx, sy, sz, step, pick):
    """Returns (status, state key or None)."""
    if res is None:
        return 'unbekannt: Start ohne begehbaren Boden', None
    dist, _, trunc = res
    near, nearest = [], None
    for k, d in dist.items():
        x, z = k[0] * step, k[1] * step
        dy = sy - w.tris[k[2]].y_at(x, z)
        dxz = math.hypot(x - sx, z - sz)
        if dxz <= pick:
            near.append((dy > 0, d, dxz, dy, k))
        if nearest is None or dxz < nearest[0]:
            nearest = (dxz, dy)
    if near:
        _, d, dxz, dy, k = min(near, key=lambda c: (c[0], c[1]))
        if dy > 0:
            return f'ja, nur mit Sprung >= {dy:.0f} (ungeprueft); Pfad {d:.0f}, XZ {dxz:.0f}', k
        return f'ja; Pfad {d:.0f}, XZ {dxz:.0f}, dy {dy:.0f}', k
    if trunc:
        return f'unbekannt (Suche abgebrochen); naechster XZ-Abstand {nearest[0]:.0f}', None
    return f'nein (exhaustiv); naechster XZ-Abstand {nearest[0]:.0f}, dy {nearest[1]:.0f}', None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('scene')
    ap.add_argument('--shine', type=int)
    ap.add_argument('--step', type=float, default=20.0)
    ap.add_argument('--pick', type=float, default=50.0)
    ap.add_argument('--max-states', type=int, default=400000)
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(errors='replace')
    except AttributeError:
        pass

    z = load_szs(a.scene)
    raw = col_grid.read_col(z['/map/map.col'])
    kept = []
    for bg, A, B, C in raw:
        n = col_grid.normal(A, B, C)
        if abs(n[0]) + abs(n[1]) + abs(n[2]) < 1e-9:
            continue
        kind = classify(bg, n[1])
        if kind:
            kept.append(Tri(len(kept), A, B, C, kind, bg))
    kinds = Counter(t.kind for t in kept)

    scene = z['/map/scene.bin']
    marios = scan_leaves(scene, 'Mario')
    shines = scan_leaves(scene, 'Shine')

    print('Beobachtet')
    print(f'  map.col: {len(raw)} Dreiecke; Typen (top): '
          + ', '.join(f'0x{b:X}:{n}' for b, n in Counter(bg for bg, _, _, _ in raw).most_common(12)))
    print(f'  behalten: floor={kinds["floor"]} hole={kinds["hole"]} wall={kinds["wall"]}')
    base = World(kept, BASE_PROBES)
    fl = [t for t in kept if t.kind == 'floor']
    ok = sum(1 for t in fl if engine_inside(t, (t.A[0] + t.B[0] + t.C[0]) / 3, (t.A[2] + t.B[2] + t.C[2]) / 3))
    print(f'  Winkeltest: {ok}/{len(fl)} Boden-Schwerpunkte bestehen engine_inside')
    print(f'  Byte-Scan Typ-Knoten: Mario={len(marios)} Shine={len(shines)}')
    if not marios:
        print('  kein Mario-Knoten; Abbruch')
        return
    mname, (mx, my, mz) = marios[0][1], marios[0][2]
    print(f'  Mario-Knoten "{mname.encode("ascii", "backslashreplace").decode()}" pos=({mx:.2f},{my:.2f},{mz:.2f})')
    g, h = base.probe(mx, my + START_UP, mz)
    if g is None:
        print('  Start-Probe (y+25): kein Boden')
    else:
        print(f'  Start-Probe (y+25): kind={g.kind} plane_y={h:.2f} ny={g.ny:.3f} bg=0x{g.bg:X}')
    for i, (_, sname, (x, y, zz)) in enumerate(shines):
        print(f'  Shine {i}: pos=({x:.2f},{y:.2f},{zz:.2f})')

    results = {}
    for vname, probes in (('base', BASE_PROBES), ('stufe60', STEP_PROBES), ('nowalls', ())):
        w = World(kept, probes)
        results[vname] = (w, search(w, (mx, my, mz), a.step, a.max_states))

    print('Modell-Varianten (Vermutet): base=Wandproben 30/25 + 60/50; stufe60=nur 60/50; nowalls=Obergrenze ohne Waende')
    for vname, (w, res) in results.items():
        n = len(res[0]) if res else 0
        tr = res[2] if res else None
        print(f'  {vname}: Zustaende={n} abgebrochen={tr}')

    for si, (_, sname, (x, y, zz)) in enumerate(shines):
        if a.shine is not None and si != a.shine:
            continue
        print(f'Shine {si}')
        chosen = None
        for vname in ('base', 'stufe60', 'nowalls'):
            w, res = results[vname]
            status, k = evaluate(w, res, x, y, zz, a.step, a.pick)
            print(f'  [{vname}] {status}')
            if k is not None and chosen is None and vname != 'nowalls':
                chosen = (vname, w, res, k)
        if chosen:
            vname, w, res, k = chosen
            wps = simplify(w, route(w, res, k, a.step))
            lines, total = walk_lines(wps)
            print(f'  Route [{vname}]: {len(wps)} Punkte, Laenge {total:.0f}')
            for ln in lines:
                print('    ' + ln)


if __name__ == '__main__':
    main()
