"""Offline proof for the TSpcBinary (SPCB .sb) host swap in sms/src/Strategic/spcinterp.cpp (spcSwapBinary) and the
big-endian immediate fetchers in sms/include/Strategic/spcinterp.hpp.

Layout derived from TSpcBinary / TSpcInterp:
  header 0x1C   magic "SPCB", u32 textOffset, dataOffset, dataNum, symbolOffset, symbolNum, s32 unk18
  text          [textOffset, dataOffset): opcode byte + immediates (u8 / u32 / s32 / f32, unaligned, big-endian)
  data offsets  dataNum x u32 at dataOffset (relative to the string pool that follows it)
  string pool   NUL-terminated strings up to symbolOffset
  symbols       symbolNum x TSpcSymbol{type, nameOffset, data, nameHash, nativeCall} (5 x u32) at symbolOffset
  names         NUL-terminated strings, nameOffset relative to symbolOffset + symbolNum * 0x14
Asserts every offset/size is consistent, the bytecode decodes to a clean end, the in-place swap of header, symbol
table and data-offset table (emulated with the C++ field list) matches the big-endian values, the text section is
untouched, and a second pass is a no-op.

Usage: python chk_spcb.py [scene.szs ...]   (default: every data/scene/*.szs plus data/common.szs)
"""
import glob, os, struct, sys
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import discfmt

# opcode -> (name, immediates); B = u8, I = u32, i = s32, f = f32 (order of the fetch calls in TSpcInterp::exec*)
OPS = {
    0: ('int', 'i'), 1: ('flt', 'f'), 2: ('str', 'I'), 3: ('adr', 'I'), 4: ('var', 'II'), 5: ('nop', ''),
    6: ('inc', 'BII'), 7: ('dec', 'BII'), 8: ('add', ''), 9: ('sub', ''), 10: ('mul', ''), 11: ('div', ''),
    12: ('mod', ''), 13: ('ass', 'BII'), 14: ('eq', ''), 15: ('ne', ''), 16: ('gt', ''), 17: ('lt', ''),
    18: ('ge', ''), 19: ('le', ''), 20: ('neg', ''), 21: ('not', ''), 22: ('and', ''), 23: ('or', ''),
    24: ('band', ''), 25: ('bor', ''), 26: ('shl', ''), 27: ('shr', ''), 28: ('call', 'Ii'), 29: ('func', 'II'),
    30: ('mkfr', 'i'), 31: ('mkds', 'i'), 32: ('ret', ''), 33: ('ret0', ''), 34: ('jne', 'I'), 35: ('jmp', 'I'),
    36: ('pop', ''), 37: ('int0', ''), 38: ('int1', ''), 39: ('end', ''),
}
SIZE = {'B': 1, 'I': 4, 'i': 4, 'f': 4}
FMT = {'I': '>I', 'i': '>i', 'f': '>f'}  # text immediates stay big-endian: fetchers use rdbe32
DISPLAY = 4  # TSpcInterp::mDisplay[4]


def host_swap(buf):
    """Mirror of spcSwapBinary on a little-endian host; buf is the raw disc image, modified in place."""
    if buf[0:4] != b'SPCB':
        return False
    for i in range(7):
        struct.pack_into('<I', buf, i * 4, struct.unpack_from('>I', buf, i * 4)[0])
    _, text, dat, dnum, sym, snum, _ = struct.unpack_from('<4s6I', buf, 0)
    for i in range(dnum):
        struct.pack_into('<I', buf, dat + i * 4, struct.unpack_from('>I', buf, dat + i * 4)[0])
    for i in range(snum * 5):
        struct.pack_into('<I', buf, sym + i * 4, struct.unpack_from('>I', buf, sym + i * 4)[0])
    return True


def cstr_end(b, off, limit, ctx):
    assert off < limit, ctx + ': string offset out of range'
    end = b.find(b'\0', off, limit)
    assert end >= 0, ctx + ': string does not terminate'
    return end


def decode(h, ctx):
    """Decode the text section of the host-swapped image h (tables little-endian, text immediates big-endian)."""
    _, text, dat, dnum, sym, snum, _ = struct.unpack_from('<4s6I', h, 0)
    pc, ins = text, []
    while pc < dat:
        if dat - pc < 4 and not any(h[pc:dat]):  # trailing zero padding that cannot hold an opcode + immediate
            break
        op = h[pc]
        assert op in OPS, '%s: unknown opcode %d at text+%d' % (ctx, op, pc - text)
        name, imm = OPS[op]
        start, vals = pc - text, []
        pc += 1
        for c in imm:
            assert pc + SIZE[c] <= dat, '%s: %s at text+%d overruns the text section' % (ctx, name, start)
            vals.append(h[pc] if c == 'B' else struct.unpack_from(FMT[c], h, pc)[0])
            pc += SIZE[c]
        ins.append((start, name, vals))
    assert ins and ins[-1][1] in ('ret', 'ret0', 'end'), ctx + ': text does not end in ret/end'
    return ins


def check(path):
    fs = discfmt.load_szs(path)
    stats = {'files': 0, 'ins': 0, 'syms': 0, 'strs': 0, 'unaligned': 0, 'ops': set()}
    for member in sorted(k for k in fs if k.endswith('.sb')):
        raw = fs[member]
        ctx = '%s:%s' % (os.path.basename(path), member)
        assert len(raw) >= 0x1C and raw[:4] == b'SPCB', ctx + ': header'
        mg, text, dat, dnum, sym, snum, u18 = struct.unpack_from('>4s6I', raw, 0)
        pool = dat + 4 * dnum
        names = sym + snum * 0x14
        assert 0x1C <= text <= dat <= pool <= sym <= names <= len(raw), ctx + ': section order'
        assert 0 <= u18 <= 0x20, ctx + ': unk18 exceeds the 0x20-slot storage stack'
        stats['unaligned'] += (dat % 4 != 0) or (sym % 4 != 0)

        offs = struct.unpack_from('>%dI' % dnum, raw, dat)
        for i, o in enumerate(offs):
            cstr_end(raw, pool + o, sym, '%s data[%d]' % (ctx, i))
        symbols = [struct.unpack_from('>5I', raw, sym + 20 * i) for i in range(snum)]

        h = bytearray(raw)
        assert host_swap(h) is True
        assert bytes(h[text:dat]) == raw[text:dat], ctx + ': text section modified'
        assert struct.unpack_from('<4s6I', h, 0)[1:] == (text, dat, dnum, sym, snum, u18), ctx + ': header swap'
        assert bytes(h[0:4]) == b'BCPS', ctx + ': magic marker'
        assert struct.unpack_from('<%dI' % dnum, h, dat) == offs, ctx + ': data offset swap'
        assert [struct.unpack_from('<5I', h, sym + 20 * i) for i in range(snum)] == symbols, ctx + ': symbol swap'
        assert bytes(h[0x1C:dat]) == raw[0x1C:dat] and bytes(h[dat + 4 * dnum:sym]) == raw[dat + 4 * dnum:sym]
        assert bytes(h[names:]) == raw[names:], ctx + ': pools modified'
        once = bytes(h)
        assert host_swap(h) is False and bytes(h) == once, ctx + ': second pass changed data'

        ins = decode(h, ctx)
        starts = {i[0] for i in ins}
        for off, name, v in ins:
            if name in ('jne', 'jmp', 'call'):
                assert v[0] in starts, '%s: %s target %d is not an instruction' % (ctx, name, v[0])
            elif name == 'str':
                assert v[0] < dnum, ctx + ': string index'
            elif name == 'func':
                assert v[0] < snum and symbols[v[0]][0] == 0, ctx + ': builtin symbol index'
            elif name in ('var', 'inc', 'dec', 'ass'):
                assert v[-2] < DISPLAY, ctx + ': display layer'
            elif name == 'mkds':
                assert 0 <= v[0] < DISPLAY, ctx + ': display index'
            stats['ops'].add(name)
        for i, (typ, noff, data, hsh, nat) in enumerate(symbols):
            assert typ in (0, 1, 2), '%s sym %d: type %d' % (ctx, i, typ)
            end = cstr_end(raw, names + noff, len(raw), '%s sym %d name' % (ctx, i))
            assert end > names + noff and all(32 <= c < 127 for c in raw[names + noff:end]), ctx + ': symbol name'
            assert hsh == 0 and nat == 0, ctx + ': nameHash/nativeCall are filled at runtime, zero on disc'
            if typ == 1:
                assert data in starts, '%s sym %d: function entry is not an instruction' % (ctx, i)
        stats['files'] += 1
        stats['ins'] += len(ins)
        stats['syms'] += snum
        stats['strs'] += dnum
    return stats


if __name__ == '__main__':
    archives = sys.argv[1:] or sorted(glob.glob(os.path.join(discfmt.ROOT, 'data', 'scene', '*.szs'))) + [
        os.path.join(discfmt.ROOT, 'data', 'common.szs')]
    tot = {'files': 0, 'ins': 0, 'syms': 0, 'strs': 0, 'unaligned': 0}
    ops = set()
    with ProcessPoolExecutor() as ex:
        for s in ex.map(check, archives):
            ops |= s.pop('ops')
            for k in tot:
                tot[k] += s[k]
    assert tot['files'] > 0 and len(ops) >= 30, 'too little coverage: %d scripts, %d opcodes' % (tot['files'], len(ops))
    print('chk_spcb OK: %d archives, %d scripts, %d instructions, %d symbols, %d strings, %d with unaligned tables, '
          '%d/%d opcodes seen' % (len(archives), tot['files'], tot['ins'], tot['syms'], tot['strs'],
                                  tot['unaligned'], len(ops), len(OPS)))
