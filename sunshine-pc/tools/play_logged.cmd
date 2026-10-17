@echo off
rem Starts the game with logging to sunshine.log. Usage: play_logged.cmd ["path\to\your\image.rvz"]  (default: game.rvz next to this file).
rem Afterwards run collect_logs.cmd and attach sunshine-logs.zip to the bug report.
cd /d "%~dp0"
set "IMG=%~1"
if "%IMG%"=="" set "IMG=%~dp0game.rvz"
if not exist "%IMG%" (echo No disc image found: "%IMG%" & echo Usage: play_logged.cmd "path\to\your\image.rvz" & pause & exit /b 1)
sunshine.exe --dvd "%IMG%" > sunshine.log 2>&1
echo Game closed. Now run collect_logs.cmd.
pause
