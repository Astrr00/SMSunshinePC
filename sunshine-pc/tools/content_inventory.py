#!/usr/bin/env python3
"""Metadata-only GMSJ01 inventory; never a gameplay/completion certificate.

Run: python sunshine-pc/tools/content_inventory.py
Check parser boundaries without disc data: add --self-check.
Output: sunshine-pc/docs/content-coverage.tsv (technical metadata only).
Scenario rows retain stageArc array order. Occurrence rows retain every source;
blue_coin rows deduplicate persistent (effective owner, event ID), not episodes.
Presence is conditional on loading that scene; script path feasibility, object
interaction requirements and normal connected reachability remain open.
"""
import collections
import csv
import hashlib
import operator
from pathlib import Path
import re
import struct
import sys

import discfmt

REPO = Path(__file__).resolve().parents[2]
ROOT = Path(discfmt.ROOT)
OUTPUT = REPO / 'sunshine-pc/docs/content-coverage.tsv'
COLUMNS = ('kind', 'identity', 'raw_stage', 'scenario_index', 'selected_episode',
           'shine_owner', 'event_id', 'flag', 'archive', 'classification',
           'metadata_status', 'runtime_status', 'source_refs', 'condition')


class DecodeError(ValueError):
    pass


class Reader:
    def __init__(self, data, pos=0, end=None):
        self.data, self.pos = data, pos
        self.end = len(data) if end is None else end
        if not 0 <= pos <= self.end <= len(data):
            raise DecodeError('reader bounds')

    def take(self, size):
        if size < 0 or self.pos + size > self.end:
            raise DecodeError('field out of bounds at 0x%x size=%d' % (self.pos, size))
        pos = self.pos
        self.pos += size
        return self.data[pos:self.pos]

    def u32(self):
        return struct.unpack('>I', self.take(4))[0]

    def s32(self):
        return struct.unpack('>i', self.take(4))[0]

    def u16(self):
        return struct.unpack('>H', self.take(2))[0]

    def string(self):
        return self.take(self.u16())

    def count(self):
        count = self.s32()
        if count < 0 or count > (self.end - self.pos) // 4:
            raise DecodeError('count exceeds enclosing block at 0x%x' % (self.pos - 4))
        return count


def key(name):
    value = 0
    for byte in name:
        value = (3 * value + byte) & 0xffff
    return value


def light_map(reader):
    for _ in range(reader.count()):
        reader.u32()
        reader.string()


def parse_node(data, pos=0, end=None):
    reader = Reader(data, pos, end)
    size = reader.u32()
    if size < 12 or pos + size > reader.end:
        raise DecodeError('node size at 0x%x: %d' % (pos, size))
    reader.end = pos + size
    type_key, typ = reader.u16(), reader.string()
    name_key, name = reader.u16(), reader.string()
    if type_key != key(typ) or name_key != key(name):
        raise DecodeError('JDrama key mismatch at 0x%x' % pos)
    try:
        typ = typ.decode('ascii')
    except UnicodeError as error:
        raise DecodeError('non-ASCII class at 0x%x' % pos) from error
    node = dict(type=typ, name=name, start=pos, payload=reader.pos,
                end=reader.end, children=[])
    # Source loadSuper prefixes, not guessed embedded nodes in arbitrary payloads.
    groups = {'NameRefGrp', 'GroupObj', 'Strategy', 'IdxGroup', 'MarScene',
              'SmJ3DScn', 'AmbAry', 'LightAry', 'ScenarioArchiveNamesInStage',
              'ScenarioArchiveNameTable'}
    if typ in groups:
        if typ in ('MarScene', 'SmJ3DScn'):
            light_map(reader)
        if typ == 'IdxGroup':
            reader.u32()
        for _ in range(reader.count()):
            child = parse_node(data, reader.pos, reader.end)
            node['children'].append(child)
            reader.pos = child['end']
        if reader.pos != reader.end:
            raise DecodeError('unconsumed group bytes at 0x%x' % reader.pos)
    return node


def flatten(node):
    yield node
    for child in node['children']:
        yield from flatten(child)


def stage_scenarios(data):
    root = parse_node(data)
    if root['end'] != len(data) or root['type'] != 'NameRefGrp':
        raise DecodeError('stageArc root/trailing bytes')
    if len(root['children']) != 1:
        raise DecodeError('stageArc outer array')
    stages = root['children'][0]
    if stages['type'] != 'ScenarioArchiveNamesInStage':
        raise DecodeError('stageArc stage array type')
    result = []
    for raw, stage in enumerate(stages['children']):
        if stage['type'] != 'ScenarioArchiveNameTable':
            raise DecodeError('stageArc scenario array type')
        for scenario, record in enumerate(stage['children']):
            if record['type'] != 'ScenarioArchiveName':
                raise DecodeError('stageArc archive record type')
            reader = Reader(data, record['payload'], record['end'])
            archive = reader.string().decode('ascii')
            if reader.pos != reader.end or not re.fullmatch(r'[A-Za-z0-9_]+\.arc', archive):
                raise DecodeError('archive string boundary/name')
            result.append((raw, scenario, archive, record['start']))
    return result


def actor_reader(data, node):
    reader = Reader(data, node['payload'], node['end'])
    reader.take(36)  # TPlacement position, TActor rotation/scaling.
    reader.string()
    light_map(reader)
    return reader


def enemy_reader(data, node, short_ok=False):
    reader = actor_reader(data, node)
    reader.string()  # TSpineEnemy manager and graph.
    if reader.pos == reader.end:
        if short_ok:
            return reader  # graph read hits the node end: retail readString leaves '' (per-node stream, JDRNameRef.cpp getType)
        raise DecodeError('retail node ends after manager; source loader still requests graph and drop fields; no event ID inferred')
    reader.string()
    return reader


def shine_event(value):
    return 120 if value == -1 else value


def blue_identity(owner, event):
    # Invalid serialized IDs stay diagnostics, never silently alias a real coin.
    if not 0 <= event < 50:
        raise DecodeError('invalid blue event ID %d' % event)
    return (1 if owner == 0 or owner >= 10 else owner), event


def identity_name(kind, owner, event):
    if kind == 'shine' and not 0 <= event < 120:
        raise DecodeError('invalid Shine reward ID %d' % event)
    return 'shine:%03d' % event if kind == 'shine' else 'blue:%d:%02d' % blue_identity(owner, event)


class Sources:
    def __init__(self):
        self.texts = {}
        self.factories, self.parents, self.loads, self.map_flags, self.models = {}, {}, {}, {}, {}
        for path in sorted((REPO / 'sms/src/System').glob('MarNameRefGen*.cpp')):
            rel = path.relative_to(REPO).as_posix()
            text = self.text(rel)
            pattern = r'if\s*\(strcmp\(name,\s*"([^"]+)"\)\s*==\s*0\)\s*return new ([\w:]+)'
            for match in re.finditer(pattern, text):
                self.factories[match[1]] = (match[2], self.ref(rel, match.start()))
        for directory in ('MoveBG', 'Enemy', 'Animal', 'NPC'):
            for path in sorted((REPO / 'sms/include' / directory).glob('*.hpp')):
                rel = path.relative_to(REPO).as_posix()
                for match in re.finditer(r'class\s+(\w+)\s*:\s*public\s+([\w:]+)', self.text(rel)):
                    self.parents[match[1]] = match[2]
            for path in sorted((REPO / 'sms/src' / directory).glob('*.cpp')):
                rel = path.relative_to(REPO).as_posix()
                text = self.text(rel)
                for match in re.finditer(r'void (\w+)::load\(JSUMemoryInputStream&\s*\w*\)\s*\{', text):
                    stop = text.find('\n}', match.end())
                    body = text[match.end():stop]
                    call = re.search(r'([\w:]+)::load\(', body)
                    self.loads[match[1]] = call[1] if call else None
        rel = 'sms/src/MoveBG/MapObjInit.cpp'
        text = self.text(rel)
        for match in re.finditer(r'static TMapObjData (\w+) = \{(.*?)\};', text, re.S):
            fields = re.findall(r'"[^"]*"|[^,]+', match[2])
            fields = [field.strip() for field in fields if field.strip()]
            if len(fields) != 15:
                raise DecodeError('TMapObjData layout: ' + match[1])
            name = fields[0].strip('"').encode('ascii')
            self.map_flags[name] = int(fields[13], 0)
            if name in (b'SandBird', b'CoinFish'):
                anim = re.search(r'\{\s*"([^"]+\.bmd)"', self.array_body(rel, fields[4].lstrip('&').replace('_info', '_data')), re.S) if fields[4] != 'nullptr' else None
                self.models[name] = anim[1] if anim else name.decode('ascii') + '.bmd'
        item_text = self.text('sms/src/MoveBG/Item.cpp')
        self.coin100_name = re.search(r'void TCoin::taken\b.*?TNameRefGen::search\("([^"]+)"\)', item_text, re.S)[1].encode('shift_jis')
        manager_text = self.text('sms/src/MoveBG/MapObjManager.cpp')
        self.hidden_shine_template = re.search(r'snprintf\(buffer, 256, "([^"]*%s[^"]*)", name\)', manager_text)[1].encode('shift_jis')

    def text(self, rel):
        if rel not in self.texts:
            self.texts[rel] = (REPO / rel).read_text(encoding='utf-8')
        return self.texts[rel]

    def ref(self, rel, offset):
        return '%s:%d' % (rel, self.text(rel).count('\n', 0, offset) + 1)

    def array_body(self, rel, name):
        match = re.search(r'\b' + re.escape(name) + r'\s*\[\s*\]\s*=\s*\{(.*?)\}', self.text(rel), re.S)
        if not match:
            raise DecodeError('missing source array: ' + name)
        return match[1]

    def numbers(self, rel, name):
        return [int(value.strip(), 0) for value in self.array_body(rel, name).split(',') if value.strip()]

    def array_ref(self, rel, name):
        return self.ref(rel, self.text(rel).index(name))

    def is_a(self, cls, parent):
        seen = set()
        while cls and cls not in seen:
            if cls == parent:
                return True
            seen.add(cls)
            cls = self.parents.get(cls)
        return False

    def loads_through(self, cls, target):
        seen = set()
        while cls and cls not in seen:
            if cls == target:
                return True
            seen.add(cls)
            cls = self.loads.get(cls) if cls in self.loads else self.parents.get(cls)
        return False


def fst_paths(data):
    reader = Reader(data)
    root_word, _, count = reader.u32(), reader.u32(), reader.u32()
    if root_word >> 24 != 1 or count < 1 or count * 12 > len(data):
        raise DecodeError('FST root')
    paths, directories = set(), [('', count)]
    names = count * 12
    for index in range(1, count):
        while directories and index >= directories[-1][1]:
            directories.pop()
        if not directories:
            raise DecodeError('FST directory extent')
        item = Reader(data, index * 12, index * 12 + 12)
        word, _, extent = item.u32(), item.u32(), item.u32()
        start = names + (word & 0xffffff)
        if not names <= start < len(data):
            raise DecodeError('FST name offset')
        stop = data.find(b'\0', start)
        if stop < 0:
            raise DecodeError('FST name terminator')
        name = data[start:stop].decode('ascii')
        path = directories[-1][0] + name
        if word >> 24:
            if word >> 24 != 1 or not index < extent <= directories[-1][1]:
                raise DecodeError('FST subdirectory extent')
            directories.append((path + '/', extent))
        else:
            paths.add(path)
    return paths


def joint_blue_indices(data):
    reader = Reader(data)
    magic, size, count = reader.take(8), reader.u32(), reader.u32()
    if magic[:3] != b'J3D' or size != len(data):
        raise DecodeError('J3D file header')
    reader.take(16)
    result = []
    for _ in range(count):
        start = reader.pos
        tag, size = reader.take(4), reader.u32()
        if size < 8 or start + size > reader.end:
            raise DecodeError('J3D block extent')
        if tag == b'JNT1':
            block = Reader(data, start + 8, start + size)
            joint_count = block.u16()
            block.take(10)
            names = start + block.u32()
            table = Reader(data, names, start + size)
            name_count = table.u16()
            table.u16()
            if name_count != joint_count:
                raise DecodeError('JNT1 name/joint count mismatch')
            entries_end = table.pos + name_count * 4
            for index in range(name_count):
                table.u16()
                offset = names + table.u16()
                if not entries_end <= offset < start + size:
                    raise DecodeError('JNT1 string offset')
                stop = data.find(b'\0', offset, start + size)
                if stop < 0:
                    raise DecodeError('JNT1 string terminator')
                name = data[offset:stop]
                if index < joint_count - 1 and b'redcoin' not in name and b'bluecoin' in name:
                    result.append(index)
        reader.pos = start + size
    return result


# SPC instruction order/operands: spcinterp.cpp:244-571. Static decoding only;
# shortcut: constants within basic blocks only; interprocedural/path proof needs gameplay or a separate analysis.
OPCODES = ('int flt str adr var nop inc dec add sub mul div mod ass eq ne gt lt ge le neg not and or band bor shl shr call func mkfr mkds ret ret0 jne jmp pop int0 int1 end').split()
OPERANDS = {'int': 4, 'flt': 4, 'str': 4, 'adr': 4, 'var': 8,
            'inc': 9, 'dec': 9, 'ass': 9, 'call': 8, 'func': 8,
            'mkfr': 4, 'mkds': 4, 'jne': 4, 'jmp': 4}
RELEVANT_CALLS = {'getSystemFlag', 'setSystemFlag', 'setEventID', 'appearShine',
                  'appearShineFromNPC', 'appearShineFromNPCWithoutDemo',
                  'appearShineFromKageMario', 'appearShineForWoodBox', 'killShine',
                  'maniCoinFallDown'}


def spc_calls(data, relevant_calls=RELEVANT_CALLS):
    reader = Reader(data)
    if reader.take(4) != b'SPCB':
        raise DecodeError('SPCB magic')
    text, data_offset, data_count, symbols, symbol_count, _ = (reader.u32() for _ in range(6))
    pool = data_offset + data_count * 4
    names = symbols + symbol_count * 20
    if not 28 <= text <= data_offset <= pool <= symbols <= names <= len(data):
        raise DecodeError('SPCB section bounds')

    def cstring(offset, lower, upper):
        if not lower <= offset < upper:
            raise DecodeError('SPCB string offset')
        stop = data.find(b'\0', offset, upper)
        if stop < 0:
            raise DecodeError('SPCB string terminator')
        return data[offset:stop]

    strings = []
    table = Reader(data, data_offset, pool)
    for _ in range(data_count):
        strings.append(cstring(pool + table.u32(), pool, symbols))
    symbol_names = []
    for index in range(symbol_count):
        entry = Reader(data, symbols + 20 * index, symbols + 20 * (index + 1))
        entry.u32()
        symbol_names.append(cstring(names + entry.u32(), names, len(data)).decode('ascii'))
    reader = Reader(data, text, data_offset)
    instructions, targets = [], {0}
    while reader.pos < reader.end:
        pc = reader.pos - text
        # Retail SPCB text ends with up to three zero-fill bytes after its final return.
        if instructions and instructions[-1][1] in ('ret', 'ret0', 'end') and reader.end - reader.pos <= 3 and not any(data[reader.pos:reader.end]):
            break
        opcode = reader.take(1)[0]
        if opcode >= len(OPCODES):
            raise DecodeError('SPCB opcode at 0x%x' % pc)
        op = OPCODES[opcode]
        args = reader.take(OPERANDS.get(op, 0))
        if op in ('inc', 'dec', 'ass'):
            if args[0] != 4:
                raise DecodeError('SPCB variable operand opcode')
            args = args[1:]
        values = struct.unpack('>' + 'I' * (len(args) // 4), args) if args else ()
        instructions.append((pc, op, values))
        if op in ('call', 'jne', 'jmp'):
            targets.add(values[0])
    starts = {pc for pc, _, _ in instructions}
    if not targets <= starts:
        raise DecodeError('SPCB branch target boundary')
    stack, variables, result = [], {}, []
    binary = {'add': operator.add, 'sub': operator.sub, 'mul': operator.mul,
              'eq': operator.eq, 'ne': operator.ne, 'gt': operator.gt,
              'lt': operator.lt, 'ge': operator.ge, 'le': operator.le,
              'band': operator.and_, 'bor': operator.or_}
    block = 0

    def pop():
        return stack.pop() if stack else None

    for pc, op, args in instructions:
        if pc in targets:
            stack.clear()
            variables.clear()
            block = pc
        if op in ('int', 'adr'):
            stack.append(args[0] if args[0] < 0x80000000 else args[0] - 0x100000000)
        elif op == 'flt':
            stack.append(None)
        elif op == 'str':
            if args[0] >= len(strings):
                raise DecodeError('SPCB data index')
            stack.append(strings[args[0]])
        elif op == 'var':
            stack.append(variables.get(args))
        elif op == 'ass':
            variables[args] = pop()
        elif op in ('inc', 'dec'):
            value = variables.get(args)
            value = value + (1 if op == 'inc' else -1) if isinstance(value, int) else None
            variables[args] = value
            stack.append(value)
        elif op in binary:
            right, left = pop(), pop()
            value = binary[op](left, right) if isinstance(left, int) and isinstance(right, int) else None
            stack.append(int(value) if value is not None and -0x80000000 <= value <= 0x7fffffff else None)
        elif op in ('div', 'mod', 'and', 'or', 'shl', 'shr'):
            pop()
            pop()
            stack.append(None)
        elif op in ('neg', 'not'):
            value = pop()
            stack.append((-value if op == 'neg' else int(not value)) if isinstance(value, int) else None)
        elif op in ('int0', 'int1'):
            stack.append(int(op == 'int1'))
        elif op == 'func':
            symbol, argc = args
            if symbol >= len(symbol_names) or argc > 1024:
                raise DecodeError('SPCB builtin index/argc')
            values = [pop() for _ in range(argc)][::-1]
            name = symbol_names[symbol]
            if name in relevant_calls:
                result.append((pc, block, name, values))
            stack.append(values[0] if name == 'getNameRefHandle' and len(values) == 1 else None)
        elif op in ('call', 'jne', 'jmp', 'ret', 'ret0', 'end'):
            stack.clear()
            variables.clear()
            block = pc + 1 + OPERANDS.get(op, 0)
        elif op == 'pop':
            pop()
    return result


def selected_episode(raw, scenario):
    if raw in (2, 3, 4, 5, 6, 8, 9) and scenario < 8:
        return str(scenario), 'selected course episode; normal selection route'
    routes = {13: {0: '0,1,3', 1: '2', 2: '4', 3: '5', 4: '6', 5: '7'},
              7: {0: '0,1,5', 1: '2', 2: '3,4', 3: '6', 4: '7'},
              14: {0: '0,1,2,3,5,6,7', 1: '4'},
              58: {0: '1,2,3,4,5,6,7', 1: '0'}}
    if raw in routes:
        return routes[raw].get(scenario, 'unresolved'), 'only when next scenario=255; source switch default included; explicit transitions may differ'
    return 'context', 'selected episode is persistent context, not inferred from archive digits/index'


def row(kind, identity='', **fields):
    result = dict.fromkeys(COLUMNS, '')
    result.update(kind=kind, identity=identity, runtime_status='open', **fields)
    return result


def collect_archive(files, sources):
    occurrences, issues = [], []
    nodes = []
    for member in ('/map/scene.bin',):
        if member not in files:
            issues.append((member, 'missing scene object stream'))
            continue
        root = parse_node(files[member])
        if any(files[member][root['end']:]):
            issues.append((member, 'nonzero trailing scene bytes'))
        nodes.extend((member, n) for n in flatten(root))
    for member, n in nodes:
        typ = n['type']
        cls, factory_ref = sources.factories.get(typ, ('', ''))
        ref = '%s@0x%x:%s' % (member, n['start'], typ)
        data = files[member]

        def add(kind, event, classification, source, condition='loaded placement; interaction/reveal and script conditions unproven'):
            occurrences.append(dict(kind=kind, event=event, classification=classification,
                                    ref=ref + ';' + factory_ref + ';' + source,
                                    condition=condition, node=n))

        try:
            if typ in ('Shine', 'CoinBlue'):
                reader = actor_reader(data, n)
                reader.string()
                if typ == 'Shine':
                    mode = reader.string()
                    event = shine_event(reader.s32())
                    reader.s32()
                    classification = 'direct-shine:coin100-target' if n['name'] == sources.coin100_name else 'direct-shine'
                    add('shine', event, classification, 'sms/src/MoveBG/Item.cpp:199-218,818-842',
                        'mode=' + ('normal' if mode == b'normal' else 'quickly' if mode == b'quickly' else 'hidden') + '; reveal/reward path unproven')
                else:
                    event = reader.s32()
                    add('blue', 0 if event == -1 else event, 'direct-blue-coin', 'sms/src/MoveBG/Item.cpp:394-409')
            elif sources.loads_through(cls, 'THideObjBase'):
                reader = actor_reader(data, n)
                model = reader.string()
                if model not in sources.map_flags:
                    raise DecodeError('hidden model absent from source TMapObjData')
                if sources.map_flags[model] & 0x80:
                    reader.take(4)
                if reader.end - reader.pos < 4:
                    # No event field (coro_ex6 MapObjSteam): retail reads an uninitialised local; the PC build presets -1,
                    # no hidden object (sms/src/MoveBG/MapObjHide.cpp THideObjBase::load).
                    continue
                event = reader.s32()
                if 0 <= event < 50:
                    add('blue', event, 'hidden-generated-blue-coin', 'sms/src/MoveBG/MapObjHide.cpp:86-118;sms/src/MoveBG/MapObjLib.cpp:57-75')
                elif event == 777:
                    target = sources.hidden_shine_template.replace(b'%s', n['name'])
                    # Resolved after the full stream, since targets can follow their generators.
                    occurrences.append(dict(kind='named-shine', target=target, event=777, classification='hidden-named-shine-reference', ref=ref + ';sms/src/MoveBG/MapObjManager.cpp:400-412', condition='named existing Shine; reveal path unproven', node=n))
            elif cls in ('TAnimalBird', 'TBeeHive', 'TButterfloid', 'TFishoid'):
                reader = enemy_reader(data, n)
                if cls != 'TAnimalBird':
                    reader.u32()
                events = [reader.s32()]
                if cls == 'TBeeHive':
                    events.append(reader.s32())
                # Butterfly types 0/2 ignore the serialized event ID.
                if cls == 'TButterfloid' and typ != 'ButterflyB':
                    continue
                for index, event in enumerate(events):
                    if 0 <= event < 50:
                        add('blue', event, 'animal-generated-blue-coin:%d' % index,
                            {'TAnimalBird': 'sms/src/Animal/Bird.cpp:107-134', 'TBeeHive': 'sms/src/Animal/BeeHive.cpp:193-212', 'TButterfloid': 'sms/src/Animal/Butterfly.cpp:136-151', 'TFishoid': 'sms/src/Animal/fishoid.cpp:271-283'}[cls])
            elif sources.is_a(cls, 'TBaseNPC'):
                reader = enemy_reader(data, n)
                if reader.end - reader.pos == 0:
                    continue  # Board/dummy actor types do not load individual reward data.
                reader.take(44)
                event = reader.s32()
                if 0 <= event < 50:
                    add('blue', event, 'npc-generated-blue-coin', 'sms/src/NPC/NpcInitPrg.cpp:363-387,454-470;sms/src/NPC/NpcCoin.cpp:19-20')
            elif sources.loads_through(cls, 'TSmallEnemy') or sources.loads_through(cls, 'TAmiNoko'):
                reader = enemy_reader(data, n, short_ok=True)
                if reader.end - reader.pos < 4:
                    # No drop field: the short read keeps the ctor's mCoinId(-1) (smallEnemy.cpp:169, 229; PC read(s32&)
                    # keeps a failed read's preset), and newAndRegisterCoin(0xFFFFFFFF) returns nullptr: no coin. All three
                    # functions match retail 100 % (objdiff), so this is retail behaviour, not a port gap.
                    continue
                event = reader.s32()
                if 0 <= event < 50:
                    add('blue', event, 'enemy-drop-blue-coin', 'sms/src/Enemy/smallEnemy.cpp:226-238;sms/src/Enemy/amiNoko.cpp:154-157')
            elif cls == 'TShellCup':
                add('blue', 2, 'source-generated-shell-blue-coin', 'sms/src/MoveBG/MapObjPinna.cpp:437-450')
            elif sources.is_a(cls, 'TJointCoin'):
                reader = actor_reader(data, n)
                model = reader.string()
                model_name = sources.models.get(model)
                matches = [path for path in files if model_name and path.lower() == ('/mapobj/' + model_name).lower()]
                if len(matches) != 1:
                    raise DecodeError('joint generator model binding unavailable')
                indices = joint_blue_indices(files[matches[0]])
                add('resource', '', 'joint-generator-model-checked', matches[0] + ':JNT1;sms/src/MoveBG/MapObjEx.cpp:109-132',
                    'blue-producing joint indices=%s;last joint excluded by source loop;no model bytes/names emitted' % ','.join(map(str, indices)))
                for index in indices:
                    add('blue', 0, 'joint-generated-default-event-blue-coin',
                        'sms/src/MoveBG/MapObjEx.cpp:82-132;sms/src/MoveBG/MapObjBase.cpp:678;sms/include/JSystem/J3D/J3DGraphLoader/J3DJointFactory.hpp:19-35',
                        'model=%s; joint_index=%d; constructor event=0; scripts may replace it' % (matches[0], index))
            elif cls == 'THideObjInfo':
                reader = actor_reader(data, n)
                event = reader.s32()
                if 0 <= event < 50:
                    issues.append((ref, 'type-only switch generator discards event identity; blue item pool binding requires resolution (MapObjTown.cpp:467-486)'))
                else:
                    add('resource', event, 'nonblue-type-only-switch-generator', 'sms/src/MoveBG/MapObjTown.cpp:467-486',
                        'serialized event selects item type, not a new persistent identity')
        except (DecodeError, struct.error, UnicodeError) as error:
            issues.append((ref, str(error)))
    for occurrence in occurrences:
        if occurrence['kind'] == 'named-shine':
            matches = [x for x in occurrences if x['kind'] == 'shine' and x['node']['name'] == occurrence['target']]
            if len(matches) == 1:
                occurrence['kind'] = 'shine'
                occurrence['event'] = matches[0]['event']
                occurrence['ref'] += ';target=' + matches[0]['ref']
            else:
                issues.append((occurrence['ref'], 'event777 target count=%d' % len(matches)))
    scripts = []
    for member in sorted(path for path in files if path.startswith(('/map/sp/', '/sp/')) and path.endswith('.sb')):
        try:
            scripts.extend((member, *call) for call in spc_calls(files[member]))
            occurrences.append(dict(kind='resource', event='', classification='decoded-spcb', ref=member,
                                    condition='bounded instruction walk; final zero-fill excluded; dynamic arguments/path feasibility unproven'))
        except (DecodeError, struct.error, UnicodeError) as error:
            issues.append((member, str(error)))
    return occurrences, scripts, issues, nodes


def inventory():
    sources = Sources()
    stage_rel = 'sms/include/System/StageUtil.hpp'
    owner_rel = 'sms/src/System/StageUtil.cpp'
    owners = sources.numbers(owner_rel, 'shineStageTable')
    scenarios = stage_scenarios((ROOT / 'data/stageArc.bin').read_bytes())
    fst_file = ROOT.parent / 'sys/fst.bin'
    fst = fst_paths(fst_file.read_bytes())
    if max(raw for raw, _, _, _ in scenarios) >= len(owners):
        raise DecodeError('stageArc exceeds source ownership table')
    classes = {}
    for table, extra in (('scShineConvTable', False), ('scEtcShineConvTable', True)):
        for owner, name in enumerate(sources.array_body(stage_rel, table).split(',')):
            name = name.strip()
            if not name or name == 'nullptr':
                continue
            for episode, event in enumerate(sources.numbers(stage_rel, name)):
                classification = ('course-100-coin-slot' if episode == 0 else 'course-extra-slot') if extra else 'course-episode-slot'
                if owner == 0:
                    classification = 'airport-accounting-slot'
                elif owner == 1:
                    classification = 'dolpic-extra-zero-slot'
                classes[event] = (owner, episode, classification, sources.array_ref(stage_rel, name))
    for event in list(range(0x46, 0x56)) + list(range(0x6c, 0x74)):
        classes[event] = ('', '', 'blue-coin-exchange-reward', 'sms/src/GC2D/GCConsole2.cpp:2035-2044')
    classes[0x58] = (0, '', 'airport-accounting-slot', 'sms/src/GC2D/Guide.cpp:809-815')
    classes[0x77] = ('', '', 'ending-movie14-reward', 'sms/src/System/MovieDirector.cpp:240-248')
    rows, scenario_rows = [], []
    for raw, scenario, archive, offset in scenarios:
        selected, condition = selected_episode(raw, scenario)
        szs = 'data/scene/' + archive[:-4] + '.szs'
        arc = 'data/scene/' + archive
        path = szs if (ROOT / szs).is_file() else arc
        present = (ROOT / path).is_file()
        classification = 'present-scene-resource' if present else 'placeholder-reference' if archive == 'none.arc' else 'absent-referenced-resource'
        evidence = 'FST-present' if szs in fst or arc in fst else 'FST-absent; absence does not prove unused'
        scenario_rows.append(row('scenario', 'stage:%02d:scenario:%02d' % (raw, scenario), raw_stage=raw,
            scenario_index=scenario, selected_episode=selected, shine_owner=owners[raw], archive=archive,
            classification=classification, metadata_status='decoded' if present else 'unresolved-resource',
            source_refs='sms/orig/GMSJ01/extracted/files/data/stageArc.bin@0x%x;sms/src/System/Application.cpp:287-289,892-916;%s;sms/src/System/MarDirectorDirect.cpp:1080-1167' % (offset, sources.array_ref(owner_rel, 'shineStageTable')),
            condition=condition + ';' + evidence))
    rows.extend(scenario_rows)
    occurrences, script_rows, issues, input_rows, resource_rows = [], [], [], [], []
    seen_inputs = set()

    def input_record(path, label):
        if label in seen_inputs:
            return
        seen_inputs.add(label)
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        input_rows.append(row('input', label, classification='metadata-provenance', metadata_status='hashed',
                              source_refs=label, condition='sha256=' + digest.hexdigest()))

    common_scripts = []
    common_path = ROOT / 'data/common.szs'
    common_files = discfmt.load_szs(str(common_path))
    for member in sorted(path for path in common_files if path.startswith('/sp/') and path.endswith('.sb')):
        try:
            common_scripts.extend(('common.szs:' + member, *call) for call in spc_calls(common_files[member]))
        except (DecodeError, struct.error, UnicodeError) as error:
            issues.append(row('unresolved', 'common-script', classification='script-decode', metadata_status='unresolved', source_refs='data/common.szs:' + member, condition=str(error)))
    common_files = None
    discfmt._cache.clear()
    input_record(common_path, 'sms/orig/GMSJ01/extracted/files/data/common.szs')
    # Repeated archives are decoded once; scenario rows and provenance remain separate.
    by_archive = collections.OrderedDict()
    for scenario_row in scenario_rows:
        by_archive.setdefault(scenario_row['archive'], []).append(scenario_row)
    archive_object_counts = collections.Counter()
    for archive, scene_rows in by_archive.items():
        szs = ROOT / ('data/scene/' + archive[:-4] + '.szs')
        path = szs if szs.is_file() else ROOT / ('data/scene/' + archive)
        if not path.is_file():
            continue
        files = discfmt.load_szs(str(path))
        found, scripts, problems, nodes = collect_archive(files, sources)
        archive_object_counts.update(n['type'] for _, n in nodes)
        for scene_row in scene_rows:
            context = {name: scene_row[name] for name in ('raw_stage', 'scenario_index', 'selected_episode', 'shine_owner', 'archive')}
            for occurrence in found:
                if occurrence['kind'] == 'resource':
                    resource_rows.append(row('resource', occurrence['ref'], **context, event_id=occurrence['event'],
                        classification=occurrence['classification'], metadata_status='decoded', source_refs=path.relative_to(ROOT).as_posix() + ':' + occurrence['ref'],
                        condition=occurrence['condition']))
                if occurrence['kind'] not in ('shine', 'blue'):
                    continue
                event, kind = occurrence['event'], occurrence['kind']
                valid = 0 <= event < (120 if kind == 'shine' else 50)
                identity = identity_name(kind, context['shine_owner'], event) if valid else 'shine-sentinel:%d' % event if kind == 'shine' else 'invalid-blue:%d' % event
                flag = 0x10000 + event if kind == 'shine' and valid else 0x10078 + (blue_identity(context['shine_owner'], event)[0] - 1) * 50 + event if valid else None
                occurrences.append(row('occurrence', identity, **context, event_id=event, flag='' if flag is None else '0x%x' % flag,
                    classification=occurrence['classification'], metadata_status='conditional-placement' if valid else 'sentinel-not-reward',
                    source_refs=path.relative_to(ROOT).as_posix() + ':' + occurrence['ref'],
                    condition=occurrence['condition']))
            for member, pc, block, name, values in common_scripts + scripts:
                numeric = ','.join('0x%x' % value if isinstance(value, int) and value >= 0 else str(value) if isinstance(value, int) else 'object-ref' if isinstance(value, bytes) else 'dynamic' for value in values)
                ref = ('data/' if member.startswith('common.szs:') else path.relative_to(ROOT).as_posix() + ':') + member + '@text+0x%x' % pc
                script_rows.append(row('script_call', '%s:0x%x' % (name, pc), **context,
                    classification=name, metadata_status='static-callsite', source_refs=ref + ';sms/src/System/EventWatcher.cpp:56-75,1086-1095',
                    condition='args=' + numeric + ';basic_block=0x%x;control-flow/eligibility unproven' % block))
                if name == 'setEventID':
                    targets = [n for _, n in nodes if len(values) == 2 and isinstance(values[0], bytes) and n['name'] == values[0]]
                    if len(targets) == 1 and isinstance(values[1], int) and targets[0]['type'] in ('Shine', 'CoinBlue'):
                        target = targets[0]
                        event = values[1] & 0xffff
                        kind = 'shine' if target['type'] == 'Shine' else 'blue'
                        valid = 0 <= event < (120 if kind == 'shine' else 50)
                        if valid:
                            identity = identity_name(kind, context['shine_owner'], event)
                            flag = 0x10000 + event if kind == 'shine' else 0x10078 + (blue_identity(context['shine_owner'], event)[0] - 1) * 50 + event
                            occurrences.append(row('occurrence', identity, **context, event_id=event, flag='0x%x' % flag,
                                classification='script-event-override', metadata_status='conditional-script-assignment',
                                source_refs=ref + ';target=/map/scene.bin@0x%x;%s' % (target['start'], 'sms/src/System/EventWatcher.cpp:1086-1095'),
                                condition='static constant assignment; callsite reachability/order not proven'))
                        else:
                            issues.append(row('unresolved', 'script-invalid-event:%d' % event, **context, event_id=event,
                                classification='sentinel-not-reward', metadata_status='unresolved', source_refs=ref,
                                condition='setEventID narrows to u16; not aliased to a reward'))
                    elif member.endswith('/manievent.sb') and len(values) == 2 and values[0] == 'シャイン（マニ屋用）'.encode('cp932') and values[1] is None:
                        # Blue-coin exchange (Delfino boathouse): text 0x322-0x37c computes local5 = 0x10000 + 70 + i, plus 22
                        # when i > 15 (i = exchanges done); setEventID narrows to u16, so i 0-23 give Shines 70-85 and
                        # 108-115: the 24 flags GCConsole2.cpp:2035-2044 counts, already listed as blue-coin-exchange-reward.
                        continue
                    else:
                        issues.append(row('unresolved', 'script-event-binding', **context, classification='dynamic-event-assignment',
                            metadata_status='unresolved', source_refs=ref, condition='target matches=%d; args=%s; requires interprocedural variable/branch analysis' % (len(targets), numeric)))
            for ref, problem in problems:
                issues.append(row('unresolved', 'scene-payload', **context, classification='generator-or-parser-condition', metadata_status='unresolved',
                    source_refs=path.relative_to(ROOT).as_posix() + ':' + ref, condition=problem))
        input_record(path, 'sms/orig/GMSJ01/extracted/files/' + path.relative_to(ROOT).as_posix())
        files = found = nodes = scripts = None
        discfmt._cache.clear()
    by_identity = collections.defaultdict(list)
    for occurrence in occurrences:
        by_identity[occurrence['identity']].append(occurrence)
    for event in range(120):
        owner, episode, classification, ref = classes.get(event, ('', '', 'scene-reward-route-unresolved', 'sms/src/System/FlagManager.cpp:334-353,498-504'))
        identity = identity_name('shine', owner, event)
        bindings = by_identity.get(identity, [])
        if event not in classes and bindings:
            ref += ';' + bindings[0]['source_refs']
        rows.append(row('shine', identity, shine_owner=owner, selected_episode=episode if classification == 'course-episode-slot' else '', event_id=event, flag='0x%x' % (0x10000 + event),
            classification=classification, metadata_status='source-classified' if event in classes else 'scene-bound-route-unresolved' if bindings else 'unresolved',
            source_refs=ref, condition='source_table_slot=%s;scene/script bindings=%d;coin100_named_target=%s;earning route and runtime open;extra slot is not selected episode or archive index' % (episode, len(bindings), 'yes' if any(item['classification'] == 'direct-shine:coin100-target' for item in bindings) else 'not-observed')))
    blue_keys = sorted({(int(item['flag'], 16), item['identity']) for item in occurrences if item['identity'].startswith('blue:')})
    for flag, identity in blue_keys:
        bindings = by_identity[identity]
        owner, event = map(int, identity.split(':')[1:])
        rows.append(row('blue_coin', identity, shine_owner=owner, event_id=event, flag='0x%x' % flag,
            classification='deduplicated-observed-persistent-identity', metadata_status='conditional-identity',
            source_refs='sms/src/System/FlagManager.cpp:362-389',
            condition='bindings=%d;see occurrence rows for each ordered scenario and source;not runtime physical/reachability proof' % len(bindings)))
    rows.extend(occurrences)
    rows.extend(script_rows)
    rows.extend(resource_rows)
    rows.extend(issues)
    ex_table = sources.numbers(owner_rel, 'exShineTable')
    for index, event in enumerate(ex_table):
        if event != 255:
            rows.append(row('extra_first_clear', 'stage:%02d' % (index + 21), raw_stage=index + 21, shine_owner=owners[index + 21], event_id=event,
                flag='0x%x' % (0x10000 + event), classification='source-extra-first-clear-map', metadata_status='source-known',
                source_refs=sources.array_ref(owner_rel, 'exShineTable') + ';sms/src/System/StageUtil.cpp:69-75', condition='numeric extra-stage predicate; not a complete playable-secret list'))
    input_record(ROOT / 'data/stageArc.bin', 'sms/orig/GMSJ01/extracted/files/data/stageArc.bin')
    input_record(fst_file, 'sms/orig/GMSJ01/extracted/sys/fst.bin')
    # Common object stream is recorded even though it contains managers, not scene placements.
    common_scene = ROOT / 'data/scenecmn.bin'
    if common_scene.is_file():
        common_nodes = list(flatten(parse_node(common_scene.read_bytes())))
        rows.append(row('resource', 'scenecmn', classification='common-object-stream', metadata_status='decoded', source_refs='data/scenecmn.bin;sms/src/System/MarDirectorSetupObjects.cpp:225-228', condition='nodes=%d;types=%s' % (len(common_nodes), ','.join(sorted({n['type'] for n in common_nodes})))))
        input_record(common_scene, 'sms/orig/GMSJ01/extracted/files/data/scenecmn.bin')
    semantic_inputs = {'sms/src/Strategic/spcinterp.cpp', 'sms/include/Strategic/spcinterp.hpp'}
    for item in rows:
        semantic_inputs.update(re.findall(r'\bsms/(?:src|include)/[A-Za-z0-9_./-]+\.(?:cpp|hpp|tpp)\b', item['source_refs']))
    for rel in sorted(set(sources.texts) | semantic_inputs):
        input_record(REPO / rel, rel)
    input_record(Path(__file__).resolve(), 'sunshine-pc/tools/content_inventory.py')
    input_record(Path(discfmt.__file__).resolve(), 'sunshine-pc/tools/discfmt.py')
    rows.extend(input_rows)
    owner_counts = collections.Counter(int(identity.split(':')[1]) for _, identity in blue_keys)
    summaries = {
        'metadata-not-runtime': 'All runtime statuses remain open; no gameplay, saves, completion or unassisted proof.',
        'blue-coin-bound': 'Observed identities=%d;owner_counts=%s;240 physical target is NOT inferred from trade capacity, 450 helper slots or 750 reserved bits.' % (len(blue_keys), ','.join('%d:%d' % item for item in sorted(owner_counts.items()))),
        'conditions-open': 'Occurrence availability means scene-load presence only; exact script path feasibility, interaction conditions, normal progression and missing-resource intent remain unresolved.',
        'stage-order': '61 raw-stage owners;ordered scenarios=%d;archive names are resource references, never episode identity;selected episodes are zero-based.' % len(scenarios),
        'scene-parser-scope': 'Scene placements decoded from source-defined group prefixes and object inheritance/load chains; tables.bin contains stage/camera descriptors, not instantiated placements (MarDirectorSetupObjects.cpp:281-310).',
        'source-shine-partitions': '56 course episodes;14 course extras;7 course100 slots;24 trade rewards;2 airport accounting;1 Dolpic extra-zero;1 ending;15 scene-bound route classifications remain open.',
        'unresolved-records': 'records=%d;includes absent serialized enemy drop/graph fields, dynamic event assignments and any remaining payload/parser errors.' % len(issues),
        'unique-scene-object-count': 'unique present archives=%d;nodes=%d' % (sum((ROOT / ('data/scene/' + a[:-4] + '.szs')).is_file() or (ROOT / ('data/scene/' + a)).is_file() for a in by_archive), sum(archive_object_counts.values())),
    }
    rows.extend(row('scope', name, classification='acceptance-open', metadata_status='bounded', condition=value) for name, value in summaries.items())
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, COLUMNS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    print('Wrote ' + OUTPUT.relative_to(REPO).as_posix())
    print('rows=' + str(len(rows)) + ';kinds=' + str(dict(collections.Counter(item['kind'] for item in rows))))
    print(summaries['blue-coin-bound'])
    print('Shine classifications=' + str(dict(collections.Counter(item['classification'] for item in rows if item['kind'] == 'shine'))))
    print('Unresolved reasons=' + str(dict(collections.Counter(item['condition'] for item in issues))))
    print('All runtime/full-content acceptance remains OPEN.')


def self_check():
    def string(value):
        return struct.pack('>H', len(value)) + value

    def node(typ, payload, name=b'test'):
        header = struct.pack('>H', key(typ)) + string(typ) + struct.pack('>H', key(name)) + string(name)
        return struct.pack('>I', 4 + len(header) + len(payload)) + header + payload

    def group(typ, children):
        return node(typ, struct.pack('>I', len(children)) + b''.join(children))

    archives = [node(b'ScenarioArchiveName', string(value)) for value in (b'pinnaBeach4.arc', b'pinnaBeach4.arc', b'pinnaBeach2.arc')]
    fixture = group(b'NameRefGrp', [group(b'ScenarioArchiveNamesInStage', [group(b'ScenarioArchiveNameTable', archives)])])
    assert [(raw, index, archive) for raw, index, archive, _ in stage_scenarios(fixture)] == [(0, 0, 'pinnaBeach4.arc'), (0, 1, 'pinnaBeach4.arc'), (0, 2, 'pinnaBeach2.arc')]
    malformed = [b'\0\0\0\3' + fixture[4:], struct.pack('>I', len(fixture) + 1) + fixture[4:], fixture[:6] + b'\xff\xff' + fixture[8:], fixture[:-1]]
    for data in malformed:
        try:
            stage_scenarios(data)
        except DecodeError:
            pass
        else:
            raise AssertionError('malformed node accepted')
    assert shine_event(-1) == 120 and shine_event(119) == 119
    identities = [blue_identity(owner, event) for owner, event in ((0, 3), (1, 3), (2, 3), (2, 3))]
    assert len(set(identities)) == 2 and identities[0] == (1, 3)
    for event in (-1, 50, 120):
        try:
            blue_identity(2, event)
        except DecodeError:
            pass
        else:
            raise AssertionError('invalid event aliased')
    actor = (struct.pack('>9f', *range(1, 10)) + string(b'fixture-character')
             + struct.pack('>I', 2) + struct.pack('>I', 0) + string(b'light-A')
             + struct.pack('>I', 1) + string(b'light-long-name-B'))
    scene = group(b'GroupObj', [
        node(b'CoinBlue', actor + string(b'coin_blue') + struct.pack('>i', 17), b'blue-direct'),
        node(b'Shine', actor + string(b'shine') + string(b'normal') + struct.pack('>ii', 3, 119), b'shine-valid'),
        node(b'Shine', actor + string(b'shine') + string(b'hidden') + struct.pack('>ii', -1, 112), b'shine-sentinel'),
        node(b'HideObj', actor + string(b'HideObj') + struct.pack('>iffi', 23, 1.0, 2.0, 100), b'blue-hidden'),
        node(b'AnimalBird', actor + string(b'bird-manager') + string(b'graph') + struct.pack('>i', 29), b'blue-bird'),
    ])
    found, calls, issues, _ = collect_archive({'/map/scene.bin': scene}, Sources())
    assert not calls and not issues
    assert [(item['kind'], item['event']) for item in found] == [
        ('blue', 17), ('shine', 3), ('shine', 120), ('blue', 23), ('blue', 29)]
    rewards, sentinels = set(), []
    for item in found:
        try:
            rewards.add(identity_name(item['kind'], 2, item['event']))
        except DecodeError:
            sentinels.append((item['kind'], item['event']))
    assert rewards == {'blue:2:17', 'shine:003', 'blue:2:23', 'blue:2:29'}
    assert sentinels == [('shine', 120)]
    text = b'\x00' + struct.pack('>I', 0x10056) + b'\x1d' + struct.pack('>II', 0, 1) + b'\x27'
    data_offset = 28 + len(text)
    symbol = struct.pack('>IIIII', 0, 0, 0, 0, 0)
    script = b'SPCB' + struct.pack('>6I', 28, data_offset, 0, data_offset, 1, 0) + text + symbol + b'getSystemFlag\0'
    assert spc_calls(script) == [(5, 0, 'getSystemFlag', [0x10056])]
    padded = b'SPCB' + struct.pack('>6I', 28, data_offset + 3, 0, data_offset + 3, 1, 0) + text + b'\0\0\0' + symbol + b'getSystemFlag\0'
    assert spc_calls(padded) == spc_calls(script)
    try:
        spc_calls(script[:-1])
    except DecodeError:
        pass
    else:
        raise AssertionError('unterminated symbol accepted')
    names = struct.pack('>HHHHHH', 2, 0xffff, 0, 12, 0, 22) + b'bluecoin0\0bluecoin1\0'
    joint = b'JNT1' + struct.pack('>IHHIII', 24 + len(names), 2, 0xffff, 0, 0, 24) + names
    model = b'J3D2bmd3' + struct.pack('>II', 32 + len(joint), 1) + bytes(16) + joint
    assert joint_blue_indices(model) == [0]
    try:
        joint_blue_indices(model[:-1] + b'X')
    except DecodeError:
        pass
    else:
        raise AssertionError('unterminated joint name accepted')
    print('PASS: ordered repeated archives; malformed sizes/string boundaries; serialized direct/generated Blue+Shine collector identities; sentinel120 excluded from rewards; owner/event deduplication; SPC calls/padding; joint generator indices/string boundaries')


if __name__ == '__main__':
    if sys.argv[1:] == ['--self-check']:
        self_check()
    elif sys.argv[1:]:
        raise SystemExit('usage: content_inventory.py [--self-check]')
    else:
        inventory()
