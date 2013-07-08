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

static inline int conc2d(GetResult,SBPP)(PTYPE A, PTYPE B, PTYPE C, PTYPE D) {
	const bool ac = (A==C);
	const bool bc = (B==C);
	const int x1 = ac;
	const int y1 = (bc & !ac);
	const bool ad = (A==D);
	const bool bd = (B==D);
	const int x2 = ad;
	const int y2 = (bd & !ad);
	const int x = x1+x2;
	const int y = y1+y2;
	static const int rmap[3][3] = {
		{0, 0, -1},
		{0, 0, -1},
		{1, 1,  0}
	};
	return rmap[y][x];
}

inline void conc2d(Super2xSaI,SBPP)(PTYPE * line0, PTYPE * line1, const PTYPE * fc)
{
	//--------------------------------------
	if (C7 == C5 && C4 != C8) {
		line1[1] = line0[1] = C7;
	} else if (C4 == C8 && C7 != C5) {
		line1[1] = line0[1] = C4;
	} else if (C4 == C8 && C7 == C5) {
		register int r = 0;
		r += conc2d(GetResult,SBPP)(C5,C4,C6,D1);
		r += conc2d(GetResult,SBPP)(C5,C4,C3,C1);
		r += conc2d(GetResult,SBPP)(C5,C4,D2,D5);
		r += conc2d(GetResult,SBPP)(C5,C4,C2,D4);

		if (r > 0)
			line1[1] = line0[1] = C5;
		else if (r < 0)
			line1[1] = line0[1] = C4;
		else {
			line1[1] = line0[1] = interp_w2(C4,C5,1U,1U);
		}
	} else {
		if (C5 == C8 && C8 == D1 && C7 != D2 && C8 != D0)
			line1[1] = interp_w2(C8,C7,3U,1U);
		else if (C4 == C7 && C7 == D2 && D1 != C8 && C7 != D6)
			line1[1] = interp_w2(C7,C8,3U,1U);
		else
			line1[1] = interp_w2(C7,C8,1U,1U);

		if (C5 == C8 && C5 == C1 && C4 != C2 && C5 != C0)
			line0[1] = interp_w2(C5,C4,3U,1U);
		else if (C4 == C7 && C4 == C2 && C1 != C5 && C4 != D3)
			line0[1] = interp_w2(C4,C5,3U,1U);
		else
			line0[1] = interp_w2(C4,C5,1U,1U);
	}

	if (C4 == C8 && C7 != C5 && C3 == C4 && C4 != D2)
		line1[0] = interp_w2(C7,C4,1U,1U);
	else if (C4 == C6 && C5 == C4 && C3 != C7 && C4 != D0)
		line1[0] = interp_w2(C7,C4,1U,1U);
	else
		line1[0] = C7;

	if (C7 == C5 && C4 != C8 && C6 == C7 && C7 != C2)
		line0[0] = interp_w2(C7,C4,1U,1U);
	else if (C3 == C7 && C8 == C7 && C6 != C4 && C7 != C0)
		line0[0] = interp_w2(C7,C4,1U,1U);
	else
		line0[0] = C4;
}

inline void conc2d(SuperEagle,SBPP)(PTYPE * line0, PTYPE * line1, const PTYPE * fc)
{
	// --------------------------------------
	if (C4 != C8) {
		if (C7 == C5) {
			line0[1] = line1[0] = C7;
			if ((C6 == C7) || (C5 == C2)) {
				line0[0] = interp_w2(C7,C4,3U,1U);
			} else {
				line0[0] = interp_w2(C4,C5,1U,1U);
			}

			if ((C5 == D4) || (C7 == D1)) {
				line1[1] = interp_w2(C7,C8,3U,1U);
			} else {
				line1[1] = interp_w2(C7,C8,1U,1U);
			}
		} else {
			line1[1] = interp_w3(C8,C7,C5,6U,1U,1U);
			line0[0] = interp_w3(C4,C7,C5,6U,1U,1U);

			line1[0] = interp_w3(C7,C4,C8,6U,1U,1U);
			line0[1] = interp_w3(C5,C4,C8,6U,1U,1U);
		}
	} else {
		if (C7 != C5) {
			line1[1] = line0[0] = C4;

			if ((C1 == C4) || (C8 == D5)) {
				line0[1] = interp_w2(C4,C5,3U,1U);
			} else {
				line0[1] = interp_w2(C4,C5,1U,1U);
			}

			if ((C8 == D2) || (C3 == C4)) {
				line1[0] = interp_w2(C4,C7,3U,1U);
			} else {
				line1[0] = interp_w2(C7,C8,1U,1U);
			}
		} else {
			register int r = 0;
			r += conc2d(GetResult,SBPP)(C5,C4,C6,D1);
			r += conc2d(GetResult,SBPP)(C5,C4,C3,C1);
			r += conc2d(GetResult,SBPP)(C5,C4,D2,D5);
			r += conc2d(GetResult,SBPP)(C5,C4,C2,D4);

			if (r > 0) {
				line0[1] = line1[0] = C7;
				line0[0] = line1[1] = interp_w2(C4,C5,1U,1U);
			} else if (r < 0) {
				line1[1] = line0[0] = C4;
				line0[1] = line1[0] = interp_w2(C4,C5,1U,1U);
			} else {
				line1[1] = line0[0] = C4;
				line0[1] = line1[0] = C7;
			}
		}
	}
}

inline void conc2d(_2xSaI,SBPP)(PTYPE * line0, PTYPE * line1, const PTYPE * fc)
{
	if ((C4 == C8) && (C5 != C7)) {
		if (((C4 == C1) && (C5 == D5)) ||
			((C4 == C7) && (C4 == C2) && (C5 != C1) && (C5 == D3))) {
				line0[1] = C4;
		} else {
			line0[1] = interp_w2(C4,C5,1U,1U);
		}

		if (((C4 == C3) && (C7 == D2)) ||
			((C4 == C5) && (C4 == C6) && (C3 != C7)  && (C7 == D0))) {
				line1[0] = C4;
		} else {
			line1[0] = interp_w2(C4,C7,1U,1U);
		}
		line1[1] = C4;
	} else if ((C5 == C7) && (C4 != C8)) {
		if (((C5 == C2) && (C4 == C6)) ||
			((C5 == C1) && (C5 == C8) && (C4 != C2) && (C4 == C0))) {
				line0[1] = C5;
		} else {
			line0[1] = interp_w2(C4,C5,1U,1U);
		}

		if (((C7 == C6) && (C4 == C2)) ||
			((C7 == C3) && (C7 == C8) && (C4 != C6) && (C4 == C0))) {
				line1[0] = C7;
		} else {
			line1[0] = interp_w2(C4,C7,1U,1U);
		}
		line1[1] = C5;
	} else if ((C4 == C8) && (C5 == C7)) {
		if (C4 == C5) {
			line0[1] = C4;
			line1[0] = C4;
			line1[1] = C4;
		} else {
			register int r = 0;
			r += conc2d(GetResult,SBPP)(C4,C5,C3,C1);
			r -= conc2d(GetResult,SBPP)(C5,C4,D4,C2);
			r -= conc2d(GetResult,SBPP)(C5,C4,C6,D1);
			r += conc2d(GetResult,SBPP)(C4,C5,D5,D2);

			if (r > 0)
				line1[1] = C4;
			else if (r < 0)
				line1[1] = C5;
			else {
				line1[1] = interp_w4(C4,C5,C7,C8,1U,1U,1U,1U);
			}

			line1[0] = interp_w2(C4,C7,1U,1U);
			line0[1] = interp_w2(C4,C5,1U,1U);
		}
	} else {
		line1[1] = interp_w4(C4,C5,C7,C8,1U,1U,1U,1U);

		if ((C4 == C7) && (C4 == C2)
			&& (C5 != C1) && (C5 == D3)) {
				line0[1] = C4;
		} else if ((C5 == C1) && (C5 == C8)
			&& (C4 != C2) && (C4 == C0)) {
				line0[1] = C5;
		} else {
			line0[1] = interp_w2(C4,C5,1U,1U);
		}

		if ((C4 == C5) && (C4 == C6)
			&& (C3 != C7) && (C7 == D0)) {
				line1[0] = C4;
		} else if ((C7 == C3) && (C7 == C8)
			&& (C4 != C6) && (C4 == C0)) {
				line1[0] = C7;
		} else {
			line1[0] = interp_w2(C4,C7,1U,1U);
		}
	}
	line0[0] = C4;
}
