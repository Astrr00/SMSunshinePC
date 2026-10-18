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
