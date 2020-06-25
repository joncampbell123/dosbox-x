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


#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"

static Bit8u cga_masks[4]={0x3f,0xcf,0xf3,0xfc};
static Bit8u cga_masks2[8]={0x7f,0xbf,0xdf,0xef,0xf7,0xfb,0xfd,0xfe};

void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color) {
	static bool putpixelwarned = false;

    if (IS_PC98_ARCH) {
        // TODO: Not supported yet
        return;
    }

	switch (CurMode->type) {
	case M_CGA4:
	{
		if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)<=5) {
			// this is a 16k mode
			Bit16u off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;

			Bit8u old=real_readb(0xb800,off);
			if (color & 0x80) {
				color&=3;
				old^=color << (2*(3-(x&3)));
			} else {
				old=(old&cga_masks[x&3])|((color&3) << (2*(3-(x&3))));
			}
			real_writeb(0xb800,off,old);
		} else {
			// a 32k mode: PCJr special case (see M_TANDY16)
			Bit16u seg;
			if (machine==MCH_PCJR) {
				Bit8u cpupage =
					(real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
				seg = cpupage << 10; // A14-16 to addr bits 14-16
			} else
				seg = 0xb800;

			Bit16u off=(y>>2)*160+((x>>2)&(~1));
			off+=(8*1024) * (y & 3);

			Bit16u old=real_readw(seg,off);
			if (color & 0x80) {
				old^=(color&1) << (7-(x&7));
				old^=((color&2)>>1) << ((7-(x&7))+8);
			} else {
				old=(old&(~(0x101<<(7-(x&7))))) | ((color&1) << (7-(x&7))) | (((color&2)>>1) << ((7-(x&7))+8));
			}
			real_writew(seg,off,old);
		}
	}
	break;
	case M_CGA2:
        if (machine == MCH_MCGA && real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) == 0x11) {
            Bit16u off=y*80+(x>>3);
            Bit8u old=real_readb(0xa000,off);

            if (color & 0x80) {
                color&=1;
                old^=color << (7-(x&7));
            } else {
                old=(old&cga_masks2[x&7])|((color&1) << (7-(x&7)));
            }
            real_writeb(0xa000,off,old);
        }
        else {
				Bit16u off=(y>>1)*80+(x>>3);
				if (y&1) off+=8*1024;
				Bit8u old=real_readb(0xb800,off);
				if (color & 0x80) {
					color&=1;
					old^=color << (7-(x&7));
				} else {
					old=(old&cga_masks2[x&7])|((color&1) << (7-(x&7)));
				}
				real_writeb(0xb800,off,old);
		}
		break;
	case M_TANDY16:
	{
		// find out if we are in a 32k mode (0x9 or 0xa)
		// This requires special handling on the PCJR
		// because only 16k are mapped at 0xB800
		bool is_32k = (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9)?
			true:false;

		Bit16u segment, offset;
		if (is_32k) {
			if (machine==MCH_PCJR) {
				Bit8u cpupage =
					(real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
				segment = cpupage << 10; // A14-16 to addr bits 14-16
			} else
				segment = 0xb800;
			// bits 1 and 0 of y select the bank
			// two pixels per byte (thus x>>1)
			offset = ((unsigned int)y >> 2u) * ((unsigned int)CurMode->swidth >> 1u) + ((unsigned int)x>>1u);
			// select the scanline bank
			offset += (8u*1024u) * ((unsigned int)y & 3u);
		} else {
			segment = 0xb800;
			// bit 0 of y selects the bank
			offset = ((unsigned int)y >> 1u) * ((unsigned int)CurMode->swidth >> 1u) + ((unsigned int)x>>1u);
			offset += (8u*1024u) * ((unsigned int)y & 1u);
		}

		// update the pixel
		Bit8u old=real_readb(segment, offset);
		Bit8u p[2];
		p[1] = (old >> 4u) & 0xf;
		p[0] = old & 0xf;
		Bitu ind = 1-(x & 0x1);

		if (color & 0x80) {
			// color is to be XORed
	 		p[ind]^=(color & 0x7f);
		} else {
			p[ind]=color;
		}
		old = (p[1] << 4u) | p[0];
		real_writeb(segment,offset, old);
	}
	break;
	case M_LIN4:
		if ((machine!=MCH_VGA) || (svgaCard!=SVGA_TsengET4K) ||
				(CurMode->swidth>800)) {
			// the ET4000 BIOS supports text output in 800x600 SVGA (Gateway 2)
			// putpixel warining?
			break;
		}
	case M_EGA:
		{
			/* Set the correct bitmask for the pixel position */
			IO_Write(0x3ce,0x8);Bit8u mask=128u>>(x&7u);IO_Write(0x3cf,mask);
			/* Set the color to set/reset register */
			IO_Write(0x3ce,0x0);IO_Write(0x3cf,color);
			/* Enable all the set/resets */
			IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
			/* test for xorring */
			if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x18); }
			//Perhaps also set mode 1 
			/* Calculate where the pixel is in video memory */
			if (CurMode->plength!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE))
				LOG(LOG_INT10,LOG_ERROR)("PutPixel_EGA_p: %x!=%x",(int)CurMode->plength,(int)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE));
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("PutPixel_EGA_w: %x!=%x",(int)CurMode->swidth,(int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
			PhysPt off=0xa0000u+(unsigned int)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE)*page+
				(((unsigned int)y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u+(unsigned int)x)>>3u);
			/* Bitmask and set/reset should do the rest */
			mem_readb(off);
			mem_writeb(off,0xff);
			/* Restore bitmask */	
			IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
			IO_Write(0x3ce,0x1);IO_Write(0x3cf,0);
			/* Restore write operating if changed */
			if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x0); }
			break;
		}

	case M_VGA:
		mem_writeb(PhysMake(0xa000,y*320u+x),color);
		break;
	case M_LIN8: {
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u)
				LOG(LOG_INT10,LOG_ERROR)("PutPixel_VGA_w: %x!=%x",(int)CurMode->swidth,(int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u);
			PhysPt off=S3_LFB_BASE+y*(unsigned int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u+x;
			mem_writeb(off,color);
			break;
		}
	default:
		if(GCC_UNLIKELY(!putpixelwarned)) {
			putpixelwarned = true;		
			LOG(LOG_INT10,LOG_ERROR)("PutPixel unhandled mode type %d",(int)CurMode->type);
		}
		break;
	}	
}

void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color) {
    if (IS_PC98_ARCH) {
        // TODO: Not supported yet
        return;
    }

	switch (CurMode->type) {
	case M_CGA4:
		{
			Bit16u off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;
			Bit8u val=real_readb(0xb800,off);
			*color=(val>>((3-(x&3))*2)) & 3 ;
		}
		break;
	case M_CGA2:
		{
			Bit16u off=(y>>1)*80+(x>>3);
			if (y&1) off+=8*1024;
			Bit8u val=real_readb(0xb800,off);
			*color=(val>>(7-(x&7))) & 1 ;
		}
		break;
	case M_TANDY16:
		{
			bool is_32k = (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9)?true:false;
			Bit16u segment, offset;
			if (is_32k) {
				if (machine==MCH_PCJR) {
					Bit8u cpupage = (real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
					segment = cpupage << 10;
				} else segment = 0xb800;
				offset = ((unsigned int)y >> 2u) * ((unsigned int)CurMode->swidth >> 1u) + ((unsigned int)x>>1u);
				offset += (8u*1024u) * (y & 3u);
			} else {
				segment = 0xb800;
				offset = ((unsigned int)y >> 1u) * ((unsigned int)CurMode->swidth >> 1u) + ((unsigned int)x>>1u);
				offset += (8u*1024u) * (y & 1u);
			}
			Bit8u val=real_readb(segment,offset);
			*color=(val>>((x&1)?0:4)) & 0xf;
		}
		break;
	case M_EGA:
		{
			/* Calculate where the pixel is in video memory */
			if (CurMode->plength!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE))
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_p: %x!=%x",(int)CurMode->plength,(int)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE));
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_w: %x!=%x",(int)CurMode->swidth,(int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
			PhysPt off=0xa0000u+(unsigned int)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE)*page+
				(((unsigned int)y*(unsigned int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u+(unsigned int)x)>>3u);
			Bitu shift=7u-(x & 7u);
			/* Set the read map */
			*color=0;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,0);
			*color|=((mem_readb(off)>>shift) & 1) << 0;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,1);
			*color|=((mem_readb(off)>>shift) & 1) << 1;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,2);
			*color|=((mem_readb(off)>>shift) & 1) << 2;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,3);
			*color|=((mem_readb(off)>>shift) & 1) << 3;
			break;
		}
	case M_VGA:
		*color=mem_readb(PhysMake(0xa000,320*y+x));
		break;
	case M_LIN8: {
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_VGA_w: %x!=%x",(int)CurMode->swidth,(int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
			PhysPt off=S3_LFB_BASE+(unsigned int)y*(unsigned int)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8u+(unsigned int)x;
			*color = mem_readb(off);
			break;
		}
	default:
		LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d",(int)CurMode->type);
		break;
	}	
}

