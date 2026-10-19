# Testing the Windows port

You need your own legally obtained Super Mario Sunshine (GMSJ01, Japanese) disc image. Nothing in this repository or in a
test package contains game data, and bug reports must not either.

## Run

1. Unpack the package and start `sunshine.exe` normally. Logging is automatic, including in Release builds.
2. Choose your disc image in the launcher, or use `sunshine.exe --dvd "C:\path\to\your\image.rvz"`.
   `play_logged.cmd` remains an optional helper; a special logging launcher is not required.
3. Play normally (keyboard or gamepad). Pause/menu and quitting through the window's close button are part of the test.

## What to look for

Crashes, freezes, wrong or missing pictures, missing sound or music, input that does not respond, saves that do not
load back, speed that is not about 30 logic ticks per second, anything that differs from the original game.

## Report

1. Run `collect_logs.cmd` from the package (or `sunshine-pc\tools` in a checkout). It writes
   `%APPDATA%\Sunshine\sunshine-logs.zip` with recent session/crash logs, `settings.ini` and `hardware.txt`
   (GPU, driver, Windows and the collected package executable's SHA-256, when present).
   It works with Windows PowerShell 5.1; Python and development tools are not needed.
   You can collect while the game is still running or unresponsive. It never includes saves or the disc image,
   and uploads nothing automatically. Logs/settings can contain local file paths; review them before posting publicly.
2. Open a GitHub issue with the **Bug report** template and attach the zip.

Tell us the stage and episode, and whether it happens every time.

## Automatic log files

`%APPDATA%\Sunshine\logs` contains a timestamp/PID-named `.log` and matching `.crash.log` for each normal start.
The session log records startup version/build information, normal output, warnings, fatal messages and gameplay
scene changes. The existing exception reporter records the fault and stack in the crash file when Windows lets it run.
An empty crash report is normal; some aborts leave only a fatal message in the session log.

The five newest sessions are retained; open files from another running instance are preserved and may temporarily
increase that count. Retention limits session count, not the size of a live log. Without `APPDATA`, logs and the
export archive fall back to the executable's directory. Logging starts at `main`; loader failures before that point
may produce no session log. PDBs stay out of public packages; matching private symbols can help interpret stack RVAs.

Explicit file/pipe redirection is preserved instead of replaced. For example,
`sunshine.exe --dvd "C:\path\to\your\image.rvz" > sunshine.log 2>&1` still writes the supplied capture and uses the
legacy working-directory `crash.log`. The collector also gathers those two legacy filenames from its current folder;
attach any differently named custom capture separately.
