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

    SDL_SetWindowMinimumSize(sdl.window, 1, 1); /* NTS: 0 x 0 is not valid */

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
            GFX_CaptureMouse();
        }
    }
    else {
        int width = sdl.draw.width;
        int height = sdl.draw.height;
        int menuheight = 0;

        sdl.clip.x = 0; sdl.clip.y = 0;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
        /* scale the menu bar if the window is large enough */
        {
            Bitu consider_height = menu.maxwindow ? currentWindowHeight : height;
            Bitu consider_width = menu.maxwindow ? currentWindowWidth : width;
            Bitu final_height = max(max(consider_height, userResizeWindowHeight), (Bitu)(sdl.clip.y + sdl.clip.h));
            Bitu final_width = max(max(consider_width, userResizeWindowWidth), (Bitu)(sdl.clip.x + sdl.clip.w));
            Bitu scale = 1;

            while ((final_width / scale) >= (640 * 2) && (final_height / scale) >= (400 * 2))
                scale++;

            LOG_MSG("menuScale=%lu", (unsigned long)scale);
            mainMenu.setScale(scale);
        }

        if (mainMenu.isVisible()) menuheight = mainMenu.menuBox.h;
#endif

        /* menu size and consideration of width and height */
        Bitu consider_height = height + (unsigned int)menuheight + (sdl.overscan_width * 2);
        Bitu consider_width = width + (sdl.overscan_width * 2);

        if (menu.maxwindow) {
            if (consider_height < currentWindowHeight)
                consider_height = currentWindowHeight;
            if (consider_width < currentWindowWidth)
                consider_width = currentWindowWidth;
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
        if (mainMenu.isVisible())
        {
            extern unsigned int min_sdldraw_menu_width;
            extern unsigned int min_sdldraw_menu_height;
            /* enforce a minimum 500x300 surface size.
             * the menus are useless below 500x300 */
            if (consider_width < (min_sdldraw_menu_width + (sdl.overscan_width * 2)))
                consider_width = (min_sdldraw_menu_width + (sdl.overscan_width * 2));
            if (consider_height < (min_sdldraw_menu_height + (sdl.overscan_width * 2) + (unsigned int)menuheight))
                consider_height = (min_sdldraw_menu_height + (sdl.overscan_width * 2) + (unsigned int)menuheight);
        }
#endif

        /* decide where the rectangle on the screen goes */
        int final_width,final_height;

#if C_XBRZ
        /* scale to fit the window.
         * fit by aspect ratio if asked to do so. */
        if (sdl_xbrz.enable)
        {
            final_height = (int)max(consider_height, userResizeWindowHeight) - (int)menuheight - ((int)sdl.overscan_width * 2);
            final_width = (int)max(consider_width, userResizeWindowWidth) - ((int)sdl.overscan_width * 2);

            sdl.clip.x = sdl.clip.y = 0;
            sdl.clip.w = final_width;
            sdl.clip.h = final_height;
            if (render.aspect) aspectCorrectFitClip(sdl.clip.w, sdl.clip.h, sdl.clip.x, sdl.clip.y, final_width, final_height);
        }
        else
#endif 
        /* center the screen in the window */
        {

            final_height = (int)max(max(consider_height, userResizeWindowHeight), (Bitu)(sdl.clip.y + sdl.clip.h)) - (int)menuheight - ((int)sdl.overscan_width * 2);
            final_width = (int)max(max(consider_width, userResizeWindowWidth), (Bitu)(sdl.clip.x + sdl.clip.w)) - ((int)sdl.overscan_width * 2);
            int ax = (final_width - (sdl.clip.x + sdl.clip.w)) / 2;
            int ay = (final_height - (sdl.clip.y + sdl.clip.h)) / 2;
            if (ax < 0) ax = 0;
            if (ay < 0) ay = 0;
            sdl.clip.x += ax + (int)sdl.overscan_width;
            sdl.clip.y += ay + (int)sdl.overscan_width;
            // sdl.clip.w = currentWindowWidth - sdl.clip.x;
            // sdl.clip.h = currentWindowHeight - sdl.clip.y;
        }

        {
            final_width += (int)sdl.overscan_width * 2;
            final_height += (int)menuheight + (int)sdl.overscan_width * 2;
            sdl.clip.y += (int)menuheight;

            LOG_MSG("surface consider=%ux%u final=%ux%u",
                (unsigned int)consider_width,
                (unsigned int)consider_height,
                (unsigned int)final_width,
                (unsigned int)final_height);
        }

        sdl.window = GFX_SetSDLWindowMode(final_width, final_height, SCREEN_SURFACE);
        if (sdl.window == NULL)
            E_Exit("Could not set windowed video mode %ix%i: %s", (int)sdl.draw.width, (int)sdl.draw.height, SDL_GetError());

        sdl.surface = SDL_GetWindowSurface(sdl.window);
        if (sdl.surface->w < (sdl.clip.x+sdl.clip.w) ||
            sdl.surface->h < (sdl.clip.y+sdl.clip.h)) {
            /* the window surface must not be smaller than the size we want!
             * This is a way to prevent that! */
            SDL_SetWindowMinimumSize(sdl.window, sdl.clip.x+sdl.clip.w, sdl.clip.y+sdl.clip.h);
            sdl.window = GFX_SetSDLWindowMode(sdl.clip.x+sdl.clip.w, sdl.clip.y+sdl.clip.h, SCREEN_SURFACE);
        }
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

#if C_XBRZ
    if (sdl_xbrz.enable)
    {
        bool old_scale_on = sdl_xbrz.scale_on;
        xBRZ_SetScaleParameters(sdl.draw.width, sdl.draw.height, sdl.clip.w, sdl.clip.h);
        if (sdl_xbrz.scale_on != old_scale_on) {
            // when we are scaling, we ask render code not to do any aspect correction
            // when we are not scaling, render code is allowed to do aspect correction at will
            // due to this, at each scale mode change we need to schedule resize again because window size could change
            PIC_AddEvent(VGA_SetupDrawing, 50); // schedule another resize here, render has already been initialized at this point and we have just changed its option
        }
    }
#endif

    /* WARNING: If the user is resizing our window to smaller than what we want, SDL2 will give us a
     *          window surface according to the smaller size, and then we crash! */
    assert(sdl.surface->w >= (sdl.clip.x+sdl.clip.w));
    assert(sdl.surface->h >= (sdl.clip.y+sdl.clip.h));

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    /* Fix a glitch with aspect=true occuring when
    changing between modes with different dimensions */
    SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = sdl.surface->w;
    mainMenu.screenHeight = sdl.surface->h;
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif

    return retFlags;
}
#endif /*defined(C_SDL2)*/
