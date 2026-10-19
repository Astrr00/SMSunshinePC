"""Native guard parser smoke checks: python tools/devscript_guard_test.py (needs Strict and the local disc)."""
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parent.parent
directory = root / "build" / "DevscriptGuardTests"
directory.mkdir(exist_ok=True)
tag = time.time_ns()
cases = [
    ("bell_limits", "guard radius=0 until bellleft=0 bellleft=1000\nend\n", 0, "done at tick"),
    ("bell_invalid", "guard radius=0 until bellleft=1001\nend\n", 2, "unexpected token in guard"),
    ("bell_negative", "guard radius=0 until bellleft=-1\nend\n", 2, "unexpected token in guard"),
    ("bell_zero_outside_scene", "waitfor bellleft=0 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("bell_full_outside_scene", "waitfor bellleft=1000 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("coaster_limits", "guard radius=0 until coasterrail>=0 coasterrail>=32767 coasterrail<=0 coasterrail<=32767 coasterloaded=0 coasterloaded=1 tinkoopa=0 tinkoopa=1 tinkoopa=2 tinkoopa=3 tinkoopa=4\nend\n", 0, "done at tick"),
    ("coaster_rail_ge_negative", "guard radius=0 until coasterrail>=-1\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_le_negative", "guard radius=0 until coasterrail<=-1\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_ge_overflow", "guard radius=0 until coasterrail>=32768\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_le_overflow", "guard radius=0 until coasterrail<=32768\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_ge_fraction", "guard radius=0 until coasterrail>=1.0\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_le_fraction", "guard radius=0 until coasterrail<=1.0\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_nan", "guard radius=0 until coasterrail>=nan\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_trailing", "guard radius=0 until coasterrail<=32767x\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_ge_empty", "guard radius=0 until coasterrail>=\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_le_empty", "guard radius=0 until coasterrail<=\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_equal", "guard radius=0 until coasterrail=0\nend\n", 2, "unexpected token in guard"),
    ("coaster_loaded_negative", "guard radius=0 until coasterloaded=-1\nend\n", 2, "unexpected token in guard"),
    ("coaster_loaded_invalid", "guard radius=0 until coasterloaded=2\nend\n", 2, "unexpected token in guard"),
    ("coaster_loaded_fraction", "guard radius=0 until coasterloaded=1.0\nend\n", 2, "unexpected token in guard"),
    ("coaster_loaded_trailing", "guard radius=0 until coasterloaded=0x\nend\n", 2, "unexpected token in guard"),
    ("coaster_loaded_empty", "guard radius=0 until coasterloaded=\nend\n", 2, "unexpected token in guard"),
    ("tinkoopa_negative", "guard radius=0 until tinkoopa=-1\nend\n", 2, "unexpected token in guard"),
    ("tinkoopa_invalid", "guard radius=0 until tinkoopa=5\nend\n", 2, "unexpected token in guard"),
    ("tinkoopa_fraction", "guard radius=0 until tinkoopa=1.0\nend\n", 2, "unexpected token in guard"),
    ("tinkoopa_trailing", "guard radius=0 until tinkoopa=3x\nend\n", 2, "unexpected token in guard"),
    ("tinkoopa_empty", "guard radius=0 until tinkoopa=\nend\n", 2, "unexpected token in guard"),
    ("coaster_rail_zero_outside_scene", "waitfor coasterrail>=0 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("coaster_rail_max_outside_scene", "waitfor coasterrail<=32767 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("coaster_loaded_zero_outside_scene", "waitfor coasterloaded=0 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("tinkoopa_zero_outside_scene", "waitfor tinkoopa=0 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("plungelo_wet", "guard radius=0 until plungelowet=0 plungelowet=1\nend\n", 0, "done at tick"),
    ("plungelo_wet_invalid", "guard radius=0 until plungelowet=2\nend\n", 2, "unexpected token in guard"),
    ("plungelo_wet_fraction", "guard radius=0 until plungelowet=1.0\nend\n", 2, "unexpected token in guard"),
    ("plungelo_wet_outside", "waitfor plungelowet=1 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("mirror_limits", "guard radius=0 until mirrorSleft=1 mirrorMleft=2 mirrorLleft=3 plungeloedge=0 plungeloedge>=4\nend\n", 0, "done at tick"),
    ("mirror_outside_scene", "waitfor mirrorSleft=0 timeout=1\nend\n", 3, "TIMEOUT waitfor"),
    ("mirror_s_invalid", "guard radius=0 until mirrorSleft=2\nend\n", 2, "unexpected token in guard"),
    ("mirror_m_invalid", "guard radius=0 until mirrorMleft=3\nend\n", 2, "unexpected token in guard"),
    ("mirror_l_invalid", "guard radius=0 until mirrorLleft=4\nend\n", 2, "unexpected token in guard"),
    ("mirror_fraction", "guard radius=0 until mirrorSleft=1.0\nend\n", 2, "unexpected token in guard"),
    ("plungelo_invalid", "guard radius=0 until plungeloedge=5\nend\n", 2, "unexpected token in guard"),
    ("plungelo_negative", "guard radius=0 until plungeloedge>=-1\nend\n", 2, "unexpected token in guard"),
    ("plungelo_fraction", "guard radius=0 until plungeloedge>=1.0\nend\n", 2, "unexpected token in guard"),
    ("gesso_pull", "guard radius=0 until gessopull=0 gessopull=1\nend\n", 0, "done at tick"),
    ("gesso_pull_invalid", "guard radius=0 until gessopull=2\nend\n", 2, "unexpected token in guard"),
    ("gesso_pull_fraction", "guard radius=0 until gessopull=1.0\nend\n", 2, "unexpected token in guard"),
    ("cargo", "guard radius=0 until cranestate=2 cargoy<=-600\nend\n", 0, "done at tick"),
    ("cargo_state_invalid", "guard radius=0 until cranestate=4\nend\n", 2, "unexpected token in guard"),
    ("cargo_height_invalid", "guard radius=0 until cargoy<=nan\nend\n", 2, "unexpected token in guard"),
    ("cargo_height_overflow", "guard radius=0 until cargoy<=32768\nend\n", 2, "unexpected token in guard"),
    ("gesso", "guard radius=0 until gessohp=2 gessomode=6 held=0\nend\n", 0, "done at tick"),
    ("gesso_hp_invalid", "guard radius=0 until gessohp=256\nend\n", 2, "unexpected token in guard"),
    ("gesso_mode_invalid", "guard radius=0 until gessomode=8\nend\n", 2, "unexpected token in guard"),
    ("held_invalid", "guard radius=0 until held=2\nend\n", 2, "unexpected token in guard"),
    ("hop_parse", "goto finish if thp=0 max=1\nwalkto x=0 z=0 hop=24 timeout=1\nlabel finish\nend\n", 0, "done at tick"),
    ("hop_zero", "walkto x=0 z=0 hop=0 timeout=1\n", 2, "hop needs an integer >= 3"),
    ("hop_fraction", "walkto x=0 z=0 hop=3.5 timeout=1\n", 2, "hop needs an integer >= 3"),
    ("hop_nan", "walkto x=0 z=0 hop=nan timeout=1\n", 2, "hop needs an integer >= 3"),
    ("hop_overflow", "walkto x=0 z=0 hop=2147483648 timeout=1\n", 2, "hop needs an integer >= 3"),
    ("stationary", "guard radius=0 stationary=1 resume=aim until bck=5\nlabel aim\nend\n", 0, "guard radius=0 stationary=1"),
    ("stationary_invalid", "guard radius=0 stationary=2\nend\n", 2, "stationary needs 0 or 1"),
    ("pitch", "guard radius=0 until pitch>=-5300 pitch<=-4500\nend\n", 0, "done at tick"),
    ("pitch_invalid", "guard radius=0 until pitch<=nan\nend\n", 2, "unexpected token in guard"),
    ("legacy", "guard radius=0\nend\n", 0, "done at tick"),
    ("if_taken", "goto finish if thp=0 max=1\nwaitfor stage=999 timeout=1\nlabel finish\nend\n", 0, "goto finish retry=1"),
    ("if_fallthrough", "goto finish if thp=2 max=1\nend\nlabel finish\nwaitfor stage=999 timeout=1\n", 0, "goto finish passed"),
    ("unless_taken", "goto finish unless thp=2 max=1\nwaitfor stage=999 timeout=1\nlabel finish\nend\n", 0, "goto finish retry=1"),
    ("resume", "guard radius=0 resume=aim until bck=5\nlabel aim\nend\n", 0, "done at tick"),
    ("observations", "guard radius=0 until gkawake=1 squat=1 talkbusy=0 bckframe>=60 bckframe<=120\nend\n", 0, "done at tick"),
    ("unknown", "guard radius=0 resume=missing\nend\n", 2, "unknown label"),
    ("invalid", "guard radius=0 resume=\nend\n", 2, "unexpected token in guard"),
]
for case, script, expected_code, expected_log in cases:
    path = directory / f"{case}.dev"
    path.write_text(script)
    name = f"guard_parser_{tag}_{case}"
    result = subprocess.run(
        [sys.executable, "tools/route.py", "Strict", str(path), "--name", name,
         "--seconds", "15", "SUNSHINE_ALLOW_CPU_ADAPTER=1"],
        cwd=root, capture_output=True, text=True,
    )
    log = (root / "build" / "Route" / name / "run.log").read_text(errors="replace")
    assert result.returncode == expected_code, (case, result.returncode, result.stdout, log[-2000:])
    assert expected_log in log, (case, log[-2000:])
    print(f"PASS guard parser {case}", flush=True)
