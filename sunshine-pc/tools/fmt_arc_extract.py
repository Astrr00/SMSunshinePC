"""Offline helper for the format checks: Yaz0 + RARC extraction of the disc archives (.szs) into a directory.

Usage: python tools/fmt_arc_extract.py <out dir> [archive .szs ...]   (default: every non-scene .szs plus 6 scenes)
Only used to produce real bytes for the offline checkers (fmtcheck_*.py); the game does not use it.
"""
import glob
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "sms", "orig", "GMSJ01", "extracted", "files"))


def yaz0(data):
    assert data[:4] == b"Yaz0", data[:4]
    n = struct.unpack(">I", data[4:8])[0]
    out = bytearray(n)
    o = 0
    i = 16
    ln = len(data)
    while o < n:
        code = data[i]
        i += 1
        for _ in range(8):
            if o >= n:
                break
            if code & 0x80:
                out[o] = data[i]
                o += 1
                i += 1
            else:
                b1 = data[i]
                b2 = data[i + 1]
                i += 2
                src = o - (((b1 & 0xF) << 8) | b2) - 1
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = data[i] + 0x12
                    i += 1
                else:
                    cnt += 2
                if src + cnt <= o:
                    out[o:o + cnt] = out[src:src + cnt]
                else:
                    for k in range(cnt):
                        out[o + k] = out[src + k]
                o += cnt
            code = (code << 1) & 0xFF
    return bytes(out)


def rarc_files(r):
    """Yield (path, bytes) for every file of a decompressed RARC image."""
    assert r[:4] == b"RARC", r[:4]
    hlen, fdo, fdl = struct.unpack(">III", r[8:20])[0:3]
    base = hlen
    nn, no, nf, fo, sl, so = struct.unpack(">IIIIII", r[base:base + 24])
    names = base + so
    dirs = []
    for d in range(nn):
        _id, noff, _h, cnt, first = struct.unpack(">IIHHI", r[base + no + d * 16:base + no + d * 16 + 16])
        dirs.append((r[names + noff:r.index(b"\0", names + noff)].decode("ascii", "replace"), cnt, first))
    # build dir paths via the parent links in the file nodes
    def path_of(di, cache={}):
        return dirs[di][0]
    out = []
    parent = {}
    nodes = []
    for f in range(nf):
        b = base + fo + f * 20
        fid, _h, ftype, noff, dataoff, dsize = struct.unpack(">HHHHII", r[b:b + 16])
        ftype >>= 8
        nodes.append((fid, ftype, noff, dataoff, dsize))
    for di, (dn, cnt, first) in enumerate(dirs):
        for k in range(cnt):
            fid, ftype, noff, dataoff, dsize = nodes[first + k]
            nm = r[names + noff:r.index(b"\0", names + noff)].decode("ascii", "replace")
            if nm in (".", ".."):
                continue
            if ftype & 0x2:  # directory node: dataoff = dir index
                parent[dataoff] = di
    def full(di):
        p = []
        while True:
            p.append(dirs[di][0])
            if di not in parent:
                break
            di = parent[di]
        return "/".join(reversed(p))
    for di, (dn, cnt, first) in enumerate(dirs):
        for k in range(cnt):
            fid, ftype, noff, dataoff, dsize = nodes[first + k]
            nm = r[names + noff:r.index(b"\0", names + noff)].decode("ascii", "replace")
            if ftype & 0x2:
                continue
            s = fdo + hlen + dataoff
            yield full(di) + "/" + nm, r[s:s + dsize]


def extract(arc, out):
    d = open(arc, "rb").read()
    r = yaz0(d) if d[:4] == b"Yaz0" else d
    n = 0
    for p, b in rarc_files(r):
        dst = os.path.join(out, os.path.basename(arc), p)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(b)
        n += 1
    return n


if __name__ == "__main__":
    out = sys.argv[1]
    arcs = sys.argv[2:]
    if not arcs:
        arcs = [f for f in sorted(glob.glob(ROOT + "/data/*.szs"))]
        arcs += [ROOT + "/data/scene/" + s + ".szs" for s in ("bianco0", "delfino0", "option", "mare0", "sirena0", "pinnaBeach0")]
    for a in arcs:
        print(a, extract(a, out), flush=True)
