
#include "config.h"

#ifdef LINUX
#include "logging.h"
#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"

void UpdateWindowDimensions(Bitu width, Bitu height);

void UpdateWindowDimensions_Linux(void) {
#if defined(C_SDL2)
    // TODO
#else
	SDL_SysWMinfo wminfo;
	memset(&wminfo,0,sizeof(wminfo));
	SDL_VERSION(&wminfo.version);
	if (SDL_GetWMInfo(&wminfo) >= 0) {
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XWindowAttributes attr;

            memset(&attr,0,sizeof(attr));
            XGetWindowAttributes(wminfo.info.x11.display, wminfo.info.x11.wmwindow, &attr);

            LOG_MSG("X11 main window is %u x %u",attr.width, attr.height);

            UpdateWindowDimensions(attr.width, attr.height);
        }
    }
#endif
}

void Linux_GetDesktopResolution(int *width,int *height) {
#if defined(C_SDL2)
    *width = 1024; // guess
    *height = 768;
#else
	/* We're most likely running on an X-windows desktop (through SDL). */
	SDL_SysWMinfo wminfo;
	memset(&wminfo,0,sizeof(wminfo));
	SDL_VERSION(&wminfo.version);
	if (SDL_GetWMInfo(&wminfo) >= 0) {
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
			LOG_MSG("GetDesktopResolution reading X11 desktop resolution");

			Window rootWindow = DefaultRootWindow(wminfo.info.x11.display);
			if (rootWindow >= 0) {
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
		else {
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

