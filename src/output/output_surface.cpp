#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "sdlmain.h"
#include "render.h"
#include "vga.h"

using namespace std;

bool setSizeButNotResize();

// output API below

#if defined(C_X11) && defined(LINUX)
void X11_ErrorHandlerInstall(void);

void OUTPUT_SURFACE_Initialize()
{
    // Apparently if the window size changes rapidly enough, SDL2 can be tricked into
    // blitting the wrong dimensions to the window and trigger an X11 BadValue error.
    // Set up an error handler that prints the error to STDERR and then returns,
    // instead of the default handler which prints an error and exit()s this program.
    X11_ErrorHandlerInstall();
}
#else
void OUTPUT_SURFACE_Initialize()
{
}
#endif

void OUTPUT_SURFACE_Select()
{
    sdl.desktop.want_type = SCREEN_SURFACE;
    render.aspectOffload = false;

#if defined(WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    SDL1_hax_inhibit_WM_PAINT = 0;
#endif
}

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
            if (sdl.window == NULL)
                E_Exit("Could not set windowed video mode %ix%i: %s", (int)sdl.draw.width, (int)sdl.draw.height, SDL_GetError());
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
     *          window surface according to the smaller size, and then we crash!
     *
     *          To avoid this crash, disable rendering until the window is big enough again. */
    if (sdl.surface->w < (sdl.clip.x+sdl.clip.w) || sdl.surface->h < (sdl.clip.y+sdl.clip.h))
        sdl.window_too_small = true;
    else
        sdl.window_too_small = false;

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    /* Fix a glitch with aspect=true occurring when
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
#else
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

bool OUTPUT_SURFACE_StartUpdate(uint8_t* &pixels, Bitu &pitch)
{
#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        sdl_xbrz.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl_xbrz.renderbuf.empty() ? nullptr : reinterpret_cast<uint8_t*>(&sdl_xbrz.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
#if C_SURFACE_POSTRENDER_ASPECT
        if (render.aspect == ASPECT_NEAREST || render.aspect == ASPECT_BILINEAR)
        {
            sdl.aspectbuf.resize(sdl.draw.width * sdl.draw.height);
            pixels = sdl.aspectbuf.empty() ? nullptr : reinterpret_cast<uint8_t*>(&sdl.aspectbuf[0]);
            pitch = sdl.draw.width * sizeof(uint32_t);
        }
        else
#endif
        {
            if (sdl.blit.surface)
            {
                if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
                    return false;
                pixels = (uint8_t *)sdl.blit.surface->pixels;
                pitch = sdl.blit.surface->pitch;
            }
            else
            {
                if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
                    return false;
                pixels = (uint8_t *)sdl.surface->pixels;
                pixels += sdl.clip.y * sdl.surface->pitch;
                pixels += sdl.clip.x * sdl.surface->format->BytesPerPixel;
                pitch = sdl.surface->pitch;
            }
        }

    GFX_SDL_Overscan();
    sdl.updating = true;
    return true;
}

void OUTPUT_SURFACE_EndUpdate(const uint16_t *changedLines)
{
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif
#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        const uint32_t srcWidth = sdl.draw.width;
        const uint32_t srcHeight = sdl.draw.height;
        if (sdl_xbrz.renderbuf.size() == (unsigned int)srcWidth * (unsigned int)srcHeight && srcWidth > 0 && srcHeight > 0)
        {
            // please use sdl.clip to keep screen positioning consistent with the rest of the emulator
            int clipWidth = sdl.clip.w;
            int clipHeight = sdl.clip.h;
            int clipX = sdl.clip.x;
            int clipY = sdl.clip.y;

            // 1. xBRZ-scale render buffer into xbrz pixel buffer
            unsigned int xbrzWidth = 0;
            unsigned int xbrzHeight = 0;
            uint32_t* xbrzBuf;
            xbrzWidth = srcWidth * (unsigned int)sdl_xbrz.scale_factor;
            xbrzHeight = srcHeight * (unsigned int)sdl_xbrz.scale_factor;
            sdl_xbrz.pixbuf.resize(xbrzWidth * xbrzHeight);

            const uint32_t* renderBuf = &sdl_xbrz.renderbuf[0]; // help VS compiler a little + support capture by value
            xbrzBuf = &sdl_xbrz.pixbuf[0];
            xBRZ_Render(renderBuf, xbrzBuf, changedLines, (int)srcWidth, (int)srcHeight, sdl_xbrz.scale_factor);

            // 2. nearest neighbor/bilinear scale xbrz buffer into output surface clipping area
            const bool mustLock = SDL_MUSTLOCK(sdl.surface);
            if (mustLock) SDL_LockSurface(sdl.surface);
            if (sdl.surface->pixels) // if locking fails, this can be nullptr, also check if we really need to draw
            {
                uint32_t* clipTrg = reinterpret_cast<uint32_t*>(static_cast<char*>(sdl.surface->pixels) + clipY * sdl.surface->pitch + (unsigned int)clipX * sizeof(uint32_t));
                xBRZ_PostScale(&xbrzBuf[0], (int)xbrzWidth, (int)xbrzHeight, (int)(xbrzWidth * sizeof(uint32_t)),
                    &clipTrg[0], clipWidth, clipHeight, sdl.surface->pitch, 
                    sdl_xbrz.postscale_bilinear, sdl_xbrz.task_granularity);
            }

            if (mustLock) SDL_UnlockSurface(sdl.surface);
            if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
#if defined(C_SDL2)
            SDL_UpdateWindowSurface(sdl.window);
#else
            SDL_Flip(sdl.surface);
#endif
        }
    }
    else
#endif /*C_XBRZ*/
#if C_SURFACE_POSTRENDER_ASPECT
    if (render.aspect == ASPECT_NEAREST || render.aspect == ASPECT_BILINEAR) {
        // here we go, adjusting source aspect ratio
        int clipWidth = sdl.clip.w;
        int clipHeight = sdl.clip.h;
        int clipX = sdl.clip.x;
        int clipY = sdl.clip.y;

        const bool mustLock = SDL_MUSTLOCK(sdl.surface);
        if (mustLock) SDL_LockSurface(sdl.surface);
        if (sdl.surface->pixels) // if locking fails, this can be nullptr, also check if we really need to draw
        {
            uint32_t* clipTrg = reinterpret_cast<uint32_t*>(static_cast<char*>(sdl.surface->pixels) + clipY * sdl.surface->pitch + clipX * sizeof(uint32_t));
            xBRZ_PostScale(&sdl.aspectbuf[0], sdl.draw.width, sdl.draw.height, sdl.draw.width * sizeof(uint32_t),
                &clipTrg[0], clipWidth, clipHeight, sdl.surface->pitch,
                (render.aspect == ASPECT_BILINEAR), C_SURFACE_POSTRENDER_ASPECT_BATCH_SIZE);
        }

        if (mustLock) SDL_UnlockSurface(sdl.surface);
        if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 1);
#else
        SDL_Flip(sdl.surface);
#endif
    }
    else
#endif /*C_SURFACE_POSTRENDER_ASPECT*/
    {
        if (SDL_MUSTLOCK(sdl.surface)) {
            if (sdl.blit.surface) {
                SDL_UnlockSurface(sdl.blit.surface);
                int Blit = SDL_BlitSurface(sdl.blit.surface, 0, sdl.surface, &sdl.clip);
                LOG(LOG_MISC, LOG_WARN)("BlitSurface returned %d", Blit);
            }
            else {
                SDL_UnlockSurface(sdl.surface);
            }
            if (changedLines && (changedLines[0] == sdl.draw.height))
                return;
            if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
#if defined(C_SDL2)
            SDL_UpdateWindowSurface(sdl.window);
#else
            SDL_Flip(sdl.surface);
#endif
        }
        else if (sdl.must_redraw_all) {
#if defined(C_SDL2)
            if (changedLines != NULL) SDL_UpdateWindowSurface(sdl.window);
#else
            if (changedLines != NULL) SDL_Flip(sdl.surface);
#endif
        }
        else if (changedLines) {
            if (changedLines[0] == sdl.draw.height)
                return;
            if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
            Bitu y = 0, index = 0, rectCount = 0;
            while (y < sdl.draw.height) {
                if (!(index & 1)) {
                    y += changedLines[index];
                }
                else {
                    SDL_Rect *rect = &sdl.updateRects[rectCount++];
                    rect->x = sdl.clip.x;
                    rect->y = sdl.clip.y + (int)y;
                    rect->w = (uint16_t)sdl.draw.width;
                    rect->h = changedLines[index];
                    y += changedLines[index];
                    SDL_rect_cliptoscreen(*rect);
                }
                index++;
            }
            if (rectCount) {
#if defined(C_SDL2)
                SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, (int)rectCount);
#else
                SDL_UpdateRects(sdl.surface, (int)rectCount, sdl.updateRects);
#endif
            }
        }
    }
}

void OUTPUT_SURFACE_Shutdown()
{
    // nothing to shutdown (yet?)
}
