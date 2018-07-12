#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"
#include "vga.h"

#include <algorithm> // std::transform

using namespace std;

#if defined(C_SDL2)
Bitu OUTPUT_SURFACE_SetSize()
{
    Bitu bpp = 0;
    Bitu retFlags = 0;
    (void)bpp;

retry:
    sdl.clip.w = sdl.draw.width;
    sdl.clip.h = sdl.draw.height;
    if (GFX_IsFullscreen()) {
        if (sdl.desktop.full.fixed) {
            sdl.clip.x = (Sint16)((sdl.desktop.full.width - sdl.draw.width) / 2);
            sdl.clip.y = (Sint16)((sdl.desktop.full.height - sdl.draw.height) / 2);
            sdl.window = GFX_SetSDLWindowMode(sdl.desktop.full.width, sdl.desktop.full.height, SCREEN_SURFACE);
            if (sdl.window == NULL)
                E_Exit("Could not set fullscreen video mode %ix%i-%i: %s", sdl.desktop.full.width, sdl.desktop.full.height, sdl.desktop.bpp, SDL_GetError());
        }
        else {
            sdl.clip.x = 0;
            sdl.clip.y = 0;
            sdl.window = GFX_SetSDLWindowMode(sdl.draw.width, sdl.draw.height, SCREEN_SURFACE);
            if (sdl.window == NULL)
                LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
            SDL_SetWindowFullscreen(sdl.window, 0);
            GFX_CaptureMouse();
            goto retry;
        }
    }
    else {
        int menuheight = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
        if (mainMenu.isVisible()) menuheight = mainMenu.menuBox.h;
#endif

        sdl.clip.x = sdl.overscan_width;
        sdl.clip.y = sdl.overscan_width + menuheight;
        sdl.window = GFX_SetSDLWindowMode(sdl.draw.width + 2 * sdl.overscan_width, sdl.draw.height + menuheight + 2 * sdl.overscan_width, SCREEN_SURFACE);
        if (sdl.window == NULL)
            E_Exit("Could not set windowed video mode %ix%i: %s", (int)sdl.draw.width, (int)sdl.draw.height, SDL_GetError());
    }
    sdl.surface = SDL_GetWindowSurface(sdl.window);
    if (sdl.surface == NULL)
        E_Exit("Could not retrieve window surface: %s", SDL_GetError());
    switch (sdl.surface->format->BitsPerPixel) {
    case 8:
        retFlags = GFX_CAN_8;
        break;
    case 15:
        retFlags = GFX_CAN_15;
        break;
    case 16:
        retFlags = GFX_CAN_16;
        break;
    case 32:
        retFlags = GFX_CAN_32;
        break;
    }

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    /* Fix a glitch with aspect=true occuring when
    changing between modes with different dimensions */
    SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = sdl.surface->w;
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif

    return retFlags;
}
#endif /*defined(C_SDL2)*/
