#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1
chmod +x configure || exit 1
ac_cv_header_iconv_h=no ac_cv_func_iconv=no ac_cv_lib_iconv_libiconv_open=no ./configure "--prefix=`pwd`/linux-host" --enable-static --disable-shared --disable-x11-shared --disable-video-x11-xrandr --disable-video-x11-vm --disable-video-x11-xv || exit 1
make -j || exit 1
make install || exit 1  # will install into ./linux-host

