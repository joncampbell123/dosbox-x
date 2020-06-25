/* modified for CMake build */

/* config.h.in.  Generated from configure.ac by autoheader.  */


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


/* Define if building universal (internal helper macro) */
#cmakedefine AC_APPLE_UNIVERSAL_BUILD @AC_APPLE_UNIVERSAL_BUILD@

/* Compiling on BSD */
#cmakedefine BSD @BSD@

/* Determines if the compilers supports always_inline attribute. */
#cmakedefine C_ATTRIBUTE_ALWAYS_INLINE @C_ATTRIBUTE_ALWAYS_INLINE@

/* Determines if the compilers supports fastcall attribute. */
#cmakedefine C_ATTRIBUTE_FASTCALL @C_ATTRIBUTE_FASTCALL@

/* Define to 1 to use FFMPEG libavcodec for video capture */
#cmakedefine C_AVCODEC @C_AVCODEC@

/* Define to 1 to use inlined memory functions in cpu core */
#cmakedefine C_CORE_INLINE @C_CORE_INLINE@

/* Define to 1 to enable internal debugger, requires libcurses */
#cmakedefine C_DEBUG @C_DEBUG@

/* Define to 1 if you want parallel passthrough support (Win32, Linux). */
#cmakedefine C_DIRECTLPT @C_DIRECTLPT@

/* Define to 1 if you want serial passthrough support (Win32, Posix and OS/2). */
#cmakedefine C_DIRECTSERIAL @C_DIRECTSERIAL@

/* Define to 1 to use x86 dynamic cpu core */
#cmakedefine C_DYNAMIC_X86 @C_DYNAMIC_X86@

/* Define to 1 to enable fluidsynth MIDI synthesis */
#cmakedefine C_FLUIDSYNTH @C_FLUIDSYNTH@

/* Define to 1 to enable floating point emulation */
#cmakedefine C_FPU @C_FPU@

/* Determines if the compilers supports attributes for structures. */
#cmakedefine HAVE___ATTRIBUTE__ @HAVE___ATTRIBUTE__@

/* Determines if the compilers supports __builtin_expect for branch prediction. */
#cmakedefine HAVE___BUILTIN_EXPECT

/* Define to 1 if you have the mprotect function */
#cmakedefine C_HAVE_MPROTECT @C_HAVE_MPROTECT@

/* Define to 1 to enable heavy debugging, also have to enable C_DEBUG */
#cmakedefine C_HEAVY_DEBUG @C_HEAVY_DEBUG@

/* Define to 1 to enable IPX over Internet networking, requires SDL_net */
#cmakedefine C_IPX @C_IPX@

/* Define to 1 if you have libpng */
#cmakedefine C_LIBPNG @C_LIBPNG@

/* Define to 1 to enable internal modem support, requires SDL_net */
#cmakedefine C_MODEM @C_MODEM@

/* Define to 1 to enable MT32 emulation (x86/x86_64 only) */
#cmakedefine C_MT32 @C_MT32@

/* Define to 1 to enable NE2000 ethernet passthrough, requires libpcap */
#cmakedefine C_NE2000 @C_NE2000@

/* Define to 1 to use opengl display output support */
#cmakedefine C_OPENGL @C_OPENGL@

/* Set to 1 to enable SDL 1.x support */
#cmakedefine C_SDL1 @C_SDL1@

/* Set to 1 to enable SDL 2.x support */
#cmakedefine C_SDL2 @C_SDL2@

/* Indicate whether SDL_net is present */
#cmakedefine C_SDL_NET @C_SDL_NET@

/* Define to 1 if you have setpriority support */
#cmakedefine C_SET_PRIORITY @C_SET_PRIORITY@

/* Define to 1 to enable screenshots, requires libpng */
#cmakedefine C_SSHOT @C_SSHOT@

/* The type of cpu this target has */
#undef C_TARGETCPU

/* Define to 1 to use a unaligned memory access */
#cmakedefine C_UNALIGNED_MEMORY @C_UNALIGNED_MEMORY@

/* define to 1 if you have XKBlib.h and X11 lib */
#undef C_X11_XKB

/* libm doesn't include powf */
#undef DB_HAVE_NO_POWF

/* struct dirent has d_type */
#undef DIRENT_HAS_D_TYPE

/* environ can be included */
#undef ENVIRON_INCLUDED

/* environ can be linked */
#undef ENVIRON_LINKED

/* Define to 1 to use ALSA for MIDI */
#undef HAVE_ALSA

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H @HAVE_INTTYPES_H@

/* Define to 1 if you have the `asound' library (-lasound). */
#undef HAVE_LIBASOUND

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H @HAVE_INTTYPES_H@

/* Define to 1 if you have the <netinet/in.h> header file. */
#cmakedefine HAVE_NETINET_IN_H @HAVE_NETINET_IN_H@

/* Define to 1 if you have the <pwd.h> header file. */
#cmakedefine HAVE_PWD_H @HAVE_PWD_H@

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H @HAVE_STDINT_H@

/* Define to 1 if you have the <stdlib.h> header file. */
#cmakedefine HAVE_STDLIB_H @HAVE_STDLIB_H@

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H @HAVE_STRINGS_H@

/* Define to 1 if you have the <string.h> header file. */
#cmakedefine HAVE_STRING_H @HAVE_STRING_H@

/* Define to 1 if you have the <sys/socket.h> header file. */
#cmakedefine HAVE_SYS_SOCKET_H @HAVE_SYS_SOCKET_H@

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H @HAVE_SYS_STAT_H@

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H @HAVE_SYS_TYPES_H@

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H @HAVE_UNISTD_H@

/* Compiling on GNU/Linux */
#undef LINUX

/* Compiling on Mac OS X */
#undef MACOSX

/* Compiling on OS/2 EMX */
#undef OS2

/* Name of package */
#define PACKAGE "dosbox-x"

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT "@PACKAGE_BUGREPORT@"

/* Define to the full name of this package. */
#cmakedefine PACKAGE_NAME "@PACKAGE_NAME@"

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING "@PACKAGE_STRING@"

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME "@PACKAGE_TARNAME@"

/* Define to the home page for this package. */
#cmakedefine PACKAGE_URL "@PACKAGE_URL@"

/* Define to the version of this package. */
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"

/* The size of `unsigned char', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_CHAR @SIZEOF_UNSIGNED_CHAR@

/* The size of `unsigned short', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_SHORT @SIZEOF_UNSIGNED_SHORT@

/* The size of `unsigned int', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_INT @SIZEOF_UNSIGNED_INT@

/* The size of `unsigned long', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_LONG @SIZEOF_UNSIGNED_LONG@

/* The size of `unsigned long long', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_LONG_LONG @SIZEOF_UNSIGNED_LONG_LONG@

/* The size of `int *', as computed by sizeof. */
#cmakedefine SIZEOF_INT_P @SIZEOF_INT_P@

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
#undef TM_IN_SYS_TIME

/* Version number of package */
#cmakedefine VERSION "@VERSION@"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#cmakedefine _FILE_OFFSET_BITS @_FILE_OFFSET_BITS@

/* Define for large files, on AIX-style hosts. */
#undef _LARGE_FILES

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

/* Define to `int` if you don't have socklen_t */
#undef socklen_t

#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif

#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif

#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif


typedef         double     Real64;

#if SIZEOF_UNSIGNED_CHAR != 1
#  error "sizeof (unsigned char) != 1"
#else
  typedef unsigned char Bit8u;
  typedef   signed char Bit8s;
#endif

#if SIZEOF_UNSIGNED_SHORT != 2
#  error "sizeof (unsigned short) != 2"
#else
  typedef unsigned short Bit16u;
  typedef   signed short Bit16s;
#endif

#if SIZEOF_UNSIGNED_INT == 4
  typedef unsigned int Bit32u;
  typedef   signed int Bit32s;
#elif SIZEOF_UNSIGNED_LONG == 4
  typedef unsigned long Bit32u;
  typedef   signed long Bit32s;
#else
#  error "can't find sizeof(type) of 4 bytes!"
#endif

#if SIZEOF_UNSIGNED_LONG == 8
  typedef unsigned long Bit64u;
  typedef   signed long Bit64s;
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
  typedef unsigned long long Bit64u;
  typedef   signed long long Bit64s;
#else
#  error "can't find data type of 8 bytes"
#endif

#if SIZEOF_INT_P == 4
  typedef Bit32u Bitu;
  typedef Bit32s Bits;
#else
  typedef Bit64u Bitu;
  typedef Bit64s Bits;
#endif


