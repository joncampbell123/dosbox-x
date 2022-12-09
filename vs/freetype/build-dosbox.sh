#!/usr/bin/env bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

chmod +x configure || exit 1
chmod +x builds/unix/install-sh || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

opts=

../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared --with-bzip2=no --with-harfbuzz=no --with-png=no $opts || exit 1

make -j || exit 1
make install || exit 1  # will install into ./linux-host

