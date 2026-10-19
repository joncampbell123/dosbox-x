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

/* DOSBox-X: internal Vulkan support shared by the video drivers.
   Only the few Vulkan definitions that are needed are declared here, so
   that SDL can be built without the Vulkan headers. */

#ifndef _SDL_vulkan_c_h
#define _SDL_vulkan_c_h

#include "SDL_vulkan.h"

#if defined(_WIN32)
#define SDL_VKAPI_CALL __stdcall
#else
#define SDL_VKAPI_CALL
#endif

typedef int SDL_VkResult; /* VkResult, 0 is VK_SUCCESS */
typedef Uint32 SDL_VkFlags;
typedef void (SDL_VKAPI_CALL *SDL_PFN_vkVoidFunction)(void);

/* look up an instance level function, NULL if the loader is not loaded */
extern SDL_PFN_vkVoidFunction SDL_Vulkan_GetInstanceProc(VkInstance instance, const char *name);

/* SDL_Vulkan_GetInstanceExtensions() semantics, for the video drivers */
extern int SDL_Vulkan_CopyExtensions(const char * const *extensions, unsigned int count,
                                     unsigned int *pCount, const char **pNames);

/* to be called by the video drivers right before the window is destroyed */
extern void SDL_Vulkan_NotifyWindowDestroy(void);

#endif /* _SDL_vulkan_c_h */
