"""Reachability boundary checks without disc data: python tools/content_reach_test.py."""
from content_reach import decode, delfino_scenarios, pinna_scenarios, scenario_reachable
from content_inventory import DecodeError, spc_calls
import struct

# Real SPCB bytes exercise the call filter as well as destination classification.
text = (b'\x00' + struct.pack('>I', 44) + b'\x25'
        + b'\x1d' + struct.pack('>II', 0, 2) + b'\x27')
offset = 28 + len(text)
script = (b'SPCB' + struct.pack('>6I', 28, offset, 0, offset, 1, 0)
          + text + bytes(20) + b'setNextStage\0')
assert spc_calls(script) == []  # Inventory intentionally filters transitions.
assert spc_calls(script, {'setNextStage'}) == [(6, 0, 'setNextStage', [44, 0])]

assert decode(0xE05) == (13, 5)
assert decode(1) == (1, 255)
selector = {1: {0, 1, 2, 5, 6, 7, 8, 9}, 13: {0, 1, 2, 3, 4, 5}}
source = 'static int decideNextScenario(u8 stage) { return 0; return 2; }\nint TMarDirector::changeState()'
assert delfino_scenarios(source) == {0, 2}
assert pinna_scenarios('void TMarDirector::moveStage() { case 13: { u32 thing = 0; thing = 5; } case 0x3A:') == {0, 5}
try:
    delfino_scenarios(source.replace('return 2', 'return stage'))
except DecodeError:
    pass
else:
    raise AssertionError('dynamic selector accepted')
requests = {(1, 255), (13, 255), (13, 5), (13, 6), (13, 7), (2, 255)}
assert scenario_reachable(1, 3, requests, selector) is False
assert scenario_reachable(1, 4, requests, selector) is False
assert scenario_reachable(1, 9, requests, selector) is True
assert scenario_reachable(13, 8, requests, selector) is False
assert scenario_reachable(13, 5, requests, selector) is True
assert scenario_reachable(25, 0, requests, selector) is False
assert scenario_reachable(2, 0, requests, selector) is None
assert scenario_reachable(1, 3, requests | {(1, 3)}, selector) is True
print('PASS reachability decoding, literal selector, explicit/dynamic/absent destinations')
