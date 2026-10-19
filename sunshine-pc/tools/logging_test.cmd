@echo off
setlocal
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
if not exist build\logging-test mkdir build\logging-test
clang-cl /nologo /std:c++20 /O2 /Ob1 /DNDEBUG /EHsc /MD /utf-8 /Isrc tests\logging_smoke.cpp src\logging.cpp src\crash.cpp /Fo:build\logging-test\ /Fe:build\logging-test\logging_smoke.exe || exit /b 1
python tools\logging_test.py
exit /b %ERRORLEVEL%
