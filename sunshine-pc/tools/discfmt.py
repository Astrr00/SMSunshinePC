"""Offline helpers to inspect Super Mario Sunshine disc data (Yaz0 .szs -> RARC -> files).

Used by the format checkers (chk_*.py) to prove the on-disc layouts the host loaders rely on.
Usage:  python discfmt.py <file.szs> [list|extract <dir>]
"""
import os, struct, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', 'sms', 'orig', 'GMSJ01', 'extracted', 'files'))


def yaz0(data):
    assert data[:4] == b'Yaz0', data[:4]
    n = struct.unpack('>I', data[4:8])[0]
    out = bytearray()
    i = 16
    while len(out) < n:
        code = data[i]; i += 1
        for _ in range(8):
            if len(out) >= n:
                break
            if code & 0x80:
                out.append(data[i]); i += 1
            else:
                b1, b2 = data[i], data[i + 1]; i += 2
                dist = ((b1 & 0xF) << 8) | b2
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = data[i] + 0x12; i += 1
                else:
                    cnt += 2
                src = len(out) - dist - 1
                for k in range(cnt):
                    out.append(out[src + k])
            code = (code << 1) & 0xFF
    return bytes(out)


def rarc_files(r):
    """Return {'/dir/name': bytes} for every file of an uncompressed RARC image."""
    assert r[:4] == b'RARC', r[:4]
    _, flen, hlen, fdo, fdl = struct.unpack('>4sIIII', r[:20])
    base = hlen
    nn, no, nf, fo, sl, so = struct.unpack('>IIIIII', r[base:base + 24])
    names = r[base + so:base + so + sl]

    def nm(off):
        e = names.index(b'\0', off)
        return names[off:e].decode('shift_jis', 'replace')

    nodes = []
    for i in range(nn):
        t, noff, h, cnt, first = struct.unpack('>4sIHHI', r[base + no + i * 16:base + no + i * 16 + 16])
        nodes.append((t, nm(noff), cnt, first))
    out = {}

    def walk(ni, prefix):
        _, name, cnt, first = nodes[ni]
        for j in range(first, first + cnt):
            fid, fh, fl_name, doff, dsz = struct.unpack('>HHIII', r[base + fo + j * 20:base + fo + j * 20 + 16])
            flags = fl_name >> 24
            n = nm(fl_name & 0xFFFFFF)
            if n in ('.', '..'):
                continue
            if flags & 2:
                walk(doff, prefix + n + '/')
            else:
                out[prefix + n] = r[0x20 + fdo + doff:0x20 + fdo + doff + dsz]

    walk(0, '/')
    return out


_cache = {}


def load_szs(path):
    if not os.path.isabs(path) and not os.path.exists(path):
        path = os.path.join(ROOT, path)
    if path not in _cache:
        d = open(path, 'rb').read()
        if d[:4] == b'Yaz0':
            d = yaz0(d)
        _cache[path] = rarc_files(d)
    return _cache[path]


if __name__ == '__main__':
    files = load_szs(sys.argv[1])
    if len(sys.argv) > 2 and sys.argv[2] == 'extract':
        for k, v in files.items():
            p = os.path.join(sys.argv[3], k.lstrip('/'))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(v)
    else:
        for k, v in sorted(files.items()):
            print('%8d %s' % (len(v), k))
