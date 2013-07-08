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

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

// 0: complex scalers off, scaler cache off, some simple scalers off, memory requirements reduced
// 1: complex scalers off, scaler cache off, all simple scalers on
// 2: complex scalers off, scaler cache on
// 3: complex scalers on
#define RENDER_USE_ADVANCED_SCALERS 3

#include "../src/gui/render_scalers.h"

#define RENDER_SKIP_CACHE	16
//Enable this for scalers to support 0 input for empty lines
//#define RENDER_NULL_INPUT

typedef struct {
	struct { 
		Bit8u red;
		Bit8u green;
		Bit8u blue;
		Bit8u unused;
	} rgb[256];
	union {
		Bit16u b16[256];
		Bit32u b32[256];
	} lut;
	bool changed;
	Bit8u modified[256];
	Bitu first;
	Bitu last;
} RenderPal_t;

typedef struct {
	struct {
		Bitu width, start;
		Bitu height;
		Bitu bpp;
		bool dblw,dblh;
		double ratio;
		float fps;
	} src;
	struct {
		Bitu count;
		Bitu max;
		Bitu index;
		Bit8u hadSkip[RENDER_SKIP_CACHE];
	} frameskip;
	struct {
		Bitu size;
		scalerMode_t inMode;
		scalerMode_t outMode;
		scalerOperation_t op;
		bool clearCache;
		bool forced;
		ScalerLineHandler_t lineHandler;
		ScalerLineHandler_t linePalHandler;
		ScalerComplexHandler_t complexHandler;
		Bitu blocks, lastBlock;
		Bitu outPitch;
		Bit8u *outWrite;
		Bitu cachePitch;
		Bit8u *cacheRead;
		Bitu inHeight, inLine, outLine;
	} scale;
	RenderPal_t pal;
	bool updating;
	bool active;
	bool aspect;
	bool fullFrame;
} Render_t;

extern Render_t render;
extern ScalerLineHandler_t RENDER_DrawLine;
void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps,double ratio,bool dblw,bool dblh);
bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue);


#endif
