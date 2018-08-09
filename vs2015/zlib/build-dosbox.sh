#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

chmod +x configure || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

../configure || exit 1
make -j || exit 1

mkdir ../linux-host/include || exit 1
cp -v *.h ../linux-host/include || exit 1

mkdir ../linux-host/lib || exit 1
cp -v *.a ../linux-host/lib || exit 1

