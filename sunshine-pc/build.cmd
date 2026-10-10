@echo off
rem Loads the MSVC x64 environment (headers, libs, linker), puts clang-cl on PATH, then configures and builds.
rem Usage: build.cmd [target]    Set LLVM_BIN to the folder containing clang-cl.exe if it is not on PATH.
rem A stray double quote inside PATH (seen in some shells) makes vcvars64.bat fail with "\Microsoft was unexpected".
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=sunshine"
cmake --preset windows-clang-relwithdebinfo || exit /b 1
cmake --build --preset windows-clang-relwithdebinfo --target %TARGET%
