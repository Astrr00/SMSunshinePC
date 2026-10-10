"""Extract every distinct J3D file (BMD/BDL/BMT and animations) from the extracted disc into build/j3d-corpus/.

Usage: python tools/j3d-corpus.py            (needs sms/orig/GMSJ01/extracted/files; ~2 min, then cached)
Output: build/j3d-corpus/<md5>.<ext>, de-duplicated by content, so format checks run over real data only once per file.
Archives are RARC (Yaz0-compressed .szs); only the pieces needed here are parsed.
"""
import glob, hashlib, os, struct, sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", ".."))
src = os.path.join(root, "sms", "orig", "GMSJ01", "extracted", "files")
dst = os.path.join(root, "sunshine-pc", "build", "j3d-corpus")
EXTS = (".bmd", ".bdl", ".bmt", ".bck", ".bca", ".btk", ".brk", ".btp", ".bpk", ".blk", ".bla", ".bxk", ".bva", ".bpa", ".bxa")


def yaz0(data):
    n = struct.unpack(">I", data[4:8])[0]
    out = bytearray(n)
    o, i = 0, 16
    while o < n:
        code = data[i]
        i += 1
        for _ in range(8):
            if o >= n:
                break
            if code & 0x80:
                out[o] = data[i]
                i += 1
                o += 1
            else:
                b1, b2 = data[i], data[i + 1]
                i += 2
                dist = ((b1 & 0xF) << 8) | b2
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = data[i] + 0x12
                    i += 1
                else:
                    cnt += 2
                s = o - dist - 1
                for k in range(cnt):
                    out[o] = out[s + k]
                    o += 1
            code = (code << 1) & 0xFF
    return bytes(out)


def rarc_files(r):
    hlen, fdo = struct.unpack(">II", r[8:16])
    base = hlen
    nn, no, nf, fo, sl, so = struct.unpack(">IIIIII", r[base:base + 24])
    fdbase = hlen + fdo
    nodes = [struct.unpack(">4sIHHI", r[base + no + i * 16: base + no + i * 16 + 16]) for i in range(nn)]
    out = {}

    def name(off):
        s = base + so + off
        return r[s:r.index(b"\0", s)].decode("latin1")

    def walk(ni, path):
        _, _, _, cnt, first = nodes[ni]
        for j in range(cnt):
            p = base + fo + (first + j) * 20
            _, _, ty, nofs, dptr, dsz = struct.unpack(">HHHHII", r[p:p + 16])
            nm = name(nofs)
            if nm in (".", ".."):
                continue
            if ty & 0x200:
                walk(dptr, path + nm + "/")
            else:
                out[path + nm] = r[fdbase + dptr: fdbase + dptr + dsz]

    walk(0, "")
    return out


def main():
    if os.path.isdir(dst) and os.listdir(dst) and "--force" not in sys.argv:
        print(len(os.listdir(dst)), "files cached in", dst)
        return
    os.makedirs(dst, exist_ok=True)
    seen = set()
    for f in sorted(glob.glob(src + "/**/*.szs", recursive=True)):
        d = open(f, "rb").read()
        if d[:4] == b"Yaz0":
            d = yaz0(d)
        for p, b in rarc_files(d).items():
            if p.lower().endswith(EXTS) and len(b) >= 0x20:
                h = hashlib.md5(b).hexdigest()
                if h not in seen:
                    seen.add(h)
                    open(os.path.join(dst, h + os.path.splitext(p)[1].lower()), "wb").write(b)
    print(len(seen), "distinct files written to", dst)


if __name__ == "__main__":
    main()
