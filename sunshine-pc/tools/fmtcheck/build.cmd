@echo off
rem Builds the offline format checker j2djut_check.exe into tools\fmtcheck\out (see fmtcheck_j2djut.py).
rem It compiles the real decomp readers with the game's flags and links them with /FORCE:UNRESOLVED: only the resource
rem lookup and four GX init functions are provided by the harness, everything else is unresolved and never called.
rem Needs LLVM_BIN (folder with clang-cl.exe) if clang-cl is not on PATH.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "PC=%~dp0..\.."
set "SMS=%~dp0..\..\..\sms"
set "OUT=%~dp0out"
if not exist "%OUT%" mkdir "%OUT%"
set "J=%SMS%\src\JSystem"
clang-cl /nologo /TP /std:c++20 /Zc:__cplusplus /utf-8 -Wno-everything -Wno-multichar -Wno-trigraphs /clang:-fno-strict-return ^
  /O1 /DTARGET_PC=1 /DVERSION_GMSJ01=1 /DNDEBUG=1 /FI"%PC%\include\pc_compat.h" ^
  /I"%PC%\include" /I"%SMS%\include" /I"%SMS%\include\dolphin" /I"%SMS%\include\PowerPC_EABI_Support" ^
  /I"%PC%\extern\aurora\include" ^
  "%~dp0j2djut_check.cpp" ^
  "%J%\J2D\J2DPane.cpp" "%J%\J2D\J2DScreen.cpp" "%J%\J2D\J2DPicture.cpp" "%J%\J2D\J2DWindow.cpp" "%J%\J2D\J2DTextBox.cpp" ^
  "%J%\JUtility\JUTTexture.cpp" "%J%\JUtility\JUTPalette.cpp" "%J%\JUtility\JUTResFont.cpp" "%J%\JUtility\JUTFont.cpp" ^
  "%J%\JUtility\JUTResource.cpp" "%J%\JUtility\JUTNameTab.cpp" "%J%\JUtility\JUTRect.cpp" ^
  "%J%\JSupport\JSUList.cpp" "%J%\JSupport\JSUInputStream.cpp" "%J%\JSupport\JSUMemoryStream.cpp" ^
  "%SMS%\src\GC2D\MessageLoader.cpp" "%SMS%\src\GC2D\MessageUtil.cpp" ^
  /Fo"%OUT%\\" /Fe"%OUT%\j2djut_check.exe" /link /FORCE:UNRESOLVED /ignore:4088
