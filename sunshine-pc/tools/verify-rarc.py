"""Check the host RARC + Yaz0/Yay0 path against an independent Python implementation.

Usage: python tools/verify-rarc.py <build name>      (after tools\\rarc_test.cmd <build name>)
For several real .szs archives from the extracted disc (sms/orig/GMSJ01/extracted/files) it decodes the file with its
own Yaz0 decoder and parses the RARC itself, runs rarc_test.exe (the real JKRDecomp + JKRMemArchive code compiled for
the host) and compares: decoded size and CRC, and every directory/file path with size and CRC (.bti files with the header swapped).
It also runs the three file-static stream decoders (JKRDvdRipper, JKRAram, JKRDvdAramRipper) via ripper_test.cpp.
The smallest archives are re-encoded as Yay0 (a simple encoder written here) to cover decodeSZP.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", ".."))
data_dir = os.path.join(root, "sms", "orig", "GMSJ01", "extracted", "files", "data")
exe = os.path.join(tempfile.gettempdir(), "sunshine_rarc_test", "rarc_test.exe")
NAMES = ["message.szs", "endsave.szs", "title.szs", "nintendo.szs", "params.szs", "scene/airport0.szs"]


def yaz0_decode(d):
    assert d[:4] == b"Yaz0"
    n = struct.unpack(">I", d[4:8])[0]
    out = bytearray()
    i = 16
    while len(out) < n:
        code = d[i]
        i += 1
        for _ in range(8):
            if len(out) >= n:
                break
            if code & 0x80:
                out.append(d[i])
                i += 1
            else:
                b1, b2 = d[i], d[i + 1]
                i += 2
                src = len(out) - (((b1 & 15) << 8) | b2) - 1
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = d[i] + 0x12
                    i += 1
                else:
                    cnt += 2
                for k in range(cnt):
                    out.append(out[src + k])
            code = (code << 1) & 0xFF
    return bytes(out)


def yay0_encode(d):
    masks, links, lit, bits, pos = [], [], bytearray(), [], 0
    table = {}
    while pos < len(d):
        best, boff = 0, 0
        for c in reversed(table.get(d[pos:pos + 3], [])[-32:]):
            if pos - c > 0x1000:
                break
            n = 0
            while n < 0x111 and pos + n < len(d) and d[c + n] == d[pos + n]:
                n += 1
            if n > best:
                best, boff = n, pos - c
        if best >= 3:
            bits.append(0)
            if best < 18:
                links.append(((best - 2) << 12) | (boff - 1))
            else:
                links.append(boff - 1)
                lit.append(best - 0x12)
            step = best
        else:
            bits.append(1)
            lit.append(d[pos])
            step = 1
        for p in range(pos, pos + step):
            table.setdefault(d[p:p + 3], []).append(p)
        pos += step
    while len(bits) % 32:
        bits.append(0)
    for i in range(0, len(bits), 32):
        w = 0
        for b in bits[i:i + 32]:
            w = (w << 1) | b
        masks.append(w)
    link_off = 16 + 4 * len(masks)
    chunk_off = link_off + 2 * len(links)
    return (b"Yay0" + struct.pack(">III", len(d), link_off, chunk_off) + b"".join(struct.pack(">I", m) for m in masks)
            + b"".join(struct.pack(">H", l) for l in links) + bytes(lit))


def swap_bti_header(b):
    """What swapResTIMG leaves in memory on a little-endian host: multi-byte header fields byte-reversed in place,
    padding byte 0x19 = 0xDE (the idempotency marker). Image and palette bytes stay as they are."""
    h = bytearray(b)
    for off, n in ((2, 2), (4, 2), (0xA, 2), (0xC, 4), (0x1A, 2), (0x1C, 4)):
        h[off:off + n] = h[off:off + n][::-1]
    h[0x19] = 0xDE
    return bytes(h)


def rarc_listing(r):
    hlen, fdo = struct.unpack(">II", r[8:16])
    nn, no, nf, fo, _, so = struct.unpack(">6I", r[hlen:hlen + 24])
    base = hlen

    def name(off):
        e = r.index(b"\0", base + so + off)
        return r[base + so + off:e].decode("latin-1")

    lines = ["archive nodes %d files %d volume %s" % (nn, nf, name(struct.unpack(">I", r[base + no + 4:base + no + 8])[0]))]

    def walk(di, prefix):
        _, _, _, cnt, first = struct.unpack(">4sIHHI", r[base + no + 16 * di:base + no + 16 * di + 16])
        for i in range(cnt):
            fid, h, fn, doff, size, _ = struct.unpack(">HHIIII", r[base + fo + 20 * (first + i):base + fo + 20 * (first + i) + 20])
            n = name(fn & 0xFFFFFF)
            p = prefix + "/" + n
            if fn >> 24 & 2:
                if n in (".", ".."):
                    continue
                lines.append("D " + p)
                walk(doff, p)
            else:
                body = r[hlen + fdo + doff:hlen + fdo + doff + size]
                if n.lower().endswith(".bti") and size >= 0x20:
                    body = swap_bti_header(body)
                lines.append("F %s %d %08x" % (p, size, zlib.crc32(body)))

    walk(0, "")
    first_name = name(struct.unpack(">HHI", r[base + fo:base + fo + 8])[2] & 0xFFFFFF)
    return lines, "remount files %d first %s" % (nf, first_name)


def run_case(path, label):
    raw = open(path, "rb").read()
    dec = yaz0_decode(raw)
    crc = "%08x" % zlib.crc32(dec)
    exp = ["decoded %d %s" % (len(dec), crc)] + ["ripper " + crc] * 3  # JKRDvdRipper, JKRAram, JKRDvdAramRipper
    listing, remount = rarc_listing(dec)
    exp += listing + [remount]
    r = subprocess.run([exe, path], capture_output=True, text=True)
    got = r.stdout.splitlines()
    ok = got == exp and r.returncode == 0
    print("%-4s %s (%d bytes decoded, %d lines)" % ("ok" if ok else "FAIL", label, len(dec), len(exp)))
    if not ok:
        print("  rc", r.returncode, r.stderr[-300:])
        for a, b in zip(exp, got):
            if a != b:
                print("  expected:", a, "\n  got:     ", b)
                break
        if len(exp) != len(got):
            print("  line count", len(exp), len(got))
    return ok


def main():
    if len(sys.argv) > 1 and not os.path.exists(exe):
        sys.exit("build first: tools\\rarc_test.cmd " + sys.argv[1])
    good = True
    for n in NAMES:
        good &= run_case(os.path.join(data_dir, n), n)
    # Yay0: re-encode the smallest archives, same checks through decodeSZP
    for n in NAMES[:3]:
        dec = yaz0_decode(open(os.path.join(data_dir, n), "rb").read())
        tmp = os.path.join(tempfile.gettempdir(), "sunshine_rarc_test", n.replace("/", "_") + ".szp")
        open(tmp, "wb").write(yay0_encode(dec))
        exp = ["decoded %d %08x" % (len(dec), zlib.crc32(dec))]
        listing, remount = rarc_listing(dec)
        exp += listing + [remount]
        r = subprocess.run([exe, tmp], capture_output=True, text=True)
        ok = r.stdout.splitlines() == exp and r.returncode == 0
        print("%-4s %s as Yay0 (%d bytes encoded)" % ("ok" if ok else "FAIL", n, os.path.getsize(tmp)))
        good &= ok
    sys.exit(0 if good else 1)


main()
