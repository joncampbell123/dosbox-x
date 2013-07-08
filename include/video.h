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

/* $Id: video.h,v 1.26 2009-05-27 09:15:41 qbix79 Exp $ */

#ifndef DOSBOX_VIDEO_H
#define DOSBOX_VIDEO_H

#define REDUCE_JOYSTICK_POLLING

typedef enum {
	GFX_CallBackReset,
	GFX_CallBackStop,
	GFX_CallBackRedraw
} GFX_CallBackFunctions_t;

typedef void (*GFX_CallBack_t)( GFX_CallBackFunctions_t function );

struct GFX_PalEntry {
	Bit8u r;
	Bit8u g;
	Bit8u b;
	Bit8u unused;
};

#define GFX_CAN_8		0x0001
#define GFX_CAN_15		0x0002
#define GFX_CAN_16		0x0004
#define GFX_CAN_32		0x0008

#define GFX_LOVE_8		0x0010
#define GFX_LOVE_15		0x0020
#define GFX_LOVE_16		0x0040
#define GFX_LOVE_32		0x0080

#define GFX_RGBONLY		0x0100

#define GFX_SCALING		0x1000
#define GFX_HARDWARE	0x2000

#define GFX_CAN_RANDOM	0x4000		//If the interface can also do random access surface

void GFX_Events(void);
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries);
Bitu GFX_GetBestMode(Bitu flags);
Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue);
Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t cb);

void GFX_ResetScreen(void);
void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);
bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch);
void GFX_EndUpdate( const Bit16u *changedLines );
void GFX_GetSize(int &width, int &height, bool &fullscreen);
void GFX_LosingFocus(void);

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void);
#endif

#if defined (REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void);
#endif

/* Mouse related */
void GFX_CaptureMouse(void);
extern bool mouselocked; //true if mouse is confined to window

#endif
