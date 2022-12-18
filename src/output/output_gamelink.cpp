#include "config.h"

#if C_GAMELINK

#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "sdlmain.h"
#include "render.h"
#include "vga.h"
#include "mem.h"
#include "mixer.h"
#include "mapper.h"
#include "../gamelink/scancodes_windows.h"
#include "../output/output_surface.h"

using namespace std;

extern uint32_t RunningProgramHash[4];
extern const char* RunningProgram;

#if !defined(C_SDL2)
#error "gamelink output requires SDL2"
#endif

void OUTPUT_GAMELINK_Select()
{
    //LOG_MSG("OUTPUT_GAMELINK: Select");
    if (!sdl.gamelink.enable) {
#ifdef WIN32
        MessageBoxA( NULL, "ERROR: Game Link output disabled.",
                                "DOSBox \"Game Link\" Error", MB_OK | MB_ICONSTOP );
#else // WIN32
        LOG_MSG( "OUTPUT_GAMELINK: Not enabled via `gamelink master = true`, falling back to `output=surface`." );
#endif // WIN32
        OUTPUT_SURFACE_Select();
        return;
    }

    sdl.desktop.want_type = SCREEN_GAMELINK;
    render.aspectOffload = true;
    sdl.desktop.fullscreen = false;
    sdl.mouse.autoenable = false;
    sdl.gamelink.want_mouse = false;
}

void OUTPUT_GAMELINK_InputEvent()
{
    //LOG_MSG("OUTPUT_GAMELINK: InputEvent");
    //
    // Client Mouse & Keyboard

    if ( GameLink::In( &sdl.gamelink.input, &sdl.gamelink.audio ) )
    {
        //
        // -- Audio

        float vol0 = sdl.gamelink.audio.master_vol_l / 100.0f;
        float vol1 = sdl.gamelink.audio.master_vol_r / 100.0f;
        MIXER_SetMaster( vol0, vol1 );


        //
        // -- Input

        Mouse_CursorMoved( 
            sdl.gamelink.input.mouse_dx, 
            sdl.gamelink.input.mouse_dy, 
            0, 
            0, true /*emulate*/ );

        // Cache old and new
        const uint8_t old = sdl.gamelink.input_prev.mouse_btn;;
        const uint8_t btn = sdl.gamelink.input.mouse_btn;

        // Generate mouse events.
        for ( uint8_t i = 0; i < 3; ++i )
        {
            const uint8_t mask = 1 << i;
            if ( ( btn & mask ) && !( old & mask ) ) {
                Mouse_ButtonPressed( i ); // L
            }
            if ( !( btn & mask ) && ( old & mask ) ) {
                Mouse_ButtonReleased( i ); // L
            }
        }

        // Generate key events
        for ( uint8_t blk = 0; blk < 8; ++blk )
        {
            const uint32_t old = sdl.gamelink.input_prev.keyb_state[ blk ];
            const uint32_t key = sdl.gamelink.input.keyb_state[ blk ];
            for ( uint8_t bit = 0; bit < 32; ++bit )
            {
                const SDL_Scancode scancode = windows_scancode_table[static_cast< int >( ( blk * 32 ) + bit )];
            
                // Build event
                SDL_Event ev;
                ev.key.type = 0;
                ev.key.keysym.scancode = scancode;
            
                ev.key.keysym.mod = KMOD_NONE; // todo
                ev.key.keysym.sym = SDLK_UNKNOWN; // todo

                const uint32_t mask = 1 << bit;
                if ( ( key & mask ) && !( old & mask ) ) {
                    ev.key.type = SDL_KEYDOWN;
                    ev.key.state = SDL_PRESSED;
                }
                if ( !( key & mask ) && ( old & mask ) ) {
                    ev.key.type = SDL_KEYUP;
                    ev.key.state = SDL_RELEASED;
                }

                // Dispatch?
                if ( ev.key.type != 0 )
                {
                    MAPPER_CheckEvent( &ev );
                }
            }
        }

        // Update history state
        memcpy( &sdl.gamelink.input_prev, &sdl.gamelink.input, sizeof( GameLink::sSharedMMapInput_R2 ) );
    }
}

bool OUTPUT_GAMELINK_InitTrackingMode()
{
    //LOG_MSG("OUTPUT_GAMELINK: Game Link %s", sdl.gamelink.enable?"enabled":"disabled");
    if (!sdl.gamelink.enable) return false;

    bool trackonly_mode = sdl.desktop.want_type != SCREEN_GAMELINK;

    memset(&sdl.gamelink.input_prev, 0, sizeof(GameLink::sSharedMMapInput_R2));
    int iresult = GameLink::Init(trackonly_mode);
    if (iresult != 1) {
#ifdef WIN32
        MessageBoxA( NULL, "ERROR: Couldn't initialise Game Link inter-process communication.",
                                "DOSBox \"Game Link\" Error", MB_OK | MB_ICONSTOP );
#else // WIN32
        LOG_MSG("GAMELINK: Couldn't initialise inter-process communication.");
#endif // WIN32
        sdl.gamelink.enable = false;
        return false;
    }
    return true;
}


Bitu OUTPUT_GAMELINK_SetSize()
{
    //LOG_MSG("OUTPUT_GAMELINK: SetSize");
    Bitu bpp = 0;
    Bitu retFlags = 0;
    (void)bpp;

    if (sdl.desktop.fullscreen) GFX_ForceFullscreenExit();

    sdl.surface = 0;
    sdl.clip.w = sdl.draw.width;
    sdl.clip.h = sdl.draw.height;
    sdl.clip.x = 0;
    sdl.clip.y = 0;

    if (sdl.gamelink.framebuf == 0 ) {
        sdl.gamelink.framebuf = malloc(SCALER_MAXWIDTH * SCALER_MAXHEIGHT * 4);   // 32 bit color frame buffer
    }
    sdl.gamelink.pitch = sdl.draw.width*4;

    sdl.desktop.type = SCREEN_GAMELINK;
    retFlags = GFX_CAN_32 | GFX_SCALING;

    LOG_MSG("GAMELINK: rendersize=%ux%u", (unsigned int)sdl.draw.width, (unsigned int)sdl.draw.height);

#if C_XBRZ
    if (sdl_xbrz.enable)
    {
        bool old_scale_on = sdl_xbrz.scale_on;
        xBRZ_SetScaleParameters(sdl.draw.width, sdl.draw.height, sdl.desktop.window.width, sdl.desktop.window.height);
        if (sdl_xbrz.scale_on != old_scale_on) {
            // when we are scaling, we ask render code not to do any aspect correction
            // when we are not scaling, render code is allowed to do aspect correction at will
            // due to this, at each scale mode change we need to schedule resize again because window size could change
            PIC_AddEvent(VGA_SetupDrawing, 50); // schedule another resize here, render has already been initialized at this point and we have just changed its option
        }
        sdl.clip.x = sdl.clip.y = 0;
        sdl.clip.w = sdl.desktop.window.width;
        sdl.clip.h = sdl.desktop.window.height;
        sdl.gamelink.pitch = sdl.clip.w*4;
    }
#endif

    if (sdl.clip.w > GameLink::sSharedMMapFrame_R1::MAX_WIDTH || sdl.clip.h > GameLink::sSharedMMapFrame_R1::MAX_HEIGHT) {
#ifdef WIN32
        MessageBoxA( NULL, "ERROR: Game Link output resolution too big (windowresolution).",
                                "DOSBox \"Game Link\" Error", MB_OK | MB_ICONSTOP );
#else // WIN32
        LOG_MSG("GAMELINK: Output resolution too big (windowresolution).");
#endif // WIN32
        sdl.gamelink.enable = false;
        return 0;
    }

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    sdl.window = GFX_SetSDLWindowMode(640, 200, SCREEN_GAMELINK);
    if (sdl.window == NULL)
        E_Exit("Could not set windowed video mode %ix%i: %s", (int)sdl.draw.width, (int)sdl.draw.height, SDL_GetError());

    sdl.surface = SDL_GetWindowSurface(sdl.window);

    // FIXME: find proper way to refresh the menu bar without causing infinite recursion
    static bool in_setsize = false;
    if (!in_setsize) {
        in_setsize = true;
        DOSBox_RefreshMenu();
        in_setsize = false;
    }

    return retFlags;
}

bool OUTPUT_GAMELINK_StartUpdate(uint8_t* &pixels, Bitu &pitch)
{
    //LOG_MSG("OUTPUT_GAMELINK: StartUpdate");
#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        sdl_xbrz.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl_xbrz.renderbuf.empty() ? nullptr : reinterpret_cast<uint8_t*>(&sdl_xbrz.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
    {
        pixels = (uint8_t *)sdl.gamelink.framebuf;
        pitch = sdl.gamelink.pitch;
    }

    sdl.updating = true;
    return true;
}

void OUTPUT_GAMELINK_Transfer()
{
    //LOG_MSG("OUTPUT_GAMELINK: Transfer");
    GameLink::Out( (uint16_t)sdl.clip.w+2*sdl.clip.x, (uint16_t)sdl.clip.h+2*sdl.clip.y, render.src.ratio,
        sdl.gamelink.want_mouse,
        RunningProgram,
        RunningProgramHash,
        (const uint8_t*)sdl.gamelink.framebuf,
        MemBase );
}

void OUTPUT_GAMELINK_EndUpdate(const uint16_t *changedLines)
{
    //LOG_MSG("OUTPUT_GAMELINK: EndUpdate");
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
            uint32_t* clipTrg = reinterpret_cast<uint32_t*>(static_cast<char*>(sdl.gamelink.framebuf) + clipY * sdl.gamelink.pitch + (unsigned int)clipX * sizeof(uint32_t));
            xBRZ_PostScale(&xbrzBuf[0], (int)xbrzWidth, (int)xbrzHeight, (int)(xbrzWidth * sizeof(uint32_t)),
                &clipTrg[0], clipWidth, clipHeight, sdl.gamelink.pitch, 
                sdl_xbrz.postscale_bilinear, sdl_xbrz.task_granularity);

        }
    }
#endif /*C_XBRZ*/
    if (!menu.hidecycles) frames++;
    SDL_UpdateWindowSurface(sdl.window);
}

void OUTPUT_GAMELINK_Shutdown()
{
    //LOG_MSG("OUTPUT_GAMELINK: Shutdown");
    GameLink::Term();
}

#endif
