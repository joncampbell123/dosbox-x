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
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t unused;
};

#define GFX_CAN_8		0x0001u
#define GFX_CAN_15		0x0002u
#define GFX_CAN_16		0x0004u
#define GFX_CAN_32		0x0008u

#define GFX_LOVE_8		0x0010u
#define GFX_LOVE_15		0x0020u
#define GFX_LOVE_16		0x0040u
#define GFX_LOVE_32		0x0080u

#define GFX_RGBONLY		0x0100u

#define GFX_SCALING		0x1000u
#define GFX_HARDWARE	0x2000u

#define GFX_CAN_RANDOM	0x4000u		//If the interface can also do random access surface

void GFX_Events(void);
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries);
Bitu GFX_GetBestMode(Bitu flags);
Bitu GFX_GetRGB(uint8_t red,uint8_t green,uint8_t blue);
Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback);
void GFX_TearDown(void);
void GFX_SetShader(const char* src);
void GFX_ResetScreen(void);
void GFX_RestoreMode(void);
void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);
bool GFX_StartUpdate(uint8_t * & pixels,Bitu & pitch);
void GFX_EndUpdate( const uint16_t *changedLines );
void GFX_GetSize(int &width, int &height, bool &fullscreen);
void GFX_LosingFocus(void);

bool GFX_IsFullscreen(void);
void GFX_SwitchLazyFullscreen(bool lazy);
bool GFX_LazyFullscreenRequested(void);
void GFX_SwitchFullscreenNoReset(void);
void GFX_RestoreMode(void);
void GFX_UpdateSDLCaptureState(void);

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void);
#endif

#if defined (REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void);
#endif

/* Mouse related */
//! \brief Toggles mouse capture.
void GFX_CaptureMouse(void);
//! \brief Sets mouse capture state manually.
void GFX_CaptureMouse(bool capture);
//! \brief Notifies mouse capture according current state.
void CaptureMouseNotify();
//! \brief Notifies mouse capture according specific state.
void CaptureMouseNotify(bool capture);
extern bool mouselocked; //true if mouse is confined to window

#endif
