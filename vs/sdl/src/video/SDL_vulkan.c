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

/* DOSBox-X: Vulkan support, modeled after the SDL 2.0 API */

#include "SDL_video.h"
#include "SDL_loadso.h"
#include "SDL_sysvideo.h"
#include "SDL_vulkan_c.h"

#if defined(_WIN32)
#define DEFAULT_VULKAN_LIBRARY "vulkan-1.dll"
#elif defined(__APPLE__)
#define DEFAULT_VULKAN_LIBRARY "libvulkan.1.dylib"
#else
#define DEFAULT_VULKAN_LIBRARY "libvulkan.so.1"
#endif

typedef SDL_PFN_vkVoidFunction (SDL_VKAPI_CALL *SDL_PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *name);

/* kept outside of the video device, so that it survives SDL_QuitSubSystem(SDL_INIT_VIDEO) */
static void *vulkan_library = NULL;
static int vulkan_refcount = 0;
static SDL_PFN_vkGetInstanceProcAddr vulkan_getinstanceprocaddr = NULL;

static SDL_Vulkan_WindowDestroyCallback window_destroy_callback = NULL;
static void *window_destroy_userdata = NULL;

int SDL_Vulkan_LoadLibrary(const char *path)
{
	if ( vulkan_library ) {
		vulkan_refcount++;
		return 0;
	}

	if ( path == NULL ) {
		path = SDL_getenv("SDL_VULKAN_LIBRARY");
	}
	if ( path == NULL ) {
		path = DEFAULT_VULKAN_LIBRARY;
	}

	vulkan_library = SDL_LoadObject(path);
	if ( vulkan_library == NULL ) {
		return -1; /* SDL_LoadObject() set the error */
	}

	vulkan_getinstanceprocaddr = (SDL_PFN_vkGetInstanceProcAddr)
		SDL_LoadFunction(vulkan_library, "vkGetInstanceProcAddr");
	if ( vulkan_getinstanceprocaddr == NULL ) {
		SDL_UnloadObject(vulkan_library);
		vulkan_library = NULL;
		SDL_SetError("vkGetInstanceProcAddr not found in %s", path);
		return -1;
	}

	vulkan_refcount = 1;
	return 0;
}

void *SDL_Vulkan_GetVkGetInstanceProcAddr(void)
{
	if ( vulkan_library == NULL ) {
		SDL_SetError("No Vulkan loader has been loaded");
		return NULL;
	}
	return (void *)vulkan_getinstanceprocaddr;
}

void SDL_Vulkan_UnloadLibrary(void)
{
	if ( vulkan_library == NULL ) {
		return;
	}
	if ( --vulkan_refcount > 0 ) {
		return;
	}
	SDL_UnloadObject(vulkan_library);
	vulkan_library = NULL;
	vulkan_getinstanceprocaddr = NULL;
}

SDL_PFN_vkVoidFunction SDL_Vulkan_GetInstanceProc(VkInstance instance, const char *name)
{
	if ( vulkan_getinstanceprocaddr == NULL ) {
		return NULL;
	}
	return vulkan_getinstanceprocaddr(instance, name);
}

int SDL_Vulkan_CopyExtensions(const char * const *extensions, unsigned int count,
                              unsigned int *pCount, const char **pNames)
{
	unsigned int i;

	if ( pNames == NULL ) {
		*pCount = count;
		return 0;
	}
	if ( *pCount < count ) {
		*pCount = 0;
		SDL_SetError("Output array for SDL_Vulkan_GetInstanceExtensions() is too small");
		return -1;
	}
	for ( i = 0; i < count; i++ ) {
		pNames[i] = extensions[i];
	}
	*pCount = count;
	return 0;
}

SDL_bool SDL_Vulkan_GetInstanceExtensions(unsigned int *pCount, const char **pNames)
{
	SDL_VideoDevice *this = current_video;

	if ( pCount == NULL ) {
		SDL_SetError("Parameter 'pCount' is invalid");
		return SDL_FALSE;
	}
	if ( this == NULL ) {
		SDL_SetError("Video subsystem has not been initialized");
		return SDL_FALSE;
	}
	if ( this->Vulkan_GetInstanceExtensions == NULL ) {
		SDL_SetError("Vulkan is not supported by the %s video driver", this->name);
		return SDL_FALSE;
	}
	return this->Vulkan_GetInstanceExtensions(this, pCount, pNames) == 0 ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_Vulkan_CreateSurface(SDL_vulkanInstance instance, SDL_vulkanSurface *surface)
{
	SDL_VideoDevice *this = current_video;

	if ( surface == NULL ) {
		SDL_SetError("Parameter 'surface' is invalid");
		return SDL_FALSE;
	}
	if ( vulkan_library == NULL ) {
		SDL_SetError("No Vulkan loader has been loaded");
		return SDL_FALSE;
	}
	if ( this == NULL || this->Vulkan_CreateSurface == NULL ) {
		SDL_SetError("Vulkan is not supported by the video driver");
		return SDL_FALSE;
	}
	if ( SDL_VideoSurface == NULL || !(SDL_VideoSurface->flags & SDL_VULKAN) ) {
		SDL_SetError("The video mode was not set with SDL_VULKAN");
		return SDL_FALSE;
	}
	return this->Vulkan_CreateSurface(this, instance, surface) == 0 ? SDL_TRUE : SDL_FALSE;
}

void SDL_Vulkan_GetDrawableSize(int *w, int *h)
{
	SDL_VideoDevice *this = current_video;
	int dw = 0, dh = 0;

	if ( this != NULL && SDL_VideoSurface != NULL ) {
		dw = SDL_VideoSurface->w;
		dh = SDL_VideoSurface->h;
	}
	if ( w ) *w = dw;
	if ( h ) *h = dh;
}

void SDL_Vulkan_SetWindowDestroyCallback(SDL_Vulkan_WindowDestroyCallback callback, void *userdata)
{
	window_destroy_callback = callback;
	window_destroy_userdata = userdata;
}

void SDL_Vulkan_NotifyWindowDestroy(void)
{
	if ( window_destroy_callback ) {
		window_destroy_callback(window_destroy_userdata);
	}
}
