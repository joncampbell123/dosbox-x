
#include "config.h"

#ifdef LINUX
#include "logging.h"
#include "keymap.h"
#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "sdlmain.h"

#if C_X11
# if C_X11_XKB
#  include <X11/XKBlib.h>
# endif
# if C_X11_EXT_XKBRULES
#  include <X11/extensions/XKBrules.h>
# endif
#endif

#if C_X11_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

char *LinuxX11_KeySymName(Uint32 x) {
    (void)x;

    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
# if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            return XKeysymToString(x);
        }
# endif
    }
#else
    if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            return XKeysymToString(x);
        }
# endif
    }
#endif

    return NULL;
}

void Linux_GetDesktopResolution(int *width,int *height) {
	/* We're most likely running on an X-windows desktop (through SDL). */
	SDL_SysWMinfo wminfo;
	memset(&wminfo,0,sizeof(wminfo));
	SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
# if C_X11
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
			LOG_MSG("GetDesktopResolution reading X11 desktop resolution");

			Window rootWindow = DefaultRootWindow(wminfo.info.x11.display);
			if (rootWindow != 0) {
				XWindowAttributes rootWinAttr;

				memset(&rootWinAttr,0,sizeof(rootWinAttr));
				XGetWindowAttributes(wminfo.info.x11.display,rootWindow,&rootWinAttr);
				LOG_MSG("Root window (ID %lu) is %lu x %lu",(unsigned long)rootWindow,(unsigned long)rootWinAttr.width,(unsigned long)rootWinAttr.height);

				if (rootWinAttr.width > 0 && rootWinAttr.height > 0) {
					*width = (int)rootWinAttr.width;
					*height = (int)rootWinAttr.height;
				}
				else {
					*width = 1024; // guess
					*height = 768;
				}
			}
			else {
				*width = 1024; // guess
				*height = 768;
			}
		}
		else
# endif
        {
			*width = 1024; // guess
			*height = 768;
		}
	}
	else {
		*width = 1024; // guess
		*height = 768;
	}
#else
	if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
			LOG_MSG("GetDesktopResolution reading X11 desktop resolution");

			Window rootWindow = DefaultRootWindow(wminfo.info.x11.display);
			if (rootWindow != 0) {
				XWindowAttributes rootWinAttr;

				memset(&rootWinAttr,0,sizeof(rootWinAttr));
				XGetWindowAttributes(wminfo.info.x11.display,rootWindow,&rootWinAttr);
				LOG_MSG("Root window (ID %lu) is %lu x %lu",(unsigned long)rootWindow,(unsigned long)rootWinAttr.width,(unsigned long)rootWinAttr.height);

				if (rootWinAttr.width > 0 && rootWinAttr.height > 0) {
					*width = (int)rootWinAttr.width;
					*height = (int)rootWinAttr.height;
				}
				else {
					*width = 1024; // guess
					*height = 768;
				}
			}
			else {
				*width = 1024; // guess
				*height = 768;
			}
		}
		else
# endif
        {
			*width = 1024; // guess
			*height = 768;
		}
	}
	else {
		*width = 1024; // guess
		*height = 768;
	}
#endif
}
#endif

