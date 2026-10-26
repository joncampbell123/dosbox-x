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

/* DOSBox-X: Wayland video driver for SDL 1.2

   The window is an xdg-shell toplevel. Windows are decorated by the
   compositor (xdg-decoration) when it can, else by libdecor if it is
   available. Software surfaces are copied to wl_shm buffers by
   SDL_UpdateRects(), OpenGL goes through EGL, and SDL_VULKAN windows are
   left to the application (see SDL_vulkan.h).

   Wayland has no global coordinates, so there is no window positioning,
   and the mouse can not be warped. Grabbing the input with the cursor
   hidden locks the pointer and gives relative motion instead. */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_syswm.h"
#include "SDL_timer.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../SDL_vulkan_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandmouse_c.h"
#include "SDL_waylandgl_c.h"
#include "SDL_waylandvulkan_c.h"
#include "SDL_waylanddecor.h"

#define WAYLANDVID_DRIVER_NAME "wayland"

/* Initialization/Query functions */
static int Wayland_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **Wayland_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *Wayland_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int Wayland_ToggleFullScreen(_THIS, int on);
static int Wayland_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void Wayland_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
static void Wayland_VideoQuit(_THIS);

/* Hardware surface functions */
static int Wayland_AllocHWSurface(_THIS, SDL_Surface *surface);
static int Wayland_LockHWSurface(_THIS, SDL_Surface *surface);
static void Wayland_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void Wayland_FreeHWSurface(_THIS, SDL_Surface *surface);

/* Window manager functions */
static void Wayland_SetCaption(_THIS, const char *title, const char *icon);
static int Wayland_IconifyWindow(_THIS);
static SDL_GrabMode Wayland_GrabInput(_THIS, SDL_GrabMode mode);
#if SDL_VIDEO_DRIVER_X11
static int Wayland_GetWMInfo(_THIS, SDL_SysWMinfo *info);
#endif

static void Wayland_ApplyWindowState(_THIS, Uint32 flags);
static void Wayland_DestroyBuffers(_THIS);

/* Wayland driver bootstrap functions */

static int Wayland_Available(void)
{
	struct wl_display *display;

	/* wl_display_connect() would try "wayland-0" without it */
	if ( SDL_getenv("WAYLAND_DISPLAY") == NULL && SDL_getenv("WAYLAND_SOCKET") == NULL ) {
		const char *driver = SDL_getenv("SDL_VIDEODRIVER");
		if ( driver == NULL || SDL_strcmp(driver, WAYLANDVID_DRIVER_NAME) != 0 ) {
			return(0);
		}
	}

	display = wl_display_connect(NULL);
	if ( display == NULL ) {
		return(0);
	}
	wl_display_disconnect(display);
	return(1);
}

static void Wayland_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->gl_data);
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *Wayland_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
		device->gl_data = (struct SDL_PrivateGLData *)
				SDL_malloc((sizeof *device->gl_data));
	}
	if ( (device == NULL) || (device->hidden == NULL) || (device->gl_data == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device->hidden);
			SDL_free(device->gl_data);
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));
	SDL_memset(device->gl_data, 0, (sizeof *device->gl_data));

	/* Set the function pointers */
	device->VideoInit = Wayland_VideoInit;
	device->ListModes = Wayland_ListModes;
	device->SetVideoMode = Wayland_SetVideoMode;
	device->ToggleFullScreen = Wayland_ToggleFullScreen;
	device->UpdateMouse = Wayland_UpdateMouse;
	device->CreateYUVOverlay = NULL;
	device->SetColors = Wayland_SetColors;
	device->UpdateRects = Wayland_UpdateRects;
	device->VideoQuit = Wayland_VideoQuit;
	device->AllocHWSurface = Wayland_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = Wayland_LockHWSurface;
	device->UnlockHWSurface = Wayland_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = Wayland_FreeHWSurface;
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	device->GL_LoadLibrary = Wayland_GL_LoadLibrary;
	device->GL_GetProcAddress = Wayland_GL_GetProcAddress;
	device->GL_GetAttribute = Wayland_GL_GetAttribute;
	device->GL_MakeCurrent = Wayland_GL_MakeCurrent;
	device->GL_SwapBuffers = Wayland_GL_SwapBuffers;
	device->GL_CreateSharedContext = Wayland_GL_CreateSharedContext;
	device->GL_DeleteContext = Wayland_GL_DeleteContext;
	device->GL_MakeContextCurrent = Wayland_GL_MakeContextCurrent;
	device->GL_GetVideoModeContext = Wayland_GL_GetVideoModeContext;
#endif
	device->Vulkan_GetInstanceExtensions = Wayland_Vulkan_GetInstanceExtensions;
	device->Vulkan_CreateSurface = Wayland_Vulkan_CreateSurface;
	device->SetCaption = Wayland_SetCaption;
	device->SetIcon = NULL;
	device->IconifyWindow = Wayland_IconifyWindow;
	device->GrabInput = Wayland_GrabInput;
#if SDL_VIDEO_DRIVER_X11
	device->GetWMInfo = Wayland_GetWMInfo;
#endif
	device->FreeWMCursor = Wayland_FreeWMCursor;
	device->CreateWMCursor = Wayland_CreateWMCursor;
	device->ShowWMCursor = Wayland_ShowWMCursor;
	device->WarpWMCursor = NULL; /* not possible, SDL posts the motion instead */
	device->CheckMouseMode = Wayland_CheckMouseMode;
	device->InitOSKeymap = Wayland_InitOSKeymap;
	device->PumpEvents = Wayland_PumpEvents;

	device->free = Wayland_DeleteDevice;

	return device;
}

VideoBootStrap WAYLAND_bootstrap = {
	WAYLANDVID_DRIVER_NAME, "SDL Wayland video driver",
	Wayland_Available, Wayland_CreateDevice
};

/* wl_output: only the first output's mode is used, for the "desktop" size */

static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
	int32_t physical_width, int32_t physical_height, int32_t subpixel,
	const char *make, const char *model, int32_t transform)
{
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
	int32_t width, int32_t height, int32_t refresh)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;

	if ( flags & WL_OUTPUT_MODE_CURRENT ) {
		this->hidden->output_w = width;
		this->hidden->output_h = height;
	}
}

static void output_done(void *data, struct wl_output *output)
{
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
}

static const struct wl_output_listener output_listener = {
	output_geometry,
	output_mode,
	output_done,
	output_scale
};

static void wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
	xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	wm_base_ping
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( SDL_strcmp(interface, wl_compositor_interface.name) == 0 ) {
		wl->compositor_version = SDL_min(version, 4);
		wl->compositor = (struct wl_compositor *)wl_registry_bind(registry, name,
			&wl_compositor_interface, wl->compositor_version);
	} else if ( SDL_strcmp(interface, wl_shm_interface.name) == 0 ) {
		wl->shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if ( SDL_strcmp(interface, xdg_wm_base_interface.name) == 0 ) {
		wl->wm_base = (struct xdg_wm_base *)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wl->wm_base, &wm_base_listener, this);
	} else if ( SDL_strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0 ) {
		wl->decoration_manager = (struct zxdg_decoration_manager_v1 *)wl_registry_bind(registry, name,
			&zxdg_decoration_manager_v1_interface, 1);
	} else if ( SDL_strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0 ) {
		wl->relative_pointer_manager = (struct zwp_relative_pointer_manager_v1 *)wl_registry_bind(registry, name,
			&zwp_relative_pointer_manager_v1_interface, 1);
	} else if ( SDL_strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0 ) {
		wl->pointer_constraints = (struct zwp_pointer_constraints_v1 *)wl_registry_bind(registry, name,
			&zwp_pointer_constraints_v1_interface, 1);
	} else if ( SDL_strcmp(interface, wl_seat_interface.name) == 0 ) {
		if ( wl->seat == NULL ) {
			wl->seat_version = SDL_min(version, 5);
			wl->seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, wl->seat_version);
			Wayland_AddSeat(this);
		}
	} else if ( SDL_strcmp(interface, wl_output_interface.name) == 0 ) {
		if ( wl->output == NULL && version >= 2 ) {
			wl->output = (struct wl_output *)wl_registry_bind(registry, name, &wl_output_interface, 2);
			wl_output_add_listener(wl->output, &output_listener, this);
		}
	}
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_global,
	registry_global_remove
};

static int Wayland_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	const char *app_id;

	wl->display = wl_display_connect(NULL);
	if ( wl->display == NULL ) {
		SDL_SetError("Couldn't connect to the Wayland display");
		return(-1);
	}

	wl->registry = wl_display_get_registry(wl->display);
	wl_registry_add_listener(wl->registry, &registry_listener, this);
	/* once for the globals, once more for their initial events */
	wl_display_roundtrip(wl->display);
	wl_display_roundtrip(wl->display);

	if ( wl->compositor == NULL || wl->shm == NULL || wl->wm_base == NULL ) {
		SDL_SetError("The Wayland compositor does not support xdg-shell");
		Wayland_VideoQuit(this);
		return(-1);
	}

	wl->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if ( wl->xkb_context == NULL ) {
		SDL_SetError("Couldn't create the xkbcommon context");
		Wayland_VideoQuit(this);
		return(-1);
	}

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	/* decorations: the compositor draws them if it can, else libdecor */
	if ( wl->decoration_manager == NULL && Wayland_LoadLibdecor() ) {
		static struct libdecor_interface libdecor_iface;
		libdecor_iface.error = Wayland_LibdecorError;
		wl->libdecor = SDL_libdecor_new(wl->display, &libdecor_iface);
	}
#endif

	/* app_id, the Wayland equivalent of the X11 WM_CLASS */
	app_id = SDL_getenv("SDL_VIDEO_WAYLAND_WMCLASS");
	if ( app_id == NULL ) app_id = SDL_getenv("SDL_VIDEO_X11_WMCLASS");
#if defined(__GLIBC__)
	if ( app_id == NULL ) app_id = program_invocation_short_name;
#endif
	if ( app_id == NULL ) app_id = "SDL_App";
	wl->app_id = SDL_strdup(app_id);

	/* SDL can always emulate other depths with a shadow surface */
	vformat->BitsPerPixel = 32;
	vformat->BytesPerPixel = 4;
	vformat->Rmask = 0x00FF0000;
	vformat->Gmask = 0x0000FF00;
	vformat->Bmask = 0x000000FF;

	this->info.wm_available = 1;
	this->info.current_w = wl->output_w;
	this->info.current_h = wl->output_h;

#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	this->gl_config.driver_loaded = 1;
#endif

	return(0);
}

/* Windows can be any size. Fullscreen is the output's size, like the X11
   driver without XRandR (SDL centers smaller modes in it). The same for any
   depth: the screen is 32 bpp, SDL emulates the others with a shadow surface.
   (A depth with no modes would make SDL_GetVideoMode() look through the
   "list" of a depth that returns -1.) */
static SDL_Rect **Wayland_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( (flags & SDL_FULLSCREEN) && wl->output_w > 0 && wl->output_h > 0 ) {
		wl->fullscreen_mode.x = 0;
		wl->fullscreen_mode.y = 0;
		wl->fullscreen_mode.w = (Uint16)wl->output_w;
		wl->fullscreen_mode.h = (Uint16)wl->output_h;
		wl->fullscreen_modes[0] = &wl->fullscreen_mode;
		wl->fullscreen_modes[1] = NULL;
		return(wl->fullscreen_modes);
	}
	return((SDL_Rect **)-1);
}

/* the window */

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
	int32_t width, int32_t height, struct wl_array *states)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	uint32_t *state;

	this->hidden->configure_w = width;
	this->hidden->configure_h = height;
	this->hidden->configure_fullscreen = 0;
	wl_array_for_each(state, states) {
		if ( *state == XDG_TOPLEVEL_STATE_FULLSCREEN ) {
			this->hidden->configure_fullscreen = 1;
		}
	}
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	SDL_PrivateQuit();
}

static const struct xdg_toplevel_listener toplevel_listener = {
	toplevel_configure,
	toplevel_close
};

/* the compositor suggests a size: the application gets a resize event.
   Not for a configure that is still for the other fullscreen state (it
   comes after the application switched, before the compositor did). */
static void Wayland_HandleConfigureSize(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->configure_w > 0 && wl->configure_h > 0 &&
	     wl->configure_fullscreen == wl->fullscreen &&
	     (wl->configure_w != wl->window_w || wl->configure_h != wl->window_h) ) {
		SDL_PrivateResize(wl->configure_w, wl->configure_h);
	}
}

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;

	xdg_surface_ack_configure(xdg_surface, serial);
	this->hidden->configured = 1;
	Wayland_HandleConfigureSize(this);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	xdg_surface_configure
};

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
void Wayland_LibdecorError(struct libdecor *context, enum libdecor_error error, const char *message)
{
	SDL_SetError("libdecor: %s", message);
}

static void decor_configure(struct libdecor_frame *frame, struct libdecor_configuration *configuration, void *data)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct libdecor_state *state;
	int w = 0, h = 0;

	if ( SDL_libdecor_configuration_get_content_size(configuration, frame, &w, &h) ) {
		wl->configure_w = w;
		wl->configure_h = h;
	} else {
		wl->configure_w = 0;
		wl->configure_h = 0;
	}
	{
		enum libdecor_window_state window_state;
		if ( SDL_libdecor_configuration_get_window_state(configuration, &window_state) ) {
			wl->configure_fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) ? 1 : 0;
		}
	}

	/* the content stays the size the application set, until it resizes it */
	w = wl->window_w > 0 ? wl->window_w : (w > 0 ? w : 640);
	h = wl->window_h > 0 ? wl->window_h : (h > 0 ? h : 480);
	state = SDL_libdecor_state_new(w, h);
	SDL_libdecor_frame_commit(frame, state, configuration);
	SDL_libdecor_state_free(state);

	wl->configured = 1;
	Wayland_HandleConfigureSize(this);
}

static void decor_close(struct libdecor_frame *frame, void *data)
{
	SDL_PrivateQuit();
}

static void decor_commit(struct libdecor_frame *frame, void *data)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;

	if ( this->hidden->surface ) {
		wl_surface_commit(this->hidden->surface);
	}
}

static void decor_dismiss_popup(struct libdecor_frame *frame, const char *seat_name, void *data)
{
}

static struct libdecor_frame_interface decor_frame_iface = {
	decor_configure,
	decor_close,
	decor_commit,
	decor_dismiss_popup
};
#endif

int Wayland_CreateWindow(_THIS, int content_mode)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	const char *title = this->wm_title ? this->wm_title : "";

	wl->surface = wl_compositor_create_surface(wl->compositor);
	if ( wl->surface == NULL ) {
		SDL_SetError("Couldn't create a Wayland surface");
		return(-1);
	}
	wl->content_mode = content_mode;
	wl->configured = 0;
	wl->shown = 0;
	wl->hold = 0;
	wl->configure_w = 0;
	wl->configure_h = 0;

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor ) {
		wl->libdecor_frame = SDL_libdecor_decorate(wl->libdecor, wl->surface, &decor_frame_iface, this);
		if ( wl->libdecor_frame == NULL ) {
			/* do without it */
			SDL_libdecor_unref(wl->libdecor);
			wl->libdecor = NULL;
		}
	}
	if ( wl->libdecor_frame ) {
		SDL_libdecor_frame_set_title(wl->libdecor_frame, title);
		SDL_libdecor_frame_set_app_id(wl->libdecor_frame, wl->app_id);
		SDL_libdecor_frame_map(wl->libdecor_frame);
	} else
#endif
	{
		wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm_base, wl->surface);
		xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, this);
		wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
		xdg_toplevel_add_listener(wl->xdg_toplevel, &toplevel_listener, this);
		xdg_toplevel_set_title(wl->xdg_toplevel, title);
		xdg_toplevel_set_app_id(wl->xdg_toplevel, wl->app_id);
		if ( wl->decoration_manager ) {
			wl->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
				wl->decoration_manager, wl->xdg_toplevel);
			zxdg_toplevel_decoration_v1_set_mode(wl->decoration,
				ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
		wl_surface_commit(wl->surface);
	}

	/* nothing may be drawn before the first configure */
	while ( !wl->configured ) {
		if ( Wayland_Dispatch(this, 1) < 0 ) {
			SDL_SetError("Lost the connection to the Wayland display");
			Wayland_DestroyWindow(this);
			return(-1);
		}
	}

	/* the pointer lock belongs to the surface */
	Wayland_UpdateRelativeMode(this);
	return(0);
}

void Wayland_DestroyWindow(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->surface == NULL ) {
		return;
	}

	/* the application's Vulkan surface goes first */
	if ( wl->content_mode == WAYLAND_MODE_VULKAN ) {
		SDL_Vulkan_NotifyWindowDestroy();
	}
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	Wayland_GL_DestroyWindow(this);
#endif
	Wayland_ReleaseRelativeMode(this);
	Wayland_DestroyBuffers(this);
	if ( wl->framebuffer ) {
		SDL_free(wl->framebuffer);
		wl->framebuffer = NULL;
		if ( this->screen ) {
			this->screen->pixels = NULL;
		}
	}

	if ( wl->decoration ) {
		zxdg_toplevel_decoration_v1_destroy(wl->decoration);
		wl->decoration = NULL;
	}
	if ( wl->xdg_toplevel ) {
		xdg_toplevel_destroy(wl->xdg_toplevel);
		wl->xdg_toplevel = NULL;
	}
	if ( wl->xdg_surface ) {
		xdg_surface_destroy(wl->xdg_surface);
		wl->xdg_surface = NULL;
	}
#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor_frame ) {
		SDL_libdecor_frame_unref(wl->libdecor_frame);
		wl->libdecor_frame = NULL;
	}
#endif
	wl_surface_destroy(wl->surface);
	wl->surface = NULL;
	wl->configured = 0;
	wl->pointer_focus = 0;
	wl_display_flush(wl->display);
}

static void Wayland_ApplyWindowState(_THIS, Uint32 flags)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	int w = wl->window_w, h = wl->window_h;

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor_frame ) {
		struct libdecor_state *state;

		if ( wl->fullscreen ) {
			SDL_libdecor_frame_set_fullscreen(wl->libdecor_frame, NULL);
		} else {
			SDL_libdecor_frame_unset_fullscreen(wl->libdecor_frame);
		}
		if ( wl->resizable ) {
			SDL_libdecor_frame_set_capabilities(wl->libdecor_frame, LIBDECOR_ACTION_RESIZE);
			SDL_libdecor_frame_set_min_content_size(wl->libdecor_frame, 0, 0);
			SDL_libdecor_frame_set_max_content_size(wl->libdecor_frame, 0, 0);
		} else {
			SDL_libdecor_frame_unset_capabilities(wl->libdecor_frame, LIBDECOR_ACTION_RESIZE);
			SDL_libdecor_frame_set_min_content_size(wl->libdecor_frame, w, h);
			SDL_libdecor_frame_set_max_content_size(wl->libdecor_frame, w, h);
		}
		SDL_libdecor_frame_set_visibility(wl->libdecor_frame, !(flags & SDL_NOFRAME));
		state = SDL_libdecor_state_new(w, h);
		SDL_libdecor_frame_commit(wl->libdecor_frame, state, NULL);
		SDL_libdecor_state_free(state);
		return;
	}
#endif

	if ( wl->fullscreen ) {
		xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
	} else {
		xdg_toplevel_unset_fullscreen(wl->xdg_toplevel);
	}
	if ( wl->resizable ) {
		xdg_toplevel_set_min_size(wl->xdg_toplevel, 0, 0);
		xdg_toplevel_set_max_size(wl->xdg_toplevel, 0, 0);
	} else {
		xdg_toplevel_set_min_size(wl->xdg_toplevel, w, h);
		xdg_toplevel_set_max_size(wl->xdg_toplevel, w, h);
	}
	if ( wl->decoration ) {
		zxdg_toplevel_decoration_v1_set_mode(wl->decoration, (flags & SDL_NOFRAME) ?
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE :
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
}

static SDL_Surface *Wayland_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	int content_mode;

	if ( flags & SDL_OPENGL ) {
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
		content_mode = WAYLAND_MODE_OPENGL;
#else
		SDL_SetError("OpenGL (EGL) support was not compiled into the Wayland driver");
		return(NULL);
#endif
	} else if ( flags & SDL_VULKAN ) {
		content_mode = WAYLAND_MODE_VULKAN;
	} else {
		content_mode = WAYLAND_MODE_SHM;
	}

	wl->window_w = width;
	wl->window_h = height;
	wl->fullscreen = (flags & SDL_FULLSCREEN) ? 1 : 0;
	wl->resizable = (flags & SDL_RESIZABLE) ? 1 : 0;

	/* EGL, Vulkan and wl_shm can't share a surface, it is made again when that changes */
	if ( wl->surface == NULL || wl->content_mode != content_mode ) {
		Wayland_DestroyWindow(this);
		if ( Wayland_CreateWindow(this, content_mode) < 0 ) {
			return(NULL);
		}
	}
	Wayland_ApplyWindowState(this, flags);

	/* 32 bpp, SDL emulates other depths with a shadow surface */
	if ( ! SDL_ReallocFormat(current, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0) ) {
		return(NULL);
	}

	/* The old buffers are the wrong size now. Wayland_PresentShm() replaces
	   them, a buffer the compositor is still showing is left alone until it
	   comes back. An update dropped at the old size is no longer wanted. */
	wl->present_pending = 0;
	if ( wl->framebuffer ) {
		SDL_free(wl->framebuffer);
		wl->framebuffer = NULL;
	}

	current->flags = flags & (SDL_FULLSCREEN|SDL_RESIZABLE|SDL_NOFRAME|SDL_HAX_NOREFRESH);
	current->w = width;
	current->h = height;
	current->pitch = width * 4;
	current->pixels = NULL;

	switch ( content_mode ) {
	case WAYLAND_MODE_SHM:
		wl->framebuffer = SDL_calloc(1, (size_t)current->pitch * height);
		if ( wl->framebuffer == NULL ) {
			SDL_OutOfMemory();
			return(NULL);
		}
		current->pixels = wl->framebuffer;
		this->UpdateRects = Wayland_UpdateRects;
		/* A new window is shown right away. A window that is already showing
		   keeps its last frame until the application draws at the new size,
		   rather than flashing black on every resize. */
		if ( !wl->shown ) {
			Wayland_PresentShm(this, 0, NULL);
		} else {
			wl->hold = 1;
			wl->hold_ticks = SDL_GetTicks();
		}
		break;
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	case WAYLAND_MODE_OPENGL:
		if ( Wayland_GL_SetupWindow(this, width, height) < 0 ) {
			return(NULL);
		}
		current->flags |= SDL_OPENGL;
		this->UpdateRects = NULL;
		break;
#endif
	case WAYLAND_MODE_VULKAN:
		/* the window shows up when the application presents to it */
		current->flags |= SDL_VULKAN;
		this->UpdateRects = NULL;
		wl_surface_commit(wl->surface);
		break;
	}

	wl_display_flush(wl->display);
	return(current);
}

static int Wayland_ToggleFullScreen(_THIS, int on)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->surface == NULL ) {
		return(0);
	}
	wl->fullscreen = on ? 1 : 0;
	Wayland_ApplyWindowState(this, this->screen ? this->screen->flags : 0);
	wl_surface_commit(wl->surface);
	wl_display_flush(wl->display);
	return(1);
}

/* wl_shm buffers */

int Wayland_CreateShmFile(size_t size)
{
	const char *dir = SDL_getenv("XDG_RUNTIME_DIR");
	char *path;
	int fd;

	if ( dir == NULL ) {
		dir = "/tmp";
	}
	path = (char *)SDL_malloc(SDL_strlen(dir) + 32);
	if ( path == NULL ) {
		return -1;
	}
	SDL_snprintf(path, SDL_strlen(dir) + 32, "%s/sdl-shared-XXXXXX", dir);
	fd = mkstemp(path);
	if ( fd >= 0 ) {
		unlink(path);
		fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
		while ( ftruncate(fd, (off_t)size) < 0 ) {
			if ( errno != EINTR ) {
				close(fd);
				fd = -1;
				break;
			}
		}
	}
	SDL_free(path);
	return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
	struct Wayland_Buffer *buf = (struct Wayland_Buffer *)data;

	buf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static void Wayland_DestroyBuffer(struct Wayland_Buffer *buf)
{
	if ( buf->buffer ) {
		wl_buffer_destroy(buf->buffer);
	}
	if ( buf->data ) {
		munmap(buf->data, buf->size);
	}
	SDL_memset(buf, 0, sizeof(*buf));
}

static void Wayland_DestroyBuffers(_THIS)
{
	int i;

	for ( i = 0; i < WAYLAND_MAX_BUFFERS; i++ ) {
		Wayland_DestroyBuffer(&this->hidden->buffers[i]);
	}
	this->hidden->present_pending = 0;
}

static int Wayland_CreateBuffer(_THIS, struct Wayland_Buffer *buf, int w, int h)
{
	struct wl_shm_pool *pool;
	int fd;

	buf->w = w;
	buf->h = h;
	buf->stride = w * 4;
	buf->size = (size_t)buf->stride * h;
	fd = Wayland_CreateShmFile(buf->size);
	if ( fd < 0 ) {
		SDL_SetError("Couldn't create a shared memory file: %s", strerror(errno));
		return -1;
	}
	buf->data = mmap(NULL, buf->size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if ( buf->data == MAP_FAILED ) {
		buf->data = NULL;
		close(fd);
		SDL_SetError("Couldn't map shared memory");
		return -1;
	}
	pool = wl_shm_create_pool(this->hidden->shm, fd, (int32_t)buf->size);
	buf->buffer = wl_shm_pool_create_buffer(pool, 0, w, h, buf->stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
	buf->busy = 0;
	return 0;
}

/* After the mode changes on a window that is showing, the new surface is
   blank until the application draws a whole frame. A partial update before
   that (DOSBox-X draws its menu bar first) would show the window black
   around it, so it waits for the whole frame, or for a short while in case
   the application never draws one. */
#define WAYLAND_HOLD_MS 250

int Wayland_HoldingUpdates(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->hold && (Uint32)(SDL_GetTicks() - wl->hold_ticks) >= WAYLAND_HOLD_MS ) {
		wl->hold = 0;
	}
	return wl->hold;
}

/* copy the SDL surface to a free buffer and show it */
void Wayland_PresentShm(_THIS, int numrects, SDL_Rect *rects)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct Wayland_Buffer *buf = NULL;
	int w = wl->window_w, h = wl->window_h;
	int damage_all;
	int i;

	if ( wl->surface == NULL || !wl->configured || wl->content_mode != WAYLAND_MODE_SHM ||
	     wl->framebuffer == NULL || w <= 0 || h <= 0 ) {
		return;
	}

	if ( wl->hold ) {
		int whole = (numrects <= 0);
		for ( i = 0; i < numrects && !whole; i++ ) {
			if ( rects[i].x == 0 && rects[i].y == 0 && rects[i].w >= w && rects[i].h >= h ) {
				whole = 1;
			}
		}
		if ( whole ) {
			wl->hold = 0;
		} else if ( Wayland_HoldingUpdates(this) ) {
			wl->present_pending = 1;
			return;
		}
	}
	/* an update that was dropped is in the frame too, but not in these rects */
	damage_all = (numrects <= 0) || wl->present_pending;

	for ( i = 0; i < WAYLAND_MAX_BUFFERS && buf == NULL; i++ ) {
		if ( wl->buffers[i].buffer && !wl->buffers[i].busy &&
		     wl->buffers[i].w == w && wl->buffers[i].h == h ) {
			buf = &wl->buffers[i];
		}
	}
	for ( i = 0; i < WAYLAND_MAX_BUFFERS && buf == NULL; i++ ) {
		if ( !wl->buffers[i].busy ) {
			Wayland_DestroyBuffer(&wl->buffers[i]);
			if ( Wayland_CreateBuffer(this, &wl->buffers[i], w, h) < 0 ) {
				Wayland_DestroyBuffer(&wl->buffers[i]);
				return;
			}
			buf = &wl->buffers[i];
		}
	}
	if ( buf == NULL ) {
		/* the compositor still has them all, try again when one comes back */
		wl->present_pending = 1;
		return;
	}

	/* the buffer may hold an older frame, so all of it is updated */
	SDL_memcpy(buf->data, wl->framebuffer, buf->size);

	wl_surface_attach(wl->surface, buf->buffer, 0, 0);
	if ( damage_all ) {
		if ( wl->compositor_version >= 4 ) {
			wl_surface_damage_buffer(wl->surface, 0, 0, w, h);
		} else {
			wl_surface_damage(wl->surface, 0, 0, w, h);
		}
	}
	for ( i = 0; i < numrects && !damage_all; i++ ) {
		if ( wl->compositor_version >= 4 ) {
			wl_surface_damage_buffer(wl->surface, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		} else {
			wl_surface_damage(wl->surface, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		}
	}
	buf->busy = 1;
	wl->present_pending = 0;
	wl->shown = 1;
	wl_surface_commit(wl->surface);
	wl_display_flush(wl->display);
}

static void Wayland_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	Wayland_PresentShm(this, numrects, rects);
}

static int Wayland_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* the screen is never palettized, SDL handles 8 bpp with a shadow surface */
	return(1);
}

/* We don't actually allow hardware surfaces other than the main one */
static int Wayland_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void Wayland_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}
static int Wayland_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}
static void Wayland_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* window manager functions */

static void Wayland_SetCaption(_THIS, const char *title, const char *icon)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( title == NULL ) {
		return;
	}
#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor_frame ) {
		SDL_libdecor_frame_set_title(wl->libdecor_frame, title);
	} else
#endif
	if ( wl->xdg_toplevel ) {
		xdg_toplevel_set_title(wl->xdg_toplevel, title);
	}
	if ( wl->display ) {
		wl_display_flush(wl->display);
	}
}

static int Wayland_IconifyWindow(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor_frame ) {
		SDL_libdecor_frame_set_minimized(wl->libdecor_frame);
	} else
#endif
	if ( wl->xdg_toplevel ) {
		xdg_toplevel_set_minimized(wl->xdg_toplevel);
	} else {
		return(0);
	}
	wl_display_flush(wl->display);
	return(1);
}

/* Wayland clients can't grab the keyboard or mouse, but the pointer can be
   locked to the window. That is done in CheckMouseMode(), which SDL calls
   when the grab mode or the cursor visibility changes. */
static SDL_GrabMode Wayland_GrabInput(_THIS, SDL_GrabMode mode)
{
	if ( mode == SDL_GRAB_FULLSCREEN ) {
		mode = SDL_GRAB_ON;
	}
	return(mode);
}

#if SDL_VIDEO_DRIVER_X11
static int Wayland_GetWMInfo(_THIS, SDL_SysWMinfo *info)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( info->version.major <= SDL_MAJOR_VERSION ) {
		info->subsystem = SDL_SYSWM_WAYLAND;
		info->info.wl.display = wl->display;
		info->info.wl.surface = wl->surface;
		info->info.wl.shell_surface = wl->xdg_toplevel;
#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
		if ( wl->libdecor_frame ) {
			info->info.wl.shell_surface = SDL_libdecor_frame_get_xdg_toplevel(wl->libdecor_frame);
		}
#endif
		return(1);
	} else {
		SDL_SetError("Application not compiled with SDL %d.%d\n",
					SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
		return(-1);
	}
}
#endif

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
static void Wayland_VideoQuit(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->display == NULL ) {
		return;
	}

	Wayland_DestroyWindow(this);
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	Wayland_GL_Shutdown(this);
#endif
	Wayland_QuitMouse(this);
	Wayland_RemoveSeat(this);

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	if ( wl->libdecor ) {
		SDL_libdecor_unref(wl->libdecor);
		wl->libdecor = NULL;
	}
#endif
	if ( wl->xkb_context ) {
		xkb_context_unref(wl->xkb_context);
		wl->xkb_context = NULL;
	}
	if ( wl->output ) {
		wl_output_destroy(wl->output);
		wl->output = NULL;
	}
	if ( wl->pointer_constraints ) {
		zwp_pointer_constraints_v1_destroy(wl->pointer_constraints);
		wl->pointer_constraints = NULL;
	}
	if ( wl->relative_pointer_manager ) {
		zwp_relative_pointer_manager_v1_destroy(wl->relative_pointer_manager);
		wl->relative_pointer_manager = NULL;
	}
	if ( wl->decoration_manager ) {
		zxdg_decoration_manager_v1_destroy(wl->decoration_manager);
		wl->decoration_manager = NULL;
	}
	if ( wl->wm_base ) {
		xdg_wm_base_destroy(wl->wm_base);
		wl->wm_base = NULL;
	}
	if ( wl->shm ) {
		wl_shm_destroy(wl->shm);
		wl->shm = NULL;
	}
	if ( wl->compositor ) {
		wl_compositor_destroy(wl->compositor);
		wl->compositor = NULL;
	}
	if ( wl->registry ) {
		wl_registry_destroy(wl->registry);
		wl->registry = NULL;
	}
	wl_display_flush(wl->display);
	wl_display_disconnect(wl->display);
	wl->display = NULL;

	if ( wl->app_id ) {
		SDL_free(wl->app_id);
		wl->app_id = NULL;
	}
	if ( this->screen ) {
		this->screen->pixels = NULL;
	}
}
