@echo off
rem Builds build\transfer\SunshineTiming.zip for a timing run on another PC: sunshine.exe, its DLLs, assets, sjis, pipeline cache,
rem the owner's own disc image as game.rvz and run_timing.cmd. Contains game data: keep it private, never commit it (build\ is untracked).
rem Usage: make_timing_zip.cmd [build name, default Strict] [disc image, default ..\sms\orig\GMSJ01\Super Mario Sunshine (Japan).rvz]
set "PC=%~dp0.."
set "B=%~1"
if "%B%"=="" set "B=Strict"
set "DISC=%~2"
if "%DISC%"=="" set "DISC=%PC%\..\sms\orig\GMSJ01\Super Mario Sunshine (Japan).rvz"
if not exist "%PC%\build\%B%\sunshine.exe" (echo no build\%B%\sunshine.exe & exit /b 1)
if not exist "%DISC%" (echo no disc image & exit /b 1)
set "T=%PC%\build\transfer\SunshineTiming"
if exist "%PC%\build\transfer" rmdir /s /q "%PC%\build\transfer"
mkdir "%T%" || exit /b 1
for %%F in (sunshine.exe dxcompiler.dll dxil.dll libpng16.dll nod.dll SDL3.dll webgpu_dawn.dll zlib1.dll initial_pipeline_cache.db) do copy /y "%PC%\build\%B%\%%F" "%T%" >nul || exit /b 1
xcopy /e /i /q /y "%PC%\build\%B%\assets" "%T%\assets" >nul || exit /b 1
xcopy /e /i /q /y "%PC%\build\%B%\sjis" "%T%\sjis" >nul || exit /b 1
copy /y "%DISC%" "%T%\game.rvz" >nul || exit /b 1
copy /y "%~dp0run_timing.cmd" "%T%\run_timing.cmd" >nul || exit /b 1
copy /y "%~dp0run_profile.cmd" "%T%\run_profile.cmd" >nul || exit /b 1
copy /y "%PC%\docs\TESTING.md" "%T%\TESTING.md" >nul || exit /b 1
copy /y "%~dp0collect_logs.cmd" "%T%\collect_logs.cmd" >nul || exit /b 1
copy /y "%~dp0play_logged.cmd" "%T%\play_logged.cmd" >nul || exit /b 1
powershell -NoProfile -Command "Compress-Archive -Path '%T%' -DestinationPath '%PC%\build\transfer\SunshineTiming.zip' -CompressionLevel Fastest" || exit /b 1
echo ok: %PC%\build\transfer\SunshineTiming.zip
