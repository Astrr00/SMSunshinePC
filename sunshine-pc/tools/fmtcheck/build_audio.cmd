@echo off
rem Builds tools\fmtcheck\audio_check.cpp (see audio_check.py) against sms_game.lib of a private build tree into
rem tools\fmtcheck\out\audio_check.exe. The harness provides the few OS services the readers touch; everything else
rem stays unresolved (/FORCE:UNRESOLVED, like the game) and must not be called by the checked paths.
rem Usage: build_audio.cmd <build name>      (the tree made by tools\build-alt.cmd <build name>)
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: build_audio.cmd ^<build name^> & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0..\.."
set "SMS=%~dp0..\..\..\sms"
set "BLD=%PC%\build\%~1"
set "OUT=%~dp0out"
if not exist "%OUT%" mkdir "%OUT%"
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 /Zi ^
  /clang:-fno-strict-return /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" /I"%SMS%\src" "%~dp0audio_check.cpp" "%PC%\src\crash.cpp" "%BLD%\sms_game.lib" ^
  /Fo"%OUT%\\" /Fe"%OUT%\audio_check.exe" /link /DEBUG /FORCE:UNRESOLVED /ignore:4088 || exit /b 1
echo built %OUT%\audio_check.exe
