#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

./autogen.sh

chmod +x configure || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

opts=

sys=`uname -s`

if [ -e "$srcdir/../sdl2/linux-host" ]; then
    sdldir="$srcdir/../sdl2/linux-host"
else
    sdldir="$srcdir/../sdl/linux-host"
fi

../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared "--with-sdl-prefix=$sdldir" $opts || exit 1

# SDL is having concurrency problems with Brew compiles, help it out
# https://jenkins.brew.sh/job/Homebrew%20Core%20Pull%20Requests/35627/version=sierra/testReport/junit/brew-test-bot/sierra/install_dosbox_x/
mkdir -p linux-host || exit 1
mkdir -p linux-build || exit 1
mkdir -p linux-build/build || exit 1
mkdir -p linux-build/include || exit 1

# Um, what?
chmod +x "$srcdir/install-sh" || exit 1

# Proceed
make -j || exit 1
make install || exit 1  # will install into ./linux-host

