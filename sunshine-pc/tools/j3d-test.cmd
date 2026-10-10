@echo off
rem Compile and run a standalone host test of header-only J3D format code with the game flags (clang-cl, TARGET_PC).
rem Usage: j3d-test.cmd <test.cpp> [args for the test]       e.g. j3d-test.cmd tools\j3d-swap-test.cpp build\j3d-corpus
rem Needs LLVM_BIN (folder with clang-cl.exe). Output goes to build\j3d-test\<name>.exe; nothing else is touched.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "SRC=%~f1"
set "OUT=%PC%\build\j3d-test"
if not exist "%OUT%" mkdir "%OUT%"
for %%F in ("%SRC%") do set "NAME=%%~nF"
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 /O1 -Wno-everything -Wno-multichar -Wno-trigraphs ^
  /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" "%SRC%" /Fe"%OUT%\%NAME%.exe" /Fo"%OUT%\%NAME%.obj" || exit /b 1
shift
"%OUT%\%NAME%.exe" %1 %2 %3 %4 %5 %6 %7 %8 %9
