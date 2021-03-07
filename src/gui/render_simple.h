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

#undef conc4d_func
#undef conc4d_sub_func
#if defined (SCALERLINEAR)
# define conc4d_func        conc4d(SCALERNAME,SBPP,DBPP,L)
# define conc4d_sub_func    conc4d(SCALERNAME,SBPP,DBPP,Lsub)
#else
# define conc4d_func        conc4d(SCALERNAME,SBPP,DBPP,R)
# define conc4d_sub_func    conc4d(SCALERNAME,SBPP,DBPP,Rsub)
#endif

static inline void conc4d_sub_func(const SRCTYPE* &src, SRCTYPE* &cache, PTYPE* &line0, const unsigned int block_proc, Bitu &hadChange) {
#if !defined(C_SCALER_FULL_LINE)
        if (memcmp(src,cache,block_proc * sizeof(SRCTYPE)) == 0
# if (SBPP == 9)
            && !(
			render.pal.modified[src[0]] | 
			render.pal.modified[src[1]] | 
			render.pal.modified[src[2]] | 
			render.pal.modified[src[3]] |
            render.pal.modified[src[4]] | 
            render.pal.modified[src[5]] | 
			render.pal.modified[src[6]] | 
			render.pal.modified[src[7]])
# endif
            ) {
			src   += block_proc;
			cache += block_proc;
			line0 += block_proc*SCALERWIDTH;
		}
        else
#endif
        {
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1) 
			PTYPE *line1 = WC[0];
#endif
#if (SCALERHEIGHT > 2) 
			PTYPE *line2 = WC[1];
#endif
#if (SCALERHEIGHT > 3) 
			PTYPE *line3 = WC[2];
#endif
#if (SCALERHEIGHT > 4) 
			PTYPE *line4 = WC[3];
#endif
#if (SCALERHEIGHT > 5) 
			PTYPE *line5 = WC[4];
#endif
#else
#if (SCALERHEIGHT > 1) 
		PTYPE *line1 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch);
#endif
#if (SCALERHEIGHT > 2) 
		PTYPE *line2 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch * 2);
#endif
#if (SCALERHEIGHT > 3) 
		PTYPE *line3 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch * 3);
#endif
#if (SCALERHEIGHT > 4) 
		PTYPE *line4 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch * 4);
#endif
#if (SCALERHEIGHT > 5) 
		PTYPE *line5 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch * 5);
#endif
#endif //defined(SCALERLINEAR)
			hadChange = 1;
            unsigned int i = block_proc; /* WARNING: assume block_proc != 0 */
            do {
				const SRCTYPE S = *src++;
				*cache++ = S;
				const PTYPE P = PMAKE(S);
				SCALERFUNC;
				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1) 
				line1 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 2) 
				line2 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 3) 
				line3 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 4) 
				line4 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 5) 
				line5 += SCALERWIDTH;
#endif
			} while (--i != 0u);
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1)
			Bitu copyLen = (Bitu)((uint8_t*)line1 - (uint8_t*)WC[0]);
			BituMove(((uint8_t*)line0)-copyLen+render.scale.outPitch  ,WC[0], copyLen );
#endif
#if (SCALERHEIGHT > 2) 
			BituMove(((uint8_t*)line0)-copyLen+render.scale.outPitch*2,WC[1], copyLen );
#endif
#if (SCALERHEIGHT > 3) 
			BituMove(((uint8_t*)line0)-copyLen+render.scale.outPitch*3,WC[2], copyLen );
#endif
#if (SCALERHEIGHT > 4) 
			BituMove(((uint8_t*)line0)-copyLen+render.scale.outPitch*4,WC[3], copyLen );
#endif
#if (SCALERHEIGHT > 5) 
			BituMove(((uint8_t*)line0)-copyLen+render.scale.outPitch*5,WC[4], copyLen );
#endif
#endif //defined(SCALERLINEAR)
		}
}

static inline void conc4d_func(const void *s) {
#ifdef RENDER_NULL_INPUT
	if (!s) {
		render.scale.cacheRead += render.scale.cachePitch;
#if defined(SCALERLINEAR) 
		Bitu skipLines = SCALERHEIGHT;
#else
		Bitu skipLines = Scaler_Aspect[ render.scale.outLine++ ];
#endif
		ScalerAddLines( 0, skipLines );
		return;
	}
#endif
	/* Clear the complete line marker */
	Bitu hadChange = 0;
	const SRCTYPE *src = (SRCTYPE*)s;

	SRCTYPE *cache = (SRCTYPE*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	PTYPE * line0=(PTYPE *)(render.scale.outWrite);

#if defined(C_SCALER_FULL_LINE)
    conc4d_sub_func(src,cache,line0,(unsigned int)render.src.width,hadChange);
#else
# if (SBPP == 9)
    // the pal change code above limits this to 8 pixels only, see function above
    const unsigned int block_size = 8;
# else
    // larger blocks encourage memcmp() to optimize, scaler loop to process before coming back to check again.
    const unsigned int block_size = 128;
# endif

    Bitu x = (Bitu)render.src.width;
    while (x >= block_size) {
        x -= block_size;
        conc4d_sub_func(src,cache,line0,block_size,hadChange);
    }
    if (x > 0) {
        conc4d_sub_func(src,cache,line0,(unsigned int)x,hadChange);
	}
#endif

#if defined(SCALERLINEAR) 
	Bitu scaleLines = SCALERHEIGHT;
#else
	Bitu scaleLines = Scaler_Aspect[ render.scale.outLine++ ];
	if ( scaleLines - SCALERHEIGHT && hadChange ) {
		BituMove( render.scale.outWrite + render.scale.outPitch * SCALERHEIGHT,
			render.scale.outWrite + render.scale.outPitch * (SCALERHEIGHT-1),
			render.src.width * SCALERWIDTH * PSIZE);
	}
#endif
	ScalerAddLines( hadChange, scaleLines );
}

#if !defined(SCALERLINEAR) 
#define SCALERLINEAR 1
#include "render_simple.h"
#undef SCALERLINEAR
#endif
