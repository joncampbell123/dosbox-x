#!/usr/bin/env bash
# 
# Setup:
#   git clone https://github.com/joncampbell123/dosbox-x dosbox-x-mingw-hx-dos
#
# Then run this script
name=$(date +%F-%T | sed -e 's/:/-/g' | sed -e 's/-//g')
name="dosbox-x-mingw-hx-dos-${name}.zip"

echo "Will pack to ${name}"
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
if [[ x"${MSYSTEM}" != x"MINGW32" ]]; then
    echo "MinGW (not MinGW 64-bit) shell environment required"
    exit 1
fi

# Check 2: Make sure this is the MinGW environment, not MinGW32/MinGW64.
#
#          MinGW32/MinGW64 will have MSYSTEM_CARCH=i686
#          MinGW will not have any such variable
if [ -n "${MSYSTEM_CARCH}" ]; then
    echo "Please use the original MinGW build environment, not MinGW32/MinGW64"
    exit 1
fi

# OK GO

top=$(pwd)
cd "${top}" || exit 1

hxdosdir="dosbox-x-mingw-hx-dos"
tooldir="${top}/${hxdosdir}/vs/tool"

cd "${top}/${hxdosdir}" || exit 1
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
hxdir="build-scripts/mingw/hxdos-bin"

strip src/dosbox-x.exe || exit 1
cp src/dosbox-x.exe dosbox-x.exe || exit 1
"${hxdir}"/pestub.exe -n dosbox-x.exe
"${tooldir}"/upx.exe --strip-relocs=0 --lzma -9 dosbox-x.exe
cp CHANGELOG CHANGELOG.txt || exit 1
cp COPYING COPYING.txt || exit 1
cp dosbox-x.reference.conf dosbox-x.ref || exit 1
cp dosbox-x.reference.full.conf dosbox-x.ref.full || exit 1
cp contrib/windows/installer/inpout32.dll INPOUT32.DLL || exit 1
cp contrib/fonts/FREECG98.BMP . || exit 1
cp contrib/fonts/wqy_1?pt.bdf . || exit 1
cp contrib/fonts/Nouveau_IBM.ttf . || exit 1
cp ${hxdir}/DPMILD32.EXE . || exit 1
cp ${hxdir}/HDPMI32.EXE . || exit 1
cp ${hxdir}/HXGUIHLP.INI . || exit 1
cp ${hxdir}/README.TXT . || exit 1
cp ${hxdir}/WATTCP.CFG . || exit 1
cp ${hxdir}/WINSPOOL.DRV . || exit 1
cp ${hxdir}/*.DLL . || exit 1
mkdir -p drivez || exit 1
cp contrib/windows/installer/drivez_readme.txt drivez/readme.txt || exit 1
mkdir -p language || exit 1
find contrib/translations/ -type f -name '*.lng' -exec cp {} language/ \;

cd "${top}/${hxdosdir}" || exit 1
echo "Packing up now..."

"${tooldir}"/zip.exe -r -9 ../"${name}" {CHANGELOG.txt,COPYING.txt,dosbox-x.exe,dosbox-x.ref,dosbox-x.ref.full,FREECG98.BMP,wqy_1?pt.bdf,Nouveau_IBM.ttf,DPMILD32.EXE,HDPMI32.EXE,HXGUIHLP.INI,README.TXT,WATTCP.CFG,WINSPOOL.DRV,*.DLL,drivez/*,language/*} || exit 1
cd ..

exit 0

