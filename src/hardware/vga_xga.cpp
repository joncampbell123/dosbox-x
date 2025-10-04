/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "logging.h"
#include "vga.h"
#include <math.h>
#include <stdio.h>
#include "callback.h"
#include "cpu.h"		// for 0x3da delay

/* do not issue CPU-side I/O here -- this code emulates functions that the GDC itself carries out, not on the CPU */
#include "cpu_io_is_forbidden.h"

#define S3_VALIDATE_VIRGE_PORTS

#ifdef S3_VALIDATE_VIRGE_PORTS
#include <assert.h>
#endif

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
#endif

#define XGA_SCREEN_WIDTH	vga.s3.xga_screen_width
#define XGA_COLOR_MODE		vga.s3.xga_color_mode

#define XGA_SHOW_COMMAND_TRACE 0

struct XGAStatus {
	struct scissorreg {
		uint16_t x1, y1, x2, y2;
	} scissors;

	uint32_t readmask;
	uint32_t writemask;

	uint32_t forecolor;
	uint32_t backcolor;

	uint32_t color_compare;

	Bitu curcommand;

	uint16_t foremix;
	uint16_t backmix;

	uint16_t curx, cury;
	uint16_t curx2, cury2;
	uint16_t destx, desty;
	uint16_t destx2, desty2;

	uint16_t ErrTerm;
	uint16_t MIPcount;
	uint16_t MAPcount;

	uint16_t pix_cntl;
	uint16_t control1;
	uint16_t control2;
	uint16_t read_sel;

	struct XGA_WaitCmd {
		bool newline;
		bool wait;
		uint16_t cmd;
		uint16_t curx, cury, curdx, curdy;
		uint16_t x1, y1, x2, y2, sizex, sizey;
		uint32_t data; /* transient data passed by multiple calls */
		Bitu datasize;
		Bitu buswidth;
		bool bswap16; /* bit 12 of the CMD register (S3 86C928, Trio32/Trio64/Trio64V+): For any 16-bit word, swap hi/lo bytes (including both 16-bit words in 32-bit transfer) */
	} waitcmd;

	/* S3 Virge state */
	struct XGA_VirgeState {
		struct reggroup {
			uint32_t src_base;               /* +00D4 */
			uint32_t dst_base;               /* +00D8 */
			uint32_t right_clip;             /* +00DC [LO WORD] "position of last pixel to be drawn" which means left <= x <= right inclusive, right? */
			uint32_t left_clip;              /* +00DC [HI WORD] */
			uint32_t bottom_clip;            /* +00E0 [LO WORD] "position of last pixel to be drawn" which means top <= y <= bottom inclusive, right? */
			uint32_t top_clip;               /* +00E0 [HI WORD] */
			uint32_t src_stride;             /* +00E4 [LO WORD] */
			uint32_t dst_stride;             /* +00E4 [HI WORD] */
			uint64_t mono_pat;               /* +00E8, +00EC */
			uint32_t mono_pat_bgcolor;       /* +00F0 */
			uint32_t mono_pat_fgcolor;       /* +00F4 */
			uint32_t src_bgcolor;            /* +00F8 */
			uint32_t src_fgcolor;            /* +00FC */
			uint32_t command_set;            /* +0100 */
			uint32_t rect_width;             /* +0104 [LO WORD] */
			uint32_t rect_height;            /* +0104 [HI WORD] */
			uint32_t rect_src_x;             /* +0108 [LO WORD] */
			uint32_t rect_src_y;             /* +0108 [HI WORD] */
			uint32_t rect_dst_x;             /* +010C [LO WORD] */
			uint32_t rect_dst_y;             /* +010C [HI WORD] */
			int32_t  lindrawend0;            /* +016C [LO WORD] first pixel */
			int32_t  lindrawend1;            /* +016C [HI WORD] last pixel */
			int32_t  lindrawxdelta;          /* +0170 */
			int32_t  lindrawstartx;          /* +0174 */
			uint32_t lindrawstarty;          /* +0178 */
			uint32_t lindrawcounty;          /* +017C bit 31 is direction */

			void set__src_base(uint32_t val); /* +00D4 */
			void set__dst_base(uint32_t val); /* +00D8 */
			void set__src_dest_stride_00e4(uint32_t val); /* +00E4 */
			void set__mono_pat_dword(unsigned int idx,uint32_t val); /* +00E8, +00EC */
			void set__mono_pat_bgcolor(uint32_t val); /* +00F0 */
			void set__mono_pat_fgcolor(uint32_t val); /* +00F4 */
			void set__src_bgcolor(uint32_t val); /* +00F8 */
			void set__src_fgcolor(uint32_t val); /* +00FC */
			void set__command_set(uint32_t val); /* +0100 */
			void set__rect_width_height_0104(uint32_t val); /* +0104 */
			void set__rect_src_xy_0108(uint32_t val); /* +0108 */
			void set__rect_dst_xy_010c(uint32_t val); /* +010C */
			void set__left_right_clip_00dc(uint32_t val); /* +00DC */
			void set__top_bottom_clip_00e0(uint32_t val); /* +00E0 */
			void set__lindrawend_016c(uint32_t val); /* +016C */
			void set__lindrawxdelta_0170(uint32_t val); /* +0170 */
			void set__lindrawstartx_0174(uint32_t val); /* +0174 */
			void set__lindrawstartx_0178(uint32_t val); /* +0178 */
			void set__lindrawcounty_017c(uint32_t val); /* +017C */

			uint32_t command_execute_on_register; /* if command set bit 0 set, writing this register will execute command */
		};
		struct reggroup                  bitblt; /* 0xA400-0xA7FF */
		struct reggroup                  line2d; /* 0xA800-0xABFF */
		struct reggroup                  poly2d; /* 0xAC00-0xAFFF */

		/* ViRGE image transfer register */
		struct reggroup*                 imgxferport;
		void                             (*imgxferportfunc)(uint32_t val);

		/* BitBlt state */
		struct BitBltState {
			uint32_t                 startx;
			uint32_t                 starty;
			uint32_t                 stopy;
			uint32_t                 src_stride;
			uint32_t                 src_xrem;
			uint32_t                 src_drem;
			uint64_t                 itf_buffer; /* we shift in 32 bits at a time, making this 64-bit simplifies code */
			uint8_t                  itf_buffer_bytecount;
			uint8_t                  itf_buffer_initskip;
		} bitbltstate;

		union colorpat_t {
			/* NTS: extra 4 bytes so a typecast to pat8 to read 24 bits does not overread the buffer */
			uint8_t                  pat8[68];    /* +A100-A13C 8bpp */
			uint16_t                 pat16[68];   /* +A100-A17C 16bpp */
			uint8_t                  pat24[68*3]; /* +A100-A1BC 24bpp */
			uint32_t                 raw[48];     /* raw DWORD access for I/O handler, ((64*3)/4) == 48 */
		};

		colorpat_t                       colorpat;
		unsigned int                     truecolor_bypp; /* ViRGE cards seem to prefer 24bpp? Windows drivers act like it */
		uint32_t                         truecolor_mask;

		inline struct reggroup &bitblt_validate_port(const uint32_t port) {
#ifdef S3_VALIDATE_VIRGE_PORTS
			assert((port&0xFC00) == 0xA400);
#endif
			return bitblt;
		}

		inline struct reggroup &line2d_validate_port(const uint32_t port) {
#ifdef S3_VALIDATE_VIRGE_PORTS
			assert((port&0xFC00) == 0xA800);
#endif
			return line2d;
		}

		inline struct reggroup &poly2d_validate_port(const uint32_t port) {
#ifdef S3_VALIDATE_VIRGE_PORTS
			assert((port&0xFC00) == 0xAC00);
#endif
			return poly2d;
		}
	} virge;
} xga;

void XGAStatus::XGA_VirgeState::reggroup::set__src_base(uint32_t val) {
	src_base = val & 0x003FFFF8; /* bits [21:3] base address in vmem source data for 2D operations */
}

void XGAStatus::XGA_VirgeState::reggroup::set__dst_base(uint32_t val) {
	dst_base = val & 0x003FFFF8; /* bits [21:3] base address in vmem source data for 2D operations */
}

void XGAStatus::XGA_VirgeState::reggroup::set__src_dest_stride_00e4(uint32_t val) {
	src_stride = val & 0x0FF8; /* bits [11:3] byte stride */
	dst_stride = (val >> 16u) & 0x0FF8; /* bits [27:19] (11+16,3+16) byte stride */
}

void XGAStatus::XGA_VirgeState::reggroup::set__mono_pat_dword(unsigned int idx,uint32_t val) {
	/* idx == 0, low 32 bits.
	 * idx == 1, high 32 bits.
	 * This trick only works if the host processor is little Endian */
	((uint32_t*)(&mono_pat))[idx&1] = val;
}

void XGAStatus::XGA_VirgeState::reggroup::set__mono_pat_bgcolor(uint32_t val) {
	mono_pat_bgcolor = val & 0x00FFFFFFul;
}

void XGAStatus::XGA_VirgeState::reggroup::set__mono_pat_fgcolor(uint32_t val) {
	mono_pat_fgcolor = val & 0x00FFFFFFul;
}

void XGAStatus::XGA_VirgeState::reggroup::set__src_bgcolor(uint32_t val) {
	src_bgcolor = val & 0x00FFFFFFul;
}

void XGAStatus::XGA_VirgeState::reggroup::set__src_fgcolor(uint32_t val) {
	src_fgcolor = val & 0x00FFFFFFul;
}

void XGAStatus::XGA_VirgeState::reggroup::set__command_set(uint32_t val) {
	command_set = val;
}

void XGAStatus::XGA_VirgeState::reggroup::set__rect_width_height_0104(uint32_t val) {
	rect_height = val & 0x07FF;
	rect_width = ((val >> 16ul) & 0x07FF) + 1; /* <- What? But then the height field is THE number of pixels. Why? */
}

void XGAStatus::XGA_VirgeState::reggroup::set__rect_src_xy_0108(uint32_t val) {
	rect_src_y = val & 0x07FF;
	rect_src_x = (val >> 16ul) & 0x07FF;
}

void XGAStatus::XGA_VirgeState::reggroup::set__rect_dst_xy_010c(uint32_t val) {
	rect_dst_y = val & 0x07FF;
	rect_dst_x = (val >> 16ul) & 0x07FF;
}

void XGAStatus::XGA_VirgeState::reggroup::set__left_right_clip_00dc(uint32_t val) {
	right_clip = val & 0x07FF;
	left_clip = (val >> 16ul) & 0x07FF;
}

void XGAStatus::XGA_VirgeState::reggroup::set__top_bottom_clip_00e0(uint32_t val) {
	bottom_clip = val & 0x07FF;
	top_clip = (val >> 16ul) & 0x07FF;
}

void XGAStatus::XGA_VirgeState::reggroup::set__lindrawend_016c(uint32_t val) {
	lindrawend0 = (int32_t)((int16_t)((val >> 16lu) & 0xFFFFu));
	lindrawend1 = (int32_t)((int16_t)(val & 0xFFFFu));
}

void XGAStatus::XGA_VirgeState::reggroup::set__lindrawxdelta_0170(uint32_t val) {
	lindrawxdelta = (int32_t)val;
}

void XGAStatus::XGA_VirgeState::reggroup::set__lindrawstartx_0174(uint32_t val) {
	lindrawstartx = (int32_t)val;
}

void XGAStatus::XGA_VirgeState::reggroup::set__lindrawstartx_0178(uint32_t val) {
	lindrawstarty = val & 0x3FFFu; /* bits [10:0] */
}

void XGAStatus::XGA_VirgeState::reggroup::set__lindrawcounty_017c(uint32_t val) {
	lindrawcounty = val & 0x80003FFFu; /* bit [31], bits [10:0] */
}

void XGA_Write_Multifunc(Bitu val, Bitu len) {
    (void)len;//UNUSED
	Bitu regselect = val >> 12ul;
	Bitu dataval = val & 0xfff;
	switch(regselect) {
		case 0: // minor axis pixel count
			xga.MIPcount = (uint16_t)dataval;
			break;
		case 1: // top scissors
			xga.scissors.y1 = (uint16_t)dataval;
			break;
		case 2: // left
			xga.scissors.x1 = (uint16_t)dataval;
			break;
		case 3: // bottom
			xga.scissors.y2 = (uint16_t)dataval;
			break;
		case 4: // right
			xga.scissors.x2 = (uint16_t)dataval;
			break;
		case 0xa: // data manip control
			xga.pix_cntl = (uint16_t)dataval;
			break;
		case 0xd: // misc 2
			xga.control2 = (uint16_t)dataval;
			break;
		case 0xe:
			xga.control1 = (uint16_t)dataval;
			break;
		case 0xf:
			xga.read_sel = (uint16_t)dataval;
			break;
		default:
			LOG_MSG("XGA: Unhandled multifunction command %x", (int)regselect);
			break;
	}
}

Bitu XGA_Read_Multifunc() {
	switch(xga.read_sel++) {
		case 0: return xga.MIPcount;
		case 1: return xga.scissors.y1;
		case 2: return xga.scissors.x1;
		case 3: return xga.scissors.y2;
		case 4: return xga.scissors.x2;
		case 5: return xga.pix_cntl;
		case 6: return xga.control1;
		case 7: return 0; // TODO
		case 8: return 0; // TODO
		case 9: return 0; // TODO
		case 10: return xga.control2;
		default: return 0;
	}
}


void XGA_DrawPoint(Bitu x, Bitu y, Bitu c) {
	if(!(xga.curcommand & 0x1)) return;
	if(!(xga.curcommand & 0x10)) return;

	if(x < xga.scissors.x1) return;
	if(x > xga.scissors.x2) return;
	if(y < xga.scissors.y1) return;
	if(y > xga.scissors.y2) return;

	uint32_t memaddr = (uint32_t)((y * XGA_SCREEN_WIDTH) + x);
	/* Need to zero out all unused bits in modes that have any (15-bit or "32"-bit -- the last
	   one is actually 24-bit. Without this step there may be some graphics corruption (mainly,
	   during windows dragging. */
	switch(XGA_COLOR_MODE) {
		case M_LIN4:
			{
				uint8_t shf = ((memaddr^1u)&1u)*4u;
				if (GCC_UNLIKELY((memaddr/2) >= vga.mem.memsize)) break;
				vga.mem.linear[memaddr/2] = (vga.mem.linear[memaddr/2] & (0xF0 >> shf)) + ((c&0xF) << shf);
			}
			break;
		case M_LIN8:
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			vga.mem.linear[memaddr] = (uint8_t)c;
			break;
		case M_LIN15:
			if (GCC_UNLIKELY(memaddr*2 >= vga.mem.memsize)) break;
			((uint16_t*)(vga.mem.linear))[memaddr] = (uint16_t)(c&0x7fff);
			break;
		case M_LIN16:
			if (GCC_UNLIKELY(memaddr*2 >= vga.mem.memsize)) break;
			((uint16_t*)(vga.mem.linear))[memaddr] = (uint16_t)(c&0xffff);
			break;
		case M_LIN32:
			if (GCC_UNLIKELY(memaddr*4 >= vga.mem.memsize)) break;
			((uint32_t*)(vga.mem.linear))[memaddr] = (uint32_t)c;
			break;
		default:
			break;
	}
}

Bitu XGA_PointMask() {
	switch(XGA_COLOR_MODE) {
		case M_LIN4:
			return 0xFul;
		case M_LIN8:
			return 0xFFul;
		case M_LIN15:
		case M_LIN16:
			return 0xFFFFul;
		case M_LIN32:
			return 0xFFFFFFFFul;
		default:
			break;
	}
	return 0;
}

Bitu XGA_GetPoint(Bitu x, Bitu y) {
	uint32_t memaddr = (uint32_t)((y * XGA_SCREEN_WIDTH) + x);

	switch(XGA_COLOR_MODE) {
		case M_LIN4:
			if (GCC_UNLIKELY((memaddr/2) >= vga.mem.memsize)) break;
			return (vga.mem.linear[memaddr/2] >> (((memaddr&1)^1)*4)) & 0xF;
		case M_LIN8:
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return vga.mem.linear[memaddr];
		case M_LIN15:
		case M_LIN16:
			if (GCC_UNLIKELY(memaddr*2 >= vga.mem.memsize)) break;
			return ((uint16_t*)(vga.mem.linear))[memaddr];
		case M_LIN32:
			if (GCC_UNLIKELY(memaddr*4 >= vga.mem.memsize)) break;
			return ((uint32_t*)(vga.mem.linear))[memaddr];
		default:
			break;
	}

	return 0;
}


Bitu XGA_GetMixResult(Bitu mixmode, Bitu srcval, Bitu dstdata) {
	Bitu destval = 0;
	switch(mixmode &  0xf) {
		case 0x00: /* not DST */
			destval = ~dstdata;
			break;
		case 0x01: /* 0 (false) */
			destval = 0;
			break;
		case 0x02: /* 1 (true) */
			destval = 0xffffffff;
			break;
		case 0x03: /* 2 DST */
			destval = dstdata;
			break;
		case 0x04: /* not SRC */
			destval = ~srcval;
			break;
		case 0x05: /* SRC xor DST */
			destval = srcval ^ dstdata;
			break;
		case 0x06: /* not (SRC xor DST) */
			destval = ~(srcval ^ dstdata);
			break;
		case 0x07: /* SRC */
			destval = srcval;
			break;
		case 0x08: /* not (SRC and DST) */
			destval = ~(srcval & dstdata);
			break;
		case 0x09: /* (not SRC) or DST */
			destval = (~srcval) | dstdata;
			break;
		case 0x0a: /* SRC or (not DST) */
			destval = srcval | (~dstdata);
			break;
		case 0x0b: /* SRC or DST */
			destval = srcval | dstdata;
			break;
		case 0x0c: /* SRC and DST */
			destval = srcval & dstdata;
			break;
		case 0x0d: /* SRC and (not DST) */
			destval = srcval & (~dstdata);
			break;
		case 0x0e: /* (not SRC) and DST */
			destval = (~srcval) & dstdata;
			break;
		case 0x0f: /* not (SRC or DST) */
			destval = ~(srcval | dstdata);
			break;
		default:
			LOG_MSG("XGA: GetMixResult: Unknown mix.  Shouldn't be able to get here!");
			break;
	}
	return destval;
}

void XGA_DrawLineVector(Bitu val) {
	Bits xat, yat;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	bool skiplast;
	Bits i;

	Bits dx, sx, sy;

	dx = xga.MAPcount; 
	xat = xga.curx;
	yat = xga.cury;

	switch((val >> 5) & 0x7) {
		case 0x00: /* 0 degrees */
			sx = 1;
			sy = 0;
			break;
		case 0x01: /* 45 degrees */
			sx = 1;
			sy = -1;
			break;
		case 0x02: /* 90 degrees */
			sx = 0;
			sy = -1;
			break;
		case 0x03: /* 135 degrees */
			sx = -1;
			sy = -1;
			break;
		case 0x04: /* 180 degrees */
			sx = -1;
			sy = 0;
			break;
		case 0x05: /* 225 degrees */
			sx = -1;
			sy = 1;
			break;
		case 0x06: /* 270 degrees */
			sx = 0;
			sy = 1;
			break;
		case 0x07: /* 315 degrees */
			sx = 1;
			sy = 1;
			break;
		default:  // Should never get here
			sx = 0;
			sy = 0;
			break;
	}

	/* Do we skip drawing the last pixel? (bit 2), Trio64 documentation.
	 * This is needed to correctly draw polylines in Windows */
	skiplast = (val >> 2) & 1;
	if (skiplast) {
		if (dx > 0) dx--;
		else return;
	}

	for (i=0;i<=dx;i++) {
		Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
		switch (mixmode) {
			case 0x00: /* FOREMIX always used */
				mixmode = xga.foremix;
				switch((mixmode >> 5) & 0x03) {
					case 0x00: /* Src is background color */
						srcval = xga.backcolor;
						break;
					case 0x01: /* Src is foreground color */
						srcval = xga.forecolor;
						break;
					case 0x02: /* Src is pixel data from PIX_TRANS register */
						//srcval = tmpval;
						//LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
						srcval = 0;
						break;
					case 0x03: /* Src is bitmap data */
						LOG_MSG("XGA: DrawRect: Wants data from srcdata");
						//srcval = srcdata;
						srcval = 0;
						break;
					default:
						LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
						srcval = 0;
						break;
				}
				dstdata = XGA_GetPoint((Bitu)xat,(Bitu)yat);

				destval = XGA_GetMixResult(mixmode, srcval, dstdata);

				XGA_DrawPoint((Bitu)xat, (Bitu)yat, destval);
				break;
			default: 
				LOG_MSG("XGA: DrawLine: Needs mixmode %x", (int)mixmode);
				break;
		}
		xat += sx;
		yat += sy;
	}

	xga.curx = (uint16_t)(xat-1);
	xga.cury = (uint16_t)yat;
}

/* NTS: The Windows 3.1 driver does not use this XGA command for horizontal and vertical lines */
void XGA_DrawLineBresenham(Bitu val) {
	Bits xat, yat;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	Bits i;
	Bits tmpswap;
	bool skiplast;
	bool steep;

#define SWAP(a,b) { tmpswap = a; a = b; b = tmpswap; }

	Bits dx, sx, dy, sy, e, dmajor, dminor, destxtmp;

	// Probably a lot easier way to do this, but this works.

	/* S3 Trio64 documentation: The "desty" register is both a destination Y for BitBlt (hence the name)
	 * and "Line Parameter Axial Step Constant" for line drawing, in case the name of the variable is
	 * confusing here. The "desty" variable name exists as inherited from DOSBox SVN source code.
	 *
	 * lpast = 2 * min(abs(dx),abs(dy)) */
	dminor = (Bits)((int16_t)xga.desty);
	if(xga.desty&0x2000) dminor |= ~((Bits)0x1fff);
	dminor >>= 1;

	/* S3 Trio64 documentation: The "destx" register is both a destination X for BitBlt (hence the name)
	 * and "Line Parameter Diagonal Step Constant" for line drawing, in case the name of the variable is
	 * confusing here. The "destx" variable name exists as inherited from DOSBox SVN source code.
	 *
	 * lpdst = 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy)) */
	destxtmp = (Bits)((int16_t)xga.destx);
	if(xga.destx&0x2000) destxtmp |= ~((Bits)0x1fff);

	dmajor = -(destxtmp - (dminor << (Bits)1)) >> (Bits)1;

	dx = dmajor;
	if ((val >> 5) & 0x1)
		sx = 1;
	else
		sx = -1;

	dy = dminor;
	if ((val >> 7) & 0x1)
		sy = 1;
	else
		sy = -1;

	/* Do we skip drawing the last pixel? (bit 2), Trio64 documentation.
	 * This is needed to correctly draw polylines in Windows */
	skiplast = (val >> 2) & 1;

	/* S3 Trio64 documentation:
	 * if x1 < x2: 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy))
	 * if x1 >= x2: 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy)) - 1 */
	e = (Bits)((int16_t)xga.ErrTerm);
	if (xga.ErrTerm&0x2000) e |= ~((Bits)0x1fff); /* sign extend 13-bit error term */

	xat = xga.curx;
	yat = xga.cury;

	if ((val >> 6) & 0x1) {
		steep = false;
		SWAP(xat, yat);
		SWAP(sx, sy);
	} else {
		steep = true;
	}

#if 0
	LOG_MSG("XGA: Bresenham: ASC %ld, LPDSC %ld, sx %ld, sy %ld, err %ld, steep %ld, length %ld, dmajor %ld, dminor %ld, xstart %ld, ystart %ld, skiplast %u",
		(signed long)dx, (signed long)dy, (signed long)sx, (signed long)sy, (signed long)e,
		(signed long)steep, (signed long)xga.MAPcount, (signed long)dmajor, (signed long)dminor,
		(signed long)xat, (signed long)yat, skiplast?1:0);
#endif

	const Bits run = xga.MAPcount - (skiplast ? 1 : 0);

	for (i=0;i<=run;i++) {
		Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;

		switch (mixmode) {
			case 0x00: /* FOREMIX always used */
				mixmode = xga.foremix;
				switch((mixmode >> 5) & 0x03) {
					case 0x00: /* Src is background color */
						srcval = xga.backcolor;
						break;
					case 0x01: /* Src is foreground color */
						srcval = xga.forecolor;
						break;
					case 0x02: /* Src is pixel data from PIX_TRANS register */
						//srcval = tmpval;
						LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
						srcval = 0;
						break;
					case 0x03: /* Src is bitmap data */
						LOG_MSG("XGA: DrawRect: Wants data from srcdata");
						//srcval = srcdata;
						srcval = 0;
						break;
					default:
						LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
						srcval = 0;
						break;
				}

				if (steep)
					dstdata = XGA_GetPoint((Bitu)xat,(Bitu)yat);
				else
					dstdata = XGA_GetPoint((Bitu)yat,(Bitu)xat);

				destval = XGA_GetMixResult(mixmode, srcval, dstdata);

				if (steep)
					XGA_DrawPoint((Bitu)xat,(Bitu)yat, destval);
				else
					XGA_DrawPoint((Bitu)yat,(Bitu)xat, destval);

				break;
			default: 
				LOG_MSG("XGA: DrawLine: Needs mixmode %x", (int)mixmode);
				break;
		}

		while (e > 0) {
			yat += sy;
			e -= (dx << 1);
		}

		xat += sx;
		e += (dy << 1);
	}

	if (steep) {
		xga.curx = (uint16_t)xat;
		xga.cury = (uint16_t)yat;
	} else {
		xga.curx = (uint16_t)yat;
		xga.cury = (uint16_t)xat;
	}
#undef SWAP
}

void XGA_DrawRectangle(Bitu val) {
	uint32_t xat, yat, xrun;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	bool skiplast;

	Bits srcx, srcy, dx, dy;

	skiplast = (val >> 2) & 1;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcy = xga.cury;

	/* Undocumented, but seen with Windows 3.1 drivers: Horizontal lines are drawn with this XGA command and "skip last pixel" set, else they are one pixel too wide */
	xrun = xga.MAPcount;
	if (skiplast) {
		if (xrun > 0u) xrun--;
		else return;
	}

	for(yat=0;yat<=xga.MIPcount;yat++) {
		srcx = xga.curx;
		for(xat=0;xat<=xrun;xat++) {
			Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
			switch (mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					switch((mixmode >> 5) & 0x03) {
						case 0x00: /* Src is background color */
							srcval = xga.backcolor;
							break;
						case 0x01: /* Src is foreground color */
							srcval = xga.forecolor;
							break;
						case 0x02: /* Src is pixel data from PIX_TRANS register */
							//srcval = tmpval;
							LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
							srcval = 0;
							break;
						case 0x03: /* Src is bitmap data */
							LOG_MSG("XGA: DrawRect: Wants data from srcdata");
							//srcval = srcdata;
							srcval = 0;
							break;
						default:
							LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
							srcval = 0;
							break;
					}
					dstdata = XGA_GetPoint((Bitu)srcx,(Bitu)srcy);

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

					XGA_DrawPoint((Bitu)srcx,(Bitu)srcy, destval);
					break;
				default: 
					LOG_MSG("XGA: DrawRect: Needs mixmode %x", (int)mixmode);
					break;
			}
			srcx += dx;
		}
		srcy += dy;
	}
	xga.curx = (uint16_t)srcx;
	xga.cury = (uint16_t)srcy;

	//LOG_MSG("XGA: Draw rect (%d, %d)-(%d, %d), %d", x1, y1, x2, y2, xga.forecolor);
}

bool XGA_CheckX(void) {
	bool newline = false;
	if(!xga.waitcmd.newline) {
	
	if((xga.waitcmd.curx<2048) && xga.waitcmd.curx > (xga.waitcmd.x2)) {
		xga.waitcmd.curx = xga.waitcmd.x1;
		xga.waitcmd.cury += xga.waitcmd.curdy;
		xga.waitcmd.cury&=0x0fff;
		newline = true;
		xga.waitcmd.newline = true;
		if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
			xga.waitcmd.wait = false;
	} else if(xga.waitcmd.curx>=2048) {
		uint16_t realx = 4096-xga.waitcmd.curx;
		if(xga.waitcmd.x2>2047) { // x end is negative too
			uint16_t realxend=4096-xga.waitcmd.x2;
			if(realx==realxend) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury += xga.waitcmd.curdy;
				xga.waitcmd.cury&=0x0fff;
				newline = true;
				xga.waitcmd.newline = true;
				if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
					xga.waitcmd.wait = false;
			}
		} else { // else overlapping
			if(realx==xga.waitcmd.x2) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury += xga.waitcmd.curdy;
				xga.waitcmd.cury&=0x0fff;
				newline = true;
				xga.waitcmd.newline = true;
				if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
					xga.waitcmd.wait = false;
				}
			}
		}
	} else {
		xga.waitcmd.newline = false;
	}
	return newline;
}

void XGA_DrawWaitSub(Bitu mixmode, Bitu srcval) {
	Bitu destval;
	Bitu dstdata;
	dstdata = XGA_GetPoint(xga.waitcmd.curx, xga.waitcmd.cury);
	destval = XGA_GetMixResult(mixmode, srcval, dstdata);
	//LOG_MSG("XGA: DrawPattern: Mixmode: %x srcval: %x", mixmode, srcval);

	XGA_DrawPoint(xga.waitcmd.curx, xga.waitcmd.cury, destval);
	xga.waitcmd.curx += xga.waitcmd.curdx;
	xga.waitcmd.curx&=0x0fff;
	XGA_CheckX();
}

void XGA_DrawWait(Bitu val, Bitu len) {
	if (s3Card >= S3_ViRGE) {
		if (xga.virge.imgxferport != NULL && xga.virge.imgxferportfunc != NULL)
			xga.virge.imgxferportfunc((uint32_t)val);
	}

	if(!xga.waitcmd.wait) return;
	Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
	Bitu srcval;

	// 86C928/Trio32/Trio64/Trio64V+ byte swap option
	if (xga.waitcmd.bswap16 && len >= 2)
		val = ((val & 0xFF00FF00u) >> 8u) | ((val & 0x00FF00FFu) << 8u);

	switch(xga.waitcmd.cmd) {
		case 2: /* Rectangle */
			switch(mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;

/*					switch((mixmode >> 5) & 0x03) {
						case 0x00: // Src is background color
							srcval = xga.backcolor;
							break;
						case 0x01: // Src is foreground color
							srcval = xga.forecolor;
							break;
						case 0x02: // Src is pixel data from PIX_TRANS register
*/
					if(((mixmode >> 5) & 0x03) != 0x2) {
						// those cases don't seem to occur
						LOG_MSG("XGA: unsupported drawwait operation");
						break;
					}
					switch(xga.waitcmd.buswidth) {
						case M_LIN8:		//  8 bit
							XGA_DrawWaitSub(mixmode, val);
							break;
						case 0x20 | M_LIN8: // 16 bit 
							for(Bitu i = 0; i < len; i++) {
								XGA_DrawWaitSub(mixmode, (val>>(8*i))&0xff);
								if(xga.waitcmd.newline) break;
							}
							break;
						case 0x40 | M_LIN8: // 32 bit
							for(int i = 0; i < 4; i++)
								XGA_DrawWaitSub(mixmode, (val>>(8*i))&0xff);
							break;
						case (0x20 | M_LIN32):
							if(len!=4) { // In case of two 16-bit transfers, first combine both WORDs into a 32-bit DWORD and then operate
								// Needed for Windows 3.1 with S386c928 drivers, and Windows NT/2000 to display 16x16 radio buttons and icons properly
								if(xga.waitcmd.datasize == 0) {
									// set it up to wait for the next word
									xga.waitcmd.data = (uint32_t)val;
									xga.waitcmd.datasize = 2;
									return;
								} else {
									srcval = (val<<16)|xga.waitcmd.data;
									xga.waitcmd.data = 0;
									xga.waitcmd.datasize = 0;
									XGA_DrawWaitSub(mixmode, srcval);
								}
								break;
							} // fall-through
						case 0x40 | M_LIN32: // 32 bit
							XGA_DrawWaitSub(mixmode, val);
							break;
						case 0x20 | M_LIN15: // 16 bit 
						case 0x20 | M_LIN16: // 16 bit 
							XGA_DrawWaitSub(mixmode, val);
							break;
						case 0x40 | M_LIN15: // 32 bit 
						case 0x40 | M_LIN16: // 32 bit 
							XGA_DrawWaitSub(mixmode, val&0xffff);
							if(!xga.waitcmd.newline)
								XGA_DrawWaitSub(mixmode, val>>16);
							break;
						default:
							// Let's hope they never show up ;)
							LOG_MSG("XGA: unsupported bpp / datawidth combination %x",
								(int)xga.waitcmd.buswidth);
							break;
					}
					break;
			
				case 0x02: // Data from PIX_TRANS selects the mix
					Bitu chunksize;
					Bitu chunks;
					switch(xga.waitcmd.buswidth&0x60) {
						case 0x0: // 8 bit
							chunksize=8;
							chunks=1;
							break;
						case 0x20: // 16 bit
							chunksize=16;
							if(len==4) chunks=2;
							else chunks=1;
							break;
						case 0x40: // 32 bit
							chunksize=32;
							chunks=1;
							break;
						case 0x60: // 32 bits, byte alignment
							chunksize=8;
							chunks=len;
							break;
						default:
							chunksize=0;
							chunks=0;
							break;
					}

					for(Bitu k = 0; k < chunks; k++) { // chunks counter
						xga.waitcmd.newline = false;
						for(Bitu n = 0; n < chunksize; n++) { // pixels
							// This formula can rule the world ;)
							Bitu mask = (Bitu)1ul << (Bitu)((((n&0xF8u)+(8u-(n&0x7u)))-1u)+chunksize*k);
							if(val&mask) mixmode = xga.foremix;
							else mixmode = xga.backmix;
							
							switch((mixmode >> 5) & 0x03) {
								case 0x00: // Src is background color
									srcval = xga.backcolor;
									break;
								case 0x01: // Src is foreground color
									srcval = xga.forecolor;
									break;
								default:
									LOG_MSG("XGA: DrawBlitWait: Unsupported src %x",
										(int)((mixmode >> 5) & 0x03));
									srcval=0;
									break;
							}
							XGA_DrawWaitSub(mixmode, srcval);

							if((xga.waitcmd.cury<2048) &&
							   (xga.waitcmd.cury >= xga.waitcmd.y2)) {
								xga.waitcmd.wait = false;
								k=1000; // no more chunks
								break;
							}
							// next chunk goes to next line
							if(xga.waitcmd.newline) break; 
						} // pixels loop
					} // chunks loop
					break;

				default:
					LOG_MSG("XGA: DrawBlitWait: Unhandled mixmode: %d", (int)mixmode);
					break;
			} // switch mixmode
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", (int)xga.waitcmd.cmd);
			break;
	}
}

void XGA_BlitRect(Bitu val) {
	uint32_t xat, yat;
	Bitu srcdata;
	Bitu dstdata;
	Bitu colorcmpdata;

	Bitu srcval;
	Bitu destval;

	Bits srcx, srcy, tarx, tary, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	colorcmpdata = xga.color_compare & XGA_PointMask();

	srcx = xga.curx;
	srcy = xga.cury;
	tarx = xga.destx;
	tary = xga.desty;

	Bitu mixselect = (xga.pix_cntl >> 6) & 0x3;
	Bitu mixmode = 0x67; /* Source is bitmap data, mix mode is src */
	switch(mixselect) {
		case 0x00: /* Foreground mix is always used */
			mixmode = xga.foremix;
			break;
		case 0x02: /* CPU Data determines mix used */
			LOG_MSG("XGA: DrawPattern: Mixselect data from PIX_TRANS register");
			break;
		case 0x03: /* Video memory determines mix */
			//LOG_MSG("XGA: Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x", srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
			break;
		default:
			LOG_MSG("XGA: BlitRect: Unknown mix select register");
			break;
	}


	/* Copy source to video ram */
	for(yat=0;yat<=xga.MIPcount ;yat++) {
		srcx = xga.curx;
		tarx = xga.destx;

		for(xat=0;xat<=xga.MAPcount;xat++) {
			srcdata = XGA_GetPoint((Bitu)srcx, (Bitu)srcy);
			dstdata = XGA_GetPoint((Bitu)tarx, (Bitu)tary);

			if(mixselect == 0x3) {
				/* Explanation in XGA_DrawPattern */
				if ((srcdata&xga.readmask) == xga.readmask)
					mixmode = xga.foremix;
				else
					mixmode = xga.backmix;
			}

			switch((mixmode >> 5) & 0x03) {
				case 0x00: /* Src is background color */
					srcval = xga.backcolor;
					break;
				case 0x01: /* Src is foreground color */
					srcval = xga.forecolor;
					break;
				case 0x02: /* Src is pixel data from PIX_TRANS register */
					LOG_MSG("XGA: DrawPattern: Wants data from PIX_TRANS register");
					srcval = 0;
					break;
				case 0x03: /* Src is bitmap data */
					srcval = srcdata;
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Shouldn't be able to get here!");
					srcval = 0;
					break;
			}

			bool doit = true;

			/* For more information, see the "S3 Vision864 Graphics Accelerator" datasheet
			 * [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision864%20Graphics%20Accelerator%20(1994-10).pdf]
			 * Page 203 for "Multifunction Control Miscellaneous Register (MULT_MISC)" which this code holds as xga.control1, and
			 * Page 198 for "Color Compare Register (COLOR_CMP)" which this code holds as xga.color_compare. */
			if (xga.control1 & 0x100) { /* COLOR_CMP enabled. control1 corresponds to XGA register BEE8h */
				/* control1 bit 7 is SRC_NE.
				 * If clear, don't update if source value == COLOR_CMP.
				 * If set, don't update if source value != COLOR_CMP */
				doit = !!(((srcval == colorcmpdata)?0:1)^((xga.control1>>7u)&1u));
			}

			if (doit) {
				destval = XGA_GetMixResult(mixmode, srcval, dstdata);
				//LOG_MSG("XGA: DrawPattern: Mixmode: %x Mixselect: %x", mixmode, mixselect);

				XGA_DrawPoint((Bitu)tarx, (Bitu)tary, destval);
			}

			srcx += dx;
			tarx += dx;
		}
		srcy += dy;
		tary += dy;
	}
}

void XGA_DrawPattern(Bitu val) {
	Bitu srcdata;
	Bitu dstdata;

	Bitu srcval;
	Bitu destval;

	Bits xat, yat, srcx, srcy, tary, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcx = xga.curx;
	srcy = xga.cury;

	tary = xga.desty;

	Bitu mixselect = (xga.pix_cntl >> 6) & 0x3;
	Bitu mixmode = 0x67; /* Source is bitmap data, mix mode is src */
	switch(mixselect) {
		case 0x00: /* Foreground mix is always used */
			mixmode = xga.foremix;
			break;
		case 0x02: /* CPU Data determines mix used */
			LOG_MSG("XGA: DrawPattern: Mixselect data from PIX_TRANS register");
			break;
		case 0x03: /* Video memory determines mix */
			//LOG_MSG("XGA: Pixctl: %x, Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x",xga.pix_cntl, srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
			break;
		default:
			LOG_MSG("XGA: DrawPattern: Unknown mix select register");
			break;
	}

	for(yat=0;yat<=xga.MIPcount;yat++) {
		Bits tarx = xga.destx;
		for(xat=0;xat<=xga.MAPcount;xat++) {

			srcdata = XGA_GetPoint((Bitu)srcx + (tarx & 0x7), (Bitu)srcy + (tary & 0x7));
			//LOG_MSG("patternpoint (%3d/%3d)v%x",srcx + (tarx & 0x7), srcy + (tary & 0x7),srcdata);
			dstdata = XGA_GetPoint((Bitu)tarx, (Bitu)tary);
			

			if (mixselect == 0x3) {
				/* S3 Trio32/Trio64 Integrated Graphics Accelerators, section 13.2 Bitmap Access Through The Graphics Engine.
				 *
				 * [https://jon.nerdgrounds.com/jmcs/docs/browse/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]
				 *
				 * "If bits 7-6 are set to 11b, the current display bit map is selected as the mask bit source. The Read Mask"
				 * "register (AAE8H) is set up to indicate the active planes. When all bits of the read-enabled planes for a"
				 * "pixel are a 1, the mask bit 'ONE' is generated. If anyone of the read-enabled planes is a 0, then a mask"
				 * "bit 'ZERO' is generated. If the mask bit is 'ONE', the Foreground Mix register is used. If the mask bit is"
				 * "'ZERO', the Background Mix register is used."
				 *
				 * Notice that when an application in Windows 3.1 draws a black rectangle, I see foreground=0 background=ff
				 * and in this loop, srcdata=ff and readmask=ff. While the original DOSBox SVN "guess" code here would
				 * misattribute that to the background color (and erroneously draw a white rectangle), what should actually
				 * happen is that we use the foreground color because (srcdata&readmask)==readmask (all bits 1).
				 *
				 * This fixes visual bugs when running Windows 3.1 and Microsoft Creative Writer, and navigating to the
				 * basement and clicking around in the dark to reveal funny random things, leaves white rectangles on the
				 * screen where the image was when you released the mouse. Creative Writer clears the image by drawing a
				 * BLACK rectangle, while the DOSBox SVN "guess" mistakenly chose the background color and therefore a
				 * WHITE rectangle. */
				if ((srcdata&xga.readmask) == xga.readmask)
					mixmode = xga.foremix;
				else
					mixmode = xga.backmix;
			}

			switch((mixmode >> 5) & 0x03) {
				case 0x00: /* Src is background color */
					srcval = xga.backcolor;
					break;
				case 0x01: /* Src is foreground color */
					srcval = xga.forecolor;
					break;
				case 0x02: /* Src is pixel data from PIX_TRANS register */
					LOG_MSG("XGA: DrawPattern: Wants data from PIX_TRANS register");
					srcval = 0;
					break;
				case 0x03: /* Src is bitmap data */
					srcval = srcdata;
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Shouldn't be able to get here!");
					srcval = 0;
					break;
			}

			destval = XGA_GetMixResult(mixmode, srcval, dstdata);

			XGA_DrawPoint((Bitu)tarx, (Bitu)tary, destval);
			
			tarx += dx;
		}
		tary += dy;
	}
}

void XGA_DrawCmd(Bitu val, Bitu len) {
    (void)len;//UNUSED
	uint16_t cmd;
	cmd = (uint16_t)(val >> 13ul);
	if (val & 0x800) cmd |= 0x8u; // S3 CMD bit 3
#if XGA_SHOW_COMMAND_TRACE == 1
	//LOG_MSG("XGA: Draw command %x", cmd);
#endif
	xga.curcommand = val;
	switch(cmd) {
		case 1: /* Draw line */
			if((val & 0x100) == 0) {
				if((val & 0x8) == 0) {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing Bresenham line");
#endif
					XGA_DrawLineBresenham(val);
				} else {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing vector line");
#endif
					XGA_DrawLineVector(val);
				}
			} else {
				LOG_MSG("XGA: Wants line drawn from PIX_TRANS register!");
			}
			break;
		case 2: /* Rectangle fill */
			if((val & 0x100) == 0) {
				xga.waitcmd.wait = false;
#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw immediate rect: xy(%3d/%3d), len(%3d/%3d)",
					xga.curx,xga.cury,xga.MAPcount,xga.MIPcount);
#endif
				XGA_DrawRectangle(val);

			} else {
				
				xga.waitcmd.newline = true;
				xga.waitcmd.wait = true;
				xga.waitcmd.curx = xga.curx;
				xga.waitcmd.cury = xga.cury;

				/* Windows will always use 101b (+X +Y) to draw left to right, top to bottom.
				 * Apparently there is an MS-DOS CAD program out there that wants to draw
				 * (+X -Y) left to right, bottom to top. */
				xga.waitcmd.curdx = ((val>>5)&1) ? 1 : uint16_t(~0u)/*equiv -1*/;
				xga.waitcmd.curdy = ((val>>7)&1) ? 1 : uint16_t(~0u)/*equiv -1*/;

				xga.waitcmd.x1 = xga.curx;
				xga.waitcmd.y1 = xga.cury;
				xga.waitcmd.x2 = (uint16_t)((xga.curx + xga.MAPcount)&0x0fff);
				xga.waitcmd.y2 = (uint16_t)((xga.cury + xga.MIPcount + 1)&0x0fff);
				xga.waitcmd.sizex = xga.MAPcount;
				xga.waitcmd.sizey = xga.MIPcount + 1;
				xga.waitcmd.cmd = 2;
				if (s3Card >= S3_Vision864) /* this is when bit 10 became part of bit 9 to allow BUS SIZE == 32 (2-bit value == binary 10 == decimal 2) */
					xga.waitcmd.buswidth = (Bitu)vga.mode | (Bitu)((val&0x600u) >> 4u);
				else /* S3 86C928 datasheet lists bit 10 as reserved, therefore BUS WIDTH can only be 8 or 16 */
					xga.waitcmd.buswidth = (Bitu)vga.mode | (Bitu)((val&0x200u) >> 4u);
				xga.waitcmd.data = 0;
				xga.waitcmd.datasize = 0;

				if (s3Card < S3_ViRGE) /* NTS: ViRGE datasheets do not mention port 9AE8H except in passing, maybe it was dropped? FIXME: How does this work with BUS SIZE == 32? */
					xga.waitcmd.bswap16 = (val&0x1200u) == 0x0200u; // BYTE SWP(12):  0=High byte first (big endian)  1=Low byte first (little endian)  and BUS SIZE  1=16-bit  0-8-bit
				else
					xga.waitcmd.bswap16 = false; // we're little endian, dammit!

#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw wait rect, w/h(%3d/%3d), x/y1(%3d/%3d), x/y2(%3d/%3d), %4x",
					xga.MAPcount+1, xga.MIPcount+1,xga.curx,xga.cury,
					(xga.curx + xga.MAPcount)&0x0fff,
					(xga.cury + xga.MIPcount + 1)&0x0fff,val&0xffff);
#endif
			
			}
			break;
		case 3: /* Polygon fill (Trio64) */
			if (s3Card == S3_Trio64) {
#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Polygon fill (Trio64)");
#endif
				/* From the datasheet [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]
				 * Section 13.3.3.12 Polygon Fill Solid (Trio64 only)
				 *
				 * The idea is that there are two current/dest X/Y pairs and this command
				 * is used to draw the polygon top to bottom as a series of trapezoids,
				 * sending new x/y coordinates for each left or right edge as the polygon
				 * continues. The acceleration function is described as rendering to the
				 * minimum of the two Y coordinates, and stopping. One side or the other
				 * is updated, and the command starts the new edge and continues the other
				 * edge.
				 *
				 * The card requires that the first and last segments have equal Y values,
				 * though not X values in order to allow polygons with flat top and/or
				 * bottom.
				 *
				 * That would imply that there's some persistent error term here, and it
				 * would also imply that the card updates current Y position to the minimum
				 * of either side so the new coordinates continue properly.
				 *
				 * NTS: The Windows 3.1 Trio64 driver likes to send this command every single
				 *      time it updates any coordinate, contrary to the Trio64 datasheet that
				 *      suggests setting cur/dest X/Y and cur2/dest2 X/Y THEN sending this
				 *      command, then setting either dest X/Y and sending the command until
				 *      the polygon has been rasterized. We can weed those out here by ignoring
				 *      any command where the cur/dest Y coordinates would result in no movement.
				 *
				 *      The Windows 3.1 driver also seems to use cur/dest X/Y for the RIGHT side,
				 *      and cur2/dest2 X/Y for the LEFT side, which is completely opposite from
				 *      the example given in the datasheet. This also implies that whatever order
				 *      the vertices end up, they draw a span when rasterizing, and the sides can
				 *      cross one another if necessary.
				 *
				 * NTS: You can test this code by bringing up Paintbrush, and drawing with the
				 *      brush tool. Despite drawing a rectangle, the S3 Trio64 driver uses the
				 *      Polygon fill command to draw it.
				 *
				 *      More testing is possible in Microsoft Word 2.0 using the shapes/graphics
				 *      editor, adding solid rectangles or rounded rectangles (but not circles).
				 *
				 * Vertex at (*)
				 *
				 *                       *             *     *
				 *                       +             +-----+
				 *                      / \           /       \
				 *                     /   \         /         \
				 *                    /_____\ *     /___________\ *
				 *                   /      /      /            |
				 *                * /______/    * /_____________|
				 *                  \     /       \             |
				 *                   \   /         \            |
				 *                    \ /           \           |
				 *                     +             \__________|
				 *                     *             *          *
				 *
				 * Windows 3.1 driver behavior suggests this is also possible?
				 *
				 *                   *
				 *                  / \
				 *                 /   \
				 *                /     \
				 *             * /_______\
				 *               \________\ *
				 *                \       /
				 *                 \     /
				 *                  \   /
				 *                   \ /
				 *                    X      <- crossover point
				 *                   / \
				 *                  /   \
				 *               * /_____\
				 *                 \      \
				 *                  \______\
				 *                  *       *
				 */

				if (xga.cury < xga.desty && xga.cury2 < xga.desty2) {
					LOG(LOG_MISC,LOG_DEBUG)("Trio64 Polygon fill: leftside=(%d,%d)-(%d,%d) rightside=(%d,%d)-(%d,%d)",
						xga.curx, xga.cury, xga.destx, xga.desty,
						xga.curx2,xga.cury2,xga.destx2,xga.desty2);

					// Not quite accurate, good enough for now.
					xga.curx = xga.destx;
					xga.cury = xga.desty;
					xga.curx2 = xga.destx2;
					xga.cury2 = xga.desty2;
				}
				else {
					LOG(LOG_MISC,LOG_DEBUG)("Trio64 Polygon fill (nothing done)");

					// Windows 3.1 Trio64 driver behavior suggests that if Y doesn't move,
					// the X coordinate may change if cur Y == dest Y, else the result
					// when actual rendering doesn't make sense.
					if (xga.cury == xga.desty) xga.curx = xga.destx;
					if (xga.cury2 == xga.desty2) xga.curx2 = xga.destx2;
				}
			}
			break;
		case 6: /* BitBLT */
#if XGA_SHOW_COMMAND_TRACE == 1
			LOG_MSG("XGA: Blit Rect");
#endif
			XGA_BlitRect(val);
			break;
		case 7: /* Pattern fill */
#if XGA_SHOW_COMMAND_TRACE == 1
			LOG_MSG("XGA: Pattern fill: src(%3d/%3d), dest(%3d/%3d), fill(%3d/%3d)",
				xga.curx,xga.cury,xga.destx,xga.desty,xga.MAPcount,xga.MIPcount);
#endif
			XGA_DrawPattern(val);
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", cmd);
			break;
	}
}

void XGA_SetDualReg(uint32_t& reg, Bitu val) {
	switch(XGA_COLOR_MODE) {
	case M_LIN4:
		reg = (uint8_t)(val&0xf); break;
	case M_LIN8:
		reg = (uint8_t)(val&0xff); break;
	case M_LIN15:
	case M_LIN16:
		reg = (uint16_t)(val&0xffff); break;
	case M_LIN32:
		if (xga.control1 & 0x200)
			reg = (uint32_t)val;
		else if (xga.control1 & 0x10)
			reg = (reg&0x0000ffff)|((uint32_t)(val<<16));
		else
			reg = (reg&0xffff0000)|((uint32_t)(val&0x0000ffff));
		xga.control1 ^= 0x10;
		break;
	default:
		break;
	}
}

Bitu XGA_GetDualReg(uint32_t reg) {
	switch(XGA_COLOR_MODE) {
	case M_LIN4:
		return (uint8_t)(reg&0xf);
	case M_LIN8:
		return (uint8_t)(reg&0xff);
	case M_LIN15: case M_LIN16:
		return (uint16_t)(reg&0xffff);
	case M_LIN32:
		if (xga.control1 & 0x200) return reg;
		xga.control1 ^= 0x10;
		if (xga.control1 & 0x10) return reg&0x0000ffff;
		else return reg>>16;
	default:
		break;
	}
	return 0;
}

extern Bitu vga_read_p3da(Bitu port,Bitu iolen);

extern void vga_write_p3d4(Bitu port,Bitu val,Bitu iolen);
extern Bitu vga_read_p3d4(Bitu port,Bitu iolen);

extern void vga_write_p3d5(Bitu port,Bitu val,Bitu iolen);
extern Bitu vga_read_p3d5(Bitu port,Bitu iolen);

uint32_t XGA_MixVirgePixel(uint32_t srcpixel,uint32_t patpixel,uint32_t dstpixel,uint8_t rop) {
	switch (rop) {
		/* S3 ViRGE Integrated 3D Accelerator Appendix A Listing of Raster Operations */
		case 0x00/*0           */: return 0;
		case 0x0A/*DPna        */: return (~patpixel) & dstpixel;
		case 0x22/*DSna        */: return (~srcpixel) & dstpixel;
		case 0x55/*Dn          */: return ~dstpixel;
		case 0x5A/*DPx         */: return dstpixel ^ patpixel;
		case 0x66/*DSx         */: return dstpixel ^ srcpixel;
		case 0x69/*PDSxxn      */: return ~(srcpixel ^ dstpixel ^ patpixel);
		case 0x88/*DSa         */: return dstpixel & srcpixel;
		case 0xA5/*PDxn        */: return ~(patpixel ^ dstpixel);
		case 0xAA/*D           */: return dstpixel;
		case 0xB8/*PSDPxax     */: return ((dstpixel ^ patpixel) & srcpixel) ^ patpixel;
		case 0xBB/*DSno        */: return (~srcpixel) | dstpixel;
		case 0xC0/*PSa         */: return patpixel & srcpixel;
		case 0xCC/*S           */: return srcpixel;
		case 0xE2/*DSPDxax     */: return ((patpixel ^ dstpixel) & srcpixel) ^ dstpixel;
		case 0xEE/*DSo         */: return dstpixel | srcpixel;
		case 0xF0/*P           */: return patpixel;
		case 0xFF/*1           */: return 0xFFFFFFFF;
		default:
			LOG_MSG("ViRGE ROP %02x unimpl",(unsigned int)rop);
			break;
	};

	return srcpixel;
}

uint32_t XGA_VirgePatPixelMono(unsigned int x,unsigned int y) {
	const uint8_t rb = ((unsigned char*)(&xga.virge.bitblt.mono_pat))[y&7]; /* WARNING: Only works on little Endian CPUs */
	return (rb & (0x80 >> (x & 7))) ? xga.virge.bitblt.mono_pat_fgcolor : xga.virge.bitblt.mono_pat_bgcolor;
}

uint32_t XGA_VirgePatPixel(unsigned int x,unsigned int y) {
	switch((xga.virge.bitblt.command_set >> 2u) & 7u) {
		case 0: // 8 bit/pixel
			return xga.virge.colorpat.pat8[((y&7u)<<3u)+(x&7u)];
		case 1: // 16 bits/pixel
			return xga.virge.colorpat.pat16[((y&7u)<<3u)+(x&7u)];
		case 2: // 24/32 bits/pixel
			return *((uint32_t*)(&xga.virge.colorpat.pat8[(((y&7u)<<3u)+(x&7u))*3u])) & 0xFFFFFF;
		default:
			break;
	}

	return 0;
}

uint32_t XGA_ReadSourceVirgePixel(XGAStatus::XGA_VirgeState::reggroup &rset,unsigned int x,unsigned int y) {
	uint32_t memaddr;

	switch((rset.command_set >> 2u) & 7u) {
		case 0: // 8 bit/pixel
			memaddr = (uint32_t)((y * rset.src_stride) + x) + rset.src_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return vga.mem.linear[memaddr];
			break;
		case 1: // 16 bits/pixel
			memaddr = (uint32_t)((y * rset.src_stride) + (x*2)) + rset.src_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return *((uint16_t*)(vga.mem.linear+memaddr));
			break;
		case 2: // 24/32 bits/pixel
			memaddr = (uint32_t)((y * rset.src_stride) + (x*xga.virge.truecolor_bypp)) + rset.src_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return *((uint32_t*)(vga.mem.linear+memaddr)) & xga.virge.truecolor_mask;
			break;
		default:
			break;
	}

	return 0;
}

uint32_t XGA_ReadDestVirgePixel(XGAStatus::XGA_VirgeState::reggroup &rset,unsigned int x,unsigned int y) {
	uint32_t memaddr;

	switch((rset.command_set >> 2u) & 7u) {
		case 0: // 8 bit/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + x) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return vga.mem.linear[memaddr];
			break;
		case 1: // 16 bits/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + (x*2)) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return *((uint16_t*)(vga.mem.linear+memaddr));
			break;
		case 2: // 24/32 bits/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + (x*xga.virge.truecolor_bypp)) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			return *((uint32_t*)(vga.mem.linear+memaddr)) & xga.virge.truecolor_mask;
			break;
		default:
			break;
	}

	return 0;
}

void XGA_DrawVirgePixel(XGAStatus::XGA_VirgeState::reggroup &rset,unsigned int x,unsigned int y,uint32_t c) {
	uint32_t memaddr;

	if (!(rset.command_set & 0x20)) return; /* bit 5 draw enable == 0 means don't update screen */

	/* Need to zero out all unused bits in modes that have any (15-bit or "32"-bit -- the last
	   one is actually 24-bit. Without this step there may be some graphics corruption (mainly,
	   during windows dragging. */
	switch((rset.command_set >> 2u) & 7u) {
		case 0: // 8 bit/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + x) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			vga.mem.linear[memaddr] = (uint8_t)c;
			break;
		case 1: // 16 bits/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + (x*2)) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			*((uint16_t*)(vga.mem.linear+memaddr)) = (uint16_t)(c&0xffff);
			break;
		case 2: // 24/32 bits/pixel
			memaddr = (uint32_t)((y * rset.dst_stride) + (x*xga.virge.truecolor_bypp)) + rset.dst_base;
			if (GCC_UNLIKELY(memaddr >= vga.mem.memsize)) break;
			if (xga.virge.truecolor_mask == 0xFFFFFFFFu) {
				*((uint32_t*)(vga.mem.linear+memaddr)) = (uint32_t)c;
			}
			else {
				vga.mem.linear[memaddr+0] = (uint8_t)c;
				vga.mem.linear[memaddr+1] = (uint8_t)(c >> 8u);
				vga.mem.linear[memaddr+2] = (uint8_t)(c >> 16u);
			}
			break;
		default:
			break;
	}
}

inline void XGA_DrawVirgePixelCR(XGAStatus::XGA_VirgeState::reggroup &rset,unsigned int x,unsigned int y,uint32_t c) {
	if (rset.command_set & 2) { /* clip enable */
		if (x >= rset.left_clip && x <= rset.right_clip && y >= rset.top_clip && y <= rset.bottom_clip)
			XGA_DrawVirgePixel(rset,x,y,c);
	}
	else {
		XGA_DrawVirgePixel(rset,x,y,c);
	}
}

void XGA_ViRGE_BitBlt_xferport(uint32_t val) {
	uint32_t srcpixel,mixpixel,dstpixel,patpixel;
	uint8_t valbytes = 4;
	unsigned int x,y;
	uint8_t msk;

//	LOG_MSG("BitBlt write %08x",(unsigned int)val);

	if (xga.virge.bitbltstate.itf_buffer_initskip > 0) {
		assert(valbytes >= xga.virge.bitbltstate.itf_buffer_initskip);
		valbytes -= xga.virge.bitbltstate.itf_buffer_initskip;
		val >>= (uint64_t)(8u * xga.virge.bitbltstate.itf_buffer_initskip);
		xga.virge.bitbltstate.itf_buffer_initskip = 0;
	}

	assert(xga.virge.bitbltstate.itf_buffer_bytecount <= 4u); /* we should be flushing data */

	if (xga.virge.bitbltstate.itf_buffer_bytecount > 0) {
		xga.virge.bitbltstate.itf_buffer |= ((uint64_t)val) << (8u * xga.virge.bitbltstate.itf_buffer_bytecount);
		xga.virge.bitbltstate.itf_buffer_bytecount += valbytes;
	}
	else {
		xga.virge.bitbltstate.itf_buffer = (uint64_t)val;
		xga.virge.bitbltstate.itf_buffer_bytecount = valbytes;
	}

	/* shifted in, write it out now */
	x = xga.virge.bitblt.rect_dst_x;
	y = xga.virge.bitblt.rect_dst_y;

	if (xga.virge.imgxferport->command_set & 0x40) {
		/* mono image bitmap */
		assert(xga.virge.bitbltstate.src_xrem != 0);
		while (xga.virge.bitbltstate.itf_buffer_bytecount > 0) {
#if 0
			LOG_MSG("BitBlt mono t=%u x=%u y=%u srm=%u/%u sw=%u patb=%02x srcb=%02x ctrl=%08x",
				(xga.virge.imgxferport->command_set & 0x200) ? 1 : 0,
				x,y,xga.virge.bitbltstate.src_xrem,xga.virge.bitbltstate.src_stride,
				xga.virge.bitblt.rect_width,
				pb,(uint8_t)xga.virge.bitbltstate.itf_buffer & 0xFFu,
				xga.virge.bitblt.command_set);
#endif

			msk = 0x80u;
			if (xga.virge.imgxferport->command_set & 0x100) { /* mono pattern, mono bitmap */
				if (xga.virge.imgxferport->command_set & 0x200) { /* transparent */
					do {
						if (xga.virge.bitbltstate.src_drem > 0) {
							if ((uint8_t)xga.virge.bitbltstate.itf_buffer & msk) {
								srcpixel = xga.virge.bitblt.src_fgcolor;
								dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
								patpixel = XGA_VirgePatPixelMono(x,y);
								mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
								XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
							}
							xga.virge.bitbltstate.src_drem--;
						}

						msk >>= 1u;
						x++;
					} while (msk != 0u);
				}
				else {
					do {
						if (xga.virge.bitbltstate.src_drem > 0) {
							srcpixel = ((uint8_t)xga.virge.bitbltstate.itf_buffer & msk) ? xga.virge.bitblt.src_fgcolor : xga.virge.bitblt.src_bgcolor;
							dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
							patpixel = XGA_VirgePatPixelMono(x,y);
							mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
							XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
							xga.virge.bitbltstate.src_drem--;
						}

						msk >>= 1u;
						x++;
					} while (msk != 0u);
				}
			}
			else { /* color pattern, mono bitmap */
				if (xga.virge.imgxferport->command_set & 0x200) { /* transparent */
					do {
						if (xga.virge.bitbltstate.src_drem > 0) {
							if ((uint8_t)xga.virge.bitbltstate.itf_buffer & msk) {
								srcpixel = xga.virge.bitblt.src_fgcolor;
								dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
								patpixel = XGA_VirgePatPixel(x,y);
								mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
								XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
							}
							xga.virge.bitbltstate.src_drem--;
						}

						msk >>= 1u;
						x++;
					} while (msk != 0u);
				}
				else {
					do {
						if (xga.virge.bitbltstate.src_drem > 0) {
							srcpixel = ((uint8_t)xga.virge.bitbltstate.itf_buffer & msk) ? xga.virge.bitblt.src_fgcolor : xga.virge.bitblt.src_bgcolor;
							dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
							patpixel = XGA_VirgePatPixel(x,y);
							mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
							XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
							xga.virge.bitbltstate.src_drem--;
						}

						msk >>= 1u;
						x++;
					} while (msk != 0u);
				}
			}

			xga.virge.bitbltstate.itf_buffer >>= (uint64_t)8;
			xga.virge.bitbltstate.itf_buffer_bytecount--;
			xga.virge.bitbltstate.src_xrem--;

			if (xga.virge.bitbltstate.src_xrem == 0) {
				if (y == xga.virge.bitbltstate.stopy) {
					xga.virge.bitbltstate.itf_buffer_bytecount = 0;
					xga.virge.bitbltstate.itf_buffer = 0;
					xga.virge.imgxferportfunc = NULL;
					xga.virge.imgxferport = NULL;
					break;
				}
				else {
					xga.virge.bitbltstate.src_drem = xga.virge.bitblt.rect_width;
					xga.virge.bitbltstate.src_xrem = xga.virge.bitbltstate.src_stride;
					x = xga.virge.bitbltstate.startx;
					y++;
				}
			}
		}
	}
	else {
		uint32_t bypmsk = 0x000000FF;
		unsigned char bypp = 1;

		switch((xga.virge.bitblt.command_set >> 2u) & 7u) {
			case 1: bypp = 2u; bypmsk = 0x0000FFFF; break; // 16 bits/pixel
			case 2: bypp = xga.virge.truecolor_bypp; bypmsk = xga.virge.truecolor_mask; break; // 24/32 bits/pixel
			default: break;
		}

		/* color image bitmap */
		assert(xga.virge.bitbltstate.src_xrem != 0);
		while (xga.virge.bitbltstate.itf_buffer_bytecount >= bypp) {
#if 0
			LOG_MSG("BitBlt color t=%u x=%u y=%u srm=%u/%u sw=%u patb=%02x srcb=%02x ctrl=%08x",
				(xga.virge.imgxferport->command_set & 0x200) ? 1 : 0,
				x,y,xga.virge.bitbltstate.src_xrem,xga.virge.bitbltstate.src_stride,
				xga.virge.bitblt.rect_width,
				pb,(uint8_t)xga.virge.bitbltstate.itf_buffer & 0xFFu,
				xga.virge.bitblt.command_set);
#endif

			if (xga.virge.imgxferport->command_set & 0x100) { /* mono pattern, color bitmap */
				if (xga.virge.imgxferport->command_set & 0x200) { /* transparent */
					// TODO
					LOG_MSG("BitBlt Color transparent mono pattern unimpl");
				}
				else {
					if (xga.virge.bitbltstate.src_drem > 0) {
						srcpixel = (uint32_t)xga.virge.bitbltstate.itf_buffer & bypmsk;
						dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
						patpixel = XGA_VirgePatPixelMono(x,y);
						mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
						XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
						xga.virge.bitbltstate.src_drem--;
					}

					x++;
				}
			}
			else { /* color pattern, color bitmap */
				if (xga.virge.imgxferport->command_set & 0x200) { /* transparent */
					// TODO
					LOG_MSG("BitBlt Color transparent color pattern unimpl");
				}
				else {
					if (xga.virge.bitbltstate.src_drem > 0) {
						srcpixel = (uint32_t)xga.virge.bitbltstate.itf_buffer & bypmsk;
						dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);
						patpixel = XGA_VirgePatPixel(x,y);
						mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
						XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);
						xga.virge.bitbltstate.src_drem--;
					}

					x++;
				}
			}

			xga.virge.bitbltstate.itf_buffer >>= (uint64_t)(8u * bypp);
			xga.virge.bitbltstate.itf_buffer_bytecount -= bypp;
			xga.virge.bitbltstate.src_xrem -= bypp;

			if (xga.virge.bitbltstate.src_xrem < bypp) {
				if (y == xga.virge.bitbltstate.stopy) {
					xga.virge.bitbltstate.itf_buffer_bytecount = 0;
					xga.virge.bitbltstate.itf_buffer = 0;
					xga.virge.imgxferportfunc = NULL;
					xga.virge.imgxferport = NULL;
					break;
				}
				else {
					if (xga.virge.bitbltstate.src_xrem > 0) {
						if (xga.virge.bitbltstate.src_xrem > xga.virge.bitbltstate.itf_buffer_bytecount) {
							xga.virge.bitbltstate.src_xrem -= xga.virge.bitbltstate.itf_buffer_bytecount;
							xga.virge.bitbltstate.itf_buffer_initskip = xga.virge.bitbltstate.src_xrem;
							break;
						}
						else {
							xga.virge.bitbltstate.itf_buffer >>= (uint64_t)(8u * xga.virge.bitbltstate.src_xrem);
							xga.virge.bitbltstate.itf_buffer_bytecount -= xga.virge.bitbltstate.src_xrem;
						}
					}

					xga.virge.bitbltstate.src_drem = xga.virge.bitblt.rect_width;
					xga.virge.bitbltstate.src_xrem = xga.virge.bitbltstate.src_stride;
					x = xga.virge.bitbltstate.startx;
					y++;
				}
			}
		}
	}

	xga.virge.bitblt.rect_dst_x = x;
	xga.virge.bitblt.rect_dst_y = y;
}

void XGA_ViRGE_BitBlt(XGAStatus::XGA_VirgeState::reggroup &rset) {
	uint32_t srcpixel,mixpixel,dstpixel,patpixel;

	if (rset.command_set & 0x80) { /* data will be coming in from the image transfer port */
		xga.virge.imgxferport = &rset;
		xga.virge.imgxferportfunc = XGA_ViRGE_BitBlt_xferport;

		xga.virge.bitbltstate.itf_buffer = 0;
		xga.virge.bitbltstate.itf_buffer_bytecount = 0;
		xga.virge.bitbltstate.itf_buffer_initskip = (rset.command_set >> 12u) & 3u;
		xga.virge.bitbltstate.startx = rset.rect_dst_x;
		xga.virge.bitbltstate.starty = rset.rect_dst_y;
		xga.virge.bitbltstate.stopy = rset.rect_dst_y + rset.rect_height - 1u;
		if (rset.command_set & 0x40) {
			/* mono image */
			xga.virge.bitbltstate.src_stride = (rset.rect_width + 7u) / 8u;
		}
		else {
			/* color image */
			xga.virge.bitbltstate.src_stride = rset.rect_width;
			switch((rset.command_set >> 2u) & 7u) {
				case 1: xga.virge.bitbltstate.src_stride *= 2u; break; // 16 bits/pixel
				case 2: xga.virge.bitbltstate.src_stride *= xga.virge.truecolor_bypp; break; // 24/32 bits/pixel
				default: break;
			}
		}

		switch((rset.command_set >> 10u) & 3u) {
			case 1: xga.virge.bitbltstate.src_stride = (xga.virge.bitbltstate.src_stride + 1u) & (~1u); break; // WORD align
			case 2: xga.virge.bitbltstate.src_stride = (xga.virge.bitbltstate.src_stride + 3u) & (~3u); break; // DWORD align
			default: break;
		}

		xga.virge.bitbltstate.src_drem = xga.virge.bitblt.rect_width;
		xga.virge.bitbltstate.src_xrem = xga.virge.bitbltstate.src_stride;

		if (xga.virge.bitbltstate.src_stride == 0) {
			xga.virge.imgxferport = NULL;
			xga.virge.imgxferportfunc = NULL;
		}
	}
	else { /* source data is video memory */
		xga.virge.imgxferport = NULL;
		xga.virge.imgxferportfunc = NULL;

		if (xga.virge.bitblt.command_set & 0x200) { /* transparent */
			LOG_MSG("BitBlt VRAM to VRAM transparent");
		}
		else {
			unsigned int sxa,sya;
			unsigned int rx,ry;
			unsigned int dx,dy;
			unsigned int ex,ey;
			unsigned int sx,sy;
			unsigned int x,y;

			if (xga.virge.bitblt.rect_width != 0 && xga.virge.bitblt.rect_height != 0) {
				if (!(xga.virge.bitblt.command_set & (1u << 25u))) {
					/* X-negative */
					rx = -1;
					dx = xga.virge.bitblt.rect_dst_x;
					ex = xga.virge.bitblt.rect_dst_x - (xga.virge.bitblt.rect_width - 1);
					if ((int)ex < 0) ex = 0;
				}
				else {
					rx = 1;
					dx = xga.virge.bitblt.rect_dst_x;
					ex = xga.virge.bitblt.rect_dst_x + (xga.virge.bitblt.rect_width - 1);
				}

				if (!(xga.virge.bitblt.command_set & (1u << 26u))) {
					/* Y-negative */
					ry = -1;
					dy = xga.virge.bitblt.rect_dst_y;
					ey = xga.virge.bitblt.rect_dst_y - (xga.virge.bitblt.rect_height - 1);
					if ((int)ey < 0) ey = 0;
				}
				else {
					ry = 1;
					dy = xga.virge.bitblt.rect_dst_y;
					ey = xga.virge.bitblt.rect_dst_y + (xga.virge.bitblt.rect_height - 1);
				}

				sxa = xga.virge.bitblt.rect_src_x - xga.virge.bitblt.rect_dst_x;
				sya = xga.virge.bitblt.rect_src_y - xga.virge.bitblt.rect_dst_y;

				sy = dy + sya;
				y = dy;
				do {
					sx = dx + sxa;
					x = dx;
					do {
						srcpixel = XGA_ReadSourceVirgePixel(xga.virge.bitblt,sx,sy);
						dstpixel = XGA_ReadDestVirgePixel(xga.virge.bitblt,x,y);

						if (xga.virge.bitblt.command_set & 0x100)
							patpixel = XGA_VirgePatPixelMono(x,y);
						else
							patpixel = XGA_VirgePatPixel(x,y);

						mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(xga.virge.bitblt.command_set>>17u)&0xFFu);
						XGA_DrawVirgePixelCR(xga.virge.bitblt,x,y,mixpixel);

						if (x == ex) break;
						sx += rx;
						x += rx;
					} while (1);

					if (y == ey) break;
					sy += ry;
					y += ry;
				} while (1);
			}
		}
	}
}

void XGA_ViRGE_DrawRect(XGAStatus::XGA_VirgeState::reggroup &rset) {
	uint32_t srcpixel,mixpixel,dstpixel,patpixel;
	unsigned int bex,bey,enx,eny;/*inclusive*/
	unsigned int x,y;
	unsigned char rb;

	if (rset.rect_width == 0 || rset.rect_height == 0)
		return;

	bex = rset.rect_dst_x;
	bey = rset.rect_dst_y;
	enx = bex + rset.rect_width - 1;
	eny = bey + rset.rect_height -1;

	if (rset.command_set & 2) { /* hardware clipping enable */
		if (bex < rset.left_clip)
			bex = rset.left_clip;
		if (bey < rset.top_clip)
			bey = rset.top_clip;
		if (enx > rset.right_clip)
			enx = rset.right_clip;
		if (eny > rset.bottom_clip)
			eny = rset.bottom_clip;
	}

	// NTS: I don't know if the monochrome pattern is being drawn properly because I can't get Windows 3.1
	//      to use this case for anything other than solid color rectangles. I don't know if I am reading
	//      out the monochrome pattern correctly here. --J.C.

	// NTS: always use mono pattern as documented by S3.
	//      Command set MP bit must be set anyway.
	for (y=bey;y <= eny;y++) {
		rb = ((unsigned char*)(&rset.mono_pat))[(y-rset.rect_dst_y)&7]; /* WARNING: Only works on little Endian CPUs */
		if (bex != rset.rect_dst_x) {
			unsigned char r = (bex - rset.rect_dst_x) & 7;
			if (r != 0) rb = (rb << r) | (rb >> (8 - r));
		}

		if (rset.command_set & 0x200) { /* TP - Transparent */
			for (x=bex;x <= enx;x++) {
				if (rb & 0x80) {
					srcpixel = rset.mono_pat_fgcolor;
					dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
					patpixel = rset.mono_pat_fgcolor/*See notes*/;
					mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
					XGA_DrawVirgePixel(rset,x,y,mixpixel);
				}
				rb = (rb << 1u) | (rb >> 7u);
			}
		}
		else {
			for (x=bex;x <= enx;x++) {
				srcpixel = (rb & 0x80) ? rset.mono_pat_fgcolor : rset.mono_pat_bgcolor;
				dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
				patpixel = rset.mono_pat_fgcolor/*See notes*/;
				mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
				XGA_DrawVirgePixel(rset,x,y,mixpixel);
				rb = (rb << 1u) | (rb >> 7u);
			}
		}
	}

	/* NTS: From the S3 datasheet "Command Set Register": "The full range of 256 ROPs are available for BitBlt. Other operations like Rectangle, Line, etc.
	 *      can only use a subset of the ROPs that does not have a source. When a ROP contains a pattern, the pattern must be mono and the hardware forces
	 *      the pattern value to the pattern foreground color regardless of the values programmed in the Mono Pattern registers."
	 *
	 *      True to this statement, Windows 3.1 Virge drivers like to issue XGA rectangle commands with the ROP set to 0xF0 (pattern fill) when drawing
	 *      solid color rectangles. */

	rset.rect_dst_x = enx + 1;
	rset.rect_dst_y = eny + 1;
}

void XGA_ViRGE_BitBlt_Execute(bool commandwrite) {
	auto &rset = xga.virge.bitblt;

	xga.virge.imgxferport = NULL;
	xga.virge.imgxferportfunc = NULL;

	if (commandwrite)
		rset.command_execute_on_register = 0;

	switch ((rset.command_set >> 27u) & 0x1F) { /* bits [31:31] 3D command if set, 2D else. bits [30:27] command */
		case 0x00: /* 2D BitBlt */
			XGA_ViRGE_BitBlt(rset);
			break;
		case 0x02: /* 2D Rectangle Fill */
			XGA_ViRGE_DrawRect(rset);
			break;
		case 0x0F: /* NOP */
			break;
		default:
			LOG(LOG_VGA,LOG_DEBUG)("BitBlt unhandled command %08x",(unsigned int)rset.command_set);
			break;
	}
}

void XGA_ViRGE_BitBlt_Execute_deferred(void) {
	auto &rset = xga.virge.bitblt;

	xga.virge.imgxferport = NULL;
	xga.virge.imgxferportfunc = NULL;
	switch ((rset.command_set >> 27u) & 0x1F) { /* bits [31:31] 3D command if set, 2D else. bits [30:27] command */
		case 0x00: /* 2D BitBlt */
		case 0x02: /* 2D Rectangle Fill */
			rset.command_execute_on_register = 0x010C; /* A50C, etc */
			break;
		default:
			if (rset.command_set & (1u << 31u))
				LOG(LOG_VGA,LOG_DEBUG)("BitBlt execute 3D unhandled command %08x def",(unsigned int)rset.command_set);
			else
				LOG(LOG_VGA,LOG_DEBUG)("BitBlt execute 2D unhandled command %08x def",(unsigned int)rset.command_set);
			rset.command_execute_on_register = 0;
			break;
	};
}

struct VIRGELineDDA {
	int32_t		xf,xdelta;	/* 1<<20 X delta and fractional */

	void		adv(void);
	int		read_xtr(void);
};

void VIRGELineDDA::adv(void) {
	xf += xdelta;
}

int VIRGELineDDA::read_xtr(void) {
	return xf >> 20;
}

void XGA_ViRGE_DrawLine(XGAStatus::XGA_VirgeState::reggroup &rset) {
	uint32_t srcpixel,mixpixel,dstpixel,patpixel;
	int y,x,ycount,xend,xto,xdir,xstart;
	unsigned int safety;
	VIRGELineDDA ldda;

	/* HACK: Why doesn't the Windows 98 S3 ViRGE driver set the stride for the line2d register set?
	 *       I'm beginning to wonder if all dest/source offset and stride registers are really just
	 *       tied together into one set in the back. This hack is needed to make sure lines and
	 *       curves aren't jumbled up at the top of the screen when drawn. */
	rset.src_stride = xga.virge.bitblt.src_stride;
	rset.dst_stride = xga.virge.bitblt.dst_stride;
	rset.src_base = xga.virge.bitblt.src_base;
	rset.dst_base = xga.virge.bitblt.dst_base;

	xdir = (rset.lindrawcounty & 0x80000000u) ? 1/*left to right*/ : -1/*right to left*/;
	ycount = (int)(rset.lindrawcounty & 0x1FFFu); /* bits [10:0] */
	y = (int)(rset.lindrawstarty & 0x1FFFu); /* bits [10:0] */
	ldda.xf = rset.lindrawstartx; /* S11.20 fixed point signed, 1.0 = 1 << 20 */
	ldda.xdelta = rset.lindrawxdelta; /* S11.20 fixed point signed, 1.0 = 1 << 20      -(dX / dY) */
	xend = (int)rset.lindrawend1;/*last pixel*/
	xstart = (int)rset.lindrawend0;/*first pixel*/

	// unused for now
	(void)safety;

	/* S3 ViRGE Integrated 3D Accelerator [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20ViRGE%20Integrated%203D%20Accelerator%20%281996%2d08%29%2epdf]
	 * PDF page 238 Line Draw X Start Register.
	 *
	 * For X major line, +XDELTA, value = (x1 << 20) + (XDELTA/2)
	 * For X major line, -XDELTA, value = (x1 << 20) + (XDELTA/2) + ((1 << 20) - 1)
	 * For Y major line, value = x1 << 20
	 *
	 * NTS: Windows 3.1 S3 ViRGE drivers use (x1 << 20) + (1 << 19) equiv (x1 + 0.5) for Y major lines.
	 *
	 * Line Draw X Delta Register: XDELTA = -(changeInX << 20) / changeInY
	 *
	 * Also notice that based on how this line rendering works, is it vital to turn on
	 * clipping and set the clip region to the area you intend for the line to sit within,
	 * else when changeInX is large and changeInY is small, the line segment will extend
	 * some pixels past the end pixel. So in reality the XDELTA and start X registers
	 * describe horizontal line segments that extend slightly past the clipping region.
	 * At least that's how the Windows 3.1 treats this hardware acceleration function. */

	x = xstart;
#if 0//DEBUG
	LOG(LOG_VGA,LOG_DEBUG)("TODO: ViRGE Line Draw xdir=%d src_base=%x dst_base=%x ycount=%d y=%d xf=%d(%.3f) xdelta=%d(%.3f) xstart=%d xend=%d x=%d cmd=%x lc=%d rc=%d tc=%d bc=%d sstr=%d dstr=%d",
		xdir,rset.src_base,rset.dst_base,ycount,y,ldda.xf,(double)ldda.xf / (1<<20),ldda.xdelta,(double)ldda.xdelta / (1<<20),xstart,xend,x,rset.command_set,
		rset.left_clip,rset.right_clip,rset.top_clip,rset.bottom_clip,
		rset.src_stride,rset.dst_stride);
#endif

	/* NTS: Drawing completely horizontal lines according to S3 documentation:
	 *      - Set XDELTA to 0, as if changeInY == 0
	 *
	 *      Drawing completely horizontal lines, Windows 3.1 style (ViRGE drivers);
	 *      - Set ycount == 1, XDELTA to (xend + 1 - xstart) without the 20-bit shift, which
	 *        is then a value that is very close to zero, but not quite you lazy hack of a driver.
	 *        xstart <= x <= xend are the extents of the horizontal line to draw.
	 *
	 *      Note that small to zero XDELTA values ALSO represent a vertical or near vertical line,
	 *      so that value alone isn't enough to determine if we're being asked to draw horizontal
	 *      lines, however when Windows 3.1 does it, ycount == 1, XDELTA is some small value,
	 *      XF is right in the middle of xstart-xend, and xstart-end are far wider than one pixel.
	 *      Not sure by what logic or special case S3 would have handled horizontal lines here.
	 *      Based on driver behavior, if the intent was to draw a 1-pixel high vertical line,
	 *      then the xstart/xend values would equal (xf >> 20) without any additional room. */

	if (ycount <= 1) {
		do {
			if ((xdir > 0 && x > xend) || (xdir < 0 && x < xend)) break;
			srcpixel = 0;
			dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
			patpixel = rset.mono_pat_fgcolor/*See notes*/;
			mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
			XGA_DrawVirgePixelCR(rset,x,y,mixpixel);
			x += xdir;
		} while (1);
	}
	else if (ldda.xdelta >= -(1 << 20) && ldda.xdelta <= (1 << 20)) { // Y-major
		/* NTS: xstart/xend must be considered to render Y-major lines correctly when the guest driver
		 *      wants to draw a line but omit the last pixel when drawing line segments of a polygon or shape.
		 *      This is needed to correctly render the shape in progress for example when drawing circles or
		 *      rounded rectangles in Microsoft Word (uses XOR raster operation), and to render curves in
		 *      Windows 98 correctly (Curves And Colors screen saver). Without the option to NOT render the
		 *      last pixel, XOR-based poly lines will be missing pixels, since the overlapping pixels cancel
		 *      each other out. */

		/* check: xstart skip last pixel, when drawing a line going downard, with XGA hardware that only draws upward */
		x = ldda.read_xtr();
		if ((xdir > 0 && x < xstart) || (xdir < 0 && x > xstart)) {
			ldda.adv();
			ycount--;
			y--;
		}

		while (ycount > 0) {
			x = ldda.read_xtr();
			if (ycount == 1) { /* check: xend skip last pixel, when drawing a line going upward */
				if ((xdir > 0 && x > xend) || (xdir < 0 && x < xend)) {
					break;
				}
			}

			srcpixel = 0;
			dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
			patpixel = rset.mono_pat_fgcolor/*See notes*/;
			mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
			XGA_DrawVirgePixelCR(rset,x,y,mixpixel);
			ldda.adv();
			y--;

			/* lines are drawn bottom-up */
			ycount--;
		}
	}
	else if (ldda.xdelta >= 0) { // X-major going to the left (draws bottom up, remember?)
		while (ycount > 0) {
			xto = ldda.read_xtr();
			while (x <= xto) {
				if (x >= xstart && x <= xend) {
					srcpixel = 0;
					dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
					patpixel = rset.mono_pat_fgcolor/*See notes*/;
					mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
					XGA_DrawVirgePixelCR(rset,x,y,mixpixel);
				}

				x++;
			}
			ldda.adv();
			y--;

			/* lines are drawn bottom-up */
			ycount--;
		}
	}
	else { // X-major going to the right, xdelta < 0 (draws bottom up, remember?)
		std::swap(xstart,xend);
		while (ycount > 0) {
			xto = ldda.read_xtr();
			while (x >= xto) {
				if (x >= xstart && x <= xend) {
					srcpixel = 0;
					dstpixel = XGA_ReadDestVirgePixel(rset,x,y);
					patpixel = rset.mono_pat_fgcolor/*See notes*/;
					mixpixel = XGA_MixVirgePixel(srcpixel,patpixel,dstpixel,(rset.command_set>>17u)&0xFFu);
					XGA_DrawVirgePixelCR(rset,x,y,mixpixel);
				}

				x--;
			}
			ldda.adv();
			y--;

			/* lines are drawn bottom-up */
			ycount--;
		}
	}
}

void XGA_ViRGE_Line2D_Execute(bool commandwrite) {
	auto &rset = xga.virge.line2d;

	xga.virge.imgxferport = NULL;
	xga.virge.imgxferportfunc = NULL;

	if (commandwrite)
		rset.command_execute_on_register = 0;

	switch ((rset.command_set >> 27u) & 0x1F) { /* bits [31:31] 3D command if set, 2D else. bits [30:27] command */
		case 0x03: /* 2D Line Draw */
			XGA_ViRGE_DrawLine(rset);
			break;
		case 0x0F: /* NOP */
			break;
		default:
			LOG(LOG_VGA,LOG_DEBUG)("Line2D unhandled command %08x",(unsigned int)rset.command_set);
			break;
	}
}

void XGA_ViRGE_Line2D_Execute_deferred(void) {
	auto &rset = xga.virge.line2d;

	xga.virge.imgxferport = NULL;
	xga.virge.imgxferportfunc = NULL;
	switch ((rset.command_set >> 27u) & 0x1F) { /* bits [31:31] 3D command if set, 2D else. bits [30:27] command */
		case 0x03: /* 2D Line Draw */
			rset.command_execute_on_register = 0x017C; /* A97C, etc */
			break;
		default:
			if (rset.command_set & (1u << 31u))
				LOG(LOG_VGA,LOG_DEBUG)("Line2D execute 3D unhandled command %08x def",(unsigned int)rset.command_set);
			else
				LOG(LOG_VGA,LOG_DEBUG)("Line2D execute 2D unhandled command %08x def",(unsigned int)rset.command_set);
			rset.command_execute_on_register = 0;
			break;
	};
}

void XGA_ViRGE_Poly2D_Execute(void) {
	auto &rset = xga.virge.poly2d;

	if (rset.command_set & (1u << 31u))
		LOG(LOG_VGA,LOG_DEBUG)("Poly2D execute 3D unhandled command %08x",(unsigned int)rset.command_set);
	else
		LOG(LOG_VGA,LOG_DEBUG)("Poly2D execute 2D unhandled command %08x",(unsigned int)rset.command_set);
}

void XGA_ViRGE_Poly2D_Execute_deferred(void) {
	auto &rset = xga.virge.poly2d;

	xga.virge.imgxferport = NULL;
	xga.virge.imgxferportfunc = NULL;
	switch ((rset.command_set >> 27u) & 0x1F) { /* bits [31:31] 3D command if set, 2D else. bits [30:27] command */
		default:
			if (rset.command_set & (1u << 31u))
				LOG(LOG_VGA,LOG_DEBUG)("Poly2D execute 3D command %08x def",(unsigned int)rset.command_set);
			else
				LOG(LOG_VGA,LOG_DEBUG)("Poly2D execute 2D command %08x def",(unsigned int)rset.command_set);
			rset.command_execute_on_register = 0;
			break;
	};
}

void XGA_Write(Bitu port, Bitu val, Bitu len) {
//	LOG_MSG("XGA: Write to port %x, val %8x, len %x", (unsigned int)port, (unsigned int)val, (unsigned int)len);

#if 0
	// streams processing debug
	if (port >= 0x8180 && port <= 0x81FF)
		LOG_MSG("XGA streams processing: Write to port %x, val %8x, len %x",(unsigned int)port,(unsigned int)val,(unsigned int)len);
#endif

	switch(port) {
		case 0x8100:// drawing control: row (low word), column (high word)
					// "CUR_X" and "CUR_Y" (see PORT 82E8h,PORT 86E8h)
			xga.cury = (uint16_t)(val & 0x0fff);
			if(len==4) xga.curx = (uint16_t)((val>>16)&0x0fff);
			break;
		case 0x8102:
			xga.curx = (uint16_t)(val& 0x0fff);
			break;
		case 0x8104:// drawing control: row (low word), column (high word)
					// "CUR_X2" and "CUR_Y2" (see PORT 82EAh,PORT 86EAh)
			if (s3Card == S3_Trio64) { // Only on Trio64, not Trio64V+, not ViRGE
				xga.cury2 = (uint16_t)(val & 0x0fff);
				if(len==4) xga.curx2 = (uint16_t)((val>>16)&0x0fff);
			}
			break;
		case 0x8106:
			if (s3Card == S3_Trio64) { // Only on Trio64, not Trio64V+, not ViRGE
				xga.curx2 = (uint16_t)(val& 0x0fff);
			}
			break;
		case 0x8108:// DWORD drawing control: destination Y and axial step
					// constant (low word), destination X and axial step
					// constant (high word) (see PORT 8AE8h,PORT 8EE8h)
			xga.desty = (uint16_t)(val&0x3FFF);
			if(len==4) xga.destx = (uint16_t)((val>>16)&0x3fff);
			break;
		case 0x810a:
			xga.destx = (uint16_t)(val&0x3fff);
			break;
		case 0x810c:// DWORD drawing control: destination Y and axial step
					// constant (low word), destination X and axial step
					// constant (high word) (see PORT 8AEAh,PORT 8EEAh)
			if (s3Card == S3_Trio64) { // Only on Trio64, not Trio64V+, not ViRGE
				xga.desty2 = (uint16_t)(val&0x3FFF);
				if(len==4) xga.destx2 = (uint16_t)((val>>16)&0x3fff);
			}
			break;
		case 0x810e:
			if (s3Card == S3_Trio64) { // Only on Trio64, not Trio64V+, not ViRGE
				xga.destx2 = (uint16_t)(val&0x3fff);
			}
			break;
		case 0x8110: // WORD error term (see PORT 92E8h)
			xga.ErrTerm = (uint16_t)(val&0x3FFF);
			break;

		case 0x8120: // packed MMIO: DWORD background color (see PORT A2E8h)
			xga.backcolor = (uint16_t)val;
			break;
		case 0x8124: // packed MMIO: DWORD foreground color (see PORT A6E8h)
			xga.forecolor = (uint16_t)val;
			break;
		case 0x8128: // DWORD	write mask (see PORT AAE8h)
			xga.writemask = (uint16_t)val;
			break;
		case 0x812C: // DWORD	read mask (see PORT AEE8h)
			xga.readmask = (uint16_t)val;
			break;
		case 0x8134: // packed MMIO: DWORD	background mix (low word) and
					 // foreground mix (high word)	(see PORT B6E8h,PORT BAE8h)
			xga.backmix = val&0xFFFF;
			if(len==4) xga.foremix = (uint16_t)(val>>16ul);
			break;
		case 0x8136:
			xga.foremix = (uint16_t)val;
			break;
		case 0x8138:// DWORD top scissors (low word) and left scissors (high
					// word) (see PORT BEE8h,#P1047)
			xga.scissors.y1=val&0x0fff;
			if(len==4) xga.scissors.x1 = (val>>16)&0x0fff;
			break;
		case 0x813a:
			xga.scissors.x1 = val&0x0fff;
			break;
		case 0x813C:// DWORD bottom scissors (low word) and right scissors
					// (high word) (see PORT BEE8h,#P1047)
			xga.scissors.y2=val&0x0fff;
			if(len==4) xga.scissors.x2 = (val>>16)&0x0fff;
			break;
		case 0x813e:
			xga.scissors.x2 = val&0x0fff;
			break;
		case 0x8140:// DWORD data manipulation control (low word) and
			// miscellaneous 2 (high word) (see PORT BEE8h,#P1047)
			xga.pix_cntl=val&0xFFFF;
			if(len==4) xga.control2=(val>>16)&0x0fff;
			break;
		case 0x8144:// DWORD miscellaneous (low word) and read register select
			// (high word)(see PORT BEE8h,#P1047)
			xga.control1=val&0xffff;
			if(len==4)xga.read_sel=(val>>16)&0x7;
			break; 
		case 0x8148:// DWORD minor axis pixel count (low word) and major axis
			// pixel count (high word) (see PORT BEE8h,#P1047,PORT 96E8h)
			xga.MIPcount = val&0x0fff;
			if(len==4) xga.MAPcount = (val>>16)&0x0fff;
			break;
		case 0x814a:
			xga.MAPcount = val&0x0fff;
			break;

		// Streams processing a.k.a overlays (0x8180-0x81FF)
		// Commonly used in Windows 3.1 through ME for the hardware YUV overlay,
		// such as playing MPEG files in ActiveMovie or XingMPEG.
		// S3 Trio64V+ and ViRGE cards have this.
		// Vision868 and Vision968 cards have a different register set for the same.

		case 0x8180: // S3 Trio64V+ streams processor, Primary Stream Control (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.psctl_psidf = (val >> 24u) & 7u;
				vga.s3.streams.psctl_psfc = (val >> 28u) & 7u;
			}
			break;
		case 0x8184: // S3 Trio64V+ streams processor, Color/Chroma Key Control (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ckctl_b_lb = val & 0xFFu;
				vga.s3.streams.ckctl_g_lb = (val >> 8u) & 0xFFu;
				vga.s3.streams.ckctl_r_lb = (val >> 16u) & 0xFFu;
				vga.s3.streams.ckctl_rgb_cc = (val >> 24u) & 7u;
				vga.s3.streams.ckctl_kc = (val >> 28u) & 1u;
			}
			break;
		case 0x8190: // S3 Trio64V+ streams processor, Secondary Stream Control (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ssctl_dda_haccum = val & 0xFFFu; // signed 12-bit value
				if (vga.s3.streams.ssctl_dda_haccum &  0x0800u)
					vga.s3.streams.ssctl_dda_haccum -= 0x1000u;
				vga.s3.streams.ssctl_sdif = (val >> 24u) & 7u;
				vga.s3.streams.ssctl_sfc = (val >> 28u) & 7u;
			}
			break;
		case 0x8194: // S3 Trio64V+ streams processor, Chroma Key Upper Bound (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ckctl_b_ub = val & 0xFFu;
				vga.s3.streams.ckctl_g_ub = (val >> 8u) & 0xFFu;
				vga.s3.streams.ckctl_r_ub = (val >> 16u) & 0xFFu;
			}
			break;
		case 0x8198: // S3 Trio64V+ streams processor, Secondary Stream Stretch/Filter Constants (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				uint32_t regmask;

				if (s3Card >= S3_ViRGEVX) /* ViRGE/VX datasheet says the register is 12 bits large */
					regmask = 0xFFFu;
				else /* earlier cards say the register is 11 bits large */
					regmask = 0x7FFu;

				/* Whoah, wait a minute! The S3 ViRGE and S3 Trio64V+ datasheets have a rather irritating error!
				 * They say K1 is bits 10-0 and K2 bits 26-16, but the visual diagram says K2 is bits 10-0 and
				 * K1 is bits 26-16!
				 *
				 * Based on what the S3 driver DCI driver writes for a 320x240 YUV overlay, it appears that K1
				 * (initial output window before scaling - 1) is the lower 16 bits.
				 *
				 * K2 is signed 2's complement */
				vga.s3.streams.ssctl_k1_hscale = (uint16_t)(val & regmask);
				vga.s3.streams.ssctl_k2_hscale = (uint16_t)((val >> 16u) & regmask);
				if (vga.s3.streams.ssctl_k2_hscale &  ((regmask+1u)>>1u))   /* (0x7FF+1)>>1 = 0x400 */
					vga.s3.streams.ssctl_k2_hscale -= (regmask+1u);         /* (0x7FF+1)    = 0x800 */
			}
			break;
		case 0x81A0: // S3 Trio64V+ streams processor, Blend Control (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.blendctl_ks = (val >> 2u) & 7u;
				vga.s3.streams.blendctl_kp = (val >> 10u) & 7u;
				vga.s3.streams.blendctl_composemode = (val >> 24u) & 7u;
			}
			break;
		case 0x81C0: // S3 Trio64V+ streams processor, Primary Stream Frame Buffer Address 0 (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ps_fba[0] = val & 0x3FFFFFu;
			}
			break;
		case 0x81C4: // S3 Trio64V+ streams processor, Primary Stream Frame Buffer Address 1 (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ps_fba[1] = val & 0x3FFFFFu;
			}
			break;
		case 0x81C8: // S3 Trio64V+ streams processor, Primary Stream Stride (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ps_stride = val & 0x1FFFu;
			}
			break;
		case 0x81CC: // S3 Trio64V+ streams processor, Double Buffer/LPB Support (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ps_bufsel = val & 1u;
				vga.s3.streams.ss_bufsel = (val >> 1u) & 3u;
				vga.s3.streams.lpb_in_bufsel = (val >> 4u) & 1u;
				vga.s3.streams.lpb_in_bufselloading = (val >> 5u) & 1u;
				vga.s3.streams.lpb_in_bufseltoggle = (val >> 6u) & 1u;
			}
			break;
		case 0x81D0: // S3 Trio64V+ streams processor, Secondary Stream Frame Buffer Address 0 (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ss_fba[0] = val & 0x3FFFFFu;
			}
			break;
		case 0x81D4: // S3 Trio64V+ streams processor, Secondary Stream Frame Buffer Address 1 (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ss_fba[1] = val & 0x3FFFFFu;
			}
			break;
		case 0x81D8: // S3 Trio64V+ streams processor, Secondary Stream Stride (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ss_stride = val & 0x1FFFu;
			}
			break;
		case 0x81DC: // S3 Trio64V+ streams processor, Opaque Overlay Control (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.ooc_pixfetch_stop = val & 0xFFFu;
				vga.s3.streams.ooc_pixfetch_resume = (val >> 16u) & 0xFFFu;
				vga.s3.streams.ooc_tss = (val >> 30u) & 1u;
				vga.s3.streams.ooc_ooc_enable = (val >> 31u) & 1u;
			}
			break;
		case 0x81E0: // S3 Trio64V+ streams processor, K1 Vertical Scale Factor (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.k1_vscale_factor = val & 0x7FFu;
			}
			break;
		case 0x81E4: // S3 Trio64V+ streams processor, K2 Vertical Scale Factor (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.k2_vscale_factor = val & 0x7FFu;
				if (vga.s3.streams.k2_vscale_factor & 0x400u)
					vga.s3.streams.k2_vscale_factor -= 0x800u;
			}
			break;
		case 0x81E8: // S3 Trio64V+ streams processor, DDA Vertical Accumulator Initial Value (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.dda_vaccum_iv = val & 0xFFFu;
				if (vga.s3.streams.dda_vaccum_iv & 0x0800u)
					vga.s3.streams.dda_vaccum_iv -= 0x1000u;
			}
			if (s3Card >= S3_ViRGE) {
				vga.s3.streams.evf = (val >> 15u) & 1u;
			}
			break;
		case 0x81EC: // S3 Trio64V+ streams processor, Streams FIFO and RAS Controls (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				/* bits [4:0] should be a value from 0 to 24 to shift priority between primary and secondary.
				 * The larger the value, the more slots allotted to secondary layer.
				 * Allocation is out of 24 slots, therefore values larger than 24 are invalid. */
				uint8_t thr = val & 0x1Fu;
				if (thr > 24u) thr -= 16u; // assume some kind of odd malfunction happens on real hardware, check later
				vga.s3.streams.fifo_alloc_ps = 24 - thr;
				vga.s3.streams.fifo_alloc_ss = thr;
				vga.s3.streams.fifo_ss_threshhold = (val >> 5u) & 0x1Fu;
				vga.s3.streams.fifo_ps_threshhold = (val >> 10u) & 0x1Fu;
				vga.s3.streams.ras_rl = (val >> 15u) & 1u;
				vga.s3.streams.ras_rp = (val >> 16u) & 1u;
				vga.s3.streams.edo_wsctl = (val >> 18u) & 1u;
			}
			break;
		case 0x81F0: // S3 Trio64V+ streams processor, Primary Stream Window Start Coordinates (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.pswnd_start_y = val & 0x3FFu;
				vga.s3.streams.pswnd_start_x = (val >> 16u) & 0x3FFu;
			}
			break;
		case 0x81F4: // S3 Trio64V+ streams processor, Primary Stream Window Size (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.pswnd_height = val & 0x3FFu;
				vga.s3.streams.pswnd_width = (val >> 16u) & 0x3FFu;
			}
			break;
		case 0x81F8: // S3 Trio64V+ streams processor, Secondary Stream Window Start Coordinates (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.sswnd_start_y = val & 0x3FFu;
				vga.s3.streams.sswnd_start_x = (val >> 16u) & 0x3FFu;
			}
			break;
		case 0x81FC: // S3 Trio64V+ streams processor, Primary Stream Window Size (MMIO only)
			if (s3Card == S3_Trio64V || s3Card >= S3_ViRGE) {
				vga.s3.streams.sswnd_height = val & 0x3FFu;
				vga.s3.streams.sswnd_width = (val >> 16u) & 0x3FFu;
			}
			break;

		case 0x92e8:
			xga.ErrTerm = val&0x3FFF;
			break;
		case 0x96e8:
			xga.MAPcount = val&0x0fff;
			break;
		case 0x9ae8:
		case 0x8118: // Trio64V+ packed MMIO
			XGA_DrawCmd(val, len);
			break;
		case 0xa2e8:
			XGA_SetDualReg(xga.backcolor, val);
			break;
		case 0xa6e8:
			XGA_SetDualReg(xga.forecolor, val);
			break;
		case 0xaae8:
			XGA_SetDualReg(xga.writemask, val);
			break;
		case 0xaee8:
			XGA_SetDualReg(xga.readmask, val);
			break;
		case 0x82e8:
			xga.cury = val&0x0fff;
			break;
		case 0x86e8:
			xga.curx = val&0x0fff;
			break;
		case 0x8ae8:
			xga.desty = val&0x3fff;
			break;
		case 0x8ee8:
			xga.destx = val&0x3fff;
			break;
		case 0xb2e8:
			XGA_SetDualReg(xga.color_compare, val);
			break;
		case 0xb6e8:
			xga.backmix = (uint16_t)val;
			break;
		case 0xbae8:
			xga.foremix = (uint16_t)val;
			break;
		case 0xbee8:
			XGA_Write_Multifunc(val, len);
			break;
		case 0xe2e8:
			xga.waitcmd.newline = false;
			XGA_DrawWait(val, len);
			break;
		case 0x83d4:
			if(len==1) vga_write_p3d4(0,val,1);
			else if(len==2) {
				vga_write_p3d4(0,val&0xff,1);
				vga_write_p3d5(0,val>>8,1);
			}
			else E_Exit("unimplemented XGA MMIO");
			break;
		case 0x83d5:
			if(len==1) vga_write_p3d5(0,val,1);
			else E_Exit("unimplemented XGA MMIO");
			break;
		case 0xa4d4:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__src_base(val);
			else goto default_case;
			break;
		case 0xa8d4:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__src_base(val);
			else goto default_case;
			break;
		case 0xacd4:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__src_base(val);
			else goto default_case;
			break;
		case 0xa4d8:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__dst_base(val);
			else goto default_case;
			break;
		case 0xa8d8:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__dst_base(val);
			else goto default_case;
			break;
		case 0xacd8:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__dst_base(val);
			else goto default_case;
			break;
		case 0xa4dc:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__left_right_clip_00dc(val);
			else goto default_case;
			break;
		case 0xa8dc:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__left_right_clip_00dc(val);
			else goto default_case;
			break;
		case 0xacdc:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__left_right_clip_00dc(val);
			else goto default_case;
			break;
		case 0xa4e0:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__top_bottom_clip_00e0(val);
			else goto default_case;
			break;
		case 0xa8e0:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__top_bottom_clip_00e0(val);
			else goto default_case;
			break;
		case 0xace0:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__top_bottom_clip_00e0(val);
			else goto default_case;
			break;
		case 0xa4e4:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__src_dest_stride_00e4(val);
			else goto default_case;
			break;
		case 0xa8e4:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__src_dest_stride_00e4(val);
			else goto default_case;
			break;
		case 0xace4:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__src_dest_stride_00e4(val);
			else goto default_case;
			break;
		case 0xa4e8:
		case 0xa4ec:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__mono_pat_dword((port>>2u)&1u,val);
			else goto default_case;
			break;
		case 0xace8:
		case 0xacec:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__mono_pat_dword((port>>2u)&1u,val);
			else goto default_case;
			break;
		case 0xa4f0:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__mono_pat_bgcolor(val);
			else goto default_case;
			break;
		case 0xacf0:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__mono_pat_bgcolor(val);
			else goto default_case;
			break;
		case 0xa4f4:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__mono_pat_fgcolor(val);
			else goto default_case;
			break;
		case 0xa8f4:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__mono_pat_fgcolor(val);
			else goto default_case;
			break;
		case 0xacf4:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__mono_pat_fgcolor(val);
			else goto default_case;
			break;
		case 0xa4f8:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__src_bgcolor(val);
			else goto default_case;
			break;
		case 0xa4fc:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__src_fgcolor(val);
			else goto default_case;
			break;
		case 0xa500:
			if (s3Card >= S3_ViRGE) {
				auto &rg = xga.virge.bitblt_validate_port(port);
				rg.set__command_set(val);
				if (rg.command_set & 1) XGA_ViRGE_BitBlt_Execute_deferred();
				else XGA_ViRGE_BitBlt_Execute(true);
			}
			else goto default_case;
			break;
		case 0xa900:
			if (s3Card >= S3_ViRGE) {
				auto &rg = xga.virge.line2d_validate_port(port);
				rg.set__command_set(val);
				if (rg.command_set & 1) XGA_ViRGE_Line2D_Execute_deferred();
				else XGA_ViRGE_Line2D_Execute(true);
			}
			else goto default_case;
			break;
		case 0xA96C:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__lindrawend_016c(val);
			else goto default_case;
			break;
		case 0xA970:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__lindrawxdelta_0170(val);
			else goto default_case;
			break;
		case 0xA974:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__lindrawstartx_0174(val);
			else goto default_case;
			break;
		case 0xA978:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__lindrawstartx_0178(val);
			else goto default_case;
			break;
		case 0xA97C:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__lindrawcounty_017c(val);
			else goto default_case;
			break;
		case 0xad00:
			if (s3Card >= S3_ViRGE) {
				auto &rg = xga.virge.poly2d_validate_port(port);
				rg.set__command_set(val);
				if (rg.command_set & 1) XGA_ViRGE_Poly2D_Execute_deferred();
				else XGA_ViRGE_Poly2D_Execute();
			}
			else goto default_case;
			break;
		case 0xa504:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__rect_width_height_0104(val);
			else goto default_case;
			break;
		case 0xa904:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__rect_width_height_0104(val);
			else goto default_case;
			break;
		case 0xad04:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__rect_width_height_0104(val);
			else goto default_case;
			break;
		case 0xa508:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__rect_src_xy_0108(val);
			else goto default_case;
			break;
		case 0xa908:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__rect_src_xy_0108(val);
			else goto default_case;
			break;
		case 0xad08:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__rect_src_xy_0108(val);
			else goto default_case;
			break;
		case 0xa50c:
			if (s3Card >= S3_ViRGE) xga.virge.bitblt_validate_port(port).set__rect_dst_xy_010c(val);
			else goto default_case;
			break;
		case 0xa90c:
			if (s3Card >= S3_ViRGE) xga.virge.line2d_validate_port(port).set__rect_dst_xy_010c(val);
			else goto default_case;
			break;
		case 0xad0c:
			if (s3Card >= S3_ViRGE) xga.virge.poly2d_validate_port(port).set__rect_dst_xy_010c(val);
			else goto default_case;
			break;
		default:
		default_case:
			if(port <= 0x4000) {
				//LOG_MSG("XGA: Wrote to port %4x with %08x, len %x", port, val, len);
				xga.waitcmd.newline = false;
				XGA_DrawWait(val, len);
				
			}
			else if (port >= 0xA100 && port < 0xA1C0 && s3Card >= S3_ViRGE) {
				/* color pattern registers */
				const unsigned int i = (port-0xA100u)>>2u;
				assert(i < 48);
				xga.virge.colorpat.raw[i] = (uint32_t)val;
			}
			else LOG_MSG("XGA: Wrote to port %x with %x, len %x", (int)port, (int)val, (int)len);
			break;
	}

	if (s3Card >= S3_ViRGE) {
		switch (port&0xFC00) {
			case 0xA400:
				{
					auto &rset = xga.virge.bitblt_validate_port(port);
					if (rset.command_execute_on_register != 0 && rset.command_execute_on_register == (port&0x3FF))
						XGA_ViRGE_BitBlt_Execute(false);
				}
				break;
			case 0xA800:
				{
					auto &rset = xga.virge.line2d_validate_port(port);
					if (rset.command_execute_on_register != 0 && rset.command_execute_on_register == (port&0x3FF))
						XGA_ViRGE_Line2D_Execute(false);
				}
				break;
			case 0xAC00:
				{
					auto &rset = xga.virge.poly2d_validate_port(port);
					if (rset.command_execute_on_register != 0 && rset.command_execute_on_register == (port&0x3FF))
						XGA_ViRGE_Poly2D_Execute();
				}
				break;
		}
	}
}

Bitu XGA_Read(Bitu port, Bitu len) {
	switch(port) {
		case 0x8118:
		case 0x9ae8:
			return 0x400; // nothing busy
		case 0x81ec: // S3 video data processor
			return 0x00007000;
		case 0x8504: // S3 ViRGE Subsystem Status Register
			if (s3Card >= S3_ViRGE) /* HACK: Always say S3D ENGINE is IDLE, or else Windows 98 will hang at startup */
				return (16 << 8)/*S3D FIFO SLOTS FREE*/ | 0x2000/*S3D ENGINE IDLE*/;
			else
				return 0xffffffff;
		case 0x850c: // S3 ViRGE Advanced Function Control Register
			if (s3Card >= S3_ViRGE) /* HACK: Always say not busy, or Windows 98 will hang at startup */
				return (8 << 6)/*COMMAND FIFO STATUS*/ | 1/*Enable SVGA*/ | 0x10/*Linear address enable*/;
			else
				return 0xffffffff;
		case 0x83da:
			{
				Bits delaycyc = CPU_CycleMax/5000;
				if(GCC_UNLIKELY(CPU_Cycles < 3*delaycyc)) delaycyc = 0;
				CPU_Cycles -= delaycyc;
				CPU_IODelayRemoved += delaycyc;
				return vga_read_p3da(0,0);
			}
		case 0x83d4:
			if(len==1) return vga_read_p3d4(0,0);
			else E_Exit("unimplemented XGA MMIO");
			break;
		case 0x83d5:
			if(len==1) return vga_read_p3d5(0,0);
			else E_Exit("unimplemented XGA MMIO");
			break;
		case 0x9ae9:
			if(xga.waitcmd.wait) return 0x4;
			else return 0x0;
		case 0xbee8:
			return XGA_Read_Multifunc();
		case 0xb2e8:
			return XGA_GetDualReg(xga.color_compare);
		case 0xa2e8:
			return XGA_GetDualReg(xga.backcolor);
		case 0xa6e8:
			return XGA_GetDualReg(xga.forecolor);
		case 0xaae8:
			return XGA_GetDualReg(xga.writemask);
		case 0xaee8:
			return XGA_GetDualReg(xga.readmask);
		default:
			//LOG_MSG("XGA: Read from port %x, len %x", port, len);
			break;
	}
	return 0xffffffff; 
}

void VGA_SetupXGA(void) {
	if (!IS_VGA_ARCH) return;

	memset(&xga, 0, sizeof(XGAStatus));

	/* FIXME: ViRGE cards like 24bpp rather than 32bpp? Or is that just Windows driver laziness? Leave the option open for 32bpp ViRGE acceleration. */
	if (s3Card >= S3_ViRGE && s3Card <= S3_ViRGEVX) {
		xga.virge.truecolor_bypp = 3;
		xga.virge.truecolor_mask = 0xFFFFFF;
	}
	else {
		xga.virge.truecolor_bypp = 4;
		xga.virge.truecolor_mask = 0xFFFFFFFF;
	}

	xga.scissors.y1 = 0;
	xga.scissors.x1 = 0;
	xga.scissors.y2 = 0xFFF;
	xga.scissors.x2 = 0xFFF;

	if (svgaCard != SVGA_S3Trio) return;

	IO_RegisterWriteHandler(0x42e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x42e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x46e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x4ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	
	IO_RegisterWriteHandler(0x82e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x82e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x82e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x82e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x86e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x86e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x86e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x86e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x8ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ae8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x8ee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x8ee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x92e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x92e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x92e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x92e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x96e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x96e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x96e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x96e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x9ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x9ae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x9ee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x9ee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xa2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xa6e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa6e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xa6e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa6e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xaae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xaae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xaee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xaee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xb2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xb2e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb2e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xb6e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb6e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xbee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xbee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xbae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xbae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2e0,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2e0,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2ea,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2ea,&XGA_Read,IO_MB | IO_MW | IO_MD);
}

// save state support
void POD_Save_VGA_XGA( std::ostream& stream )
{
	// static globals


	// - pure struct data
	WRITE_POD( &xga, xga );
}


void POD_Load_VGA_XGA( std::istream& stream )
{
	// static globals


	// - pure struct data
	READ_POD( &xga, xga );
}

void SD3_Reset(bool enable) {
	// STUB
	LOG(LOG_VGA,LOG_DEBUG)("S3D reset %s",enable?"begin":"end");
}

