@echo off
rem Profiling run (diagnosis only): dev-starts Delfino without movies, forces one replay per tick, prints the replay's
rem per-phase and per-draw-link cost and samples all threads for 20 s (from 40 s on). Ends by itself after 75 s; do not
rem touch the game. Send profile_gpu.log to Claude.
cd /d "%~dp0"
set SUNSHINE_ALLOW_CPU_ADAPTER=
set AURORA_SCREENSHOT_EVERY=
set SUNSHINE_DEV_START=dolpic/0
set SUNSHINE_DEV_SKIP_MOVIES=1
set SUNSHINE_DEV_REPLAY_PROFILE=1
set SUNSHINE_DEV_SAMPLE=40,20
set SUNSHINE_DEV_MAX_SECONDS=75
sunshine.exe --dvd "%~dp0game.rvz" > profile_gpu.log 2>&1
echo Done: send profile_gpu.log
pause
