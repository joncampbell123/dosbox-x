/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <assert.h>

#include "dosbox.h"
#include "logging.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"
#include "inout.h"
#include "programs.h"
#include "support.h"
#include "setup.h"
#include "timer.h"
#include "mem.h"
#include "util_units.h"
#include "control.h"
#include "pc98_cg.h"
#include "pc98_dac.h"
#include "pc98_gdc.h"
#include "pc98_gdc_const.h"
#include "mixer.h"
#include "menu.h"
#include "mem.h"
#include "render.h"
#include "jfont.h"
#include "bitop.h"
#include "sdlmain.h"

#include <string.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>

struct ATIState {
	uint8_t			index = 0;
	uint8_t			input_status_register = 0; /* index 0xBB */
};

ATIState ati_state;

Bitu ATIExtIndex_Read(Bitu /*port*/, Bitu /*len*/) {
	return ati_state.index;
}

void ATIExtIndex_Write(Bitu /*port*/, Bitu val, Bitu /*len*/) {
	ati_state.index = (uint8_t)val;
}

Bitu ATIExtData_Read(Bitu port, Bitu /*len*/) {
	switch (ati_state.index) {
		case 0xBB: /* Input status register */
			return ati_state.input_status_register;
		default:
			break;
	};

	LOG(LOG_MISC,LOG_DEBUG)("Unhandled ATI extended read port=%x index=%x",(unsigned int)port,ati_state.index);
	return 0;
}

void ATIExtData_Write(Bitu port, Bitu val, Bitu /*len*/) {
	switch (ati_state.index) {
		case 0xBB: /* Input status register */
			ati_state.input_status_register = (uint8_t)val;
			break;
		default:
			break;
	};

	LOG(LOG_MISC,LOG_DEBUG)("Unhandled ATI extended write port=%x index=%x val=%x",(unsigned int)port,ati_state.index,(unsigned int)val);
}

void SVGA_Setup_ATI(void) {
	if (vga.mem.memsize == 0)
		vga.mem.memsize = 512*1024;

	if (vga.mem.memsize >= (512*1024))
		vga.mem.memsize = (512*1024);
	else
		vga.mem.memsize = (256*1024);

	ati_state.input_status_register = 0x05/*multisync monitor*/;

	/* FIXME: ATI 188xx-specific */
	if (vga.mem.memsize >= (512*1024)) ati_state.input_status_register |= 0x20; /* 512KB, not 256KB, of RAM */

	IO_RegisterWriteHandler(0x1ce,&ATIExtIndex_Write,IO_MB);
	IO_RegisterReadHandler(0x1ce,&ATIExtIndex_Read,IO_MB);
	IO_RegisterWriteHandler(0x1cf,&ATIExtData_Write,IO_MB);
	IO_RegisterReadHandler(0x1cf,&ATIExtData_Read,IO_MB);
}

