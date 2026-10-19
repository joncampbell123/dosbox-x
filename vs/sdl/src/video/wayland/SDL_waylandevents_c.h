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

/* DOSBox-X: Wayland input */

#include "SDL_waylandvideo.h"

extern void Wayland_InitOSKeymap(_THIS);
extern void Wayland_PumpEvents(_THIS);
extern void Wayland_UpdateMouse(_THIS);

/* read and dispatch events, waiting for some if blocking. -1 if the connection is lost */
extern int Wayland_Dispatch(_THIS, int blocking);

extern void Wayland_AddSeat(_THIS);
extern void Wayland_RemoveSeat(_THIS);

/* lock the pointer and use relative motion if relative_wanted, or stop */
extern void Wayland_UpdateRelativeMode(_THIS);
extern void Wayland_ReleaseRelativeMode(_THIS);
