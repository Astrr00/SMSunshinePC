"""Floor cells (40 units) from a stage's /map/map.col, for the devscript nav file (SUNSHINE_DEV_NAV).

Usage: python tools/nav/col_grid.py <stage> <out.nav> [ylo yhi]
Writes little-endian int16 triples (cx, cz, y) per cell, one per upward-facing floor triangle centre cell (normal.y >= 0.7).
ylo/yhi (world units) keep only sampled floor heights inside the band, to pick one storey of a multi-level stage.
Where several triangles share a cell inside the band, the highest one wins.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from discfmt import load_szs  # noqa: E402

CELL = 40


def read_col(data):
    vn, vo, gn, go = struct.unpack('>4I', data[:16])
    verts = [struct.unpack('>3f', data[vo + i * 12:vo + i * 12 + 12]) for i in range(vn)]
    tris = []
    for g in range(gn):
        bg, tn, _fl, _u6, io, _uc, _u10, _ao = struct.unpack('>4H4I', data[go + g * 24:go + g * 24 + 24])
        for t in range(tn):
            a, b, c = struct.unpack('>3H', data[io + t * 6:io + t * 6 + 6])
            tris.append((bg, verts[a], verts[b], verts[c]))
    return tris


def normal(a, b, c):
    ux, uy, uz = (b[i] - a[i] for i in range(3))
    vx, vy, vz = (c[i] - a[i] for i in range(3))
    n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
    l = math.sqrt(sum(x * x for x in n)) or 1.0
    return [x / l for x in n]


def cells_of(tri):
    """Yield (cx, cz) of cells whose centre lies inside the XZ projection of the triangle."""
    _bg, A, B, C = tri
    xs = (A[0], B[0], C[0])
    zs = (A[2], B[2], C[2])

    def side(p, q, r):
        return (p[0] - r[0]) * (q[2] - r[2]) - (q[0] - r[0]) * (p[2] - r[2])

    for cx in range(int(math.floor(min(xs) / CELL)), int(math.floor(max(xs) / CELL)) + 1):
        for cz in range(int(math.floor(min(zs) / CELL)), int(math.floor(max(zs) / CELL)) + 1):
            P = ((cx + 0.5) * CELL, 0.0, (cz + 0.5) * CELL)
            d1, d2, d3 = side(P, A, B), side(P, B, C), side(P, C, A)
            if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)):
                yield cx, cz


def wall_cells(tri):
    """Cells touched by a wall triangle's XZ footprint: edges sampled every 10 units, plus the centroid."""
    _bg, A, B, C = tri
    cells = set()
    for P, Q in ((A, B), (B, C), (C, A)):
        n = max(1, int(math.hypot(Q[0] - P[0], Q[2] - P[2]) // 10))
        for i in range(n + 1):
            t = i / n
            x, z = P[0] + (Q[0] - P[0]) * t, P[2] + (Q[2] - P[2]) * t
            cells.add((int(math.floor(x / CELL)), int(math.floor(z / CELL))))
    cx, cz = (A[0] + B[0] + C[0]) / 3.0, (A[2] + B[2] + C[2]) / 3.0
    cells.add((int(math.floor(cx / CELL)), int(math.floor(cz / CELL))))
    return cells


def main():
    stage, out = sys.argv[1], sys.argv[2]
    ylo, yhi = (float(sys.argv[3]), float(sys.argv[4])) if len(sys.argv) > 4 else (-1e9, 1e9)
    tris = read_col(load_szs('data/scene/%s.szs' % stage)['/map/map.col'])
    grid = {}
    for tri in tris:
        _bg, A, B, C = tri
        nx, ny, nz = normal(A, B, C)
        if ny < 0.7:
            continue
        if max(A[1], B[1], C[1]) < ylo or min(A[1], B[1], C[1]) > yhi:
            continue
        for cell in cells_of(tri):
            x, z = ((c + 0.5) * CELL for c in cell)
            y = A[1] - (nx * (x - A[0]) + nz * (z - A[2])) / ny
            if ylo <= y <= yhi and (cell not in grid or grid[cell] < y):
                grid[cell] = y
    # Walls have no floor cell: drop floor cells within one cell of any wall that reaches the band (the chase must not steer into cliffs).
    blocked = set()
    for tri in tris:
        _bg, A, B, C = tri
        if abs(normal(A, B, C)[1]) >= 0.7:
            continue
        if max(A[1], B[1], C[1]) < ylo + 30 or min(A[1], B[1], C[1]) > yhi + 150:
            continue
        for cx, cz in wall_cells(tri):
            blocked.update((cx + dx, cz + dz) for dx in (-1, 0, 1) for dz in (-1, 0, 1))
    for cell in blocked:
        grid.pop(cell, None)
    with open(out, 'wb') as f:
        for (cx, cz), y in sorted(grid.items()):
            f.write(struct.pack('<hhh', cx, cz, int(round(y))))
    xs = [k[0] for k in grid]
    zs = [k[1] for k in grid]
    ys = list(grid.values())
    print('cells=%d triangles=%d x=[%d,%d] z=[%d,%d] y=[%.0f,%.0f]' % (
        len(grid), len(tris), min(xs) * CELL, max(xs) * CELL, min(zs) * CELL, max(zs) * CELL, min(ys), max(ys)))


if __name__ == '__main__':
    main()
