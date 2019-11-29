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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

#ifdef SDL_WIN32_NO_PARENT_WINDOW
# define ParentWindowHWND SDL_Window
#else
extern HWND ParentWindowHWND;
#endif

#include "SDL_main.h"
#include "SDL_events.h"
#include "SDL_syswm.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../wincommon/SDL_lowvideo.h"
#include "SDL_gapidibvideo.h"
#include "SDL_vkeys.h"

void (*SDL1_hax_INITMENU_cb)() = NULL;

#ifdef SDL_VIDEO_DRIVER_GAPI
#include "../gapi/SDL_gapivideo.h"
#endif

#ifdef SDL_VIDEO_DRIVER_WINDIB
#include "SDL_dibvideo.h"
#endif

#ifndef WM_APP
#define WM_APP	0x8000
#endif

#ifdef _WIN32_WCE
#define NO_GETKEYBOARDSTATE
#endif

#ifdef SDL_WIN32_HX_DOS
#define NO_GETKEYBOARDSTATE
#endif

HKL hLayout = NULL;
unsigned char hLayoutChanged = 0;

unsigned char SDL1_hax_hasLayoutChanged(void) {
	return hLayoutChanged;
}

void SDL1_hax_ackLayoutChanged(void) {
	hLayoutChanged = 0;
}

/* The translation table from a Microsoft VK keysym to a SDL keysym */
static SDLKey VK_keymap[SDLK_LAST];
static SDL_keysym *TranslateKey(WPARAM vkey, UINT scancode, SDL_keysym *keysym, int pressed);
static SDLKey Arrows_keymap[4];

/* Masks for processing the windows KEYDOWN and KEYUP messages */
#define REPEATED_KEYMASK	(1<<30)
#define EXTENDED_KEYMASK	(1<<24)

/* DJM: If the user setup the window for us, we want to save his window proc,
   and give him a chance to handle some messages. */
#ifdef STRICT
#define WNDPROCTYPE	WNDPROC
#else
#define WNDPROCTYPE	FARPROC
#endif
static WNDPROCTYPE userWindowProc = NULL;


#ifdef SDL_VIDEO_DRIVER_GAPI

WPARAM rotateKey(WPARAM key,int direction) 
{
	if(direction ==0 ) return key;
	
	switch (key) {
		case 0x26: /* up */
			return Arrows_keymap[(2 + direction) % 4];
		case 0x27: /* right */
			return Arrows_keymap[(1 + direction) % 4];
		case 0x28: /* down */
			return Arrows_keymap[direction % 4];
		case 0x25: /* left */
			return Arrows_keymap[(3 + direction) % 4];
	}

	return key;
}

static void GapiTransform(GapiInfo *gapiInfo, LONG *x, LONG *y)
{
    if(gapiInfo->hiresFix)
    {
	*x *= 2;
	*y *= 2;
    }

    // 0 3 0
    if((!gapiInfo->userOrientation && gapiInfo->systemOrientation && !gapiInfo->gapiOrientation) ||
    // 3 0 3
       (gapiInfo->userOrientation && !gapiInfo->systemOrientation && gapiInfo->gapiOrientation) ||
    // 3 0 0
       (gapiInfo->userOrientation && !gapiInfo->systemOrientation && !gapiInfo->gapiOrientation))
    {
	Sint16 temp = *x;
        *x = SDL_VideoSurface->w - *y;
        *y = temp;
    }
    else
    // 0 0 0
    if((!gapiInfo->userOrientation && !gapiInfo->systemOrientation && !gapiInfo->gapiOrientation) ||
    // 0 0 3
      (!gapiInfo->userOrientation && !gapiInfo->systemOrientation && gapiInfo->gapiOrientation))
    {
	// without changes
	// *x = *x;
	// *y = *y;
    }
    // default
    else
    {
	// without changes
	// *x = *x;
	// *y = *y;
    }
}
#endif 

/* DOSBox-X deviation: hack to ignore Num/Scroll/Caps if set */
#if defined(WIN32)
unsigned char _dosbox_x_hack_ignore_toggle_keys = 0;
unsigned char _dosbox_x_hack_wm_user100_to_keyevent = 0;

void __declspec(dllexport) SDL_DOSBox_X_Hack_Set_Toggle_Key_WM_USER_Hack(unsigned char x) {
	_dosbox_x_hack_ignore_toggle_keys = x;
	_dosbox_x_hack_wm_user100_to_keyevent = x;
}
#endif

/* SDL has only so much queue, we don't want to pass EVERY message
   that comes to us into it. As this SDL code is specialized for
   DOSBox-X, only messages that DOSBox-X would care about are
   queued. */
int Win32_ShouldPassMessageToSysWMEvent(UINT msg) {
	switch (msg) {
		case WM_COMMAND:
		case WM_SYSCOMMAND:
			return 1;
	}

	return 0;
}

/* The main Win32 event handler */
LRESULT DIB_HandleMessage(_THIS, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#if defined(WIN32)
	unsigned char toggle_key_xlat = 0;
#endif
	extern int posted;

	/* DOSBox-X deviation: alternate route for user Num/Scroll/Caps from LL keyboard hook */
#if defined(WIN32)
	if (_dosbox_x_hack_wm_user100_to_keyevent) {
		if (msg == (WM_USER + 0x100)) {
			toggle_key_xlat = 1;
			msg = WM_KEYDOWN;
		}
		else if (msg == (WM_USER + 0x101)) {
			toggle_key_xlat = 1;
			msg = WM_KEYUP;
		}
	}
	if (_dosbox_x_hack_ignore_toggle_keys && !toggle_key_xlat) {
		if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
			if (wParam == VK_NUMLOCK || wParam == VK_CAPITAL || wParam == VK_SCROLL)
				return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	}
#endif

	switch (msg) {
		case WM_KILLFOCUS:
# ifndef SDL_WIN32_HX_DOS
			if (!SDL_resizing &&
				 SDL_PublicSurface &&
				(SDL_PublicSurface->flags & SDL_FULLSCREEN)) {
				/* In fullscreen mode, this window must have focus... or else we must exit fullscreen mode! */
				ShowWindow(ParentWindowHWND, SW_RESTORE);
			}
# endif
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			SDL_keysym keysym;

#ifdef SDL_VIDEO_DRIVER_GAPI
			if(this->hidden->gapiInfo)
			{
				// Drop GAPI artefacts
				if (wParam == 0x84 || wParam == 0x5B)
					return 0;

				wParam = rotateKey(wParam, this->hidden->gapiInfo->coordinateTransform);
			}
#endif 
			switch (wParam) {
				case VK_CONTROL:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RCONTROL;
					else
						wParam = VK_LCONTROL;
					break;
				case VK_SHIFT:
					/* EXTENDED trick doesn't work here */
					{
					Uint8 *state = SDL_GetKeyState(NULL);
					if (state[SDLK_LSHIFT] == SDL_RELEASED && (GetKeyState(VK_LSHIFT) & 0x8000)) {
                        posted = SDL_PrivateKeyboard(SDL_PRESSED,
                            TranslateKey(wParam = VK_LSHIFT, HIWORD(lParam), &keysym, 0));
                    }
                    if (state[SDLK_RSHIFT] == SDL_RELEASED && (GetKeyState(VK_RSHIFT) & 0x8000)) {
                        posted = SDL_PrivateKeyboard(SDL_PRESSED,
                            TranslateKey(wParam = VK_RSHIFT, HIWORD(lParam), &keysym, 0));
					}
                    if (wParam == VK_SHIFT) {
						/* Win9x */
						int sc = HIWORD(lParam) & 0xFF;

						if (sc == 0x2A)
							wParam = VK_LSHIFT;
						else if (sc == 0x36)
							wParam = VK_RSHIFT;
						else
							wParam = VK_LSHIFT;
					}
                    else {
                        return(0);
                    }
					}
					break;
				case VK_MENU:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RMENU;
					else
						wParam = VK_LMENU;
					break;
                default:
                    /* Ignore repeated keys */
                    if (lParam&REPEATED_KEYMASK) {
                        return(0);
                    }
                    break;
			}
#ifdef NO_GETKEYBOARDSTATE
			/* this is the workaround for the missing ToAscii() and ToUnicode() in CE (not necessary at KEYUP!) */
			if ( SDL_TranslateUNICODE ) {
				MSG m;

				m.hwnd = hwnd;
				m.message = msg;
				m.wParam = wParam;
				m.lParam = lParam;
				m.time = 0;
				if ( TranslateMessage(&m) && PeekMessage(&m, hwnd, 0, WM_USER, PM_NOREMOVE) && (m.message == WM_CHAR) ) {
					GetMessage(&m, hwnd, 0, WM_USER);
			    		wParam = m.wParam;
				}
			}
#endif /* NO_GETKEYBOARDSTATE */
			posted = SDL_PrivateKeyboard(SDL_PRESSED,
				TranslateKey(wParam,HIWORD(lParam),&keysym,1));
		}
		return(0);

		case WM_INITMENU:
#ifdef SDL_WIN32_NO_PARENT_WINDOW
			if (SDL1_hax_INITMENU_cb != NULL)
				SDL1_hax_INITMENU_cb();
#endif
			break;

		case WM_SYSKEYUP:
		case WM_KEYUP: {
			SDL_keysym keysym;

#ifdef SDL_VIDEO_DRIVER_GAPI
			if(this->hidden->gapiInfo)
			{
				// Drop GAPI artifacts
				if (wParam == 0x84 || wParam == 0x5B)
					return 0;
	
				wParam = rotateKey(wParam, this->hidden->gapiInfo->coordinateTransform);
			}
#endif

			switch (wParam) {
				case VK_CONTROL:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RCONTROL;
					else
						wParam = VK_LCONTROL;
					break;
                case VK_SHIFT:
                    /* EXTENDED trick doesn't work here */
                {
                    Uint8 *state = SDL_GetKeyState(NULL);
                    if (state[SDLK_LSHIFT] == SDL_PRESSED && !(GetKeyState(VK_LSHIFT) & 0x8000)) {
                        posted = SDL_PrivateKeyboard(SDL_RELEASED,
                            TranslateKey(wParam = VK_LSHIFT, HIWORD(lParam), &keysym, 0));
                    }
                    if (state[SDLK_RSHIFT] == SDL_PRESSED && !(GetKeyState(VK_RSHIFT) & 0x8000)) {
                        posted = SDL_PrivateKeyboard(SDL_RELEASED,
                            TranslateKey(wParam = VK_RSHIFT, HIWORD(lParam), &keysym, 0));
                    }
                    if (wParam == VK_SHIFT) {
                        /* Win9x */
                        int sc = HIWORD(lParam) & 0xFF;

                        if (sc == 0x2A)
                            wParam = VK_LSHIFT;
                        else if (sc == 0x36)
                            wParam = VK_RSHIFT;
                        else
                            wParam = VK_LSHIFT;
                    }
                    else {
                        return(0);
                    }
                }
                break;
                case VK_MENU:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RMENU;
					else
						wParam = VK_LMENU;
					break;
			}
			/* Windows only reports keyup for print screen */
			if ( wParam == VK_SNAPSHOT && SDL_GetKeyState(NULL)[SDLK_PRINT] == SDL_RELEASED ) {
				posted = SDL_PrivateKeyboard(SDL_PRESSED,
					TranslateKey(wParam,HIWORD(lParam),&keysym,1));
			}
			posted = SDL_PrivateKeyboard(SDL_RELEASED,
				TranslateKey(wParam,HIWORD(lParam),&keysym,0));
		}
		return(0);
#if defined(SC_SCREENSAVE) && defined(SC_MONITORPOWER)
		case WM_SYSCOMMAND: {
			const DWORD val = (DWORD) (wParam & 0xFFF0);
			if ((val == SC_SCREENSAVE) || (val == SC_MONITORPOWER)) {
				if (this->hidden->dibInfo && !allow_screensaver) {
					/* Note that this doesn't stop anything on Vista
					   if the screensaver has a password. */
					return(0);
				}
			}
		}
		/* Fall through to default processing */
#endif /* SC_SCREENSAVE && SC_MONITORPOWER */

		default: {
			/* Only post the event if we're watching for it */
			if (SDL_ProcessEvents[SDL_SYSWMEVENT] == SDL_ENABLE) {
				SDL_SysWMmsg wmmsg;

				/* Stop queuing all the various blather the Win32 world
				   throws at us, we only have so much queue to hold it.
				   Queue only what is important. */
				if (Win32_ShouldPassMessageToSysWMEvent(msg)) {
					SDL_VERSION(&wmmsg.version);
					wmmsg.hwnd = hwnd;
					wmmsg.msg = msg;
					wmmsg.wParam = wParam;
					wmmsg.lParam = lParam;
					posted = SDL_PrivateSysWMEvent(&wmmsg);
				}

			/* DJM: If the user isn't watching for private
				messages in her SDL event loop, then pass it
				along to any win32 specific window proc.
			 */
			} else if (userWindowProc) {
				return CallWindowProc(userWindowProc, hwnd, msg, wParam, lParam);
			}
		}
		break;
	}
	return(DefWindowProc(hwnd, msg, wParam, lParam));
}

#ifdef _WIN32_WCE
static BOOL GetLastStylusPos(POINT* ptLast)
{
    BOOL bResult = FALSE;
    UINT nRet;
    GetMouseMovePoints(ptLast, 1, &nRet);
    if ( nRet == 1 ) {
        ptLast->x /= 4;
        ptLast->y /= 4;
        bResult = TRUE;
    }
    return bResult;
}
#endif

static void DIB_GenerateMouseMotionEvent(_THIS)
{
	extern int mouse_relative;
	extern int posted;

	POINT mouse;
#ifdef _WIN32_WCE
	if ( !GetCursorPos(&mouse) && !GetLastStylusPos(&mouse) ) return;
#else
	if ( !GetCursorPos(&mouse) ) return;
#endif

	if ( mouse_relative ) {
		POINT center;
		center.x = (SDL_VideoSurface->w/2);
		center.y = (SDL_VideoSurface->h/2);
		ClientToScreen(SDL_Window, &center);

		mouse.x -= center.x;
		mouse.y -= center.y;
		if ( mouse.x || mouse.y ) {
			SetCursorPos(center.x, center.y);
			posted = SDL_PrivateMouseMotion(0, 1, (Sint16)mouse.x, (Sint16)mouse.y);
		}
	} else {
		ScreenToClient(SDL_Window, &mouse);
#ifdef SDL_VIDEO_DRIVER_GAPI
       if (SDL_VideoSurface && this->hidden->gapiInfo)
			GapiTransform(this->hidden->gapiInfo, &mouse.x, &mouse.y);
#endif
		posted = SDL_PrivateMouseMotion(0, 0, (Sint16)mouse.x, (Sint16)mouse.y);
	}
}

#include "SDL_timer.h"

static unsigned long last_dib_mouse_motion = 0;

void DIB_PumpEvents(_THIS)
{
	/* NTS: Impose a 60Hz cap on mouse motion polling because SetCursorPos/GetCursorPos
	        no longer have an immediate effect on cursor position in the way that the
			DIB mouse motion code expects. In Windows 10 there seems to be enough of
			a round-trip delay between SetCursorPos/GetCursorPos and the compositor
			to cause the DIB mouse motion code to often register many duplicate mouse
			movements that never happened. Imposing a time interval between polling
			seems to fix this. This should not have any negative effects on older
			versions of Windows. --J.C. */
	unsigned long pollInterval = 1000 / 60; /* 60Hz mouse motion polling rate */
	unsigned long now = SDL_GetTicks();
	_Bool mouseMotion = 0;
	MSG msg;

	while ( PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) ) {
		if ( GetMessage(&msg, NULL, 0, 0) > 0 ) {
			DispatchMessage(&msg);
		}
	}

    /* *sigh* It seems the Windows keyboard events allow us to sort of
       differentiate left/right shift, but only as individual keys,
       and Windows 10 won't send a WM_KEYUP if you release one shift
       key while holding the other. Therefore, this hack:

       If both shift keys are held down, poll the keyboard state to
       detect when either one is released. */
    Uint8 *state = SDL_GetKeyState(NULL);

    if (state[SDLK_LSHIFT] == SDL_PRESSED && state[SDLK_RSHIFT] == SDL_PRESSED) {
        if (state[SDLK_LSHIFT] == SDL_PRESSED) {
            SDL_keysym keysym;

            if (!(GetKeyState(VK_LSHIFT) & 0x8000)) {
                SDL_PrivateKeyboard(SDL_RELEASED,
                    TranslateKey(VK_LSHIFT, HIWORD(0), &keysym, 0));
            }
        }
        if (state[SDLK_RSHIFT] == SDL_PRESSED) {
            SDL_keysym keysym;

            if (!(GetKeyState(VK_RSHIFT) & 0x8000)) {
                SDL_PrivateKeyboard(SDL_RELEASED,
                    TranslateKey(VK_RSHIFT, HIWORD(0), &keysym, 0));
            }
        }
    }

	if (!mouseMotion && now > (last_dib_mouse_motion + pollInterval))
		mouseMotion = 1;

	if ( mouseMotion && (SDL_GetAppState() & SDL_APPMOUSEFOCUS) ) {
		DIB_GenerateMouseMotionEvent( this );
		last_dib_mouse_motion = now;
	}
}

void DIB_CheckMouse(void) {
	DIB_GenerateMouseMotionEvent(NULL/*FIXME*/);
	last_dib_mouse_motion = SDL_GetTicks();
}

void DIB_InitOSKeymapPriv(void) {
	int	i;

	hLayout = GetKeyboardLayout(0);

	/* Map the VK keysyms */
	for (i = 0; i<SDL_arraysize(VK_keymap); ++i)
		VK_keymap[i] = SDLK_UNKNOWN;

	VK_keymap[VK_BACK] = SDLK_BACKSPACE;
	VK_keymap[VK_TAB] = SDLK_TAB;
	VK_keymap[VK_CLEAR] = SDLK_CLEAR;
	VK_keymap[VK_RETURN] = SDLK_RETURN;
	VK_keymap[VK_PAUSE] = SDLK_PAUSE;
	VK_keymap[VK_ESCAPE] = SDLK_ESCAPE;
	VK_keymap[VK_SPACE] = SDLK_SPACE;
	VK_keymap[VK_APOSTROPHE] = SDLK_QUOTE;
	VK_keymap[VK_COMMA] = SDLK_COMMA;
	VK_keymap[VK_MINUS] = SDLK_MINUS;
	VK_keymap[VK_PERIOD] = SDLK_PERIOD;
	VK_keymap[VK_SLASH] = SDLK_SLASH;
	VK_keymap[VK_0] = SDLK_0;
	VK_keymap[VK_1] = SDLK_1;
	VK_keymap[VK_2] = SDLK_2;
	VK_keymap[VK_3] = SDLK_3;
	VK_keymap[VK_4] = SDLK_4;
	VK_keymap[VK_5] = SDLK_5;
	VK_keymap[VK_6] = SDLK_6;
	VK_keymap[VK_7] = SDLK_7;
	VK_keymap[VK_8] = SDLK_8;
	VK_keymap[VK_9] = SDLK_9;
	VK_keymap[VK_SEMICOLON] = SDLK_SEMICOLON;
	VK_keymap[VK_EQUALS] = SDLK_EQUALS;
	VK_keymap[VK_LBRACKET] = SDLK_LEFTBRACKET;
	VK_keymap[VK_BACKSLASH] = SDLK_BACKSLASH;
	VK_keymap[VK_RBRACKET] = SDLK_RIGHTBRACKET;
	VK_keymap[VK_GRAVE] = SDLK_BACKQUOTE;
	VK_keymap[VK_BACKTICK] = SDLK_BACKQUOTE;
	VK_keymap[VK_A] = SDLK_a;
	VK_keymap[VK_B] = SDLK_b;
	VK_keymap[VK_C] = SDLK_c;
	VK_keymap[VK_D] = SDLK_d;
	VK_keymap[VK_E] = SDLK_e;
	VK_keymap[VK_F] = SDLK_f;
	VK_keymap[VK_G] = SDLK_g;
	VK_keymap[VK_H] = SDLK_h;
	VK_keymap[VK_I] = SDLK_i;
	VK_keymap[VK_J] = SDLK_j;
	VK_keymap[VK_K] = SDLK_k;
	VK_keymap[VK_L] = SDLK_l;
	VK_keymap[VK_M] = SDLK_m;
	VK_keymap[VK_N] = SDLK_n;
	VK_keymap[VK_O] = SDLK_o;
	VK_keymap[VK_P] = SDLK_p;
	VK_keymap[VK_Q] = SDLK_q;
	VK_keymap[VK_R] = SDLK_r;
	VK_keymap[VK_S] = SDLK_s;
	VK_keymap[VK_T] = SDLK_t;
	VK_keymap[VK_U] = SDLK_u;
	VK_keymap[VK_V] = SDLK_v;
	VK_keymap[VK_W] = SDLK_w;
	VK_keymap[VK_X] = SDLK_x;
	VK_keymap[VK_Y] = SDLK_y;
	VK_keymap[VK_Z] = SDLK_z;
	VK_keymap[VK_DELETE] = SDLK_DELETE;

	VK_keymap[VK_NUMPAD0] = SDLK_KP0;
	VK_keymap[VK_NUMPAD1] = SDLK_KP1;
	VK_keymap[VK_NUMPAD2] = SDLK_KP2;
	VK_keymap[VK_NUMPAD3] = SDLK_KP3;
	VK_keymap[VK_NUMPAD4] = SDLK_KP4;
	VK_keymap[VK_NUMPAD5] = SDLK_KP5;
	VK_keymap[VK_NUMPAD6] = SDLK_KP6;
	VK_keymap[VK_NUMPAD7] = SDLK_KP7;
	VK_keymap[VK_NUMPAD8] = SDLK_KP8;
	VK_keymap[VK_NUMPAD9] = SDLK_KP9;
	VK_keymap[VK_DECIMAL] = SDLK_KP_PERIOD;
	VK_keymap[VK_DIVIDE] = SDLK_KP_DIVIDE;
	VK_keymap[VK_MULTIPLY] = SDLK_KP_MULTIPLY;
	VK_keymap[VK_SUBTRACT] = SDLK_KP_MINUS;
	VK_keymap[VK_ADD] = SDLK_KP_PLUS;

	VK_keymap[VK_UP] = SDLK_UP;
	VK_keymap[VK_DOWN] = SDLK_DOWN;
	VK_keymap[VK_RIGHT] = SDLK_RIGHT;
	VK_keymap[VK_LEFT] = SDLK_LEFT;
	VK_keymap[VK_INSERT] = SDLK_INSERT;
	VK_keymap[VK_HOME] = SDLK_HOME;
	VK_keymap[VK_END] = SDLK_END;
	VK_keymap[VK_PRIOR] = SDLK_PAGEUP;
	VK_keymap[VK_NEXT] = SDLK_PAGEDOWN;

	VK_keymap[VK_F1] = SDLK_F1;
	VK_keymap[VK_F2] = SDLK_F2;
	VK_keymap[VK_F3] = SDLK_F3;
	VK_keymap[VK_F4] = SDLK_F4;
	VK_keymap[VK_F5] = SDLK_F5;
	VK_keymap[VK_F6] = SDLK_F6;
	VK_keymap[VK_F7] = SDLK_F7;
	VK_keymap[VK_F8] = SDLK_F8;
	VK_keymap[VK_F9] = SDLK_F9;
	VK_keymap[VK_F10] = SDLK_F10;
	VK_keymap[VK_F11] = SDLK_F11;
	VK_keymap[VK_F12] = SDLK_F12;
	VK_keymap[VK_F13] = SDLK_F13;
	VK_keymap[VK_F14] = SDLK_F14;
	VK_keymap[VK_F15] = SDLK_F15;

	VK_keymap[VK_NUMLOCK] = SDLK_NUMLOCK;
	VK_keymap[VK_CAPITAL] = SDLK_CAPSLOCK;
	VK_keymap[VK_SCROLL] = SDLK_SCROLLOCK;
	VK_keymap[VK_RSHIFT] = SDLK_RSHIFT;
	VK_keymap[VK_LSHIFT] = SDLK_LSHIFT;
	VK_keymap[VK_RCONTROL] = SDLK_RCTRL;
	VK_keymap[VK_LCONTROL] = SDLK_LCTRL;
	VK_keymap[VK_RMENU] = SDLK_RALT;
	VK_keymap[VK_LMENU] = SDLK_LALT;
	VK_keymap[VK_RWIN] = SDLK_RSUPER;
	VK_keymap[VK_LWIN] = SDLK_LSUPER;

	VK_keymap[VK_HELP] = SDLK_HELP;
#ifdef VK_PRINT
	VK_keymap[VK_PRINT] = SDLK_PRINT;
#endif
	VK_keymap[VK_SNAPSHOT] = SDLK_PRINT;
	VK_keymap[VK_CANCEL] = SDLK_BREAK;
	VK_keymap[VK_APPS] = SDLK_MENU;

	VK_keymap[VK_OEM_102] = SDLK_LESS;

	/* per-layout adjustments */
	switch (LOWORD(hLayout)) {
		case 0x411: /* JP */
			VK_keymap[VK_OEM_PERIOD] = SDLK_PERIOD;
			VK_keymap[VK_OEM_MINUS] = SDLK_MINUS;
			VK_keymap[VK_OEM_COMMA] = SDLK_COMMA;
			VK_keymap[VK_OEM_PLUS] = SDLK_SEMICOLON;
			VK_keymap[VK_OEM_102] = SDLK_JP_RO;
			VK_keymap[VK_OEM_1] = SDLK_COLON;
			VK_keymap[VK_OEM_7] = SDLK_CARET;
			VK_keymap[VK_OEM_3] = SDLK_AT;
			VK_keymap[VK_OEM_4] = SDLK_LEFTBRACKET;
			VK_keymap[VK_OEM_6] = SDLK_RIGHTBRACKET;
			VK_keymap[VK_OEM_5] = SDLK_JP_YEN;
			break;
	}

	Arrows_keymap[3] = 0x25;
	Arrows_keymap[2] = 0x26;
	Arrows_keymap[1] = 0x27;
	Arrows_keymap[0] = 0x28;
}

void DIB_InitOSKeymap(_THIS)
{
	DIB_InitOSKeymapPriv();
}

#define EXTKEYPAD(keypad) ((scancode & 0x100)?(mvke):(keypad))

static int SDL_MapVirtualKey(int scancode, int vkey)
{
#ifndef _WIN32_WCE
	int	mvke  = MapVirtualKeyEx(scancode & 0xFF, 1, hLayout);
#else
	int	mvke  = MapVirtualKey(scancode & 0xFF, 1);
#endif

#if 0 /* set to 1 to debug VK scancodes i.e. if debugging foreign keyboard layouts and SDL 1.x */
	{
		char tmp[128];
		char tmp2[256];

		tmp[0] = 0;
		GetKeyNameText(scancode << 16, tmp, sizeof(tmp) - 1);
		sprintf(tmp2, "Scan 0x%x VK 0x%x name=%s\n", scancode, mvke, tmp);
		OutputDebugString(tmp2);
	}
#endif

	switch(vkey) {
		/* These are always correct */
		case VK_DIVIDE:
		case VK_MULTIPLY:
		case VK_SUBTRACT:
		case VK_ADD:
		case VK_LWIN:
		case VK_RWIN:
		case VK_APPS:
		/* These are already handled */
		case VK_LCONTROL:
		case VK_RCONTROL:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_LMENU:
		case VK_RMENU:
		case VK_SNAPSHOT:
		case VK_PAUSE:
			return vkey;
	}	
	switch(mvke) {
		/* Distinguish between keypad and extended keys */
		case VK_INSERT: return EXTKEYPAD(VK_NUMPAD0);
		case VK_DELETE: return EXTKEYPAD(VK_DECIMAL);
		case VK_END:    return EXTKEYPAD(VK_NUMPAD1);
		case VK_DOWN:   return EXTKEYPAD(VK_NUMPAD2);
		case VK_NEXT:   return EXTKEYPAD(VK_NUMPAD3);
		case VK_LEFT:   return EXTKEYPAD(VK_NUMPAD4);
		case VK_CLEAR:  return EXTKEYPAD(VK_NUMPAD5);
		case VK_RIGHT:  return EXTKEYPAD(VK_NUMPAD6);
		case VK_HOME:   return EXTKEYPAD(VK_NUMPAD7);
		case VK_UP:     return EXTKEYPAD(VK_NUMPAD8);
		case VK_PRIOR:  return EXTKEYPAD(VK_NUMPAD9);
	}
	return mvke?mvke:vkey;
}

static SDL_keysym *TranslateKey(WPARAM vkey, UINT scancode, SDL_keysym *keysym, int pressed)
{
	/* Set the keysym information */
	keysym->win32_vk = (Uint32)vkey;
	keysym->scancode = (unsigned char) scancode;
	keysym->mod = KMOD_NONE;
	keysym->unicode = 0;

	if ((vkey == VK_RETURN) && (scancode & 0x100)) {
		/* No VK_ code for the keypad enter key */
		keysym->sym = SDLK_KP_ENTER;
	}
	else {
		keysym->sym = VK_keymap[SDL_MapVirtualKey(scancode, (int)vkey)];
	}

	if ( pressed && SDL_TranslateUNICODE ) {
#ifdef NO_GETKEYBOARDSTATE
		/* Uh oh, better hope the vkey is close enough.. */
		if((keysym->sym == vkey) || (vkey > 0x7f))
		keysym->unicode = vkey;
#else
		BYTE	keystate[256];
		Uint16	wchars[2];

		GetKeyboardState(keystate);
		/* Numlock isn't taken into account in ToUnicode,
		 * so we handle it as a special case here */
		if ((keystate[VK_NUMLOCK] & 1) && vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9)
		{
			keysym->unicode = (Uint16)(vkey - VK_NUMPAD0 + '0');
		}
		else if (SDL_ToUnicode((UINT)vkey, scancode, keystate, wchars, sizeof(wchars)/sizeof(wchars[0]), 0) > 0)
		{
			keysym->unicode = wchars[0];
		}
#endif /* NO_GETKEYBOARDSTATE */
	}

	return(keysym);
}

#ifndef SDL_WIN32_NO_PARENT_WINDOW
/*-----------------------------------------------------------*/
HANDLE			ParentWindowThread = INVALID_HANDLE_VALUE;
DWORD			ParentWindowThreadID = 0;
HWND			ParentWindowHWND = NULL;
volatile int	ParentWindowInit = 0;
volatile int	ParentWindowShutdown = 0;
volatile int	ParentWindowReady = 0;
volatile BOOL	ParentWindowIsBeingResized = FALSE;
volatile RECT	ParentWindowDeferredResizeRect = { -1,-1,-1,-1 };
CRITICAL_SECTION ParentWindowCritSec;

LRESULT CALLBACK ParentWinMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CREATE) {
		return(0);
	}
	else if (msg == WM_PAINT) {
		PAINTSTRUCT ps;
		HDC hdc;

		hdc = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);

		return(0);
	}
	else if (msg == WM_ACTIVATEAPP) {
		if (wParam)
			SetFocus(SDL_Window);

		SendMessage(SDL_Window, msg, wParam, lParam);
		return(0);
	}
	else if (msg == WM_ACTIVATE) {
		if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
			SetFocus(SDL_Window);

		SendMessage(SDL_Window, msg, wParam, lParam);
		return(0);
	}
	else if (msg == WM_SIZE) {
		SetWindowPos(SDL_Window, HWND_TOP, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOACTIVATE);
		return(0);
	}
	else if (msg == WM_SYSCOMMAND) {
		/* CAREFUL! We only want to forward custom system menu items. Anything standard to Windows must be handled ourselves. */
		if (wParam < 0xF000) {
			PostMessage(SDL_Window, WM_SYSCOMMAND, wParam, lParam);
			return(0);
		}
		else if ((wParam & 0xFFF0) == SC_SIZE) {
			LRESULT r;
			RECT nr;

			/* Windows 10 has recently developed a problem where calling SetWindowPos() from the main thread
			   on this window while the user is resizing the window eventually results in a deadlock where
			   the user is unable to move or resize the window anymore.

			   To avoid this, set a flag and run DefWindowProc() here so that SDL_SetVideoMode() will know
			   NOT to use SetWindowPos() */
			EnterCriticalSection(&ParentWindowCritSec);
			ParentWindowIsBeingResized = TRUE;
			LeaveCriticalSection(&ParentWindowCritSec);

			r = DefWindowProc(hwnd, msg, wParam, lParam);

            EnterCriticalSection(&ParentWindowCritSec);

            nr = ParentWindowDeferredResizeRect;
			ParentWindowDeferredResizeRect.top = -1;
			ParentWindowDeferredResizeRect.left = -1;
			ParentWindowDeferredResizeRect.right = -1;
			ParentWindowDeferredResizeRect.bottom = -1;
            ParentWindowIsBeingResized = FALSE;

            LeaveCriticalSection(&ParentWindowCritSec);

			/* SDL_dibvideo.c gave us a deferred window position/size to apply after resize.
               PROBLEM: The rect is calculated with AdjustWindowRectEx() which computes the menu height
                        as if the menu is normal and does not take into consideration cases where the menu
                        bar has doubled or tripled due to wrapping. To avoid the window jumping and snapping
                        erratically, don't set the new size unless it's BIGGER than the current window size.

                        We can't use GetMenuBarInfo() here since that appeared only in Windows Vista or higher
                        and this code is intended to work as low as Windows XP, or the DOS HX extender. */
            if (nr.right > 0 && nr.bottom > 0) {
                RECT r;

                GetWindowRect(ParentWindowHWND, &r);

                if ((nr.right - nr.left) > (r.right - r.left) || (nr.bottom - nr.top) > (r.bottom - r.top)) {
                    SetWindowPos(ParentWindowHWND, NULL,
                        nr.left, nr.top, nr.right - nr.left, nr.bottom - nr.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
                }
            }

 			return r;
		}
		else {
			/* fall through, to DefWindowProc() */
		}
	}
	else if (msg == WM_COMMAND) {
		PostMessage(SDL_Window, msg, wParam, lParam);
		return(0);
	}
	else if (msg == WM_WINDOWPOSCHANGING) {
		WINDOWPOS *windowpos = (WINDOWPOS*)lParam;

		/* FIXME: Why do MinGW builds crash here on thread shutdown, as if SDL_PublicSurface never existed? */
		if (ParentWindowShutdown) return(0);

		/* When menu is at the side or top, Windows likes
		to try to reposition the fullscreen window when
		changing video modes.
		*/
		if (!SDL_resizing && SDL_PublicSurface) {
			if (SDL_PublicSurface->flags & SDL_FULLSCREEN) {
				windowpos->x = 0;
				windowpos->y = 0;
			}
		}

		return(0);
	}
	else if (msg == WM_WINDOWPOSCHANGED) {
		/* Before we forward to the child, we need to get the new dimensions, resize the child,
		   and THEN forward it to the child. Note that this forwarding is required if SDL is
		   to keep the window position intact when resizing the DIB window. */
		{
			RECT rc;

			GetClientRect(hwnd, &rc);
			SetWindowPos(SDL_Window, HWND_TOP, 0, 0, rc.right, rc.bottom, SWP_NOACTIVATE);
		}

		return SendMessage(SDL_Window, msg, wParam, lParam);
	}
	else if (msg == WM_INITMENU) {
		if (SDL1_hax_INITMENU_cb != NULL)
			SDL1_hax_INITMENU_cb();

		/* fall through */
	}
	else if (msg == WM_CLOSE) {
		return SendMessage(SDL_Window, msg, wParam, lParam);
	}
	else if (msg == WM_DESTROY) {
		DestroyWindow(SDL_Window);
		PostQuitMessage(0);
	}

	return(DefWindowProc(hwnd, msg, wParam, lParam));
}

extern LPSTR SDL_AppnameParent;

unsigned int __stdcall ParentWindowThreadProc(void *arg) {
	MSG msg;

	if (!ParentWindowInit) return 1;

	/* main thread already registered our wndclass */
	ParentWindowHWND = CreateWindow(SDL_AppnameParent, SDL_Appname,
		(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, SDL_Instance, NULL);
	if (ParentWindowHWND == NULL) {
		ParentWindowInit = 0;
		return 1;
	}

	ParentWindowReady = 1;
	ParentWindowInit = 0;

	while (!ParentWindowShutdown && GetMessage(&msg, ParentWindowHWND, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyWindow(ParentWindowHWND);
	ParentWindowReady = 0;
	ParentWindowInit = 0;

	return 0;
}

int ParentWindowThreadRunning(void) {
	return (ParentWindowThread != INVALID_HANDLE_VALUE);
}

void ParentWindowThreadCheck(void) {
	if (ParentWindowThread != INVALID_HANDLE_VALUE) {
		if (WaitForSingleObject(ParentWindowThread, 0) == WAIT_OBJECT_0) {
			/* thread just died */
			ParentWindowThread = INVALID_HANDLE_VALUE;
			ParentWindowThreadID = 0;
		}
	}
}

void StopParentWindow(void) {
	if (ParentWindowThreadRunning()) {
		ParentWindowShutdown = 1;

		PostMessage(ParentWindowHWND, WM_USER, 0, 0); // make the pump respond
		while (ParentWindowThreadRunning()) {
			Sleep(100);
			ParentWindowThreadCheck();
		}

		DeleteCriticalSection(&ParentWindowCritSec);
	}
}

int InitParentWindow(void) {
	if (ParentWindowThread == INVALID_HANDLE_VALUE) {
		unsigned int patience;

		InitializeCriticalSection(&ParentWindowCritSec);

		ParentWindowShutdown = 0;
		ParentWindowReady = 0;
		ParentWindowInit = 1;

		ParentWindowThread = (HANDLE)_beginthreadex(NULL,0,ParentWindowThreadProc,NULL,0,&ParentWindowThreadID);
		if (ParentWindowThread == NULL) {
			ParentWindowThread = INVALID_HANDLE_VALUE;
			return 0;
		}

		patience = 50;
		while (ParentWindowInit && !ParentWindowReady) {
			ParentWindowThreadCheck();
			if (!ParentWindowThreadRunning()) {
				/* it ended early?? */
				ParentWindowShutdown = 0;
				ParentWindowReady = 0;
				ParentWindowInit = 0;
				return 0;
			}

			if (--patience == 0)
				break;
			else
				Sleep(100);
		}

		if (!ParentWindowReady) {
			ParentWindowShutdown = 1;
			Sleep(1000); // shutdown now. you have one second.
			ParentWindowThreadCheck();

			if (ParentWindowThreadRunning()) {
				/* hasn't terminated. kill it. */
				/* this is brutal and may cause memory leaks in the Windows API, but it must be done */
				TerminateThread(ParentWindowThread, 1);
				do {
					ParentWindowThreadCheck();
				} while (ParentWindowThreadRunning());
			}

			ParentWindowShutdown = 0;
			ParentWindowReady = 0;
			ParentWindowInit = 0;
			return 0;
		}
	}

	return 1;
}
/*-----------------------------------------------------------*/
#endif

int DIB_CreateWindow(_THIS)
{
	char *windowid;

	SDL_RegisterApp(NULL, 0, 0);

	windowid = SDL_getenv("SDL_WINDOWID");
	SDL_windowid = (windowid != NULL);
	if ( SDL_windowid ) {
#if defined(_WIN32_WCE) && (_WIN32_WCE < 300)
		/* wince 2.1 does not have strtol */
		wchar_t *windowid_t = SDL_malloc((SDL_strlen(windowid) + 1) * sizeof(wchar_t));
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, windowid, -1, windowid_t, SDL_strlen(windowid) + 1);
		SDL_Window = (HWND)wcstol(windowid_t, NULL, 0);
		SDL_free(windowid_t);
#else
		SDL_Window = (HWND)((size_t)SDL_strtoull(windowid, NULL, 0));
#endif
		if ( SDL_Window == NULL ) {
			SDL_SetError("Couldn't get user specified window");
			return(-1);
		}

		/* DJM: we want all event's for the user specified
			window to be handled by SDL.
		 */
		userWindowProc = (WNDPROCTYPE)GetWindowLongPtr(SDL_Window, GWLP_WNDPROC);
		SetWindowLongPtr(SDL_Window, GWLP_WNDPROC, (LONG_PTR)WinMessage);
	} else {
#ifndef SDL_WIN32_NO_PARENT_WINDOW
		if (!InitParentWindow()) {
			SDL_SetError("Couldn't init parent window");
			return(-1);
		}
#endif

#ifdef SDL_WIN32_NO_PARENT_WINDOW
# ifdef SDL_WIN32_HX_DOS
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname,
			WS_POPUP, /* NTS: WS_OVERLAPPED implies WS_CAPTION */
			0, 0, 640, 480, NULL, NULL, SDL_Instance, NULL);
# else
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname,
			(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
			0, 0, 640, 480, NULL, NULL, SDL_Instance, NULL);
# endif
#else
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname,
                        WS_CHILD,
                        0, 0, 640, 480, ParentWindowHWND, NULL, SDL_Instance, NULL);
#endif
		if ( SDL_Window == NULL ) {
			SDL_SetError("Couldn't create window");
			return(-1);
		}

		SetFocus(SDL_Window);
		ShowWindow(SDL_Window, SW_HIDE);
	}

	/* JC 14 Mar 2006
		Flush the message loop or this can cause big problems later
		Especially if the user decides to use dialog boxes or assert()!
	*/
	WIN_FlushMessageQueue();

	return(0);
}

void DIB_DestroyWindow(_THIS)
{
	if ( SDL_windowid ) {
		SetWindowLongPtr(SDL_Window, GWLP_WNDPROC, (LONG_PTR)userWindowProc);
	} else {
		DestroyWindow(SDL_Window);
	}

// NTS: DOSBox-X likes to call SQL_Quit/SQL_QuitSubSystem just to reinit the window.
//      It's better if the parent window doesn't disappear and reappear.
//	StopParentWindow();

	SDL_UnregisterApp();

	/* JC 14 Mar 2006
		Flush the message loop or this can cause big problems later
		Especially if the user decides to use dialog boxes or assert()!
	*/
	WIN_FlushMessageQueue();
}
