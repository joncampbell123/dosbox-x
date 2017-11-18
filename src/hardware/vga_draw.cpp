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


#include <string.h>
#include <math.h>
#include <stdio.h>
#include "dosbox.h"
#if defined (WIN32)
#include <d3d9.h>
#endif
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"
#include "timer.h"
#include "config.h"
#include "control.h"

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

bool et4k_highcolor_half_pixel_rate();

double vga_fps = 70;
double vga_mode_time_base = -1;
int vga_mode_frames_since_time_base = 0;

extern bool vga_3da_polled;
extern bool vga_page_flip_occurred;
extern bool vga_enable_hpel_effects;
extern bool vga_enable_hretrace_effects;
extern unsigned int vga_display_start_hretrace;
extern float hretrace_fx_avg_weight;
extern bool ignore_vblank_wraparound;
extern bool vga_double_buffered_line_compare;

void memxor(void *_d,unsigned int byte,size_t count) {
	unsigned char *d = (unsigned char*)_d;
	while (count-- > 0) *d++ ^= byte;
}

void memxor_greendotted_16bpp(uint16_t *d,unsigned int count,unsigned int line) {
	static const uint16_t greenptrn[2] = { (0x3F << 5), 0 };
	line &= 1;
	count >>= 1;
	while (count-- > 0) {
		*d++ ^= greenptrn[line];
		*d++ ^= greenptrn[line^1];
	}
}

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[SCALER_MAXWIDTH * 4 + 256];
static float hretrace_fx_avg = 0;

static Bit8u * VGA_Draw_AMS_4BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	const Bit8u *lbase;
	Bit32u *draw = (Bit32u *)TempLine;
	for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
		lbase = &base[ (vidstart & (8 * 1024 -1)) ];
		Bitu val0 = lbase[ 0 ];
		Bitu val1 = lbase[ 16384 ];
		Bitu val2 = lbase[ 32768 ];
		Bitu val3 = lbase[ 49152 ];
		
		*draw++=( ( CGA_2_Table[ val0 >> 4 ] << 0 ) | 
			( CGA_2_Table[ val1 >> 4 ] << 1 ) |
			( CGA_2_Table[ val2 >> 4 ] << 2 ) |
			( CGA_2_Table[ val3 >> 4 ] << 3 ) ) & vga.amstrad.mask_plane;
		*draw++=( ( CGA_2_Table[ val0 & 0x0F ] << 0 ) | 
			( CGA_2_Table[ val1 & 0x0F ] << 1 ) |
			( CGA_2_Table[ val2 & 0x0F ] << 2 ) |
			( CGA_2_Table[ val3 & 0x0F ] << 3 ) ) & vga.amstrad.mask_plane;
	}
	return TempLine;
}

struct vsync_state vsync;

float uservsyncjolt=0.0f;

void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode) {
	switch(vsyncmode) {
	case VS_Off:
		vsync.manual	= false;
		vsync.persistent= false;
		vsync.faithful	= false;
		break;
	case VS_On:
		vsync.manual	= true;
		vsync.persistent= true;
		vsync.faithful	= true;
		break;
	case VS_Force:
	case VS_Host:
		vsync.manual	= true;
		vsync.persistent= true;
		vsync.faithful	= false;
		break;
	default:
		LOG_MSG("VGA_VsyncUpdateMode: Invalid mode, using defaults.");
		vsync.manual	= false;
		vsync.persistent= false;
		vsync.faithful	= false;
		break;
	}
}

void VGA_TweakUserVsyncOffset(float val) { uservsyncjolt = val; }

static Bit8u * VGA_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u *draw = (Bit32u *)TempLine;
	for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
		Bitu val = base[(vidstart & (8 * 1024 -1))];
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_1BPP_Blend_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u *draw = (Bit32u *)TempLine;
	Bitu carry = 0;
	for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
		Bitu val1 = base[(vidstart & (8 * 1024 -1))];
		Bitu val2 = (val1 >> 1) + carry;
		carry = (val1 & 1) << 7;
		*draw++=CGA_2_Table[val1 >> 4] + CGA_2_Table[val2 >> 4];
		*draw++=CGA_2_Table[val1 & 0xf] + CGA_2_Table[val2 & 0xf];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
	const Bit32u *base = (Bit32u*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	VGA_Latch pixels;
	Bitu val,i;

	for (Bitu x=0;x<vga.draw.blocks;x++) {
		pixels.d = base[vidstart & vga.tandy.addr_mask];
		vidstart += 1<<vga.config.addr_shift;

		/* CGA odd/even mode, first plane */
		val=pixels.b[0];
		for (i=0;i < 4;i++,val <<= 2)
			*draw++ = vga.dac.xlat32[(val>>6)&3];

		/* CGA odd/even mode, second plane */
		val=pixels.b[1];
		for (i=0;i < 4;i++,val <<= 2)
			*draw++ = vga.dac.xlat32[(val>>6)&3];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_1BPP_Line_as_VGA(Bitu vidstart, Bitu line) {
	const Bit32u *base = (Bit32u*)vga.draw.linear_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	VGA_Latch pixels;
	Bitu val,i;

	for (Bitu x=0;x<vga.draw.blocks;x++) {
		pixels.d = base[vidstart & vga.tandy.addr_mask];
		vidstart += 1<<vga.config.addr_shift;

		val=pixels.b[0];
		for (i=0;i < 8;i++,val <<= 1)
			*draw++ = vga.dac.xlat32[(val>>7)&1];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		vidstart++;
		*draw++=CGA_4_Table[val];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPPHiRes_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=CGA_4_HiRes_Table[(val1>>4)|(val2&0xf0)];
		*draw++=CGA_4_HiRes_Table[(val1&0x0f)|((val2&0x0f)<<4)];
	}
	return TempLine;
}

static Bitu temp[643]={0};

static Bit8u * VGA_Draw_CGA16_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
#define CGA16_READER(OFF) (base[(vidstart +(OFF))& (8*1024 -1)])
	Bit32u * draw=(Bit32u *)TempLine;
	//There are 640 hdots in each line of the screen.
	//The color of an even hdot always depends on only 4 bits of video RAM.
	//The color of an odd hdot depends on 4 bits of video RAM in
	//1-hdot-per-pixel modes and 6 bits of video RAM in 2-hdot-per-pixel
	//modes. We always assume 6 and use duplicate palette entries in
	//1-hdot-per-pixel modes so that we can use the same routine for all
	//composite modes.
  temp[1] = (CGA16_READER(0) >> 6) & 3;
	for(Bitu x = 2; x < 640; x+=2) {
		temp[x] = (temp[x-1] & 0xf);
		temp[x+1] = (temp[x] << 2) | ((( CGA16_READER(x>>3)) >> (6-(x&6)) )&3);
	}
	temp[640] = temp[639] & 0xf;
	temp[641] = temp[640] << 2;
	temp[642] = temp[641] & 0xf;

	Bitu i = 2;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		*draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
		i += 4;
		*draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
		i += 4;
	}
	return TempLine;
#undef CGA16_READER
}

static Bit8u * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit8u* draw=TempLine;
	Bitu end = vga.draw.blocks*2;
	while(end) {
		Bit8u byte = base[vidstart & vga.tandy.addr_mask];
		*draw++=vga.attr.palette[byte >> 4];
		*draw++=vga.attr.palette[byte & 0x0f];
		vidstart++;
		end--;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit8u* draw=TempLine;
	Bitu end = vga.draw.blocks;
	while(end) {
		Bit8u byte = base[vidstart & vga.tandy.addr_mask];
		Bit8u data = vga.attr.palette[byte >> 4];
		*draw++ = data; *draw++ = data;
		data = vga.attr.palette[byte & 0x0f];
		*draw++ = data; *draw++ = data;
		vidstart++;
		end--;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_Linear_Line_24_to_32(Bitu vidstart, Bitu /*line*/) {
	Bitu offset = vidstart & vga.draw.linear_mask;
	Bitu i;

	/* DOSBox's render/scalar code can't handle 24bpp natively, so we have
	 * to convert 24bpp -> 32bpp.
	 *
	 * WARNING: My clever trick might crash on processors that don't support
	 *          unaligned memory addressing. To explain what it's doing, is
	 *          it's using a DWORD read to fetch the 24bpp pixel (plus an
	 *          extra byte), then overwrites the extra byte with 0xFF to
	 *          produce a valid RGBA 8:8:8:8 value with the original pixel's
	 *          RGB plus alpha channel value of 0xFF. */
	for (i=0;i < vga.draw.width;i++)
		((uint32_t*)TempLine)[i] = *((uint32_t*)(vga.draw.linear_base+offset+(i*3))) | 0xFF000000;

	return TempLine;
}

static Bit8u * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bitu offset = vidstart & vga.draw.linear_mask;
	Bit8u* ret = &vga.draw.linear_base[offset];
	
	// in case (vga.draw.line_length + offset) has bits set that
	// are not set in the mask: ((x|y)!=y) equals (x&~y)
	if (GCC_UNLIKELY((vga.draw.line_length + offset)& ~vga.draw.linear_mask)) {
		// this happens, if at all, only once per frame (1 of 480 lines)
		// in some obscure games
		Bitu end = (offset + vga.draw.line_length) & vga.draw.linear_mask;
		
		// assuming lines not longer than 4096 pixels
		Bitu wrapped_len = end & 0xFFF;
		Bitu unwrapped_len = vga.draw.line_length-wrapped_len;
		
		// unwrapped chunk: to top of memory block
		memcpy(TempLine, &vga.draw.linear_base[offset], unwrapped_len);
		// wrapped chunk: from base of memory block
		memcpy(&TempLine[unwrapped_len], vga.draw.linear_base, wrapped_len);

		ret = TempLine;
	}

#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;
}

/* WARNING: This routine assumes (vidstart&3) == 0 */
static Bit8u * VGA_Draw_Xlat32_VGA_CRTC_bmode_Line(Bitu vidstart, Bitu /*line*/) {
	Bit32u* temps = (Bit32u*) TempLine;
	int poff = 0;
	Bitu skip; /* how much to skip after drawing 4 pixels */

	skip = 4 << vga.config.addr_shift;

	/* *sigh* it looks like DOSBox's VGA scanline code will pass nonzero bits 0-1 in vidstart */
	poff += vidstart & 3;
	vidstart &= ~3;

	/* hack for Surprise! productions "copper" demo.
	 * when the demo talks about making the picture waver, what it's doing is diddling
	 * with the Start Horizontal Retrace register of the CRTC once per scanline.
	 * ...yeah, really. It's a wonder in retrospect the programmer didn't burn out his
	 * VGA monitor, and I bet this makes the demo effect unwatchable on LCD flat panels or
	 * scan converters that rely on the pulses to detect VGA mode changes! */
	if (vga_enable_hretrace_effects) {
		/* NTS: This is NOT BACKWARDS. It makes sense if you think about it: the monitor
		 *      begins swinging the electron beam back on horizontal retract, so if the
		 *      retrace starts sooner, then the blanking on the left side appears to last
		 *      longer because there are more clocks until active display.
		 *
		 *      Also don't forget horizontal total/blank/retrace etc. registers are in
		 *      character clocks not pixels. In 320x200x256 mode, one character clock is
		 *      4 pixels.
		 *
		 *      Finally, we average it with "weight" because CRTs have "inertia" */
		float a = 1.0 / (hretrace_fx_avg_weight + 1);

		hretrace_fx_avg *= 1.0 - a;
		hretrace_fx_avg += a * skip * ((int)vga_display_start_hretrace - (int)vga.crtc.start_horizontal_retrace);
		int x = (int)floor(hretrace_fx_avg + 0.5);

		vidstart += skip * (x >> 2);
		poff += x & 3;
	}

	for(Bitu i = 0; i < ((vga.draw.line_length>>(2/*32bpp*/+2/*4 pixels*/))+((poff+3)>>2)); i++) {
		Bit8u *ret = &vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];

		/* one group of 4 */
		*temps++ = vga.dac.xlat32[*ret++];
		*temps++ = vga.dac.xlat32[*ret++];
		*temps++ = vga.dac.xlat32[*ret++];
		*temps++ = vga.dac.xlat32[*ret++];
		/* and skip */
		vidstart += skip;
	}

	return TempLine + (poff * 4);
}

static Bit8u * VGA_Draw_Xlat32_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bit32u* temps = (Bit32u*) TempLine;

	for(Bitu i = 0; i < (vga.draw.line_length>>2); i++)
		temps[i]=vga.dac.xlat32[vga.draw.linear_base[(vidstart+i)&vga.draw.linear_mask]];

	return TempLine;
}

extern Bit32u Expand16Table[4][16];

static Bit8u * VGA_Draw_VGA_Planar_Xlat32_Line(Bitu vidstart, Bitu /*line*/) {
	Bit32u* temps = (Bit32u*) TempLine;
	Bit32u t1,t2,tmp;

	for (Bitu i = 0; i < ((vga.draw.line_length>>2)+vga.draw.panning); i += 8) {
		t1 = t2 = *((Bit32u*)(&vga.draw.linear_base[ vidstart & vga.draw.linear_mask ]));
		t1 = (t1 >> 4) & 0x0f0f0f0f;
		t2 &= 0x0f0f0f0f;
		vidstart += 4;

		tmp =	Expand16Table[0][(t1>>0)&0xFF] |
			Expand16Table[1][(t1>>8)&0xFF] |
			Expand16Table[2][(t1>>16)&0xFF] |
			Expand16Table[3][(t1>>24)&0xFF];
		temps[i+0] = vga.dac.xlat32[(tmp>>0)&0xFF];
		temps[i+1] = vga.dac.xlat32[(tmp>>8)&0xFF];
		temps[i+2] = vga.dac.xlat32[(tmp>>16)&0xFF];
		temps[i+3] = vga.dac.xlat32[(tmp>>24)&0xFF];

		tmp =	Expand16Table[0][(t2>>0)&0xFF] |
			Expand16Table[1][(t2>>8)&0xFF] |
			Expand16Table[2][(t2>>16)&0xFF] |
			Expand16Table[3][(t2>>24)&0xFF];
		temps[i+4] = vga.dac.xlat32[(tmp>>0)&0xFF];
		temps[i+5] = vga.dac.xlat32[(tmp>>8)&0xFF];
		temps[i+6] = vga.dac.xlat32[(tmp>>16)&0xFF];
		temps[i+7] = vga.dac.xlat32[(tmp>>24)&0xFF];
	}

	return TempLine + (vga.draw.panning*4);
}

//Test version, might as well keep it
/* static Bit8u * VGA_Draw_Chain_Line(Bitu vidstart, Bitu line) {
	Bitu i = 0;
	for ( i = 0; i < vga.draw.width;i++ ) {
		Bitu addr = vidstart + i;
		TempLine[i] = vga.mem.linear[((addr&~3)<<2)+(addr&3)];
	}
	return TempLine;
} */

static Bit8u * VGA_Draw_VGA_Line_Xlat32_HWMouse( Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		// HW Mouse not enabled, use the tried and true call
		return VGA_Draw_Xlat32_Linear_Line(vidstart, 0);

	Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) ||
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		// the mouse cursor *pattern* is not on this line
		return VGA_Draw_Xlat32_Linear_Line(vidstart, 0);
	} else {
		// Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
		// 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
		// This is used when the mouse cursor partially leaves the screen.
		// It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
		// AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
		Bit32u* temp = (Bit32u*)VGA_Draw_Xlat32_Linear_Line(vidstart, 0);
		//memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);

		// the index of the bit inside the cursor bitmap we start at:
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx;
		// convert to video memory addr and bit index
		// start adjusted to the pattern structure (thus shift address by 2 instead of 3)
		// Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		// stay at the right position in the pattern
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit32u* xat = &temp[vga.s3.hgc.originx]; // mouse data start pos. in scanline
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0; // only the first byte has some bits cut off
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= 0xFFFFFFFF; // Invert screen data
					//else Transparent
				} else if (bitsB&bit) {
					*xat = vga.dac.xlat32[vga.s3.hgc.forestack[0]]; // foreground color
				} else {
					*xat = vga.dac.xlat32[vga.s3.hgc.backstack[0]];
				}
				xat++;
			}
		}
		return (Bit8u*)temp;
	}
}

static Bit8u * VGA_Draw_VGA_Line_HWMouse( Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		// HW Mouse not enabled, use the tried and true call
		return &vga.mem.linear[vidstart];

	Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		// the mouse cursor *pattern* is not on this line
		return &vga.mem.linear[ vidstart ];
	} else {
		// Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
		// 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
		// This is used when the mouse cursor partially leaves the screen.
		// It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
		// AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);
		// the index of the bit inside the cursor bitmap we start at:
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		// convert to video memory addr and bit index
		// start adjusted to the pattern structure (thus shift address by 2 instead of 3)
		// Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		// stay at the right position in the pattern
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit8u* xat = &TempLine[vga.s3.hgc.originx]; // mouse data start pos. in scanline
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0; // only the first byte has some bits cut off
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= 0xFF; // Invert screen data
					//else Transparent
				} else if (bitsB&bit) {
					*xat = vga.s3.hgc.forestack[0]; // foreground color
				} else {
					*xat = vga.s3.hgc.backstack[0];
				}
				xat++;
			}
		}
		return TempLine;
	}
}

/* render 16bpp line DOUBLED horizontally */
static Bit8u * VGA_Draw_LIN16_Line_2x(Bitu vidstart, Bitu /*line*/) {
	Bit16u *s = (Bit16u*)(&vga.mem.linear[vidstart]);
	Bit16u *d = (Bit16u*)TempLine;

	for (Bitu i = 0; i < (vga.draw.line_length>>2); i++) {
		d[0] = d[1] = *s++;
		d += 2;
	}

	return TempLine;
}

static Bit8u * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 1) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[vidstart];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*2);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
 		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit16u* xat = &((Bit16u*)TempLine)[vga.s3.hgc.originx];
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					// byte order doesn't matter here as all bits get flipped
					if (bitsB&bit) *xat ^= ~0U;
					//else Transparent
				} else if (bitsB&bit) {
					// Source as well as destination are Bit8u arrays, 
					// so this should work out endian-wise?
					*xat = *(Bit16u*)vga.s3.hgc.forestack;
				} else {
					*xat = *(Bit16u*)vga.s3.hgc.backstack;
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static Bit8u * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 2) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[ vidstart ];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*4);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit32u* xat = &((Bit32u*)TempLine)[vga.s3.hgc.originx];
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= ~0U;
					//else Transparent
				} else if (bitsB&bit) {
					*xat = *(Bit32u*)vga.s3.hgc.forestack;
				} else {
					*xat = *(Bit32u*)vga.s3.hgc.backstack;
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static const Bit32u* VGA_Planar_Memwrap(Bitu vidstart) {
	vidstart &= vga.draw.planar_mask;
	return (Bit32u*)(&vga.mem.linear[vidstart << 2]);
}

static const Bit8u* VGA_Text_Memwrap(Bitu vidstart) {
	vidstart &= vga.draw.linear_mask;
	Bitu line_end = 2 * vga.draw.blocks;
	if (GCC_UNLIKELY((vidstart + line_end) > vga.draw.linear_mask)) {
		// wrapping in this line
		Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
		// need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
		memcpy(&TempLine[sizeof(TempLine)/2], &vga.tandy.draw_base[vidstart], break_pos);
		memcpy(&TempLine[sizeof(TempLine)/2 + break_pos],&vga.tandy.draw_base[0], line_end - break_pos);
		return &TempLine[sizeof(TempLine)/2];
	} else return &vga.tandy.draw_base[vidstart];
}

static Bit32u FontMask[2]={0xffffffff,0x0};
static Bit32u CGA_PRND = 1;

static Bit8u * VGA_CGASNOW_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);

	/* HACK: our code does not have render control during VBLANK, zero our
	 *       noise bits on the first scanline */
	if (line == 0)
		memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));

	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr,col;
		chr=vidmem[cx*2];
		col=vidmem[cx*2+1];
		if ((cx&1) == 0 && cx <= 78) {
			/* Trixter's "CGA test" program and reference video seems to suggest
			 * to me that the CGA "snow" might contain the value written by the CPU. */
			if (vga.draw.cga_snow[cx] != 0)
				chr = vga.draw.cga_snow[cx];
			if (vga.draw.cga_snow[cx+1] != 0)
				col = vga.draw.cga_snow[cx+1];

			CGA_PRND = ((CGA_PRND+1)*9421)&0xFFFF;
		}

		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		Bit32u mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		*draw++=(fg&mask1) | (bg&~mask1);
		*draw++=(fg&mask2) | (bg&~mask2);
	}
	memset(vga.draw.cga_snow,0,sizeof(vga.draw.cga_snow));
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit32u att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf];
		*draw++=att;*draw++=att;
	}
skip_cursor:
	return TempLine;
}

static Bit8u * VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);
	//assert(FontMask[0] == 0xffffffff);
	if (FontMask[1] == 0) {
		for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
			Bitu chr=vidmem[cx*2];
			Bitu col=vidmem[cx*2+1];
			Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
			Bit32u font_mask = (((Bit32s)col) << 24) >> 31;
			font_mask = ~font_mask;
			Bit32u mask1=TXT_Font_Table[font>>4] & font_mask;
			Bit32u mask2=TXT_Font_Table[font&0xf] & font_mask;
			Bit32u fg=TXT_FG_Table[col&0xf];
			Bit32u bg=TXT_BG_Table[col>>4];
			*draw++=(fg&mask1) | (bg&~mask1);
			*draw++=(fg&mask2) | (bg&~mask2);
		}
	} else {
		//assert(FontMask[1] == 0xffffffff);
		for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
			Bitu chr=vidmem[cx*2];
			Bitu col=vidmem[cx*2+1];
			Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
			Bit32u mask1=TXT_Font_Table[font>>4];
			Bit32u mask2=TXT_Font_Table[font&0xf];
			Bit32u fg=TXT_FG_Table[col&0xf];
			Bit32u bg=TXT_BG_Table[col>>4];
			*draw++=(fg&mask1) | (bg&~mask1);
			*draw++=(fg&mask2) | (bg&~mask2);
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit32u att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf];
		*draw++=att;*draw++=att;
	}
skip_cursor:
	return TempLine;
}

static Bit8u * VGA_TEXT_Herc_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);

	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu attrib=vidmem[cx*2+1];
		if (!(attrib&0x77)) {
			// 00h, 80h, 08h, 88h produce black space
			*draw++=0;
			*draw++=0;
		} else {
			Bit32u bg, fg;
			bool underline=false;
			if ((attrib&0x77)==0x70) {
				bg = TXT_BG_Table[0x7];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x0];
			} else {
				if (((Bitu)(vga.crtc.underline_location&0x1f)==line) && ((attrib&0x77)==0x1)) underline=true;
				bg = TXT_BG_Table[0x0];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x7];
			}
			Bit32u mask1, mask2;
			if (GCC_UNLIKELY(underline)) mask1 = mask2 = FontMask[attrib >> 7];
			else {
				Bitu font=vga.draw.font_tables[0][chr*32+line];
				mask1=TXT_Font_Table[font>>4] & FontMask[attrib >> 7]; // blinking
				mask2=TXT_Font_Table[font&0xf] & FontMask[attrib >> 7];
			}
			*draw++=(fg&mask1) | (bg&~mask1);
			*draw++=(fg&mask2) | (bg&~mask2);
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit8u attr = vga.tandy.draw_base[vga.draw.cursor.address+1];
		Bit32u cg;
		if (attr&0x8) {
			cg = TXT_FG_Table[0xf];
		} else if ((attr&0x77)==0x70) {
			cg = TXT_FG_Table[0x0];
		} else {
			cg = TXT_FG_Table[0x7];
		}
		*draw++=cg;*draw++=cg;
	}
skip_cursor:
	return TempLine;
}

// combined 8/9-dot wide text mode 16bpp line drawing function
static Bit8u* VGA_TEXT_Xlat32_Draw_Line(Bitu vidstart, Bitu line) {
	// keep it aligned:
	Bit32u* draw = ((Bit32u*)TempLine) + 16 - vga.draw.panning;
	const Bit32u* vidmem = VGA_Planar_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	if (vga.draw.panning) blocks++; // if the text is panned part of an 
									// additional character becomes visible
	while (blocks--) { // for each character in the line
		VGA_Latch pixels;

		pixels.d = *vidmem;
		vidmem += 1<<vga.config.addr_shift;

		Bitu chr = pixels.b[0];
		Bitu attr = pixels.b[1];
		// the font pattern
		Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
		
		Bitu background = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) background &= ~0x8;
		// choose foreground color if blinking not set for this cell or blink on
		Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
			(attr&0xf):background;
		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
			(vga.crtc.underline_location&0x1f)==line))
				background = foreground;
		if (vga.draw.char9dot) {
			font <<=1; // 9 pixels
			// extend to the 9th pixel if needed
			if ((font&0x2) && (vga.attr.mode_control&0x04) &&
				(chr>=0xc0) && (chr<=0xdf)) font |= 1;
			for (Bitu n = 0; n < 9; n++) {
				*draw++ = vga.dac.xlat32[(font&0x100)? foreground:background];
				font <<= 1;
			}
		} else {
			for (Bitu n = 0; n < 8; n++) {
				*draw++ = vga.dac.xlat32[(font&0x80)? foreground:background];
				font <<= 1;
			}
		}
	}
	// draw the text mode cursor if needed
	if ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address - vidstart) >> vga.config.addr_shift; /* <- FIXME: This right? */
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * (vga.draw.char9dot?9:8) * 4;
			draw = (Bit32u*)(&TempLine[index]) + 16 - vga.draw.panning;
			
			Bitu foreground = vga.tandy.draw_base[(vga.draw.cursor.address<<2)+1] & 0xf;
			for (Bitu i = 0; i < 8; i++) {
				*draw++ = vga.dac.xlat32[foreground];
			}
		}
	}
	return TempLine+(16*4);
}

unsigned int pc98_map_charfont(Bit16u chr,unsigned char line,unsigned char righthalf/*if fullwidth*/) {
    unsigned int index;

    if (chr & 0xFF00) {
        /* 16-bit code bit[14:8] + bit[6:0] => bit[13:0] 14-bit code */
        /* NTS: Fullwidth references that wrap back to 0x0000 do not map to the same characters as single-wide */
        index  = ((chr & 0x7F00) >> 1) + (chr & 0x7F);
        if (index < 0x80) index += 0x4000; /* the last 4KB row @512KB */
        index *= 16 * 2;
        index += line * 2;
        index += righthalf;
    }
    else {
        index  = chr;
        index *= 16;
        index += line;
    }

    return index;
}

static Bit8u* VGA_PC98_Xlat32_Draw_Line(Bitu vidstart, Bitu line) {
	// keep it aligned:
	Bit32u* draw = ((Bit32u*)TempLine);
	Bitu blocks = vga.draw.blocks;
    Bit32u vidmem = vidstart;
    Bit16u chr = 0,attr = 0;
    Bit16u lineoverlay = 0; // vertical + underline overlay over the character cell, but apparently with a 4-pixel delay
    bool doublewide = false;
    Bit16u font;

    // this rendering is layered, zero the scanline first
    memset(TempLine,0,4 * 8 * blocks);

    // Text RAM layer
    if (pc98_gdc[GDC_MASTER].display_enable) {
        draw = ((Bit32u*)TempLine);
        blocks = vga.draw.blocks;
        vidmem = vidstart;
        while (blocks--) { // for each character in the line
            if (!doublewide) {
                chr = ((Bit16u*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x0000U];
                attr = ((Bit16u*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x1000U];

                // NTS: The display handles single-wide vs double-wide by whether or not the 8 bits are nonzero.
                //      If zero, the char is one cell wide.
                //      If nonzero, the char is two cells wide (doublewide) and the current character is rendered
                //      into both cells (the character code in the next cell is ignored). The attribute (as far
                //      as I know) repeats for both.
                //
                //      NTS: It seems different character ROM is used between single and double wide chars.
                //           Contrary to what this suggests, (chr & 0xFF00) == 0x8000 is doublewide but not the
                //           same as single-wide (chr & 0xFF00) == 0x0000.
                //
                //      Specific ranges that would be fullwidth where bits[6:0] are 0x08 to 0x0B inclusive are
                //      apparently not fullwidth (the halfwidth char repeats) if both cells filled in.
                if ((chr & 0xFF00) != 0 && (chr & 0x7CU) != 0x08) {
                    // left half of doublewide char. it appears only bits[14:8] and bits[6:0] have any real effect on which char is displayed.
                    doublewide = true;
                }

                font = vga.draw.font_tables[0][pc98_map_charfont(chr,line,0)];
            }
            else {
                // right half of doublewide char.
                //
                // NTS: Strange idiosyncratic behavior observed on real hardware shows that MOST fullwidth codes
                //      fill two cells and ignore the other cell, EXCEPT, that specific ranges require you to
                //      enter the same fullwidth code in both cells.
                doublewide = false;

                // It seems that for any fullwidth char, you need the same code in both cells for bit[6:0] values
                // from 0x08 to 0x0F inclusive. 0x08 to 0x0B inclusive are not fullwidth, apparently.
                // Same applies 0x54 to 0x5F.
                if ((chr&0x78U) == 0x08 || (chr&0x7FU) >= 0x54)
                    chr = ((Bit16u*)vga.mem.linear)[(vidmem & 0xFFFU) + 0x0000U];

                font = vga.draw.font_tables[0][pc98_map_charfont(chr,line,1)];
            }

            lineoverlay <<= 8;

            /* the character is not rendered if "~secret" (bit 0) is not set */
            if (!(attr & 1)) font = 0;

            /* "blink" seems to count at the same speed as the cursor blink rate,
             * through a 4-cycle pattern in which the character is invisible only
             * at the first count. */
            if ((attr & 0x02/*blink*/) && pc98_gdc[GDC_MASTER].cursor_blink_state == 0) font = 0;

            /* reverse attribute. seems to take effect BEFORE vertical & underline attributes */
            if (attr & 0x04/*reverse*/) font ^= 0xFF;

            /* based on real hardware, the cursor seems to act like a reverse attribute */
            if (vidmem == vga.draw.cursor.address &&
                pc98_gdc[GDC_MASTER].cursor_enable &&
                ((!pc98_gdc[GDC_MASTER].cursor_blink) || (pc98_gdc[GDC_MASTER].cursor_blink_state&1)) &&
                (line >= vga.draw.cursor.sline) &&
                (line <= vga.draw.cursor.eline)) {
                font ^= 0xFF;
            }

            /* "vertical line" bit puts a vertical line on the 4th pixel of the cell */
            if (attr & 0x10) lineoverlay |= 1U << 7U;

            /* underline fills the row to underline the text */
            if ((attr & 0x08) && line == (vga.crtc.maximum_scan_line & 0x1FU)) lineoverlay |= 0xFFU;

            /* lineoverlay overlays font with 4-pixel delay */
            font |= (lineoverlay >> 4) & 0xFFU;

            Bitu foreground = (attr >> 5) & 7; /* bits[7:5] are GRB foreground color */
            Bitu background = 0; // FIXME: How do you do non-black background?
            unsigned char color;

            for (Bitu n = 0; n < 8; n++) {
                color = (font&0x80) ? foreground:background;

                // FIXME: Is this really how the master text GDC overlays the slave graphics GDC? This is a GUESS
                if (color != 0)
                    *draw++ = vga.dac.xlat32[color];
                else
                    draw++;

                font <<= 1;
            }

            vidmem++;
        }
    }

	return TempLine;
}

static void VGA_ProcessSplit() {
	vga.draw.has_split = true;
	if (vga.attr.mode_control&0x20) {
		vga.draw.address=0;
		// reset panning to 0 here so we don't have to check for 
		// it in the character draw functions. It will be set back
		// to its proper value in v-retrace
		vga.draw.panning=0; 
	} else {
		// In text mode only the characters are shifted by panning, not the address;
		// this is done in the text line draw function. EGA/VGA planar is handled the same way.
		vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
		if (machine != MCH_EGA) {
			switch (vga.mode) {
                case M_PC98:
				case M_TEXT:
				case M_EGA:
				case M_LIN4:
					/* ignore */
					break;
				default:
					vga.draw.address += vga.draw.panning;
					break;
			}
		}
	}
	vga.draw.address_line=0;
}

static Bit8u bg_color_index = 0; // screen-off black index
static Bit8u VGA_GetBlankedIndex() {
	if (vga.dac.xlat16[bg_color_index] != 0) {
		for(Bitu i = 0; i < 256; i++)
			if (vga.dac.xlat16[i] == 0) {
				bg_color_index = i;
				break;
			}
	}
	return bg_color_index;
}

/* this is now called PER LINE because most VGA cards do not double-buffer the value.
 * a few demos rely on line compare schenanigans to play with the raster, as does my own VGA test program --J.C. */
void VGA_Update_SplitLineCompare() {
	vga.draw.split_line = vga.config.line_compare+1;
	if (svgaCard==SVGA_S3Trio) {
		/* FIXME: Is this really necessary? Is this what S3 chipsets do?
		 *        What is supposed to happen is that line_compare == 0 on normal VGA will cause the first
		 *        scanline to repeat twice. Do S3 chipsets NOT reproduce that quirk?
		 *
		 *        The other theory I have about whoever wrote this code is that they wanted to multiply
		 *        the scan line by two but had to compensate for the line_compare+1 assignment above.
		 *        Rather than end up with a splitscreen too far down, they did that.
		 *
		 *        I think the proper code to put here would be:
		 *
		 *        if (vga.s3.reg_42 & 0x20) {
		 *            vga.draw.split_line--;
		 *            vga.draw.split_line *= 2;
		 *            vga.draw.split_line++;
		 *        }
		 *
		 *        Is that right?
		 *
		 *        This behavior is the reason for Issue #40 "Flash productions "monstra" extra white line at top of screen"
		 *        because it causes line compare to miss and the first scanline of the white bar appears on scanline 2,
		 *        when the demo coder obviously intended for line_compare == 0 to repeat the first scanline twice so that
		 *        on line 3, it can begin updating line compare to continue repeating the first scanline.
		 *
		 * TODO: Pull out some S3 graphics cards and check split line behavior when line_compare == 0 */
		if (vga.config.line_compare==0) vga.draw.split_line=0;
		if (vga.s3.reg_42 & 0x20) { // interlaced mode
			vga.draw.split_line *= 2;
		}
	}
	vga.draw.split_line -= vga.draw.vblank_skip;
}

static void VGA_DrawSingleLine(Bitu /*blah*/) {
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		switch(machine) {
		case MCH_PCJR:
			// Displays the border color when screen is disabled
			bg_color_index = vga.tandy.border_color;
			break;
		case MCH_TANDY:
			// Either the PCJr way or the CGA way
			if (vga.tandy.gfx_control& 0x4) { 
				bg_color_index = vga.tandy.border_color;
			} else if (vga.mode==M_TANDY4) 
				bg_color_index = vga.attr.palette[0];
			else bg_color_index = 0;
			break;
		case MCH_CGA:
			// the background color
			bg_color_index = vga.attr.overscan_color;
			break;
		case MCH_EGA:
		case MCH_VGA:
        case MCH_PC98:
			// DoWhackaDo, Alien Carnage, TV sports Football
			// when disabled by attribute index bit 5:
			//  ET3000, ET4000, Paradise display the border color
			//  S3 displays the content of the currently selected attribute register
			// when disabled by sequencer the screen is black "257th color"
			
			// the DAC table may not match the bits of the overscan register
			// so use black for this case too...
			//if (vga.attr.disabled& 2) {
			VGA_GetBlankedIndex();
			//} else 
            //    bg_color_index = vga.attr.overscan_color;
			break;
		default:
			bg_color_index = 0;
			break;
		}
		if (vga.draw.bpp==8) {
			memset(TempLine, bg_color_index, sizeof(TempLine));
		} else if (vga.draw.bpp==16) {
			Bit16u* wptr = (Bit16u*) TempLine;
			Bit16u value = vga.dac.xlat16[bg_color_index];
			for (Bitu i = 0; i < sizeof(TempLine)/2; i++) {
				wptr[i] = value;
			}
		} else if (vga.draw.bpp==32) {
			Bit32u* wptr = (Bit32u*) TempLine;
			Bit32u value = vga.dac.xlat32[bg_color_index];
			for (Bitu i = 0; i < sizeof(TempLine)/4; i++) {
				wptr[i] = value;
			}
		}

		if (vga_page_flip_occurred) {
			memxor(TempLine,0xFF,vga.draw.width*(vga.draw.bpp>>3));
			vga_page_flip_occurred = false;
		}
		if (vga_3da_polled) {
			memxor_greendotted_16bpp((uint16_t*)TempLine,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
			vga_3da_polled = false;
		}
		RENDER_DrawLine(TempLine);
	} else {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		if (vga_page_flip_occurred) {
			memxor(data,0xFF,vga.draw.width*(vga.draw.bpp>>3));
			vga_page_flip_occurred = false;
		}
		if (vga_3da_polled) {
			memxor_greendotted_16bpp((uint16_t*)TempLine,(vga.draw.width>>1)*(vga.draw.bpp>>3),vga.draw.lines_done);
			vga_3da_polled = false;
		}
		RENDER_DrawLine(data);
	}

	vga.draw.address_line++;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	vga.draw.lines_done++;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawSingleLine,(float)vga.draw.delay.singleline_delay);
	} else {
		vga_mode_frames_since_time_base++;
		RENDER_EndUpdate(false);
	}

	/* some VGA cards (ATI chipsets especially) do not double-buffer the
	 * horizontal panning register. some DOS demos take advantage of that
	 * to make the picture "waver".
	 *
	 * We stop doing this though if the attribute controller is setup to set hpel=0 at splitscreen. */
	if (IS_VGA_ARCH && vga_enable_hpel_effects) {
		/* Attribute Mode Controller: If bit 5 (Pixel Panning Mode) is set, then upon line compare the bottom portion is displayed as if Pixel Shift Count and Byte Panning are set to 0.
		 * This ensures some demos like Future Crew "Yo" display correctly instead of the bottom non-scroller half jittering because the top half is scrolling. */
		if (vga.draw.has_split && (vga.attr.mode_control&0x20))
			vga.draw.panning = 0;
		else
			vga.draw.panning = vga.config.pel_panning;
	}

	if (IS_EGAVGA_ARCH && !vga_double_buffered_line_compare) VGA_Update_SplitLineCompare();
}

static void VGA_DrawEGASingleLine(Bitu /*blah*/) {
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		memset(TempLine, 0, sizeof(TempLine));
		RENDER_DrawLine(TempLine);
	} else {
		Bitu address = vga.draw.address;
		if (machine != MCH_EGA) {
			switch (vga.mode) {
                case M_PC98:
				case M_TEXT:
				case M_EGA:
				case M_LIN4:
					/* ignore */
					break;
				default:
					vga.draw.address += vga.draw.panning;
					break;
			}
		}
		Bit8u * data=VGA_DrawLine(address, vga.draw.address_line );	
		RENDER_DrawLine(data);
	}

	vga.draw.address_line++;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	vga.draw.lines_done++;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawEGASingleLine,(float)vga.draw.delay.singleline_delay);
	} else {
		vga_mode_frames_since_time_base++;
		RENDER_EndUpdate(false);
	}
}

void VGA_SetBlinking(Bitu enabled) {
	Bitu b;
	LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",(int)enabled);
	if (enabled) {
		b=0;vga.draw.blinking=1; //used to -1 but blinking is unsigned
		vga.attr.mode_control|=0x08;
		vga.tandy.mode_control|=0x20;
	} else {
		b=8;vga.draw.blinking=0;
		vga.attr.mode_control&=~0x08;
		vga.tandy.mode_control&=~0x20;
	}
	for (Bitu i=0;i<8;i++) TXT_BG_Table[i+8]=(b+i) | ((b+i) << 8)| ((b+i) <<16) | ((b+i) << 24);
}

static void VGA_VertInterrupt(Bitu /*val*/) {
	if ((!vga.draw.vret_triggered) && ((vga.crtc.vertical_retrace_end&0x30)==0x10)) {
		vga.draw.vret_triggered=true;
		if (GCC_UNLIKELY(machine==MCH_EGA)) PIC_ActivateIRQ(9);
	}
}

static void VGA_Other_VertInterrupt(Bitu val) {
	if (val) PIC_ActivateIRQ(5);
	else PIC_DeActivateIRQ(5);
}

static void VGA_DisplayStartLatch(Bitu /*val*/) {
	/* hretrace fx support: store the hretrace value at start of picture so we have
	 * a point of reference how far to displace the scanline when wavy effects are
	 * made */
	vga_display_start_hretrace = vga.crtc.start_horizontal_retrace;
	vga.config.real_start=vga.config.display_start & (vga.vmemwrap-1);
	vga.draw.bytes_skip = vga.config.bytes_skip;
}
 
static void VGA_PanningLatch(Bitu /*val*/) {
	vga.draw.panning = vga.config.pel_panning;
}

static void VGA_VerticalTimer(Bitu /*val*/) {
	double current_time = PIC_FullIndex();

	vga.draw.delay.framestart = current_time; /* FIXME: Anyone use this?? If not, remove it */
	vga_page_flip_occurred = false;
	vga.draw.has_split = false;
	vga_3da_polled = false;

	// FIXME: While this code is quite good at keeping time, I'm seeing drift "reset" back to
	//        14-30ms every video mode change. Is our INT 10h code that slow?
	/* compensate for floating point drift, make sure we're keeping the frame rate.
	 * be very gentle about it. generally the drift is very small, and large adjustments can cause
	 * DOS games dependent on vsync to fail/hang. */
	double shouldbe = (((double)vga_mode_frames_since_time_base * 1000.0) / vga_fps) + vga_mode_time_base;
	double vsync_err = shouldbe - current_time; /* < 0 too slow     > 0 too fast */
	double vsync_adj = (vsync_err < 0 ? -1 : 1) * vsync_err * vsync_err * 0.05;
	if (vsync_adj < -0.1) vsync_adj = -0.1;
	else if (vsync_adj > 0.1) vsync_adj = 0.1;

//	LOG_MSG("Vsync err %.6fms adj=%.6fms",vsync_err,vsync_adj);

	float vsynctimerval;
	float vdisplayendtimerval;
	if( vsync.manual ) {
		static float hack_memory = 0.0f;
		if( hack_memory > 0.0f ) {
			uservsyncjolt+=hack_memory;
			hack_memory = 0.0f;
		}

		float faithful_framerate_adjustment_delay = 0.0f;
		if( vsync.faithful ) {
			static float counter = 0.0f;
			const float gfxmode_vsyncrate	= 1000.0f/vga.draw.delay.vtotal;
			const float user_vsyncrate		= 1000.0f/vsync.period;
			const float framerate_diff		= user_vsyncrate - gfxmode_vsyncrate;
			if( framerate_diff >= 0 ) {
				// User vsync rate is greater than the target vsync rate
				const float adjustment_deadline	= gfxmode_vsyncrate / framerate_diff;
				counter += 1.0f;
				if(counter >= adjustment_deadline) {
					// double vsync duration this frame to resynchronize with the target vsync timing
					faithful_framerate_adjustment_delay = vsync.period;
					counter -= adjustment_deadline;
				}
			} else {
				// User vsync rate is less than the target vsync rate

				// I don't have a good solution for this right now.
				// I also don't have access to a 60Hz display for proper testing.
				// Making an instant vsync is more difficult than making a long vsync.. because
				// the emulated app's retrace loop must be able to detect that the retrace has both
				// begun and ended.  So it's not as easy as adjusting timer durations.
				// I think adding a hack to cause port 3da's code to temporarily force the
				// vertical retrace bit to cycle could work.. Even then, it's possible that
				// some shearing could be seen when the app draws two new frames during a single
				// host refresh.
				// Anyway, this could be worth dealing with for console ports since they'll be
				// running at either 60 or 50Hz (below 70Hz).
				/*
				const float adjustment_deadline = -(gfxmode_vsyncrate / framerate_diff);
				counter += 1.0f;
				if(counter >= adjustment_deadline) {
					// nullify vsync duration this frame to resynchronize with the target vsync timing
					// TODO(AUG): proper low user vsync rate synchronization
					faithful_framerate_adjustment_delay = -uservsyncperiod + 1.2f;
					vsync_hackval = 10;
					hack_memory = -1.2f;
					counter -= adjustment_deadline;
				}
				*/
			}
		}

		const Bitu persistent_sync_update_interval = 100;
		static Bitu persistent_sync_counter = persistent_sync_update_interval;
		Bits current_tick = GetTicks();
		static Bitu jolt_tick = 0;
		if( uservsyncjolt > 0.0f ) {
			jolt_tick = current_tick;

			// set the update counter to a low value so that the user will almost
			// immediately see the effects of an auto-correction.  This gives the
			// user a chance to compensate for it.
			persistent_sync_counter = 50;
		}

		float real_diff = 0.0f;
		if( vsync.persistent ) {
			if( persistent_sync_counter == 0 ) {
				float ticks_since_jolt = current_tick - jolt_tick;
				double num_host_syncs_in_those_ticks = floor(ticks_since_jolt / vsync.period);
				float diff_thing = ticks_since_jolt - (num_host_syncs_in_those_ticks * (double)vsync.period);

				if( diff_thing > (vsync.period / 2.0f) ) real_diff = diff_thing - vsync.period;
				else real_diff = diff_thing;

//				LOG_MSG("diff is %f",real_diff);

				if( ((real_diff > 0.0f) && (real_diff < 1.5f)) || ((real_diff < 0.0f) && (real_diff > -1.5f)) )
					real_diff = 0.0f;

				persistent_sync_counter = persistent_sync_update_interval;
			} else --persistent_sync_counter;
		}

//		vsynctimerval		= uservsyncperiod + faithful_framerate_adjustment_delay + uservsyncjolt;
		vsynctimerval		= vsync.period - (real_diff/1.0f);	// formerly /2.0f
		vsynctimerval		+= faithful_framerate_adjustment_delay + uservsyncjolt;

		// be sure to provide delay between end of one refresh, and start of the next
//		vdisplayendtimerval	= vsynctimerval - 1.18f;

		// be sure to provide delay between end of one refresh, and start of the next
		vdisplayendtimerval	= vsynctimerval - (vga.draw.delay.vtotal - vga.draw.delay.vrstart);

		// in case some calculations above cause this.  this really shouldn't happen though.
		if( vdisplayendtimerval < 0.0f ) vdisplayendtimerval = 0.0f;

		uservsyncjolt = 0.0f;
	} else {
		// Standard vsync behaviour
		vsynctimerval		= (float)vga.draw.delay.vtotal;
		vdisplayendtimerval	= (float)vga.draw.delay.vrstart;
	}

	{
		double fv;

		fv = vsynctimerval + vsync_adj;
		if (fv < 1) fv = 1;
		PIC_AddEvent(VGA_VerticalTimer,fv);

		fv = vdisplayendtimerval + vsync_adj;
		if (fv < 1) fv = 1;
		PIC_AddEvent(VGA_DisplayStartLatch,fv);
	}
	
	switch(machine) {
	case MCH_PCJR:
	case MCH_TANDY:
		// PCJr: Vsync is directly connected to the IRQ controller
		// Some earlier Tandy models are said to have a vsync interrupt too
		PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrstart, 1);
		PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrend, 0);
		// fall-through
	case MCH_AMSTRAD:
	case MCH_CGA:
	case MCH_HERC:
		// MC6845-powered graphics: Loading the display start latch happens somewhere
		// after vsync off and before first visible scanline, so probably here
		VGA_DisplayStartLatch(0);
		break;
	case MCH_VGA:
    case MCH_PC98:
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
		// EGA: 82c435 datasheet: interrupt happens at display end
		// VGA: checked with scope; however disabled by default by jumper on VGA boards
		// add a little amount of time to make sure the last drawpart has already fired
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	case MCH_EGA:
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrend);
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	default:
		//E_Exit("This new machine needs implementation in VGA_VerticalTimer too.");
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	}
	// for same blinking frequency with higher frameskip
	vga.draw.cursor.count++;

    if (IS_PC98_ARCH) {
        for (unsigned int i=0;i < 2;i++)
            pc98_gdc[i].cursor_advance();
    }

	//Check if we can actually render, else skip the rest
	if (vga.draw.vga_override || !RENDER_StartUpdate()) return;

	vga.draw.address_line = vga.config.hlines_skip;
	if (IS_EGAVGA_ARCH) VGA_Update_SplitLineCompare();
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;

	switch (vga.mode) {
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1)) vga.draw.linear_mask &= ~0x10000;
		else vga.draw.linear_mask |= 0x10000;
	case M_LIN4:
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		break;
	case M_VGA:
		/* TODO: Various SVGA chipsets have a bit to enable/disable 256KB wrapping */
		vga.draw.linear_mask = 0x3ffff;
		if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
			if (vga.config.addr_shift == 1) /* NTS: Remember the ET4K steps by 4 pixels, one per byteplane, treats BYTE and DWORD modes the same */
				vga.draw.address *= 2;
		}
		else {
			vga.draw.address *= 1<<vga.config.addr_shift; /* NTS: Remember the bizarre 4 x 4 mode most SVGA chipsets do */
		}
		/* fall through */
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
		break;
    case M_PC98:
        vga.draw.linear_mask = 0xfff; // 1 page
		vga.draw.byte_panning_shift = 2;
		vga.draw.address += vga.draw.bytes_skip;
        vga.draw.cursor.address = vga.config.cursor_start;
        break;
    case M_TEXT:
		vga.draw.byte_panning_shift = 2;
		vga.draw.address += vga.draw.bytes_skip;
        // fall-through
	case M_TANDY_TEXT:
	case M_HERC_TEXT:
		if (machine==MCH_HERC) vga.draw.linear_mask = 0xfff; // 1 page
		else if (IS_EGAVGA_ARCH) vga.draw.linear_mask = 0x7fff; // 8 pages
		else vga.draw.linear_mask = 0x3fff; // CGA, Tandy 4 pages
		if (IS_EGAVGA_ARCH)
			vga.draw.cursor.address=vga.config.cursor_start<<vga.config.addr_shift;
		else
			vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.address *= 2;

		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.draw.blinking & (vga.draw.cursor.count >> 4)) ?
			0 : 0xffffffff;
		/* if blinking is enabled, 'blink' will toggle between true
		 * and false. Otherwise it's true */
		vga.draw.blink = ((vga.draw.blinking & (vga.draw.cursor.count >> 4))
			|| !vga.draw.blinking) ? true:false;
		break;
	case M_HERC_GFX:
	case M_CGA4:
	case M_CGA2:
		vga.draw.address=(vga.draw.address*2)&0x1fff;
		break;
	case M_AMSTRAD: // Base address: No difference?
		vga.draw.address=(vga.draw.address*2)&0xffff;
		break;
	case M_CGA16:
	case M_TANDY2:case M_TANDY4:case M_TANDY16:
		vga.draw.address *= 2;
		break;
	default:
		break;
	}

	/* ET4000 High Sierra DAC programs can change SVGA mode */
	if ((vga.mode == M_LIN15 || vga.mode == M_LIN16) && (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)) {
		if (et4k_highcolor_half_pixel_rate())
			VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
		else
			VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
	}

	// check if some lines at the top off the screen are blanked
	float draw_skip = 0.0;
	if (GCC_UNLIKELY(vga.draw.vblank_skip > 0)) {
		draw_skip = (float)(vga.draw.delay.htotal * vga.draw.vblank_skip);
		vga.draw.address_line += (vga.draw.vblank_skip % vga.draw.address_line_total);
		vga.draw.address += vga.draw.address_add * (vga.draw.vblank_skip / vga.draw.address_line_total);
	}

	/* do VGA split now if line compare <= 0. NTS: vga.draw.split_line is defined as Bitu (unsigned integer) so we need the typecast. */
	if (GCC_UNLIKELY((Bits)vga.draw.split_line <= 0)) {
		VGA_ProcessSplit();

		/* if vblank_skip != 0, line compare can become a negative value! Fixes "Warlock" 1992 demo by Warlock */
		if (GCC_UNLIKELY((Bits)vga.draw.split_line < 0)) {
			vga.draw.address_line += (-((Bits)vga.draw.split_line) % vga.draw.address_line_total);
			vga.draw.address += vga.draw.address_add * (-((Bits)vga.draw.split_line) / vga.draw.address_line_total);
		}
	}

	// add the draw event
	switch (vga.draw.mode) {
	case LINE:
	case EGALINE:
		if (GCC_UNLIKELY(vga.draw.lines_done < vga.draw.lines_total)) {
			LOG(LOG_VGAMISC,LOG_NORMAL)( "Lines left: %d", 
				(int)(vga.draw.lines_total-vga.draw.lines_done));
			if (vga.draw.mode==EGALINE) PIC_RemoveEvents(VGA_DrawEGASingleLine);
			else PIC_RemoveEvents(VGA_DrawSingleLine);
			vga_mode_frames_since_time_base++;
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		if (vga.draw.mode==EGALINE)
			PIC_AddEvent(VGA_DrawEGASingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
		else PIC_AddEvent(VGA_DrawSingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
		break;
	}
}

void VGA_CheckScanLength(void) {
	switch (vga.mode) {
	case M_EGA:
	case M_LIN4:
		if ((machine==MCH_EGA)&&(vga.crtc.mode_control&0x8))
			vga.draw.address_add=vga.config.scan_len*16;
		else
			vga.draw.address_add=vga.config.scan_len*8;
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32:
		if (vga.mode == M_VGA) {
			if (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) {
				/* Observed ET4000AX behavior:
				 *    byte mode OR dword mode scans 256-color 4-pixel groups byte by byte from
				 *    planar RAM. word mode scans every other byte (skips by two).
				 *    We can just scan the buffer normally as linear because of the address
				 *    translation carried out by the ET4000 in chained mode:
				 *
				 *    plane = (addr & 3)   addr = (addr >> 2)
				 *
				 *    TODO: Validate that this is correct. */
				vga.draw.address_add=vga.config.scan_len*((vga.config.addr_shift == 1)?16:8);
			}
			else {
				/* Most (almost ALL) VGA clones render chained modes as 4 8-bit planes one DWORD apart.
				 * They all act as if writes to chained VGA memory are translated as:
				 * addr = ((addr & ~3) << 2) + (addr & 3) */
				vga.draw.address_add=vga.config.scan_len*((2*4)<<vga.config.addr_shift);
			}
		}
		else {
			/* the rest (SVGA modes) can be rendered with sanity */
			vga.draw.address_add=vga.config.scan_len*(2<<vga.config.addr_shift);
		}
		break;
    case M_PC98:
        vga.draw.address_add=vga.draw.blocks;
        break;
    case M_TEXT:
	case M_CGA2:
	case M_CGA4:
	case M_CGA16:
	case M_AMSTRAD:	// Next line.
		if (IS_EGAVGA_ARCH || IS_PC98_ARCH)
			vga.draw.address_add=vga.config.scan_len*(2<<vga.config.addr_shift);
		else
			vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY2:
		vga.draw.address_add=vga.draw.blocks/4;
		break;
	case M_TANDY4:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY16:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_GFX:
		vga.draw.address_add=vga.draw.blocks;
		break;
	default:
		vga.draw.address_add=vga.draw.blocks*8;
		break;
	}
}

void VGA_ActivateHardwareCursor(void) {
	bool hwcursor_active=false;
	if (svga.hardware_cursor_active) {
		if (svga.hardware_cursor_active()) hwcursor_active=true;
	}
	if (hwcursor_active && vga.mode != M_LIN24) {
		switch(vga.mode) {
		case M_LIN32:
			VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
			break;
		case M_LIN15:
		case M_LIN16:
			if ((svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) && et4k_highcolor_half_pixel_rate())
				VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
			else
				VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
			break;
		case M_LIN8:
			VGA_DrawLine=VGA_Draw_VGA_Line_Xlat32_HWMouse;
			break;
		default:
			VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
			break;
		}
	} else {
		switch(vga.mode) {
		case M_LIN8:
			VGA_DrawLine=VGA_Draw_Xlat32_Linear_Line;
			break;
		case M_LIN24:
			VGA_DrawLine=VGA_Draw_Linear_Line_24_to_32;
			break;
		default:
			VGA_DrawLine=VGA_Draw_Linear_Line;
			break;
		}
	}
}

void VGA_SetupDrawing(Bitu /*val*/) {
	if (vga.mode==M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
	}
	// user choosable special trick support
	// multiscan -- zooming effects - only makes sense if linewise is enabled
	// linewise -- scan display line by line instead of 4 blocks
	// keep compatibility with other builds of DOSBox for vgaonly.
	vga.draw.doublescan_effect = vga.draw.doublescan_set;

	// set the drawing mode
	switch (machine) {
	case MCH_CGA:
	case MCH_PCJR:
	case MCH_TANDY:
		vga.draw.mode = LINE;
		break;
	case MCH_EGA:
		// Note: The Paradise SVGA uses the same panning mechanism as EGA
		vga.draw.mode = EGALINE;
		break;
	case MCH_VGA:
    case MCH_PC98:
		if (svgaCard==SVGA_None) {
			vga.draw.mode = LINE;
			break;
		}
		// fall-through
	default:
		vga.draw.mode = LINE;
		break;
	}
	
	/* Calculate the FPS for this screen */
	Bitu oscclock = 0, clock;
	Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
	Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
	Bitu hbend_mask, vbend_mask;
	Bitu vblank_skip;

    /* NTS: PC-98 emulation re-uses VGA state FOR NOW.
     *      This will slowly change to accomodate PC-98 display controller over time
     *      and IS_PC98_ARCH will become it's own case statement. */

	if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
		htotal = vga.crtc.horizontal_total;
		hdend = vga.crtc.horizontal_display_end;
		hbend = vga.crtc.end_horizontal_blanking&0x1F;
		hbstart = vga.crtc.start_horizontal_blanking;
		hrstart = vga.crtc.start_horizontal_retrace;

		vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
		vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		
		if (IS_VGA_ARCH || IS_PC98_ARCH) {
			// additional bits only present on vga cards
			htotal |= (vga.s3.ex_hor_overflow & 0x1) << 8;
			htotal += 3;
			hdend |= (vga.s3.ex_hor_overflow & 0x2) << 7;
			hbend |= (vga.crtc.end_horizontal_retrace&0x80) >> 2;
			hbstart |= (vga.s3.ex_hor_overflow & 0x4) << 6;
			hrstart |= (vga.s3.ex_hor_overflow & 0x10) << 4;
			hbend_mask = 0x3f;
			
			vtotal |= (vga.crtc.overflow & 0x20) << 4;
			vtotal |= (vga.s3.ex_ver_overflow & 0x1) << 10;
			vtotal += 2;
			vdend |= (vga.crtc.overflow & 0x40) << 3; 
			vdend |= (vga.s3.ex_ver_overflow & 0x2) << 9;
			vbstart |= (vga.crtc.maximum_scan_line & 0x20) << 4;
			vbstart |= (vga.s3.ex_ver_overflow & 0x4) << 8;
			vrstart |= ((vga.crtc.overflow & 0x80) << 2);
			vrstart |= (vga.s3.ex_ver_overflow & 0x10) << 6;
			vbend_mask = 0xff;
		} else { // EGA
			hbend_mask = 0x1f;
			vbend_mask = 0x1f;
		}
		htotal += 2;
		hdend += 1;
		vdend += 1;

		// horitzontal blanking
		if (hbend <= (hbstart & hbend_mask)) hbend += hbend_mask + 1;
		hbend += hbstart - (hbstart & hbend_mask);
		
		// horizontal retrace
		hrend = vga.crtc.end_horizontal_retrace & 0x1f;
		if (hrend <= (hrstart&0x1f)) hrend += 32;
		hrend += hrstart - (hrstart&0x1f);
		if (hrend > hbend) hrend = hbend; // S3 BIOS (???)
		
		// vertical retrace
		vrend = vga.crtc.vertical_retrace_end & 0xf;
		if (vrend <= (vrstart&0xf)) vrend += 16;
		vrend += vrstart - (vrstart&0xf);

		// vertical blanking
		vbend = vga.crtc.end_vertical_blanking & vbend_mask;
		if (vbstart != 0) {
		// Special case vbstart==0:
		// Most graphics cards agree that lines zero to vbend are
		// blanked. ET4000 doesn't blank at all if vbstart==vbend.
		// ET3000 blanks lines 1 to vbend (255/6 lines).
			vbstart += 1;
			if (vbend <= (vbstart & vbend_mask)) vbend += vbend_mask + 1;
			vbend += vbstart - (vbstart & vbend_mask);
		}
		vbend++;

        // TODO: Found a monitor document that lists two different scan rates for PC-98:
        //
        //       640x400  25.175MHz dot clock  70.15Hz refresh  31.5KHz horizontal refresh (basically, VGA)
        //       640x400  21.05MHz dot clock   56.42Hz refresh  24.83Hz horizontal refresh (original spec?)

        if (svga.get_clock) {
			oscclock = svga.get_clock();
        } else if (vga.mode == M_PC98) {
            if (false/*future 15KHz hsync*/) {
                oscclock = 14318180;
            }
            else if (true/*24KHz hsync*/) {
                oscclock = 21052600;
            }
            else {/*31KHz VGA-like hsync*/
                oscclock = 25175000;
            }
		} else {
			switch ((vga.misc_output >> 2) & 3) {
			case 0:	
				oscclock = (machine==MCH_EGA) ? (PIT_TICK_RATE*12) : 25175000;
				break;
			case 1:
			default:
				oscclock = (machine==MCH_EGA) ? 16257000 : 28322000;
				break;
			}
		}

		/* Check for 8 or 9 character clock mode */
		if (vga.seq.clocking_mode & 1 ) clock = oscclock/8; else clock = oscclock/9;
		if (vga.mode==M_LIN15 || vga.mode==M_LIN16) clock *= 2;
		/* Check for pixel doubling, master clock/2 */
		if (vga.seq.clocking_mode & 0x8) clock /=2;

		if (svgaCard==SVGA_S3Trio) {
			// support for interlacing used by the S3 BIOS and possibly other drivers
			if (vga.s3.reg_42 & 0x20) {
				vtotal *= 2;	vdend *= 2;
				vbstart *= 2;	vbend *= 2;
				vrstart *= 2;	vrend *= 2;
				//clock /= 2;
		}
		}

		/* FIXME: This is based on correcting the hicolor mode for MFX/Transgression 2.
		 *        I am not able to test against the Windows drivers at this time. */
		if ((svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K) && et4k_highcolor_half_pixel_rate())
			clock /= 2;
	} else {
		// not EGAVGA_ARCH
		vga.draw.split_line = 0x10000;	// don't care

		htotal = vga.other.htotal + 1;
		hdend = vga.other.hdend;
		hbstart = hdend;
		hbend = htotal;
		hrstart = vga.other.hsyncp;
		hrend = hrstart + (vga.other.hsyncw) ;

		vga.draw.address_line_total = vga.other.max_scanline + 1;
		vtotal = vga.draw.address_line_total * (vga.other.vtotal+1)+vga.other.vadjust;
		vdend = vga.draw.address_line_total * vga.other.vdend;
		vrstart = vga.draw.address_line_total * vga.other.vsyncp;
		vrend = vrstart + 16; // vsync width is fixed to 16 lines on the MC6845 TODO Tandy
		vbstart = vdend;
		vbend = vtotal;

		switch (machine) {
		case MCH_AMSTRAD:
			clock=(16000000/2)/8;
			break;
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			clock = (PIT_TICK_RATE*12)/8;
			if (!(vga.tandy.mode_control & 1)) clock /= 2;
			break;
		case MCH_HERC:
			clock=16000000/8;
			if (vga.herc.mode_control & 0x2) clock/=2;

			break;
		default:
			clock = (PIT_TICK_RATE*12);
			break;
		}
		vga.draw.delay.hdend = hdend*1000.0/clock; //in milliseconds
	}
#if C_DEBUG
	LOG(LOG_VGA,LOG_NORMAL)("h total %3d end %3d blank (%3d/%3d) retrace (%3d/%3d)",
		(int)htotal, (int)hdend, (int)hbstart, (int)hbend, (int)hrstart, (int)hrend );
	LOG(LOG_VGA,LOG_NORMAL)("v total %3d end %3d blank (%3d/%3d) retrace (%3d/%3d)",
		(int)vtotal, (int)vdend, (int)vbstart, (int)vbend, (int)vrstart, (int)vrend );
#endif
	if (!htotal) return;
	if (!vtotal) return;
	
	// The screen refresh frequency
	double fps;
	extern double vga_force_refresh_rate;
	if (vga_force_refresh_rate > 0) {
		/* force the VGA refresh rate by setting fps and bending the clock to our will */
		LOG(LOG_VGA,LOG_NORMAL)("VGA forced refresh rate in effect, %.3f",vga_force_refresh_rate);
		fps=vga_force_refresh_rate;
		clock=((double)(vtotal*htotal))*fps;
	}
	else {
		// The screen refresh frequency
		fps=(double)clock/(vtotal*htotal);
		LOG(LOG_VGA,LOG_NORMAL)("VGA refresh rate is now, %.3f",fps);
	}

	/* clip display end to stay within vtotal ("Monolith" demo part 4 320x570 mode fix) */
	if (vdend > vtotal) {
		LOG(LOG_VGA,LOG_WARN)("VGA display end greater than vtotal!");
		vdend = vtotal;
	}

	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = htotal*1000.0/clock; //in milliseconds
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = hbstart*1000.0/clock; //in milliseconds
	vga.draw.delay.hblkend = hbend*1000.0/clock; 
	// Start and End of horizontal retrace
	vga.draw.delay.hrstart = hrstart*1000.0/clock;
	vga.draw.delay.hrend = hrend*1000.0/clock;
	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = vbstart * vga.draw.delay.htotal;
	vga.draw.delay.vblkend = vbend * vga.draw.delay.htotal;
	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = vrstart * vga.draw.delay.htotal;
	vga.draw.delay.vrend = vrend * vga.draw.delay.htotal;

	// Vertical blanking tricks
	vblank_skip = 0;
	if ((IS_VGA_ARCH || IS_PC98_ARCH) && !ignore_vblank_wraparound) { // others need more investigation
		if (vbstart < vtotal) { // There will be no blanking at all otherwise
			if (vbend > vtotal) {
				// blanking wraps to the start of the screen
				vblank_skip = vbend&0x7f;
				
				// on blanking wrap to 0, the first line is not blanked
				// this is used by the S3 BIOS and other S3 drivers in some SVGA modes
				if ((vbend&0x7f)==1) vblank_skip = 0;
				
				// it might also cut some lines off the bottom
				if (vbstart < vdend) {
					vdend = vbstart;
				}
				LOG(LOG_VGA,LOG_WARN)("Blanking wrap to line %d", (int)vblank_skip);
			} else if (vbstart<=1) {
				// blanking is used to cut lines at the start of the screen
				vblank_skip = vbend;
				LOG(LOG_VGA,LOG_WARN)("Upper %d lines of the screen blanked", (int)vblank_skip);
			} else if (vbstart < vdend) {
				if (vbend < vdend) {
					// the game wants a black bar somewhere on the screen
					LOG(LOG_VGA,LOG_WARN)("Unsupported blanking: line %d-%d",(int)vbstart,(int)vbend);
				} else {
					// blanking is used to cut off some lines from the bottom
					vdend = vbstart;
				}
			}
			vdend -= vblank_skip;
		}
	}
	vga.draw.vblank_skip = vblank_skip;

	// Display end
	vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

	// EGA frequency dependent monitor palette
	if (machine == MCH_EGA) {
		if (vga.misc_output & 1) {
			// EGA card is in color mode
			if ((1.0f/vga.draw.delay.htotal) > 19.0f) {
				// 64 color EGA mode
				VGA_ATTR_SetEGAMonitorPalette(EGA);
			} else {
				// 16 color CGA mode compatibility
				VGA_ATTR_SetEGAMonitorPalette(CGA);
			}
		} else {
			// EGA card in monochrome mode
			// It is not meant to be autodetected that way, you either
			// have a monochrome or color monitor connected and
			// the EGA switches configured appropriately.
			// But this would only be a problem if a program sets 
			// the adapter to monochrome mode and still expects color output.
			// Such a program should be shot to the moon...
			VGA_ATTR_SetEGAMonitorPalette(MONO);
		}
	}

	/*
      6  Horizontal Sync Polarity. Negative if set
      7  Vertical Sync Polarity. Negative if set
         Bit 6-7 indicates the number of lines on the display:
            1:  400, 2: 350, 3: 480
	*/
	//Try to determine the pixel size, aspect correct is based around square pixels

	//Base pixel width around 100 clocks horizontal
	//For 9 pixel text modes this should be changed, but we don't support that anyway :)
	//Seems regular vga only listens to the 9 char pixel mode with character mode enabled
	//Base pixel height around vertical totals of modes that have 100 clocks horizontal
	//Different sync values gives different scaling of the whole vertical range
	//VGA monitor just seems to thighten or widen the whole vertical range

	vga.draw.resizing=false;
	vga.draw.has_split=false;
	vga.draw.vret_triggered=false;

	//Check to prevent useless black areas
	if (hbstart<hdend) hdend=hbstart;
	if ((!(IS_VGA_ARCH || IS_PC98_ARCH)) && (vbstart<vdend)) vdend=vbstart;

	Bitu width=hdend;
	Bitu height=vdend;

	if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
		vga.draw.address_line_total=(vga.crtc.maximum_scan_line&0x1f)+1;
		switch(vga.mode) {
		case M_CGA16:
		case M_CGA2:
		case M_CGA4:
        case M_PC98:
        case M_TEXT:
			// these use line_total internal
			// doublescanning needs to be emulated by renderer doubleheight
			// EGA has no doublescanning bit at 0x80
			if (vga.crtc.maximum_scan_line&0x80) {
				// vga_draw only needs to draw every second line
				height/=2;
			}
			break;
		default:
			if (vga.draw.doublescan_effect) {
				// don't merge doublescanned lines
				if (vga.crtc.maximum_scan_line&0x80) {
					// double scan method 1
					vga.draw.address_line_total*=2;
				}
			} else {
				if (vga.crtc.maximum_scan_line & 0x80) {
					// double scan method 1
					height/=2;
				} else if (vga.draw.address_line_total == 2) { // 4,8,16?
					// double scan method 2
					height/=2;
					vga.draw.address_line_total=1; // don't repeat in this case
				}
			}
			break;
		}
	}

	//Set the bpp
	Bitu bpp;
	switch (vga.mode) {
	case M_LIN15:
		bpp = 15;
		break;
	case M_LIN16:
		bpp = 16;
		break;
	case M_LIN24:
	case M_LIN32:
		bpp = 32;
		break;
	default:
		bpp = 8;
		break;
	}
	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap - 1;
    vga.draw.planar_mask = vga.draw.linear_mask >> 2;
	Bitu pix_per_char = 8;
	switch (vga.mode) {
	case M_VGA:
		// hack for tgr2 -hc high color mode demo
		if (vga.dac.reg02==0x80) {
			bpp=16;
			vga.mode=M_LIN16;
			VGA_SetupHandlers();
			VGA_DrawLine=VGA_Draw_LIN16_Line_2x;
			pix_per_char = 4;
			break;
		}
		bpp = 32;
		pix_per_char = 4;
		if (vga.mode == M_VGA && (svgaCard == SVGA_TsengET3K || svgaCard == SVGA_TsengET4K)) {
			/* ET4000 chipsets handle the chained mode (in my opinion) with sanity and we can scan linearly for it.
			 * Chained VGA mode maps planar byte addr = (addr >> 2) and plane = (addr & 3) */
			VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;
		}
		else {
			/* other SVGA chipsets appear to handle chained mode by writing 4 pixels to 4 planes, and showing
			 * only every 4th byte, which is why when you switch the CRTC to byte or word mode on these chipsets,
			 * you see 16-pixel groups with 4 pixels from the chained display you expect followed by 12 pixels
			 * of whatever contents of memory remain. but when you unchain the bitplanes the card will allow
			 * "planar" writing to all 16 pixels properly. Chained VGA maps like planar byte = (addr & ~3) and
			 * plane = (addr & 3) */
			VGA_DrawLine = VGA_Draw_Xlat32_VGA_CRTC_bmode_Line;
		}
		break;
	case M_LIN8:
		bpp = 32;
		VGA_DrawLine = VGA_Draw_Xlat32_Linear_Line;

		if ((vga.s3.reg_3a & 0x10)||(svgaCard!=SVGA_S3Trio))
			pix_per_char = 8; // TODO fiddle out the bits for other svga cards
		else
			pix_per_char = 4;

		VGA_ActivateHardwareCursor();
		break;
	case M_LIN24:
	case M_LIN32:
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN15:
 	case M_LIN16:
		pix_per_char = 4; // 15/16 bpp modes double the horizontal values
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN4:
	case M_EGA:
		bpp = 32;
		vga.draw.blocks = width;
		VGA_DrawLine = VGA_Draw_VGA_Planar_Xlat32_Line;
		break;
	case M_CGA16:
		vga.draw.blocks=width*2;
		pix_per_char = 16;
		VGA_DrawLine=VGA_Draw_CGA16_Line;
		break;
	case M_CGA4:
		if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
			vga.draw.blocks=width;
			VGA_DrawLine=VGA_Draw_2BPP_Line_as_VGA;
			bpp = 32;
		}
		else {
			vga.draw.blocks=width*2;
			VGA_DrawLine=VGA_Draw_2BPP_Line;
		}
		break;
	case M_CGA2:
		if (IS_EGAVGA_ARCH || IS_PC98_ARCH) {
			vga.draw.blocks=width;
			VGA_DrawLine=VGA_Draw_1BPP_Line_as_VGA;
			bpp = 32;
		}
		else {
			vga.draw.blocks=width*2;
			VGA_DrawLine=VGA_Draw_1BPP_Line;
		}
		break;
    case M_PC98:
		vga.draw.blocks=width;
        vga.draw.char9dot = false;
        VGA_DrawLine=VGA_PC98_Xlat32_Draw_Line;
        bpp = 32;
        break;
    case M_TEXT:
		vga.draw.blocks=width;
		// if char9_set is true, allow 9-pixel wide fonts
		if ((vga.seq.clocking_mode&0x01) || !vga.draw.char9_set) {
			// 8-pixel wide
			vga.draw.char9dot = false;
			VGA_DrawLine=VGA_TEXT_Xlat32_Draw_Line;
			bpp = 32;
		} else {
			// 9-pixel wide
			pix_per_char = 9;
			vga.draw.char9dot = true;
			VGA_DrawLine=VGA_TEXT_Xlat32_Draw_Line;
			bpp = 32;
		}
		break;
	case M_HERC_GFX:
		vga.draw.blocks=width*2;
		pix_per_char = 16;
		if (vga.herc.blend) VGA_DrawLine=VGA_Draw_1BPP_Blend_Line;
		else VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY2:
		if (((machine==MCH_PCJR)&&(vga.tandy.gfx_control & 0x8)) ||
			(vga.tandy.mode_control & 0x10)) {
			vga.draw.blocks=width * 8;
			pix_per_char = 16;
		} else {
			vga.draw.blocks=width * 4;
			pix_per_char = 8;
		}
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY4:
		vga.draw.blocks=width * 2;
		pix_per_char = 8;
		if ((machine==MCH_TANDY && (vga.tandy.gfx_control & 0x8)) ||
			(machine==MCH_PCJR && (vga.tandy.mode_control==0x0b)))
			VGA_DrawLine=VGA_Draw_2BPPHiRes_Line;
		else VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		if (vga.tandy.mode_control & 0x1) {
			if (( machine==MCH_TANDY ) && ( vga.tandy.mode_control & 0x10 )) {
				vga.draw.blocks=width*4;
				pix_per_char = 8;
			} else {
				vga.draw.blocks=width*2;
				pix_per_char = 4;
			}
			VGA_DrawLine=VGA_Draw_4BPP_Line;
		} else {
			vga.draw.blocks=width*2;
			pix_per_char = 8;
			VGA_DrawLine=VGA_Draw_4BPP_Line_Double;
		}
		break;
	case M_TANDY_TEXT: /* Also CGA */
		vga.draw.blocks=width;
		if (machine==MCH_CGA /*&& !doublewidth*/ && enableCGASnow && (vga.tandy.mode_control & 1)/*80-column mode*/)
			VGA_DrawLine=VGA_CGASNOW_TEXT_Draw_Line; /* Alternate version that emulates CGA snow */
		else
			VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	case M_HERC_TEXT:
		vga.draw.blocks=width;
		VGA_DrawLine=VGA_TEXT_Herc_Draw_Line;
		break;
	case M_AMSTRAD: // Probably OK?
		pix_per_char = 16;
		vga.draw.blocks=width*2;
		VGA_DrawLine=VGA_Draw_AMS_4BPP_Line;
//		VGA_DrawLine=VGA_Draw_4BPP_Line;
/*		doubleheight=true;
		vga.draw.blocks = 2*width; width<<=4;
		vga.draw.linear_base = vga.mem.linear + VGA_CACHE_OFFSET;
		vga.draw.linear_mask = 512 * 1024 - 1; */
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution",vga.mode);
		break;
	}
	width *= pix_per_char;
	VGA_CheckScanLength();

	vga.draw.lines_total=height;
	vga.draw.line_length = width * ((bpp + 1) / 8);
	vga.draw.clock = clock;

	double vratio = ((double)width)/(double)height; // ratio if pixels were square

	// the picture ratio factor
	double scanratio =	((double)hdend/(double)(htotal-(hrend-hrstart)))/
						((double)vdend/(double)(vtotal-(vrend-vrstart)));
	double scanfield_ratio = 4.0/3.0;
	switch(machine) {
		case MCH_CGA:
		case MCH_PCJR:
		case MCH_TANDY:
			scanfield_ratio = 1.382;
			break;
		case MCH_HERC:
			scanfield_ratio = 1.535;
			break;
		case MCH_EGA:
			switch (vga.misc_output >> 6) {
			case 0: // 200 lines:
				scanfield_ratio = 1.085; // DOSBugs
				//scanfield_ratio = 1.375; // IBM EGA BIOS
				break;
			case 2: // 350 lines
				// TODO monitors seem to display this with a bit of black borders on top and bottom
				scanfield_ratio = 1.45;
				break;
			default:
				// other cases are undefined for EGA - scale them to 4:3
				scanfield_ratio = (4.0/3.0) / scanratio;
				break;
			}
			break;

		default: // VGA
			switch (vga.misc_output >> 6) {
			case 0: // VESA: "OTHER" scanline amount
				scanfield_ratio = (4.0/3.0) / scanratio;
				break;
			case 1: // 400 lines
				scanfield_ratio = 1.312;
				break;
			case 2: // 350 lines
				scanfield_ratio = 1.249;
				break;
			case 3: // 480 lines
				scanfield_ratio = 1.345;
				break;
			}
			break;
	}
	// calculate screen ratio
	double screenratio = scanratio * scanfield_ratio;

	// override screenratio for certain cases:
	if (vratio == 1.6) screenratio = 4.0 / 3.0;
	else if (vratio == 0.8) screenratio = 4.0 / 3.0;
	else if (vratio == 3.2) screenratio = 4.0 / 3.0;
	else if (vratio == (4.0/3.0)) screenratio = 4.0 / 3.0;
	else if (vratio == (2.0/3.0)) screenratio = 4.0 / 3.0;
	else if ((width >= 800)&&(height>=600)) screenratio = 4.0 / 3.0;

#if C_DEBUG
			LOG(LOG_VGA,LOG_NORMAL)("screen: %1.3f, scanfield: %1.3f, scan: %1.3f, vratio: %1.3f",
				screenratio, scanfield_ratio, scanratio, vratio);
#endif

	bool fps_changed = false;

#if C_DEBUG
	LOG(LOG_VGA,LOG_NORMAL)("h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
		vga.draw.delay.htotal,(1.0/vga.draw.delay.htotal),
		vga.draw.delay.hblkstart,vga.draw.delay.hblkend,
		vga.draw.delay.hrstart,vga.draw.delay.hrend);
	LOG(LOG_VGA,LOG_NORMAL)("v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
        vga.draw.delay.vtotal,(1000.0/vga.draw.delay.vtotal),
		vga.draw.delay.vblkstart,vga.draw.delay.vblkend,
		vga.draw.delay.vrstart,vga.draw.delay.vrend);

	const char* const mode_texts[] = {
		"M_CGA2", "M_CGA4",
		"M_EGA", "M_VGA",
		"M_LIN4", "M_LIN8", "M_LIN15", "M_LIN16", "M_LIN24", "M_LIN32",
		"M_TEXT",
		"M_HERC_GFX", "M_HERC_TEXT",
		"M_CGA16", "M_TANDY2", "M_TANDY4", "M_TANDY16", "M_TANDY_TEXT",
        "M_AMSTRAD", "M_PC98",
		"M_ERROR"
	};

	LOG(LOG_VGA,LOG_NORMAL)("video clock: %3.2fMHz mode %s",
		oscclock/1000000.0, mode_texts[vga.mode]);
#endif

	// need to change the vertical timing?
	if (vga_mode_time_base < 0 || fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001)
		fps_changed = true;

	// need to resize the output window?
	if ((width != vga.draw.width) ||
		(height != vga.draw.height) ||
		(fabs(screenratio - vga.draw.screen_ratio) > 0.0001) ||
		(vga.draw.bpp != bpp) || fps_changed) {

		VGA_KillDrawing();

		vga.draw.width = width;
		vga.draw.height = height;
		vga.draw.screen_ratio = screenratio;
		vga.draw.bpp = bpp;
#if C_DEBUG
		LOG(LOG_VGA,LOG_NORMAL)("%dx%d, %3.2fHz, %dbpp, screen %1.3f",(int)width,(int)height,fps,(int)bpp,screenratio);
#endif
		if (!vga.draw.vga_override)
			RENDER_SetSize(width,height,bpp,(float)fps,screenratio);

		if (fps_changed) {
			vga_mode_time_base = PIC_FullIndex();
			vga_mode_frames_since_time_base = 0;
			PIC_RemoveEvents(VGA_Other_VertInterrupt);
			PIC_RemoveEvents(VGA_VerticalTimer);
			PIC_RemoveEvents(VGA_PanningLatch);
			PIC_RemoveEvents(VGA_DisplayStartLatch);
			vga.draw.delay.vtotal = 1000.0 / fps;
			vga.draw.lines_done = vga.draw.lines_total;
			vga_fps = fps;
			VGA_VerticalTimer(0);
		}

		VGA_DAC_UpdateColorPalette();
	}
	vga.draw.delay.singleline_delay = (float)vga.draw.delay.htotal;
}

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawSingleLine);
	PIC_RemoveEvents(VGA_DrawEGASingleLine);
}

void VGA_SetOverride(bool vga_override) {
	if (vga.draw.vga_override!=vga_override) {
		
		if (vga_override) {
			VGA_KillDrawing();
			vga.draw.vga_override=true;
		} else {
			vga.draw.vga_override=false;
			vga.draw.width=0; // change it so the output window gets updated
			VGA_SetupDrawing(0);
		}
	}
}

