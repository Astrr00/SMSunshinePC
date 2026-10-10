"""Independent reference for the host DSP's ADPCM decoder (tools/dsp_test.cpp).

Takes real 4-bit DSP-ADPCM (AFC, 9 bytes -> 16 samples) data from the disc's wave banks (AudioRes/Banks/*.aw in the
extracted disc tree), decodes it with a straight transcription of the format definition below (it shares no code and no
coefficient table with src/sdk/dsp_mixer.hpp), and writes a fixture for the C++ test.

Format definition (JAudio AFC / Nintendo DSP-ADPCM): frame = header byte + 8 data bytes. header low nibble = coefficient
pair index, high nibble = scale shift. The 16 samples are the high then low nibble of each data byte, each a signed 4-bit
value n. sample = clamp16((n << scale) + ((c1 * y1 + c2 * y2) >> 11)) with y1/y2 the previous two output samples.

Usage: python dsp_ref.py <fixture.bin>      (needs sms/orig/GMSJ01/extracted/files; falls back to synthetic frames only)
Fixture: "DSPT", u32 chunk count, then per chunk: u32 kind (1 = real disc data, 0 = synthetic), u32 frames, frames * 9 bytes,
frames * 16 little-endian s16 expected samples.
"""
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BANKS = os.path.join(HERE, "..", "..", "sms", "orig", "GMSJ01", "extracted", "files", "AudioRes", "Banks")

# The 16 coefficient pairs of the format, as (c1 for y[n-1], c2 for y[n-2]).
COEFS = [
    (0x0000, 0x0000), (0x0800, 0x0000), (0x0000, 0x0800), (0x0400, 0x0400),
    (0x1000, -0x0800), (0x0E00, -0x0600), (0x0C00, -0x0400), (0x1200, -0x0A00),
    (0x1068, -0x08C8), (0x12C0, -0x08FC), (0x1400, -0x0C00), (0x0800, -0x0800),
    (0x0400, -0x0400), (-0x0400, 0x0400), (-0x0400, 0x0000), (-0x0800, 0x0000),
]


def decode(data):
    out = []
    y1 = y2 = 0
    for f in range(0, len(data), 9):
        hdr = data[f]
        c1, c2 = COEFS[hdr & 15]
        scale = hdr >> 4
        for i in range(16):
            b = data[f + 1 + i // 2]
            n = (b >> 4) if i % 2 == 0 else (b & 15)
            if n >= 8:
                n -= 16
            v = (n << scale) + ((c1 * y1 + c2 * y2) >> 11)
            v = max(-32768, min(32767, v))
            out.append(v)
            y2, y1 = y1, v
    return out


def chunk(kind, data):
    frames = len(data) // 9
    data = data[: frames * 9]
    pcm = decode(data)
    return struct.pack("<II", kind, frames) + bytes(data) + struct.pack("<%dh" % len(pcm), *pcm)


def main():
    out = sys.argv[1]
    chunks = []
    real = [("w1stLoad_0.aw", 0, 1500), ("w1stLoad_0.aw", 9 * 3000, 1500), ("wScene_0.aw", 0, 2000),
            ("wScene_0.aw", 9 * 200000, 2000), ("wScene_9.aw", 9 * 5000, 2000), ("wScene_21.aw", 9 * 100, 2000)]
    for name, off, frames in real:
        path = os.path.join(BANKS, name)
        if not os.path.exists(path):
            continue
        with open(path, "rb") as fh:
            fh.seek(off)
            data = fh.read(frames * 9)
        if len(data) >= 9 * 16:
            chunks.append(chunk(1, data))
            print("real chunk: %s @0x%X, %d frames" % (name, off, len(data) // 9))
    if not chunks:
        print("WARNING: no wave bank found under %s; only synthetic frames are checked" % BANKS)
    rnd = random.Random(1234)
    # every coefficient index and scale, random nibbles: exercises the 16-bit saturation
    chunks.append(chunk(0, bytes(rnd.randrange(256) for _ in range(9 * 2000))))
    with open(out, "wb") as fh:
        fh.write(b"DSPT" + struct.pack("<I", len(chunks)) + b"".join(chunks))


main()
