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

class Config;
class Section;

enum MachineType {
	MCH_HERC,
	MCH_CGA,
	MCH_TANDY,
	MCH_PCJR,
	MCH_EGA,
	MCH_VGA,
	MCH_AMSTRAD
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

GCC_ATTRIBUTE(noreturn) void		E_Exit(const char * message,...) GCC_ATTRIBUTE( __format__(__printf__, 1, 2));

void					MSG_Add(const char*,const char*); //add messages to the internal languagefile
const char*				MSG_Get(char const *);     //get messages from the internal languagefile

void					DOSBOX_RunMachine();
void					DOSBOX_SetLoop(LoopHandler * handler);
void					DOSBOX_SetNormalLoop();
void					DOSBOX_Init(void);

/* machine tests for use with if() statements */
#define IS_TANDY_ARCH			((machine==MCH_TANDY) || (machine==MCH_PCJR))
#define IS_EGAVGA_ARCH			((machine==MCH_EGA) || (machine==MCH_VGA))
#define IS_VGA_ARCH			(machine==MCH_VGA)

/* machine tests for use with switch() statements */
#define TANDY_ARCH_CASE			MCH_TANDY: case MCH_PCJR
#define EGAVGA_ARCH_CASE		MCH_EGA: case MCH_VGA
#define VGA_ARCH_CASE			MCH_VGA

#ifndef DOSBOX_LOGGING_H
#include "logging.h"
#endif // the logging system.

#endif /* DOSBOX_DOSBOX_H */
