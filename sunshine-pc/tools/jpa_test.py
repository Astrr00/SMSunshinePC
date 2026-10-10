"""Check the host JPA (.jpa particle resource) pipeline against an independent big-endian parse of the same file bytes.

Usage: python tools/jpa_test.py <jpa_test.exe> [--scenes] [--no-calc]        (tools\\jpa_test.cmd <build name> builds the exe)
  1. extracts every .jpa of data/particle.szs, data/bosshanachanJpa.szs and data/select.szs with its own Yaz0 + RARC code
     (tools/fmt_arc_extract.py); --scenes also takes every data/scene/*.szs (decoded by the exe's real JKRDecomp, the
     pure-Python Yaz0 decoder is too slow for 250 MB),
  2. runs jpa_test.exe, which loads every resource through the real JPAResourceManager/JPAEmitterManager code and prints what
     the host structures hold ("key value", floats as f<bits>),
  3. parses the raw big-endian bytes here with struct.unpack('>...') following the retail reader order (the loader code,
     and loadFieldBlock/loadBaseEmitterBlock checked against the retail assembly) and diffs every key.
Also checks the file layout itself: header, blocks tile the file, TEX1 blocks last, every block size/offset/texture index/key
table inside its block, and that the loader left the file image untouched (the exe compares it). Exit code 0 only if
everything matches.
"""
import glob
import math
import os
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fmt_arc_extract  # noqa: E402

DATA = os.path.join(fmt_arc_extract.ROOT, "data")
FAILS = []
STATS = {}
MALFORMED = []


def fail(msg):
    FAILS.append(msg)
    if len(FAILS) <= 60:
        print("FAIL", msg)


def stat(k, n=1):
    STATS[k] = STATS.get(k, 0) + n


def u8(b, o): return b[o]
def s8(b, o): return struct.unpack_from(">b", b, o)[0]
def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def s16(b, o): return struct.unpack_from(">h", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]
def s32(b, o): return struct.unpack_from(">i", b, o)[0]
def fbits(b, o): return "f%08x" % u32(b, o)


def f32(x):
    """round a Python double to the nearest float32 (+, -, *, / and sqrt of float32 operands are then exact IEEE float ops:
    53 >= 2 * 24 + 2, no double rounding)"""
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def fb(x): return "f%08x" % struct.unpack("<I", struct.pack("<f", x))[0]
def fval(s): return struct.unpack("<f", struct.pack("<I", int(s[1:], 16)))[0]
def hexs(b): return "h" + bytes(b).hex()


def fix(p):
    """JPAConvertFixToFloat(s16)"""
    if p == 0x7FFF:
        return 1.0
    f0 = f32(f32(f32(float(p)) * f32(1.0 / 32768.0)) * 100000.0)
    r5 = int(f0)
    r0 = int(math.fmod(r5, 10))
    if r0 >= 5:
        r5 += 10 - r0
    else:
        r5 -= r0
    return f32(f32(float(r5)) * f32(1e-05))


def fixb(b, o): return fb(fix(s16(b, o)))
def fixb10(b, o): return fb(f32(fix(s16(b, o)) * 10.0))


# GX enums used by the shapes
GX_CC = {"ZERO": 15, "TEXC": 8, "ONE": 12, "C0": 2, "C1": 4}
TEV = {
    0: ("ZERO", "TEXC", "ONE", "ZERO"),
    1: ("ZERO", "C0", "TEXC", "ZERO"),
    2: ("C0", "ONE", "TEXC", "ZERO"),
    3: ("C1", "C0", "TEXC", "ZERO"),
    4: ("ZERO", "TEXC", "C0", "C1"),
    5: ("ZERO", "ZERO", "ZERO", "C0"),
}
GX_ITM = {0: 1, 1: 1, 2: 5, 3: 9}


def blocks_of(raw):
    n = u32(raw, 12)
    off = 0x20
    out = []
    for _ in range(n):
        out.append((raw[off:off + 4], off, u32(raw, off + 4)))
        off += u32(raw, off + 4)
    return out, off


def cstr(raw, o, maxlen=0x14):
    e = raw.find(b"\0", o, o + maxlen)
    return raw[o:e] if e >= 0 else None


# ---- BTI image sizes (structural check of TEX1) ------------------------------------------------------------------
# format -> (block w, block h, bits per pixel)
BTI_FMT = {0: (8, 8, 4), 1: (8, 4, 8), 2: (8, 4, 8), 3: (4, 4, 16), 4: (4, 4, 16), 5: (4, 4, 16), 6: (4, 4, 32),
           8: (8, 8, 4), 9: (8, 4, 8), 10: (4, 4, 16), 14: (8, 8, 4)}


def image_size(fmt, w, h, mips):
    bw, bh, bpp = BTI_FMT[fmt]
    total = 0
    for _ in range(max(mips, 1)):
        pw = (w + bw - 1) // bw * bw
        ph = (h + bh - 1) // bh * bh
        total += pw * ph * bpp // 8
        w, h = max(w // 2, 1), max(h // 2, 1)
    return total


# ---- expected values ---------------------------------------------------------------------------------------------
def color_table(raw, boff, bsize, key_off, key_num, size):
    """makeColorTable: float32 stepping, keys read like the console (also the one past the end)"""
    base = boff + key_off

    def kidx(j):
        o = base + 6 * j
        return s16(raw, o) if o + 2 <= len(raw) else 0

    def kcol(j):
        o = base + 6 * j + 2
        return raw[o:o + 4].ljust(4, b"\0")

    out = bytearray((size + 1) * 4)
    steps = [0.0] * 4
    cur = [float(x) for x in kcol(0)]
    j = 0
    for i in range(size + 1):
        if i == kidx(j):
            col = kcol(j)
            out[i * 4:i * 4 + 4] = col
            cur = [float(x) for x in col]
            j += 1
            if j < key_num:
                diff = [float(x) for x in kcol(j)]
                d = kidx(j) - kidx(j - 1)
                if d == 0:
                    fail("color key %d has the same index as the previous one" % j)
                    return bytes(out)
                step = f32(1.0 / d)
                steps = [f32(step * f32(diff[c] - cur[c])) for c in range(4)]
            else:
                steps = [0.0] * 4
        else:
            for c in range(4):
                cur[c] = f32(cur[c] + steps[c])
                out[i * 4 + c] = int(cur[c]) & 0xFF
    return bytes(out)


def expect_bsp(raw, bo, bs, ntex, d):
    b = raw[bo:bo + bs]
    # the shape reads from the file image: all offsets are relative to the block start
    d["bsp.sizeY"] = fbits(b, 0x18)
    d["bsp.sizeX"] = fbits(b, 0x1C)
    d["bsp.loopOffset"] = s16(b, 0x20)
    d["bsp.type"], d["bsp.dirType"], d["bsp.rotType"] = b[0x24], b[0x25], b[0x26]
    r5, r6 = b[0x22], b[0x23]
    d["bsp.colLoop"] = -1 if r5 & 1 else 0
    d["bsp.texLoop"] = -1 if r6 & 1 else 0
    t = b[0x24]
    a = 1 if ((r5 & 2) or t in (5, 6)) else 0
    c = 1 if ((r6 & 2) or t in (5, 6)) else 0
    d["bsp.unk68"] = (a << 1) | c
    if b[0x30] not in TEV:
        fail("BSP1 tev mode %d" % b[0x30])
    else:
        for k, nm in enumerate(TEV[b[0x30]]):
            d["bsp.tev%d" % k] = GX_CC[nm]
    d["bsp.unk6C"] = b[0x31]
    d["bsp.blend"], d["bsp.src"], d["bsp.dst"], d["bsp.blendOp"] = b[0x35], b[0x36], b[0x37], b[0x38]
    d["bsp.acmp0"], d["bsp.aref0"], d["bsp.aop"], d["bsp.acmp1"], d["bsp.aref1"] = b[0x39], b[0x3A], b[0x3B], b[0x3C], b[0x3D]
    d["bsp.zloc"] = 0 if (b[0x3E] >> 1) & 1 else 1
    d["bsp.unk77"], d["bsp.zfunc"], d["bsp.unk79"], d["bsp.unk7A"], d["bsp.unk7B"] = b[0x3F], b[0x40], b[0x41], b[0x42], b[0x43]
    d["bsp.flags"], d["bsp.unk7D"] = b[0x44], b[0x4D]
    nk = b[0x4E]
    d["bsp.texAnmKeyNum"], d["bsp.texIndex"] = nk, b[0x4F]
    d["bsp.unk80"] = 1 if ((b[0x4C] & 1) and nk != 0) else 0
    d["bsp.unk81"] = 0 if (b[0x4C] & 2) else 1
    d["bsp.colMaxFrm"] = s16(b, 0x5C)
    d["bsp.unk82"], d["bsp.unk83"], d["bsp.unk84"], d["bsp.unk85"], d["bsp.unk86"] = b[0x5E], b[0x60], b[0x61], b[0x62], b[0x63]
    d["bsp.unk87"] = b[0x96]
    d["bsp.prm"], d["bsp.env"] = hexs(b[0x64:0x68]), hexs(b[0x68:0x6C])
    for k, o in (("texStaticTransX", 0x80), ("texStaticTransY", 0x82), ("texStaticScaleX", 0x84), ("texStaticScaleY", 0x86),
                 ("tilingX", 0x88), ("tilingY", 0x8A)):
        d["bsp." + k] = fixb10(b, o)
    d["bsp.scrollTransX"], d["bsp.scrollTransY"] = fixb(b, 0x8C), fixb(b, 0x8E)
    d["bsp.scrollScaleX"] = fb(f32(fix(s16(b, 0x90)) * f32(0.1)))
    d["bsp.scrollScaleY"] = fb(f32(fix(s16(b, 0x92)) * f32(0.1)))
    d["bsp.scrollRotate"] = fixb(b, 0x94)
    # structural: tables inside the block
    fr = d["bsp.colMaxFrm"]
    if d["bsp.unk80"]:
        o = s16(b, 0x12)
        if not (0x98 <= o and o + nk <= bs):
            fail("BSP1 texture index table %#x+%d outside block %#x" % (o, nk, bs))
        d["bsp.texIndices"] = hexs(b[o:o + nk])
        for v in b[o:o + nk]:
            if v >= ntex:
                fail("BSP1 animated texture index %d of %d" % (v, ntex))
    # an unanimated, non-empty texture addresses the file's texture list directly
    if not d["bsp.unk81"] and not d["bsp.unk80"] and b[0x4F] >= ntex:
        fail("BSP1 texture index %d of %d" % (b[0x4F], ntex))
    for flag, ko, kn, key in ((d["bsp.unk83"] & 2, 0x14, d["bsp.unk85"], "bsp.prmTable"),
                              (d["bsp.unk84"] & 2, 0x16, d["bsp.unk86"], "bsp.envTable")):
        if flag:
            o = s16(b, ko)
            if kn == 0 or fr < 0 or not (0x98 <= o and o + 6 * kn <= bs):
                fail("BSP1 color key table %#x x%d (frames %d) outside block %#x" % (o, kn, fr, bs))
                continue
            d[key] = hexs(color_table(raw, bo, bs, o, kn, fr))
    return None


def expect_esp(b, d):
    d["esp.alphaInTiming"] = fixb(b, 0x14)
    d["esp.alphaOutTiming"] = fixb(b, 0x16)
    d["esp.alphaInValue"] = fixb(b, 0x18)
    d["esp.alphaBaseValue"] = fixb(b, 0x1A)
    d["esp.unk18"] = fixb(b, 0x1C)
    d["esp.unk78"], d["esp.unk79"] = b[0x1E], b[0x1F]
    d["esp.waveParam1"], d["esp.waveParam2"], d["esp.waveParam3"] = fixb(b, 0x20), fixb(b, 0x22), fixb(b, 0x24)
    d["esp.waveRandom"] = fixb(b, 0x26)
    ain, aout, aval, abase, a18 = (fix(s16(b, o)) for o in (0x14, 0x16, 0x18, 0x1A, 0x1C))
    d["esp.alphaIncRate"] = fb(f32(f32(abase - aval) / ain) if ain != 0.0 else 1.0)
    d["esp.alphaDecRate"] = fb(f32(f32(a18 - abase) / f32(1.0 - aout)) if aout != 1.0 else 1.0)
    d["esp.randomScale"] = fixb(b, 0x34)
    sin_, sout = fix(s16(b, 0x36)), fix(s16(b, 0x38))
    d["esp.scaleInTiming"], d["esp.scaleOutTiming"] = fb(sin_), fb(sout)
    sy = f32(fix(s16(b, 0x3A)) * 10.0)
    u44 = f32(fix(s16(b, 0x3E)) * 10.0)
    sx = f32(fix(s16(b, 0x44)) * 10.0)
    u4c = f32(fix(s16(b, 0x48)) * 10.0)
    d["esp.scaleInValueY"], d["esp.unk44"], d["esp.scaleInValueX"], d["esp.unk4C"] = fb(sy), fb(u44), fb(sx), fb(u4c)
    d["esp.unk7A"], d["esp.unk7B"] = b[0x40], b[0x41]
    d["esp.cycleY"], d["esp.cycleX"] = s16(b, 0x42), s16(b, 0x4C)
    d["esp.unk7C"], d["esp.unk7D"], d["esp.unk7E"] = b[0x4A], b[0x4B], b[0x4E]
    if sin_ != 0.0:
        d["esp.incRateX"] = fb(f32(f32(1.0 - sx) / sin_))
        d["esp.incRateY"] = fb(f32(f32(1.0 - sy) / sin_))
    else:
        d["esp.incRateX"] = d["esp.incRateY"] = fb(1.0)
    if sout != 1.0:
        den = f32(1.0 - sout)
        d["esp.decRateX"] = fb(f32(f32(u4c - 1.0) / den))
        d["esp.decRateY"] = fb(f32(f32(u44 - 1.0) / den))
    else:
        d["esp.decRateX"] = d["esp.decRateY"] = fb(1.0)
    d["esp.rotAngle"], d["esp.rotSpeed"] = fixb(b, 0x5A), fixb(b, 0x5C)
    d["esp.rotRandSpeed"], d["esp.rotRandAngle"], d["esp.rotDir"] = fixb(b, 0x5E), fixb(b, 0x60), fixb(b, 0x62)
    d["esp.unk7F"] = b[0x64]


def expect_ssp(b, ntex, d):
    d["ssp.type"], d["ssp.dirType"], d["ssp.rotType"] = b[0x10], b[0x11], b[0x12]
    d["ssp.life"], d["ssp.rate"] = s16(b, 0x14), s16(b, 0x16)
    d["ssp.step"] = b[0x1A]
    d["ssp.unk8"], d["ssp.baseVel"] = fbits(b, 0x28), fbits(b, 0x2C)
    d["ssp.affectedByFields"], d["ssp.unk49"], d["ssp.scaleOut"], d["ssp.alphaOut"] = b[0x36], b[0x44], b[0x45], b[0x46]
    d["ssp.texIndex"] = b[0x47]
    if b[0x47] >= max(ntex, 1):
        fail("SSP1 texture index %d of %d" % (b[0x47], ntex))
    d["ssp.scaleY"], d["ssp.scaleX"] = fbits(b, 0x4C), fbits(b, 0x50)
    d["ssp.enableRotate"], d["ssp.unk4E"] = b[0x56], b[0x57]
    d["ssp.prm"], d["ssp.env"] = hexs(b[0x58:0x5C]), hexs(b[0x5C:0x60])
    d["ssp.timing"], d["ssp.velInh"], d["ssp.baseVelRndm"], d["ssp.unk24"] = fixb(b, 0x18), fixb(b, 0x30), fixb(b, 0x32), fixb(b, 0x34)
    d["ssp.inheritScale"], d["ssp.inheritAlpha"], d["ssp.inheritRGB"], d["ssp.rotSpeed"] = fixb(b, 0x48), fixb(b, 0x4A), fixb(b, 0x60), fixb(b, 0x54)


def expect_etx(b, d):
    sel = b[0x11]
    d["etx.mode"] = b[0x10]
    if sel > 3:
        fail("ETX1 matrix mode %d" % sel)
        return
    d["etx.mtxId"] = GX_ITM[sel]
    if sel <= 1:
        for k, o in enumerate(range(0x12, 0x1E, 2)):
            d["etx.m%d%d" % (k // 3, k % 3)] = fixb(b, o)
    d["etx.expScale"], d["etx.indTex"], d["etx.subTex"] = b[0x1E], b[0x1F], b[0x20]
    d["etx.flags"], d["etx.secondTex"] = b[0x30], b[0x33]


def expect_kfa(i, b, d):
    n = b[0x10]
    p = "kfa%d." % i
    d[p + "count"], d[p + "loop"], d[p + "size"] = n, b[0x12], u32(b, 4)
    if (0x20 + 16 * n + 0x1F) & ~0x1F != u32(b, 4):  # blocks are padded to 32 bytes
        fail("KFA1 %d: %d keys do not fit the block (%#x)" % (i, n, u32(b, 4)))
    for j in range(n * 4):
        d[p + "k%d" % j] = fbits(b, 0x20 + 4 * j)
    d[p + "crcHead"] = zlib.crc32(b[:0x20])
    # key times must increase (the binary search in JPAGetKeyFrameInterpolation relies on it)
    t = [struct.unpack_from(">f", b, 0x20 + 16 * k)[0] for k in range(n)]
    if any(t[k] > t[k + 1] for k in range(n - 1)):
        fail("KFA1 %d key times not ascending" % i)


def expect_tex(i, b, idx, d):
    p = "tex%d." % i
    size = u32(b, 4)
    d[p + "idx"] = idx
    d[p + "nameStr"] = "s" + cstr(b, 0xC).decode("ascii")
    h = 0x20
    fmt, alpha, w, ht = b[h], b[h + 1], u16(b, h + 2), u16(b, h + 4)
    d[p + "format"], d[p + "alpha"], d[p + "width"], d[p + "height"] = fmt, alpha, w, ht
    d[p + "wrapS"], d[p + "wrapT"], d[p + "isIndex"], d[p + "colorFormat"] = b[h + 6], b[h + 7], b[h + 8], b[h + 9]
    d[p + "numColors"] = u16(b, h + 0xA)
    d[p + "paletteOffset"] = s32(b, h + 0xC)
    d[p + "mipmap"], d[p + "edgeLod"], d[p + "biasClamp"], d[p + "aniso"] = b[h + 0x10], b[h + 0x11], b[h + 0x12], b[h + 0x13]
    d[p + "minFilter"], d[p + "magFilter"] = b[h + 0x14], b[h + 0x15]
    d[p + "minLod"], d[p + "maxLod"] = s8(b, h + 0x16), s8(b, h + 0x17)
    d[p + "mipCount"] = b[h + 0x18]
    d[p + "lodBias"] = s16(b, h + 0x1A)
    ofs = s32(b, h + 0x1C)
    d[p + "imageOffset"], d[p + "size"] = ofs, size
    d[p + "jut.width"], d[p + "jut.height"], d[p + "jut.format"] = w, ht, fmt
    dofs = h + ofs if ofs != 0 else h + 0x20
    d[p + "jut.dataOfs"] = d[p + "obj.dataOfs"] = dofs
    d[p + "obj.width"], d[p + "obj.height"], d[p + "obj.format"] = w, ht, fmt
    d[p + "crc"] = zlib.crc32(b[0x40:size], zlib.crc32(b[:0x20]))
    # structural: image (+ palette) inside the block
    if fmt not in BTI_FMT:
        fail("TEX1 %s: unknown BTI format %d" % (d[p + "nameStr"], fmt))
    else:
        end = dofs + image_size(fmt, w, ht, b[h + 0x18] if b[h + 0x10] else 1)
        if end > size:
            fail("TEX1 %s: image ends at %#x past block %#x" % (d[p + "nameStr"], end, size))
        if d[p + "numColors"]:
            po = h + s32(b, h + 0xC)
            if po + d[p + "numColors"] * 2 > size:
                fail("TEX1 %s: palette outside block" % d[p + "nameStr"])
    stat("textures")


def expect_bem(b, d):
    d["bem.scale.x"], d["bem.scale.y"], d["bem.scale.z"] = fbits(b, 0xC), fbits(b, 0x10), fbits(b, 0x14)
    d["bem.trans.x"], d["bem.trans.y"], d["bem.trans.z"] = fbits(b, 0x18), fbits(b, 0x1C), fbits(b, 0x20)
    d["bem.rot.x"], d["bem.rot.y"], d["bem.rot.z"] = s16(b, 0x24), s16(b, 0x26), s16(b, 0x28)
    d["bem.volumeType"], d["bem.emitInterval"] = b[0x2A], b[0x2B]
    d["bem.volumeSub"] = u16(b, 0x2E)
    d["bem.spawnRate"] = fbits(b, 0x30)
    d["bem.spawnRateVar"] = fixb(b, 0x34)
    d["bem.maxFrame"] = s16(b, 0x36)
    d["bem.startFrame"], d["bem.volumeSize"] = s16(b, 0x38), u16(b, 0x3A)
    d["bem.yawSweep"], d["bem.minRadius"] = fixb(b, 0x3C), fixb(b, 0x3E)
    d["bem.baseLife"] = s16(b, 0x40)
    d["bem.lifeRand"], d["bem.baseWeight"], d["bem.weightRand"] = fixb(b, 0x42), fixb(b, 0x44), fixb(b, 0x46)
    d["bem.unk1C4"], d["bem.unk1CC"] = fixb(b, 0x48), fixb(b, 0x4A)
    d["bem.airRes"], d["bem.airResVar"] = fixb(b, 0x4C), fixb(b, 0x4E)
    d["bem.unk1FC"], d["bem.unk200"], d["bem.unk204"], d["bem.unk208"], d["bem.unk1C8"] = (fbits(b, o) for o in (0x50, 0x54, 0x58, 0x5C, 0x60))
    v = [fix(s16(b, o)) for o in (0x64, 0x66, 0x68)]
    n = math.sqrt(sum(x * x for x in v))
    for k, c in enumerate("xyz"):
        d["bem.dir." + c] = fb(f32(v[k] / n)) if n > 1e-6 else fb(0.0)
    d["bem.unk1E4"] = fixb(b, 0x6A)
    d["bem.emitFlags"], d["bem.keyMask"] = u32(b, 0x6C), u32(b, 0x70)
    d["bem.crc"] = zlib.crc32(b)
    # the volume type indexes a switch of 7, the key mask must not name more animations than the block has
    if b[0x2A] > 6:
        fail("BEM1 volume type %d" % b[0x2A])


def expect_fld(i, b, d):
    p = "fld%d." % i
    d[p + "type"], d[p + "unk51"], d[p + "unk52"], d[p + "unk53"] = b[0xC], b[0xD], b[0xE], b[0xF]
    if b[0xC] > 7:
        fail("FLD1 %d: type %d" % (i, b[0xC]))
    d[p + "status"] = u16(b, 0x10)
    d[p + "unk10"], d[p + "unk14"], d[p + "maxDist"] = fbits(b, 0x14), fbits(b, 0x18), fbits(b, 0x1C)
    for k, c in enumerate("xyz"):
        d[p + "unk18." + c] = fbits(b, 0x20 + 4 * k)
        d[p + "unk24." + c] = fbits(b, 0x2C + 4 * k)
    d[p + "unk30"], d[p + "unk34"], d[p + "unk38"] = fbits(b, 0x38), fbits(b, 0x3C), u32(b, 0x40)
    d[p + "fadeInEnd"], d[p + "fadeOutStart"], d[p + "fadeInStart"], d[p + "fadeOutEnd"] = (fixb(b, o) for o in (0x44, 0x46, 0x48, 0x4A))
    d[p + "crc"] = zlib.crc32(b)


def expect_file(raw, tex_state, path):
    """-> dict of expected key/value strings"""
    d = {}
    if raw[:8] != b"JEFFjpa1":
        fail("bad magic %r" % raw[:8])
        return d
    bl, end = blocks_of(raw)
    known = (b"BEM1", b"BSP1", b"ESP1", b"SSP1", b"ETX1", b"FLD1", b"KFA1", b"TEX1")
    if u32(raw, 8) != len(raw) or end != len(raw) or any(t not in known for t, _, _ in bl):
        # a defect of the disc data (ms_m_spinos.jpa in coronaBoss.szs has a 1-byte shift before its TEX1 block): the
        # loader must survive it (the exe loads it), but there is nothing meaningful to compare
        MALFORMED.append(path)
        return None
    types = [t for t, _, _ in bl]
    ntex = types.count(b"TEX1")
    if types[len(types) - ntex:] != [b"TEX1"] * ntex:
        fail("TEX1 blocks are not last: %s" % types)
    for need in (b"BEM1", b"BSP1"):
        if types.count(need) != 1:
            fail("expected one %s, found %d" % (need, types.count(need)))
    for t in (b"ESP1", b"SSP1", b"ETX1"):
        if types.count(t) > 1:
            fail("more than one %s" % t)
    for t in set(types):
        if t not in (b"BEM1", b"BSP1", b"ESP1", b"SSP1", b"ETX1", b"FLD1", b"KFA1", b"TEX1"):
            fail("unknown block %r" % t)
    minsz = {b"BEM1": 0x74, b"BSP1": 0x98, b"ESP1": 0x68, b"SSP1": 0x64, b"ETX1": 0x34, b"FLD1": 0x4C, b"KFA1": 0x20, b"TEX1": 0x40}
    d["ed.count"] = 1
    d["link.fld"], d["link.kfa"], d["link.tex"] = types.count(b"FLD1"), types.count(b"KFA1"), ntex
    for k, t in (("bem", b"BEM1"), ("bsp", b"BSP1"), ("esp", b"ESP1"), ("ssp", b"SSP1"), ("etx", b"ETX1")):
        d["has." + k] = 1 if t in types else 0
    names, firsts = tex_state
    before = len(names)
    fld = kfa = tex = 0
    for t, o, sz in bl:
        b = raw[o:o + sz]
        if sz < minsz[t]:
            fail("%r block %#x smaller than %#x" % (t, sz, minsz[t]))
            continue
        if t == b"BEM1":
            expect_bem(b, d)
        elif t == b"BSP1":
            expect_bsp(raw, o, sz, ntex, d)
        elif t == b"ESP1":
            expect_esp(b, d)
        elif t == b"SSP1":
            expect_ssp(b, ntex, d)
        elif t == b"ETX1":
            expect_etx(b, d)
        elif t == b"FLD1":
            expect_fld(fld, b, d)
            fld += 1
        elif t == b"KFA1":
            expect_kfa(kfa, b, d)
            kfa += 1
    for t, o, sz in bl:
        if t != b"TEX1" or sz < 0x40:
            continue
        b = raw[o:o + sz]
        nm = cstr(b, 0xC)
        if nm is None:
            fail("TEX1 name not terminated")
            continue
        if nm in names:
            idx = names.index(nm)
            first = firsts[idx]
        else:
            idx = len(names)
            names.append(nm)
            firsts.append(b)
            first = b
        expect_tex(tex, first, idx, d)
        tex += 1
    d["tex.newCount"] = len(names) - before
    d["tex.total"] = len(names)
    return d


# ---- driver ----------------------------------------------------------------------------------------------------------
def python_files(arc):
    """{'/path/in/archive': bytes} of the .jpa files of a disc archive, decoded here"""
    d = open(arc, "rb").read()
    r = fmt_arc_extract.yaz0(d) if d[:4] == b"Yaz0" else d
    out = {}
    for p, b in fmt_arc_extract.rarc_files(r):
        if p.endswith(".jpa"):
            out["/" + p.split("/", 1)[1]] = b
    return out


def parse_dump(lines):
    """-> list of (archive name, [(path, size, user, {key: value})])"""
    archives = []
    cur = None
    for ln in lines:
        if ln.startswith("@ARCHIVE "):
            archives.append((ln.split()[1], []))
        elif ln.startswith("@FILE "):
            _, path, size, user = ln.split()
            archives[-1][1].append((path, int(size), int(user), {}))
        elif ln.strip():
            k, _, v = ln.partition(" ")
            archives[-1][1][-1][3][k] = v
    return archives


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    exe = sys.argv[1]
    opts = sys.argv[2:]
    arcs = [os.path.join(DATA, n) for n in ("particle.szs", "bosshanachanJpa.szs", "select.szs")]
    extra = []
    if "--scenes" in opts:
        extra = sorted(glob.glob(os.path.join(DATA, "scene", "*.szs")))
    expected = {}
    for a in arcs:
        expected[os.path.basename(a)] = python_files(a)
    tmp = None
    if extra:
        tmp = tempfile.mkdtemp(prefix="jpa_scenes_")
        r = subprocess.run([exe, "--extract", tmp] + extra, capture_output=True, text=True, errors="replace")
        if r.returncode != 0:
            fail("exe --extract exit %#x: %s" % (r.returncode & 0xFFFFFFFF, r.stderr[-300:]))
        for a in extra:
            base = os.path.basename(a)
            files = {}
            root = os.path.join(tmp, base)
            for dp, _, fns in os.walk(root):
                for fn in fns:
                    full = os.path.join(dp, fn)
                    files["/" + os.path.relpath(full, root).replace("\\", "/")] = open(full, "rb").read()
            if files:
                expected[base] = files
    run_arcs = [a for a in arcs + extra if os.path.basename(a) in expected and expected[os.path.basename(a)]]
    cmd = [exe] + run_arcs + (["--no-calc"] if "--no-calc" in opts else [])
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    sys.stderr.write(r.stderr[-1500:])
    if r.returncode != 0:
        fail("exe exit %#x" % (r.returncode & 0xFFFFFFFF))
    dump = parse_dump(r.stdout.splitlines())
    names = [a for a, _ in dump]
    for a in run_arcs:
        if os.path.basename(a) not in names:
            fail("archive %s missing from the dump" % a)

    for arcname, files in dump:
        exp_files = expected[arcname]
        malformed_before = len(MALFORMED)
        tex_state = ([], [])
        if sorted(p for p, _, _, _ in files) != sorted(exp_files):
            fail("%s: file list differs (%d dumped, %d expected)" % (arcname, len(files), len(exp_files)))
        for path, size, user, got in files:
            raw = exp_files.get(path)
            if raw is None:
                continue
            stat("files")
            if size != len(raw):
                fail("%s %s: size %d vs %d" % (arcname, path, size, len(raw)))
            before = len(FAILS)
            exp = expect_file(raw, tex_state, path)
            if exp is None:
                continue
            exp["ed.user"] = user
            if got.keys() != exp.keys():
                miss = sorted(set(exp) - set(got))[:6]
                extra_k = sorted(set(got) - set(exp))[:6]
                fail("%s %s: key sets differ; missing %s extra %s" % (arcname, path, miss, extra_k))
            for k in exp.keys() & got.keys():
                e, g = str(exp[k]), got[k]
                stat("keys")
                if k.startswith("bem.dir."):
                    ok = abs(fval(e) - fval(g)) <= 2e-3 if e[0] == "f" and g[0] == "f" else e == g
                else:
                    ok = e == g
                if not ok:
                    fail("%s %s: %s host %s expected %s" % (arcname, path, k, g, e))
            if len(FAILS) == before:
                stat("files ok")
        if arcname in ("particle.szs", "bosshanachanJpa.szs", "select.szs") and len(MALFORMED) != malformed_before:
            fail("%s: %d malformed resources (only scene archives may carry disc defects)" % (arcname, len(MALFORMED) - malformed_before))
    for m in MALFORMED:
        print("malformed on the disc (loaded without crashing, not compared):", m)
    print("jpa_test:", ", ".join("%s %d" % (k, v) for k, v in sorted(STATS.items())))
    print("%s (%d failures)" % ("FAILED" if FAILS else "OK", len(FAILS)))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
