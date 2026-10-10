@echo off
set "PATH=%PATH:"=%"
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul
set "PATH=C:\llvm-dl\t\clang+llvm-23.1.3-x86_64-pc-windows-msvc\bin;%PATH%"
clang-cl /nologo /std:c++20 /O2 /EHsc tools\fres_test.cpp src\sdk\ppc_fres.cpp /Fe:build\fres_test.exe /Fo:build\
build\fres_test.exe
