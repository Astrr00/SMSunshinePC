"""Offline proof for the TGraphGroup (.ral) host swap in sms/src/Enemy/graph.cpp (grSwapGraphGroup).

Layout derived from TGraphGroup::TGraphGroup / getNode / getNodeName and TRailNode:
  GraphDesc[n+1]  {s32 mNodeNum, u32 mNameOffset, u32 mRailNodesOffset}, ended by mNodeNum == 0
  names           NUL-terminated strings, offsets from the buffer start
  TRailNode[]     0x44 bytes each, offsets from the buffer start
Asserts every offset/count is consistent over real files, that the in-place swap (emulated with the same field
list as the C++) reproduces the big-endian values on a little-endian host, that a second pass is a no-op, and that
the idempotency tag cannot occur on disc.

Usage: python chk_ral.py [scene.szs ...]   (default: every data/scene/*.szs)
"""
import glob, math, os, struct, sys
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import discfmt

TAG = 0x4C415221  # GR_SWAPPED_TAG in graph.cpp
NODE_BE = '>3hhIHHHH8H8f'
NODE_LE = '<3hhIHHHH8H8f'
assert struct.calcsize(NODE_BE) == 0x44  # static_assert(sizeof(TRailNode) == 0x44)


def host_swap(buf):
    """Mirror of grSwapGraphGroup on a little-endian host (buf is the raw disc image, modified in place)."""
    num = 0
    while struct.unpack_from('<I', buf, num * 12)[0] != 0:  # zero reads as zero in either byte order
        num += 1
    if struct.unpack_from('<I', buf, num * 12 + 4)[0] == TAG:
        return False
    for i in range(num):
        cnt, no, ro = struct.unpack_from('>iII', buf, i * 12)
        struct.pack_into('<iII', buf, i * 12, cnt, no, ro)
        for j in range(cnt):
            o = ro + j * 0x44
            struct.pack_into(NODE_LE, buf, o, *struct.unpack_from(NODE_BE, buf, o))
    struct.pack_into('<I', buf, num * 12 + 4, TAG)
    return True


def check(path):
    fs = discfmt.load_szs(path)
    b = fs.get('/map/scene.ral')
    name = os.path.basename(path)
    assert b is not None, name + ': no /map/scene.ral'
    n = 0
    descs = []
    while True:
        assert (n + 1) * 12 <= len(b), name + ': descriptor table runs off the buffer'
        d = struct.unpack_from('>iII', b, n * 12)
        n += 1
        if d[0] == 0:
            term = d
            break
        descs.append(d)
    table_end = n * 12
    assert term[1] != TAG, name + ': tag value present on disc'
    assert TAG > len(b), name + ': tag could be a valid offset'

    spans = []
    nodes_total = 0
    for gi, (cnt, no, ro) in enumerate(descs):
        ctx = '%s graph %d' % (name, gi)
        assert cnt > 0, ctx + ': count'
        assert table_end <= no < len(b), ctx + ': name offset'
        end = b.index(b'\0', no)
        nm = b[no:end]
        assert nm and all(32 <= c < 127 for c in nm), ctx + ': name %r' % nm
        assert ro % 4 == 0 and table_end <= ro and ro + cnt * 0x44 <= len(b), ctx + ': node range'
        spans.append((ro, ro + cnt * 0x44, ctx))
        spans.append((no, end + 1, ctx + ' name'))
        for j in range(cnt):
            v = struct.unpack_from(NODE_BE, b, ro + j * 0x44)
            conn_num, conns, periods = v[3], v[9:17], v[17:25]
            assert 0 <= conn_num <= 8, '%s node %d: connection count %d' % (ctx, j, conn_num)
            assert all(c < cnt for c in conns[:conn_num]), '%s node %d: connection index' % (ctx, j)
            assert all(math.isfinite(p) for p in periods), '%s node %d: period' % (ctx, j)
        nodes_total += cnt
    spans.sort()
    for a, c in zip(spans, spans[1:]):  # disjoint => the in-place swap can never touch a record twice
        assert a[1] <= c[0], '%s: %s overlaps %s' % (name, a[2], c[2])

    # Emulate the host swap, compare against the big-endian parse, then prove the second pass is a no-op.
    host = bytearray(b)
    assert host_swap(host) is True
    for cnt, no, ro in descs:
        for j in range(cnt):
            o = ro + j * 0x44
            assert struct.unpack_from(NODE_LE, host, o) == struct.unpack_from(NODE_BE, b, o), name + ': swap mismatch'
    for gi, d in enumerate(descs):
        assert struct.unpack_from('<iII', host, gi * 12) == d, name + ': descriptor swap mismatch'
    once = bytes(host)
    assert host_swap(host) is False and bytes(host) == once, name + ': second pass changed data'
    return len(descs), nodes_total


if __name__ == '__main__':
    scenes = sys.argv[1:] or sorted(glob.glob(os.path.join(discfmt.ROOT, 'data', 'scene', '*.szs')))
    graphs = nodes = 0
    with ProcessPoolExecutor() as ex:
        for g, nn in ex.map(check, scenes):
            graphs += g
            nodes += nn
    print('chk_ral OK: %d scenes, %d graphs, %d rail nodes' % (len(scenes), graphs, nodes))
