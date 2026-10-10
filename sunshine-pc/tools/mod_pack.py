"""Builds the data of the overlay-demo example mod from the user's own disc extract.

Writes <out>/files/data/message.szs: the stock archive with one station name replaced.
Replacement is same byte length (Shift-JIS), so every RARC offset stays valid.
The output derives from copyrighted game data: it is generated locally and git-ignored (*.szs).

Usage: python tools/mod_pack.py [--src <extracted/files>] [--out <mod dir>]
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import discfmt  # noqa: E402

REL = "data/message.szs"
# (old, new) station names, Shift-JIS, equal byte length. old must occur exactly once in the archive.
PATCHES = [("ドルピックタウン駅", "スーパーマリオ駅前")]
DEFAULT_OUT = os.path.normpath(os.path.join(HERE, "..", "mods-example", "overlay-demo"))


def yaz0_encode(data):
    """Yaz0 without back-references: every code bit is 1 (literal). Valid, larger than the input."""
    out = bytearray(b"Yaz0" + struct.pack(">I", len(data)) + bytes(8))
    for i in range(0, len(data), 8):
        chunk = data[i:i + 8]
        out.append((0xFF << (8 - len(chunk))) & 0xFF)
        out += chunk
    return bytes(out)


def build(src, out):
    blob = bytearray(discfmt.yaz0(open(os.path.join(src, REL), "rb").read()))
    for old, new in PATCHES:
        ob, nb = old.encode("shift_jis"), new.encode("shift_jis")
        assert len(ob) == len(nb), (old, new)
        hits = blob.count(ob)
        assert hits == 1, "%r found %d times" % (old, hits)
        i = blob.index(ob)
        blob[i:i + len(ob)] = nb
    enc = yaz0_encode(bytes(blob))
    assert discfmt.yaz0(enc) == bytes(blob), "Yaz0 round-trip failed"
    dst = os.path.join(out, "files", REL)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(enc)
    return dst, len(enc)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=discfmt.ROOT, help="extracted disc files dir (default: sms/orig/GMSJ01)")
    ap.add_argument("--out", default=DEFAULT_OUT, help="mod directory to write files/ into")
    args = ap.parse_args()
    dst, size = build(args.src, args.out)
    print("wrote %s (%d bytes)" % (dst, size))
