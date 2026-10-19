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

/* DOSBox-X: Vulkan support for the Wayland driver (VK_KHR_wayland_surface) */

#include "SDL_waylandvideo.h"
#include "../SDL_vulkan_c.h"
#include "SDL_waylandvulkan_c.h"

#define SDL_VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR 1000006000

typedef struct {
	int sType; /* VkStructureType */
	const void *pNext;
	SDL_VkFlags flags;
	struct wl_display *display;
	struct wl_surface *surface;
} SDL_VkWaylandSurfaceCreateInfoKHR;

typedef SDL_VkResult (SDL_VKAPI_CALL *SDL_PFN_vkCreateWaylandSurfaceKHR)(VkInstance instance,
	const SDL_VkWaylandSurfaceCreateInfoKHR *pCreateInfo, const void *pAllocator, VkSurfaceKHR *pSurface);

int Wayland_Vulkan_GetInstanceExtensions(_THIS, unsigned int *count, const char **names)
{
	static const char * const extensions[] = { "VK_KHR_surface", "VK_KHR_wayland_surface" };
	return SDL_Vulkan_CopyExtensions(extensions, SDL_arraysize(extensions), count, names);
}

int Wayland_Vulkan_CreateSurface(_THIS, VkInstance instance, VkSurfaceKHR *surface)
{
	SDL_VkWaylandSurfaceCreateInfoKHR info;
	SDL_VkResult result;
	SDL_PFN_vkCreateWaylandSurfaceKHR create = (SDL_PFN_vkCreateWaylandSurfaceKHR)
		SDL_Vulkan_GetInstanceProc(instance, "vkCreateWaylandSurfaceKHR");

	if ( create == NULL ) {
		SDL_SetError("vkCreateWaylandSurfaceKHR not available, was the instance created with " "VK_KHR_wayland_surface?");
		return -1;
	}
	if ( this->hidden->surface == NULL ) {
		SDL_SetError("No window");
		return -1;
	}

	SDL_memset(&info, 0, sizeof(info));
	info.sType = SDL_VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
	info.display = this->hidden->display;
	info.surface = this->hidden->surface;
	result = create(instance, &info, NULL, surface);
	if ( result != 0 ) {
		SDL_SetError("vkCreateWaylandSurfaceKHR failed (VkResult %d)", result);
		return -1;
	}
	return 0;
}
