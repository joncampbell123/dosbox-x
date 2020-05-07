
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

void UpdateWindowDimensions(Bitu width, Bitu height);
void UpdateWindowMaximized(bool flag);

#if C_X11_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

/* The XRandR extension may or may not be there */
#if C_X11_XRANDR
bool X11_XRR_avail = false;
bool X11_XRR_checked = false;

bool X11_XRR_check(Display *display) {
    if (!X11_XRR_checked) {
        X11_XRR_checked = true;

        int major=0,minor=0,err=0;

        if (XQueryExtension(display,"RANDR",&major,&minor,&err) && major != 0)
            X11_XRR_avail = true;
        else
            X11_XRR_avail = false;

        LOG_MSG("X11 extension XRANDR is %s",X11_XRR_avail ? "available" : "not available");
    }

    return X11_XRR_avail;
}
#endif

#if !defined(C_SDL2)
extern "C" void SDL1_hax_X11_jpfix(int ro_scan,int yen_scan);
#endif

#if 1
# define _NET_WM_STATE_REMOVE        0    // remove/unset property
# define _NET_WM_STATE_ADD           1    // add/set property
# define _NET_WM_STATE_TOGGLE        2    // toggle property
#endif

void LinuxX11_OnTop(bool f) {
    (void)f;

    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
    # if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            Atom wmStateAbove = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_ABOVE", 1);
            if (wmStateAbove == None) return;

            Atom wmNetWmState = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE", 1);
            if (wmNetWmState == None) return;

            XClientMessageEvent xclient;
            memset(&xclient,0,sizeof(xclient));

            xclient.type = ClientMessage;
            xclient.window = wminfo.info.x11.window;
            xclient.message_type = wmNetWmState;
            xclient.format = 32;
            xclient.data.l[0] = f ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
            xclient.data.l[1] = (long int)wmStateAbove;
            xclient.data.l[2] = 0;
            xclient.data.l[3] = 0;
            xclient.data.l[4] = 0;

            XSendEvent(
                    wminfo.info.x11.display,
                    DefaultRootWindow(wminfo.info.x11.display),
                    False,
                    SubstructureRedirectMask | SubstructureNotifyMask,
                    (XEvent *)&xclient );
        }
    # endif
    }
#else
    if (SDL_GetWMInfo(&wminfo) >= 0) {
    # if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            Atom wmStateAbove = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_ABOVE", 1);
            if (wmStateAbove == None) return;

            Atom wmNetWmState = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE", 1);
            if (wmNetWmState == None) return;

            XClientMessageEvent xclient;
            memset(&xclient,0,sizeof(xclient));

            xclient.type = ClientMessage;
            xclient.window = wminfo.info.x11.wmwindow;
            xclient.message_type = wmNetWmState;
            xclient.format = 32;
            xclient.data.l[0] = f ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
            xclient.data.l[1] = (long int)wmStateAbove;
            xclient.data.l[2] = 0;
            xclient.data.l[3] = 0;
            xclient.data.l[4] = 0;

            XSendEvent(
                    wminfo.info.x11.display,
                    DefaultRootWindow(wminfo.info.x11.display),
                    False,
                    SubstructureRedirectMask | SubstructureNotifyMask,
                    (XEvent *)&xclient );
        }
    # endif
    }
#endif
}

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

void Linux_JPXKBFix(void) {
#if !defined(C_SDL2)
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
#  if C_X11_XKB
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            /* detect xkbmap with Ro and Yen keys mapped to \, determine the scan codes,
             * and then hack the SDL 1.x X11 driver to handle it.
             *
             * Never assume specific scan codes, even though on my system the scan codes
             * are 97 (Yen) and 132 (Ro) both mapped to \ backslash.
             *
             * We can use the index to look for keysyms that mention backslash and underscore (Ro)
             * or backslash and bar (Yen) */
            /* TODO: If xkbmap actually maps these keys properly, how can we tell? */
            int ro=-1,yen=-1;
            unsigned int i,j;
            KeySym sym[4];

            for (i=0;i < 256;i++) {
                for (j=0;j < 4;j++)
                    sym[j] = XKeycodeToKeysym(wminfo.info.x11.display,(KeyCode)i,(int)j);

                if (sym[0] == '\\') {
                    if (sym[1] == '_') { /* shift + backslash == _   means we found Ro */
                        if (ro < 0) ro = (int)i;
                    }
                    else if (sym[1] == '|') { /* shift + backslash == |   means we found Yen */
                        if (yen < 0) yen = (int)i;
                    }
                }
            }

            LOG_MSG("JP Linux/X11 fix: Found Ro=%d Yen=%d",ro,yen);

            SDL1_hax_X11_jpfix(ro,yen);
        }
#  endif
# endif
    }
#endif
}

unsigned int Linux_GetKeyboardLayout(void) {
    unsigned int ret = DKM_US;

    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
# if C_X11
#  if C_X11_XKB
#   if C_X11_EXT_XKBRULES
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XkbRF_VarDefsRec vd = {0};
            XkbStateRec state = {0};

            XkbGetState(wminfo.info.x11.display, XkbUseCoreKbd, &state);

            XkbDescPtr desc = XkbGetKeyboard(wminfo.info.x11.display, XkbAllComponentsMask, XkbUseCoreKbd);
            char *group = desc ? XGetAtomName(wminfo.info.x11.display, desc->names->groups[state.group]) : NULL;

            if (group != NULL) LOG_MSG("Current X11 keyboard layout (full name) is: '%s'\n",group);

            /* FIXME: Does this allocate anything? Do I need to free it? I am concerned about memory leaks --J.C. */
            XkbRF_GetNamesProp(wminfo.info.x11.display, NULL, &vd);

            char *tok = vd.layout ? strtok(vd.layout, ",") : NULL;

            if (tok != NULL) {
                for (int i = 0; i < state.group; i++) {
                    tok = strtok(NULL, ",");
                    if (tok == NULL) break;
                }
                if (tok != NULL) {
                    LOG_MSG("Current X11 keyboard layout (token) is: '%s'\n",tok);

                    if (!strcmp(tok,"us"))
                        ret = DKM_US;
                    else if (!strcmp(tok,"jp"))
                        ret = DKM_JPN;
                    else if (!strcmp(tok,"de"))
                        ret = DKM_DEU;
                }
            }

            if (group) XFree(group);
            if (desc) XFree(desc);
        }
#   endif
#  endif
# endif
    }
#else
    if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
#  if C_X11_XKB
#   if C_X11_EXT_XKBRULES
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XkbRF_VarDefsRec vd = {0};
            XkbStateRec state = {0};

            XkbGetState(wminfo.info.x11.display, XkbUseCoreKbd, &state);

            XkbDescPtr desc = XkbGetKeyboard(wminfo.info.x11.display, XkbAllComponentsMask, XkbUseCoreKbd);
            char *group = desc ? XGetAtomName(wminfo.info.x11.display, desc->names->groups[state.group]) : NULL;

            if (group != NULL) LOG_MSG("Current X11 keyboard layout (full name) is: '%s'\n",group);

            /* FIXME: Does this allocate anything? Do I need to free it? I am concerned about memory leaks --J.C. */
            XkbRF_GetNamesProp(wminfo.info.x11.display, NULL, &vd);

            char *tok = vd.layout ? strtok(vd.layout, ",") : NULL;

            if (tok != NULL) {
                for (int i = 0; i < state.group; i++) {
                    tok = strtok(NULL, ",");
                    if (tok == NULL) break;
                }
                if (tok != NULL) {
                    LOG_MSG("Current X11 keyboard layout (token) is: '%s'\n",tok);

                    if (!strcmp(tok,"us"))
                        ret = DKM_US;
                    else if (!strcmp(tok,"jp"))
                        ret = DKM_JPN;
                    else if (!strcmp(tok,"de"))
                        ret = DKM_DEU;
                }
            }

            if (group) XFree(group);
            if (desc) XFree(desc);
        }
#   endif
#  endif
# endif
    }
#endif

    return ret;
}

void UpdateWindowDimensions_Linux(void) {
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
# if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XWindowAttributes attr;
            bool maximized = false;

            memset(&attr,0,sizeof(attr));
            XGetWindowAttributes(wminfo.info.x11.display, wminfo.info.x11.window, &attr);

            /* we also want to ask X11 if the various atoms have been set by the window manager
             * to signal that our window has been maximized. generally when the window has been
             * maximized, SDL video mode setting has no effect on the dimensions of the window
             * when it's been maximized so it helps to know in order to make better choices */
            if (GFX_IsFullscreen()) {
            }
            else {
                Atom atomWmState = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE",                False);
                Atom atomMaxVert = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
                Atom atomMaxHorz = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

                unsigned long nItem = 0, bytesAfter = 0;
                unsigned char *properties = NULL;
                Atom type = None;
                int format = 0;

                XGetWindowProperty(wminfo.info.x11.display, wminfo.info.x11.window,
                    /*atom to query*/atomWmState,/*offset*/0,/*length*/16384,/*delete*/False,
                    /*request type*/AnyPropertyType,
                    /*returns...*/&type,&format,&nItem,&bytesAfter,&properties);

                if (properties != NULL) {
                    if (format == 32) { /* it usually is */
                        for (unsigned long i=0;i < nItem;i++) {
                            uint32_t val = ((uint32_t*)properties)[i];

                            if ((Atom)val == atomMaxVert || (Atom)val == atomMaxHorz)
                                maximized = true;
                        }
                    }

                    XFree(properties);
                }
            }

            LOG_MSG("X11 main window is %u x %u maximized=%u",attr.width, attr.height, maximized);

            UpdateWindowDimensions((unsigned int)attr.width, (unsigned int)attr.height);
            UpdateWindowMaximized(maximized);
        }
# endif
    }
#else
    if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            XWindowAttributes attr;
            bool maximized = false;

            memset(&attr,0,sizeof(attr));
            XGetWindowAttributes(wminfo.info.x11.display, GFX_IsFullscreen() ? wminfo.info.x11.fswindow : wminfo.info.x11.wmwindow, &attr);

            /* we also want to ask X11 if the various atoms have been set by the window manager
             * to signal that our window has been maximized. generally when the window has been
             * maximized, SDL video mode setting has no effect on the dimensions of the window
             * when it's been maximized so it helps to know in order to make better choices */
            if (GFX_IsFullscreen()) {
            }
            else {
                Atom atomWmState = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE",                False);
                Atom atomMaxVert = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
                Atom atomMaxHorz = XInternAtom(wminfo.info.x11.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

                unsigned long nItem = 0, bytesAfter = 0;
                unsigned char *properties = NULL;
                Atom type = None;
                int format = 0;

                XGetWindowProperty(wminfo.info.x11.display, wminfo.info.x11.wmwindow,
                    /*atom to query*/atomWmState,/*offset*/0,/*length*/16384,/*delete*/False,
                    /*request type*/AnyPropertyType,
                    /*returns...*/&type,&format,&nItem,&bytesAfter,&properties);

                if (properties != NULL) {
                    if (format == 32) { /* it usually is */
                        for (unsigned long i=0;i < nItem;i++) {
                            uint32_t val = ((uint32_t*)properties)[i];

                            if ((Atom)val == atomMaxVert || (Atom)val == atomMaxHorz)
                                maximized = true;
                        }
                    }

                    XFree(properties);
                }
            }

            LOG_MSG("X11 main window is %u x %u maximized=%u",attr.width, attr.height, maximized);

            UpdateWindowDimensions((unsigned int)attr.width, (unsigned int)attr.height);
            UpdateWindowMaximized(maximized);
        }
# endif
    }
#endif
}

#if C_X11
/* Retrieve screen size/dimensions/DPI using XRandR */
static bool Linux_TryXRandrGetDPI(ScreenSizeInfo &info,Display *display,Window window) {
    bool result = false;
# if C_X11_XRANDR
    XRRScreenResources *xr_screen;
    XWindowAttributes attr;
    int x = 0, y = 0;
    Window child;

    /* do not use XRandR if the X11 server does not support it */
    if (!X11_XRR_check(display))
        return false;

    memset(&attr,0,sizeof(attr));
    XGetWindowAttributes(display, window, &attr);
    XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child );

    attr.x = x - attr.x;
    attr.y = y - attr.y;

    if ((xr_screen=XRRGetScreenResources(display, DefaultRootWindow(display))) != NULL) {
        /* Look for a valid CRTC, don't assume the first is valid (as the StackOverflow example does) */
        for (int c=0;c < xr_screen->ncrtc;c++) {
            XRRCrtcInfo *chk = XRRGetCrtcInfo(display, xr_screen, xr_screen->crtcs[c]);
            if (chk == NULL) continue;
            if (chk->width == 0 || chk->height == 0) continue;

            LOG_MSG("XRandR CRTC %u: pos=(%d,%d) size=(%d,%d) outputs=%d",
                c,chk->x,chk->y,chk->width,chk->height,chk->noutput);

            /* match our window position to the display, use the center */
            int match_x = attr.x + (attr.width / 2);
            int match_y = attr.y + (attr.height / 2);

            if (match_x >= chk->x && match_x < (chk->x+(int)chk->width) &&
                match_y >= chk->y && match_y < (chk->y+(int)chk->height)) {
                LOG_MSG("Our window lies on this CRTC display (window pos=(%d,%d) size=(%d,%d) match=(%d,%d)).",
                    attr.x,attr.y,
                    attr.width,attr.height,
                    match_x,match_y);

                if (chk->noutput > 0) {
                    for (int o=0;o < chk->noutput;o++) {
                        XRROutputInfo *ochk = XRRGetOutputInfo(display, xr_screen, chk->outputs[o]);
                        if (ochk == NULL) continue;

                        std::string oname;

                        if (ochk->nameLen > 0 && ochk->name != NULL)
                            oname = std::string(ochk->name,(size_t)ochk->nameLen);

                        LOG_MSG("  Goes to output %u: name='%s' size_mm=(%lu x %lu)",
                                o,oname.c_str(),ochk->mm_width,ochk->mm_height);

                        if (true/*TODO: Any decision making?*/) {
                            o = chk->noutput; /* short circuit the for loop */
                            c = xr_screen->ncrtc; /* and the other */
                            result = true;

                            /* choose this combo to determine screen size, and dimensions */
                            info.method = METHOD_XRANDR;

                            info.screen_position_pixels.x        = chk->x;
                            info.screen_position_pixels.y        = chk->y;

                            info.screen_dimensions_pixels.width  = chk->width;
                            info.screen_dimensions_pixels.height = chk->height;

                            info.screen_dimensions_mm.width      = ochk->mm_width;
                            info.screen_dimensions_mm.height     = ochk->mm_height;

                            if (info.screen_dimensions_mm.width > 0)
                                info.screen_dpi.width =
                                    ((((double)info.screen_dimensions_pixels.width) * 25.4) /
                                     ((double)info.screen_dimensions_mm.width));

                            if (info.screen_dimensions_mm.height > 0)
                                info.screen_dpi.height =
                                    ((((double)info.screen_dimensions_pixels.height) * 25.4) /
                                     ((double)info.screen_dimensions_mm.height));
                        }

                        XRRFreeOutputInfo(ochk);
                        ochk = NULL;
                    }
                }
            }

            XRRFreeCrtcInfo(chk);
            chk = NULL;
        }

        XRRFreeScreenResources(xr_screen);
        xr_screen = NULL;
    }
#else
    (void)info;    //UNUSED
    (void)display; //UNUSED
    (void)window;  //UNUSED
# endif

    return result;
}
#endif

void Linux_GetWindowDPI(ScreenSizeInfo &info) {
    info.clear();

	SDL_SysWMinfo wminfo;
	memset(&wminfo,0,sizeof(wminfo));
	SDL_VERSION(&wminfo.version);

#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);

    if (SDL_GetWindowWMInfo(GFX_GetSDLWindow(),&wminfo) >= 0) {
# if C_X11
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            if (Linux_TryXRandrGetDPI(info,wminfo.info.x11.display,wminfo.info.x11.window)) {
                /* got it */
            }
            else {
                /* fallback to X11 method, which may not return accurate info on modern systems */
                Window rootWindow = DefaultRootWindow(wminfo.info.x11.display);
                if (rootWindow != 0) {
                    int screen = 0;

                    info.method = METHOD_X11;

                    /* found on StackOverflow */

                    /*
                     * there are 2.54 centimeters to an inch; so there are 25.4 millimeters.
                     *
                     *     dpi = N pixels / (M millimeters / (25.4 millimeters / 1 inch))
                     *         = N pixels / (M inch / 25.4)
                     *         = N * 25.4 pixels / M inch
                     */
                    info.screen_dimensions_pixels.width  = DisplayWidth(   wminfo.info.x11.display,screen);
                    info.screen_dimensions_pixels.height = DisplayHeight(  wminfo.info.x11.display,screen);

                    info.screen_dimensions_mm.width      = DisplayWidthMM( wminfo.info.x11.display,screen);
                    info.screen_dimensions_mm.height     = DisplayHeightMM(wminfo.info.x11.display,screen);

                    if (info.screen_dimensions_mm.width > 0)
                        info.screen_dpi.width =
                            ((((double)info.screen_dimensions_pixels.width) * 25.4) /
                             ((double)info.screen_dimensions_mm.width));

                    if (info.screen_dimensions_mm.height > 0)
                        info.screen_dpi.height =
                            ((((double)info.screen_dimensions_pixels.height) * 25.4) /
                             ((double)info.screen_dimensions_mm.height));
                }
            }
        }
# endif
    }
#else
	if (SDL_GetWMInfo(&wminfo) >= 0) {
# if C_X11
		if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            if (Linux_TryXRandrGetDPI(info,wminfo.info.x11.display,GFX_IsFullscreen() ? wminfo.info.x11.fswindow : wminfo.info.x11.wmwindow)) {
                /* got it */
            }
            else {
                /* fallback to X11 method, which may not return accurate info on modern systems */
                Window rootWindow = DefaultRootWindow(wminfo.info.x11.display);
                if (rootWindow != 0) {
                    int screen = 0;

                    info.method = METHOD_X11;

                    /* found on StackOverflow */

                    /*
                     * there are 2.54 centimeters to an inch; so there are 25.4 millimeters.
                     *
                     *     dpi = N pixels / (M millimeters / (25.4 millimeters / 1 inch))
                     *         = N pixels / (M inch / 25.4)
                     *         = N * 25.4 pixels / M inch
                     */
                    info.screen_dimensions_pixels.width  = DisplayWidth(   wminfo.info.x11.display,screen);
                    info.screen_dimensions_pixels.height = DisplayHeight(  wminfo.info.x11.display,screen);

                    info.screen_dimensions_mm.width      = DisplayWidthMM( wminfo.info.x11.display,screen);
                    info.screen_dimensions_mm.height     = DisplayHeightMM(wminfo.info.x11.display,screen);

                    if (info.screen_dimensions_mm.width > 0)
                        info.screen_dpi.width =
                            ((((double)info.screen_dimensions_pixels.width) * 25.4) /
                             ((double)info.screen_dimensions_mm.width));

                    if (info.screen_dimensions_mm.height > 0)
                        info.screen_dpi.height =
                            ((((double)info.screen_dimensions_pixels.height) * 25.4) /
                             ((double)info.screen_dimensions_mm.height));
                }
            }
        }
# endif
    }
#endif
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

