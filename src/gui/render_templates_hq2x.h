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
 * The HQ2x high quality 2x graphics filter.
 * Original author Maxim Stepin (see http://www.hiend3d.com/hq2x.html).
 * Adapted for DOSBox from ScummVM and HiEnd3D code by Kronuz.
 */

#ifndef RENDER_TEMPLATES_HQ2X_TABLE_H
#define RENDER_TEMPLATES_HQ2X_TABLE_H

#define PIXEL00_0	line0[0] = C4;
#define PIXEL00_10	line0[0] = interp_w2(C4,C0,3U,1U);
#define PIXEL00_11	line0[0] = interp_w2(C4,C3,3U,1U);
#define PIXEL00_12	line0[0] = interp_w2(C4,C1,3U,1U);
#define PIXEL00_20	line0[0] = interp_w3(C4,C3,C1,2U,1U,1U);
#define PIXEL00_21	line0[0] = interp_w3(C4,C0,C1,2U,1U,1U);
#define PIXEL00_22	line0[0] = interp_w3(C4,C0,C3,2U,1U,1U);
#define PIXEL00_60	line0[0] = interp_w3(C4,C1,C3,5U,2U,1U);
#define PIXEL00_61	line0[0] = interp_w3(C4,C3,C1,5U,2U,1U);
#define PIXEL00_70	line0[0] = interp_w3(C4,C3,C1,6U,1U,1U);
#define PIXEL00_90	line0[0] = interp_w3(C4,C3,C1,2U,3U,3U);
#define PIXEL00_100	line0[0] = interp_w3(C4,C3,C1,14U,1U,1U);

#define PIXEL01_0	line0[1] = C4;
#define PIXEL01_10	line0[1] = interp_w2(C4,C2,3U,1U);
#define PIXEL01_11	line0[1] = interp_w2(C4,C1,3U,1U);
#define PIXEL01_12	line0[1] = interp_w2(C4,C5,3U,1U);
#define PIXEL01_20	line0[1] = interp_w3(C4,C1,C5,2U,1U,1U);
#define PIXEL01_21	line0[1] = interp_w3(C4,C2,C5,2U,1U,1U);
#define PIXEL01_22	line0[1] = interp_w3(C4,C2,C1,2U,1U,1U);
#define PIXEL01_60	line0[1] = interp_w3(C4,C5,C1,5U,2U,1U);
#define PIXEL01_61	line0[1] = interp_w3(C4,C1,C5,5U,2U,1U);
#define PIXEL01_70	line0[1] = interp_w3(C4,C1,C5,6U,1U,1U);
#define PIXEL01_90	line0[1] = interp_w3(C4,C1,C5,2U,3U,3U);
#define PIXEL01_100	line0[1] = interp_w3(C4,C1,C5,14U,1U,1U);

#define PIXEL10_0	line1[0] = C4;
#define PIXEL10_10	line1[0] = interp_w2(C4,C6,3U,1U);
#define PIXEL10_11	line1[0] = interp_w2(C4,C7,3U,1U);
#define PIXEL10_12	line1[0] = interp_w2(C4,C3,3U,1U);
#define PIXEL10_20	line1[0] = interp_w3(C4,C7,C3,2U,1U,1U);
#define PIXEL10_21	line1[0] = interp_w3(C4,C6,C3,2U,1U,1U);
#define PIXEL10_22	line1[0] = interp_w3(C4,C6,C7,2U,1U,1U);
#define PIXEL10_60	line1[0] = interp_w3(C4,C3,C7,5U,2U,1U);
#define PIXEL10_61	line1[0] = interp_w3(C4,C7,C3,5U,2U,1U);
#define PIXEL10_70	line1[0] = interp_w3(C4,C7,C3,6U,1U,1U);
#define PIXEL10_90	line1[0] = interp_w3(C4,C7,C3,2U,3U,3U);
#define PIXEL10_100	line1[0] = interp_w3(C4,C7,C3,14U,1U,1U);

#define PIXEL11_0	line1[1] = C4;
#define PIXEL11_10	line1[1] = interp_w2(C4,C8,3U,1U);
#define PIXEL11_11	line1[1] = interp_w2(C4,C5,3U,1U);
#define PIXEL11_12	line1[1] = interp_w2(C4,C7,3U,1U);
#define PIXEL11_20	line1[1] = interp_w3(C4,C5,C7,2U,1U,1U);
#define PIXEL11_21	line1[1] = interp_w3(C4,C8,C7,2U,1U,1U);
#define PIXEL11_22	line1[1] = interp_w3(C4,C8,C5,2U,1U,1U);
#define PIXEL11_60	line1[1] = interp_w3(C4,C7,C5,5U,2U,1U);
#define PIXEL11_61	line1[1] = interp_w3(C4,C5,C7,5U,2U,1U);
#define PIXEL11_70	line1[1] = interp_w3(C4,C5,C7,6U,1U,1U);
#define PIXEL11_90	line1[1] = interp_w3(C4,C5,C7,2U,3U,3U);
#define PIXEL11_100	line1[1] = interp_w3(C4,C5,C7,14U,1U,1U);

#endif

#if SBPP == 32
#define RGBtoYUV(c) _RGBtoYUV[((c & 0xf80000) >> 8) | ((c & 0x00fc00) >> 5) | ((c & 0x0000f8) >> 3)]
#else
#define RGBtoYUV(c) _RGBtoYUV[c]
#endif

inline void conc2d(Hq2x,SBPP)(PTYPE * line0, PTYPE * line1, const PTYPE * fc)
{
	if (_RGBtoYUV == 0) conc2d(InitLUTs,SBPP)();

	Bit32u pattern = 0;
	const Bit32u YUV4 = RGBtoYUV(C4);
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
		PIXEL00_20
		PIXEL01_20
		PIXEL10_20
		PIXEL11_20
		break;
	case 2:
	case 34:
	case 130:
	case 162:
		PIXEL00_22
		PIXEL01_21
		PIXEL10_20
		PIXEL11_20
		break;
	case 16:
	case 17:
	case 48:
	case 49:
		PIXEL00_20
		PIXEL01_22
		PIXEL10_20
		PIXEL11_21
		break;
	case 64:
	case 65:
	case 68:
	case 69:
		PIXEL00_20
		PIXEL01_20
		PIXEL10_21
		PIXEL11_22
		break;
	case 8:
	case 12:
	case 136:
	case 140:
		PIXEL00_21
		PIXEL01_20
		PIXEL10_22
		PIXEL11_20
		break;
	case 3:
	case 35:
	case 131:
	case 163:
		PIXEL00_11
		PIXEL01_21
		PIXEL10_20
		PIXEL11_20
		break;
	case 6:
	case 38:
	case 134:
	case 166:
		PIXEL00_22
		PIXEL01_12
		PIXEL10_20
		PIXEL11_20
		break;
	case 20:
	case 21:
	case 52:
	case 53:
		PIXEL00_20
		PIXEL01_11
		PIXEL10_20
		PIXEL11_21
		break;
	case 144:
	case 145:
	case 176:
	case 177:
		PIXEL00_20
		PIXEL01_22
		PIXEL10_20
		PIXEL11_12
		break;
	case 192:
	case 193:
	case 196:
	case 197:
		PIXEL00_20
		PIXEL01_20
		PIXEL10_21
		PIXEL11_11
		break;
	case 96:
	case 97:
	case 100:
	case 101:
		PIXEL00_20
		PIXEL01_20
		PIXEL10_12
		PIXEL11_22
		break;
	case 40:
	case 44:
	case 168:
	case 172:
		PIXEL00_21
		PIXEL01_20
		PIXEL10_11
		PIXEL11_20
		break;
	case 9:
	case 13:
	case 137:
	case 141:
		PIXEL00_12
		PIXEL01_20
		PIXEL10_22
		PIXEL11_20
		break;
	case 18:
	case 50:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_20
		}
		PIXEL10_20
		PIXEL11_21
		break;
	case 80:
	case 81:
		PIXEL00_20
		PIXEL01_22
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_20
		}
		break;
	case 72:
	case 76:
		PIXEL00_21
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 10:
	case 138:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		PIXEL10_22
		PIXEL11_20
		break;
	case 66:
		PIXEL00_22
		PIXEL01_21
		PIXEL10_21
		PIXEL11_22
		break;
	case 24:
		PIXEL00_21
		PIXEL01_22
		PIXEL10_22
		PIXEL11_21
		break;
	case 7:
	case 39:
	case 135:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_20
		PIXEL11_20
		break;
	case 148:
	case 149:
	case 180:
		PIXEL00_20
		PIXEL01_11
		PIXEL10_20
		PIXEL11_12
		break;
	case 224:
	case 228:
	case 225:
		PIXEL00_20
		PIXEL01_20
		PIXEL10_12
		PIXEL11_11
		break;
	case 41:
	case 169:
	case 45:
		PIXEL00_12
		PIXEL01_20
		PIXEL10_11
		PIXEL11_20
		break;
	case 22:
	case 54:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_20
		PIXEL11_21
		break;
	case 208:
	case 209:
		PIXEL00_20
		PIXEL01_22
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 104:
	case 108:
		PIXEL00_21
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 11:
	case 139:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		PIXEL10_22
		PIXEL11_20
		break;
	case 19:
	case 51:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_11
			PIXEL01_10
		} else {
			PIXEL00_60
			PIXEL01_90
		}
		PIXEL10_20
		PIXEL11_21
		break;
	case 146:
	case 178:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
			PIXEL11_12
		} else {
			PIXEL01_90
			PIXEL11_61
		}
		PIXEL10_20
		break;
	case 84:
	case 85:
		PIXEL00_20
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL01_11
			PIXEL11_10
		} else {
			PIXEL01_60
			PIXEL11_90
		}
		PIXEL10_21
		break;
	case 112:
	case 113:
		PIXEL00_20
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL10_12
			PIXEL11_10
		} else {
			PIXEL10_61
			PIXEL11_90
		}
		break;
	case 200:
	case 204:
		PIXEL00_21
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
			PIXEL11_11
		} else {
			PIXEL10_90
			PIXEL11_60
		}
		break;
	case 73:
	case 77:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_12
			PIXEL10_10
		} else {
			PIXEL00_61
			PIXEL10_90
		}
		PIXEL01_20
		PIXEL11_22
		break;
	case 42:
	case 170:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
			PIXEL10_11
		} else {
			PIXEL00_90
			PIXEL10_60
		}
		PIXEL01_21
		PIXEL11_20
		break;
	case 14:
	case 142:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
			PIXEL01_12
		} else {
			PIXEL00_90
			PIXEL01_61
		}
		PIXEL10_22
		PIXEL11_20
		break;
	case 67:
		PIXEL00_11
		PIXEL01_21
		PIXEL10_21
		PIXEL11_22
		break;
	case 70:
		PIXEL00_22
		PIXEL01_12
		PIXEL10_21
		PIXEL11_22
		break;
	case 28:
		PIXEL00_21
		PIXEL01_11
		PIXEL10_22
		PIXEL11_21
		break;
	case 152:
		PIXEL00_21
		PIXEL01_22
		PIXEL10_22
		PIXEL11_12
		break;
	case 194:
		PIXEL00_22
		PIXEL01_21
		PIXEL10_21
		PIXEL11_11
		break;
	case 98:
		PIXEL00_22
		PIXEL01_21
		PIXEL10_12
		PIXEL11_22
		break;
	case 56:
		PIXEL00_21
		PIXEL01_22
		PIXEL10_11
		PIXEL11_21
		break;
	case 25:
		PIXEL00_12
		PIXEL01_22
		PIXEL10_22
		PIXEL11_21
		break;
	case 26:
	case 31:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_22
		PIXEL11_21
		break;
	case 82:
	case 214:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 88:
	case 248:
		PIXEL00_21
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 74:
	case 107:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 27:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_10
		PIXEL10_22
		PIXEL11_21
		break;
	case 86:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_21
		PIXEL11_10
		break;
	case 216:
		PIXEL00_21
		PIXEL01_22
		PIXEL10_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 106:
		PIXEL00_10
		PIXEL01_21
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 30:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_22
		PIXEL11_21
		break;
	case 210:
		PIXEL00_22
		PIXEL01_10
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 120:
		PIXEL00_21
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_10
		break;
	case 75:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		PIXEL10_10
		PIXEL11_22
		break;
	case 29:
		PIXEL00_12
		PIXEL01_11
		PIXEL10_22
		PIXEL11_21
		break;
	case 198:
		PIXEL00_22
		PIXEL01_12
		PIXEL10_21
		PIXEL11_11
		break;
	case 184:
		PIXEL00_21
		PIXEL01_22
		PIXEL10_11
		PIXEL11_12
		break;
	case 99:
		PIXEL00_11
		PIXEL01_21
		PIXEL10_12
		PIXEL11_22
		break;
	case 57:
		PIXEL00_12
		PIXEL01_22
		PIXEL10_11
		PIXEL11_21
		break;
	case 71:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_21
		PIXEL11_22
		break;
	case 156:
		PIXEL00_21
		PIXEL01_11
		PIXEL10_22
		PIXEL11_12
		break;
	case 226:
		PIXEL00_22
		PIXEL01_21
		PIXEL10_12
		PIXEL11_11
		break;
	case 60:
		PIXEL00_21
		PIXEL01_11
		PIXEL10_11
		PIXEL11_21
		break;
	case 195:
		PIXEL00_11
		PIXEL01_21
		PIXEL10_21
		PIXEL11_11
		break;
	case 102:
		PIXEL00_22
		PIXEL01_12
		PIXEL10_12
		PIXEL11_22
		break;
	case 153:
		PIXEL00_12
		PIXEL01_22
		PIXEL10_22
		PIXEL11_12
		break;
	case 58:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_11
		PIXEL11_21
		break;
	case 83:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 92:
		PIXEL00_21
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 202:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		PIXEL01_21
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		PIXEL11_11
		break;
	case 78:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		PIXEL11_22
		break;
	case 154:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_22
		PIXEL11_12
		break;
	case 114:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 89:
		PIXEL00_12
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 90:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 55:
	case 23:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_11
			PIXEL01_0
		} else {
			PIXEL00_60
			PIXEL01_90
		}
		PIXEL10_20
		PIXEL11_21
		break;
	case 182:
	case 150:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
			PIXEL11_12
		} else {
			PIXEL01_90
			PIXEL11_61
		}
		PIXEL10_20
		break;
	case 213:
	case 212:
		PIXEL00_20
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL01_11
			PIXEL11_0
		} else {
			PIXEL01_60
			PIXEL11_90
		}
		PIXEL10_21
		break;
	case 241:
	case 240:
		PIXEL00_20
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL10_12
			PIXEL11_0
		} else {
			PIXEL10_61
			PIXEL11_90
		}
		break;
	case 236:
	case 232:
		PIXEL00_21
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
			PIXEL11_11
		} else {
			PIXEL10_90
			PIXEL11_60
		}
		break;
	case 109:
	case 105:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_12
			PIXEL10_0
		} else {
			PIXEL00_61
			PIXEL10_90
		}
		PIXEL01_20
		PIXEL11_22
		break;
	case 171:
	case 43:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
			PIXEL10_11
		} else {
			PIXEL00_90
			PIXEL10_60
		}
		PIXEL01_21
		PIXEL11_20
		break;
	case 143:
	case 15:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
			PIXEL01_12
		} else {
			PIXEL00_90
			PIXEL01_61
		}
		PIXEL10_22
		PIXEL11_20
		break;
	case 124:
		PIXEL00_21
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_10
		break;
	case 203:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		PIXEL10_10
		PIXEL11_11
		break;
	case 62:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_11
		PIXEL11_21
		break;
	case 211:
		PIXEL00_11
		PIXEL01_10
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 118:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_12
		PIXEL11_10
		break;
	case 217:
		PIXEL00_12
		PIXEL01_22
		PIXEL10_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 110:
		PIXEL00_10
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 155:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_10
		PIXEL10_22
		PIXEL11_12
		break;
	case 188:
		PIXEL00_21
		PIXEL01_11
		PIXEL10_11
		PIXEL11_12
		break;
	case 185:
		PIXEL00_12
		PIXEL01_22
		PIXEL10_11
		PIXEL11_12
		break;
	case 61:
		PIXEL00_12
		PIXEL01_11
		PIXEL10_11
		PIXEL11_21
		break;
	case 157:
		PIXEL00_12
		PIXEL01_11
		PIXEL10_22
		PIXEL11_12
		break;
	case 103:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_12
		PIXEL11_22
		break;
	case 227:
		PIXEL00_11
		PIXEL01_21
		PIXEL10_12
		PIXEL11_11
		break;
	case 230:
		PIXEL00_22
		PIXEL01_12
		PIXEL10_12
		PIXEL11_11
		break;
	case 199:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_21
		PIXEL11_11
		break;
	case 220:
		PIXEL00_21
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 158:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_22
		PIXEL11_12
		break;
	case 234:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		PIXEL01_21
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_11
		break;
	case 242:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 59:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_11
		PIXEL11_21
		break;
	case 121:
		PIXEL00_12
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 87:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 79:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		PIXEL11_22
		break;
	case 122:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 94:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 218:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 91:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 229:
		PIXEL00_20
		PIXEL01_20
		PIXEL10_12
		PIXEL11_11
		break;
	case 167:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_20
		PIXEL11_20
		break;
	case 173:
		PIXEL00_12
		PIXEL01_20
		PIXEL10_11
		PIXEL11_20
		break;
	case 181:
		PIXEL00_20
		PIXEL01_11
		PIXEL10_20
		PIXEL11_12
		break;
	case 186:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_11
		PIXEL11_12
		break;
	case 115:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 93:
		PIXEL00_12
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 206:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		PIXEL11_11
		break;
	case 205:
	case 201:
		PIXEL00_12
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_10
		} else {
			PIXEL10_70
		}
		PIXEL11_11
		break;
	case 174:
	case 46:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_10
		} else {
			PIXEL00_70
		}
		PIXEL01_12
		PIXEL10_11
		PIXEL11_20
		break;
	case 179:
	case 147:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_10
		} else {
			PIXEL01_70
		}
		PIXEL10_20
		PIXEL11_12
		break;
	case 117:
	case 116:
		PIXEL00_20
		PIXEL01_11
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_10
		} else {
			PIXEL11_70
		}
		break;
	case 189:
		PIXEL00_12
		PIXEL01_11
		PIXEL10_11
		PIXEL11_12
		break;
	case 231:
		PIXEL00_11
		PIXEL01_12
		PIXEL10_12
		PIXEL11_11
		break;
	case 126:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_10
		break;
	case 219:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_10
		PIXEL10_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 125:
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL00_12
			PIXEL10_0
		} else {
			PIXEL00_61
			PIXEL10_90
		}
		PIXEL01_11
		PIXEL11_10
		break;
	case 221:
		PIXEL00_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL01_11
			PIXEL11_0
		} else {
			PIXEL01_60
			PIXEL11_90
		}
		PIXEL10_10
		break;
	case 207:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
			PIXEL01_12
		} else {
			PIXEL00_90
			PIXEL01_61
		}
		PIXEL10_10
		PIXEL11_11
		break;
	case 238:
		PIXEL00_10
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
			PIXEL11_11
		} else {
			PIXEL10_90
			PIXEL11_60
		}
		break;
	case 190:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
			PIXEL11_12
		} else {
			PIXEL01_90
			PIXEL11_61
		}
		PIXEL10_11
		break;
	case 187:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
			PIXEL10_11
		} else {
			PIXEL00_90
			PIXEL10_60
		}
		PIXEL01_10
		PIXEL11_12
		break;
	case 243:
		PIXEL00_11
		PIXEL01_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL10_12
			PIXEL11_0
		} else {
			PIXEL10_61
			PIXEL11_90
		}
		break;
	case 119:
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL00_11
			PIXEL01_0
		} else {
			PIXEL00_60
			PIXEL01_90
		}
		PIXEL10_12
		PIXEL11_10
		break;
	case 237:
	case 233:
		PIXEL00_12
		PIXEL01_20
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		PIXEL11_11
		break;
	case 175:
	case 47:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		PIXEL01_12
		PIXEL10_11
		PIXEL11_20
		break;
	case 183:
	case 151:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_20
		PIXEL11_12
		break;
	case 245:
	case 244:
		PIXEL00_20
		PIXEL01_11
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 250:
		PIXEL00_10
		PIXEL01_10
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 123:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_10
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_10
		break;
	case 95:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_10
		PIXEL11_10
		break;
	case 222:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 252:
		PIXEL00_21
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 249:
		PIXEL00_12
		PIXEL01_22
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 235:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_21
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		PIXEL11_11
		break;
	case 111:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_22
		break;
	case 63:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_11
		PIXEL11_21
		break;
	case 159:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_22
		PIXEL11_12
		break;
	case 215:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_21
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 246:
		PIXEL00_22
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 254:
		PIXEL00_10
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 253:
		PIXEL00_12
		PIXEL01_11
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 251:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		PIXEL01_10
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 239:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		PIXEL01_12
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		PIXEL11_11
		break;
	case 127:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_20
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_20
		}
		PIXEL11_10
		break;
	case 191:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_11
		PIXEL11_12
		break;
	case 223:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_20
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_10
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_20
		}
		break;
	case 247:
		PIXEL00_11
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		PIXEL10_12
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	case 255:
		if (diffYUV(RGBtoYUV(C3), RGBtoYUV(C1))) {
			PIXEL00_0
		} else {
			PIXEL00_100
		}
		if (diffYUV(RGBtoYUV(C1), RGBtoYUV(C5))) {
			PIXEL01_0
		} else {
			PIXEL01_100
		}
		if (diffYUV(RGBtoYUV(C7), RGBtoYUV(C3))) {
			PIXEL10_0
		} else {
			PIXEL10_100
		}
		if (diffYUV(RGBtoYUV(C5), RGBtoYUV(C7))) {
			PIXEL11_0
		} else {
			PIXEL11_100
		}
		break;
	}
}

#undef RGBtoYUV
