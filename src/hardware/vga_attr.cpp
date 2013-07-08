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

/* $Id: vga_attr.cpp,v 1.31 2009-06-28 14:56:13 c2woody Exp $ */

#include "dosbox.h"
#include "inout.h"
#include "vga.h"

#define attr(blah) vga.attr.blah

void VGA_ATTR_SetPalette(Bit8u index,Bit8u val) {
	vga.attr.palette[index] = val;
	if (vga.attr.mode_control & 0x80) val = (val&0xf) | (vga.attr.color_select << 4);
	val &= 63;
	val |= (vga.attr.color_select & 0xc) << 4;
	if (GCC_UNLIKELY(machine==MCH_EGA)) {
		if ((vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8)) == 260) {
			// check for intensity bit
			if (val&0x10) val|=0x38;
			else {
				val&=0x7;
				// check for special brown
				if (val==6) val=0x14;
			}
		}
	}
	VGA_DAC_CombineColor(index,val);
}

Bitu read_p3c0(Bitu /*port*/,Bitu /*iolen*/) {
	// Wcharts, Win 3.11 & 95 SVGA
	Bitu retval = attr(index) & 0x1f;
	if (!(attr(disabled) & 0x1)) retval |= 0x20;
	return retval;
}
 
void write_p3c0(Bitu /*port*/,Bitu val,Bitu iolen) {
	if (!vga.internal.attrindex) {
		attr(index)=val & 0x1F;
		vga.internal.attrindex=true;
		if (val & 0x20) attr(disabled) &= ~1;
		else attr(disabled) |= 1;
		/* 
			0-4	Address of data register to write to port 3C0h or read from port 3C1h
			5	If set screen output is enabled and the palette can not be modified,
				if clear screen output is disabled and the palette can be modified.
		*/
		return;
	} else {
		vga.internal.attrindex=false;
		switch (attr(index)) {
			/* Palette */
		case 0x00:		case 0x01:		case 0x02:		case 0x03:
		case 0x04:		case 0x05:		case 0x06:		case 0x07:
		case 0x08:		case 0x09:		case 0x0a:		case 0x0b:
		case 0x0c:		case 0x0d:		case 0x0e:		case 0x0f:
			if (attr(disabled) & 0x1) VGA_ATTR_SetPalette(attr(index),(Bit8u)val);
			/*
				0-5	Index into the 256 color DAC table. May be modified by 3C0h index
				10h and 14h.
			*/
			break;
		case 0x10: /* Mode Control Register */
			if (!IS_VGA_ARCH) val&=0x1f;	// not really correct, but should do it
			if ((attr(mode_control) ^ val) & 0x80) {
				attr(mode_control)^=0x80;
				for (Bit8u i=0;i<0x10;i++) {
					VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
				}
			}
			if ((attr(mode_control) ^ val) & 0x08) {
				VGA_SetBlinking(val & 0x8);
			}
			if ((attr(mode_control) ^ val) & 0x04) {
				attr(mode_control)=(Bit8u)val;
				VGA_DetermineMode();
				if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) VGA_StartResize();
			} else {
				attr(mode_control)=(Bit8u)val;
				VGA_DetermineMode();
			}

			/*
				0	Graphics mode if set, Alphanumeric mode else.
				1	Monochrome mode if set, color mode else.
				2	9-bit wide characters if set.
					The 9th bit of characters C0h-DFh will be the same as
					the 8th bit. Otherwise it will be the background color.
				3	If set Attribute bit 7 is blinking, else high intensity.
				5	If set the PEL panning register (3C0h index 13h) is temporarily set
					to 0 from when the line compare causes a wrap around until the next
					vertical retrace when the register is automatically reloaded with
					the old value, else the PEL panning register ignores line compares.
				6	If set pixels are 8 bits wide. Used in 256 color modes.
				7	If set bit 4-5 of the index into the DAC table are taken from port
					3C0h index 14h bit 0-1, else the bits in the palette register are
					used.
			*/
			break;
		case 0x11:	/* Overscan Color Register */
			attr(overscan_color)=(Bit8u)val;
			/* 0-5  Color of screen border. Color is defined as in the palette registers. */
			break;
		case 0x12:	/* Color Plane Enable Register */
			/* Why disable colour planes? */
			attr(color_plane_enable)=(Bit8u)val;
			/* 
				0	Bit plane 0 is enabled if set.
				1	Bit plane 1 is enabled if set.
				2	Bit plane 2 is enabled if set.
				3	Bit plane 3 is enabled if set.
				4-5	Video Status MUX. Diagnostics use only.
					Two attribute bits appear on bits 4 and 5 of the Input Status
					Register 1 (3dAh). 0: Bit 2/0, 1: Bit 5/4, 2: bit 3/1, 3: bit 7/6
			*/
			break;
		case 0x13:	/* Horizontal PEL Panning Register */
			attr(horizontal_pel_panning)=val & 0xF;
			switch (vga.mode) {
			case M_TEXT:
				if ((val==0x7) && (svgaCard==SVGA_None)) vga.config.pel_panning=7;
				if (val>0x7) vga.config.pel_panning=0;
				else vga.config.pel_panning=(Bit8u)(val+1);
				break;
			case M_VGA:
			case M_LIN8:
				vga.config.pel_panning=(val & 0x7)/2;
				break;
			case M_LIN16:
			default:
				vga.config.pel_panning=(val & 0x7);
			}
			/*
				0-3	Indicates number of pixels to shift the display left
					Value  9bit textmode   256color mode   Other modes
					0          1               0              0
					1          2              n/a             1
					2          3               1              2
					3          4              n/a             3
					4          5               2              4
					5          6              n/a             5
					6          7               3              6
					7          8              n/a             7
					8          0              n/a            n/a
			*/
			break;
		case 0x14:	/* Color Select Register */
			if (!IS_VGA_ARCH) {
				attr(color_select)=0;
				break;
			}
			if (attr(color_select) ^ val) {
				attr(color_select)=(Bit8u)val;
				for (Bit8u i=0;i<0x10;i++) {
					VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
				}
			}
			/*
				0-1	If 3C0h index 10h bit 7 is set these 2 bits are used as bits 4-5 of
					the index into the DAC table.
				2-3	These 2 bits are used as bit 6-7 of the index into the DAC table
					except in 256 color mode.
					Note: this register does not affect 256 color modes.
			*/
			break;
		default:
			if (svga.write_p3c0) {
				svga.write_p3c0(attr(index), val, iolen);
				break;
			}
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:Write to unkown Index %2X",attr(index));
			break;
		}
	}
}

Bitu read_p3c1(Bitu /*port*/,Bitu iolen) {
//	vga.internal.attrindex=false;
	switch (attr(index)) {
			/* Palette */
	case 0x00:		case 0x01:		case 0x02:		case 0x03:
	case 0x04:		case 0x05:		case 0x06:		case 0x07:
	case 0x08:		case 0x09:		case 0x0a:		case 0x0b:
	case 0x0c:		case 0x0d:		case 0x0e:		case 0x0f:
		return attr(palette[attr(index)]);
	case 0x10: /* Mode Control Register */
		return attr(mode_control);
	case 0x11:	/* Overscan Color Register */
		return attr(overscan_color);
	case 0x12:	/* Color Plane Enable Register */
		return attr(color_plane_enable);
	case 0x13:	/* Horizontal PEL Panning Register */
		return attr(horizontal_pel_panning);
	case 0x14:	/* Color Select Register */
		return attr(color_select);
	default:
		if (svga.read_p3c1)
			return svga.read_p3c1(attr(index), iolen);
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:Read from unkown Index %2X",attr(index));
	}
	return 0;
}


void VGA_SetupAttr(void) {
	if (IS_EGAVGA_ARCH) {
		IO_RegisterWriteHandler(0x3c0,write_p3c0,IO_MB);
		if (IS_VGA_ARCH) {
			IO_RegisterReadHandler(0x3c0,read_p3c0,IO_MB);
			IO_RegisterReadHandler(0x3c1,read_p3c1,IO_MB);
		}
	}
}
