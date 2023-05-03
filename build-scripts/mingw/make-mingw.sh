#!/usr/bin/env bash
# 
# Setup:
#   git clone https://github.com/joncampbell123/dosbox-x dosbox-x-mingw
#
# Then run this script

# Check 1: Make sure this is the MinGW32 environment.
# 
#          MinGW and MinGW32 will have MSYSTEM=MINGW32
#          MinGW64 will have MSYSTEM=MINGW64.
if [[ "${MSYSTEM}" == "MINGW32" ]]; then
    cputype=win32
elif [[ "${MSYSTEM}" == "MINGW64" ]]; then
    cputype=win64
else
    echo "MinGW32/MINGW64 (not MinGW) shell environment required"
    exit 1
fi

name=$(date +%F-%T | sed -e 's/:/-/g' | sed -e 's/-//g')
name="dosbox-x-mingw-${cputype}-${name}.zip"

echo "Will pack to ${name}"
sleep 1

# OK GO

top=$(pwd)
cd "${top}" || exit 1

build="${top}/mingw-build"
rm -Rf "${build}"
mkdir -p "${build}" || exit 1

ziptool="${top}/dosbox-x-mingw/vs/tool/zip.exe"

# do it
for what in mingw mingw-sdl2 mingw-lowend mingw-lowend-sdl2; do
buildtarget="${build}/${what}"
mkdir -p "${buildtarget}" || exit 1
mkdir -p "${buildtarget}/drivez" || exit 1
mkdir -p "${buildtarget}/scripts" || exit 1
mkdir -p "${buildtarget}/shaders" || exit 1
mkdir -p "${buildtarget}/glshaders" || exit 1
mkdir -p "${buildtarget}/languages" || exit 1
cd "${top}/dosbox-x-mingw" || exit 1
git clean -dfx
git reset --hard
git checkout master
git clean -dfx
git reset --hard
git pull
git clean -dfx
git reset --hard
./build-${what} || exit 1
strip src/dosbox-x.exe || exit 1
cp src/dosbox-x.exe "$buildtarget/dosbox-x.exe" || exit 1
cp CHANGELOG "$buildtarget/CHANGELOG.txt" || exit 1
cp dosbox-x.reference.conf "$buildtarget/dosbox-x.reference.conf" || exit 1
cp dosbox-x.reference.full.conf "$buildtarget/dosbox-x.reference.full.conf" || exit 1
if [[ "${MSYSTEM}" == "MINGW32" ]]; then
    cp contrib/windows/installer/inpout32.dll "${buildtarget}/inpout32.dll" || exit 1
else
    cp contrib/windows/installer/inpoutx64.dll "${buildtarget}/inpoutx64.dll" || exit 1
fi
cp contrib/fonts/FREECG98.BMP "${buildtarget}/FREECG98.BMP" || exit 1
cp contrib/fonts/wqy_1?pt.bdf "${buildtarget}/" || exit 1
cp contrib/fonts/Nouveau_IBM.ttf "${buildtarget}/Nouveau_IBM.ttf" || exit 1
cp contrib/fonts/SarasaGothicFixed.ttf "${buildtarget}/SarasaGothicFixed.ttf" || exit 1
cp contrib/windows/installer/drivez_readme.txt "${buildtarget}/drivez/readme.txt" || exit 1
cp contrib/windows/installer/windows_explorer_context_menu*.bat "{$buildtarget}/scripts/" || exit 1
cp contrib/windows/shaders/* "${buildtarget}/shaders/" || exit 1
cp contrib/glshaders/* "${buildtarget}/glshaders/" || exit 1
cp contrib/translations/*/*.lng "${buildtarget}/languages/" || exit 1

done

cd "${top}" || exit 1
echo "Packing up now..."

$ziptool -r -9 "${name}" "mingw-build" || exit 1

exit 0
