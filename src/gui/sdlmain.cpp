/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <algorithm> // std::transform
#include "cross.h"
#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "bios.h"
#include "support.h"
#include "debug.h"
#include "render.h"
#include "menu.h"
#include "SDL_video.h"
#include "ide.h"
#if 0
#include "../src/libs/gui_tk/gui_tk.h"
#endif

#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "fpu.h"
#include "control.h"

// HACK: Scan codes between SDL and SDL2 are very different.
//       Using SDL1-based DOSBox-X mapper and then running SDL2-based DOSBox-X
//       will result in non-working keyboard input!
#define MAPPERFILE "mapper-" VERSION ".sdl2.map"
//#define DISABLE_JOYSTICK

extern bool keep_umb_on_boot;
extern bool keep_private_area_on_boot;
extern bool dos_kernel_disabled;
bool guest_machine_power_on = false;

std::string custom_savedir;

void SHELL_Run();
void DisableINT33();
void EMS_DoShutDown();
void XMS_DoShutDown();
void DOS_DoShutDown();
void GUS_DOS_Shutdown();
void SBLASTER_DOS_Shutdown();
void DOS_ShutdownDevices(void);
void RemoveEMSPageFrame(void);
void RemoveUMBBlock();
void DOS_GetMemory_unmap();
void VFILE_Shutdown(void);
void PROGRAMS_Shutdown(void);
void DOS_UninstallMisc(void);
void CALLBACK_Shutdown(void);
void DOS_ShutdownDrives();
void VFILE_Shutdown(void);
void DOS_ShutdownFiles();
void FreeBIOSDiskList();
void GFX_ShutDown(void);
void MAPPER_Shutdown();
void SHELL_Init(void);
#if C_DYNAMIC_X86
void CPU_Core_Dyn_X86_Shutdown(void);
#endif

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_ARB           0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB         0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB   0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
typedef void (APIENTRYP PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERARBPROC) (GLenum target);
#endif

PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;

#endif //C_OPENGL

/* TODO: move to general header */
static inline int int_log2(int val) {
    int log = 0;
    while ((val >>= 1) != 0) log++;
    return log;
}

// FIXME: Why is this defined twice?
#define MAPPERFILE "mapper-" VERSION ".sdl2.map"

void						GUI_LoadFonts();
void						GUI_Run(bool);
void						EndSplashScreen();
void						Restart(bool pressed);
bool						RENDER_GetAspect(void);
bool						RENDER_GetAutofit(void);

extern const char*				RunningProgram;
extern bool					CPU_CycleAutoAdjust;
#if !(ENVIRON_INCLUDED)
extern char**					environ;
#endif

Bitu						frames = 0;
double                      rtdelta = 0;
bool						emu_paused = false;
bool						mouselocked = false; //Global variable for mapper
bool						load_videodrv = true;
bool						dos_kernel_disabled = true;
bool						startup_state_numlock = false; // Global for keyboard initialisation
bool						startup_state_capslock = false; // Global for keyboard initialisation

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#if defined(MACOSX)
#define DEFAULT_CONFIG_FILE			"/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE			"/.dosboxrc"
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL				(PRIO_MAX-PRIO_MIN)
#endif

enum SCREEN_TYPES {
    SCREEN_SURFACE,
    SCREEN_TEXTURE,
    SCREEN_OPENGL
};

enum PRIORITY_LEVELS {
    PRIORITY_LEVEL_PAUSE,
    PRIORITY_LEVEL_LOWEST,
    PRIORITY_LEVEL_LOWER,
    PRIORITY_LEVEL_NORMAL,
    PRIORITY_LEVEL_HIGHER,
    PRIORITY_LEVEL_HIGHEST
};


struct SDL_Block {
    bool inited;
    bool active;							//If this isn't set don't draw
    bool updating;
    bool update_display_contents;
    bool update_window;
    int window_desired_width, window_desired_height;
    struct {
        Bit32u width;
        Bit32u height;
        Bitu flags;
        double scalex, scaley;
        GFX_CallBack_t callback;
    } draw;
    bool wait_on_error;
    struct {
        struct {
            Bit16u width, height;
            bool fixed;
            bool display_res;
        } full;
        struct {
            Bit16u width, height;
        } window;
        Bit8u bpp;
        Bit32u pixelFormat;
        bool lazy_fullscreen;
        bool lazy_fullscreen_req;
        bool vsync;
        SCREEN_TYPES type;
        SCREEN_TYPES want_type;
    } desktop;
#if C_OPENGL
    struct {
        SDL_GLContext context;
        Bitu pitch;
        void * framebuf;
        GLuint buffer;
        GLuint texture;
        GLuint displaylist;
        GLint max_texsize;
        bool bilinear;
        bool packed_pixel;
        bool paletted_texture;
        bool pixel_buffer_object;
    } opengl;
#endif  // C_OPENGL
    struct {
        PRIORITY_LEVELS focus;
        PRIORITY_LEVELS nofocus;
    } priority;
    SDL_Rect clip;
    SDL_Surface * surface;
    SDL_Window * window;
    SDL_Renderer * renderer;
    const char * rendererDriver;
    int displayNumber;
    struct {
        SDL_Texture * texture;
        SDL_PixelFormat * pixelFormat;
    } texture;
    SDL_cond *cond;
    struct {
        bool autolock;
        bool autoenable;
        bool requestlock;
        bool locked;
        Bitu sensitivity;
    } mouse;
    SDL_Rect updateRects[1024];
    Bitu overscan_color;
    Bitu overscan_width;
    Bitu num_joysticks;
    // state of alt-keys for certain special handlings
    SDL_EventType laltstate;
    SDL_EventType raltstate;
};

static SDL_Block sdl;

/* TODO: should move to it's own file ================================================ */
static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};

static void DOSBox_SetOriginalIcon(void) {
#if !defined(MACOSX)
    SDL_Surface *logos;

#if WORDS_BIGENDIAN
    logos = SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
    logos = SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif // WORDS_BIGENDIAN

    SDL_SetWindowIcon(sdl.window, logos);
#endif // !defined(MACOSX)
}
/* =================================================================================== */

void GFX_SetIcon(void) {
#if !defined(MACOSX)
    /* Set Icon (must be done before any sdl_setvideomode call) */
    /* But don't set it on OS X, as we use a nicer external icon there. */
    /* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
    if (menu_compatible) {
        DOSBox_SetOriginalIcon();
        return;
    }
#endif
}

extern std::string dosbox_title;

void GFX_SetTitle(Bit32s cycles,Bits frameskip,Bits timing,bool paused){
	static Bits internal_frameskip=0;
	static Bit32s internal_cycles=0;
	static Bits internal_timing=0;
	char title[200] = {0};

	if (cycles != -1) internal_cycles = cycles;
	if (timing != -1) internal_timing = timing;
	if (frameskip != -1) internal_frameskip = frameskip;

	if (!menu_startup) {
		sprintf(title,"%s%sDOSBox-X %s, %d cyc/ms, %s",
			dosbox_title.c_str(),dosbox_title.empty()?"":": ",
			VERSION,(int)internal_cycles,RunningProgram);

	    if (!menu.hidecycles) {
            char *p = title + strlen(title); // append to end of string

            sprintf(p,", FPS %2d",(int)frames);
        }

	    if (menu.showrt) {
            char *p = title + strlen(title); // append to end of string

            sprintf(p,", %2d%%/RT",(int)floor((rtdelta / 10) + 0.5));
        }

        SDL_SetWindowTitle(sdl.window,title);
		return;
	}

	if (menu.hidecycles) {
		if (CPU_CycleAutoAdjust) {
			sprintf(title,"%s%sDOSBox-X %s, max %3d%% cyc/ms, %s",
				dosbox_title.c_str(),dosbox_title.empty()?"":": ",
				VERSION,(int)CPU_CyclePercUsed,RunningProgram);
		}
		else {
			sprintf(title,"%s%sDOSBox-X %s, %d cyc/ms, %s",
				dosbox_title.c_str(),dosbox_title.empty()?"":": ",
				VERSION,(int)internal_cycles,RunningProgram);
		}
	} else if (CPU_CycleAutoAdjust) {
		sprintf(title,"%s%sDOSBox-X %s, CPU : %s %d%% = max %3d, %d FPS - %2d %8s %i.%i%%",
			dosbox_title.c_str(),dosbox_title.empty()?"":": ",
			VERSION,core_mode,(int)CPU_CyclePercUsed,(int)internal_cycles,(int)frames,
			(int)internal_frameskip,RunningProgram,(int)(internal_timing/100),(int)(internal_timing%100/10));
	} else {
		sprintf(title,"%s%sDOSBox-X %s, CPU : %s %d = %8d, %d FPS - %2d %8s %i.%i%%",
			dosbox_title.c_str(),dosbox_title.empty()?"":": ",
			VERSION,core_mode,(int)0,(int)internal_cycles,(int)frames,(int)internal_frameskip,
			RunningProgram,(int)(internal_timing/100),(int)((internal_timing%100)/10));
	}

	if (paused) strcat(title," PAUSED");
    SDL_SetWindowTitle(sdl.window,title);
}

bool warn_on_mem_write = false;

void CPU_Snap_Back_To_Real_Mode();

static void KillSwitch(bool pressed) {
    if (!pressed)
        return;
    if (GFX_IsFullscreen()) GFX_SwitchFullScreen();
#if 0 /* Re-enable this hack IF DOSBox continues to have problems page-faulting on kill switch */
    CPU_Snap_Back_To_Real_Mode(); /* TEMPORARY HACK. There are portions of DOSBox that write to memory as if still running DOS. */
    /* ^ Without this hack, when running Windows NT 3.1 this Kill Switch effectively becomes the Instant Page Fault BSOD switch
     * because the DOSBox code attempting to write to real mode memory causes a page fault (though hitting the kill switch a
     * second time shuts DOSBox down properly). It's sort of the same issue behind the INT 33h emulation causing instant BSOD
     * in Windows NT the instant you moved or clicked the mouse. The purpose of this hack is that, before any of that DOSBox
     * code has a chance, we force the CPU back into real mode so that the code doesn't trigger funny page faults and DOSBox
     * shuts down properly. */
#endif
    warn_on_mem_write = true;
    throw 1;
}

static void SDL_Overscan(void) {
    sdl.overscan_color=0;
    if (sdl.overscan_width) {
        Bitu border_color =  GFX_GetRGB(vga.dac.rgb[vga.attr.overscan_color].red<<2,
                                        vga.dac.rgb[vga.attr.overscan_color].green<<2, vga.dac.rgb[vga.attr.overscan_color].blue<<2);
        if (border_color != sdl.overscan_color) {
            sdl.overscan_color = border_color;

            // Find four rectangles forming the border
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0;
            rect->y = 0;
            rect->w = sdl.draw.width+2*sdl.clip.x;
            rect->h = sdl.clip.y; // top
            if ((Bitu)rect->h > (Bitu)sdl.overscan_width) {
                rect->y += (rect->h-sdl.overscan_width);
                rect->h = sdl.overscan_width;
            }
            if ((Bitu)sdl.clip.x > (Bitu)sdl.overscan_width) {
                rect->x += (sdl.clip.x-sdl.overscan_width);
                rect->w -= 2*(sdl.clip.x-sdl.overscan_width);
            }
            rect = &sdl.updateRects[1];
            rect->x = 0;
            rect->y = sdl.clip.y;
            rect->w = sdl.clip.x;
            rect->h = sdl.draw.height; // left
            if (rect->w > sdl.overscan_width) {
                rect->x += (rect->w-sdl.overscan_width);
                rect->w = sdl.overscan_width;
            }
            rect = &sdl.updateRects[2];
            rect->x = sdl.clip.x+sdl.draw.width;
            rect->y = sdl.clip.y;
            rect->w = sdl.clip.x;
            rect->h = sdl.draw.height; // right
            if (rect->w > sdl.overscan_width) {
                rect->w = sdl.overscan_width;
            }
            rect = &sdl.updateRects[3];
            rect->x = 0;
            rect->y = sdl.clip.y+sdl.draw.height;
            rect->w = sdl.draw.width+2*sdl.clip.x;
            rect->h = sdl.clip.y; // bottom
            if ((Bitu)rect->h > (Bitu)sdl.overscan_width) {
                rect->h = sdl.overscan_width;
            }
            if ((Bitu)sdl.clip.x > (Bitu)sdl.overscan_width) {
                rect->x += (sdl.clip.x-sdl.overscan_width);
                rect->w -= 2*(sdl.clip.x-sdl.overscan_width);
            }

            if (sdl.surface->format->BitsPerPixel == 8) { // SDL_FillRect seems to have some issues with palettized hw surfaces
                Bit8u* pixelptr = (Bit8u*)sdl.surface->pixels;
                Bitu linepitch = sdl.surface->pitch;
                for (Bits i=0; i<4; i++) {
                    rect = &sdl.updateRects[i];
                    Bit8u* start = pixelptr + rect->y*linepitch + rect->x;
                    for (Bits j=0; j<rect->h; j++) {
                        memset(start, vga.attr.overscan_color, rect->w);
                        start += linepitch;
                    }
                }
            } else {
                for (Bits i=0; i<4; i++)
                    SDL_FillRect(sdl.surface, &sdl.updateRects[i], border_color);
                SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
            }
        }
    }
}

bool DOSBox_Paused()
{
    return emu_paused;
}

bool pause_on_vsync = false;

void PauseDOSBox(bool pressed) {
    bool paused = true;
    SDL_Event event;

    if (!pressed) return;
    GFX_SetTitle(-1,-1,-1,true);
    KEYBOARD_ClrBuffer();
    GFX_LosingFocus();
    while (SDL_PollEvent(&event)); // flush event queue.

    // reset pause conditions
    pause_on_vsync = false;

    // give mouse to win32 (ex. alt-tab)
    SDL_SetRelativeMouseMode(SDL_FALSE);

    while (paused) {
        SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.

        switch (event.type) {

        case SDL_QUIT:
            KillSwitch(true);
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
                // We may need to re-create a texture and more
                GFX_ResetScreen();
            }
	    break;
        case SDL_KEYDOWN:   // Must use Pause/Break or escape Key to resume.
            if(event.key.keysym.sym == SDLK_PAUSE || event.key.keysym.sym == SDLK_ESCAPE) {

                paused = false;
                GFX_SetTitle(-1,-1,-1,false);
                break;
            }
            else if (event.key.keysym.sym == SDLK_SPACE) { /* spacebar = single frame step */
                /* resume, but let the VGA code know to call us on vertical retrace */
                paused = false;
                pause_on_vsync = true;
                GFX_SetTitle(-1,-1,-1,false);
                break;
            }
#if defined (MACOSX)
            if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RGUI || event.key.keysym.mod == KMOD_LGUI) ) {
                /* On macs, all aps exit when pressing cmd-q */
                KillSwitch(true);
                break;
            }
#endif
        }
    }


    // restore mouse state
    void GFX_UpdateSDLCaptureState();
    GFX_UpdateSDLCaptureState();

    KEYBOARD_ClrBuffer();
    GFX_LosingFocus();

    // redraw screen (ex. fullscreen - pause - alt+tab x2 - unpause)
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackReset );
}


void GFX_ResetScreen(void) {
    GFX_Stop();
    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
    GFX_Start();
    CPU_Reset_AutoAdjust();
}

void GFX_ForceFullscreenExit(void) {
    if (sdl.desktop.lazy_fullscreen) {
//		sdl.desktop.lazy_fullscreen_req=true;
        LOG_MSG("GFX LF: invalid screen change");
    } else {
        SDL_SetWindowFullscreen(sdl.window, 0);
        GFX_ResetScreen();
    }
}

uint32_t GFX_Rmask;
unsigned char GFX_Rshift;
uint32_t GFX_Gmask;
unsigned char GFX_Gshift;
uint32_t GFX_Bmask;
unsigned char GFX_Bshift;
uint32_t GFX_Amask;
unsigned char GFX_Ashift;
unsigned char GFX_bpp;

unsigned int GFX_GetBShift() {
    return sdl.surface->format->Bshift;
}

void GFX_LogSDLState(void) {
    if (sdl.desktop.type == SCREEN_SURFACE) {
        LOG(LOG_MISC,LOG_DEBUG)("SDL video mode: %ux%u (clip %ux%u with upper-left at %ux%u) %ubpp",
                                (unsigned)sdl.surface->w,(unsigned)sdl.surface->h,
                                (unsigned)sdl.clip.w,(unsigned)sdl.clip.h,
                                (unsigned)sdl.clip.x,(unsigned)sdl.clip.y,
                                (unsigned)sdl.surface->format->BitsPerPixel);
        LOG(LOG_MISC,LOG_DEBUG)("   red: shift=%u mask=0x%08lx",
                                (unsigned)sdl.surface->format->Rshift,
                                (unsigned long)sdl.surface->format->Rmask);
        LOG(LOG_MISC,LOG_DEBUG)("   green: shift=%u mask=0x%08lx",
                                (unsigned)sdl.surface->format->Gshift,
                                (unsigned long)sdl.surface->format->Gmask);
        LOG(LOG_MISC,LOG_DEBUG)("   blue: shift=%u mask=0x%08lx",
                                (unsigned)sdl.surface->format->Bshift,
                                (unsigned long)sdl.surface->format->Bmask);
        LOG(LOG_MISC,LOG_DEBUG)("   alpha: shift=%u mask=0x%08lx",
                                (unsigned)sdl.surface->format->Ashift,
                                (unsigned long)sdl.surface->format->Amask);

        GFX_bpp = sdl.surface->format->BitsPerPixel;
        GFX_Rmask = sdl.surface->format->Rmask;
        GFX_Rshift = sdl.surface->format->Rshift;
        GFX_Gmask = sdl.surface->format->Gmask;
        GFX_Gshift = sdl.surface->format->Gshift;
        GFX_Bmask = sdl.surface->format->Bmask;
        GFX_Bshift = sdl.surface->format->Bshift;
        GFX_Amask = sdl.surface->format->Amask;
        GFX_Ashift = sdl.surface->format->Ashift;
    } else if (sdl.desktop.type == SCREEN_TEXTURE) {
        GFX_bpp = sdl.texture.pixelFormat->BitsPerPixel;
        GFX_Rmask = sdl.texture.pixelFormat->Rmask;
        GFX_Rshift = sdl.texture.pixelFormat->Rshift;
        GFX_Gmask = sdl.texture.pixelFormat->Gmask;
        GFX_Gshift = sdl.texture.pixelFormat->Gshift;
        GFX_Bmask = sdl.texture.pixelFormat->Bmask;
        GFX_Bshift = sdl.texture.pixelFormat->Bshift;
        GFX_Amask = sdl.texture.pixelFormat->Amask;
        GFX_Ashift = sdl.texture.pixelFormat->Ashift;
    }
}

static SDL_Window * GFX_SetSDLWindowMode(Bit16u width, Bit16u height, SCREEN_TYPES screenType) {
    static SCREEN_TYPES lastType = SCREEN_SURFACE;
    if (sdl.renderer) {
        SDL_DestroyRenderer(sdl.renderer);
        sdl.renderer=0;
    }
    if (sdl.texture.pixelFormat) {
        SDL_FreeFormat(sdl.texture.pixelFormat);
        sdl.texture.pixelFormat = 0;
    }
    if (sdl.texture.texture) {
        SDL_DestroyTexture(sdl.texture.texture);
        sdl.texture.texture=0;
    }
#if C_OPENGL
    if (sdl.opengl.context) {
        SDL_GL_DeleteContext(sdl.opengl.context);
        sdl.opengl.context=0;
    }
#endif
    sdl.window_desired_width = width;
    sdl.window_desired_height = height;
    int currWidth, currHeight;
    if (sdl.window) {
        //SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
        if (!sdl.update_window) {
            SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
            sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
            return sdl.window;
        }
    }
    /* If we change screen type, recreate the window. Furthermore, if
     * it is our very first time then we simply create a new window.
     */
    if (!sdl.window
            || (lastType != screenType)
//          || (currWidth != width) || (currHeight != height)
//          || (glwindow != (0 != (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_OPENGL)))
//          || (fullscreen && (0 == (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN)))
//          || (fullscreen != (SDL_WINDOW_FULLSCREEN == (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN)))
//          || (fullscreen && ((width != currWidth) || (height != currHeight)))
       ) {
        lastType = screenType;
        if (sdl.window) {
            SDL_DestroyWindow(sdl.window);
        }
        sdl.window = SDL_CreateWindow("",
                                      SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber),
                                      SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber),
                                      width, height,
                                      (GFX_IsFullscreen() ? (sdl.desktop.full.display_res ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0)
                                      | ((screenType == SCREEN_OPENGL) ? SDL_WINDOW_OPENGL : 0) | SDL_WINDOW_SHOWN);
        if (sdl.window) {
            GFX_SetTitle(-1, -1, -1, false); //refresh title.
        }
        SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
        sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
        return sdl.window;
    }
    /* Fullscreen mode switching has its limits, and is also problematic on
     * some window managers. For now, the following may work up to some
     * level. On X11, SDL_VIDEO_X11_LEGACY_FULLSCREEN=1 can also help,
     * although it has its own issues.
     * Suggestion: Use the desktop res if possible, with output=surface
     * if one is not interested in scaling.
     * On Android, desktop res is the only way.
     */
    if (GFX_IsFullscreen()) {
        SDL_DisplayMode displayMode;
        SDL_GetWindowDisplayMode(sdl.window, &displayMode);
        displayMode.w = width;
        displayMode.h = height;
        SDL_SetWindowDisplayMode(sdl.window, &displayMode);

        SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(sdl.window, 0);

        SDL_SetWindowSize(sdl.window, width, height);
    }
    /* Maybe some requested fullscreen resolution is unsupported? */
    SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
    sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
    return sdl.window;
}

// Used for the mapper UI and more: Creates a fullscreen window with desktop res
// on Android, and a non-fullscreen window with the input dimensions otherwise.
SDL_Window * GFX_SetSDLSurfaceWindow(Bit16u width, Bit16u height) {
    return GFX_SetSDLWindowMode(width, height, SCREEN_SURFACE);
}

// Returns the rectangle in the current window to be used for scaling a
// sub-window with the given dimensions, like the mapper UI.
SDL_Rect GFX_GetSDLSurfaceSubwindowDims(Bit16u width, Bit16u height) {
    SDL_Rect rect;
    rect.x=rect.y=0;
    rect.w=width;
    rect.h=height;
    return rect;
}

// Currently used for an initial test here
static SDL_Window * GFX_SetSDLOpenGLWindow(Bit16u width, Bit16u height) {
    return GFX_SetSDLWindowMode(width, height, SCREEN_OPENGL);
}

static SDL_Window * GFX_SetupWindowScaled(SCREEN_TYPES screenType)
{
    Bit16u fixedWidth;
    Bit16u fixedHeight;

    if (GFX_IsFullscreen()) {
        fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
        fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
    } else {
        fixedWidth = sdl.desktop.window.width;
        fixedHeight = sdl.desktop.window.height;
    }
    if (fixedWidth && fixedHeight) {
        double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
        double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
        if ( ratio_w < ratio_h) {
            sdl.clip.w=fixedWidth;
            sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*ratio_w);
        } else {
            sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h);
            sdl.clip.h=(Bit16u)fixedHeight;
        }
        if (GFX_IsFullscreen()) {
            sdl.window = GFX_SetSDLWindowMode(fixedWidth, fixedHeight, screenType);
        } else {
            sdl.window = GFX_SetSDLWindowMode(sdl.clip.w, sdl.clip.h, screenType);
        }
        if (sdl.window && SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
            int windowWidth;
            SDL_GetWindowSize(sdl.window, &windowWidth, NULL);
            sdl.clip.x=(Sint16)((windowWidth-sdl.clip.w)/2);
            sdl.clip.y=(Sint16)((fixedHeight-sdl.clip.h)/2);
        } else {
            sdl.clip.x = 0;
            sdl.clip.y = 0;
        }
    } else {
        sdl.clip.x=0;
        sdl.clip.y=0;
        sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
        sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
        sdl.window = GFX_SetSDLWindowMode(sdl.clip.w, sdl.clip.h, screenType);
    }

    GFX_LogSDLState();
    return sdl.window;
}

static void GFX_ResetSDL() {
#ifdef WIN32
    if(!load_videodrv && !sdl.using_windib) {
        LOG_MSG("Resetting to WINDIB mode");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        putenv("SDL_VIDEODRIVER=windib");
        sdl.using_windib=true;
        if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
        GFX_SetIcon();
        GFX_SetTitle(-1,-1,-1,false);
        if(!IsFullscreen() && GetMenu(GetHWND()) == NULL) DOSBox_RefreshMenu();
    }
#endif
}


Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback) {
    EndSplashScreen();
    if (sdl.updating)
        GFX_EndUpdate( 0 );

    sdl.draw.width=width;
    sdl.draw.height=height;
    sdl.draw.callback=callback;
    sdl.draw.scalex=scalex;
    sdl.draw.scaley=scaley;

    Bitu retFlags = 0;
    switch (sdl.desktop.want_type) {
    case SCREEN_SURFACE:
    {
        GFX_ResetSDL();
dosurface:
        sdl.desktop.type=SCREEN_SURFACE;
        sdl.clip.w=width;
        sdl.clip.h=height;
        if (GFX_IsFullscreen()) {
            if (sdl.desktop.full.fixed) {
                sdl.clip.x=(Sint16)((sdl.desktop.full.width-width)/2);
                sdl.clip.y=(Sint16)((sdl.desktop.full.height-height)/2);
                sdl.window = GFX_SetSDLWindowMode(sdl.desktop.full.width,
                                                  sdl.desktop.full.height,
                                                  sdl.desktop.type);
                if (sdl.window == NULL)
                    E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",sdl.desktop.full.width,sdl.desktop.full.height,sdl.desktop.bpp,SDL_GetError());
            } else {
                sdl.clip.x=0;
                sdl.clip.y=0;
                sdl.window = GFX_SetSDLWindowMode(width, height,
                                                  sdl.desktop.type);
                if (sdl.window == NULL)
                    LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
                SDL_SetWindowFullscreen(sdl.window, 0);
                GFX_CaptureMouse();
                goto dosurface;
            }
        } else {
            sdl.clip.x=sdl.overscan_width;
            sdl.clip.y=sdl.overscan_width;
            sdl.window=GFX_SetSDLWindowMode(width+2*sdl.overscan_width, height+2*sdl.overscan_width,
                                            sdl.desktop.type);
            if (sdl.window == NULL)
                E_Exit("Could not set windowed video mode %ix%i-%i: %s",(int)width,(int)height,SDL_GetError());
        }
        sdl.surface = SDL_GetWindowSurface(sdl.window);
        if (sdl.surface == NULL)
            E_Exit("Could not retrieve window surface: %s",SDL_GetError());
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
        SDL_UpdateWindowSurface(sdl.window);
        break;
    }
    case SCREEN_TEXTURE:
    {
//        GFX_ResetSDL();
        if (!GFX_SetupWindowScaled(sdl.desktop.want_type)) {
            LOG_MSG("SDL:Can't set video mode, falling back to surface");
            goto dosurface;
        }
        if (strcmp(sdl.rendererDriver, "auto"))
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, sdl.rendererDriver);
        sdl.renderer = SDL_CreateRenderer(sdl.window, -1,
                                          SDL_RENDERER_ACCELERATED |
                                          (sdl.desktop.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!sdl.renderer) {
            SDL_DestroyRenderer(sdl.renderer);
            LOG_MSG("SDL_CreateRenderer failed: %s. Falling back to surface.", SDL_GetError());
            goto dosurface;
        }

        /* SDL_PIXELFORMAT_ARGB8888 is possible with most
        rendering drivers, "opengles" being a notable exception */
        sdl.texture.texture = SDL_CreateTexture(sdl.renderer,
                                                SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!sdl.texture.texture) {
            SDL_DestroyRenderer(sdl.renderer);
            sdl.renderer = NULL;
            LOG_MSG("SDL:Can't create texture, falling back to surface");
            goto dosurface;
        }
        SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        sdl.desktop.type=SCREEN_TEXTURE;
        Uint32 pixelFormat;
        SDL_QueryTexture(sdl.texture.texture, &pixelFormat, NULL, NULL, NULL);
        sdl.texture.pixelFormat = SDL_AllocFormat(pixelFormat);
        switch (SDL_BITSPERPIXEL(pixelFormat)) {
        case 8:
            retFlags = GFX_CAN_8;
            break;
        case 15:
            retFlags = GFX_CAN_15;
            break;
        case 16:
            retFlags = GFX_CAN_16;
            break;
        case 24: /* SDL_BYTESPERPIXEL is probably 4, though. */
        case 32:
            retFlags = GFX_CAN_32;
            break;
        }
        retFlags |= GFX_SCALING;
        SDL_RendererInfo rendererInfo;
        SDL_GetRendererInfo(sdl.renderer, &rendererInfo);
        LOG_MSG("Using driver \"%s\" for renderer", rendererInfo.name);
        if (rendererInfo.flags & SDL_RENDERER_ACCELERATED)
            retFlags |= GFX_HARDWARE;
        break;
    }
#if C_OPENGL
    case SCREEN_OPENGL:
    {
        GFX_ResetSDL();
        if (sdl.opengl.pixel_buffer_object) {
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
            if (sdl.opengl.buffer) glDeleteBuffersARB(1, &sdl.opengl.buffer);
        } else if (sdl.opengl.framebuf) {
            free(sdl.opengl.framebuf);
        }
        sdl.opengl.framebuf=0;
        if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface; // BGRA otherwise
        int texsize=2 << int_log2(width > height ? width : height);
        if (texsize>sdl.opengl.max_texsize) {
            LOG_MSG("SDL:OPENGL: No support for texturesize of %d, falling back to surface",texsize);
            goto dosurface;
        }
        SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
        GFX_SetupWindowScaled(sdl.desktop.want_type);
        if (!sdl.window || SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window))<2) {
            LOG_MSG("SDL:OPENGL:Can't open drawing window, are you running in 16bpp(or higher) mode?");
            goto dosurface;
        }
        sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
        if (sdl.opengl.context == NULL) {
            LOG_MSG("SDL:OPENGL:Can't create OpenGL context, falling back to surface");
            goto dosurface;
        }
        /* Sync to VBlank if desired */
        SDL_GL_SetSwapInterval(sdl.desktop.vsync ? 1 : 0 );
        /* Create the texture and display list */
        if (sdl.opengl.pixel_buffer_object) {
            glGenBuffersARB(1, &sdl.opengl.buffer);
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
            glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, width*height*4, NULL, GL_STREAM_DRAW_ARB);
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
        } else
        {
            sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
        }
        sdl.opengl.pitch=width*4;
        int windowHeight;
        SDL_GetWindowSize(sdl.window, NULL, &windowHeight);
        glViewport(sdl.clip.x,sdl.clip.y,sdl.clip.w,sdl.clip.h);
        glMatrixMode (GL_PROJECTION);
        glDeleteTextures(1,&sdl.opengl.texture);
        glGenTextures(1,&sdl.opengl.texture);
        glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);
        // No borders
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        if (sdl.opengl.bilinear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

        glClearColor (0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(sdl.window);
        glClear(GL_COLOR_BUFFER_BIT);
        glShadeModel (GL_FLAT);
        glDisable (GL_DEPTH_TEST);
        glDisable (GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        glMatrixMode (GL_MODELVIEW);
        glLoadIdentity ();

        GLfloat tex_width=((GLfloat)(width)/(GLfloat)texsize);
        GLfloat tex_height=((GLfloat)(height)/(GLfloat)texsize);

        //if (glIsList(sdl.opengl.displaylist)) glDeleteLists(sdl.opengl.displaylist, 1);
        //sdl.opengl.displaylist = glGenLists(1);
        sdl.opengl.displaylist = 1;
        glNewList(sdl.opengl.displaylist, GL_COMPILE);
        glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
        glBegin(GL_QUADS);
        // lower left
        glTexCoord2f(0,tex_height);
        glVertex2f(-1.0f,-1.0f);
        // lower right
        glTexCoord2f(tex_width,tex_height);
        glVertex2f(1.0f, -1.0f);
        // upper right
        glTexCoord2f(tex_width,0);
        glVertex2f(1.0f, 1.0f);
        // upper left
        glTexCoord2f(0,0);
        glVertex2f(-1.0f, 1.0f);
        glEnd();
        glEndList();
        sdl.desktop.type=SCREEN_OPENGL;
        retFlags = GFX_CAN_32 | GFX_SCALING;
        if (sdl.opengl.pixel_buffer_object)
            retFlags |= GFX_HARDWARE;
        break;
    }//OPENGL
#endif	//C_OPENGL
    default:
        goto dosurface;
        break;
    }//CASE
    GFX_LogSDLState();
    if (retFlags)
        GFX_Start();
    if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
    return retFlags;
}

// WARNING: Not recommended, there is danger you cannot exit emulator because mouse+keyboard are taken
static bool enable_hook_everything = false;

// Whether or not to hook the keyboard and block special keys.
// Setting this is recommended so that your keyboard is fully usable in the guest OS when you
// enable the mouse+keyboard capture. But hooking EVERYTHING is not recommended because there is a
// danger you become trapped in the DOSBox emulator!
static bool enable_hook_special_keys = true;

// Whether or not to hook Num/Scroll/Caps lock in order to give the guest OS full control of the
// LEDs on the keyboard (i.e. the LEDs do not change until the guest OS changes their state).
// This flag also enables code to set the LEDs to guest state when setting mouse+keyboard capture,
// and restoring LED state when releasing capture.
static bool enable_hook_lock_toggle_keys = true;

// and this is where we store host LED state when capture is set.
static bool on_capture_num_lock_was_on = true; // reasonable guess
static bool on_capture_scroll_lock_was_on = false;
static bool on_capture_caps_lock_was_on = false;

static bool exthook_enabled = false;

Bitu Keyboard_Guest_LED_State();
void UpdateKeyboardLEDState(Bitu led_state/* in the same bitfield arrangement as using command 0xED on PS/2 keyboards */);

void UpdateKeyboardLEDState(Bitu led_state/* in the same bitfield arrangement as using command 0xED on PS/2 keyboards */) {
}

void DoExtendedKeyboardHook(bool enable) {
    if (exthook_enabled == enable)
        return;

}

void GFX_ReleaseMouse(void) {
	if (sdl.mouse.locked)
        GFX_CaptureMouse();
}

/* Toggles the mouse capture */
void GFX_CaptureMouse(void) {
    sdl.mouse.locked=!sdl.mouse.locked;
    if (sdl.mouse.locked) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        if (enable_hook_special_keys) DoExtendedKeyboardHook(true);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        DoExtendedKeyboardHook(false);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        if (sdl.mouse.autoenable || !sdl.mouse.autolock) {
            SDL_ShowCursor(SDL_ENABLE);
        }
    }
    mouselocked=sdl.mouse.locked;
}

static void CaptureMouse(bool pressed) {
    if (!pressed)
        return;
    GFX_CaptureMouse();
}

void GFX_UpdateSDLCaptureState(void) {
    if (sdl.mouse.locked) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        if (enable_hook_special_keys) DoExtendedKeyboardHook(true);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        DoExtendedKeyboardHook(false);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
    }
    CPU_Reset_AutoAdjust();
    GFX_SetTitle(-1,-1,-1,false);
}

void res_init(void) {
    Section * sec = control->GetSection("sdl");
    Section_prop * section=static_cast<Section_prop *>(sec);
    sdl.desktop.full.fixed=false;
    const char* fullresolution=section->Get_string("fullresolution");
    sdl.desktop.full.width  = 0;
    sdl.desktop.full.height = 0;
    if(fullresolution && *fullresolution) {
        char res[100];
        safe_strncpy( res, fullresolution, sizeof( res ));
        fullresolution = lowcase (res);//so x and X are allowed
        if (strcmp(fullresolution,"original")) {
            sdl.desktop.full.fixed = true;
            if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
                char* height = const_cast<char*>(strchr(fullresolution,'x'));
                if(height && * height) {
                    *height = 0;
                    sdl.desktop.full.height = atoi(height+1);
                    sdl.desktop.full.width  = atoi(res);
                }
            }
        }
    }

    sdl.desktop.window.width  = 0;
    sdl.desktop.window.height = 0;
    const char* windowresolution=section->Get_string("windowresolution");
    if(windowresolution && *windowresolution) {
        //if(sdl.desktop.type==SCREEN_SURFACE) return;
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if(strcmp(windowresolution,"original")) {
            char* height = const_cast<char*>(strchr(windowresolution,'x'));
            if(height && *height) {
                *height = 0;
                sdl.desktop.window.height = (Bit16u)atoi(height+1);
                sdl.desktop.window.width  = (Bit16u)atoi(res);
            }
        }
    }
    sdl.desktop.vsync=section->Get_bool("vsync");

    int width = 1024;
    int height = 768;

    // fullresolution == desktop -> get/set desktop size
    auto sdlSection = control->GetSection("sdl");
    auto sdlSectionProp = static_cast<Section_prop*>(sdlSection);
    auto fullRes = sdlSectionProp->Get_string("fullresolution");
//    if (!strcmp(fullRes, "desktop")) GetDesktopResolution(&width, &height);

    if (!sdl.desktop.full.width) {
        sdl.desktop.full.width=width;
    }
    if (!sdl.desktop.full.height) {
        sdl.desktop.full.height=height;
    }
    if(sdl.desktop.type==SCREEN_SURFACE && !GFX_IsFullscreen()) return;
    else {
        GFX_Stop();
        if (sdl.draw.callback)
            (sdl.draw.callback)( GFX_CallBackReset );
        GFX_Start();
    }
}

void res_input(bool type, const char * res) {
    Section* sec = control->GetSection("sdl");
    char win_res[11];
    if(sec) {
        strcpy(win_res,res);
        if(type) {
            std::string tmp("windowresolution=");
            tmp.append(win_res);
            sec->HandleInputline(tmp);
        } else {
            std::string tmp("fullresolution=");
            tmp.append(win_res);
            sec->HandleInputline(tmp);
        }

        res_init();
    }
}

void change_output(int output) {
    GFX_Stop();
    Section * sec = control->GetSection("sdl");
    Section_prop * section=static_cast<Section_prop *>(sec);
    sdl.overscan_width=section->Get_int("overscan");
    switch (output) {
    case 0:
        sdl.desktop.want_type=SCREEN_SURFACE;
        break;
    case 1:
        sdl.desktop.want_type=SCREEN_SURFACE;
        break;
    case 2:
        break;
    case 3:
        change_output(2);
        sdl.desktop.want_type=SCREEN_OPENGL;
        break;
    case 4:
        change_output(2);
        sdl.desktop.want_type=SCREEN_OPENGL;
        break;
    case 6:
        break;
    case 7:
        // do not set want_type
        break;
    case 8:
        if(sdl.desktop.want_type==SCREEN_OPENGL) { }
        break;
    default:
        LOG_MSG("SDL:Unsupported output device %d, switching back to surface",output);
        sdl.desktop.want_type=SCREEN_SURFACE;
        break;
    }
    const char* windowresolution=section->Get_string("windowresolution");
    if(windowresolution && *windowresolution) {
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if(strcmp(windowresolution,"original")) {
            if(output == 0) {
                std::string tmp("windowresolution=original");
                sec->HandleInputline(tmp);
            }
        }
    }
    res_init();

    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
    GFX_SetTitle(CPU_CycleMax,-1,-1,false);
    GFX_LogSDLState();
}


void GFX_SwitchFullScreen(void)
{
    menu.resizeusing = true;

    if (GFX_IsFullscreen()) {
        SDL_SetWindowFullscreen(sdl.window, 0);
        LOG_MSG("INFO: switched to fullscreen mode");
    } else {
        SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        LOG_MSG("INFO: switched to window mode");
    }

    // (re-)assign menu to window
    if (GFX_IsFullscreen() && menu.gui) SetMenu(GetHWND(), nullptr);

    // ensure mouse capture when fullscreen || (re-)capture if user said so when windowed
    auto locked = sdl.mouse.locked;
    if (GFX_IsFullscreen() && !locked || !GFX_IsFullscreen() && locked) GFX_CaptureMouse();


    GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed) {
    if (!pressed)
        return;

    GFX_LosingFocus();
    if (sdl.desktop.lazy_fullscreen) {
//		sdl.desktop.lazy_fullscreen_req=true;
        LOG_MSG("GFX LF: fullscreen switching not supported");
    } else {
        GFX_SwitchFullScreen();
    }
}

void GFX_SwitchLazyFullscreen(bool lazy) {
    sdl.desktop.lazy_fullscreen=lazy;
    sdl.desktop.lazy_fullscreen_req=false;
}

void GFX_SwitchFullscreenNoReset(void) {
    if (GFX_IsFullscreen()) {
        SDL_SetWindowFullscreen(sdl.window, 0);
        LOG_MSG("INFO: switched to fullscreen mode with no graphics reset");
    } else {
        SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        LOG_MSG("INFO: switched to window mode with no graphics reset");
    }
}

bool GFX_LazyFullscreenRequested(void) {
    if (sdl.desktop.lazy_fullscreen) return sdl.desktop.lazy_fullscreen_req;
    return false;
}

void GFX_RestoreMode(void) {
    if (!sdl.draw.callback) return;
    GFX_SetSize(sdl.draw.width,sdl.draw.height,sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
    GFX_UpdateSDLCaptureState();
}

bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
    if (!sdl.update_display_contents)
        return false;
    if (!sdl.active || sdl.updating)
        return false;
    switch (sdl.desktop.type) {
    case SCREEN_SURFACE:
        pixels=(Bit8u *)sdl.surface->pixels;
        pixels+=sdl.clip.y*sdl.surface->pitch;
        pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
        pitch=sdl.surface->pitch;
        sdl.updating=true;
        return true;
    case SCREEN_TEXTURE:
        void * texPixels;
        int texPitch;
        if (SDL_LockTexture(sdl.texture.texture, NULL, &texPixels, &texPitch) < 0)
            return false;
        pixels = (Bit8u *)texPixels;
        pitch = texPitch;
        SDL_Overscan();
        sdl.updating=true;
        return true;
#if C_OPENGL
    case SCREEN_OPENGL:
        if(sdl.opengl.pixel_buffer_object) {
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
            pixels=(Bit8u *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
        } else {
            pixels=(Bit8u *)sdl.opengl.framebuf;
        }
        pitch=sdl.opengl.pitch;
        sdl.updating=true;
        return true;
#endif
    default:
        break;
    }
    return false;
}

void GFX_EndUpdate( const Bit16u *changedLines ) {
    if (!sdl.update_display_contents)
        return;
    if (!sdl.updating)
        return;
    sdl.updating=false;
    switch (sdl.desktop.type) {
    case SCREEN_SURFACE:
        if (changedLines) {
            if(changedLines[0] == sdl.draw.height)
                return;
            if(!menu.hidecycles && !GFX_IsFullscreen()) frames++;
            Bitu y = 0, index = 0, rectCount = 0;
            while (y < sdl.draw.height) {
                if (!(index & 1)) {
                    y += changedLines[index];
                } else {
                    SDL_Rect *rect = &sdl.updateRects[rectCount++];
                    rect->x = sdl.clip.x;
                    rect->y = sdl.clip.y + y;
                    rect->w = (Bit16u)sdl.draw.width;
                    rect->h = changedLines[index];
                    y += changedLines[index];
                }
                index++;
            }
            if (rectCount)
                SDL_UpdateWindowSurfaceRects( sdl.window, sdl.updateRects, rectCount );
        }
        break;
    case SCREEN_TEXTURE:
        SDL_UnlockTexture(sdl.texture.texture);
        SDL_RenderClear(sdl.renderer);
        SDL_RenderCopy(sdl.renderer, sdl.texture.texture, NULL, &sdl.clip);
        SDL_RenderPresent(sdl.renderer);
        break;
#if C_OPENGL
    case SCREEN_OPENGL:
        if (sdl.opengl.pixel_buffer_object) {
            if(changedLines && (changedLines[0] == sdl.draw.height))
                return;
            glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
            glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            sdl.draw.width, sdl.draw.height, GL_BGRA_EXT,
                            GL_UNSIGNED_INT_8_8_8_8_REV, 0);
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
            glCallList(sdl.opengl.displaylist);
            SDL_GL_SwapWindow(sdl.window);
        } else if (changedLines) {
            if(changedLines[0] == sdl.draw.height)
                return;
            Bitu y = 0, index = 0;
            glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
            while (y < sdl.draw.height) {
                if (!(index & 1)) {
                    y += changedLines[index];
                } else {
                    Bit8u *pixels = (Bit8u *)sdl.opengl.framebuf + y * sdl.opengl.pitch;
                    Bitu height = changedLines[index];
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
                                    sdl.draw.width, height, GL_BGRA_EXT,
                                    GL_UNSIGNED_INT_8_8_8_8_REV, pixels );
                    y += height;
                }
                index++;
            }
            glCallList(sdl.opengl.displaylist);
            SDL_GL_SwapWindow(sdl.window);
        }
        if(!menu.hidecycles && !GFX_IsFullscreen()) frames++;
        SDL_GL_SwapWindow(sdl.window);
        break;
#endif
    default:
        break;
    }
}

/* Paletted window surfaced are not longer supported as of SDL2. */
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
    switch (sdl.desktop.type) {
    case SCREEN_SURFACE:
        return SDL_MapRGB(sdl.surface->format,red,green,blue);
    case SCREEN_TEXTURE:
        return SDL_MapRGB(sdl.texture.pixelFormat,red,green,blue);
    case SCREEN_OPENGL:
        //USE BGRA otherwise
        return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
    default:
        break;
    }
    return 0;
}

void GFX_Stop() {
    if (sdl.updating)
        GFX_EndUpdate( 0 );
    sdl.active=false;
}

void GFX_Start() {
    sdl.active=true;
}

/* NOTE: The following appears to do its job on Android only *before*
 * a screen rotation occurs. After that, the old dimensions are retrieved.
 * For the updated dimensions we should listen to a window resize event.
 */
void GFX_ObtainDisplayDimensions() {
    SDL_Rect displayDimensions;
    SDL_GetDisplayBounds(sdl.displayNumber, &displayDimensions);
    sdl.desktop.full.width = displayDimensions.w;
    sdl.desktop.full.height = displayDimensions.h;
}

/* Manually update display dimensions in case of a window resize,
 * IF there is the need for that ("yes" on Android, "no" otherwise).
 * Used for the mapper UI on Android.
 * Reason is the usage of GFX_GetSDLSurfaceSubwindowDims, as well as a
 * mere notification of the fact that the window's dimensions are modified.
 */
void GFX_UpdateDisplayDimensions(int width, int height) {
    if (sdl.desktop.full.display_res && GFX_IsFullscreen()) {
        /* Note: We should not use GFX_ObtainDisplayDimensions
        (SDL_GetDisplayBounds) on Android after a screen rotation:
        The older values from application startup are returned. */
        sdl.desktop.full.width = width;
        sdl.desktop.full.height = height;
    }
}

static void GUI_ShutDown(Section * /*sec*/) {
    GFX_Stop();
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
    if (sdl.mouse.locked) GFX_CaptureMouse();
    if (GFX_IsFullscreen()) GFX_SwitchFullScreen();
}

static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest
// priority can not be set as users can only lower priority (not restore it)

    if((sdl.priority.focus != sdl.priority.nofocus ) &&
            (getuid()!=0) ) return;

#endif
    switch (level) {
#if C_SET_PRIORITY
    /* Linux use group as dosbox has mulitple threads under linux */
    case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
    case PRIORITY_LEVEL_LOWEST:
        setpriority (PRIO_PGRP, 0,PRIO_MAX);
        break;
    case PRIORITY_LEVEL_LOWER:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
        break;
    case PRIORITY_LEVEL_NORMAL:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
        break;
    case PRIORITY_LEVEL_HIGHER:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
        break;
    case PRIORITY_LEVEL_HIGHEST:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
        break;
#endif
    default:
        break;
    }
}

extern Bit8u int10_font_14[256 * 14];

static void OutputString(Bitu x,Bitu y,const char * text,Bit32u color,Bit32u color2,SDL_Surface * output_surface) {
    Bit32u * draw=(Bit32u*)(((Bit8u *)output_surface->pixels)+((y)*output_surface->pitch))+x;
    while (*text) {
        Bit8u * font=&int10_font_14[(*text)*14];
        Bitu i,j;
        Bit32u * draw_line=draw;
        for (i=0; i<14; i++) {
            Bit8u map=*font++;
            for (j=0; j<8; j++) {
                if (map & 0x80) *((Bit32u*)(draw_line+j))=color;
                else *((Bit32u*)(draw_line+j))=color2;
                map<<=1;
            }
            draw_line+=output_surface->pitch/4;
        }
        text++;
        draw+=8;
    }
}

#include "dosbox_splash.h"

/* The endian part is intentionally disabled as somehow it produces correct results without according to rhoenie*/
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//    Bit32u rmask = 0xff000000;
//    Bit32u gmask = 0x00ff0000;
//    Bit32u bmask = 0x0000ff00;
//#else
Bit32u rmask = 0x000000ff;
Bit32u gmask = 0x0000ff00;
Bit32u bmask = 0x00ff0000;
//#endif

static SDL_Surface* 	splash_surf;
static bool		splash_active;
static Bit8u*		splash_tmpbuf;
static Bit32u		splash_startticks;

static void ShowSplashScreen() {
    splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
    if (splash_surf) {
        splash_active=true;
        SDL_FillRect(splash_surf, NULL, SDL_MapRGB(splash_surf->format, 0, 0, 0));
        splash_tmpbuf = new Bit8u[640*400*3];
        GIMP_IMAGE_RUN_LENGTH_DECODE(splash_tmpbuf,gimp_image.rle_pixel_data,640*400,3);
        for (Bitu y=0; y<400; y++) {

            Bit8u* tmpbuf = splash_tmpbuf + y*640*3;
            Bit32u * draw=(Bit32u*)(((Bit8u *)splash_surf->pixels)+((y)*splash_surf->pitch));
            for (Bitu x=0; x<640; x++) {
                *draw++ = tmpbuf[x*3+0]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+2]*0x10000+0x00000000;
            }
        }
        Bit32u lasttick=GetTicks();
        for(Bitu i = 0; i <=5; i++) {
            if((GetTicks()-lasttick)>20) i++;
            while((GetTicks()-lasttick)<15) SDL_Delay(5);
            lasttick = GetTicks();
            SDL_SetSurfaceAlphaMod(splash_surf, (Bit8u)(51*i));
            SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
            SDL_UpdateWindowSurface(sdl.window);
        }

        splash_startticks=GetTicks();
    } else {
        splash_active=false;
        splash_startticks=0;

    }
}

void EndSplashScreen() {
    if(!splash_active) return;
    //SDL_FillRect(splash_surf, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
    //SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
    //SDL_Flip(sdl.surface);
    while((GetTicks()-splash_startticks)< 500) SDL_Delay(10);

    SDL_FreeSurface(splash_surf);
    delete [] splash_tmpbuf;
    splash_active=false;
}

bool has_GUI_StartUp = false;

static void GUI_StartUp() {
    if (has_GUI_StartUp) return;
    has_GUI_StartUp = true;

    LOG(LOG_GUI,LOG_DEBUG)("Starting GUI");

    AddExitFunction(AddExitFunctionFuncPair(GUI_ShutDown));

    sdl.active=false;
    sdl.updating=false;
    sdl.update_window=true;
    sdl.update_display_contents=true;

    GFX_SetIcon();

    sdl.desktop.lazy_fullscreen=false;
    sdl.desktop.lazy_fullscreen_req=false;

    Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl"));
    assert(section != NULL);

    if (section->Get_bool("fullscreen")) {
        SDL_SetWindowFullscreen(sdl.window, 0);
    } else {
        SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    sdl.wait_on_error=section->Get_bool("waitonerror");

    Prop_multival* p=section->Get_multival("priority");
    std::string focus = p->GetSection()->Get_string("active");
    std::string notfocus = p->GetSection()->Get_string("inactive");

    if      (focus == "lowest")  {
        sdl.priority.focus = PRIORITY_LEVEL_LOWEST;
    }
    else if (focus == "lower")   {
        sdl.priority.focus = PRIORITY_LEVEL_LOWER;
    }
    else if (focus == "normal")  {
        sdl.priority.focus = PRIORITY_LEVEL_NORMAL;
    }
    else if (focus == "higher")  {
        sdl.priority.focus = PRIORITY_LEVEL_HIGHER;
    }
    else if (focus == "highest") {
        sdl.priority.focus = PRIORITY_LEVEL_HIGHEST;
    }

    if      (notfocus == "lowest")  {
        sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;
    }
    else if (notfocus == "lower")   {
        sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;
    }
    else if (notfocus == "normal")  {
        sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
    }
    else if (notfocus == "higher")  {
        sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;
    }
    else if (notfocus == "highest") {
        sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST;
    }
    else if (notfocus == "pause")   {
        /* we only check for pause here, because it makes no sense
         * for DOSBox to be paused while it has focus
         */
        sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
    }

    SetPriority(sdl.priority.focus); //Assume focus on startup
    sdl.mouse.locked=false;
    mouselocked=false; //Global for mapper
    sdl.mouse.requestlock=false;
    sdl.desktop.full.fixed=false;
    const char* fullresolution=section->Get_string("fullresolution");
    sdl.desktop.full.width  = 0;
    sdl.desktop.full.height = 0;
    if(fullresolution && *fullresolution) {
        char res[100];
        safe_strncpy( res, fullresolution, sizeof( res ));
        fullresolution = lowcase (res);//so x and X are allowed
        if (strcmp(fullresolution,"original")) {
            sdl.desktop.full.fixed = true;
            if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
                char* height = const_cast<char*>(strchr(fullresolution,'x'));
                if (height && * height) {
                    *height = 0;
                    sdl.desktop.full.height = (Bit16u)atoi(height+1);
                    sdl.desktop.full.width  = (Bit16u)atoi(res);
                }
            }
        }
    }
    sdl.desktop.window.width  = 0;
    sdl.desktop.window.height = 0;
    const char* windowresolution=section->Get_string("windowresolution");
    if(windowresolution && *windowresolution) {
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if(strcmp(windowresolution,"original")) {
            char* height = const_cast<char*>(strchr(windowresolution,'x'));
            if(height && *height) {
                *height = 0;
                sdl.desktop.window.height = (Bit16u)atoi(height+1);
                sdl.desktop.window.width  = (Bit16u)atoi(res);
            }
        }
    }

    sdl.desktop.vsync=section->Get_bool("vsync");

    sdl.displayNumber=section->Get_int("display");
    if ((sdl.displayNumber < 0) || (sdl.displayNumber >= SDL_GetNumVideoDisplays())) {
        sdl.displayNumber = 0;
        LOG_MSG("SDL:Display number out of bounds, switching back to 0");
    }
    sdl.desktop.full.display_res = sdl.desktop.full.fixed && (!sdl.desktop.full.width || !sdl.desktop.full.height);
    if (sdl.desktop.full.display_res) {
        GFX_ObtainDisplayDimensions();
    }

    int width=1024;// int height=768;
    if (!sdl.desktop.full.width) {
        sdl.desktop.full.width=width;
    }
    if (!sdl.desktop.full.height) {
        sdl.desktop.full.height=width;
    }
    sdl.mouse.autoenable=section->Get_bool("autolock");
    if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
    sdl.mouse.autolock=false;
    sdl.mouse.sensitivity=section->Get_int("sensitivity");
    std::string output=section->Get_string("output");

    /* Setup Mouse correctly if fullscreen */
    if(GFX_IsFullscreen()) GFX_CaptureMouse();

    if (output == "surface") {
        sdl.desktop.want_type=SCREEN_SURFACE;
    } else if (output == "texture") {
        sdl.desktop.want_type=SCREEN_TEXTURE;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    } else if (output == "texturenb") {
        sdl.desktop.want_type=SCREEN_TEXTURE;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#if C_OPENGL
    } else if (output == "opengl") {
        sdl.desktop.want_type=SCREEN_OPENGL;
        sdl.opengl.bilinear=true;
    } else if (output == "openglnb") {
        sdl.desktop.want_type=SCREEN_OPENGL;
        sdl.opengl.bilinear=false;
#endif
    } else {
        LOG_MSG("SDL: Unsupported output device %s, switching back to surface",output.c_str());
        sdl.desktop.want_type=SCREEN_SURFACE;//SHOULDN'T BE POSSIBLE anymore
    }
    sdl.overscan_width=section->Get_int("overscan");
    sdl.texture.texture=0;
    sdl.texture.pixelFormat=0;
    sdl.window=0;
    sdl.renderer=0;
    sdl.rendererDriver = section->Get_string("texture.renderer");

    if (!sdl.desktop.full.width || !sdl.desktop.full.height) {
        SDL_Rect displayDimensions;
        SDL_GetDisplayBounds(sdl.displayNumber, &displayDimensions);
        sdl.desktop.full.width = displayDimensions.w;
        sdl.desktop.full.height = displayDimensions.h;
    }
#if C_OPENGL
    if(sdl.desktop.want_type==SCREEN_OPENGL) { /* OPENGL is requested */
        if (!GFX_SetSDLOpenGLWindow(640,400)) {
            LOG_MSG("Could not create OpenGL window, switching back to surface");
            sdl.desktop.want_type=SCREEN_SURFACE;
        } else {
            sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
            if (sdl.opengl.context == 0) {
                LOG_MSG("Could not create OpenGL context, switching back to surface");
                sdl.desktop.want_type=SCREEN_SURFACE;
            }
        }
        if (sdl.desktop.want_type==SCREEN_OPENGL) {
            sdl.opengl.buffer=0;
            sdl.opengl.framebuf=0;
            sdl.opengl.texture=0;
            sdl.opengl.displaylist=0;

            glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
            glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)SDL_GL_GetProcAddress("glGenBuffersARB");
            glBindBufferARB = (PFNGLBINDBUFFERARBPROC)SDL_GL_GetProcAddress("glBindBufferARB");
            glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)SDL_GL_GetProcAddress("glDeleteBuffersARB");
            glBufferDataARB = (PFNGLBUFFERDATAARBPROC)SDL_GL_GetProcAddress("glBufferDataARB");
            glMapBufferARB = (PFNGLMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glMapBufferARB");
            glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glUnmapBufferARB");
            const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
            if(gl_ext && *gl_ext) {
                sdl.opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") > 0);
                sdl.opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") > 0);
                sdl.opengl.pixel_buffer_object=(strstr(gl_ext,"GL_ARB_pixel_buffer_object") >0 ) &&
                                               glGenBuffersARB && glBindBufferARB && glDeleteBuffersARB && glBufferDataARB &&
                                               glMapBufferARB && glUnmapBufferARB;
            } else {
                sdl.opengl.packed_pixel=sdl.opengl.paletted_texture=false;
            }
        }
    } /* OPENGL is requested end */

#endif	//OPENGL
    /* Initialize screen for first time */
    if (!GFX_SetSDLSurfaceWindow(640,400))
        E_Exit("Could not initialize video: %s",SDL_GetError());
    sdl.surface = SDL_GetWindowSurface(sdl.window);
    SDL_Rect splash_rect=GFX_GetSDLSurfaceSubwindowDims(640,400);
    sdl.desktop.pixelFormat = SDL_GetWindowPixelFormat(sdl.window);
    LOG_MSG("SDL:Current window pixel format: %s", SDL_GetPixelFormatName(sdl.desktop.pixelFormat));
    sdl.desktop.bpp=8*SDL_BYTESPERPIXEL(sdl.desktop.pixelFormat);
    if (SDL_BITSPERPIXEL(sdl.desktop.pixelFormat) == 24) {
        LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
    }
    if (sdl.desktop.bpp==24) {
        LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
    }
    GFX_LogSDLState();
    GFX_Stop();
    SDL_SetWindowTitle(sdl.window,"DOSBox");

//* The endian part is intentionally disabled as somehow it produces correct results without according to rhoenie*/
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//    Bit32u rmask = 0xff000000;
//    Bit32u gmask = 0x00ff0000;
//    Bit32u bmask = 0x0000ff00;
//#else
    Bit32u rmask = 0x000000ff;
    Bit32u gmask = 0x0000ff00;
    Bit32u bmask = 0x00ff0000;
//#endif

    /* Please leave the Splash screen stuff in working order in DOSBox. We spend a lot of time making DOSBox. */
    //ShowSplashScreen();	/* I will keep the splash screen alive. But now, the BIOS will do it --J.C. */
    /* Get some Event handlers */
    MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
    MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
    MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
    MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
#if C_DEBUG
    /* Pause binds with activate-debugger */
    MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD1, "pause", "Pause");
#else
    MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause");
#endif
//    MAPPER_AddHandler(&GUI_Run, MK_f10, MMOD2, "gui", "ShowGUI");
    /* Get Keyboard state of numlock and capslock */
    SDL_Keymod keystate = SDL_GetModState();
    if(keystate&KMOD_NUM) startup_state_numlock = true;
    if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

void Mouse_AutoLock(bool enable) {
    sdl.mouse.autolock=enable;
    if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
    else {
        SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
        sdl.mouse.requestlock=false;
    }
}

static void RedrawScreen(Bit32u nWidth, Bit32u nHeight) {
    int width;
    int height;
    width=sdl.draw.width;
    height=sdl.draw.height;
    void RENDER_CallBack( GFX_CallBackFunctions_t function );
    while (GFX_IsFullscreen()) {
        int temp_size;
        temp_size=render.scale.size;
        if(!GFX_IsFullscreen()) {
            render.scale.size=temp_size;
            RENDER_CallBack( GFX_CallBackReset);
            return;
        }
    }
    if((Bitu)nWidth == (Bitu)width && (Bitu)nHeight == (Bitu)height) {
        RENDER_CallBack( GFX_CallBackReset);
        return;
    }
    Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl"));
    if ((!strcmp(section->Get_string("windowresolution"),"original") || (!strcmp(section->Get_string("windowresolution"),"desktop"))) && (render.src.dblw && render.src.dblh)) {
        switch (render.scale.op) {
        case scalerOpNormal:
            if(!render.scale.hardware) {
                if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) {
                    if (render.scale.size <= 4 && render.scale.size >=1) ++render.scale.size;
                    break;
                } else {
                    if (render.scale.size <= 5 && render.scale.size >= 2) --render.scale.size;
                    break;
                }
            } else {
                if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) {
                    if (render.scale.size == 1) {
                        render.scale.size=4;
                        break;
                    }
                    if (render.scale.size == 4) {
                        render.scale.size=6;
                        break;
                    }
                    if (render.scale.size == 6) {
                        render.scale.size=8;
                        break;
                    }
                    if (render.scale.size == 8) {
                        render.scale.size=10;
                        break;
                    }
                }
                if((Bitu)nWidth<(Bitu)width || (Bitu)nHeight<(Bitu)height) {
                    if (render.scale.size == 10) {
                        render.scale.size=8;
                        break;
                    }
                    if (render.scale.size == 8) {
                        render.scale.size=6;
                        break;
                    }
                    if (render.scale.size == 6) {
                        render.scale.size=4;
                        break;
                    }
                    if (render.scale.size == 4) {
                        render.scale.size=1;
                        break;
                    }
                }
            }
            break;
        case scalerOpAdvMame:
        case scalerOpHQ:
        case scalerOpAdvInterp:
        case scalerOpTV:
        case scalerOpRGB:
        case scalerOpScan:
            if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) {
                if (render.scale.size == 2) ++render.scale.size;
            }
            if((Bitu)nWidth<(Bitu)width || (Bitu)nHeight<(Bitu)height) {
                if (render.scale.size == 3) --render.scale.size;
            }
            break;
        case scalerOpSaI:
        case scalerOpSuperSaI:
        case scalerOpSuperEagle:
        default: // other scalers
            break;
        }
    }
    RENDER_CallBack( GFX_CallBackReset);
}

extern unsigned int mouse_notify_mode;

bool user_cursor_locked = false;
int user_cursor_x = 0,user_cursor_y = 0;
int user_cursor_sw = 640,user_cursor_sh = 480;

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
    user_cursor_x = motion->x - sdl.clip.x;
    user_cursor_y = motion->y - sdl.clip.y;
    user_cursor_locked = sdl.mouse.locked;
    user_cursor_sw = sdl.clip.w;
    user_cursor_sh = sdl.clip.h;

	if (sdl.mouse.locked || !sdl.mouse.autoenable)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.sensitivity/100.0f,
						  sdl.mouse.locked);
    else if (mouse_notify_mode != 0) { /* for mouse integration driver */
		Mouse_CursorMoved(0,0,0,0,sdl.mouse.locked);
		SDL_ShowCursor(SDL_DISABLE); /* TODO: If guest has not read mouse cursor position within 250ms show cursor again */
    }
    else {
		SDL_ShowCursor(SDL_ENABLE);
    }
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked && mouse_notify_mode == 0) {
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && mouse_notify_mode == 0 && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
			break;
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
#if 0
		case SDL_BUTTON_WHEELUP: /* Ick, really SDL? */
			Mouse_ButtonPressed(100-1);
			break;
		case SDL_BUTTON_WHEELDOWN: /* Ick, really SDL? */
			Mouse_ButtonPressed(100+1);
			break;
#endif
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
#if 0
		case SDL_BUTTON_WHEELUP: /* Ick, really SDL? */
			Mouse_ButtonReleased(100-1);
			break;
		case SDL_BUTTON_WHEELDOWN: /* Ick, really SDL? */
			Mouse_ButtonReleased(100+1);
			break;
#endif
		}
		break;
	}
}

void GFX_LosingFocus(void) {
    sdl.laltstate=SDL_KEYUP;
    sdl.raltstate=SDL_KEYUP;
    MAPPER_LosingFocus();
    DoExtendedKeyboardHook(false);
}

bool GFX_IsFullscreen() {
    uint32_t windowFlags = SDL_GetWindowFlags(sdl.window);
    if (windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) return true;
    return false;
}

static bool IsFullscreen() {
    return GFX_IsFullscreen();
}

void GFX_HandleVideoResize(int width, int height) {
    /* Maybe a screen rotation has just occurred, so we simply resize.
    There may be a different cause for a forced resized, though.    */
    if (sdl.desktop.full.display_res && IsFullscreen()) {
        /* Note: We should not use GFX_ObtainDisplayDimensions
        (SDL_GetDisplayBounds) on Android after a screen rotation:
        The older values from application startup are returned. */
        sdl.desktop.full.width = width;
        sdl.desktop.full.height = height;
    }
    /* Even if the new window's dimensions are actually the desired ones
     * we may still need to re-obtain a new window surface or do
     * a different thing. So we basically call GFX_SetSize, but without
     * touching the window itself (or else we may end in an infinite loop).
     *
     * Furthermore, if the new dimensions are *not* the desired ones, we
     * don't fight it. Rather than attempting to resize it back, we simply
     * keep the window as-is and disable screen updates. This is done
     * in SDL_SetSDLWindowSurface by setting sdl.update_display_contents
     * to false.
     */
    sdl.update_window = false;
    GFX_ResetScreen();
    sdl.update_window = true;
}

void GFX_Events() {
    SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
    static int poll_delay=0;
    int time=GetTicks();
    if (time-poll_delay>20) {
        poll_delay=time;
        if (sdl.num_joysticks>0) SDL_JoystickUpdate();
        MAPPER_UpdateJoysticks();
    }
#endif
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESTORED:
                GFX_ResetScreen();
                continue;
            case SDL_WINDOWEVENT_RESIZED:
                GFX_HandleVideoResize(event.window.data1, event.window.data2);
                continue;
            case SDL_WINDOWEVENT_EXPOSED:
                if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
                continue;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (IsFullscreen() && !sdl.mouse.locked)
                    GFX_CaptureMouse();
                SetPriority(sdl.priority.focus);
                CPU_Disable_SkipAutoAdjust();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                if (sdl.mouse.locked) {
                    GFX_CaptureMouse();
                }
                SetPriority(sdl.priority.nofocus);
                GFX_LosingFocus();
                CPU_Enable_SkipAutoAdjust();
                break;
            default:
                ;
            }

            /* Non-focus priority is set to pause; check to see if we've lost window or input focus
             * i.e. has the window been minimised or made inactive?
             */
            if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
                if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
                    /* Window has lost focus, pause the emulator.
                     * This is similar to what PauseDOSBox() does, but the exit criteria is different.
                     * Instead of waiting for the user to hit Alt-Break, we wait for the window to
                     * regain window or input focus.
                     */
                    bool paused = true;
                    SDL_Event ev;

                    GFX_SetTitle(-1,-1,-1,true);
                    KEYBOARD_ClrBuffer();
//					SDL_Delay(500);
//					while (SDL_PollEvent(&ev)) {
                    // flush event queue.
//					}

                    while (paused) {
                        // WaitEvent waits for an event rather than polling, so CPU usage drops to zero
                        SDL_WaitEvent(&ev);

                        switch (ev.type) {
                        case SDL_QUIT:
                            throw(0);
                            break; // a bit redundant at linux at least as the active events gets before the quit event.
                        case SDL_WINDOWEVENT:     // wait until we get window focus back
                            if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) || (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                                // We've got focus back, so unpause and break out of the loop
                                if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                                    paused = false;
                                    GFX_SetTitle(-1,-1,-1,false);
                                }

                                /* Now poke a "release ALT" command into the keyboard buffer
                                 * we have to do this, otherwise ALT will 'stick' and cause
                                 * problems with the app running in the DOSBox.
                                 */
                                KEYBOARD_AddKey(KBD_leftalt, false);
                                KEYBOARD_AddKey(KBD_rightalt, false);
                                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                                    // We may need to re-create a texture and more
                                    GFX_ResetScreen();
                                }
                            }
                            break;
                        }
                    }
                }
            }
            break;
        case SDL_MOUSEMOTION:
            HandleMouseMotion(&event.motion);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            HandleMouseButton(&event.button);
            break;
        case SDL_QUIT:
            throw(0);
            break;
#if defined (MACOSX)
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            /* On macs CMD-Q is the default key to close an application */
            if (event.key.keysym.sym == SDLK_q &&
                    (event.key.keysym.mod == KMOD_RGUI ||
                     event.key.keysym.mod == KMOD_LGUI)
               ) {
                KillSwitch(true);
                break;
            }
#endif
        default:
            void MAPPER_CheckEvent(SDL_Event * event);
            MAPPER_CheckEvent(&event);
        }
    }
}

void Null_Init(Section *sec);

void SDL_SetupConfigSection() {
    Section_prop * sdl_sec=control->AddSection_prop("sdl",&Null_Init);

    Prop_bool* Pbool;
    Prop_string* Pstring;
    Prop_int* Pint;
    Prop_multival* Pmulti;

    Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,false);
    Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

    Pbool = sdl_sec->Add_bool("vsync",Property::Changeable::Always,false);
    Pbool->Set_help("Sync to Vblank IF supported by the output device and renderer (if relevant).\n"
                    "It can reduce screen flickering, but it can also result in a slow DOSBox.");
    //Pbool = sdl_sec->Add_bool("sdlresize",Property::Changeable::Always,false);
    //Pbool->Set_help("Makes window resizable (depends on scalers)");

    Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"0x0");
    Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
                      "  Using your monitor's native resolution with aspect=true might give the best results.\n"
                      "  If you end up with small window on a large screen, try an output different from surface.");

    Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"original");
    Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
                      "  (output=surface does not!)");

    const char* outputs[] = {
        "surface",
        "texture",
        "texturenb",
#if C_OPENGL
        "opengl", "openglnb",
#endif
        0
    };
    Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"surface");
    Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"texture");
    Pstring->Set_help("What video system to use for output.");
    Pstring->Set_values(outputs);

    const char* renderers[] = {
        "auto",
        "opengl",
        "software",
        0
    };
    Pstring = sdl_sec->Add_string("texture.renderer",Property::Changeable::Always,"auto");
    Pstring->Set_help("Choose a renderer driver if output=texture or output=texturenb. Use output=auto for an automatic choice.");
    Pstring->Set_values(renderers);

    Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
    Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

    Pint = sdl_sec->Add_int("sensitivity",Property::Changeable::Always,100);
    Pint->SetMinMax(1,1000);
    Pint->Set_help("Mouse sensitivity.");

    Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
    Pbool->Set_help("Wait before closing the console if dosbox has an error.");

    Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
    Pmulti->SetValue("higher,normal",/*init*/true);
    Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
                     "  pause is only valid for the second entry.");

    const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
    Pstring = Pmulti->GetSection()->Add_string("active",Property::Changeable::Always,"higher");
    Pstring->Set_values(actt);

    const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
    Pstring = Pmulti->GetSection()->Add_string("inactive",Property::Changeable::Always,"normal");
    Pstring->Set_values(inactt);

    Pstring = sdl_sec->Add_path("mapperfile",Property::Changeable::Always,MAPPERFILE);
    Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");

    Pint = sdl_sec->Add_int("overscan",Property::Changeable::Always, 0);
    Pint->SetMinMax(0,10);
    Pint->Set_help("Width of overscan border (0 to 10). (works only if output=surface)");

//	Pint = sdl_sec->Add_int("overscancolor",Property::Changeable::Always, 0);
//	Pint->SetMinMax(0,1000);
//	Pint->Set_help("Value of overscan color.");
}

static void show_warning(char const * const message) {
    bool textonly = true;
    LOG_MSG( "Warning: %s", message);
    if(textonly) return;
    if (!sdl.window)
        if (!GFX_SetSDLSurfaceWindow(640,400)) return;
    sdl.surface = SDL_GetWindowSurface(sdl.window);
    if(!sdl.surface) return;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Bit32u rmask = 0xff000000;
    Bit32u gmask = 0x00ff0000;
    Bit32u bmask = 0x0000ff00;
#else
    Bit32u rmask = 0x000000ff;
    Bit32u gmask = 0x0000ff00;
    Bit32u bmask = 0x00ff0000;
#endif
    SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
    if (!splash_surf) return;

    int x = 120,y = 20;
    std::string m(message),m2;
    std::string::size_type a,b,c,d;

    while(m.size()) { //Max 50 characters. break on space before or on a newline
        c = m.find('\n');
        d = m.rfind(' ',50);
        if(c>d) a=b=d;
        else a=b=c;
        if( a != std::string::npos) b++;
        m2 = m.substr(0,a);
        m.erase(0,b);
        OutputString(x,y,m2.c_str(),0xffffffff,0,splash_surf);
        y += 20;
    }

    SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
    SDL_UpdateWindowSurface(sdl.window);
    SDL_Delay(12000);
}

static void launcheditor(std::string edit) {
    std::string path,file;
    Cross::CreatePlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
    FILE* f = fopen(path.c_str(),"r");
    if(!f && !control->PrintConfig(path.c_str())) {
        printf("tried creating %s. but failed.\n",path.c_str());
        exit(1);
    }
    if(f) fclose(f);

    execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);

    //if you get here the launching failed!
    printf("can't find editor(s) specified at the command line.\n");
    exit(1);
}
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void restart_program(std::vector<std::string> & parameters) {
    char** newargs = new char* [parameters.size()+1];
    // parameter 0 is the executable path
    // contents of the vector follow
    // last one is NULL
    for(Bitu i = 0; i < parameters.size(); i++) newargs[i]=(char*)parameters[i].c_str();
    newargs[parameters.size()] = NULL;
    if(IsFullscreen()) SwitchFullScreen(1);
    if(!load_videodrv) putenv((char*)("SDL_VIDEODRIVER="));
    SDL_CloseAudio();
    SDL_Delay(50);
    SDL_Quit();
#if C_DEBUG
    // shutdown curses
    DEBUG_ShutDown(NULL);
#endif

    if(execvp(newargs[0], newargs) == -1) {
        E_Exit("Restarting failed");
    }
    free(newargs);
}
void Restart(bool pressed) { // mapper handler
    restart_program(control->startup_params);
}

static void launchcaptures(std::string const& edit) {
    std::string path,file;
    struct stat cstat;
    Section* t = control->GetSection("dosbox");
    if(t) file = t->GetPropValue("captures");
    if(!t || file == NO_SUCH_PROPERTY) {
        printf("Config system messed up.\n");
        exit(1);
    }
    path = ".";
    path += CROSS_FILESPLIT;
    path += file;

    stat(path.c_str(),&cstat);
    if(cstat.st_mode & S_IFDIR) {
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    } else {
        path = "";
        Cross::CreatePlatformConfigDir(path);
        path += file;
        Cross::CreateDir(path);
        stat(path.c_str(),&cstat);
        if((cstat.st_mode & S_IFDIR) == 0) {
            printf("%s doesn't exist or isn't a directory.\n",path.c_str());
            exit(1);
        }
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    }
}

static void launchsaves(std::string const& edit) {
    std::string path,file;
    struct stat cstat;
    file="SAVE";
    path = ".";
    path += CROSS_FILESPLIT;
    path += file;
    stat(path.c_str(),&cstat);
    if(cstat.st_mode & S_IFDIR) {
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    } else {
        path = "";
        Cross::CreatePlatformConfigDir(path);
        path += file;
        Cross::CreateDir(path);
        stat(path.c_str(),&cstat);
        if((cstat.st_mode & S_IFDIR) == 0) {
            printf("%s doesn't exists or isn't a directory.\n",path.c_str());
            exit(1);
        }
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    }
}

static void printconfiglocation() {
    std::string path,file;
    Cross::CreatePlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;

    FILE* f = fopen(path.c_str(),"r");
    if(!f && !control->PrintConfig(path.c_str())) {
        printf("tried creating %s. but failed",path.c_str());
        exit(1);
    }
    if(f) fclose(f);
    printf("%s\n",path.c_str());
    exit(0);
}

static void eraseconfigfile() {
    FILE* f = fopen("dosbox.conf","r");
    if(f) {
        fclose(f);
        show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
    }
    std::string path,file;
    Cross::GetPlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
    f = fopen(path.c_str(),"r");
    if(!f) exit(0);
    fclose(f);
    unlink(path.c_str());
    exit(0);
}

static void erasemapperfile() {
    FILE* g = fopen("dosbox.conf","r");
    if(g) {
        fclose(g);
        show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
                     "Please reset configuration as well and delete the dosbox.conf.\n");
    }

    std::string path,file=MAPPERFILE;
    Cross::GetPlatformConfigDir(path);
    path += file;
    FILE* f = fopen(path.c_str(),"r");
    if(!f) exit(0);
    fclose(f);
    unlink(path.c_str());
    exit(0);
}


// DELETE THESE THREE
void SetNumLock(void) {
}

void CheckNumLockState(void) {
}

extern bool log_keyboard_scan_codes;

void DOSBox_ShowConsole() {
}

void DOSBox_ConsolePauseWait() {
    char c;

    printf("Hit ENTER to continue\n");
    do {
        if (fread(&c, 1, 1, stdin) != 1) break;
    } while (!(c == 13 || c == 10)); /* wait for Enter key */
}

bool DOSBOX_parse_argv() {
    std::string optname,tmp;

    assert(control != NULL);
    assert(control->cmdline != NULL);

    control->cmdline->BeginOpt();
    while (control->cmdline->GetOpt(optname)) {
        std::transform(optname.begin(), optname.end(), optname.begin(), ::tolower);

        if (optname == "version") {
            DOSBox_ShowConsole();

            fprintf(stderr,"\nDOSBox version %s, copyright 2002-2015 DOSBox Team.\n\n",VERSION);
            fprintf(stderr,"DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
            fprintf(stderr,"DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
            fprintf(stderr,"and you are welcome to redistribute it under certain conditions;\n");
            fprintf(stderr,"please read the COPYING file thoroughly before doing so.\n\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        }
        else if (optname == "h" || optname == "help") {
            DOSBox_ShowConsole();

            fprintf(stderr,"\ndosbox [options]\n");
            fprintf(stderr,"\nDOSBox version %s, copyright 2002-2015 DOSBox Team.\n\n",VERSION);
            fprintf(stderr,"  -h     -help                            Show this help\n");
            fprintf(stderr,"  -editconf                               Launch editor\n");
            fprintf(stderr,"  -opencaptures <param>                   Launch captures\n");
            fprintf(stderr,"  -opensaves <param>                      Launch saves\n");
            fprintf(stderr,"  -eraseconf                              Erase config file\n");
            fprintf(stderr,"  -resetconf                              Erase config file\n");
            fprintf(stderr,"  -printconf                              Print config file location\n");
            fprintf(stderr,"  -erasemapper                            Erase mapper file\n");
            fprintf(stderr,"  -resetmapper                            Erase mapper file\n");
            fprintf(stderr,"  -console                                Show console (win32)\n");
            fprintf(stderr,"  -noconsole                              Don't show console (debug+win32 only)\n");
            fprintf(stderr,"  -nogui                                  Don't show gui (win32 only)\n");
            fprintf(stderr,"  -nomenu                                 Don't show menu (win32 only)\n");
            fprintf(stderr,"  -userconf                               Create user level config file\n");
            fprintf(stderr,"  -conf <param>                           Use config file <param>\n");
            fprintf(stderr,"  -startui -startgui                      Start DOSBox-X with UI\n");
            fprintf(stderr,"  -startmapper                            Start DOSBox-X with mapper\n");
            fprintf(stderr,"  -showcycles                             Show cycles count\n");
            fprintf(stderr,"  -showrt                                 Show emulation speed relative to realtime\n");
            fprintf(stderr,"  -fullscreen                             Start in fullscreen\n");
            fprintf(stderr,"  -savedir <path>                         Save path\n");
            fprintf(stderr,"  -disable-numlock-check                  Disable numlock check (win32 only)\n");
            fprintf(stderr,"  -date-host-forced                       Force synchronization of date with host\n");
            fprintf(stderr,"  -debug                                  Set all logging levels to debug\n");
            fprintf(stderr,"  -early-debug                            Log early initialization messages in DOSBox (implies -console)\n");
            fprintf(stderr,"  -keydbg                                 Log all SDL key events (debugging)\n");
            fprintf(stderr,"  -lang <message file>                    Use specific message file instead of language= setting\n");
            fprintf(stderr,"  -nodpiaware                             Ignore (don't signal) Windows DPI awareness\n");
            fprintf(stderr,"  -securemode                             Enable secure mode\n");
            fprintf(stderr,"  -noautoexec                             Don't execute AUTOEXEC.BAT config section\n");
            fprintf(stderr,"  -exit                                   Exit after executing AUTOEXEC.BAT\n");
            fprintf(stderr,"  -c <command string>                     Execute this command in addition to AUTOEXEC.BAT.\n");
            fprintf(stderr,"                                          Make sure to surround the command in quotes to cover spaces.\n");
            fprintf(stderr,"  -break-start                            Break into debugger at startup\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        }
        else if (optname == "c") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_c.push_back(tmp);
        }
        else if (optname == "break-start") {
            control->opt_break_start = true;
        }
        else if (optname == "exit") {
            control->opt_exit = true;
        }
        else if (optname == "noautoexec") {
            control->opt_noautoexec = true;
        }
        else if (optname == "securemode") {
            control->opt_securemode = true;
        }
        else if (optname == "nodpiaware") {
            control->opt_disable_dpi_awareness = true;
        }
        else if (optname == "keydbg") {
            log_keyboard_scan_codes = true;
        }
        else if (optname == "date-host-forced" || optname == "date_host_forced") {
            control->opt_date_host_forced = true;
        }
        else if (optname == "showrt") {
            control->opt_showrt = true;
        }
        else if (optname == "showcycles") {
            control->opt_showcycles = true;
        }
        else if (optname == "startmapper") {
            control->opt_startmapper = true;
        }
        else if (optname == "fullscreen") {
            control->opt_fullscreen = true;
        }
        else if (optname == "startui" || optname == "startgui") {
            control->opt_startui = true;
        }
        else if (optname == "disable-numlock-check" || optname == "disable_numlock_check") {
            /* mainline DOSBox expects -disable_numlock_check so we support that here too */
            control->opt_disable_numlock_check = true;
        }
        else if (optname == "savedir") {
            if (!control->cmdline->NextOptArgv(custom_savedir)) return false;
        }
        else if (optname == "userconf") {
            control->opt_userconf = true;
        }
        else if (optname == "lang") {
            if (!control->cmdline->NextOptArgv(control->opt_lang)) return false;
        }
        else if (optname == "conf") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->config_file_list.push_back(tmp);
        }
        else if (optname == "editconf") {
            if (!control->cmdline->NextOptArgv(control->opt_editconf)) return false;
        }
        else if (optname == "opencaptures") {
            if (!control->cmdline->NextOptArgv(control->opt_opencaptures)) return false;
        }
        else if (optname == "opensaves") {
            if (!control->cmdline->NextOptArgv(control->opt_opensaves)) return false;
        }
        else if (optname == "eraseconf") {
            control->opt_eraseconf = true;
        }
        else if (optname == "resetconf") {
            control->opt_resetconf = true;
        }
        else if (optname == "printconf") {
            control->opt_printconf = true;
        }
        else if (optname == "erasemapper") {
            control->opt_erasemapper = true;
        }
        else if (optname == "resetmapper") {
            control->opt_resetmapper = true;
        }
        else if (optname == "noconsole") {
            control->opt_noconsole = true;
            control->opt_console = false;
        }
        else if (optname == "console") {
            control->opt_noconsole = false;
            control->opt_console = true;
        }
        else if (optname == "nomenu") {
            control->opt_nomenu = true;
        }
        else if (optname == "nogui") {
            control->opt_nogui = true;
        }
        else if (optname == "debug") {
            control->opt_debug = true;
        }
        else if (optname == "early-debug") {
            control->opt_earlydebug = true;
            control->opt_console = true;
        }
        else {
            printf("WARNING: Unknown option %s (first parsing stage)\n",optname.c_str());
        }
    }

    return true;
}

void MSG_Init();
void DOSBOX_RealInit();
void DOSBOX_InitTickLoop();
void TIMER_ShutdownTickHandlers();
void DOSBOX_SetupConfigSections(void);
void PAGING_Init();
void IO_Init();
void Init_VGABIOS();
void Init_AddressLimitAndGateMask();
void Init_RAM();
void Init_MemHandles();
void Init_MemoryAccessArray();
void Init_PCJR_CartridgeROM();
void Init_PS2_Port_92h();
void Init_A20_Gate();
void HARDWARE_Init();
void CAPTURE_Init();
void ROMBIOS_Init();
void CALLBACK_Init();
void Init_DMA();
void Init_PIC();
void PCIBUS_Init();
void PROGRAMS_Init();
void RENDER_Init();
void TIMER_Init();
void CMOS_Init();
void VGA_Init();
void CPU_Init();
void ISAPNP_Cfg_Init();
#if C_FPU
void FPU_Init();
#endif
void KEYBOARD_Init();
void VOODOO_Init();
void MIXER_Init();
void MIDI_Init();

/* Init all the sections */
void MPU401_Init();
#if C_DEBUG
void DEBUG_Init();
#endif
void SBLASTER_Init();
void GUS_Init();
void INNOVA_Init();
void PCSPEAKER_Init();
void TANDYSOUND_Init();
void DISNEY_Init();
void PS1SOUND_Init();
void BIOS_Init();
void INT10_Init();
void JOYSTICK_Init();
void SERIAL_Init();
void PARALLEL_Init();
void DONGLE_Init();
void DOS_Init();
void XMS_Init();
void EMS_Init();
void MOUSE_Init();
void DOS_KeyboardLayout_Init();
void MSCDEX_Init();
void DRIVES_Init();
void IPX_Init();
void IDE_Init();
void NE2K_Init();
void FDC_Primary_Init();
void AUTOEXEC_Init();

bool VM_Boot_DOSBox_Kernel() {
    if (dos_kernel_disabled) {
        DispatchVMEvent(VM_EVENT_DOS_BOOT); // <- just starting the DOS kernel now

        /* DOS kernel init */
        dos_kernel_disabled = false; // FIXME: DOS_Init should install VM callback handler to set this
        void DOS_Startup(Section* sec);
        DOS_Startup(NULL);

        void DRIVES_Startup(Section *s);
        DRIVES_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY); // <- kernel is ready

        /* keyboard mapping, at this point in CONFIG.SYS parsing, right? */
        void DOS_KeyboardLayout_Startup(Section* sec);
        DOS_KeyboardLayout_Startup(NULL);

        /* Most MS-DOS installations have a DEVICE=C:\HIMEM.SYS somewhere near the top of their CONFIG.SYS */
        void XMS_Startup(Section *sec);
        XMS_Startup(NULL);

        /* And then after that, usually a DEVICE=C:\EMM386.EXE just after HIMEM.SYS */
        void EMS_Startup(Section* sec);
        EMS_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_CONFIG_SYS_DONE); // <- we just finished executing CONFIG.SYS
        SHELL_Init(); // <- NTS: this will change CPU instruction pointer!
        DispatchVMEvent(VM_EVENT_DOS_INIT_SHELL_READY); // <- we just finished loading the shell (COMMAND.COM)

        /* it's time to init parsing AUTOEXEC.BAT */
        void AUTOEXEC_Startup(Section *sec);
        AUTOEXEC_Startup(NULL);

        /* Most MS-DOS installations run MSCDEX.EXE from somewhere in AUTOEXEC.BAT. We do the same here, in a fashion. */
        /* TODO: Can we make this an OPTION if the user doesn't want to make MSCDEX.EXE resident? */
        /* TODO: When we emulate executing AUTOEXEC.BAT between INIT_SHELL_READY and AUTOEXEC_BAT_DONE, can we make a fake MSCDEX.EXE within drive Z:\
         *       and auto-add a Z:\MSCDEX.EXE to the top of AUTOEXEC.BAT, command line switches and all. if the user has not already added it? */
//        void MSCDEX_Startup(Section* sec);
//        MSCDEX_Startup(NULL);

        /* Some installations load the MOUSE.COM driver from AUTOEXEC.BAT as well */
        /* TODO: Can we make this an option? Can we add a fake MOUSE.COM to the Z:\ drive as well? */
        void MOUSE_Startup(Section *sec);
        MOUSE_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE); // <- we just finished executing AUTOEXEC.BAT
        DispatchVMEvent(VM_EVENT_DOS_INIT_AT_PROMPT); // <- now, we're at the DOS prompt
        SHELL_Run();
    }

    return true;
}

bool VM_PowerOn() {
    if (!guest_machine_power_on) {
        // powering on means power on event, followed by reset assert, then reset deassert
        guest_machine_power_on = true;
        DispatchVMEvent(VM_EVENT_POWERON);
        DispatchVMEvent(VM_EVENT_RESET);
        DispatchVMEvent(VM_EVENT_RESET_END);
    }

    return true;
}

//extern void UI_Init(void);
int main(int argc, char* argv[]) {
    CommandLine com_line(argc,argv);
    {
        /* NTS: Warning, do NOT move the Config myconf() object out of this scope.
         * The destructor relies on executing section destruction code as part of
         * DOSBox shutdown. */
        std::string tmp,config_path;
        Config myconf(&com_line);
        control=&myconf;
        /* -- parse command line arguments */
        if (!DOSBOX_parse_argv()) return 1;
        if (control->opt_console)
            DOSBox_ShowConsole();
        /* -- Handle some command line options */
        if (control->opt_eraseconf || control->opt_resetconf)
            eraseconfigfile();
        if (control->opt_printconf)
            printconfiglocation();
        if (control->opt_erasemapper || control->opt_resetmapper)
            erasemapperfile();

        /* -- Early logging init, in case these details are needed to debug problems at this level */
        /*    If --early-debug was given this opens up logging to STDERR until Log::Init() */
        LOG::EarlyInit();

        /* -- Init the configuration system and add default values */
        CheckNumLockState();

        /* -- setup the config sections for config parsing */
        LOG::SetupConfigSection();
        SDL_SetupConfigSection();
        DOSBOX_SetupConfigSections();
        /* -- Parse configuration files */
        Cross::GetPlatformConfigDir(config_path);
        /* -- -- first the user config file */
        if (control->opt_userconf) {
            tmp.clear();

            Cross::GetPlatformConfigDir(config_path);
            Cross::GetPlatformConfigName(tmp);
            config_path += tmp;
            LOG(LOG_MISC,LOG_DEBUG)("Loading config file according to -userconf from %s",config_path.c_str());
            control->ParseConfigFile(config_path.c_str());
            if (!control->configfiles.size()) {
                //Try to create the userlevel configfile.
                tmp.clear();
                Cross::CreatePlatformConfigDir(config_path);
                Cross::GetPlatformConfigName(tmp);
                config_path += tmp;

                LOG(LOG_MISC,LOG_DEBUG)("Attempting to write config file according to -userconf, to %s",config_path.c_str());
                if (control->PrintConfig(config_path.c_str())) {
                    LOG(LOG_MISC,LOG_NORMAL)("Generating default configuration. Writing it to %s",config_path.c_str());
                    //Load them as well. Makes relative paths much easier
                    control->ParseConfigFile(config_path.c_str());
                }
            }
        }

        /* -- -- second the -conf switches from the command line */
        for (size_t si=0; si < control->config_file_list.size(); si++) {
            std::string &cfg = control->config_file_list[si];
            if (!control->ParseConfigFile(cfg.c_str())) {
                // try to load it from the user directory
                control->ParseConfigFile((config_path + cfg).c_str());
            }
        }

        /* -- -- if none found, use dosbox.conf */
        if(!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

        /* -- -- if none found, use userlevel conf */
        if (!control->configfiles.size()) {
            tmp.clear();
            Cross::GetPlatformConfigName(tmp);
            control->ParseConfigFile((config_path + tmp).c_str());
        }

#if (ENVIRON_LINKED)
        /* -- parse environment block (why?) */
        control->ParseEnv(environ);
#endif

        /* -- initialize logging first, so that higher level inits can report problems to the log file */
        LOG::Init();

        /* -- Welcome to DOSBox-X! */
        LOG_MSG("DOSBox-X version %s",VERSION);
        LOG(LOG_MISC,LOG_NORMAL)("Copyright 2002-2015 enhanced branch by The Great Codeholio, forked from the main project by the DOSBox Team, published under GNU GPL.");

        {
            int id, major, minor;

            DOSBox_CheckOS(id, major, minor);
            if (id == 1) menu.compatible=true;

            /* use all variables to shut up the compiler about unused vars */
            LOG(LOG_MISC,LOG_DEBUG)("DOSBox_CheckOS results: id=%u major=%u minor=%u",id,major,minor);
        }

        /* -- SDL init hackery */
#if SDL_VERSION_ATLEAST(1, 2, 14)
        /* hack: On debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
         * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version of libsdl */
        putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
        LOG(LOG_GUI,LOG_DEBUG)("SDL 1.2.14 hack: SDL_DISABLE_LOCK_KEYS=1");
#endif

        /* -- SDL init */
        if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|/*SDL_INIT_CDROM|*/SDL_INIT_NOPARACHUTE) >= 0)
            sdl.inited = true;
        else
            E_Exit("Can't init SDL %s",SDL_GetError());

        /* -- -- decide whether to show menu in GUI */
        if (control->opt_nogui || menu.compatible)
            menu.gui=false;

        /* -- -- decide whether to set menu */
        if (menu_gui && !control->opt_nomenu)
            DOSBox_SetMenu();

        /* -- -- helpful advice */
        LOG(LOG_GUI,LOG_NORMAL)("Press Ctrl-F10 to capture/release mouse, Alt-F10 for configuration.");

        /* -- -- Initialise Joystick seperately. This way we can warn when it fails instead of exiting the application */
        LOG(LOG_MISC,LOG_DEBUG)("Initializing SDL joystick subsystem...");
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) >= 0) {
            sdl.num_joysticks = SDL_NumJoysticks();
            LOG(LOG_MISC,LOG_DEBUG)("SDL reports %u joysticks",(unsigned int)sdl.num_joysticks);
        }
        else {
            LOG(LOG_GUI,LOG_WARN)("Failed to init joystick support");
            sdl.num_joysticks = 0;
        }

        /* assume L+R ALT keys are up */
        sdl.laltstate = SDL_KEYUP;
        sdl.raltstate = SDL_KEYUP;

        /* GUI init */
        GUI_StartUp();

        /* FIXME: We need a more general "init list", outside of the section-based design,
         *        that we then execute serially here. */
        /* TODO: Each section currently uses "AddDestroyFunction" per section. We need to
         *       change over that code to a global destroy callback list instead. */
        /* TODO: Get rid of "init" and "destroy" callback lists per section. */
        /* TODO: Add a global (within the Config object) init and destroy callback list.
         *       On each call, init functions are added to the end of the list, and
         *       destroy functions added to the beginning of the list. That way, init
         *       is lowest level to highest, destroy is highest level to lowest. */
        /* TODO: Config object should also have a "reset" callback list. On system
         *       reset each device would be notified so that it can emulate hardware
         *       reset (the RESET line on ISA/PCI bus), lowest level to highest. */
        /* TODO: Each "init" function should do the work of getting the section object,
         *       whatever section it wants to read, instead of us doing the work. When
         *       that's complete, the call to init should be without parameters (void).
         *       The hope is that the init functions can read whatever sections it wants,
         *       both newer DOSBox-X sections and existing DOSBox (mainline) compatible
         *       sections. */

        /* The order is important here:
         * Init functions are called low-level first to high level last,
         * because some init functions rely on others. */

//        if (control->opt_startui)
//            GUI_Run(false);
        if (control->opt_editconf.length() != 0)
            launcheditor(control->opt_editconf);
        if (control->opt_opencaptures.length() != 0)
            launchcaptures(control->opt_opencaptures);
        if (control->opt_opensaves.length() != 0)
            launchsaves(control->opt_opensaves);

        {
            /* Some extra SDL Functions */
            Section_prop *sdl_sec = static_cast<Section_prop*>(control->GetSection("sdl"));

            if (control->opt_fullscreen || sdl_sec->Get_bool("fullscreen")) {
                LOG(LOG_MISC,LOG_DEBUG)("Going fullscreen immediately, during startup");

                //only switch if not already in fullscreen
                if (!IsFullscreen()) GFX_SwitchFullScreen();
            }
        }

        /* Start up main machine */

        // Shows menu bar (window)
        menu.startup = true;
        menu.showrt = control->opt_showrt;
        menu.hidecycles = (control->opt_showcycles ? false : true);

        MSG_Init();
        MAPPER_StartUp();
        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();
        RENDER_Init();
        CAPTURE_Init();
        IO_Init();
        HARDWARE_Init();
        Init_AddressLimitAndGateMask(); /* <- need to init address mask so Init_RAM knows the maximum amount of RAM possible */
        Init_MemoryAccessArray(); /* <- NTS: In DOSBox-X this is the "cache" of devices that responded to memory access */
        Init_A20_Gate(); // FIXME: Should be handled by motherboard!
        Init_PS2_Port_92h(); // FIXME: Should be handled by motherboard!
        Init_RAM();
        Init_DMA();
        Init_PIC();
        TIMER_Init();
        PCIBUS_Init();
        PAGING_Init(); /* <- NTS: At this time, must come before memory init because paging is so well integrated into emulation code */
        CMOS_Init();
        ROMBIOS_Init();
        CALLBACK_Init(); /* <- NTS: This relies on ROM BIOS allocation and it must happen AFTER ROMBIOS init */
#if C_DEBUG
        DEBUG_Init(); /* <- NTS: Relies on callback system */
#endif
        Init_VGABIOS();
//        VOODOO_Init();
        PROGRAMS_Init(); /* <- NTS: Does not init programs, it inits the callback used later when creating the .COM programs on drive Z: */
        PCSPEAKER_Init();
        TANDYSOUND_Init();
        MPU401_Init();
        MIXER_Init();
        MIDI_Init();
        CPU_Init();
#if C_FPU
        FPU_Init();
#endif
        VGA_Init();
        ISAPNP_Cfg_Init();
        FDC_Primary_Init();
        KEYBOARD_Init();
        SBLASTER_Init();
        JOYSTICK_Init();
        PS1SOUND_Init();
        DISNEY_Init();
        GUS_Init();
        IDE_Init();
        INNOVA_Init();
        BIOS_Init();
        INT10_Init();
        SERIAL_Init();
        DONGLE_Init();
        PARALLEL_Init();
#if C_NE2000
        NE2K_Init();
#endif

        /* If PCjr emulation, map cartridge ROM */
        if (machine == MCH_PCJR)
            Init_PCJR_CartridgeROM();

        /* let's assume motherboards are sane on boot because A20 gate is ENABLED on first boot */
        MEM_A20_Enable(true);

        /* OS init now */
        DOS_Init();
        DRIVES_Init();
        DOS_KeyboardLayout_Init();
        MOUSE_Init(); // FIXME: inits INT 15h and INT 33h at the same time. Also uses DOS_GetMemory() which is why DOS_Init must come first
        XMS_Init();
        EMS_Init();
        AUTOEXEC_Init();
#if C_IPX
        IPX_Init();
#endif
//        MSCDEX_Init();

        /* Init memhandle system. This part is used by DOSBox's XMS/EMS emulation to associate handles
         * per page. FIXME: I would like to push this down to the point that it's never called until
         * XMS/EMS emulation needs it. I would also like the code to free the mhandle array immediately
         * upon booting into a guest OS, since memory handles no longer have meaning in the guest OS
         * memory layout. */
        Init_MemHandles();

        /* finally, the mapper */
        MAPPER_Init();

        /* stop at this point, and show the mapper, if instructed */
        if (control->opt_startmapper) {
            LOG(LOG_MISC,LOG_DEBUG)("Running mapper interface, during startup, as instructed");
            MAPPER_RunInternal();
        }

        /* The machine just "powered on", and then reset finished */
        if (!VM_PowerOn()) E_Exit("VM failed to power on");

        bool run_machine;
        bool reboot_machine;
        bool dos_kernel_shutdown;

        run_machine = false;
        reboot_machine = false;
        dos_kernel_shutdown = false;

        /* NTS: CPU reset handler, and BIOS init, has the instruction pointer poised to run through BIOS initialization,
         *      which will then "boot" into the DOSBox kernel, and then the shell, by calling VM_Boot_DOSBox_Kernel() */
        /* FIXME: throwing int() is a stupid and nondescriptive way to signal shutdown/reset. */
        try {
#if C_DEBUG
            if (control->opt_break_start) DEBUG_EnableDebugger();
#endif
            DOSBOX_RunMachine();
        } catch (int x) {
            if (x == 2) { /* booting a guest OS. "boot" has already done the work to load the image and setup CPU registers */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to boot guest OS");

                run_machine = true; /* make note. don't run the whole shebang from an exception handler! */
                dos_kernel_shutdown = true;
            }
            else if (x == 3) { /* reboot the system */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to reboot the system");

                reboot_machine = true;
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw DOSBox kill switch signal");

                // kill switch (see instances of throw(0) and throw(1) elsewhere in DOSBox)
                run_machine = false;
                dos_kernel_shutdown = false;
            }
        }
        catch (...) {
            throw;
        }

        if (dos_kernel_shutdown) {
            /* new code: fire event */
            DispatchVMEvent(VM_EVENT_DOS_EXIT_BEGIN);

            /* older shutdown code */
            RemoveEMSPageFrame();

            /* remove UMB block */
            if (!keep_umb_on_boot) RemoveUMBBlock();

            /* disable INT 33h mouse services. it can interfere with guest OS paging and control of the mouse */
            DisableINT33();

            /* unmap the DOSBox kernel private segment. if the user told us not to,
             * but the segment exists below 640KB, then we must, because the guest OS
             * will trample it and assume control of that region of RAM. */
            if (!keep_private_area_on_boot)
                DOS_GetMemory_unmap();
            else if (DOS_PRIVATE_SEGMENT < 0xA000)
                DOS_GetMemory_unmap();

            /* revector some dos-allocated interrupts */
            real_writed(0,0x01*4,BIOS_DEFAULT_HANDLER_LOCATION);
            real_writed(0,0x03*4,BIOS_DEFAULT_HANDLER_LOCATION);

            /* shutdown DOSBox's virtual drive Z */
            VFILE_Shutdown();

            /* shutdown the programs */
            PROGRAMS_Shutdown();		/* FIXME: Is this safe? Or will this cause use-after-free bug? */

            /* remove environment variables for some components */
            DOS_UninstallMisc();
            SBLASTER_DOS_Shutdown();
            GUS_DOS_Shutdown();
            /* disable Expanded Memory. EMM is a DOS API, not a BIOS API */
            EMS_DoShutDown();
            /* and XMS, also a DOS API */
            XMS_DoShutDown();
            /* and the DOS API in general */
            DOS_DoShutDown();

            /* set the "disable DOS kernel" flag so other parts of this program
             * do not attempt to manipulate now-defunct parts of the kernel
             * such as the environment block */
            dos_kernel_disabled = true;

            /* new code: fire event */
            DispatchVMEvent(VM_EVENT_DOS_EXIT_KERNEL);
        }

        if (run_machine) {
            /* new code: fire event */
            DispatchVMEvent(VM_EVENT_GUEST_OS_BOOT);

            LOG_MSG("Alright: DOS kernel shutdown, booting a guest OS\n");
            LOG_MSG("  CS:IP=%04x:%04x SS:SP=%04x:%04x AX=%04x BX=%04x CX=%04x DX=%04x\n",
                    SegValue(cs),reg_ip,
                    SegValue(ss),reg_sp,
                    reg_ax,reg_bx,reg_cx,reg_dx);

            try {
                /* go! */
                while (1/*execute until some other part of DOSBox throws exception*/)
                    DOSBOX_RunMachine();
            }
            catch (int x) {
                if (x == 3) { /* reboot the machine */
                    LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to reboot the system");

                    reboot_machine = true;
                }
                else {
                    LOG(LOG_MISC,LOG_DEBUG)("Emulation threw DOSBox kill switch signal");

                    // kill switch (see instances of throw(0) and throw(1) elsewhere in DOSBox)
                }
            }
            catch (...) {
                throw;
            }
        }

        if (reboot_machine) {
            LOG_MSG("Rebooting the system\n");

            /* new code: fire event (FIXME: DOSBox's current method of "rebooting" the emulator makes this meaningless!) */
            DispatchVMEvent(VM_EVENT_RESET);
            DispatchVMEvent(VM_EVENT_RESET_END);

            /* restart DOSBox (NOTE: Yuck) */
            restart_program(control->startup_params);
        }

        /* and then shutdown */
        GFX_ShutDown();

        void CPU_Snap_Back_Forget();
        /* Shutdown everything. For shutdown to work properly we must force CPU to real mode */
        CPU_Snap_Back_To_Real_Mode();
        CPU_Snap_Back_Forget();

        /* NTS: The "control" object destructor is called here because the "myconf" object leaves scope.
         * The destructor calls all section destroy functions here. After this point, all sections have
         * freed resources. */
    }

    /* GUI font registry shutdown */
//    GUI::Font::registry_freeall();
    DOS_ShutdownDrives();
    DOS_ShutdownFiles();
    DOS_ShutdownDevices();
    CALLBACK_Shutdown();
#if C_DYNAMIC_X86
    CPU_Core_Dyn_X86_Shutdown();
#endif
    FreeBIOSDiskList();
    MAPPER_Shutdown();
    VFILE_Shutdown();
    PROGRAMS_Shutdown();
    TIMER_ShutdownTickHandlers();
#if C_DEBUG
    DEBUG_ShutDown(NULL);
#endif

    //Force visible mouse to end user. Somehow this sometimes doesn't happen
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);

    /* Exit functions */
    while (!exitfunctions.empty()) {
        Function_wrapper &ent = exitfunctions.front();

        LOG(LOG_MISC,LOG_DEBUG)("Calling exit function (%p) '%s'",(void*)ent.function,ent.name.c_str());
        ent.function(NULL);
        exitfunctions.pop_front();
    }

    LOG::Exit();
    SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
    return 0;
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
    width = sdl.draw.width;
    height = sdl.draw.height;
    fullscreen = IsFullscreen();
}

void GFX_ShutDown(void) {
    LOG(LOG_MISC,LOG_DEBUG)("Shutting down GFX renderer");
    GFX_Stop();
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
    if (sdl.mouse.locked) GFX_CaptureMouse();
    if (IsFullscreen()) GFX_SwitchFullScreen();
}

bool OpenGL_using(void) {
    return (sdl.desktop.want_type==SCREEN_OPENGL?true:false);
}

bool Get_Custom_SaveDir(std::string& savedir) {
    if (custom_savedir.length() != 0)
        return true;

    return false;
}

