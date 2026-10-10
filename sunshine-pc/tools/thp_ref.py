"""Independent references for tools/thp_test.cpp (the decomp's THP decoder, TARGET_PC code in sms/src/dolphin/thp).

Usage: python thp_ref.py <movie.thp> <frames> <dir with dec_video.bin / dec_audio.pcm from thp_test.exe>

Video: a second implementation of the whole frame decode, written from the console's assembly, not from the C port:
  * JPEG headers + canonical Huffman decode (the THP frames carry no 0xFF stuffing and no restart markers),
  * the paired-single inverse DCT as float32 arrays, one numpy operation per ps_* instruction; ps_madd/msub/nmsub are an
    exactly rounded float32 fma (float64 product, TwoSum, tie correction) so no double rounding,
  * GQR6 store: scale 2^-3, saturate, truncate toward zero.
The decoder output (GX I8 tile order) is un-tiled here and must match this model exactly (max error 0). ffmpeg's THP/MJPEG
decoder is a second, looser reference (integer IDCT: expect a few LSB).
Audio: the SDK algorithm re-written with Python integers (round to nearest, ties to even) and ffmpeg's adpcm_thp.
Writes frame0.png (the TEV YUV -> RGB of GXYuv2Rgb applied to frame 0) next to the dumps.
"""
import os
import struct
import subprocess
import sys

import numpy as np

f32 = np.float32


# ----------------------------------------------------------------------------------------------------------------------
# THP container
# ----------------------------------------------------------------------------------------------------------------------
def parse_thp(path, nframes, start=0):
    f = open(path, "rb")
    h = f.read(0x60)
    assert h[:4] == b"THP\0" and struct.unpack(">I", h[4:8])[0] == 0x11000
    bufsize, amax, rate, numframes, firstsize, mdsize, compoff, offsoff, movoff = struct.unpack(">IIfIIIIII", h[8:44])
    f.seek(compoff)
    ci = f.read(0x40)
    ncomp = struct.unpack(">I", ci[:4])[0]
    comps = list(ci[4:4 + ncomp])
    rec = 20
    video = audio = None
    for c in comps:
        if c == 0:
            video = struct.unpack(">III", ci[rec:rec + 12])
            rec += 12
        else:
            audio = struct.unpack(">IIII", ci[rec:rec + 16])
            rec += 16
    frames = []
    off, size = movoff, firstsize
    for _ in range(start):  # every frame starts with the size of the next one
        f.seek(off)
        nxt = struct.unpack(">I", f.read(4))[0]
        off += size
        size = nxt
    for _ in range(nframes):
        f.seek(off)
        d = f.read(size)
        nxt = struct.unpack(">I", d[:4])[0]
        sizes = struct.unpack(">%dI" % ncomp, d[8:8 + 4 * ncomp])
        v0 = 8 + 4 * ncomp
        vid = d[v0:v0 + sizes[0]]
        aud = []
        if audio:
            a0 = v0 + sizes[0]
            for t in range(audio[3]):
                aud.append(d[a0 + t * sizes[1]:a0 + (t + 1) * sizes[1]])
        frames.append((vid, aud))
        off += size
        size = nxt
    return dict(video=video, audio=audio, rate=rate, frames=frames, numframes=numframes)


# ----------------------------------------------------------------------------------------------------------------------
# JPEG (baseline, 4:2:0, THP flavour) -> coefficients
# ----------------------------------------------------------------------------------------------------------------------
ZIGZAG = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14,
          21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53,
          60, 61, 54, 47, 55, 62, 63] + [63] * 16
AAN = [float(f32(x)) for x in (1.0, 1.387039845, 1.306562965, 1.175875602, 1.0, 0.785694958, 0.541196100, 0.275899379)]


def parse_jpeg(d):
    qt, ht, comps, size = {}, {}, [], None
    i = 0
    while True:
        assert d[i] == 0xFF
        while d[i + 1] == 0xFF:
            i += 1
        m = d[i + 1]
        if m == 0xD8:
            i += 2
            continue
        ln = struct.unpack(">H", d[i + 2:i + 4])[0]
        seg = d[i + 4:i + 2 + ln]
        i += 2 + ln
        if m == 0xDB:
            while seg:
                pq, vals = seg[0], seg[1:65]
                assert pq >> 4 == 0
                nat = [0] * 64
                for k in range(64):
                    nat[ZIGZAG[k]] = float(vals[k])
                q = np.zeros((8, 8), f32)
                for r in range(8):
                    for c in range(8):
                        q[r, c] = f32(nat[r * 8 + c] * AAN[r] * AAN[c])  # (f32)((f64)q * aan[row] * aan[col])
                qt[pq & 15] = q
                seg = seg[65:]
        elif m == 0xC0:
            size = (struct.unpack(">H", seg[3:5])[0], struct.unpack(">H", seg[1:3])[0])  # (w, h)
            assert seg[5] == 3
            comps = [dict(samp=seg[7 + 3 * k], qt=seg[8 + 3 * k]) for k in range(3)]
            assert [c["samp"] for c in comps] == [0x22, 0x11, 0x11]
        elif m == 0xC4:
            while seg:
                tc, th = seg[0] >> 4, seg[0] & 15
                counts = list(seg[1:17])
                n = sum(counts)
                syms = list(seg[17:17 + n])
                code, k, table = 0, 0, {}
                for ln_ in range(1, 17):
                    for _ in range(counts[ln_ - 1]):
                        table[(ln_, code)] = syms[k]
                        code += 1
                        k += 1
                    code <<= 1
                ht[(tc, th)] = table
                seg = seg[17 + n:]
        elif m == 0xDA:
            assert seg[0] == 3
            for k in range(3):
                comps[k]["dc"] = seg[2 + 2 * k] >> 4
                comps[k]["ac"] = seg[2 + 2 * k] & 15
            return qt, ht, comps, size, d[i:]
        elif m == 0xDD:
            raise AssertionError("restart interval: not used by the game's movies")


def entropy_decode(scan, ht, comps, size):
    w, h = size
    bits = np.unpackbits(np.frombuffer(scan, np.uint8)).tolist()
    pos = 0
    pred = [0, 0, 0]
    mw, mh = w // 16, h // 16
    blocks = [np.zeros((mw * mh * (4 if c == 0 else 1), 8, 8), np.int16) for c in range(3)]
    idx = [0, 0, 0]

    def sym(table):
        nonlocal pos
        code = 0
        for ln in range(1, 17):
            code = (code << 1) | bits[pos]
            pos += 1
            s = table.get((ln, code))
            if s is not None:
                return s
        raise ValueError("bad Huffman code")

    def recv(n):
        nonlocal pos
        v = 0
        for _ in range(n):
            v = (v << 1) | bits[pos]
            pos += 1
        return v

    def ext(v, n):
        return v if v >= (1 << (n - 1)) else v - (1 << n) + 1

    def block(c):
        nonlocal pos
        b = blocks[c][idx[c]].reshape(64)
        idx[c] += 1
        t = sym(ht[(0, comps[c]["dc"])])
        diff = ext(recv(t), t) if t else 0
        pred[c] = np.int16(pred[c] + diff)
        b[0] = pred[c]
        k = 1
        while k < 64:
            rs = sym(ht[(1, comps[c]["ac"])])
            r, s = rs >> 4, rs & 15
            if s == 0:
                if r != 15:
                    break
                k += 16
                continue
            k += r
            b[ZIGZAG[k]] = ext(recv(s), s)
            k += 1

    for _ in range(mw * mh):
        for _ in range(4):
            block(0)
        block(1)
        block(2)
    return blocks


# ----------------------------------------------------------------------------------------------------------------------
# float32 paired-single model of the console inverse DCT
# ----------------------------------------------------------------------------------------------------------------------
def fma32(a, b, c):
    """a*b+c with one float32 rounding (a, b, c float32 arrays)."""
    a64, b64, c64 = (np.asarray(x, np.float32).astype(np.float64) for x in (a, b, c))
    p = a64 * b64  # exact: 24 x 24 bits
    s = p + c64
    bb = s - p
    e = (p - (s - bb)) + (c64 - bb)  # exact error of the double sum
    f = s.astype(np.float32)
    below = f.astype(np.float64) < s
    lo = np.where(below, f, np.nextafter(f, f32(-np.inf)))
    hi = np.nextafter(lo, f32(np.inf))
    mid = (lo.astype(np.float64) + hi.astype(np.float64)) * 0.5
    tie = (s == mid) & (e != 0)
    return np.where(tie, np.where(e > 0, hi, lo), f).astype(np.float32)


class PS:
    __slots__ = ("a", "b")

    def __init__(self, a, b):
        self.a, self.b = a, b


def K(v):
    return PS(f32(v), f32(v))


def add(x, y): return PS(x.a + y.a, x.b + y.b)
def sub(x, y): return PS(x.a - y.a, x.b - y.b)
def mul(x, y): return PS(x.a * y.a, x.b * y.b)
def madd(x, c, b): return PS(fma32(x.a, c.a, b.a), fma32(x.b, c.b, b.b))          # x*c + b
def msub(x, c, b): return PS(fma32(x.a, c.a, -b.a), fma32(x.b, c.b, -b.b))        # x*c - b
def nmsub(x, c, b): return PS(fma32(-x.a, c.a, b.a), fma32(-x.b, c.b, b.b))       # -(x*c - b)
def m00(x, y): return PS(x.a, y.a)
def m01(x, y): return PS(x.a, y.b)
def m10(x, y): return PS(x.b, y.a)
def m11(x, y): return PS(x.b, y.b)


CC4, CC2, CC2C6S, CC2C6A, BIAS = K(1.414213562), K(1.847759065), K(1.082392200), K(-2.613125930), K(1024.0)


def row_regular(t10, c, q):
    t9 = mul(PS(c[2], c[3]), PS(q[2], q[3]))
    t2, t6 = PS(c[4], c[5]), PS(q[4], q[5])
    t0 = m01(t10, t9)
    t3 = PS(c[6], c[7])
    t1 = m01(t9, t10)
    t7 = PS(q[6], q[7])
    t4 = madd(t2, t6, t0)
    t5 = nmsub(t2, t6, t0)
    t6 = madd(t3, t7, t1)
    t7 = nmsub(t3, t7, t1)
    t0 = add(t4, t6)
    t3 = sub(t4, t6)
    t2 = msub(t7, CC4, t6)
    t8 = sub(t7, t5)
    t1 = add(t5, t2)
    t2 = sub(t5, t2)
    t8 = mul(t8, CC2)
    t1 = m00(t0, t1)
    t6 = nmsub(t5, CC2C6A, t8)
    t4 = msub(t7, CC2C6S, t8)
    t6 = sub(t6, t0)
    t2 = m00(t2, t3)
    t5 = madd(t3, CC4, t6)
    t7 = m11(t0, t6)
    t4 = sub(t4, t5)
    t3 = add(t1, t7)
    t4 = m11(t5, t4)
    t0 = sub(t1, t7)
    t5 = add(t2, t4)
    t6 = sub(t2, t4)
    t5 = m10(t5, t5)
    t0 = m10(t0, t0)
    return [t3, t6, t5, t0]


def row_half(t10, c, q):
    t1 = mul(PS(c[2], c[3]), PS(q[2], q[3]))
    t3 = sub(t10, t1)
    t2 = add(t10, t1)
    t4 = madd(t1, CC4, t3)
    t5 = nmsub(t1, CC4, t2)
    t8 = mul(t3, CC2)
    t4 = m00(t2, t4)
    t6 = nmsub(t1, CC2C6A, t8)
    t5 = m00(t5, t3)
    t6 = sub(t6, t2)
    t7 = nmsub(t10, CC2C6S, t8)
    t2 = m11(t2, t6)
    t8 = msub(t3, CC4, t6)
    t9 = add(t4, t2)
    t7 = sub(t7, t8)
    t3 = m11(t8, t7)
    t4 = sub(t4, t2)
    t0 = add(t5, t3)
    t1 = sub(t5, t3)
    t1 = m10(t1, t1)
    t4 = m10(t4, t4)
    return [t9, t0, t1, t4]


def row_quarter(t10, c, q, fix=False):
    t2 = msub(t10, CC2, t10)
    t9 = m00(t10, t10)
    t1 = sub(CC2, CC2C6S)
    t3 = msub(t10, CC4, t2)
    t5 = m11(t10, t2)
    t4 = nmsub(t10, t1, t3)
    t7 = add(t9, t5)
    t6 = m11(t3, t4)
    t5 = sub(t9, t5)
    t8 = add(t9, t6)
    t6 = sub(t9, t6)
    t6 = m10(t6, t6)
    t5 = m10(t5, t5)
    if fix:  # undo the SDK's swap of output columns 3 and 4 (what a correct inverse DCT stores)
        return [t7, PS(t8.a, t6.a), PS(t8.b, t6.b), t5]
    return [t7, t8, t6, t5]


def row_dc(t10, c, q):
    t0 = m00(t10, t10)
    return [t0, t0, t0, t0]


def idct_blocks(coef, Q, fix=False):
    """coef (nb,8,8) int16, Q (8,8) float32 -> (nb,8,8) uint8 pixels"""
    nb = coef.shape[0]
    C = coef.astype(np.float32)
    c = [C[:, :, k] for k in range(8)]
    q = [Q[:, k][None, :] for k in range(8)]
    t10 = mul(PS(c[0], c[1]), PS(q[0], q[1]))
    reg = (coef[:, :, 4:] != 0).any(axis=2)
    half = ~reg & (coef[:, :, 2:4] != 0).any(axis=2)
    quar = ~reg & ~half & (coef[:, :, 1] != 0)
    paths = [(reg, row_regular(t10, c, q)), (half, row_half(t10, c, q)), (quar, row_quarter(t10, c, q, fix))]
    res = row_dc(t10, c, q)
    for mask, r in paths:
        res = [PS(np.where(mask, r[i].a, res[i].a), np.where(mask, r[i].b, res[i].b)) for i in range(4)]
    W = np.zeros((nb, 8, 8), np.float32)
    for i in range(4):
        W[:, :, 2 * i] = res[i].a
        W[:, :, 2 * i + 1] = res[i].b
    r = [PS(W[:, k, 0::2], W[:, k, 1::2]) for k in range(8)]  # row k, column pairs 0..3
    t6 = add(r[0], r[4])
    t8 = sub(r[0], r[4])
    t6 = add(t6, BIAS)
    t7 = add(r[2], r[6])
    t9 = sub(r[2], r[6])
    t0 = add(t6, t7)
    t8 = add(t8, BIAS)
    t4 = r[1]
    t9 = msub(t9, CC4, t7)
    t5 = r[3]
    t3 = sub(t6, t7)
    t1 = add(t8, t9)
    t6 = r[5]
    t2 = sub(t8, t9)
    t7 = r[7]
    t8 = add(t6, t5)
    t6 = sub(t6, t5)
    t9 = add(t4, t7)
    t4 = sub(t4, t7)
    t7 = add(t9, t8)
    t5 = sub(t9, t8)
    t8 = add(t6, t4)
    t9 = add(t0, t7)
    t8 = mul(t8, CC2)
    t23 = sub(t0, t7)
    t6 = madd(t6, CC2C6A, t8)
    t6 = sub(t6, t7)
    row0 = t9
    t4 = msub(t4, CC2C6S, t8)
    t9 = add(t1, t6)
    t5 = msub(t5, CC4, t6)
    t22 = sub(t1, t6)
    row1 = t9
    t8 = add(t2, t5)
    t4 = add(t4, t5)
    row2 = t8
    t9 = sub(t3, t4)
    t20 = add(t3, t4)
    row3 = t9
    t21 = sub(t2, t5)
    rows = [row0, row1, row2, row3, t20, t21, t22, t23]
    out = np.zeros((nb, 8, 8), np.uint8)
    for k, p in enumerate(rows):
        for lane, v in ((0, p.a), (1, p.b)):
            s = v * f32(0.125)
            out[:, k, lane::2] = np.where(s > 0, np.minimum(np.trunc(np.where(s > 0, s, 0)), 255), 0).astype(np.uint8)
    return out


def decode_frame(vid, fix=False):
    qt, ht, comps, (w, h), scan = parse_jpeg(vid)
    blocks = entropy_decode(scan, ht, comps, (w, h))
    mw, mh = w // 16, h // 16
    planes = []
    for ci in range(3):
        px = idct_blocks(blocks[ci], qt[comps[ci]["qt"]], fix)
        if ci == 0:  # blocks come in MCU order: (x0,y0) (x0+8,y0) (x0,y0+8) (x0+8,y0+8)
            px = px.reshape(mh, mw, 2, 2, 8, 8).transpose(0, 2, 4, 1, 3, 5).reshape(h, w)
        else:
            px = px.reshape(mh, mw, 8, 8).transpose(0, 2, 1, 3).reshape(h // 2, w // 2)
        planes.append(px)
    return planes


def untile(plane, w, h):
    return np.frombuffer(plane, np.uint8).reshape(h // 4, w // 8, 4, 8).transpose(0, 2, 1, 3).reshape(h, w)


# ----------------------------------------------------------------------------------------------------------------------
# audio
# ----------------------------------------------------------------------------------------------------------------------
def audio_decode(a, tie_up=False):
    """THPAudioDecode(flag 0) re-written with Python integers: returns interleaved s16 as the console buffer."""
    off_next, nsamp = struct.unpack(">II", a[:8])
    lc = struct.unpack(">16h", a[8:40])
    rc = struct.unpack(">16h", a[40:72])
    lyn, ryn = struct.unpack(">2h", a[72:76]), struct.unpack(">2h", a[76:80])

    def chan(data, coef, yn1, yn2):
        out = []
        nib = 2
        pos = 1
        pred, scale = (data[0] >> 4) & 7, data[0] & 15
        for _ in range(nsamp):
            if nib & 15 == 0:
                pred, scale = (data[pos] >> 4) & 7, data[pos] & 15
                pos += 1
                nib += 2
            if nib & 1:
                s = data[pos] & 15
                pos += 1
            else:
                s = data[pos] >> 4
            nib += 1
            s = s - 16 if s >= 8 else s
            acc = coef[2 * pred + 1] * yn2 + coef[2 * pred] * yn1 + ((s << scale) << 11)
            q, r = divmod(acc * 32, 65536)
            if r > 32768 or (r == 32768 and (tie_up or q & 1)):
                q += 1
            y = max(-32768, min(32767, q))
            out.append(y)
            yn2, yn1 = yn1, y
        return out

    body = a[80:]
    first = chan(body, lc, lyn[0], lyn[1])
    out = np.zeros(2 * nsamp, np.int16)
    if off_next == 0:
        second = first
    else:
        second = chan(body[off_next:], rc, ryn[0], ryn[1])
    out[1::2] = first   # decLeftPtr = audioBuffer + 1
    out[0::2] = second  # decRightPtr = audioBuffer
    return out


# ----------------------------------------------------------------------------------------------------------------------
def ffmpeg_yuv(path, n, extra=()):
    r = subprocess.run(["ffmpeg", "-v", "error", *extra, "-i", path, "-frames:v", str(n), "-f", "rawvideo", "-pix_fmt",
                        "yuvj420p", "-"], capture_output=True)
    if r.returncode:
        raise RuntimeError(r.stderr.decode())
    return np.frombuffer(r.stdout, np.uint8)


def ffmpeg_pcm(path, seconds):
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-t", f"{seconds:.3f}", "-map", "0:a:0", "-f", "s16le", "-ac", "2",
                        "-"],
                       capture_output=True)
    if r.returncode:
        raise RuntimeError(r.stderr.decode())
    return np.frombuffer(r.stdout, np.int16)


def tev_rgb(y, u, v):
    """GXYuv2RgbSetup's TEV (THPDraw.c): R = Y + 1.402 (V-128), B = Y + 1.772 (U-128), G = Y - 0.344 (U-128) - 0.714 (V-128)."""
    Y = y.astype(np.float32)
    U = np.repeat(np.repeat(u, 2, 0), 2, 1).astype(np.float32)
    V = np.repeat(np.repeat(v, 2, 0), 2, 1).astype(np.float32)
    r = 2 * (-90 + 179 / 255 * V) + Y
    g = (135 - 88 / 255 * U - 182 / 255 * V) + Y
    b = 2 * (-114 + 226 / 255 * U) + Y
    return np.clip(np.stack([r, g, b], -1), 0, 255).round().astype(np.uint8)


def main():
    path, n, d = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    start = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    thp = parse_thp(path, n, start)
    w, h, _ = thp["video"]
    ysz, csz = w * h, w * h // 4
    fsz = ysz + 2 * csz
    dec = np.fromfile(os.path.join(d, "dec_video.bin"), np.uint8)
    assert dec.size == n * fsz, (dec.size, n)
    ff = ffmpeg_yuv(path, start + n)
    ff2 = ffmpeg_yuv(path, start + n, ("-idct", "faani"))  # ffmpeg's float AAN IDCT

    def planes_of(buf, k):
        b = buf[(start + k) * fsz:(start + k + 1) * fsz]
        return [b[:ysz].reshape(h, w), b[ysz:ysz + csz].reshape(h // 2, w // 2), b[ysz + csz:].reshape(h // 2, w // 2)]

    ok = True
    worst_fix = [0, 0, 0]
    for k in range(n):
        got = [untile(dec[k * fsz:][:ysz], w, h), untile(dec[k * fsz + ysz:][:csz], w // 2, h // 2),
               untile(dec[k * fsz + ysz + csz:][:csz], w // 2, h // 2)]
        model = decode_frame(thp["frames"][k][0])
        fixed = decode_frame(thp["frames"][k][0], True)
        e_model = [int(np.abs(got[i].astype(int) - model[i]).max()) for i in range(3)]
        e_bug = [int(np.abs(got[i].astype(int) - planes_of(ff, k)[i]).max()) for i in range(3)]
        e_fix = [max(int(np.abs(fixed[i].astype(int) - planes_of(x, k)[i]).max()) for x in (ff, ff2)) for i in range(3)]
        diff_fix = [int((model[i] != fixed[i]).sum()) for i in range(3)]
        print(f"frame {k}: decoder vs float32/fma model of the assembly: max err Y/U/V {e_model}; "
              f"vs ffmpeg {e_bug}; model with the SDK's quarter-IDCT quirk undone vs ffmpeg (int and float IDCT) {e_fix} "
              f"({diff_fix} samples differ with and without the quirk)")
        ok = ok and not any(e_model)
        worst_fix = [max(a, b) for a, b in zip(worst_fix, e_fix)]
        if k == 0:
            from PIL import Image
            Image.fromarray(tev_rgb(*got)).save(os.path.join(d, "frame0.png"))
            Image.fromarray(tev_rgb(*planes_of(ff, 0))).save(os.path.join(d, "frame0_ffmpeg.png"))
    ok = ok and max(worst_fix) <= 1
    print("video: decoder == float32/fma model of the assembly (max err 0); the model is within 1 LSB of ffmpeg "
          "once the SDK's quarter-IDCT quirk is undone" if ok else
          "video: FAILED (decoder differs from the model, or the model is not within 1 LSB of ffmpeg)")

    if thp["audio"] and os.path.exists(os.path.join(d, "dec_audio.pcm")):
        got = np.fromfile(os.path.join(d, "dec_audio.pcm"), np.int16)
        ref = np.concatenate([audio_decode(fr[1][0]) for fr in thp["frames"]])
        assert got.size == ref.size, (got.size, ref.size)
        e_ref = int(np.abs(got.astype(int) - ref).max())
        print(f"audio: {got.size // 2} samples, decoder vs integer model (ties to even): max err {e_ref}")
        if e_ref:
            ok = False
        alt = np.concatenate([audio_decode(fr[1][0], True) for fr in thp["frames"]])
        print(f"audio: round-half-up of the ties (Aurora's decoder) would change {int((alt != ref).sum())} of {ref.size} samples, "
              f"max {int(np.abs(alt.astype(int) - ref).max())}")
        pcm = ffmpeg_pcm(path, (n + 1) / thp["rate"])[:got.size] if start == 0 else np.zeros(0, np.int16)
        if pcm.size == got.size:
            direct = np.abs(got.astype(int) - pcm).max()
            swapped = np.abs(got.reshape(-1, 2)[:, ::-1].reshape(-1).astype(int) - pcm).max()
            print(f"audio vs ffmpeg adpcm_thp: max err {int(direct)} as interleaved, {int(swapped)} with L/R swapped; "
                  f"differing samples {int((got != pcm).sum())} of {got.size}; peak |sample| {int(np.abs(got).max())}, "
                  f"rms {float(np.sqrt((got.astype(float) ** 2).mean())):.0f}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
