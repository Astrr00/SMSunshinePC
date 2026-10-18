#!/usr/bin/env python3
"""Which raw stages can a normal game enter? Collects every stage change the data and code can request, then lists the
stages of the 44 unresolved scenario resources (docs/content-unresolved.md) that none of them names.

Sources: (1) MapObjChangeStage / MapObjChangeStageHipDrop destinations in every scene.bin, (2) setNextStage(stage,
scenario) calls in every SPC script, (3) constants passed to TMarDirector::setNextStage in the source (HARDCODED below,
from `rg setNextStage sms/src`), (4) ground-plane warps and TMarioAutodemo. The normal Delfino selector is decoded
from its literal returns; the debug stage menu is excluded from normal reachability. Output: stdout table.
"""
import sys, re, csv, collections
sys.path.insert(0, 'tools')
import content_inventory as ci
import discfmt

# setNextStage(param) encoding: param >= 0x100 -> stage (param >> 8) - 1, scenario param & 0xff; else stage = param.
def decode(param):
    return ((param >> 8) - 1, param & 0xff) if param >= 0x100 else (param, 0xff)


def delfino_scenarios(source):
    body = source.split('static int decideNextScenario(', 1)[1].split('int TMarDirector::changeState()', 1)[0]
    values = re.findall(r'\breturn\s+([^;]+);', body)
    if not values or any(not re.fullmatch(r'\d+', value.strip()) for value in values):
        raise ci.DecodeError('Delfino selector has a nonliteral return; audit it before classifying resources')
    return set(map(int, values))


def pinna_scenarios(source):
    body = source.split('void TMarDirector::moveStage()', 1)[1].split('case 13: {', 1)[1].split('case 0x3A:', 1)[0]
    values = re.findall(r'\bthing\s*=\s*([^;]+);', body)
    if not values or any(not re.fullmatch(r'\d+', value.strip()) for value in values):
        raise ci.DecodeError('Pinna selector changed; audit it before classifying resources')
    return set(map(int, values))


def scenario_reachable(stage, scenario, requests, selectors):
    if (stage, scenario) in requests:
        return True
    if (stage, 0xff) not in requests:
        return False
    return scenario in selectors[stage] if stage in selectors else None

HARDCODED = {  # file:line -> param (sms/src), 0xE05 etc. as passed
    'CameraJetCoaster.cpp:35': 0xE05, 'sunmgr.cpp:101': 9, 'CardLoad.cpp:2554': 0, 'CardLoad.cpp:2556': 1,
    'MarioAutodemo.cpp:195': None,  # 2, 3 or 4 (lines 182-192)
    'MarDirectorEvent.cpp:271': 0x1, 'MarDirectorEvent.cpp:281': 0x3B, 'MarDirectorEvent.cpp:289': 0xE06,
    'MarDirectorEvent.cpp:297': 0xE07, 'MarDirectorEvent.cpp:305': 0x3C, 'MarDirectorEvent.cpp:313': 0x101,
    'MarDirectorEvent.cpp:322': 0xF,
}


def col_warps(data):
    # .col (big-endian): {vtxNum, vtxOfs, grpNum, grpOfs}; group 0x18 B {u16 bgType, s16 triNum, u16 flags, u16, u32 idxOfs,
    # u32, u32, u32 addOfs}; additional = s16[triNum].
    grpNum, grpOfs = ci.struct.unpack_from('>II', data, 8)
    for g in range(grpNum):
        bg, tri, flags, _, _, _, _, add = ci.struct.unpack_from('>HhHHIIII', data, grpOfs + g * 0x18)
        if bg != 0x300:
            continue
        yield from (set(ci.struct.unpack_from('>%dh' % tri, data, add)) if flags & 1 else {0})


def main():
    sources = ci.Sources()
    dest = collections.defaultdict(list)  # (stage, scenario) -> [origin]
    for ref, p in HARDCODED.items():
        for q in ([2, 3, 4] if p is None else [p]):
            dest[decode(q)].append('code ' + ref)
    unknown = []
    # These are both script roots loaded by TMarDirector::setupObjects.
    archives = [ci.ROOT / 'data/common.szs', *sorted((ci.ROOT / 'data/scene').glob('*.szs'))]
    for path in archives:
        files = discfmt.load_szs(str(path))
        for member in sorted(m for m in files if m.endswith('.col')):
            try:
                for ws in col_warps(files[member]):
                    for w in ws:
                        dest[decode(w & 0xffff)].append('%s:%s ground warp 0x%x' % (path.name, member, w & 0xffff))
            except ci.struct.error as e:
                unknown.append('%s:%s col: %s' % (path.name, member, e))
        if '/map/scene.bin' in files:
            data = files['/map/scene.bin']
            for n in ci.flatten(ci.parse_node(data)):
                if n['type'] not in ('MapObjChangeStage', 'MapObjChangeStageHipDrop'):
                    continue
                try:
                    r = ci.actor_reader(data, n)
                    model = r.string()
                    if sources.map_flags.get(model, 0) & 0x80:
                        r.take(4)
                    dest[decode(r.u32())].append('%s %s' % (path.name, n['type']))
                except ci.DecodeError as e:
                    unknown.append('%s %s: %s' % (path.name, n['type'], e))
        for member in sorted(m for m in files if m.endswith('.sb')):
            try:
                calls = ci.spc_calls(files[member], {'setNextStage'})
            except (ci.DecodeError, ci.struct.error, UnicodeError) as e:
                unknown.append('%s:%s script: %s' % (path.name, member, e))
                continue
            for pc, _, name, vals in [(c[0], c[1], c[2], c[3]) for c in calls]:
                if name != 'setNextStage':
                    continue
                if len(vals) == 2 and all(isinstance(v, int) for v in vals):
                    dest[(vals[0], vals[1] & 0xff)].append('%s:%s@0x%x' % (path.name, member, pc))
                else:
                    unknown.append('%s:%s@0x%x dynamic %r' % (path.name, member, pc, vals))
    unresolved = []
    for x in csv.DictReader(open('docs/content-coverage.tsv', encoding='utf-8'), delimiter='\t'):
        if x['kind'] == 'scenario' and x['metadata_status'] == 'unresolved-resource':
            unresolved.append(x)
    source = (ci.REPO / 'sms/src/System/MarDirectorDirect.cpp').read_text(encoding='utf-8')
    selectors = {1: delfino_scenarios(source), 13: pinna_scenarios(source)}
    print('stages named by a stage change: %s' % sorted({s for s, _ in dest}))
    print('Delfino selector returns: %s; Pinna selector values: %s' % (sorted(selectors[1]), sorted(selectors[13])))
    print('undecoded/dynamic: %d' % len(unknown))
    for u in unknown[:10]:
        print('  ' + u)
    counts = collections.Counter()
    for row in unresolved:
        stage, scenario = int(row['raw_stage']), int(row['scenario_index'])
        reachable = scenario_reachable(stage, scenario, dest, selectors)
        status = 'named' if reachable else 'unreferenced' if reachable is False else 'undetermined'
        counts[status] += 1
        print('stage %2d scenario %2d %-18s %s' % (stage, scenario, row['archive'], status))
    print('resource references: %s' % dict(counts))
    return int(bool(unknown) or counts['named'] > 0 or counts['undetermined'] > 0)


if __name__ == '__main__':
    sys.exit(main())
