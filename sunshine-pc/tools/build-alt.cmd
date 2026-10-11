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
set "SCCACHE="
for /f "delims=" %%S in ('where sccache.exe 2^>nul') do if not defined SCCACHE set "SCCACHE=%%S"
if not defined SCCACHE for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Mozilla.sccache_*") do for /d %%E in ("%%D\sccache-*") do if exist "%%E\sccache.exe" set "SCCACHE=%%E\sccache.exe"
set "XLAUNCH="
if defined SCCACHE set XLAUNCH="-DCMAKE_C_COMPILER_LAUNCHER=%SCCACHE%" "-DCMAKE_CXX_COMPILER_LAUNCHER=%SCCACHE%"
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DSUNSHINE_ALLOW_UNRESOLVED=ON -DCMAKE_POLICY_DEFAULT_CMP0141=NEW -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded %XLAUNCH% %XCMAKE% >nul || exit /b 1
rem One build at a time across all trees (8 jobs, low priority); the lock is released when the build process ends.
start "" /b /wait /low python "%~dp0locked.py" cmake --build "%BLD%" --target %TARGET% -j 8
