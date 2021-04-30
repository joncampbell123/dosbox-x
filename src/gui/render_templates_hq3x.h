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

#ifndef RENDER_TEMPLATES_HQ3X_TABLE_H
#define RENDER_TEMPLATES_HQ3X_TABLE_H

#define PIXEL00_1M  line0[0] = interp_w2(C4,C0,3U,1U);
#define PIXEL00_1U  line0[0] = interp_w2(C4,C1,3U,1U);
#define PIXEL00_1L  line0[0] = interp_w2(C4,C3,3U,1U);
#define PIXEL00_2   line0[0] = interp_w3(C4,C3,C1,2U,1U,1U);
#define PIXEL00_4   line0[0] = interp_w3(C4,C3,C1,2U,7U,7U);
#define PIXEL00_5   line0[0] = interp_w2(C3,C1,1U,1U);
#define PIXEL00_C   line0[0] = C4;

#define PIXEL01_1   line0[1] = interp_w2(C4,C1,3U,1U);
#define PIXEL01_3   line0[1] = interp_w2(C4,C1,7U,1U);
#define PIXEL01_6   line0[1] = interp_w2(C1,C4,3U,1U);
#define PIXEL01_C   line0[1] = C4;

#define PIXEL02_1M  line0[2] = interp_w2(C4,C2,3U,1U);
#define PIXEL02_1U  line0[2] = interp_w2(C4,C1,3U,1U);
#define PIXEL02_1R  line0[2] = interp_w2(C4,C5,3U,1U);
#define PIXEL02_2   line0[2] = interp_w3(C4,C1,C5,2U,1U,1U);
#define PIXEL02_4   line0[2] = interp_w3(C4,C1,C5,2U,7U,7U);
#define PIXEL02_5   line0[2] = interp_w2(C1,C5,1U,1U);
#define PIXEL02_C   line0[2] = C4;

#define PIXEL10_1   line1[0] = interp_w2(C4,C3,3U,1U);
#define PIXEL10_3   line1[0] = interp_w2(C4,C3,7U,1U);
#define PIXEL10_6   line1[0] = interp_w2(C3,C4,3U,1U);
#define PIXEL10_C   line1[0] = C4;

#define PIXEL11     line1[1] = C4;

#define PIXEL12_1   line1[2] = interp_w2(C4,C5,3U,1U);
#define PIXEL12_3   line1[2] = interp_w2(C4,C5,7U,1U);
#define PIXEL12_6   line1[2] = interp_w2(C5,C4,3U,1U);
#define PIXEL12_C   line1[2] = C4;

#define PIXEL20_1M  line2[0] = interp_w2(C4,C6,3U,1U);
#define PIXEL20_1D  line2[0] = interp_w2(C4,C7,3U,1U);
#define PIXEL20_1L  line2[0] = interp_w2(C4,C3,3U,1U);
#define PIXEL20_2   line2[0] = interp_w3(C4,C7,C3,2U,1U,1U);
#define PIXEL20_4   line2[0] = interp_w3(C4,C7,C3,2U,7U,7U);
#define PIXEL20_5   line2[0] = interp_w2(C7,C3,1U,1U);
#define PIXEL20_C   line2[0] = C4;

#define PIXEL21_1   line2[1] = interp_w2(C4,C7,3U,1U);
#define PIXEL21_3   line2[1] = interp_w2(C4,C7,7U,1U);
#define PIXEL21_6   line2[1] = interp_w2(C7,C4,3U,1U);
#define PIXEL21_C   line2[1] = C4;

#define PIXEL22_1M  line2[2] = interp_w2(C4,C8,3U,1U);
#define PIXEL22_1D  line2[2] = interp_w2(C4,C7,3U,1U);
#define PIXEL22_1R  line2[2] = interp_w2(C4,C5,3U,1U);
#define PIXEL22_2   line2[2] = interp_w3(C4,C5,C7,2U,1U,1U);
#define PIXEL22_4   line2[2] = interp_w3(C4,C5,C7,2U,7U,7U);
#define PIXEL22_5   line2[2] = interp_w2(C5,C7,1U,1U);
#define PIXEL22_C   line2[2] = C4;

#endif

#if SBPP == 32
#define RGBtoYUV(c) _RGBtoYUV[((c & 0xf80000) >> 8) | ((c & 0x00fc00) >> 5) | ((c & 0x0000f8) >> 3)]
#else
#define RGBtoYUV(c) _RGBtoYUV[c]
#endif

inline void conc2d(Hq3x,SBPP)(PTYPE * line0, PTYPE * line1, PTYPE * line2, const PTYPE * fc)
{
# if !defined(_MSC_VER) /* Microsoft C++ thinks this is a failed attempt at a function call---it's not */
	(void)conc2d(Hq3x,SBPP);
# endif

	if (_RGBtoYUV == 0) conc2d(InitLUTs,SBPP)();

	uint32_t pattern = 0;
	const uint32_t YUV4 = RGBtoYUV(C4);

	if (C4 != C0 && diffYUV(YUV4, RGBtoYUV(C0))) pattern |= 0x0001;
	if (C4 != C1 && diffYUV(YUV4, RGBtoYUV(C1))) pattern |= 0x0002;
	if (C4 != C2 && diffYUV(YUV4, RGBtoYUV(C2))) pattern |= 0x0004;
	if (C4 != C3 && diffYUV(YUV4, RGBtoYUV(C3))) pattern |= 0x0008;
	if (C4 != C5 && diffYUV(YUV4, RGBtoYUV(C5))) pattern |= 0x0010;
	if (C4 != C6 && diffYUV(YUV4, RGBtoYUV(C6))) pattern |= 0x0020;
	if (C4 != C7 && diffYUV(YUV4, RGBtoYUV(C7))) pattern |= 0x0040;
	if (C4 != C8 && diffYUV(YUV4, RGBtoYUV(C8))) pattern |= 0x0080;

	switch (pattern) {
	case 0:
	case 1:
	case 4:
	case 32:
	case 128:
	case 5:
	case 132:
	case 160:
	case 33:
	case 129:
	case 36:
	case 133:
	case 164:
	case 161:
	case 37:
	case 165:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 2:
	case 34:
	case 130:
	case 162:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 16:
	case 17:
	case 48:
	case 49:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 64:
	case 65:
	case 68:
	case 69:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 8:
	case 12:
	case 136:
	case 140:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 3:
	case 35:
	case 131:
	case 163:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 6:
	case 38:
	case 134:
	case 166:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 20:
	case 21:
	case 52:
	case 53:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 144:
	case 145:
	case 176:
	case 177:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1D
		break;
	case 192:
	case 193:
	case 196:
	case 197:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 96:
	case 97:
	case 100:
	case 101:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 40:
	case 44:
	case 168:
	case 172:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_2
		break;
	case 9:
	case 13:
	case 137:
	case 141:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 18:
	case 50:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_1M
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 80:
	case 81:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_1M
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 72:
	case 76:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_1M
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 10:
	case 138:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 66:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 24:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 7:
	case 39:
	case 135:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 148:
	case 149:
	case 180:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1D
		break;
	case 224:
	case 228:
	case 225:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 41:
	case 169:
	case 45:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_2
		break;
	case 22:
	case 54:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 208:
	case 209:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 104:
	case 108:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 11:
	case 139:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 19:
	case 51:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_1L
			PIXEL01_C
			PIXEL02_1M
			PIXEL12_C
		} else {
			PIXEL00_2
			PIXEL01_6
			PIXEL02_5
			PIXEL12_1
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 146:
	case 178:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_1M
			PIXEL12_C
			PIXEL22_1D
		} else {
			PIXEL01_1
			PIXEL02_5
			PIXEL12_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		break;
	case 84:
	case 85:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL02_1U
			PIXEL12_C
			PIXEL21_C
			PIXEL22_1M
		} else {
			PIXEL02_2
			PIXEL12_6
			PIXEL21_1
			PIXEL22_5
		}
		PIXEL00_2
		PIXEL01_1
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		break;
	case 112:
	case 113:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL20_1L
			PIXEL21_C
			PIXEL22_1M
		} else {
			PIXEL12_1
			PIXEL20_2
			PIXEL21_6
			PIXEL22_5
		}
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		break;
	case 200:
	case 204:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_1M
			PIXEL21_C
			PIXEL22_1R
		} else {
			PIXEL10_1
			PIXEL20_5
			PIXEL21_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		break;
	case 73:
	case 77:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_1U
			PIXEL10_C
			PIXEL20_1M
			PIXEL21_C
		} else {
			PIXEL00_2
			PIXEL10_6
			PIXEL20_5
			PIXEL21_1
		}
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		PIXEL22_1M
		break;
	case 42:
	case 170:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
			PIXEL01_C
			PIXEL10_C
			PIXEL20_1D
		} else {
			PIXEL00_5
			PIXEL01_1
			PIXEL10_6
			PIXEL20_2
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL21_1
		PIXEL22_2
		break;
	case 14:
	case 142:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
			PIXEL01_C
			PIXEL02_1R
			PIXEL10_C
		} else {
			PIXEL00_5
			PIXEL01_6
			PIXEL02_2
			PIXEL10_1
		}
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 67:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 70:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 28:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 152:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 194:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 98:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 56:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 25:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 26:
	case 31:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL10_3
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL11
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 82:
	case 214:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
		} else {
			PIXEL01_3
			PIXEL02_4
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 88:
	case 248:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
		} else {
			PIXEL10_3
			PIXEL20_4
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL22_4
		}
		break;
	case 74:
	case 107:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
		} else {
			PIXEL00_4
			PIXEL01_3
		}
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 27:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 86:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 216:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 106:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 30:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_C
		PIXEL11
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 210:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 120:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 75:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 29:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1M
		break;
	case 198:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 184:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 99:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 57:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 71:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 156:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 226:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 60:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 195:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 102:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 153:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 58:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 83:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 92:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 202:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 78:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1M
		break;
	case 154:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 114:
		PIXEL00_1M
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 89:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 90:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 55:
	case 23:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_1L
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL00_2
			PIXEL01_6
			PIXEL02_5
			PIXEL12_1
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1M
		break;
	case 182:
	case 150:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
			PIXEL22_1D
		} else {
			PIXEL01_1
			PIXEL02_5
			PIXEL12_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_2
		PIXEL21_1
		break;
	case 213:
	case 212:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL02_1U
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL02_2
			PIXEL12_6
			PIXEL21_1
			PIXEL22_5
		}
		PIXEL00_2
		PIXEL01_1
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		break;
	case 241:
	case 240:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL20_1L
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_1
			PIXEL20_2
			PIXEL21_6
			PIXEL22_5
		}
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		break;
	case 236:
	case 232:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
			PIXEL22_1R
		} else {
			PIXEL10_1
			PIXEL20_5
			PIXEL21_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		break;
	case 109:
	case 105:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_1U
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL00_2
			PIXEL10_6
			PIXEL20_5
			PIXEL21_1
		}
		PIXEL01_1
		PIXEL02_2
		PIXEL11
		PIXEL12_1
		PIXEL22_1M
		break;
	case 171:
	case 43:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
			PIXEL20_1D
		} else {
			PIXEL00_5
			PIXEL01_1
			PIXEL10_6
			PIXEL20_2
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL21_1
		PIXEL22_2
		break;
	case 143:
	case 15:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL02_1R
			PIXEL10_C
		} else {
			PIXEL00_5
			PIXEL01_6
			PIXEL02_2
			PIXEL10_1
		}
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_2
		break;
	case 124:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 203:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 62:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_C
		PIXEL11
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 211:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 118:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 217:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 110:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 155:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 188:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 185:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 61:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 157:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 103:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 227:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 230:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 199:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 220:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 158:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_C
		PIXEL11
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 234:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1M
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1R
		break;
	case 242:
		PIXEL00_1M
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_1L
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 59:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 121:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 87:
		PIXEL00_1L
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_1M
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 79:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1R
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1M
		break;
	case 122:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 94:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_C
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 218:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 91:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 229:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_2
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 167:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_2
		PIXEL21_1
		PIXEL22_2
		break;
	case 173:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_2
		break;
	case 181:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1D
		break;
	case 186:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 115:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 93:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 206:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 205:
	case 201:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_1M
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 174:
	case 46:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_1M
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_2
		break;
	case 179:
	case 147:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_1M
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1D
		break;
	case 117:
	case 116:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_1M
		} else {
			PIXEL22_2
		}
		break;
	case 189:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 231:
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_1
		PIXEL11
		PIXEL12_1
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1R
		break;
	case 126:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 219:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL01_3
			PIXEL10_3
		}
		PIXEL02_1M
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 125:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_1U
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL00_2
			PIXEL10_6
			PIXEL20_5
			PIXEL21_1
		}
		PIXEL01_1
		PIXEL02_1U
		PIXEL11
		PIXEL12_C
		PIXEL22_1M
		break;
	case 221:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL02_1U
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL02_2
			PIXEL12_6
			PIXEL21_1
			PIXEL22_5
		}
		PIXEL00_1U
		PIXEL01_1
		PIXEL10_C
		PIXEL11
		PIXEL20_1M
		break;
	case 207:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL02_1R
			PIXEL10_C
		} else {
			PIXEL00_5
			PIXEL01_6
			PIXEL02_2
			PIXEL10_1
		}
		PIXEL11
		PIXEL12_1
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1R
		break;
	case 238:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
			PIXEL22_1R
		} else {
			PIXEL10_1
			PIXEL20_5
			PIXEL21_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1R
		PIXEL11
		PIXEL12_1
		break;
	case 190:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
			PIXEL22_1D
		} else {
			PIXEL01_1
			PIXEL02_5
			PIXEL12_6
			PIXEL22_2
		}
		PIXEL00_1M
		PIXEL10_C
		PIXEL11
		PIXEL20_1D
		PIXEL21_1
		break;
	case 187:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
			PIXEL20_1D
		} else {
			PIXEL00_5
			PIXEL01_1
			PIXEL10_6
			PIXEL20_2
		}
		PIXEL02_1M
		PIXEL11
		PIXEL12_C
		PIXEL21_1
		PIXEL22_1D
		break;
	case 243:
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL20_1L
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_1
			PIXEL20_2
			PIXEL21_6
			PIXEL22_5
		}
		PIXEL00_1L
		PIXEL01_C
		PIXEL02_1M
		PIXEL10_1
		PIXEL11
		break;
	case 119:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_1L
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL00_2
			PIXEL01_6
			PIXEL02_5
			PIXEL12_1
		}
		PIXEL10_1
		PIXEL11
		PIXEL20_1L
		PIXEL21_C
		PIXEL22_1M
		break;
	case 237:
	case 233:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_2
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 175:
	case 47:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_2
		break;
	case 183:
	case 151:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_2
		PIXEL21_1
		PIXEL22_1D
		break;
	case 245:
	case 244:
		PIXEL00_2
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	case 250:
		PIXEL00_1M
		PIXEL01_C
		PIXEL02_1M
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
		} else {
			PIXEL10_3
			PIXEL20_4
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL22_4
		}
		break;
	case 123:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
		} else {
			PIXEL00_4
			PIXEL01_3
		}
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 95:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL10_3
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL11
		PIXEL20_1M
		PIXEL21_C
		PIXEL22_1M
		break;
	case 222:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
		} else {
			PIXEL01_3
			PIXEL02_4
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 252:
		PIXEL00_1M
		PIXEL01_1
		PIXEL02_1U
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
		} else {
			PIXEL10_3
			PIXEL20_4
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	case 249:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL22_4
		}
		break;
	case 235:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
		} else {
			PIXEL00_4
			PIXEL01_3
		}
		PIXEL02_1M
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 111:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 63:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL10_C
		PIXEL11
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1M
		break;
	case 159:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL10_3
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		PIXEL21_1
		PIXEL22_1D
		break;
	case 215:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 246:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
		} else {
			PIXEL01_3
			PIXEL02_4
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	case 254:
		PIXEL00_1M
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
		} else {
			PIXEL01_3
			PIXEL02_4
		}
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
		} else {
			PIXEL10_3
			PIXEL20_4
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL21_3
			PIXEL22_2
		}
		break;
	case 253:
		PIXEL00_1U
		PIXEL01_1
		PIXEL02_1U
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	case 251:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
		} else {
			PIXEL00_4
			PIXEL01_3
		}
		PIXEL02_1M
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_C
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL10_3
			PIXEL20_2
			PIXEL21_3
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL12_C
			PIXEL22_C
		} else {
			PIXEL12_3
			PIXEL22_4
		}
		break;
	case 239:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		PIXEL02_1R
		PIXEL10_C
		PIXEL11
		PIXEL12_1
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		PIXEL22_1R
		break;
	case 127:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL01_C
			PIXEL10_C
		} else {
			PIXEL00_2
			PIXEL01_3
			PIXEL10_3
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL02_4
			PIXEL12_3
		}
		PIXEL11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
			PIXEL21_C
		} else {
			PIXEL20_4
			PIXEL21_3
		}
		PIXEL22_1M
		break;
	case 191:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		PIXEL20_1D
		PIXEL21_1
		PIXEL22_1D
		break;
	case 223:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
			PIXEL10_C
		} else {
			PIXEL00_4
			PIXEL10_3
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_C
			PIXEL02_C
			PIXEL12_C
		} else {
			PIXEL01_3
			PIXEL02_2
			PIXEL12_3
		}
		PIXEL11
		PIXEL20_1M
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL21_C
			PIXEL22_C
		} else {
			PIXEL21_3
			PIXEL22_4
		}
		break;
	case 247:
		PIXEL00_1L
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL10_1
		PIXEL11
		PIXEL12_C
		PIXEL20_1L
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	case 255:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_C
		} else {
			PIXEL00_2
		}
		PIXEL01_C
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL02_C
		} else {
			PIXEL02_2
		}
		PIXEL10_C
		PIXEL11
		PIXEL12_C
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL20_C
		} else {
			PIXEL20_2
		}
		PIXEL21_C
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL22_C
		} else {
			PIXEL22_2
		}
		break;
	}
}

#undef RGBtoYUV
