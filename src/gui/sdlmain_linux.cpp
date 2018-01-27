
#include "config.h"

#ifdef LINUX
#include <limits.h>
#include "logging.h"
#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"

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

#if defined(LINUX) && !defined(C_SDL2)
void UpdateWindowDimensionsLinux(void) {
    void UpdateWindowDimensions(Bitu width, Bitu height);
    void UpdateMaxWindow(bool x);

	SDL_SysWMinfo wminfo;
	memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XWindowAttributes attr = {0};
            bool maxwindow = false;

            XGetWindowAttributes(wminfo.info.x11.display, wminfo.info.x11.wmwindow, &attr);

            {
                Atom maxVert = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
                Atom maxHorz = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
                Atom wmState = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE", True);
                unsigned char *properties = NULL;
                unsigned long bytesAfter = 0;
                unsigned long nItem = 0;
                int format = 0;
                Atom type = 0;

                XGetWindowProperty(wminfo.info.x11.display, wminfo.info.x11.wmwindow, wmState,
                    0, LONG_MAX, False, AnyPropertyType, &type, &format, &nItem, &bytesAfter, &properties);

                if (properties != NULL && format == 32) {
                    for (unsigned long i=0;i < nItem;i++) {
                        if (((uint32_t*)(properties))[i] == maxHorz)
                            maxwindow = true;
                        else if (((uint32_t*)(properties))[i] == maxVert)
                            maxwindow = true;
                    }

                    XFree(properties);
                }
            }

            LOG_MSG("Current window dimensions are %u x %u maxwindow=%u",attr.width,attr.height,maxwindow);

            UpdateMaxWindow(maxwindow);
            UpdateWindowDimensions(attr.width,attr.height);
        }
    }
}
#endif

