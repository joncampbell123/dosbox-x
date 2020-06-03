dnl Process this file with autoconf to produce a configure script.
AC_INIT(README)

dnl Set various version strings - taken gratefully from the GTk sources

# Making releases:
#   MICRO_VERSION += 1;
#   INTERFACE_AGE += 1;
#   BINARY_AGE += 1;
# if any functions have been added, set INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set BINARY_AGE and INTERFACE_AGE to 0.

MAJOR_VERSION=1
MINOR_VERSION=2
MICRO_VERSION=8
INTERFACE_AGE=0
BINARY_AGE=8
VERSION=$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION

AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(INTERFACE_AGE)
AC_SUBST(BINARY_AGE)
AC_SUBST(VERSION)

# libtool versioning
LT_INIT([win32-dll])

LT_RELEASE=$MAJOR_VERSION.$MINOR_VERSION
LT_CURRENT=`expr $MICRO_VERSION - $INTERFACE_AGE`
LT_REVISION=$INTERFACE_AGE
LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Detect the canonical build and host environments
AC_CANONICAL_HOST

dnl Setup for automake
AM_INIT_AUTOMAKE(SDL_net, $VERSION)

dnl Check for tools
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
if test -z "$host_alias"; then
    hostaliaswindres=
else
    hostaliaswindres="$host_alias-windres"
fi
AC_CHECK_PROGS(WINDRES, [windres $hostaliaswindres $host_os-windres])

case "$host" in
    *-*-beos*)
        ac_default_prefix=/boot/develop/tools/gnupro
        ;;
    *-*-cygwin* | *-*-mingw32*)
        if test "$build" != "$host"; then # cross-compiling
            # Default cross-compile location
            ac_default_prefix=/usr/local/cross-tools/i386-mingw32
        else
            # Look for the location of the tools and install there
            if test "$BUILD_PREFIX" != ""; then
                ac_default_prefix=$BUILD_PREFIX
            fi
        fi
        if test x$WINDRES != x; then
            use_version_rc=true
        fi
        ;;
esac
AM_CONDITIONAL(USE_VERSION_RC, test x$use_version_rc = xtrue)

dnl Figure out which networking libraries to use
case "$host" in
    *mingw32ce*)
        INETLIB="-lwinsock -lws2"
	;;
    x86_64-*-mingw32*)
        INETLIB="-lws2_32 -liphlpapi"
        ;;
    *-*-cygwin* | *-*-mingw32*)
        INETLIB="-lwsock32 -liphlpapi"
        ;;
    sparc*-*-solaris*)
        INETLIB="-lsocket -lnsl"
        ;;
    *-*-qnx*)
        INETLIB="-lsocket"
        ;;
    *-*-beos*)
        INETLIB=""
        ;;
    *-*-haiku*)
        INETLIB="-lnetwork"
        ;;
    *)
        INETLIB=""
        ;;
esac
AC_SUBST(INETLIB)

dnl LIBRARY TEST: SDL 2.x
SDL2_VERSION=2.0.5
AM_PATH_SDL2($SDL2_VERSION)

dnl LIBRARY TEST: SDL 1.x
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION)

dnl LIBRARY USE: SDL selection
if test -n "$SDL2_LIBS"; then
    LIBS="$LIBS $SDL2_LIBS"
    CPPFLAGS="$CPPFLAGS $SDL2_CFLAGS"
else
  if test -n "$SDL_LIBS"; then
    LIBS="$LIBS $SDL_LIBS"
    CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
  else
    AC_MSG_ERROR([SDL 1.x or SDL 2.x is required to compile this program])
  fi
fi

dnl Check for GUI library for the chat client
have_GUI=no
AC_ARG_ENABLE(gui,
[  --enable-gui            Try to build the GUI test programs [default=yes]],
              , enable_gui=yes)
if test x$enable_gui = xyes; then
    AC_LANG_PUSH(C++)
    AC_CHECK_LIB(GUI, GUI_Create, have_GUI=yes)
    AC_LANG_POP(C++)
    if test x$have_GUI != xyes; then
        AC_MSG_WARN([*** Can't find the GUI library - not building test programs
The GUI library can be found at:
http://www.libsdl.org/projects/GUIlib/
])
    fi
fi
AM_CONDITIONAL(HAVE_GUI_LIB, test x$have_GUI = xyes)

dnl C++ flags are the same as the C flags
CXXFLAGS="$CXXFLAGS $CFLAGS"

AC_SUBST([WINDRES])

# Finally create all the generated files
AC_OUTPUT([
Makefile SDL_net.spec SDL_net.qpg SDL_net.pc
])
