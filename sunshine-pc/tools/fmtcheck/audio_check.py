"""Offline format check of the JAudio readers (AAF, sound tables, BNK, WSYS, BARC, .bas, oscillator tables, sequence
operands) against real disc bytes.

Usage: python tools/fmtcheck/audio_check.py <private build name> [--no-build] [--bas-limit N]
  1. extracts mSound.aaf from data/nintendo.szs and every .bas of the disc archives (tools/fmt_arc_extract.py),
  2. builds tools/fmtcheck/out/audio_check.exe (build_audio.cmd: the real decomp readers from sms_game.lib of the
     private build tree <name>, made by tools\\build-alt.cmd <name>; LLVM_BIN must point at clang-cl),
  3. for every format runs the exe (host structures after the swap/parse code ran) and parses the same bytes here with
     struct.unpack('>...'), the disc format independent of the host byte order, then diffs the two outputs line by line.
Also checks the layout derivation itself: counts and offsets of every block are consistent (blocks inside the file,
no overlap, tables terminated, indices inside their tables). Exit code 0 only if everything matches.
"""
import glob
import os
import random
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(HERE, "out")
EXE = os.path.join(OUT, "audio_check.exe")
sys.path.insert(0, os.path.join(HERE, ".."))
import fmt_arc_extract  # noqa: E402

FAILS = []
STATS = {}


def u8(b, o): return b[o]
def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def s16(b, o): return struct.unpack_from(">h", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]
def s32(b, o): return struct.unpack_from(">i", b, o)[0]
def fbits(b, o): return "%08x" % u32(b, o)


def fail(msg):
    FAILS.append(msg)
    if len(FAILS) <= 40:
        print("FAIL", msg)


def check(cond, msg):
    if not cond:
        fail(msg)
    return cond


def stat(k, n=1):
    STATS[k] = STATS.get(k, 0) + n


def run(args, timeout=1200):
    r = subprocess.run([EXE] + args, capture_output=True, text=True, errors="replace", timeout=timeout)
    if r.returncode != 0:
        fail("harness %s exit %#x: %s" % (args[0], r.returncode & 0xFFFFFFFF, r.stderr[-400:]))
    return r.stdout.splitlines()


def diff(mode, got, want):
    n = 0
    for i in range(max(len(got), len(want))):
        g = got[i] if i < len(got) else "<missing>"
        w = want[i] if i < len(want) else "<missing>"
        if g != w:
            n += 1
            if n <= 6:
                fail("%s line %d:\n   host : %s\n   disc : %s" % (mode, i, g, w))
    if n > 6:
        fail("%s: %d more differing lines" % (mode, n - 6))
    print("[%s] %d lines compared, %d differ" % (mode, len(want), n))


# --------------------------------------------------------------------------------------------------- AAF layout
def parse_aaf(d):
    """Returns the list of (cmd, ofs, size, arg) records; checks every blob lies inside the file."""
    recs = []
    i = 0
    while True:
        c = u32(d, i * 4)
        i += 1
        if c == 0:
            break
        if c in (2, 3):
            while u32(d, i * 4) != 0:
                recs.append((c, u32(d, i * 4), u32(d, i * 4 + 4), u32(d, i * 4 + 8)))
                i += 3
            i += 1
        elif c in (1, 4, 5, 6, 7, 8):
            recs.append((c, u32(d, i * 4), u32(d, i * 4 + 4), u32(d, i * 4 + 8)))
            i += 3
        else:
            fail("AAF: unknown command %d at word %d" % (c, i - 1))
            break
    spans = []
    for c, o, s, a in recs:
        check(o + s <= len(d), "AAF cmd %d blob %#x+%#x outside the %#x byte file" % (c, o, s, len(d)))
        spans.append((o, o + s, c))
    spans.sort()
    for (a0, a1, ca), (b0, b1, cb) in zip(spans, spans[1:]):
        check(a1 <= b0, "AAF blobs of cmd %d and %d overlap" % (ca, cb))
    check(i * 4 <= min(s[0] for s in spans), "AAF command list overlaps its first blob")
    return recs


def expect_aaf(d, recs):
    L = []
    se = [r for r in recs if r[0] == 1][0]
    t = d[se[1]:se[1] + se[2]]
    check(se[3] == 0, "AAF: expected a single sound table (third word 0)")
    n = u16(t, 4)
    cats = [(u16(t, 6 + 4 * c), u16(t, 8 + 4 * c)) for c in range(18)]
    catMax = 0
    for c in range(16):
        if cats[c][0]:
            catMax = c + 1
    check(0x50 + 0x10 * (n + 1) == len(t), "sound table: size %#x != header 0x50 + 0x10 * (count %d + 1)" % (len(t), n))
    ends = [first + cnt for cnt, first in cats]
    check(max(ends) == n, "sound table: the categories end at %d, the table says %d" % (max(ends), n))
    spans = sorted((first, first + cnt) for cnt, first in cats if cnt)
    for (a0, a1), (b0, b1) in zip(spans, spans[1:]):
        check(a1 <= b0, "sound table: category ranges overlap")
    L.append("SE size=%d separate=0 fmt=%d,%d,%d,%d unk0=%d catMax=%d" % (len(t), t[0], t[1], t[2], t[3], t[3], catMax))
    L.append("SE count=%d" % n)
    for c, (cnt, first) in enumerate(cats):
        L.append("CAT %d max=%d first=%d" % (c, cnt, first))
    for i in range((len(t) - 0x50) // 0x10):
        o = 0x50 + i * 0x10
        L.append("INFO %d sw=%08x pri=%d trk=%d off=%d pitch=%s vol=%d fx=%d" %
                 (i, u32(t, o), t[o + 4], t[o + 5], u16(t, o + 6), fbits(t, o + 8), t[o + 0xC], t[o + 0xD]))
        stat("sound infos")
    for c, (cnt, first) in enumerate(cats):
        if cnt:
            L.append("LOOK cat=%d id=%08x idx=%d" % (c, (c << 12) if c < 16 else (0x80000000 if c == 16 else 0xC0000000), first))
    for i, r in enumerate([r for r in recs if r[0] == 2]):
        L.append("BANKREC %d ofs=%d size=%d wave=%d" % (i, r[1], r[2], r[3]))
    for i, r in enumerate([r for r in recs if r[0] == 3]):
        L.append("WAVEREC %d ofs=%d size=%d timing=%d" % (i, r[1], r[2], r[3]))
    sa = [r for r in recs if r[0] == 4][0]
    L.append("SEQARC size=%d" % sa[2])
    st = [r for r in recs if r[0] == 5][0]
    L.append("STREAM size=%d" % st[2])
    check(st[2] % 0x40 == 0, "stream list size %#x is not a multiple of the 0x40-byte entry" % st[2])
    for off in range(0, st[2], 4):
        L.append("S%02x=%s" % (off, d[st[1] + off:st[1] + off + 4].hex()))
    nm = d[st[1] + 0x10:st[1] + 0x20]
    L.append("STREAMNAME %s" % nm.split(b"\0")[0].decode("ascii"))
    sc = [r for r in recs if r[0] == 6][0]
    s = d[sc[1]:sc[1] + sc[2]]
    smax = u32(s, 0)
    L.append("SCENES max=%d" % smax)
    offs = [u32(s, 4 + 4 * k) for k in range(smax)]
    for k, o in enumerate(offs):
        check(4 + 4 * smax <= o and o + 18 <= len(s), "sound scene %d at %#x outside its table" % (k, o))
        L.append("SCENE %d %s" % (k, " ".join("%d,%d" % (s[o + 2 * c], s[o + 2 * c + 1]) for c in range(9))))
    fx = [r for r in recs if r[0] == 7][0]
    f = d[fx[1]:fx[1] + fx[2]]
    fmax = u32(f, 0)
    buf = [u32(f, 4 + 4 * k) for k in range(4)]
    L.append("FX max=%d buf=%d,%d,%d,%d" % ((fmax,) + tuple(buf)))
    foffs = [u32(f, 0x14 + 4 * k) for k in range(fmax)]
    for k, o in enumerate(foffs):
        check(0x14 + 4 * fmax <= o and o + 0x80 <= len(f), "fx scene %d at %#x outside its table" % (k, o))
        for l in range(4):
            c = o + 0x20 * l
            L.append("FXL %d %d u0=%d u2=%d u4=%d u6=%d u8=%d uC=%d f=%s" %
                     (k, l, f[c], u16(f, c + 2), s16(f, c + 4), u16(f, c + 6), s16(f, c + 8), s32(f, c + 0xC),
                      ",".join(str(s16(f, c + 0x10 + 2 * j)) for j in range(8))))
            stat("fx line configs")
    check(all(buf[i] <= 0x10000 for i in range(4)), "fx buffer sizes look wrong: %s" % buf)
    L.append("WAVEGROUPS 1")
    return L


# --------------------------------------------------------------------------------------------------------- BNK
def f32mul_div(a_bits_float, b):
    return a_bits_float / b


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def f32bits(x):
    return "%08x" % struct.unpack(">I", struct.pack(">f", x))[0]


def osc_table(b, o):
    """s16 triplets (mode, time, value) up to and including the first triplet with mode > 0xA."""
    out = []
    while True:
        m = s16(b, o)
        out += [m, s16(b, o + 2), s16(b, o + 4)]
        o += 6
        if m > 0xA:
            return out
        check(o < len(b), "oscillator table runs out of the blob")


def effect_str(b, kind, off):
    if not off:
        return " E:-"
    if kind == "rand":
        return " E:rand,%d,%s,%s" % (b[off], fbits(b, off + 4), fbits(b, off + 8))
    return " E:sense,%d,%d,%d,%s,%s" % (b[off], b[off + 1], b[off + 2], fbits(b, off + 4), fbits(b, off + 8))


def velo_lines(b, base, count, indent="   "):
    L = []
    for v in range(count):
        vm = u32(b, base + 4 * v)
        check(vm and vm + 0x10 <= len(b), "velocity map offset %#x outside the bank" % vm)
        # unk4 is a u32 of which the parser keeps JSULoHalf = the low 16 bits = the u16 at +6 (wave id)
        L.append("%sVEL %d %d %d %s %s" % (indent, v, b[vm], u16(b, vm + 6), fbits(b, vm + 8), fbits(b, vm + 0xC)))
        stat("velocity regions")
    return L


def expect_bnk(d, recs):
    L = []
    osc_refs = []  # (aaf offset of a table)
    for bankNo, r in enumerate([r for r in recs if r[0] == 2]):
        b = d[r[1]:r[1] + r[2]]
        check(b[:4] == b"IBNK" and u32(b, 4) == r[2] and b[0x20:0x24] == b"BANK", "bank %d: bad IBNK header" % bankNo)
        insts = [u32(b, 0x24 + 4 * i) for i in range(128)]
        percs = [u32(b, 0x3B4 + 4 * i) for i in range(12)]
        L.append("BANK %d insts=%d" % (bankNo, sum(1 for o in insts if o) + sum(1 for o in percs if o)))
        for i, o in enumerate(insts):
            if not o:
                continue
            check(b[o:o + 4] == b"INST" and o + 0x2C <= len(b), "bank %d inst %d: bad INST block" % (bankNo, i))
            nkey = u32(b, o + 0x28)
            L.append("INST %d.%d vol=%s pitch=%s nosc=2 neff=4 nkey=%d" % (bankNo, i, fbits(b, o + 8), fbits(b, o + 0xC), nkey))
            present = [u32(b, o + 0x10 + 4 * j) for j in range(2) if u32(b, o + 0x10 + 4 * j)]
            for k in range(2):
                if k >= len(present):
                    L.append("  OSC %d -" % k)
                    continue
                os_ = present[k]
                ads, rel = u32(b, os_ + 8), u32(b, os_ + 0xC)
                line = "  OSC %d t=%d rate=%s width=%s vertex=%s" % (k, b[os_], fbits(b, os_ + 4), fbits(b, os_ + 0x10), fbits(b, os_ + 0x14))
                for tag, to in (("ads", ads), ("rel", rel)):
                    if to:
                        line += " %s=%s" % (tag, ",".join(str(x) for x in osc_table(b, to)))
                        osc_refs.append(r[1] + to)
                        stat("oscillator tables")
                    else:
                        line += " %s=-" % tag
                L.append(line)
                stat("oscillators")
            eff = "  EFF"
            for j in range(2):
                eff += effect_str(b, "rand", u32(b, o + 0x18 + 4 * j))
            for j in range(2):
                eff += effect_str(b, "sense", u32(b, o + 0x20 + 4 * j))
            L.append(eff)
            for k in range(nkey):
                km = u32(b, o + 0x2C + 4 * k)
                check(km and km + 8 <= len(b), "bank %d inst %d keymap offset %#x outside the bank" % (bankNo, i, km))
                nvelo = u32(b, km + 4)
                L.append("  KEY %d high=%d nvelo=%d" % (k, b[km], nvelo))
                L += velo_lines(b, km + 8, nvelo)
            stat("instruments")
        for p, po in enumerate(percs):
            if not po:
                continue
            magic = u32(b, po)
            L.append("PERC %d.%d" % (bankNo, 0xE4 + p))
            for j in range(128):
                pm = u32(b, po + 0x88 + 4 * j)
                vol, pitch, pan, rel, effs, nvelo = "3f800000", "3f800000", "3f000000", 1000, [], 0
                if pm:
                    vol, pitch = fbits(b, pm), fbits(b, pm + 4)
                    if magic == 0x50455232:  # 'PER2'
                        pan = f32bits(f32(struct.unpack_from(">b", b, po + 0x288 + j)[0] / 127.0))
                        rel = u16(b, po + 0x308 + 2 * j)
                    effs = [u32(b, pm + 8 + 4 * k) for k in range(2) if u32(b, pm + 8 + 4 * k)]
                    nvelo = u32(b, pm + 0x10)
                neff = 2 if pm else 0
                line = "  P %d vol=%s pitch=%s pan=%s rel=%d neff=%d nvelo=%d" % (j, vol, pitch, pan, rel, neff, nvelo)
                for k in range(neff):
                    line += effect_str(b, "rand", effs[k] if k < len(effs) else 0)
                L.append(line)
                if pm:
                    L += velo_lines(b, pm + 0x14, nvelo)
                    stat("drum pmaps")
            stat("drum sets")
        stat("banks")
    return L, osc_refs


# -------------------------------------------------------------------------------------------------------- WSYS
def expect_wsys(d, recs):
    L = []
    for no, r in enumerate([r for r in recs if r[0] == 3]):
        w = d[r[1]:r[1] + r[2]]
        check(w[:4] == b"WSYS" and u32(w, 4) <= r[2] and r[2] - u32(w, 4) < 0x20, "wave bank %d: bad WSYS header (size %#x, blob %#x)" % (no, u32(w, 4), r[2]))
        abank, cgroup = u32(w, 0x10), u32(w, 0x14)
        check(w[abank:abank + 4] == b"WINF" and w[cgroup:cgroup + 4] == b"WBCT", "wave bank %d: bad WINF/WBCT" % no)
        ngroups = u32(w, cgroup + 8)
        groups = []
        maxid = 0
        for g in range(ngroups):
            cs = u32(w, cgroup + 0xC + 4 * g)
            check(w[cs:cs + 4] == b"SCNE", "wave bank %d group %d: bad SCNE" % (no, g))
            ctrl = u32(w, cs + 0xC)
            nwave = u32(w, ctrl + 4)
            arc = u32(w, abank + 8 + 4 * g)
            name = w[arc:arc + 0x74].split(b"\0")[0].decode("ascii")
            waves = []
            for k in range(nwave):
                wid = u32(w, u32(w, ctrl + 8 + 4 * k)) & 0xFFFF
                wo = u32(w, arc + 0x74 + 4 * k)
                check(wo + 0x2C <= len(w), "wave bank %d: wave record %#x outside the bank" % (no, wo))
                waves.append((wid, wo))
                maxid = max(maxid, wid)
            groups.append((name, waves))

        def wline(wo):
            return (" u0=%d u1=%d u2=%d rate=%s off=%d C=%d 10=%d 14=%d 18=%d 1c=%d 20=%d 22=%d 28=%d" %
                    (w[wo], w[wo + 1], w[wo + 2], fbits(w, wo + 4), s32(w, wo + 8), s32(w, wo + 0xC), s32(w, wo + 0x10),
                     s32(w, wo + 0x14), s32(w, wo + 0x18), s32(w, wo + 0x1C), s16(w, wo + 0x20), s16(w, wo + 0x22),
                     s32(w, wo + 0x28)))
        if ngroups == 1:
            name, waves = groups[0]
            L.append("WBANK %d simple groups=1 tableSize=%d name=%s" % (no, maxid + 1, name))
            table = {}
            for wid, wo in waves:
                table[wid] = wo
            for wid in sorted(table):
                L.append("  WAVE id=%d%s" % (wid, wline(table[wid])))
                stat("waves")
        else:
            L.append("WBANK %d basic groups=%d tableSize=%d" % (no, ngroups, maxid + 1))
            for g, (name, waves) in enumerate(groups):
                L.append(" GROUP %d waves=%d name=%s" % (g, len(waves), name))
                for wid, wo in waves:
                    L.append("  WAVE id=%d%s" % (wid, wline(wo)))
                    stat("waves")
        stat("wave banks")
    return L


# -------------------------------------------------------------------------------------------------------- BARC
def expect_barc(d, recs, arcfile):
    L = []
    h = [r for r in recs if r[0] == 4][0]
    b = d[h[1]:h[1] + h[2]]
    check(b[:8] == b"BARC----", "sequence archive header: bad magic")
    count = u32(b, 0xC)
    check(0x20 + 0x20 * count == len(b), "BARC: header says %d entries, blob has room for %d" % (count, (len(b) - 0x20) // 0x20))
    name = b[0x10:0x20].split(b"\0")[0].decode("ascii")
    check(name == "sequence.arc", "BARC file name %r" % name)
    entries = []
    for i in range(count):
        e = 0x20 + 0x20 * i
        entries.append((u16(b, e + 0xE), u32(b, e + 0x18), u32(b, e + 0x1C), "%08x" % u32(b, e + 0x14)))
    spans = []
    for i, (link, ofs, size, u14) in enumerate(entries):
        seen = set()
        while link != 0xFFFF:  # follow the alias chain like Vload::getRealHandle
            check(link < count and link not in seen, "BARC entry %d: bad alias link" % i)
            seen.add(link)
            link, ofs, size, u14 = entries[link][0], entries[link][1], entries[link][2], entries[link][3]
        spans.append((ofs, size))
    arcsize = os.path.getsize(arcfile)
    for ofs, size in spans:
        check(ofs + size <= arcsize, "BARC: sequence %#x+%#x outside sequence.arc (%#x)" % (ofs, size, arcsize))
    srt = sorted(set(spans))
    for (a0, a1), (b0, b1) in zip(srt, srt[1:]):
        check(a0 + a1 <= b0, "BARC: sequences overlap")
    for pas in range(2):
        L.append("BARC pass=%d handle=%08x" % (pas, pas << 16))
        for i, (ofs, size) in enumerate(spans):
            e = i
            link, o2, s2, u14 = entries[e]
            while link != 0xFFFF:
                link, o2, s2, u14 = entries[link]
            L.append("ENT %d size=%d ofs=%d link=65535 u14=%s" % (i, s2, o2, u14))
    stat("sequences", count)
    return L, spans


# ---------------------------------------------------------------------------------------------------------- BAS
def collect_bas(limit):
    d = os.path.join(OUT, "audio", "bas")
    lst = os.path.join(OUT, "audio", "bas.txt")
    files = []
    if os.path.exists(lst):
        files = [l.strip() for l in open(lst, encoding="utf-8") if l.strip()]
    if not files:
        os.makedirs(d, exist_ok=True)
        arcs = sorted(glob.glob(fmt_arc_extract.ROOT + "/data/*.szs")) + sorted(glob.glob(fmt_arc_extract.ROOT + "/data/scene/*.szs"))
        n = 0
        for a in arcs:
            raw = open(a, "rb").read()
            r = fmt_arc_extract.yaz0(raw) if raw[:4] == b"Yaz0" else raw
            for p, b in fmt_arc_extract.rarc_files(r):
                if p.lower().endswith(".bas"):
                    fn = os.path.join(d, "%05d.bas" % n)
                    open(fn, "wb").write(b)
                    files.append(fn)
                    n += 1
        open(lst, "w", encoding="utf-8", newline="\n").write("\n".join(files) + "\n")
    return files[:limit] if limit else files


def expect_bas(files):
    L = []
    for fn in files:
        b = open(fn, "rb").read()
        n = u16(b, 0)
        check(len(b) == 8 + 0x20 * n, "%s: size %d != 8 + 0x20 * %d" % (fn, len(b), n))
        L.append("BAS %s count=%d" % (fn, n))
        for i in range(n):
            e = 8 + 0x20 * i
            L.append(" E %d id=%08x s=%s e=%s p=%s f=%08x v=%d ps=%d sd=%d pan=%d vs=%d" %
                     (i, u32(b, e), fbits(b, e + 4), fbits(b, e + 8), fbits(b, e + 0xC), u32(b, e + 0x10), b[e + 0x14],
                      struct.unpack_from(">b", b, e + 0x15)[0], b[e + 0x16], b[e + 0x17], struct.unpack_from(">b", b, e + 0x18)[0]))
            stat("bas records")
        stat("bas files")
    return L



# ---------------------------------------------------------------------------------- wave archives and streams
def check_aw(d, recs, banksdir):
    """Every wave of every WSYS group lies inside its .aw file (the loader copies the file to ARAM and getWavePtr() adds
    the wave offset to the ARAM base) and the waves of a group do not overlap."""
    for no, r in enumerate([r for r in recs if r[0] == 3]):
        w = d[r[1]:r[1] + r[2]]
        abank, cgroup = u32(w, 0x10), u32(w, 0x14)
        for g in range(u32(w, cgroup + 8)):
            arc = u32(w, abank + 8 + 4 * g)
            name = w[arc:arc + 0x74].split(b"\0")[0].decode("ascii")
            path = os.path.join(banksdir, name)
            if not check(os.path.exists(path), "wave bank %d group %d: %s is not on the disc" % (no, g, name)):
                continue
            size = os.path.getsize(path)
            cs = u32(w, cgroup + 0xC + 4 * g)
            nwave = u32(w, u32(w, cs + 0xC) + 4)
            spans = []
            for k in range(nwave):
                wo = u32(w, arc + 0x74 + 4 * k)
                off, sz = u32(w, wo + 8), u32(w, wo + 0xC)
                check(off + sz <= size, "%s wave %d: %#x+%#x beyond the file (%#x)" % (name, k, off, sz, size))
                spans.append((off, off + sz))
                stat("wave ranges inside their .aw")
            spans.sort()
            for (a0, a1), (b0, b1) in zip(spans, spans[1:]):
                check(a1 <= b0, "%s: waves overlap" % name)
            stat("wave archives (.aw)")


def check_stream(d, recs, afc):
    """The 0x20-byte AFC header cached in the stream list is the header of the file itself."""
    st = [r for r in recs if r[0] == 5][0]
    head = open(afc, "rb").read(0x20)
    check(d[st[1] + 0x20:st[1] + 0x40] == head, "stream list header differs from the first 0x20 bytes of %s" % afc)
    n, nsamp, rate, fmt = u32(head, 0), u32(head, 4), u16(head, 8), u16(head, 10)
    check(os.path.getsize(afc) >= 0x20 + n, "stream: %d audio bytes do not fit in %s" % (n, afc))
    check(fmt == 4 and rate == 32000 and n // 18 * 16 >= nsamp - 32, "stream header %s looks inconsistent" % head.hex())
    stat("stream headers")

# ------------------------------------------------------------------------------------------------------- driver
def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    name = args[0]
    limit = 0
    if "--bas-limit" in sys.argv:
        limit = int(sys.argv[sys.argv.index("--bas-limit") + 1])
    aafdir = os.path.join(OUT, "audio")
    aaf = os.path.join(aafdir, "nintendo.szs", "nintendo", "audi", "msound.aaf")
    if not os.path.exists(aaf):
        os.makedirs(aafdir, exist_ok=True)
        fmt_arc_extract.extract(os.path.join(fmt_arc_extract.ROOT, "data", "nintendo.szs"), aafdir)
    seqarc = os.path.join(fmt_arc_extract.ROOT, "AudioRes", "Seqs", "sequence.arc")
    if "--no-build" not in sys.argv:
        r = subprocess.run(["cmd", "/c", os.path.join(HERE, "build_audio.cmd"), name], capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            print(r.stdout[-3000:], r.stderr[-2000:])
            print("build failed")
            return 1
    d = open(aaf, "rb").read()
    recs = parse_aaf(d)
    print("AAF: %d bytes, %d records: %s" % (len(d), len(recs), sorted(set(r[0] for r in recs))))

    diff("aaf", run(["aaf", aaf]), expect_aaf(d, recs))
    bnk, osc_refs = expect_bnk(d, recs)
    diff("bnk", run(["bnk", aaf]), bnk)
    diff("wsys", run(["wsys", aaf]), expect_wsys(d, recs))
    check_aw(d, recs, os.path.join(fmt_arc_extract.ROOT, "AudioRes", "Banks"))
    check_stream(d, recs, os.path.join(fmt_arc_extract.ROOT, "AudioRes", "Streams", "title.afc"))
    barc, spans = expect_barc(d, recs, seqarc)
    diff("barc", run(["barc", aaf, seqarc]), barc)

    files = collect_bas(limit)
    lst = os.path.join(aafdir, "bas_run.txt")
    open(lst, "w", encoding="utf-8", newline="\n").write("\n".join(files) + "\n")
    diff("bas", run(["bas", lst]), expect_bas(files))

    # oscillator tables of the banks: converted by the loader, converted again by oscTableToHost (must be a no-op)
    olst = os.path.join(aafdir, "osc.txt")
    uniq = sorted(set(osc_refs))
    open(olst, "w", encoding="utf-8", newline="\n").write("\n".join("%s %d" % (aaf, o) for o in uniq) + "\n")
    want = []
    for o in uniq:
        t = osc_table(d, o)
        want.append("OSC %s %d n=%d %s" % (aaf, o, len(t), " ".join(str(x) for x in t)))
    diff("osc", run(["osc", olst]), want)

    # sequence operand readers on the real sequence bytes
    sq = open(seqarc, "rb").read()
    rnd = random.Random(1234)
    offs = [rnd.randrange(0, len(sq) - 4) for _ in range(3000)] + [0, 1, len(sq) - 4]
    want = []
    for o in offs:
        want.append("SEQ %d g16=%04x g24=%06x g32=%08x r16=%04x r24=%06x r32=%08x" %
                    (o, u16(sq, o), u32(sq, o) >> 8, u32(sq, o), u16(sq, o), u32(sq, o) >> 8, u32(sq, o)))
    diff("seq", run(["seq", seqarc] + [str(o) for o in offs]), want)

    # replay of every sequence through the real TTrack/TSeqParser: no register id, port, timed parameter, child track,
    # oscillator, interrupt slot or table index outside its table (the game code reports each one on stderr as
    # "JASystem: ..." / "BADREG" and TRegisterParam counts register accesses outside 0x00-0x1F and the special ids)
    r = subprocess.run([EXE, "replay", aaf, seqarc, "6000"], capture_output=True, text=True, errors="replace", timeout=900)
    out = r.stdout.splitlines()
    summ = [l for l in out if l.startswith("SUMMARY")]
    print("[replay]", summ[0] if summ else "no summary", "| exit", r.returncode)
    for l in out:
        if l.startswith("REGISTERS"):
            print("   ", l[:200])
    check(r.returncode == 0 and summ and "badRegisterAccess=0" in summ[0], "replay: register access outside the register file (exit %d)" % r.returncode)
    bad = [l for l in r.stderr.splitlines() if l.startswith("JASystem:") or l.startswith("BADREG")]
    check(not bad, "replay: game code reported out-of-table accesses: %s" % "; ".join(bad[:3]))
    stat("sequences replayed", len([l for l in out if l.startswith("REPLAY")]))

    print("coverage:", ", ".join("%s %d" % (k, v) for k, v in sorted(STATS.items())))
    print("RESULT:", "FAIL (%d)" % len(FAILS) if FAILS else "all formats match")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
