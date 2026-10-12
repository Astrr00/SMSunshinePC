"""Dump the JDrama object tree of a stage's /map/scene.bin (names, types, raw class data).

Usage: python tools/nav/scene_dump.py <stage>   (e.g. bianco0)
Walks the same framing as tools/chk_scenebin.py: node = u32 size | u16 typeKey | str type | u16 nameKey | str name | data;
a group is s32 count followed by that many nodes that consume the node exactly. Prints one line per node.
"""
import os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from discfmt import load_szs  # noqa: E402


def rd_str(d, p):
    (n,) = struct.unpack('>H', d[p:p + 2])
    return d[p + 2:p + 2 + n], p + 2 + n


def walk(d, p, depth, out):
    (size,) = struct.unpack('>I', d[p:p + 4])
    nend = p + size
    q = p + 4 + 2
    typ, q = rd_str(d, q)
    q += 2
    name, q = rd_str(d, q)
    t = typ.decode('shift_jis', 'replace')
    n = name.decode('shift_jis', 'replace')
    if q + 4 <= nend:
        (cnt,) = struct.unpack('>i', d[q:q + 4])
        if 0 <= cnt < 100000:
            kids, c = [], q + 4
            try:
                for _ in range(cnt):
                    c = walk(d, c, depth + 1, kids)
                if c == nend:
                    out.append('  ' * depth + 'G %s %s' % (t, n))
                    out.extend(kids)
                    return nend
            except (AssertionError, struct.error, IndexError):
                pass
    out.append('  ' * depth + 'L %s %s size=%d data=%s' % (t, n, size, d[q:nend].hex()))
    return nend


def main():
    stage = sys.argv[1]
    d = load_szs('data/scene/%s.szs' % stage)['/map/scene.bin']
    out = []
    walk(d, 0, 0, out)
    print('\n'.join(out))


if __name__ == '__main__':
    main()
