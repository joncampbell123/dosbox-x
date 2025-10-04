#!/usr/bin/env bash
rm -Rfv linux-host || exit 1
mkdir -p linux-host || exit 1

rm -Rfv linux-build || exit 1
mkdir -p linux-build || exit 1

if [ "$1" == "hx-dos" ]; then
    cp SDLnet.c SDLnet.c.default || exit 1
    chmod +w SDLnet.c || exit 1
    sed -b -e 's/^\(#elif defined(__WIN32__)\)/\1 \&\& 0/' SDLnet.c.default >SDLnet.c || exit 1
fi

./autogen.sh

chmod +x configure || exit 1

srcdir="`pwd`"
instdir="`pwd`/linux-host"

cd linux-build || exit 1

opts=

sys=`uname -s`

if [ -e "$srcdir/../sdl2/linux-host" ]; then
    sdldirspec="--with-sdl-prefix=$srcdir/../sdl2/linux-host"
elif [ -e "$srcdir/../sdl/linux-host" ]; then
    sdldirspec="--with-sdl-prefix=$srcdir/../sdl/linux-host"
fi

../configure "--srcdir=$srcdir" "--prefix=$instdir" --enable-static --disable-shared "$sdldirspec" $opts || exit 1

# SDL is having concurrency problems with Brew compiles, help it out
# https://jenkins.brew.sh/job/Homebrew%20Core%20Pull%20Requests/35627/version=sierra/testReport/junit/brew-test-bot/sierra/install_dosbox_x/
mkdir -p linux-host || exit 1
mkdir -p linux-build || exit 1
mkdir -p linux-build/build || exit 1
mkdir -p linux-build/include || exit 1

# Um, what?
chmod +x "$srcdir/install-sh" || exit 1

# Proceed
make -j3 || exit 1
make install || exit 1  # will install into ./linux-host

cd "$srcdir" || exit 1
if [ "$1" == "hx-dos" ]; then
    cp SDLnet.c.default SDLnet.c || exit 1
fi

# Delete libSDLmain.a and libSDL.a from archive file
# otherwise linking will fail on macOS
ar dv linux-host/lib/libSDL_net.a libSDLmain.a || true
ar dv linux-host/lib/libSDL_net.a libSDL.a || true