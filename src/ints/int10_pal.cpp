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
#include "mem.h"
#include "inout.h"
#include "int10.h"

#define ACTL_MAX_REG   0x14

static INLINE void ResetACTL(void) {
	IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6);
}

static INLINE void WriteTandyACTL(Bit8u creg,Bit8u val) {
	IO_Write(VGAREG_TDY_ADDRESS,creg);
	if (machine==MCH_TANDY) IO_Write(VGAREG_TDY_DATA,val);
	else IO_Write(VGAREG_PCJR_DATA,val);
}

void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val) {
	switch (machine) {
	case MCH_PCJR:
		reg&=0xf;
		IO_Read(VGAREG_TDY_RESET);
		WriteTandyACTL(reg+0x10,val);
		IO_Write(0x3da,0x0); // palette back on
		break;
	case MCH_TANDY:
		// TODO waits for vertical retrace
		switch(vga.mode) {
		case M_TANDY2:
			if (reg >= 0x10) break;
			else if (reg==1) reg = 0x1f;
			else reg |= 0x10;
			WriteTandyACTL(reg+0x10,val);
			break;
		case M_TANDY4: {
			if (CurMode->mode!=0x0a) {
				// Palette values are kept constand by the BIOS.
				// The four colors are mapped to special palette values by hardware.
				// 3D8/3D9 registers influence this mapping. We need to figure out
				// which entry is used for the requested color.
				if (reg > 3) break;
				if (reg != 0) { // 0 is assumed to be at 0
					Bit8u color_select=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
					reg = reg*2+8; // Green Red Brown
					if (color_select& 0x20) reg++; // Cyan Magenta White
				}
				WriteTandyACTL(reg+0x10,val);
			} 
			// 4-color high resolution mode 0x0a isn't handled specially
			else WriteTandyACTL(reg+0x10,val);
			break;
		}
		default:
			WriteTandyACTL(reg+0x10,val);
			break;
		}
		IO_Write(0x3da,0x0); // palette back on
		break;
	case EGAVGA_ARCH_CASE:
		if (!IS_VGA_ARCH) reg&=0x1f;
		if(reg<=ACTL_MAX_REG) {
			ResetACTL();
			IO_Write(VGAREG_ACTL_ADDRESS,reg);
			IO_Write(VGAREG_ACTL_WRITE_DATA,val);
		}
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
		break;
	}
}


void INT10_SetOverscanBorderColor(Bit8u val) {
	switch (machine) {
	case TANDY_ARCH_CASE:
		IO_Read(VGAREG_TDY_RESET);
		WriteTandyACTL(0x02,val);
		IO_Write(VGAREG_TDY_ADDRESS, 0); // enable the screen
		break;
	case EGAVGA_ARCH_CASE:
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,0x11);
		IO_Write(VGAREG_ACTL_WRITE_DATA,val);
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
		break;
	}
}

void INT10_SetAllPaletteRegisters(PhysPt data) {
	switch (machine) {
	case TANDY_ARCH_CASE:
		IO_Read(VGAREG_TDY_RESET);
		// First the colors
		for(Bit8u i=0;i<0x10;i++) {
			WriteTandyACTL(i+0x10,mem_readb(data));
			data++;
		}
		// Then the border
		WriteTandyACTL(0x02,mem_readb(data));
		break;
	case EGAVGA_ARCH_CASE:
		ResetACTL();
		// First the colors
		for(Bit8u i=0;i<0x10;i++) {
			IO_Write(VGAREG_ACTL_ADDRESS,i);
			IO_Write(VGAREG_ACTL_WRITE_DATA,mem_readb(data));
			data++;
		}
		// Then the border
		IO_Write(VGAREG_ACTL_ADDRESS,0x11);
		IO_Write(VGAREG_ACTL_WRITE_DATA,mem_readb(data));
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
		break;
	}
}

void INT10_ToggleBlinkingBit(Bit8u state) {
	if(IS_VGA_ARCH) {
		Bit8u value;
	//	state&=0x01;
		if ((state>1) && (svgaCard==SVGA_S3Trio)) return;
		ResetACTL();
		
		IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		value=IO_Read(VGAREG_ACTL_READ_DATA);
		if (state<=1) {
			value&=0xf7;
			value|=state<<3;
		}

		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA,value);
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette

		if (state<=1) {
			Bit8u msrval=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR)&0xdf;
			if (state) msrval|=0x20;
			real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,msrval);
		}
	} else { // EGA
		// Usually it reads this from the mode list in ROM
		if (CurMode->type!=M_TEXT) return;

		Bit8u value = (CurMode->cwidth==9)? 0x4:0x0;
		if (state) value |= 0x8;
		
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA,value);
		IO_Write(VGAREG_ACTL_ADDRESS,0x20);

		Bit8u msrval=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR)& ~0x20;
		if (state) msrval|=0x20;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,msrval);
	}
}

void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val) {
	if(reg<=ACTL_MAX_REG) {
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,reg+32);
		*val=IO_Read(VGAREG_ACTL_READ_DATA);
		IO_Write(VGAREG_ACTL_WRITE_DATA,*val);
	}
}

void INT10_GetOverscanBorderColor(Bit8u * val) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	*val=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,*val);
}

void INT10_GetAllPaletteRegisters(PhysPt data) {
	ResetACTL();
	// First the colors
	for(Bit8u i=0;i<0x10;i++) {
		IO_Write(VGAREG_ACTL_ADDRESS,i);
		mem_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
		ResetACTL();
		data++;
	}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	mem_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
	ResetACTL();
}

void INT10_SetSingleDACRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue) {
	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	if ((real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL)&0x06)==0) {
		IO_Write(VGAREG_DAC_DATA,red);
		IO_Write(VGAREG_DAC_DATA,green);
		IO_Write(VGAREG_DAC_DATA,blue);
	} else {
		/* calculate clamped intensity, taken from VGABIOS */
		Bit32u i=(( 77*red + 151*green + 28*blue ) + 0x80) >> 8;
		Bit8u ic=(i>0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
		IO_Write(VGAREG_DAC_DATA,ic);
		IO_Write(VGAREG_DAC_DATA,ic);
		IO_Write(VGAREG_DAC_DATA,ic);
	}
}

void INT10_GetSingleDACRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue) {
	IO_Write(VGAREG_DAC_READ_ADDRESS,index);
	*red=IO_Read(VGAREG_DAC_DATA);
	*green=IO_Read(VGAREG_DAC_DATA);
	*blue=IO_Read(VGAREG_DAC_DATA);
}

void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	if ((real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL)&0x06)==0) {
		for (;count>0;count--) {
			IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
			IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
			IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
		}
	} else {
		for (;count>0;count--) {
			Bit8u red=mem_readb(data++);
			Bit8u green=mem_readb(data++);
			Bit8u blue=mem_readb(data++);

			/* calculate clamped intensity, taken from VGABIOS */
			Bit32u i=(( 77*red + 151*green + 28*blue ) + 0x80) >> 8;
			Bit8u ic=(i>0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
			IO_Write(VGAREG_DAC_DATA,ic);
			IO_Write(VGAREG_DAC_DATA,ic);
			IO_Write(VGAREG_DAC_DATA,ic);
		}
	}
}

void INT10_GetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_READ_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
	}
}

void INT10_SelectDACPage(Bit8u function,Bit8u mode) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	Bit8u old10=IO_Read(VGAREG_ACTL_READ_DATA);
	if (!function) {		//Select paging mode
		if (mode) old10|=0x80;
		else old10&=0x7f;
		//IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA,old10);
	} else {				//Select page
		IO_Write(VGAREG_ACTL_WRITE_DATA,old10);
		if (!(old10 & 0x80)) mode<<=2;
		mode&=0xf;
		IO_Write(VGAREG_ACTL_ADDRESS,0x14);
		IO_Write(VGAREG_ACTL_WRITE_DATA,mode);
	}
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

void INT10_GetDACPage(Bit8u* mode,Bit8u* page) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	Bit8u reg10=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,reg10);
	*mode=(reg10&0x80)?0x01:0x00;
	IO_Write(VGAREG_ACTL_ADDRESS,0x14);
	*page=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,*page);
	if(*mode) {
		*page&=0xf;
	} else {
		*page&=0xc;
		*page>>=2;
	}
}

void INT10_SetPelMask(Bit8u mask) {
	IO_Write(VGAREG_PEL_MASK,mask);
}	

void INT10_GetPelMask(Bit8u & mask) {
	mask=IO_Read(VGAREG_PEL_MASK);
}

void INT10_SetBackgroundBorder(Bit8u val) {
	Bit8u color_select=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	color_select=(color_select & 0xe0) | (val & 0x1f);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
	
	switch (machine) {
	case MCH_CGA:
		// only write the color select register
		IO_Write(0x3d9,color_select);
		break;
	case MCH_TANDY:
		// TODO handle val == 0x1x, wait for retrace
		switch(CurMode->mode) {
		default: // modes 0-5: write to color select and border
			INT10_SetOverscanBorderColor(val);
			IO_Write(0x3d9, color_select);
			break;
		case 0x06: // 2-color: only write the color select register
			IO_Write(0x3d9, color_select);
			break;
		case 0x07: // Tandy monochrome not implemented
			break; 
		case 0x08:
		case 0x09: // 16-color: write to color select, border and pal. index 0
			INT10_SetOverscanBorderColor(val);
			INT10_SetSinglePaletteRegister(0, val);
			IO_Write(0x3d9, color_select);
			break;
		case 0x0a: // 4-color highres:
			// write zero to color select, write palette to indexes 1-3
			// TODO palette
			IO_Write(0x3d9, 0);
			break;
		}
		break;
	case MCH_PCJR:
		IO_Read(VGAREG_TDY_RESET); // reset the flipflop
		if (vga.mode!=M_TANDY_TEXT) {
			IO_Write(VGAREG_TDY_ADDRESS, 0x10);
			IO_Write(VGAREG_PCJR_DATA, color_select&0xf);
		}
		IO_Write(VGAREG_TDY_ADDRESS, 0x2); // border color
		IO_Write(VGAREG_PCJR_DATA, color_select&0xf);
		break;
	case EGAVGA_ARCH_CASE:
		val = ((val << 1) & 0x10) | (val & 0x7);
		/* Always set the overscan color */
		INT10_SetSinglePaletteRegister( 0x11, val );
		/* Don't set any extra colors when in text mode */
		if (CurMode->mode <= 3)
			return;
		INT10_SetSinglePaletteRegister( 0, val );
		val = (color_select & 0x10) | 2 | ((color_select & 0x20) >> 5);
		INT10_SetSinglePaletteRegister( 1, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 2, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 3, val );
		break;
	}
}

void INT10_SetColorSelect(Bit8u val) {
	Bit8u temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xdf) | ((val & 1) ? 0x20 : 0x0);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	if (machine == MCH_CGA || machine == MCH_AMSTRAD || machine==MCH_TANDY)
		IO_Write(0x3d9,temp);
	else if (machine == MCH_PCJR) {
		IO_Read(VGAREG_TDY_RESET); // reset the flipflop
		switch(vga.mode) {
		case M_TANDY2:
			IO_Write(VGAREG_TDY_ADDRESS, 0x11);
			IO_Write(VGAREG_PCJR_DATA, val&1? 0xf:0);
			break;
		case M_TANDY4:
			for(Bit8u i = 0x11; i < 0x14; i++) {
				const Bit8u t4_table[] = {0,2,4,6, 0,3,5,0xf};
				IO_Write(VGAREG_TDY_ADDRESS, i);
				IO_Write(VGAREG_PCJR_DATA, t4_table[(i-0x10)+(val&1? 4:0)]);
			}
			break;
		default:
			// 16-color modes: always write the same palette
			for(Bit8u i = 0x11; i < 0x20; i++) {
				IO_Write(VGAREG_TDY_ADDRESS, i);
				IO_Write(VGAREG_PCJR_DATA, i-0x10);
			}
			break;
		}
		IO_Write(VGAREG_TDY_ADDRESS, 0); // enable palette
	}
	else if (IS_EGAVGA_ARCH) {
		if (CurMode->mode <= 3) //Maybe even skip the total function!
			return;
		val = (temp & 0x10) | 2 | val;
		INT10_SetSinglePaletteRegister( 1, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 2, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 3, val );
	}
}

void INT10_PerformGrayScaleSumming(Bit16u start_reg,Bit16u count) {
	if (count>0x100) count=0x100;
	for (Bitu ct=0; ct<count; ct++) {
		IO_Write(VGAREG_DAC_READ_ADDRESS,start_reg+ct);
		Bit8u red=IO_Read(VGAREG_DAC_DATA);
		Bit8u green=IO_Read(VGAREG_DAC_DATA);
		Bit8u blue=IO_Read(VGAREG_DAC_DATA);

		/* calculate clamped intensity, taken from VGABIOS */
		Bit32u i=(( 77*red + 151*green + 28*blue ) + 0x80) >> 8;
		Bit8u ic=(i>0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
		INT10_SetSingleDACRegister(start_reg+ct,ic,ic,ic);
	}
}
