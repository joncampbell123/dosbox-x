#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"
#include "vga.h"

using namespace std;

// output API below

void OUTPUT_SURFACE_Initialize()
{
    // nothing to initialize (yet?)
}

void OUTPUT_SURFACE_Select()
{
    sdl.desktop.want_type = SCREEN_SURFACE;
    render.aspectOffload = false;

#if defined(WIN32) && !defined(C_SDL2)
    SDL1_hax_inhibit_WM_PAINT = 0;
#endif
}

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

void OUTPUT_SURFACE_EndUpdate(const Bit16u *changedLines)
{
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif
#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        const Bit32u srcWidth = sdl.draw.width;
        const Bit32u srcHeight = sdl.draw.height;
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
                    rect->w = (Bit16u)sdl.draw.width;
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
