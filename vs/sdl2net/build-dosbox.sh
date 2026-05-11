#!/bin/sh
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

sdldirspec="--with-sdl-prefix=$srcdir/../sdl2/linux-host"

autoreconf -fi
../configure "--srcdir=$srcdir" "--prefix=$instdir" --disable-examples "$sdldirspec" $opts || exit 1

mkdir -p linux-host || exit 1
mkdir -p linux-build || exit 1
mkdir -p linux-build/build || exit 1
mkdir -p linux-build/include || exit 1

# Um, what?
#chmod +x "$srcdir/install-sh" || exit 1

# Proceed
make -j3 || exit 1
make install || exit 1  # will install into ./linux-host
