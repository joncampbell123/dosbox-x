#!/bin/bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1
chmod +x configure || exit 1
# NTS: We use --disable-pulseaudio-shared because the dynamic loading seems to be slightly unstable
ac_cv_header_iconv_h=no ac_cv_func_iconv=no ac_cv_lib_iconv_libiconv_open=no ./configure "--prefix=`pwd`/linux-host" --enable-static --disable-shared --disable-pulseaudio-shared || exit 1
make -j || exit 1
make install || exit 1  # will install into ./linux-host
cp -vf include/SDL_config.h.default include/SDL_config.h || exit 1

