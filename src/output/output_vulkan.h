#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_VULKAN_H
#define DOSBOX_OUTPUT_VULKAN_H

#if C_VULKAN_OUTPUT
#include "SDL.h"

// output API
void OUTPUT_VULKAN_Select();
Bitu OUTPUT_VULKAN_GetBestMode(Bitu flags);
Bitu OUTPUT_VULKAN_SetSize();
bool OUTPUT_VULKAN_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_VULKAN_EndUpdate(const uint16_t *changedLines);
void OUTPUT_VULKAN_Shutdown();

// The Vulkan window surface and swapchain must be released before the SDL
// window they were created for is destroyed (e.g. when the mapper or the
// configuration GUI switch the window back to a plain SDL surface window).
void OUTPUT_VULKAN_ReleaseWindow();

// sdl.surface is a window-sized overlay (SDL drawn menus) while Vulkan output
// is active. Call this after drawing to it, where surface output would call
// SDL_UpdateWindowSurfaceRects(). Returns false if sdl.surface is not the
// Vulkan overlay, in which case the caller should update the window surface.
bool OUTPUT_VULKAN_UpdateOverlay();
#endif

#endif /*DOSBOX_OUTPUT_VULKAN_H*/
