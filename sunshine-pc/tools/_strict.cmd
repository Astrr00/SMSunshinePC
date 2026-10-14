@echo off
rem Strict link gate: builds sunshine.exe into build\Strict WITHOUT /FORCE:UNRESOLVED, so any missing symbol is an LNK2019/LNK1120 error.
rem Usage: python tools\locked.py cmd /c tools\_strict.cmd      (run from sunshine-pc)
rem Needs LLVM_BIN (folder with clang-cl.exe). Exit code 0 = 0 unresolved symbols.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "SRC=%~dp0.."
set "BLD=%~dp0..\build\Strict"
set "SCCACHE="
for /f "delims=" %%S in ('where sccache.exe 2^>nul') do if not defined SCCACHE set "SCCACHE=%%S"
if not defined SCCACHE for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Mozilla.sccache_*") do for /d %%E in ("%%D\sccache-*") do if exist "%%E\sccache.exe" set "SCCACHE=%%E\sccache.exe"
rem sccache only with SUNSHINE_SCCACHE=1: a hit on an object compiled in another checkout replays that compile's
rem clang-cl /showIncludes, so ninja records the other checkout's headers and a header edit here does not rebuild
rem the object. The empty launcher also clears one an earlier configure cached.
if not "%SUNSHINE_SCCACHE%"=="1" set "SCCACHE="
set XLAUNCH="-DCMAKE_C_COMPILER_LAUNCHER=" "-DCMAKE_CXX_COMPILER_LAUNCHER="
if defined SCCACHE set XLAUNCH="-DCMAKE_C_COMPILER_LAUNCHER=%SCCACHE%" "-DCMAKE_CXX_COMPILER_LAUNCHER=%SCCACHE%"
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DSUNSHINE_ALLOW_UNRESOLVED=OFF -DCMAKE_POLICY_DEFAULT_CMP0141=NEW -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded %XLAUNCH% >nul || exit /b 1
cmake --build "%BLD%" --target sunshine -j 6 > "%BLD%\_link.log" 2>&1
set "RC=%ERRORLEVEL%"
findstr /c:"LNK2019" /c:"LNK2001" /c:"LNK1120" "%BLD%\_link.log" >nul && (echo STRICT: unresolved symbols, see %BLD%\_link.log & exit /b 1)
if not "%RC%"=="0" (echo STRICT: build failed rc=%RC%, see %BLD%\_link.log & exit /b %RC%)
if not exist "%BLD%\sunshine.exe" (echo STRICT: FAIL, no sunshine.exe in %BLD% & exit /b 1)
echo STRICT: 0 unresolved symbols
exit /b 0
