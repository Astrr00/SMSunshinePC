"""Offline format check for the J2D/JUT/GC2D readers (BTI, BFN, BLO, BMG, ResNTAB) against real disc bytes.

Usage: python tools/fmtcheck/fmtcheck_j2djut.py [--extract-dir DIR] [--no-build] [--quick]
  1. extracts every archive of the local disc files (tools/fmt_arc_extract.py) unless DIR already holds them,
  2. builds tools/fmtcheck/out/j2djut_check.exe (build.cmd: the real decomp readers, LLVM_BIN must point at clang-cl),
  3. for each format writes a manifest, runs the exe (host structs after the swap-on-load code ran) and parses the same
     bytes here with struct.unpack('>...') (the disc format, independent of the host byte order), and diffs the two.
Exit code 0 only if every format matches. --quick samples the large sets (J3D TEX1 headers, name tables).
"""
import glob
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
EXE = os.path.join(HERE, "out", "j2djut_check.exe")
sys.path.insert(0, os.path.join(HERE, ".."))
import fmt_arc_extract  # noqa: E402

FAILS = []


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def fnv(data, h=2166136261):
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def run(mode, lines, tmp):
    mf = os.path.join(tmp, mode + ".txt")
    with open(mf, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    r = subprocess.run([EXE, mode, mf], capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        print(f"[{mode}] harness exit {r.returncode}: {r.stderr[-500:]}")
        FAILS.append(mode)
    return r.stdout.splitlines()


def compare(name, got, want):
    bad = 0
    for i in range(max(len(got), len(want))):
        g = got[i] if i < len(got) else "<missing>"
        w = want[i] if i < len(want) else "<missing>"
        if g != w:
            bad += 1
            if bad <= 5:
                print(f"[{name}] line {i}:\n   host: {g}\n   disc: {w}")
    print(f"[{name}] {len(want)} lines, {bad} mismatches")
    if bad or not want:
        FAILS.append(name)


# ---- BTI -----------------------------------------------------------------------------------------------------------

TIMG = struct.Struct(">BBHHBBBBHIBBBBBBbbBBhI")


def timg_fields(b, o=0):
    (fmt, alpha, w, h, ws, wt, idx, cf, nc, po, mip, edge, bc, an, minf, magf, minl, maxl, mc, u19, bias,
     io) = TIMG.unpack_from(b, o)
    return dict(fmt=fmt, alpha=alpha, w=w, h=h, ws=ws, wt=wt, idx=idx, cf=cf, nc=nc, po=s32(po), mip=mip, edge=edge,
                bc=bc, an=an, min=minf, mag=magf, minlod=minl, maxlod=maxl, mc=mc, bias=bias, io=s32(io), u19=u19)


def timg_line(tag, t):
    return (f"{tag} fmt={t['fmt']} alpha={t['alpha']} w={t['w']} h={t['h']} ws={t['ws']} wt={t['wt']} idx={t['idx']} "
            f"cf={t['cf']} nc={t['nc']} po={t['po']} mip={t['mip']} edge={t['edge']} bc={t['bc']} an={t['an']} "
            f"min={t['min']} mag={t['mag']} minlod={t['minlod']} maxlod={t['maxlod']} mc={t['mc']} bias={t['bias']} "
            f"io={t['io']}")


def bti_expected(path):
    b = open(path, "rb").read()
    t = timg_fields(b)
    out = ["BTI " + os.path.basename(path), timg_line("  hdr", t)]
    out.append(f"  mark=de tex={t['fmt']},{t['alpha']},{t['w']},{t['h']},{t['ws']},{t['wt']},{t['min']},{t['mag']},"
               f"{(t['minlod'])} bias={t['bias']}")
    # JUTTexture keeps min/max LOD in u16 fields; storeTIMG(): mMinLOD = (s8)minLod, printed back through (s8)
    img = t["io"] if t["io"] != 0 else 0x20
    minlod = f32(((t["minlod"]) & 0xFFFF) / 8.0)
    maxlod = f32(((t["maxlod"]) & 0xFFFF) / 8.0)
    bias = f32(t["bias"] / 100.0)
    tlut_n = t["nc"]
    ci = tlut_n != 0 and t["idx"] != 0
    calls = 1 if (tlut_n == 0 or ci) else 0
    tl = (16 if tlut_n > 0x100 else 0) if ci else -1
    if calls:
        out.append(f"  gx calls=1 w={t['w']} h={t['h']} fmt={t['fmt']} ws={t['ws']} wt={t['wt']} "
                   f"mip={1 if t['mip'] else 0} tlut={tl} imgoff={img} minlod={minlod:g} maxlod={maxlod:g} bias={bias:g} "
                   f"bc={t['bc']} edge={t['edge']} an={t['an']}")
    else:
        out.append("  gx calls=0 w=0 h=0 fmt=0 ws=0 wt=0 mip=0 tlut=0 imgoff=0 minlod=0 maxlod=0 bias=0 bc=0 edge=0 an=0")
    if tlut_n:
        out.append(f"  tlut calls=1 fmt={t['cf']} n={tlut_n} off={t['po']}")
    return out


# ---- BFN -----------------------------------------------------------------------------------------------------------

def halftofull():
    src = open(os.path.join(ROOT, "sms", "src", "JSystem", "JUtility", "JUTResFont.cpp"), encoding="utf-8").read()
    body = src[src.index("halftofull[95]"):]
    body = body[body.index("{") + 1:body.index("};")]
    tab = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", body)]
    assert len(tab) == 95
    return tab


def bfn_expected(path, tab):
    b = open(path, "rb").read()
    magic, fsz, nb = struct.unpack(">8sII", b[:16])
    out = ["BFN " + os.path.basename(path), f"  magic={int.from_bytes(magic, 'big'):016x} filesize={fsz} blocks={nb}"]
    blocks = []
    off = 0x20
    for _ in range(nb):
        t, s = struct.unpack(">4sI", b[off:off + 8])
        blocks.append((t, s, off))
        off += s
    inf = [x for x in blocks if x[0] == b"INF1"][0]
    ftype, asc, desc, width, lead, defc = struct.unpack(">6H", b[inf[2] + 8:inf[2] + 0x14])
    wids = [x for x in blocks if x[0] == b"WID1"]
    maps = [x for x in blocks if x[0] == b"MAP1"]
    glys = [x for x in blocks if x[0] == b"GLY1"]
    maxcode = 0xFFFF
    for _, _, o in maps:
        st = struct.unpack(">H", b[o + 0xA:o + 0xC])[0]
        maxcode = min(maxcode, st)
    out.append(f"  INF1 type={ftype} ascent={asc} descent={desc} width={width} leading={lead} defcode={defc} max={maxcode}")
    out.append(f"  counts wid={len(wids)} gly={len(glys)} map={len(maps)}")
    W = []
    for _, s, o in wids:
        st, en = struct.unpack(">HH", b[o + 8:o + 12])
        cnt = en - st + 1
        W.append((st, en, o))
        out.append(f"  WID1 size={s} start={st} end={en} hash={fnv(b[o + 12:o + 12 + cnt * 2]):08x}")
    M = []
    for _, s, o in maps:
        meth, st, en, n = struct.unpack(">4H", b[o + 8:o + 16])
        words = (s - 0x10) // 2
        tabw = struct.unpack(">%dH" % words, b[o + 0x10:o + 0x10 + words * 2])
        M.append((meth, st, en, n, tabw))
        out.append(f"  MAP1 size={s} method={meth} start={st} end={en} n={n} hash={fnv(struct.pack('<%dH' % words, *tabw)):08x}")
    for _, s, o in glys:
        (st, en, cw, ch, ts, fmt, rows, cols, tw, th, pad) = struct.unpack(">HHHHIHHHHHH", b[o + 8:o + 0x20])
        out.append(f"  GLY1 size={s} start={st} end={en} cw={cw} ch={ch} tsize={ts} fmt={fmt} rows={rows} cols={cols} "
                   f"tw={tw} th={th} pad={pad} data={fnv(b[o + 0x20:o + s]):08x}")

    def font_code(c):
        ret = defc
        if ftype == 2 and maxcode >= 0x8000 and 0x20 <= c < 0x7F:
            c = tab[c - 32]
        for meth, st, en, n, tw in M:
            if st <= c <= en:
                if meth == 0:
                    ret = c - st
                elif meth == 2:
                    ret = tw[c - st]
                elif meth == 3:
                    lo, hi = 0, n - 1
                    while hi >= lo:
                        mid = (hi + lo) // 2
                        if c < tw[mid * 2]:
                            hi = mid - 1
                            continue
                        if c > tw[mid * 2]:
                            lo = mid + 1
                            continue
                        ret = tw[mid * 2 + 1]
                        break
                elif meth == 1:
                    hb = (c >> 8) & 0xFF
                    lb = (c & 0xFF) - 0x40
                    if lb >= 0x40:
                        lb -= 1
                    ret = lb + (hb - 0x88) * 0xBC + 0x2BE
                break
        return ret

    for c in [0x20, 0x31, 0x41, 0x7A, 0x8140, 0x8250, 0x82A0, 0x8340, 0x889F, 0x97CA, 0x9FDC, 0xDEAD]:
        fc = font_code(c)
        w0, w1 = 0, width
        for st, en, o in W:
            if st <= fc <= en:
                w0, w1 = b[o + 12 + (fc - st) * 2], b[o + 12 + (fc - st) * 2 + 1]
                break
        out.append(f"  code {c:04x} -> font={fc} w={w0},{w1}")
    return out


# ---- BMG -----------------------------------------------------------------------------------------------------------

def bmg_expected(path):
    b = open(path, "rb").read()
    units, nblk = struct.unpack(">II", b[8:16])
    off = 0x20
    ents, dat, fid, esz = [], None, 0, 0
    for _ in range(nblk):
        t, s = struct.unpack(">4sI", b[off:off + 8])
        if t == b"INF1":
            cnt, esz, fid, _p = struct.unpack(">4H", b[off + 8:off + 16])
            for i in range(cnt):
                o = off + 0x10 + i * esz
                ents.append(b[o:o + 12] if esz == 12 else b[o:o + esz])
        elif t == b"DAT1":
            dat = off + 8
        off += s

    def text_hash(o):
        end = b.find(b"\0", dat + o)
        return fnv(b[dat + o:end if end >= 0 else len(b)])

    if esz != 12:  # only SMSGetMessageData reads these (stage/scenario names, save messages, system texts)
        out = ["BMG " + os.path.basename(path), f"  count={len(ents)} esz={esz}"]
        for i, e in enumerate(ents):
            o = struct.unpack(">I", e[:4])[0]
            out.append(f"  [{i}] util={(text_hash(o) if o else 0):08x}")
        return out
    out = ["BMG " + os.path.basename(path), f"  count={len(ents)} fileId={fid} ctorCount={len(ents)}"]
    for i, e in enumerate(ents):
        o, a, bb, t0, t1, t2, t3 = struct.unpack(">IhhBBBB", e)
        h = text_hash(o)
        out.append(f"  [{i}] off={o} a={a} b={bb} tail={t0:02x}{t1:02x}{t2:02x}{t3:02x} text={h:08x} "
                   f"util={(h if o else 0):08x}")
    out.append("  oob=1")
    out.append("  reload=1")
    return out


# ---- name tables ---------------------------------------------------------------------------------------------------

NT_SPEC = {b"JNT1": [0x14], b"MAT3": [0x14], b"TEX1": [0x10], b"TTK1": [0x1C, 0x44], b"TRK1": [0x30, 0x34],
           b"TPT1": [0x1C], b"PAK1": [0x20]}


def j3d_blocks(b):
    if len(b) < 0x20 or b[:3] != b"J3D":
        return
    n = struct.unpack(">I", b[12:16])[0]
    off = 0x20
    for _ in range(n):
        if off + 8 > len(b):
            return
        t, s = struct.unpack(">4sI", b[off:off + 8])
        yield t, s, off
        off += s


def ntab_expected(path, off):
    b = open(path, "rb").read()
    n = struct.unpack(">H", b[off:off + 2])[0]
    hk = hn = 2166136261
    for i in range(n):
        key, o = struct.unpack(">HH", b[off + 4 + i * 4:off + 8 + i * 4])
        hk = fnv(struct.pack("<H", key), hk)
        hk = fnv(struct.pack("<H", o), hk)
        end = b.index(b"\0", off + o)
        hn = fnv(b[off + o:end + 1], hn)
    return f"NTAB {os.path.basename(path)} {off} n={n} keys={hk:08x} names={hn:08x} bad=0"


# ---- BLO -----------------------------------------------------------------------------------------------------------

class Stream:
    def __init__(self, b):
        self.b, self.p = b, 0

    def _u(self, fmt, n):
        v = struct.unpack_from(fmt, self.b, self.p)[0]
        self.p += n
        return v

    def u8(self): return self._u(">B", 1)
    def u16(self): return self._u(">H", 2)
    def s16(self): return self._u(">h", 2)
    def u32(self): return self._u(">I", 4)

    def skip(self, n):
        self.p = max(0, min(len(self.b), self.p + n))

    def seek(self, n):
        self.p = max(0, min(len(self.b), n))

    def align(self, a):
        self.seek((self.p + a - 1) // a * a)

    def peek32(self):
        return struct.unpack_from(">I", self.b, self.p)[0]


class Archive:
    def __init__(self, d):
        self.files = {}
        for dp, _, fn in os.walk(d):
            for f in fn:
                p = os.path.join(dp, f)
                self.files[norm(os.path.relpath(p, d))] = p
        self.keys = sorted(self.files)

    def find(self, name):
        n = norm(name)
        if n in self.files:
            return open(self.files[n], "rb").read()
        for k in self.keys:  # bare file name inside the archive ("title_logo.bti")
            if k.endswith("/" + n):
                return open(self.files[k], "rb").read()
        for k in self.keys:  # absolute game path ("/common/2d/x.bmg")
            if n.endswith(k):
                return open(self.files[k], "rb").read()
        return None


def norm(s):
    return s.replace("\\", "/").lower().lstrip("/")


def res_ref(s, arc):
    typ = s.u8()
    ln = s.u8()
    name = s.b[s.p:s.p + ln]
    s.skip(ln)
    if typ in (2, 3, 4):
        return arc.find(name.decode("latin-1"))
    return None


def tr(a, b):  # C int division by 2 / the base position math is on non-negative values
    return int(a / b)


def blo_expected(path, archive_dir):
    arc = Archive(archive_dir)
    b = open(path, "rb").read()
    s = Stream(b)
    is_ex = struct.unpack(">4s", b[:4])[0] == b"SCRN"
    assert is_ex
    s.skip(4)
    assert s.u32() == 0x626C6F31
    s.skip(0x18)
    assert s.u32() == 0x494E4631
    s.skip(4)
    w = s.s16()
    h = s.s16()
    color = s.u32()
    out = ["BLO " + os.path.basename(path), f"S color={color:08x} bounds=0,0,{w},{h}"]

    def timg_short(data):
        t = timg_fields(data)
        return t

    def base(kind_tag):
        kind = s.u32()
        s.skip(4)
        fields = s.u8()
        vis = s.u8()
        s.skip(2)
        user = s.u32()
        x1 = s.s16()
        y1 = s.s16()
        x2 = x1 + s.s16()
        y2 = y1 + s.s16()
        fields = (fields - 6) & 0xFF
        rot = 0
        if fields:
            rot = s.u16()
            fields -= 1
        basepos = 0
        if fields:
            basepos = s.u8()
            fields -= 1
        alpha = 255
        if fields:
            alpha = s.u8()
            fields -= 1
        infl = 1
        if fields:
            infl = 1 if s.u8() else 0
            fields -= 1
        s.align(4)
        wd, ht = x2 - x1, y2 - y1
        rox = 0 if basepos % 3 == 0 else (tr(wd, 2) if basepos % 3 == 1 else wd)
        roy = 0 if basepos // 3 == 0 else (tr(ht, 2) if basepos // 3 == 1 else ht)
        return kind, vis, user, (x1, y1, x2, y2), rot, basepos, rox, roy, alpha, infl

    def head(depth, p):
        kind, vis, user, bnd, rot, bp, rox, roy, alpha, infl = p
        return (f"P d={depth} k={kind:08x} vis={vis} user={user:08x} b={bnd[0]},{bnd[1]},{bnd[2]},{bnd[3]} rot={rot} "
                f"base={bp} rox={rox} roy={roy} alpha={alpha} infl={infl}")

    def pic(depth):
        p = base("PIC1")
        fields = s.u8()
        timg = res_ref(s, arc)
        tlut = res_ref(s, arc)
        binding = s.u8()
        fields = (fields - 3) & 0xFF
        mirror, flip, wh, wv = 0, 0, 0, 0
        if fields:
            m = s.u8()
            mirror, flip = m & 3, 1 if m & 4 else 0
            fields -= 1
        if fields:
            wr = s.u8()
            wh, wv = (wr >> 2) & 3, wr & 3
            fields -= 1
        black, white = 0, 0xFFFFFFFF
        if fields:
            black = s.u32()
            fields -= 1
        if fields:
            white = s.u32()
            fields -= 1
        corners = [0xFFFFFFFF] * 4
        i = 0
        while fields and i < 4:
            corners[i] = s.u32()
            fields -= 1
            i += 1
        s.align(4)
        line = head(depth, p) + " | pic nt=%d" % (1 if timg else 0)
        if timg:
            t = timg_fields(timg)
            line += f" tex={t['fmt']},{t['w']},{t['h']},{t['alpha']}"
        line += (f" pal={1 if tlut else 0} bind={binding} mirror={mirror} flip={flip} wh={wh} wv={wv} black={black:08x} "
                 f"white={white:08x} c={corners[0]:08x},{corners[1]:08x},{corners[2]:08x},{corners[3]:08x}")
        return line

    def win(depth):
        p = base("WIN1")
        fields = s.u8()
        x1 = s.u16()
        y1 = s.u16()
        x2 = x1 + s.u16()
        y2 = y1 + s.u16()
        tex = [timg_fields(d) if (d := res_ref(s, arc)) else None for _ in range(4)]
        tlut = res_ref(s, arc)
        flags = s.u8()
        c118 = s.u32()
        c120 = s.u32()
        c11c = s.u32()
        c124 = s.u32()
        fields = (fields - 14) & 0xFF
        t110 = None
        if fields:
            d = res_ref(s, arc)
            t110 = timg_fields(d) if d else None
            fields -= 1
        c12c, c128 = 0, 0xFFFFFFFF
        if fields:
            c12c = s.u32()
            fields -= 1
        if fields:
            c128 = s.u32()
        s.align(4)
        if all(tex):
            size = (tex[0]["w"] + tex[1]["w"], tex[0]["h"] + tex[2]["h"])
        else:
            size = (1, 1)
        line = head(depth, p) + f" | win cb={x1},{y1},{x2},{y2}"
        for i, t in enumerate(tex + [t110]):
            line += f" t{i}=" + (f"{t['fmt']},{t['w']},{t['h']}" if t else "-")
        line += (f" pal={1 if tlut else 0} flags={flags} c118={c118:08x} c120={c120:08x} c11c={c11c:08x} c124={c124:08x} "
                 f"c12c={c12c:08x} c128={c128:08x} size={size[0]},{size[1]}")
        return line

    def tbx(depth):
        p = base("TBX1")
        fields = s.u8()
        fnt = res_ref(s, arc)
        cc = s.u32()
        gc = s.u32()
        bi = s.u8()
        hb, vb = (bi >> 2) & 3, bi & 3
        cs = s.s16()
        ls = s.s16()
        fsx = s.u16()
        fsy = s.u16()
        ln = s.u16()
        ln = ln - 0x10000 if ln & 0x8000 else ln
        text = s.b[s.p:s.p + ln]
        s.skip(ln)
        fields = (fields - 10) & 0xFF
        if fields:
            s.u8()
            fields -= 1
        black, white = 0, 0xFFFFFFFF
        if fields:
            black = s.u32()
            fields -= 1
        if fields:
            white = s.u32()
        s.align(4)
        line = head(depth, p) + " | tbx"
        if fnt:
            ftype, asc, desc, wd, lead, _ = struct.unpack(">6H", fnt[0x20 + 8:0x20 + 0x14])
            line += f" font={asc},{desc},{wd},{lead},{ftype}"
        else:
            line += " font=-"
        line += (f" cc={cc:08x} gc={gc:08x} hb={hb} vb={vb} cs={cs} ls={ls} fsx={fsx} fsy={fsy} black={black:08x} "
                 f"white={white:08x} text={text.split(b'\0')[0].hex()}")
        return line

    def pan(depth):
        return head(depth, base("PAN1"))

    lines = out
    # makeHiearachyPanes: panes created inside a BGN1..END1 pair are children of the pane created last before the BGN1
    # (depth + 1). The "next pane offset" is ONE variable shared by all recursion levels (out_next_pane_offset).
    shared = [0]

    def make(depth):
        while True:
            magic = s.u32()
            size = s.peek32()
            s.skip(-4)
            shared[0] = size + s.p
            tag = struct.pack(">I", magic)
            if tag == b"EXT1":
                return
            if tag == b"BGN1":
                s.seek(shared[0])
                make(depth + 1)
            elif tag == b"END1":
                s.seek(shared[0])
                return
            elif tag == b"PAN1":
                lines.append(pan(depth))
            elif tag == b"WIN1":
                lines.append(win(depth))
            elif tag == b"PIC1":
                lines.append(pic(depth))
            elif tag == b"TBX1":
                lines.append(tbx(depth))
            s.seek(shared[0])

    make(0)
    return out


# ---- driver --------------------------------------------------------------------------------------------------------

def main():
    args = sys.argv[1:]
    quick = "--quick" in args
    ed = os.path.join(tempfile.gettempdir(), "fmtj2d_all")
    if "--extract-dir" in args:
        ed = args[args.index("--extract-dir") + 1]
    if not os.path.isdir(ed):
        data = os.path.join(fmt_arc_extract.ROOT, "data")
        for a in sorted(glob.glob(data + "/*.szs")) + sorted(glob.glob(data + "/scene/*.szs")):
            fmt_arc_extract.extract(a, ed)
    if "--no-build" not in args:
        r = subprocess.run(["cmd", "/c", os.path.join(HERE, "build.cmd")], capture_output=True, text=True, errors="replace")
        if not os.path.exists(EXE):
            print(r.stdout[-3000:], r.stderr[-1000:])
            print("build failed (set LLVM_BIN to the folder with clang-cl.exe)")
            return 2
    tmp = tempfile.mkdtemp(prefix="j2djut_")

    def files(ext):
        return sorted(glob.glob(os.path.join(ed, "**", "*." + ext), recursive=True))

    # BTI: every .bti of the disc
    btis = files("bti")
    if quick:
        btis = btis[::7]
    got = run("bti", btis, tmp)
    want = [l for p in btis for l in bti_expected(p)]
    compare(f"bti ({len(btis)} files)", got, want)

    # TEX1 headers embedded in J3D files (stray bytes at 0x19): swap + idempotency
    tex1_lines, tex1_want = [], []
    j3d = files("bmd") + files("bdl") + files("bmt")
    if quick:
        j3d = j3d[::25]
    for p in j3d:
        b = open(p, "rb").read()
        for t, s_, o in j3d_blocks(b):
            if t == b"TEX1":
                cnt, _pad, hoff = struct.unpack(">HHI", b[o + 8:o + 16])
                tex1_lines.append(f"{p}\t{o + hoff}\t{cnt}")
                tex1_want.append(f"TEX1 {os.path.basename(p)} {o + hoff}")
                tex1_want += [timg_line(f"  [{i}]", timg_fields(b, o + hoff + i * 0x20)) for i in range(cnt)]
    compare(f"tex1 ({len(tex1_lines)} blocks)", run("tex1", tex1_lines, tmp), tex1_want)

    # name tables of every J3D flavor
    nt_lines, nt_want = [], []
    anim = files("btk") + files("brk") + files("btp") + files("bpk")
    nts = j3d + (anim[::25] if quick else anim)
    for p in nts:
        b = open(p, "rb").read()
        for t, s_, o in j3d_blocks(b):
            for fo in NT_SPEC.get(t, []):
                if t == b"TTK1" and fo == 0x44 and s_ < 0x60:
                    continue
                no = struct.unpack(">I", b[o + fo:o + fo + 4])[0]
                if no:
                    nt_lines.append(f"{p}\t{o + no}")
                    nt_want.append(ntab_expected(p, o + no))
    compare(f"ntab ({len(nt_lines)} tables)", run("ntab", nt_lines, tmp), nt_want)

    # fonts
    tab = halftofull()
    bfns = files("bfn")
    compare(f"bfn ({len(bfns)} fonts)", run("bfn", bfns, tmp), [l for p in bfns for l in bfn_expected(p, tab)])

    # messages
    bmgs = files("bmg")
    if quick:
        bmgs = bmgs[::9]
    # TMessageLoader::unk8 holds 255 entries (the game only loads balloon/infomess/subtitle files with it)
    small = [p for p in bmgs if struct.unpack(">H", open(p, "rb").read()[0x28:0x2A])[0] <= 255]
    print(f"[bmg] {len(bmgs) - len(small)} of {len(bmgs)} files have more than 255 entries and are skipped")
    bmgs = small
    compare(f"bmg ({len(bmgs)} files)", run("bmg", bmgs, tmp), [l for p in bmgs for l in bmg_expected(p)])

    # screens: every BLO with the content folder of its archive as resource root
    blos = files("blo")
    blo_lines, blo_want = [], []
    for p in blos:
        rel = os.path.relpath(p, ed).split(os.sep)
        adir = os.path.join(ed, rel[0], rel[1])  # <arc>.szs/<root dir of the archive>
        blo_lines.append(f"{p}\t{adir}")
        blo_want += blo_expected(p, adir)
    compare(f"blo ({len(blos)} screens)", run("blo", blo_lines, tmp), blo_want)

    print("FAIL: " + ", ".join(FAILS) if FAILS else "ALL FORMATS MATCH")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
