dnl SDL2_net static-only configure.ac

AC_PREREQ([2.69])
AC_INIT([SDL2_net], [2.2.0], [https://github.com/libsdl-org/SDL_net/issues])
AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([SDLnet.c])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])

# Check for C compiler
AC_PROG_CC
AC_PROG_RANLIB

# Check for SDL2
SDL_VERSION=2.0.4
AC_SUBST(SDL_VERSION)
AM_PATH_SDL2($SDL_VERSION, , AC_MSG_ERROR([*** SDL2 version $SDL_VERSION not found!]))
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

# Determine networking library
case "$host" in
  *-*-cygwin* | *-*-mingw*)
    INETLIB="-lws2_32 -liphlpapi"
    ;;
  sparc*-*-solaris*)
    INETLIB="-lsocket -lnsl"
    ;;
  *-*-qnx*)
    INETLIB="-lsocket"
    ;;
  *-*-haiku*)
    INETLIB="-lnetwork"
    ;;
  *)
    INETLIB=""
    ;;
esac
AC_SUBST(INETLIB)

# Enable/disable examples
AC_ARG_ENABLE([examples],
  AS_HELP_STRING([--disable-examples], [Do not build example programs]),
  enable_examples=yes, enable_examples=no)
AM_CONDITIONAL(ENABLE_EXAMPLES, [test x$enable_examples = xyes])

# Output files
AC_CONFIG_FILES([
Makefile
sdl2_net-config.cmake
sdl2_net-config-version.cmake
SDL2_net.spec
SDL2_net.pc
])
AC_OUTPUT