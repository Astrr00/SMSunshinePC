# Persistent release diagnostics (2026-10-19)

Normal launches now capture stdout, stderr and native standard-error writes in
`%APPDATA%\Sunshine\logs`. Session files record UTC time, process/build metadata,
existing engine diagnostics and stage/scenario changes. Fatal exception reports
use a matching `.crash.log`. An empty crash report is normal.

`collect_logs.cmd` exports the five most recent sessions, matching crash reports,
settings and hardware/driver information to `%APPDATA%\Sunshine\sunshine-logs.zip`.
It also records the collected package executable's SHA256. It works with an open
session log, requires only Windows PowerShell 5.1, excludes saves and game images,
and uploads nothing. See [the tester guide](TESTING.md) for fallbacks and limits.

## Verification

- Strict: `STRICT: 0 unresolved symbols`; `tools\port_tests.cmd`: 68 checks, zero failures.
- `tools\logging_test.cmd`: five tests pass against the actual logger/crash handler
  compiled with `/O2`. Coverage includes normal output, Win32 stderr, fatal SEH,
  abort diagnostics, explicit file/pipe capture, unavailable storage, Unicode paths,
  retention with a live session, executable-directory fallback and ZIP export
  while a writer is open. `python tools/package_test.py`: pass.
- Actual game: normal controller-only Pinna 3 cold-load route, automatic logging
  with no harness stream capture, exit 0 in 47.05 seconds. Thirteen earned Shines
  remain present; the card hash is unchanged. The session records startup/GPU and
  stage/scenario diagnostics; the paired crash file is empty.
- Private `package.py` validation archive `0.1.0.20261019.2`: 21,006,139 bytes,
  packaged collector exits 0 and exports the actual game session. Required helpers
  are included; no PDB, GCI, ISO or RVZ is shipped. Archive SHA256:
  `e5c861531df2a1aad1b8d59c2e240e0581c6760c3c33d1a3bda60f66c13dd98c`.

Tested and packaged executable SHA256:
`0b4ba455453051d8bfb1e677658e53d768ee7421f9b56849378873a04909b291`.
Cold-loaded card SHA256:
`99f868abf960e0eba6d68b0e6fc4248947ba075f8d5b75dbaface967a96545e4`.
Local evidence is under `build/ReleaseLogs/`, with build/check logs named
`build/release-logs-*.log` and focused results in `build/logging-focused-runtime.log`.

This is a private RelWithDebInfo validation package, not the final Release build
or a new external Clean-PC acceptance. Logging starts at `main`; loader failures
can precede it. Retention bounds session count, not live file size. Publication
and the remaining [release gates](release-checklist.md) remain pending.
