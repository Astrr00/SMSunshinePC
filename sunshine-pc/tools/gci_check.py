#!/usr/bin/env python3
"""Independent Dolphin-style .gci checker (stdlib only).

Usage: gci_check.py <file.gci> [--dump N]

DEntry (0x40 bytes, big-endian multi-byte fields, layout per Aurora
card/File.hpp): game 0x00[4], maker 0x04[2], filename 0x06[32], modifiedTime
0x26, iconAddr 0x2A, iconFmt 0x2E, animSpeed 0x30, permissions 0x32,
copyCounter 0x33, firstBlock 0x34, blockCount 0x36, reserved 0x38,
commentAddr 0x3C.

Data offsets (commentAddr, iconAddr) are relative to the data start at file
0x40. Each 0x2000 block follows the decomp TCardSector layout: write count at
0x0, title 0x4, comment 0x24, banner 0x44, icons 0xE44, option block 0x1844,
u32 checksum at 0x1FFC (top/bottom u16 sums over the preceding 0x1FFC bytes,
big-endian). Exit code 0 when every check passes.
"""
import struct
import sys

HEADER = 0x40
BLOCK = 0x2000
CHECKSUM_OFF = 0x1FFC


def be16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def sector_checksum(sec):
    top = 0
    bottom = 0
    for i in range(0, CHECKSUM_OFF, 2):
        w = be16(sec, i)
        top = (top + w) & 0xFFFF
        bottom = (bottom + (~w & 0xFFFF)) & 0xFFFF
    return (top << 16) | bottom


def cstr(b):
    return b.split(b"\0", 1)[0].decode("latin-1")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    dump = 0
    if "--dump" in argv:
        dump = int(argv[argv.index("--dump") + 1], 0)
    data = open(path, "rb").read()
    fails = []

    def check(cond, msg):
        print(("  ok   " if cond else "  FAIL ") + msg)
        if not cond:
            fails.append(msg)

    print(f"file: {path}  size=0x{len(data):X}")
    if len(data) < HEADER + BLOCK:
        print("  FAIL file smaller than DEntry + one block")
        return 1

    game = data[0x00:0x04]
    maker = data[0x04:0x06]
    fname = cstr(data[0x06:0x26])
    mtime = be32(data, 0x26)
    icon_addr = be32(data, 0x2A)
    icon_fmt = be16(data, 0x2E)
    anim = be16(data, 0x30)
    perm = data[0x32]
    copy_ctr = data[0x33]
    first_block = be16(data, 0x34)
    block_count = be16(data, 0x36)
    comment_addr = be32(data, 0x3C)
    print(f"DEntry: game={game!r} maker={maker!r} file={fname!r}")
    print(f"        mtime=0x{mtime:08X} iconAddr=0x{icon_addr:X} iconFmt={icon_fmt} "
          f"anim={anim} perm=0x{perm:02X} copy={copy_ctr}")
    print(f"        firstBlock={first_block} blockCount={block_count} commentAddr=0x{comment_addr:X}")

    body = len(data) - HEADER
    check(game == b"GMSJ", "game code is GMSJ")
    check(maker == b"01", "maker code is 01")
    check(body % BLOCK == 0, f"body 0x{body:X} is a multiple of 0x2000")
    check(block_count == body // BLOCK, f"blockCount {block_count} matches body size")
    check(comment_addr == 0x4, "commentAddr is 0x4 (title at sector 0x4)")
    check(icon_addr == 0x44, "iconAddr is 0x44 (banner at sector 0x44)")

    nblocks = body // BLOCK
    for b in range(nblocks):
        base = HEADER + b * BLOCK
        sec = data[base:base + BLOCK]
        wc = be32(sec, 0x0)
        stored = be32(sec, CHECKSUM_OFF)
        calc = sector_checksum(sec)
        print(f"block {b}: writeCount={wc} checksum stored=0x{stored:08X} calc=0x{calc:08X}")
        check(stored == calc, f"block {b} checksum (BE) matches recompute")

    title = cstr(data[HEADER + 0x4:HEADER + 0x24])
    comment = cstr(data[HEADER + 0x24:HEADER + 0x44])
    print(f"title={title!r} comment={comment!r}")

    if dump:
        print(f"first 0x{dump:X} bytes of file:")
        for i in range(0, dump, 16):
            row = data[i:i + 16]
            hexs = " ".join(f"{x:02X}" for x in row)
            asc = "".join(chr(x) if 32 <= x < 127 else "." for x in row)
            print(f"  {i:06X}  {hexs:<47}  {asc}")

    print("RESULT: " + ("PASS" if not fails else f"FAIL ({len(fails)})"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
