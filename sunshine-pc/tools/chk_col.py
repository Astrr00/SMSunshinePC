"""Offline check of the .col collision layout used by TMapCollisionBase::init (MapCollisionEntry.cpp).
Disc layout (big-endian): header 4 x u32 {vertexNum, vertexOfs, groupNum, groupOfs}; vertices 3 x f32; groups of 0x18
bytes {u16 bgType, s16 triNum, u16 flags, u16 unk6 (0xFFFF on disc), u32 indicesOfs, u32 unkCOfs, u32 unk10Ofs,
u32 additionalOfs}; indices s16[3*triNum]; unkC u8[triNum]; unk10 u8[triNum]; additional s16[triNum] when
additionalOfs != 0.
1. parse the big-endian bytes and assert ranges/counts are consistent,
2. replay swapCollisionData() (same steps as the C++) on a copy, assert no byte is swapped twice and that the result
   read back as little-endian equals the big-endian parse (so every field is covered exactly once),
3. assert the idempotency marker: first two bytes zero on disc, nonzero after the swap.
Usage: python chk_col.py [scene.szs ...]   (default: several scenes)"""
import struct, sys, math
from discfmt import load_szs


def parse_be(name, d):
    vn, vo, gn, go = struct.unpack('>4I', d[:16])
    assert 1 <= vn <= 0xFFFF and vo + vn * 12 <= len(d), (name, 'vertices', vn, vo)
    verts = [struct.unpack('>3f', d[vo + i * 12:vo + i * 12 + 12]) for i in range(vn)]
    for i, v in enumerate(verts):
        assert all(math.isfinite(x) and abs(x) < 1e7 for x in v), (name, 'vertex', i, v)
    assert go + gn * 0x18 <= len(d), (name, 'groups')
    groups = []
    for g in range(gn):
        bg, tn, fl, u6, io, co, ao, ado = struct.unpack('>HhHHIIII', d[go + g * 0x18:go + g * 0x18 + 0x18])
        assert tn >= 0 and io + tn * 6 <= len(d) and co + tn <= len(d) and ao + tn <= len(d), (name, 'group', g)
        idx = [struct.unpack('>h', d[io + t * 2:io + t * 2 + 2])[0] for t in range(tn * 3)]
        assert all(0 <= x < vn for x in idx), (name, 'index out of range', g)
        if fl & 1:
            assert ado, (name, 'flag 1 without additional data', g)
        add = [struct.unpack('>h', d[ado + t * 2:ado + t * 2 + 2])[0] for t in range(tn)] if ado else None
        groups.append((bg, tn, fl, u6, io, co, ao, ado, idx, add))
    return (vn, vo, gn, go), verts, groups


def swap_like_cpp(d):
    """Mirror of swapCollisionData(); returns the swapped copy and the list of swapped (offset, size) spans."""
    b = bytearray(d)
    spans = []

    def sw(off, n):
        spans.append((off, n))
        b[off:off + n] = b[off:off + n][::-1]

    assert not (b[0] | b[1]), 'marker: disc image must look unswapped'
    for i in range(4):
        sw(i * 4, 4)
    vn, vo, gn, go = struct.unpack('<4I', b[:16])
    for i in range(vn * 3):
        sw(vo + i * 4, 4)
    for g in range(gn):
        rec = go + g * 0x18
        for i in range(4):
            sw(rec + i * 2, 2)
        for i in range(4):
            sw(rec + 8 + i * 4, 4)
        _, tn, _, _, io, _, _, ado = struct.unpack('<HhHHIIII', b[rec:rec + 0x18])
        for i in range(tn * 3):
            sw(io + i * 2, 2)
        if ado:
            for i in range(tn):
                sw(ado + i * 2, 2)
    assert (b[0] | b[1]), 'marker: swapped image must look swapped'
    return bytes(b), spans


def check(name, d):
    hdr, verts, groups = parse_be(name, d)
    sw, spans = swap_like_cpp(d)
    owned = {}
    for off, n in spans:
        for k in range(off, off + n):
            assert k not in owned, (name, 'byte swapped twice (overlapping arrays)', k)
            owned[k] = 1
    vn, vo, gn, go = struct.unpack('<4I', sw[:16])
    assert (vn, vo, gn, go) == hdr, name
    assert [struct.unpack('<3f', sw[vo + i * 12:vo + i * 12 + 12]) for i in range(vn)] == verts, name
    for g, (bg, tn, fl, u6, io, co, ao, ado, idx, add) in enumerate(groups):
        r = struct.unpack('<HhHHIIII', sw[go + g * 0x18:go + g * 0x18 + 0x18])
        assert r == (bg, tn, fl, u6, io, co, ao, ado), (name, g)
        assert list(struct.unpack('<%dh' % (tn * 3), sw[io:io + tn * 6])) == idx, (name, g)
        if ado:
            assert list(struct.unpack('<%dh' % tn, sw[ado:ado + tn * 2])) == add, (name, g)
    return hdr[0], hdr[2], sum(g[1] for g in groups)


scenes = sys.argv[1:] or ['data/scene/%s.szs' % s for s in
                          ('bianco0', 'delfino0', 'mare0', 'airport0', 'monte0', 'dolpic0', 'coro_ex0', 'mamma0')]
n = 0
big = (0,)
for s in scenes:
    for k, d in load_szs(s).items():
        if k.endswith('.col'):
            vn, gn, tris = check(s + k, d)
            n += 1
            if vn > big[0]:
                big = (vn, s + k, gn, tris)
print('checked', n, '.col files; largest:', big)
