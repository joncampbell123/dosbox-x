/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: vga_misc.cpp,v 1.39 2009-01-25 12:00:49 c2woody Exp $ */

#include "dosbox.h"
#include "inout.h"
#include "pic.h"
#include "vga.h"
#include <math.h>


void vga_write_p3d4(Bitu port,Bitu val,Bitu iolen);
Bitu vga_read_p3d4(Bitu port,Bitu iolen);
void vga_write_p3d5(Bitu port,Bitu val,Bitu iolen);
Bitu vga_read_p3d5(Bitu port,Bitu iolen);

Bitu vga_read_p3da(Bitu port,Bitu iolen) {
	Bit8u retval=0;
	double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;

	vga.internal.attrindex=false;
	vga.tandy.pcjr_flipflop=false;

	// 3DAh (R):  Status Register
	// bit   0  Horizontal or Vertical blanking
	//       3  Vertical sync

	if (timeInFrame >= vga.draw.delay.vrstart &&
		timeInFrame <= vga.draw.delay.vrend)
		retval |= 8;
	if (timeInFrame >= vga.draw.delay.vdend) {
		retval |= 1;
	} else {
		double timeInLine=fmod(timeInFrame,vga.draw.delay.htotal);
		if (timeInLine >= vga.draw.delay.hblkstart && 
			timeInLine <= vga.draw.delay.hblkend) {
			retval |= 1;
		}
	}
	return retval;
}

static void write_p3c2(Bitu port,Bitu val,Bitu iolen) {
	vga.misc_output=val;
	if (val & 0x1) {
		IO_RegisterWriteHandler(0x3d4,vga_write_p3d4,IO_MB);
		IO_RegisterReadHandler(0x3d4,vga_read_p3d4,IO_MB);
		IO_RegisterReadHandler(0x3da,vga_read_p3da,IO_MB);

		IO_RegisterWriteHandler(0x3d5,vga_write_p3d5,IO_MB);
		IO_RegisterReadHandler(0x3d5,vga_read_p3d5,IO_MB);

		IO_FreeWriteHandler(0x3b4,IO_MB);
		IO_FreeReadHandler(0x3b4,IO_MB);
		IO_FreeWriteHandler(0x3b5,IO_MB);
		IO_FreeReadHandler(0x3b5,IO_MB);
		IO_FreeReadHandler(0x3ba,IO_MB);
	} else {
		IO_RegisterWriteHandler(0x3b4,vga_write_p3d4,IO_MB);
		IO_RegisterReadHandler(0x3b4,vga_read_p3d4,IO_MB);
		IO_RegisterReadHandler(0x3ba,vga_read_p3da,IO_MB);

		IO_RegisterWriteHandler(0x3b5,vga_write_p3d5,IO_MB);
		IO_RegisterReadHandler(0x3b5,vga_read_p3d5,IO_MB);


		IO_FreeWriteHandler(0x3d4,IO_MB);
		IO_FreeReadHandler(0x3d4,IO_MB);
		IO_FreeWriteHandler(0x3d5,IO_MB);
		IO_FreeReadHandler(0x3d5,IO_MB);
		IO_FreeReadHandler(0x3da,IO_MB);
	}
	/*
		0	If set Color Emulation. Base Address=3Dxh else Mono Emulation. Base Address=3Bxh.
		2-3	Clock Select. 0: 25MHz, 1: 28MHz
		5	When in Odd/Even modes Select High 64k bank if set
		6	Horizontal Sync Polarity. Negative if set
		7	Vertical Sync Polarity. Negative if set
			Bit 6-7 indicates the number of lines on the display:
			1:  400, 2: 350, 3: 480
			Note: Set to all zero on a hardware reset.
			Note: This register can be read from port 3CCh.
	*/
}


static Bitu read_p3cc(Bitu port,Bitu iolen) {
	return vga.misc_output;
}

// VGA feature control register
static Bitu read_p3ca(Bitu port,Bitu iolen) {
	return 0;
}

static Bitu read_p3c8(Bitu port,Bitu iolen) {
	return 0x10;
}

static Bitu read_p3c2(Bitu port,Bitu iolen) {
	Bit8u retval=0;

	if (machine==MCH_EGA) retval = 0x0F;
	else if (IS_VGA_ARCH) retval = 0x60;
	if ((machine==MCH_VGA) || (((vga.misc_output>>2)&3)==0) || (((vga.misc_output>>2)&3)==3)) {
		retval |= 0x10;
	}

	if (vga.draw.vret_triggered) retval |= 0x80;
	return retval;
	/*
		0-3 0xF on EGA, 0x0 on VGA 
		4	Status of the switch selected by the Miscellaneous Output
			Register 3C2h bit 2-3. Switch high if set.
			(apparently always 1 on VGA)
		5	(EGA) Pin 19 of the Feature Connector (FEAT0) is high if set
		6	(EGA) Pin 17 of the Feature Connector (FEAT1) is high if set
			(default differs by card, ET4000 sets them both)
		7	If set IRQ 2 has happened due to Vertical Retrace.
			Should be cleared by IRQ 2 interrupt routine by clearing port 3d4h
			index 11h bit 4.
	*/
}

void VGA_SetupMisc(void) {
	if (IS_EGAVGA_ARCH) {
		vga.draw.vret_triggered=false;
		IO_RegisterReadHandler(0x3c2,read_p3c2,IO_MB);
		IO_RegisterWriteHandler(0x3c2,write_p3c2,IO_MB);
		if (IS_VGA_ARCH) {
			IO_RegisterReadHandler(0x3ca,read_p3ca,IO_MB);
			IO_RegisterReadHandler(0x3cc,read_p3cc,IO_MB);
		} else {
			IO_RegisterReadHandler(0x3c8,read_p3c8,IO_MB);
		}
	} else if (machine==MCH_CGA || machine==MCH_AMSTRAD || IS_TANDY_ARCH) {
		IO_RegisterReadHandler(0x3da,vga_read_p3da,IO_MB);
	}
}
