# Testing the Windows port

You need your own legally obtained Super Mario Sunshine (GMSJ01, Japanese) disc image. Nothing in this repository or in a
test package contains game data, and bug reports must not either.

## Run

1. Unpack the package into a folder you can write to.
2. Start it with logging, from that folder:

   ```
   sunshine.exe --dvd "C:\path\to\your\image.rvz" > sunshine.log 2>&1
   ```

3. Play normally (keyboard or gamepad). Pause/menu and quitting through the window's close button are part of the test.

## What to look for

Crashes, freezes, wrong or missing pictures, missing sound or music, input that does not respond, saves that do not
load back, speed that is not about 30 logic ticks per second, anything that differs from the original game.

## Report

1. Run `tools\collect_logs.cmd` from the package folder (or from `sunshine-pc\tools` in a checkout). It writes
   `sunshine-logs.zip` with `crash.log`, `sunshine.log`, `settings.ini` and `hardware.txt` (GPU, driver, Windows). It
   never includes save files or the disc image.
2. Open a GitHub issue with the **Bug report** template and attach the zip.

Tell us the stage and episode, and whether it happens every time.
