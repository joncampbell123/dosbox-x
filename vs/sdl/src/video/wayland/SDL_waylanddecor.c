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

/* DOSBox-X: libdecor, loaded at runtime */

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR

#include "SDL_loadso.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylanddecor.h"

#define LIBDECOR_FUNC(ret, name, params) SDL_PFN_##name SDL_##name = NULL;
SDL_LIBDECOR_FUNCS
#undef LIBDECOR_FUNC

static void *libdecor_handle = NULL;

int Wayland_LoadLibdecor(void)
{
	const char *name = SDL_getenv("SDL_VIDEO_WAYLAND_LIBDECOR");

	if ( libdecor_handle ) {
		return 1;
	}
	libdecor_handle = SDL_LoadObject(name ? name : "libdecor-0.so.0");
	if ( libdecor_handle == NULL ) {
		return 0;
	}

#define LIBDECOR_FUNC(ret, fn, params) \
	SDL_##fn = (SDL_PFN_##fn)SDL_LoadFunction(libdecor_handle, #fn); \
	if ( SDL_##fn == NULL ) { Wayland_UnloadLibdecor(); return 0; }
	SDL_LIBDECOR_FUNCS
#undef LIBDECOR_FUNC

	return 1;
}

void Wayland_UnloadLibdecor(void)
{
	if ( libdecor_handle ) {
		SDL_UnloadObject(libdecor_handle);
		libdecor_handle = NULL;
	}
#define LIBDECOR_FUNC(ret, name, params) SDL_##name = NULL;
	SDL_LIBDECOR_FUNCS
#undef LIBDECOR_FUNC
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR */
