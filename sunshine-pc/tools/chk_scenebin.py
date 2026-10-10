"""Offline check of the JDrama object stream framing (scene.bin, tables.bin, scenecmn.bin, stageArc.bin, PerformLists.bin).
Layout (big-endian), as read by TNameRef::getType/genObject/load and TNameRefPtrListT::load:
  node  = u32 size (includes itself) | u16 typeKey | string type | u16 nameKey | string name | class data (size-4-... bytes)
  string = u16 length | bytes (no terminator)
  group class data = s32 childCount | childCount nodes
The checker walks the tree: a node is treated as a group when its class data parses as s32 count followed by exactly
that many consistent nodes that consume the node completely. Leaf nodes only get their framing checked.
Also checks the nameKey equals TNameRef::calcKeyCode(name) (hash = 3*h + c, u16), which proves the u16 keycode
and the string framing are decoded in the right byte order.
Usage: python chk_scenebin.py"""
import struct, sys, collections
from discfmt import load_szs, ROOT
import os

stats = collections.Counter()


def calc_key(name):
    h = 0
    for c in name:
        h = (h * 3 + c) & 0xFFFFFFFF
    return h & 0xFFFF


def rd_str(d, p, end):
    (n,) = struct.unpack('>H', d[p:p + 2])
    assert p + 2 + n <= end, 'string out of range'
    return d[p + 2:p + 2 + n], p + 2 + n


def parse_node(d, p, end, depth, path):
    (size,) = struct.unpack('>I', d[p:p + 4])
    assert size >= 4 and p + size <= end, (path, 'node size', size, p, end)
    q = p + 4
    nend = p + size
    (tkey,) = struct.unpack('>H', d[q:q + 2])
    typ, q = rd_str(d, q + 2, nend)
    (nkey,) = struct.unpack('>H', d[q:q + 2])
    name, q = rd_str(d, q + 2, nend)
    assert nkey == calc_key(name), (path, typ, name, hex(nkey), hex(calc_key(name)))
    stats[typ.decode('ascii', 'replace')] += 1
    # group attempt
    if q + 4 <= nend:
        (cnt,) = struct.unpack('>i', d[q:q + 4])
        if 0 <= cnt < 100000:
            try:
                c = q + 4
                for _ in range(cnt):
                    c = parse_node(d, c, nend, depth + 1, path + '/' + name.decode('shift_jis', 'replace'))
                if c == nend:
                    stats['<group>'] += 1
                    return nend
            except (AssertionError, struct.error, IndexError):
                pass
    stats['<leaf>'] += 1
    return nend


def check(label, d):
    # root node: size field may be the node size; the file may carry trailing padding
    (size,) = struct.unpack('>I', d[:4])
    assert 4 < size <= len(d), (label, size, len(d))
    end = parse_node(d, 0, len(d), 0, label)
    assert end == size
    return size


files = []
for s in ['bianco0', 'delfino0', 'mare0', 'airport0', 'monte0', 'dolpic0', 'mamma0', 'casino0', 'coro_ex0', 'bianco1']:
    f = load_szs('data/scene/%s.szs' % s)
    for k in ('/map/scene.bin', '/map/tables.bin'):
        if k in f:
            files.append((s + k, f[k]))
for n in ('scenecmn.bin', 'stageArc.bin', 'PerformLists.bin'):
    p = os.path.join(ROOT, 'data', n)
    if os.path.exists(p):
        files.append((n, open(p, 'rb').read()))
for label, d in files:
    try:
        sz = check(label, d)
        print('ok  %-28s size=%d/%d' % (label, sz, len(d)))
    except AssertionError as e:
        print('FAIL', label, str(e)[:160])
print(sorted(stats.items(), key=lambda kv: -kv[1])[:25])
