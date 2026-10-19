@echo off
rem Collect recent automatic logs and hardware/build information. No saves or game data; nothing is uploaded.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0collect_logs.ps1" -PackageDirectory "%~dp0."
set "RESULT=%ERRORLEVEL%"
pause
exit /b %RESULT%
