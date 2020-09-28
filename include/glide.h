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


#ifndef DOSBOX_GLIDE_H
#define DOSBOX_GLIDE_H

#define __3DFX_H__

/*
** basic data types
*/
typedef uint8_t   FxU8;
typedef int8_t   FxI8;
typedef uint16_t  FxU16;
typedef int16_t  FxI16;
typedef Bit32u  FxU32;
typedef Bit32s  FxI32;
typedef Bit32s  FxBool;
typedef float   FxFloat;
typedef double  FxDouble;

/*
** color types
*/
typedef Bit32u                       FxColor_t;
typedef struct { float r, g, b, a; } FxColor4;

/*
** fundamental types
*/
#define FXTRUE    1
#define FXFALSE   0

/*
** helper macros
*/
#define FXUNUSED( a ) ((void)(a))
#define FXBIT( i )    ( 1L << (i) )

#define FX_ENTRY
#define FX_GLIDE_NO_FUNC_PROTO

#if defined (WIN32)
#define FX_CALL __stdcall
#else
#define FX_CALL
#endif

#include <sdk2_glide.h>
#include "glidedef.h"

// Careful with structures containing pointers
//
// GrTexInfo; GrLfbInfo_t; Gu3dfInfo; GrMipMapInfo;
//

// Some glide structs might have different size in guest 32-bit DOS (pointers)
typedef struct {
    Bit32s		smallLod;
    Bit32s		largeLod;
    Bit32s		aspectRatio;
    Bit32s		format;
    PhysPt		data;
} DBGrTexInfo;

typedef struct {
    Bit32s		size;
    PhysPt		lfbPtr;
    Bit32u		strideInBytes;
    Bit32s		writeMode;
    Bit32s		origin;
} DBGrLfbInfo_t;

typedef struct {
    Gu3dfHeader 	header;
    GuTexTable		table;
    PhysPt		data;
    Bit32u		mem_required;
} DBGu3dfInfo;

typedef struct {
    const char * name;
    const uint8_t parms;
} GLIDE_TABLE;

typedef void (FX_CALL *pfunc0)		(void);
typedef void (FX_CALL *pfunc1i)		(FxU32);
typedef void (FX_CALL *pfunc1p)		(void*);
typedef void (FX_CALL *pfunc1f)		(float);
typedef void (FX_CALL *pfunc2i)		(FxU32, FxU32);
typedef void (FX_CALL *pfunc1i1p)	(FxU32, void*);
typedef void (FX_CALL *pfunc2p)		(void*, void*);
typedef void (FX_CALL *pfunc1i1f)	(FxU32, float);
typedef void (FX_CALL *pfunc1p1f)	(void*, float);
typedef void (FX_CALL *pfunc3i)		(FxU32, FxU32, FxU32);
typedef void (FX_CALL *pfunc2i1p)	(FxU32, FxU32, void*);
typedef void (FX_CALL *pfunc1i2p)	(FxU32, void*, void*);
typedef void (FX_CALL *pfunc3p)		(void*, void*, void*);
typedef void (FX_CALL *pfunc1p2f)	(void*, float, float);
typedef void (FX_CALL *pfunc4i)		(FxU32, FxU32, FxU32, FxU32);
typedef void (FX_CALL *pfunc3i1p)	(FxU32, FxU32, FxU32, void*);
typedef void (FX_CALL *pfunc3i1f)	(FxU32, FxU32, FxU32, float);
typedef void (FX_CALL *pfunc4f)		(float, float, float, float);
typedef void (FX_CALL *pfunc5i)		(FxU32, FxU32, FxU32, FxU32, FxU32);
typedef void (FX_CALL *pfunc2i1p2i)	(FxU32, FxU32, void*, FxU32, FxU32);
typedef void (FX_CALL *pfunc4f1i)	(float, float, float, float, FxU32);
typedef void (FX_CALL *pfunc3p3i)	(void*, void*, void*, FxU32, FxU32, FxU32);
typedef void (FX_CALL *pfunc7i)		(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32);
typedef void (FX_CALL *pfunc7i1p)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*);
typedef void (FX_CALL *pfunc7i1p2i)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*,
					    FxU32, FxU32);
typedef void (FX_CALL *pfunc7i1p7i1p)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*,
					    FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*);

typedef FxU32 (FX_CALL *prfunc0)		(void);
typedef FxU32 (FX_CALL *prfunc1i)	(FxU32);
typedef FxU32 (FX_CALL *prfunc1p)	(void*);
typedef FxU32 (FX_CALL *prfunc2i)	(FxU32, FxU32);
typedef FxU32 (FX_CALL *prfunc1i1p)	(FxU32, void*);
typedef FxU32 (FX_CALL *prfunc2p)	(void*, void*);
typedef FxU32 (FX_CALL *prfunc4i)	(FxU32, FxU32, FxU32, FxU32);
typedef FxU32 (FX_CALL *prfunc5i1p)	(FxU32, FxU32, FxU32, FxU32, FxU32, void*);
typedef FxU32 (FX_CALL *prfunc7i)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32);
typedef FxU32 (FX_CALL *prfunc1p6i)	(void*, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32);
typedef FxU32 (FX_CALL *prfunc6i1p)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*);
typedef FxU32 (FX_CALL *prfunc7i1p)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, void*);
typedef FxU32 (FX_CALL *prfunc12i)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32,
					    FxU32, FxU32, FxU32, FxU32, FxU32);
typedef FxU32 (FX_CALL *prfunc13i1f1i)	(FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32, FxU32,
					    FxU32, FxU32, FxU32, FxU32, FxU32, float, FxU32);

typedef void* (FX_CALL *prptfunc1i)	(FxU32);
typedef float (FX_CALL *pffunc1i)	(FxU32);

typedef union {
    pfunc0	grFunction0;
    pfunc1i	grFunction1i;
    pfunc1p	grFunction1p;
    pfunc1f	grFunction1f;
    pfunc2i	grFunction2i;
    pfunc1i1p	grFunction1i1p;
    pfunc2p	grFunction2p;
    pfunc1i1f	grFunction1i1f;
    pfunc1p1f	grFunction1p1f;
    pfunc3i	grFunction3i;
    pfunc2i1p	grFunction2i1p;
    pfunc1i2p	grFunction1i2p;
    pfunc3p	grFunction3p;
    pfunc1p2f	grFunction1p2f;
    pfunc4i	grFunction4i;
    pfunc3i1p	grFunction3i1p;
    pfunc3i1f	grFunction3i1f;
    pfunc4f	grFunction4f;
    pfunc5i	grFunction5i;
    pfunc2i1p2i	grFunction2i1p2i;
    pfunc4f1i	grFunction4f1i;
    pfunc3p3i	grFunction3p3i;
    pfunc7i	grFunction7i;
    pfunc7i1p	grFunction7i1p;
    pfunc7i1p2i	grFunction7i1p2i;
    pfunc7i1p7i1p grFunction7i1p7i1p;

    prfunc0	grRFunction0;
    prfunc1i	grRFunction1i;
    prfunc1p	grRFunction1p;
    prfunc2i	grRFunction2i;
    prfunc1i1p	grRFunction1i1p;
    prfunc2p	grRFunction2p;
    prfunc4i	grRFunction4i;
    prfunc5i1p	grRFunction5i1p;
    prfunc7i	grRFunction7i;
    prfunc1p6i	grRFunction1p6i;
    prfunc6i1p	grRFunction6i1p;
    prfunc7i1p	grRFunction7i1p;
    prfunc12i	grRFunction12i;
    prfunc13i1f1i grRFunction13i1f1i;

    prptfunc1i	grRPTFunction1i;

    pffunc1i	grFFunction1i;
} FncPointers;

static const GLIDE_TABLE grTable[] = {
    { "grAADrawLine", 8 },
    { "grAADrawPoint", 4 },
    { "grAADrawPolygon", 12 },
    { "grAADrawPolygonVertexList", 8 },
    { "grAADrawTriangle", 24 },
    { "grAlphaBlendFunction", 16 },
    { "grAlphaCombine", 20 },
    { "grAlphaControlsITRGBLighting", 4 },
    { "grAlphaTestFunction", 4 },
    { "grAlphaTestReferenceValue", 4 },
    { "grBufferClear", 12 },
    { "grBufferNumPending", 0 },
    { "grBufferSwap", 4 },
    { "grCheckForRoom", 4 },
    { "grChromakeyMode", 4 },
    { "grChromakeyValue", 4 },
    { "grClipWindow", 16 },
    { "grColorCombine", 20 },
    { "grColorMask", 8 },
    { "grConstantColorValue4", 16 },
    { "grConstantColorValue", 4 },
    { "grCullMode", 4 },
    { "grDepthBiasLevel", 4 },
    { "grDepthBufferFunction", 4 },
    { "grDepthBufferMode", 4 },
    { "grDepthMask", 4 },
    { "grDisableAllEffects", 0 },
    { "grDitherMode", 4 },
    { "grDrawLine", 8 },
    { "grDrawPlanarPolygon", 12 },
    { "grDrawPlanarPolygonVertexList", 8 },
    { "grDrawPoint", 4 },
    { "grDrawPolygon", 12 },
    { "grDrawPolygonVertexList", 8 },
    { "grDrawTriangle", 12 },
    { "grErrorSetCallback", 4 },
    { "grFogColorValue", 4 },
    { "grFogMode", 4 },
    { "grFogTable", 4 },
    { "grGammaCorrectionValue", 4 },
    { "grGlideGetState", 4 },
    { "grGlideGetVersion", 4 },
    { "grGlideInit", 0 },
    { "grGlideSetState", 4 },
    { "grGlideShamelessPlug", 4 },
    { "grGlideShutdown", 0 },
    { "grHints", 8 },
    { "grLfbConstantAlpha", 4 },
    { "grLfbConstantDepth", 4 },
    { "grLfbLock", 24 },
    { "grLfbReadRegion", 28 },
    { "grLfbUnlock", 8 },
    { "grLfbWriteColorFormat", 4 },
    { "grLfbWriteColorSwizzle", 8 },
    { "grLfbWriteRegion", 32 },
    { "grRenderBuffer", 4 },
    { "grResetTriStats", 0 },
    { "grSplash", 20 },
    { "grSstConfigPipeline", 12 },
    { "grSstControl", 4 },
    { "grSstIdle", 0 },
    { "grSstIsBusy", 0 },
    { "grSstOrigin", 4 },
    { "grSstPerfStats", 4 },
    { "grSstQueryBoards", 4 },
    { "grSstQueryHardware", 4 },
    { "grSstResetPerfStats", 0 },
    { "grSstScreenHeight", 0 },
    { "grSstScreenWidth", 0 },
    { "grSstSelect", 4 },
    { "grSstStatus", 0 },
    { "grSstVRetraceOn", 0 },
    { "grSstVidMode", 8 },
    { "grSstVideoLine", 0 },
    { "grSstWinClose", 0 },
    { "grSstWinOpen", 28 },
    { "grTexCalcMemRequired", 16 },
    { "grTexClampMode", 12 },
    { "grTexCombine", 28 },
    { "grTexCombineFunction", 8 },
    { "grTexDetailControl", 16 },
    { "grTexDownloadMipMap", 16 },
    { "grTexDownloadMipMapLevel", 32 },
    { "grTexDownloadMipMapLevelPartial", 40 },
    { "grTexDownloadTable", 12 },
    { "grTexDownloadTablePartial", 20 },
    { "grTexFilterMode", 12 },
    { "grTexLodBiasValue", 8 },
    { "grTexMaxAddress", 4 },
    { "grTexMinAddress", 4 },
    { "grTexMipMapMode", 12 },
    { "grTexMultibase", 8 },
    { "grTexMultibaseAddress", 20 },
    { "grTexNCCTable", 8 },
    { "grTexSource", 16 },
    { "grTexTextureMemRequired", 8 },
    { "grTriStats", 8 },
    { "gu3dfGetInfo", 8 },
    { "gu3dfLoad", 8 },
    { "guAADrawTriangleWithClip", 12 },
    { "guAlphaSource", 4 },
    { "guColorCombineFunction", 4 },
    { "guDrawPolygonVertexListWithClip", 8 },
    { "guDrawTriangleWithClip", 12 },
    { "guEncodeRLE16", 16 },
    { "guEndianSwapBytes", 4 },
    { "guEndianSwapWords", 4 },
    { "guFogGenerateExp2", 8 },
    { "guFogGenerateExp", 8 },
    { "guFogGenerateLinear", 12 },
    { "guFogTableIndexToW", 4 },
    { "guMPDrawTriangle", 12 },
    { "guMPInit", 0 },
    { "guMPTexCombineFunction", 4 },
    { "guMPTexSource", 8 },
    { "guMovieSetName", 4 },
    { "guMovieStart", 0 },
    { "guMovieStop", 0 },
    { "guTexAllocateMemory", 60 },
    { "guTexChangeAttributes", 48 },
    { "guTexCombineFunction", 8 },
    { "guTexCreateColorMipMap", 0 },
    { "guTexDownloadMipMap", 12 },
    { "guTexDownloadMipMapLevel", 12 },
    { "guTexGetCurrentMipMap", 4 },
    { "guTexGetMipMapInfo", 4 },
    { "guTexMemQueryAvail", 4 },
    { "guTexMemReset", 0 },
    { "guTexSource", 4 },
    { "ConvertAndDownloadRle", 64 }
};

#endif // DOSBOX_GLIDE_H
