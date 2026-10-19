@echo off
rem Headless /O2 checks of the real graph lookup; run from sunshine-pc with LLVM_BIN set.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if not exist build mkdir build
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
clang-cl /nologo /std:c++20 /O2 /Gy /Gw /EHsc /w -Wno-register /DTARGET_PC /Iinclude /FIinclude\pc_compat.h /I..\sms\include /Iextern\aurora\include tools\replay_node_test.cpp ..\sms\src\Enemy\graph.cpp ..\sms\src\Enemy\Spline.cpp /Fo:build\ /Fe:build\replay_node_test.exe /link /OPT:REF /OPT:ICF || exit /b 1
build\replay_node_test.exe
