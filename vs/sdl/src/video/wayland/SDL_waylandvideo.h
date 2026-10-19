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

/* DOSBox-X: Wayland video driver for SDL 1.2 */

#ifndef _SDL_waylandvideo_h
#define _SDL_waylandvideo_h

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"

#include "protocols/xdg-shell-client-protocol.h"
#include "protocols/xdg-decoration-unstable-v1-client-protocol.h"
#include "protocols/relative-pointer-unstable-v1-client-protocol.h"
#include "protocols/pointer-constraints-unstable-v1-client-protocol.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this

/* how the window content is presented */
enum {
	WAYLAND_MODE_SHM = 0,	/* SDL software surface, copied to wl_shm buffers */
	WAYLAND_MODE_OPENGL,	/* EGL */
	WAYLAND_MODE_VULKAN	/* the application presents with Vulkan */
};

#define WAYLAND_MAX_BUFFERS 4

struct Wayland_Buffer {
	struct wl_buffer *buffer;
	void *data;
	size_t size;
	int w, h, stride;
	int busy;		/* attached, not released by the compositor yet */
};

struct libdecor;
struct libdecor_frame;

/* Private display data */
struct SDL_PrivateVideoData {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	Uint32 compositor_version;
	struct wl_shm *shm;
	struct xdg_wm_base *wm_base;
	struct zxdg_decoration_manager_v1 *decoration_manager;
	struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
	struct zwp_pointer_constraints_v1 *pointer_constraints;
	struct wl_output *output;
	int output_w, output_h;	/* current mode of the (first) output */

	/* input */
	struct wl_seat *seat;
	Uint32 seat_version;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct zwp_relative_pointer_v1 *relative_pointer;
	struct zwp_locked_pointer_v1 *locked_pointer;
	int relative_mode;	/* the pointer is locked, motion is relative */
	int relative_wanted;	/* input grabbed with the cursor hidden */
	double relative_frac_x, relative_frac_y;
	double wheel_accum;
	Uint32 pointer_serial;	/* of the last pointer enter, for set_cursor */
	int pointer_focus;
	int pointer_x, pointer_y;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;

	/* cursor */
	struct wl_surface *cursor_surface;
	struct wl_cursor_theme *cursor_theme;
	WMcursor *cursor;		/* the cursor to show, NULL if hidden */

	/* the window */
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct zxdg_toplevel_decoration_v1 *decoration;
	struct libdecor *libdecor;
	struct libdecor_frame *libdecor_frame;
	int configured;		/* got the first configure */
	int content_mode;	/* WAYLAND_MODE_* */
	int window_w, window_h;	/* size of the content */
	int configure_w, configure_h;	/* size suggested by the compositor, 0 if none */
	int fullscreen;
	int resizable;
	char *app_id;

	/* WAYLAND_MODE_SHM */
	struct Wayland_Buffer buffers[WAYLAND_MAX_BUFFERS];
	void *framebuffer;	/* SDL_VideoSurface pixels */
	int present_pending;	/* an update was dropped, all buffers were busy */

	/* WAYLAND_MODE_OPENGL */
	struct wl_egl_window *egl_window;
};


/* the window: create, or recreate for another content mode */
extern int Wayland_CreateWindow(_THIS, int content_mode);
extern void Wayland_DestroyWindow(_THIS);
/* an unlinked file of the given size for wl_shm, -1 on error */
extern int Wayland_CreateShmFile(size_t size);
/* present the software surface */
extern void Wayland_PresentShm(_THIS, int numrects, SDL_Rect *rects);

#endif /* _SDL_waylandvideo_h */
