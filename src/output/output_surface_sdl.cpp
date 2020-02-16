#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"
#include "vga.h"

#include <algorithm> // std::transform

using namespace std;

bool setSizeButNotResize();

#if !defined(C_SDL2)
Bitu OUTPUT_SURFACE_GetBestMode(Bitu flags)
{
    Bitu testbpp, gotbpp;

    flags &= ~GFX_LOVE_8;       //Disable love for 8bpp modes
                                /* Check if we can satisfy the depth it loves */
    if (flags & GFX_LOVE_8) testbpp = 8;
    else if (flags & GFX_LOVE_15) testbpp = 15;
    else if (flags & GFX_LOVE_16) testbpp = 16;
    else if (flags & GFX_LOVE_32) testbpp = 32;
    else testbpp = 0;

    if (sdl.desktop.fullscreen)
        gotbpp = (unsigned int)SDL_VideoModeOK(640, 480, (int)testbpp,
        (unsigned int)SDL_FULLSCREEN | (unsigned int)SDL_HWSURFACE | (unsigned int)SDL_HWPALETTE);
    else
        gotbpp = sdl.desktop.bpp;

    /* SDL 1.x and sometimes SDL 2.x mistake 15-bit 5:5:5 RGB for 16-bit 5:6:5 RGB
    * which causes colors to mis-display. This seems to be common with Windows and Linux.
    * If SDL said 16-bit but the bit masks suggest 15-bit, then make the correction now. */
    if (gotbpp == 16) {
        if (sdl.surface->format->Gshift == 5 && sdl.surface->format->Gmask == (31U << 5U)) {
            LOG_MSG("NOTE: SDL returned 16-bit/pixel mode (5:6:5) but failed to recognize your screen is 15-bit/pixel mode (5:5:5)");
            gotbpp = 15;
        }
    }

    /* If we can't get our favorite mode check for another working one */
    switch (gotbpp) {
    case 8:
        if (flags & GFX_CAN_8) flags &= ~(GFX_CAN_15 | GFX_CAN_16 | GFX_CAN_32);
        break;
    case 15:
        if (flags & GFX_CAN_15) flags &= ~(GFX_CAN_8 | GFX_CAN_16 | GFX_CAN_32);
        break;
    case 16:
        if (flags & GFX_CAN_16) flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_32);
        break;
    case 24:
    case 32:
        if (flags & GFX_CAN_32) flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
        break;
    }
    flags |= GFX_CAN_RANDOM;
    return flags;
}

Bitu OUTPUT_SURFACE_SetSize()
{
    Bitu bpp = 0, retFlags = 0;

    // localize width and height variables because they can be locally adjusted by aspect ratio correction
retry:
    Bitu width = sdl.draw.width;
    Bitu height = sdl.draw.height;

    if (sdl.draw.flags & GFX_CAN_32)
        bpp = 32;
    else if (sdl.draw.flags & GFX_CAN_16)
        bpp = 16;
    else if (sdl.draw.flags & GFX_CAN_15)
        bpp = 15;
    else if (sdl.draw.flags & GFX_CAN_8)
        bpp = 8;

#if defined(WIN32) && !defined(C_SDL2)
    /* SDL 1.x might mis-inform us on 16bpp for 15-bit color, which is bad enough.
    But on Windows, we're still required to ask for 16bpp to get the 15bpp mode we want. */
    if (bpp == 15)
    {
        if (sdl.surface->format->Gshift == 5 && sdl.surface->format->Gmask == (31U << 5U))
        {
            LOG_MSG("SDL hack: Asking for 16-bit color (5:6:5) to get SDL to give us 15-bit color (5:5:5) to match your screen.");
            bpp = 16;
        }
    }
#endif

#if C_XBRZ || C_SURFACE_POSTRENDER_ASPECT
    // there is a small problem we need to solve here: aspect corrected windows can be smaller than needed due to source with non-4:3 pixel ratio
    // if we detect non-4:3 pixel ratio here with aspect correction on, we correct it so original fits into resized window properly
    if (render.aspect) aspectCorrectExtend(width, height);
#endif

    sdl.clip.w = (Uint16)width; sdl.clip.h = (Uint16)height;
    if (sdl.desktop.fullscreen)
    {
        Uint32 wflags = SDL_FULLSCREEN | SDL_HWPALETTE |
            ((sdl.draw.flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) |
            (sdl.desktop.doublebuf ? SDL_DOUBLEBUF | SDL_ASYNCBLIT : 0);

        if (sdl.desktop.full.fixed)
        {
            sdl.clip.x = (Sint16)((sdl.desktop.full.width - width) / 2);
            sdl.clip.y = (Sint16)((sdl.desktop.full.height - height) / 2);
            if (sdl.clip.x < 0) sdl.clip.x = 0;
            if (sdl.clip.y < 0) sdl.clip.y = 0;

            int fw = (std::max)((int)sdl.desktop.full.width,  (sdl.clip.x+sdl.clip.w));
            int fh = (std::max)((int)sdl.desktop.full.height, (sdl.clip.y+sdl.clip.h));

            sdl.surface = SDL_SetVideoMode(fw, fh, (int)bpp, wflags);
            sdl.deferred_resize = false;
            sdl.must_redraw_all = true;

#if C_XBRZ
            /* scale to fit the window.
             * fit by aspect ratio if asked to do so. */
            if (sdl_xbrz.enable)
            {
                sdl.clip.x = sdl.clip.y = 0;
                sdl.clip.w = sdl.desktop.full.width;
                sdl.clip.h = sdl.desktop.full.height;
                if (render.aspect) aspectCorrectFitClip(sdl.clip.w, sdl.clip.h, sdl.clip.x, sdl.clip.y, sdl.desktop.full.width, sdl.desktop.full.height);
            }
#endif 
        }
        else
        {
            sdl.clip.x = 0; sdl.clip.y = 0;
            sdl.surface = SDL_SetVideoMode((int)width, (int)height, (int)bpp, wflags);
            sdl.deferred_resize = false;
            sdl.must_redraw_all = true;
        }

        if (sdl.surface == NULL) {
            LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
            sdl.desktop.fullscreen = false;
            GFX_CaptureMouse();
            goto retry;
        }
    }
    else
    {
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
        int final_width,final_height,ax,ay;

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
            ax = (final_width - (sdl.clip.x + sdl.clip.w)) / 2;
            ay = (final_height - (sdl.clip.y + sdl.clip.h)) / 2;
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

            sdl.surface = SDL_SetVideoMode((int)final_width, (int)final_height, (int)bpp,
                (unsigned int)((sdl.draw.flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) |
#ifdef SDL_DOSBOX_X_SPECIAL
                (unsigned int)SDL_HAX_NOREFRESH | (unsigned int)(setSizeButNotResize() ? SDL_HAX_NORESIZEWINDOW : 0) |
#endif
                (unsigned int)SDL_RESIZABLE);

            sdl.deferred_resize = false;
            sdl.must_redraw_all = true;

            if (SDL_MUSTLOCK(sdl.surface))
                SDL_LockSurface(sdl.surface);

            memset(sdl.surface->pixels, 0, (unsigned int)sdl.surface->pitch * (unsigned int)sdl.surface->h);

            if (SDL_MUSTLOCK(sdl.surface))
                SDL_UnlockSurface(sdl.surface);
        }

#ifdef WIN32
        if (sdl.surface == NULL)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);

            if (!sdl.using_windib)
            {
                LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with windib enabled.");
                putenv("SDL_VIDEODRIVER=windib");
                sdl.using_windib = true;
            }
            else
            {
                LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with directx enabled.");
                putenv("SDL_VIDEODRIVER=directx");
                sdl.using_windib = false;
            }

            SDL_InitSubSystem(SDL_INIT_VIDEO);
            GFX_SetIcon(); // set icon again

            sdl.surface = SDL_SetVideoMode((int)width, (int)height, (int)bpp, SDL_HWSURFACE);
            sdl.deferred_resize = false;
            sdl.must_redraw_all = true;

            if (sdl.surface) GFX_SetTitle(-1, -1, -1, false); //refresh title
        }
#endif
        if (sdl.surface == NULL)
            E_Exit("Could not set windowed video mode %ix%i-%i: %s", (int)width, (int)height, (int)bpp, SDL_GetError());
    }

    if (sdl.surface)
    {
        switch (sdl.surface->format->BitsPerPixel)
        {
            case 8:
                retFlags = GFX_CAN_8;
                break;
            case 15:
                retFlags = GFX_CAN_15;
                break;
            case 16:
                if (sdl.surface->format->Gshift == 5 && sdl.surface->format->Gmask == (31U << 5U))
                    retFlags = GFX_CAN_15;
                else
                    retFlags = GFX_CAN_16;
                break;
            case 32:
                retFlags = GFX_CAN_32;
                break;
        }

        if (retFlags && (sdl.surface->flags & SDL_HWSURFACE))
            retFlags |= GFX_HARDWARE;

        if (retFlags && (sdl.surface->flags & SDL_DOUBLEBUF))
        {
            sdl.blit.surface = SDL_CreateRGBSurface((Uint32)SDL_HWSURFACE,
                (int)sdl.draw.width, (int)sdl.draw.height,
                (int)sdl.surface->format->BitsPerPixel,
                (Uint32)sdl.surface->format->Rmask,
                (Uint32)sdl.surface->format->Gmask,
                (Uint32)sdl.surface->format->Bmask,
                (Uint32)0u);
            /* If this one fails be ready for some flickering... */
        }

#if C_XBRZ
        if (sdl_xbrz.enable)
        {
            bool old_scale_on = sdl_xbrz.scale_on;
            xBRZ_SetScaleParameters((int)sdl.draw.width, (int)sdl.draw.height, sdl.clip.w, sdl.clip.h);
            if (sdl_xbrz.scale_on != old_scale_on) {
                // when we are scaling, we ask render code not to do any aspect correction
                // when we are not scaling, render code is allowed to do aspect correction at will
                // due to this, at each scale mode change we need to schedule resize again because window size could change
                PIC_AddEvent(VGA_SetupDrawing, 50); // schedule another resize here, render has already been initialized at this point and we have just changed its option
            }
        }
#endif
    }

    /* WARNING: If the user is resizing our window to smaller than what we want, SDL2 will give us a
     *          window surface according to the smaller size, and then we crash! */
    assert(sdl.surface->w >= (sdl.clip.x+sdl.clip.w));
    assert(sdl.surface->h >= (sdl.clip.y+sdl.clip.h));

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)sdl.surface->w;
    mainMenu.screenHeight = (size_t)sdl.surface->h;
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif

    return retFlags;
}
#endif /*!defined(C_SDL2)*/
