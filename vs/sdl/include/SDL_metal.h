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

/** @file SDL_metal.h
 *  DOSBox-X specific: Metal support for SDL 1.2 (Mac OS X), modeled after
 *  the SDL 2.0 Metal API. There is only one window in SDL 1.2, so the
 *  functions do not take a window or view parameter.
 *
 *  Usage:
 *   - SDL_SetVideoMode() with SDL_METAL to get a window that SDL will not
 *     draw to (the returned surface has no pixels, like SDL_OPENGL). Its
 *     view is backed by a CAMetalLayer.
 *   - SDL_Metal_GetLayer() for that layer: set its device and pixel format,
 *     and draw into its drawables.
 *
 *  SDL_SetVideoMode() may destroy and recreate the window (fullscreen and
 *  windowed, or another frame style), and with it the layer: get the layer
 *  again after every SDL_SetVideoMode().
 *
 *  Supported by the quartz video driver.
 */

#ifndef _SDL_metal_h
#define _SDL_metal_h

#include "SDL_stdinc.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The CAMetalLayer of the video mode set with SDL_METAL (cast it to
 * CAMetalLayer *), or NULL if there is none.
 */
extern DECLSPEC void * SDLCALL SDL_Metal_GetLayer(void);

/**
 * The size of the layer's drawables in pixels (the window size, times the
 * backing scale factor on HighDPI displays), 0 x 0 if there is no layer.
 */
extern DECLSPEC void SDLCALL SDL_Metal_GetDrawableSize(int *w, int *h);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_metal_h */
