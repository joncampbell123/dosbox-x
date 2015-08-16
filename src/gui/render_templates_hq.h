/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

/*
 * The HQ3x high quality 3x graphics filter.
 * Original author Maxim Stepin (see http://www.hiend3d.com/hq3x.html).
 * Adapted for DOSBox from ScummVM and HiEnd3D code by Kronuz.
 */

#include <stdlib.h>

#ifndef RENDER_TEMPLATES_HQNX_TABLE_H
#define RENDER_TEMPLATES_HQNX_TABLE_H

static Bit32u *_RGBtoYUV = 0;
static inline bool diffYUV(Bit32u yuv1, Bit32u yuv2)
{
	static const Bit32u Ymask = 0x00FF0000;
	static const Bit32u Umask = 0x0000FF00;
	static const Bit32u Vmask = 0x000000FF;
	static const Bit32u trY   = 0x00300000;
	static const Bit32u trU   = 0x00000700;
	static const Bit32u trV   = 0x00000006;

	Bit32u diff;
	Bit32u mask;

	diff = ((yuv1 & Ymask) - (yuv2 & Ymask));
	mask = diff >> 31; // -1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trY) return true;

	diff = ((yuv1 & Umask) - (yuv2 & Umask));
	mask = diff >> 31; // -1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trU) return true;

	diff = ((yuv1 & Vmask) - (yuv2 & Vmask));
	mask = diff >> 31; // -1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trV) return true;

	return false;
}

#endif

static inline void conc2d(InitLUTs,SBPP)(void)
{
	int r, g, b;
	int Y, u, v;

	_RGBtoYUV = (Bit32u *)malloc(65536 * sizeof(Bit32u));

	for (int color = 0; color < 65536; ++color) {
#if SBPP == 32
		r = ((color & 0xF800) >> 11) << (8 - 5);
		g = ((color & 0x07E0) >> 5) << (8 - 6);
		b = ((color & 0x001F) >> 0) << (8 - 5);
#else
		r = ((color & redMask) >> redShift) << (8 - redBits);
		g = ((color & greenMask) >> greenShift) << (8 - greenBits);
		b = ((color & blueMask) >> blueShift) << (8 - blueBits);
#endif
		Y = (r + g + b) >> 2;
		u = 128 + ((r - b) >> 2);
		v = 128 + ((-r + 2 * g - b) >> 3);
		_RGBtoYUV[color] = (Y << 16) | (u << 8) | v;
	}
}
