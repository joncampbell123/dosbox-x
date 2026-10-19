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

/* DOSBox-X: libdecor (client side window decorations) for the Wayland
   driver. It is loaded at runtime, so that it is not a requirement: without
   it, windows on compositors that do not draw server side decorations
   (GNOME, Weston) just have no title bar. */

#ifndef _SDL_waylanddecor_h
#define _SDL_waylanddecor_h

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR

#include <libdecor.h>

#define SDL_LIBDECOR_FUNCS \
	LIBDECOR_FUNC(struct libdecor *, libdecor_new, (struct wl_display *, struct libdecor_interface *)) \
	LIBDECOR_FUNC(void, libdecor_unref, (struct libdecor *)) \
	LIBDECOR_FUNC(int, libdecor_dispatch, (struct libdecor *, int)) \
	LIBDECOR_FUNC(struct libdecor_frame *, libdecor_decorate, (struct libdecor *, struct wl_surface *, struct libdecor_frame_interface *, void *)) \
	LIBDECOR_FUNC(void, libdecor_frame_unref, (struct libdecor_frame *)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_title, (struct libdecor_frame *, const char *)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_app_id, (struct libdecor_frame *, const char *)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_capabilities, (struct libdecor_frame *, enum libdecor_capabilities)) \
	LIBDECOR_FUNC(void, libdecor_frame_unset_capabilities, (struct libdecor_frame *, enum libdecor_capabilities)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_min_content_size, (struct libdecor_frame *, int, int)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_max_content_size, (struct libdecor_frame *, int, int)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_fullscreen, (struct libdecor_frame *, struct wl_output *)) \
	LIBDECOR_FUNC(void, libdecor_frame_unset_fullscreen, (struct libdecor_frame *)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_minimized, (struct libdecor_frame *)) \
	LIBDECOR_FUNC(void, libdecor_frame_set_visibility, (struct libdecor_frame *, bool)) \
	LIBDECOR_FUNC(void, libdecor_frame_map, (struct libdecor_frame *)) \
	LIBDECOR_FUNC(void, libdecor_frame_commit, (struct libdecor_frame *, struct libdecor_state *, struct libdecor_configuration *)) \
	LIBDECOR_FUNC(struct xdg_toplevel *, libdecor_frame_get_xdg_toplevel, (struct libdecor_frame *)) \
	LIBDECOR_FUNC(struct libdecor_state *, libdecor_state_new, (int, int)) \
	LIBDECOR_FUNC(void, libdecor_state_free, (struct libdecor_state *)) \
	LIBDECOR_FUNC(bool, libdecor_configuration_get_content_size, (struct libdecor_configuration *, struct libdecor_frame *, int *, int *)) \
	LIBDECOR_FUNC(bool, libdecor_configuration_get_window_state, (struct libdecor_configuration *, enum libdecor_window_state *))

#define LIBDECOR_FUNC(ret, name, params) typedef ret (*SDL_PFN_##name) params; extern SDL_PFN_##name SDL_##name;
SDL_LIBDECOR_FUNCS
#undef LIBDECOR_FUNC

extern void Wayland_LibdecorError(struct libdecor *context, enum libdecor_error error, const char *message);

/* returns nonzero if libdecor could be loaded */
extern int Wayland_LoadLibdecor(void);
extern void Wayland_UnloadLibdecor(void);

#endif /* SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR */

#endif /* _SDL_waylanddecor_h */
