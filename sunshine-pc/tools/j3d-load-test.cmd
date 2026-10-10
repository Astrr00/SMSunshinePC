@echo off
rem Builds tools\j3d-load-test.cpp against sms_game.lib + Aurora's GD/MTX/OS libs of a private build tree (build-alt.cmd <name>)
rem and runs it over build\j3d-corpus. Usage: j3d-load-test.cmd <build name> [max models] [max bck]
rem Needs LLVM_BIN (folder with clang-cl.exe). Links with /FORCE:UNRESOLVED like the game (an unresolved symbol faults when called).
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: j3d-load-test.cmd ^<build name^> [max models] [max bck] & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "BLD=%PC%\build\%~1"
set "OUT=%PC%\build\j3d-test"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 /Zi /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1"
set INC=/FI"%PC%\include\pc_compat.h" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include" /I"%SMS%\src"
clang-cl %CF% %INC% "%~dp0j3d-load-test.cpp" "%PC%\src\crash.cpp" "%BLD%\sms_game.lib" "%BLD%\extern\aurora\aurora_gd.lib" "%BLD%\extern\aurora\aurora_mtx.lib" "%BLD%\extern\aurora\aurora_os.lib" ^
  /Fo"%OUT%\\" /Fe"%OUT%\j3d-load-test.exe" /link /DEBUG /FORCE:UNRESOLVED || exit /b 1
"%OUT%\j3d-load-test.exe" "%PC%\build\j3d-corpus" %2 %3
