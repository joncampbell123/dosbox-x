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
	case TANDY_ARCH_CASE:
		IO_Read(VGAREG_TDY_RESET);
		WriteTandyACTL(reg+0x10,val);
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

void INT10_SetSingleDacRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue) {
	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	IO_Write(VGAREG_DAC_DATA,red);
	IO_Write(VGAREG_DAC_DATA,green);
	IO_Write(VGAREG_DAC_DATA,blue);
}

void INT10_GetSingleDacRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue) {
	IO_Write(VGAREG_DAC_READ_ADDRESS,index);
	*red=IO_Read(VGAREG_DAC_DATA);
	*green=IO_Read(VGAREG_DAC_DATA);
	*blue=IO_Read(VGAREG_DAC_DATA);
}

void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
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
	Bit8u temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xe0) | (val & 0x1f);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	if (machine == MCH_CGA || machine == MCH_AMSTRAD || IS_TANDY_ARCH)
		IO_Write(0x3d9,temp);
	else if (IS_EGAVGA_ARCH) {
		val = ((val << 1) & 0x10) | (val & 0x7);
		/* Aways set the overscan color */
		INT10_SetSinglePaletteRegister( 0x11, val );
		/* Don't set any extra colors when in text mode */
		if (CurMode->mode <= 3)
			return;
		INT10_SetSinglePaletteRegister( 0, val );
		val = (temp & 0x10) | 2 | ((temp & 0x20) >> 5);
		INT10_SetSinglePaletteRegister( 1, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 2, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 3, val );
	}
}

void INT10_SetColorSelect(Bit8u val) {
	Bit8u temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xdf) | ((val & 1) ? 0x20 : 0x0);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	if (machine == MCH_CGA || IS_TANDY_ARCH)
		IO_Write(0x3d9,temp);
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
		INT10_SetSingleDacRegister(start_reg+ct,ic,ic,ic);
	}
}
