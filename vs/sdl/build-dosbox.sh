#!/usr/bin/env bash
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
if [ "$1" == "hx-dos" ]; then
opts="--disable-video-opengl"
fi

ac_cv_header_iconv_h=no ac_cv_func_iconv=no ac_cv_lib_iconv_libiconv_open=no ../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared --disable-x11-shared --disable-video-x11-xrandr --disable-video-x11-vm --disable-video-x11-xv $opts || exit 1

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

echo "#undef ENABLE_IM_EVENT" >>include/SDL_platform.h
fi

# SDL is having concurrency problems with Brew compiles, help it out
# https://jenkins.brew.sh/job/Homebrew%20Core%20Pull%20Requests/35627/version=sierra/testReport/junit/brew-test-bot/sierra/install_dosbox_x/
mkdir -p linux-host || exit 1
mkdir -p linux-build || exit 1
mkdir -p linux-build/build || exit 1
mkdir -p linux-build/include || exit 1

make -j3 || exit 1
make install || exit 1  # will install into ./linux-host

# STOP DELETING THE FILE!!!!! I need it for Windows builds!
cd "$srcdir" || exit 1
cp -v include/SDL_config.h.default include/SDL_config.h
cp -v include/SDL_platform.h.default include/SDL_platform.h

