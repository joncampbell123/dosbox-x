/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* X11 based SDL video driver implementation.
   Note:  This implementation does not currently need X11 thread locking,
          since the event thread uses a separate X connection and any
          additional locking necessary is handled internally.  However,
          if full locking is neccessary, take a look at XInitThreads().
*/

#include <unistd.h>
#include <sys/ioctl.h>
#ifdef MTRR_SUPPORT
#include <asm/mtrr.h>
#include <sys/fcntl.h>
#endif

#include "SDL_endian.h"
#include "SDL_timer.h"
#include "SDL_thread.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "SDL_x11video.h"
#include "SDL_x11wm_c.h"
#include "SDL_x11mouse_c.h"
#include "SDL_x11events_c.h"
#include "SDL_x11modes_c.h"
#include "SDL_x11image_c.h"
#include "SDL_x11yuv_c.h"
#include "SDL_x11gl_c.h"
#include "SDL_x11gamma_c.h"
#include "../blank_cursor.h"

#ifdef X_HAVE_UTF8_STRING
#include <locale.h>
#endif

/* Initialization/Query functions */
static int X11_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Surface *X11_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int X11_ToggleFullScreen(_THIS, int on);
static void X11_UpdateMouse(_THIS);
static int X11_SetColors(_THIS, int firstcolor, int ncolors,
			 SDL_Color *colors);
static int X11_SetGammaRamp(_THIS, Uint16 *ramp);
static void X11_VideoQuit(_THIS);

#ifdef ENABLE_IM_EVENT
#include <ctype.h>
/* When this flag is enabled, force to use the default visual.
   Some Input Methods fails to create preedit window in other visuals.
   kinput2 fails. skkinput is OK. */
#define FORCE_USE_DEFAULT_VISUAL

/* XIM initialization function */
static int xim_init(_THIS);
static void xim_free(_THIS);

/* IM CallBack Function */
static int ef_height=0, ef_width=0, ef_ascent=0;

typedef struct {
    XIMStyle style;
    char *description;
} im_style_t;

/* Enumeration of XIM input style */
static im_style_t im_styles[] = {
    { XIMPreeditNothing  | XIMStatusNothing,	"Root" },
    { XIMPreeditPosition | XIMStatusNothing,	"OverTheSpot" },
    { XIMPreeditArea     | XIMStatusArea,	"OffTheSpot" },
    { XIMPreeditCallbacks| XIMStatusCallbacks,	"OnTheSpot" },
    { (XIMStyle)0,				NULL }};

#include <locale.h>
#include <X11/Xlocale.h>

/* Locale-specific data taken by locale_init and xim_init */
static char *im_name;
static char *lc_ctype;

#endif /* ENABLE_IM_EVENT */

/* X11 driver bootstrap functions */

static int X11_Available(void)
{
	Display *display = NULL;
	if ( SDL_X11_LoadSymbols() ) {
		display = XOpenDisplay(NULL);
		if ( display != NULL ) {
			XCloseDisplay(display);
		}
		SDL_X11_UnloadSymbols();
	}
	return(display != NULL);
}

static void X11_DeleteDevice(SDL_VideoDevice *device)
{
	if ( device ) {
		if ( device->hidden ) {
			SDL_free(device->hidden);
		}
		if ( device->gl_data ) {
			SDL_free(device->gl_data);
		}
		SDL_free(device);
		SDL_X11_UnloadSymbols();
	}
}

static SDL_VideoDevice *X11_CreateDevice(int devindex)
{
	SDL_VideoDevice *device = NULL;

	if ( SDL_X11_LoadSymbols() ) {
		/* Initialize all variables that we clean on shutdown */
		device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
		if ( device ) {
			SDL_memset(device, 0, (sizeof *device));
			device->hidden = (struct SDL_PrivateVideoData *)
					SDL_malloc((sizeof *device->hidden));
			device->gl_data = (struct SDL_PrivateGLData *)
					SDL_malloc((sizeof *device->gl_data));
		}
		if ( (device == NULL) || (device->hidden == NULL) ||
		                         (device->gl_data == NULL) ) {
			SDL_OutOfMemory();
			X11_DeleteDevice(device); /* calls SDL_X11_UnloadSymbols(). */
			return(0);
		}
		SDL_memset(device->hidden, 0, (sizeof *device->hidden));
		SDL_memset(device->gl_data, 0, (sizeof *device->gl_data));

#if SDL_VIDEO_OPENGL_GLX
		device->gl_data->swap_interval = -1;
#endif

		/* Set the driver flags */
		device->handles_any_size = 1;

		/* Set the function pointers */
		device->VideoInit = X11_VideoInit;
		device->ListModes = X11_ListModes;
		device->SetVideoMode = X11_SetVideoMode;
		device->ToggleFullScreen = X11_ToggleFullScreen;
		device->UpdateMouse = X11_UpdateMouse;
#if SDL_VIDEO_DRIVER_X11_XV
		device->CreateYUVOverlay = X11_CreateYUVOverlay;
#endif
		device->SetColors = X11_SetColors;
		device->UpdateRects = NULL;
		device->VideoQuit = X11_VideoQuit;
		device->AllocHWSurface = X11_AllocHWSurface;
		device->CheckHWBlit = NULL;
		device->FillHWRect = NULL;
		device->SetHWColorKey = NULL;
		device->SetHWAlpha = NULL;
		device->LockHWSurface = X11_LockHWSurface;
		device->UnlockHWSurface = X11_UnlockHWSurface;
		device->FlipHWSurface = X11_FlipHWSurface;
		device->FreeHWSurface = X11_FreeHWSurface;
		device->SetGamma = X11_SetVidModeGamma;
		device->GetGamma = X11_GetVidModeGamma;
		device->SetGammaRamp = X11_SetGammaRamp;
		device->GetGammaRamp = NULL;
#if SDL_VIDEO_OPENGL_GLX
		device->GL_LoadLibrary = X11_GL_LoadLibrary;
		device->GL_GetProcAddress = X11_GL_GetProcAddress;
		device->GL_GetAttribute = X11_GL_GetAttribute;
		device->GL_MakeCurrent = X11_GL_MakeCurrent;
		device->GL_SwapBuffers = X11_GL_SwapBuffers;
#endif
		device->SetCaption = X11_SetCaption;
		device->SetIcon = X11_SetIcon;
		device->IconifyWindow = X11_IconifyWindow;
		device->GrabInput = X11_GrabInput;
		device->GetWMInfo = X11_GetWMInfo;
		device->FreeWMCursor = X11_FreeWMCursor;
		device->CreateWMCursor = X11_CreateWMCursor;
		device->ShowWMCursor = X11_ShowWMCursor;
		device->WarpWMCursor = X11_WarpWMCursor;
		device->CheckMouseMode = X11_CheckMouseMode;
		device->InitOSKeymap = X11_InitOSKeymap;
		device->PumpEvents = X11_PumpEvents;

		device->SetIMPosition = X11_SetIMPosition;
		device->SetIMValues = X11_SetIMValues;
		device->GetIMValues = X11_GetIMValues;
		device->FlushIMString = X11_FlushIMString;
		#ifdef ENABLE_IM_EVENT
		device->GetIMInfo = X11_GetIMInfo;
		#endif

		device->free = X11_DeleteDevice;
	}

	return device;
}

VideoBootStrap X11_bootstrap = {
	"x11", "X Window System",
	X11_Available, X11_CreateDevice
};

/* Normal X11 error handler routine */
static int (*X_handler)(Display *, XErrorEvent *) = NULL;
static int x_errhandler(Display *d, XErrorEvent *e)
{
#if SDL_VIDEO_DRIVER_X11_VIDMODE
	extern int vm_error;
#endif
#if SDL_VIDEO_DRIVER_X11_DGAMOUSE
	extern int dga_error;
#endif

#if SDL_VIDEO_DRIVER_X11_VIDMODE
	/* VidMode errors are non-fatal. :) */
	/* Are the errors offset by one from the error base?
	   e.g. the error base is 143, the code is 148, and the
	        actual error is XF86VidModeExtensionDisabled (4) ?
	 */
        if ( (vm_error >= 0) &&
	     (((e->error_code == BadRequest)&&(e->request_code == vm_error)) ||
	      ((e->error_code > vm_error) &&
	       (e->error_code <= (vm_error+XF86VidModeNumberErrors)))) ) {
#ifdef X11_DEBUG
{ char errmsg[1024];
  XGetErrorText(d, e->error_code, errmsg, sizeof(errmsg));
printf("VidMode error: %s\n", errmsg);
}
#endif
        	return(0);
        }
#endif /* SDL_VIDEO_DRIVER_X11_VIDMODE */

#if SDL_VIDEO_DRIVER_X11_DGAMOUSE
	/* DGA errors can be non-fatal. :) */
        if ( (dga_error >= 0) &&
	     ((e->error_code > dga_error) &&
	      (e->error_code <= (dga_error+XF86DGANumberErrors))) ) {
#ifdef X11_DEBUG
{ char errmsg[1024];
  XGetErrorText(d, e->error_code, errmsg, sizeof(errmsg));
printf("DGA error: %s\n", errmsg);
}
#endif
        	return(0);
        }
#endif /* SDL_VIDEO_DRIVER_X11_DGAMOUSE */

	return(X_handler(d,e));
}

/* X11 I/O error handler routine */
static int (*XIO_handler)(Display *) = NULL;
static int xio_errhandler(Display *d)
{
	/* Ack!  Lost X11 connection! */

	/* We will crash if we try to clean up our display */
	if ( SDL_VideoSurface && current_video->hidden->Ximage ) {
		SDL_VideoSurface->pixels = NULL;
	}
	current_video->hidden->X11_Display = NULL;

	/* Continue with the standard X11 error handler */
	return(XIO_handler(d));
}

static int (*Xext_handler)(Display *, _Xconst char *, _Xconst char *) = NULL;
static int xext_errhandler(Display *d, _Xconst char *ext, _Xconst char *reason)
{
#ifdef X11_DEBUG
	printf("Xext error inside SDL (may be harmless):\n");
	printf("  Extension \"%s\" %s on display \"%s\".\n",
	       ext, reason, XDisplayString(d));
#endif

	if (SDL_strcmp(reason, "missing") == 0) {
		/*
		 * Since the query itself, elsewhere, can handle a missing extension
		 *  and the default behaviour in Xlib is to write to stderr, which
		 *  generates unnecessary bug reports, we just ignore these.
		 */
		return 0;
	}

	/* Everything else goes to the default handler... */
	return Xext_handler(d, ext, reason);
}

/* Find out what class name we should use */
static char *get_classname(char *classname, int maxlen)
{
	char *spot;
#if defined(__LINUX__) || defined(__FREEBSD__)
	char procfile[1024];
	char linkfile[1024];
	int linksize;
#endif

	/* First allow environment variable override */
	spot = SDL_getenv("SDL_VIDEO_X11_WMCLASS");
	if ( spot ) {
		SDL_strlcpy(classname, spot, maxlen);
		return classname;
	}

	/* Next look at the application's executable name */
#if defined(__LINUX__) || defined(__FREEBSD__)
#if defined(__LINUX__)
	SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/exe", getpid());
#elif defined(__FREEBSD__)
	SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/file", getpid());
#else
#error Where can we find the executable name?
#endif
	linksize = readlink(procfile, linkfile, sizeof(linkfile)-1);
	if ( linksize > 0 ) {
		linkfile[linksize] = '\0';
		spot = SDL_strrchr(linkfile, '/');
		if ( spot ) {
			SDL_strlcpy(classname, spot+1, maxlen);
		} else {
			SDL_strlcpy(classname, linkfile, maxlen);
		}
		return classname;
	}
#endif /* __LINUX__ */

	/* Finally use the default we've used forever */
	SDL_strlcpy(classname, "SDL_App", maxlen);
	return classname;
}

static void destroy_aux_windows(_THIS)
{
    if (FSwindow) {
	    XDestroyWindow(SDL_Display, FSwindow);
        FSwindow = 0;
    }
    if (WMwindow) {
        XDestroyWindow(SDL_Display, WMwindow);
        WMwindow = 0;
    }
}

/* Create auxiliary (toplevel) windows with the current visual */
static void create_aux_windows(_THIS, const unsigned int force)
{
    int x = 0, y = 0;
    char classname[1024];
    XSetWindowAttributes xattr;
    XWMHints *hints;
    unsigned long app_event_mask;
    int def_vis = (SDL_Visual == DefaultVisual(SDL_Display, SDL_Screen));

    /* Look up some useful Atoms */
    WM_DELETE_WINDOW = XInternAtom(SDL_Display, "WM_DELETE_WINDOW", False);

    /* Don't create any extra windows if we are being managed */
    if ( SDL_windowid ) {
	FSwindow = 0;
	WMwindow = SDL_strtol(SDL_windowid, NULL, 0);
        return;
    }

    if (FSwindow)
        XDestroyWindow(SDL_Display, FSwindow);

#if SDL_VIDEO_DRIVER_X11_XINERAMA
    if ( use_xinerama ) {
        x = xinerama_info.x_org;
        y = xinerama_info.y_org;
    }
#endif
    xattr.override_redirect = True;
    xattr.background_pixel = def_vis ? BlackPixel(SDL_Display, SDL_Screen) : 0;
    xattr.border_pixel = 0;
    xattr.colormap = SDL_XColorMap;

    FSwindow = XCreateWindow(SDL_Display, SDL_Root,
                             x, y, 32, 32, 0,
			     this->hidden->depth, InputOutput, SDL_Visual,
			     CWOverrideRedirect/* | CWBackPixel | CWBorderPixel*/
			     | CWColormap,
			     &xattr);

    XSelectInput(SDL_Display, FSwindow, StructureNotifyMask);

    /* Tell KDE to keep the fullscreen window on top */
    {
	XEvent ev;
	long mask;

	SDL_memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = SDL_Root;
	ev.xclient.message_type = XInternAtom(SDL_Display,
					      "KWM_KEEP_ON_TOP", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = FSwindow;
	ev.xclient.data.l[1] = CurrentTime;
	mask = SubstructureRedirectMask;
	XSendEvent(SDL_Display, SDL_Root, False, mask, &ev);
    }

    hints = NULL;
    if(WMwindow) {
	/* All window attributes must survive the recreation */
	hints = XGetWMHints(SDL_Display, WMwindow);
	XDestroyWindow(SDL_Display, WMwindow);
    }

    /* Create the window for windowed management */
    /* (reusing the xattr structure above) */
    WMwindow = XCreateWindow(SDL_Display, SDL_Root,
                             x, y, 32, 32, 0,
			     this->hidden->depth, InputOutput, SDL_Visual,
			     /*CWBackPixel | CWBorderPixel | */CWColormap,
			     &xattr);

    /* Set the input hints so we get keyboard input */
    if(!hints) {
	hints = XAllocWMHints();
	hints->input = True;
	hints->flags = InputHint;
    }
    XSetWMHints(SDL_Display, WMwindow, hints);
    XFree(hints);
    X11_SetCaptionNoLock(this, this->wm_title, this->wm_icon);

#ifdef ENABLE_IM_EVENT
    if(!xim_init(this)) {
		SDL_SetError("Error: Can not initialize XIM.");
	}
#endif

    app_event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask
	| PropertyChangeMask | StructureNotifyMask | KeymapStateMask;
    XSelectInput(SDL_Display, WMwindow, app_event_mask);

    /* Set the class hints so we can get an icon (AfterStep) */
    get_classname(classname, sizeof(classname));
    {
	XClassHint *classhints;
	classhints = XAllocClassHint();
	if(classhints != NULL) {
	    classhints->res_name = classname;
	    classhints->res_class = classname;
	    XSetClassHint(SDL_Display, WMwindow, classhints);
	    XFree(classhints);
	}
    }

	{
		pid_t pid = getpid();
		char hostname[256];

		if (pid > 0 && gethostname(hostname, sizeof(hostname)) > -1) {
			Atom _NET_WM_PID = XInternAtom(SDL_Display, "_NET_WM_PID", False);
			Atom WM_CLIENT_MACHINE = XInternAtom(SDL_Display, "WM_CLIENT_MACHINE", False);
			
			hostname[sizeof(hostname)-1] = '\0';
			XChangeProperty(SDL_Display, WMwindow, _NET_WM_PID, XA_CARDINAL, 32,
					PropModeReplace, (unsigned char *)&pid, 1);
			XChangeProperty(SDL_Display, WMwindow, WM_CLIENT_MACHINE, XA_STRING, 8,
					PropModeReplace, (unsigned char *)hostname, SDL_strlen(hostname));
		}
	}

	/* Setup the communication with the IM server */
	/* create_aux_windows may be called several times against the same
	   Display.  We should reuse the SDL_IM if one has been opened for
	   the Display, so we should not simply reset SDL_IM here.  */

	#ifdef X_HAVE_UTF8_STRING
	if (SDL_X11_HAVE_UTF8) {
		/* Discard obsolete resources if any.  */
		if (SDL_IM != NULL && SDL_Display != XDisplayOfIM(SDL_IM)) {
			/* Just a double check. I don't think this
		           code is ever executed. */
			SDL_SetError("display has changed while an IM is kept");
			if (SDL_IC) {
				XUnsetICFocus(SDL_IC);
				XDestroyIC(SDL_IC);
				SDL_IC = NULL;
			}
			XCloseIM(SDL_IM);
			SDL_IM = NULL;
		}

		/* Open an input method.  */
		if (SDL_IM == NULL) {
			char *old_locale = NULL, *old_modifiers = NULL;
			const char *p;
			size_t n;
			/* I'm not comfortable to do locale setup
			   here.  However, we need C library locale
			   (and xlib modifiers) to be set based on the
			   user's preference to use XIM, and many
			   existing game programs doesn't take care of
			   users' locale preferences, so someone other
			   than the game program should do it.
			   Moreover, ones say that some game programs
			   heavily rely on the C locale behaviour,
			   e.g., strcol()'s, and we can't change the C
			   library locale.  Given the situation, I
			   couldn't find better place to do the
			   job... */

			/* Save the current (application program's)
			   locale settings.  */
			p = setlocale(LC_ALL, NULL);
			if ( p ) {
				n = SDL_strlen(p)+1;
				old_locale = SDL_stack_alloc(char, n);
				if ( old_locale ) {
					SDL_strlcpy(old_locale, p, n);
				}
			}
			p = XSetLocaleModifiers(NULL);
			if ( p ) {
				n = SDL_strlen(p)+1;
				old_modifiers = SDL_stack_alloc(char, n);
				if ( old_modifiers ) {
					SDL_strlcpy(old_modifiers, p, n);
				}
			}

			/* Fetch the user's preferences and open the
			   input method with them.  */
			setlocale(LC_ALL, "");
			XSetLocaleModifiers("");
			SDL_IM = XOpenIM(SDL_Display, NULL, classname, classname);

			/* Restore the application's locale settings
			   so that we don't break the application's
			   expected behaviour.  */
			if ( old_locale ) {
				/* We need to restore the C library
				   locale first, since the
				   interpretation of the X modifier
				   may depend on it.  */
				setlocale(LC_ALL, old_locale);
				SDL_stack_free(old_locale);
			}
			if ( old_modifiers ) {
				XSetLocaleModifiers(old_modifiers);
				SDL_stack_free(old_modifiers);
			}
		}

		/* Create a new input context for the new window just created.  */
		if (SDL_IM == NULL) {
			SDL_SetError("no input method could be opened");
		} else {
			if (SDL_IC != NULL) {
				/* Discard the old IC before creating new one.  */
			    XUnsetICFocus(SDL_IC);
			    XDestroyIC(SDL_IC);
			}
			/* Theoretically we should check the current IM supports
			   PreeditNothing+StatusNothing style (i.e., root window method)
			   before creating the IC.  However, it is the bottom line method,
			   and we supports any other options.  If the IM didn't support
			   root window method, the following call fails, and SDL falls
			   back to pre-XIM keyboard handling.  */
			SDL_IC = pXCreateIC(SDL_IM,
					XNClientWindow, WMwindow,
					XNFocusWindow, WMwindow,
					XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
					XNResourceName, classname,
					XNResourceClass, classname,
					NULL);

			if (SDL_IC == NULL) {
				SDL_SetError("no input context could be created");
				XCloseIM(SDL_IM);
				SDL_IM = NULL;
			} else {
				/* We need to receive X events that an IM wants and to pass
				   them to the IM through XFilterEvent. The set of events may
				   vary depending on the IM implementation and the options
				   specified through various routes. Although unlikely, the
				   xlib specification allows IM to change the event requirement
				   with its own circumstances, it is safe to call SelectInput
				   whenever we re-create an IC.  */
				unsigned long mask = 0;
				char *ret = pXGetICValues(SDL_IC, XNFilterEvents, &mask, NULL);
				if (ret != NULL) {
					XUnsetICFocus(SDL_IC);
					XDestroyIC(SDL_IC);
					SDL_IC = NULL;
					SDL_SetError("no input context could be created");
					XCloseIM(SDL_IM);
					SDL_IM = NULL;
				} else {
					XSelectInput(SDL_Display, WMwindow, app_event_mask | mask);
					XSetICFocus(SDL_IC);
				}
			}
		}
	}
	#endif

	/* Allow the window to be deleted by the window manager */
	XSetWMProtocols(SDL_Display, WMwindow, &WM_DELETE_WINDOW, 1);
}

static int X11_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	const char *env;
	char *display;
	int i;

	/* Open the X11 display */
	display = NULL;		/* Get it from DISPLAY environment variable */

	if ( (SDL_strncmp(XDisplayName(display), ":", 1) == 0) ||
	     (SDL_strncmp(XDisplayName(display), "unix:", 5) == 0) ) {
		local_X11 = 1;
	} else {
		local_X11 = 0;
	}
	SDL_Display = XOpenDisplay(display);
#if defined(__osf__) && defined(SDL_VIDEO_DRIVER_X11_DYNAMIC)
	/* On Tru64 if linking without -lX11, it fails and you get following message.
	 * Xlib: connection to ":0.0" refused by server
	 * Xlib: XDM authorization key matches an existing client!
	 *
	 * It succeeds if retrying 1 second later
	 * or if running xhost +localhost on shell.
	 *
	 */
	if ( SDL_Display == NULL ) {
		SDL_Delay(1000);
		SDL_Display = XOpenDisplay(display);
	}
#endif
	if ( SDL_Display == NULL ) {
		SDL_SetError("Couldn't open X11 display");
		return(-1);
	}
#ifdef X11_DEBUG
	XSynchronize(SDL_Display, True);
#endif

	/* Create an alternate X display for graphics updates -- allows us
	   to do graphics updates in a separate thread from event handling.
	   Thread-safe X11 doesn't seem to exist.
	 */
	GFX_Display = XOpenDisplay(display);
	if ( GFX_Display == NULL ) {
		XCloseDisplay(SDL_Display);
		SDL_Display = NULL;
		SDL_SetError("Couldn't open X11 display");
		return(-1);
	}

	/* Set the normal X error handler */
	X_handler = XSetErrorHandler(x_errhandler);

	/* Set the error handler if we lose the X display */
	XIO_handler = XSetIOErrorHandler(xio_errhandler);

	/* Set the X extension error handler */
	Xext_handler = XSetExtensionErrorHandler(xext_errhandler);

	/* use default screen (from $DISPLAY) */
	SDL_Screen = DefaultScreen(SDL_Display);

#ifndef NO_SHARED_MEMORY
	/* Check for MIT shared memory extension */
	use_mitshm = 0;
	if ( local_X11 ) {
		use_mitshm = XShmQueryExtension(SDL_Display);
	}
#endif /* NO_SHARED_MEMORY */

	/* Get the available video modes */
	if(X11_GetVideoModes(this) < 0) {
		XCloseDisplay(GFX_Display);
		GFX_Display = NULL;
		XCloseDisplay(SDL_Display);
		SDL_Display = NULL;
	    return -1;
	}

	/* Determine the current screen size */
	this->info.current_w = DisplayWidth(SDL_Display, SDL_Screen);
	this->info.current_h = DisplayHeight(SDL_Display, SDL_Screen);

	/* Determine the default screen depth:
	   Use the default visual (or at least one with the same depth) */
	SDL_DisplayColormap = DefaultColormap(SDL_Display, SDL_Screen);
	for(i = 0; i < this->hidden->nvisuals; i++)
	    if(this->hidden->visuals[i].depth == DefaultDepth(SDL_Display,
							      SDL_Screen))
		break;
	if(i == this->hidden->nvisuals) {
	    /* default visual was useless, take the deepest one instead */
	    i = 0;
	}
	SDL_Visual = this->hidden->visuals[i].visual;
	if ( SDL_Visual == DefaultVisual(SDL_Display, SDL_Screen) ) {
	    SDL_XColorMap = SDL_DisplayColormap;
	} else {
	    SDL_XColorMap = XCreateColormap(SDL_Display, SDL_Root,
					    SDL_Visual, AllocNone);
	}
	this->hidden->depth = this->hidden->visuals[i].depth;
	vformat->BitsPerPixel = this->hidden->visuals[i].bpp;
	if ( vformat->BitsPerPixel > 8 ) {
		vformat->Rmask = SDL_Visual->red_mask;
	  	vformat->Gmask = SDL_Visual->green_mask;
	  	vformat->Bmask = SDL_Visual->blue_mask;
	}
	if ( this->hidden->depth == 32 ) {
		vformat->Amask = (0xFFFFFFFF & ~(vformat->Rmask|vformat->Gmask|vformat->Bmask));
	}
	X11_SaveVidModeGamma(this);

	/* Allow environment override of screensaver disable. */
	env = SDL_getenv("SDL_VIDEO_ALLOW_SCREENSAVER");
	if ( env ) {
		allow_screensaver = SDL_atoi(env);
	} else {
#ifdef SDL_VIDEO_DISABLE_SCREENSAVER
		allow_screensaver = 0;
#else
		allow_screensaver = 1;
#endif
	}

	/* See if we have been passed a window to use */
	SDL_windowid = SDL_getenv("SDL_WINDOWID");

	/* Create the fullscreen and managed windows */
	create_aux_windows(this, 0);

	/* Create the blank cursor */
	SDL_BlankCursor = this->CreateWMCursor(this, blank_cdata, blank_cmask,
					BLANK_CWIDTH, BLANK_CHEIGHT,
						BLANK_CHOTX, BLANK_CHOTY);

	/* Fill in some window manager capabilities */
	this->info.wm_available = 1;

	/* We're done! */
	XFlush(SDL_Display);
	return(0);
}

static void X11_DestroyWindow(_THIS, SDL_Surface *screen)
{
	/* Clean up OpenGL */
	if ( screen ) {
		screen->flags &= ~(SDL_OPENGL|SDL_OPENGLBLIT);
	}
	X11_GL_Shutdown(this);

	if ( ! SDL_windowid ) {
		/* Hide the managed window */
		if ( screen && (screen->flags & SDL_FULLSCREEN) ) {
            if ( WMwindow )
                XUnmapWindow(SDL_Display, WMwindow);

			screen->flags &= ~SDL_FULLSCREEN;
			X11_LeaveFullScreen(this);
		}

		/* Destroy the output window */
		if ( SDL_Window ) {
			XDestroyWindow(SDL_Display, SDL_Window);
		}

		/* Free the colormap entries */
		if ( SDL_XPixels ) {
			int numcolors;
			unsigned long pixel;
			numcolors = SDL_Visual->map_entries;
			for ( pixel=0; pixel<numcolors; ++pixel ) {
				while ( SDL_XPixels[pixel] > 0 ) {
					XFreeColors(GFX_Display,
						SDL_DisplayColormap,&pixel,1,0);
					--SDL_XPixels[pixel];
				}
			}
			SDL_free(SDL_XPixels);
			SDL_XPixels = NULL;
		} 

		/* Free the graphics context */
		if ( SDL_GC ) {
			XFreeGC(SDL_Display, SDL_GC);
			SDL_GC = 0;
		}
	}
}

static SDL_bool X11_WindowPosition(_THIS, int *x, int *y, int w, int h)
{
	const char *window = SDL_getenv("SDL_VIDEO_WINDOW_POS");
	const char *center = SDL_getenv("SDL_VIDEO_CENTERED");
	if ( window ) {
		if ( SDL_sscanf(window, "%d,%d", x, y) == 2 ) {
			return SDL_TRUE;
		}
		if ( SDL_strcmp(window, "center") == 0 ) {
			center = window;
		}
	}
	if ( center ) {
		*x = (DisplayWidth(SDL_Display, SDL_Screen) - w)/2;
		*y = (DisplayHeight(SDL_Display, SDL_Screen) - h)/2;
		return SDL_TRUE;
	}
	return SDL_FALSE;
}

/* NTS: This code ALSO used to position the window, but that apparently breaks
 *      with XFCE 4.14. The breakage manifests itself as a window that is always
 *      given it's original size from ConfigureNotify no matter what size the
 *      host application is trying to size it. Removing the XMoveWindow() call
 *      here fixes that. --J.C. */
static void X11_SetSizeHints(_THIS, int w, int h, Uint32 flags)
{
	XSizeHints *hints;

    hints = XAllocSizeHints();
    if ( hints ) {
        if ( flags & SDL_FULLSCREEN ) {
            hints->x = 0;
            hints->y = 0;
            hints->flags |= USPosition;
        } else {
            if (!(flags & SDL_RESIZABLE)) {
                hints->min_width = hints->max_width = w;
                hints->min_height = hints->max_height = h;
                hints->flags = PMaxSize | PMinSize;
            }
        }
        XSetWMNormalHints(SDL_Display, WMwindow, hints);
        XFree(hints);
    }

	/* Respect the window caption style */
	if ( flags & SDL_NOFRAME ) {
		SDL_bool set;
		Atom WM_HINTS;

		/* We haven't modified the window manager hints yet */
		set = SDL_FALSE;

		/* First try to set MWM hints */
		WM_HINTS = XInternAtom(SDL_Display, "_MOTIF_WM_HINTS", True);
		if ( WM_HINTS != None ) {
			/* Hints used by Motif compliant window managers */
			struct {
				unsigned long flags;
				unsigned long functions;
				unsigned long decorations;
				long input_mode;
				unsigned long status;
			} MWMHints = { (1L << 1), 0, 0, 0, 0 };

			XChangeProperty(SDL_Display, WMwindow,
			                WM_HINTS, WM_HINTS, 32,
			                PropModeReplace,
					(unsigned char *)&MWMHints,
					sizeof(MWMHints)/sizeof(long));
			set = SDL_TRUE;
		}
		/* Now try to set KWM hints */
		WM_HINTS = XInternAtom(SDL_Display, "KWM_WIN_DECORATION", True);
		if ( WM_HINTS != None ) {
			long KWMHints = 0;

			XChangeProperty(SDL_Display, WMwindow,
			                WM_HINTS, WM_HINTS, 32,
			                PropModeReplace,
					(unsigned char *)&KWMHints,
					sizeof(KWMHints)/sizeof(long));
			set = SDL_TRUE;
		}
		/* Now try to set GNOME hints */
		WM_HINTS = XInternAtom(SDL_Display, "_WIN_HINTS", True);
		if ( WM_HINTS != None ) {
			long GNOMEHints = 0;

			XChangeProperty(SDL_Display, WMwindow,
			                WM_HINTS, WM_HINTS, 32,
			                PropModeReplace,
					(unsigned char *)&GNOMEHints,
					sizeof(GNOMEHints)/sizeof(long));
			set = SDL_TRUE;
		}
		/* Finally set the transient hints if necessary */
		if ( ! set ) {
			XSetTransientForHint(SDL_Display, WMwindow, SDL_Root);
		}
	} else {
		SDL_bool set;
		Atom WM_HINTS;

		/* We haven't modified the window manager hints yet */
		set = SDL_FALSE;

		/* First try to unset MWM hints */
		WM_HINTS = XInternAtom(SDL_Display, "_MOTIF_WM_HINTS", True);
		if ( WM_HINTS != None ) {
			XDeleteProperty(SDL_Display, WMwindow, WM_HINTS);
			set = SDL_TRUE;
		}
		/* Now try to unset KWM hints */
		WM_HINTS = XInternAtom(SDL_Display, "KWM_WIN_DECORATION", True);
		if ( WM_HINTS != None ) {
			XDeleteProperty(SDL_Display, WMwindow, WM_HINTS);
			set = SDL_TRUE;
		}
		/* Now try to unset GNOME hints */
		WM_HINTS = XInternAtom(SDL_Display, "_WIN_HINTS", True);
		if ( WM_HINTS != None ) {
			XDeleteProperty(SDL_Display, WMwindow, WM_HINTS);
			set = SDL_TRUE;
		}
		/* Finally unset the transient hints if necessary */
		if ( ! set ) {
			XDeleteProperty(SDL_Display, WMwindow, XA_WM_TRANSIENT_FOR);
		}
	}
}

static int X11_CreateWindow(_THIS, SDL_Surface *screen,
			    int w, int h, int bpp, Uint32 flags)
{
	int i, depth;
	Visual *vis;
	int vis_change;
	Uint32 Amask;

	/* If a window is already present, destroy it and start fresh */
	if ( SDL_Window ) {
		X11_DestroyWindow(this, screen);
		switch_waiting = 0; /* Prevent jump back to now-meaningless state. */
	}

	/* See if we have been given a window id */
	if ( SDL_windowid ) {
		SDL_Window = SDL_strtol(SDL_windowid, NULL, 0);
	} else {
		SDL_Window = 0;
	}

	/* find out which visual we are going to use */
	if ( flags & SDL_OPENGL ) {
		XVisualInfo *vi;

		vi = X11_GL_GetVisual(this);
		if( !vi ) {
			return -1;
		}
		vis = vi->visual;
		depth = vi->depth;
	} else if ( SDL_windowid ) {
		XWindowAttributes a;

		XGetWindowAttributes(SDL_Display, SDL_Window, &a);
		vis = a.visual;
		depth = a.depth;
	} else {
		for ( i = 0; i < this->hidden->nvisuals; i++ ) {
			if ( this->hidden->visuals[i].bpp == bpp )
				break;
		}
		if ( i == this->hidden->nvisuals ) {
			SDL_SetError("No matching visual for requested depth");
			return -1;	/* should never happen */
		}
		vis = this->hidden->visuals[i].visual;
		depth = this->hidden->visuals[i].depth;
	}
#ifdef X11_DEBUG
        printf("Choosing %s visual at %d bpp - %d colormap entries\n", vis->class == PseudoColor ? "PseudoColor" : (vis->class == TrueColor ? "TrueColor" : (vis->class == DirectColor ? "DirectColor" : "Unknown")), depth, vis->map_entries);
#endif
        /* NTS: Comparing Visual pointers will ALWAYS show a visual change because the pointers differ.
         *      If you look at the ACTUAL contents of the struct nothing changes! */
    vis_change =    (depth                  != this->hidden->depth)     ||
                    (vis->class             != SDL_Visual->class)       ||
                    (vis->red_mask          != SDL_Visual->red_mask)    ||
                    (vis->green_mask        != SDL_Visual->green_mask)  ||
                    (vis->blue_mask         != SDL_Visual->blue_mask)   ||
                    (vis->bits_per_rgb      != SDL_Visual->bits_per_rgb);

    SDL_Visual = vis;
	this->hidden->depth = depth;

	/* Allocate the new pixel format for this video mode */
	if ( this->hidden->depth == 32 ) {
		Amask = (0xFFFFFFFF & ~(vis->red_mask|vis->green_mask|vis->blue_mask));
	} else {
		Amask = 0;
	}
	if ( ! SDL_ReallocFormat(screen, bpp,
			vis->red_mask, vis->green_mask, vis->blue_mask, Amask) ) {
		return -1;
	}

	/* Create the appropriate colormap */
	if ( SDL_XColorMap != SDL_DisplayColormap ) {
		XFreeColormap(SDL_Display, SDL_XColorMap);
	}
	if ( SDL_Visual->class == PseudoColor ) {
	    int ncolors;

	    /* Allocate the pixel flags */
	    ncolors = SDL_Visual->map_entries;
	    SDL_XPixels = SDL_malloc(ncolors * sizeof(int));
	    if(SDL_XPixels == NULL) {
		SDL_OutOfMemory();
		return -1;
	    }
	    SDL_memset(SDL_XPixels, 0, ncolors * sizeof(*SDL_XPixels));

	    /* always allocate a private colormap on non-default visuals */
	    if ( SDL_Visual != DefaultVisual(SDL_Display, SDL_Screen) ) {
		flags |= SDL_HWPALETTE;
	    }
	    if ( flags & SDL_HWPALETTE ) {
		screen->flags |= SDL_HWPALETTE;
		SDL_XColorMap = XCreateColormap(SDL_Display, SDL_Root,
		                                SDL_Visual, AllocAll);
	    } else {
		SDL_XColorMap = SDL_DisplayColormap;
	    }
	} else if ( SDL_Visual->class == DirectColor ) {

	    /* Create a colormap which we can manipulate for gamma */
	    SDL_XColorMap = XCreateColormap(SDL_Display, SDL_Root,
		                            SDL_Visual, AllocAll);
            XSync(SDL_Display, False);

	    /* Initialize the colormap to the identity mapping */
	    SDL_GetGammaRamp(0, 0, 0);
	    this->screen = screen;
	    X11_SetGammaRamp(this, this->gamma);
	    this->screen = NULL;
	} else {
	    /* Create a read-only colormap for our window */
	    SDL_XColorMap = XCreateColormap(SDL_Display, SDL_Root,
	                                    SDL_Visual, AllocNone);
	}

	/* Recreate the auxiliary windows, if needed (required for GL) */
	if ( vis_change )
	    create_aux_windows(this, 1);

	if(screen->flags & SDL_HWPALETTE) {
	    /* Since the full-screen window might have got a nonzero background
	       colour (0 is white on some displays), we should reset the
	       background to 0 here since that is what the user expects
	       with a private colormap */
	    XSetWindowBackground(SDL_Display, FSwindow, 0);
	    XClearWindow(SDL_Display, FSwindow);
	}

	/* resize the (possibly new) window manager window */
	if( !SDL_windowid ) {
		int x = -999,y = -999;

		X11_SetSizeHints(this, w, h, flags);
		window_w = w;
		window_h = h;

		/* Center it, if desired */
		if (!X11_WindowPosition(this, &x, &y, w, h))
			x = y = -999;

		if (x > -999 && y > -999)
			XMoveResizeWindow(SDL_Display, WMwindow, x, y, w, h);
		else
			XResizeWindow(SDL_Display, WMwindow, w, h);
	}

	/* Create (or use) the X11 display window */
	if ( !SDL_windowid ) {
		if ( flags & SDL_OPENGL ) {
			if ( X11_GL_CreateWindow(this, w, h) < 0 ) {
				return(-1);
			}
		} else {
			XSetWindowAttributes swa;

			swa.background_pixel = 0;
			swa.border_pixel = 0;
			swa.colormap = SDL_XColorMap;
			SDL_Window = XCreateWindow(SDL_Display, WMwindow,
		                           	0, 0, w, h, 0, depth,
		                           	InputOutput, SDL_Visual,
		                           	/*CWBackPixel | CWBorderPixel
		                           	| */CWColormap, &swa);
		}
		/* Only manage our input if we own the window */
		XSelectInput(SDL_Display, SDL_Window,
					( EnterWindowMask | LeaveWindowMask
					| ButtonPressMask | ButtonReleaseMask
					| PointerMotionMask | ExposureMask ));
	}
	/* Create the graphics context here, once we have a window */
	if ( flags & SDL_OPENGL ) {
		if ( X11_GL_CreateContext(this) < 0 ) {
			return(-1);
		} else {
			screen->flags |= SDL_OPENGL;
		}
	} else {
		XGCValues gcv;

		gcv.graphics_exposures = False;
		SDL_GC = XCreateGC(SDL_Display, SDL_Window,
		                   GCGraphicsExposures, &gcv);
		if ( ! SDL_GC ) {
			SDL_SetError("Couldn't create graphics context");
			return(-1);
		}
	}

	/* Set our colormaps when not setting a GL mode */
	if ( ! (flags & SDL_OPENGL) ) {
		XSetWindowColormap(SDL_Display, SDL_Window, SDL_XColorMap);
		if( !SDL_windowid ) {
		    XSetWindowColormap(SDL_Display, FSwindow, SDL_XColorMap);
		    XSetWindowColormap(SDL_Display, WMwindow, SDL_XColorMap);
		}
	}

#if 0 /* This is an experiment - are the graphics faster now? - nope. */
	if ( SDL_getenv("SDL_VIDEO_X11_BACKINGSTORE") )
#endif
	/* Cache the window in the server, when possible */
	{
		Screen *xscreen;
		XSetWindowAttributes a;

		xscreen = ScreenOfDisplay(SDL_Display, SDL_Screen);
		a.backing_store = DoesBackingStore(xscreen);
		if ( a.backing_store != NotUseful ) {
			XChangeWindowAttributes(SDL_Display, SDL_Window,
			                        CWBackingStore, &a);
		}
	}

	/* Map them both and go fullscreen, if requested */
	if ( ! SDL_windowid ) {
        XWindowAttributes a;

        XMapWindow(SDL_Display, SDL_Window);

        /* WARNING: If WMwindow is already mapped, X11_WaitMapped() will hang indefinitely unless we check map state first. */

        /*          This code maps WMwindow once and then never unmaps it until shutdown. */
        /*          XFCE's window manager likes to move unmapped windows to the upper left hand corner. */
        /*          Rather than have to reposition the window back to where I had it, it's better not to unmap it in X11_DestroyWindow */
        memset(&a,0,sizeof(a));
        XGetWindowAttributes(SDL_Display, WMwindow, &a);
        if (a.map_state != IsViewable) {
            XMapWindow(SDL_Display, WMwindow);
            X11_WaitMapped(this, WMwindow);
        }

        if ( flags & SDL_FULLSCREEN ) {
			screen->flags |= SDL_FULLSCREEN;
			X11_EnterFullScreen(this);
		} else {
			screen->flags &= ~SDL_FULLSCREEN;
		}
	}
	
	return(0);
}

static int X11_ResizeWindow(_THIS,
			SDL_Surface *screen, int w, int h, Uint32 flags)
{
	int x = -999,y = -999;

	if ( ! SDL_windowid ) {
		/* Resize the window manager window */
		X11_SetSizeHints(this, w, h, flags);
		window_w = w;
		window_h = h;

		/* Center it, if desired */
		if (!X11_WindowPosition(this, &x, &y, w, h))
			x = y = -999;

		if (x > -999 && y > -999)
			XMoveResizeWindow(SDL_Display, WMwindow, x, y, w, h);
		else
			XResizeWindow(SDL_Display, WMwindow, w, h);

		if ((flags & SDL_HAX_NORESIZEWINDOW) && (flags & SDL_FULLSCREEN) == 0 && (screen->flags & SDL_FULLSCREEN) == 0) {
			/* do nothing */
		}
		else {
			XResizeWindow(SDL_Display, WMwindow, w, h);
		}

		/* Resize the fullscreen and display windows */
		if ( flags & SDL_FULLSCREEN ) {
			if ( screen->flags & SDL_FULLSCREEN ) {
				X11_ResizeFullScreen(this);
			} else {
				screen->flags |= SDL_FULLSCREEN;
				X11_EnterFullScreen(this);
			}
		} else {
			if ( screen->flags & SDL_FULLSCREEN ) {
				screen->flags &= ~SDL_FULLSCREEN;
				X11_LeaveFullScreen(this);
			}
		}
		XResizeWindow(SDL_Display, SDL_Window, w, h);
	}
	return(0);
}

SDL_Surface *X11_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	Uint32 saved_flags;

	/* Lock the event thread, in multi-threading environments */
	SDL_Lock_EventThread();

	/* Check the combination of flags we were passed */
	if ( flags & SDL_FULLSCREEN ) {
		/* Clear fullscreen flag if not supported */
		if ( SDL_windowid ) {
			flags &= ~SDL_FULLSCREEN;
		}
	}

	/* Flush any delayed updates */
	XSync(GFX_Display, False);

	/* Set up the X11 window */
	saved_flags = current->flags;
	if ( (SDL_Window) && ((saved_flags&SDL_OPENGL) == (flags&SDL_OPENGL))
	      && (bpp == current->format->BitsPerPixel)
          && ((saved_flags&SDL_NOFRAME) == (flags&SDL_NOFRAME)) ) {
        if (X11_ResizeWindow(this, current, width, height, flags) < 0) {
			current = NULL;
			goto done;
		}
		X11_PendingConfigureNotifyWidth = width;
		X11_PendingConfigureNotifyHeight = height;
	} else {
		if (X11_CreateWindow(this,current,width,height,bpp,flags) < 0) {
			current = NULL;
			goto done;
		}
	}

	/* Update the internal keyboard state */
	X11_SetKeyboardState(SDL_Display, NULL);

	/* When the window is first mapped, ignore non-modifier keys */
	if ( !current->w && !current->h ) {
		Uint8 *keys = SDL_GetKeyState(NULL);
		int i;
		for ( i = 0; i < SDLK_LAST; ++i ) {
			switch (i) {
			    case SDLK_NUMLOCK:
			    case SDLK_CAPSLOCK:
			    case SDLK_LCTRL:
			    case SDLK_RCTRL:
			    case SDLK_LSHIFT:
			    case SDLK_RSHIFT:
			    case SDLK_LALT:
			    case SDLK_RALT:
			    case SDLK_LMETA:
			    case SDLK_RMETA:
			    case SDLK_MODE:
				break;
			    default:
				keys[i] = SDL_RELEASED;
				break;
			}
		}
	}

	/* Set up the new mode framebuffer */
	if ( ((current->w != width) || (current->h != height)) ||
             ((saved_flags&SDL_OPENGL) != (flags&SDL_OPENGL)) ) {
		current->w = width;
		current->h = height;
		current->pitch = SDL_CalculatePitch(current);
		if (X11_ResizeImage(this, current, flags) < 0) {
			current = NULL;
			goto done;
		}
	}

	/* Clear these flags and set them only if they are in the new set. */
	current->flags &= ~(SDL_RESIZABLE|SDL_NOFRAME|SDL_HAX_NOREFRESH);
	current->flags |= (flags&(SDL_RESIZABLE|SDL_NOFRAME|SDL_HAX_NOREFRESH));

  done:
	/* Release the event thread */
	XSync(SDL_Display, False);
	SDL_Unlock_EventThread();

	/* We're done! */
	return(current);
}

static int X11_ToggleFullScreen(_THIS, int on)
{
	Uint32 event_thread;

	/* Don't switch if we don't own the window */
	if ( SDL_windowid ) {
		return(0);
	}

	/* Don't lock if we are the event thread */
	event_thread = SDL_EventThreadID();
	if ( event_thread && (SDL_ThreadID() == event_thread) ) {
		event_thread = 0;
	}
	if ( event_thread ) {
		SDL_Lock_EventThread();
	}
	if ( on ) {
		this->screen->flags |= SDL_FULLSCREEN;
		X11_EnterFullScreen(this);
	} else {
		this->screen->flags &= ~SDL_FULLSCREEN;
		X11_LeaveFullScreen(this);
	}
	X11_RefreshDisplay(this);
	if ( event_thread ) {
		SDL_Unlock_EventThread();
	}
	SDL_ResetKeyboard();
	return(1);
}

/* Update the current mouse state and position */
static void X11_UpdateMouse(_THIS)
{
	Window u1; int u2;
	Window current_win;
	int x, y;
	unsigned int mask;

	/* Lock the event thread, in multi-threading environments */
	SDL_Lock_EventThread();
	if ( XQueryPointer(SDL_Display, SDL_Window, &u1, &current_win,
	                   &u2, &u2, &x, &y, &mask) ) {
		if ( (x >= 0) && (x < SDL_VideoSurface->w) &&
		     (y >= 0) && (y < SDL_VideoSurface->h) ) {
			SDL_PrivateAppActive(1, SDL_APPMOUSEFOCUS);
			SDL_PrivateMouseMotion(0, 0, x, y);
		} else {
			SDL_PrivateAppActive(0, SDL_APPMOUSEFOCUS);
		}
	}
	SDL_Unlock_EventThread();
}

/* simple colour distance metric. Supposed to be better than a plain
   Euclidian distance anyway. */
#define COLOUR_FACTOR 3
#define LIGHT_FACTOR 1
#define COLOUR_DIST(r1, g1, b1, r2, g2, b2)				\
	(COLOUR_FACTOR * (abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2))	\
	 + LIGHT_FACTOR * abs(r1 + g1 + b1 - (r2 + g2 + b2)))

static void allocate_nearest(_THIS, SDL_Color *colors,
			     SDL_Color *want, int nwant)
{
	/*
	 * There is no way to know which ones to choose from, so we retrieve
	 * the entire colormap and try the nearest possible, until we find one
	 * that is shared.
	 */
	XColor all[256];
	int i;
	for(i = 0; i < 256; i++)
		all[i].pixel = i;
	/* 
	 * XQueryColors sets the flags in the XColor struct, so we use
	 * that to keep track of which colours are available
	 */
	XQueryColors(GFX_Display, SDL_XColorMap, all, 256);

	for(i = 0; i < nwant; i++) {
		XColor *c;
		int j;
		int best = 0;
		int mindist = 0x7fffffff;
		int ri = want[i].r;
		int gi = want[i].g;
		int bi = want[i].b;
		for(j = 0; j < 256; j++) {
			int rj, gj, bj, d2;
			if(!all[j].flags)
				continue;	/* unavailable colour cell */
			rj = all[j].red >> 8;
			gj = all[j].green >> 8;
			bj = all[j].blue >> 8;
			d2 = COLOUR_DIST(ri, gi, bi, rj, gj, bj);
			if(d2 < mindist) {
				mindist = d2;
				best = j;
			}
		}
		if(SDL_XPixels[best])
			continue; /* already allocated, waste no more time */
		c = all + best;
		if(XAllocColor(GFX_Display, SDL_XColorMap, c)) {
			/* got it */
			colors[c->pixel].r = c->red >> 8;
			colors[c->pixel].g = c->green >> 8;
			colors[c->pixel].b = c->blue >> 8;
			++SDL_XPixels[c->pixel];
		} else {
			/* 
			 * The colour couldn't be allocated, probably being
			 * owned as a r/w cell by another client. Flag it as
			 * unavailable and try again. The termination of the
			 * loop is guaranteed since at least black and white
			 * are always there.
			 */
			c->flags = 0;
			i--;
		}
	}
}

int X11_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	int nrej = 0;

	/* Check to make sure we have a colormap allocated */
	if ( SDL_XPixels == NULL ) {
		return(0);
	}
	if ( (this->screen->flags & SDL_HWPALETTE) == SDL_HWPALETTE ) {
	        /* private writable colormap: just set the colours we need */
	        XColor  *xcmap;
		int i;
	        xcmap = SDL_stack_alloc(XColor, ncolors);
		if(xcmap == NULL)
		        return 0;
		for ( i=0; i<ncolors; ++i ) {
			xcmap[i].pixel = i + firstcolor;
			xcmap[i].red   = (colors[i].r<<8)|colors[i].r;
			xcmap[i].green = (colors[i].g<<8)|colors[i].g;
			xcmap[i].blue  = (colors[i].b<<8)|colors[i].b;
			xcmap[i].flags = (DoRed|DoGreen|DoBlue);
		}
		XStoreColors(GFX_Display, SDL_XColorMap, xcmap, ncolors);
		XSync(GFX_Display, False);
		SDL_stack_free(xcmap);
	} else {
	        /*
		 * Shared colormap: We only allocate read-only cells, which
		 * increases the likelyhood of colour sharing with other
		 * clients. The pixel values will almost certainly be
		 * different from the requested ones, so the user has to
		 * walk the colormap and see which index got what colour.
		 *
		 * We can work directly with the logical palette since it
		 * has already been set when we get here.
		 */
		SDL_Color *want, *reject;
	        unsigned long *freelist;
		int i;
		int nfree = 0;
		int nc = this->screen->format->palette->ncolors;
	        colors = this->screen->format->palette->colors;
		freelist = SDL_stack_alloc(unsigned long, nc);
		/* make sure multiple allocations of the same cell are freed */
	        for(i = 0; i < ncolors; i++) {
		        int pixel = firstcolor + i;
		        while(SDL_XPixels[pixel]) {
			        freelist[nfree++] = pixel;
				--SDL_XPixels[pixel];
			}
		}
		XFreeColors(GFX_Display, SDL_XColorMap, freelist, nfree, 0);
		SDL_stack_free(freelist);

		want = SDL_stack_alloc(SDL_Color, ncolors);
		reject = SDL_stack_alloc(SDL_Color, ncolors);
		SDL_memcpy(want, colors + firstcolor, ncolors * sizeof(SDL_Color));
		/* make sure the user isn't fooled by her own wishes
		   (black is safe, always available in the default colormap) */
		SDL_memset(colors + firstcolor, 0, ncolors * sizeof(SDL_Color));

		/* now try to allocate the colours */
		for(i = 0; i < ncolors; i++) {
		        XColor col;
			col.red = want[i].r << 8;
			col.green = want[i].g << 8;
			col.blue = want[i].b << 8;
			col.flags = DoRed | DoGreen | DoBlue;
			if(XAllocColor(GFX_Display, SDL_XColorMap, &col)) {
			        /* We got the colour, or at least the nearest
				   the hardware could get. */
			        colors[col.pixel].r = col.red >> 8;
				colors[col.pixel].g = col.green >> 8;
				colors[col.pixel].b = col.blue >> 8;
				++SDL_XPixels[col.pixel];
			} else {
				/*
				 * no more free cells, add it to the list
				 * of rejected colours
				 */
				reject[nrej++] = want[i];
			}
		}
		if(nrej)
			allocate_nearest(this, colors, reject, nrej);
		SDL_stack_free(reject);
		SDL_stack_free(want);
	}
	return nrej == 0;
}

int X11_SetGammaRamp(_THIS, Uint16 *ramp)
{
	int i, ncolors;
	XColor xcmap[256];

	/* See if actually setting the gamma is supported */
	if ( SDL_Visual->class != DirectColor ) {
	    SDL_SetError("Gamma correction not supported on this visual");
	    return(-1);
	}

	/* Calculate the appropriate palette for the given gamma ramp */
	ncolors = SDL_Visual->map_entries;
	for ( i=0; i<ncolors; ++i ) {
		Uint8 c = (256 * i / ncolors);
		xcmap[i].pixel = SDL_MapRGB(this->screen->format, c, c, c);
		xcmap[i].red   = ramp[0*256+c];
		xcmap[i].green = ramp[1*256+c];
		xcmap[i].blue  = ramp[2*256+c];
		xcmap[i].flags = (DoRed|DoGreen|DoBlue);
	}
	XStoreColors(GFX_Display, SDL_XColorMap, xcmap, ncolors);
	XSync(GFX_Display, False);
	return(0);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void X11_VideoQuit(_THIS)
{
	/* Shutdown everything that's still up */
	/* The event thread should be done, so we can touch SDL_Display */
	if ( SDL_Display != NULL ) {
		/* Flush any delayed updates */
		XSync(GFX_Display, False);

		/* Close the connection with the IM server */
		#ifdef X_HAVE_UTF8_STRING
		if (SDL_IC != NULL) {
			XUnsetICFocus(SDL_IC);
			XDestroyIC(SDL_IC);
			SDL_IC = NULL;
		}
		if (SDL_IM != NULL) {
			XCloseIM(SDL_IM);
			SDL_IM = NULL;
		}
		#endif

		/* Start shutting down the windows */
        if ( WMwindow )
            XUnmapWindow(SDL_Display, WMwindow);

		X11_DestroyImage(this, this->screen);
		X11_DestroyWindow(this, this->screen);
        destroy_aux_windows(this);
		X11_FreeVideoModes(this);
		if ( SDL_XColorMap != SDL_DisplayColormap ) {
			XFreeColormap(SDL_Display, SDL_XColorMap);
		}
		if ( SDL_iconcolors ) {
			unsigned long pixel;
			Colormap dcmap = DefaultColormap(SDL_Display,
							 SDL_Screen);
			for(pixel = 0; pixel < 256; ++pixel) {
				while(SDL_iconcolors[pixel] > 0) {
					XFreeColors(GFX_Display,
						    dcmap, &pixel, 1, 0);
					--SDL_iconcolors[pixel];
				}
			}
			SDL_free(SDL_iconcolors);
			SDL_iconcolors = NULL;
		} 

		/* Restore gamma settings if they've changed */
		if ( SDL_GetAppState() & SDL_APPACTIVE ) {
			X11_SwapVidModeGamma(this);
		}

		/* Free that blank cursor */
		if ( SDL_BlankCursor != NULL ) {
			this->FreeWMCursor(this, SDL_BlankCursor);
			SDL_BlankCursor = NULL;
		}

        /* If any pending errors are waiting, we want them NOW */
        if ( GFX_Display != NULL )
            XSync(GFX_Display, True/*discard events, we're shutting down*/);
        if ( SDL_Display != NULL )
            XSync(SDL_Display, True/*discard events, we're shutting down*/);

		/* Close the X11 graphics connection */
		if ( GFX_Display != NULL ) {
			XCloseDisplay(GFX_Display);
			GFX_Display = NULL;
		}

		/* Close the X11 display connection */
		XCloseDisplay(SDL_Display);
		SDL_Display = NULL;

		/* Reset the X11 error handlers */
		if ( XIO_handler ) {
			XSetIOErrorHandler(XIO_handler);
		}
		if ( X_handler ) {
			XSetErrorHandler(X_handler);
		}

		/* Unload GL library after X11 shuts down */
		X11_GL_UnloadLibrary(this);
	}
	if ( this->screen && (this->screen->flags & SDL_HWSURFACE) ) {
		/* Direct screen access, no memory buffer */
		this->screen->pixels = NULL;
	}

#if SDL_VIDEO_DRIVER_X11_XME
    XiGMiscDestroy();
#endif
}

#ifdef ENABLE_IM_EVENT
//#define DEBUG_XEVENTS

void destroy_callback_func(XIM current_ic, XPointer client_data, XPointer call_data)
{
    SDL_VideoDevice *this = current_video;
    xim_free(this);
}

void im_callback(XIM xim, XPointer client_data, XPointer call_data)
{
	XIMStyle input_style;
	XIMStyles *xim_styles = NULL;
	XIMCallback destroy;
	int j;

	XPoint spot;
	char *env_sdlim_style;

	SDL_VideoDevice *this = current_video;
	XVaNestedList preedit_attr = NULL;

	/*
	 *  Open connection to IM server.
	*/
	{
		char *env_xmodifiers = getenv("XMODIFIERS");
		if (env_xmodifiers != NULL) 
			im_name = XSetLocaleModifiers(env_xmodifiers);
		else 
			fprintf(stderr, "Warning: XMODIFIERS is unspecified\n");
	}
	if (! (IM_Context.SDL_XIM = XOpenIM(SDL_Display, NULL, NULL, NULL))) {
		SDL_SetError("Cannot open the connection to XIM server.");
#ifdef DEBUG_XEVENTS
		printf("Cannot open the connection to XIM server.\n");
#endif
		return;
	}

	destroy.callback = (XIMProc)destroy_callback_func;
	destroy.client_data = NULL;
	XSetIMValues(IM_Context.SDL_XIM, XNDestroyCallback, &destroy, NULL);

	/*
	 *  Detect the input style supported by XIM server.
	 */
	if (XGetIMValues(IM_Context.SDL_XIM, XNQueryInputStyle, &xim_styles, NULL) || !xim_styles) {
#ifdef DEBUG_XEVENTS
		printf("input method doesn't support any style.");
#endif
		SDL_SetError("input method doesn't support any style.");
		XCloseIM(IM_Context.SDL_XIM);
		return;
	}
#ifdef DEBUG_XEVENTS
	else {
		int i;
		for (i=0; i<xim_styles->count_styles; i++) {
			for (j=0; im_styles[j].description!=NULL; j++) {
				if (im_styles[j].style == xim_styles->supported_styles[i]) {
					printf("XIM server support input_style = %s\n", im_styles[j].description);
					break;
				}
			}
			if (im_styles[j].description==NULL)
				printf("XIM server support unknown input_style = %x\n", (unsigned)(xim_styles->supported_styles[i]));
		}
	}
#endif

	/*
	 *  Setting the XIM style.
	 */
	/* OverTheSpot input_style as the default */
	input_style = 0;
	for (j = 0; im_styles[j].description != NULL; j++) {
		if (! strcmp(im_styles[j].description, "OverTheSpot")) {
			input_style = im_styles[j].style;
			IM_Context.bEnable_OverTheSpot = 1;
#ifdef DEBUG_XEVENTS
			printf("OverTheSpot mode supported.\n");
#endif
		}
		if (! strcmp(im_styles[j].description, "OnTheSpot")) {
			IM_Context.bEnable_OnTheSpot = 1;
#ifdef DEBUG_XEVENTS
			printf("OnTheSpot mode supported.\n");
#endif
		}
		if (! strcmp(im_styles[j].description, "Root")) {
			IM_Context.bEnable_Root = 1;
#ifdef DEBUG_XEVENTS
			printf("Root mode supported.\n");
#endif
		}
	}

	/* If not support OverTheSpot mode, use Root mode. */
	if (input_style != (XIMPreeditPosition | XIMStatusNothing)) {
		SDL_SetError("The XIM doesn't support OverTheSpot mode.");
		for (j=0; im_styles[j].description!=NULL; j++) {
			if (! strcmp(im_styles[j].description, "Root")) {
#ifdef DEBUG_XEVENTS
				printf("Root\n");
#endif
				input_style = im_styles[j].style;
			}
		}

		/* If not support Root mode, use OnTheSpot mode.*/
		if (input_style != (XIMPreeditNothing | XIMStatusNothing)) {
			SDL_SetError("The XIM doesn't support OverTheSpot and Root mode.");
			for (j = 0; im_styles[j].description != NULL; j++) {
				if (! strcmp(im_styles[j].description, "OnTheSpot")) {
#ifdef DEBUG_XEVENTS
					printf("OnTheSpot\n");
#endif
					input_style = im_styles[j].style;
				}
			}
			if (input_style != (XIMPreeditCallbacks | XIMStatusCallbacks)) {
				SDL_SetError("The XIM doesn't support OverTheSpot, Root, and OnTheSpot mode.");
			}
		}
	}

	XFree(xim_styles);

	/* If the environmet variable SDLIM_STYLE is set,
	   override the style setting.
	   In current SDL-IM implementation, this is required
	   since some Input Methods don't work in OverTheSpot style
	   despite they tell they support OverTheSpot style. */
	env_sdlim_style = getenv("SDLIM_STYLE");
	if (env_sdlim_style != NULL) {
#ifdef DEBUG_XEVENTS
		printf("SDLIM_STYLE=%s\n", env_sdlim_style);
#endif
		if (strcmp(env_sdlim_style, "Root") == 0) {
			input_style = (XIMPreeditNothing | XIMStatusNothing);
		} else if (strcmp(env_sdlim_style, "OverTheSpot") == 0) {
			input_style = (XIMPreeditPosition | XIMStatusNothing);
		} else if (strcmp(env_sdlim_style, "OnTheSpot") == 0) {
			input_style = (XIMPreeditCallbacks | XIMStatusCallbacks);
		}
	}
#ifdef DEBUG_XEVENTS
	else
		printf("SDLIM_STYLE= \n");
#endif

#ifdef DEBUG_XEVENTS
	/* print which mode used. */
	switch(input_style)
	{
	case (XIMPreeditNothing | XIMStatusNothing):
		printf("use Root mode.\n");
		break;
	case (XIMPreeditPosition | XIMStatusNothing):
		printf("use OverTheSpot mode.\n");
		break;
	case (XIMPreeditCallbacks | XIMStatusCallbacks):
		printf("use OnTheSpot mode.\n");
		break;
	case  (XIMPreeditArea|XIMStatusArea):
		printf("use OffTheSpot mode.\n");
		break;
	default:
		printf("use Unknown mode.\n");
		break;
	}
#endif

	/* for XIMPreeditPosition(OverTheSpot) */
	preedit_attr = 0;
	spot.x = 0;
	spot.y = 2*ef_height + 3*(ef_ascent+5);
	preedit_attr = XVaCreateNestedList(0,
				       XNSpotLocation, &spot,   
				       (IM_Context.fontset) ? XNFontSet : NULL, 
				       IM_Context.fontset,
				       NULL);

	/*
	 *  Create IC.
	 */
	IM_Context.SDL_XIC = XCreateIC(IM_Context.SDL_XIM, 
				   XNInputStyle, input_style,
				   XNClientWindow, WMwindow,
				   XNFocusWindow, WMwindow,
				   (input_style & (XIMPreeditPosition | XIMStatusNothing) &&
				    preedit_attr) ? 
				   XNPreeditAttributes : NULL, preedit_attr,
				   NULL);

	if(IM_Context.SDL_XIC == NULL) {
		// try Root mode
#ifdef DEBUG_XEVENTS
		printf("Cannot create XIC. Try Root mode.\n");
#endif
		input_style = (XIMPreeditNothing | XIMStatusNothing);
		IM_Context.SDL_XIC = XCreateIC(IM_Context.SDL_XIM, 
				    XNInputStyle, input_style,
				    XNClientWindow, WMwindow,
				    XNFocusWindow, WMwindow,
				    NULL);

		if (IM_Context.SDL_XIC == NULL) {
#ifdef DEBUG_XEVENTS
			printf("Cannot create XIC. ");
#endif
			SDL_SetError("Cannot create XIC.");
			return;
		}
	}
	IM_Context.preedit_attr_now = preedit_attr;
	IM_Context.im_style_now = input_style;

	XSetICFocus(IM_Context.SDL_XIC);
	XUnsetICFocus(IM_Context.SDL_XIC);

	return;
}

int create_fontset(void)
{
	int  i, fsize, charset_count, fontset_count = 1;
	char *s1, *s2;
	char **charset_list, *def_string;
	XFontStruct **font_structs;
	char *fontset_name = NULL;
	SDL_VideoDevice *this = current_video;

	fontset_name = getenv("SDLIM_FONTSET");
	if (fontset_name == NULL || !isprint(*fontset_name)) {
#ifdef DEBUG_XEVENTS
		printf("Please set environment variable: SDLIM_FONTSET\nbash ex.\n\texport SDLIM_FONTSET=*-ISO8859-1,*-BIG5-0\n");
#endif
		//SDL_SetError("Please set environment variable: SDLIM_FONTSET\nbash ex.\n\texport SDLIM_FONTSET=*-ISO8859-1,*-BIG5-0");
		//fontset_name = "*-ISO8859-1";
		fontset_name = "-*-fixed-medium-r-normal--16-*-*-*";
	}
#ifdef DEBUG_XEVENTS
	printf("IM fontset: %s\n", fontset_name);
#endif

	/*
	 *  Calculate the number of fonts.
	 */
	s1 = fontset_name;
	while ((s2=strchr(s1, ',')) != NULL) {
		s2 ++;
		while (isspace((int)(*s2)))
			s2++;
		if (*s2 && *s2 != ',') {
			fontset_count++;
			s1 = s2;
		}
		else {
			break;
			*s1 = '\0';
		}
	}
	/*
	 *  Create fontset and extract font information.
	 */
	IM_Context.fontset = XCreateFontSet(SDL_Display, fontset_name, &charset_list, &charset_count, &def_string);
	if (charset_count || !IM_Context.fontset) {
		SDL_SetError("Error: cannot create fontset.");
#ifdef DEBUG_XEVENTS
		printf("Error: cannot create fontset. %d %d %s\n", charset_count, IM_Context.fontset, def_string);
#endif
		return 0;
	}
	if (fontset_count != XFontsOfFontSet(IM_Context.fontset, &font_structs, &charset_list)) {
		SDL_SetError("Warning: fonts not consistant to fontset.");
#ifdef DEBUG_XEVENTS
		printf("Warning: fonts not consistant to fontset.\n");
#endif
		fontset_count = XFontsOfFontSet(IM_Context.fontset, &font_structs, &charset_list);
	}


	for (i = 0; i < fontset_count; i++) {
		fsize = font_structs[i]->max_bounds.width / 2;
		if (fsize > ef_width)
			ef_width = fsize;
		fsize = font_structs[i]->ascent + font_structs[i]->descent;
		if (fsize > ef_height) {
			ef_height = fsize;
			ef_ascent = font_structs[i]->ascent;
		}
	}

	if (charset_list)
		XFreeStringList(charset_list);

	return 0;
}

int locale_init(void)
{
	char buf[1024];

	if ((lc_ctype = setlocale(LC_CTYPE, "")) == NULL) {
		SDL_SetError("setlocale LC_CTYPE false.");
#ifdef DEBUG_XEVENTS
		printf("setlocale LC_CTYPE false.\n");
#endif
		return 0;
	}

	if (XSupportsLocale() != True) {
		SDL_SetError("XSupportsLocale false.");
#ifdef DEBUG_XEVENTS
		printf("XSupportsLocale false.\n");
#endif
		return 0;
	}

	if (im_name){
		sprintf(buf, "@im=%s", im_name);
	}
	else {
		/* clean up buf if environment variable wasn't specified. */
		memset(buf, 0, 1024);
	}

	if (XSetLocaleModifiers(buf) == NULL) {
		SDL_SetError("XSetLocaleModifiers false.");
#ifdef DEBUG_XEVENTS
		SDL_SetError("XSetLocaleModifiers false.\n");
#endif
		return 0;
	}

    create_fontset();

    return 1;
}

int xim_init(_THIS)
{
	IM_Context.SDL_XIM = NULL;
	IM_Context.SDL_XIC = NULL;
	IM_Context.string.im_wide_char_buffer = '\0';
	IM_Context.im_buffer_len = 0;
	IM_Context.im_compose_len = 0;
	IM_Context.ic_focus = 0;
	IM_Context.im_enable = 0;
	IM_Context.bEnable_OverTheSpot = 0;
	IM_Context.bEnable_OnTheSpot = 0;
	IM_Context.bEnable_Root = 0;
	IM_Context.preedit_attr_orig = NULL;
	IM_Context.status_attr_orig = NULL;
	IM_Context.im_style_orig = 0;
	IM_Context.preedit_attr_now = NULL;
	IM_Context.status_attr_now = NULL;
	IM_Context.im_style_now = 0;
	IM_Context.fontset = NULL;

	if (!locale_init()) {
		return 0;
	}

	if (XRegisterIMInstantiateCallback(SDL_Display, NULL, NULL, NULL, (XIMProc)im_callback, NULL) != True) {
		SDL_SetError("XRegisterIMInstantiateCallback false.");
#ifdef DEBUG_XEVENTS
		printf("XRegisterIMInstantiateCallback false.\n");
#endif
		return 0;
	}
	return 1;
}

static void xim_free(_THIS)
{
	if (IM_Context.SDL_XIM) {
		XCloseIM(IM_Context.SDL_XIM);
	}
	if (IM_Context.SDL_XIC) {
		XDestroyIC(IM_Context.SDL_XIC);
	}
	IM_Context.SDL_XIC = NULL;
	IM_Context.SDL_XIM = NULL;
	IM_Context.ic_focus = 0;

	IM_Context.im_compose_len = 0;
	IM_Context.im_buffer_len = 0;
	if (IM_Context.string.im_wide_char_buffer) {
		free(IM_Context.string.im_wide_char_buffer);
	}
	/*IM_Context.string.im_wide_char_buffer = '\0';*/

	IM_Context.bEnable_OverTheSpot = 0;
	IM_Context.bEnable_OnTheSpot = 0;
	IM_Context.bEnable_Root = 0;

	if (IM_Context.preedit_attr_orig) {
		XFree(IM_Context.preedit_attr_orig);
	}
	if ((IM_Context.preedit_attr_now != IM_Context.preedit_attr_orig) && IM_Context.preedit_attr_now) {
		XFree(IM_Context.preedit_attr_now);
	}
	IM_Context.preedit_attr_now = NULL;
	IM_Context.preedit_attr_orig = NULL;

	if (IM_Context.fontset) {
		XFreeFontSet(SDL_Display, IM_Context.fontset);
	}
	IM_Context.fontset = NULL;
}

#endif /* ENABLE_IM_EVENT */
