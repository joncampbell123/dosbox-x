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


#include <string.h>
#include <stddef.h>

#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "dos_inc.h"

int hack_lfb_yadjust = 0;

bool vesa_zero_on_get_information = true;

extern int vesa_mode_width_cap;
extern int vesa_mode_height_cap;
extern bool enable_vga_8bit_dac;
extern bool allow_explicit_vesa_24bpp;
extern bool allow_vesa_lowres_modes;
extern bool allow_vesa_4bpp_packed;
extern bool vesa12_modes_32bpp;
extern bool allow_vesa_32bpp;
extern bool allow_vesa_24bpp;
extern bool allow_vesa_16bpp;
extern bool allow_vesa_15bpp;
extern bool allow_vesa_8bpp;
extern bool allow_vesa_4bpp;
extern bool allow_vesa_tty;
extern bool vga_8bit_dac;

#define VESA_SUCCESS          0x00
#define VESA_FAIL             0x01
#define VESA_HW_UNSUPPORTED   0x02
#define VESA_MODE_UNSUPPORTED 0x03
// internal definition to pass to the caller
#define VESA_UNIMPLEMENTED    0xFF

static struct {
	Bitu rmWindow;
	Bitu pmStart;
	Bitu pmWindow;
	Bitu pmPalette;
} callback = {0};

void CALLBACK_DeAllocate(Bitu in);

static char string_oem[]="S3 Incorporated. Trio64";
static char string_vendorname[]="DOSBox Development Team";
static char string_productname[]="DOSBox - The DOS Emulator";
static char string_productrev[]="2";

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

void VESA_OnReset_Clear_Callbacks(void) {
    if (callback.rmWindow != 0) {
        CALLBACK_DeAllocate(callback.rmWindow);
        callback.rmWindow = 0;
    }
    if (callback.pmPalette != 0) {
        CALLBACK_DeAllocate(callback.pmPalette);
        callback.pmPalette = 0;
    }
    if (callback.pmStart != 0) {
        CALLBACK_DeAllocate(callback.pmStart);
        callback.pmStart = 0;
    }
    if (callback.pmWindow != 0) {
        CALLBACK_DeAllocate(callback.pmWindow);
        callback.pmWindow = 0;
    }
}

extern bool vesa_bios_modelist_in_info;

Bit8u VESA_GetSVGAInformation(Bit16u seg,Bit16u off) {
	/* Fill 256 byte buffer with VESA information */
	PhysPt buffer=PhysMake(seg,off);
	Bitu i;
	bool vbe2=false;Bit16u vbe2_pos;
	Bitu id=mem_readd(buffer);
	if (((id==0x56424532)||(id==0x32454256)) && (!int10.vesa_oldvbe)) vbe2=true;

    /* The reason this is an option is that there are some old DOS games that assume the BIOS
     * fills in only the structure members. These games do not provide enough room for the
     * full 256-byte block. If we zero the entire block, unrelated data next to the buffer
     * gets wiped and the game crashes. */
    if (vesa_zero_on_get_information) {
        if (vbe2) {
            for (i=0;i<0x200;i++) mem_writeb((PhysPt)(buffer+i),0);		
        } else {
            for (i=0;i<0x100;i++) mem_writeb((PhysPt)(buffer+i),0);
        }
    }

	/* Fill common data */
	MEM_BlockWrite(buffer,(void *)"VESA",4);				//Identification
	if (!int10.vesa_oldvbe) mem_writew(buffer+0x04,0x200);	//Vesa version 2.0
	else mem_writew(buffer+0x04,0x102);						//Vesa version 1.2
	if (vbe2) {
        vbe2_pos=256+off;

		mem_writed(buffer+0x06,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_oem);i++) real_writeb(seg,vbe2_pos++,(Bit8u)string_oem[i]);
		mem_writew(buffer+0x14,0x200);					//VBE 2 software revision
		mem_writed(buffer+0x16,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_vendorname);i++) real_writeb(seg,vbe2_pos++,(Bit8u)string_vendorname[i]);
		mem_writed(buffer+0x1a,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productname);i++) real_writeb(seg,vbe2_pos++,(Bit8u)string_productname[i]);
		mem_writed(buffer+0x1e,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productrev);i++) real_writeb(seg,vbe2_pos++,(Bit8u)string_productrev[i]);
    } else {
        vbe2_pos=0x20+off;

        mem_writed(buffer+0x06,int10.rom.oemstring);	//Oemstring
	}

    if (vesa_bios_modelist_in_info) {
        /* put the modelist into the VBE struct itself, as modern BIOSes like to do.
         * NOTICE: This limits the modelist to what is able to fit! Extended modes may not fit, which is why the option is OFF by default. */
        uint16_t modesg = int10.rom.vesa_modes >> 16;
        uint16_t modoff = int10.rom.vesa_modes & 0xFFFF;

        mem_writed(buffer+0x0e,RealMake(seg,vbe2_pos));	//VESA Mode list

        do {
            if (vbe2) {
                if (vbe2_pos >= (509+off)) break;
            }
            else {
                if (vbe2_pos >= (253+off)) break;
            }
            uint16_t m = real_readw(modesg,modoff);
            if (m == 0xFFFF) break;
            real_writew(seg,vbe2_pos,m);
            vbe2_pos += 2;
            modoff += 2;
        } while (1);
        real_writew(seg,vbe2_pos,0xFFFF);
    }
    else {
        mem_writed(buffer+0x0e,int10.rom.vesa_modes);	//VESA Mode list
    }

	mem_writed(buffer+0x0a,(enable_vga_8bit_dac ? 1 : 0));		//Capabilities and flags
	mem_writew(buffer+0x12,(Bit16u)(vga.mem.memsize/(64*1024))); // memory size in 64kb blocks
	return VESA_SUCCESS;
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
	while (ModeList_VGA[i].mode!=0xffff) {
		/* Hack for VBE 1.2 modes and 24/32bpp ambiguity */
		if (ModeList_VGA[i].mode >= 0x100 && ModeList_VGA[i].mode <= 0x11F &&
            !(ModeList_VGA[i].special & _USER_MODIFIED) &&
			((ModeList_VGA[i].type == M_LIN32 && !vesa12_modes_32bpp) ||
			 (ModeList_VGA[i].type == M_LIN24 && vesa12_modes_32bpp))) {
			/* ignore */
			i++;
		}
        /* ignore deleted modes */
        else if (ModeList_VGA[i].type == M_ERROR) {
            /* ignore */
            i++;
        }
		else if (mode==ModeList_VGA[i].mode)
			goto foundit;
		else
			i++;
	}
	return VESA_FAIL;
foundit:
	if ((int10.vesa_oldvbe) && (ModeList_VGA[i].mode>=0x120)) return 0x01;
	VideoModeBlock * mblock=&ModeList_VGA[i];

    /* Don't allow querying modes the SVGA card does not accept,
     * unless the user modified the mode. */
    if (svga.accepts_mode && !(mblock->special & _USER_MODIFIED)) {
		if (!svga.accepts_mode(mode)) return 0x01;
	}

    /* do not return information on deleted modes */
    if (mblock->type == M_ERROR) return 0x01;

	bool allow_res = allow_vesa_lowres_modes ||
		(ModeList_VGA[i].swidth >= 640 && ModeList_VGA[i].sheight >= 400);

	switch (mblock->type) {
	case M_PACKED4:
		if (!allow_vesa_4bpp_packed) return VESA_FAIL;//TODO: New option to disable
		pageSize = mblock->sheight * mblock->swidth/2;
		var_write(&minfo.BytesPerScanLine,(Bit16u)((((mblock->swidth+15U)/8U)&(~1U))*4)); /* NTS: 4bpp requires even value due to VGA registers, round up */
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,4);
		var_write(&minfo.MemoryModel,4);	//packed pixel
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN4:
		if (!allow_vesa_4bpp) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth/2;
		var_write(&minfo.BytesPerScanLine,(Bit16u)(((mblock->swidth+15U)/8U)&(~1U))); /* NTS: 4bpp requires even value due to VGA registers, round up */
		var_write(&minfo.NumberOfPlanes,0x4);
		var_write(&minfo.BitsPerPixel,4);//FIXME: Shouldn't this say 4 planes, 1 bit per pixel??
		var_write(&minfo.MemoryModel,3);	//ega planar mode
		modeAttributes = 0x1b;	// Color, graphics, no linear buffer
		break;
	case M_LIN8:
		if (!allow_vesa_8bpp || !allow_res) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth;
		var_write(&minfo.BytesPerScanLine,(Bit16u)mblock->swidth);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,8);
		var_write(&minfo.MemoryModel,4);		//packed pixel
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN15:
		if (!allow_vesa_15bpp || !allow_res) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth*2;
		var_write(&minfo.BytesPerScanLine,(Bit16u)(mblock->swidth*2));
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
		if (!allow_vesa_16bpp || !allow_res) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth*2;
		var_write(&minfo.BytesPerScanLine,(Bit16u)(mblock->swidth*2));
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
	case M_LIN24:
		if (!allow_vesa_24bpp || !allow_res) return VESA_FAIL;
        if (mode >= 0x120 && !allow_explicit_vesa_24bpp) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth*3;
		var_write(&minfo.BytesPerScanLine,(Bit16u)(mblock->swidth*3));
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,24);
		var_write(&minfo.MemoryModel,6);	//HiColour
		var_write(&minfo.RedMaskSize,8);
		var_write(&minfo.RedMaskPos,0x10);
		var_write(&minfo.GreenMaskSize,0x8);
		var_write(&minfo.GreenMaskPos,0x8);
		var_write(&minfo.BlueMaskSize,0x8);
		var_write(&minfo.BlueMaskPos,0x0);
		modeAttributes = 0x1b;	// Color, graphics
		if (!int10.vesa_nolfb) modeAttributes |= 0x80;	// linear framebuffer
		break;
	case M_LIN32:
		if (!allow_vesa_32bpp || !allow_res) return VESA_FAIL;
		pageSize = mblock->sheight * mblock->swidth*4;
		var_write(&minfo.BytesPerScanLine,(Bit16u)(mblock->swidth*4));
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
	case M_TEXT:
		if (!allow_vesa_tty) return VESA_FAIL;
		pageSize = 0;
		var_write(&minfo.BytesPerScanLine, (Bit16u)(mblock->twidth * 2));
		var_write(&minfo.NumberOfPlanes,0x4);
		var_write(&minfo.BitsPerPixel,4);
		var_write(&minfo.MemoryModel,0);	// text
		modeAttributes = 0x0f;	//Color, text, bios output
		break;
	default:
		return VESA_FAIL;
	}
	if (pageSize & 0xFFFFu) {
		// It is documented that many applications assume 64k-aligned page sizes
		// VBETEST is one of them
		pageSize +=  0xFFFFu;
		pageSize &= ~0xFFFFu;
	}
	Bitu pages = 0;
	if (pageSize > vga.mem.memsize || (mblock->special & _USER_DISABLED)) {
		// mode not supported by current hardware configuration
		modeAttributes &= ~0x1;
	} else if (pageSize) {
		pages = (vga.mem.memsize / pageSize)-1;
	}
	var_write(&minfo.NumberOfImagePages, (Bit8u)pages);
	var_write(&minfo.ModeAttributes, modeAttributes);
	var_write(&minfo.WinAAttributes, 0x7);	// Exists/readable/writable

	if (mblock->type==M_TEXT) {
		var_write(&minfo.WinGranularity,32);
		var_write(&minfo.WinSize,32);
		var_write(&minfo.WinASegment,(Bit16u)0xb800);
		var_write(&minfo.XResolution,(Bit16u)mblock->twidth);
		var_write(&minfo.YResolution,(Bit16u)mblock->theight);
	} else {
		var_write(&minfo.WinGranularity,64);
		var_write(&minfo.WinSize,64);
		var_write(&minfo.WinASegment,(Bit16u)0xa000);
		var_write(&minfo.XResolution,(Bit16u)mblock->swidth);
		var_write(&minfo.YResolution,(Bit16u)mblock->sheight);
	}
	var_write(&minfo.WinFuncPtr,int10.rom.set_window);
	var_write(&minfo.NumberOfBanks,0x1);
	var_write(&minfo.Reserved_page,0x1);
	var_write(&minfo.XCharSize,(Bit8u)mblock->cwidth);
	var_write(&minfo.YCharSize,(Bit8u)mblock->cheight);
	if (!int10.vesa_nolfb) var_write(&minfo.PhysBasePtr,S3_LFB_BASE + (hack_lfb_yadjust*(long)host_readw((HostPt)(&minfo.BytesPerScanLine))));

	MEM_BlockWrite(buf,&minfo,sizeof(MODE_INFO));
	return VESA_SUCCESS;
}


Bit8u VESA_SetSVGAMode(Bit16u mode) {
	if (INT10_SetVideoMode(mode)) {
		int10.vesa_setmode=mode&0x7fff;
		return VESA_SUCCESS;
	}
	return VESA_FAIL;
}

Bit8u VESA_GetSVGAMode(Bit16u & mode) {
	if (int10.vesa_setmode!=0xffff) mode=int10.vesa_setmode;
	else mode=CurMode->mode;
	return VESA_SUCCESS;
}

Bit8u VESA_SetCPUWindow(Bit8u window,Bit8u address) {
	if (window) return VESA_FAIL;
	if ((Bit32u)(address)*64*1024<vga.mem.memsize) {
		IO_Write(0x3d4,0x6a);
		IO_Write(0x3d5,(Bit8u)address);
		return VESA_SUCCESS;
	} else return VESA_FAIL;
}

Bit8u VESA_GetCPUWindow(Bit8u window,Bit16u & address) {
	if (window) return VESA_FAIL;
	IO_Write(0x3d4,0x6a);
	address=IO_Read(0x3d5);
	return VESA_SUCCESS;
}


Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count,bool wait) {
//Structure is (vesa 3.0 doc): blue,green,red,alignment
	if (index>255) return VESA_FAIL;
	if (index+count>256) return VESA_FAIL;
	
	// Wait for retrace if requested
	if (wait) CALLBACK_RunRealFar(RealSeg(int10.rom.wait_retrace),RealOff(int10.rom.wait_retrace));
	
	IO_Write(0x3c8,(Bit8u)index);
	while (count) {
		Bit8u b = mem_readb(data++);
		Bit8u g = mem_readb(data++);
		Bit8u r = mem_readb(data++);
		data++;
		IO_Write(0x3c9,r);
		IO_Write(0x3c9,g);
		IO_Write(0x3c9,b);
		count--;
	}
	return VESA_SUCCESS;
}


Bit8u VESA_GetPalette(PhysPt data,Bitu index,Bitu count) {
	if (index>255) return VESA_FAIL;
	if (index+count>256) return VESA_FAIL;
	IO_Write(0x3c7,(Bit8u)index);
	while (count) {
		Bit8u r = IO_Read(0x3c9);
		Bit8u g = IO_Read(0x3c9);
		Bit8u b = IO_Read(0x3c9);
		mem_writeb(data++,b);
		mem_writeb(data++,g);
		mem_writeb(data++,r);
		data++;
		count--;
	}
	return VESA_SUCCESS;
}

// maximum offset for the S3 Trio64 is 10 bits
#define S3_MAX_OFFSET 0x3ff

Bit8u VESA_ScanLineLength(Bit8u subcall,Bit16u val, Bit16u & bytes,Bit16u & pixels,Bit16u & lines) {
	// offset register: virtual scanline length
	Bitu pixels_per_offset;
	Bitu bytes_per_offset = 8;
	Bitu vmemsize = vga.mem.memsize;
	Bitu new_offset = vga.config.scan_len;
	Bitu screen_height = CurMode->sheight;

	switch (CurMode->type) {
	case M_TEXT:
		vmemsize = 0x8000;      // we have only the 32kB window here
		screen_height = CurMode->theight;
		pixels_per_offset = 16; // two characters each 8 pixels wide
		bytes_per_offset = 4;   // 2 characters + 2 attributes
		break;
	case M_LIN4:
	case M_PACKED4:
		pixels_per_offset = 16;
		break;
	case M_LIN8:
		pixels_per_offset = 8;
		break;
	case M_LIN15:
	case M_LIN16:
		pixels_per_offset = 4;
		break;
	case M_LIN32:
		pixels_per_offset = 2;
		break;
	default:
		return VESA_MODE_UNSUPPORTED;
	}
	switch (subcall) {
	case 0x00: // set scan length in pixels
		new_offset = val / pixels_per_offset;
		if (val % pixels_per_offset) new_offset++;
		
		if (new_offset > S3_MAX_OFFSET)
			return VESA_HW_UNSUPPORTED; // scanline too long
		vga.config.scan_len = new_offset;
		VGA_CheckScanLength();
		break;

	case 0x01: // get current scanline length
		// implemented at the end of this function
		break;

	case 0x02: // set scan length in bytes
		new_offset = val / bytes_per_offset;
		if (val % bytes_per_offset) new_offset++;
		
		if (new_offset > S3_MAX_OFFSET)
			return VESA_HW_UNSUPPORTED; // scanline too long
		vga.config.scan_len = new_offset;
		VGA_CheckScanLength();
		break;

	case 0x03: // get maximum scan line length
		// the smaller of either the hardware maximum scanline length or
		// the limit to get full y resolution of this mode
		new_offset = S3_MAX_OFFSET;
		if ((new_offset * bytes_per_offset * screen_height) > vmemsize)
			new_offset = vmemsize / (bytes_per_offset * screen_height);
		break;

	default:
		return VESA_UNIMPLEMENTED;
	}

	// set up the return values
	bytes = (Bit16u)(new_offset * bytes_per_offset);
	pixels = (Bit16u)(new_offset * pixels_per_offset);
	if (!bytes)
		// return failure on division by zero
		// some real VESA BIOS implementations may crash here
		return VESA_FAIL;

	lines = (Bit16u)(vmemsize / bytes);
	
	if (CurMode->type==M_TEXT)
		lines *= (Bit16u)(CurMode->cheight);

	return VESA_SUCCESS;
}

Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y,bool wait) {
	Bitu pixels_per_offset;
	Bitu panning_factor = 1;

	switch (CurMode->type) {
	case M_TEXT:
	case M_LIN4:
	case M_PACKED4:
		pixels_per_offset = 16;
		break;
	case M_LIN8:
		panning_factor = 2; // the panning register ignores bit0 in this mode
		pixels_per_offset = 8;
		break;
	case M_LIN15:
	case M_LIN16:
		panning_factor = 2; // this may be DOSBox specific
		pixels_per_offset = 4;
		break;
	case M_LIN32:
		pixels_per_offset = 2;
		break;
	default:
		return VESA_MODE_UNSUPPORTED;
	}
	// We would have to divide y by the character height for text modes and
	// write the remainder to the CRTC preset row scan register,
	// but VBE2 BIOSes that actually get that far also don't.
	// Only a VBE3 BIOS got it right.
	Bitu virtual_screen_width = vga.config.scan_len * pixels_per_offset;
	Bitu new_start_pixel = virtual_screen_width * y + x;
	Bitu new_crtc_start = new_start_pixel / (pixels_per_offset/2);
	Bitu new_panning = new_start_pixel % (pixels_per_offset/2);
	new_panning *= panning_factor;

	vga.config.display_start = new_crtc_start;
	
	// Setting the panning register is nice as it allows for super smooth
	// scrolling, but if we hit the retrace pulse there may be flicker as
	// panning and display start are latched at different times. 

	IO_Read(0x3da);              // reset attribute flipflop
	IO_Write(0x3c0,0x13 | 0x20); // panning register, screen on
	IO_Write(0x3c0,(Bit8u)new_panning);
	
	// Wait for retrace if requested
	if (wait) CALLBACK_RunRealFar(RealSeg(int10.rom.wait_retrace),RealOff(int10.rom.wait_retrace));

	return VESA_SUCCESS;
}

Bit8u VESA_GetDisplayStart(Bit16u & x,Bit16u & y) {
	Bitu pixels_per_offset;
	Bitu panning_factor = 1;

	switch (CurMode->type) {
	case M_TEXT:
		pixels_per_offset = 16;
		break;
	case M_LIN4:
	case M_PACKED4:
		pixels_per_offset = 16;
		break;
	case M_LIN8:
		panning_factor = 2;
		pixels_per_offset = 8;
		break;
	case M_LIN15:
	case M_LIN16:
		panning_factor = 2;
		pixels_per_offset = 4;
		break;
	case M_LIN32:
		pixels_per_offset = 2;
		break;
	default:
		return VESA_MODE_UNSUPPORTED;
	}

	IO_Read(0x3da);              // reset attribute flipflop
	IO_Write(0x3c0,0x13 | 0x20); // panning register, screen on
	Bit8u panning = IO_Read(0x3c1);

	Bitu virtual_screen_width = vga.config.scan_len * pixels_per_offset;
	Bitu start_pixel = vga.config.display_start * (pixels_per_offset/2) 
		+ panning / panning_factor;
	
	y = (Bit16u)(start_pixel / virtual_screen_width);
	x = (Bit16u)(start_pixel % virtual_screen_width);
	return VESA_SUCCESS;
}

static Bitu VESA_SetWindow(void) {
	if (reg_bh) reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
	else reg_ah=VESA_SetCPUWindow(reg_bl,(Bit8u)reg_dx);
	reg_al=0x4f;
	return CBRET_NONE;
}

static Bitu VESA_PMSetWindow(void) {
	IO_Write(0x3d4,0x6a);
	IO_Write(0x3d5,reg_dl);
	return CBRET_NONE;
}
static Bitu VESA_PMSetPalette(void) {
	PhysPt data=SegPhys(es)+reg_edi;
	Bit32u count=reg_cx;
	IO_Write(0x3c8,reg_dl);
	do {
		IO_Write(0x3c9,mem_readb(data+2));
		IO_Write(0x3c9,mem_readb(data+1));
		IO_Write(0x3c9,mem_readb(data));
		data+=4;
	} while (--count);
	return CBRET_NONE;
}
static Bitu VESA_PMSetStart(void) {
	Bit32u start = (Bit32u)(((unsigned int)reg_dx << 16u) | (unsigned int)reg_cx);
	vga.config.display_start = start;
	return CBRET_NONE;
}



extern int vesa_modelist_cap;

Bitu INT10_WriteVESAModeList(Bitu max_modes) {
    Bitu mode_wptr = int10.rom.vesa_modes;
	Bitu i=0,modecount=0;

    //TODO Maybe add normal vga modes too, but only seems to complicate things
    while (ModeList_VGA[i].mode!=0xffff) {
        bool canuse_mode=false;

        /* Hack for VBE 1.2 modes and 24/32bpp ambiguity */
        if (ModeList_VGA[i].mode >= 0x100 && ModeList_VGA[i].mode <= 0x11F &&
                ((ModeList_VGA[i].type == M_LIN32 && !vesa12_modes_32bpp) ||
                 (ModeList_VGA[i].type == M_LIN24 && vesa12_modes_32bpp))) {
            /* ignore */
        }
        /* ignore deleted modes */
        else if (ModeList_VGA[i].type == M_ERROR) {
            /* ignore */
        }
        else {
            /* If there is no "accepts mode" then accept.
             *
             * If the user modified the mode, then accept.
             * If the mode exceeds video memory, then the mode will be reported as not supported by VESA BIOS functions.
             *
             * If the SVGA card would accept the mode (generally it's a memsize check), then accept. */
            if (!svga.accepts_mode)
                canuse_mode=true;
            else if (ModeList_VGA[i].special & _USER_MODIFIED)
                canuse_mode=true;
            else if (svga.accepts_mode(ModeList_VGA[i].mode))
                canuse_mode=true;

            if (canuse_mode) {
                if (ModeList_VGA[i].mode >= 0x100) {
                    bool allow1 = allow_vesa_lowres_modes ||
                        (ModeList_VGA[i].swidth >= 640 && ModeList_VGA[i].sheight >= 400);
                    bool allow2 =
                        allow_explicit_vesa_24bpp || ModeList_VGA[i].type != M_LIN24 ||
                        (ModeList_VGA[i].type == M_LIN24 && ModeList_VGA[i].mode < 0x120);
                    bool allow_res = allow1 && allow2;

                    switch (ModeList_VGA[i].type) {
                        case M_LIN32:	canuse_mode=allow_vesa_32bpp && allow_res; break;
                        case M_LIN24:	canuse_mode=allow_vesa_24bpp && allow_res; break;
                        case M_LIN16:	canuse_mode=allow_vesa_16bpp && allow_res; break;
                        case M_LIN15:	canuse_mode=allow_vesa_15bpp && allow_res; break;
                        case M_LIN8:	canuse_mode=allow_vesa_8bpp && allow_res; break;
                        case M_LIN4:	canuse_mode=allow_vesa_4bpp && allow_res; break;
                        case M_PACKED4:	canuse_mode=allow_vesa_4bpp_packed && allow_res; break;
                        case M_TEXT:	canuse_mode=allow_vesa_tty && allow_res; break;
                        default:	break;
                    }
                }
            }
        }

        if (canuse_mode && vesa_modelist_cap > 0 && (unsigned int)modecount >= (unsigned int)vesa_modelist_cap)
            canuse_mode = false;
        if (canuse_mode && (unsigned int)modecount >= (unsigned int)max_modes)
            canuse_mode = false;

        if (ModeList_VGA[i].type != M_TEXT) {
            if (canuse_mode && vesa_mode_width_cap > 0 && (unsigned int)ModeList_VGA[i].swidth > (unsigned int)vesa_mode_width_cap)
                canuse_mode = false;

            if (canuse_mode && vesa_mode_height_cap > 0 && (unsigned int)ModeList_VGA[i].sheight > (unsigned int)vesa_mode_height_cap)
                canuse_mode = false;
        }

        if (ModeList_VGA[i].mode>=0x100 && canuse_mode) {
            if ((!int10.vesa_oldvbe) || (ModeList_VGA[i].mode<0x120)) {
                phys_writew(PhysMake((Bit16u)0xc000,(Bit16u)mode_wptr),(Bit16u)ModeList_VGA[i].mode);
                mode_wptr+=2;
                modecount++;
            }
        }

        i++;
    }

    assert(modecount <= int10.rom.vesa_alloc_modes); /* do not overrun the buffer */

    /* after the buffer, is 0xFFFF */
    phys_writew(PhysMake((Bit16u)0xc000,(Bit16u)mode_wptr),(Bit16u)0xFFFF);
    mode_wptr+=2;

    return modecount;
}

void INT10_SetupVESA(void) {
	Bitu i,modecount=0;

	/* BUGFIX: Generating VESA BIOS data when machine=ega or machine=vgaonly is dumb.
	 * Stop wasting ROM space! --J.C. */
	if (machine != MCH_VGA) return;
	if (svgaCard == SVGA_None) return;

	/* Put the mode list somewhere in memory */
    int10.rom.vesa_alloc_modes = (Bit16u)(~0u);
    int10.rom.vesa_modes = RealMake(0xc000,int10.rom.used);
    modecount = INT10_WriteVESAModeList(0xFFFF/*max mode count*/);
    int10.rom.vesa_alloc_modes = (Bit16u)modecount;
    int10.rom.used += (Bit16u)(modecount * 2u);
	phys_writew(PhysMake(0xc000,int10.rom.used),0xffff);
	int10.rom.used+=2;
	int10.rom.oemstring=RealMake(0xc000,int10.rom.used);
	Bitu len=(Bitu)(strlen(string_oem)+1);
	for (i=0;i<len;i++) {
		phys_writeb(0xc0000u+(int10.rom.used++),(Bit8u)string_oem[i]);
	}
	/* Prepare the real mode interface */
	int10.rom.wait_retrace=RealMake(0xc000,int10.rom.used);
	int10.rom.used += (Bit16u)CALLBACK_Setup(0, NULL, CB_VESA_WAIT, PhysMake(0xc000,int10.rom.used), "");
	callback.rmWindow=CALLBACK_Allocate();
	int10.rom.set_window=RealMake(0xc000,int10.rom.used);
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.rmWindow, VESA_SetWindow, CB_RETF, PhysMake(0xc000,int10.rom.used), "VESA Real Set Window");
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
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.pmStart, VESA_PMSetStart, CB_VESA_PM, PhysMake(0xc000,int10.rom.used), "VESA PM Set Start");
	/* PM Set Palette call */
	int10.rom.pmode_interface_palette = int10.rom.used - RealOff( int10.rom.pmode_interface );
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 4, int10.rom.pmode_interface_palette);
	callback.pmPalette=CALLBACK_Allocate();
	int10.rom.used += (Bit16u)CALLBACK_Setup(0, NULL, CB_VESA_PM, PhysMake(0xc000,int10.rom.used), "");
	int10.rom.used += (Bit16u)CALLBACK_Setup(callback.pmPalette, VESA_PMSetPalette, CB_RETN, PhysMake(0xc000,int10.rom.used), "VESA PM Set Palette");
	/* Finalize the size and clear the required ports pointer */
	phys_writew( Real2Phys(int10.rom.pmode_interface) + 6, 0);
	int10.rom.pmode_interface_size=int10.rom.used - RealOff( int10.rom.pmode_interface );
}

