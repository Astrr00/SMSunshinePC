@echo off
rem Builds tools\jpa_test.cpp against sms_game.lib + Aurora's GD/MTX/OS libs of a private build tree (build-alt.cmd <name>)
rem and runs tools\jpa_test.py (independent big-endian parse of every .jpa of particle.szs / bosshanachanJpa.szs / select.szs
rem against what the real JPA loader code holds on the host).
rem Usage: jpa_test.cmd <build name> [jpa_test.py options, e.g. --scenes]
rem Needs LLVM_BIN (folder with clang-cl.exe). Links with /FORCE:UNRESOLVED like the game (an unresolved symbol faults when called).
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: jpa_test.cmd ^<build name^> [options] & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "TOOLS=%~dp0"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "BLD=%PC%\build\%~1"
set "OUT=%PC%\build\jpa-test"
if not defined JPA_LIB set "JPA_LIB=%BLD%\sms_game.lib"
if not exist "%OUT%" mkdir "%OUT%"
set "CF=/nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 /Zi /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1"
set INC=/FI"%PC%\include\pc_compat.h" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include" /I"%SMS%\src"
clang-cl %CF% %INC% "%TOOLS%jpa_test.cpp" "%PC%\src\crash.cpp" "%JPA_LIB%" "%BLD%\extern\aurora\aurora_gd.lib" "%BLD%\extern\aurora\aurora_mtx.lib" "%BLD%\extern\aurora\aurora_os.lib" ^
  /Fo"%OUT%\\" /Fe"%OUT%\jpa_test.exe" /link /DEBUG /FORCE:UNRESOLVED || exit /b 1
shift
python "%TOOLS%jpa_test.py" "%OUT%\jpa_test.exe" %1 %2 %3 %4
