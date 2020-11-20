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

enum ASPECT_MODES {
    ASPECT_FALSE = 0
    ,ASPECT_TRUE
#if C_SURFACE_POSTRENDER_ASPECT
    ,ASPECT_NEAREST
    ,ASPECT_BILINEAR
#endif
};

#if !defined(USE_TTF)
#define USE_TTF
#endif

typedef struct {
	struct { 
		uint8_t red;
		uint8_t green;
		uint8_t blue;
		uint8_t unused;
	} rgb[256];
	union {
		uint16_t b16[256];
		uint32_t b32[256];
	} lut;
	bool changed;
	uint8_t modified[256];
	Bitu first;
	Bitu last;
} RenderPal_t;

typedef struct Render_t {
	struct {
		Bitu width, start;
		Bitu height;
		Bitu bpp;
		bool dblw,dblh;
		double ratio;
		float fps;
		double scrn_ratio;
	} src;
	struct {
		int count;
		int max;
		Bitu index;
		uint8_t hadSkip[RENDER_SKIP_CACHE];
	} frameskip;
	struct {
		Bitu size;
		scalerMode_t inMode;
		scalerMode_t outMode;
		scalerOperation_t op;
		bool clearCache;
		bool forced;
		bool hardware;
		ScalerLineHandler_t lineHandler;
		ScalerLineHandler_t linePalHandler;
		ScalerComplexHandler_t complexHandler;
		Bitu blocks, lastBlock;
		Bitu outPitch;
		uint8_t *outWrite;
		Bitu cachePitch;
		uint8_t *cacheRead;
		Bitu inHeight, inLine, outLine;
	} scale;
	struct {
		bool invalid;
		bool nextInvalid;
		uint8_t *pointer;
		Bitu width, height;
		int start_x, past_x, start_y, past_y, curr_y;
	} cache;
#if C_OPENGL
	char* shader_src;
    bool shader_def=false;
#endif
    RenderPal_t pal;
	bool updating;
	bool active;
	int aspect;
    bool aspectOffload;
	bool fullFrame;
	bool forceUpdate;
	bool autofit;
} Render_t;

#if defined(USE_TTF)
#include "SDL_ttf.h"
#define txtMaxCols 160
#define txtMaxLins 60
typedef struct {
	bool	inUse;
	char	fontName[32];
	TTF_Font *SDL_font;
	bool	DOSBox;								// is DOSBox-X internal TTF loaded, pointsizes should be even to look really nice
	int		pointsize;
	int		height;								// height of character cell
	int		width;								// width
	int		cursor;
	int		lins;								// number of lines 24-60
	int		cols;								// number of columns 80-160
	bool	fullScrn;							// in fake fullscreen
	int		offX;								// horizontal offset to center content
	int		offY;								// vertical ,,
} Render_ttf;

struct ttf_cell {
    uint16_t        chr;                        // unicode code point OR just the raw code point. set to ' ' (0x20) for empty space.
    unsigned int    fg:4;                       // foreground color (one of 16)
    unsigned int    bg:4;                       // background color (one of 16)
    unsigned int    doublewide:1;               // double-wide (e.g. PC-98 JIS), therefore skip next character cell.
    unsigned int    blink:1;                    // blink attribute
    unsigned int    underline:1;                // underline attribute
    unsigned int    unicode:1;                  // chr is unicode code point
    unsigned int    skipped:1;                  // adjacent (ignored) cell to a doublewide

    ttf_cell() {
        chr = 0x20;
        fg = 7;
        bg = 0;
        doublewide = 0;
        underline = 0;
        unicode = 0;
        skipped = 0;
        blink = 0;
    }

    bool operator==(const ttf_cell &lhs) const {
        return  chr         == lhs.chr &&
                fg          == lhs.fg &&
                bg          == lhs.bg &&
                doublewide  == lhs.doublewide &&
                blink       == lhs.blink &&
                skipped     == lhs.skipped &&
                underline   == lhs.underline &&
                unicode     == lhs.unicode;
    }
    bool operator!=(const ttf_cell &lhs) const {
        return !(*this == lhs);
    }
};
// FIXME: Perhaps the TTF output code should just render unicode code points and vga_draw should do the code page conversion

extern ttf_cell curAttrChar[];					// currently displayed textpage
extern ttf_cell newAttrChar[];					// to be replaced by
extern Render_ttf ttf;
#endif
extern Render_t render;
extern Bitu last_gfx_flags;
extern ScalerLineHandler_t RENDER_DrawLine;
void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps,double scrn_ratio);
bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_SetPal(uint8_t entry,uint8_t red,uint8_t green,uint8_t blue);
bool RENDER_GetForceUpdate(void);
void RENDER_SetForceUpdate(bool);

#endif
