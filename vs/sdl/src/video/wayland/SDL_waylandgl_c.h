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

/* DOSBox-X: OpenGL for the Wayland driver, through EGL */

#include "SDL_waylandvideo.h"

#if SDL_VIDEO_DRIVER_WAYLAND_EGL
#include <EGL/egl.h>
#endif

struct SDL_PrivateGLData {
#if SDL_VIDEO_DRIVER_WAYLAND_EGL
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	void *libgl;		/* for GL functions eglGetProcAddress() doesn't know */
#endif
	int initialized;
};

#if SDL_VIDEO_DRIVER_WAYLAND_EGL
extern int Wayland_GL_LoadLibrary(_THIS, const char *path);
extern void *Wayland_GL_GetProcAddress(_THIS, const char *proc);
extern int Wayland_GL_GetAttribute(_THIS, SDL_GLattr attrib, int* value);
extern int Wayland_GL_MakeCurrent(_THIS);
extern SDL_GLContext Wayland_GL_CreateSharedContext(_THIS);
extern void Wayland_GL_DeleteContext(_THIS, SDL_GLContext context);
extern int Wayland_GL_MakeContextCurrent(_THIS, SDL_GLContext context);
extern SDL_GLContext Wayland_GL_GetVideoModeContext(_THIS);
extern void Wayland_GL_SwapBuffers(_THIS);

/* make (or resize) the EGL surface for the window */
extern int Wayland_GL_SetupWindow(_THIS, int w, int h);
extern void Wayland_GL_DestroyWindow(_THIS);
extern void Wayland_GL_Shutdown(_THIS);
#endif
