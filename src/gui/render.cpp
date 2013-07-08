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

/* $Id: render.cpp,v 1.60 2009-04-26 19:14:50 harekiet Exp $ */

#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "setup.h"
#include "control.h"
#include "mapper.h"
#include "cross.h"
#include "hardware.h"
#include "support.h"

#include "render_scalers.h"

Render_t render;
ScalerLineHandler_t RENDER_DrawLine;

static void RENDER_CallBack( GFX_CallBackFunctions_t function );

static void Check_Palette(void) {
	/* Clean up any previous changed palette data */
	if (render.pal.changed) {
		memset(render.pal.modified, 0, sizeof(render.pal.modified));
		render.pal.changed = false;
	}
	if (render.pal.first>render.pal.last) 
		return;
	Bitu i;
	switch (render.scale.outMode) {
	case scalerMode8:
		GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
		break;
	case scalerMode15:
	case scalerMode16:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Bit16u newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b16[i]) {
				render.pal.changed = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b16[i] = newPal;
			}
		}
		break;
	case scalerMode32:
	default:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Bit32u newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b32[i]) {
				render.pal.changed = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b32[i] = newPal;
			}
		}
		break;
	}
	/* Setup pal index to startup values */
	render.pal.first=256;
	render.pal.last=0;
}

void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
}

static void RENDER_EmptyLineHandler(const void * src) {
}

static void RENDER_StartLineHandler(const void * s) {
	if (s) {
		const Bitu *src = (Bitu*)s;
		Bitu *cache = (Bitu*)(render.scale.cacheRead);
		for (Bits x=render.src.start;x>0;) {
			if (GCC_UNLIKELY(src[0] != cache[0])) {
				if (!GFX_StartUpdate( render.scale.outWrite, render.scale.outPitch )) {
					RENDER_DrawLine = RENDER_EmptyLineHandler;
					return;
				}
				render.scale.outWrite += render.scale.outPitch * Scaler_ChangedLines[0];
				RENDER_DrawLine = render.scale.lineHandler;
				RENDER_DrawLine( s );
				return;
			}
			x--; src++; cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
	Scaler_ChangedLines[0] += Scaler_Aspect[ render.scale.inLine ];
	render.scale.inLine++;
	render.scale.outLine++;
}

static void RENDER_FinishLineHandler(const void * s) {
	if (s) {
		const Bitu *src = (Bitu*)s;
		Bitu *cache = (Bitu*)(render.scale.cacheRead);
		for (Bits x=render.src.start;x>0;) {
			cache[0] = src[0];
			x--; src++; cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
}


static void RENDER_ClearCacheHandler(const void * src) {
	Bitu x, width;
	Bit32u *srcLine, *cacheLine;
	srcLine = (Bit32u *)src;
	cacheLine = (Bit32u *)render.scale.cacheRead;
	width = render.scale.cachePitch / 4;
	for (x=0;x<width;x++)
		cacheLine[x] = ~srcLine[x];
	render.scale.lineHandler( src );
}

bool RENDER_StartUpdate(void) {
	if (GCC_UNLIKELY(render.updating))
		return false;
	if (GCC_UNLIKELY(!render.active))
		return false;
	if (GCC_UNLIKELY(render.frameskip.count<render.frameskip.max)) {
		render.frameskip.count++;
		return false;
	}
	render.frameskip.count=0;
	if (render.scale.inMode == scalerMode8) {
		Check_Palette();
	}
	render.scale.inLine = 0;
	render.scale.outLine = 0;
	render.scale.cacheRead = (Bit8u*)&scalerSourceCache;
	render.scale.outWrite = 0;
	render.scale.outPitch = 0;
	Scaler_ChangedLines[0] = 0;
	Scaler_ChangedLineIndex = 0;
	/* Clearing the cache will first process the line to make sure it's never the same */
	if (GCC_UNLIKELY( render.scale.clearCache) ) {
//		LOG_MSG("Clearing cache");
		//Will always have to update the screen with this one anyway, so let's update already
		if (GCC_UNLIKELY(!GFX_StartUpdate( render.scale.outWrite, render.scale.outPitch )))
			return false;
		render.fullFrame = true;
		render.scale.clearCache = false;
		RENDER_DrawLine = RENDER_ClearCacheHandler;
	} else {
		if (render.pal.changed) {
			/* Assume pal changes always do a full screen update anyway */
			if (GCC_UNLIKELY(!GFX_StartUpdate( render.scale.outWrite, render.scale.outPitch )))
				return false;
			RENDER_DrawLine = render.scale.linePalHandler;
			render.fullFrame = true;
		} else {
			RENDER_DrawLine = RENDER_StartLineHandler;
			if (GCC_UNLIKELY(CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO))) 
				render.fullFrame = true;
			else
				render.fullFrame = false;
		}
	}
	render.updating = true;
	return true;
}

static void RENDER_Halt( void ) {
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	GFX_EndUpdate( 0 );
	render.updating=false;
	render.active=false;
}

extern Bitu PIC_Ticks;
void RENDER_EndUpdate( bool abort ) {
	if (GCC_UNLIKELY(!render.updating))
		return;
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	if (GCC_UNLIKELY(CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO))) {
		Bitu pitch, flags;
		flags = 0;
		if (render.src.dblw != render.src.dblh) {
			if (render.src.dblw) flags|=CAPTURE_FLAG_DBLW;
			if (render.src.dblh) flags|=CAPTURE_FLAG_DBLH;
		}
		float fps = render.src.fps;
		pitch = render.scale.cachePitch;
		if (render.frameskip.max)
			fps /= 1+render.frameskip.max;
		CAPTURE_AddImage( render.src.width, render.src.height, render.src.bpp, pitch,
			flags, fps, (Bit8u *)&scalerSourceCache, (Bit8u*)&render.pal.rgb );
	}
	if ( render.scale.outWrite ) {
		GFX_EndUpdate( abort? NULL : Scaler_ChangedLines );
		render.frameskip.hadSkip[render.frameskip.index] = 0;
	} else {
#if 0
		Bitu total = 0, i;
		render.frameskip.hadSkip[render.frameskip.index] = 1;
		for (i = 0;i<RENDER_SKIP_CACHE;i++) 
			total += render.frameskip.hadSkip[i];
		LOG_MSG( "Skipped frame %d %d", PIC_Ticks, (total * 100) / RENDER_SKIP_CACHE );
#endif
	}
	render.frameskip.index = (render.frameskip.index + 1) & (RENDER_SKIP_CACHE - 1);
	render.updating=false;
}

static Bitu MakeAspectTable(Bitu skip,Bitu height,double scaley,Bitu miny) {
	Bitu i;
	double lines=0;
	Bitu linesadded=0;
	for (i=0;i<skip;i++)
		Scaler_Aspect[i] = 0;

	height += skip;
	for (i=skip;i<height;i++) {
		lines += scaley;
		if (lines >= miny) {
			Bitu templines = (Bitu)lines;
			lines -= templines;
			linesadded += templines;
			Scaler_Aspect[i] = templines;
		} else {
			Scaler_Aspect[i] = 0;
		}
	}
	return linesadded;
}


static void RENDER_Reset( void ) {
	Bitu width=render.src.width;
	Bitu height=render.src.height;
	bool dblw=render.src.dblw;
	bool dblh=render.src.dblh;

	double gfx_scalew;
	double gfx_scaleh;
	
	Bitu gfx_flags, xscale, yscale;
	ScalerSimpleBlock_t		*simpleBlock = &ScaleNormal1x;
	ScalerComplexBlock_t	*complexBlock = 0;
	if (render.aspect) {
		if (render.src.ratio>1.0) {
			gfx_scalew = 1;
			gfx_scaleh = render.src.ratio;
		} else {
			gfx_scalew = (1/render.src.ratio);
			gfx_scaleh = 1;
		}
	} else {
		gfx_scalew = 1;
		gfx_scaleh = 1;
	}
	if ((dblh && dblw) || (render.scale.forced && !dblh && !dblw)) {
		/* Initialize always working defaults */
		if (render.scale.size == 2)
			simpleBlock = &ScaleNormal2x;
		else if (render.scale.size == 3)
			simpleBlock = &ScaleNormal3x;
		else
			simpleBlock = &ScaleNormal1x;
		/* Maybe override them */
#if RENDER_USE_ADVANCED_SCALERS>0
		switch (render.scale.op) {
#if RENDER_USE_ADVANCED_SCALERS>2
		case scalerOpAdvInterp:
			if (render.scale.size == 2)
				complexBlock = &ScaleAdvInterp2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleAdvInterp3x;
			break;
		case scalerOpAdvMame:
			if (render.scale.size == 2)
				complexBlock = &ScaleAdvMame2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleAdvMame3x;
			break;
		case scalerOpHQ:
			if (render.scale.size == 2)
				complexBlock = &ScaleHQ2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleHQ3x;
			break;
		case scalerOpSuperSaI:
			if (render.scale.size == 2)
				complexBlock = &ScaleSuper2xSaI;
			break;
		case scalerOpSuperEagle:
			if (render.scale.size == 2)
				complexBlock = &ScaleSuperEagle;
			break;
		case scalerOpSaI:
			if (render.scale.size == 2)
				complexBlock = &Scale2xSaI;
			break;
#endif
		case scalerOpTV:
			if (render.scale.size == 2)
				simpleBlock = &ScaleTV2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleTV3x;
			break;
		case scalerOpRGB:
			if (render.scale.size == 2)
				simpleBlock = &ScaleRGB2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleRGB3x;
			break;
		case scalerOpScan:
			if (render.scale.size == 2)
				simpleBlock = &ScaleScan2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleScan3x;
			break;
		default:
			break;
		}
#endif
	} else if (dblw) {
		simpleBlock = &ScaleNormalDw;
	} else if (dblh) {
		simpleBlock = &ScaleNormalDh;
	} else  {
forcenormal:
		complexBlock = 0;
		simpleBlock = &ScaleNormal1x;
	}
	if (complexBlock) {
#if RENDER_USE_ADVANCED_SCALERS>1
		if ((width >= SCALER_COMPLEXWIDTH - 16) || height >= SCALER_COMPLEXHEIGHT - 16) {
			LOG_MSG("Scaler can't handle this resolution, going back to normal");
			goto forcenormal;
		}
#else
		goto forcenormal;
#endif
		gfx_flags = complexBlock->gfxFlags;
		xscale = complexBlock->xscale;	
		yscale = complexBlock->yscale;
//		LOG_MSG("Scaler:%s",complexBlock->name);
	} else {
		gfx_flags = simpleBlock->gfxFlags;
		xscale = simpleBlock->xscale;	
		yscale = simpleBlock->yscale;
//		LOG_MSG("Scaler:%s",simpleBlock->name);
	}
	switch (render.src.bpp) {
	case 8:
		render.src.start = ( render.src.width * 1) / sizeof(Bitu);
		if (gfx_flags & GFX_CAN_8)
			gfx_flags |= GFX_LOVE_8;
		else
			gfx_flags |= GFX_LOVE_32;
			break;
	case 15:
			render.src.start = ( render.src.width * 2) / sizeof(Bitu);
			gfx_flags |= GFX_LOVE_15;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 16:
			render.src.start = ( render.src.width * 2) / sizeof(Bitu);
			gfx_flags |= GFX_LOVE_16;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 32:
			render.src.start = ( render.src.width * 4) / sizeof(Bitu);
			gfx_flags |= GFX_LOVE_32;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	}
	gfx_flags=GFX_GetBestMode(gfx_flags);
	if (!gfx_flags) {
		if (!complexBlock && simpleBlock == &ScaleNormal1x) 
			E_Exit("Failed to create a rendering output");
		else 
			goto forcenormal;
	}
	width *= xscale;
	Bitu skip = complexBlock ? 1 : 0;
	if (gfx_flags & GFX_SCALING) {
		height = MakeAspectTable(skip, render.src.height, yscale, yscale );
	} else {
		if ((gfx_flags & GFX_CAN_RANDOM) && gfx_scaleh > 1) {
			gfx_scaleh *= yscale;
			height = MakeAspectTable( skip, render.src.height, gfx_scaleh, yscale );
		} else {
			gfx_flags &= ~GFX_CAN_RANDOM;		//Hardware surface when possible
			height = MakeAspectTable( skip, render.src.height, yscale, yscale);
		}
	}
/* Setup the scaler variables */
	gfx_flags=GFX_SetSize(width,height,gfx_flags,gfx_scalew,gfx_scaleh,&RENDER_CallBack);
	if (gfx_flags & GFX_CAN_8)
		render.scale.outMode = scalerMode8;
	else if (gfx_flags & GFX_CAN_15)
		render.scale.outMode = scalerMode15;
	else if (gfx_flags & GFX_CAN_16)
		render.scale.outMode = scalerMode16;
	else if (gfx_flags & GFX_CAN_32)
		render.scale.outMode = scalerMode32;
	else 
		E_Exit("Failed to create a rendering output");
	ScalerLineBlock_t *lineBlock;
	if (gfx_flags & GFX_HARDWARE) {
#if RENDER_USE_ADVANCED_SCALERS>1
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Linear[ render.scale.outMode ];
		} else
#endif
		{
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Linear;
		}
	} else {
#if RENDER_USE_ADVANCED_SCALERS>1
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Random[ render.scale.outMode ];
		} else
#endif
		{
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Random;
		}
	}
	switch (render.src.bpp) {
	case 8:
		render.scale.lineHandler = (*lineBlock)[0][render.scale.outMode];
		render.scale.linePalHandler = (*lineBlock)[4][render.scale.outMode];
		render.scale.inMode = scalerMode8;
		render.scale.cachePitch = render.src.width * 1;
		break;
	case 15:
		render.scale.lineHandler = (*lineBlock)[1][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode15;
		render.scale.cachePitch = render.src.width * 2;
		break;
	case 16:
		render.scale.lineHandler = (*lineBlock)[2][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode16;
		render.scale.cachePitch = render.src.width * 2;
		break;
	case 32:
		render.scale.lineHandler = (*lineBlock)[3][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode32;
		render.scale.cachePitch = render.src.width * 4;
		break;
	default:
		E_Exit("RENDER:Wrong source bpp %d", render.src.bpp );
	}
	render.scale.blocks = render.src.width / SCALER_BLOCKSIZE;
	render.scale.lastBlock = render.src.width % SCALER_BLOCKSIZE;
	render.scale.inHeight = render.src.height;
	/* Reset the palette change detection to it's initial value */
	render.pal.first= 0;
	render.pal.last = 255;
	render.pal.changed = false;
	memset(render.pal.modified, 0, sizeof(render.pal.modified));
	//Finish this frame using a copy only handler
	RENDER_DrawLine = RENDER_FinishLineHandler;
	render.scale.outWrite = 0;
	/* Signal the next frame to first reinit the cache */
	render.scale.clearCache = true;
	render.active=true;
}

static void RENDER_CallBack( GFX_CallBackFunctions_t function ) {
	if (function == GFX_CallBackStop) {
		RENDER_Halt( );	
		return;
	} else if (function == GFX_CallBackRedraw) {
		render.scale.clearCache = true;
		return;
	} else if ( function == GFX_CallBackReset) {
		GFX_EndUpdate( 0 );	
		RENDER_Reset();
	} else {
		E_Exit("Unhandled GFX_CallBackReset %d", function );
	}
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps,double ratio,bool dblw,bool dblh) {
	RENDER_Halt( );
	if (!width || !height || width > SCALER_MAXWIDTH || height > SCALER_MAXHEIGHT) { 
		return;	
	}
	if ( ratio > 1 ) {
		double target = height * ratio + 0.1;
		ratio = target / height;
	} else {
		//This would alter the width of the screen, we don't care about rounding errors here
	}
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.dblw=dblw;
	render.src.dblh=dblh;
	render.src.fps=fps;
	render.src.ratio=ratio;
	RENDER_Reset( );
}

extern void GFX_SetTitle(Bit32s cycles, Bits frameskip,bool paused);
static void IncreaseFrameSkip(bool pressed) {
	if (!pressed)
		return;
	if (render.frameskip.max<10) render.frameskip.max++;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}

static void DecreaseFrameSkip(bool pressed) {
	if (!pressed)
		return;
	if (render.frameskip.max>0) render.frameskip.max--;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}
/* Disabled as I don't want to waste a keybind for that. Might be used in the future (Qbix)
static void ChangeScaler(bool pressed) {
	if (!pressed)
		return;
	render.scale.op = (scalerOperation)((int)render.scale.op+1);
	if((render.scale.op) >= scalerLast || render.scale.size == 1) {
		render.scale.op = (scalerOperation)0;
		if(++render.scale.size > 3)
			render.scale.size = 1;
	}
	RENDER_CallBack( GFX_CallBackReset );
} */

void RENDER_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);

	//For restarting the renderer.
	static bool running = false;
	bool aspect = render.aspect;
	Bitu scalersize = render.scale.size;
	bool scalerforced = render.scale.forced;
	scalerOperation_t scaleOp = render.scale.op;

	render.pal.first=256;
	render.pal.last=0;
	render.aspect=section->Get_bool("aspect");
	render.frameskip.max=section->Get_int("frameskip");
	render.frameskip.count=0;
	std::string cline;
	std::string scaler;
	//Check for commandline paramters and parse them through the configclass so they get checked against allowed values
	if (control->cmdline->FindString("-scaler",cline,false)) {
		section->HandleInputline(std::string("scaler=") + cline);
	} else if (control->cmdline->FindString("-forcescaler",cline,false)) {
		section->HandleInputline(std::string("scaler=") + cline + " forced");
	}
	   
	Prop_multival* prop = section->Get_multival("scaler");
	scaler = prop->GetSection()->Get_string("type");
	std::string f = prop->GetSection()->Get_string("force");
	render.scale.forced = false;
	if(f == "forced") render.scale.forced = true;
   
	if (scaler == "none") { render.scale.op = scalerOpNormal;render.scale.size = 1; }
	else if (scaler == "normal2x") { render.scale.op = scalerOpNormal;render.scale.size = 2; }
	else if (scaler == "normal3x") { render.scale.op = scalerOpNormal;render.scale.size = 3; }
#if RENDER_USE_ADVANCED_SCALERS>2
	else if (scaler == "advmame2x") { render.scale.op = scalerOpAdvMame;render.scale.size = 2; }
	else if (scaler == "advmame3x") { render.scale.op = scalerOpAdvMame;render.scale.size = 3; }
	else if (scaler == "advinterp2x") { render.scale.op = scalerOpAdvInterp;render.scale.size = 2; }
	else if (scaler == "advinterp3x") { render.scale.op = scalerOpAdvInterp;render.scale.size = 3; }
	else if (scaler == "hq2x") { render.scale.op = scalerOpHQ;render.scale.size = 2; }
	else if (scaler == "hq3x") { render.scale.op = scalerOpHQ;render.scale.size = 3; }
	else if (scaler == "2xsai") { render.scale.op = scalerOpSaI;render.scale.size = 2; }
	else if (scaler == "super2xsai") { render.scale.op = scalerOpSuperSaI;render.scale.size = 2; }
	else if (scaler == "supereagle") { render.scale.op = scalerOpSuperEagle;render.scale.size = 2; }
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
	else if (scaler == "tv2x") { render.scale.op = scalerOpTV;render.scale.size = 2; }
	else if (scaler == "tv3x") { render.scale.op = scalerOpTV;render.scale.size = 3; }
	else if (scaler == "rgb2x"){ render.scale.op = scalerOpRGB;render.scale.size = 2; }
	else if (scaler == "rgb3x"){ render.scale.op = scalerOpRGB;render.scale.size = 3; }
	else if (scaler == "scan2x"){ render.scale.op = scalerOpScan;render.scale.size = 2; }
	else if (scaler == "scan3x"){ render.scale.op = scalerOpScan;render.scale.size = 3; }
#endif

	//If something changed that needs a ReInit
	// Only ReInit when there is a src.bpp (fixes crashes on startup and directly changing the scaler without a screen specified yet)
	if(running && render.src.bpp && ((render.aspect != aspect) || (render.scale.op != scaleOp) || 
				  (render.scale.size != scalersize) || (render.scale.forced != scalerforced) ||
				   render.scale.forced))
		RENDER_CallBack( GFX_CallBackReset );

	if(!running) render.updating=true;
	running = true;

	MAPPER_AddHandler(DecreaseFrameSkip,MK_f7,MMOD1,"decfskip","Dec Fskip");
	MAPPER_AddHandler(IncreaseFrameSkip,MK_f8,MMOD1,"incfskip","Inc Fskip");
	GFX_SetTitle(-1,render.frameskip.max,false);
}

