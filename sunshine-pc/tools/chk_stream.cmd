@echo off
rem Builds and runs chk_stream.cpp (host test of the endian-aware JSU stream layer) against real scene streams.
rem Usage: chk_stream.cmd <stream file> ...   (e.g. files written by discfmt.py extract); needs LLVM_BIN like syntax.cmd.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "OUT=%~dp0..\build\_chk"
if not exist "%OUT%" mkdir "%OUT%"
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar /EHsc /O1 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" /I"%PC%\extern\aurora\include" ^
  "%~dp0chk_stream.cpp" "%SMS%\src\JSystem\JSupport\JSUInputStream.cpp" "%SMS%\src\JSystem\JSupport\JSUMemoryStream.cpp" ^
  /Fe:"%OUT%\chk_stream.exe" /Fo:"%OUT%\\" || exit /b 1
"%OUT%\chk_stream.exe" %*
