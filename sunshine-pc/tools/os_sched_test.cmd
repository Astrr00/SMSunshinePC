@echo off
rem Builds and runs tools\os_sched_test.cpp against src\sdk\os_thread.cpp and os_misc.cpp (own temp dir, no cmake).
rem os_misc.cpp reads sunshine::settings() (OSSetSoundMode), so src\settings.cpp is linked too.
rem Usage: os_sched_test.cmd      Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "OUT=%TEMP%\sunshine_os_sched_test"
if not exist "%OUT%" mkdir "%OUT%"
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 /W0 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 ^
  /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" ^
  "%~dp0os_sched_test.cpp" "%PC%\src\sdk\os_thread.cpp" "%PC%\src\sdk\os_misc.cpp" "%PC%\src\settings.cpp" ^
  /Fo"%OUT%\\" /Fe"%OUT%\os_sched_test.exe" || exit /b 1
"%OUT%\os_sched_test.exe"
