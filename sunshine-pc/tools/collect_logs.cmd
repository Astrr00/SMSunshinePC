@echo off
rem Collects logs for a bug report into sunshine-logs.zip next to this script's working folder. No saves, no disc image, no game data.
rem Run from the folder that holds sunshine.exe (crash.log and sunshine.log live there).
setlocal
set "OUT=%TEMP%\sunshine-logs-%RANDOM%"
mkdir "%OUT%" || exit /b 1
for %%F in (crash.log sunshine.log) do if exist "%%F" copy /y "%%F" "%OUT%" >nul
if exist "%APPDATA%\Sunshine\settings.ini" copy /y "%APPDATA%\Sunshine\settings.ini" "%OUT%" >nul
powershell -NoProfile -Command "'Windows: ' + [Environment]::OSVersion.VersionString; 'CPU: ' + (Get-CimInstance Win32_Processor).Name; Get-CimInstance Win32_VideoController | ForEach-Object { 'GPU: ' + $_.Name + ' driver ' + $_.DriverVersion }" > "%OUT%\hardware.txt" 2>&1
if exist sunshine.log findstr /C:"Device:" /C:"Backend:" sunshine.log >> "%OUT%\hardware.txt"
if exist sunshine-logs.zip del sunshine-logs.zip
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath 'sunshine-logs.zip'" || exit /b 1
rmdir /s /q "%OUT%"
echo ok: %CD%\sunshine-logs.zip - attach it to the GitHub issue.
