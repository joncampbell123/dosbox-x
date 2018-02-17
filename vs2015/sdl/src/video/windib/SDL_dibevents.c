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

#include "SDL_main.h"
#include "SDL_events.h"
#include "SDL_syswm.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../wincommon/SDL_lowvideo.h"
#include "SDL_gapidibvideo.h"
#include "SDL_vkeys.h"

#ifdef SDL_VIDEO_DRIVER_GAPI
#include "../gapi/SDL_gapivideo.h"
#endif

#ifdef SDL_VIDEO_DRIVER_WINDIB
#include "SDL_dibvideo.h"
#endif

#include <dinput.h> // for the DIK_ defines

#ifndef WM_APP
#define WM_APP	0x8000
#endif

#ifdef _WIN32_WCE
#define NO_GETKEYBOARDSTATE
#endif

/* The translation table from Windows scan codes to a SDL keysym */
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
			/* Ignore repeated keys */
			if ( lParam&REPEATED_KEYMASK ) {
				return(0);
			}
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
						wParam = VK_LSHIFT;
					} else if (state[SDLK_RSHIFT] == SDL_RELEASED && (GetKeyState(VK_RSHIFT) & 0x8000)) {
						wParam = VK_RSHIFT;
					} else {
						/* Win9x */
						int sc = HIWORD(lParam) & 0xFF;

						if (sc == 0x2A)
							wParam = VK_LSHIFT;
						else
						if (sc == 0x36)
							wParam = VK_RSHIFT;
						else
							wParam = VK_LSHIFT;
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
						wParam = VK_LSHIFT;
					} else if (state[SDLK_RSHIFT] == SDL_PRESSED && !(GetKeyState(VK_RSHIFT) & 0x8000)) {
						wParam = VK_RSHIFT;
					} else {
						/* Win9x */
						int sc = HIWORD(lParam) & 0xFF;

						if (sc == 0x2A)
							wParam = VK_LSHIFT;
						else
						if (sc == 0x36)
							wParam = VK_RSHIFT;
						else
							wParam = VK_LSHIFT;
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

void DIB_InitOSKeymap(_THIS)
{
	int	i;

	/* Map the scan codes to SDL keysyms */
	for ( i=0; i<SDL_arraysize(VK_keymap); ++i )
		VK_keymap[i] = SDLK_UNKNOWN;

	VK_keymap[DIK_ESCAPE] = SDLK_ESCAPE;
	VK_keymap[DIK_1] = SDLK_1;
	VK_keymap[DIK_2] = SDLK_2;
	VK_keymap[DIK_3] = SDLK_3;
	VK_keymap[DIK_4] = SDLK_4;
	VK_keymap[DIK_5] = SDLK_5;
	VK_keymap[DIK_6] = SDLK_6;
	VK_keymap[DIK_7] = SDLK_7;
	VK_keymap[DIK_8] = SDLK_8;
	VK_keymap[DIK_9] = SDLK_9;
	VK_keymap[DIK_0] = SDLK_0;
	VK_keymap[DIK_MINUS] = SDLK_MINUS;
	VK_keymap[DIK_EQUALS] = SDLK_EQUALS;
	VK_keymap[DIK_BACK] = SDLK_BACKSPACE;
	VK_keymap[DIK_TAB] = SDLK_TAB;
	VK_keymap[DIK_Q] = SDLK_q;
	VK_keymap[DIK_W] = SDLK_w;
	VK_keymap[DIK_E] = SDLK_e;
	VK_keymap[DIK_R] = SDLK_r;
	VK_keymap[DIK_T] = SDLK_t;
	VK_keymap[DIK_Y] = SDLK_y;
	VK_keymap[DIK_U] = SDLK_u;
	VK_keymap[DIK_I] = SDLK_i;
	VK_keymap[DIK_O] = SDLK_o;
	VK_keymap[DIK_P] = SDLK_p;
	VK_keymap[DIK_LBRACKET] = SDLK_LEFTBRACKET;
	VK_keymap[DIK_RBRACKET] = SDLK_RIGHTBRACKET;
	VK_keymap[DIK_RETURN] = SDLK_RETURN;
	VK_keymap[DIK_LCONTROL] = SDLK_LCTRL;
	VK_keymap[DIK_A] = SDLK_a;
	VK_keymap[DIK_S] = SDLK_s;
	VK_keymap[DIK_D] = SDLK_d;
	VK_keymap[DIK_F] = SDLK_f;
	VK_keymap[DIK_G] = SDLK_g;
	VK_keymap[DIK_H] = SDLK_h;
	VK_keymap[DIK_J] = SDLK_j;
	VK_keymap[DIK_K] = SDLK_k;
	VK_keymap[DIK_L] = SDLK_l;
	VK_keymap[DIK_SEMICOLON] = SDLK_SEMICOLON;
	VK_keymap[DIK_APOSTROPHE] = SDLK_QUOTE;
	VK_keymap[DIK_GRAVE] = SDLK_BACKQUOTE;
	VK_keymap[DIK_LSHIFT] = SDLK_LSHIFT;
	VK_keymap[DIK_BACKSLASH] = SDLK_BACKSLASH;
	VK_keymap[DIK_OEM_102] = SDLK_LESS;
	VK_keymap[DIK_Z] = SDLK_z;
	VK_keymap[DIK_X] = SDLK_x;
	VK_keymap[DIK_C] = SDLK_c;
	VK_keymap[DIK_V] = SDLK_v;
	VK_keymap[DIK_B] = SDLK_b;
	VK_keymap[DIK_N] = SDLK_n;
	VK_keymap[DIK_M] = SDLK_m;
	VK_keymap[DIK_COMMA] = SDLK_COMMA;
	VK_keymap[DIK_PERIOD] = SDLK_PERIOD;
	VK_keymap[DIK_SLASH] = SDLK_SLASH;
	VK_keymap[DIK_RSHIFT] = SDLK_RSHIFT;
	VK_keymap[DIK_MULTIPLY] = SDLK_KP_MULTIPLY;
	VK_keymap[DIK_LMENU] = SDLK_LALT;
	VK_keymap[DIK_SPACE] = SDLK_SPACE;
	VK_keymap[DIK_CAPITAL] = SDLK_CAPSLOCK;
	VK_keymap[DIK_F1] = SDLK_F1;
	VK_keymap[DIK_F2] = SDLK_F2;
	VK_keymap[DIK_F3] = SDLK_F3;
	VK_keymap[DIK_F4] = SDLK_F4;
	VK_keymap[DIK_F5] = SDLK_F5;
	VK_keymap[DIK_F6] = SDLK_F6;
	VK_keymap[DIK_F7] = SDLK_F7;
	VK_keymap[DIK_F8] = SDLK_F8;
	VK_keymap[DIK_F9] = SDLK_F9;
	VK_keymap[DIK_F10] = SDLK_F10;
	VK_keymap[DIK_NUMLOCK^0x80] = SDLK_NUMLOCK; // for some reason this is swapped
	VK_keymap[DIK_SCROLL] = SDLK_SCROLLOCK;
	VK_keymap[DIK_NUMPAD7] = SDLK_KP7;
	VK_keymap[DIK_NUMPAD8] = SDLK_KP8;
	VK_keymap[DIK_NUMPAD9] = SDLK_KP9;
	VK_keymap[DIK_SUBTRACT] = SDLK_KP_MINUS;
	VK_keymap[DIK_NUMPAD4] = SDLK_KP4;
	VK_keymap[DIK_NUMPAD5] = SDLK_KP5;
	VK_keymap[DIK_NUMPAD6] = SDLK_KP6;
	VK_keymap[DIK_ADD] = SDLK_KP_PLUS;
	VK_keymap[DIK_NUMPAD1] = SDLK_KP1;
	VK_keymap[DIK_NUMPAD2] = SDLK_KP2;
	VK_keymap[DIK_NUMPAD3] = SDLK_KP3;
	VK_keymap[DIK_NUMPAD0] = SDLK_KP0;
	VK_keymap[DIK_DECIMAL] = SDLK_KP_PERIOD;
	VK_keymap[DIK_F11] = SDLK_F11;
	VK_keymap[DIK_F12] = SDLK_F12;

	VK_keymap[DIK_F13] = SDLK_F13;
	VK_keymap[DIK_F14] = SDLK_F14;
	VK_keymap[DIK_F15] = SDLK_F15;

	VK_keymap[DIK_NUMPADEQUALS] = SDLK_KP_EQUALS;
	VK_keymap[DIK_NUMPADENTER] = SDLK_KP_ENTER;
	VK_keymap[DIK_RCONTROL] = SDLK_RCTRL;
	VK_keymap[DIK_DIVIDE] = SDLK_KP_DIVIDE;
	VK_keymap[DIK_SYSRQ] = SDLK_PRINT;
	VK_keymap[DIK_RMENU] = SDLK_RALT;
	VK_keymap[DIK_PAUSE^0x80] = SDLK_PAUSE; // for some reason this is swapped
	VK_keymap[DIK_HOME] = SDLK_HOME;
	VK_keymap[DIK_UP] = SDLK_UP;
	VK_keymap[DIK_PRIOR] = SDLK_PAGEUP;
	VK_keymap[DIK_LEFT] = SDLK_LEFT;
	VK_keymap[DIK_RIGHT] = SDLK_RIGHT;
	VK_keymap[DIK_END] = SDLK_END;
	VK_keymap[DIK_DOWN] = SDLK_DOWN;
	VK_keymap[DIK_NEXT] = SDLK_PAGEDOWN;
	VK_keymap[DIK_INSERT] = SDLK_INSERT;
	VK_keymap[DIK_DELETE] = SDLK_DELETE;
	VK_keymap[DIK_LWIN] = SDLK_LMETA;
	VK_keymap[DIK_RWIN] = SDLK_RMETA;
	VK_keymap[DIK_APPS] = SDLK_MENU;

	Arrows_keymap[3] = 0x25;
	Arrows_keymap[2] = 0x26;
	Arrows_keymap[1] = 0x27;
	Arrows_keymap[0] = 0x28;
}

static BYTE ConvertWordScanCodeToByteScanCode(UINT scancode)
{
	BYTE scan = scancode & 0xFF;
	if (scancode & 0x100) {
		scan |= 0x80;
	} else {
		scan &= ~0x80;
	}
	return scan;
}

static SDL_keysym *TranslateKey(WPARAM vkey, UINT scancode, SDL_keysym *keysym, int pressed)
{
	/* Set the keysym information */
	keysym->scancode = (unsigned char) scancode;
	keysym->mod = KMOD_NONE;
	keysym->unicode = 0;
	
	if ((vkey == VK_RETURN) && (scancode & 0x100)) {
		/* No VK_ code for the keypad enter key */
		keysym->sym = SDLK_KP_ENTER;
	}
	else {
		keysym->sym = VK_keymap[ConvertWordScanCodeToByteScanCode(scancode)];
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
			keysym->unicode = vkey - VK_NUMPAD0 + '0';
		}
		else if (SDL_ToUnicode((UINT)vkey, scancode, keystate, wchars, sizeof(wchars)/sizeof(wchars[0]), 0) > 0)
		{
			keysym->unicode = wchars[0];
		}
#endif /* NO_GETKEYBOARDSTATE */
	}

#if 0
	printf("SYM:%d, VK:0x%02X, SC:0x%04X\n", keysym->sym, vkey, scancode);
#endif

#if 0
	{
		HKL     hLayoutCurrent = GetKeyboardLayout(0);
		int     sc = scancode & 0xFF;

		printf("SYM:%d, VK:0x%02X, SC:0x%04X, US:(1:0x%02X, 3:0x%02X), "
			"Current:(1:0x%02X, 3:0x%02X)\n",
			keysym->sym, vkey, scancode,
			MapVirtualKeyEx(sc, 1, hLayoutUS),
			MapVirtualKeyEx(sc, 3, hLayoutUS),
			MapVirtualKeyEx(sc, 1, hLayoutCurrent),
			MapVirtualKeyEx(sc, 3, hLayoutCurrent)
		);
	}
#endif
	return(keysym);
}

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
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname,
                        (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX),
                        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, SDL_Instance, NULL);
		if ( SDL_Window == NULL ) {
			SDL_SetError("Couldn't create window");
			return(-1);
		}
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
	SDL_UnregisterApp();

	/* JC 14 Mar 2006
		Flush the message loop or this can cause big problems later
		Especially if the user decides to use dialog boxes or assert()!
	*/
	WIN_FlushMessageQueue();
}
