@echo off
rem Real SPC opcodes at /O2; optional old-header include directory and old TU.
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if defined LLVM_BIN set "PATH=%LLVM_BIN%;%PATH%"
if not exist build\spc-pointer-conditions mkdir build\spc-pointer-conditions
set "OLD_INCLUDE="
if not "%~1"=="" set OLD_INCLUDE=/I"%~1"
set "SPC_SOURCE=..\sms\src\Strategic\spcinterp.cpp"
if not "%~2"=="" set "SPC_SOURCE=%~2"
clang-cl /nologo /std:c++20 /O2 /Ob1 /EHsc /w /DTARGET_PC=1 /FIinclude\pc_compat.h %OLD_INCLUDE% /Iinclude /I..\sms\include /Iextern\aurora\include tests\spc_pointer_conditions.cpp "%SPC_SOURCE%" /Fo:build\spc-pointer-conditions\ /Fe:build\spc-pointer-conditions\test.exe || exit /b 1
build\spc-pointer-conditions\test.exe
