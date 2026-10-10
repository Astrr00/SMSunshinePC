"""Offline check of the .pad input replay layout read by TMarioInputReplay::init (Player/MarioRecord.cpp).
Disc layout (big-endian): 16-byte magic "MARIO RECORDv0.2", s32 replayLength @0x10, then five pairs of s32 offsets
(relative to the file start) @0x14..0x3C: {durations, values} for mag (f32), yaw (s16), btnMask (u16), unk64 (u8),
unk80 (u8). A channel has n = (valuesOfs - durationsOfs) / 4 entries: u32 duration[n] at durationsOfs, then value[n] of
the channel's element size (padded to the next array). The durations of each channel sum to replayLength (one shipped file, tutorialak, has a short last channel).
Replays the swap done by swapReplayData() in the C++ and verifies it (every field swapped exactly once).
Usage: python chk_pad.py"""
import struct
from discfmt import load_szs

SIZES = (4, 2, 2, 1, 1)
FMT = ('f', 'h', 'H', 'B', 'B')


def check(name, d):
    assert d[:16] == b'MARIO RECORDv0.2', name
    length = struct.unpack('>i', d[16:20])[0]
    ofs = struct.unpack('>10i', d[20:60])
    spans = []
    for c in range(5):
        do, vo = ofs[c * 2], ofs[c * 2 + 1]
        end = ofs[c * 2 + 2] if c < 4 else len(d)
        n = (vo - do) // 4
        assert n > 0 and do + n * 4 == vo and vo + n * SIZES[c] <= end <= len(d), (name, c, do, vo, end)
        durs = struct.unpack('>%dI' % n, d[do:do + n * 4])
        assert sum(durs) <= length, (name, c, sum(durs), length)  # == for all but tutorialak's unk80 channel (335 of 1155)
        spans += [(do, n * 4), (vo, n * SIZES[c])]
    # replay of the C++ swap on a copy
    b = bytearray(d)
    touched = bytearray(len(d))

    def sw(o, n):
        assert not any(touched[o:o + n]), (name, 'byte swapped twice', o)
        for k in range(o, o + n):
            touched[k] = 1
        b[o:o + n] = b[o:o + n][::-1]

    sw(16, 4)
    for i in range(10):
        sw(20 + i * 4, 4)
    o2 = struct.unpack('<10i', b[20:60])
    for c in range(5):
        do, vo = o2[c * 2], o2[c * 2 + 1]
        n = (vo - do) // 4
        for i in range(n):
            sw(do + i * 4, 4)
            if SIZES[c] > 1:
                sw(vo + i * SIZES[c], SIZES[c])
        assert struct.unpack('<%dI' % n, b[do:do + n * 4]) == struct.unpack('>%dI' % n, d[do:do + n * 4])
        assert struct.unpack('<%d%s' % (n, FMT[c]), b[vo:vo + n * SIZES[c]]) == struct.unpack('>%d%s' % (n, FMT[c]) if SIZES[c] > 1 else '<%dB' % n, d[vo:vo + n * SIZES[c]]), (name, c)
    return length


n = 0
for s in ('ricco6', 'rico_ex0'):
    for k, v in load_szs('data/scene/%s.szs' % s).items():
        if k.endswith('.pad'):
            check(s + k, v)
            n += 1
print('checked', n, '.pad files OK')
