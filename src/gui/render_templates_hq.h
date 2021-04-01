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

/*
 * The HQ3x high quality 3x graphics filter.
 * Original author Maxim Stepin (see http://www.hiend3d.com/hq3x.html).
 * Adapted for DOSBox from ScummVM and HiEnd3D code by Kronuz.
 */

#include <stdlib.h>

#ifndef RENDER_TEMPLATES_HQNX_TABLE_H
#define RENDER_TEMPLATES_HQNX_TABLE_H

static uint32_t *_RGBtoYUV = 0;
static inline bool diffYUV(uint32_t yuv1, uint32_t yuv2)
{
	static const uint32_t Ymask = 0x00FF0000;
	static const uint32_t Umask = 0x0000FF00;
	static const uint32_t Vmask = 0x000000FF;
	static const uint32_t trY   = 0x00300000;
	static const uint32_t trU   = 0x00000700;
	static const uint32_t trV   = 0x00000006;

	uint32_t diff;
	uint32_t mask;

	diff = ((yuv1 & Ymask) - (yuv2 & Ymask));
	mask = (uint32_t)(((int32_t)diff) >> 31); // ~1/-1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trY) return true;

	diff = ((yuv1 & Umask) - (yuv2 & Umask));
	mask = (uint32_t)(((int32_t)diff) >> 31); // ~1/-1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trU) return true;

	diff = ((yuv1 & Vmask) - (yuv2 & Vmask));
	mask = (uint32_t)(((int32_t)diff) >> 31); // ~1/-1 if value < 0, 0 otherwise
	diff = (diff ^ mask) - mask; //-1: ~value + 1; 0: value
	if (diff > trV) return true;

	return false;
}

#endif

static inline void conc2d(InitLUTs,SBPP)(void)
{
# if !defined(_MSC_VER) /* Microsoft C++ thinks this is a failed attempt at a function call---it's not */
	(void)conc2d(InitLUTs,SBPP);
# endif

	_RGBtoYUV = (uint32_t *)malloc(65536 * sizeof(uint32_t));

	for (int color = 0; color < 65536; ++color) {
		int r, g, b;
#if SBPP == 32
		r = ((color & 0xF800) >> 11) << (8 - 5);
		g = ((color & 0x07E0) >> 5) << (8 - 6);
		b = ((color & 0x001F) >> 0) << (8 - 5);
#else
		r = ((color & redMask) >> redShift) << (8 - redBits);
		g = ((color & greenMask) >> greenShift) << (8 - greenBits);
		b = ((color & blueMask) >> blueShift) << (8 - blueBits);
#endif
		int Y = (r + g + b) >> 2;
		int u = 128 + ((r - b) >> 2);
		int v = 128 + ((-r + 2 * g - b) >> 3);
        if (_RGBtoYUV != NULL)
            _RGBtoYUV[color] = (uint32_t)((Y << 16) | (u << 8) | v);
        else
            E_Exit("Memory allocation failed in conc2d");
	}
}
