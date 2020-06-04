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


#include "dosbox.h"
#include "inout.h"
#include "vga.h"

#define attr(blah) vga.attr.blah

void VGA_ATTR_SetEGAMonitorPalette(EGAMonitorMode m) {
	// palette bit assignment:
	// bit | pin | EGA        | CGA       | monochrome
	// ----+-----+------------+-----------+------------
	// 0   | 5   | blue       | blue      | nc
	// 1   | 4   | green      | green*    | nc
	// 2   | 3   | red        | red*      | nc
	// 3   | 7   | blue sec.  | nc        | video
	// 4   | 6   | green sec. | intensity | intensity
	// 5   | 2   | red sec.   | nc        | nc
    // 6-7 | not used
	// * additive color brown instead of yellow
	switch(m) {
		case CGA:
			//LOG_MSG("Monitor CGA");
			for (Bitu i=0;i<64;i++) {
				vga.dac.rgb[i].red=((i & 0x4)? 0x2a:0) + ((i & 0x10)? 0x15:0);
				vga.dac.rgb[i].blue=((i & 0x1)? 0x2a:0) + ((i & 0x10)? 0x15:0);
				
				// replace yellow with brown
				if ((i & 0x17) == 0x6) vga.dac.rgb[i].green = 0x15;
				else vga.dac.rgb[i].green =
					((i & 0x2)? 0x2a:0) + ((i & 0x10)? 0x15:0);
			}
			break;
		case EGA:
			//LOG_MSG("Monitor EGA");
			for (Bitu i=0;i<64;i++) {
				vga.dac.rgb[i].red=((i & 0x4)? 0x2a:0) + ((i & 0x20)? 0x15:0);
				vga.dac.rgb[i].green=((i & 0x2)? 0x2a:0) + ((i & 0x10)? 0x15:0);
				vga.dac.rgb[i].blue=((i & 0x1)? 0x2a:0) + ((i & 0x8)? 0x15:0);
			}
			break;
		case MONO:
			//LOG_MSG("Monitor MONO");
			for (Bitu i=0;i<64;i++) {
				Bit8u value = ((i & 0x8)? 0x2a:0) + ((i & 0x10)? 0x15:0);
				vga.dac.rgb[i].red = vga.dac.rgb[i].green =
					vga.dac.rgb[i].blue = value;
			}
			break;
	}

	// update the mappings
	for (Bit8u i=0;i<0x10;i++)
		VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
}

void VGA_ATTR_SetPalette(Bit8u index, Bit8u val) {
	// the attribute table stores only 6 bits
	val &= 63; 
	vga.attr.palette[index] = val;

    if (IS_VGA_ARCH) {
        // apply the plane mask
        val = vga.attr.palette[index & vga.attr.color_plane_enable];

        // Tseng ET4000AX behavior (according to how COPPER.EXE treats the hardware)
        // and according to real hardware:
        //
        // - If P54S (palette select bits 5-4) are enabled, replace bits 7-4 of the
        //   color index with the entire color select register. COPPER.EXE line fading
        //   tricks will not work correctly otherwise.
        //
        // - If P54S is not enabled, then do not apply any Color Select register bits.
        //   This is contrary to standard VGA behavior that would always apply Color
        //   Select bits 3-2 to index bits 7-6 in any mode other than 256-color mode.
        if (VGA_AC_remap == AC_low4) {
            if (vga.attr.mode_control & 0x80)
                val = (val&0xf) | (vga.attr.color_select << 4);
        }
        // normal VGA/SVGA behavior:
        //
        // - ignore color select in 256-color mode entirely
        //
        // - otherwise, if P54S is enabled, replace bits 5-4 with bits 1-0 of color select.
        //
        // - always replace bits 7-6 with bits 3-2 of color select.
        else {
            if (!(vga.mode == M_VGA || vga.mode == M_LIN8)) {
                // replace bits 5-4 if P54S is enabled
                if (vga.attr.mode_control & 0x80)
                    val = (val&0xf) | ((vga.attr.color_select & 0x3) << 4);

                // always replace bits 7-6
                val |= (vga.attr.color_select & 0xc) << 4;
            }
        }

        // apply
        VGA_DAC_CombineColor(index,val);
    }
    else {
        VGA_DAC_CombineColor(index,index);
    }
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
        /* NOTES: Paradise/Western Digital SVGA appears to respond to 0x00-0x0F as
         *        expected, but 0x10-0x17 have an alias at 0x18-0x1F according to
         *        DOSLIB TMODESET.EXE dumps.
         *
         *        Original IBM PS/2 VGA hardware acts the same.
         *
         *        if (val & 0x10)
         *          index = val & 0x17;
         *        else
         *          index = val & 0x0F;
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
			if (attr(disabled) & 0x1) {
                VGA_ATTR_SetPalette(attr(index),(Bit8u)val);

                /* if the color plane enable register is anything other than 0x0F, then
                 * the whole attribute palette must be re-sent to the DAC because the
                 * masking causes one entry to affect others due to the way the VGA
                 * lookup table works (array xlat32[]). This is not necessary for EGA
                 * emulation because it uses the color plane enable mask directly. */
                if (IS_VGA_ARCH && (attr(color_plane_enable) & 0x0F) != 0x0F) {
                    /* update entries before the desired index */
                    for (Bit8u i=0;i < attr(index);i++)
                        VGA_ATTR_SetPalette(i,vga.attr.palette[i]);

                    /* update entries after the desired index */
                    for (Bit8u i=attr(index)+1;i < 0x10;i++)
                        VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
                }
            }
			/*
				0-5	Index into the 256 color DAC table. May be modified by 3C0h index
				10h and 14h.
			*/
			break;
		case 0x10: { /* Mode Control Register */
			if (!IS_VGA_ARCH) val&=0x1f;	// not really correct, but should do it
			Bitu difference = attr(mode_control)^val;
			attr(mode_control)=(Bit8u)val;

			if (difference & 0x80) {
				for (Bit8u i=0;i<0x10;i++)
					VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
			}
			if (difference & 0x08)
				VGA_SetBlinking(val & 0x8);
			
			if (difference & 0x41)
				VGA_DetermineMode();
            if ((difference & 0x40) && (vga.mode == M_VGA)) // 8BIT changes in 256-color mode must be handled
                VGA_StartResize(50);

			if (difference & 0x04) {
				// recompute the panning value
				if(vga.mode==M_TEXT) {
					Bit8u pan_reg = attr(horizontal_pel_panning);
					if (pan_reg > 7)
						vga.config.pel_panning=0;
					else if (val&0x4) // 9-dot wide characters
						vga.config.pel_panning=(Bit8u)(pan_reg+1);
					else // 8-dot characters
						vga.config.pel_panning=(Bit8u)pan_reg;
				}
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
		}
		case 0x11:	/* Overscan Color Register */
			attr(overscan_color)=(Bit8u)val;
			/* 0-5  Color of screen border. Color is defined as in the palette registers. */
			break;
		case 0x12:	/* Color Plane Enable Register */
			/* Why disable colour planes? */
			/* To support weird modes. */
			if ((attr(color_plane_enable)^val) & 0xf) {
				// in case the plane enable bits change...
				attr(color_plane_enable)=(Bit8u)val;
				for (Bit8u i=0;i<0x10;i++)
					VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
			} else
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
				if (val > 7)
					vga.config.pel_panning=0;
				else if (vga.attr.mode_control&0x4) // 9-dot wide characters
					vga.config.pel_panning=(Bit8u)(val+1);
				else // 8-dot characters
					vga.config.pel_panning=(Bit8u)val;
				break;
			case M_VGA:
			case M_LIN8:
				vga.config.pel_panning=(val & 0x7)/2;
				break;
			case M_LIN16:
			default:
				vga.config.pel_panning=(val & 0x7);
			}
			if (machine==MCH_EGA)
				// On the EGA panning can be programmed for every scanline:
				vga.draw.panning = vga.config.pel_panning;
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
				for (Bit8u i=0;i<0x10;i++)
					VGA_ATTR_SetPalette(i,vga.attr.palette[i]);
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

void VGA_UnsetupAttr(void) {
    IO_FreeWriteHandler(0x3c0,IO_MB);
    IO_FreeReadHandler(0x3c0,IO_MB);
    IO_FreeWriteHandler(0x3c1,IO_MB);
    IO_FreeReadHandler(0x3c1,IO_MB);
}

// save state support
void POD_Save_VGA_Attr( std::ostream& stream )
{
	// - pure struct data
	WRITE_POD( &vga.attr, vga.attr );


	// no static globals found
}


void POD_Load_VGA_Attr( std::istream& stream )
{
	// - pure struct data
	READ_POD( &vga.attr, vga.attr );


	// no static globals found
}
