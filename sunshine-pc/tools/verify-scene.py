"""Runs scene_test.exe (native decomp loaders on the host) on real scene archives and compares every output line with an
independent Python parse of the big-endian disc bytes. Build the test first: tools\\scene_test.cmd <build name>.
Usage: python tools/verify-scene.py [build name (unused, kept for symmetry)] [scene ...]"""
import os, struct, subprocess, sys, tempfile
from discfmt import load_szs, ROOT

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
exe = os.path.join(tempfile.gettempdir(), 'sunshine_scene_test', 'scene_test.exe')
scenes = [a for a in sys.argv[2:]] or ['bianco0', 'delfino0', 'mare0', 'monte0', 'dolpic0', 'airport0', 'mamma0']


def f32(b):
    return struct.unpack('>f', b)[0]


def exp_col(path, d):
    vn, vo, gn, go = struct.unpack('>4I', d[:16])
    vsum = 0.0
    for i in range(vn * 3):
        vsum += f32(d[vo + i * 4:vo + i * 4 + 4])
    tris = idxsum = addsum = gsum = csum = 0
    for g in range(gn):
        bg, tn, fl, _, io, co, ao, ado = struct.unpack('>HhHHIIII', d[go + g * 0x18:go + g * 0x18 + 0x18])
        gsum += bg * 1000 + fl
        tris += tn
        idxsum += sum(struct.unpack('>%dh' % (tn * 3), d[io:io + tn * 6]))
        if ado:
            addsum += sum(struct.unpack('>%dh' % tn, d[ado:ado + tn * 2]))
        for i in range(tn):
            csum += d[co + i] * 7 + d[ao + i]
    line = '%s %u %u %d %.3f %d %d %d %d' % (path, vn, gn, tris, vsum, idxsum, addsum, gsum, csum)
    return ['COL0 ' + line, 'COL1 ' + line]


def exp_ymp(d):
    n = struct.unpack('>H', d[:2])[0]
    ofs = struct.unpack('>I', d[4:8])[0]
    out = []
    for p in range(2):
        out.append('YMP%d %d' % (p, n))
        for i in range(n):
            t, fl, pt, _, vo, ts, l, tp, rt, bt, lw, lh, u24, hm = struct.unpack('>HHHHffffffHHII', d[ofs + i * 0x2C:ofs + i * 0x2C + 0x2C])
            out.append('YMPL %d %u %u %u %.3f %.3f %.3f %.3f %.3f %.3f %u %u %u %d' % (i, t, fl, pt, vo, ts, l, tp, rt, bt, lw, lh, u24, hm))
    return out


def cstr(d, o):
    return d[o:d.index(b'\0', o)].decode('latin1')


def exp_ral(d):
    descs = []
    while True:
        nn, no, ro = struct.unpack('>III', d[len(descs) * 12:len(descs) * 12 + 12])
        if nn == 0:
            break
        descs.append((nn, no, ro))
    out = []
    for p in range(2):
        out.append('RAL%d %d' % (p, len(descs)))
        for i, (nn, no, ro) in enumerate(descs):
            out.append('RALG %d %s %d' % (i, cstr(d, no), nn))
            for j in range(nn):
                r = d[ro + j * 0x44:ro + j * 0x44 + 0x44]
                x, y, z, cn, fl, pi, ya, ro_, sp = struct.unpack('>3hhIHHHH', r[:0x14])
                conns = struct.unpack('>8H', r[0x14:0x24])
                per = struct.unpack('>8f', r[0x24:0x44])
                line = 'RALN %d %d %d %d %d %u %u %u %u %u' % (i, j, x, y, z, fl, pi, ya, ro_, sp)
                if not (fl & 0x208):
                    line += ' %d' % cn + ''.join(' %u:%.4f' % (conns[k], per[k]) for k in range(cn))
                out.append(line)
    return out


def exp_sb(path, d):
    magic, text, dof, dn, so, sn, _ = struct.unpack('>4s6I', d[:0x1C])
    assert magic == b'SPCB'
    acc = 0
    nameBase = so + sn * 0x14
    for i in range(sn):
        t, no, dat, nh, nc = struct.unpack('>5I', d[so + i * 0x14:so + i * 0x14 + 0x14])
        for c in cstr(d, nameBase + no).encode('latin1'):
            acc = (acc * 131 + c) & M64
        acc = (((acc * 131 + ((t * 3) & M32)) & M64) + ((dat * 5) & M32) + nh) & M64
    dacc = 0
    for i in range(dn):
        dacc = (dacc * 131 + struct.unpack('>I', d[dof + i * 4:dof + i * 4 + 4])[0]) & M64
    h = '%u %u %u %u %u' % (text, dof, dn, so, sn)
    return ['SB0 %s %s' % (path, h), 'SBS %s %d %d' % (path, acc, dacc), 'SB1 %s %s' % (path, h), 'SBS %s %d %d' % (path, acc, dacc)]


def exp_prm(path, d):
    length = struct.unpack('>i', d[:4])[0]
    q = 4
    ents = []
    for _ in range(length):
        key, nl = struct.unpack('>HH', d[q:q + 4])
        name = d[q + 4:q + 4 + nl]
        sz = struct.unpack('>i', d[q + 4 + nl:q + 8 + nl])[0]
        if sz not in (1, 2, 4, 12):
            return ['PRMBAD %s %d' % (path, sz)]
        raw = d[q + 8 + nl:q + 8 + nl + sz]
        if sz == 1:
            bits = [raw[0], 0, 0]
        elif sz == 2:
            bits = [struct.unpack('>H', raw)[0], 0, 0]
        elif sz == 4:
            bits = [struct.unpack('>I', raw)[0], 0, 0]
        else:
            bits = list(struct.unpack('>3I', raw))
        ents.append((key, name, bits))
        q += 8 + nl + sz
    vals = [[0, 0, 0] for _ in ents]  # first matching param (by key+name) receives each entry, like TParams::load
    for key, name, bits in ents:
        for j, (k2, n2, _) in enumerate(ents):
            if k2 == key and n2 == name:
                vals[j] = bits
                break
    acc = 1469598103934665603
    for b in vals:
        for k in range(3):
            acc = ((acc ^ b[k]) * 1099511628211) & M64
    line = '%s %d %d' % (path, length, acc)
    return ['PRM0 ' + line, 'PRM1 ' + line]


bad = 0
for s in scenes:
    path = os.path.join(ROOT, 'data', 'scene', s + '.szs')
    files = load_szs(path)
    expect = []
    for k, d in files.items():
        if k.endswith('.col'):
            expect += exp_col(k, d)
        elif k == '/map/ymap.ymp':
            expect += exp_ymp(d)
        elif k == '/map/scene.ral':
            expect += exp_ral(d)
        elif k.endswith('.sb'):
            expect += exp_sb(k, d)
        elif k.endswith('.prm'):
            expect += exp_prm(k, d)
    r = subprocess.run([exe, path], capture_output=True, text=True, errors='replace')
    got = [l for l in r.stdout.splitlines() if l]
    kinds = {}
    for l in got:
        kinds[l.split()[0].rstrip('01')] = kinds.get(l.split()[0].rstrip('01'), 0) + 1
    if r.returncode != 0 or got != expect:
        bad += 1
        print('FAIL', s, 'rc=%s' % hex(r.returncode & 0xFFFFFFFF), 'lines got/expected %d/%d' % (len(got), len(expect)))
        for i, (a, b) in enumerate(zip(got, expect)):
            if a != b:
                print('  first diff @%d\n   got      %s\n   expected %s' % (i, a[:200], b[:200]))
                break
        print(r.stderr[-1500:])
    else:
        print('OK  %-10s %5d lines %s' % (s, len(got), kinds))

sys.exit(1 if bad else 0)
