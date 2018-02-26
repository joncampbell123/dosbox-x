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

#include "dosbox.h"
#include "inout.h"
#include "render.h"
#include "vga.h"

extern bool vga_enable_3C6_ramdac;

/*
3C6h (R/W):  PEL Mask
bit 0-7  This register is anded with the palette index sent for each dot.
         Should be set to FFh.

3C7h (R):  DAC State Register
bit 0-1  0 indicates the DAC is in Write Mode and 3 indicates Read mode.

3C7h (W):  PEL Address Read Mode
bit 0-7  The PEL data register (0..255) to be read from 3C9h.
Note: After reading the 3 bytes at 3C9h this register will increment,
      pointing to the next data register.

3C8h (R/W):  PEL Address Write Mode
bit 0-7  The PEL data register (0..255) to be written to 3C9h.
Note: After writing the 3 bytes at 3C9h this register will increment, pointing
      to the next data register.

3C9h (R/W):  PEL Data Register
bit 0-5  Color value
Note:  Each read or write of this register will cycle through first the
       registers for Red, Blue and Green, then increment the appropriate
       address register, thus the entire palette can be loaded by writing 0 to
       the PEL Address Write Mode register 3C8h and then writing all 768 bytes
       of the palette to this register.
*/

enum {DAC_READ,DAC_WRITE};

static void VGA_DAC_SendColor( Bitu index, Bitu src ) {
	/* NTS: Don't forget red/green/blue are 6-bit RGB not 8-bit RGB */
	const Bit8u red = vga.dac.rgb[src].red;
	const Bit8u green = vga.dac.rgb[src].green;
	const Bit8u blue = vga.dac.rgb[src].blue;

	if (GFX_bpp >= 24) /* FIXME: Assumes 8:8:8. What happens when desktops start using the 10:10:10 format? */
		vga.dac.xlat32[index] = (blue<<(2+GFX_Bshift)) | (green<<(2+GFX_Gshift)) | (red<<(2+GFX_Rshift)) | GFX_Amask;
	else {
        /* FIXME: Assumes 5:6:5. I need to test against 5:5:5 format sometime. Perhaps I could dig out some older VGA cards and XFree86 drivers that support that format? */
		vga.dac.xlat16[index] = ((((blue&0x3f)>>1)<<GFX_Bshift)) | ((green&0x3f)<<GFX_Gshift) | (((red&0x3f)>>1)<<GFX_Rshift) | GFX_Amask;

        /* PC-98 mode always renders 32bpp, therefore needs this fix */
        if (GFX_Bshift == 0)
            vga.dac.xlat32[index] = (blue << 2U) | (green << 10U) | (red << 18U);
        else
            vga.dac.xlat32[index] = (blue << 18U) | (green << 10U) | (red << 2U);
    }

	RENDER_SetPal( index, (red << 2) | ( red >> 4 ), (green << 2) | ( green >> 4 ), (blue << 2) | ( blue >> 4 ) );
}

void VGA_DAC_UpdateColor( Bitu index ) {
	Bitu maskIndex;

    if (IS_EGA_ARCH) {
        VGA_DAC_SendColor( index, index );
    }
    else {
        switch (vga.mode) {
            case M_VGA:
            case M_LIN8:
                maskIndex = index & vga.dac.pel_mask;
                VGA_DAC_SendColor( index, maskIndex );
                break;
            default:
                /* Remember the lookup table is there to handle the color palette AND the DAC mask AND the attribute controller palette */
                /* FIXME: Is it: index -> attribute controller -> dac mask, or
                 *               index -> dac mask -> attribute controller? */
                maskIndex = vga.dac.combine[index&0xF] & vga.dac.pel_mask;
                VGA_DAC_SendColor( index, maskIndex );
                break;
        }
    }
}

void VGA_DAC_UpdateColorPalette() {
	for ( Bitu i = 0;i<256;i++) 
		VGA_DAC_UpdateColor( i );
}

void write_p3c6(Bitu port,Bitu val,Bitu iolen) {
	if((IS_VGA_ARCH) && (svgaCard==SVGA_None) && (vga.dac.hidac_counter>3)) {
		vga.dac.reg02=val;
		vga.dac.hidac_counter=0;
		VGA_StartResize();
		return;
	}
	if ( vga.dac.pel_mask != val ) {
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DCA:Pel Mask set to %X", (int)val);
		vga.dac.pel_mask = val;
		VGA_DAC_UpdateColorPalette();
	}
}


Bitu read_p3c6(Bitu port,Bitu iolen) {
	if (vga_enable_3C6_ramdac)
		vga.dac.hidac_counter++;

	return vga.dac.pel_mask;
}


void write_p3c7(Bitu port,Bitu val,Bitu iolen) {
	vga.dac.hidac_counter=0;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_READ;
	vga.dac.read_index=val;         /* NTS: Paradise SVGA behavior, read index = x, write index = x + 1 */
	vga.dac.write_index=val + 1;
}

Bitu read_p3c7(Bitu port,Bitu iolen) {
	vga.dac.hidac_counter=0;
	if (vga.dac.state==DAC_READ) return 0x3;
	else return 0x0;
}

void write_p3c8(Bitu port,Bitu val,Bitu iolen) {
	vga.dac.hidac_counter=0;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_WRITE;
	vga.dac.write_index=val;        /* NTS: Paradise SVGA behavior, this affects write index, but not read index */
}

Bitu read_p3c8(Bitu port, Bitu iolen){
	vga.dac.hidac_counter=0;
	return vga.dac.write_index;
}

void write_p3c9(Bitu port,Bitu val,Bitu iolen) {
	vga.dac.hidac_counter=0;
	val&=0x3f;
	switch (vga.dac.pel_index) {
	case 0:
		vga.dac.rgb[vga.dac.write_index].red=val;
		vga.dac.pel_index=1;
		break;
	case 1:
		vga.dac.rgb[vga.dac.write_index].green=val;
		vga.dac.pel_index=2;
		break;
	case 2:
		vga.dac.rgb[vga.dac.write_index].blue=val;
		switch (vga.mode) {
		case M_VGA:
		case M_LIN8:
			VGA_DAC_UpdateColor( vga.dac.write_index );
			if ( GCC_UNLIKELY( vga.dac.pel_mask != 0xff)) {
				Bitu index = vga.dac.write_index;
				if ( (index & vga.dac.pel_mask) == index ) {
					for ( Bitu i = index+1;i<256;i++) 
						if ( (i & vga.dac.pel_mask) == index )
							VGA_DAC_UpdateColor( i );
				}
			} 
			break;
		default:
			/* Check for attributes and DAC entry link */
			for (Bitu i=0;i<16;i++) {
				if (vga.dac.combine[i]==vga.dac.write_index) {
					VGA_DAC_SendColor( i, vga.dac.write_index );
				}
			}
		}
		vga.dac.read_index=vga.dac.write_index++;                           // NTS: Paradise SVGA behavior
		vga.dac.pel_index=0;
		break;
	default:
		LOG(LOG_VGAGFX,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
		break;
	};
}

Bitu read_p3c9(Bitu port,Bitu iolen) {
	vga.dac.hidac_counter=0;
	Bit8u ret;
	switch (vga.dac.pel_index) {
	case 0:
		ret=vga.dac.rgb[vga.dac.read_index].red;
		vga.dac.pel_index=1;
		break;
	case 1:
		ret=vga.dac.rgb[vga.dac.read_index].green;
		vga.dac.pel_index=2;
		break;
	case 2:
		ret=vga.dac.rgb[vga.dac.read_index].blue;
		vga.dac.pel_index=0;
        vga.dac.read_index=vga.dac.write_index++;                           // NTS: Paradise SVGA behavior
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
		ret=0;
		break;
	}
	return ret;
}

void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal) {
	/* Check if this is a new color */
	vga.dac.combine[attr]=pal;
	switch (vga.mode) {
	case M_LIN8:
		break;
	case M_VGA:
		// used by copper demo; almost no video card seems to support it
		// Update: supported by ET4000AX (and not by ET4000AF)
	default:
		VGA_DAC_SendColor( attr, pal );
	}
}

void VGA_DAC_SetEntry(Bitu entry,Bit8u red,Bit8u green,Bit8u blue) {
	//Should only be called in machine != vga
	vga.dac.rgb[entry].red=red;
	vga.dac.rgb[entry].green=green;
	vga.dac.rgb[entry].blue=blue;
	for (Bitu i=0;i<16;i++) 
		if (vga.dac.combine[i]==entry)
			VGA_DAC_SendColor( i, i );
}

void VGA_SetupDAC(void) {
	vga.dac.first_changed=256;
	vga.dac.bits=6;
	vga.dac.pel_mask=0xff;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_READ;
	vga.dac.read_index=0;
	vga.dac.write_index=0;
	vga.dac.hidac_counter=0;
	vga.dac.reg02=0;
	if (IS_VGA_ARCH) {
		/* Setup the DAC IO port Handlers */
		if (svga.setup_dac) {
			svga.setup_dac();
		} else {
			IO_RegisterWriteHandler(0x3c6,write_p3c6,IO_MB);
			IO_RegisterReadHandler(0x3c6,read_p3c6,IO_MB);
			IO_RegisterWriteHandler(0x3c7,write_p3c7,IO_MB);
			IO_RegisterReadHandler(0x3c7,read_p3c7,IO_MB);
			IO_RegisterWriteHandler(0x3c8,write_p3c8,IO_MB);
			IO_RegisterReadHandler(0x3c8,read_p3c8,IO_MB);
			IO_RegisterWriteHandler(0x3c9,write_p3c9,IO_MB);
			IO_RegisterReadHandler(0x3c9,read_p3c9,IO_MB);
		}
	}
}

void VGA_UnsetupDAC(void) {
    IO_FreeWriteHandler(0x3c6,IO_MB);
    IO_FreeReadHandler(0x3c6,IO_MB);
    IO_FreeWriteHandler(0x3c7,IO_MB);
    IO_FreeReadHandler(0x3c7,IO_MB);
    IO_FreeWriteHandler(0x3c8,IO_MB);
    IO_FreeReadHandler(0x3c8,IO_MB);
    IO_FreeWriteHandler(0x3c9,IO_MB);
    IO_FreeReadHandler(0x3c9,IO_MB);
}

