@echo off
rem Headless checks for the JSON reader, cheat registry, mod manifest and game-code host conversions (no game, no Aurora).
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul
set "PATH=C:\llvm-dl\t\clang+llvm-23.1.3-x86_64-pc-windows-msvc\bin;%PATH%"
if not exist build mkdir build
rem game code under TARGET_PC (sms headers stay out of the port sources): stream reads, float conversions
clang-cl /nologo /std:c++20 /O1 /EHsc /w /DTARGET_PC /I..\sms\include /Iextern\aurora\include /c tests\sms_host_checks.cpp ..\sms\src\JSystem\JSupport\JSUInputStream.cpp ..\sms\src\JSystem\JSupport\JSUMemoryStream.cpp /Fo:build\ || exit /b 1
clang-cl /nologo /std:c++20 /O1 /EHsc /Isrc tests\port_tests.cpp src\json\json_lite.cpp src\cheats\cheats_core.cpp src\mods\mod_manifest.cpp build\sms_host_checks.obj build\JSUInputStream.obj build\JSUMemoryStream.obj /Fe:build\port_tests.exe /Fo:build\ || exit /b 1
build\port_tests.exe
