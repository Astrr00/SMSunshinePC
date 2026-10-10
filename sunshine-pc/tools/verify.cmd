@echo off
rem Ordered verification steps (PASS/FAIL/SKIP with timing). Usage: tools\verify.cmd [--build] [--matching] [--full] [--boot-secs N] [--tile x,y]
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
python "%~dp0verify.py" %*
