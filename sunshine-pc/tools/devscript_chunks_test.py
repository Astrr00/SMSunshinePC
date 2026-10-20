"""Check optional controller chunks after script completion (local Strict/disc required)."""
from pathlib import Path
import re
import subprocess
import sys
import time


root = Path(__file__).resolve().parent.parent
tag = f"chunks_{time.time_ns()}"
directory = root / "build" / tag
directory.mkdir()
initial = directory / "initial.dev"
initial.write_text("wait frames=30\nend\n", encoding="utf-8")
for number, text in [(1, "wait frames=1\nend\n"), (2, "end\n"), (4, "end\n")]:
    (directory / f"{number}.dev").write_text(text, encoding="utf-8")

for enabled in (False, True):
    name = f"{tag}_{int(enabled)}"
    result = subprocess.run([
        sys.executable, str(root / "tools" / "route.py"), "Strict", str(initial),
        "--name", name, "--seconds", "6" if enabled else "3",
        "SUNSHINE_DEV_EXIT_ON_END=0",
        f"SUNSHINE_DEV_SCRIPT_CHUNKS={directory if enabled else ''}",
    ], cwd=root, capture_output=True, text=True)
    run = root / "build" / "Route" / name
    log = (run / "run.log").read_text(encoding="utf-8", errors="replace")
    chunks = re.findall(r"\[devscript\] chunk=(\d+) tick=(\d+)", log)
    expected = ["1", "2"] if enabled else []
    assert result.returncode == 124, (name, result.returncode, result.stdout, result.stderr)
    assert [number for number, _ in chunks] == expected, (name, chunks)
    assert not chunks or int(chunks[0][1]) >= 60, chunks
    assert log.count("[devscript] done at tick=") == (3 if enabled else 1), name
    assert (run / "crash.log").stat().st_size == 0, name
    print(f"PASS: chunks {'ordered and idle-only' if enabled else 'disabled by default'}", flush=True)
