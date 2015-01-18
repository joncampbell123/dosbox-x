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

#ifdef WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
#endif

#ifdef OS2
# define INCL_DOS
# define INCL_WIN
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef WIN32
# include <signal.h>
# include <process.h>
#endif

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

#include "../src/libs/gui_tk/gui_tk.h"

#ifdef __WIN32__
# include "callback.h"
# include "dos_inc.h"
# include <malloc.h>
# include "Commdlg.h"
# include "windows.h"
# include "Shellapi.h"
# include "shell.h"
# include "SDL_syswm.h"
# include <cstring>
# include <fstream>
# include <sstream>
#endif // WIN32

#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "cross.h"
#include "control.h"

extern bool keep_umb_on_boot;
extern bool keep_private_area_on_boot;
extern bool dos_kernel_disabled;

bool opt_date_host_forced = false;
bool opt_disable_numlock_check = false;
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

enum SCREEN_TYPES {
	SCREEN_OPENGLHQ,
	SCREEN_SURFACE,
	SCREEN_SURFACE_DDRAW,
	SCREEN_OVERLAY,
	SCREEN_OPENGL,
	SCREEN_DIRECT3D
};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

#define MAPPERFILE				"mapper-" VERSION ".map"

void						UI_Init();
void						UI_Run(bool);
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
bool						emu_paused = false;
bool						mouselocked = false; //Global variable for mapper
bool						load_videodrv = true;
bool						fullscreen_switch = true;
bool						dos_kernel_disabled = false;
bool						startup_state_numlock = false; // Global for keyboard initialisation
bool						startup_state_capslock = false; // Global for keyboard initialisation

#ifdef WIN32
# include <windows.h>
#endif

#if (HAVE_DDRAW_H)
# include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3			dd_surface;
	LPDIRECTDRAWSURFACE3			dd_writebuf;
};
#endif

#if (HAVE_D3D9_H)
# include "direct3d.h"
#endif

#if (HAVE_D3D9_H)
CDirect3D*					d3d = NULL;
#endif

#ifdef WIN32
# define STDOUT_FILE				TEXT("stdout.txt")
# define STDERR_FILE				TEXT("stderr.txt")
# define DEFAULT_CONFIG_FILE			"/dosbox.conf"
#elif defined(MACOSX)
# define DEFAULT_CONFIG_FILE			"/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
# define DEFAULT_CONFIG_FILE			"/.dosboxrc"
#endif

#if C_SET_PRIORITY
# include <sys/resource.h>
# define PRIO_TOTAL				(PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
# include <os2.h>
#endif

struct SDL_Block {
	bool inited;
	bool active;							//If this isn't set don't draw
	bool updating;
	struct {
		Bit32u width;
		Bit32u height;
		Bit32u bpp;
		Bitu flags;
		double scalex,scaley;
		GFX_CallBack_t callback;
	} draw;
	bool wait_on_error;
	struct {
		struct {
			Bit16u width, height;
			bool fixed;
		} full;
		struct {
			Bit16u width, height;
		} window;
		Bit8u bpp;
		bool fullscreen;
		bool lazy_fullscreen;
		bool lazy_fullscreen_req;
		bool doublebuf;
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
	} desktop;
#if C_OPENGL
	struct {
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
#endif
	struct {
		SDL_Surface * surface;
#if (HAVE_DDRAW_H) && defined(WIN32)
		RECT rect;
#endif
	} blit;
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
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
#if defined (WIN32)
	bool using_windib;
#endif
	// state of alt-keys for certain special handlings
	Bit8u laltstate;
	Bit8u raltstate;
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
#endif

    	SDL_WM_SetIcon(logos,NULL);
#endif
}
/* =================================================================================== */

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void) {
	return sdl.using_windib;
}
#endif

void GFX_SetIcon(void) {
#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
	/* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
	if (menu_compatible) { DOSBox_SetOriginalIcon(); return; }
#endif

#ifdef WIN32
	HICON hIcon1;

	hIcon1 = (HICON) LoadImage( GetModuleHandle(NULL), MAKEINTRESOURCE(dosbox_ico), IMAGE_ICON,
		16,16,LR_DEFAULTSIZE);

	SendMessage(GetHWND(), WM_SETICON, ICON_SMALL, (LPARAM) hIcon1 ); 
#endif
}

void GFX_SetTitle(Bit32s cycles,Bits frameskip,Bits timing,bool paused){
	static Bits internal_frameskip=0;
	static Bit32s internal_cycles=0;
	static Bits internal_timing=0;
	char title[200] = {0};

	if (cycles != -1) internal_cycles = cycles;
	if (timing != -1) internal_timing = timing;
	if (frameskip != -1) internal_frameskip = frameskip;

	if (!menu_startup) {
		sprintf(title,"DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, %8s",VERSION,
			internal_cycles,internal_frameskip,RunningProgram);
		SDL_WM_SetCaption(title,VERSION);
		return;
	}
	if (menu.hidecycles) {
		if (CPU_CycleAutoAdjust) {
			sprintf(title,"DOSBox %s, CPU speed: max %3d%% cycles, Frameskip %2d, %8s",
				VERSION,CPU_CyclePercUsed,internal_frameskip,RunningProgram);
		}
		else {
			sprintf(title,"DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, %8s",
				VERSION,internal_cycles,internal_frameskip,RunningProgram);
		}
	} else if (CPU_CycleAutoAdjust) {
		sprintf(title,"DOSBox %s, CPU : %s %8d%% = max %3d, %d FPS - %2d %8s %i.%i%%",
			VERSION,core_mode,CPU_CyclePercUsed,internal_cycles,frames,
			internal_frameskip,RunningProgram,internal_timing/100,internal_timing%100/10);
	} else {
		sprintf(title,"DOSBox %s, CPU : %s %8d = %8d, %d FPS - %2d %8s %i.%i%%",
			VERSION,core_mode,CPU_CyclesCur,internal_cycles,frames,internal_frameskip,
			RunningProgram,internal_timing/100,internal_timing%100/10);
	}

	if (paused) strcat(title," PAUSED");
	SDL_WM_SetCaption(title,VERSION);
}

bool warn_on_mem_write = false;

void CPU_Snap_Back_To_Real_Mode();

static void KillSwitch(bool pressed) {
	if (!pressed) return;
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
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
			rect->x = 0; rect->y = 0; rect->w = sdl.draw.width+2*sdl.clip.x; rect->h = sdl.clip.y; // top
			if (rect->h > sdl.overscan_width) { rect->y += (rect->h-sdl.overscan_width); rect->h = sdl.overscan_width; }
			if (sdl.clip.x > sdl.overscan_width) { rect->x += (sdl.clip.x-sdl.overscan_width); rect->w -= 2*(sdl.clip.x-sdl.overscan_width); }
			rect = &sdl.updateRects[1];
			rect->x = 0; rect->y = sdl.clip.y; rect->w = sdl.clip.x; rect->h = sdl.draw.height; // left
			if (rect->w > sdl.overscan_width) { rect->x += (rect->w-sdl.overscan_width); rect->w = sdl.overscan_width; }
			rect = &sdl.updateRects[2];
			rect->x = sdl.clip.x+sdl.draw.width; rect->y = sdl.clip.y; rect->w = sdl.clip.x; rect->h = sdl.draw.height; // right
			if (rect->w > sdl.overscan_width) { rect->w = sdl.overscan_width; }
			rect = &sdl.updateRects[3];
			rect->x = 0; rect->y = sdl.clip.y+sdl.draw.height; rect->w = sdl.draw.width+2*sdl.clip.x; rect->h = sdl.clip.y; // bottom
			if (rect->h > sdl.overscan_width) { rect->h = sdl.overscan_width; }
			if (sdl.clip.x > sdl.overscan_width) { rect->x += (sdl.clip.x-sdl.overscan_width); rect->w -= 2*(sdl.clip.x-sdl.overscan_width); }

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
				SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
			}
		}
	}
}

bool DOSBox_Paused()
{
	return emu_paused;
}

void PauseDOSBox(bool pressed) {
	bool paused = true;

	if (!pressed) return;
	GFX_SetTitle(-1,-1,-1,true);
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}

	// give mouse to win32 (ex. alt-tab)
	SDL_WM_GrabInput(SDL_GRAB_OFF);

	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
#ifdef __WIN32__
		if (event.type==SDL_SYSWMEVENT && event.syswm.msg->msg==WM_COMMAND && event.syswm.msg->wParam==ID_PAUSE) {
			paused=false;
			GFX_SetTitle(-1,-1,-1,false);	
			break;
		}
#endif
		switch (event.type) {

			case SDL_QUIT: KillSwitch(true); break;
			case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
			case SDL_KEYUP:
			if(event.key.keysym.sym == SDLK_PAUSE) {

				paused = false;
				GFX_SetTitle(-1,-1,-1,false);
				break;
			}
#if defined (MACOSX)
			if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
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

	// redraw screen (ex. fullscreen - pause - alt+tab x2 - unpause)
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackReset );
}

static void SDLScreen_Reset(void) {
	char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
	if ((sdl_videodrv && !strcmp(sdl_videodrv,"windib")) || sdl.desktop.fullscreen || fullscreen_switch || sdl.desktop.want_type==SCREEN_OPENGLHQ || menu_compatible) return;
	int id, major, minor;
	DOSBox_CheckOS(id, major, minor);
	if((id==VER_PLATFORM_WIN32_NT) && (major<6) || sdl.desktop.want_type==SCREEN_DIRECT3D) return;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);	SDL_Delay(500);
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	GFX_SetIcon();
	GFX_SetTitle(-1,-1,-1,false);
}

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) {
	Bitu testbpp,gotbpp;
	switch (sdl.desktop.want_type) {
	case SCREEN_OPENGLHQ:
		flags|=GFX_SCALING;
	case SCREEN_SURFACE:
check_surface:
		flags &= ~GFX_LOVE_8;		//Disable love for 8bpp modes
		/* Check if we can satisfy the depth it loves */
		if (flags & GFX_LOVE_8) testbpp=8;
		else if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
#if (HAVE_DDRAW_H) && defined(WIN32)
check_gotbpp:
#endif
		if (sdl.desktop.fullscreen) gotbpp=SDL_VideoModeOK(640,480,testbpp,SDL_FULLSCREEN|SDL_HWSURFACE|SDL_HWPALETTE);
		else gotbpp=sdl.desktop.bpp;
		/* If we can't get our favorite mode check for another working one */
		switch (gotbpp) {
		case 8:
			if (flags & GFX_CAN_8) flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (flags & GFX_CAN_15) flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (flags & GFX_CAN_16) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (flags & GFX_CAN_32) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
		}
		flags |= GFX_CAN_RANDOM;
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (!(flags&(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32))) goto check_surface;
		if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
		flags|=GFX_SCALING;
		goto check_gotbpp;
#endif
	case SCREEN_OVERLAY:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#endif
#if (HAVE_D3D9_H) && defined(WIN32)
	case SCREEN_DIRECT3D:
		flags|=GFX_SCALING;
		if(GCC_UNLIKELY(d3d->bpp16))
		    flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
		else
		    flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#endif
	default:
		goto check_surface;
		break;
	}
	return flags;
}

void SDL_Prepare(void) {
	if(menu_compatible) return;
	SDL_PumpEvents(); SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	DragAcceptFiles(GetHWND(), TRUE);
}

void GFX_ForceRedrawScreen(void) {
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
}

void GFX_ResetScreen(void) {
	fullscreen_switch=false; 
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
	CPU_Reset_AutoAdjust();
	fullscreen_switch=true;
	if (!sdl.desktop.want_type==SCREEN_OPENGLHQ && !sdl.desktop.fullscreen && GetMenu(GetHWND()) == NULL)
		DOSBox_RefreshMenu(); // for menu
}

void GFX_ForceFullscreenExit(void) {
	if (sdl.desktop.lazy_fullscreen) {
		LOG_MSG("GFX LF: invalid screen change");
	} else {
		sdl.desktop.fullscreen=false;
		GFX_ResetScreen();
	}
}

static SDL_Surface * GFX_SetupSurfaceScaled(Bit32u sdl_flags, Bit32u bpp) {
	Bit16u fixedWidth;
	Bit16u fixedHeight;

	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
		sdl_flags |= SDL_FULLSCREEN|SDL_HWSURFACE;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
		sdl_flags |= SDL_HWSURFACE;
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
		if (sdl.desktop.fullscreen)
			sdl.surface = SDL_SetVideoMode(fixedWidth,fixedHeight,bpp,sdl_flags);
		else
			sdl.surface = SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		if (sdl.surface && sdl.surface->flags & SDL_FULLSCREEN) {
			sdl.clip.x=(Sint16)((sdl.surface->w-sdl.clip.w)/2);
			sdl.clip.y=(Sint16)((sdl.surface->h-sdl.clip.h)/2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}
		return sdl.surface;
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
		sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		return sdl.surface;
	}
}

void GFX_TearDown(void) {
	if (sdl.updating)
		GFX_EndUpdate( 0 );

	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}
}

static void GFX_ResetSDL() {
#ifdef WIN32
	if(!load_videodrv && !sdl.using_windib) {
		LOG_MSG("Resetting to WINDIB mode");
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		putenv("SDL_VIDEODRIVER=windib");
		sdl.using_windib=true;
		if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
		GFX_SetIcon(); GFX_SetTitle(-1,-1,-1,false);
		if(!sdl.desktop.fullscreen && GetMenu(GetHWND()) == NULL) DOSBox_RefreshMenu();
	}
#endif
}

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback) {
	EndSplashScreen();
	if (sdl.updating)
		GFX_EndUpdate( 0 );

	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.flags=flags;
	sdl.draw.callback=callback;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	Bitu bpp=0;
	Bitu retFlags = 0;

	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}
	switch (sdl.desktop.want_type) {
	case SCREEN_OPENGLHQ:
		static char scale[64];
		if (flags & GFX_CAN_8) bpp=8;
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.x=0;
		sdl.clip.y=0;
		if(!sdl.desktop.fullscreen) {
		    if(sdl.desktop.window.width && sdl.desktop.window.height) {
			scalex=(double)sdl.desktop.window.width/(sdl.draw.width*sdl.draw.scalex);
			scaley=(double)sdl.desktop.window.height/(sdl.draw.height*sdl.draw.scaley);
			if(scalex < scaley) {
			    sdl.clip.w=sdl.desktop.window.width;
			    sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*scalex);
			} else {
			    sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*scaley);
			    sdl.clip.h=(Bit16u)sdl.desktop.window.height;
			}
		    } else {
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
			sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		    }
		    snprintf(scale,64,"SDL_OPENGLHQ_WINRES=%dx%d",sdl.clip.w,sdl.clip.h);
		    sdl.clip.w=width; sdl.clip.h=height;

    		} else if(!sdl.desktop.full.fixed) {
		    snprintf(scale,64,"SDL_OPENGLHQ_FULLRES=%dx%d",sdl.draw.width*(Uint16)sdl.draw.scalex,
								sdl.draw.height*(Uint16)sdl.draw.scaley);
		    sdl.clip.w=width; sdl.clip.h=height;
		} else {
		    snprintf(scale,64,"SDL_OPENGLHQ_FULLRES=%dx%d",sdl.desktop.full.width,sdl.desktop.full.height);
		    scalex=(double)sdl.desktop.full.width/(sdl.draw.width*sdl.draw.scalex);
		    scaley=(double)sdl.desktop.full.height/(sdl.draw.height*sdl.draw.scaley);
		    sdl.clip.w=width; sdl.clip.h=height;

		    if (scalex < scaley)
			height *= scaley/scalex;
		    else
			width *= scalex/scaley;
		    sdl.clip.x=(Sint16)((width-sdl.clip.w)/2);
		    sdl.clip.y=(Sint16)((height-sdl.clip.h)/2);
		}
		putenv(scale);
		sdl.surface=SDL_SetVideoMode(width,height,bpp,(sdl.desktop.fullscreen?SDL_FULLSCREEN:0)|SDL_HWSURFACE|SDL_ANYFORMAT);
		if (sdl.surface) {
		    switch (sdl.surface->format->BitsPerPixel) {
			case 8:retFlags = GFX_CAN_8;break;
			case 15:retFlags = GFX_CAN_15;break;
			case 16:retFlags = GFX_CAN_16;break;
			case 32:retFlags = GFX_CAN_32;break;
			default:break;
		    }
		    if (retFlags) {
			if (sdl.surface->flags & SDL_HWSURFACE)
			    retFlags |= GFX_HARDWARE;
			retFlags |= GFX_SCALING;
		    }
		}
		break;
	case SCREEN_SURFACE:
		GFX_ResetSDL();
dosurface:
		if (flags & GFX_CAN_8) bpp=8;
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.w=width;
		sdl.clip.h=height;
		SDLScreen_Reset();
		if (sdl.desktop.fullscreen) {
			Uint32 wflags = SDL_FULLSCREEN | SDL_HWPALETTE |
				((flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) |
				(sdl.desktop.doublebuf ? SDL_DOUBLEBUF|SDL_ASYNCBLIT : 0);
			if (sdl.desktop.full.fixed
			) {
				sdl.clip.x=(Sint16)((sdl.desktop.full.width-width)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.full.height-height)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.full.width,
					sdl.desktop.full.height, bpp, wflags);
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.surface=SDL_SetVideoMode(width, height, bpp, wflags);
			}
			if (sdl.surface == NULL) {
				LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
				sdl.desktop.fullscreen=false;
				GFX_CaptureMouse();
				goto dosurface;
			}
		} else {
			sdl.clip.x=sdl.overscan_width;sdl.clip.y=sdl.overscan_width;
			sdl.surface=SDL_SetVideoMode(width+2*sdl.overscan_width,height+2*sdl.overscan_width,bpp,(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE | SDL_RESIZABLE : SDL_HWSURFACE | SDL_RESIZABLE);
#ifdef WIN32
			if (sdl.surface == NULL) {
				SDL_QuitSubSystem(SDL_INIT_VIDEO);
				if (!sdl.using_windib) {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with windib enabled.");
					putenv("SDL_VIDEODRIVER=windib");
					sdl.using_windib=true;
				} else {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with directx enabled.");
					putenv("SDL_VIDEODRIVER=directx");
					sdl.using_windib=false;
				}
				SDL_InitSubSystem(SDL_INIT_VIDEO);
				GFX_SetIcon(); //Set Icon again
				sdl.surface = SDL_SetVideoMode(width,height,bpp,SDL_HWSURFACE);
				if(sdl.surface) GFX_SetTitle(-1,-1,-1,false); //refresh title.
			}
#endif
			if (sdl.surface == NULL)
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",width,height,bpp,SDL_GetError());
		}
		if (sdl.surface) {
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
			if (retFlags && (sdl.surface->flags & SDL_HWSURFACE))
				retFlags |= GFX_HARDWARE;
			if (retFlags && (sdl.surface->flags & SDL_DOUBLEBUF)) {
				sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,
					sdl.draw.width, sdl.draw.height,
					sdl.surface->format->BitsPerPixel,
					sdl.surface->format->Rmask,
					sdl.surface->format->Gmask,
					sdl.surface->format->Bmask,
				0);
				/* If this one fails be ready for some flickering... */
			}
		}
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
    {
		if(!load_videodrv && sdl.using_windib) {
			LOG_MSG("Resetting to DirectX mode");
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			putenv("SDL_VIDEODRIVER=directx");
			sdl.using_windib=false;
			if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
			GFX_SetIcon(); GFX_SetTitle(-1,-1,-1,false);
			if(!sdl.desktop.fullscreen && GetMenu(GetHWND()) == NULL) DOSBox_RefreshMenu();
		}

		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		SDLScreen_Reset();
		if (!GFX_SetupSurfaceScaled((sdl.desktop.doublebuf && sdl.desktop.fullscreen) ? SDL_DOUBLEBUF : SDL_RESIZABLE,bpp)) goto dosurface;

		
		sdl.blit.rect.top=sdl.clip.y+sdl.overscan_width;
		sdl.blit.rect.left=sdl.clip.x+sdl.overscan_width;
		sdl.blit.rect.right=sdl.clip.x+sdl.clip.w;
		sdl.blit.rect.bottom=sdl.clip.y+sdl.clip.h;
		sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,sdl.draw.width+sdl.overscan_width*2,sdl.draw.height+sdl.overscan_width*2,
				sdl.surface->format->BitsPerPixel,
				sdl.surface->format->Rmask,
				sdl.surface->format->Gmask,
				sdl.surface->format->Bmask,
				0);
		if (!sdl.blit.surface || (!sdl.blit.surface->flags&SDL_HWSURFACE)) {
			if (sdl.blit.surface) {
				SDL_FreeSurface(sdl.blit.surface);
				sdl.blit.surface=0;
			}
			sdl.desktop.want_type=SCREEN_SURFACE;
			LOG_MSG("Failed to create ddraw surface, back to normal surface.");
			goto dosurface;
		}
		switch (sdl.surface->format->BitsPerPixel) {
		case 15:
			retFlags = GFX_CAN_15 | GFX_SCALING | GFX_HARDWARE;
			break;
		case 16:
			retFlags = GFX_CAN_16 | GFX_SCALING | GFX_HARDWARE;
               break;
		case 32:
			retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
               break;
		}
		sdl.desktop.type=SCREEN_SURFACE_DDRAW;
		break;
    }
#endif
	case SCREEN_OVERLAY:
    {
		GFX_ResetSDL();
		if (sdl.overlay) {
			SDL_FreeYUVOverlay(sdl.overlay);
			sdl.overlay=0;
		}
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		SDLScreen_Reset();
		if (!GFX_SetupSurfaceScaled(SDL_RESIZABLE,0)) goto dosurface;
		sdl.overlay=SDL_CreateYUVOverlay(width*2,height,SDL_UYVY_OVERLAY,sdl.surface);
		if (!sdl.overlay) {
			LOG_MSG("SDL:Failed to create overlay, switching back to surface");
			goto dosurface;
		}
		sdl.desktop.type=SCREEN_OVERLAY;
		retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
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
		//if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		// SDLScreen_Reset();
		int texsize=2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
#if SDL_VERSION_ATLEAST(1, 2, 11)
		Section_prop * sec=static_cast<Section_prop *>(control->GetSection("vsync"));
		if(sec) {
			SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, (!strcmp(sec->Get_string("vsyncmode"),"host"))?1:0 );
		}
#endif
		GFX_SetupSurfaceScaled(SDL_OPENGL|SDL_RESIZABLE,0);
		if (!sdl.surface || sdl.surface->format->BitsPerPixel<15) {
			LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}
		/* Create the texture and display list */
		if (sdl.opengl.pixel_buffer_object) {
			glGenBuffersARB(1, &sdl.opengl.buffer);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, width*height*4, NULL, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		} else {
			sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
		}
		sdl.opengl.pitch=width*4;

		//correction for viewport if 640x400
		if(sdl.clip.h < 480 && sdl.desktop.fullscreen) sdl.clip.y=(480-sdl.clip.h)/2;

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
		SDL_GL_SwapBuffers();
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
		glTexCoord2f(0,tex_height); glVertex2f(-1.0f,-1.0f);
		// lower right
		glTexCoord2f(tex_width,tex_height); glVertex2f(1.0f, -1.0f);
		// upper right
		glTexCoord2f(tex_width,0); glVertex2f(1.0f, 1.0f);
		// upper left
		glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
		glEnd();
		glEndList();
		sdl.desktop.type=SCREEN_OPENGL;
		retFlags = GFX_CAN_32 | GFX_SCALING;
		if (sdl.opengl.pixel_buffer_object)
			retFlags |= GFX_HARDWARE;
	break;
		}//OPENGL
#endif	//C_OPENGL
#if (HAVE_D3D9_H) && defined(WIN32)
	    case SCREEN_DIRECT3D: {
		// Calculate texture size
		if((!d3d->square) && (!d3d->pow2)) {
		    d3d->dwTexWidth=width;
		    d3d->dwTexHeight=height;
		} else if(d3d->square) {
		    int texsize=2 << int_log2(width > height ? width : height);
		    d3d->dwTexWidth=d3d->dwTexHeight=texsize;
		} else {
		    d3d->dwTexWidth=2 << int_log2(width);
		    d3d->dwTexHeight=2 << int_log2(height);
		}

		sdl.clip.x=0; sdl.clip.y=0;
		if(sdl.desktop.fullscreen) {
		    if(sdl.desktop.full.fixed) {
			sdl.clip.w=sdl.desktop.full.width;
			sdl.clip.h=sdl.desktop.full.height;
			scalex=(double)sdl.desktop.full.width/width;
			scaley=(double)sdl.desktop.full.height/height;
		    }
		} else {
		    if((sdl.desktop.window.width) && (sdl.desktop.window.height)) {
			scalex=(double)sdl.desktop.window.width/(sdl.draw.width*sdl.draw.scalex);
			scaley=(double)sdl.desktop.window.height/(sdl.draw.height*sdl.draw.scaley);
			if(scalex < scaley) {
			    sdl.clip.w=sdl.desktop.window.width;
			    sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*scalex);
			} else {
			    sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*scaley);
			    sdl.clip.h=(Bit16u)sdl.desktop.window.height;
			}
			scalex=(double)sdl.clip.w/width;
			scaley=(double)sdl.clip.h/height;
		    } else {
			sdl.clip.w=(Bit16u)(width*scalex);
			sdl.clip.h=(Bit16u)(height*scaley);
		    }
		}

		Section_prop *section=static_cast<Section_prop *>(control->GetSection("sdl"));
		if(section) {
		    Prop_multival* prop = section->Get_multival("pixelshader");
		    std::string f = prop->GetSection()->Get_string("force");
		    d3d->LoadPixelShader(prop->GetSection()->Get_string("type"), scalex, scaley, (f == "forced"));
		} else {
		    LOG_MSG("SDL:D3D:Could not get pixelshader info, shader disabled");
		}

		d3d->aspect=RENDER_GetAspect();
		d3d->autofit=RENDER_GetAutofit();
		if((sdl.desktop.fullscreen) && (!sdl.desktop.full.fixed)) {
		    // Don't do aspect ratio correction when fullfixed=false + aspect=false
			if(d3d->aspect == 0)
		    d3d->aspect=2;
	    sdl.clip.w=(Uint16)scalex;
	    sdl.clip.h=(Uint16)scaley;
	    // Do fullscreen scaling if pixel shaders are enabled
	    // or the game uses some weird resolution
	    if((d3d->psActive) || (sdl.clip.w != sdl.clip.h)) {
				sdl.clip.w*=width;
				sdl.clip.h*=height;
			} else { // just use native resolution
				sdl.clip.w=width;
				sdl.clip.h=height;
			}
		} else if(!sdl.desktop.fullscreen) d3d->aspect=-1;

		// Create a dummy sdl surface
		// D3D will hang or crash when using fullscreen with ddraw surface, therefore we hack SDL to provide
		// a GDI window with an additional 0x40 flag. If this fails or stock SDL is used, use WINDIB output
		if(GCC_UNLIKELY(d3d->bpp16)) {
		    sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,16,sdl.desktop.fullscreen ? SDL_FULLSCREEN|0x40 : SDL_RESIZABLE|0x40);
		    retFlags = GFX_CAN_16 | GFX_SCALING;
		} else {
		    sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,0,sdl.desktop.fullscreen ? SDL_FULLSCREEN|0x40 : SDL_RESIZABLE|0x40);
		    retFlags = GFX_CAN_32 | GFX_SCALING;
		}

		if (sdl.surface == NULL) E_Exit("Could not set video mode %ix%i-%i: %s",sdl.clip.w,sdl.clip.h,
					d3d->bpp16 ? 16:32,SDL_GetError());
		sdl.desktop.type=SCREEN_DIRECT3D;

		if(d3d->dynamic) retFlags |= GFX_HARDWARE;

		if(GCC_UNLIKELY(d3d->Resize3DEnvironment(sdl.clip.w,sdl.clip.h,width,
						    height,sdl.desktop.fullscreen) != S_OK)) {
		    retFlags = 0;
		}
#if LOG_D3D
		LOG_MSG("SDL:D3D:Display mode set to: %dx%d with %fx%f scale",
				    sdl.clip.w, sdl.clip.h,sdl.draw.scalex, sdl.draw.scaley);
#endif
		break;
	    }
#endif
	default:
		goto dosurface;
		break;
	}//CASE
	if (retFlags)
		GFX_Start();
	if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
	return retFlags;
}

void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

void GFX_UpdateSDLCaptureState(void) {
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	CPU_Reset_AutoAdjust();
	GFX_SetTitle(-1,-1,-1,false);
}

static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore){
	static bool inited = false;
	if (!inited){
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	} 
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if ( !(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#else
#define sticky_keys(a)
#endif

#ifdef __WIN32__
static void d3d_init(void) {
#if 1
	E_Exit("D3D not supported");
#else
	void change_output(int output);
	change_output(2);
	sdl.desktop.want_type=SCREEN_DIRECT3D;
	if(!load_videodrv && !sdl.using_windib) {
		LOG_MSG("Resetting to WINDIB mode");
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		putenv("SDL_VIDEODRIVER=windib");
		sdl.using_windib=true;
		if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
		GFX_SetIcon(); GFX_SetTitle(-1,-1,-1,false);
		if(!sdl.desktop.fullscreen && GetMenu(GetHWND()) == NULL) DOSBox_RefreshMenu();
	}
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);

	if(!SDL_GetWMInfo(&wmi)) {
		LOG_MSG("SDL:Error retrieving window information");
		LOG_MSG("Failed to get window info");
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else {
		if(sdl.desktop.fullscreen) {
			GFX_CaptureMouse();
		}
		if(d3d) delete d3d;
		d3d = new CDirect3D(640,400);

		if(!d3d) {
			LOG_MSG("Failed to create d3d object");
			sdl.desktop.want_type=SCREEN_SURFACE;
		} else if(d3d->InitializeDX(wmi.window,sdl.desktop.doublebuf) != S_OK) {
			LOG_MSG("Unable to initialize DirectX");
			sdl.desktop.want_type=SCREEN_SURFACE;
		}
	}
#endif
}
#endif

static void openglhq_init(void) {
#ifdef WIN32
	DOSBox_NoMenu(); menu.gui=false;
	HMENU m_handle=GetMenu(GetHWND());
	if(m_handle) RemoveMenu(m_handle,0,0);
	DestroyWindow(GetHWND());
#endif
	sdl.overlay=0;
	char *oldvideo = getenv("SDL_VIDEODRIVER");

	if (oldvideo && strcmp(oldvideo,"openglhq")) {
	    char *driver = (char *)malloc(strlen(oldvideo)+strlen("SDL_OPENGLHQ_VIDEODRIVER=")+1);
	    strcpy(driver,"SDL_OPENGLHQ_VIDEODRIVER=");
	    strcat(driver,oldvideo);
	    putenv(driver);
	    free(driver);
	}
	if (sdl.desktop.doublebuf) putenv((char*)("SDL_OPENGLHQ_DOUBLEBUF=1"));
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	putenv((char*)("SDL_VIDEODRIVER=openglhq"));
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	DOSBox_SetOriginalIcon();
	if(!menu_compatible) { SDL_PumpEvents(); SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); }
	GFX_SetTitle(-1,-1,-1,false);
	sdl.desktop.want_type=SCREEN_OPENGLHQ;
}

void res_init(void) {
	if(sdl.desktop.want_type==SCREEN_OPENGLHQ) return;
	Section * sec = control->GetSection("sdl");
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.desktop.full.fixed=false;
	const char* fullresolution=section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0; sdl.desktop.full.height = 0;
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
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");

	int width=1024, height=768;
	if (!sdl.desktop.full.width) {
		sdl.desktop.full.width=width;
	}
	if (!sdl.desktop.full.height) {
		sdl.desktop.full.height=height;
	}
	if(sdl.desktop.type==SCREEN_SURFACE && !sdl.desktop.fullscreen) return;
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
		sec->ExecuteDestroy(false);
		if(type) {
			std::string tmp("windowresolution="); tmp.append(win_res);
			sec->HandleInputline(tmp);
		} else {
			std::string tmp("fullresolution="); tmp.append(win_res);
			sec->HandleInputline(tmp); }
		sec->ExecuteInit(false);

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
#ifdef WIN32
		sdl.surface=SDL_SetVideoMode(640,400,0,SDL_HWSURFACE|SDL_HWPALETTE);
		sdl.desktop.want_type=SCREEN_SURFACE_DDRAW;
#else
		sdl.desktop.want_type=SCREEN_SURFACE;
#endif
		break;
	case 2:
		sdl.desktop.want_type=SCREEN_OVERLAY;
		break;
	case 3:
		change_output(2);
		sdl.desktop.want_type=SCREEN_OPENGL;
		break;
	case 4:
		change_output(2);
		sdl.desktop.want_type=SCREEN_OPENGL;
		break;
#ifdef __WIN32__
	case 5:
		sdl.desktop.want_type=SCREEN_DIRECT3D;
		d3d_init();
		break;
#endif
	case 6: {
#ifdef __WIN32__
		if (MessageBox(GetHWND(),"GUI will be disabled if output is set to OpenglHQ. Do you want to continue?","Warning",MB_YESNO)==IDNO) {
			GFX_Stop(); GFX_Start(); return;
		}
#endif
		openglhq_init();
		}
		break;
	case 7:
		// do not set want_type
		break;
	case 8:
		if(sdl.desktop.want_type==SCREEN_OPENGL) { }
#ifdef WIN32
		else if(sdl.desktop.want_type==SCREEN_DIRECT3D) { if(sdl.desktop.fullscreen) GFX_CaptureMouse(); d3d_init(); }
#endif
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
				sec->ExecuteInit(false);
			}
		}
	}
	res_init();

	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	if(sdl.desktop.want_type==SCREEN_OPENGLHQ) {
		if(!render.scale.hardware) SetVal("render","scaler",!render.scale.forced?"hardware2x":"hardware2x forced");
		if(!menu.compatible) {
			SDL_PumpEvents();
			SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
		}
		static SDL_Surface* screen_surf;
		Bit32u rmask = 0x000000ff;
		Bit32u gmask = 0x0000ff00;
		Bit32u bmask = 0x00ff0000;
		screen_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
		Bit32u lasttick=GetTicks();
		for(Bitu i = 0; i <=5; i++) {
			if((GetTicks()-lasttick)>20) i++;
			while((GetTicks()-lasttick)<15) SDL_Delay(5);
			lasttick = GetTicks();
			SDL_SetAlpha(screen_surf, SDL_SRCALPHA,(Bit8u)(51*i));
			SDL_BlitSurface(screen_surf, NULL, sdl.surface, NULL);
			SDL_Flip(sdl.surface);
		}
		SDL_FreeSurface(screen_surf);
	}
	GFX_SetTitle(CPU_CycleMax,-1,-1,false);
}


void GFX_SwitchFullScreen(void) {
    menu.resizeusing=true;
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if(sdl.desktop.want_type != SCREEN_OPENGLHQ) { if(menu.gui) SetMenu(GetHWND(),NULL); }
		if (!sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(false); //disable sticky keys in fullscreen mode
#endif
	} else {
		if (sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)		
		sticky_keys(true); //restore sticky keys to default state in windowed mode.
#endif
	}
	GFX_ResetScreen();
#ifdef WIN32
	if(menu.startup) {
		Section_prop * sec=static_cast<Section_prop *>(control->GetSection("vsync"));
		if(sec) {
			if(!strcmp(sec->Get_string("vsyncmode"),"host")) SetVal("vsync","vsyncmode","host");
		}
	}
#endif //WIN32
}

static void SwitchFullScreen(bool pressed) {
	if (!pressed)
		return;

	GFX_LosingFocus();
	if (sdl.desktop.lazy_fullscreen) {
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
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
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
	if (!sdl.active || sdl.updating)
		return false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (sdl.blit.surface) {
			if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
				return false;
			pixels=(Bit8u *)sdl.blit.surface->pixels;
			pitch=sdl.blit.surface->pitch;
		} else {
			if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
				return false;
			pixels=(Bit8u *)sdl.surface->pixels;
			pixels+=sdl.clip.y*sdl.surface->pitch;
			pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
			pitch=sdl.surface->pitch;
		}
        SDL_Overscan();
		sdl.updating=true;
		return true;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (SDL_LockSurface(sdl.blit.surface)) {
//			LOG_MSG("SDL Lock failed");
			return false;
		}
		pixels=(Bit8u *)sdl.blit.surface->pixels;
		pitch=sdl.blit.surface->pitch;
        SDL_Overscan();
		sdl.updating=true;
		return true;
#endif
	case SCREEN_OVERLAY:
		if (SDL_LockYUVOverlay(sdl.overlay)) return false;
		pixels=(Bit8u *)*(sdl.overlay->pixels);
		pitch=*(sdl.overlay->pitches);
		sdl.updating=true;
		return true;
#if C_OPENGL
	case SCREEN_OPENGL:
		if(sdl.opengl.pixel_buffer_object) {
		    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
		    pixels=(Bit8u *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
		} else
		    pixels=(Bit8u *)sdl.opengl.framebuf;
		pitch=sdl.opengl.pitch;
		sdl.updating=true;
		return true;
#endif
#if (HAVE_D3D9_H) && defined(WIN32)
	case SCREEN_DIRECT3D:
		sdl.updating=d3d->LockTexture(pixels, pitch);
		return sdl.updating;
#endif
	default:
		break;
	}
	return false;
}


void GFX_EndUpdate( const Bit16u *changedLines ) {
#if (HAVE_DDRAW_H) && defined(WIN32)
	int ret;
#endif

#if (HAVE_D3D9_H) && defined(WIN32)
	if (d3d && d3d->getForceUpdate()); // continue
	else
#endif
	if (!sdl.updating)
		return;
	sdl.updating=false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (SDL_MUSTLOCK(sdl.surface)) {
			if (sdl.blit.surface) {
				SDL_UnlockSurface(sdl.blit.surface);
				int Blit = SDL_BlitSurface( sdl.blit.surface, 0, sdl.surface, &sdl.clip );
				LOG(LOG_MISC,LOG_WARN)("BlitSurface returned %d",Blit);
			} else {
				SDL_UnlockSurface(sdl.surface);
			}
	if(changedLines && (changedLines[0] == sdl.draw.height)) 
	return; 
	if(!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
			SDL_Flip(sdl.surface);
		} else if (changedLines) {
	if(changedLines[0] == sdl.draw.height) 
	return; 
	if(!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
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
				SDL_UpdateRects( sdl.surface, rectCount, sdl.updateRects );
		}
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		SDL_UnlockSurface(sdl.blit.surface);
	if(changedLines && (changedLines[0] == sdl.draw.height)) 
	return; 
	if(!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
		ret=IDirectDrawSurface3_Blt(
			sdl.surface->hwdata->dd_writebuf,&sdl.blit.rect,
			sdl.blit.surface->hwdata->dd_surface,0,
			DDBLT_WAIT, NULL);
		switch (ret) {
		case DD_OK:
			break;
		case DDERR_SURFACELOST:
			IDirectDrawSurface3_Restore(sdl.blit.surface->hwdata->dd_surface);
			IDirectDrawSurface3_Restore(sdl.surface->hwdata->dd_surface);
			break;
		default:
			LOG_MSG("DDRAW:Failed to blit, error %X",ret);
		}
		SDL_Flip(sdl.surface);
		break;
#endif
	case SCREEN_OVERLAY:
		SDL_UnlockYUVOverlay(sdl.overlay);
		if(changedLines && (changedLines[0] == sdl.draw.height)) 
		return; 
		if(!menu.hidecycles && !sdl.desktop.fullscreen) frames++; 
		SDL_DisplayYUVOverlay(sdl.overlay,&sdl.clip);
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
			SDL_GL_SwapBuffers();
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
		}
		if(!menu.hidecycles && !sdl.desktop.fullscreen) frames++; 
		SDL_GL_SwapBuffers();
		break;
#endif
#if (HAVE_D3D9_H) && defined(WIN32)
	case SCREEN_DIRECT3D:
		if(!menu.hidecycles) frames++; //implemented
		if(GCC_UNLIKELY(!d3d->UnlockTexture(changedLines))) {
			E_Exit("Failed to draw screen!");
		}
		break;
#endif
	default:
		break;
	}
}


void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
	/* I should probably not change the GFX_PalEntry :) */
	if (sdl.surface->flags & SDL_HWPALETTE) {
		if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	} else {
		if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
	case SCREEN_SURFACE_DDRAW:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_OVERLAY:
		{
			Bit8u y =  ( 9797*(red) + 19237*(green) +  3734*(blue) ) >> 15;
			Bit8u u =  (18492*((blue)-(y)) >> 15) + 128;
			Bit8u v =  (23372*((red)-(y)) >> 15) + 128;
#ifdef WORDS_BIGENDIAN
			return (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
			return (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
		}
	case SCREEN_OPENGL:
//		return ((red << 0) | (green << 8) | (blue << 16)) | (255 << 24);
		//USE BGRA
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	case SCREEN_DIRECT3D:
#if (HAVE_D3D9_H) && defined(WIN32)
		if(GCC_UNLIKELY(d3d->bpp16))
		    return SDL_MapRGB(sdl.surface->format,red,green,blue);
		else
#endif
		    return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
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

static void GUI_ShutDown(Section * /*sec*/) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
#if (HAVE_D3D9_H) && defined(WIN32)
	if ((sdl.desktop.type==SCREEN_DIRECT3D) && (d3d)) delete d3d;
#endif
}


static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest
// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus ) &&
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
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
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				if (map & 0x80) *((Bit32u*)(draw_line+j))=color; else *((Bit32u*)(draw_line+j))=color2;
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

static SDL_Surface* splash_surf;
static bool			splash_active;
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
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//				*draw++ = tmpbuf[x*3+2]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+0]*0x10000+0x00000000;
//#else
				*draw++ = tmpbuf[x*3+0]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+2]*0x10000+0x00000000;
//#endif
			}
		}
		Bit32u lasttick=GetTicks();
		for(Bitu i = 0; i <=5; i++) {
			if((GetTicks()-lasttick)>20) i++;
			while((GetTicks()-lasttick)<15) SDL_Delay(5);
			lasttick = GetTicks();
			SDL_SetAlpha(splash_surf, SDL_SRCALPHA,(Bit8u)(51*i));
			SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
			SDL_Flip(sdl.surface);
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

#if (HAVE_D3D9_H) && defined(WIN32)
# include "SDL_syswm.h"
#endif

#if (HAVE_D3D9_H) && defined(WIN32)
static void D3D_reconfigure(Section * sec) {
	if (d3d) {
		Section_prop *section=static_cast<Section_prop *>(sec);
		Prop_multival* prop = section->Get_multival("pixelshader");
		if(SUCCEEDED(d3d->LoadPixelShader(prop->GetSection()->Get_string("type"), 0, 0))) {
			GFX_ResetScreen();
		}
	}
}
#endif

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;

	GFX_SetIcon();

	sdl.desktop.lazy_fullscreen=false;
	sdl.desktop.lazy_fullscreen_req=false;

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.wait_on_error=section->Get_bool("waitonerror");

	Prop_multival* p=section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if      (focus == "lowest")  { sdl.priority.focus = PRIORITY_LEVEL_LOWEST;  }
	else if (focus == "lower")   { sdl.priority.focus = PRIORITY_LEVEL_LOWER;   }
	else if (focus == "normal")  { sdl.priority.focus = PRIORITY_LEVEL_NORMAL;  }
	else if (focus == "higher")  { sdl.priority.focus = PRIORITY_LEVEL_HIGHER;  }
	else if (focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if      (notfocus == "lowest")  { sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;  }
	else if (notfocus == "lower")   { sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;   }
	else if (notfocus == "normal")  { sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;  }
	else if (notfocus == "higher")  { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;  }
	else if (notfocus == "highest") { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST; }
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
		strncpy( res, fullresolution, sizeof( res ));
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
		strncpy( res,windowresolution, sizeof( res ));
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
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");
#if SDL_VERSION_ATLEAST(1, 2, 10)
	if (!sdl.desktop.full.width || !sdl.desktop.full.height){
		//Can only be done on the very first call! Not restartable.
		const SDL_VideoInfo* vidinfo = SDL_GetVideoInfo();
		if (vidinfo) {
			sdl.desktop.full.width = vidinfo->current_w;
			sdl.desktop.full.height = vidinfo->current_h;
		}
	}
#endif

	int width=1024; int height=768;
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
	if(sdl.desktop.fullscreen) GFX_CaptureMouse();

	if (output == "surface") {
		sdl.desktop.want_type=SCREEN_SURFACE;
#if (HAVE_DDRAW_H) && defined(WIN32)
	} else if (output == "ddraw") {
		sdl.desktop.want_type=SCREEN_SURFACE_DDRAW;
#endif
	} else if (output == "overlay") {
		sdl.desktop.want_type=SCREEN_OVERLAY;
#if C_OPENGL
	} else if (output == "opengl") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=true;
	} else if (output == "openglnb") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=false;
#endif
#if (HAVE_D3D9_H) && defined(WIN32)
	} else if (output == "direct3d") {
		sdl.desktop.want_type=SCREEN_DIRECT3D;
#if LOG_D3D
		LOG_MSG("SDL:Direct3D activated");
#endif
#endif
	} else if (output == "openglhq") {
		char *oldvideo = getenv("SDL_VIDEODRIVER");

		if (oldvideo && strcmp(oldvideo,"openglhq")) {
		    char *driver = (char *)malloc(strlen(oldvideo)+strlen("SDL_OPENGLHQ_VIDEODRIVER=")+1);
		    strcpy(driver,"SDL_OPENGLHQ_VIDEODRIVER=");
		    strcat(driver,oldvideo);
		    putenv(driver);
		    free(driver);
		}
		if (sdl.desktop.doublebuf) putenv((char*)("SDL_OPENGLHQ_DOUBLEBUF=1"));
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		putenv((char*)("SDL_VIDEODRIVER=openglhq"));
		SDL_InitSubSystem(SDL_INIT_VIDEO);
		sdl.desktop.want_type=SCREEN_OPENGLHQ;

	} else {
		LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output.c_str());
		sdl.desktop.want_type=SCREEN_SURFACE;//SHOULDN'T BE POSSIBLE anymore
	}
	sdl.overscan_width=section->Get_int("overscan");
//	sdl.overscan_color=section->Get_int("overscancolor");

	sdl.overlay=0;
	/* Initialize screen for first time */
	sdl.surface=SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
	if (sdl.surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
	sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
	}
#if (HAVE_D3D9_H) && defined(WIN32)
	if(sdl.desktop.want_type==SCREEN_DIRECT3D) {
	    SDL_SysWMinfo wmi;
	    SDL_VERSION(&wmi.version);

	    if(!SDL_GetWMInfo(&wmi)) {
			LOG_MSG("SDL:Error retrieving window information");
			LOG_MSG("Failed to get window info");
			sdl.desktop.want_type=SCREEN_SURFACE;
	    } else {
			if(d3d) delete d3d;
			d3d = new CDirect3D(640,400);

			if(!d3d) {
				LOG_MSG("Failed to create d3d object");
				sdl.desktop.want_type=SCREEN_SURFACE;
			} else if(d3d->InitializeDX(wmi.window,sdl.desktop.doublebuf) != S_OK) {
				LOG_MSG("Unable to initialize DirectX");
				sdl.desktop.want_type=SCREEN_SURFACE;
			}
		}
	}
#endif

	GFX_Stop();
	SDL_WM_SetCaption("DOSBox",VERSION);

/* Please leave the Splash screen stuff in working order in DOSBox. We spend a lot of time making DOSBox. */
	ShowSplashScreen();

	/* Get some Event handlers */
#ifdef __WIN32__
	MAPPER_AddHandler(ToggleMenu,MK_return,MMOD1|MMOD2,"togglemenu","ToggleMenu");
#endif // WIN32
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
	MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause");
#endif
	MAPPER_AddHandler(&UI_Run, MK_f10, MMOD2, "gui", "ShowGUI");
	/* Get Keyboard state of numlock and capslock */
	SDLMod keystate = SDL_GetModState();
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
#ifdef __WIN32__
   width=sdl.clip.w; 
   height=sdl.clip.h;
#else
	width=sdl.draw.width; 
	height=sdl.draw.height;
#endif
	void RENDER_CallBack( GFX_CallBackFunctions_t function );
	while (sdl.desktop.fullscreen) {
		int temp_size;
		temp_size=render.scale.size;
		if(!sdl.desktop.fullscreen) { render.scale.size=temp_size; RENDER_CallBack( GFX_CallBackReset); return; }
    }
#ifdef WIN32
	if(menu.resizeusing) {
		RENDER_CallBack( GFX_CallBackReset);
		return;
	}
#endif
	if(nWidth == width && nHeight == height) {
		RENDER_CallBack( GFX_CallBackReset);
		return;
	}
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl")); 
	if ((!strcmp(section->Get_string("windowresolution"),"original") || (!strcmp(section->Get_string("windowresolution"),"desktop"))) && (render.src.dblw && render.src.dblh)) {
		switch (render.scale.op) {
			case scalerOpNormal:
				if(!render.scale.hardware) {
					if(nWidth>width || nHeight>height) {
						if (render.scale.size <= 4 && render.scale.size >=1) ++render.scale.size; break;
					} else {
						if (render.scale.size <= 5 && render.scale.size >= 2) --render.scale.size; break;
					}
				} else {
					if(nWidth>width || nHeight>height) {
						if (render.scale.size == 1) { render.scale.size=4; break; }
						if (render.scale.size == 4) { render.scale.size=6; break; }
						if (render.scale.size == 6) { render.scale.size=8; break; }
						if (render.scale.size == 8) { render.scale.size=10; break; }
					}
					if(nWidth<width || nHeight<height) {
						if (render.scale.size == 10) { render.scale.size=8; break; }
						if (render.scale.size == 8) { render.scale.size=6; break; }
						if (render.scale.size == 6) { render.scale.size=4; break; }
						if (render.scale.size == 4) { render.scale.size=1; break; }
					}
				}
				break;
			case scalerOpAdvMame:
			case scalerOpHQ:
			case scalerOpAdvInterp:
			case scalerOpTV:
			case scalerOpRGB:
			case scalerOpScan:
				if(nWidth>width || nHeight>height) { if (render.scale.size == 2) ++render.scale.size; }
				if(nWidth<width || nHeight<height) { if (render.scale.size == 3) --render.scale.size; }
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

static void HandleVideoResize(void * event) {
	if(sdl.desktop.fullscreen) return;

	SDL_ResizeEvent* ResizeEvent = (SDL_ResizeEvent*)event;
	RedrawScreen(ResizeEvent->w, ResizeEvent->h);
/*	if(sdl.desktop.want_type!=SCREEN_DIRECT3D) {
		HWND hwnd=GetHWND();
		RECT myrect;
		GetClientRect(hwnd,&myrect);
		if(myrect.right==GetSystemMetrics(SM_CXSCREEN)) 
			GFX_SwitchFullScreen();
	} */
#ifdef WIN32
	menu.resizeusing=false;
#endif
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (sdl.mouse.locked || !sdl.mouse.autoenable)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.sensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
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
		}
		break;
	}
}

void GFX_LosingFocus(void) {
	sdl.laltstate=SDL_KEYUP;
	sdl.raltstate=SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen(void) {
	return sdl.desktop.fullscreen;
}

#ifdef __WIN32__
void OpenFileDialog( char * path_arg ) {
	if(control->SecureMode()) {
		LOG_MSG(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}
	DOS_MCB mcb(dos.psp()-1);
	static char pcname[9];
	mcb.GetFileName(pcname);
	if(strlen(pcname)) return;

	OPENFILENAME OpenFileName;
	char szFile[MAX_PATH];
	char CurrentDir[MAX_PATH];
	const char * Temp_CurrentDir = CurrentDir;

	if (Drives['C'-'A']) {
		if (MessageBox(GetHWND(),
			"Quick launch automatically mounts drive C in DOSBox.\nDrive C has already been mounted. Do you want to continue?",
			"Warning",MB_YESNO)==IDNO) return;
	}

	if(path_arg) goto search;
	szFile[0] = 0;

	GetCurrentDirectory( MAX_PATH, CurrentDir );

	OpenFileName.lStructSize = sizeof( OPENFILENAME );
	OpenFileName.hwndOwner = NULL;
	if(DOSBox_Kor())
		OpenFileName.lpstrFilter = "실행 파일(*.com, *.exe, *.bat)\0*.com;*.exe;*.bat\0모든 파일(*.*)\0*.*\0";
	else
		OpenFileName.lpstrFilter = "Executable files(*.com, *.exe, *.bat)\0*.com;*.exe;*.bat\0All files(*.*)\0*.*\0";
	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.nFilterIndex = 0;
	OpenFileName.lpstrFile = szFile;
	OpenFileName.nMaxFile = sizeof( szFile );
	OpenFileName.lpstrFileTitle = NULL;
	OpenFileName.nMaxFileTitle = 0;
	OpenFileName.lpstrInitialDir = CurrentDir;
	OpenFileName.lpstrTitle = "Select an executable";
	OpenFileName.nFileOffset = 0;
	OpenFileName.nFileExtension = 0;
	OpenFileName.lpstrDefExt = NULL;
	OpenFileName.lCustData = 0;
	OpenFileName.lpfnHook = NULL;
	OpenFileName.lpTemplateName = NULL;
	OpenFileName.Flags = OFN_EXPLORER;

search:
	if(GetOpenFileName( &OpenFileName ) || path_arg) {
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		char drive	[_MAX_DRIVE]; 
		char dir	[_MAX_DIR]; 
		char fname	[_MAX_FNAME]; 
		char ext	[_MAX_EXT]; 
		char * path = 0;
		if(path_arg) {
			szFile[0] = 0;
			sprintf(szFile,path_arg);
		}
		path = szFile;
		_splitpath (path, drive, dir, fname, ext);
		char ext_temp [_MAX_EXT]; ext_temp[0] = 0; sprintf(ext_temp,ext);

		hFind = FindFirstFile(szFile, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			if(strcasecmp(ext,"")) goto search;
			szFile[0] = 0;
			ext[0] = 0; sprintf(ext,".com");
			sprintf(szFile,"%s%s%s%s",drive,dir,fname,".com");
			hFind = FindFirstFile(szFile, &FindFileData);
			if (hFind == INVALID_HANDLE_VALUE) {
				szFile[0] = 0;
				ext[0] = 0; sprintf(ext,".exe");
				sprintf(szFile,"%s%s%s%s",drive,dir,fname,".exe");
				hFind = FindFirstFile(szFile, &FindFileData);
				if (hFind == INVALID_HANDLE_VALUE) {
					szFile[0] = 0;
					ext[0] = 0; sprintf(ext,".bat");
					sprintf(szFile,"%s%s%s%s",drive,dir,fname,".bat");
					hFind = FindFirstFile(szFile, &FindFileData);
					if (hFind == INVALID_HANDLE_VALUE) {
						szFile[0] = 0;
						ext[0]=0;
						goto search;
					}
				}
			}
		}

		if((!strcmp(ext,".com")) || (!strcmp(ext,".exe")) || (!strcmp(ext,".bat"))) {
			char pathname[DOS_PATHLENGTH];
			sprintf(pathname,"%s%s",drive,dir);
			MountDrive_2('C',pathname,"LOCAL");
			DOS_SetDrive(toupper('C') - 'A');
		} else {
			LOG_MSG("GUI: Unsupported filename extension.");
			goto godefault;
		}

		#define DOSNAMEBUF 256
		char name1[DOSNAMEBUF+1];
		sprintf(name1,"%s%s",fname,ext);
		Bit16u n=1; Bit8u c='\n';
		DOS_WriteFile(STDOUT,&c,&n);

		DOS_Shell shell;
		DOS_MCB mcb(dos.psp()-1);
		static char name[9];
		mcb.GetFileName(name);

		SetCurrentDirectory( Temp_CurrentDir );
		do {
			shell.Execute(name1," ");
			if(!strcmp(ext,".bat")) shell.RunInternal();
			if (!strlen(name)) break;
		} while (1);

		if(strcmp(ext,".bat")) DOS_WriteFile(STDOUT,&c,&n);
		shell.ShowPrompt();
	}

godefault:
	SetCurrentDirectory( Temp_CurrentDir );
	return;
}

void Go_Boot(const char boot_drive[_MAX_DRIVE]) {
		if(control->SecureMode()) {
			LOG_MSG(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}

	OPENFILENAME OpenFileName;
	char szFile[MAX_PATH];
	char CurrentDir[MAX_PATH];
	const char * Temp_CurrentDir = CurrentDir;
	szFile[0] = 0;
	GetCurrentDirectory( MAX_PATH, CurrentDir );

	OpenFileName.lStructSize = sizeof( OPENFILENAME );
	OpenFileName.hwndOwner = NULL;

	if(DOSBox_Kor())
		OpenFileName.lpstrFilter = "이미지 파일(*.img, *.ima, *.pcjr, *.jrc)\0*.pcjr;*.img;*.ima;*.jrc\0모든 파일(*.*)\0*.*\0";
	else
		OpenFileName.lpstrFilter = "Image files(*.img, *.ima, *.pcjr, *.jrc)\0*.pcjr;*.img;*.ima;*.jrc\0All files(*.*)\0*.*\0";

	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.nFilterIndex = 0;
	OpenFileName.lpstrFile = szFile;
	OpenFileName.nMaxFile = sizeof( szFile );
	OpenFileName.lpstrFileTitle = NULL;
	OpenFileName.nMaxFileTitle = 0;
	OpenFileName.lpstrInitialDir = CurrentDir;
	OpenFileName.lpstrTitle = "Select an image file";
	OpenFileName.nFileOffset = 0;
	OpenFileName.nFileExtension = 0;
	OpenFileName.lpstrDefExt = NULL;
	OpenFileName.lCustData = 0;
	OpenFileName.lpfnHook = NULL;
	OpenFileName.lpTemplateName = NULL;
	OpenFileName.Flags = OFN_EXPLORER;
search:
	if(GetOpenFileName( &OpenFileName )) {
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		char drive	[_MAX_DRIVE]; 
		char dir	[_MAX_DIR]; 
		char fname	[_MAX_FNAME]; 
		char ext	[_MAX_EXT]; 
		char * path = 0;
		path = szFile;
		_splitpath (path, drive, dir, fname, ext);
		char ext_temp [_MAX_EXT]; ext_temp[0] = 0; sprintf(ext_temp,ext);

		hFind = FindFirstFile(szFile, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) goto search;

		if((!strcmp(ext,".img")) || (!strcmp(ext,".pcjr")) || (!strcmp(ext,".jrc")) || (!strcmp(ext,".ima"))) {
			extern Bitu ZDRIVE_NUM;
			char root[4] = {'A'+ZDRIVE_NUM,':','\\',0};
			char cmd[20];
			DOS_Shell shell;
			Bit16u n=1; Bit8u c='\n';
			if(strcmp(boot_drive,"a")) {
				char szFile_pre[MAX_PATH];
				szFile_pre[0] = 0;
				strcpy(szFile_pre,boot_drive);
				strcat(szFile_pre," -t hdd "); 
				strcat(szFile_pre,szFile);
				DOS_WriteFile(STDOUT,&c,&n);
				cmd[0] = 0;
				strcpy(cmd,root);
				strcat(cmd,"imgmount.com");
				shell.Execute(cmd,szFile_pre);
				shell.RunInternal();
			}
			DOS_WriteFile(STDOUT,&c,&n);
			strcat(szFile," -l ");
			strcat(szFile,boot_drive);
			cmd[0] = 0;
			strcpy(cmd,root);
			strcat(cmd,"boot.com");
			shell.Execute(cmd,szFile);
			shell.RunInternal();
			DOS_WriteFile(STDOUT,&c,&n);
			shell.ShowPrompt(); // if failed
		} else {
			LOG_MSG("GUI: Unsupported filename extension.");
			goto godefault;
		}
	}

godefault:
	SetCurrentDirectory( Temp_CurrentDir );
	return;
}

void Go_Boot2(const char boot_drive[_MAX_DRIVE]) {
	Bit16u n=1; Bit8u c='\n';
	DOS_WriteFile(STDOUT,&c,&n);
	char temp[7];
	extern Bitu ZDRIVE_NUM;
	char root[4] = {'A'+ZDRIVE_NUM,':','\\',0};
	char cmd[20];
	temp[0] = 0;
	cmd[0] = 0;
	strcpy(cmd,root);
	strcat(cmd,"boot.com");
	strcpy(temp,"-l ");
	strcat(temp,boot_drive);
	DOS_Shell shell;
	shell.Execute(cmd,temp);
	shell.RunInternal();
	DOS_WriteFile(STDOUT,&c,&n);
	shell.ShowPrompt(); // if failed
}

void Drag_Drop( char * path_arg ) {
	if(control->SecureMode()) {
		LOG_MSG(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}
	DOS_MCB mcb(dos.psp()-1);
	static char name[9];
	mcb.GetFileName(name);
	if((!path_arg) || (strlen(name)))  return;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	char drive	[_MAX_DRIVE]; 
	char dir	[_MAX_DIR]; 
	char fname	[_MAX_FNAME]; 
	char ext	[_MAX_EXT]; 
	char szFile[MAX_PATH];

	szFile[0] = 0;
	sprintf(szFile,path_arg);
	char * path = szFile;
	_splitpath (path, drive, dir, fname, ext);
	char ext_temp [_MAX_EXT];
	ext_temp[0] = 0;
	sprintf(ext_temp,ext);

	hFind = FindFirstFile(szFile, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) return;

	if((!strcmp(ext,".com")) || (!strcmp(ext,".exe")) || (!strcmp(ext,".bat")))
		OpenFileDialog(path_arg);
	else
		LOG_MSG("GUI: Unsupported filename extension.");
}

HHOOK hhk;
LRESULT CALLBACK CBTProc(INT nCode, WPARAM wParam, LPARAM lParam) {
	lParam;
	if( HCBT_ACTIVATE == nCode ) {
		HWND hChildWnd;
		hChildWnd = (HWND)wParam;
		SetDlgItemText(hChildWnd,IDYES,"CD-ROM");
		SetDlgItemText(hChildWnd,IDNO,"Floppy");
		SetDlgItemText(hChildWnd,IDCANCEL,"Harddisk");
		UnhookWindowsHookEx(hhk);
	}
	CallNextHookEx(hhk, nCode, wParam, lParam);
	return 0;
}

int MountMessageBox( HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType ) {
	hhk = SetWindowsHookEx( WH_CBT, &CBTProc, 0, GetCurrentThreadId() );
	const int iRes = MessageBox( hWnd, lpText, lpCaption, uType | MB_SETFOREGROUND );
		return iRes;
}

void OpenFileDialog_Img( char drive ) {
		if(control->SecureMode()) {
			LOG_MSG(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}
	if (Drives[drive-'A']) {
		LOG_MSG("GUI: Unmount drive %c first, and then try again.",drive);
		return;
	}
	OPENFILENAME OpenFileName;
	char szFile[MAX_PATH];
	char CurrentDir[MAX_PATH];
	const char * Temp_CurrentDir = CurrentDir;

	szFile[0] = 0;
	GetCurrentDirectory( MAX_PATH, CurrentDir );
	OpenFileName.lStructSize = sizeof( OPENFILENAME );
	OpenFileName.hwndOwner = NULL;

	if(DOSBox_Kor())
		OpenFileName.lpstrFilter = "이미지/ZIP 파일(*.ima, *.img, *.iso, *.cue, *.bin, *.mdf, *.zip, *.7z)\0*.ima;*.img;*.iso;*.mdf;*.zip;*.cue;*.bin;*.7z\0모든 파일(*.*)\0*.*\0";
	else
		OpenFileName.lpstrFilter = "Image/Zip files(*.ima, *.img, *.iso, *.cue, *.bin, *.mdf, *.zip, *.7z)\0*.ima;*.img;*.iso;*.mdf;*.zip;*.cue;*.bin;*.7z\0All files(*.*)\0*.*\0";

	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.nFilterIndex = 0;
	OpenFileName.lpstrFile = szFile;
	OpenFileName.nMaxFile = sizeof( szFile );
	OpenFileName.lpstrFileTitle = NULL;
	OpenFileName.nMaxFileTitle = 0;
	OpenFileName.lpstrInitialDir = CurrentDir;
	OpenFileName.lpstrTitle = "Select an image file";
	OpenFileName.nFileOffset = 0;
	OpenFileName.nFileExtension = 0;
	OpenFileName.lpstrDefExt = NULL;
	OpenFileName.lCustData = 0;
	OpenFileName.lpfnHook = NULL;
	OpenFileName.lpTemplateName = NULL;
	OpenFileName.Flags = OFN_EXPLORER;

search:
	if(GetOpenFileName( &OpenFileName )) {
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		hFind = FindFirstFile(szFile, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) goto search;
		char drive2	[_MAX_DRIVE]; 
		char dir	[_MAX_DIR]; 
		char fname	[_MAX_FNAME]; 
		char ext	[_MAX_EXT]; 
		char * path = szFile;

		_splitpath (path, drive2, dir, fname, ext);

		if((!strcmp(ext,".img")) || (!strcmp(ext,".iso")) || (!strcmp(ext,".cue")) || (!strcmp(ext,".bin")) || (!strcmp(ext,".mdf"))) {
			if(!strcmp(ext,".img")) {
				int whichval=MountMessageBox(GetHWND(),"Drive type:","Mount as Image",MB_YESNOCANCEL);
				if(whichval == IDYES) Mount_Img(drive,path);// CD-ROM
				else if(whichval == IDNO) Mount_Img_Floppy(drive,path); // Floppy
				else if(whichval == IDCANCEL)  Mount_Img_HDD(drive,path);// Harddisk
			} else
				Mount_Img(drive,path);
		} else if(!strcmp(ext,".ima")) {
			Mount_Img_Floppy(drive,path);
		} else
			LOG_MSG("GUI: Unsupported filename extension.");
	}
	SetCurrentDirectory( Temp_CurrentDir );
}

void D3D_PS(void) {
	OPENFILENAME OpenFileName;
	char szFile[MAX_PATH];
	char CurrentDir[MAX_PATH];
	const char * Temp_CurrentDir = CurrentDir;
	szFile[0] = 0;

	GetCurrentDirectory( MAX_PATH, CurrentDir );

	OpenFileName.lStructSize = sizeof( OPENFILENAME );
	OpenFileName.hwndOwner = NULL;
	if(DOSBox_Kor())
		OpenFileName.lpstrFilter = "효과 파일(*.fx)\0*.fx\0모든 파일(*.*)\0*.*\0";
	else
		OpenFileName.lpstrFilter = "Effect files(*.fx)\0*.fx\0All files(*.*)\0*.*\0";
	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.nFilterIndex = 0;
	OpenFileName.lpstrFile = szFile;
	OpenFileName.nMaxFile = sizeof( szFile );
	OpenFileName.lpstrFileTitle = NULL;
	OpenFileName.nMaxFileTitle = 0;
	OpenFileName.lpstrInitialDir = ".\\Shaders";;
	OpenFileName.lpstrTitle = "Select an effect file";
	OpenFileName.nFileOffset = 0;
	OpenFileName.nFileExtension = 0;
	OpenFileName.lpstrDefExt = NULL;
	OpenFileName.lCustData = 0;
	OpenFileName.lpfnHook = NULL;
	OpenFileName.lpTemplateName = NULL;
	OpenFileName.Flags = OFN_EXPLORER;

search:
	if(GetOpenFileName( &OpenFileName )) {
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		char drive	[_MAX_DRIVE]; 
		char dir	[_MAX_DIR]; 
		char fname	[_MAX_FNAME]; 
		char ext	[_MAX_EXT]; 
		char * path = 0;
		path = szFile;
		_splitpath (path, drive, dir, fname, ext);

		if(!strcmp(ext,".fx")) {
			if(!strcmp(fname,"none")) { SetVal("sdl","pixelshader","none"); goto godefault; }
			if (sdl.desktop.want_type != SCREEN_DIRECT3D) MessageBox(GetHWND(),
				"Set output to Direct3D for the changes to take effect", "Warning", 0);
			if (MessageBox(GetHWND(),
				"Always enable this pixelshader under any circumstances in Direct3D?" \
				"\nIf yes, the shader will be used even if the result might not be desired.",
				fname, MB_YESNO) == IDYES) strcat(fname, ".fx forced");
			else strcat(fname, ".fx");
			SetVal("sdl","pixelshader",fname);
		} else {
			LOG_MSG("GUI: Unsupported filename extension.");
			goto godefault;
		}
	}

godefault:
	SetCurrentDirectory( Temp_CurrentDir );
	return;
}

void* GetSetSDLValue(int isget, std::string target, void* setval) {
	if (target == "wait_on_error") {
		if (isget) return (void*) sdl.wait_on_error;
		else sdl.wait_on_error = setval;
/*
	} else if (target == "draw.callback") {
		if (isget) return (void*) sdl.draw.callback;
		else sdl.draw.callback = *static_cast<GFX_CallBack_t*>(setval);
	} else if (target == "desktop.full.width") {
		if (isget) return (void*) sdl.desktop.full.width;
		else sdl.desktop.full.width = *static_cast<Bit16u*>(setval);
	} else if (target == "desktop.full.height") {
		if (isget) return (void*) sdl.desktop.full.height;
		else sdl.desktop.full.height = *static_cast<Bit16u*>(setval);
	} else if (target == "desktop.full.fixed") {
		if (isget) return (void*) sdl.desktop.full.fixed;
		else sdl.desktop.full.fixed = setval;
	} else if (target == "desktop.window.width") {
		if (isget) return (void*) sdl.desktop.window.width;
		else sdl.desktop.window.width = *static_cast<Bit16u*>(setval);
	} else if (target == "desktop.window.height") {
		if (isget) return (void*) sdl.desktop.window.height;
		else sdl.desktop.window.height = *static_cast<Bit16u*>(setval);
*/
	} else if (target == "desktop.fullscreen") {
		if (isget) return (void*) sdl.desktop.fullscreen;
		else sdl.desktop.fullscreen = setval;
	} else if (target == "desktop.doublebuf") {
		if (isget) return (void*) sdl.desktop.doublebuf;
		else sdl.desktop.doublebuf = setval;
/*
	} else if (target == "desktop.type") {
		if (isget) return (void*) sdl.desktop.type;
		else sdl.desktop.type = *static_cast<SCREEN_TYPES*>(setval);
*/
	} else if (target == "desktop.want_type") {
		if (isget) return (void*) sdl.desktop.want_type;
		else sdl.desktop.want_type = *static_cast<SCREEN_TYPES*>(setval);
/*
	} else if (target == "surface") {
		if (isget) return (void*) sdl.surface;
		else sdl.surface = static_cast<SDL_Surface*>(setval);
	} else if (target == "overlay") {
		if (isget) return (void*) sdl.overlay;
		else sdl.overlay = static_cast<SDL_Overlay*>(setval);
*/
	} else if (target == "mouse.autoenable") {
		if (isget) return (void*) sdl.mouse.autoenable;
		else sdl.mouse.autoenable = setval;
/*
	} else if (target == "overscan_width") {
		if (isget) return (void*) sdl.overscan_width;
		else sdl.overscan_width = *static_cast<Bitu*>(setval);
*/
#if defined (WIN32)
	} else if (target == "using_windib") {
		if (isget) return (void*) sdl.using_windib;
		else sdl.using_windib = setval;
#endif
	}
}
#endif

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
#ifdef __WIN32__
		case SDL_SYSWMEVENT : {
			if(menu_compatible) break;
			switch( event.syswm.msg->msg ) {
				case WM_SYSCOMMAND:
					switch (event.syswm.msg->wParam) {
						case SC_MAXIMIZE:
						case 0xF032:
							if(sdl.desktop.want_type==SCREEN_DIRECT3D)
								menu.maxwindow=true;
							else
								GFX_SwitchFullScreen();
							break;
						case 0xF122:
						case SC_RESTORE:
							menu.maxwindow=false;
							break;
					}
				case WM_MOVE:
					break;
				case WM_DROPFILES: {
					char buff[50];
					DragQueryFile((HDROP)event.syswm.msg->wParam,0,buff,200);
					Drag_Drop(buff);
					break;
				}
				default:
					break;
			}
		}
#endif
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (event.active.gain) {
					if (sdl.desktop.fullscreen && !sdl.mouse.locked)
						GFX_CaptureMouse();
					SetPriority(sdl.priority.focus);
					CPU_Disable_SkipAutoAdjust();
				} else {
					if (sdl.mouse.locked) {
#ifdef WIN32
						if (sdl.desktop.fullscreen) {
							VGA_KillDrawing();
							GFX_ForceFullscreenExit();
						}
#endif
						GFX_CaptureMouse();
					}
					SetPriority(sdl.priority.nofocus);
					GFX_LosingFocus();
					CPU_Enable_SkipAutoAdjust();
				}
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
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
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
						case SDL_ACTIVEEVENT:     // wait until we get window focus back
							if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
								// We've got focus back, so unpause and break out of the loop
								if (ev.active.gain) {
									paused = false;
									GFX_SetTitle(-1,-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
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
		case SDL_VIDEORESIZE:
			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		case SDL_VIDEOEXPOSE:
			if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
			break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = event.key.type;
			if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) &&
				((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN))) { MAPPER_LosingFocus(); break; }
#endif
#if defined (MACOSX)			
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			/* On macs CMD-Q is the default key to close an application */
			if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
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

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif

void Config_Add_SDL() {
	Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
#if (HAVE_D3D9_H) && defined(WIN32)
	// Allows dynamic pixelshader change
	sdl_sec->AddInitFunction(&D3D_reconfigure,true);
#endif
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,false);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");
     
	Pbool = sdl_sec->Add_bool("fulldouble",Property::Changeable::Always,false);
	Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox.");

	//Pbool = sdl_sec->Add_bool("sdlresize",Property::Changeable::Always,false);
	//Pbool->Set_help("Makes window resizable (depends on scalers)");

	Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"desktop");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
	                  "  Using your monitor's native resolution with aspect=true might give the best results.\n"
			  "  If you end up with small window on a large screen, try an output different from surface.");

	Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"original");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
	                  "  (output=surface does not!)");

	const char* outputs[] = {
		"surface", "overlay",
#if C_OPENGL
		"opengl", "openglnb", "openglhq",
#endif
#if (HAVE_DDRAW_H) && defined(WIN32)
		"ddraw",
#endif
#if (HAVE_D3D9_H) && defined(WIN32)
		"direct3d",
#endif
		0 };
#ifdef __WIN32__
		Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"direct3d");
#else
		Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"surface");
#endif
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
	Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

	Pint = sdl_sec->Add_int("sensitivity",Property::Changeable::Always,100);
	Pint->SetMinMax(1,1000);
	Pint->Set_help("Mouse sensitivity.");

	Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
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

#if (HAVE_D3D9_H) && (C_D3DSHADERS) && defined(WIN32)
	Pmulti = sdl_sec->Add_multi("pixelshader",Property::Changeable::Always," ");
	Pmulti->SetValue("none");
	Pmulti->Set_help("Pixelshader program (effect file must be in Shaders subdirectory). If 'forced' is appended,\n"
	    "then the shader will be used even if the result might not be desired.");

	Pstring = Pmulti->GetSection()->Add_string("type",Property::Changeable::Always,"none");
	Pstring = Pmulti->GetSection()->Add_string("force",Property::Changeable::Always,"");
#endif

	Pbool = sdl_sec->Add_bool("usescancodes",Property::Changeable::Always,false);
	Pbool->Set_help("Avoid usage of symkeys, might not work on all operating systems.");

	Pint = sdl_sec->Add_int("overscan",Property::Changeable::Always, 0);
	Pint->SetMinMax(0,10);
	Pint->Set_help("Width of overscan border (0 to 10). (works only if output=surface)");

//	Pint = sdl_sec->Add_int("overscancolor",Property::Changeable::Always, 0);
//	Pint->SetMinMax(0,1000);
//	Pint->Set_help("Value of overscan color.");
}

static void show_warning(char const * const message) {
	bool textonly = true;
#ifdef WIN32
	textonly = false;
	if ( !sdl.inited && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0 ) textonly = true;
	sdl.inited = true;
#endif
	LOG_MSG( "Warning: %s", message);
	if(textonly) return;
	if(!sdl.surface) sdl.surface = SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
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
		if(c>d) a=b=d; else a=b=c;
		if( a != std::string::npos) b++; 
		m2 = m.substr(0,a); m.erase(0,b);
		OutputString(x,y,m2.c_str(),0xffffffff,0,splash_surf);
		y += 20;
	}
   
	SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
	SDL_Flip(sdl.surface);
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
	if(sdl.desktop.fullscreen) SwitchFullScreen(1);
	if(!load_videodrv) putenv((char*)("SDL_VIDEODRIVER="));
#ifndef WIN32
	SDL_CloseAudio();
	SDL_Delay(50);
	SDL_Quit();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif
#endif

#ifndef WIN32
	execvp(newargs[0], newargs);
#endif
#ifdef __MINGW32__
#ifdef WIN32 // if failed under win32
    PROCESS_INFORMATION pi;
    STARTUPINFO si; 
    ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    ZeroMemory(&pi,sizeof(pi));

    if(CreateProcess(NULL, newargs[0], NULL, NULL, false, 0, NULL, NULL, &si, &pi)) {
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );
		SDL_CloseAudio();
		SDL_Delay(50);
        throw(0);
		SDL_Quit();
#if C_DEBUG
	// shutdown curses
		DEBUG_ShutDown(NULL);
#endif
    }
#endif
#else // if not MINGW
#ifdef WIN32
	char newargs_temp[32767];
	strcpy(newargs_temp, "");
	for(Bitu i = 1; i < parameters.size(); i++) {
		strcat(newargs_temp, " ");
		strcat(newargs_temp, newargs[i]);
	}

    if(ShellExecute(NULL, "open", newargs[0], newargs_temp, NULL, SW_SHOW)) {
		SDL_CloseAudio();
		SDL_Delay(50);
        throw(0);
		SDL_Quit();
#if C_DEBUG
	// shutdown curses
		DEBUG_ShutDown(NULL);
#endif
    }
#endif
#endif
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
		if(cstat.st_mode & S_IFDIR == 0) {
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
		if(cstat.st_mode & S_IFDIR == 0) {
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

void SetNumLock(void) {
#ifdef WIN32
	if (opt_disable_numlock_check)
		return;

	// Simulate a key press
	keybd_event(VK_NUMLOCK,0x45,KEYEVENTF_EXTENDEDKEY | 0,0);

	// Simulate a key release
	keybd_event(VK_NUMLOCK,0x45,KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP,0);
#endif
}

#ifdef WIN32
bool numlock_stat=false;
#endif

void CheckNumLockState(void) {
#ifdef WIN32
	BYTE keyState[256];

	GetKeyboardState((LPBYTE)(&keyState));
	if (keyState[VK_NUMLOCK] & 1) numlock_stat=true;
	if (numlock_stat) SetNumLock();
#endif
}

//extern void UI_Init(void);
int main(int argc, char* argv[]) {
	{
		/* NTS: Warning, do NOT move the Config myconf() object out of this scope.
		 * The destructor relies on executing section destruction code as part of
		 * DOSBox shutdown. */
		std::string opt_editconf;
		bool opt_nogui = false;
		bool opt_nomenu = false;
		bool opt_startui = false;
		bool opt_noconsole = false;
		bool opt_eraseconf = false;
		bool opt_resetconf = false;
		bool opt_printconf = false;
		bool opt_erasemapper = false;
		bool opt_resetmapper = false;
		bool opt_startmapper = false;
		bool opt_fullscreen = false;
		bool opt_showcycles = false;
		bool opt_userconf = false;
		std::string opt_opensaves,opt_opencaptures;
		std::string config_file,config_path,optname,optarg;
		std::vector<std::string> config_file_list;
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;

		control->cmdline->BeginOpt();
		while (control->cmdline->GetOpt(optname)) {
			if (optname == "version") {
				printf("\nDOSBox version %s, copyright 2002-2015 DOSBox Team.\n\n",VERSION);
				printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
				printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
				printf("and you are welcome to redistribute it under certain conditions;\n");
				printf("please read the COPYING file thoroughly before doing so.\n\n");
				return 0;
			}
			else if (optname == "h" || optname == "help") {
				printf("\ndosbox [options]\n");
				printf("\nDOSBox version %s, copyright 2002-2015 DOSBox Team.\n\n",VERSION);
				printf("  -h     -help                            Show this help\n");
				printf("  -editconf                               Launch editor\n");
				printf("  -opencaptures <param>                   Launch captures\n");
				printf("  -opensaves <param>                      Launch saves\n");
				printf("  -eraseconf                              Erase config file\n");
				printf("  -resetconf                              Erase config file\n");
				printf("  -printconf                              Print config file location\n");
				printf("  -erasemapper                            Erase mapper file\n");
				printf("  -resetmapper                            Erase mapper file\n");
				printf("  -noconsole                              Don't show console (debug+win32 only)\n");
				printf("  -nogui                                  Don't show gui (win32 only)\n");
				printf("  -nomenu                                 Don't show menu (win32 only)\n");
				printf("  -userconf                               Create user level config file\n");
				printf("  -conf <param>                           Use config file <param>\n");
				printf("  -startui                                Start DOSBox-X with UI\n");
				printf("  -startmapper                            Start DOSBox-X with mapper\n");
				printf("  -showcycles                             Show cycles count\n");
				printf("  -fullscreen                             Start in fullscreen\n");
				printf("  -savedir <path>                         Save path\n");
				printf("  -disable-numlock-check                  Disable numlock check (win32 only)\n");
				printf("  -date-host-forced                       Force synchronization of date with host\n");
				return 0;
			}
			else if (optname == "date-host-forced" || optname == "date_host_forced") {
				opt_date_host_forced = true;
			}
			else if (optname == "showcycles") {
				opt_showcycles = true;
			}
			else if (optname == "startmapper") {
				opt_startmapper = true;
			}
			else if (optname == "fullscreen") {
				opt_fullscreen = true;
			}
			else if (optname == "startui") {
				opt_startui = true;
			}
			else if (optname == "disable-numlock-check" || optname == "disable_numlock_check") {
				/* mainline DOSBox expects -disable_numlock_check so we support that here too */
				opt_disable_numlock_check = true;
			}
			else if (optname == "savedir") {
				if (!control->cmdline->NextOptArgv(custom_savedir)) return 1;
			}
			else if (optname == "userconf") {
				opt_userconf = true;
			}
			else if (optname == "conf") {
				if (!control->cmdline->NextOptArgv(config_file)) return 1;
				config_file_list.push_back(config_file);
			}
			else if (optname == "editconf") {
				if (!control->cmdline->NextOptArgv(opt_editconf)) return 1;
			}
			else if (optname == "opencaptures") {
				if (!control->cmdline->NextOptArgv(opt_opencaptures)) return 1;
			}
			else if (optname == "opensaves") {
				if (!control->cmdline->NextOptArgv(opt_opensaves)) return 1;
			}
			else if (optname == "eraseconf") {
				opt_eraseconf = true;
			}
			else if (optname == "resetconf") {
				opt_resetconf = true;
			}
			else if (optname == "printconf") {
				opt_printconf = true;
			}
			else if (optname == "erasemapper") {
				opt_erasemapper = true;
			}
			else if (optname == "resetmapper") {
				opt_resetmapper = true;
			}
			else if (optname == "noconsole") {
				opt_noconsole = true;
			}
			else if (optname == "nomenu") {
				opt_nomenu = true;
			}
			else if (optname == "nogui") {
				opt_nogui = true;
			}
			else {
				printf("WARNING: Unknown option %s (first parsing stage)\n",optname.c_str());
			}
		}

		/* Init the configuration system and add default values */
		CheckNumLockState();
		Config_Add_SDL();
		DOSBOX_Init();

		if (opt_editconf.length() != 0)
			launcheditor(opt_editconf);
		if (opt_opencaptures.length() != 0)
			launchcaptures(opt_opencaptures);
		if (opt_opensaves.length() != 0)
			launchsaves(opt_opensaves);
		if (opt_eraseconf || opt_resetconf)
			eraseconfigfile();
		if (opt_printconf)
			printconfiglocation();
		if (opt_erasemapper || opt_resetmapper)
			erasemapperfile();

#if C_DEBUG
# if defined(WIN32)
		/* Can't disable the console with debugger enabled */
		if (opt_noconsole) {
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			DestroyWindow(GetConsoleWindow());
		} else
# endif
		{
			DEBUG_SetupConsole();
		}
#endif

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

		/* Display Welcometext in the console */
		LOG_MSG("DOSBox version %s",VERSION);
		LOG_MSG("Copyright 2002-2015 DOSBox Team, published under GNU GPL.");

		{
			int id, major, minor;

			DOSBox_CheckOS(id, major, minor);
			if (id == 1) menu.compatible=true;
			if (!menu_compatible) LOG_MSG("---");
		}

		/* Init SDL */
#if SDL_VERSION_ATLEAST(1, 2, 14)
		/* Or debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
		 * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version
		 * of libsdl
		 */
		putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
#endif

#ifdef WIN32
		if (getenv("SDL_VIDEODRIVER")==NULL) {
			putenv("SDL_VIDEODRIVER=windib");
			sdl.using_windib=true;
			load_videodrv=false;
		}
#endif

		if (SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_CDROM|SDL_INIT_NOPARACHUTE) < 0)
			E_Exit("Can't init SDL %s",SDL_GetError());
		sdl.inited = true;

		if (opt_nogui || menu.compatible)
			menu.gui=false;
		if (menu_gui && !opt_nomenu)
			DOSBox_SetMenu();

		if (menu_gui) {
			if (GetMenu(GetHWND())) {
				LOG_MSG("GUI: Press Ctrl-F10 to capture/release mouse.\n"
					"     Save your configuration and restart DOSBox if your settings do not take effect.");
			}
		} else {
			LOG_MSG("GUI: Press Ctrl-F10 to capture/release mouse, Alt-F10 for configuration.");
		}
		SDL_Prepare();

		//Initialise Joystick seperately. This way we can warn when it fails instead
		//of exiting the application
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) LOG_MSG("Failed to init joystick support");
		sdl.num_joysticks = SDL_NumJoysticks();

		/* assume L+R ALT keys are up */
		sdl.laltstate = SDL_KEYUP;
		sdl.raltstate = SDL_KEYUP;

#if defined (WIN32)
# if SDL_VERSION_ATLEAST(1, 2, 10)
		sdl.using_windib=true;
# else
		sdl.using_windib=false;
# endif

		if (getenv("SDL_VIDEODRIVER")==NULL) {
			char sdl_drv_name[128];

			if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
				sdl.using_windib=false;
				if (strcmp(sdl_drv_name,"directx")!=0) {
					SDL_QuitSubSystem(SDL_INIT_VIDEO);
					putenv("SDL_VIDEODRIVER=directx");
					if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) {
						putenv("SDL_VIDEODRIVER=windib");
						if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
						sdl.using_windib=true;
					}
				}
			}
		} else {
			char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
			if (strcmp(sdl_videodrv,"directx")==0) sdl.using_windib = false;
			else if (strcmp(sdl_videodrv,"windib")==0) sdl.using_windib = true;
		}
#endif

		/* Parse configuration files */
		Cross::GetPlatformConfigDir(config_path);

		//First parse -userconf
		if (opt_userconf) {
			config_file.clear();
			Cross::GetPlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			control->ParseConfigFile(config_path.c_str());
			if (!control->configfiles.size()) {
				//Try to create the userlevel configfile.
				config_file.clear();
				Cross::CreatePlatformConfigDir(config_path);
				Cross::GetPlatformConfigName(config_file);
				config_path += config_file;
				if (control->PrintConfig(config_path.c_str())) {
					LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_path.c_str());
					//Load them as well. Makes relative paths much easier
					control->ParseConfigFile(config_path.c_str());
				}
			}
		}

		//Second parse all -conf switches we read from command line
		for (size_t si=0;si < config_file_list.size();si++) {
			std::string &cfg = config_file_list[si];
			if (!control->ParseConfigFile(cfg.c_str())) {
				// try to load it from the user directory
				control->ParseConfigFile((config_path + cfg).c_str());
			}
		}
		config_file_list.clear();

		// if none found => parse localdir conf
		if (!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

		// if none found => parse userlevel conf
		if (!control->configfiles.size()) {
			config_file.clear();
			Cross::GetPlatformConfigName(config_file);
			control->ParseConfigFile((config_path + config_file).c_str());
		}

		if (!control->configfiles.size()) {
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			if (control->PrintConfig(config_path.c_str())) {
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_path.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_path.c_str());
			} else {
				LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
			}
		}

#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif

		UI_Init();
		if (opt_startui) UI_Run(false);

		/* Init all the sections */
		control->Init();

		{
			/* Some extra SDL Functions */
			Section_prop *sdl_sec = static_cast<Section_prop*>(control->GetSection("sdl"));

			if (opt_fullscreen || sdl_sec->Get_bool("fullscreen")) {
				if (sdl.desktop.want_type != SCREEN_OPENGLHQ) SetMenu(GetHWND(),NULL);
				//only switch if not already in fullscreen
				if (!sdl.desktop.fullscreen) GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if (opt_startmapper)
			MAPPER_RunInternal();

		/* Start up main machine */

		// Shows menu bar (window)
		menu.startup = true;
		menu.hidecycles = (opt_showcycles ? false : true);
		if (sdl.desktop.want_type == SCREEN_OPENGLHQ) {
			menu.gui=false; DOSBox_SetOriginalIcon();
			if (!render.scale.hardware) SetVal("render","scaler",!render.scale.forced?"hardware2x":"hardware2x forced");
		}

#ifdef WIN32
		if (sdl.desktop.want_type == SCREEN_OPENGL && sdl.using_windib) {
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
				E_Exit("Can't init SDL Video %s",SDL_GetError());

			change_output(4);
			GFX_SetIcon();
			SDL_Prepare();
			if (menu.gui && !opt_nomenu) {
				SetMenu(GetHWND(), LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
				DrawMenuBar(GetHWND());
			}
		}
#endif

#ifdef WIN32
		{
			Section_prop *sec = static_cast<Section_prop *>(control->GetSection("sdl"));
			if (!strcmp(sec->Get_string("output"),"ddraw") && sdl.using_windib) {
				SDL_QuitSubSystem(SDL_INIT_VIDEO);
				putenv("SDL_VIDEODRIVER=directx");
				sdl.using_windib=false;
				if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0)
					E_Exit("Can't init SDL Video %s",SDL_GetError());

				change_output(1);
				GFX_SetIcon();
				SDL_Prepare();
				if (menu.gui && !opt_nomenu) {
					SetMenu(GetHWND(), LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
					DrawMenuBar(GetHWND());
				}
			}

			if (!load_videodrv && numlock_stat)
				SetNumLock ();
		}
#endif

		bool run_machine;
		bool reboot_machine;
		bool dos_kernel_shutdown;

		/* start the shell */
		control->StartUp();
		SHELL_Init();

		/* main execution. run the DOSBox shell. various exceptions will be thrown. some,
		 * which have type "int", have special actions. int(2) means to boot a guest OS
		 * (thrown as an exception to force stack unwinding), while int(0) and int(1)
		 * means someone pressed DOSBox's killswitch (CTRL+F9). */
		run_machine = false;
		reboot_machine = false;
		dos_kernel_shutdown = false;

		try {
			SHELL_Run();
		} catch (int x) {
			if (x == 2) { /* booting a guest OS. "boot" has already done the work to load the image and setup CPU registers */
				run_machine = true; /* make note. don't run the whole shebang from an exception handler! */
				dos_kernel_shutdown = true;
			}
			else if (x == 3) { /* reboot the system */
				reboot_machine = true;
			}
			else {
				// kill switch (see instances of throw(0) and throw(1) elsewhere in DOSBox)
				run_machine = false;
				dos_kernel_shutdown = false;
			}
		}
		catch (...) {
			throw;
		}

		if (dos_kernel_shutdown) {
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
		}

		if (run_machine) {
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
					reboot_machine = true;
				}
				else {
					// kill switch (see instances of throw(0) and throw(1) elsewhere in DOSBox)
				}
			}
			catch (...) {
				throw;
			}
		}

		if (reboot_machine) {
			LOG_MSG("Rebooting the system\n");
			control->startup_params.insert(control->startup_params.begin(),control->cmdline->GetFileName());
			restart_program(control->startup_params);
		}

		/* and then shutdown */
		GFX_ShutDown();
		/* Shutdown everything */
	}

	/* GUI font registry shutdown */
	GUI::Font::registry_freeall();
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
#if C_DEBUG
	DEBUG_ShutDown(NULL);
#endif

	sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt

	//Force visible mouse to end user. Somehow this sometimes doesn't happen
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(SDL_ENABLE);

	SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
	return 0;
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
	width = sdl.clip.w; // draw.width
	height = sdl.clip.h; // draw.height
	fullscreen = sdl.desktop.fullscreen;
}

void GFX_ShutDown(void) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}

bool OpenGL_using(void) {
	return (sdl.desktop.want_type==SCREEN_OPENGL?true:false);
}

bool Get_Custom_SaveDir(std::string& savedir) {
	if (custom_savedir.length() != 0)
		return true;

	return false;
}

