@echo off
rem Offline check of the decomp's THP decoder on the host (no game, no Aurora, no cmake):
rem   thp_test.cpp  builds sms\src\dolphin\thp\THPDec.c + THPAudio.c (TARGET_PC code) and decodes the first N frames of a movie
rem   thp_ref.py    compares the dump with an independent model of the console's assembly (float32 + exact fma) and ffmpeg
rem Usage: thp_test.cmd [movie.thp, default data\openingA.thp] [frames, default 8] [first frame, default 0]
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH, python with numpy + Pillow, ffmpeg on PATH.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "MOVIE=%~1"
if "%MOVIE%"=="" set "MOVIE=%SMS%\orig\GMSJ01\extracted\files\data\openingA.thp"
set "N=%~2"
if "%N%"=="" set "N=8"
set "START=%~3"
if "%START%"=="" set "START=0"
set "OUT=%TEMP%\sunshine_thp_test"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /utf-8 /W0 -Wno-everything /MD /O2 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h""
set "INC=/I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include""
clang-cl %CF% %INC% /TC /c "%SMS%\src\dolphin\thp\THPDec.c" /Fo"%OUT%\THPDec.obj" || exit /b 1
clang-cl %CF% %INC% /TC /c "%SMS%\src\dolphin\thp\THPAudio.c" /Fo"%OUT%\THPAudio.obj" || exit /b 1
clang-cl %CF% %INC% /TP /std:c++20 /EHsc "%~dp0thp_test.cpp" "%OUT%\THPDec.obj" "%OUT%\THPAudio.obj" ^
  /Fo"%OUT%\\" /Fe"%OUT%\thp_test.exe" || exit /b 1
"%OUT%\thp_test.exe" "%MOVIE%" %N% "%OUT%" %START% || exit /b 1
python "%~dp0thp_ref.py" "%MOVIE%" %N% "%OUT%" %START% || exit /b 1
