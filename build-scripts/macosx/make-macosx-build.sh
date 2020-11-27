#!/bin/bash
# 
# Setup:
#   git clone https://github.com/joncampbell123/dosbox-x dosbox-x
#   git clone https://github.com/joncampbell123/dosbox-x dosbox-x-sdl2
#
# Then run this script
arch=`uname -m`
name=`date +%F-%T | sed -e 's/:/-/g' | sed -e 's/-//g'`
name="dosbox-x-macosx-$arch-$name.zip"

echo "Will pack to $name"
sleep 1

top=`pwd`
cd "$top" || exit 1

cd "$top/dosbox-x" || exit 1
git clean -dfx
git reset --hard
git checkout master
git clean -dfx
git reset --hard
git pull
git clean -dfx
git reset --hard
./build-macosx || exit 1
make dosbox-x.app || exit 1
cp CHANGELOG CHANGELOG.txt || exit 1

cd "$top/dosbox-x-sdl2" || exit 1
git clean -dfx
git reset --hard
git checkout master
git clean -dfx
git reset --hard
git pull
git clean -dfx
git reset --hard
./build-macosx-sdl2 || exit 1
make dosbox-x.app || exit 1
cp CHANGELOG CHANGELOG.txt || exit 1

cd "$top" || exit 1
echo "Packing up now..."

zip -r -9 "$name" dosbox-x/dosbox-x.app dosbox-x/CHANGELOG.txt dosbox-x-sdl2/dosbox-x.app dosbox-x-sdl2/CHANGELOG.txt || exit 1

exit 0

