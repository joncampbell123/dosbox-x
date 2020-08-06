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


#ifndef GLIDEDEF_H
#define GLIDEDEF_H

#ifdef DOSBOX_DOSBOX_H
struct GLIDE_Block
{
    bool splash;
    bool enabled;
    bool * fullscreen;
    Bit16u width, height;
    class GLIDE_PageHandler * lfb_pagehandler;
    GLIDE_Block():enabled(false),fullscreen(0),width(0),height(0),lfb_pagehandler((GLIDE_PageHandler*)0) { }
};
extern GLIDE_Block glide;
extern void GLIDE_ResetScreen(bool update=false);
extern void GLIDE_DisableScreen(void);
#endif

#define GLIDE_LFB		0x60000000
#define GLIDE_BUFFERS		3			/* Front, Back, AUX */
#define GLIDE_PAGE_BITS		11			/* =2048 pages per buffer, should be enough for 1600x1200x4 */
#define GLIDE_PAGES		(GLIDE_BUFFERS*(1<<GLIDE_PAGE_BITS))

#ifdef __3DFX_H__
/* If you change these defines, don't forget to change the table in glide.h and compile a matching GLIDE2X.OVL */

#define	_grAADrawLine8			0	// void grAADrawLine(GrVertex *va, GrVertex *vb)
#define	_grAADrawPoint4			1	// void grAADrawPoint(GrVertex *p)
#define	_grAADrawPolygon12		2	// void grAADrawPolygon(int nVerts, const int ilist[], const GrVertex vlist[])
#define	_grAADrawPolygonVertexList8	3	// void grAADrawPolygonVertexList(int nVerts, const GrVertex vlist[])
#define	_grAADrawTriangle24		4	// void grAADrawTriangle(GrVertex *a, GrVertex *b, GrVertex *c, FxBool antialiasAB, FxBool antialiasBC, FxBool antialiasCA)
#define	_grAlphaBlendFunction16		5	// void grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df)
#define	_grAlphaCombine20		6	// void grAlphaCombine(GrCombineFunction_t func, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
#define	_grAlphaControlsITRGBLighting4	7	// void grAlphaControlsITRGBLighting(FxBool enable)
#define	_grAlphaTestFunction4		8	// void grAlphaTestFunction(GrCmpFnc_t function)
#define	_grAlphaTestReferenceValue4	9	// void grAlphaTestReferenceValue(GrAlpha_t value)
#define	_grBufferClear12		10	// void grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
#define	_grBufferNumPending0		11	// int grBufferNumPending(void)
#define	_grBufferSwap4			12	// void grBufferSwap(int swap_interval)
#define	_grCheckForRoom4		13	// void grCheckForRoom(FxI32 n)
#define	_grChromakeyMode4		14	// void grChromakeyMode(GrChromakeyMode_t mode)
#define	_grChromakeyValue4		15	// void grChromakeyValue(GrColor_t value)
#define	_grClipWindow16			16	// void grClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy)
#define	_grColorCombine20		17	// void grColorCombine(GrCombineFunction_t func, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
#define	_grColorMask8			18	// void grColorMask(FxBool rgb, FxBool alpha)
#define	_grConstantColorValue416	19	// void grConstantColorValue4(float a, float r, float g, float b)
#define	_grConstantColorValue4		20	// void grConstantColorValue(GrColor_t color)
#define	_grCullMode4			21	// void grCullMode(GrCullMode_t mode)
#define	_grDepthBiasLevel4		22	// void grDepthBiasLevel(FxI16 level)
#define	_grDepthBufferFunction4		23	// void grDepthBufferFunction(GrCmpFnc_t func)
#define	_grDepthBufferMode4		24	// void grDepthBufferMode(GrDepthBufferMode_t mode)
#define	_grDepthMask4			25	// void grDepthMask(FxBool enable)
#define	_grDisableAllEffects0		26	// void grDisableAllEffects(void)
#define	_grDitherMode4			27	// void grDitherMode(GrDitherMode_t mode)
#define	_grDrawLine8			28	// void grDrawLine(const GrVertex *a, const GrVertex *b)
#define	_grDrawPlanarPolygon12		29	// void grDrawPlanarPolygon(int nVerts, int ilist[], const GrVertex vlist[])
#define	_grDrawPlanarPolygonVertexList8 30	// void grDrawPlanarPolygonVertexList(int nVerts, const GrVertex vlist[])
#define	_grDrawPoint4			31	// void grDrawPoint(const GrVertex *a)
#define	_grDrawPolygon12		32	// void grDrawPolygon(int nVerts, int ilist[], const GrVertex vlist[])
#define	_grDrawPolygonVertexList8	33	// void grDrawPolygonVertexList(int nVerts, const GrVertex vlist[])
#define	_grDrawTriangle12		34	// void grDrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c)
#define	_grErrorSetCallback4		35	// void grErrorSetCallback(void (*function)(const char *string, FxBool fatal))
#define	_grFogColorValue4		36	// void grFogColorValue(GrColor_t value)
#define	_grFogMode4			37	// void grFogMode(GrFogMode_t mode)
#define	_grFogTable4			38	// void grFogTable(const GrFog_t table[GR_FOG_TABLE_SIZE])
#define	_grGammaCorrectionValue4	39	// void grGammaCorrectionValue(float value)
#define	_grGlideGetState4		40	// void grGlideGetState(GrState *state)
#define	_grGlideGetVersion4		41	// void grGlideGetVersion(char version[80])
#define	_grGlideInit0			42	// void grGlideInit(void)
#define	_grGlideSetState4		43	// void grGlideSetState(const GrState *state)
#define	_grGlideShamelessPlug4		44	// void grGlideShamelessPlug(const FxBool on)
#define	_grGlideShutdown0		45	// void grGlideShutdown(void)
#define	_grHints8			46	// void grHints(GrHint_t type, FxU32 hintMask)
#define	_grLfbConstantAlpha4		47	// void grLfbConstantAlpha(GrAlpha_t alpha)
#define	_grLfbConstantDepth4		48	// void grLfbConstantDepth(FxU16 depth)
#define	_grLfbLock24			49	// FxBool grLfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info)
#define	_grLfbReadRegion28		50	// FxBool grLfbReadRegion(GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width, FxU32 src_height, FxU32 dst_stride, void *dst_data)
#define	_grLfbUnlock8			51	// FxBool grLfbUnlock(GrLock_t type, GrBuffer_t buffer)
#define	_grLfbWriteColorFormat4		52	// void grLfbWriteColorFormat(GrColorFormat_t colorFormat)
#define	_grLfbWriteColorSwizzle8	53	// void grLfbWriteColorSwizzle(FxBool swizzleBytes, FxBool swapWords)
#define	_grLfbWriteRegion32		54	// FxBool grLfbWriteRegion(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y, GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height, FxU32 src_stride, void *src_data)
#define	_grRenderBuffer4		55	// void grRenderBuffer(GrBuffer_t buffer)
#define	_grResetTriStats0		56	// void grResetTriStats()
#define	_grSplash20			57	// void grSplash(float x, float y, float width, float height, FxU32 frame)
#define	_grSstConfigPipeline12		58	//
#define	_grSstControl4			59	// FxBool grSstControl(FxU32 code)
#define	_grSstIdle0			60	// void grSstIdle(void)
#define	_grSstIsBusy0			61	// FxBool grSstIsBusy(void)
#define	_grSstOrigin4			62	// void grSstOrigin(GrOriginLocation_t origin)
#define	_grSstPerfStats4		63	// void grSstPerfStats(GrSstPerfStats_t *pStats)
#define	_grSstQueryBoards4		64	// FxBool grSstQueryBoards(GrHwConfiguration *hwConfig)
#define	_grSstQueryHardware4		65	// FxBool grSstQueryHardware(GrHwConfiguration *hwConfig)
#define	_grSstResetPerfStats0		66	// void grSstResetPerfStats(void)
#define	_grSstScreenHeight0		67	// FxU32 grSstScreenHeight(void)
#define	_grSstScreenWidth0		68	// FxU32 grSstScreenWidth(void)
#define	_grSstSelect4			69	// void grSstSelect(int which_sst)
#define	_grSstStatus0			70	// FxU32 grSstStatus(void)
#define	_grSstVRetraceOn0		71	// FxBool grSstVRetraceOn(void)
#define	_grSstVidMode8			72	//
#define	_grSstVideoLine0		73	// FxU32 grSstVideoLine(void)
#define	_grSstWinClose0			74	// void grSstWinClose(void)
#define	_grSstWinOpen28			75	// FxBool grSstWinOpen(FxU32 hwnd, GrScreenResolution_t res, GrScreenRefresh_t ref, GrColorFormat_t cformat, GrOriginLocation_t org_loc, int num_buffers, int num_aux_buffers)
#define	_grTexCalcMemRequired16		76	// FxU32 grTexCalcMemRequired(GrLOD_t smallLod, GrLOD_t largeLod, GrAspectRatio_t aspect, GrTextureFormat_t format)
#define	_grTexClampMode12		77	// void grTexClampMode(GrChipID_t tmu, GrTextureClampMode_t sClampMode, GrTextureClampMode_t tClampMode)
#define	_grTexCombine28			78	// void grTexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert)
#define	_grTexCombineFunction8		79	// void grTexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc)
#define	_grTexDetailControl16		80	// void grTexDetailControl(GrChipID_t tmu, int lodBias, FxU8 detailScale, float detailMax)
#define	_grTexDownloadMipMap16		81	// void grTexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
#define	_grTexDownloadMipMapLevel32	82	// void grTexDownloadMipMapLevel(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data)
#define	_grTexDownloadMipMapLevelPartial40 83	// void grTexDownloadMipMapLevelPartial(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data, int start, int end)
#define	_grTexDownloadTable12		84	// void grTexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data)
#define	_grTexDownloadTablePartial20	85	// void grTexDownloadTablePartial(GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end)
#define	_grTexFilterMode12		86	// void grTexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode)
#define	_grTexLodBiasValue8		87	// void grTexLodBiasValue(GrChipID_t tmu, float bias)
#define	_grTexMaxAddress4		88	// FxU32 grTexMaxAddress(GrChipID_t tmu)
#define	_grTexMinAddress4		89	// FxU32 grTexMinAddress(GrChipID_t tmu)
#define	_grTexMipMapMode12		90	// void grTexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend)
#define	_grTexMultibase8		91	// void grTexMultibase(GrChipID_t tmu, FxBool enable)
#define	_grTexMultibaseAddress20	92	// void grTexMultibaseAddress(GrChipID_t tmu, GrTexBaseRange_t range, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
#define	_grTexNCCTable8			93	// void grTexNCCTable(GrChipID_t tmu, GrNCCTable_t table)
#define	_grTexSource16			94	// void grTexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
#define	_grTexTextureMemRequired8	95	// FxU32 grTexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info)
#define	_grTriStats8			96	// void grTriStats(FxU32 *trisProcessed, FxU32 *trisDrawn)
#define	_gu3dfGetInfo8			97	// FxBool gu3dfGetInfo(const char *filename, Gu3dfInfo *info)
#define	_gu3dfLoad8			98	// FxBool gu3dfLoad(const char *filename, Gu3dfInfo *info)
#define	_guAADrawTriangleWithClip12	99	// void guAADrawTriangleWithClip(const GrVertex *va, const GrVertex *vb, const GrVertex *vc)
#define	_guAlphaSource4			100	// void guAlphaSource(GrAlphaSource_t mode)
#define	_guColorCombineFunction4	101	// void guColorCombineFunction(GrColorCombineFnc_t func)
#define	_guDrawPolygonVertexListWithClip8 102	// void guDrawPolygonVertexListWithClip(int nverts, const GrVertex vlist[])
#define	_guDrawTriangleWithClip12	103	// void guDrawTriangleWithClip(const GrVertex *va, const GrVertex *vb, const GrVertex *vc)
#define	_guEncodeRLE1616		104	//
#define	_guEndianSwapBytes4		105	//
#define	_guEndianSwapWords4		106	//
#define	_guFogGenerateExp28		107	// void guFogGenerateExp2(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float density)
#define	_guFogGenerateExp8		108	// void guFogGenerateExp(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float density)
#define	_guFogGenerateLinear12		109	// void guFogGenerateLinear(GrFog_t fogTable[GR_FOG_TABLE_SIZE], float nearW, float farW)
#define	_guFogTableIndexToW4		110	// float guFogTableIndexToW(int i)
#define	_guMPDrawTriangle12		111	//
#define	_guMPInit0			112	//
#define	_guMPTexCombineFunction4	113	//
#define	_guMPTexSource8			114	//
#define	_guMovieSetName4		115	//
#define	_guMovieStart0			116	//
#define	_guMovieStop0			117	//
#define	_guTexAllocateMemory60		118	// GrMipMapId_t guTexAllocateMemory(GrChipID_t tmu, FxU8 evenOddMask, int width, int height, GrTextureFormat_t format, GrMipMapMode_t mmMode, GrLOD_t smallLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureClampMode_t sClampMode, GrTextureClampMode_t tClampMode, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode, float lodBias, FxBool lodBlend)
#define	_guTexChangeAttributes48	119	// FxBool guTexChangeAttributes(GrMipMapID_t mmid, int width, int height, GrTextureFormat_t format, GrMipMapMode_t mmMode, GrLOD_t smallLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureClampMode_t sClampMode, GrTextureClampMode_t tClampMode, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode)
#define	_guTexCombineFunction8		120	// void guTexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t func)
#define	_guTexCreateColorMipMap0	121	//
#define	_guTexDownloadMipMap12		122	// void guTexDownloadMipMap(GrMipMapId_t mmid, const void *src, const GuNccTable *nccTable)
#define	_guTexDownloadMipMapLevel12	123	// void guTexDownloadMipMapLevel(GrMipMapId_t mmid, GrLOD_t lod, const void **src)
#define	_guTexGetCurrentMipMap4		124	// GrMipMapId_t guTexGetCurrentMipMap (GrChipID_t tmu)
#define	_guTexGetMipMapInfo4		125	// GrMipMapInfo *guTexGetMipMapInfo(GrMipMapId_t mmid)
#define	_guTexMemQueryAvail4		126	// FxU32 guTexMemQueryAvail(GrChipID_t tmu)
#define	_guTexMemReset0			127	// void guTexMemReset(void)
#define	_guTexSource4			128	// void guTexSource(GrMipMapId_t mmid)
#define	_ConvertAndDownloadRle64	129	// void ConvertAndDownloadRle(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, FxU8 *bm_data, long  bm_h, FxU32 u0, FxU32 v0, FxU32 width, FxU32 height, FxU32 dest_width, FxU32 dest_height, FxU16 *tlut)
#define GLIDE_MAX			129

#endif // __3DFX_H__

#endif // GLIDEDEF_H
