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

/** @file SDL_vulkan.h
 *  DOSBox-X specific: Vulkan support for SDL 1.2, modeled after the SDL 2.0
 *  Vulkan API. There is only one window in SDL 1.2, so the functions do not
 *  take a window parameter.
 *
 *  Usage:
 *   - SDL_Vulkan_LoadLibrary(NULL) to load the Vulkan loader
 *   - SDL_SetVideoMode() with SDL_VULKAN to get a window that SDL will not
 *     draw to (the returned surface has no pixels, like SDL_OPENGL)
 *   - create a VkInstance with the extensions from
 *     SDL_Vulkan_GetInstanceExtensions(), then SDL_Vulkan_CreateSurface()
 *
 *  SDL_SetVideoMode() may have to destroy and recreate the window, which
 *  invalidates the Vulkan surface. Use SDL_Vulkan_SetWindowDestroyCallback()
 *  to destroy the swapchain and surface before that happens.
 *
 *  Supported by the x11 and windib video drivers.
 */

#ifndef _SDL_vulkan_h
#define _SDL_vulkan_h

#include "SDL_stdinc.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Avoid including vulkan.h, don't define VkInstance if it's already included */
#ifdef VULKAN_H_
#define NO_SDL_VULKAN_TYPEDEFS
#endif
#ifndef NO_SDL_VULKAN_TYPEDEFS
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef Uint64 object;
#endif

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)

#endif /* !NO_SDL_VULKAN_TYPEDEFS */

typedef VkInstance SDL_vulkanInstance;
typedef VkSurfaceKHR SDL_vulkanSurface; /* for compatibility with Tizen */

/**
 * Load the Vulkan loader. If path is NULL, the SDL_VULKAN_LIBRARY
 * environment variable is used, or the platform's default name.
 * Calls are reference counted.
 * Returns 0 on success, -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_Vulkan_LoadLibrary(const char *path);

/** Get the address of vkGetInstanceProcAddr(), or NULL if not loaded */
extern DECLSPEC void * SDLCALL SDL_Vulkan_GetVkGetInstanceProcAddr(void);

/** Unload the Vulkan loader loaded by SDL_Vulkan_LoadLibrary() */
extern DECLSPEC void SDLCALL SDL_Vulkan_UnloadLibrary(void);

/**
 * Get the names of the instance extensions needed to create a surface
 * with SDL_Vulkan_CreateSurface(). If pNames is NULL, *pCount is set to the
 * number of extensions. Otherwise up to *pCount names are stored and
 * *pCount is set to the number stored.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_Vulkan_GetInstanceExtensions(unsigned int *pCount, const char **pNames);

/**
 * Create a Vulkan surface for the window. A video mode must have been set
 * with SDL_VULKAN, and the instance created with the extensions from
 * SDL_Vulkan_GetInstanceExtensions().
 */
extern DECLSPEC SDL_bool SDLCALL SDL_Vulkan_CreateSurface(SDL_vulkanInstance instance, SDL_vulkanSurface *surface);

/** Get the size of the window's drawable area in pixels */
extern DECLSPEC void SDLCALL SDL_Vulkan_GetDrawableSize(int *w, int *h);

/**
 * DOSBox-X: Set a function to be called right before SDL destroys the window
 * (in SDL_SetVideoMode(), or when the video subsystem is shut down). Any
 * Vulkan swapchain and surface for the window must be destroyed by then.
 */
typedef void (SDLCALL *SDL_Vulkan_WindowDestroyCallback)(void *userdata);
extern DECLSPEC void SDLCALL SDL_Vulkan_SetWindowDestroyCallback(SDL_Vulkan_WindowDestroyCallback callback, void *userdata);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_vulkan_h */
