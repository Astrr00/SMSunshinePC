@echo off
rem Private build tree for parallel work, so several people/agents can build without touching build/windows-clang-*.
rem Usage: build-alt.cmd <name> [target]      builds into sunshine-pc\build\<name>, default target: sunshine
rem Needs LLVM_BIN (folder with clang-cl.exe). Links with /FORCE:UNRESOLVED (diagnostics only), so the exe starts
rem even though some decomp sources are still missing; a missing function then faults when it is called.
set "PATH=%PATH:"=%"
if "%~1"=="" (echo usage: build-alt.cmd ^<name^> [target] & exit /b 2)
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "SRC=%~dp0.."
set "BLD=%~dp0..\build\%~1"
set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=sunshine"
rem Set ASAN=1 to build with AddressSanitizer (see SUNSHINE_ASAN in CMakeLists.txt).
set "XCMAKE="
if defined ASAN set "XCMAKE=-DSUNSHINE_ASAN=ON"
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DSUNSHINE_ALLOW_UNRESOLVED=ON %XCMAKE% >nul || exit /b 1
cmake --build "%BLD%" --target %TARGET%
