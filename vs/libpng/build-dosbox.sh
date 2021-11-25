#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

chmod +x configure || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

opts=

../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared $opts || exit 1

# mingw: need to remove ^M from the header file.
# first make the file exist, then filter it.
make pnglibconf.h || exit 1
(cat pnglibconf.h | sed -e 's/\x0D//' >x) || exit 1
mv x pnglibconf.h || exit 1

make -j || exit 1
make install || exit 1  # will install into ./linux-host

