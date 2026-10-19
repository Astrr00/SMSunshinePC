"""Real native stdout/crash persistence and collector tests; run tools/logging_test.cmd first."""
import hashlib
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / 'build/logging-test/logging_smoke.exe'


class LoggingTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="sunshine-log-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "Release [test] O'Brien 日本語"
        self.root.mkdir()
        self.env = dict(os.environ, APPDATA=str(self.root / 'appdata'))
        self.logs = self.root / 'appdata/Sunshine/logs'

    def run_native(self, *args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL):
        return subprocess.run([str(EXE), *args], env=self.env, cwd=self.root, stdout=stdout,
                              stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW, timeout=20)

    def sessions(self):
        return sorted(p for p in self.logs.glob('session-*.log')
                      if re.fullmatch(r'session-\d{8}-\d{6}-\d{3}-\d+\.log', p.name))

    def test_normal_crash_and_abort_are_saved(self):
        for option in ('', '--crash', '--abort'):
            with self.subTest(option=option):
                result = self.run_native(*([option] if option else []))
                self.assertEqual(result.returncode == 0, not option)
                log = self.sessions()[-1]
                text = log.read_text(errors='replace')
                for marker in ('[session]', '[build]', 'stdout survives', 'stderr survives', 'Win32 stderr survives'):
                    self.assertIn(marker, text)
                crash = log.with_suffix('.crash.log').read_text(errors='replace')
                if option == '--crash':
                    self.assertIn('fatal exception 0xc0000005', crash)
                    self.assertIn('fatal exception 0xc0000005', text)
                elif option == '--abort':
                    self.assertIn('fatal diagnostic before abort', text)
                else:
                    self.assertIn('normal exit', text)
                    self.assertEqual(crash, '')

    def test_explicit_redirection_and_unwritable_directory(self):
        result = self.run_native(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'stdout survives', result.stdout)
        self.assertIn(b'stderr survives', result.stderr)
        self.assertFalse(self.logs.exists())
        self.assertTrue((self.root / 'crash.log').exists())
        result = self.run_native(stdout=subprocess.PIPE)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'stdout survives', result.stdout)
        self.assertIn('stderr survives', self.sessions()[-1].read_text())
        blocked = self.root / 'blocked'
        blocked.write_text('must survive')
        self.env['APPDATA'] = str(blocked)
        result = self.run_native(stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'Automatic log capture unavailable', result.stderr)
        self.assertEqual(blocked.read_text(), 'must survive')

    def test_retention_preserves_active_and_unrelated_files(self):
        self.logs.mkdir(parents=True)
        unrelated = self.logs / 'session-personal-日本語.log'
        unrelated.write_text('keep')
        for i in range(7):
            log = self.logs / f'session-20000101-000000-000-{i}.log'
            log.write_text('old')
            log.with_suffix('.crash.log').write_text('old crash')
        child = subprocess.Popen([str(EXE), '--hold'], env=self.env, cwd=self.root,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        self.addCleanup(lambda: child.kill() if child.poll() is None else None)
        deadline = time.monotonic() + 3
        while len(self.sessions()) != 5 and time.monotonic() < deadline:
            time.sleep(.02)
        self.assertEqual(len(self.sessions()), 5)
        active = self.sessions()[-1]
        for _ in range(6):
            self.assertEqual(self.run_native().returncode, 0)
        self.assertIsNone(child.poll())
        self.assertIn('stdout survives', active.read_text())
        self.assertEqual(child.wait(timeout=10), 0)
        self.assertIn('normal exit', active.read_text())
        self.assertEqual(unrelated.read_text(), 'keep')
        self.assertEqual(self.run_native().returncode, 0)
        self.assertEqual(len(self.sessions()), 5)
        self.assertEqual(len(list(self.logs.glob('*.crash.log'))), 5)

    def test_collector_excludes_saves_and_game_data(self):
        self.assertEqual(self.run_native().returncode, 0)
        saves = self.logs.parent / 'saves'
        saves.mkdir()
        (saves / 'secret.gci').write_bytes(b'private save')
        (self.root / 'game.rvz').write_bytes(b'private image')
        (self.logs / 'unrelated.log').write_text('private unrelated log')
        (self.logs.parent / 'settings.ini').write_text('[video]\nfullscreen=0\n')
        (self.root / 'sunshine.exe').write_bytes(b'test package executable')
        collector = ROOT / 'tools/collect_logs.ps1'
        # Keep a writer open, as when collecting a still-running or hung game's log.
        with self.sessions()[-1].open('ab') as writer:
            writer.write(b'[test] live append\n')
            writer.flush()
            result = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                     str(collector), '-PackageDirectory', str(self.root)], env=self.env,
                                    cwd=self.root, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        with zipfile.ZipFile(self.logs.parent / 'sunshine-logs.zip') as archive:
            names = archive.namelist()
            self.assertEqual(len(names), 4, names)
            self.assertIn('settings.ini', names)
            self.assertIn(b'live append', archive.read(self.sessions()[-1].name))
            hardware = archive.read('hardware.txt').decode('utf-8-sig')
            self.assertIn(hashlib.sha256(b'test package executable').hexdigest().upper(), hardware)
            self.assertFalse(any(name.endswith(('.gci', '.rvz')) or name == 'unrelated.log' for name in names))
        self.assertEqual((saves / 'secret.gci').read_bytes(), b'private save')

    def test_missing_appdata_uses_executable_directory(self):
        self.env.pop('APPDATA', None)
        executable = self.root / 'sunshine.exe'
        shutil.copy2(EXE, executable)
        elsewhere = self.root / 'unrelated working directory'
        elsewhere.mkdir()
        result = subprocess.run([str(executable)], env=self.env, cwd=elsewhere,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                creationflags=subprocess.CREATE_NO_WINDOW, timeout=20)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(len(list((self.root / 'logs').glob('*.log'))), 2)
        self.assertFalse((elsewhere / 'logs').exists())


if __name__ == '__main__':
    unittest.main()
