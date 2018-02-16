#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

chmod +x configure || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

ac_cv_header_iconv_h=no ac_cv_func_iconv=no ac_cv_lib_iconv_libiconv_open=no ../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared --disable-x11-shared --disable-video-x11-xrandr --disable-video-x11-vm --disable-video-x11-xv || exit 1
make -j || exit 1
make install || exit 1  # will install into ./linux-host

# STOP DELETING THE FILE!!!!! I need it for Windows builds!
cd "$srcdir" || exit 1
cp -v include/SDL_config.h.default include/SDL_config.h

