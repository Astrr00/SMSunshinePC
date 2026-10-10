"""Offline check of ymap.ymp (TPollutionManager::setDataAddress / initPollutionInfo, PollutionManager.cpp).
Disc layout (big-endian): u16 layerCount @0, u16 pad, u32 layerInfosOfs @4 (relative to the file start);
layerCount records of 0x2C bytes {u16 pollutionType, u16 flags, u16 planeType, u16 pad, f32 verticalOffset,
f32 texelSize, f32 left, top, right, bottom, u16 log2Width, u16 log2Height, u32 unk24, u32 heightMapOfs};
each height map is (1<<log2Width)*(1<<log2Height) bytes at heightMapOfs (relative to the file start).
Usage: python chk_ymp.py [scene.szs ...]"""
import struct, sys, math
from discfmt import load_szs

scenes = sys.argv[1:] or ['data/scene/%s.szs' % s for s in ('bianco0', 'delfino0', 'mare0', 'airport0', 'monte0', 'dolpic0', 'mamma0', 'sirena0')]
n = layers = 0
for s in scenes:
    try:
        files = load_szs(s)
    except FileNotFoundError:
        continue
    d = files.get('/map/ymap.ymp')
    if d is None:
        continue
    cnt, pad, ofs = struct.unpack('>HHI', d[:8])
    assert 0 <= cnt <= 20 and ofs + cnt * 0x2C <= len(d), (s, cnt, ofs, len(d))
    spans = []
    for i in range(cnt):
        r = d[ofs + i * 0x2C:ofs + i * 0x2C + 0x2C]
        t, fl, pt, p2, vo, ts, l, tp, rt, bt, lw, lh, u24, hm = struct.unpack('>HHHHffffffHHII', r)
        assert all(math.isfinite(x) for x in (vo, ts, l, tp, rt, bt)) and ts > 0, (s, i)
        assert 0 < lw <= 12 and 0 < lh <= 12, (s, i, lw, lh)
        size = (1 << lw) * (1 << lh)
        assert hm + size <= len(d), (s, i, hm, size, len(d))
        spans.append((hm, hm + size))
        layers += 1
    spans.sort()
    assert all(a[1] <= b[0] for a, b in zip(spans, spans[1:])), (s, 'overlapping height maps')
    n += 1
print('checked', n, 'ymap.ymp files,', layers, 'layers (0x2C records, height maps inside file, no overlap)')
