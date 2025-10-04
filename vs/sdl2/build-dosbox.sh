#!/usr/bin/env bash

MAKE=""
find_make()
{
  if test "$MAKE" = "" && \
     command -v $1 >/dev/null && \
     $1 --version | grep -q "GNU Make"; then
    MAKE="$1"
  fi
}
find_make make
find_make gmake
if test "$MAKE" = ""; then
  echo "Couldn't find GNU Make!"
  exit 1
fi

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
elif [ "$sys" != "Linux" ]; then
# These are supported on BSDs but the SDL2 code assumes Linux
opts="--disable-video-wayland --disable-libudev"
fi
if [ "$1" == "hx-dos" ]; then
opts="--disable-video-opengl"
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

$MAKE -j3 || exit 1
$MAKE install || exit 1  # will install into ./linux-host

