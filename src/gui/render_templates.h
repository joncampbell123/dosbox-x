/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include "config.h"
#include "SDL_endian.h"

#if DBPP == 8
#define PSIZE 1
#define PTYPE uint8_t
#define WC scalerWriteCache.b8
//#define FC scalerFrameCache.b8
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b8
#define redMask		0
#define	greenMask	0
#define blueMask	0
#define redBits		0
#define greenBits	0
#define blueBits	0
#define redShift	0
#define greenShift	0
#define blueShift	0
#elif DBPP == 15 || DBPP == 16
#define PSIZE 2
#define PTYPE uint16_t
#define WC scalerWriteCache.b16
//#define FC scalerFrameCache.b16
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b16
#if DBPP == 15
#define	redMask		0x7C00
#define	greenMask	0x03E0
#define	blueMask	0x001F
#define redBits		5
#define greenBits	5
#define blueBits	5
#define redShift	10
#define greenShift	5
#define blueShift	0
#elif DBPP == 16
#define redMask		0xF800
#define greenMask	0x07E0
#define blueMask	0x001F
#define redBits		5
#define greenBits	6
#define blueBits	5
#define redShift	11
#define greenShift	5
#define blueShift	0
#endif
#elif DBPP == 32
#define PSIZE 4
#define PTYPE uint32_t
#define WC scalerWriteCache.b32
//#define FC scalerFrameCache.b32
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b32
# if !defined(C_SDL2) && defined(MACOSX) /* SDL1 builds are subject to Mac OS X strange BGRA (alpha in low byte) order */
#  define redMask       0x0000ff00
#  define greenMask     0x00ff0000
#  define blueMask      0xff000000
#  define redBits       8
#  define greenBits     8
#  define blueBits      8
#  define redShift      8
#  define greenShift    16
#  define blueShift     24
# else
#  define redMask       0xff0000
#  define greenMask     0x00ff00
#  define blueMask      0x0000ff
#  define redBits       8
#  define greenBits     8
#  define blueBits      8
#  define redShift      16
#  define greenShift    8
#  define blueShift     0
# endif
#endif

#define redblueMask (redMask | blueMask)


#if SBPP == 8 || SBPP == 9
#define SC scalerSourceCache.b8
#if DBPP == 8
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 15
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 16
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 32
#define PMAKE(_VAL) render.pal.lut.b32[_VAL]
#endif
#define SRCTYPE uint8_t
#endif

#if SBPP == 15
#define SC scalerSourceCache.b16
#if DBPP == 15
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 16
#define PMAKE(_VAL) (((_VAL) & 31) | ((_VAL) & ~31) << 1)
#elif DBPP == 32
# if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
#  define PMAKE(_VAL)  (((_VAL&(31u<<10u))<<1u)|((_VAL&(31u<<5u))<<14u)|((_VAL&31u)<<27u))
# else
#  define PMAKE(_VAL)  (((_VAL&(31u<<10u))<<9u)|((_VAL&(31u<<5u))<<6u)|((_VAL&31u)<<3u))
# endif
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 16
#define SC scalerSourceCache.b16
#if DBPP == 15
#define PMAKE(_VAL) (((_VAL&~31u)>>1u)|(_VAL&31u))
#elif DBPP == 16
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 32
# if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* Mac OS X Intel builds use a weird RGBA order (alpha in the low 8 bits) */
#  define PMAKE(_VAL)  (((_VAL&(31u<<11u))<<0u)|((_VAL&(63u<<5u))<<13u)|((_VAL&31u)<<27u))
# else
#  define PMAKE(_VAL)  (((_VAL&(31u<<11u))<<8u)|((_VAL&(63u<<5u))<<5u)|((_VAL&31u)<<3u))
# endif
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 32
#define SC scalerSourceCache.b32
#if DBPP == 15
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31u<<19u))>>9u)|((_VAL&(31u<<11u))>>6u)|((_VAL&(31u<<3u))>>3u))
#elif DBPP == 16
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31u<<19u))>>8u)|((_VAL&(63u<<10u))>>5u)|((_VAL&(31u<<3u))>>3u))
#elif DBPP == 32
#define PMAKE(_VAL) (_VAL)
#endif
#define SRCTYPE uint32_t
#endif

//  C0 C1 C2 D3
//  C3 C4 C5 D4
//  C6 C7 C8 D5
//  D0 D1 D2 D6

#define C0 fc[-1 - SCALER_COMPLEXWIDTH]
#define C1 fc[+0 - SCALER_COMPLEXWIDTH]
#define C2 fc[+1 - SCALER_COMPLEXWIDTH]
#define C3 fc[-1 ]
#define C4 fc[+0 ]
#define C5 fc[+1 ]
#define C6 fc[-1 + SCALER_COMPLEXWIDTH]
#define C7 fc[+0 + SCALER_COMPLEXWIDTH]
#define C8 fc[+1 + SCALER_COMPLEXWIDTH]

#define D0 fc[-1 + 2*SCALER_COMPLEXWIDTH]
#define D1 fc[+0 + 2*SCALER_COMPLEXWIDTH]
#define D2 fc[+1 + 2*SCALER_COMPLEXWIDTH]
#define D3 fc[+2 - SCALER_COMPLEXWIDTH]
#define D4 fc[+2]
#define D5 fc[+2 + SCALER_COMPLEXWIDTH]
#define D6 fc[+2 + 2*SCALER_COMPLEXWIDTH]


#if RENDER_USE_ADVANCED_SCALERS>1
static inline void conc3d(Cache,SBPP,DBPP) (const void * s) {
# if !defined(_MSC_VER) /* Microsoft C++ thinks this is a failed attempt at a function call---it's not */
	(void)conc3d(Cache,SBPP,DBPP);
# endif
#ifdef RENDER_NULL_INPUT
	if (!s) {
		render.scale.cacheRead += render.scale.cachePitch;
		render.scale.inLine++;
		render.scale.complexHandler();
		return;
	}
#endif
	const SRCTYPE * src = (SRCTYPE*)s;
	PTYPE *fc= &FC[render.scale.inLine+1][1];
	SRCTYPE *sc = (SRCTYPE*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	Bitu b;
	bool hadChange = false;
	/* This should also copy the surrounding pixels but it looks nice enough without */
	for (b=0;b<render.scale.blocks;b++) {
#if (SBPP == 9)
		for (Bitu x=0;x<SCALER_BLOCKSIZE;x++) {
			PTYPE pixel = PMAKE(src[x]);
			if (pixel != fc[x]) {
#else 
		for (Bitu x=0;x<SCALER_BLOCKSIZE;x+=sizeof(Bitu)/sizeof(SRCTYPE)) {
			if (*(Bitu const*)&src[x] != *(Bitu*)&sc[x]) {
#endif
				do {
					fc[x] = PMAKE(src[x]);
					sc[x] = src[x];
					x++;
				} while (x<SCALER_BLOCKSIZE);
				hadChange = true;
				/* Change the surrounding blocks */
				CC[render.scale.inLine+0][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+0][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+0][1+b+1] |= SCALE_LEFT;
				CC[render.scale.inLine+1][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+1][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+1][1+b+1] |= SCALE_LEFT;
				CC[render.scale.inLine+2][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+2][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+2][1+b+1] |= SCALE_LEFT;
				continue;
			}
		}
		fc += SCALER_BLOCKSIZE;
		sc += SCALER_BLOCKSIZE;
		src += SCALER_BLOCKSIZE;
	}
	if (hadChange) {
		CC[render.scale.inLine+0][0] = 1;
		CC[render.scale.inLine+1][0] = 1;
		CC[render.scale.inLine+2][0] = 1;
	}
	render.scale.inLine++;
	render.scale.complexHandler();
}
#endif


/* Simple scalers */
#define SCALERNAME		Normal1x
#define SCALERWIDTH		1
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;								\
	line1[0] = P;								\
	line1[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;								\
	line0[2] = P;								\
	line1[0] = P;								\
	line1[1] = P;								\
	line1[2] = P;								\
	line2[0] = P;								\
	line2[1] = P;								\
	line2[2] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal4x
#define SCALERWIDTH		4
#define SCALERHEIGHT	4
#define SCALERFUNC							   \
	line0[0] = P;								\
	line0[1] = P;								\
	line0[2] = P;								\
	line0[3] = P;								\
	line1[0] = P;								\
	line1[1] = P;								\
	line1[2] = P;								\
	line1[3] = P;								\
	line2[0] = P;								\
	line2[1] = P;								\
	line2[2] = P;								\
	line2[3] = P;								\
	line3[0] = P;								\
	line3[1] = P;								\
	line3[2] = P;								\
	line3[3] = P; 
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal5x
#define SCALERWIDTH		5
#define SCALERHEIGHT	5
#define SCALERFUNC							   \
	line0[0] = P;								\
	line0[1] = P;								\
	line0[2] = P;								\
	line0[3] = P;								\
	line0[4] = P;								\
	line1[0] = P;								\
	line1[1] = P;								\
	line1[2] = P;								\
	line1[3] = P;								\
	line1[4] = P;								\
	line2[0] = P;								\
	line2[1] = P;								\
	line2[2] = P;								\
	line2[3] = P;								\
	line2[4] = P;								\
	line3[0] = P;								\
	line3[1] = P;								\
	line3[2] = P;								\
	line3[3] = P;								 \
	line3[4] = P;								\
	line4[0] = P;								\
	line4[1] = P;								\
	line4[2] = P;								\
	line4[3] = P;								 \
	line4[4] = P;								
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC
/*
#define SCALERNAME		Normal6x
#define SCALERWIDTH		6
#define SCALERHEIGHT	6
#define SCALERFUNC							   \
	line0[0] = P;								\
	line0[1] = P;								\
	line0[2] = P;								\
	line0[3] = P;								\
	line0[4] = P;								\
	line0[5] = P;								\
	line1[0] = P;								\
	line1[1] = P;								\
	line1[2] = P;								\
	line1[3] = P;								\
	line1[4] = P;								\
	line1[5] = P;								\
	line2[0] = P;								\
	line2[1] = P;								\
	line2[2] = P;								\
	line2[3] = P;								\
	line2[4] = P;								\
	line2[5] = P;								\
	line3[0] = P;								\
	line3[1] = P;								\
	line3[2] = P;								\
	line3[3] = P;								 \
	line3[4] = P;								\
	line3[5] = P;								\
	line4[0] = P;								\
	line4[1] = P;								\
	line4[2] = P;								\
	line4[3] = P;								 \
	line4[4] = P;								\
	line4[5] = P;								\
	line5[0] = P;								\
	line5[1] = P;								\
	line5[2] = P;								\
	line5[3] = P;								 \
	line5[4] = P;								\
	line5[5] = P;								
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC
*/
#define SCALERNAME		NormalDw
#define SCALERWIDTH		2
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line1[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal2xDw
#define SCALERWIDTH		4
#define SCALERHEIGHT	2
#define SCALERFUNC                              \
    line0[0] = P;                               \
    line0[1] = P;                               \
    line0[2] = P;                               \
    line0[3] = P;                               \
    line1[0] = P;                               \
    line1[1] = P;                               \
    line1[2] = P;                               \
    line1[3] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME      Normal2xDh
#define SCALERWIDTH     2
#define SCALERHEIGHT    4
#define SCALERFUNC                              \
    line0[0] = P;                               \
    line0[1] = P;                               \
    line1[0] = P;                               \
    line1[1] = P;                               \
    line2[0] = P;                               \
    line2[1] = P;                               \
    line3[0] = P;                               \
    line3[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#if (DBPP > 8)

#if RENDER_USE_ADVANCED_SCALERS>0

#define SCALERNAME		TV2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC									\
{													\
	PTYPE halfpixel=((P & redblueMask) >> 1) & redblueMask;	\
	halfpixel|=((P & greenMask) >> 1) & greenMask;			\
	line0[0]=P;							\
	line0[1]=P;							\
	line1[0]=halfpixel;						\
	line1[1]=halfpixel;						\
}
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		TVDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC									\
{													\
	PTYPE halfpixel=((P & redblueMask) >> 1) & redblueMask;	\
	halfpixel|=((P & greenMask) >> 1) & greenMask;			\
	line0[0]=P;							\
	line1[0]=halfpixel;						\
}
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		TV3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#if !defined(C_SDL2) && defined(MACOSX) /* SDL1 builds are subject to Mac OS X strange BGRA (alpha in low byte) order */
#define SCALERFUNC							\
{											\
	PTYPE halfpixel=(((uint64_t)(P & redblueMask) * (uint64_t)5) >> (uint64_t)3) & redblueMask;	\
	halfpixel|=(((uint64_t)(P & greenMask) * (uint64_t)5) >> (uint64_t)3) & greenMask;			\
	line0[0]=P;								\
	line0[1]=P;								\
	line0[2]=P;								\
	line1[0]=halfpixel;						\
	line1[1]=halfpixel;						\
	line1[2]=halfpixel;						\
	halfpixel=(((uint64_t)(P & redblueMask) * (uint64_t)5) >> (uint64_t)4) & redblueMask;	\
	halfpixel|=(((uint64_t)(P & greenMask) * (uint64_t)5) >> (uint64_t)4) & greenMask;			\
	line2[0]=halfpixel;						\
	line2[1]=halfpixel;						\
	line2[2]=halfpixel;						\
}
#else
#define SCALERFUNC							\
{											\
	PTYPE halfpixel=(((P & redblueMask) * 5) >> 3) & redblueMask;	\
	halfpixel|=(((P & greenMask) * 5) >> 3) & greenMask;			\
	line0[0]=P;								\
	line0[1]=P;								\
	line0[2]=P;								\
	line1[0]=halfpixel;						\
	line1[1]=halfpixel;						\
	line1[2]=halfpixel;						\
	halfpixel=(((P & redblueMask) * 5) >> 4) & redblueMask;	\
	halfpixel|=(((P & greenMask) * 5) >> 4) & greenMask;			\
	line2[0]=halfpixel;						\
	line2[1]=halfpixel;						\
	line2[2]=halfpixel;						\
}
#endif
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		RGB2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC						\
	line0[0]=P & redMask;				\
	line0[1]=P & greenMask;			\
	line1[0]=P & blueMask;				\
	line1[1]=P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		RGB3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC						\
	line0[0]=P;							\
	line0[1]=P & greenMask;				\
	line0[2]=P & blueMask;				\
	line1[0]=P & greenMask;				\
	line1[1]=P & redMask; 						\
	line1[2]=P;				\
	line2[0]=P;				\
	line2[1]=P & blueMask;				\
	line2[2]=P & redMask;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Scan2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC						\
	line0[0]=P;							\
	line0[1]=P;							\
	line1[0]=0;							\
	line1[1]=0;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		ScanDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line1[0] = 0;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Scan3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC			\
	line0[0]=P;				\
	line0[1]=P;				\
	line0[2]=P;				\
	line1[0]=P;				\
	line1[1]=P;				\
	line1[2]=P;				\
	line2[0]=0;				\
	line2[1]=0;				\
	line2[2]=0;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

/* Grayscale scalers */
#define SCALERNAME		GrayNormal
#define SCALERWIDTH		1
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	PTYPE _red=(P&redMask)>>redShift,_green=(P&greenMask)>>greenShift,_blue=(P&blueMask)>>blueShift;	\
  double _gray=0.2125*(double)_red+0.7154*(double)_green+0.0721*(double)_blue; \
  PTYPE _value = _gray>255?0xff:(uint8_t)(_gray);  \
	line0[0] = ((_value<<redShift)|(_value<<greenShift)|(_value)<<blueShift);
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		GrayDw
#define SCALERWIDTH		2
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	PTYPE _red=(P&redMask)>>redShift,_green=(P&greenMask)>>greenShift,_blue=(P&blueMask)>>blueShift;	\
  double _gray=0.2125*(double)_red+0.7154*(double)_green+0.0721*(double)_blue; \
  PTYPE _value = _gray>255?0xff:(uint8_t)(_gray);  \
	line0[0]=line0[1] = ((_value<<redShift)|(_value<<greenShift)|(_value)<<blueShift);
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		GrayDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	PTYPE _red=(P&redMask)>>redShift,_green=(P&greenMask)>>greenShift,_blue=(P&blueMask)>>blueShift;	\
  double _gray=0.2125*(double)_red+0.7154*(double)_green+0.0721*(double)_blue; \
  PTYPE _value = _gray>255?0xff:(uint8_t)(_gray);  \
	line0[0]=line1[0] = ((_value<<redShift)|(_value<<greenShift)|(_value)<<blueShift);
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Gray2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	PTYPE _red=(P&redMask)>>redShift,_green=(P&greenMask)>>greenShift,_blue=(P&blueMask)>>blueShift;	\
  double _gray=0.2125*(double)_red+0.7154*(double)_green+0.0721*(double)_blue; \
  PTYPE _value = _gray>255?0xff:(uint8_t)(_gray);  \
	line0[0]=line1[0]=line0[1]=line1[1] = ((_value<<redShift)|(_value<<greenShift)|(_value)<<blueShift);
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif		//#if RENDER_USE_ADVANCED_SCALERS>0

#endif		//#if (DBPP > 8)

/* Complex scalers */

#if RENDER_USE_ADVANCED_SCALERS>2

#if (SBPP == DBPP) 


#if (DBPP > 8)

#include "render_templates_hq.h"

#define SCALERNAME		HQ2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#include "render_templates_hq2x.h"
#define SCALERFUNC		conc2d(Hq2x,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		HQ3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#include "render_templates_hq3x.h"
#define SCALERFUNC		conc2d(Hq3x,SBPP)(line0, line1, line2, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#include "render_templates_sai.h"

#define SCALERNAME		Super2xSaI
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(Super2xSaI,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		SuperEagle
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(SuperEagle,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		_2xSaI
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(_2xSaI,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		AdvInterp2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC												\
	if (C1 != C7 && C3 != C5) {									\
		line0[0] = C3 == C1 ? interp_w2(C3,C4,5U,3U) : C4;		\
		line0[1] = C1 == C5 ? interp_w2(C5,C4,5U,3U) : C4;		\
		line1[0] = C3 == C7 ? interp_w2(C3,C4,5U,3U) : C4;		\
		line1[1] = C7 == C5 ? interp_w2(C5,C4,5U,3U) : C4;		\
	} else {													\
		line0[0] = line0[1] = C4;								\
		line1[0] = line1[1] = C4;								\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

//TODO, come up with something better for this one
#define SCALERNAME		AdvInterp3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC												\
	if ((C1 != C7) && (C3 != C5)) {													\
		line0[0] = C3 == C1 ?  interp_w2(C3,C4,5U,3U) : C4;												\
		line0[1] = (C3 == C1 && C4 != C2) || (C5 == C1 && C4 != C0) ? C1 : C4;		\
		line0[2] = C5 == C1 ?  interp_w2(C5,C4,5U,3U) : C4;												\
		line1[0] = (C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4;		\
		line1[1] = C4;																\
		line1[2] = (C5 == C1 && C4 != C8) || (C5 == C7 && C4 != C2) ? C5 : C4;		\
		line2[0] = C3 == C7 ?  interp_w2(C3,C4,5U,3U) : C4;												\
		line2[1] = (C3 == C7 && C4 != C8) || (C5 == C7 && C4 != C6) ? C7 : C4;		\
		line2[2] = C5 == C7 ?  interp_w2(C5,C4,5U,3U) : C4;												\
	} else {																		\
		line0[0] = line0[1] = line0[2] = C4;										\
		line1[0] = line1[1] = line1[2] = C4;										\
		line2[0] = line2[1] = line2[2] = C4;										\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif // #if (DBPP > 8)

#define SCALERNAME		AdvMame2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC												\
	if (C1 != C7 && C3 != C5) {									\
		line0[0] = C3 == C1 ? C3 : C4;							\
		line0[1] = C1 == C5 ? C5 : C4;							\
		line1[0] = C3 == C7 ? C3 : C4;							\
		line1[1] = C7 == C5 ? C5 : C4;							\
	} else {													\
		line0[0] = line0[1] = C4;								\
		line1[0] = line1[1] = C4;								\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		AdvMame3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC																	\
	if ((C1 != C7) && (C3 != C5)) {													\
		line0[0] = C3 == C1 ?  C3 : C4;												\
		line0[1] = (C3 == C1 && C4 != C2) || (C5 == C1 && C4 != C0) ? C1 : C4;		\
		line0[2] = C5 == C1 ?  C5 : C4;												\
		line1[0] = (C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4;		\
		line1[1] = C4;																\
		line1[2] = (C5 == C1 && C4 != C8) || (C5 == C7 && C4 != C2) ? C5 : C4;		\
		line2[0] = C3 == C7 ?  C3 : C4;												\
		line2[1] = (C3 == C7 && C4 != C8) || (C5 == C7 && C4 != C6) ? C7 : C4;		\
		line2[2] = C5 == C7 ?  C5 : C4;												\
	} else {																		\
		line0[0] = line0[1] = line0[2] = C4;										\
		line1[0] = line1[1] = line1[2] = C4;										\
		line2[0] = line2[1] = line2[2] = C4;										\
	}

#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC


#endif // (SBPP == DBPP) && !defined (CACHEWITHPAL)

#endif // #if RENDER_USE_ADVANCED_SCALERS>2

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef WC
#undef LC
#undef FC
#undef SC
#undef redMask
#undef greenMask
#undef blueMask
#undef redblueMask
#undef redBits
#undef greenBits
#undef blueBits
#undef redShift
#undef greenShift
#undef blueShift
#undef SRCTYPE
