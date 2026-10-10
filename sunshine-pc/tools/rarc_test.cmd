@echo off
rem Builds tools\rarc_test.cpp against sms_game.lib of a private build tree (build-alt.cmd <name>) into %TEMP%.
rem Usage: rarc_test.cmd <build name>      then: python tools\verify-rarc.py <build name>
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH. Links with /FORCE:UNRESOLVED like the game.
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: rarc_test.cmd ^<build name^> & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "BLD=%PC%\build\%~1"
set "OUT=%TEMP%\sunshine_rarc_test"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1"
set INC=/FI"%PC%\include\pc_compat.h" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include" /I"%SMS%\src"
rem ripper_test.cpp includes one of the decoders' sources per object (see its header comment)
for %%R in (1 2 3) do clang-cl %CF% %INC% /c /DRIPPER=%%R "%~dp0ripper_test.cpp" /Fo"%OUT%\ripper%%R.obj" || exit /b 1
clang-cl %CF% %INC% "%~dp0rarc_test.cpp" "%OUT%\ripper1.obj" "%OUT%\ripper2.obj" "%OUT%\ripper3.obj" "%BLD%\sms_game.lib" ^
  /Fo"%OUT%\\" /Fe"%OUT%\rarc_test.exe" /link /FORCE:UNRESOLVED || exit /b 1
echo built %OUT%\rarc_test.exe
