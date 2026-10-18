"""Sweep verdict regressions without launching the game: python tools/o2_sweep_test.py."""
from contextlib import redirect_stdout
import io
from pathlib import Path
import runpy
import subprocess
import unittest
from unittest.mock import mock_open, patch


class SweepVerdictTest(unittest.TestCase):
    def test_exit_completion_and_crash_evidence_all_count(self):
        cases = [
            (0, "done at tick=12000", b"", "PASS"),
            (0, "done at tick=12000", None, "PASS"),
            (3, "done at tick=12000", None, "FAIL"),
            (0, "TIMEOUT hold-until", None, "FAIL"),
            (0, "done at tick=12000", b"x", "FAIL"),
            (3, "TIMEOUT hold-until", b"", "FAIL"),
        ]
        for code, log, crash, expected in cases:
            with self.subTest(code=code, log=log, crash=crash):
                output = io.StringIO()
                with (
                    patch("sys.argv", ["o2_sweep.py", "test", "gatekeeper"]),
                    patch("subprocess.run", return_value=subprocess.CompletedProcess([], code)),
                    patch("os.path.exists", side_effect=lambda p: str(p).endswith("run.log") or crash is not None),
                    patch("os.path.getsize", return_value=len(crash or b"")),
                    patch("builtins.open", mock_open(read_data=log)),
                    redirect_stdout(output),
                ):
                    runpy.run_path(str(Path(__file__).with_name("o2_sweep.py")), run_name="__main__")
                self.assertTrue(output.getvalue().startswith(expected + " pc_gatekeeper_unassisted"), output.getvalue())


if __name__ == "__main__":
    unittest.main()
