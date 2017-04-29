@echo off

IF NOT EXIST build mkdir build
pushd build

set flags=-nologo -FC -Zi -Od
set linkerflags=/NOLOGO /DEBUG /MACHINE:X64 user32.lib

REM BUILD FILEPACKER
cl -DPACKER %flags%  ..\filepacker.cpp -Fefilepacker /link %linkerflags%

REM BUILD FILEUNPACKER
cl -DUNPACKER %flags% ..\filepacker.cpp -Fefileunpacker /link %linkerflags%

REM BUILD FILEPACKERTEST
cl -DFILEPACKERTEST %flags%  ..\filepacker.cpp -Fefilepackertest /link %linkerflags%

popd
