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

/* DOSBox-X: Vulkan support for the windib driver (VK_KHR_win32_surface) */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "SDL_lowvideo.h"
#include "../SDL_vulkan_c.h"
#include "SDL_winvulkan_c.h"

#if SDL_WIN32_VULKAN

#define SDL_VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR 1000009000

typedef struct {
	int sType; /* VkStructureType */
	const void *pNext;
	SDL_VkFlags flags;
	HINSTANCE hinstance;
	HWND hwnd;
} SDL_VkWin32SurfaceCreateInfoKHR;

typedef SDL_VkResult (SDL_VKAPI_CALL *SDL_PFN_vkCreateWin32SurfaceKHR)(VkInstance instance,
	const SDL_VkWin32SurfaceCreateInfoKHR *pCreateInfo, const void *pAllocator, VkSurfaceKHR *pSurface);

int WIN_Vulkan_GetInstanceExtensions(_THIS, unsigned int *count, const char **names)
{
	static const char * const extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
	return SDL_Vulkan_CopyExtensions(extensions, SDL_arraysize(extensions), count, names);
}

int WIN_Vulkan_CreateSurface(_THIS, VkInstance instance, VkSurfaceKHR *surface)
{
	SDL_VkWin32SurfaceCreateInfoKHR info;
	SDL_VkResult result;
	SDL_PFN_vkCreateWin32SurfaceKHR create = (SDL_PFN_vkCreateWin32SurfaceKHR)
		SDL_Vulkan_GetInstanceProc(instance, "vkCreateWin32SurfaceKHR");

	if ( create == NULL ) {
		SDL_SetError("vkCreateWin32SurfaceKHR not available, was the instance created with " "VK_KHR_win32_surface?");
		return -1;
	}
	if ( SDL_Window == NULL ) {
		SDL_SetError("No window");
		return -1;
	}

	SDL_memset(&info, 0, sizeof(info));
	info.sType = SDL_VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = SDL_Instance;
	info.hwnd = SDL_Window;
	result = create(instance, &info, NULL, surface);
	if ( result != 0 ) {
		SDL_SetError("vkCreateWin32SurfaceKHR failed (VkResult %d)", result);
		return -1;
	}
	return 0;
}

#endif /* SDL_WIN32_VULKAN */
