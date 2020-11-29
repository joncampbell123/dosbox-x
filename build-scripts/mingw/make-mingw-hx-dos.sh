#!/bin/bash
# 
# Setup:
#   git clone https://github.com/joncampbell123/dosbox-x dosbox-x-mingw-hx-dos
#
# Then run this script
name=`date +%F-%T | sed -e 's/:/-/g' | sed -e 's/-//g'`
name="dosbox-x-mingw-hx-dos-$name.zip"

echo "Will pack to $name"
sleep 1

# CHECK: You must use MinGW, not MinGW32/MinGW64.
#        The MinGW32/MinGW64 version has extra dependencies that require
#        Windows XP or higher and KERNEL functions not provided by HX-DOS.
#
#        The main MinGW version can target all the way down to Windows 95
#        (as long as a working MSVCRT.DLL is installed) and the compiled
#        binaries will work in HX-DOS.

# Check 1: Make sure this is the MinGW environment.
# 
#          MinGW and MinGW32 will have MSYSTEM=MINGW32
#          MinGW64 will have MSYSTEM=MINGW64.
if [[ x"$MSYSTEM" != x"MINGW32" ]]; then
    echo "MinGW (not MinGW 64-bit) shell environment required"
    exit 1
fi

# Check 2: Make sure this is the MinGW environment, not MinGW32/MinGW64.
#
#          MinGW32/MinGW64 will have MSYSTEM_CARCH=i686
#          MinGW will not have any such variable
if [ -n "$MSYSTEM_CARCH" ]; then
    echo "Please use the original MinGW build environment, not MinGW32/MinGW64"
    exit 1
fi

# OK GO

top=`pwd`
cd "$top" || exit 1

ziptool="$top/dosbox-x-mingw-hx-dos/vs2015/tool/zip.exe"

cd "$top/dosbox-x-mingw-hx-dos" || exit 1
git clean -dfx
git reset --hard
git checkout master
git clean -dfx
git reset --hard
git pull
git clean -dfx
git reset --hard
./build-mingw-hx-dos || exit 1

# The paths of copied files and HX DOS Extender files 
copydir="build-scripts/mingw/dosbox-x-mingw-hx-dos"
hxdir="build-scripts/mingw/hxdos"

strip src/dosbox-x.exe || exit 1
cp src/dosbox-x.exe dosbox-x.exe || exit 1
$hxdir/pestub.exe -n dosbox-x.exe
cp CHANGELOG CHANGELOG.txt || exit 1
cp dosbox-x.reference.conf dosbox-x.ref || exit 1
cp $hxdir/DPMILD32.EXE . || exit 1
cp $hxdir/HDPMI32.EXE . || exit 1
cp $hxdir/HXGUIHLP.INI . || exit 1
cp $hxdir/README.TXT . || exit 1
cp $hxdir/*.DLL . || exit 1

cd "$top"/dosbox-x-mingw-hx-dos || exit 1
echo "Packing up now..."

$ziptool -r -9 ../"$name" {CHANGELOG.txt,dosbox-x.exe,dosbox-x.ref,DPMILD32.EXE,HDPMI32.EXE,HXGUIHLP.INI,README.TXT,*.DLL} || exit 1
cd ..

exit 0

