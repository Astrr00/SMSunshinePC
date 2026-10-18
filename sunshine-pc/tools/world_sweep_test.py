"""World probe verdict checks without game data: python tools/world_sweep_test.py."""
from world_sweep import selects, verdict

assert selects(8, 1, set())
assert selects(8, 1, {(8,)})
assert selects(8, 1, {(8, 1)})
assert not selects(8, 0, {(8, 1)})
assert not selects(9, 1, {(8,)})


def snapshot(stage=3, scenario=0, point="1,2,3", demo=0, director=4):
    return f"[devscript] tick=10 stage={stage} scen={scenario} mario=({point}) camDemo={demo} dir.state={director}\n"


render = "API: D3D12\nscreenshot written: scene_ready.png\n"
good = render + snapshot() + snapshot(point="1,5,3") + snapshot(point="3,2,3") + "done at tick=100"
cases = [
    (good, 0, 0, "pass"),
    (good, 0, 1, "crash"),
    (good, 3, 0, "failed-exit"),
    (good, 124, 0, "timeout"),
    (good.replace("done at tick=100", ""), 0, 0, "incomplete"),
    (good.replace("API: D3D12", "API: Null"), 0, 0, "no-render-proof"),
    (good.replace("screenshot written: scene_ready.png", ""), 0, 0, "no-render-proof"),
    (good.replace("stage=3", "stage=4"), 0, 0, "wrong-scene"),
    (good.replace("camDemo=0", "camDemo=1"), 0, 0, "not-controllable"),
    (good.replace("dir.state=4", "dir.state=7"), 0, 0, "not-controllable"),
    (good.replace("1,5,3", "nan,5,3"), 0, 0, "invalid-position"),
    (good + "\n[status-mario3] health=0 floor=(0,0,0)", 0, 0, "death"),
    (render + snapshot() * 3 + "done at tick=100", 0, 0, "no-motion"),
]
for log, code, crash, expected in cases:
    assert verdict(log, code, crash, 3, 0) == expected, expected
print(f"PASS {len(cases)} world probe verdict checks")
