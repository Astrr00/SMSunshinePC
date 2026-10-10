@echo off
rem Syntax-only check of one file with the exact flags the game library uses (clang-cl, C++20, TARGET_PC).
rem Usage: syntax.cmd [extra clang flags] <path to .cpp>   e.g. syntax.cmd -Wpointer-to-int-cast file.cpp
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
rem Safe to run in parallel with other syntax checks; it does not touch the build directory.
rem -Wno-everything silences all warnings by default; flags you pass after it re-enable single ones.
rem A stray double quote inside PATH (seen in some shells) makes vcvars64.bat fail with "\Microsoft was unexpected".
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
clang-cl /nologo /c /Zs /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs ^
  /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" %*
