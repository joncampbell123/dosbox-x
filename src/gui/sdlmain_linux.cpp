
#include "config.h"

#ifdef LINUX
#include "logging.h"
#include "keymap.h"
#include "SDL.h"
#include "SDL_version.h"
#include "SDL_syswm.h"

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

void UpdateWindowDimensions(Bitu width, Bitu height);
void UpdateWindowMaximized(bool flag);

#if !defined(C_SDL2)
extern "C" void SDL1_hax_X11_jpfix(int ro_scan,int yen_scan);

# define _NET_WM_STATE_REMOVE        0    // remove/unset property
# define _NET_WM_STATE_ADD           1    // add/set property
# define _NET_WM_STATE_TOGGLE        2    // toggle property
#endif

void LinuxX11_OnTop(bool f) {
#if !defined(C_SDL2)
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
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
            xclient.data.l[1] = wmStateAbove;
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
    }
#endif
}

char *LinuxX11_KeySymName(Uint32 x) {
#if !defined(C_SDL2)
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
        if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
            return XKeysymToString(x);
        }
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
                    sym[j] = XKeycodeToKeysym(wminfo.info.x11.display,(KeyCode)i,j);

                if (sym[0] == '\\') {
                    if (sym[1] == '_') { /* shift + backslash == _   means we found Ro */
                        if (ro < 0) ro = i;
                    }
                    else if (sym[1] == '|') { /* shift + backslash == |   means we found Yen */
                        if (yen < 0) yen = i;
                    }
                }
            }

            LOG_MSG("JP Linux/X11 fix: Found Ro=%d Yen=%d",ro,yen);

            SDL1_hax_X11_jpfix(ro,yen);
        }
    }
#endif
}

unsigned int Linux_GetKeyboardLayout(void) {
    unsigned int ret = DKM_US;

#if defined(C_SDL2)
    // TODO
#else
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
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
    }
#endif

    return ret;
}

void UpdateWindowDimensions_Linux(void) {
#if defined(C_SDL2)
    // TODO
#else
    bool GFX_IsFullscreen();

    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
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

            UpdateWindowDimensions(attr.width, attr.height);
            UpdateWindowMaximized(maximized);
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

