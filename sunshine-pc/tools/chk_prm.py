"""Offline proof for the .prm (TParams) on-disc layout read by TParams::load / TParamT<T>::load.

Layout (big-endian), exactly as the stream reads consume it:
  s32 count
  count x { u16 keycode; u16 nameLen; char name[nameLen]; s32 size; u8 payload[size] }
TParams::load matches (keycode, name) against the declared TParamT members; TParamT<T>::load reads `size` (discarded)
then sizeof(T) raw payload bytes (u8=1, s16=2, s32/f32=4, TVec3<f32>=12). A non-matching entry is skipped by `size`.
So for a matched entry size MUST equal sizeof(T), otherwise the stream would desynchronise.

The loading class of a file is found from the sources (`new Cls("x.prm")` or `Cls::Cls(...) : Base("x.prm")`), its
member types from the headers (base classes included). Files whose class cannot be found (names built at run time:
camera/shake tables) are checked by member name alone against every declaration.

Usage: python chk_prm.py [scene.szs ...]   (default: params.szs, common.szs and every data/scene/*.szs)
"""
import glob, os, re, struct, sys

sys.path.insert(0, os.path.dirname(__file__))
import discfmt

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..'))
SIZEOF = {'u8': 1, 's16': 2, 's32': 4, 'f32': 4, 'JGeometry::TVec3<f32>': 12}
MEMBER = re.compile(r'\bTParam(?:RT|T)\s*<\s*([\w:]+(?:\s*<\s*\w+\s*>)?)\s*>\s+(\w+)\s*;|\bTParamVec\s+(\w+)\s*;')


def key_code(name):  # JDrama::TNameRef::calcKeyCode
    r = 0
    for c in name.encode('ascii'):
        r = (r * 3 + c) & 0xFFFFFFFF
    return r & 0xFFFF


def read_tree(root, exts):
    for dp, _, fn in os.walk(os.path.join(REPO, root)):
        for f in fn:
            if f.endswith(exts):
                t = open(os.path.join(dp, f), encoding='utf-8', errors='replace').read()
                yield re.sub(r'//[^\n]*|/\*.*?\*/', '', t, flags=re.S)


def norm(p):  # TParams path -> archive path: case-insensitive, "/../map/x" is the scene archive root
    p = p.lower()
    return p[3:] if p.startswith('/../') else p


def class_tables():
    """classes {name: (base, {member: type})}, every declared (name -> types), path -> [class]."""
    classes, anyname, paths = {}, {}, {}
    head = re.compile(r'\b(?:class|struct)\s+(\w+)\s*:\s*public\s+(\w+)\s*\{')
    for t in read_tree('sms/include', ('.hpp', '.h')):
        for m in head.finditer(t):
            depth, i = 1, m.end()
            while depth and i < len(t):
                depth += (t[i] == '{') - (t[i] == '}')
                i += 1
            mem = {}
            for x in MEMBER.finditer(t[m.end():i]):
                n, ty = (x.group(2), re.sub(r'\s+', '', x.group(1))) if x.group(2) else (x.group(3), 'JGeometry::TVec3<f32>')
                mem[n] = ty
                anyname.setdefault(n, set()).add(ty)
            classes[m.group(1)] = (m.group(2), mem)
    rx_new = re.compile(r'\bnew\s+(\w+)\s*\(\s*"([^"]+\.prm)"', re.I)
    rx_ctor = re.compile(r'\b(\w+)::\1\s*\([^)]*\)\s*:\s*(\w+)\s*\(\s*"([^"]+\.prm)"', re.I)
    for t in read_tree('sms/src', ('.cpp',)):
        for m in rx_new.finditer(t):
            paths.setdefault(norm(m.group(2)), set()).add(m.group(1))
        for m in rx_ctor.finditer(t):
            paths.setdefault(norm(m.group(3)), set()).add(m.group(1))
    return classes, anyname, paths


def members_of(classes, cls):
    out = {}
    while cls in classes:
        base, mem = classes[cls]
        for k, v in mem.items():
            out.setdefault(k, v)
        cls = base
    return out


def parse(path, d):
    assert len(d) >= 4, path
    n = struct.unpack('>i', d[:4])[0]
    assert 0 <= n < 1000, (path, n)
    p, out = 4, []
    for _ in range(n):
        assert p + 4 <= len(d), (path, 'eof in entry header')
        kc, ln = struct.unpack('>HH', d[p:p + 4]); p += 4
        assert p + ln + 4 <= len(d), (path, 'eof in name')
        name = d[p:p + ln].decode('ascii'); p += ln
        assert name.isidentifier(), (path, name)
        sz = struct.unpack('>i', d[p:p + 4])[0]; p += 4
        assert 0 <= sz and p + sz <= len(d), (path, name, 'payload out of range')
        out.append((kc, name, sz, d[p:p + sz])); p += sz
    assert p == len(d), (path, 'trailing bytes', p, len(d))
    return out


def score(classes, cls, ents):
    """(matches, conflicts): entries the class would hand to TParamT<T>::load, and those whose size != sizeof(T)."""
    mem = members_of(classes, cls)
    m = c = 0
    for _, name, sz, _ in ents:
        if name in mem:
            m += 1
            c += sz != SIZEOF[mem[name]]
    return m, c


def main():
    classes, anyname, paths = class_tables()
    names = sys.argv[1:] or ['data/params.szs', 'data/common.szs'] + sorted(
        'data/scene/' + os.path.basename(s) for s in glob.glob(discfmt.ROOT + '/data/scene/*.szs'))
    srcs = {}
    for a in names:
        for k, v in discfmt.load_szs(a).items():
            if k.endswith('.prm'):
                key = norm(k)
                if key.startswith('/map/params/'):  # scene archive override of a params.szs file
                    key = key[len('/map/params'):]
                srcs[os.path.basename(a) + ':' + k] = (key, v)
    st = dict(files=0, entries=0, class_files=0, matched=0, unmapped_files=0, unreferenced_files=0)
    bad_floats, bad, by_type = 0, [], {}
    for path, (key, d) in srcs.items():
        st['files'] += 1
        ents = parse(path, d)
        for kc, name, sz, pl in ents:
            st['entries'] += 1
            assert kc == key_code(name), (path, name, kc, key_code(name))  # same key as PARAM_INIT computes
        mapped = paths.get(key)
        if mapped:  # loading class known from the source: every entry it declares must have the right size
            st['class_files'] += 1
            for c in mapped:
                m, conf = score(classes, c, ents)
                st['matched'] += m
                mem = members_of(classes, c)
                for _, name, _, _ in ents:
                    if name in mem:
                        by_type[mem[name]] = by_type.get(mem[name], 0) + 1
                if conf:
                    bad.append((path, c, conf))
        else:  # loading class built at run time: some declared class must read the file without desync
            st['unmapped_files'] += 1
            sc = [score(classes, c, ents) for c in classes]
            if not any(m for m, _ in sc):
                st['unreferenced_files'] += 1  # no decompiled class declares any of its entries: nothing reads it
            elif not any(m and not conf for m, conf in sc):
                bad.append((path, 'every class declaring its entries disagrees on a size'))
        for kc, name, sz, pl in ents:  # every declared f32 member: payload is a plausible BE float
            if anyname.get(name) == {'f32'} and sz == 4:
                v = struct.unpack('>f', pl)[0]
                bad_floats += not (v == v and abs(v) < 1e30)
    print(st)
    print('matched entries by C++ type (all checked size == sizeof):', by_type)
    assert not bad, bad[:5]
    assert bad_floats == 0, bad_floats
    print('OK')


if __name__ == '__main__':
    main()
