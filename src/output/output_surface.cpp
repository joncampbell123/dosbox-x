#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"
#include "menu.h"
#include "vga.h"

void OUTPUT_SURFACE_Select()
{
    sdl.desktop.want_type = SCREEN_SURFACE;
    render.aspectOffload = false;
}

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
        gotbpp = (unsigned int)SDL_VideoModeOK(640, 480, testbpp,
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

Bitu OUTPUT_SURFACE_SetSize_SDL()
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
    if (render.aspect)
    {
        if (width * sdl.srcAspect.y != height * sdl.srcAspect.x)
        {
            // abnormal aspect ratio detected, apply correction
            if (width * sdl.srcAspect.y > height * sdl.srcAspect.x)
            {
                // wide pixel ratio, height should be extended to fit
                height = (Bitu)floor((double)width * sdl.srcAspect.y / sdl.srcAspect.x + 0.5);
            }
            else
            {
                // long pixel ratio, width should be extended
                width = (Bitu)floor((double)height * sdl.srcAspect.x / sdl.srcAspect.y + 0.5);
            }
        }
    }
#endif

    sdl.desktop.type = SCREEN_SURFACE;
    sdl.clip.w = width; sdl.clip.h = height;
    if (sdl.desktop.fullscreen)
    {
        Uint32 wflags = SDL_FULLSCREEN | SDL_HWPALETTE |
            ((sdl.draw.flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) |
            (sdl.desktop.doublebuf ? SDL_DOUBLEBUF | SDL_ASYNCBLIT : 0);

        if (sdl.desktop.full.fixed)
        {
            sdl.clip.x = (Sint16)((sdl.desktop.full.width - width) / 2);
            sdl.clip.y = (Sint16)((sdl.desktop.full.height - height) / 2);
            sdl.surface = SDL_SetVideoMode(sdl.desktop.full.width, sdl.desktop.full.height, bpp, wflags);
            sdl.deferred_resize = false;
            sdl.must_redraw_all = true;
        }
        else
        {
            sdl.clip.x = 0; sdl.clip.y = 0;
            sdl.surface = SDL_SetVideoMode(width, height, bpp, wflags);
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
#endif

        /* center the screen in the window */
        {
            int menuheight = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
            if (mainMenu.isVisible()) menuheight = mainMenu.menuBox.h;
#endif
            Bitu consider_height = menu.maxwindow ? currentWindowHeight : (height + (unsigned int)menuheight + (sdl.overscan_width * 2));
            Bitu consider_width = menu.maxwindow ? currentWindowWidth : (width + (sdl.overscan_width * 2));

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
            if (mainMenu.isVisible())
            {
                /* enforce a minimum 640x400 surface size.
                * the menus are useless below 640x400 */
                if (consider_width < (640 + (sdl.overscan_width * 2)))
                    consider_width = (640 + (sdl.overscan_width * 2));
                if (consider_height < (400 + (sdl.overscan_width * 2) + (unsigned int)menuheight))
                    consider_height = (400 + (sdl.overscan_width * 2) + (unsigned int)menuheight);
            }
#endif

            int final_height = (int)max(max(consider_height, userResizeWindowHeight), (Bitu)(sdl.clip.y + sdl.clip.h)) - (int)menuheight - ((int)sdl.overscan_width * 2);
            int final_width = (int)max(max(consider_width, userResizeWindowWidth), (Bitu)(sdl.clip.x + sdl.clip.w)) - ((int)sdl.overscan_width * 2);
            int ax = (final_width - (sdl.clip.x + sdl.clip.w)) / 2;
            int ay = (final_height - (sdl.clip.y + sdl.clip.h)) / 2;
            if (ax < 0) ax = 0;
            if (ay < 0) ay = 0;
            sdl.clip.x += ax + (int)sdl.overscan_width;
            sdl.clip.y += ay + (int)sdl.overscan_width;
            // sdl.clip.w = currentWindowWidth - sdl.clip.x;
            // sdl.clip.h = currentWindowHeight - sdl.clip.y;

            final_width += (int)sdl.overscan_width * 2;
            final_height += (int)menuheight + (int)sdl.overscan_width * 2;
            sdl.clip.y += (int)menuheight;

            LOG_MSG("surface consider=%ux%u final=%ux%u",
                (unsigned int)consider_width,
                (unsigned int)consider_height,
                (unsigned int)final_width,
                (unsigned int)final_height);

            sdl.surface = SDL_SetVideoMode(final_width, final_height, bpp,
                (unsigned int)((sdl.draw.flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) |
                (unsigned int)SDL_HAX_NOREFRESH |
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

            sdl.surface = SDL_SetVideoMode(width, height, bpp, SDL_HWSURFACE);
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
        if (render.xBRZ.enable)
        {
            // pre-calculate scaling factor adjusting for aspect rate if needed
            int clipWidth = sdl.surface->w;
            int clipHeight = sdl.surface->h;

            if (render.aspect)
            {
                if (clipWidth > sdl.srcAspect.xToY * clipHeight)
                    clipWidth = static_cast<int>(clipHeight * sdl.srcAspect.xToY); // black bars left and right
                else
                    clipHeight = static_cast<int>(clipWidth * sdl.srcAspect.yToX); // black bars top and bottom
            }

            bool old_scale_on = render.xBRZ.scale_on;
            xBRZ_SetScaleParameters(sdl.draw.width, sdl.draw.height, clipWidth, clipHeight);
            if (render.xBRZ.scale_on != old_scale_on) {
                // when we are scaling, we ask render code not to do any aspect correction
                // when we are not scaling, render code is allowed to do aspect correction at will
                // due to this, at each scale mode change we need to schedule resize again because window size could change
                PIC_AddEvent(VGA_SetupDrawing, 50); // schedule another resize here, render has already been initialized at this point and we have just changed its option
            }
        }
#endif
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)sdl.surface->w;
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif

    return retFlags;
}

#if defined(C_SDL2)
Bitu OUTPUT_SURFACE_SetSize_SDL2()
{
    Bitu bpp = 0;
    Bitu retFlags = 0;

    (void)bpp;

    sdl.desktop.type = SCREEN_SURFACE;
    sdl.clip.w = width;
    sdl.clip.h = height;
    if (GFX_IsFullscreen()) {
        if (sdl.desktop.full.fixed) {
            sdl.clip.x = (Sint16)((sdl.desktop.full.width - width) / 2);
            sdl.clip.y = (Sint16)((sdl.desktop.full.height - height) / 2);
            sdl.window = GFX_SetSDLWindowMode(sdl.desktop.full.width,
                sdl.desktop.full.height,
                sdl.desktop.type);
            if (sdl.window == NULL)
                E_Exit("Could not set fullscreen video mode %ix%i-%i: %s", sdl.desktop.full.width, sdl.desktop.full.height, sdl.desktop.bpp, SDL_GetError());
        }
        else {
            sdl.clip.x = 0;
            sdl.clip.y = 0;
            sdl.window = GFX_SetSDLWindowMode(width, height,
                sdl.desktop.type);
            if (sdl.window == NULL)
                LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
            SDL_SetWindowFullscreen(sdl.window, 0);
            GFX_CaptureMouse();
            goto dosurface;
        }
    }
    else {
        int menuheight = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
        if (mainMenu.isVisible()) menuheight = mainMenu.menuBox.h;
#endif

        sdl.clip.x = sdl.overscan_width;
        sdl.clip.y = sdl.overscan_width + menuheight;
        sdl.window = GFX_SetSDLWindowMode(width + 2 * sdl.overscan_width, height + menuheight + 2 * sdl.overscan_width,
            sdl.desktop.type);
        if (sdl.window == NULL)
            E_Exit("Could not set windowed video mode %ix%i: %s", (int)width, (int)height, SDL_GetError());
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
    /* Fix a glitch with aspect=true occuring when
    changing between modes with different dimensions */
    SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = sdl.surface->w;
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif
    SDL_UpdateWindowSurface(sdl.window);

    return retFlags;
}
#endif /*defined(C_SDL2)*/

Bitu OUTPUT_SURFACE_SetSize()
{
#if !defined(C_SDL2)
    return OUTPUT_SURFACE_SetSize_SDL();
#else
    return OUTPUT_SURFACE_SetSize_SDL2();
#endif
}

bool OUTPUT_SURFACE_StartUpdate(Bit8u* &pixels, Bitu &pitch)
{
#if C_XBRZ
    if (render.xBRZ.enable && render.xBRZ.scale_on)
    {
        sdl.xBRZ.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl.xBRZ.renderbuf.empty() ? nullptr : reinterpret_cast<Bit8u*>(&sdl.xBRZ.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
#if C_SURFACE_POSTRENDER_ASPECT
        if (render.aspect == ASPECT_NEAREST || render.aspect == ASPECT_BILINEAR)
        {
            sdl.aspectbuf.resize(sdl.draw.width * sdl.draw.height);
            pixels = sdl.aspectbuf.empty() ? nullptr : reinterpret_cast<Bit8u*>(&sdl.aspectbuf[0]);
            pitch = sdl.draw.width * sizeof(uint32_t);
        }
        else
#endif
        {
            if (sdl.blit.surface)
            {
                if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
                    return false;
                pixels = (Bit8u *)sdl.blit.surface->pixels;
                pitch = sdl.blit.surface->pitch;
            }
            else
            {
                if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
                    return false;
                pixels = (Bit8u *)sdl.surface->pixels;
                pixels += sdl.clip.y * sdl.surface->pitch;
                pixels += sdl.clip.x * sdl.surface->format->BytesPerPixel;
                pitch = sdl.surface->pitch;
            }
        }

    GFX_SDL_Overscan();
    sdl.updating = true;
    return true;
}

void OUTPUT_SURFACE_EndUpdate(const Bit16u *changedLines)
{
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif
#if C_XBRZ
    if (render.xBRZ.enable && render.xBRZ.scale_on)
    {
        const int srcWidth = sdl.draw.width;
        const int srcHeight = sdl.draw.height;
        if (sdl.xBRZ.renderbuf.size() == srcWidth * srcHeight && srcWidth > 0 && srcHeight > 0)
        {
            // we assume render buffer is *not* scaled!
            // recalculation to full output width/height is deliberate here, with xBRZ we nicely fill entire output size!
            const int outputHeight = sdl.surface->h;
            const int outputWidth = sdl.surface->w;
            int clipWidth = outputWidth;
            int clipHeight = outputHeight;
            int clipX = 0;
            int clipY = 0;

            if (render.aspect) {
                if (outputWidth > sdl.srcAspect.xToY * outputHeight) // output broader than input => black bars left and right
                {
                    clipWidth = static_cast<int>(outputHeight * sdl.srcAspect.xToY);
                    clipX = (outputWidth - clipWidth) / 2;
                }
                else // black bars top and bottom
                {
                    clipHeight = static_cast<int>(outputWidth * sdl.srcAspect.yToX);
                    clipY = (outputHeight - clipHeight) / 2;
                }
            }

            // 1. xBRZ-scale render buffer into xbrz pixel buffer
            int xbrzWidth = 0;
            int xbrzHeight = 0;
            uint32_t* xbrzBuf;
            xbrzWidth = srcWidth * sdl.xBRZ.scale_factor;
            xbrzHeight = srcHeight * sdl.xBRZ.scale_factor;
            sdl.xBRZ.pixbuf.resize(xbrzWidth * xbrzHeight);

            const uint32_t* renderBuf = &sdl.xBRZ.renderbuf[0]; // help VS compiler a little + support capture by value
            xbrzBuf = &sdl.xBRZ.pixbuf[0];
            xBRZ_Render(renderBuf, xbrzBuf, changedLines, srcWidth, srcHeight, sdl.xBRZ.scale_factor);

            // 2. nearest neighbor/bilinear scale xbrz buffer into output surface clipping area
            const bool mustLock = SDL_MUSTLOCK(sdl.surface);
            if (mustLock) SDL_LockSurface(sdl.surface);
            if (sdl.surface->pixels) // if locking fails, this can be nullptr, also check if we really need to draw
            {
                uint32_t* clipTrg = reinterpret_cast<uint32_t*>(static_cast<char*>(sdl.surface->pixels) + clipY * sdl.surface->pitch + clipX * sizeof(uint32_t));
                xBRZ_PostScale(&xbrzBuf[0], xbrzWidth, xbrzHeight, xbrzWidth * sizeof(uint32_t), 
                    &clipTrg[0], clipWidth, clipHeight, sdl.surface->pitch, 
                    render.xBRZ.postscale_bilinear, render.xBRZ.task_granularity);
            }

            if (mustLock) SDL_UnlockSurface(sdl.surface);
            if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
#if defined(C_SDL2)
            SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 1);
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
#if !defined(C_SDL2)
            SDL_Flip(sdl.surface);
#endif
        }
        else if (sdl.must_redraw_all) {
#if !defined(C_SDL2)
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
                    rect->y = sdl.clip.y + y;
                    rect->w = (Bit16u)sdl.draw.width;
                    rect->h = changedLines[index];
                    y += changedLines[index];
                    SDL_rect_cliptoscreen(*rect);
                }
                index++;
            }
            if (rectCount) {
#if defined(C_SDL2)
                SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, rectCount);
#else
                SDL_UpdateRects(sdl.surface, rectCount, sdl.updateRects);
#endif
            }
        }
    }
}
