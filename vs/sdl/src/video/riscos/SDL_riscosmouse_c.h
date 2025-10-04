/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_riscosvideo.h"

/* The implementation dependent data for the window manager cursor */
struct WMcursor {
	int w;
	int h;
	int hot_x;
	int hot_y;
	Uint8 *data;
};

/* Functions to be exported */
void RISCOS_FreeWMCursor(_THIS, WMcursor *cursor);
WMcursor *RISCOS_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y);

int RISCOS_ShowWMCursor(_THIS, WMcursor *cursor);
void FULLSCREEN_WarpWMCursor(_THIS, Uint16 x, Uint16 y);

int WIMP_ShowWMCursor(_THIS, WMcursor *cursor);
void WIMP_WarpWMCursor(_THIS, Uint16 x, Uint16 y);

