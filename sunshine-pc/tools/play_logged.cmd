@echo off
rem Starts the game with automatic session logs. Optional argument: disc image path; otherwise open the launcher.
rem Afterwards run collect_logs.cmd and attach sunshine-logs.zip to the bug report.
cd /d "%~dp0"
set "IMG=%~1"
if "%IMG%"=="" (sunshine.exe) else (sunshine.exe --dvd "%IMG%")
echo Game closed. Now run collect_logs.cmd.
pause
