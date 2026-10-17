#!/usr/bin/env python3
"""Which raw stages can a normal game enter? Collects every stage change the data and code can request, then lists the
stages of the 44 unresolved scenario resources (docs/content-unresolved.md) that none of them names.

Sources: (1) MapObjChangeStage / MapObjChangeStageHipDrop destinations in every scene.bin, (2) setNextStage(stage,
scenario) calls in every SPC script, (3) constants passed to TMarDirector::setNextStage in the source (HARDCODED below,
from `grep setNextStage sms/src`). NOT covered: ground-plane warps (collision type with data, MarioMove.cpp:2020, needs a
.col decoder), the debug stage menu (MenuDir.cpp) and TMarioAutodemo. Output: stdout table.
"""
import sys, re, csv, collections
sys.path.insert(0, 'tools')
import content_inventory as ci
import discfmt

# setNextStage(param) encoding: param >= 0x100 -> stage (param >> 8) - 1, scenario param & 0xff; else stage = param.
def decode(param):
    return ((param >> 8) - 1, param & 0xff) if param >= 0x100 else (param, 0xff)

HARDCODED = {  # file:line -> param (sms/src), 0xE05 etc. as passed
    'CameraJetCoaster.cpp:35': 0xE05, 'sunmgr.cpp:101': 9, 'CardLoad.cpp:2554': 0, 'CardLoad.cpp:2556': 1,
    'MarioAutodemo.cpp:195': None,  # 2, 3 or 4 (lines 182-192)
    'MarDirectorEvent.cpp:271': 0x1, 'MarDirectorEvent.cpp:281': 0x3B, 'MarDirectorEvent.cpp:289': 0xE06,
    'MarDirectorEvent.cpp:297': 0xE07, 'MarDirectorEvent.cpp:305': 0x3C, 'MarDirectorEvent.cpp:313': 0x101,
    'MarDirectorEvent.cpp:322': 0xF,
}


def main():
    sources = ci.Sources()
    dest = collections.defaultdict(list)  # stage -> [origin]
    for ref, p in HARDCODED.items():
        for q in ([2, 3, 4] if p is None else [p]):
            dest[decode(q)[0]].append('code ' + ref)
    unknown = []
    for path in sorted((ci.ROOT / 'data/scene').glob('*.szs')):
        files = discfmt.load_szs(str(path))
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
                    dest[decode(r.u32())[0]].append('%s %s' % (path.name, n['type']))
                except ci.DecodeError as e:
                    unknown.append('%s %s: %s' % (path.name, n['type'], e))
        for member in sorted(m for m in files if m.endswith('.sb')):
            try:
                calls = ci.spc_calls(files[member])
            except (ci.DecodeError, ci.struct.error, UnicodeError):
                continue
            for pc, _, name, vals in [(c[0], c[1], c[2], c[3]) for c in calls]:
                if name != 'setNextStage':
                    continue
                if len(vals) == 2 and all(isinstance(v, int) for v in vals):
                    dest[vals[0]].append('%s:%s@0x%x' % (path.name, member, pc))
                else:
                    unknown.append('%s:%s@0x%x dynamic %r' % (path.name, member, pc, vals))
    open_stages = collections.defaultdict(list)
    for x in csv.DictReader(open('docs/content-coverage.tsv', encoding='utf-8'), delimiter='\t'):
        if x['kind'] == 'scenario' and x['metadata_status'] == 'unresolved-resource':
            open_stages[int(x['raw_stage'])].append(x['archive'])
    print('stages named by a stage change: %s' % sorted(dest))
    print('undecoded/dynamic: %d' % len(unknown))
    for u in unknown[:10]:
        print('  ' + u)
    for s in sorted(open_stages):
        print('stage %2d %-24s %s' % (s, ','.join(open_stages[s])[:24], 'NAMED by ' + '; '.join(dest[s][:3]) if s in dest else 'not named'))


main()
