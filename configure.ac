dnl Init.
AC_INIT(dosbox-x,0.82.11,[https://github.com/joncampbell123/dosbox-x/issues],[],[http://dosbox-x.software])
AC_PREREQ(2.60)
AC_CONFIG_SRCDIR(README)

dnl Utility function ============================

# AC_CHECK_CPPFLAGS(ADDITIONAL-CPPFLAGS, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
#
# checks whether the $(C) compiler accepts the ADDITIONAL-CPPFLAGS
# if so, they are added to the CPPFLAGS
AC_DEFUN([AC_CHECK_CPPFLAGS],
[
  AC_MSG_CHECKING([whether $CPP accepts "$1"])
  temp_check_cppflags="${CPPFLAGS}"
  CPPFLAGS="$1 ${CPPFLAGS}"
  AC_PREPROC_IFELSE(
        [AC_LANG_SOURCE([[int main(void){return 0;}]])],
        [AC_MSG_RESULT([yes])],
        [AC_MSG_RESULT([no]); CPPFLAGS="${temp_check_cppflags}"])
])# AC_CHECK_CPPFLAGS

dnl Utility function ============================

# AC_CHECK_CXXFLAGS(ADDITIONAL-CXXFLAGS, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
#
# checks whether the $(CXX) (c++) compiler accepts the ADDITIONAL-CXXFLAGS
# if so, they are added to the CXXFLAGS
AC_DEFUN([AC_CHECK_CXXFLAGS],
[
  AC_MSG_CHECKING([whether $CXX accepts "$1"])
cat > conftest.c++ << EOF
int main(){
  return 0;
}
EOF
if $CXX $CPPFLAGS $CXXFLAGS -o conftest.o conftest.c++ [$1] > /dev/null 2>&1
then
  AC_MSG_RESULT([yes])
  CXXFLAGS="${CXXFLAGS} [$1]"
  AC_CHECK_CPPFLAGS([$1])
  [$2]
else
  AC_MSG_RESULT([no])
  [$3]
fi
])# AC_CHECK_CXXFLAGS

dnl End Utility function ============================

dnl Detect the canonical host and target build environment
AC_CANONICAL_HOST
AC_CANONICAL_BUILD

dnl Setup for automake
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_OBJCXX
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PREFIX_DEFAULT([/usr/local])

dnl this code needs large file support on 32-bit systems
AC_SYS_LARGEFILE

#Check for big endian machine, should #define WORDS_BIGENDIAN if so
AC_C_BIGENDIAN

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_SIZEOF(unsigned char)
AC_CHECK_SIZEOF(unsigned short)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(int *)

dnl some semi complex check for sys/socket so it works on darwin as well
AC_CHECK_HEADERS([stdlib.h sys/types.h])
AC_CHECK_HEADERS([sys/socket.h  netinet/in.h pwd.h], [], [],
[#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])

dnl check for the socklen_t (darwin doesn't always have it)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])],[],[AC_DEFINE([socklen_t],[int],[Define to `int` if you don't have socklen_t])])

dnl C++0x is now considered deprecated.
dnl This warning will stay here for a long enough period of time before making C++11 mandatory.

dnl AC_CHECK_CXXFLAGS will prepend gnu++11 to CXXFLAGS if supported, for us.
dnl our code adds -std=gnu++0x if not supported where the function would not do anything.
dnl This code checks the CXX flags but does not keep the changes because AC_CHECK_CXXFLAGS
dnl also adds it to CPPFLAGS which then also affects the C compilation (with warnings like
dnl "-std=gnu++11 is not valid for C")
OLDCFLAGS="$CFLAGS"
OLDCPPFLAGS="$CPPFLAGS"
OLDCXXFLAGS="$CXXFLAGS"

AC_MSG_CHECKING([checking for C++11 support])
AC_CHECK_CXXFLAGS([ -std=gnu++11 ], [  ], [
    warn_cpp0x=1
])

CXXFLAGS="$OLDCXXFLAGS"
CPPFLAGS="$OLDCPPFLAGS"
CFLAGS="$OLDCFLAGS"

if test x$warn_cpp0x = x1; then
    AC_MSG_ERROR([ Your compiler must support C++11 or better ])
fi

CFLAGS="$CFLAGS -std=gnu11";
CXXFLAGS="$CXXFLAGS -std=gnu++0x";

dnl yksoft1 wants a MinGW build for Windows that doesn't use the Windows menu system.
AH_TEMPLATE(C_FORCE_MENU_SDLDRAW,[Define to 1 to force SDL-drawn menus])
AC_ARG_ENABLE(force-menu-sdldraw,AC_HELP_STRING([--enable-force-menu-sdldraw],[Force SDL drawn menus]),enable_force_menu_sdldraw=yes)

dnl This is how the build script can specify a HX DOS extender target
AH_TEMPLATE(C_HX_DOS,[Define to 1 to target HX DOS])
AC_ARG_ENABLE(hx-dos,AC_HELP_STRING([--enable-hx-dos],[Enable HX target]),enable_hx=yes)

dnl Let me know if you're targeting Emscripten
AH_TEMPLATE(C_EMSCRIPTEN,[Define to 1 to target Emscripten])
AC_ARG_ENABLE(emscripten,AC_HELP_STRING([--enable-emscripten],[Enable Emscripten target]),enable_emscripten=$enableval,enable_emscripten=no)

dnl Optimize for speed by default
AC_ARG_ENABLE(optimize,AC_HELP_STRING([--disable-optimize],[Don't enable compiler optimizations]))

dnl FIXME: Remove default "-O2" set by some autotools versions. TODO: check availability of sed.
CFLAGS=["`echo $CFLAGS' ' | sed -e 's/-O[^ ]* //g'`"]
CXXFLAGS=["`echo $CXXFLAGS' ' | sed -e 's/-O[^ ]* //g'`"]

if test x$enable_optimize != xno; then
    if test x$enable_emscripten == xyes; then
	    CFLAGS="$CFLAGS -Os"
	    CXXFLAGS="$CXXFLAGS -Os"
    else
	    CFLAGS="$CFLAGS -O2"
	    CXXFLAGS="$CXXFLAGS -O2"
    fi
fi

if test x$enable_emscripten == xyes; then
	AC_DEFINE(C_EMSCRIPTEN,1,[Targeting Emscripten])
fi

dnl Some stuff for the icon.
case "$host" in
    *-*-cygwin* | *-*-mingw32*)
    if test x$enable_hx == xyes; then
        CXXFLAGS="$CXXFLAGS -DHX_DOS"
        AC_DEFINE(C_HX_DOS,1,[Targeting HX DOS extender])
    fi
    ;;
esac

if test x$enable_force_menu_sdldraw == xyes; then
    CXXFLAGS="$CXXFLAGS -DFORCE_SDLDRAW"
    AC_DEFINE(C_FORCE_MENU_SDLDRAW,1,[Force SDL drawn menus])
fi

dnl TEST: Environ can be included
AC_MSG_CHECKING(if environ can be included)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
#include <stdlib.h>]],[[*environ;]])],
[AC_MSG_RESULT(yes);AC_DEFINE(ENVIRON_INCLUDED,1,[environ can be included])],AC_MSG_RESULT(no))

dnl TEST: Environ can be linked
AC_MSG_CHECKING(if environ can be linked)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern char ** environ;]],[[*environ;]])],
[AC_MSG_RESULT(yes);AC_DEFINE(ENVIRON_LINKED,1,[environ can be linked])],AC_MSG_RESULT(no))

dnl TEST: dirent includes d_type
AC_MSG_CHECKING([if dirent includes d_type])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <sys/types.h>
#include <dirent.h>
void blah(){
struct dirent d_test;
d_test.d_type = 0;
}])],[AC_MSG_RESULT(yes);AC_DEFINE(DIRENT_HAS_D_TYPE,1,[struct dirent has d_type])],AC_MSG_RESULT(no))

dnl TEST: Check for powf
AC_MSG_CHECKING(for powf in libm);
LIBS_BACKUP=$LIBS;
LIBS="$LIBS -lm";
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]],[[
        powf(1.0f, 1.0f);
]])], [AC_MSG_RESULT(yes)], [AC_DEFINE([DB_HAVE_NO_POWF],[1],[libm doesn't include powf])])
LIBS=$LIBS_BACKUP

dnl TEST: Check if the compiler support attributes
AH_TEMPLATE([C_HAS_ATTRIBUTE],[Determines if the compilers supports attributes for structures.])
AC_MSG_CHECKING(if compiler allows __attribute__)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
typedef struct { } __attribute__((packed)) junk;]],
[[ ]])],[ AC_MSG_RESULT(yes);AC_DEFINE(C_HAS_ATTRIBUTE)],AC_MSG_RESULT(no))

dnl TEST: Check if the compiler supports certain attributes
OLDCFLAGS="$CFLAGS"
CFLAGS="-Werror"

AH_TEMPLATE([C_ATTRIBUTE_ALWAYS_INLINE],[Determines if the compilers supports always_inline attribute.])
AC_MSG_CHECKING(if compiler allows __attribute__((always_inline)) )
AC_COMPILE_IFELSE([AC_LANG_SOURCE([ void __attribute__((always_inline)) test(){}
])],[ AC_MSG_RESULT(yes);AC_DEFINE(C_ATTRIBUTE_ALWAYS_INLINE)],AC_MSG_RESULT(no))

AH_TEMPLATE([C_ATTRIBUTE_FASTCALL],[Determines if the compilers supports fastcall attribute.])
AC_MSG_CHECKING(if compiler allows __attribute__((fastcall)) )
AC_COMPILE_IFELSE([AC_LANG_SOURCE([ void __attribute__((fastcall)) test(){}
])],[ AC_MSG_RESULT(yes);AC_DEFINE(C_ATTRIBUTE_FASTCALL)],AC_MSG_RESULT(no))

CFLAGS="$OLDCFLAGS"

dnl TEST: Check if the compiler supports __builtin_expect
#Switch language to c++
AC_LANG_PUSH(C++)
AH_TEMPLATE([C_HAS_BUILTIN_EXPECT],[Determines if the compilers supports __builtin_expect for branch prediction.])
AC_MSG_CHECKING(if compiler allows __builtin_expect)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[
int x=10;if( __builtin_expect ((x==1),0) ) ;
]])], [ AC_MSG_RESULT(yes);AC_DEFINE(C_HAS_BUILTIN_EXPECT)],AC_MSG_RESULT(no))
#switch language back
AC_LANG_POP(C++)

dnl Check for mprotect. Needed for 64 bits linux 
AH_TEMPLATE(C_HAVE_MPROTECT,[Define to 1 if you have the mprotect function])
AC_CHECK_HEADER([sys/mman.h], [
AC_CHECK_FUNC([mprotect],[AC_DEFINE(C_HAVE_MPROTECT,1)])
])

dnl Setpriority
AH_TEMPLATE(C_SET_PRIORITY,[Define to 1 if you have setpriority support])
AC_MSG_CHECKING(for setpriority support)
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <sys/resource.h>
int main(int argc,char * argv[]) {
	return setpriority (PRIO_PROCESS, 0,PRIO_MIN+PRIO_MAX);
};
])],AC_MSG_RESULT(yes);AC_DEFINE(C_SET_PRIORITY,1),AC_MSG_RESULT(no))

dnl Some target detection and actions for them
case "$host" in
    *-*-darwin*)
       dnl El Capitan's refusal to allow sudo make install to /usr/include, and the failure of most open source to
       dnl include from /usr/local/include is irritating and tiresome...
       CFLAGS="$CFLAGS -I/usr/local/include"
       CPPFLAGS="$CPPFLAGS -I/usr/local/include"
       ;;
esac

if test x$enable_emscripten == xyes; then
    CXXFLAGS="$CXXFLAGS"
else
    dnl Some default CPU flags
    case "$host_cpu" in
        x86_64 | amd64)
        # SSE is part of the x86_64 ABI
        CXXFLAGS="$CXXFLAGS -msse"
        ;;
    esac
fi

dnl Some needed libaries for OS2
dnl perharps join this with the other target depended checks. move them upwards
if test x$host = xi386-pc-os2-emx ; then
    CXXFLAGS="$CXXFLAGS -Zmt"
    LDFLAGS="$LDFLAGS -Zomf -Zmt"
    LIBS="$LIBS -los2me"
fi

dnl I would like to know of any concerns given by the C++ compiler.
dnl Clang/LLVM already does this to some degree, let's get GCC to do it too.
AC_CHECK_CXXFLAGS([ -Wall ])
AC_CHECK_CXXFLAGS([ -Wextra ])
AC_CHECK_CXXFLAGS([ -pedantic ])
#AC_CHECK_CXXFLAGS([ -Wconversion ])      DO NOT ENABLE. THIS WARNING IS WAY TOO PEDANTIC TO BE USEFUL, EXCEPT FOR SPECIFIC CASES
#AC_CHECK_CXXFLAGS([ -Wsign-conversion ])
AC_CHECK_CXXFLAGS([ -Wconversion-null ])

dnl Let GCC 7.3.x know that the "fall through" switch cases in this codebase
dnl are often intentional.
AC_CHECK_CXXFLAGS([ -Wno-implicit-fallthrough ])

dnl Stop reporting "type punning" warnings, I'm sick of hearing about it.
AC_CHECK_CXXFLAGS([ -Wno-strict-aliasing ])

dnl other
AC_CHECK_CXXFLAGS([ -Wno-missing-field-initializers ])
AC_CHECK_CXXFLAGS([ -Wno-format-zero-length ])

dnl Clang/LLVM warning: don't care the address of a member may be unaligned, unless targeting ARM!
AC_CHECK_CXXFLAGS([ -Wno-address-of-packed-member ])
dnl Clang/LLVM warning: don't care about int to void*, since void* is either same size or larger
AC_CHECK_CXXFLAGS([ -Wno-int-to-void-pointer-cast ])
dnl Clang/LLVM warning: extended field designator
AC_CHECK_CXXFLAGS([ -Wno-extended-offsetof ])

dnl Some stuff for the icon.
case "$host" in
    *-*-cygwin* | *-*-mingw32*)
       dnl Some stuff for the ico
       AC_CHECK_TOOL(WINDRES, windres, :)
       LDFLAGS="-static -static-libgcc -static-libstdc++ $LDFLAGS"
    ;;
    *)
       WINDRES=":"
    ;;
esac
       AM_CONDITIONAL(HAVE_WINDRES, test "x$WINDRES" != "x:")
       AC_SUBST(WINDRES)

if test x$enable_emscripten != xyes; then
dnl Some target detection and actions for them
case "$host" in
    *-*-cygwin* | *-*-mingw32*)
       LIBS="$LIBS -lwinmm -limm32"
       CXXFLAGS="$CXXFLAGS -mno-ms-bitfields"
       if test x$have_sdl_net_lib = xyes -a x$have_sdl_net_h = xyes ; then
         LIBS="$LIBS -lws2_32"
       fi
       dnl FEATURE: Whether to support direct parallel port passthrough
       AC_DEFINE(C_DIRECTLPT, 1, [ Define to 1 if you want parallel passthrough support (Win32, Linux).])
       ;;
    *-*-darwin*)
       dnl We have a problem here: both Mac OS X and Darwin report 
       dnl the same signature "powerpc-apple-darwin*" - so we have
       dnl to do more to distinguish them.
       dnl For now I am lazy and do not add proper detection code.
       macosx=1
       OBJCXXFLAGS="$OBJCXXFLAGS -std=gnu++0x";
       AC_DEFINE(MACOSX, 1, [Compiling on Mac OS X])
       LIBS="$LIBS -framework CoreMIDI -framework AudioUnit -framework AudioToolbox"
       ;;
    *-*-linux*)
       AC_DEFINE(LINUX, 1, [Compiling on GNU/Linux])
       CXXFLAGS="$CXXFLAGS -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L"
       dnl FEATURE: Whether to support direct parallel port passthrough
       AC_DEFINE(C_DIRECTLPT, 1, [ Define to 1 if you want parallel passthrough support (Win32, Linux).])
       ;;
    *-*-freebsd* | *-*-dragonfly* | *-*-netbsd* | *-*-openbsd*)
       dnl Disabled directserial for now. It doesn't do anything without
       dnl specifying an extra ifdef in directserial_posix.*
       dnl directserial detection should be rewritten to test for the needed
       dnl functions and headers. I currently do not know 
       dnl which ones are needed for BSD
       AC_DEFINE(BSD, 1, [Compiling on BSD])
       ;;
    *-*-haiku*)
       AC_DEFINE(BSD, 1, [Compiling on BSD])
       LIBS="$LIBS -lbsd -lnetwork"
       CXXFLAGS="$CXXFLAGS -D_BSD_SOURCE"
       ;;
    *-*-os2-emx*)
       AC_DEFINE(OS2, 1, [Compiling on OS/2 EMX])
       ;;
esac
fi

AM_CONDITIONAL(MACOSX, test x"$macosx" == x"1")
AM_CONDITIONAL(EMSCRIPTEN, test x"$enable_emscripten" == x"yes")

dnl The target cpu checks for dynamic cores
AH_TEMPLATE(C_TARGETCPU,[The type of cpu this target has])
AC_MSG_CHECKING(for target cpu type) 
case "$host_cpu" in
  x86_64 | amd64)
    AC_DEFINE(C_TARGETCPU,X86_64)
    AC_MSG_RESULT(x86-64 bit compatible)
    c_targetcpu="x86_64"
    c_unalignedmemory=yes
    ;;
  i?86)
    AC_DEFINE(C_TARGETCPU,X86)
    AC_MSG_RESULT(x86 compatible)
    c_targetcpu="x86"
    c_unalignedmemory=yes
    ;;
   powerpc*)
    AC_DEFINE(C_TARGETCPU,POWERPC)
    AC_MSG_RESULT(Power PC)
    c_targetcpu="powerpc"
    c_unalignedmemory=yes
    ;;
   m68k*)
    AC_DEFINE(C_TARGETCPU,M68K)
    AC_MSG_RESULT(Motorola 68000)
    c_targetcpu="m68k"
    c_unalignedmemory=yes
    ;;
   *)
    AC_DEFINE(C_TARGETCPU,UNKNOWN)
    AC_MSG_RESULT(unknown)
    c_unalignedmemory=no
    ;;
esac

dnl TODO: Need GCC to know this code is using C++ lamda functions

dnl LIBRARY TEST: ALSA
AM_PATH_ALSA(0.9.0, AC_DEFINE(HAVE_ALSA,1,[Define to 1 to use ALSA for MIDI]) , : )

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

dnl Some stuff for the icon.
case "$host" in
    *-*-cygwin* | *-*-mingw32*)
       dnl MinGW puts ncurses headers in a subdir
       ncursescfg=`ncursesw6-config --cflags`
       CXXFLAGS="$CXXFLAGS $ncursescfg"
       CPPFLAGS="$CPPFLAGS $ncursescfg"
       CFLAGS="$CFLAGS $ncursescfg"
    ;;
    *)
       WINDRES=":"
    ;;
esac

dnl LIBRARY TEST: ncurses
AC_CHECK_HEADER(curses.h,have_curses_h=yes,)
AC_CHECK_LIB(curses, initscr, have_curses_lib=yes, , )
AC_CHECK_LIB(ncurses, initscr, have_ncurses_lib=yes, , )
AC_CHECK_LIB(pdcurses, initscr, have_pdcurses_lib=yes, , )
# Check if this system's ncurses uses a separate tinfo library
AC_CHECK_LIB(tinfo, nodelay,
	     if test x$have_ncurses_lib = xyes ; then
	       LIBS="$LIBS -ltinfo";
	     fi)

dnl LIBRARY TEST: libzlib
AC_CHECK_HEADER(zlib.h,have_zlib_h=yes,)
AC_CHECK_LIB(z, inflateEnd, have_zlib_lib=yes, ,)

dnl LIBRARY TEST: libpng
AC_CHECK_HEADER(png.h,have_png_h=yes,)
AC_CHECK_LIB(png, png_get_io_ptr, have_png_lib=yes, ,-lz)

dnl LIBRARY TEST: libpcap
AC_CHECK_HEADER(pcap.h,have_pcap_h=yes,)
AC_CHECK_LIB(pcap, pcap_open_live, have_pcap_lib=yes, ,-lz)

dnl LIBRARY TEST: SDLnet 1.x
AC_CHECK_HEADER(SDL_net.h,have_sdl_net_h=yes,)
AC_CHECK_LIB(SDL_net, SDLNet_Init, have_sdl_net_lib=yes, , )

if test x$enable_emscripten != xyes; then
  dnl LIBRARY TEST: X11 Xlib support
  AC_CHECK_LIB(X11, main, have_x11_lib=yes, have_x11_lib=no, )
  AC_CHECK_HEADER(X11/XKBlib.h, have_x11_h=yes, have_x11_h=no, )
fi

dnl LIBRARY TEST: Fluidsynth support
AC_CHECK_HEADER(fluidsynth.h,have_fluidsynth_h=yes,)
AC_CHECK_LIB(fluidsynth, fluid_synth_sysex, have_fluidsynth_lib=yes,,)

dnl LIBRARY TEST: FFMPEG support
AC_CHECK_LIB(avcodec,avcodec_open2,have_ffmpeg=yes,have_ffmpeg=no,)
AC_CHECK_HEADER(libavcodec/avcodec.h,have_avcodec_h=yes,have_avcodec_h=no,)

dnl LIBRARY TEST: OpenGL support
AC_CHECK_LIB(GL, main, have_gl_lib=yes, have_gl_lib=no , )
AC_CHECK_LIB(opengl32, main, have_opengl32_lib=yes,have_opengl32_lib=no , )
AC_CHECK_HEADER(GL/gl.h, have_gl_h=yes , have_gl_h=no , )

dnl LIBRARY TEST: Direct3D 9 header support
AC_CHECK_HEADER(d3d9.h, have_d3d9_h=yes , have_d3d9_h=no , )
AC_CHECK_HEADER(d3dx9math.h, have_d3dx9math_h=yes , have_d3dx9math_h=no , )

dnl ================== SDL net special test for OS/2
if test x$host = xi386-pc-os2-emx ; then
  LIBS_BACKUP=$LIBS;
  LIBS="$LIBS -lSDL_Net";
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <SDL_Net.h>]],[[
	SDLNet_Init ();
  ]])], [AC_MSG_RESULT(yes); have_sdl_net_lib=yes], AC_MSG_RESULT(no))
  LIBS=$LIBS_BACKUP
fi

dnl FEATURE: xBRZ
AH_TEMPLATE(C_XBRZ,[Define to 1 to enable XBRZ scaler])
AC_ARG_ENABLE(xbrz,AC_HELP_STRING([--enable-xbrz],[compile with xBRZ scaler (default yes)]),enable_xbrz=$enableval,enable_xbrz=yes)
AM_CONDITIONAL(C_XBRZ, test "x$enable_xbrz" == "xyes")
if test x$enable_emscripten != xyes; then
  if test x$enable_xbrz = xyes; then
    AC_DEFINE(C_XBRZ,1)
  fi
fi

dnl FEATURE: MIDI through ALSA
AC_ARG_ENABLE(alsa-midi,
AC_HELP_STRING([--enable-alsa-midi],[compile with alsa midi support (default yes)]),
[ case "${enableval}" in
 yes) alsa_midi=true;;
 no)  alsa_midi=false;;
esac],
[alsa_midi=true])
if test x$alsa_midi = xtrue ; then 
  CXXFLAGS="$CXXFLAGS $ALSA_CFLAGS"
fi

dnl FEATURE: MT32 (MUNT) emulation
AH_TEMPLATE(C_MT32,[Define to 1 to enable MT32 emulation (x86/x86_64 only)])
AC_ARG_ENABLE(mt32,AC_HELP_STRING([--disable-mt32],[Disable MT32 emulation]),,enable_mt32=yes)
AC_MSG_CHECKING(whether MT32 emulation will be enabled) 

# test
# Note that MT32 (MUNT) requires the SSE extensions
if test x$enable_mt32 = xyes ; then 
  case "$host_cpu" in
    x86_64 | amd64)
      AC_MSG_RESULT(yes)
      AC_DEFINE(C_MT32,1)
      CXXFLAGS="$CXXFLAGS -mmmx -msse -msse2"
      ;;
    i?86)
      AC_MSG_RESULT(yes)
      AC_DEFINE(C_MT32,1)
      CXXFLAGS="$CXXFLAGS -mmmx -msse -msse2"
      ;;
    *)
      enable_mt32=no
      AC_MSG_RESULT(no)
      ;;
  esac
else
  enable_mt32=no
  AC_MSG_RESULT(no)
fi 
AM_CONDITIONAL(C_MT32, test "x$enable_mt32" == "xyes")

dnl NASM (Netwide Assembler)
AC_PATH_PROG([NASM], [nasm])
if test -z "$ac_cv_path_NASM"; then
    AC_MSG_WARN([NASM (netwide assembler) not found, you will not be able to compile the external x86 .asm files if they are modified])
fi

dnl LNKDOS16 (16-bit MS-DOS linker, DOSLIB project)
dnl To be fair to others compiling this code, search the PATH.
dnl If not found, let the developer know that they can git clone DOSLIB
dnl and use GNU Make + GCC to compile lnkdos16 in tool/linker in DOSLIB,
dnl then include the Linux binary in the PATH variable so this script can find it.
AC_PATH_PROG([LNKDOS16], [lnkdos16])
if test -z "$ac_cv_path_LNKDOS16"; then
    AC_MSG_WARN([LNKDOS16 (DOSLIB 16-bit linker) not found, you will not be able to compile the external x86 .asm files if they are modified.])
    AC_MSG_WARN([LNKDOS16 can be found in the DOSLIB project under tool/linker. Compile it using GNU make and GCC and then either install it])
    AC_MSG_WARN([on your system or run this configure script again with PATH directed at the compiled binary in the linux-host directory.])
fi

dnl FEATURE: DEBUG and HEAVY DEBUG options (debugger)
AH_TEMPLATE(C_DEBUG,[Define to 1 to enable internal debugger, requires libcurses])
AH_TEMPLATE(C_HEAVY_DEBUG,[Define to 1 to enable heavy debugging, also have to enable C_DEBUG])
AC_ARG_ENABLE(debug,AC_HELP_STRING([--enable-debug],[Enable debug mode]),[
   if test x$enable_debug = xno; then
     AC_MSG_RESULT([Debugger not enabled])
   elif test x$have_curses_lib = xyes -a x$have_curses_h = xyes ; then
     LIBS="$LIBS -lcurses"
     AC_DEFINE(C_DEBUG,1)
     if test x$enable_debug = xheavy ; then 
       AC_DEFINE(C_HEAVY_DEBUG,1)
     fi
   elif test x$have_ncurses_lib = xyes -a x$have_curses_h = xyes ; then
     LIBS="$LIBS -lncurses"
     AC_DEFINE(C_DEBUG,1)
     if test x$enable_debug = xheavy ; then 
       AC_DEFINE(C_HEAVY_DEBUG,1)
     fi
   elif test x$have_pdcurses_lib = xyes -a x$have_curses_h = xyes ; then
     LIBS="$LIBS -lpdcurses"
     AC_DEFINE(C_DEBUG,1)
     if test x$enable_debug = xheavy ; then 
       AC_DEFINE(C_HEAVY_DEBUG,1)
     fi
   else 
     AC_MSG_ERROR([Can't find curses, which is required for debug mode])
   fi
],)

dnl FEATURE: Whether to use inline functions for memory access
AH_TEMPLATE(C_CORE_INLINE,[Define to 1 to use inlined memory functions in cpu core])
AC_ARG_ENABLE(core-inline,AC_HELP_STRING([--enable-core-inline],[Enable inlined memory handling in CPU Core]),[
  if test x$enable_core_inline = xyes ; then 
    AC_MSG_RESULT([enabling inlined memory handling in CPU Core])
    AC_DEFINE(C_CORE_INLINE,1)
  fi
],)

dnl FEATURE: Whether to enable dynamic core
AC_ARG_ENABLE(dynamic-core,AC_HELP_STRING([--disable-dynamic-core],[Disable all dynamic cores]),,enable_dynamic_core=yes)

dnl FEATURE: Whether to enable x86 dynamic core
AH_TEMPLATE(C_DYNAMIC_X86,[Define to 1 to use x86 dynamic cpu core])
AC_ARG_ENABLE(dynamic-x86,AC_HELP_STRING([--disable-dynamic-x86],[Disable x86 dynamic cpu core]),,enable_dynamic_x86=yes)
AC_MSG_CHECKING(whether x86 dynamic cpu core will be enabled) 
if test x$enable_dynamic_x86 = xno -o x$enable_dynamic_core = xno; then 
   AC_MSG_RESULT(no)
else
  if test x$c_targetcpu = xx86 ; then
      AC_DEFINE(C_DYNAMIC_X86,32)
      AC_MSG_RESULT(yes)
  elif test x$c_targetcpu = xx86_64 ; then
      true
dnl     TODO
dnl      AC_DEFINE(C_DYNAMIC_X86,64)
dnl      AC_MSG_RESULT(yes)
  else
      AC_MSG_RESULT(no)
  fi
fi

dnl FEATURE: Whether to emulate the floating point unit
AH_TEMPLATE(C_FPU,[Define to 1 to enable floating point emulation])
AC_ARG_ENABLE(fpu,AC_HELP_STRING([--disable-fpu],[Disable fpu support]),,enable_fpu=yes)
AC_MSG_CHECKING(whether fpu emulation will be enabled) 
if test x$enable_fpu = xyes ; then 
  AC_MSG_RESULT(yes)
  AC_DEFINE(C_FPU,1)
else 
  AC_MSG_RESULT(no)
fi 

dnl FEATURE: Whether to enable unaligned memory access
AH_TEMPLATE(C_UNALIGNED_MEMORY,[Define to 1 to use a unaligned memory access])
AC_ARG_ENABLE(unaligned_memory,AC_HELP_STRING([--disable-unaligned-memory],[Disable unaligned memory access]),,enable_unaligned_memory=yes)
AC_MSG_CHECKING(whether to enable unaligned memory access) 
if test x$enable_unaligned_memory = xyes -a x$c_unalignedmemory = xyes; then 
  AC_DEFINE(C_UNALIGNED_MEMORY,1)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

dnl FEATURE: Whether to support direct serial port passthrough
AC_DEFINE(C_DIRECTSERIAL, 1, [ Define to 1 if you want serial passthrough support (Win32, Posix and OS/2).])

dnl FEATURE: Whether to support SDL net, and emulate modem and IPX connections
AH_TEMPLATE(C_SDL_NET,[Indicate whether SDL_net is present])
AH_TEMPLATE(C_MODEM,[Define to 1 to enable internal modem support, requires SDL_net])
AH_TEMPLATE(C_IPX,[Define to 1 to enable IPX over Internet networking, requires SDL_net])

# HACK: Don't enable SDL2 net if SDL2
if test -z "$SDL2_LIBS"; then
  if test x$have_sdl_net_lib = xyes -a x$have_sdl_net_h = xyes ; then
   LIBS="$LIBS -lSDL_net"
   AC_DEFINE(C_SDL_NET,1)
   AC_DEFINE(C_MODEM,1)
   AC_DEFINE(C_IPX,1)
  else 
   AC_MSG_WARN([Can't find SDL_net, internal modem and ipx disabled])
  fi
fi

dnl FEATURE: Whether to support libz, and enable snapshots
AH_TEMPLATE(C_LIBZ,[Define to 1 if you have libz])
if test x$have_zlib_lib = xyes -a x$have_zlib_h = xyes ; then
  LIBS="$LIBS -lz"
  AC_DEFINE(C_LIBZ,1)
fi

dnl FEATURE: Whether to support libpng, and enable snapshots
AH_TEMPLATE(C_LIBPNG,[Define to 1 if you have libpng])
AH_TEMPLATE(C_SSHOT,[Define to 1 to enable screenshots, requires libpng])
if test x$have_png_lib = xyes -a x$have_png_h = xyes ; then
  LIBS="$LIBS -lpng -lz"
  AC_DEFINE(C_LIBPNG,1)
  AC_DEFINE(C_SSHOT,1)
else
  AC_MSG_WARN([Can't find libpng, screenshot support disabled])
fi

dnl FEATURE: Whether to use libpcap, and enable NE2000 emulation
AH_TEMPLATE(C_NE2000,[Define to 1 to enable NE2000 ethernet passthrough, requires libpcap])
if test x$have_pcap_lib = xyes -a x$have_pcap_h = xyes ; then
  LIBS="$LIBS -lpcap";
  AC_DEFINE(C_NE2000,1)
else
  AC_MSG_WARN([Can't find libpcap, NE2000 ethernet passthrough disabled])
fi

if test x$enable_emscripten != xyes; then
  dnl FEATURE: Whether to use X11 XKBlib
  AH_TEMPLATE(C_X11_XKB,[define to 1 if you have XKBlib.h and X11 lib])
  AC_MSG_CHECKING(for XKBlib support)
  if test x$have_x11_lib = xyes -a x$have_x11_h = xyes ; then
    LIBS="$LIBS -lX11 -lxkbfile"
    AC_DEFINE(C_X11_XKB,1)
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
fi

if test x$enable_emscripten != xyes; then
  dnl FEATURE: Whether to use X11 XRandR
  AH_TEMPLATE(C_X11_XRANDR,[define to 1 if you have XRandr.h and X11 lib])
  AC_MSG_CHECKING(for XRandR support)
  if test x$have_x11_lib = xyes -a x$have_x11_h = xyes ; then
    LIBS="$LIBS -lXrandr"
    AC_DEFINE(C_X11_XRANDR,1)
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
fi

dnl FEATURE: Whether to use Fluidsynth
AH_TEMPLATE(C_FLUIDSYNTH,[Define to 1 to enable fluidsynth MIDI synthesis])
if test x$have_fluidsynth_lib = xyes -a x$have_fluidsynth_h = xyes ; then
  LIBS="$LIBS -lfluidsynth"
  AC_DEFINE(C_FLUIDSYNTH,1)
else
  AC_MSG_WARN([fluidsynth MIDI synthesis not available])
fi

dnl FEATURE: FFMPEG output support
AH_TEMPLATE(C_AVCODEC,[Define to 1 to use FFMPEG libavcodec for video capture])
AC_ARG_ENABLE(avcodec,AC_HELP_STRING([--disable-avcodec],[Disable FFMPEG avcodec support]),,enable_ffmpeg=yes)
if test x$enable_avcodec = xyes; then
	CFLAGS="$CFLAGS "`pkg-config libavcodec --cflags`
	CPPFLAGS="$CPPFLAGS "`pkg-config libavcodec --cflags`
	LIBS="$LIBS -lavcodec -lavformat -lavutil -lswscale "`pkg-config libavcodec --libs`
fi
if test x$enable_avcodec = xyes; then
    if test x$have_ffmpeg = xyes; then
        if test x$have_avcodec_h = xyes; then
            AC_MSG_RESULT(yes)
            AC_DEFINE(C_AVCODEC,1)
        fi
    fi
fi

dnl FEATURE: Whether to use OpenGL
AH_TEMPLATE(C_OPENGL,[Define to 1 to use opengl display output support])
AC_ARG_ENABLE(opengl,AC_HELP_STRING([--disable-opengl],[Disable opengl support]),,enable_opengl=yes)
AC_MSG_CHECKING(whether opengl display output will be enabled)

# HACK: Don't enable OPENGL on SDL2
if test -n "$SDL2_LIBS"; then
    enable_opengl=no
fi

# test
if test x$enable_opengl = xyes; then
case "$host" in
    *-*-darwin*)
       AC_MSG_RESULT(yes)
       LIBS="$LIBS -framework OpenGL"
       AC_DEFINE(C_OPENGL,1)
       ;;
    *)
       if test x$have_gl_h = xyes -a x$have_gl_lib = xyes ; then
         AC_MSG_RESULT(yes)
         LIBS="$LIBS -lGL"
         AC_DEFINE(C_OPENGL,1)
       elif test x$have_gl_h = xyes -a x$have_opengl32_lib = xyes ; then
         AC_MSG_RESULT(yes)
         LIBS="$LIBS -lopengl32"
         AC_DEFINE(C_OPENGL,1)
       else
         AC_MSG_RESULT(no)
       fi
       ;;
esac
fi

dnl FEATURE: Whether to use Direct3D 9 output
AH_TEMPLATE(HAVE_D3D9_H,[Define to 1 to use Direct3D 9 display output support])
AC_ARG_ENABLE(d3d9,AC_HELP_STRING([--enable-d3d9],[Enable Direct3D 9 support]),enable_d3d9=$enableval,enable_d3d9=no)
AC_MSG_CHECKING(whether Direct3D 9 display output will be enabled)

dnl FEATURE: Direct3D9 shaders
AH_TEMPLATE(C_D3DSHADERS,[Define to 1 to use Direct3D shaders])
AC_ARG_ENABLE(d3d-shaders,AC_HELP_STRING([--enable-d3d-shaders],[Enable Direct3D shaders]),enable_d3d_shaders=$enableval,enable_d3d_shaders=no)

# HACK: Don't enable Direct 3D 9 on SDL2
if test -n "$SDL2_LIBS"; then
    enable_d3d9=no
    enable_d3d_shaders=no
fi

if test x$enable_d3d9 = xyes; then
case "$host" in
    *-*-cygwin* | *-*-mingw32*)
      if test x$have_d3d9_h = xyes -a x$have_d3dx9math_h = xyes ; then
        AC_MSG_RESULT(yes)
        AC_DEFINE(HAVE_D3D9_H,1)
	if test x$enable_d3d_shaders = xyes; then
		AC_DEFINE(C_D3DSHADERS,1)
	fi
	  else
        AC_MSG_RESULT(no)
      fi
    ;;
    *)
     AC_MSG_RESULT(no)
    ;;
esac
else
  AC_MSG_RESULT(no)
fi

AC_CONFIG_FILES([ 
Makefile
src/Makefile
src/cpu/Makefile
src/cpu/core_full/Makefile
src/cpu/core_normal/Makefile
src/debug/Makefile
src/dos/Makefile
src/fpu/Makefile
src/gui/Makefile
src/mt32/Makefile
src/hardware/Makefile
src/hardware/serialport/Makefile
src/hardware/reSID/Makefile
src/hardware/parport/Makefile
src/aviwriter/Makefile
src/ints/Makefile
src/libs/Makefile
src/libs/zmbv/Makefile
src/libs/gui_tk/Makefile
src/libs/porttalk/Makefile
src/output/Makefile
src/builtin/Makefile
src/misc/Makefile
src/shell/Makefile
src/platform/Makefile
src/xBRZ/Makefile
include/Makefile
dosbox-x.spec
make-rpm.sh
])
AC_OUTPUT

# HACK: it's a script...
chmod +x make-rpm.sh

# HACK: Write all PACKAGE defines to vs2015/config_package.h so Windows builds are kept in sync
grep -E 'PACKAGE|VERSION' config.h >vs2015/config_package.h

