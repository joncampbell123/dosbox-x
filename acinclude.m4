dnl AM_PATH_SDL2([MINIMUM-VERSION])
AC_DEFUN([AM_PATH_SDL2],
[
AC_ARG_WITH(sdl2-prefix,[  --with-sdl2-prefix=PFX   Prefix where SDL2 is installed (optional)],
            sdl2_prefix="$withval", sdl2_prefix="")
AC_ARG_WITH(sdl2-exec-prefix,[  --with-sdl2-exec-prefix=PFX Exec prefix where SDL2 is installed (optional)],
            sdl2_exec_prefix="$withval", sdl2_exec_prefix="")
AC_ARG_ENABLE(sdl2test, [  --disable-sdl2test      Do not try to compile and run a test SDL program],
		    enable_sdl2test=no, enable_sdl2test=yes)
AC_ARG_ENABLE(sdl2,     [  --enable-sdl2           Enable SDL 2.x],
		    enable_sdl2enable=$enableval, enable_sdl2enable=no)

  AH_TEMPLATE(C_SDL2,[Set to 1 to enable SDL 2.x support])

  SDL2_CONFIG=no
  if test x$enable_sdl2enable = xyes ; then
    if test x$sdl2_exec_prefix != x ; then
      sdl2_args="$sdl2_args --exec-prefix=$sdl2_exec_prefix"
      if test x${SDL2_CONFIG+set} != xset ; then
        SDL2_CONFIG=$sdl2_exec_prefix/bin/sdl2-config
      fi
    fi
    if test x$sdl2_prefix != x ; then
      sdl2_args="$sdl2_args --prefix=$sdl2_prefix"
      if test x${SDL2_CONFIG+set} != xset ; then
        SDL2_CONFIG=$sdl2_prefix/bin/sdl2-config
      fi
    fi

    if test -x vs2015/sdl2/linux-host/bin/sdl2-config ; then
      SDL2_CONFIG=vs2015/sdl2/linux-host/bin/sdl2-config
      PATH=vs2015/sdl2/linux-host/bin:$PATH
    fi

    AC_PATH_PROG(SDL2_CONFIG, sdl2-config, no)
    min_sdl2_version=ifelse([$1], ,0.11.0,$1)
    AC_MSG_CHECKING(for SDL2 - version >= $min_sdl2_version)
    no_sdl2=""
    if test "$SDL2_CONFIG" = "no" ; then
      no_sdl2=yes
    else
      SDL2_CFLAGS=`$SDL2_CONFIG $sdl2conf_args --cflags`
      SDL2_LIBS=`$SDL2_CONFIG $sdl2conf_args --libs`
      AC_DEFINE(C_SDL2,1)
    fi
  fi

  AC_SUBST(SDL2_CFLAGS)
  AC_SUBST(SDL2_LIBS)
])

dnl AM_PATH_SDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for SDL, and define SDL_CFLAGS and SDL_LIBS
dnl
AC_DEFUN([AM_PATH_SDL],
[dnl 
dnl Get the cflags and libraries from the sdl-config script
dnl
AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)],
            sdl_prefix="$withval", sdl_prefix="")
AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)],
            sdl_exec_prefix="$withval", sdl_exec_prefix="")
AC_ARG_ENABLE(sdltest,      [  --disable-sdltest       Do not try to compile and run a test SDL program],
		    enable_sdltest=no, enable_sdltest=yes)
AC_ARG_ENABLE(sdl, [  --enable-sdl            Enable SDL 1.x],
		    enable_sdlenable=$enableval, enable_sdlenable=yes)

  AH_TEMPLATE(C_SDL1,[Set to 1 to enable SDL 1.x support])

  SDL_CONFIG=no
  if test x$enable_sdlenable = xyes ; then
    if test x$sdl_exec_prefix != x ; then
      sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
      if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_exec_prefix/bin/sdl-config
      fi
    fi
    if test x$sdl_prefix != x ; then
      sdl_args="$sdl_args --prefix=$sdl_prefix"
      if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_prefix/bin/sdl-config
      fi
    fi

    if test -x vs2015/sdl/linux-host/bin/sdl-config ; then
      SDL_CONFIG=vs2015/sdl/linux-host/bin/sdl-config
      PATH=vs2015/sdl/linux-host/bin:$PATH
    fi

    AC_PATH_PROG(SDL_CONFIG, sdl-config, no)
    min_sdl_version=ifelse([$1], ,0.11.0,$1)
    AC_MSG_CHECKING(for SDL - version >= $min_sdl_version)
    no_sdl=""
    if test "$SDL_CONFIG" = "no" ; then
      no_sdl=yes
    else
      SDL_CFLAGS=`$SDL_CONFIG $sdlconf_args --cflags`
      SDL_LIBS=`$SDL_CONFIG $sdlconf_args --libs`
      AC_DEFINE(C_SDL1,1)
    fi
  fi

  AC_SUBST(SDL_CFLAGS)
  AC_SUBST(SDL_LIBS)
])

dnl Configure Paths for Alsa
dnl Some modifications by Richard Boulton <richard-alsa@tartarus.org>
dnl Christopher Lansdown <lansdoct@cs.alfred.edu>
dnl Jaroslav Kysela <perex@suse.cz>
dnl Last modification: alsa.m4,v 1.22 2002/05/27 11:14:20 tiwai Exp
dnl AM_PATH_ALSA([MINIMUM-VERSION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libasound, and define ALSA_CFLAGS and ALSA_LIBS as appropriate.
dnl enables arguments --with-alsa-prefix=
dnl                   --with-alsa-enc-prefix=
dnl                   --disable-alsatest  (this has no effect, as yet)
dnl
dnl For backwards compatibility, if ACTION_IF_NOT_FOUND is not specified,
dnl and the alsa libraries are not found, a fatal AC_MSG_ERROR() will result.
dnl
AC_DEFUN([AM_PATH_ALSA],
[dnl Save the original CFLAGS, LDFLAGS, and LIBS
alsa_save_CFLAGS="$CFLAGS"
alsa_save_LDFLAGS="$LDFLAGS"
alsa_save_LIBS="$LIBS"
alsa_found=yes

dnl
dnl Get the cflags and libraries for alsa
dnl
AC_ARG_WITH(alsa-prefix,
[  --with-alsa-prefix=PFX  Prefix where Alsa library is installed(optional)],
[alsa_prefix="$withval"], [alsa_prefix=""])

AC_ARG_WITH(alsa-inc-prefix,
[  --with-alsa-inc-prefix=PFX  Prefix where include libraries are (optional)],
[alsa_inc_prefix="$withval"], [alsa_inc_prefix=""])

dnl FIXME: this is not yet implemented
AC_ARG_ENABLE(alsatest,
[  --disable-alsatest      Do not try to compile and run a test Alsa program],
[enable_alsatest=no],
[enable_alsatest=yes])

dnl Add any special include directories
AC_MSG_CHECKING(for ALSA CFLAGS)
if test "$alsa_inc_prefix" != "" ; then
	ALSA_CFLAGS="$ALSA_CFLAGS -I$alsa_inc_prefix"
	CFLAGS="$CFLAGS -I$alsa_inc_prefix"
fi
AC_MSG_RESULT($ALSA_CFLAGS)

dnl add any special lib dirs
AC_MSG_CHECKING(for ALSA LDFLAGS)
if test "$alsa_prefix" != "" ; then
	ALSA_LIBS="$ALSA_LIBS -L$alsa_prefix"
	LDFLAGS="$LDFLAGS $ALSA_LIBS"
fi

dnl add the alsa library
ALSA_LIBS="$ALSA_LIBS -lasound -lm -ldl -lpthread"
LIBS=`echo $LIBS | sed 's/-lm//'`
LIBS=`echo $LIBS | sed 's/-ldl//'`
LIBS=`echo $LIBS | sed 's/-lpthread//'`
LIBS=`echo $LIBS | sed 's/  //'`
LIBS="$ALSA_LIBS $LIBS"
AC_MSG_RESULT($ALSA_LIBS)

dnl Check for a working version of libasound that is of the right version.
min_alsa_version=ifelse([$1], ,0.1.1,$1)
AC_MSG_CHECKING(for libasound headers version >= $min_alsa_version)
no_alsa=""
    alsa_min_major_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    alsa_min_minor_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    alsa_min_micro_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

AC_LANG_SAVE
AC_LANG_C
AC_TRY_COMPILE([
#include <alsa/asoundlib.h>
], [
/* ensure backward compatibility */
#if !defined(SND_LIB_MAJOR) && defined(SOUNDLIB_VERSION_MAJOR)
#define SND_LIB_MAJOR SOUNDLIB_VERSION_MAJOR
#endif
#if !defined(SND_LIB_MINOR) && defined(SOUNDLIB_VERSION_MINOR)
#define SND_LIB_MINOR SOUNDLIB_VERSION_MINOR
#endif
#if !defined(SND_LIB_SUBMINOR) && defined(SOUNDLIB_VERSION_SUBMINOR)
#define SND_LIB_SUBMINOR SOUNDLIB_VERSION_SUBMINOR
#endif

#  if(SND_LIB_MAJOR > $alsa_min_major_version)
  exit(0);
#  else
#    if(SND_LIB_MAJOR < $alsa_min_major_version)
#       error not present
#    endif

#   if(SND_LIB_MINOR > $alsa_min_minor_version)
  exit(0);
#   else
#     if(SND_LIB_MINOR < $alsa_min_minor_version)
#          error not present
#      endif

#      if(SND_LIB_SUBMINOR < $alsa_min_micro_version)
#        error not present
#      endif
#    endif
#  endif
exit(0);
],
  [AC_MSG_RESULT(found.)],
  [AC_MSG_RESULT(not present.)
   ifelse([$3], , [AC_MSG_ERROR(Sufficiently new version of libasound not found.)])
   alsa_found=no]
)
AC_LANG_RESTORE

dnl Now that we know that we have the right version, let's see if we have the library and not just the headers.
AC_CHECK_LIB([asound], [snd_ctl_open],,
	[ifelse([$3], , [AC_MSG_ERROR(No linkable libasound was found.)])
	 alsa_found=no]
)

if test "x$alsa_found" = "xyes" ; then
   ifelse([$2], , :, [$2])
   LIBS=`echo $LIBS | sed 's/-lasound//g'`
   LIBS=`echo $LIBS | sed 's/  //'`
   LIBS="-lasound $LIBS"
fi
if test "x$alsa_found" = "xno" ; then
   ifelse([$3], , :, [$3])
   CFLAGS="$alsa_save_CFLAGS"
   LDFLAGS="$alsa_save_LDFLAGS"
   LIBS="$alsa_save_LIBS"
   ALSA_CFLAGS=""
   ALSA_LIBS=""
fi

dnl That should be it.  Now just export out symbols:
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
])

AH_TOP([
/*
 *  Copyright (C) 2002-2020  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
])

AH_BOTTOM([#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif])

AH_BOTTOM([#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif])


AH_BOTTOM([#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif])

AH_BOTTOM([#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif])

dnl These custom typedefs are unnecessary and should be deprecated.
dnl Linux systems for ages now have had stdint.h to define uint8_t, etc.
AH_BOTTOM([
#include <cstdint>

typedef uintptr_t Bitu;
typedef intptr_t Bits;
])
