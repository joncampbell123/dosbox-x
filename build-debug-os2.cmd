echo off
bash autogen.sh
dash configure --enable-debug
REM since cpu cores need a lot of memory,
REM we don't want to build parallel
cd src\cpu
make
cd ..\..
make -j5
