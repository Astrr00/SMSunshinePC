@echo off
rem Builds and runs tools\thp_player_test.cpp: the decomp's THP player (THPPlayer/*.c, dolphin/thp/*.c with their TARGET_PC code)
rem on the port's real OS layer and VI/AI code, own temp dir, no cmake, no Aurora, no disc image (the movie is read from the
rem extracted files). Plays a movie for N retraces and checks frames and audio against direct decodes.
rem Usage: thp_player_test.cmd [movie.thp, default data\openingA.thp] [retraces, default 150]
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
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
if "%N%"=="" set "N=150"
set "OUT=%TEMP%\sunshine_thp_player_test"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /utf-8 /W0 -Wno-everything -Wno-multichar -Wno-trigraphs /clang:-fno-strict-return /EHsc /MD /O2 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h""
set "INC=/I"%PC%\src\sdk" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include""
rem the THPPlayer sources are .c files the decomp builds as C++; the SDK part (dolphin/thp) is C
clang-cl %CF% %INC% /TC /c "%SMS%\src\dolphin\thp\THPDec.c" /Fo"%OUT%\THPDec.obj" || exit /b 1
clang-cl %CF% %INC% /TC /c "%SMS%\src\dolphin\thp\THPAudio.c" /Fo"%OUT%\THPAudio.obj" || exit /b 1
clang-cl %CF% %INC% /TP /std:c++20 /c "%SMS%\src\THPPlayer\THPPlayer.c" "%SMS%\src\THPPlayer\THPRead.c" ^
  "%SMS%\src\THPPlayer\THPVideoDecode.c" "%SMS%\src\THPPlayer\THPAudioDecode.c" /Fo"%OUT%\\" || exit /b 1
clang-cl %CF% %INC% /TP /std:c++20 "%~dp0thp_player_test.cpp" "%OUT%\THPDec.obj" "%OUT%\THPAudio.obj" "%OUT%\THPPlayer.obj" ^
  "%OUT%\THPRead.obj" "%OUT%\THPVideoDecode.obj" "%OUT%\THPAudioDecode.obj" ^
  "%PC%\src\sdk\os_thread.cpp" "%PC%\src\sdk\os_misc.cpp" "%PC%\src\sdk\av.cpp" "%PC%\src\sdk\av_win.cpp" "%PC%\src\sdk\av_dump.cpp" "%PC%\src\settings.cpp" "%PC%\src\crash.cpp" ^
  /Zi /Fo"%OUT%\\" /Fe"%OUT%\thp_player_test.exe" /link /DEBUG || exit /b 1
"%OUT%\thp_player_test.exe" "%MOVIE%" %N%
