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

#if SDL_VIDEO_DRIVER_WAYLAND_EGL

#include <wayland-egl.h>

#include "SDL_loadso.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandgl_c.h"

static int Wayland_GL_Init(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;
	EGLint config_attribs[32];
	EGLint num_configs = 0;
	EGLint major, minor;
	int i = 0;

	if ( gl->initialized ) {
		return 0;
	}

	gl->display = eglGetDisplay((EGLNativeDisplayType)this->hidden->display);
	if ( gl->display == EGL_NO_DISPLAY || !eglInitialize(gl->display, &major, &minor) ) {
		SDL_SetError("Couldn't initialize EGL");
		gl->display = EGL_NO_DISPLAY;
		return -1;
	}
	/* desktop OpenGL, with the old fixed function pipeline */
	if ( !eglBindAPI(EGL_OPENGL_API) ) {
		SDL_SetError("EGL has no desktop OpenGL");
		eglTerminate(gl->display);
		gl->display = EGL_NO_DISPLAY;
		return -1;
	}

	config_attribs[i++] = EGL_SURFACE_TYPE;    config_attribs[i++] = EGL_WINDOW_BIT;
	config_attribs[i++] = EGL_RENDERABLE_TYPE; config_attribs[i++] = EGL_OPENGL_BIT;
	config_attribs[i++] = EGL_RED_SIZE;        config_attribs[i++] = this->gl_config.red_size;
	config_attribs[i++] = EGL_GREEN_SIZE;      config_attribs[i++] = this->gl_config.green_size;
	config_attribs[i++] = EGL_BLUE_SIZE;       config_attribs[i++] = this->gl_config.blue_size;
	if ( this->gl_config.alpha_size ) {
		config_attribs[i++] = EGL_ALPHA_SIZE;  config_attribs[i++] = this->gl_config.alpha_size;
	}
	if ( this->gl_config.depth_size ) {
		config_attribs[i++] = EGL_DEPTH_SIZE;  config_attribs[i++] = this->gl_config.depth_size;
	}
	if ( this->gl_config.stencil_size ) {
		config_attribs[i++] = EGL_STENCIL_SIZE; config_attribs[i++] = this->gl_config.stencil_size;
	}
	if ( this->gl_config.multisamplebuffers ) {
		config_attribs[i++] = EGL_SAMPLE_BUFFERS; config_attribs[i++] = this->gl_config.multisamplebuffers;
		config_attribs[i++] = EGL_SAMPLES;        config_attribs[i++] = this->gl_config.multisamplesamples;
	}
	config_attribs[i++] = EGL_NONE;

	if ( !eglChooseConfig(gl->display, config_attribs, &gl->config, 1, &num_configs) || num_configs < 1 ) {
		SDL_SetError("Couldn't find a matching EGL configuration");
		eglTerminate(gl->display);
		gl->display = EGL_NO_DISPLAY;
		return -1;
	}

	gl->context = eglCreateContext(gl->display, gl->config, EGL_NO_CONTEXT, NULL);
	if ( gl->context == EGL_NO_CONTEXT ) {
		SDL_SetError("Couldn't create an EGL context");
		eglTerminate(gl->display);
		gl->display = EGL_NO_DISPLAY;
		return -1;
	}

	gl->initialized = 1;
	return 0;
}

int Wayland_GL_SetupWindow(_THIS, int w, int h)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( Wayland_GL_Init(this) < 0 ) {
		return -1;
	}

	if ( wl->egl_window ) {
		wl_egl_window_resize(wl->egl_window, w, h, 0, 0);
		return 0;
	}

	wl->egl_window = wl_egl_window_create(wl->surface, w, h);
	if ( wl->egl_window == NULL ) {
		SDL_SetError("Couldn't create a Wayland EGL window");
		return -1;
	}
	gl->surface = eglCreateWindowSurface(gl->display, gl->config, (EGLNativeWindowType)wl->egl_window, NULL);
	if ( gl->surface == EGL_NO_SURFACE ) {
		wl_egl_window_destroy(wl->egl_window);
		wl->egl_window = NULL;
		SDL_SetError("Couldn't create an EGL window surface");
		return -1;
	}
	if ( Wayland_GL_MakeCurrent(this) < 0 ) {
		return -1;
	}
	/* EGL waits for frame callbacks by default, which never come while the
	   window is hidden. Only wait for vsync if asked to. */
	eglSwapInterval(gl->display, this->gl_config.swap_control > 0 ? this->gl_config.swap_control : 0);
	return 0;
}

void Wayland_GL_DestroyWindow(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( gl->initialized ) {
		eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if ( gl->surface != EGL_NO_SURFACE ) {
			eglDestroySurface(gl->display, gl->surface);
			gl->surface = EGL_NO_SURFACE;
		}
	}
	if ( wl->egl_window ) {
		wl_egl_window_destroy(wl->egl_window);
		wl->egl_window = NULL;
	}
}

void Wayland_GL_Shutdown(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	Wayland_GL_DestroyWindow(this);
	if ( gl->initialized ) {
		eglDestroyContext(gl->display, gl->context);
		eglTerminate(gl->display);
		gl->context = EGL_NO_CONTEXT;
		gl->display = EGL_NO_DISPLAY;
		gl->initialized = 0;
	}
	if ( gl->libgl ) {
		SDL_UnloadObject(gl->libgl);
		gl->libgl = NULL;
	}
}

int Wayland_GL_LoadLibrary(_THIS, const char *path)
{
	/* EGL is linked in */
	this->gl_config.driver_loaded = 1;
	return 0;
}

void *Wayland_GL_GetProcAddress(_THIS, const char *proc)
{
	struct SDL_PrivateGLData *gl = this->gl_data;
	void *func = (void *)eglGetProcAddress(proc);

	if ( func == NULL ) {
		if ( gl->libgl == NULL ) {
			gl->libgl = SDL_LoadObject("libOpenGL.so.0");
			if ( gl->libgl == NULL ) {
				gl->libgl = SDL_LoadObject("libGL.so.1");
			}
		}
		if ( gl->libgl ) {
			func = SDL_LoadFunction(gl->libgl, proc);
		}
	}
	return func;
}

int Wayland_GL_GetAttribute(_THIS, SDL_GLattr attrib, int* value)
{
	struct SDL_PrivateGLData *gl = this->gl_data;
	EGLint egl_attrib;
	EGLint egl_value = 0;

	if ( !gl->initialized ) {
		SDL_SetError("No OpenGL context");
		return -1;
	}
	switch ( attrib ) {
	    case SDL_GL_RED_SIZE:		egl_attrib = EGL_RED_SIZE; break;
	    case SDL_GL_GREEN_SIZE:		egl_attrib = EGL_GREEN_SIZE; break;
	    case SDL_GL_BLUE_SIZE:		egl_attrib = EGL_BLUE_SIZE; break;
	    case SDL_GL_ALPHA_SIZE:		egl_attrib = EGL_ALPHA_SIZE; break;
	    case SDL_GL_BUFFER_SIZE:		egl_attrib = EGL_BUFFER_SIZE; break;
	    case SDL_GL_DEPTH_SIZE:		egl_attrib = EGL_DEPTH_SIZE; break;
	    case SDL_GL_STENCIL_SIZE:		egl_attrib = EGL_STENCIL_SIZE; break;
	    case SDL_GL_MULTISAMPLEBUFFERS:	egl_attrib = EGL_SAMPLE_BUFFERS; break;
	    case SDL_GL_MULTISAMPLESAMPLES:	egl_attrib = EGL_SAMPLES; break;
	    case SDL_GL_DOUBLEBUFFER:		*value = 1; return 0;
	    case SDL_GL_ACCELERATED_VISUAL:
		if ( !eglGetConfigAttrib(gl->display, gl->config, EGL_CONFIG_CAVEAT, &egl_value) ) {
			return -1;
		}
		*value = (egl_value != EGL_SLOW_CONFIG);
		return 0;
	    case SDL_GL_SWAP_CONTROL:
		*value = this->gl_config.swap_control > 0 ? this->gl_config.swap_control : 0;
		return 0;
	    default:
		/* no accumulation buffers or stereo with EGL */
		*value = 0;
		return 0;
	}
	if ( !eglGetConfigAttrib(gl->display, gl->config, egl_attrib, &egl_value) ) {
		SDL_SetError("eglGetConfigAttrib failed");
		return -1;
	}
	*value = egl_value;
	return 0;
}

int Wayland_GL_MakeCurrent(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( !gl->initialized || gl->surface == EGL_NO_SURFACE ) {
		SDL_SetError("No OpenGL window");
		return -1;
	}
	if ( !eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context) ) {
		SDL_SetError("eglMakeCurrent failed");
		return -1;
	}
	return 0;
}

/* DOSBox-X: more contexts, sharing with the video mode's. The EGL context outlives the
   window surfaces, so the shared contexts always share with the current one. */
SDL_GLContext Wayland_GL_CreateSharedContext(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;
	EGLContext context;

	if ( !gl->initialized ) {
		SDL_SetError("OpenGL video mode has not been set");
		return NULL;
	}
	context = eglCreateContext(gl->display, gl->config, gl->context, NULL);
	if ( context == EGL_NO_CONTEXT ) {
		SDL_SetError("Couldn't create a shared EGL context");
		return NULL;
	}
	return (SDL_GLContext)context;
}

void Wayland_GL_DeleteContext(_THIS, SDL_GLContext context)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( !gl->initialized || (EGLContext)context == gl->context ) {
		return;
	}
	if ( eglGetCurrentContext() == (EGLContext)context ) {
		eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
	eglDestroyContext(gl->display, (EGLContext)context);
}

int Wayland_GL_MakeContextCurrent(_THIS, SDL_GLContext context)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( !gl->initialized || gl->surface == EGL_NO_SURFACE ) {
		SDL_SetError("No OpenGL window");
		return -1;
	}
	if ( !eglMakeCurrent(gl->display, gl->surface, gl->surface, (EGLContext)context) ) {
		SDL_SetError("eglMakeCurrent failed");
		return -1;
	}
	return 0;
}

SDL_GLContext Wayland_GL_GetVideoModeContext(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	return gl->initialized ? (SDL_GLContext)gl->context : NULL;
}

void Wayland_GL_SwapBuffers(_THIS)
{
	struct SDL_PrivateGLData *gl = this->gl_data;

	if ( gl->initialized && gl->surface != EGL_NO_SURFACE ) {
		eglSwapBuffers(gl->display, gl->surface);
	}
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_EGL */
