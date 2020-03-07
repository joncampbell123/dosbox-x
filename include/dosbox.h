/*
 *  Copyright (C) 2002-2019  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "config.h"
#include "logging.h"

#if defined(C_ICONV)
/* good */
#elif defined(C_ICONV_WIN32)
/* good */
#else
/* auto-pick */
 #if defined(_WIN32) || defined(WINDOWS)
  #define C_ICONV_WIN32 1 /* use the Win32 API */
 #else
  #pragma warning "iconv backend not chosen, will become mandatory at some point"
 #endif
#endif

/* Mac OS X: There seems to be a problem with Macbooks where the touchpad
             is seen by SDL2 as a "touchscreen", even though the screen is
	     not a touchscreen. The result is DOSBox-X getting mixed messages
             from both the mouse cursor and the touch pad, which makes the
             interface unusable. The solution is to ignore touch events on
             Mac OS X.

             Perhaps if DOSBox-X is someday ported to run on an iPad (with
             a touchscreen) this can be made conditional to allow touch
             events there. */
#if defined(C_SDL2) && defined(MACOSX)
# define IGNORE_TOUCHSCREEN
#endif

/* SANITY CHECK */
#if defined(C_HEAVY_DEBUG) && !defined(C_DEBUG)
# error If C_HEAVY_DEBUG is defined, then so must C_DEBUG
#endif
/* ------------ */

#if defined(_MSC_VER)
# include <sys/types.h>
# include <sys/stat.h>

/* Microsoft has their own stat/stat64 scheme */
# define pref_stat			_stati64
# define pref_struct_stat	struct _stat64
#else
/* As long as FILE_OFFSET_BITS==64 Linux is quite good at allowing stat to work with 64-bit sizes */
# define pref_stat			stat
# define pref_struct_stat	struct stat
#endif

// TODO: The autoconf script should test the size of long double
#if defined(_MSC_VER)
// Microsoft C++ sizeof(long double) == sizeof(double)
#elif defined(__arm__)
// ARMv7 (Raspberry Pi) does not have long double, sizeof(long double) == sizeof(double)
#else
// GCC, other compilers, have sizeof(long double) == 10 80-bit IEEE
# define HAS_LONG_DOUBLE		1
#endif

GCC_ATTRIBUTE(noreturn) void		E_Exit(const char * format,...) GCC_ATTRIBUTE( __format__(__printf__, 1, 2));

typedef Bits cpu_cycles_count_t;
typedef Bitu cpu_cycles_countu_t;

#include "clockdomain.h"

class Config;
class Section;

#if defined(__GNUC__)
# define DEPRECATED __attribute__((deprecated))
#else
# define DEPRECATED
#endif

enum MachineType {
	MCH_HERC,
	MCH_CGA,
	MCH_TANDY,
	MCH_PCJR,
	MCH_EGA,
	MCH_VGA,
	MCH_AMSTRAD,
	MCH_PC98,

    MCH_FM_TOWNS,                    // STUB!!

    MCH_MCGA,                        // IBM PS/2 model 30 Multi-Color Graphics Adapter
    MCH_MDA
};

enum SVGACards {
	SVGA_None,
	SVGA_S3Trio,
	SVGA_TsengET4K,
	SVGA_TsengET3K,
	SVGA_ParadisePVGA1A
};

typedef Bitu				(LoopHandler)(void);

extern Config*				control;
extern SVGACards			svgaCard;
extern MachineType			machine;
extern bool             SDLNetInited, uselfn;
extern bool				mono_cga;
extern bool				DEPRECATED mainline_compatible_mapping;
extern bool				DEPRECATED mainline_compatible_bios_mapping;

#ifdef __SSE__
extern bool				sse1_available;
extern bool				sse2_available;
#endif

void					MSG_Add(const char*,const char*); //add messages to the internal languagefile
const char*				MSG_Get(char const *);     //get messages from the internal languagefile

void					DOSBOX_RunMachine();
void					DOSBOX_SetLoop(LoopHandler * handler);
void					DOSBOX_SetNormalLoop();

/* machine tests for use with if() statements */
#define IS_TANDY_ARCH			((machine==MCH_TANDY) || (machine==MCH_PCJR))
#define IS_EGAVGA_ARCH			((machine==MCH_EGA) || (machine==MCH_VGA))
#define IS_EGA_ARCH             (machine==MCH_EGA)
#define IS_VGA_ARCH             (machine==MCH_VGA)
#define IS_PC98_ARCH            (machine==MCH_PC98)

#define IS_FM_TOWNS             (machine==MCH_FM_TOWNS)

/* machine tests for use with switch() statements */
#define TANDY_ARCH_CASE			MCH_TANDY: case MCH_PCJR
#define EGAVGA_ARCH_CASE		MCH_EGA: case MCH_VGA
#define VGA_ARCH_CASE			MCH_VGA
#define PC98_ARCH_CASE			MCH_PC98

#define FM_TOWNS_ARCH_CASE      MCH_FM_TOWNS

#ifndef DOSBOX_LOGGING_H
#include "logging.h"
#endif // the logging system.

extern ClockDomain			clockdom_PCI_BCLK;
extern ClockDomain			clockdom_ISA_OSC;
extern ClockDomain			clockdom_ISA_BCLK;

signed long long time_to_clockdom(ClockDomain &src,double t);
unsigned long long update_clockdom_from_now(ClockDomain &dst);

extern bool enable_pc98_jump;

enum {
	UTF8ERR_INVALID=-1,
	UTF8ERR_NO_ROOM=-2
};

#ifndef UNICODE_BOM
#define UNICODE_BOM 0xFEFF
#endif

int utf8_encode(char **ptr,char *fence,uint32_t code);
int utf8_decode(const char **ptr,const char *fence);
int utf16le_encode(char **ptr,char *fence,uint32_t code);
int utf16le_decode(const char **ptr,const char *fence);

typedef char utf8_t;
typedef uint16_t utf16_t;

/* for DOS filename handling we want a toupper that uses the MS-DOS code page within not the locale of the host */
int ascii_toupper(int c);

enum {
    BOOTHAX_NONE=0,
    BOOTHAX_MSDOS
};

extern Bit32u guest_msdos_LoL;
extern Bit16u guest_msdos_mcb_chain;
extern int boothax;

/* C++11 user-defined literal, to help with byte units */
typedef unsigned long long bytecount_t;

static inline constexpr bytecount_t operator "" _bytes(const bytecount_t x) {
    return x;
}

static inline constexpr bytecount_t operator "" _parabytes(const bytecount_t x) { /* AKA bytes per segment increment in real mode */
    return x << bytecount_t(4u);
}

static inline constexpr bytecount_t operator "" _kibibytes(const bytecount_t x) {
    return x << bytecount_t(10u);
}

static inline constexpr bytecount_t operator "" _pagebytes(const bytecount_t x) { /* bytes per 4KB page in protected mode */
    return x << bytecount_t(12u);
}

static inline constexpr bytecount_t operator "" _mibibytes(const bytecount_t x) {
    return x << bytecount_t(20u);
}

static inline constexpr bytecount_t operator "" _gibibytes(const bytecount_t x) {
    return x << bytecount_t(30u);
}

static inline constexpr bytecount_t operator "" _tebibytes(const bytecount_t x) {
    return x << bytecount_t(40u);
}

/* and the same, for variables */

static inline constexpr bytecount_t _bytes(const bytecount_t x) {
    return x;
}

static inline constexpr bytecount_t _parabytes(const bytecount_t x) { /* AKA bytes per segment increment in real mode */
    return x << bytecount_t(4u);
}

static inline constexpr bytecount_t _kibibytes(const bytecount_t x) {
    return x << bytecount_t(10u);
}

static inline constexpr bytecount_t _pagebytes(const bytecount_t x) { /* bytes per 4KB page in protected mode */
    return x << bytecount_t(12u);
}

static inline constexpr bytecount_t _mibibytes(const bytecount_t x) {
    return x << bytecount_t(20u);
}

static inline constexpr bytecount_t _gibibytes(const bytecount_t x) {
    return x << bytecount_t(30u);
}

static inline constexpr bytecount_t _tebibytes(const bytecount_t x) {
    return x << bytecount_t(40u);
}

#endif /* DOSBOX_DOSBOX_H */
