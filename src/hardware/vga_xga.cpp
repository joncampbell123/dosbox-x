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
#include "vga.h"
#include <math.h>
#include <stdio.h>
#include "callback.h"
#include "cpu.h"		// for 0x3da delay

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
		uint16_t curx, cury;
		uint16_t x1, y1, x2, y2, sizex, sizey;
		uint32_t data; /* transient data passed by multiple calls */
		Bitu datasize;
		Bitu buswidth;
	} waitcmd;

} xga;

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

void XGA_DrawLineBresenham(Bitu val) {
	Bits xat, yat;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	Bits i;
	Bits tmpswap;
	bool steep;

#define SWAP(a,b) tmpswap = a; a = b; b = tmpswap;

	Bits dx, sx, dy, sy, e, dmajor, dminor,destxtmp;

	// Probably a lot easier way to do this, but this works.

	dminor = (Bits)((int16_t)xga.desty);
	if(xga.desty&0x2000) dminor |= ~((Bits)0x1fff);
	dminor >>= 1;

	destxtmp=(Bits)((int16_t)xga.destx);
	if(xga.destx&0x2000) destxtmp |= ~((Bits)0x1fff);


	dmajor = -(destxtmp - (dminor << (Bits)1)) >> (Bits)1;
	
	dx = dmajor;
	if((val >> 5) & 0x1) {
        sx = 1;
	} else {
		sx = -1;
	}
	dy = dminor;
	if((val >> 7) & 0x1) {
        sy = 1;
	} else {
		sy = -1;
	}
	e = (Bits)((int16_t)xga.ErrTerm);
	if(xga.ErrTerm&0x2000) e |= ~((Bits)0x1fff); /* sign extend 13-bit error term */
	xat = xga.curx;
	yat = xga.cury;

	if((val >> 6) & 0x1) {
		steep = false;
		SWAP(xat, yat);
		SWAP(sx, sy);
	} else {
		steep = true;
	}
    
//	LOG_MSG("XGA: Bresenham: ASC %ld, LPDSC %ld, sx %ld, sy %ld, err %ld, steep %ld, length %ld, dmajor %ld, dminor %ld, xstart %ld, ystart %ld",
//		dx, dy, sx, sy, e, (unsigned long)steep, (unsigned long)xga.MAPcount, dmajor, dminor, xat, yat);

	for (i=0;i<=xga.MAPcount;i++) { 
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

					if(steep) {
						dstdata = XGA_GetPoint((Bitu)xat,(Bitu)yat);
					} else {
						dstdata = XGA_GetPoint((Bitu)yat,(Bitu)xat);
					}

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

					if(steep) {
						XGA_DrawPoint((Bitu)xat,(Bitu)yat, destval);
					} else {
						XGA_DrawPoint((Bitu)yat,(Bitu)xat, destval);
					}

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

	if(steep) {
		xga.curx = (uint16_t)xat;
		xga.cury = (uint16_t)yat;
	} else {
		xga.curx = (uint16_t)yat;
		xga.cury = (uint16_t)xat;
	}
	//	}
	//}
	
}

void XGA_DrawRectangle(Bitu val) {
	uint32_t xat, yat;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;

	Bits srcx, srcy, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcy = xga.cury;

	for(yat=0;yat<=xga.MIPcount;yat++) {
		srcx = xga.curx;
		for(xat=0;xat<=xga.MAPcount;xat++) {
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
		xga.waitcmd.cury++;
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
				xga.waitcmd.cury++;
				xga.waitcmd.cury&=0x0fff;
				newline = true;
				xga.waitcmd.newline = true;
				if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
					xga.waitcmd.wait = false;
			}
		} else { // else overlapping
			if(realx==xga.waitcmd.x2) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury++;
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
	xga.waitcmd.curx++;
	xga.waitcmd.curx&=0x0fff;
	XGA_CheckX();
}

void XGA_DrawWait(Bitu val, Bitu len) {
	if(!xga.waitcmd.wait) return;
	Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
	Bitu srcval;
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
							if(len!=4) { // Win 3.11 864 'hack?'
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
						case 0x0:
							chunksize=8;
							chunks=1;
							break;
						case 0x20: // 16 bit
							chunksize=16;
							if(len==4) chunks=2;
							else chunks = 1;
							break;
						case 0x40: // 32 bit
							chunksize=16;
							if(len==4) chunks=2;
							else chunks = 1;
							break;
						case 0x60: // undocumented guess (but works)
							chunksize=8;
							chunks=4;
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
				if(srcdata == xga.forecolor) {
					mixmode = xga.foremix;
				} else {
					if(srcdata == xga.backcolor) {
						mixmode = xga.backmix;
					} else {
						/* Best guess otherwise */
						mixmode = 0x67; /* Source is bitmap data, mix mode is src */
					}
				}
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
			

			if(mixselect == 0x3) {
				// TODO lots of guessing here but best results this way
				/*if(srcdata == xga.forecolor)*/ mixmode = xga.foremix;
				// else 
				if(srcdata == xga.backcolor || srcdata == 0) 
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
				xga.waitcmd.x1 = xga.curx;
				xga.waitcmd.y1 = xga.cury;
				xga.waitcmd.x2 = (uint16_t)((xga.curx + xga.MAPcount)&0x0fff);
				xga.waitcmd.y2 = (uint16_t)((xga.cury + xga.MIPcount + 1)&0x0fff);
				xga.waitcmd.sizex = xga.MAPcount;
				xga.waitcmd.sizey = xga.MIPcount + 1;
				xga.waitcmd.cmd = 2;
				xga.waitcmd.buswidth = (Bitu)vga.mode | (Bitu)((val&0x600u) >> 4u);
				xga.waitcmd.data = 0;
				xga.waitcmd.datasize = 0;

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

void XGA_Write(Bitu port, Bitu val, Bitu len) {
//	LOG_MSG("XGA: Write to port %x, val %8x, len %x", port,val, len);

#if 0
	// streams procesing debug
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
		// Vision868 cards have a different register set for the same.

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
				vga.s3.streams.ssctl_k1_hscale = val & regmask;
				vga.s3.streams.ssctl_k2_hscale = (val >> 16u) & regmask;
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
				 * The larger the value, the more slots alloted to secondary layer.
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
		default:
			if(port <= 0x4000) {
				//LOG_MSG("XGA: Wrote to port %4x with %08x, len %x", port, val, len);
				xga.waitcmd.newline = false;
				XGA_DrawWait(val, len);
				
			}
			else LOG_MSG("XGA: Wrote to port %x with %x, len %x", (int)port, (int)val, (int)len);
			break;
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

	xga.scissors.y1 = 0;
	xga.scissors.x1 = 0;
	xga.scissors.y2 = 0xFFF;
	xga.scissors.x2 = 0xFFF;
	
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

