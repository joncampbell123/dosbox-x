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

/* $Id: vga_draw.cpp,v 1.112 2009-11-03 21:06:59 h-a-l-9000 Exp $ */

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

#define VGA_PARTS 4

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[SCALER_MAXWIDTH * 4];
extern double vga_force_refresh_rate;
 
static Bit8u * VGA_Draw_AMS_4BPP_Line(Bitu vidstart, Bitu line) {/*AMSTRAD rendering*/
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
	const Bit8u *reader = base + vidstart;
	Bit32u * draw=(Bit32u *)TempLine;
	//Generate a temporary bitline to calculate the avarage
	//over bit-2  bit-1  bit  bit+1.
	//Combine this number with the current colour to get 
	//an unigue index in the pallete. Or it with bit 7 as they are stored 
	//in the upperpart to keep them from interfering the regular cga stuff

	for(Bitu x = 0; x < 640; x++)
		temp[x+2] = (( reader[(x>>3)] >> (7-(x&7)) )&1) << 4;
		//shift 4 as that is for the index.
	Bitu i = 0,temp1,temp2,temp3,temp4;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = *reader++;
		Bitu val2 = val1&0xf;
		val1 >>= 4;

		temp1 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp2 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp3 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp4 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;

		*draw++ = 0x80808080|(temp1|val1) |
		          ((temp2|val1) << 8) |
		          ((temp3|val1) <<16) |
		          ((temp4|val1) <<24);
		temp1 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp2 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp3 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp4 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		*draw++ = 0x80808080|(temp1|val2) |
		          ((temp2|val2) << 8) |
		          ((temp3|val2) <<16) |
		          ((temp4|val2) <<24);
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=(val1 & 0x0f) << 8  |
				(val1 & 0xf0) >> 4  |
				(val2 & 0x0f) << 24 |
				(val2 & 0xf0) << 12;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=(val & 0xf0) >> 4  |
				(val & 0xf0) << 4  |
				(val & 0x0f) << 16 |
				(val & 0x0f) << 24;
	}
	return TempLine;
}

#ifdef VGA_KEEP_CHANGES
static Bit8u * VGA_Draw_Changes_Line(Bitu vidstart, Bitu line) {
	Bitu checkMask = vga.changes.checkMask;
	Bit8u *map = vga.changes.map;
	Bitu start = (vidstart >> VGA_CHANGE_SHIFT);
	Bitu end = ((vidstart + vga.draw.line_length ) >> VGA_CHANGE_SHIFT);
	for (; start <= end;start++) {
		if ( map[start] & checkMask ) {
			Bitu offset = vidstart & vga.draw.linear_mask;
			if(vga.draw.linear_mask-offset < vga.draw.line_length)
				memcpy(vga.draw.linear_base+vga.draw.linear_mask+1, vga.draw.linear_base, vga.draw.line_length);
			Bit8u *ret = &vga.draw.linear_base[ offset ];
#if !defined(C_UNALIGNED_MEMORY)
			if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
				memcpy( TempLine, ret, vga.draw.line_length );
				return TempLine;
			}
#endif
			return ret;
		}
	}
//	memset( TempLine, 0x30, vga.changes.lineWidth );
//	return TempLine;
	return 0;
}

#endif

static Bit8u * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
// There is guaranteed extra memory past the wrap boundary. So, instead of using temporary
// storage just copy appropriate chunk from the beginning to the wrap boundary when needed.
	Bitu offset = vidstart & vga.draw.linear_mask;
	if (vga.draw.linear_mask-offset < vga.draw.line_length)
		memcpy(vga.draw.linear_base+vga.draw.linear_mask+1, vga.draw.linear_base, vga.draw.line_length);
	Bit8u *ret = &vga.draw.linear_base[ offset ];
#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;
}

static Bit8u * VGA_Draw_Xlat16_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bit8u *ret = &vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];
	Bit16u* temps = (Bit16u*) TempLine;
	for(Bitu i = 0; i < vga.draw.line_length; i++) {
		temps[i]=vga.dac.xlat16[ret[i]];
	}
	return TempLine;
	/*
#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;*/
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
	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu col=vidmem[cx*2+1];
		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		Bit32u mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		*draw++=(fg&mask1) | (bg&~mask1);
		*draw++=(fg&mask2) | (bg&~mask2);
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

static Bit8u * VGA_TEXT_Xlat16_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit16u * draw=(Bit16u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);
	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu col=vidmem[cx*2+1];
		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		Bit32u mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		
		mask1=(fg&mask1) | (bg&~mask1);
		mask2=(fg&mask2) | (bg&~mask2);

		for(int i = 0; i < 4; i++) {
			*draw++ = vga.dac.xlat16[(mask1>>8*i)&0xff];
		}
		for(int i = 0; i < 4; i++) {
			*draw++ = vga.dac.xlat16[(mask2>>8*i)&0xff];
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit16u *)&TempLine[font_addr*16];
		Bit8u att=(Bit8u)(TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf]&0xff);
		for(int i = 0; i < 8; i++) {
			*draw++ = vga.dac.xlat16[att];
		}
	}
skip_cursor:
	return TempLine;
}

/*
static Bit8u * VGA_TEXT_Draw_Line_9(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit8u * draw=(Bit8u *)TempLine;
	bool underline=(Bitu)(vga.crtc.underline_location&0x1f)==line;
	Bit8u pel_pan=(Bit8u)vga.draw.panning;
	if ((vga.attr.mode_control&0x20) && (vga.draw.lines_done>=vga.draw.split_line)) pel_pan=0;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);
	Bit8u chr=vidmem[0];
	Bit8u col=vidmem[1];
	Bit8u font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
	if (underline && ((col&0x07) == 0x01)) font=0xff;
	Bit8u fg=col&0xf;
	Bit8u bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
	Bitu draw_blocks=vga.draw.blocks;
	draw_blocks++;
	for (Bitu cx=1;cx<draw_blocks;cx++) {
		if (pel_pan) {
			chr=vidmem[cx*2];
			col=vidmem[cx*2+1];
			if (underline && ((col&0x07) == 0x01)) font|=0xff>>(8-pel_pan);
			else font|=vga.draw.font_tables[(col >> 3)&1][chr*32+line]>>(8-pel_pan);
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		} else {
			chr=vidmem[(cx-1)*2];
			col=vidmem[(cx-1)*2+1];
			if (underline && ((col&0x07) == 0x01)) font=0xff;
			else font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		}
		if (FontMask[col>>7]==0) font=0;
		*draw++=(font&0x80)?fg:bg;		*draw++=(font&0x40)?fg:bg;
		*draw++=(font&0x20)?fg:bg;		*draw++=(font&0x10)?fg:bg;
		*draw++=(font&0x08)?fg:bg;		*draw++=(font&0x04)?fg:bg;
		*draw++=(font&0x02)?fg:bg;
		Bit8u last=(font&0x01)?fg:bg;
		*draw++=last;
		*draw++=((vga.attr.mode_control&0x04) && ((chr<0xc0) || (chr>0xdf))) ? bg : last;
		if (pel_pan) {
			if (underline && ((col&0x07) == 0x01)) font=0xff;
			else font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=&TempLine[font_addr*9];
		Bit8u fg=vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf;
		*draw++=fg;		*draw++=fg;		*draw++=fg;		*draw++=fg;
		*draw++=fg;		*draw++=fg;		*draw++=fg;		*draw++=fg;
	}
skip_cursor:
	return TempLine;
}
*/

/* FIXME: This... doesn't quite render hpel panning correctly. The 9th pixel is static, and the color attribute is static. Ugh */
static Bit8u * VGA_TEXT_Xlat16_Draw_Line_9(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit16u * draw=(Bit16u *)TempLine;
	bool underline=(Bitu)(vga.crtc.underline_location&0x1f)==line;
	Bit8u pel_pan=(Bit8u)vga.draw.panning;
	if ((vga.attr.mode_control&0x20) && (vga.draw.lines_done>=vga.draw.split_line)) pel_pan=0;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);
	Bit8u chr=vidmem[0];
	Bit8u col=vidmem[1];
	Bit8u font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
	if (underline && ((col&0x07) == 0x01)) font=0xff;
	Bit8u fg=col&0xf;
	Bit8u bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
	Bitu draw_blocks=vga.draw.blocks;
	draw_blocks++;
	for (Bitu cx=1;cx<draw_blocks;cx++) {
		if (pel_pan) {
			chr=vidmem[cx*2];
			col=vidmem[cx*2+1];
			if (underline && ((col&0x07) == 0x01)) font|=0xff>>(8-pel_pan);
			else font|=vga.draw.font_tables[(col >> 3)&1][chr*32+line]>>(8-pel_pan);
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		} else {
			chr=vidmem[(cx-1)*2];
			col=vidmem[(cx-1)*2+1];
			if (underline && ((col&0x07) == 0x01)) font=0xff;
			else font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		}
		if (FontMask[col>>7]==0) font=0;
		Bit8u mask=0x80;
		for (int i = 0; i < 7; i++) {
			*draw++=vga.dac.xlat16[font&mask?fg:bg];
			mask>>=1;
		}
		Bit16u lastval=vga.dac.xlat16[font&mask?fg:bg];
		*draw++=lastval;
		*draw++=(((vga.attr.mode_control&0x04) && ((chr<0xc0) || (chr>0xdf))) && 
			!(underline && ((col&0x07) == 0x01))) ? 
			(vga.dac.xlat16[bg]) : lastval;
		if (pel_pan) {
			if (underline && ((col&0x07) == 0x01)) font=0xff;
			else font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit16u*)&TempLine[font_addr*18];
		Bit8u fg=vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf;
		for(int i = 0; i < 8; i++) {
			*draw++ = vga.dac.xlat16[fg];
		}
		//if(underline && ((col&0x07) == 0x01)) 
		//	*draw = vga.dac.xlat16[fg];
	}
skip_cursor:
	return TempLine;
}

#ifdef VGA_KEEP_CHANGES
static INLINE void VGA_ChangesEnd(void ) {
	if ( vga.changes.active ) {
//		vga.changes.active = false;
		Bitu end = vga.draw.address >> VGA_CHANGE_SHIFT;
		Bitu total = 4 + end - vga.changes.start;
		Bit32u clearMask = vga.changes.clearMask;
		total >>= 2;
		Bit32u *clear = (Bit32u *)&vga.changes.map[  vga.changes.start & ~3 ];
		while ( total-- ) {
			clear[0] &= clearMask;
			clear++;
		}
	}
}
#endif


static void VGA_ProcessSplit() {
	// On the EGA the address is always reset to 0.
	if ((vga.attr.mode_control&0x20) || (machine==MCH_EGA)) {
		vga.draw.address=0;
	} else {
		// In text mode only the characters are shifted by panning, not the address;
		// this is done in the text line draw function.
		vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
		if (!(vga.mode==M_TEXT)) vga.draw.address += vga.draw.panning;
	}
	vga.draw.address_line=0;
}

static void VGA_DrawSingleLine(Bitu /*blah*/) {
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		// draw blanked line (DoWhackaDo, Alien Carnage, TV sports Football)
		memset(TempLine, 0, sizeof(TempLine));
		RENDER_DrawLine(TempLine);
	} else {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );	
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
		PIC_AddEvent(VGA_DrawSingleLine,(float)vga.draw.delay.htotal);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawPart(Bitu lines) {
	while (lines--) {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		RENDER_DrawLine(data);
		vga.draw.address_line++;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
		vga.draw.lines_done++;
		if (vga.draw.split_line==vga.draw.lines_done) {
#ifdef VGA_KEEP_CHANGES
			VGA_ChangesEnd( );
#endif
			VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
			vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
#endif
		}
	}
	if (--vga.draw.parts_left) {
		PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts,
			 (vga.draw.parts_left!=1) ? vga.draw.parts_lines  : (vga.draw.lines_total - vga.draw.lines_done));
	} else {
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesEnd();
#endif
		RENDER_EndUpdate(false);
	}
}

void VGA_SetBlinking(Bitu enabled) {
	Bitu b;
	LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",enabled);
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

#ifdef VGA_KEEP_CHANGES
static void INLINE VGA_ChangesStart( void ) {
	vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
	vga.changes.last = vga.changes.start;
	if ( vga.changes.lastAddress != vga.draw.address ) {
//		LOG_MSG("Address");
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.changes.lastAddress = vga.draw.address;
	} else if ( render.fullFrame ) {
//		LOG_MSG("Full Frame");
		VGA_DrawLine = VGA_Draw_Linear_Line;
	} else {
//		LOG_MSG("Changes");
		VGA_DrawLine = VGA_Draw_Changes_Line;
	}
	vga.changes.active = true;
	vga.changes.checkMask = vga.changes.writeMask;
	vga.changes.clearMask = ~( 0x01010101 << (vga.changes.frame & 7));
	vga.changes.frame++;
	vga.changes.writeMask = 1 << (vga.changes.frame & 7);
}
#endif

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
	vga.config.real_start=vga.config.display_start & (vga.vmemwrap-1);
	vga.draw.bytes_skip = vga.config.bytes_skip;
}
 
static void VGA_PanningLatch(Bitu /*val*/) {
	vga.draw.panning = vga.config.pel_panning;
}

static void VGA_VerticalTimer(Bitu /*val*/) {
	vga.draw.delay.framestart = PIC_FullIndex();
	PIC_AddEvent( VGA_VerticalTimer, (float)vga.draw.delay.vtotal );
	
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
	case MCH_EGA:
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
		// EGA: 82c435 datasheet: interrupt happens at display end
		// VGA: checked with scope; however disabled by default by jumper on VGA boards
		// add a little amount of time to make sure the last drawpart has already fired
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	default:
		E_Exit("This new machine needs implementation in VGA_VerticalTimer too.");
		break;
	}
	//Check if we can actually render, else skip the rest (frameskip)
	if (!RENDER_StartUpdate())
		return;

	vga.draw.address_line = vga.config.hlines_skip;
	if (IS_EGAVGA_ARCH) {
		vga.draw.split_line = (Bitu)((vga.config.line_compare+1)/vga.draw.lines_scaled);
		if ((svgaCard==SVGA_S3Trio) && (vga.config.line_compare==0)) vga.draw.split_line=0;
		vga.draw.split_line -= vga.draw.vblank_skip;
	} else {
		vga.draw.split_line = 0x10000;	// don't care
	}
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;
	// go figure...
	if (machine==MCH_EGA) vga.draw.split_line*=2;
//	if (machine==MCH_EGA) vga.draw.split_line = ((((vga.config.line_compare&0x5ff)+1)*2-1)/vga.draw.lines_scaled);
#ifdef VGA_KEEP_CHANGES
	bool startaddr_changed=false;
#endif
	switch (vga.mode) {
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1)) vga.draw.linear_mask &= ~0x10000;
		else vga.draw.linear_mask |= 0x10000;
	case M_LIN4:
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
		break;
	case M_VGA:
		if(vga.config.compatible_chain4 && (vga.crtc.underline_location & 0x40)) {
			/* FIX: Some demos rely on being able to access and scroll the first 128KB at least.
			 *      This fixes screen corruption in "Abaddon - Cycling" */
			vga.draw.linear_base = vga.fastmem;
			vga.draw.linear_mask = 0x3ffff;	/* 256KB */
		} else {
			vga.draw.linear_base = vga.mem.linear;
			vga.draw.linear_mask = vga.vmemwrap - 1;
		}
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
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
		vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.address *= 2;
		vga.draw.cursor.count++;
		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.draw.blinking & (vga.draw.cursor.count >> 4)) ?
			0 : 0xffffffff;
		break;
	case M_HERC_GFX:
		break;
	case M_CGA4:case M_CGA2:case M_AMSTRAD:
		vga.draw.address=(vga.draw.address*2)&0x1fff;
		break;
	case M_CGA16:
	case M_TANDY2:case M_TANDY4:case M_TANDY16:
		vga.draw.address *= 2;
		break;
	default:
		break;
	}
	if (GCC_UNLIKELY(vga.draw.split_line==0)) VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
	if (startaddr_changed) VGA_ChangesStart();
#endif

	// check if some lines at the top off the screen are blanked
	float draw_skip = 0.0;
	if (GCC_UNLIKELY(vga.draw.vblank_skip)) {
		draw_skip = (float)(vga.draw.delay.htotal * vga.draw.vblank_skip);
		vga.draw.address += vga.draw.address_add * (vga.draw.vblank_skip/(vga.draw.address_line_total));
	}

	// add the draw event
	switch (vga.draw.mode) {
	case PART:
		if (GCC_UNLIKELY(vga.draw.parts_left)) {
			LOG(LOG_VGAMISC,LOG_NORMAL)( "Parts left: %d", vga.draw.parts_left );
			PIC_RemoveEvents(VGA_DrawPart);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		vga.draw.parts_left = vga.draw.parts_total;
		PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts + draw_skip,vga.draw.parts_lines);
		break;
	case LINE:
		if (GCC_UNLIKELY(vga.draw.lines_done < vga.draw.lines_total)) {
			LOG(LOG_VGAMISC,LOG_NORMAL)( "Lines left: %d", 
				vga.draw.lines_total-vga.draw.lines_done);
			PIC_RemoveEvents(VGA_DrawSingleLine);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		PIC_AddEvent(VGA_DrawSingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
		break;
	//case EGALINE:
	}
}

void VGA_CheckScanLength(void) {
	switch (vga.mode) {
	case M_EGA:
	case M_LIN4:
		vga.draw.address_add=vga.config.scan_len*16;
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.address_add=vga.config.scan_len*8;
		break;
	case M_TEXT:
		vga.draw.address_add=vga.config.scan_len*4;
		break;
	case M_CGA2:
	case M_CGA4:
	case M_CGA16:
	case M_AMSTRAD:
		vga.draw.address_add=80;
		return;
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
	if (hwcursor_active) {
		switch(vga.mode) {
		case M_LIN32:
			VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
			break;
		case M_LIN15:
		case M_LIN16:
			VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
			break;
		default:
			VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
		}
	} else {
		VGA_DrawLine=VGA_Draw_Linear_Line;
	}
}

void VGA_SetupDrawing(Bitu /*val*/) {
	if (vga.mode==M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
	}
	// set the drawing mode
	switch (machine) {
	case MCH_CGA:
	case MCH_PCJR:
		vga.draw.mode = LINE;
		break;
	case MCH_VGA:
		if (svgaCard==SVGA_None) {
			vga.draw.mode = LINE;
			break;
		}
		// fall-through
	default:
		vga.draw.mode = PART;
		break;
	}
	
	/* Calculate the FPS for this screen */
	double fps; Bitu clock;
	Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
	Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
	Bitu vblank_skip;
	if (IS_EGAVGA_ARCH) {
		htotal = vga.crtc.horizontal_total;
		hdend = vga.crtc.horizontal_display_end;
		hbend = vga.crtc.end_horizontal_blanking&0x1F;
		hbstart = vga.crtc.start_horizontal_blanking;
		hrstart = vga.crtc.start_horizontal_retrace;

		vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
		vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		
		if (IS_VGA_ARCH) {
			// additional bits only present on vga cards
			htotal |= (vga.s3.ex_hor_overflow & 0x1) << 8;
			htotal += 3;
			hdend |= (vga.s3.ex_hor_overflow & 0x2) << 7;
			hbend |= (vga.crtc.end_horizontal_retrace&0x80) >> 2;
			hbstart |= (vga.s3.ex_hor_overflow & 0x4) << 6;
			hrstart |= (vga.s3.ex_hor_overflow & 0x10) << 4;
			
			vtotal |= (vga.crtc.overflow & 0x20) << 4;
			vtotal |= (vga.s3.ex_ver_overflow & 0x1) << 10;
			vdend |= (vga.crtc.overflow & 0x40) << 3; 
			vdend |= (vga.s3.ex_ver_overflow & 0x2) << 9;
			vbstart |= (vga.crtc.maximum_scan_line & 0x20) << 4;
			vbstart |= (vga.s3.ex_ver_overflow & 0x4) << 8;
			vrstart |= ((vga.crtc.overflow & 0x80) << 2);
			vrstart |= (vga.s3.ex_ver_overflow & 0x10) << 6;
			vbend = vga.crtc.end_vertical_blanking & 0x7f;
		} else { // EGA
			vbend = vga.crtc.end_vertical_blanking & 0x1f;
		}
		htotal += 2;
		vtotal += 2;
		hdend += 1;
		vdend += 1;
		vbstart += 1;

		hbend = hbstart + ((hbend - hbstart) & 0x3F);
		hrend = vga.crtc.end_horizontal_retrace & 0x1f;
		hrend = (hrend - hrstart) & 0x1f;
		
		if ( !hrend ) hrend = hrstart + 0x1f + 1;
		else hrend = hrstart + hrend;

		vrend = vga.crtc.vertical_retrace_end & 0xF;
		vrend = ( vrend - vrstart)&0xF;
		
		if ( !vrend) vrend = vrstart + 0xf + 1;
		else vrend = vrstart + vrend;

		vbend = (vbend - vbstart) & 0x7f;
		if ( !vbend) vbend = vbstart + 0x7f + 1;
		else vbend = vbstart + vbend;

		vbend++;
			
		if (svga.get_clock) {
			clock = svga.get_clock();
		} else {
			switch ((vga.misc_output >> 2) & 3) {
			case 0:	
				clock = (machine==MCH_EGA) ? 14318180 : 25175000;
				break;
			case 1:
			default:
				clock = (machine==MCH_EGA) ? 16257000 : 28322000;
				break;
			}
		}

		/* Check for 8 for 9 character clock mode */
		if (vga.seq.clocking_mode & 1 ) clock/=8; else clock/=9;
		/* Check for pixel doubling, master clock/2 */
		if (vga.seq.clocking_mode & 0x8) {
			htotal*=2;
		}
		vga.draw.address_line_total=(vga.crtc.maximum_scan_line&0x1f)+1;
		if(IS_VGA_ARCH && (svgaCard==SVGA_None) && (vga.mode==M_EGA || vga.mode==M_VGA)) {
			// vgaonly; can't use with CGA because these use address_line for their
			// own purposes.
			// Set the low resolution modes to have as many lines as are scanned - 
			// Quite a few demos change the max_scanline register at display time
			// to get SFX: Majic12 show, Magic circle, Copper, GBU, Party91
			if( vga.crtc.maximum_scan_line&0x80) vga.draw.address_line_total*=2;
			vga.draw.double_scan=false;
		}
		else if (IS_VGA_ARCH) vga.draw.double_scan=(vga.crtc.maximum_scan_line&0x80)>0;
		else vga.draw.double_scan=(vtotal==262);
	} else {
		htotal = vga.other.htotal + 1;
		hdend = vga.other.hdend;
		hbstart = hdend;
		hbend = htotal;
		hrstart = vga.other.hsyncp;
		hrend = hrstart + vga.other.hsyncw;

		vga.draw.address_line_total = vga.other.max_scanline + 1;
		vtotal = vga.draw.address_line_total * (vga.other.vtotal+1)+vga.other.vadjust;
		vdend = vga.draw.address_line_total * vga.other.vdend;
		vrstart = vga.draw.address_line_total * vga.other.vsyncp;
		vrend = vrstart + 16; // vsync width is fixed to 16 lines on the MC6845 TODO Tandy
		vbstart = vdend;
		vbend = vtotal;
		vga.draw.double_scan=false;
		switch (machine) {
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			clock=((vga.tandy.mode_control & 1) ? 14318180 : (14318180/2))/8;
			break;
		case MCH_HERC:
			if (vga.herc.mode_control & 0x2) clock=16000000/16;
			else clock=16000000/8;
			break;
		default:
			clock = 14318180;
			break;
		}
		vga.draw.delay.hdend = hdend*1000.0/clock; //in milliseconds
	}
#if C_DEBUG
	LOG(LOG_VGA,LOG_NORMAL)("h total %d end %d blank (%d/%d) retrace (%d/%d)",
		htotal, hdend, hbstart, hbend, hrstart, hrend );
	LOG(LOG_VGA,LOG_NORMAL)("v total %d end %d blank (%d/%d) retrace (%d/%d)",
		vtotal, vdend, vbstart, vbend, vrstart, vrend );
#endif
	if (!htotal) return;
	if (!vtotal) return;
	
	if (vga_force_refresh_rate > 0) {
		/* force the VGA refresh rate by setting fps and bending the clock to our will */
		LOG(LOG_VGA,LOG_NORMAL)("VGA forced refresh rate in effect, %.3f",vga_force_refresh_rate);
		fps=vga_force_refresh_rate;
		clock=((double)(vtotal*htotal))*fps;
	}
	else {
		// The screen refresh frequency
		fps=(double)clock/(vtotal*htotal);
		LOG(LOG_VGA,LOG_NORMAL)("VGA refresh rate is now, %.3f",vga_force_refresh_rate);
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
	if (IS_VGA_ARCH) { // others need more investigation
		if (vbend > vtotal) {
			// blanking wraps to the start of the screen
			vblank_skip = vbend&0x7f;
			
			// on blanking wrap to 0, the first line is not blanked
			// this is used by the S3 BIOS and other S3 drivers in some SVGA modes
			if((vbend&0x7f)==1) vblank_skip = 0;
			
			// it might also cut some lines off the bottom
			if(vbstart < vdend) {
				vdend = vbstart;
			}
			LOG(LOG_VGA,LOG_WARN)("Blanking wrap to line %d", vblank_skip);
		} else if (vbstart==1) {
			// blanking is used to cut lines at the start of the screen
			vblank_skip = vbend;
			LOG(LOG_VGA,LOG_WARN)("Upper %d lines of the screen blanked", vblank_skip);
		} else if (vbstart < vdend) {
			if(vbend < vdend) {
				// the game wants a black bar somewhere on the screen
				LOG(LOG_VGA,LOG_WARN)("Unsupported blanking: line %d-%d",vbstart,vbend);
			} else {
				// blanking is used to cut off some lines from the bottom
				vdend = vbstart;
			}
		}
		vdend -= vblank_skip;
	}
	// Display end
	vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

	vga.draw.parts_total=VGA_PARTS;
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
	double pwidth = (machine==MCH_EGA) ? (114.0 / htotal) : (100.0 / htotal);
	//Base pixel height around vertical totals of modes that have 100 clocks horizontal
	//Different sync values gives different scaling of the whole vertical range
	//VGA monitor just seems to thighten or widen the whole vertical range
	double pheight;
	double target_total = (machine==MCH_EGA) ? 262.0 : 449.0;
	Bitu sync = vga.misc_output >> 6;
	switch ( sync ) {
	case 0:		// This is not defined in vga specs,
				// Kiet, seems to be slightly less than 350 on my monitor
		//340 line mode, filled with 449 total
		pheight = (480.0 / 340.0) * ( target_total / vtotal );
		break;
	case 1:		//400 line mode, filled with 449 total
		pheight = (480.0 / 400.0) * ( target_total / vtotal );
		break;
	case 2:		//350 line mode, filled with 449 total
		//This mode seems to get regular 640x400 timing and goes for a loong retrace
		//Depends on the monitor to stretch the screen
		pheight = (480.0 / 350.0) * ( target_total / vtotal );
		break;
	case 3:		//480 line mode, filled with 525 total
	default:
		pheight = (480.0 / 480.0) * ( 525.0 / vtotal );
		break;
	}

	double aspect_ratio = pheight / pwidth;

	vga.draw.delay.parts = vga.draw.delay.vdend/vga.draw.parts_total;
	vga.draw.resizing=false;
	vga.draw.vret_triggered=false;

	//Check to prevent useless black areas
	if (hbstart<hdend) hdend=hbstart;
	if ((!IS_VGA_ARCH) && (vbstart<vdend)) vdend=vbstart;


	Bitu width=hdend;
	Bitu height=vdend;
	bool doubleheight=false;
	bool doublewidth=false;

	//Set the bpp
	Bitu bpp;
	switch (vga.mode) {
	case M_LIN15:
		bpp = 15;
		break;
	case M_LIN16:
		bpp = 16;
		break;
	case M_LIN32:
		bpp = 32;
		break;
	default:
		bpp = 8;
		break;
	}
	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap - 1;
	switch (vga.mode) {
	case M_VGA:
		doublewidth=true;
		width<<=2;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) {
			bpp=16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		} else VGA_DrawLine = VGA_Draw_Linear_Line;
		break;
	case M_LIN8:
		if (vga.crtc.mode_control & 0x8)
			width >>=1;
		else if(svgaCard == SVGA_S3Trio && !(vga.s3.reg_3a&0x10)) {
			doublewidth=true;
			width >>=1;
		}
		// fall-through
	case M_LIN32:
		width<<=3;
		if (vga.crtc.mode_control & 0x8)
 			doublewidth = true;
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN15:
 	case M_LIN16:
		// 15/16 bpp modes double the horizontal values
		width<<=2;
		if ((vga.crtc.mode_control & 0x8) || (svgaCard == SVGA_S3Trio && (vga.s3.pll.cmd & 0x10)))
			doublewidth = true;
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN4:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap<<1) - 1;
		break;
	case M_EGA:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) {
			bpp=16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		} else VGA_DrawLine=VGA_Draw_Linear_Line;

		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap<<1) - 1;
		break;
	case M_CGA16:
		doubleheight=true;
		vga.draw.blocks=width*2;
		width<<=4;
		VGA_DrawLine=VGA_Draw_CGA16_Line;
		break;
	case M_CGA4:
		doublewidth=true;
		vga.draw.blocks=width*2;
		width<<=3;
		VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_CGA2:
		doubleheight=true;
		vga.draw.blocks=2*width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TEXT:
		aspect_ratio=1.0;
		vga.draw.blocks=width;
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None) && !(vga.seq.clocking_mode&0x01)) {
			width*=9;				/* 9 bit wide text font */
			VGA_DrawLine=VGA_TEXT_Xlat16_Draw_Line_9;
			bpp=16;
//			VGA_DrawLine=VGA_TEXT_Draw_Line_9;
		} else {
			width<<=3;				/* 8 bit wide text font */
			if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) {
				VGA_DrawLine=VGA_TEXT_Xlat16_Draw_Line;
				bpp=16;
			} else VGA_DrawLine=VGA_TEXT_Draw_Line;
		}
		break;
	case M_HERC_GFX:
		aspect_ratio=1.5;
		vga.draw.blocks=width*2;
		width*=16;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY2:
		aspect_ratio=1.2;
		doubleheight=true;
		if (machine==MCH_PCJR) doublewidth=(vga.tandy.gfx_control & 0x8)==0x00;
		else doublewidth=(vga.tandy.mode_control & 0x10)==0;
		vga.draw.blocks=width * (doublewidth ? 4:8);
		width=vga.draw.blocks*2;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY4:
		aspect_ratio=1.2;
		doubleheight=true;
		if (machine==MCH_TANDY) doublewidth=(vga.tandy.mode_control & 0x10)==0;
		else doublewidth=(vga.tandy.mode_control & 0x01)==0x00;
		vga.draw.blocks=width * 2;
		width=vga.draw.blocks*4;
		if ((machine==MCH_TANDY && (vga.tandy.gfx_control & 0x8)) ||
			(machine==MCH_PCJR && (vga.tandy.mode_control==0x0b)))
			VGA_DrawLine=VGA_Draw_2BPPHiRes_Line;
		else VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		aspect_ratio=1.2;
		doubleheight=true;
		vga.draw.blocks=width*2;
		if (vga.tandy.mode_control & 0x1) {
			if (( machine==MCH_TANDY ) && ( vga.tandy.mode_control & 0x10 )) {
				doublewidth = false;
				vga.draw.blocks*=2;
				width=vga.draw.blocks*2;
			} else {
				doublewidth = true;
				width=vga.draw.blocks*2;
			}
			VGA_DrawLine=VGA_Draw_4BPP_Line;
		} else {
			doublewidth=true;
			width=vga.draw.blocks*4;
			VGA_DrawLine=VGA_Draw_4BPP_Line_Double;
		}
		break;
	case M_TANDY_TEXT: /* Also CGA */
		doublewidth=(vga.tandy.mode_control & 0x1)==0;
		aspect_ratio=1;
		doubleheight=true;
		vga.draw.blocks=width;
		width<<=3;
		if (machine==MCH_CGA && !doublewidth && enableCGASnow)
			VGA_DrawLine=VGA_CGASNOW_TEXT_Draw_Line; /* Alternate version that emulates CGA snow */
		else
			VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	case M_HERC_TEXT:
		aspect_ratio=1;
		vga.draw.blocks=width;
		width<<=3;
		VGA_DrawLine=VGA_TEXT_Herc_Draw_Line;
		break;
	case M_AMSTRAD:
		doubleheight=true;
		vga.draw.blocks=width*2;
		VGA_DrawLine=VGA_Draw_AMS_4BPP_Line;
		width<<=4;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution",vga.mode);
		break;
	}
	VGA_CheckScanLength();
	if (vga.draw.double_scan) {
		if (IS_VGA_ARCH) { 
			vga.draw.vblank_skip /= 2;
			height/=2;
		}
		doubleheight=true;
	}
	vga.draw.vblank_skip = vblank_skip;
		
	if(!(IS_VGA_ARCH && (svgaCard==SVGA_None) && (vga.mode==M_EGA || vga.mode==M_VGA))) {
		//Only check for extra double height in vga modes
		//(line multiplying by address_line_total)
		if (!doubleheight && (vga.mode<M_TEXT) && !(vga.draw.address_line_total & 1)) {
			vga.draw.address_line_total/=2;
			doubleheight=true;
			height/=2;
		}
	}
	vga.draw.lines_total=height;
	vga.draw.parts_lines=vga.draw.lines_total/vga.draw.parts_total;
	vga.draw.line_length = width * ((bpp + 1) / 8);
	vga.draw.clock = clock;
#ifdef VGA_KEEP_CHANGES
	vga.changes.active = false;
	vga.changes.frame = 0;
	vga.changes.writeMask = 1;
#endif
    /* 
	   Cheap hack to just make all > 640x480 modes have 4:3 aspect ratio
	*/
	if ( width >= 640 && height >= 480 ) {
		aspect_ratio = ((float)width / (float)height) * ( 3.0 / 4.0);
	}
//	LOG_MSG("ht %d vt %d ratio %f", htotal, vtotal, aspect_ratio );

	// need to change the vertical timing?
	if (fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001) {
		vga.draw.delay.vtotal = 1000.0 / fps;
		VGA_KillDrawing();
		PIC_RemoveEvents(VGA_Other_VertInterrupt);
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		VGA_VerticalTimer(0);
	}

#if C_DEBUG
	LOG(LOG_VGA,LOG_NORMAL)("h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
		vga.draw.delay.htotal,(1.0/vga.draw.delay.htotal),
		vga.draw.delay.hblkstart,vga.draw.delay.hblkend,
		vga.draw.delay.hrstart,vga.draw.delay.hrend);
	LOG(LOG_VGA,LOG_NORMAL)("v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
		vga.draw.delay.vtotal,(1000.0/vga.draw.delay.vtotal),
		vga.draw.delay.vblkstart,vga.draw.delay.vblkend,
		vga.draw.delay.vrstart,vga.draw.delay.vrend);
#endif

	// need to resize the output window?
	if ((width != vga.draw.width) ||
		(height != vga.draw.height) ||
		(vga.draw.doublewidth != doublewidth) ||
		(vga.draw.doubleheight != doubleheight) ||
		(fabs(aspect_ratio - vga.draw.aspect_ratio) > 0.0001) ||
		(vga.draw.bpp != bpp)) {

		VGA_KillDrawing();

		vga.draw.width = width;
		vga.draw.height = height;
		vga.draw.doublewidth = doublewidth;
		vga.draw.doubleheight = doubleheight;
		vga.draw.aspect_ratio = aspect_ratio;
		vga.draw.bpp = bpp;
		if (doubleheight) vga.draw.lines_scaled=2;
		else vga.draw.lines_scaled=1;
#if C_DEBUG
		LOG(LOG_VGA,LOG_NORMAL)("Width %d, Height %d, fps %f",width,height,fps);
		LOG(LOG_VGA,LOG_NORMAL)("%s width, %s height aspect %f",
			doublewidth ? "double":"normal",doubleheight ? "double":"normal",aspect_ratio);
#endif
		RENDER_SetSize(width,height,bpp,(float)fps,aspect_ratio,doublewidth,doubleheight);
	}
}

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawPart);
	PIC_RemoveEvents(VGA_DrawSingleLine);
	vga.draw.parts_left = 0;
	vga.draw.lines_done = ~0;
	RENDER_EndUpdate(true);
}
