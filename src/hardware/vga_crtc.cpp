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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <stdlib.h>
#include "dosbox.h"
#include "inout.h"
#include "vga.h"
#include "debug.h"
#include "cpu.h"
#include "video.h"
#include "pic.h"

#define crtc(blah) vga.crtc.blah


void VGA_MapMMIO(void);
void VGA_UnmapMMIO(void);
void page_flip_debug_notify();

void VGA_CheckAddrShift() {
    //Byte,word,dword mode
    if ( IS_VGA_ARCH && crtc(underline_location) & 0x40 )
        vga.config.addr_shift = 2u;
    else if ( IS_EGAVGA_ARCH && crtc( mode_control) & 0x40 )
        vga.config.addr_shift = 0u;
    else
        vga.config.addr_shift = 1u;
}

void vga_write_p3d5(Bitu port,Bitu val,Bitu iolen);
Bitu DEBUG_EnableDebugger(void);

extern bool vga_ignore_hdispend_change_if_smaller;

void vga_write_p3d4(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
	crtc(index)=(Bit8u)val;
}

void vga_pc98_direct_cursor_pos(Bit16u address) {
	vga.config.cursor_start = address;
}

Bitu vga_read_p3d4(Bitu port,Bitu iolen) {
    (void)port;//UNUSED
    (void)iolen;//UNUSED

    /* NOTES: Paradise/Westdern Digital SVGA decodes only bits [5:0] inclusive and repeat every 0x40 */

	return crtc(index);
}

void vga_write_p3d5(Bitu port,Bitu val,Bitu iolen) {
    (void)port;//UNUSED
//	if((crtc(index)!=0xe)&&(crtc(index)!=0xf)) 
//		LOG_MSG("CRTC w #%2x val %2x",crtc(index),val);
	switch(crtc(index)) {
	case 0x00:	/* Horizontal Total Register */
		if (crtc(read_only)) break;
		crtc(horizontal_total)=(Bit8u)val;
		/* 	0-7  Horizontal Total Character Clocks-5 */
		break;
	case 0x01:	/* Horizontal Display End Register */
		if (crtc(read_only)) break;
		if (val != crtc(horizontal_display_end)) {
			/* we permit a configuration option that if set, means do NOT call VGA_StartResize()
			 * if the new value is less than the current value, to protect against rapid changes
			 * from demos like DoWhackaDo. */
			if (vga_ignore_hdispend_change_if_smaller && val < crtc(horizontal_display_end)) {
				/* do not call VGA_StartResize, allow change */
				crtc(horizontal_display_end)=(Bit8u)val;
				LOG_MSG("VGA Horz. Display End: accepting change but will not call VGA_StartResize()");
			}
			else {
				crtc(horizontal_display_end)=(Bit8u)val;
				VGA_StartResize();
			}
		}
		/* 	0-7  Number of Character Clocks Displayed -1 */
		break;
	case 0x02:	/* Start Horizontal Blanking Register */
		if (crtc(read_only)) break;
		crtc(start_horizontal_blanking)=(Bit8u)val;
		/*	0-7  The count at which Horizontal Blanking starts */
		break;
	case 0x03:	/* End Horizontal Blanking Register */
		if (crtc(read_only)) break;
		crtc(end_horizontal_blanking)=(Bit8u)val;
		/*
			0-4	Horizontal Blanking ends when the last 6 bits of the character
				counter equals this field. Bit 5 is at 3d4h index 5 bit 7.
			5-6	Number of character clocks to delay start of display after Horizontal
				Total has been reached.
			7	Access to Vertical Retrace registers if set. If clear reads to 3d4h
				index 10h and 11h access the Lightpen read back registers ??
		*/
		break;
	case 0x04:	/* Start Horizontal Retrace Register */
		if (crtc(read_only)) break;
		crtc(start_horizontal_retrace)=(Bit8u)val;
		/*	0-7  Horizontal Retrace starts when the Character Counter reaches this value. */
		break;
	case 0x05:	/* End Horizontal Retrace Register */
		if (crtc(read_only)) break;
		crtc(end_horizontal_retrace)=(Bit8u)val;
		/*
			0-4	Horizontal Retrace ends when the last 5 bits of the character counter
				equals this value.
			5-6	Number of character clocks to delay start of display after Horizontal
				Retrace.
			7	bit 5 of the End Horizontal Blanking count (See 3d4h index 3 bit 0-4)
		*/	
		break;
	case 0x06: /* Vertical Total Register */
		if (crtc(read_only)) break;
		if (val != crtc(vertical_total)) {
			crtc(vertical_total)=(Bit8u)val;	
			VGA_StartResize();
		}
		/*	0-7	Lower 8 bits of the Vertical Total. Bit 8 is found in 3d4h index 7
				bit 0. Bit 9 is found in 3d4h index 7 bit 5.
			Note: For the VGA this value is the number of scan lines in the display -2.
		*/
		break;
	case 0x07:	/* Overflow Register */
		//Line compare bit ignores read only */
		vga.config.line_compare=(vga.config.line_compare & 0x6ff) | (val & 0x10) << 4;
		if (crtc(read_only)) break;
		if ((vga.crtc.overflow ^ val) & 0xd6) {
			crtc(overflow)=(Bit8u)val;
			VGA_StartResize();
		} else crtc(overflow)=(Bit8u)val;
		/*
			0  Bit 8 of Vertical Total (3d4h index 6)
			1  Bit 8 of Vertical Display End (3d4h index 12h)
			2  Bit 8 of Vertical Retrace Start (3d4h index 10h)
			3  Bit 8 of Start Vertical Blanking (3d4h index 15h)
			4  Bit 8 of Line Compare Register (3d4h index 18h)
			5  Bit 9 of Vertical Total (3d4h index 6)
			6  Bit 9 of Vertical Display End (3d4h index 12h)
			7  Bit 9 of Vertical Retrace Start (3d4h index 10h)
		*/
		break;
	case 0x08:	/* Preset Row Scan Register */
		crtc(preset_row_scan)=(Bit8u)val;
		vga.config.hlines_skip=val&31;
		if (IS_VGA_ARCH) vga.config.bytes_skip=(val>>5)&3;
		else vga.config.bytes_skip=0;
//		LOG_DEBUG("Skip lines %d bytes %d",vga.config.hlines_skip,vga.config.bytes_skip);
		/*
			0-4	Number of lines we have scrolled down in the first character row.
				Provides Smooth Vertical Scrolling.b
			5-6	Number of bytes to skip at the start of scanline. Provides Smooth
				Horizontal Scrolling together with the Horizontal Panning Register
				(3C0h index 13h).
		*/
		break;
	case 0x09: /* Maximum Scan Line Register */
	{
		if (IS_VGA_ARCH) {
			vga.config.line_compare &= 0x5ff;
			vga.config.line_compare |= (val&0x40)<<3;
		} else if(machine==MCH_EGA) {
			val &= 0x7f; // EGA ignores the doublescan bit
			}
		Bit8u old = crtc(maximum_scan_line);
		crtc(maximum_scan_line) = (Bit8u)val;

        unsigned char chk = 0x20;

        if (!vga.draw.doublescan_set)
            chk |= 0x81; /* doublescan + LSB of maximum scanline */

		if ((old ^ val) & chk) VGA_StartResize();
		vga.draw.address_line_total = (val & 0x1F) + 1;
		if (val&0x80) vga.draw.address_line_total *= 2;
		/*
			0-4	Number of scan lines in a character row -1. In graphics modes this is
				the number of times (-1) the line is displayed before passing on to
				the next line (0: normal, 1: double, 2: triple...).
				This is independent of bit 7, except in CGA modes which seems to
				require this field to be 1 and bit 7 to be set to work.
			5	Bit 9 of Start Vertical Blanking
			6	Bit 9 of Line Compare Register
			7	Doubles each scan line if set. I.e. displays 200 lines on a 400 display.
		*/
		break;
	}
	case 0x0A:	/* Cursor Start Register */
		crtc(cursor_start)=(Bit8u)val;
		vga.draw.cursor.sline=val&0x1f;
		if (IS_VGA_ARCH) vga.draw.cursor.enabled=!(val&0x20);
		else vga.draw.cursor.enabled=true;
		/*
			0-4	First scanline of cursor within character.
			5	Turns Cursor off if set
		*/
		break;
	case 0x0B:	/* Cursor End Register */
		crtc(cursor_end)=(Bit8u)val;
		vga.draw.cursor.eline=val&0x1f;
		vga.draw.cursor.delay=(val>>5)&0x3;

		/* 
			0-4	Last scanline of cursor within character
			5-6	Delay of cursor data in character clocks.
		*/
		break;
	case 0x0C:	/* Start Address High Register */
		crtc(start_address_high)=(Bit8u)val;
		vga.config.display_start=(vga.config.display_start & 0xFF00FF)| (val << 8);
		/* 0-7  Upper 8 bits of the start address of the display buffer */
		page_flip_debug_notify();
		break;
	case 0x0D:	/* Start Address Low Register */
		crtc(start_address_low)=(Bit8u)val;
		vga.config.display_start=(vga.config.display_start & 0xFFFF00)| val;
		/*	0-7	Lower 8 bits of the start address of the display buffer */
		page_flip_debug_notify();
		break;
	case 0x0E:	/*Cursor Location High Register */
		crtc(cursor_location_high)=(Bit8u)val;
		vga.config.cursor_start&=0xff00ff;
		vga.config.cursor_start|=val << 8;
		/*	0-7  Upper 8 bits of the address of the cursor */
		break;
	case 0x0F:	/* Cursor Location Low Register */
//TODO update cursor on screen
		crtc(cursor_location_low)=(Bit8u)val;
		vga.config.cursor_start&=0xffff00;
		vga.config.cursor_start|=val;
		/*	0-7  Lower 8 bits of the address of the cursor */
		break;
	case 0x10:	/* Vertical Retrace Start Register */
		crtc(vertical_retrace_start)=(Bit8u)val;
		/*	
			0-7	Lower 8 bits of Vertical Retrace Start. Vertical Retrace starts when
			the line counter reaches this value. Bit 8 is found in 3d4h index 7
			bit 2. Bit 9 is found in 3d4h index 7 bit 7.
		*/
		break;
	case 0x11:	/* Vertical Retrace End Register */
		crtc(vertical_retrace_end)=(Bit8u)val;
		
		if (IS_EGAVGA_ARCH && !(val & 0x10)) {
			vga.draw.vret_triggered=false;
			if (GCC_UNLIKELY(machine==MCH_EGA)) PIC_DeActivateIRQ(9);
		}
		if (IS_VGA_ARCH) crtc(read_only)=(val & 128)>0;
		else crtc(read_only)=false;
		/*
			0-3	Vertical Retrace ends when the last 4 bits of the line counter equals
				this value.
			4	if clear Clears pending Vertical Interrupts.
			5	Vertical Interrupts (IRQ 2) disabled if set. Can usually be left
				disabled, but some systems (including PS/2) require it to be enabled.
			6	If set selects 5 refresh cycles per scanline rather than 3.
			7	Disables writing to registers 0-7 if set 3d4h index 7 bit 4 is not
				affected by this bit.
		*/
		break;
	case 0x12:	/* Vertical Display End Register */
		if (val!=crtc(vertical_display_end)) {
			if (abs(static_cast<int>((Bits)val-(Bits)crtc(vertical_display_end)))<3) {
				// delay small vde changes a bit to avoid screen resizing
				// if they are reverted in a short timeframe
				PIC_RemoveEvents(VGA_SetupDrawing);
				vga.draw.resizing=false;
				crtc(vertical_display_end)=(Bit8u)val;
				VGA_StartResize(150);
			} else {
				crtc(vertical_display_end)=(Bit8u)val;
				VGA_StartResize();
			}
		}
		/*
			0-7	Lower 8 bits of Vertical Display End. The display ends when the line
				counter reaches this value. Bit 8 is found in 3d4h index 7 bit 1.
			Bit 9 is found in 3d4h index 7 bit 6.
		*/
		break;
	case 0x13:	/* Offset register */
		crtc(offset)=(Bit8u)val;
		vga.config.scan_len&=0x300;
		vga.config.scan_len|=val;
		VGA_CheckScanLength();
		/*
			0-7	Number of bytes in a scanline / K. Where K is 2 for byte mode, 4 for
				word mode and 8 for Double Word mode.
		*/
		break;
	case 0x14:	/* Underline Location Register */
		crtc(underline_location)=(Bit8u)val;
        VGA_CheckAddrShift();
		VGA_CheckScanLength();
		/*
			0-4	Position of underline within Character cell.
			5	If set memory address is only changed every fourth character clock.
			6	Double Word mode addressing if set
		*/
		break;
	case 0x15:	/* Start Vertical Blank Register */
		if (val!=crtc(start_vertical_blanking)) {
			crtc(start_vertical_blanking)=(Bit8u)val;
			VGA_StartResize();
		}
		/* 
			0-7	Lower 8 bits of Vertical Blank Start. Vertical blanking starts when
				the line counter reaches this value. Bit 8 is found in 3d4h index 7
				bit 3.
		*/
		break;
	case 0x16:	/*  End Vertical Blank Register */
		if (val!=crtc(end_vertical_blanking)) {
			crtc(end_vertical_blanking)=(Bit8u)val;
			VGA_StartResize();
		}
		/*
			0-6	Vertical blanking stops when the lower 7 bits of the line counter
				equals this field. Some SVGA chips uses all 8 bits!
				IBM actually says bits 0-7.
		*/
		break;
	case 0x17:	/* Mode Control Register */
		crtc(mode_control)=(Bit8u)val;
		vga.tandy.line_mask = (~val) & 3u;

		if ( vga.tandy.line_mask ) {
			vga.tandy.line_shift = 13u;
			vga.tandy.addr_mask = (1u << 13u) - 1u;
		} else {
			vga.tandy.addr_mask = ~0u;
			vga.tandy.line_shift = 0;
		}

        VGA_CheckAddrShift();
		VGA_CheckScanLength();

		//Should we really need to do a determinemode here?
//		VGA_DetermineMode();
		/*
			0	If clear use CGA compatible memory addressing system
				by substituting character row scan counter bit 0 for address bit 13,
				thus creating 2 banks for even and odd scan lines.
			1	If clear use Hercules compatible memory addressing system by
				substituting character row scan counter bit 1 for address bit 14,
				thus creating 4 banks.
			2	If set increase scan line counter only every second line.
			3	If set increase memory address counter only every other character clock.
			5	When in Word Mode bit 15 is rotated to bit 0 if this bit is set else
				bit 13 is rotated into bit 0.
			6	If clear system is in word mode. Addresses are rotated 1 position up
				bringing either bit 13 or 15 into bit 0.
			7	Clearing this bit will reset the display system until the bit is set again.
		*/
		break;
	case 0x18:	/* Line Compare Register */
		crtc(line_compare)=(Bit8u)val;
		vga.config.line_compare=(vga.config.line_compare & 0x700) | val;
		/*
			0-7	Lower 8 bits of the Line Compare. When the Line counter reaches this
				value, the display address wraps to 0. Provides Split Screen
				facilities. Bit 8 is found in 3d4h index 7 bit 4.
				Bit 9 is found in 3d4h index 9 bit 6.
		*/
		break;
	default:
		if (svga.write_p3d5) {
			svga.write_p3d5(crtc(index), val, iolen);
		} else {
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:Write to unknown index %X",crtc(index));
		}
		break;
	}
}


Bitu vga_read_p3d5x(Bitu port,Bitu iolen);
Bitu vga_read_p3d5(Bitu port,Bitu iolen) {
	Bitu retval = vga_read_p3d5x(port,iolen);
//	LOG_MSG("CRTC r #%2x val %2x",crtc(index),retval);
	return retval;
}

Bitu vga_read_p3d5x(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
	switch(crtc(index)) {
	case 0x00:	/* Horizontal Total Register */
		return crtc(horizontal_total);
	case 0x01:	/* Horizontal Display End Register */
		return crtc(horizontal_display_end);
	case 0x02:	/* Start Horizontal Blanking Register */
		return crtc(start_horizontal_blanking);
	case 0x03:	/* End Horizontal Blanking Register */
		return crtc(end_horizontal_blanking);
	case 0x04:	/* Start Horizontal Retrace Register */
		return crtc(start_horizontal_retrace);
	case 0x05:	/* End Horizontal Retrace Register */
		return crtc(end_horizontal_retrace);
	case 0x06: /* Vertical Total Register */
		return crtc(vertical_total);	
	case 0x07:	/* Overflow Register */
		return crtc(overflow);
	case 0x08:	/* Preset Row Scan Register */
		return crtc(preset_row_scan);
	case 0x09: /* Maximum Scan Line Register */
		return crtc(maximum_scan_line);
	case 0x0A:	/* Cursor Start Register */
		return crtc(cursor_start);
	case 0x0B:	/* Cursor End Register */
		return crtc(cursor_end);
	case 0x0C:	/* Start Address High Register */
		return crtc(start_address_high);
	case 0x0D:	/* Start Address Low Register */
		return crtc(start_address_low);
	case 0x0E:	/*Cursor Location High Register */
		return crtc(cursor_location_high);
	case 0x0F:	/* Cursor Location Low Register */
		return crtc(cursor_location_low);
	case 0x10:	/* Vertical Retrace Start Register */
		return crtc(vertical_retrace_start);
	case 0x11:	/* Vertical Retrace End Register */
		return crtc(vertical_retrace_end);
	case 0x12:	/* Vertical Display End Register */
		return crtc(vertical_display_end);
	case 0x13:	/* Offset register */
		return crtc(offset);
	case 0x14:	/* Underline Location Register */
		return crtc(underline_location);
	case 0x15:	/* Start Vertical Blank Register */
		return crtc(start_vertical_blanking);
	case 0x16:	/*  End Vertical Blank Register */
		return crtc(end_vertical_blanking);
	case 0x17:	/* Mode Control Register */
		return crtc(mode_control);
	case 0x18:	/* Line Compare Register */
		return crtc(line_compare);
	default:
		if (svga.read_p3d5) {
			return svga.read_p3d5(crtc(index), iolen);
		} else {
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:Read from unknown index %X",crtc(index));
			return 0x0;
		}
	}
}

// save state support 
void POD_Save_VGA_Crtc( std::ostream& stream )
{
	// - pure struct data
	WRITE_POD( &vga.crtc, vga.crtc );
 
 
	// no static globals found
}


void POD_Load_VGA_Crtc( std::istream& stream )
{
	// - pure struct data
	READ_POD( &vga.crtc, vga.crtc );


	// no static globals found
}
