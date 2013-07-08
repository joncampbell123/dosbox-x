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

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

//#include "render.h"
#include "video.h"
#if RENDER_USE_ADVANCED_SCALERS>0
#define SCALER_MAXWIDTH		1280 
#define SCALER_MAXHEIGHT	1024
#else
// reduced to save some memory
#define SCALER_MAXWIDTH		800 
#define SCALER_MAXHEIGHT	600
#endif

#if RENDER_USE_ADVANCED_SCALERS>1
#define SCALER_COMPLEXWIDTH		800
#define SCALER_COMPLEXHEIGHT	600
#endif

#define SCALER_BLOCKSIZE	16

typedef enum {
	scalerMode8, scalerMode15, scalerMode16, scalerMode32
} scalerMode_t;

typedef enum scalerOperation {
	scalerOpNormal,
#if RENDER_USE_ADVANCED_SCALERS>2
	scalerOpAdvMame,
	scalerOpAdvInterp,
	scalerOpHQ,
	scalerOpSaI,
	scalerOpSuperSaI,
	scalerOpSuperEagle,
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
	scalerOpTV,
	scalerOpRGB,
	scalerOpScan,
#endif
	scalerLast
} scalerOperation_t;

typedef void (*ScalerLineHandler_t)(const void *src);
typedef void (*ScalerComplexHandler_t)(void);

extern Bit8u Scaler_Aspect[];
extern Bit8u diff_table[];
extern Bitu Scaler_ChangedLineIndex;
extern Bit16u Scaler_ChangedLines[];
#if RENDER_USE_ADVANCED_SCALERS>1
/* Not entirely happy about those +2's since they make a non power of 2, with muls instead of shift */
typedef Bit8u scalerChangeCache_t [SCALER_COMPLEXHEIGHT][SCALER_COMPLEXWIDTH / SCALER_BLOCKSIZE] ;
typedef union {
	Bit32u b32	[SCALER_COMPLEXHEIGHT] [SCALER_COMPLEXWIDTH];
	Bit16u b16	[SCALER_COMPLEXHEIGHT] [SCALER_COMPLEXWIDTH];
	Bit8u b8	[SCALER_COMPLEXHEIGHT] [SCALER_COMPLEXWIDTH];
} scalerFrameCache_t;
#endif
typedef union {
	Bit32u b32	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit16u b16	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit8u b8	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
} scalerSourceCache_t;
extern scalerSourceCache_t scalerSourceCache;
#if RENDER_USE_ADVANCED_SCALERS>1
extern scalerChangeCache_t scalerChangeCache;
#endif
typedef ScalerLineHandler_t ScalerLineBlock_t[5][4];

typedef struct {
	const char *name;
	Bitu gfxFlags;
	Bitu xscale,yscale;
	ScalerComplexHandler_t Linear[4];
	ScalerComplexHandler_t Random[4];
} ScalerComplexBlock_t;

typedef struct {
	const char *name;
	Bitu gfxFlags;
	Bitu xscale,yscale;
	ScalerLineBlock_t	Linear;
	ScalerLineBlock_t	Random;
} ScalerSimpleBlock_t;


#define SCALE_LEFT	0x1
#define SCALE_RIGHT	0x2
#define SCALE_FULL	0x4

/* Simple scalers */
extern ScalerSimpleBlock_t ScaleNormal1x;
extern ScalerSimpleBlock_t ScaleNormalDw;
extern ScalerSimpleBlock_t ScaleNormalDh;
extern ScalerSimpleBlock_t ScaleNormal2x;
extern ScalerSimpleBlock_t ScaleNormal3x;
#if RENDER_USE_ADVANCED_SCALERS>0
extern ScalerSimpleBlock_t ScaleTV2x;
extern ScalerSimpleBlock_t ScaleTV3x;
extern ScalerSimpleBlock_t ScaleRGB2x;
extern ScalerSimpleBlock_t ScaleRGB3x;
extern ScalerSimpleBlock_t ScaleScan2x;
extern ScalerSimpleBlock_t ScaleScan3x;
#endif
/* Complex scalers */
#if RENDER_USE_ADVANCED_SCALERS>2
extern ScalerComplexBlock_t ScaleHQ2x;
extern ScalerComplexBlock_t ScaleHQ3x;
extern ScalerComplexBlock_t Scale2xSaI;
extern ScalerComplexBlock_t ScaleSuper2xSaI;
extern ScalerComplexBlock_t ScaleSuperEagle;
extern ScalerComplexBlock_t ScaleAdvMame2x;
extern ScalerComplexBlock_t ScaleAdvMame3x;
extern ScalerComplexBlock_t ScaleAdvInterp2x;
extern ScalerComplexBlock_t ScaleAdvInterp3x;
#endif
#if RENDER_USE_ADVANCED_SCALERS>1
extern ScalerLineBlock_t ScalerCache;
#endif
#endif
