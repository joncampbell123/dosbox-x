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

sys=`uname -s`

if [ "$sys" == "Darwin" ]; then
opts="--disable-video-x11"
fi

ac_cv_header_iconv_h=no ac_cv_func_iconv=no ac_cv_lib_iconv_libiconv_open=no ../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared --disable-x11-shared --disable-video-x11-xrandr --disable-video-x11-vm --disable-video-x11-xv $opts || exit 1

# MinGW mod
cat >>include/SDL_config.h <<_EOF
/* do NOT allow ddraw! */
#ifdef SDL_VIDEO_DRIVER_DDRAW
#undef SDL_VIDEO_DRIVER_DDRAW
#endif
_EOF

if [ "$1" == "hx-dos" ]; then
cat >>include/SDL_config.h <<_EOF
/* For HX-DOS, no parent window */
#ifndef SDL_WIN32_NO_PARENT_WINDOW
#define SDL_WIN32_NO_PARENT_WINDOW
#endif

#ifndef SDL_WIN32_HX_DOS
#define SDL_WIN32_HX_DOS
#endif
_EOF
fi

make -j || exit 1
make install || exit 1  # will install into ./linux-host

# STOP DELETING THE FILE!!!!! I need it for Windows builds!
cd "$srcdir" || exit 1
cp -v include/SDL_config.h.default include/SDL_config.h

