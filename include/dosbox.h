/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "config.h"
#include "logging.h"

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
#else
// GCC, other compilers, have sizeof(long double) == 10 80-bit IEEE
# define HAS_LONG_DOUBLE		1
#endif

GCC_ATTRIBUTE(noreturn) void		E_Exit(const char * message,...) GCC_ATTRIBUTE( __format__(__printf__, 1, 2));

#include "clockdomain.h"

class Config;
class Section;

enum MachineType {
	MCH_HERC,
	MCH_CGA,
	MCH_TANDY,
	MCH_PCJR,
	MCH_EGA,
	MCH_VGA,
	MCH_AMSTRAD,
	MCH_PC98,

    MCH_FM_TOWNS                    // STUB!!
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
extern bool				SDLNetInited;
extern bool				mono_cga;
extern bool				mainline_compatible_mapping;
extern bool				mainline_compatible_bios_mapping;

#ifdef __SSE__
extern bool				sse1_available;
extern bool				sse2_available;
#endif

void					MSG_Add(const char*,const char*); //add messages to the internal languagefile
const char*				MSG_Get(char const *);     //get messages from the internal languagefile

void					DOSBOX_RunMachine();
void					DOSBOX_SetLoop(LoopHandler * handler);
void					DOSBOX_SetNormalLoop();
void					DOSBOX_Init(void);

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
extern ClockDomain			clockdom_8254_PIT;
extern ClockDomain			clockdom_8250_UART;

signed long long time_to_clockdom(ClockDomain &src,double t);
unsigned long long update_clockdom_from_now(ClockDomain &dst);
unsigned long long update_ISA_OSC_clock();
unsigned long long update_8254_PIT_clock();
unsigned long long update_ISA_BCLK_clock();
unsigned long long update_PCI_BCLK_clock();

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

#endif /* DOSBOX_DOSBOX_H */
