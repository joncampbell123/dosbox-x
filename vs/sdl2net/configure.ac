dnl Process this file with autoconf to produce a configure script.

dnl Set various version strings - taken gratefully from the GTk sources
# See release_checklist.md
m4_define([MAJOR_VERSION_MACRO], [2])
m4_define([MINOR_VERSION_MACRO], [2])
m4_define([MICRO_VERSION_MACRO], [0])

AC_INIT([SDL2_net],
        [MAJOR_VERSION_MACRO.MINOR_VERSION_MACRO.MICRO_VERSION_MACRO],
        [https://github.com/libsdl-org/SDL_net/issues],
        [SDL2_net])
AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([SDLnet.c])

AC_SUBST([MAJOR_VERSION], MAJOR_VERSION_MACRO)
AC_SUBST([MINOR_VERSION], MINOR_VERSION_MACRO)
AC_SUBST([MICRO_VERSION], MICRO_VERSION_MACRO)

BINARY_AGE=`expr $MINOR_VERSION \* 100 + $MICRO_VERSION`
AS_CASE(["$MINOR_VERSION"],
  [*@<:@02468@:>@],
    dnl Stable branch, 2.2.1 -> libSDL2_net-2.0.so.0.200.1
    [INTERFACE_AGE="$MICRO_VERSION"],
  [*],
    dnl Development branch, 2.1.1 -> libSDL2_net-2.0.so.0.101.0
    [INTERFACE_AGE=0])

AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(INTERFACE_AGE)
AC_SUBST(BINARY_AGE)
AC_SUBST(VERSION)

dnl libtool versioning
LT_INIT([win32-dll])

# For historical reasons, the library name redundantly includes the major
# version twice: libSDL2_net-2.0.so.0.
# TODO: in SDL 3, stop using -release, which will simplify it to libSDL3_net.so.0
LT_RELEASE=2.0

# Increment this if there is an incompatible change - but if that happens,
# we should rename the library from SDL2 to SDL3, at which point this would
# reset to 0 anyway.
LT_MAJOR=0

LT_REVISION=$INTERFACE_AGE
LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`
LT_CURRENT=`expr $LT_MAJOR + $LT_AGE`
LT_EXTRA=""
m4_pattern_allow([^LT_MAJOR$])

dnl For use in static assertions
AC_DEFINE_UNQUOTED([SDL_BUILD_MAJOR_VERSION], $MAJOR_VERSION, [ ])
AC_DEFINE_UNQUOTED([SDL_BUILD_MINOR_VERSION], $MINOR_VERSION, [ ])
AC_DEFINE_UNQUOTED([SDL_BUILD_MICRO_VERSION], $MICRO_VERSION, [ ])

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_EXTRA)

dnl Detect the canonical build and host environments
AC_CANONICAL_HOST

dnl Setup for automake
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])

dnl Check for tools
AC_PROG_CC
AC_CHECK_TOOL(RC,[windres],[:])
AC_PROG_INSTALL
AC_PROG_MAKE_SET

case "$host" in
    *-*-beos*)
        ac_default_prefix=/boot/develop/tools/gnupro
        ;;
    *-*-cygwin* | *-*-mingw*)
        if test "$build" != "$host"; then # cross-compiling
            # Default cross-compile location
            ac_default_prefix=/usr/local/cross-tools/$host
        else
            # Look for the location of the tools and install there
            if test "$BUILD_PREFIX" != ""; then
                ac_default_prefix=$BUILD_PREFIX
            fi
        fi
        use_version_rc=true
        LT_EXTRA="-Wl,version.o"
        ;;
    *-*-os2*)
        # disable static builds on os/2
        enable_static=no
        # -DBUILD_SDL is needed for DECLSPEC
        CFLAGS="$CFLAGS -DBUILD_SDL"
        # OS/2 does not support a DLL name longer than 8 characters.
        LT_EXTRA="-os2dllname SDL2net"
        ;;
esac
AM_CONDITIONAL(USE_VERSION_RC, test x$use_version_rc = xtrue)

dnl Figure out which networking libraries to use
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

dnl See if GCC's -Wall is supported.
CheckWarnAll()
{
    AC_MSG_CHECKING(for GCC -Wall option)
    have_gcc_Wall=no

    save_CFLAGS="$CFLAGS"
    CFLAGS="$save_CFLAGS -Wall"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([int x = 0;])], [have_gcc_Wall=yes])
    AC_MSG_RESULT($have_gcc_Wall)
    CFLAGS="$save_CFLAGS"

    if test x$have_gcc_Wall = xyes; then
        CFLAGS="$CFLAGS -Wall"

dnl Haiku headers use multicharacter constants all over the place. Ignore these warnings when using -Wall.
        AC_MSG_CHECKING(for necessary GCC -Wno-multichar option)
        need_gcc_Wno_multichar=no
        case "$host" in
            *-*-haiku*)
                need_gcc_Wno_multichar=yes
                ;;
        esac
        AC_MSG_RESULT($need_gcc_Wno_multichar)
        if test x$need_gcc_Wno_multichar = xyes; then
            CFLAGS="$CFLAGS -Wno-multichar"
        fi
    fi
}

dnl See if GCC's -fvisibility=hidden is supported (gcc4 and later, usually).
CheckVisibilityHidden()
{
    AC_MSG_CHECKING(for GCC -fvisibility=hidden option)
    have_gcc_fvisibility=no
    case "$host" in
    *-*-cygwin* | *-*-mingw* | *-*-os2*)
        AC_MSG_RESULT([ignored for $host_os])
        return
        ;;
    esac

    visibility_CFLAGS="-fvisibility=hidden"
    save_CFLAGS="$CFLAGS"
    CFLAGS="$save_CFLAGS $visibility_CFLAGS -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #if !defined(__GNUC__) || __GNUC__ < 4
    #error SDL only uses visibility attributes in GCC 4 or newer
    #endif
    ]],[])], [have_gcc_fvisibility=yes],[])
    AC_MSG_RESULT($have_gcc_fvisibility)
    CFLAGS="$save_CFLAGS"

    if test x$have_gcc_fvisibility = xyes; then
        CFLAGS="$CFLAGS $visibility_CFLAGS"
    fi
}

AC_ARG_ENABLE([examples],
  AS_HELP_STRING([--disable-examples], [Do not build example programs]),,
    enable_examples=yes)
AM_CONDITIONAL(ENABLE_EXAMPLES, [test x$enable_examples = xyes])

dnl Check for SDL
SDL_VERSION=2.0.4
AC_SUBST(SDL_VERSION)
AM_PATH_SDL2($SDL_VERSION,
            :,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

dnl Calculate the location of the prefix, relative to the cmake folder
pkg_cmakedir='$libdir/cmake/SDL2_net'
AX_COMPUTE_RELATIVE_PATHS([pkg_cmakedir:prefix:cmake_prefix_relpath])
AC_SUBST([cmake_prefix_relpath])

dnl check for GCC warning options
CheckWarnAll

dnl check for GCC visibility attributes
CheckVisibilityHidden

if test x$enable_shared = xyes; then
    ENABLE_SHARED_TRUE=
    ENABLE_SHARED_FALSE="#"
else
    ENABLE_SHARED_TRUE="#"
    ENABLE_SHARED_FALSE=
fi
if test x$enable_static = xyes; then
    ENABLE_STATIC_TRUE=
    ENABLE_STATIC_FALSE="#"
else
    ENABLE_STATIC_TRUE="#"
    ENABLE_STATIC_FALSE=
fi
AC_SUBST(ENABLE_SHARED_TRUE)
AC_SUBST(ENABLE_SHARED_FALSE)
AC_SUBST(ENABLE_STATIC_TRUE)
AC_SUBST(ENABLE_STATIC_FALSE)

# Finally create all the generated files
AC_CONFIG_FILES([
Makefile
sdl2_net-config.cmake
sdl2_net-config-version.cmake
SDL2_net.spec
SDL2_net.pc
])
AC_OUTPUT
