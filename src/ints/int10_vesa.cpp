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

/* $Id: int10_vesa.cpp,v 1.40 2009-07-31 15:36:01 c2woody Exp $ */

#include <string.h>
#include <stddef.h>

#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "dos_inc.h"

static struct {
	Bitu setwindow;
	Bitu pmStart;
	Bitu pmWindow;
	Bitu pmPalette;
} callback;

static char string_oem[]="S3 Incorporated. Trio64";
static char string_vendorname[]="DOSBox Development Team";
static char string_productname[]="DOSBox - The DOS Emulator";
static char string_productrev[]="DOSBox "VERSION;

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct MODE_INFO{
	Bit16u ModeAttributes;
	Bit8u WinAAttributes;
	Bit8u WinBAttributes;
	Bit16u WinGranularity;
	Bit16u WinSize;
	Bit16u WinASegment;
	Bit16u WinBSegment;
	Bit32u WinFuncPtr;
	Bit16u BytesPerScanLine;
	Bit16u XResolution;
	Bit16u YResolution;
	Bit8u XCharSize;
	Bit8u YCharSize;
	Bit8u NumberOfPlanes;
	Bit8u BitsPerPixel;
	Bit8u NumberOfBanks;
	Bit8u MemoryModel;
	Bit8u BankSize;
	Bit8u NumberOfImagePages;
	Bit8u Reserved_page;
	Bit8u RedMaskSize;
	Bit8u RedMaskPos;
	Bit8u GreenMaskSize;
	Bit8u GreenMaskPos;
	Bit8u BlueMaskSize;
	Bit8u BlueMaskPos;
	Bit8u ReservedMaskSize;
	Bit8u ReservedMaskPos;
	Bit8u DirectColorModeInfo;
	Bit32u PhysBasePtr;
	Bit32u OffScreenMemOffset;
	Bit16u OffScreenMemSize;
	Bit8u Reserved[206];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif



Bit8u VESA_GetSVGAInformation(Bit16u seg,Bit16u off) {
	/* Fill 256 byte buffer with VESA information */
	PhysPt buffer=PhysMake(seg,off);
	Bitu i;
	bool vbe2=false;Bit16u vbe2_pos=256+off;
	Bitu id=mem_readd(buffer);
	if (((id==0x56424532)||(id==0x32454256)) && (!int10.vesa_oldvbe)) vbe2=true;
	if (vbe2) {
		for (i=0;i<0x200;i++) mem_writeb(buffer+i,0);		
	} else {
		for (i=0;i<0x100;i++) mem_writeb(buffer+i,0);
	}
	/* Fill common data */
	MEM_BlockWrite(buffer,(void *)"VESA",4);				//Identification
	if (!int10.vesa_oldvbe) mem_writew(buffer+0x04,0x200);	//Vesa version 2.0
	else mem_writew(buffer+0x04,0x102);						//Vesa version 1.2
	if (vbe2) {
		mem_writed(buffer+0x06,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_oem);i++) real_writeb(seg,vbe2_pos++,string_oem[i]);
		mem_writew(buffer+0x14,0x200);					//VBE 2 software revision
		mem_writed(buffer+0x16,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_vendorname);i++) real_writeb(seg,vbe2_pos++,string_vendorname[i]);
		mem_writed(buffer+0x1a,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productname);i++) real_writeb(seg,vbe2_pos++,string_productname[i]);
		mem_writed(buffer+0x1e,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productrev);i++) real_writeb(seg,vbe2_pos++,string_productrev[i]);
	} else {
		mem_writed(buffer+0x06,int10.rom.oemstring);	//Oemstring
	}
	mem_writed(buffer+0x0a,0x0);					//Capabilities and flags
	mem_writed(buffer+0x0e,int10.rom.vesa_modes);	//VESA Mode list
	mem_writew(buffer+0x12,(Bit16u)(vga.vmemsize/(64*1024))); // memory size in 64kb blocks
	return 0x00;
}

Bit8u VESA_GetSVGAModeInformation(Bit16u mode,Bit16u seg,Bit16u off) {
	MODE_INFO minfo;
	memset(&minfo,0,sizeof(minfo));
	PhysPt buf=PhysMake(seg,off);
	Bitu pageSize;
	Bit8u modeAttributes;
	Bitu i=0;

	mode&=0x3fff;	// vbe2 compatible, ignore lfb and keep screen content bits
	if (mode<0x100) return 0x01;
	if (svga.accepts_mode) {
		if (!svga.accepts_mode(mode)) return 0x01;
	}
	while (ModeList_VGA[i].mode!=0xffff) {
		if (mode==ModeList_VGA[i].mode) goto foundit; else i++;
	}
	return 0x01;
foundit:
	if ((int10.vesa_oldvbe) && (ModeList_VGA[i].mode>=0x120)) return 0x01;
	VideoModeBlock * mblock=&ModeList_VGA[i];
	switch (mblock->type) {
	case M_LIN4:
		pageSize = mblock->sheight * mblock->swidth/2;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth/8);
		var_write(&minfo.NumberOfPlanes,0x4);
		var_write(&minfo.BitsPerPixel,4);
		var_write(&minfo.MemoryModel,3);	//ega planar mode
		modeAttributes = 0x1b;	// Color, graphics, no linear buffer
		break;
	case M_LIN8:
		pageSize = mblock->sheight * mblock->swidth;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,8);
		var_write(&minfo.MemoryModel,4);		//packed pixel
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN15:
		pageSize = mblock->sheight * mblock->swidth*2;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth*2);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,15);
		var_write(&minfo.MemoryModel,6);	//HiColour
		var_write(&minfo.RedMaskSize,5);
		var_write(&minfo.RedMaskPos,10);
		var_write(&minfo.GreenMaskSize,5);
		var_write(&minfo.GreenMaskPos,5);
		var_write(&minfo.BlueMaskSize,5);
		var_write(&minfo.BlueMaskPos,0);
		var_write(&minfo.ReservedMaskSize,0x01);
		var_write(&minfo.ReservedMaskPos,0x0f);
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN16:
		pageSize = mblock->sheight * mblock->swidth*2;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth*2);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,16);
		var_write(&minfo.MemoryModel,6);	//HiColour
		var_write(&minfo.RedMaskSize,5);
		var_write(&minfo.RedMaskPos,11);
		var_write(&minfo.GreenMaskSize,6);
		var_write(&minfo.GreenMaskPos,5);
		var_write(&minfo.BlueMaskSize,5);
		var_write(&minfo.BlueMaskPos,0);
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN32:
		pageSize = mblock->sheight * mblock->swidth*4;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth*4);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,32);
		var_write(&minfo.MemoryModel,6);	//HiColour
		var_write(&minfo.RedMaskSize,8);
		var_write(&minfo.RedMaskPos,0x10);
		var_write(&minfo.GreenMaskSize,0x8);
		var_write(&minfo.GreenMaskPos,0x8);
		var_write(&minfo.BlueMaskSize,0x8);
		var_write(&minfo.BlueMaskPos,0x0);
		var_write(&minfo.ReservedMaskSize,0x8);
		var_write(&minfo.ReservedMaskPos,0x18);
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
/*	case M_TEXT:
		pageSize = mblock->sheight/8 * mblock->swidth*2/8;
		pageSize = (pageSize | 15) & ~ 15;
		var_write(&minfo.BytesPerScanLine,mblock->swidth*2/8);
		var_write(&minfo.NumberOfPlanes,0x4);
		var_write(&minfo.BitsPerPixel,4);
		var_write(&minfo.MemoryModel,0);	//Text
		modeAttributes = 0x0f;	//Color, text, bios output
		break; */
	default:
		return 0x1;
	}
	var_write(&minfo.WinAAttributes,0x7);	// Exists/readable/writable
	
	if(pageSize > vga.vmemsize) {
		// Mode not supported by current hardware configuration
		var_write(&minfo.ModeAttributes, modeAttributes & ~0x1);
		var_write(&minfo.NumberOfImagePages,0);
	} else {
		var_write(&minfo.ModeAttributes, modeAttributes);
		Bitu pages = (vga.vmemsize / pageSize)-1;
		var_write(&minfo.NumberOfImagePages,pages);
	}

	if (mblock->type==M_TEXT) {
		var_write(&minfo.WinGranularity,32);
		var_write(&minfo.WinSize,32);
		var_write(&minfo.WinASegment,0xb800);
		var_write(&minfo.XResolution,mblock->swidth/8);
		var_write(&minfo.YResolution,mblock->sheight/8);
	} else {
		var_write(&minfo.WinGranularity,64);
		var_write(&minfo.WinSize,64);
		var_write(&minfo.WinASegment,0xa000);
		var_write(&minfo.XResolution,mblock->swidth);
		var_write(&minfo.YResolution,mblock->sheight);
	}
	var_write(&minfo.WinFuncPtr,CALLBACK_RealPointer(callback.setwindow));
	var_write(&minfo.NumberOfBanks,0x1);
	var_write(&minfo.Reserved_page,0x1);
	var_write(&minfo.XCharSize,mblock->cwidth);
	var_write(&minfo.YCharSize,mblock->cheight);
	if (!int10.vesa_nolfb) var_write(&minfo.PhysBasePtr,S3_LFB_BASE);

	MEM_BlockWrite(buf,&minfo,sizeof(MODE_INFO));
	return 0x00;
}


Bit8u VESA_SetSVGAMode(Bit16u mode) {
	if (INT10_SetVideoMode(mode)) {
		int10.vesa_setmode=mode&0x7fff;
		return 0x00;
	}
	return 0x01;
}

Bit8u VESA_GetSVGAMode(Bit16u & mode) {
	if (int10.vesa_setmode!=0xffff) mode=int10.vesa_setmode;
	else mode=CurMode->mode;
	return 0x00;
}

Bit8u VESA_SetCPUWindow(Bit8u window,Bit8u address) {
	if (window) return 0x1;
	if (((Bit32u)(address)*64*1024<vga.vmemsize)) {
		IO_Write(0x3d4,0x6a);
		IO_Write(0x3d5,(Bit8u)address);
		return 0x0;
	} else return 0x1;
}

Bit8u VESA_GetCPUWindow(Bit8u window,Bit16u & address) {
	if (window) return 0x1;
	IO_Write(0x3d4,0x6a);
	address=IO_Read(0x3d5);
	return 0x0;
}


Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count) {
//Structure is (vesa 3.0 doc): blue,green,red,alignment
	Bit8u r,g,b;
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c8,(Bit8u)index);
	while (count) {
		b = mem_readb(data++);
		g = mem_readb(data++);
		r = mem_readb(data++);
		data++;
		IO_Write(0x3c9,r);
		IO_Write(0x3c9,g);
		IO_Write(0x3c9,b);
		count--;
	}
	return 0x00;
}


Bit8u VESA_GetPalette(PhysPt data,Bitu index,Bitu count) {
	Bit8u r,g,b;
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c7,(Bit8u)index);
	while (count) {
		r = IO_Read(0x3c9);
		g = IO_Read(0x3c9);
		b = IO_Read(0x3c9);
		mem_writeb(data++,b);
		mem_writeb(data++,g);
		mem_writeb(data++,r);
		data++;
		count--;
	}
	return 0x00;
}


Bit8u VESA_ScanLineLength(Bit8u subcall,Bit16u val, Bit16u & bytes,Bit16u & pixels,Bit16u & lines) {
	Bit8u bpp;
	switch (CurMode->type) {
	case M_LIN4:
		bpp = 1;
		break;
	case M_LIN8:
		bpp=1;
		break;
	case M_LIN15:
	case M_LIN16:
		bpp=2;
		break;
	case M_LIN32:
		bpp=4;
		break;
	default:
		return 0x1;
	}
	switch (subcall) {
	case 0x00:	/* Set in pixels */
		if(CurMode->type==M_LIN4) vga.config.scan_len=val/2;
		else vga.config.scan_len = (val * bpp);
		break;
	case 0x02:	/* Set in bytes */
		if(CurMode->type==M_LIN4) vga.config.scan_len = val*4;
		else vga.config.scan_len = val;
		break;
	case 0x03:	/* Get maximum */
		bytes=0x400*4;
		pixels=bytes/bpp;
		lines = (Bit16u)(vga.vmemsize / bytes);
		return 0x00;
	case 0x01:	/* Get lengths */
		break;
	default:
		return 0x1;			//Illegal call
	}
	if (subcall!=0x01) {
		/* Write the scan line to video card the simple way */
		if (vga.config.scan_len & 7)
			vga.config.scan_len += 8;
		vga.config.scan_len /= 8;
	}
	if(CurMode->type==M_LIN4) {
		pixels=(vga.config.scan_len*16)/bpp;
		bytes=vga.config.scan_len*2;
		lines = (Bit16u)(vga.vmemsize /( bytes*4));
	}
	else {
		pixels=(vga.config.scan_len*8)/bpp;
		bytes=vga.config.scan_len*8;
		lines = (Bit16u)(vga.vmemsize / bytes);
	}
	VGA_StartResize();
	return 0x0;
}

Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y) {
	//TODO Maybe do things differently with lowres double line modes?	
	Bitu start;
	switch (CurMode->type) {
	case M_LIN4:
		start=vga.config.scan_len*16*y+x;
		vga.config.display_start=start/8;
		IO_Read(0x3da);
		IO_Write(0x3c0,0x13+32);
		IO_Write(0x3c0,start % 8);
		break;
	case M_LIN8:
		start=vga.config.scan_len*8*y+x;
		vga.config.display_start=start/4;
		IO_Read(0x3da);
		IO_Write(0x3c0,0x13+32);
		IO_Write(0x3c0,(start % 4)*2);
		break;
	case M_LIN16:
	case M_LIN15:
		start=vga.config.scan_len*8*y+x*2;
		vga.config.display_start=start/4;
		break;
	case M_LIN32:
		start=vga.config.scan_len*8*y+x*4;
		vga.config.display_start=start/4;
		break;
	default:
		return 0x1;
	}
	return 0x00;
}

Bit8u VESA_GetDisplayStart(Bit16u & x,Bit16u & y) {
	Bitu times=(vga.config.display_start*4)/(vga.config.scan_len*8);
	Bitu rem=(vga.config.display_start*4) % (vga.config.scan_len*8);
	Bitu pan=vga.config.pel_panning;
	switch (CurMode->type) {
	case M_LIN8:
		y=(Bit16u)times;
		x=(Bit16u)(rem+pan);
		break;
	default:
		return 0x1;
	}
	return 0x00;
}

static Bitu VESA_SetWindow(void) {
	if (reg_bh) reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
	else reg_ah=VESA_SetCPUWindow(reg_bl,(Bit8u)reg_dx);
	reg_al=0x4f;
	return 0;
}

static Bitu VESA_PMSetWindow(void) {
	VESA_SetCPUWindow(0,(Bit8u)reg_dx);
	return 0;
}
static Bitu VESA_PMSetPalette(void) {
	VESA_SetPalette(SegPhys(es) +  reg_edi, reg_dx, reg_cx );
	return 0;
}
static Bitu VESA_PMSetStart(void) {
	Bit32u start = (reg_dx << 16) | reg_cx;
	vga.config.display_start = start;
	return 0;
}




void INT10_SetupVESA(void) {
	/* Put the mode list somewhere in memory */
	Bitu i;
	i=0;
	int10.rom.vesa_modes=RealMake(0xc000,int10.rom.used);
//TODO Maybe add normal vga modes too, but only seems to complicate things
	while (ModeList_VGA[i].mode!=0xffff) {
		bool canuse_mode=false;
		if (!svga.accepts_mode) canuse_mode=true;
		else {
			if (svga.accepts_mode(ModeList_VGA[i].mode)) canuse_mode=true;
		}
		if (ModeList_VGA[i].mode>=0x100 && canuse_mode) {
			if ((!int10.vesa_oldvbe) || (ModeList_VGA[i].mode<0x120)) {
				phys_writew(PhysMake(0xc000,int10.rom.used),ModeList_VGA[i].mode);
				int10.rom.used+=2;
			}
		}
		i++;
	}
	phys_writew(PhysMake(0xc000,int10.rom.used),0xffff);
	int10.rom.used+=2;
	int10.rom.oemstring=RealMake(0xc000,int10.rom.used);
	Bitu len=(Bitu)(strlen(string_oem)+1);
	for (i=0;i<len;i++) {
		phys_writeb(0xc0000+int10.rom.used++,string_oem[i]);
	}
	switch (svgaCard) {
	case SVGA_S3Trio:
		break;
	}
	callback.setwindow=CALLBACK_Allocate();
	callback.pmPalette=CALLBACK_Allocate();
	callback.pmStart=CALLBACK_Allocate();
	CALLBACK_Setup(callback.setwindow,VESA_SetWindow,CB_RETF, "VESA Real Set Window");
	/* Prepare the pmode interface */
	int10.rom.pmode_interface=RealMake(0xc000,int10.rom.used);
	int10.rom.used += 8;		//Skip the byte later used for offsets
	/* PM Set Window call */
	int10.rom.pmode_interface_window = int10.rom.used - RealOff( int10.rom.pmode_interface );
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 0, int10.rom.pmode_interface_window );
	callback.pmWindow=CALLBACK_Allocate();
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.pmWindow, VESA_PMSetWindow, CB_RETN, PhysMake(0xc000,int10.rom.used), "VESA PM Set Window");
	/* PM Set start call */
	int10.rom.pmode_interface_start = int10.rom.used - RealOff( int10.rom.pmode_interface );
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 2, int10.rom.pmode_interface_start);
	callback.pmStart=CALLBACK_Allocate();
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.pmStart, VESA_PMSetStart, CB_RETN, PhysMake(0xc000,int10.rom.used), "VESA PM Set Start");
	/* PM Set Palette call */
	int10.rom.pmode_interface_palette = int10.rom.used - RealOff( int10.rom.pmode_interface );
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 4, int10.rom.pmode_interface_palette);
	callback.pmPalette=CALLBACK_Allocate();
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.pmPalette, VESA_PMSetPalette, CB_RETN, PhysMake(0xc000,int10.rom.used), "VESA PM Set Palette");
	/* Finalize the size and clear the required ports pointer */
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 6, 0);
	int10.rom.pmode_interface_size=int10.rom.used - RealOff( int10.rom.pmode_interface );
}

