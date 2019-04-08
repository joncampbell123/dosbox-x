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
extern bool vga_8bit_dac;

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

    /* FIXME: CGA composite mode calls RENDER_SetPal itself, which conflicts with this code */
    if (vga.mode == M_CGA16)
        return;

    /* FIXME: Can someone behind the GCC project explain how (unsigned int) OR (unsigned int) somehow becomes (signed int)?? */

    if (vga_8bit_dac) {
        if (GFX_bpp >= 24) /* FIXME: Assumes 8:8:8. What happens when desktops start using the 10:10:10 format? */
            vga.dac.xlat32[index] =
                (uint32_t)(blue << (GFX_Bshift)) |
                (uint32_t)(green << (GFX_Gshift)) |
                (uint32_t)(red<< (GFX_Rshift)) |
                (uint32_t)GFX_Amask;
        else {
            /* FIXME: Assumes 5:6:5. I need to test against 5:5:5 format sometime. Perhaps I could dig out some older VGA cards and XFree86 drivers that support that format? */
            vga.dac.xlat16[index] =
                (uint16_t)(((blue&0xffu)>>3u)<<GFX_Bshift) |
                (uint16_t)(((green&0xffu)>>2u)<<GFX_Gshift) |
                (uint16_t)(((red&0xffu)>>3u)<<GFX_Rshift) |
                (uint16_t)GFX_Amask;

            /* PC-98 mode always renders 32bpp, therefore needs this fix */
            if (GFX_Bshift == 0)
                vga.dac.xlat32[index] = (uint32_t)(blue << 0U) | (uint32_t)(green << 8U) | (uint32_t)(red << 16U);
            else
                vga.dac.xlat32[index] = (uint32_t)(blue << 16U) | (uint32_t)(green << 8U) | (uint32_t)(red << 0U);
        }

        RENDER_SetPal( index, red, green, blue );
    }
    else {
        if (GFX_bpp >= 24) /* FIXME: Assumes 8:8:8. What happens when desktops start using the 10:10:10 format? */
            vga.dac.xlat32[index] =
                (uint32_t)(blue << (2u + GFX_Bshift)) |
                (uint32_t)(green << (2u + GFX_Gshift)) |
                (uint32_t)(red<<(2u + GFX_Rshift)) |
                (uint32_t)GFX_Amask;
        else {
            /* FIXME: Assumes 5:6:5. I need to test against 5:5:5 format sometime. Perhaps I could dig out some older VGA cards and XFree86 drivers that support that format? */
            vga.dac.xlat16[index] =
                (uint16_t)(((blue&0x3fu)>>1u)<<GFX_Bshift) |
                (uint16_t)((green&0x3fu)<<GFX_Gshift) |
                (uint16_t)(((red&0x3fu)>>1u)<<GFX_Rshift) |
                (uint16_t)GFX_Amask;

            /* PC-98 mode always renders 32bpp, therefore needs this fix */
            if (GFX_Bshift == 0)
                vga.dac.xlat32[index] = (uint32_t)(blue << 2U) | (uint32_t)(green << 10U) | (uint32_t)(red << 18U);
            else
                vga.dac.xlat32[index] = (uint32_t)(blue << 18U) | (uint32_t)(green << 10U) | (uint32_t)(red << 2U);
        }

        RENDER_SetPal( index, (red << 2u) | ( red >> 4u ), (green << 2u) | ( green >> 4u ), (blue << 2u) | ( blue >> 4u ) );
    }
}

void VGA_DAC_UpdateColor( Bitu index ) {
    Bitu maskIndex;

    if (IS_VGA_ARCH) {
        if (vga.mode == M_VGA || vga.mode == M_LIN8) {
            /* WARNING: This code assumes index < 256 */
            switch (VGA_AC_remap) {
                case AC_4x4:
                default: // <- just in case
                    /* Standard VGA hardware (including the original IBM PS/2 hardware) */
                    maskIndex  =  vga.dac.combine[index&0xF] & 0x0F;
                    maskIndex += (vga.dac.combine[index>>4u] & 0x0F) << 4u;
                    maskIndex &=  vga.dac.pel_mask;
                    break;
                case AC_low4:
                    /* Tseng ET4000 behavior, according to the SVGA card I have where only the low 4 bits are translated. --J.C. */
                    maskIndex  =  vga.dac.combine[index&0xF] & 0x0F;

                    /* FIXME: TEST THIS ON THE ACTUAL ET4000. This seems to make COPPER.EXE work correctly.
                     *        Is this what actual ET4000 hardware does in 256-color mode with Color Select? */
                    if (vga.attr.mode_control & 0x80)
                        maskIndex += vga.attr.color_select << 4;
                    else
                        maskIndex += index & 0xF0;

                    maskIndex &=  vga.dac.pel_mask;
                    break;
            }
        }
        else {
            maskIndex = vga.dac.combine[index&0xF] & vga.dac.pel_mask;
        }

        VGA_DAC_SendColor( index, maskIndex );
    }
    else if (machine == MCH_MCGA) {
        if (vga.mode == M_VGA || vga.mode == M_LIN8)
            maskIndex = index & vga.dac.pel_mask;
        else
            maskIndex = vga.dac.combine[index&0xF] & vga.dac.pel_mask;

        VGA_DAC_SendColor( index, maskIndex );
    }
    else {
        VGA_DAC_SendColor( index, index );
    }
}

void VGA_DAC_UpdateColorPalette() {
    for ( Bitu i = 0;i<256;i++) 
        VGA_DAC_UpdateColor( i );
}

void write_p3c6(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    if((IS_VGA_ARCH) && (vga.dac.hidac_counter>3)) {
        vga.dac.reg02=val;
        vga.dac.hidac_counter=0;
        VGA_StartResize();
        return;
    }
    if ( vga.dac.pel_mask != val ) {
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DCA:Pel Mask set to %X", (int)val);
        vga.dac.pel_mask = val;

        // TODO: MCGA 640x480 2-color mode appears to latch the DAC at retrace
        //       for background/foreground. Does that apply to the PEL mask too?

        VGA_DAC_UpdateColorPalette();
    }
}


Bitu read_p3c6(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    if (vga_enable_3C6_ramdac)
        vga.dac.hidac_counter++;

    return vga.dac.pel_mask;
}


void write_p3c7(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    vga.dac.hidac_counter=0;
    vga.dac.pel_index=0;
    vga.dac.state=DAC_READ;
    vga.dac.read_index=val;         /* NTS: Paradise SVGA behavior, read index = x, write index = x + 1 */
    vga.dac.write_index=val + 1;
}

Bitu read_p3c7(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    vga.dac.hidac_counter=0;
    if (vga.dac.state==DAC_READ) return 0x3;
    else return 0x0;
}

void write_p3c8(Bitu port,Bitu val,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    vga.dac.hidac_counter=0;
    vga.dac.pel_index=0;
    vga.dac.state=DAC_WRITE;
    vga.dac.write_index=val;        /* NTS: Paradise SVGA behavior, this affects write index, but not read index */
}

Bitu read_p3c8(Bitu port, Bitu iolen){
    (void)iolen;//UNUSED
    (void)port;//UNUSED
    vga.dac.hidac_counter=0;
    return vga.dac.write_index;
}

extern bool enable_vga_8bit_dac;
extern bool vga_palette_update_on_full_load;

static unsigned char tmp_dac[3] = {0,0,0};

void write_p3c9(Bitu port,Bitu val,Bitu iolen) {
    bool update = false;

    (void)iolen;//UNUSED
    (void)port;//UNUSED
    vga.dac.hidac_counter=0;

    if (!enable_vga_8bit_dac)
        val&=0x3f;

    if (vga.dac.pel_index < 3) {
        tmp_dac[vga.dac.pel_index]=val;

        if (!vga_palette_update_on_full_load) {
            /* update palette right away, partial change */
            switch (vga.dac.pel_index) {
                case 0:
                    vga.dac.rgb[vga.dac.write_index].red=tmp_dac[0];
                    break;
                case 1:
                    vga.dac.rgb[vga.dac.write_index].green=tmp_dac[1];
                    break;
                case 2:
                    vga.dac.rgb[vga.dac.write_index].blue=tmp_dac[2];
                    break;
            }
            update = true;
        }
        else if (vga.dac.pel_index == 2) {
            /* update palette ONLY when all three are given */
            vga.dac.rgb[vga.dac.write_index].red=tmp_dac[0];
            vga.dac.rgb[vga.dac.write_index].green=tmp_dac[1];
            vga.dac.rgb[vga.dac.write_index].blue=tmp_dac[2];
            update = true;
        }

        if ((++vga.dac.pel_index) >= 3)
            vga.dac.pel_index = 0;
    }

    if (update) {
        // As seen on real hardware: 640x480 2-color is the ONLY video mode
        // where the MCGA hardware appears to latch foreground and background
        // colors from the DAC at retrace, instead of always reading through
        // the DAC.
        //
        // Perhaps IBM couldn't get the DAC to run fast enough for 640x480 2-color mode.
        if (machine == MCH_MCGA && (vga.other.mcga_mode_control & 2)) {
            /* do not update the palette right now.
             * MCGA double-buffers foreground and background colors */
        }
        else {
            VGA_DAC_UpdateColorPalette(); // FIXME: Yes, this is very inefficient. Will improve later.
        }

        /* only if we just completed a color should we advance */
        if (vga.dac.pel_index == 0)
            vga.dac.read_index = vga.dac.write_index++;                           // NTS: Paradise SVGA behavior
    }
}

Bitu read_p3c9(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
    (void)port;//UNUSED
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
        LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");           //If this can actually happen that will be the day
        ret=0;
        break;
    }
    return ret;
}

void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal) {
    vga.dac.combine[attr] = pal;

    if (IS_VGA_ARCH) {
        if (vga.mode == M_VGA || vga.mode == M_LIN8) {
            switch (VGA_AC_remap) {
                case AC_4x4:
                default: // <- just in case
                    /* Standard VGA hardware (including the original IBM PS/2 hardware) */
                    for (unsigned int i=(unsigned int)attr;i < 0x100;i += 0x10)
                        VGA_DAC_UpdateColor( i );
                    for (unsigned int i=0;i < 0x10;i++)
                        VGA_DAC_UpdateColor( i + (attr<<4u) );
                    break;
                case AC_low4:
                    /* Tseng ET4000 behavior, according to the SVGA card I have where only the low 4 bits are translated. --J.C. */
                    for (unsigned int i=(unsigned int)attr;i < 0x100;i += 0x10)
                        VGA_DAC_UpdateColor( i );
                    break;
            }
        }
        else {
            for (unsigned int i=(unsigned int)attr;i < 0x100;i += 0x10)
                VGA_DAC_UpdateColor( i );
        }
    }
    else if (machine == MCH_MCGA) {
        VGA_DAC_UpdateColor( attr );
    }
    else {
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
    if (IS_VGA_ARCH || machine == MCH_MCGA) {
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

