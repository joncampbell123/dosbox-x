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

/* DOSBox-X: Wayland input for SDL 1.2 */

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_cursor_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandmouse_c.h"
#include "SDL_waylanddecor.h"

/* from linux/input-event-codes.h, which is not available everywhere Wayland is */
#define WL_BTN_LEFT	0x110
#define WL_BTN_RIGHT	0x111
#define WL_BTN_MIDDLE	0x112
#define WL_BTN_SIDE	0x113
#define WL_BTN_EXTRA	0x114
#define WL_KEY_RO	89
#define WL_KEY_YEN	124

/* evdev key codes are X11 key codes - 8 */
#define WL_XKB_KEYCODE(key)	((key) + 8)

/* one mouse wheel notch is 10 units of axis motion */
#define WL_WHEEL_STEP	10.0

static SDLKey ODD_keymap[256];
static SDLKey MISC_keymap[256];

/* keyboard translation, the same as the X11 driver (xkbcommon keysyms have
   the values of X11 keysyms) */
void Wayland_InitOSKeymap(_THIS)
{
	int i;

	/* Odd keys used in international keyboards */
	for ( i=0; i<SDL_arraysize(ODD_keymap); ++i )
		ODD_keymap[i] = SDLK_UNKNOWN;

	/* Map the miscellaneous keys */
	for ( i=0; i<SDL_arraysize(MISC_keymap); ++i )
		MISC_keymap[i] = SDLK_UNKNOWN;

	ODD_keymap[XKB_KEY_dead_grave&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_acute&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_tilde&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_macron&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_breve&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_abovedot&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_diaeresis&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_abovering&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_doubleacute&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_caron&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_cedilla&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_ogonek&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_iota&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_voiced_sound&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_semivoiced_sound&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_belowdot&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_hook&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_horn&0xFF] = SDLK_COMPOSE;
	ODD_keymap[XKB_KEY_dead_circumflex&0xFF] = SDLK_CARET;
	ODD_keymap[XKB_KEY_ISO_Level3_Shift&0xFF] = SDLK_MODE; /* "Alt Gr" key */
	MISC_keymap[XKB_KEY_BackSpace&0xFF] = SDLK_BACKSPACE;
	MISC_keymap[XKB_KEY_Tab&0xFF] = SDLK_TAB;
	MISC_keymap[XKB_KEY_Clear&0xFF] = SDLK_CLEAR;
	MISC_keymap[XKB_KEY_Return&0xFF] = SDLK_RETURN;
	MISC_keymap[XKB_KEY_Pause&0xFF] = SDLK_PAUSE;
	MISC_keymap[XKB_KEY_Escape&0xFF] = SDLK_ESCAPE;
	MISC_keymap[XKB_KEY_Delete&0xFF] = SDLK_DELETE;
	MISC_keymap[XKB_KEY_KP_0&0xFF] = SDLK_KP0;		/* Keypad 0-9 */
	MISC_keymap[XKB_KEY_KP_1&0xFF] = SDLK_KP1;
	MISC_keymap[XKB_KEY_KP_2&0xFF] = SDLK_KP2;
	MISC_keymap[XKB_KEY_KP_3&0xFF] = SDLK_KP3;
	MISC_keymap[XKB_KEY_KP_4&0xFF] = SDLK_KP4;
	MISC_keymap[XKB_KEY_KP_5&0xFF] = SDLK_KP5;
	MISC_keymap[XKB_KEY_KP_6&0xFF] = SDLK_KP6;
	MISC_keymap[XKB_KEY_KP_7&0xFF] = SDLK_KP7;
	MISC_keymap[XKB_KEY_KP_8&0xFF] = SDLK_KP8;
	MISC_keymap[XKB_KEY_KP_9&0xFF] = SDLK_KP9;
	MISC_keymap[XKB_KEY_KP_Insert&0xFF] = SDLK_KP0;
	MISC_keymap[XKB_KEY_KP_End&0xFF] = SDLK_KP1;	
	MISC_keymap[XKB_KEY_KP_Down&0xFF] = SDLK_KP2;
	MISC_keymap[XKB_KEY_KP_Page_Down&0xFF] = SDLK_KP3;
	MISC_keymap[XKB_KEY_KP_Left&0xFF] = SDLK_KP4;
	MISC_keymap[XKB_KEY_KP_Begin&0xFF] = SDLK_KP5;
	MISC_keymap[XKB_KEY_KP_Right&0xFF] = SDLK_KP6;
	MISC_keymap[XKB_KEY_KP_Home&0xFF] = SDLK_KP7;
	MISC_keymap[XKB_KEY_KP_Up&0xFF] = SDLK_KP8;
	MISC_keymap[XKB_KEY_KP_Page_Up&0xFF] = SDLK_KP9;
	MISC_keymap[XKB_KEY_KP_Delete&0xFF] = SDLK_KP_PERIOD;
	MISC_keymap[XKB_KEY_KP_Decimal&0xFF] = SDLK_KP_PERIOD;
	MISC_keymap[XKB_KEY_KP_Divide&0xFF] = SDLK_KP_DIVIDE;
	MISC_keymap[XKB_KEY_KP_Multiply&0xFF] = SDLK_KP_MULTIPLY;
	MISC_keymap[XKB_KEY_KP_Subtract&0xFF] = SDLK_KP_MINUS;
	MISC_keymap[XKB_KEY_KP_Add&0xFF] = SDLK_KP_PLUS;
	MISC_keymap[XKB_KEY_KP_Enter&0xFF] = SDLK_KP_ENTER;
	MISC_keymap[XKB_KEY_KP_Equal&0xFF] = SDLK_KP_EQUALS;
	MISC_keymap[XKB_KEY_Up&0xFF] = SDLK_UP;
	MISC_keymap[XKB_KEY_Down&0xFF] = SDLK_DOWN;
	MISC_keymap[XKB_KEY_Right&0xFF] = SDLK_RIGHT;
	MISC_keymap[XKB_KEY_Left&0xFF] = SDLK_LEFT;
	MISC_keymap[XKB_KEY_Insert&0xFF] = SDLK_INSERT;
	MISC_keymap[XKB_KEY_Home&0xFF] = SDLK_HOME;
	MISC_keymap[XKB_KEY_End&0xFF] = SDLK_END;
	MISC_keymap[XKB_KEY_Page_Up&0xFF] = SDLK_PAGEUP;
	MISC_keymap[XKB_KEY_Page_Down&0xFF] = SDLK_PAGEDOWN;
	MISC_keymap[XKB_KEY_F1&0xFF] = SDLK_F1;
	MISC_keymap[XKB_KEY_F2&0xFF] = SDLK_F2;
	MISC_keymap[XKB_KEY_F3&0xFF] = SDLK_F3;
	MISC_keymap[XKB_KEY_F4&0xFF] = SDLK_F4;
	MISC_keymap[XKB_KEY_F5&0xFF] = SDLK_F5;
	MISC_keymap[XKB_KEY_F6&0xFF] = SDLK_F6;
	MISC_keymap[XKB_KEY_F7&0xFF] = SDLK_F7;
	MISC_keymap[XKB_KEY_F8&0xFF] = SDLK_F8;
	MISC_keymap[XKB_KEY_F9&0xFF] = SDLK_F9;
	MISC_keymap[XKB_KEY_F10&0xFF] = SDLK_F10;
	MISC_keymap[XKB_KEY_F11&0xFF] = SDLK_F11;
	MISC_keymap[XKB_KEY_F12&0xFF] = SDLK_F12;
	MISC_keymap[XKB_KEY_F13&0xFF] = SDLK_F13;
	MISC_keymap[XKB_KEY_F14&0xFF] = SDLK_F14;
	MISC_keymap[XKB_KEY_F15&0xFF] = SDLK_F15;
	MISC_keymap[XKB_KEY_Num_Lock&0xFF] = SDLK_NUMLOCK;
	MISC_keymap[XKB_KEY_Caps_Lock&0xFF] = SDLK_CAPSLOCK;
	MISC_keymap[XKB_KEY_Scroll_Lock&0xFF] = SDLK_SCROLLOCK;
	MISC_keymap[XKB_KEY_Shift_R&0xFF] = SDLK_RSHIFT;
	MISC_keymap[XKB_KEY_Shift_L&0xFF] = SDLK_LSHIFT;
	MISC_keymap[XKB_KEY_Control_R&0xFF] = SDLK_RCTRL;
	MISC_keymap[XKB_KEY_Control_L&0xFF] = SDLK_LCTRL;
	MISC_keymap[XKB_KEY_Alt_R&0xFF] = SDLK_RALT;
	MISC_keymap[XKB_KEY_Alt_L&0xFF] = SDLK_LALT;
	MISC_keymap[XKB_KEY_Meta_R&0xFF] = SDLK_RMETA;
	MISC_keymap[XKB_KEY_Meta_L&0xFF] = SDLK_LMETA;
	MISC_keymap[XKB_KEY_Super_L&0xFF] = SDLK_LSUPER; /* Left "Windows" */
	MISC_keymap[XKB_KEY_Super_R&0xFF] = SDLK_RSUPER; /* Right "Windows */
	MISC_keymap[XKB_KEY_Mode_switch&0xFF] = SDLK_MODE; /* "Alt Gr" key */
	MISC_keymap[XKB_KEY_Multi_key&0xFF] = SDLK_COMPOSE; /* Multi-key compose */
	MISC_keymap[XKB_KEY_Help&0xFF] = SDLK_HELP;
	MISC_keymap[XKB_KEY_Print&0xFF] = SDLK_PRINT;
	MISC_keymap[XKB_KEY_Sys_Req&0xFF] = SDLK_SYSREQ;
	MISC_keymap[XKB_KEY_Break&0xFF] = SDLK_BREAK;
	MISC_keymap[XKB_KEY_Menu&0xFF] = SDLK_MENU;
	MISC_keymap[XKB_KEY_Hyper_R&0xFF] = SDLK_MENU;   /* Windows "Menu" key */
}

static SDLKey Wayland_TranslateKey(_THIS, Uint32 key)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	const xkb_keysym_t *syms = NULL;
	xkb_keysym_t sym = XKB_KEY_NoSymbol;
	xkb_keycode_t keycode = WL_XKB_KEYCODE(key);
	SDLKey sdlkey = SDLK_UNKNOWN;

	if ( wl->xkb_keymap == NULL || wl->xkb_state == NULL ) {
		return SDLK_UNKNOWN;
	}

	/* the unshifted symbol of the current layout, like the X11 driver */
	if ( xkb_keymap_key_get_syms_by_level(wl->xkb_keymap, keycode,
	         xkb_state_key_get_layout(wl->xkb_state, keycode), 0, &syms) > 0 ) {
		sym = syms[0];
	}

	/* the JP Ro and Yen keys are both backslash */
	if ( sym == XKB_KEY_backslash ) {
		if ( key == WL_KEY_RO ) return SDLK_JP_RO;
		if ( key == WL_KEY_YEN ) return SDLK_JP_YEN;
	}

	switch (sym>>8) {
	    case 0x00:	/* Latin 1 */
		sdlkey = (SDLKey)(sym & 0xFF);
		break;
	    case 0x01:	/* Latin 2 */
	    case 0x02:	/* Latin 3 */
	    case 0x03:	/* Latin 4 */
	    case 0x04:	/* Katakana */
	    case 0x05:	/* Arabic */
	    case 0x06:	/* Cyrillic */
	    case 0x07:	/* Greek */
	    case 0x08:	/* Technical */
	    case 0x0A:	/* Publishing */
	    case 0x0C:	/* Hebrew */
	    case 0x0D:	/* Thai */
		/* These are wrong, but it's better than nothing */
		sdlkey = (SDLKey)(sym & 0xFF);
		break;
	    case 0xFE:
		sdlkey = ODD_keymap[sym&0xFF];
		break;
	    case 0xFF:
		sdlkey = MISC_keymap[sym&0xFF];
		break;
	    default:
		break;
	}
	return sdlkey;
}

/* the lock keys are reported by the compositor, SDL only sees their key presses */
static void Wayland_SyncLockModifiers(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	SDLMod mod = SDL_GetModState() & ~(KMOD_NUM|KMOD_CAPS);

	if ( wl->xkb_state == NULL ) {
		return;
	}
	if ( xkb_state_mod_name_is_active(wl->xkb_state, XKB_MOD_NAME_NUM, XKB_STATE_MODS_EFFECTIVE) > 0 ) {
		mod |= KMOD_NUM;
	}
	if ( xkb_state_mod_name_is_active(wl->xkb_state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE) > 0 ) {
		mod |= KMOD_CAPS;
	}
	SDL_SetModState(mod);
}

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;
	char *map;

	if ( format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 ) {
		close(fd);
		return;
	}
	map = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if ( map == MAP_FAILED ) {
		return;
	}

	if ( wl->xkb_state ) {
		xkb_state_unref(wl->xkb_state);
		wl->xkb_state = NULL;
	}
	if ( wl->xkb_keymap ) {
		xkb_keymap_unref(wl->xkb_keymap);
		wl->xkb_keymap = NULL;
	}
	wl->xkb_keymap = xkb_keymap_new_from_string(wl->xkb_context, map,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size);
	if ( wl->xkb_keymap ) {
		wl->xkb_state = xkb_state_new(wl->xkb_keymap);
	}
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
	struct wl_surface *surface, struct wl_array *keys)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;

	Wayland_SyncLockModifiers(this);
	SDL_PrivateAppActive(1, SDL_APPINPUTFOCUS);
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
	struct wl_surface *surface)
{
	/* the keys being held won't be released here */
	SDL_ResetKeyboard();
	SDL_PrivateAppActive(0, SDL_APPINPUTFOCUS);
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial,
	uint32_t time, uint32_t key, uint32_t state)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;
	SDL_keysym keysym;
	int pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

	SDL_memset(&keysym, 0, sizeof(keysym));
	keysym.scancode = (Uint8)WL_XKB_KEYCODE(key); /* the X11 key code */
	keysym.sym = Wayland_TranslateKey(this, key);
	keysym.mod = KMOD_NONE;
	keysym.unicode = 0;
	if ( pressed && SDL_TranslateUNICODE && wl->xkb_state ) {
		Uint32 ch = xkb_state_key_get_utf32(wl->xkb_state, WL_XKB_KEYCODE(key));
		if ( ch < 0x10000 ) {
			keysym.unicode = (Uint16)ch;
		}
	}
	SDL_PrivateKeyboard(pressed ? SDL_PRESSED : SDL_RELEASED, &keysym);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial,
	uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->xkb_state ) {
		xkb_state_update_mask(wl->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
		Wayland_SyncLockModifiers(this);
	}
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
	/* SDL does key repeat itself (SDL_EnableKeyRepeat) */
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_keymap,
	keyboard_enter,
	keyboard_leave,
	keyboard_key,
	keyboard_modifiers,
	keyboard_repeat_info
};

static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( surface != wl->surface ) {
		return; /* a decoration */
	}
	wl->pointer_serial = serial;
	wl->pointer_focus = 1;
	Wayland_ApplyCursor(this);
	SDL_PrivateAppActive(1, SDL_APPMOUSEFOCUS);
	if ( !wl->relative_mode ) {
		wl->pointer_x = wl_fixed_to_int(sx);
		wl->pointer_y = wl_fixed_to_int(sy);
		SDL_PrivateMouseMotion(0, 0, (Sint16)wl->pointer_x, (Sint16)wl->pointer_y);
	}
}

static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
	struct wl_surface *surface)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( surface != wl->surface || !wl->pointer_focus ) {
		return;
	}
	wl->pointer_focus = 0;
	SDL_PrivateAppActive(0, SDL_APPMOUSEFOCUS);
}

static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
	wl_fixed_t sx, wl_fixed_t sy)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( !wl->pointer_focus || wl->relative_mode ) {
		return;
	}
	wl->pointer_x = wl_fixed_to_int(sx);
	wl->pointer_y = wl_fixed_to_int(sy);
	SDL_PrivateMouseMotion(0, 0, (Sint16)wl->pointer_x, (Sint16)wl->pointer_y);
}

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	Uint8 sdlbutton;

	if ( !this->hidden->pointer_focus ) {
		return;
	}
	switch ( button ) {
	case WL_BTN_LEFT:	sdlbutton = SDL_BUTTON_LEFT; break;
	case WL_BTN_MIDDLE:	sdlbutton = SDL_BUTTON_MIDDLE; break;
	case WL_BTN_RIGHT:	sdlbutton = SDL_BUTTON_RIGHT; break;
	case WL_BTN_SIDE:	sdlbutton = SDL_BUTTON_X1; break;
	case WL_BTN_EXTRA:	sdlbutton = SDL_BUTTON_X2; break;
	default:		return;
	}
	SDL_PrivateMouseButton(state == WL_POINTER_BUTTON_STATE_PRESSED ? SDL_PRESSED : SDL_RELEASED,
		sdlbutton, 0, 0);
}

static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
	uint32_t axis, wl_fixed_t value)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( !wl->pointer_focus || axis != WL_POINTER_AXIS_VERTICAL_SCROLL ) {
		return;
	}

	/* SDL 1.2 has wheel "buttons" */
	wl->wheel_accum += wl_fixed_to_double(value);
	while ( wl->wheel_accum >= WL_WHEEL_STEP ) {
		wl->wheel_accum -= WL_WHEEL_STEP;
		SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_WHEELDOWN, 0, 0);
		SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_WHEELDOWN, 0, 0);
	}
	while ( wl->wheel_accum <= -WL_WHEEL_STEP ) {
		wl->wheel_accum += WL_WHEEL_STEP;
		SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_WHEELUP, 0, 0);
		SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_WHEELUP, 0, 0);
	}
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t source)
{
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;

	if ( axis == WL_POINTER_AXIS_VERTICAL_SCROLL ) {
		this->hidden->wheel_accum = 0.0;
	}
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete)
{
	/* the axis event that comes with it is used instead */
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_enter,
	pointer_leave,
	pointer_motion,
	pointer_button,
	pointer_axis,
	pointer_frame,
	pointer_axis_source,
	pointer_axis_stop,
	pointer_axis_discrete
};

static void relative_pointer_motion(void *data, struct zwp_relative_pointer_v1 *relative_pointer,
	uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy,
	wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;
	int ix, iy;

	if ( !wl->relative_mode ) {
		return;
	}

	/* keep the fractions for the next motion */
	wl->relative_frac_x += wl_fixed_to_double(dx);
	wl->relative_frac_y += wl_fixed_to_double(dy);
	ix = (int)wl->relative_frac_x;
	iy = (int)wl->relative_frac_y;
	wl->relative_frac_x -= ix;
	wl->relative_frac_y -= iy;
	if ( ix || iy ) {
		SDL_PrivateMouseMotion(0, 1, (Sint16)ix, (Sint16)iy);
	}
}

static const struct zwp_relative_pointer_v1_listener relative_pointer_listener = {
	relative_pointer_motion
};

static void locked_pointer_locked(void *data, struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static void locked_pointer_unlocked(void *data, struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static const struct zwp_locked_pointer_v1_listener locked_pointer_listener = {
	locked_pointer_locked,
	locked_pointer_unlocked
};

void Wayland_ReleaseRelativeMode(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->locked_pointer ) {
		zwp_locked_pointer_v1_destroy(wl->locked_pointer);
		wl->locked_pointer = NULL;
	}
	if ( wl->relative_pointer ) {
		zwp_relative_pointer_v1_destroy(wl->relative_pointer);
		wl->relative_pointer = NULL;
	}
	wl->relative_mode = 0;
}

void Wayland_UpdateRelativeMode(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( !wl->relative_wanted ) {
		Wayland_ReleaseRelativeMode(this);
		return;
	}
	if ( wl->relative_mode ) {
		return;
	}
	if ( wl->pointer == NULL || wl->surface == NULL ||
	     wl->relative_pointer_manager == NULL || wl->pointer_constraints == NULL ) {
		return; /* not possible, the motion stays absolute */
	}

	wl->relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
		wl->relative_pointer_manager, wl->pointer);
	zwp_relative_pointer_v1_add_listener(wl->relative_pointer, &relative_pointer_listener, this);
	wl->locked_pointer = zwp_pointer_constraints_v1_lock_pointer(wl->pointer_constraints,
		wl->surface, wl->pointer, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	zwp_locked_pointer_v1_add_listener(wl->locked_pointer, &locked_pointer_listener, this);
	wl->relative_frac_x = wl->relative_frac_y = 0.0;
	wl->relative_mode = 1;
	wl_display_flush(wl->display);
}

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	SDL_VideoDevice *this = (SDL_VideoDevice *)data;
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( (caps & WL_SEAT_CAPABILITY_POINTER) && wl->pointer == NULL ) {
		wl->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(wl->pointer, &pointer_listener, this);
		Wayland_UpdateRelativeMode(this);
	} else if ( !(caps & WL_SEAT_CAPABILITY_POINTER) && wl->pointer ) {
		Wayland_ReleaseRelativeMode(this);
		wl_pointer_destroy(wl->pointer);
		wl->pointer = NULL;
		wl->pointer_focus = 0;
	}

	if ( (caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->keyboard == NULL ) {
		wl->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl->keyboard, &keyboard_listener, this);
	} else if ( !(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->keyboard ) {
		wl_keyboard_destroy(wl->keyboard);
		wl->keyboard = NULL;
	}
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities,
	seat_name
};

void Wayland_AddSeat(_THIS)
{
	wl_seat_add_listener(this->hidden->seat, &seat_listener, this);
}

void Wayland_RemoveSeat(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	Wayland_ReleaseRelativeMode(this);
	if ( wl->pointer ) {
		wl_pointer_destroy(wl->pointer);
		wl->pointer = NULL;
	}
	if ( wl->keyboard ) {
		wl_keyboard_destroy(wl->keyboard);
		wl->keyboard = NULL;
	}
	if ( wl->xkb_state ) {
		xkb_state_unref(wl->xkb_state);
		wl->xkb_state = NULL;
	}
	if ( wl->xkb_keymap ) {
		xkb_keymap_unref(wl->xkb_keymap);
		wl->xkb_keymap = NULL;
	}
	if ( wl->seat ) {
		wl_seat_destroy(wl->seat);
		wl->seat = NULL;
	}
}

int Wayland_Dispatch(_THIS, int blocking)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct pollfd pfd;

#if SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR
	/* libdecor has its own events to handle along with the display's */
	if ( wl->libdecor ) {
		return SDL_libdecor_dispatch(wl->libdecor, blocking ? -1 : 0) < 0 ? -1 : 0;
	}
#endif
	if ( blocking ) {
		return wl_display_dispatch(wl->display) < 0 ? -1 : 0;
	}

	while ( wl_display_prepare_read(wl->display) != 0 ) {
		if ( wl_display_dispatch_pending(wl->display) < 0 ) {
			return -1;
		}
	}
	if ( wl_display_flush(wl->display) < 0 && errno != EAGAIN ) {
		wl_display_cancel_read(wl->display);
		return -1;
	}
	pfd.fd = wl_display_get_fd(wl->display);
	pfd.events = POLLIN;
	pfd.revents = 0;
	if ( poll(&pfd, 1, 0) > 0 ) {
		if ( wl_display_read_events(wl->display) < 0 ) {
			return -1;
		}
	} else {
		wl_display_cancel_read(wl->display);
	}
	return wl_display_dispatch_pending(wl->display) < 0 ? -1 : 0;
}

void Wayland_PumpEvents(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( Wayland_Dispatch(this, 0) < 0 ) {
		static int reported = 0;
		if ( !reported ) {
			reported = 1;
			SDL_SetError("Lost the connection to the Wayland display");
			SDL_PrivateQuit();
		}
		return;
	}

	/* an update that was dropped because all buffers were busy */
	if ( wl->present_pending ) {
		Wayland_PresentShm(this, 0, NULL);
	}
}

/* called after the video mode is set */
void Wayland_UpdateMouse(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	if ( wl->pointer_focus ) {
		SDL_PrivateAppActive(1, SDL_APPMOUSEFOCUS);
		if ( !wl->relative_mode ) {
			SDL_PrivateMouseMotion(0, 0, (Sint16)wl->pointer_x, (Sint16)wl->pointer_y);
		}
	} else {
		SDL_PrivateAppActive(0, SDL_APPMOUSEFOCUS);
	}

	/* SDL_SetVideoMode() drops resize events, repeat the compositor's suggestion */
	if ( wl->configure_w > 0 && wl->configure_h > 0 &&
	     wl->configure_fullscreen == wl->fullscreen &&
	     (wl->configure_w != wl->window_w || wl->configure_h != wl->window_h) ) {
		SDL_PrivateResize(wl->configure_w, wl->configure_h);
	}
}
