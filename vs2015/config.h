/* config.h.in.  Generated from configure.ac by autoheader.  */
/* Hand-edited by Jonathan Campbell for Visual Studio 2008 */

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

/* DOSBox-X currently targets Windows XP or higher. */
/* TODO: Can we drop this to 0x500 for Windows 2000? */
/* TODO: What is the minimum appropriate WINVER for HX DOS extender? */
#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

/* Define if building universal (internal helper macro) */
#undef AC_APPLE_UNIVERSAL_BUILD

/* Compiling on BSD */
#undef BSD

/* Determines if the compilers supports always_inline attribute. */
#undef C_ATTRIBUTE_ALWAYS_INLINE

/* Determines if the compilers supports fastcall attribute. */
#undef C_ATTRIBUTE_FASTCALL

/* Define to 1 to use inlined memory functions in cpu core */
#define C_CORE_INLINE	1

/* Indicate whether SDL_net is present */
#define C_SDL_NET 1

/* Define to 1 if you have the <d3d9.h> header file. */
#if !defined(C_SDL2)
#define HAVE_D3D9_H 1
#endif

#if HAVE_D3D9_H
/* Define to 1 if you want to add Direct3D output to the list of available outputs */
#define C_DIRECT3D 1
/* Define to 1 to use Direct3D shaders, requires d3d9.h and libd3dx9 */
#define C_D3DSHADERS 1
#endif

/* MT32 (munt) emulation */
#define C_MT32 1

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 1

/* Define to 1 if you want parallel passthrough support (Win32, Linux). */
#define C_DIRECTLPT 1

/* Define to 1 if you want serial passthrough support (Win32, Posix and OS/2).
   */
#define C_DIRECTSERIAL 1

#if defined (_M_AMD64)
/* The type of cpu this target has */
# define C_TARGETCPU X86_64
/* Define to 1 to use x86 dynamic cpu core */
# define C_DYNAMIC_X86 1
# define C_DYNREC 1
#elif defined (_M_ARM64) || defined (_M_ARM) /* Microsoft C++ amd64, arm32 and arm64 */
# undef C_TARGETCPU
# undef C_DYNAMIC_X86
# define C_DYNREC 1
#else
# define C_TARGETCPU X86
# define C_DYNAMIC_X86 1
# define C_DYNREC 1
#endif

/* Define to 1 to enable libfluidsynth MIDI synthesis */
#undef C_FLUIDSYNTH

/* Define to 1 to enable floating point emulation */
#define C_FPU					1

/* Define to 1 to use a x86/x64 assembly fpu core */
/* FIXME: VS2015 x86_64 will not allow inline asm! */
#ifdef _M_AMD64 /* Microsoft C++ amd64 */
//TODO
#elif defined(_M_ARM64) || defined (_M_ARM) /* Microsoft C++ arm32 and arm64 */
# undef C_FPU_X86
#else
# define C_FPU_X86 1
#endif

/* Define to 1 to enable freetype support */
#define C_FREETYPE 1

/* Determines if the compilers supports attributes for structures. */
#undef C_HAS_ATTRIBUTE

/* Determines if the compilers supports __builtin_expect for branch
   prediction. */
#undef C_HAS_BUILTIN_EXPECT

/* Define to 1 if you have the mprotect function */
#undef C_HAVE_MPROTECT

/* Define to 1 to enable heavy debugging, also have to enable C_DEBUG */
#define C_HEAVY_DEBUG 1

/* Define to 1 to enable IPX over Internet networking, requires SDL_net */
#define C_IPX 1

/* Define to 1 if you have libpng */
#define C_LIBPNG 1

/* Define to 1 to enable internal modem support, requires SDL_net */
#define C_MODEM 1

/* Define to 1 to enable internal printer redirection support*/
#define C_PRINTER 1

/* Define to 1 to enable ethernet passthrough, requires libpcap */
#define C_PCAP 1

/* Define to 1 to enable userspace TCP/IP emulation, requires libslirp */
/* #undef C_SLIRP */

/* Set to 1 to enable SDL 1.x support */
#define C_SDL1 1

/* Set to 1 to enable SDL 2.x support */
/* #undef C_SDL2 */

/* Define to 1 to use opengl display output support */
#if !defined(_M_ARM64) && !defined (_M_ARM)
# define C_OPENGL 1
#endif

/* Set to 1 to enable XBRZ support */
#define C_XBRZ 1

/* Set to 1 to enable scaler friendly but CPU intensive aspect ratio correction options (post-scalers) for 'surface' output */
/* Please note that this option includes small part of xBRZ code and uses task group parallelism like xBRZ (batch size is hardcoded here) */
#define C_SURFACE_POSTRENDER_ASPECT 1
#define C_SURFACE_POSTRENDER_ASPECT_BATCH_SIZE 16

/* Define to 1 if you have setpriority support */
#undef C_SET_PRIORITY

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to use a unaligned memory access */
#define C_UNALIGNED_MEMORY		1

/* define to 1 if you have XKBlib.h and X11 lib */
#undef C_X11_XKB

/* libm doesn't include powf */
#undef DB_HAVE_NO_POWF

/* struct dirent has d_type */
#undef DIRENT_HAS_D_TYPE

/* environ can be included */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#undef ENVIRON_LINKED

/* Define to 1 to use ALSA for MIDI */
#undef HAVE_ALSA

/* Define to 1 if you have the <ddraw.h> header file. */
#if !defined(C_SDL2)
#define HAVE_DDRAW_H 1
#endif

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the `asound' library (-lasound). */
#undef HAVE_LIBASOUND

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the <netinet/in.h> header file. */
#undef HAVE_NETINET_IN_H

/* Define to 1 if you have the <pwd.h> header file. */
#undef HAVE_PWD_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H		1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H		1

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h>	header file. */
#define HAVE_SYS_STAT_H		1

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Compiling on GNU/Linux */
#undef LINUX

/* Compiling on Mac OS X */
#undef MACOSX

/* Compiling on OS/2 EMX */
#undef OS2

/* Define to the address where bug reports for this package should be sent. */

/* Define to the full name of this package. */

/* Define to the full name and version of this package. */

/* Define to the one symbol short name of this package. */

/* Define to the home page for this package. */

/* Define to the version of this package. */

/* The size of `int *', as computed by sizeof. */
#if defined (_M_AMD64) || defined (_M_ARM64) /* Microsoft C++ amd64 and arm64*/
# define SIZEOF_INT_P				8
#else
# define SIZEOF_INT_P				4
#endif

/* The size of `unsigned char', as computed by sizeof. */
#define SIZEOF_UNSIGNED_CHAR		1

/* The size of `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT		4

/* The size of `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG		4

/* The size of `unsigned long long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG_LONG	8

/* The size of `unsigned short', as computed by sizeof. */
#define SIZEOF_UNSIGNED_SHORT	2

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
#undef TM_IN_SYS_TIME

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

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

#ifndef CONST
#define CONST const
#endif

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

/* Fuck off MSVC I don't care if some C library functions aren't POSIX compliant --J.C. */
#if defined(WIN32)
# pragma warning(disable:4996)
#endif

/* Linux-side configure script will write/rewrite this file so both Windows and Linux builds carry the same information --J.C. */
#include "config_package.h"

