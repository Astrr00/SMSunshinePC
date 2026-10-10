@echo off
rem Builds tools\spc_bind_test.cpp against sms_game.lib of a private build tree (build-alt.cmd <name>) into %TEMP%.
rem Usage: spc_bind_test.cmd <build name>      then: python tools\spc_bind_test.py <build name>
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH. Links with /FORCE:UNRESOLVED like the game.
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: spc_bind_test.cmd ^<build name^> & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "BLD=%PC%\build\%~1"
set "OUT=%TEMP%\sunshine_spc_bind_test"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1"
set INC=/FI"%PC%\include\pc_compat.h" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include" /I"%SMS%\src"
clang-cl %CF% %INC% /Zi "%~dp0spc_bind_test.cpp" "%BLD%\sms_game.lib" "%BLD%\extern\aurora\aurora_mtx.lib" "%BLD%\extern\aurora\aurora_os.lib" "%BLD%\extern\aurora\aurora_gx.lib" "%BLD%\extern\aurora\aurora_gd.lib" "%BLD%\extern\aurora\aurora_core.lib" "%BLD%\extern\aurora\aurora_dvd.lib" "%BLD%\extern\aurora\aurora_vi.lib" "%BLD%\extern\aurora\aurora_pad.lib" "%BLD%\extern\aurora\aurora_si.lib" "%BLD%\extern\aurora\aurora_card.lib" ^
  /Fo"%OUT%\\" /Fe"%OUT%\spc_bind_test.exe" /link /DEBUG /FORCE:UNRESOLVED || exit /b 1
echo built %OUT%\spc_bind_test.exe
