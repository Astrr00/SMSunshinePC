@echo off
rem Builds and runs two offline checks of the host DSP (own temp dir, no cmake):
rem   dsp_test.cpp        the mixer (src\sdk\dsp_mixer.cpp) against tools\dsp_ref.py's ADPCM fixture and synthetic voices
rem   dsp_proto_test.cpp  the mailbox protocol (src\sdk\dsp_glue.cpp) over the real OS layer and AI/DSP mailbox (av.cpp)
rem The fixture is generated first from the extracted disc's wave banks (python on PATH).
rem Usage: dsp_test.cmd      Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0.."
set "SMS=%~dp0..\..\sms"
set "OUT=%TEMP%\sunshine_dsp_test"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\proto" mkdir "%OUT%\proto"
python "%~dp0dsp_ref.py" "%OUT%\fixture.bin" || exit /b 1
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 /W0 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 ^
  /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\src\sdk" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" ^
  "%~dp0dsp_test.cpp" "%PC%\src\sdk\dsp_mixer.cpp" "%SMS%\src\JSystem\JAudio\JASystem\JASDriverTables.cpp" ^
  /Fo"%OUT%\\" /Fe"%OUT%\dsp_test.exe" || exit /b 1
"%OUT%\dsp_test.exe" "%OUT%\fixture.bin" || exit /b 1
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 /W0 -Wno-everything -Wno-multichar -Wno-trigraphs /EHsc /MD /O1 ^
  /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\src\sdk" /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" ^
  "%~dp0dsp_proto_test.cpp" "%PC%\src\sdk\dsp_glue.cpp" "%PC%\src\sdk\dsp_mixer.cpp" "%PC%\src\sdk\av.cpp" ^
  "%PC%\src\sdk\av_win.cpp" "%PC%\src\sdk\os_thread.cpp" "%PC%\src\sdk\os_misc.cpp" ^
  "%SMS%\src\JSystem\JAudio\JASystem\JASDriverTables.cpp" ^
  /Fo"%OUT%\proto\\" /Fe"%OUT%\dsp_proto_test.exe" || exit /b 1
"%OUT%\dsp_proto_test.exe"
