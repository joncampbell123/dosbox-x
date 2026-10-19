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

/* DOSBox-X: Vulkan support for the X11 driver (VK_KHR_xlib_surface) */

#include "SDL_x11video.h"
#include "../SDL_vulkan_c.h"
#include "SDL_x11vulkan_c.h"

#define SDL_VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR 1000004000

typedef struct {
	int sType; /* VkStructureType */
	const void *pNext;
	SDL_VkFlags flags;
	Display *dpy;
	Window window;
} SDL_VkXlibSurfaceCreateInfoKHR;

typedef SDL_VkResult (SDL_VKAPI_CALL *SDL_PFN_vkCreateXlibSurfaceKHR)(VkInstance instance,
	const SDL_VkXlibSurfaceCreateInfoKHR *pCreateInfo, const void *pAllocator, VkSurfaceKHR *pSurface);

int X11_Vulkan_GetInstanceExtensions(_THIS, unsigned int *count, const char **names)
{
	static const char * const extensions[] = { "VK_KHR_surface", "VK_KHR_xlib_surface" };
	return SDL_Vulkan_CopyExtensions(extensions, SDL_arraysize(extensions), count, names);
}

int X11_Vulkan_CreateSurface(_THIS, VkInstance instance, VkSurfaceKHR *surface)
{
	SDL_VkXlibSurfaceCreateInfoKHR info;
	SDL_VkResult result;
	SDL_PFN_vkCreateXlibSurfaceKHR create = (SDL_PFN_vkCreateXlibSurfaceKHR)
		SDL_Vulkan_GetInstanceProc(instance, "vkCreateXlibSurfaceKHR");

	if ( create == NULL ) {
		SDL_SetError("vkCreateXlibSurfaceKHR not available, was the instance created with " "VK_KHR_xlib_surface?");
		return -1;
	}
	if ( ! SDL_Window ) {
		SDL_SetError("No window");
		return -1;
	}

	/* make sure the X server knows about the window before Vulkan uses it */
	XSync(SDL_Display, False);

	SDL_memset(&info, 0, sizeof(info));
	info.sType = SDL_VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	info.dpy = SDL_Display;
	info.window = SDL_Window;
	result = create(instance, &info, NULL, surface);
	if ( result != 0 ) {
		SDL_SetError("vkCreateXlibSurfaceKHR failed (VkResult %d)", result);
		return -1;
	}
	return 0;
}
