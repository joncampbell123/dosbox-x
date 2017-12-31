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


#include <vector>
#include <list>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>


#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "keyboard.h"
#include "pic.h"
#include "control.h"
#include "joystick.h"
#include "support.h"
#include "mapper.h"
#include "setup.h"
#include "menu.h"

#include <map>

std::map<std::string,std::string> pending_string_binds;

void MAPPER_CheckKeyboardLayout();

Bitu next_handler_xpos=0;
Bitu next_handler_ypos=0;

bool mapper_addhandler_create_buttons = false;

bool isJPkeyboard = false;

enum {
	CLR_BLACK=0,
	CLR_WHITE=1,
	CLR_RED=2,
	CLR_BLUE=3,
	CLR_GREEN=4
};

enum BB_Types {
	BB_Next,BB_Add,BB_Del,
	BB_Save,BB_Exit,BB_Capture
};

enum BC_Types {
	BC_Mod1,BC_Mod2,BC_Mod3,
	BC_Hold
};

#define BMOD_Mod1 0x0001
#define BMOD_Mod2 0x0002
#define BMOD_Mod3 0x0004

#define BFLG_Hold 0x0001
#define BFLG_Repeat 0x0004


#define MAXSTICKS 8
#define MAXACTIVE 16
#define MAXBUTTON 32
#define MAXBUTTON_CAP 16
#define MAXAXIS 8
#define MAXHAT 2

class CEvent;
class CHandlerEvent;
class CButton;
class CBind;
class CBindGroup;

static void SetActiveEvent(CEvent * event);
static void SetActiveBind(CBind * _bind);
extern Bit8u int10_font_14[256 * 14];

static std::vector<CEvent *> events;
static std::vector<CButton *> buttons;
static std::vector<CBindGroup *> bindgroups;
static std::vector<CHandlerEvent *> handlergroup;
typedef std::list<CBind *> CBindList;
typedef std::list<CEvent *>::iterator CEventList_it;
typedef std::list<CBind *>::iterator CBindList_it;
typedef std::vector<CButton *>::iterator CButton_it;
typedef std::vector<CEvent *>::iterator CEventVector_it;
typedef std::vector<CHandlerEvent *>::iterator CHandlerEventVector_it;
typedef std::vector<CBindGroup *>::iterator CBindGroup_it;

static CBindList holdlist;

class CEvent {
public:
	CEvent(char const * const _entry) {
		safe_strncpy(entry,_entry,16);
		events.push_back(this);
		bindlist.clear();
		activity=0;
		current_value=0;
	}
	void AddBind(CBind * bind);
	virtual ~CEvent();
	virtual void Active(bool yesno)=0;
	virtual void ActivateEvent(bool ev_trigger,bool skip_action)=0;
	virtual void DeActivateEvent(bool ev_trigger)=0;
	void DeActivateAll(void);
	void SetValue(Bits value){
		current_value=value;
	}
	Bits GetValue(void) {
		return current_value;
	}
	char * GetName(void) { return entry; }
	virtual bool IsTrigger(void)=0;
	CBindList bindlist;
protected:
	Bitu activity;
	char entry[16];
	Bits current_value;
};

/* class for events which can be ON/OFF only: key presses, joystick buttons, joystick hat */
class CTriggeredEvent : public CEvent {
public:
	CTriggeredEvent(char const * const _entry) : CEvent(_entry) {}
	virtual ~CTriggeredEvent() {}
	virtual bool IsTrigger(void) {
		return true;
	}
	void ActivateEvent(bool ev_trigger,bool skip_action) {
		if (current_value>25000) {
			/* value exceeds boundary, trigger event if not active */
			if (!activity && !skip_action) Active(true);
			if (activity<32767) activity++;
		} else {
			if (activity>0) {
				/* untrigger event if it is fully inactive */
				DeActivateEvent(ev_trigger);
				activity=0;
			}
		}
	}
	void DeActivateEvent(bool /*ev_trigger*/) {
		activity--;
		if (!activity) Active(false);
	}
};

/* class for events which have a non-boolean state: joystick axis movement */
class CContinuousEvent : public CEvent {
public:
	CContinuousEvent(char const * const _entry) : CEvent(_entry) {}
	virtual ~CContinuousEvent() {}
	virtual bool IsTrigger(void) {
		return false;
	}
	void ActivateEvent(bool ev_trigger,bool skip_action) {
		if (ev_trigger) {
			activity++;
			if (!skip_action) Active(true);
		} else {
			/* test if no trigger-activity is present, this cares especially
			   about activity of the opposite-direction joystick axis for example */
			if (!GetActivityCount()) Active(true);
		}
	}
	void DeActivateEvent(bool ev_trigger) {
		if (ev_trigger) {
			if (activity>0) activity--;
			if (activity==0) {
				/* test if still some trigger-activity is present,
				   adjust the state in this case accordingly */
				if (GetActivityCount()) RepostActivity();
				else Active(false);
			}
		} else {
			if (!GetActivityCount()) Active(false);
		}
	}
	virtual Bitu GetActivityCount(void) {
		return activity;
	}
	virtual void RepostActivity(void) {}
};

class CBind {
public:
	virtual ~CBind () {
		list->remove(this);
	}
	CBind(CBindList * _list) {
		list=_list;
		_list->push_back(this);
		mods=flags=0;
		event=0;
		active=holding=false;
	}
	void AddFlags(char * buf) {
		if (mods & BMOD_Mod1) strcat(buf," mod1");
		if (mods & BMOD_Mod2) strcat(buf," mod2");
		if (mods & BMOD_Mod3) strcat(buf," mod3");
		if (flags & BFLG_Hold) strcat(buf," hold");
	}
	void SetFlags(char * buf) {
		char * word;
		while (*(word=StripWord(buf))) {
			if (!strcasecmp(word,"mod1")) mods|=BMOD_Mod1;
			if (!strcasecmp(word,"mod2")) mods|=BMOD_Mod2;
			if (!strcasecmp(word,"mod3")) mods|=BMOD_Mod3;
			if (!strcasecmp(word,"hold")) flags|=BFLG_Hold;
		}
	}
	void ActivateBind(Bits _value,bool ev_trigger,bool skip_action=false) {
		if (event->IsTrigger()) {
			/* use value-boundary for on/off events */
			if (_value>25000) {
				event->SetValue(_value);
				if (active) return;
				event->ActivateEvent(ev_trigger,skip_action);
				active=true;
			} else {
				if (active) {
					event->DeActivateEvent(ev_trigger);
					active=false;
				}
			}
		} else {
			/* store value for possible later use in the activated event */
			event->SetValue(_value);
			event->ActivateEvent(ev_trigger,false);
		}
	}
	void DeActivateBind(bool ev_trigger) {
		if (event->IsTrigger()) {
			if (!active) return;
			active=false;
			if (flags & BFLG_Hold) {
				if (!holding) {
					holdlist.push_back(this);
					holding=true;
					return;
				} else {
					holdlist.remove(this);
					holding=false;
				}
			}
			event->DeActivateEvent(ev_trigger);
		} else {
			/* store value for possible later use in the activated event */
			event->SetValue(0);
			event->DeActivateEvent(ev_trigger);
		}
	}
	virtual void ConfigName(char * buf)=0;
	virtual void BindName(char * buf)=0;
   
	Bitu mods,flags;
	Bit16s value;
	CEvent * event;
	CBindList * list;
	bool active,holding;
};

CEvent::~CEvent() {
	CBindList_it it;

	while ((it=bindlist.begin()) != bindlist.end()) {
		delete (*it);
		bindlist.erase(it);
	}
}

void CEvent::AddBind(CBind * bind) {
	bindlist.push_front(bind);
	bind->event=this;
}
void CEvent::DeActivateAll(void) {
	for (CBindList_it bit=bindlist.begin();bit!=bindlist.end();bit++) {
		(*bit)->DeActivateBind(true);
	}
}



class CBindGroup {
public:
	CBindGroup() {
		bindgroups.push_back(this);
	}
	virtual ~CBindGroup (void) { }
	void ActivateBindList(CBindList * list,Bits value,bool ev_trigger);
	void DeactivateBindList(CBindList * list,bool ev_trigger);
	virtual CBind * CreateConfigBind(char *&buf)=0;
	virtual CBind * CreateEventBind(SDL_Event * event)=0;

	virtual bool CheckEvent(SDL_Event * event)=0;
	virtual const char * ConfigStart(void)=0;
	virtual const char * BindStart(void)=0;

protected:

};

#if defined(C_SDL2) /* SDL 2.x */

/* HACK */
typedef SDL_Scancode SDLKey;

#else /* !defined(C_SDL2) SDL 1.x */

#define MAX_SDLKEYS 323

static bool usescancodes;
static Bit8u scancode_map[MAX_SDLKEYS];

#define Z SDLK_UNKNOWN

#if defined (MACOSX)
static SDLKey sdlkey_map[]={
	/* Main block printables */
	/*00-05*/ Z, SDLK_s, SDLK_d, SDLK_f, SDLK_h, SDLK_g,
	/*06-0B*/ SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_WORLD_0, SDLK_b,
	/*0C-11*/ SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_y, SDLK_t, 
	/*12-17*/ SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_6, SDLK_5, 
	/*18-1D*/ SDLK_EQUALS, SDLK_9, SDLK_7, SDLK_MINUS, SDLK_8, SDLK_0, 
	/*1E-21*/ SDLK_RIGHTBRACKET, SDLK_o, SDLK_u, SDLK_LEFTBRACKET, 
	/*22-23*/ SDLK_i, SDLK_p,
	/*24-29*/ SDLK_RETURN, SDLK_l, SDLK_j, SDLK_QUOTE, SDLK_k, SDLK_SEMICOLON, 
	/*2A-29*/ SDLK_BACKSLASH, SDLK_COMMA, SDLK_SLASH, SDLK_n, SDLK_m, 
	/*2F-2F*/ SDLK_PERIOD,

	/* Spaces, controls, modifiers (dosbox uses LMETA only for
	 * hotkeys, it's not really mapped to an emulated key) */
	/*30-33*/ SDLK_TAB, SDLK_SPACE, SDLK_BACKQUOTE, SDLK_BACKSPACE,
	/*34-37*/ Z, SDLK_ESCAPE, Z, SDLK_LMETA,
	/*38-3B*/ SDLK_LSHIFT, SDLK_CAPSLOCK, SDLK_LALT, SDLK_LCTRL,

	/*3C-40*/ Z, Z, Z, Z, Z,

	/* Keypad (KP_EQUALS not supported, NUMLOCK used on what is CLEAR
	 * in Mac OS X) */
	/*41-46*/ SDLK_KP_PERIOD, Z, SDLK_KP_MULTIPLY, Z, SDLK_KP_PLUS, Z,
	/*47-4A*/ SDLK_NUMLOCK /*==SDLK_CLEAR*/, Z, Z, Z,
	/*4B-4D*/ SDLK_KP_DIVIDE, SDLK_KP_ENTER, Z,
	/*4E-51*/ SDLK_KP_MINUS, Z, Z, SDLK_KP_EQUALS,
	/*52-57*/ SDLK_KP0, SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5, 
	/*58-5C*/ SDLK_KP6, SDLK_KP7, Z, SDLK_KP8, SDLK_KP9, 

	/*5D-5F*/ Z, Z, SDLK_a,
	
	/* Function keys and cursor blocks (F13 not supported, F14 =>
	 * PRINT[SCREEN], F15 => SCROLLOCK, F16 => PAUSE, HELP => INSERT) */
	/*60-64*/ SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F3, SDLK_F8,
	/*65-6A*/ SDLK_F9, Z, SDLK_F11, Z, SDLK_F13, SDLK_PAUSE /*==SDLK_F16*/,
	/*6B-70*/ SDLK_PRINT /*==SDLK_F14*/, Z, SDLK_F10, Z, SDLK_F12, Z,
	/*71-72*/ SDLK_SCROLLOCK /*==SDLK_F15*/, SDLK_INSERT /*==SDLK_HELP*/, 
	/*73-77*/ SDLK_HOME, SDLK_PAGEUP, SDLK_DELETE, SDLK_F4, SDLK_END,
	/*78-7C*/ SDLK_F2, SDLK_PAGEDOWN, SDLK_F1, SDLK_LEFT, SDLK_RIGHT,
	/*7D-7E*/ SDLK_DOWN, SDLK_UP,

	/*7F-7F*/ Z,

	/* 4 extra keys that don't really exist, but are needed for
	 * round-trip mapping (dosbox uses RMETA only for hotkeys, it's
	 * not really mapped to an emulated key) */
	SDLK_RMETA, SDLK_RSHIFT, SDLK_RALT, SDLK_RCTRL
};
#define MAX_SCANCODES (0x80+4)
/* Make sure that the table above has the expected size.  This
   expression will raise a compiler error if the condition is false.  */
typedef char assert_right_size [MAX_SCANCODES == (sizeof(sdlkey_map)/sizeof(sdlkey_map[0]))	? 1 : -1];

#else // !MACOSX

#define MAX_SCANCODES 0xdf
static SDLKey sdlkey_map[MAX_SCANCODES]={SDLK_UNKNOWN,SDLK_ESCAPE,
	SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,SDLK_0,
	/* 0x0c: */
	SDLK_MINUS,SDLK_EQUALS,SDLK_BACKSPACE,SDLK_TAB,
	SDLK_q,SDLK_w,SDLK_e,SDLK_r,SDLK_t,SDLK_y,SDLK_u,SDLK_i,SDLK_o,SDLK_p,
	SDLK_LEFTBRACKET,SDLK_RIGHTBRACKET,SDLK_RETURN,SDLK_LCTRL,
	SDLK_a,SDLK_s,SDLK_d,SDLK_f,SDLK_g,SDLK_h,SDLK_j,SDLK_k,SDLK_l,
	SDLK_SEMICOLON,SDLK_QUOTE,SDLK_BACKQUOTE,SDLK_LSHIFT,SDLK_BACKSLASH,
	SDLK_z,SDLK_x,SDLK_c,SDLK_v,SDLK_b,SDLK_n,SDLK_m,
	/* 0x33: */
	SDLK_COMMA,SDLK_PERIOD,SDLK_SLASH,SDLK_RSHIFT,SDLK_KP_MULTIPLY,
	SDLK_LALT,SDLK_SPACE,SDLK_CAPSLOCK,
	SDLK_F1,SDLK_F2,SDLK_F3,SDLK_F4,SDLK_F5,SDLK_F6,SDLK_F7,SDLK_F8,SDLK_F9,SDLK_F10,
	/* 0x45: */
	SDLK_NUMLOCK,SDLK_SCROLLOCK,
	SDLK_KP7,SDLK_KP8,SDLK_KP9,SDLK_KP_MINUS,SDLK_KP4,SDLK_KP5,SDLK_KP6,SDLK_KP_PLUS,
	SDLK_KP1,SDLK_KP2,SDLK_KP3,SDLK_KP0,SDLK_KP_PERIOD,
	SDLK_UNKNOWN,SDLK_UNKNOWN,
	SDLK_LESS,SDLK_F11,SDLK_F12, Z, Z, Z, Z, Z, Z, Z,
	/* 0x60: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0x70: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0x80: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0x90: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xA0: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xB0: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xC0: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xD0: */
	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z//,Z,Z,
	/* 0xE0: */
	//Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xF0: */
//	Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z//,Z,Z

};
#endif

#undef Z


SDLKey MapSDLCode(Bitu skey) {
//	LOG_MSG("MapSDLCode %d %X",skey,skey);
	if (usescancodes) {
		if (skey<MAX_SCANCODES) return sdlkey_map[skey];
		else return SDLK_UNKNOWN;
	} else return (SDLKey)skey;
}

Bitu GetKeyCode(SDL_keysym keysym) {
//	LOG_MSG("GetKeyCode %X %X %X",keysym.scancode,keysym.sym,keysym.mod);
	if (usescancodes) {
		Bitu key=(Bitu)keysym.scancode;

#if defined (MACOSX)
		if ((keysym.scancode == 0) && (keysym.sym == 'a')) key = 0x5f;  // zero value makes the keyboar crazy
#endif

		if (key==0
#if defined (MACOSX)
		    /* On Mac on US keyboards, scancode 0 is actually the 'a'
		     * key.  For good measure exclude all printables from this
		     * condition. */
		    && (keysym.sym < SDLK_SPACE || keysym.sym > SDLK_WORLD_95)
#endif
			) {
			/* try to retrieve key from symbolic key as scancode is zero */
			if (keysym.sym<MAX_SDLKEYS) key=scancode_map[(Bitu)keysym.sym];
		} 
#if !defined (WIN32) && !defined (MACOSX) && !defined(OS2)
		/* Linux adds 8 to all scancodes */
		else key-=8;
#endif
#if defined (WIN32)
		switch (key) {
			case 0x1c:	// ENTER
			case 0x1d:	// CONTROL
			case 0x35:	// SLASH
			case 0x37:	// PRINTSCREEN
			case 0x38:	// ALT
			case 0x45:	// PAUSE
			case 0x47:	// HOME
			case 0x48:	// cursor UP
			case 0x49:	// PAGE UP
			case 0x4b:	// cursor LEFT
			case 0x4d:	// cursor RIGHT
			case 0x4f:	// END
			case 0x50:	// cursor DOWN
			case 0x51:	// PAGE DOWN
			case 0x52:	// INSERT
			case 0x53:	// DELETE
				if (GFX_SDLUsingWinDIB()) key=scancode_map[(Bitu)keysym.sym];
				break;
		}
#endif
		return key;
	} else {
#if defined (WIN32)
		/* special handling of 102-key under windows */
		if ((keysym.sym==SDLK_BACKSLASH) && (keysym.scancode==0x56)) return (Bitu)SDLK_LESS;
		/* special case of the \ _ key on Japanese 106-keyboards. seems to be the same code whether or not you've told Windows it's a 106-key */
		/* NTS: SDL source on Win32 maps this key (VK_OEM_102) to SDLK_LESS */
		if (isJPkeyboard && (keysym.sym == 0) && (keysym.scancode == 0x73)) return (Bitu)SDLK_WORLD_10; //FIXME: There's no SDLK code for that key! Re-use one of the world keys!
		/* another hack, for the Yen \ pipe key on Japanese 106-keyboards.
		   sym == 0 if English layout, sym == 0x5C if Japanese layout */
		if (isJPkeyboard && (keysym.sym == 0 || keysym.sym == 0x5C) && (keysym.scancode == 0x7D)) return (Bitu)SDLK_WORLD_11; //FIXME: There's no SDLK code for that key! Re-use one of the world keys!
		/* what is ~ ` on American keyboards is "Hankaku" on Japanese keyboards. Same scan code. */
		if (isJPkeyboard && keysym.sym == SDLK_BACKQUOTE) return (int)SDLK_WORLD_12;
		/* Muhenkan */
		if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x7B) return (int)SDLK_WORLD_13;
		/* Henkan/Zenkouho */
		if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x79) return (int)SDLK_WORLD_14;
		/* Hiragana/Katakana */
		if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x70) return (int)SDLK_WORLD_15;
#endif
		return (Bitu)keysym.sym;
	}
}

#endif /* !defined(C_SDL2) */

class CKeyBind;
class CKeyBindGroup;

class CKeyBind : public CBind {
public:
	CKeyBind(CBindList * _list,SDLKey _key) : CBind(_list) {
		key = _key;
	}
	virtual ~CKeyBind() {}
	void BindName(char * buf) {
#if defined(C_SDL2)
        sprintf(buf,"Key %s",SDL_GetScancodeName(key));
#else
		sprintf(buf,"Key %s",SDL_GetKeyName(MapSDLCode((Bitu)key)));
#endif
	}
	void ConfigName(char * buf) {
#if defined(C_SDL2)
        sprintf(buf,"key %d",key);
#else
		sprintf(buf,"key %d",MapSDLCode((Bitu)key));
#endif
	}
public:
	SDLKey key;
};

class CKeyBindGroup : public  CBindGroup {
public:
	CKeyBindGroup(Bitu _keys) : CBindGroup (){
		lists=new CBindList[_keys];
		for (Bitu i=0;i<_keys;i++) lists[i].clear();
		keys=_keys;
		configname="key";
	}
	virtual ~CKeyBindGroup() { delete[] lists; }
	CBind * CreateConfigBind(char *& buf) {
		if (strncasecmp(buf,configname,strlen(configname))) return 0;
		StripWord(buf);char * num=StripWord(buf);
		Bitu code=ConvDecWord(num);
#if defined(C_SDL2)
        CBind * bind=CreateKeyBind((SDL_Scancode)code);
#else
		if (usescancodes) {
			if (code<MAX_SDLKEYS) code=scancode_map[code];
			else code=0;
		}
		CBind * bind=CreateKeyBind((SDLKey)code);
#endif
		return bind;
	}
	CBind * CreateEventBind(SDL_Event * event) {
		if (event->type!=SDL_KEYDOWN) return 0;
#if defined(C_SDL2)
        return CreateKeyBind(event->key.keysym.scancode);
#else
		return CreateKeyBind((SDLKey)GetKeyCode(event->key.keysym));
#endif
	};
	bool CheckEvent(SDL_Event * event) {
		if (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP) return false;
#if defined(C_SDL2)
        Bitu key = event->key.keysym.scancode;
#else
		Bitu key=GetKeyCode(event->key.keysym);
		assert(Bitu(event->key.keysym.sym)<keys);
#endif
//		LOG_MSG("key type %i is %x [%x %x]",event->type,key,event->key.keysym.sym,event->key.keysym.scancode);

#if defined(WIN32) && !defined(C_SDL2)
		/* HACK: When setting up the Japanese keyboard layout, I'm seeing some bizarre keyboard handling
		         from within Windows when pressing the ~ ` (grave) aka Hankaku key. I know it's not hardware
				 because when you switch back to English the key works normally as the tilde/grave key.
				 What I'm seeing is that pressing the key only sends the "down" event (even though you're
				 not holding the key down). When you press the key again, an "up" event is sent immediately
				 followed by a "down" event. This is confusing to the mapper, so we work around it here. */
		if (isJPkeyboard && key == SDLK_WORLD_12/*Hankaku*/) {
			if (event->type == SDL_KEYDOWN) {
				// send down, then up (right?)
				ActivateBindList(&lists[key], 0x7fff, true);
				DeactivateBindList(&lists[key], true);
			}

			return 0; // ignore up event
		}
#endif
		if (event->type==SDL_KEYDOWN) ActivateBindList(&lists[key],0x7fff,true);
		else DeactivateBindList(&lists[key],true);
		return 0;
	}
	CBind * CreateKeyBind(SDLKey _key) {
#if !defined(C_SDL2)
		if (!usescancodes) assert((Bitu)_key<keys);
#endif
		return new CKeyBind(&lists[(Bitu)_key],_key);
	}
private:
	const char * ConfigStart(void) {
		return configname;
	}
	const char * BindStart(void) {
		return "Key";
	}
protected:
	const char * configname;
	CBindList * lists;
	Bitu keys;
};

#define MAX_VJOY_BUTTONS 8
#define MAX_VJOY_AXES 8

static struct {
	bool button_pressed[MAX_VJOY_BUTTONS];
	Bit16s axis_pos[MAX_VJOY_AXES];
	bool hat_pressed[16];
} virtual_joysticks[2];


class CJAxisBind;
class CJButtonBind;
class CJHatBind;

class CJAxisBind : public CBind {
public:
	CJAxisBind(CBindList * _list,CBindGroup * _group,Bitu _axis,bool _positive) : CBind(_list){
		group = _group;
		axis = _axis;
		positive = _positive;
	}
	virtual ~CJAxisBind() {}
	void ConfigName(char * buf) {
		sprintf(buf,"%s axis %d %d",group->ConfigStart(),(int)axis,positive ? 1 : 0);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s Axis %d%s",group->BindStart(),(int)axis,positive ? "+" : "-");
	}
protected:
	CBindGroup * group;
	Bitu axis;
	bool positive;
};

class CJButtonBind : public CBind {
public:
	CJButtonBind(CBindList * _list,CBindGroup * _group,Bitu _button) : CBind(_list) {
		group = _group;
		button=_button;
	}
	virtual ~CJButtonBind() {}
	void ConfigName(char * buf) {
		sprintf(buf,"%s button %d",group->ConfigStart(),(int)button);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s Button %d",group->BindStart(),(int)button);
	}
protected:
	CBindGroup * group;
	Bitu button;
};

class CJHatBind : public CBind {
public:
	CJHatBind(CBindList * _list,CBindGroup * _group,Bitu _hat,Bit8u _dir) : CBind(_list) {
		group = _group;
		hat   = _hat;
		dir   = _dir;
		/* allow only one hat position */
		if (dir&SDL_HAT_UP) dir=SDL_HAT_UP;
		else if (dir&SDL_HAT_RIGHT) dir=SDL_HAT_RIGHT;
		else if (dir&SDL_HAT_DOWN) dir=SDL_HAT_DOWN;
		else if (dir&SDL_HAT_LEFT) dir=SDL_HAT_LEFT;
		else E_Exit("MAPPER:JOYSTICK:Invalid hat position");
	}
	virtual ~CJHatBind() {}
	void ConfigName(char * buf) {
		sprintf(buf,"%s hat %d %d",group->ConfigStart(),(int)hat,(int)dir);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s Hat %d %s",group->BindStart(),(int)hat,(dir==SDL_HAT_UP)?"up":
														((dir==SDL_HAT_RIGHT)?"right":
														((dir==SDL_HAT_DOWN)?"down":"left")));
	}
protected:
	CBindGroup * group;
	Bitu hat;
	Bit8u dir;
};

bool autofire = false;

class CStickBindGroup : public  CBindGroup {
public:
	CStickBindGroup(Bitu _stick,Bitu _emustick,bool _dummy=false) : CBindGroup (){
		stick=_stick;		// the number of the physical device (SDL numbering|)
		emustick=_emustick;	// the number of the emulated device
		sprintf(configname,"stick_%d",(int)emustick);

		sdl_joystick=NULL;
		axes=0;	buttons=0; hats=0;
		button_wrap=0;
		button_cap=0; axes_cap=0; hats_cap=0;
		emulated_buttons=0; emulated_axes=0; emulated_hats=0;
		pos_axis_lists = neg_axis_lists = NULL; /* <- Initialize the pointers to NULL. The C++ compiler won't do it for us. */
		button_lists = hat_lists = NULL;

		is_dummy=_dummy;
		if (_dummy) return;

		// initialize binding lists and position data
		pos_axis_lists=new CBindList[MAXAXIS];
		neg_axis_lists=new CBindList[MAXAXIS];
		button_lists=new CBindList[MAXBUTTON];
		hat_lists=new CBindList[4];
		Bitu i;
		for (i=0; i<MAXBUTTON; i++) {
			button_autofire[i]=0;
			old_button_state[i]=0;
		}
		for(i=0;i<16;i++) old_hat_state[i]=0;
		for (i=0; i<MAXAXIS; i++) {
			old_pos_axis_state[i]=false;
			old_neg_axis_state[i]=false;
		}

		// initialize emulated joystick state
		emulated_axes=2;
		emulated_buttons=2;
		emulated_hats=0;
		JOYSTICK_Enable(emustick,true);

		sdl_joystick=SDL_JoystickOpen(_stick);
		if (sdl_joystick==NULL) {
			button_wrap=emulated_buttons;
			return;
		}

		axes=SDL_JoystickNumAxes(sdl_joystick);
		if (axes > MAXAXIS) axes = MAXAXIS;
		axes_cap=emulated_axes;
		if (axes_cap>axes) axes_cap=axes;

		hats=SDL_JoystickNumHats(sdl_joystick);
		if (hats > MAXHAT) hats = MAXHAT;
		hats_cap=emulated_hats;
		if (hats_cap>hats) hats_cap=hats;

		buttons=SDL_JoystickNumButtons(sdl_joystick);
		button_wrap=buttons;
		button_cap=buttons;
		if (button_wrapping_enabled) {
			button_wrap=emulated_buttons;
			if (buttons>MAXBUTTON_CAP) button_cap = MAXBUTTON_CAP;
		}
		if (button_wrap > MAXBUTTON) button_wrap = MAXBUTTON;

#if defined(C_SDL2)
        LOG_MSG("Using joystick %s with %d axes, %d buttons and %d hat(s)",SDL_JoystickNameForIndex(stick),(int)axes,(int)buttons,(int)hats);
#else
        LOG_MSG("Using joystick %s with %d axes, %d buttons and %d hat(s)",SDL_JoystickName(stick),(int)axes,(int)buttons,(int)hats);
#endif
	}
	virtual ~CStickBindGroup() {
		SDL_JoystickClose(sdl_joystick);
		if (pos_axis_lists != NULL) delete[] pos_axis_lists;
		if (neg_axis_lists != NULL) delete[] neg_axis_lists;
		if (button_lists != NULL) delete[] button_lists;
		if (hat_lists != NULL) delete[] hat_lists;
	}

	CBind * CreateConfigBind(char *& buf) {
		if (strncasecmp(configname,buf,strlen(configname))) return 0;
		StripWord(buf);char * type=StripWord(buf);
		CBind * bind=0;
		if (!strcasecmp(type,"axis")) {
			Bitu ax=ConvDecWord(StripWord(buf));
			bool pos=ConvDecWord(StripWord(buf)) > 0;
			bind=CreateAxisBind(ax,pos);
		} else if (!strcasecmp(type,"button")) {
			Bitu but=ConvDecWord(StripWord(buf));			
			bind=CreateButtonBind(but);
		} else if (!strcasecmp(type,"hat")) {
			Bitu hat=ConvDecWord(StripWord(buf));			
			Bit8u dir=(Bit8u)ConvDecWord(StripWord(buf));			
			bind=CreateHatBind(hat,dir);
		}
		return bind;
	}
	CBind * CreateEventBind(SDL_Event * event) {
		if (event->type==SDL_JOYAXISMOTION) {
			if (event->jaxis.which!=stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
			if (event->jaxis.axis>=axes) return 0;
#endif
			if (abs(event->jaxis.value)<25000) return 0;
			return CreateAxisBind(event->jaxis.axis,event->jaxis.value>0);
		} else if (event->type==SDL_JOYBUTTONDOWN) {
			if (event->button.which!=stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
			return CreateButtonBind(event->jbutton.button%button_wrap);
#else
			return CreateButtonBind(event->jbutton.button);
#endif
		} else if (event->type==SDL_JOYHATMOTION) {
			if (event->jhat.which!=stick) return 0;
			if (event->jhat.value==0) return 0;
			if (event->jhat.value>(SDL_HAT_UP|SDL_HAT_RIGHT|SDL_HAT_DOWN|SDL_HAT_LEFT)) return 0;
			return CreateHatBind(event->jhat.hat,event->jhat.value);
		} else return 0;
	}

	virtual bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick) {
					if(jaxis->axis == 0)
						JOYSTICK_Move_X(emustick,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 1)
						JOYSTICK_Move_Y(emustick,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
					JOYSTICK_Button(emustick,but,state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		if (is_dummy) return;
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		Bitu i;
		for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
		for (i=0; i<MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[emustick].button_pressed[i])
				button_pressed[i % button_wrap]=true;
		}
		for (i=0; i<emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(emustick,i,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(emustick,i,button_pressed[i]);
		}

		JOYSTICK_Move_X(emustick,((float)virtual_joysticks[emustick].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(emustick,((float)virtual_joysticks[emustick].axis_pos[1])/32768.0f);
	}

	void ActivateJoystickBoundEvents() {
		if (GCC_UNLIKELY(sdl_joystick==NULL)) return;

		Bitu i;

		bool button_pressed[MAXBUTTON];
		for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
		/* read button states */
		for (i=0; i<button_cap; i++) {
			if (SDL_JoystickGetButton(sdl_joystick,i))
				button_pressed[i % button_wrap]=true;
		}
		for (i=0; i<button_wrap; i++) {
			/* activate binding if button state has changed */
			if (button_pressed[i]!=old_button_state[i]) {
				if (button_pressed[i]) ActivateBindList(&button_lists[i],32767,true);
				else DeactivateBindList(&button_lists[i],true);
				old_button_state[i]=button_pressed[i];
			}
		}

		for (i=0; i<axes; i++) {
			Sint16 caxis_pos=SDL_JoystickGetAxis(sdl_joystick,i);
			/* activate bindings for joystick position */
			if (caxis_pos>1) {
				if (old_neg_axis_state[i]) {
					DeactivateBindList(&neg_axis_lists[i],false);
					old_neg_axis_state[i] = false;
				}
				ActivateBindList(&pos_axis_lists[i],caxis_pos,false);
				old_pos_axis_state[i] = true;
			} else if (caxis_pos<-1) {
				if (old_pos_axis_state[i]) {
					DeactivateBindList(&pos_axis_lists[i],false);
					old_pos_axis_state[i] = false;
				}
				if (caxis_pos!=-32768) caxis_pos=(Sint16)abs(caxis_pos);
				else caxis_pos=32767;
				ActivateBindList(&neg_axis_lists[i],caxis_pos,false);
				old_neg_axis_state[i] = true;
			} else {
				/* center */
				if (old_pos_axis_state[i]) {
					DeactivateBindList(&pos_axis_lists[i],false);
					old_pos_axis_state[i] = false;
				}
				if (old_neg_axis_state[i]) {
					DeactivateBindList(&neg_axis_lists[i],false);
					old_neg_axis_state[i] = false;
				}
			}
		}

		for (i=0; i<hats; i++) {
			Uint8 chat_state=SDL_JoystickGetHat(sdl_joystick,i);

			/* activate binding if hat state has changed */
			if ((chat_state & SDL_HAT_UP) != (old_hat_state[i] & SDL_HAT_UP)) {
				if (chat_state & SDL_HAT_UP) ActivateBindList(&hat_lists[(i<<2)+0],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+0],true);
			}
			if ((chat_state & SDL_HAT_RIGHT) != (old_hat_state[i] & SDL_HAT_RIGHT)) {
				if (chat_state & SDL_HAT_RIGHT) ActivateBindList(&hat_lists[(i<<2)+1],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+1],true);
			}
			if ((chat_state & SDL_HAT_DOWN) != (old_hat_state[i] & SDL_HAT_DOWN)) {
				if (chat_state & SDL_HAT_DOWN) ActivateBindList(&hat_lists[(i<<2)+2],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+2],true);
			}
			if ((chat_state & SDL_HAT_LEFT) != (old_hat_state[i] & SDL_HAT_LEFT)) {
				if (chat_state & SDL_HAT_LEFT) ActivateBindList(&hat_lists[(i<<2)+3],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+3],true);
			}
			old_hat_state[i]=chat_state;
		}
	}

private:
	CBind * CreateAxisBind(Bitu axis,bool positive) {
		if (axis<axes) {
			if (positive) return new CJAxisBind(&pos_axis_lists[axis],this,axis,positive);
			else return new CJAxisBind(&neg_axis_lists[axis],this,axis,positive);
		}
		return NULL;
	}
	CBind * CreateButtonBind(Bitu button) {
		if (button<button_wrap) 
			return new CJButtonBind(&button_lists[button],this,button);
		return NULL;
	}
	CBind * CreateHatBind(Bitu hat,Bit8u value) {
		Bitu hat_dir;
		if (value&SDL_HAT_UP) hat_dir=0;
		else if (value&SDL_HAT_RIGHT) hat_dir=1;
		else if (value&SDL_HAT_DOWN) hat_dir=2;
		else if (value&SDL_HAT_LEFT) hat_dir=3;
		else return NULL;
		return new CJHatBind(&hat_lists[(hat<<2)+hat_dir],this,hat,value);
	}
	const char * ConfigStart(void) {
		return configname;
	}
	const char * BindStart(void) {
#if defined(C_SDL2)
        if (sdl_joystick!=NULL) return SDL_JoystickNameForIndex(stick);
#else
        if (sdl_joystick!=NULL) return SDL_JoystickName(stick);
#endif
        else return "[missing joystick]";
	}

protected:
	CBindList * pos_axis_lists;
	CBindList * neg_axis_lists;
	CBindList * button_lists;
	CBindList * hat_lists;
	Bitu stick,emustick,axes,buttons,hats,emulated_axes,emulated_buttons,emulated_hats;
	Bitu button_wrap,button_cap,axes_cap,hats_cap;
	SDL_Joystick * sdl_joystick;
	char configname[10];
	Bitu button_autofire[MAXBUTTON];
	bool old_button_state[MAXBUTTON];
	bool old_pos_axis_state[MAXAXIS];
	bool old_neg_axis_state[MAXAXIS];
	Uint8 old_hat_state[16];
	bool is_dummy;
};

class C4AxisBindGroup : public  CStickBindGroup {
public:
	C4AxisBindGroup(Bitu _stick,Bitu _emustick) : CStickBindGroup (_stick,_emustick){
		emulated_axes=4;
		emulated_buttons=4;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;

		axes_cap=emulated_axes;
		if (axes_cap>axes) axes_cap=axes;
		hats_cap=emulated_hats;
		if (hats_cap>hats) hats_cap=hats;

		JOYSTICK_Enable(1,true);
	}
	virtual ~C4AxisBindGroup() {}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick && jaxis->axis < 4) {
					if(jaxis->axis & 1)
						JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
					else
						JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
					JOYSTICK_Button((but >> 1),(but & 1),state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		Bitu i;
		for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
		for (i=0; i<MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap]=true;
			
		}
		for (i=0; i<emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(i>>1,i&1,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(i>>1,i&1,button_pressed[i]);
		}

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);
		JOYSTICK_Move_Y(1,((float)virtual_joysticks[0].axis_pos[3])/32768.0f);
	}
};

class CFCSBindGroup : public  CStickBindGroup {
public:
	CFCSBindGroup(Bitu _stick,Bitu _emustick) : CStickBindGroup (_stick,_emustick){
		emulated_axes=4;
		emulated_buttons=4;
		old_hat_position=0;
		emulated_hats=1;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;

		axes_cap=emulated_axes;
		if (axes_cap>axes) axes_cap=axes;
		hats_cap=emulated_hats;
		if (hats_cap>hats) hats_cap=hats;

		JOYSTICK_Enable(1,true);
		JOYSTICK_Move_Y(1,1.0);
	}
	virtual ~CFCSBindGroup() {}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick) {
					if(jaxis->axis == 0)
						JOYSTICK_Move_X(0,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 1)
						JOYSTICK_Move_Y(0,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 2)
						JOYSTICK_Move_X(1,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYHATMOTION:
				jhat = &event->jhat;
				if(jhat->which == stick) DecodeHatPosition(jhat->value);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
						JOYSTICK_Button((but >> 1),(but & 1),state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		Bitu i;
		for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
		for (i=0; i<MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap]=true;
			
		}
		for (i=0; i<emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(i>>1,i&1,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(i>>1,i&1,button_pressed[i]);
		}

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);

		Uint8 hat_pos=0;
		if (virtual_joysticks[0].hat_pressed[0]) hat_pos|=SDL_HAT_UP;
		else if (virtual_joysticks[0].hat_pressed[2]) hat_pos|=SDL_HAT_DOWN;
		if (virtual_joysticks[0].hat_pressed[3]) hat_pos|=SDL_HAT_LEFT;
		else if (virtual_joysticks[0].hat_pressed[1]) hat_pos|=SDL_HAT_RIGHT;

		if (hat_pos!=old_hat_position) {
			DecodeHatPosition(hat_pos);
			old_hat_position=hat_pos;
		}
	}

private:
	Uint8 old_hat_position;

	void DecodeHatPosition(Uint8 hat_pos) {
		switch(hat_pos) {
			case SDL_HAT_CENTERED:
				JOYSTICK_Move_Y(1,1.0);
				break;
			case SDL_HAT_UP:
				JOYSTICK_Move_Y(1,-1.0);
				break;
			case SDL_HAT_RIGHT:
				JOYSTICK_Move_Y(1,-0.5);
				break;
			case SDL_HAT_DOWN:
				JOYSTICK_Move_Y(1,0.0);
				break;
			case SDL_HAT_LEFT:
				JOYSTICK_Move_Y(1,0.5);
				break;
			case SDL_HAT_LEFTUP:
				if(JOYSTICK_GetMove_Y(1) < 0)
					JOYSTICK_Move_Y(1,0.5);
				else
					JOYSTICK_Move_Y(1,-1.0);
				break;
			case SDL_HAT_RIGHTUP:
				if(JOYSTICK_GetMove_Y(1) < -0.7)
					JOYSTICK_Move_Y(1,-0.5);
				else
					JOYSTICK_Move_Y(1,-1.0);
				break;
			case SDL_HAT_RIGHTDOWN:
				if(JOYSTICK_GetMove_Y(1) < -0.2)
					JOYSTICK_Move_Y(1,0.0);
				else
					JOYSTICK_Move_Y(1,-0.5);
				break;
			case SDL_HAT_LEFTDOWN:
				if(JOYSTICK_GetMove_Y(1) > 0.2)
					JOYSTICK_Move_Y(1,0.0);
				else
					JOYSTICK_Move_Y(1,0.5);
				break;
		}
	}
};

class CCHBindGroup : public  CStickBindGroup {
public:
	CCHBindGroup(Bitu _stick,Bitu _emustick) : CStickBindGroup (_stick,_emustick){
		emulated_axes=4;
		emulated_buttons=6;
		emulated_hats=1;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;

		axes_cap=emulated_axes;
		if (axes_cap>axes) axes_cap=axes;
		hats_cap=emulated_hats;
		if (hats_cap>hats) hats_cap=hats;

		JOYSTICK_Enable(1,true);
		button_state=0;
	}
	virtual ~CCHBindGroup() {}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;
		Bitu but = 0;
		static unsigned const button_magic[6]={0x02,0x04,0x10,0x100,0x20,0x200};
		static unsigned const hat_magic[2][5]={{0x8888,0x8000,0x800,0x80,0x08},
							   {0x5440,0x4000,0x400,0x40,0x1000}};
		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick && jaxis->axis < 4) {
					if(jaxis->axis & 1)
						JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
					else
						JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYHATMOTION:
				jhat = &event->jhat;
				if(jhat->which == stick && jhat->hat < 2) {
					if(jhat->value == SDL_HAT_CENTERED)
						button_state&=~hat_magic[jhat->hat][0];
					if(jhat->value & SDL_HAT_UP)
						button_state|=hat_magic[jhat->hat][1];
					if(jhat->value & SDL_HAT_RIGHT)
						button_state|=hat_magic[jhat->hat][2];
					if(jhat->value & SDL_HAT_DOWN)
						button_state|=hat_magic[jhat->hat][3];
					if(jhat->value & SDL_HAT_LEFT)
						button_state|=hat_magic[jhat->hat][4];
				}
				break;
			case SDL_JOYBUTTONDOWN:
				jbutton = &event->jbutton;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick)
					button_state|=button_magic[but];
				break;
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick)
					button_state&=~button_magic[but];
				break;
		}

		unsigned i;
		Bit16u j;
		j=button_state;
		for(i=0;i<16;i++) if (j & 1) break; else j>>=1;
		JOYSTICK_Button(0,0,i&1);
		JOYSTICK_Button(0,1,(i>>1)&1);
		JOYSTICK_Button(1,0,(i>>2)&1);
		JOYSTICK_Button(1,1,(i>>3)&1);
		return false;
	}

	void UpdateJoystick() {
		static unsigned const button_priority[6]={7,11,13,14,5,6};
		static unsigned const hat_priority[2][4]={{0,1,2,3},{8,9,10,12}};

		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);
		JOYSTICK_Move_Y(1,((float)virtual_joysticks[0].axis_pos[3])/32768.0f);

		Bitu bt_state=15;

		Bitu i;
		for (i=0; i<(hats<2?hats:2); i++) {
			Uint8 hat_pos=0;
			if (virtual_joysticks[0].hat_pressed[(i<<2)+0]) hat_pos|=SDL_HAT_UP;
			else if (virtual_joysticks[0].hat_pressed[(i<<2)+2]) hat_pos|=SDL_HAT_DOWN;
			if (virtual_joysticks[0].hat_pressed[(i<<2)+3]) hat_pos|=SDL_HAT_LEFT;
			else if (virtual_joysticks[0].hat_pressed[(i<<2)+1]) hat_pos|=SDL_HAT_RIGHT;

			if (hat_pos & SDL_HAT_UP)
				if (bt_state>hat_priority[i][0]) bt_state=hat_priority[i][0];
			if (hat_pos & SDL_HAT_DOWN)
				if (bt_state>hat_priority[i][1]) bt_state=hat_priority[i][1];
			if (hat_pos & SDL_HAT_RIGHT)
				if (bt_state>hat_priority[i][2]) bt_state=hat_priority[i][2];
			if (hat_pos & SDL_HAT_LEFT)
				if (bt_state>hat_priority[i][3]) bt_state=hat_priority[i][3];
		}

		bool button_pressed[MAXBUTTON];
		for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
		for (i=0; i<MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap]=true;
			
		}
		for (i=0; i<6; i++) {
			if ((button_pressed[i]) && (bt_state>button_priority[i]))
				bt_state=button_priority[i];
		}

		if (bt_state>15) bt_state=15;
		JOYSTICK_Button(0,0,(bt_state&8)==0);
		JOYSTICK_Button(0,1,(bt_state&4)==0);
		JOYSTICK_Button(1,0,(bt_state&2)==0);
		JOYSTICK_Button(1,1,(bt_state&1)==0);
	}

protected:
	Bit16u button_state;
};

static struct CMapper {
#if defined(C_SDL2)
    SDL_Window * window;
    SDL_Rect draw_rect;
    SDL_Surface * draw_surface_nonpaletted;
	SDL_Surface * draw_surface;
#endif
	SDL_Surface * surface;
	bool exit;
	CEvent * aevent;				//Active Event
	CBind * abind;					//Active Bind
	CBindList_it abindit;			//Location of active bind in list
	bool redraw;
	bool addbind;
	Bitu mods;
	struct {
		Bitu num_groups,num;
		CStickBindGroup * stick[MAXSTICKS];
	} sticks;
	std::string filename;
} mapper;

void CBindGroup::ActivateBindList(CBindList * list,Bits value,bool ev_trigger) {
	Bitu validmod=0;
	CBindList_it it;
	for (it=list->begin();it!=list->end();it++) {
		if (((*it)->mods & mapper.mods) == (*it)->mods) {
			if (validmod<(*it)->mods) validmod=(*it)->mods;
		}
	}
	for (it=list->begin();it!=list->end();it++) {
	/*BUG:CRASH if keymapper key is removed*/
		if (validmod==(*it)->mods) (*it)->ActivateBind(value,ev_trigger);
	}
}

void CBindGroup::DeactivateBindList(CBindList * list,bool ev_trigger) {
	CBindList_it it;
	for (it=list->begin();it!=list->end();it++) {
		(*it)->DeActivateBind(ev_trigger);
	}
}

static void DrawText(Bitu x,Bitu y,const char * text,Bit8u color,Bit8u bkcolor=CLR_BLACK) {
#if defined(C_SDL2)
    Bit8u * draw=((Bit8u *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
    Bit8u * draw=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
	while (*text) {
		Bit8u * font=&int10_font_14[(*text)*14];
		Bitu i,j;Bit8u * draw_line=draw;
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				if (map & 0x80) *(draw_line+j)=color;
				else *(draw_line+j)=bkcolor;
				map<<=1;
			}
#if defined(C_SDL2)
			draw_line+=mapper.draw_surface->w;
#else
			draw_line+=mapper.surface->pitch;
#endif
		}
		text++;draw+=8;
	}
}

class CButton {
public:
	virtual ~CButton(){};
	CButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy) {
		x=_x;y=_y;dx=_dx;dy=_dy;
		buttons.push_back(this);
        bkcolor=CLR_BLACK;
		color=CLR_WHITE;
		enabled=true;
	}
	virtual void Draw(void) {
		if (!enabled) return;
#if defined(C_SDL2)
        Bit8u * point=((Bit8u *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
        Bit8u * point=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
        for (Bitu lines=0;lines<dy;lines++)  {
			if (lines==0 || lines==(dy-1)) {
				for (Bitu cols=0;cols<dx;cols++) *(point+cols)=color;
			} else {
				for (Bitu cols=1;cols<(dx-1);cols++) *(point+cols)=bkcolor;
				*point=color;*(point+dx-1)=color;
			}
#if defined(C_SDL2)
			point+=mapper.draw_surface->w;
#else
			point+=mapper.surface->pitch;
#endif
		}
	}
	virtual bool OnTop(Bitu _x,Bitu _y) {
		return ( enabled && (_x>=x) && (_x<x+dx) && (_y>=y) && (_y<y+dy));
	}
    virtual void BindColor(void) {}
	virtual void Click(void) {}
	void Enable(bool yes) { 
		enabled=yes; 
		mapper.redraw=true;
	}
	void SetColor(Bit8u _col) { color=_col; }
protected:
	Bitu x,y,dx,dy;
	Bit8u color;
    Bit8u bkcolor;
	bool enabled;
};

class CTextButton : public CButton {
public:
	CTextButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text) : CButton(_x,_y,_dx,_dy) { text=_text;}
	virtual ~CTextButton() {}
	void Draw(void) {
		if (!enabled) return;
		CButton::Draw();
		DrawText(x+2,y+2,text,color,bkcolor);
	}
	void SetText(const char *_text) {
		text = _text;
	}
protected:
	const char * text;
};

class CEventButton;
static CEventButton * last_clicked = NULL;

class CEventButton : public CTextButton {
public:
	CEventButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,CEvent * _event) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		event=_event;	
	}
	virtual ~CEventButton() {}
	void Click(void) {
		if (last_clicked) last_clicked->SetColor(CLR_WHITE);
		this->SetColor(CLR_GREEN);
		SetActiveEvent(event);
		last_clicked=this;
	}
protected:
	CEvent * event;
};

class CCaptionButton : public CButton {
public:
	CCaptionButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy) : CButton(_x,_y,_dx,_dy){
		caption[0]=0;
	}
	virtual ~CCaptionButton() {}
	void Change(const char * format,...) GCC_ATTRIBUTE(__format__(__printf__,2,3));

	void Draw(void) {
		if (!enabled) return;
		DrawText(x+2,y+2,caption,color);
	}
protected:
	char caption[128];
};

void CCaptionButton::Change(const char * format,...) {
	va_list msg;
	va_start(msg,format);
	vsprintf(caption,format,msg);
	va_end(msg);
	mapper.redraw=true;
}		

static void change_action_text(const char* text,Bit8u col);

static void MAPPER_SaveBinds(void);
class CBindButton : public CTextButton {
public:	
	CBindButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BB_Types _type) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		type=_type;
	}
	virtual ~CBindButton() {}
	void Click(void) {
		switch (type) {
		case BB_Add: 
			mapper.addbind=true;
			SetActiveBind(0);
			change_action_text("Press a key/joystick button or move the joystick.",CLR_RED);
			break;
		case BB_Del:
			if (mapper.abindit!=mapper.aevent->bindlist.end())  {
				delete (*mapper.abindit);
				mapper.abindit=mapper.aevent->bindlist.erase(mapper.abindit);
				if (mapper.abindit==mapper.aevent->bindlist.end()) 
					mapper.abindit=mapper.aevent->bindlist.begin();
			}
			if (mapper.abindit!=mapper.aevent->bindlist.end()) SetActiveBind(*(mapper.abindit));
			else SetActiveBind(0);
			break;
		case BB_Next:
			if (mapper.abindit!=mapper.aevent->bindlist.end()) 
				mapper.abindit++;
			if (mapper.abindit==mapper.aevent->bindlist.end()) 
				mapper.abindit=mapper.aevent->bindlist.begin();
			SetActiveBind(*(mapper.abindit));
			break;
		case BB_Save:
			MAPPER_SaveBinds();
			break;
		case BB_Exit:   
			mapper.exit=true;
			break;
		case BB_Capture:
			GFX_CaptureMouse();
			if (mouselocked) change_action_text("Capture enabled. Hit ESC to release capture.",CLR_WHITE);
			break;
		}
	}
protected:
	BB_Types type;
};

class CCheckButton : public CTextButton {
public:	
	CCheckButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BC_Types _type) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		type=_type;
	}
	virtual ~CCheckButton() {}
	void Draw(void) {
		if (!enabled) return;
		bool checked=false;
		switch (type) {
		case BC_Mod1:
			checked=(mapper.abind->mods&BMOD_Mod1)>0;
			break;
		case BC_Mod2:
			checked=(mapper.abind->mods&BMOD_Mod2)>0;
			break;
		case BC_Mod3:
			checked=(mapper.abind->mods&BMOD_Mod3)>0;
			break;
		case BC_Hold:
			checked=(mapper.abind->flags&BFLG_Hold)>0;
			break;
		}
		if (checked) {
#if defined(C_SDL2)
			Bit8u * point=((Bit8u *)mapper.draw_surface->pixels)+((y+2)*mapper.draw_surface->pitch)+x+dx-dy+2;
#else
			Bit8u * point=((Bit8u *)mapper.surface->pixels)+((y+2)*mapper.surface->pitch)+x+dx-dy+2;
#endif
			for (Bitu lines=0;lines<(dy-4);lines++)  {
				memset(point,color,dy-4);
#if defined(C_SDL2)
				point+=mapper.draw_surface->w;
#else
				point+=mapper.surface->pitch;
#endif
			}
		}
		CTextButton::Draw();
	}
	void Click(void) {
		switch (type) {
		case BC_Mod1:
			mapper.abind->mods^=BMOD_Mod1;
			break;
		case BC_Mod2:
			mapper.abind->mods^=BMOD_Mod2;
			break;
		case BC_Mod3:
			mapper.abind->mods^=BMOD_Mod3;
			break;
		case BC_Hold:
			mapper.abind->flags^=BFLG_Hold;
			break;
		}
		mapper.redraw=true;
	}
protected:
	BC_Types type;
};

class CKeyEvent : public CTriggeredEvent {
public:
	CKeyEvent(char const * const _entry,KBD_KEYS _key) : CTriggeredEvent(_entry) {
		key=_key;
	}
	virtual ~CKeyEvent() {}
	void Active(bool yesno) {
		KEYBOARD_AddKey(key,yesno);
	};
	KBD_KEYS key;
};

class CJAxisEvent : public CContinuousEvent {
public:
	CJAxisEvent(char const * const _entry,Bitu _stick,Bitu _axis,bool _positive,CJAxisEvent * _opposite_axis) : CContinuousEvent(_entry) {
		stick=_stick;
		axis=_axis;
		positive=_positive;
		opposite_axis=_opposite_axis;
		if (_opposite_axis) {
			_opposite_axis->SetOppositeAxis(this);
		}
	}
	virtual ~CJAxisEvent() {}
	void Active(bool /*moved*/) {
		virtual_joysticks[stick].axis_pos[axis]=(Bit16s)(GetValue()*(positive?1:-1));
	}
	virtual Bitu GetActivityCount(void) {
		return activity|opposite_axis->activity;
	}
	virtual void RepostActivity(void) {
		/* caring for joystick movement into the opposite direction */
		opposite_axis->Active(true);
	}
protected:
	void SetOppositeAxis(CJAxisEvent * _opposite_axis) {
		opposite_axis=_opposite_axis;
	}
	Bitu stick,axis;
	bool positive;
	CJAxisEvent * opposite_axis;
};

class CJButtonEvent : public CTriggeredEvent {
public:
	CJButtonEvent(char const * const _entry,Bitu _stick,Bitu _button) : CTriggeredEvent(_entry) {
		stick=_stick;
		button=_button;
	}
	virtual ~CJButtonEvent() {}
	void Active(bool pressed) {
		virtual_joysticks[stick].button_pressed[button]=pressed;
	}
protected:
	Bitu stick,button;
};

class CJHatEvent : public CTriggeredEvent {
public:
	CJHatEvent(char const * const _entry,Bitu _stick,Bitu _hat,Bitu _dir) : CTriggeredEvent(_entry) {
		stick=_stick;
		hat=_hat;
		dir=_dir;
	}
	virtual ~CJHatEvent() {}
	void Active(bool pressed) {
		virtual_joysticks[stick].hat_pressed[(hat<<2)+dir]=pressed;
	}
protected:
	Bitu stick,hat,dir;
};


class CModEvent : public CTriggeredEvent {
public:
	CModEvent(char const * const _entry,Bitu _wmod) : CTriggeredEvent(_entry) {
		wmod=_wmod;
	}
	virtual ~CModEvent() {}
	void Active(bool yesno) {
		if (yesno) mapper.mods|=(1 << (wmod-1));
		else mapper.mods&=~(1 << (wmod-1));
	};
protected:
	Bitu wmod;
};

class CHandlerEvent : public CTriggeredEvent {
public:
	CHandlerEvent(char const * const _entry,MAPPER_Handler * _handler,MapKeys _key,Bitu _mod,char const * const _buttonname) : CTriggeredEvent(_entry) {
		handler=_handler;
		defmod=_mod;
		defkey=_key;
		buttonname=_buttonname;
		handlergroup.push_back(this);
	}
	virtual ~CHandlerEvent() {}
	void Active(bool yesno) {
		(*handler)(yesno);
	};
	const char * ButtonName(void) {
		return buttonname;
	}
#if defined(C_SDL2)
	void MakeDefaultBind(char * buf) {
		Bitu key=0;
		switch (defkey) {
		case MK_f1:case MK_f2:case MK_f3:case MK_f4:
		case MK_f5:case MK_f6:case MK_f7:case MK_f8:
		case MK_f9:case MK_f10:case MK_f11:case MK_f12:	
			key=SDL_SCANCODE_F1+(defkey-MK_f1);
			break;
		case MK_return:
			key=SDL_SCANCODE_RETURN;
			break;
		case MK_kpminus:
			key=SDL_SCANCODE_KP_MINUS;
			break;
		case MK_equals:
			key=SDL_SCANCODE_EQUALS;
			break;
		case MK_scrolllock:
			key=SDL_SCANCODE_SCROLLLOCK;
			break;
		case MK_pause:
			key=SDL_SCANCODE_PAUSE;
			break;
		case MK_printscreen:
			key=SDL_SCANCODE_PRINTSCREEN;
			break;
		case MK_home: 
			key=SDL_SCANCODE_HOME;
			break;
		case MK_1:
			key=SDL_SCANCODE_1;
			break;
		case MK_2:
			key=SDL_SCANCODE_2;
			break;
		case MK_3:
			key=SDL_SCANCODE_3;
			break;
		case MK_4:
			key=SDL_SCANCODE_4;
			break;
        default:
            break;
		}
		sprintf(buf,"%s \"key %d%s%s%s\"",
			entry,
			(int)key,
			defmod & 1 ? " mod1" : "",
			defmod & 2 ? " mod2" : "",
			defmod & 4 ? " mod3" : ""
		);
	}
#else
	void MakeDefaultBind(char * buf) {
		Bitu key=0;
		switch (defkey) {
		case MK_f1:case MK_f2:case MK_f3:case MK_f4:
		case MK_f5:case MK_f6:case MK_f7:case MK_f8:
		case MK_f9:case MK_f10:case MK_f11:case MK_f12:	
			key=SDLK_F1+(defkey-MK_f1);
			break;
		case MK_return:
			key=SDLK_RETURN;
			break;
		case MK_kpminus:
			key=SDLK_KP_MINUS;
			break;
        case MK_kpplus:
			key=SDLK_KP_PLUS;
			break;
		case MK_equals:
			key=SDLK_EQUALS;
			break;
		case MK_scrolllock:
#if defined(C_SDL2)
			key=SDLK_SCROLLLOCK;
#else
			key=SDLK_SCROLLOCK;
#endif
			break;
		case MK_pause:
			key=SDLK_PAUSE;
			break;
		case MK_printscreen:
#if defined(C_SDL2)
			key=SDLK_PRINTSCREEN;
#else
			key=SDLK_PRINT;
#endif
			break;
		case MK_home: 
			key=SDLK_HOME; 
			break;
		case MK_1:
			key=SDLK_1;
			break;
		case MK_2:
			key=SDLK_2;
			break;
		case MK_3:
			key=SDLK_3;
			break;
		case MK_4:
			key=SDLK_4;
			break;
		}
		sprintf(buf,"%s \"key %d%s%s%s\"",
			entry,
			(int)key,
			defmod & 1 ? " mod1" : "",
			defmod & 2 ? " mod2" : "",
			defmod & 4 ? " mod3" : ""
		);
	}
#endif
protected:
	MapKeys defkey;
	Bitu defmod;
	MAPPER_Handler * handler;
public:
	const char * buttonname;
};


static struct {
	CCaptionButton *  event_title;
	CCaptionButton *  bind_title;
	CCaptionButton *  selected;
	CCaptionButton *  action;
	CCaptionButton *  dbg;
	CBindButton * save;
	CBindButton * exit;   
	CBindButton * cap;
	CBindButton * add;
	CBindButton * del;
	CBindButton * next;
	CCheckButton * mod1,* mod2,* mod3,* hold;
} bind_but;


static void change_action_text(const char* text,Bit8u col) {
	bind_but.action->Change(text,"");
	bind_but.action->SetColor(col);
}


static void SetActiveBind(CBind * _bind) {
	mapper.abind=_bind;
	if (_bind) {
		bind_but.bind_title->Enable(true);
		char buf[256];_bind->BindName(buf);
		bind_but.bind_title->Change("BIND:%s",buf);
		bind_but.del->Enable(true);
		bind_but.next->Enable(true);
		bind_but.mod1->Enable(true);
		bind_but.mod2->Enable(true);
		bind_but.mod3->Enable(true);
		bind_but.hold->Enable(true);
	} else {
		bind_but.bind_title->Enable(false);
		bind_but.del->Enable(false);
		bind_but.next->Enable(false);
		bind_but.mod1->Enable(false);
		bind_but.mod2->Enable(false);
		bind_but.mod3->Enable(false);
		bind_but.hold->Enable(false);
	}
}

static void SetActiveEvent(CEvent * event) {
	mapper.aevent=event;
	mapper.redraw=true;
	mapper.addbind=false;
	bind_but.event_title->Change("EVENT:%s",event ? event->GetName(): "none");
	if (!event) {
		change_action_text("Select an event to change.",CLR_WHITE);
		bind_but.add->Enable(false);
		SetActiveBind(0);
	} else {
		change_action_text("Select a different event or hit the Add/Del/Next buttons.",CLR_WHITE);
		mapper.abindit=event->bindlist.begin();
		if (mapper.abindit!=event->bindlist.end()) {
			SetActiveBind(*(mapper.abindit));
		} else SetActiveBind(0);
		bind_but.add->Enable(true);
	}
}

#if defined(C_SDL2)
extern SDL_Window * GFX_SetSDLSurfaceWindow(Bit16u width, Bit16u height);
extern SDL_Rect GFX_GetSDLSurfaceSubwindowDims(Bit16u width, Bit16u height);
extern void GFX_UpdateDisplayDimensions(int width, int height);
#endif

static void DrawButtons(void) {
	SDL_FillRect(mapper.surface,0,0);
#if !defined(C_SDL2)
	SDL_LockSurface(mapper.surface);
#endif
	for (CButton_it but_it = buttons.begin();but_it!=buttons.end();but_it++) {
		(*but_it)->Draw();
	}
#if defined(C_SDL2)
    // We can't just use SDL_BlitScaled (say for Android) in one step
    SDL_BlitSurface(mapper.draw_surface, NULL, mapper.draw_surface_nonpaletted, NULL);
    SDL_BlitScaled(mapper.draw_surface_nonpaletted, NULL, mapper.surface, &mapper.draw_rect);
//    SDL_BlitSurface(mapper.draw_surface, NULL, mapper.surface, NULL);
    SDL_UpdateWindowSurface(mapper.window);
#else
	SDL_UnlockSurface(mapper.surface);
	SDL_Flip(mapper.surface);
#endif
}

static CKeyEvent * AddKeyButtonEvent(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,char const * const entry,KBD_KEYS key) {
	char buf[64];
	strcpy(buf,"key_");
	strcat(buf,entry);
	CKeyEvent * event=new CKeyEvent(buf,key);
	new CEventButton(x,y,dx,dy,title,event);
	return event;
}

static CJAxisEvent * AddJAxisButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu stick,Bitu axis,bool positive,CJAxisEvent * opposite_axis) {
	char buf[64];
	sprintf(buf,"jaxis_%d_%d%s",(int)stick,(int)axis,positive ? "+" : "-");
	CJAxisEvent	* event=new CJAxisEvent(buf,stick,axis,positive,opposite_axis);
	new CEventButton(x,y,dx,dy,title,event);
	return event;
}
static CJAxisEvent * AddJAxisButton_hidden(Bitu stick,Bitu axis,bool positive,CJAxisEvent * opposite_axis) {
	char buf[64];
	sprintf(buf,"jaxis_%d_%d%s",(int)stick,(int)axis,positive ? "+" : "-");
	return new CJAxisEvent(buf,stick,axis,positive,opposite_axis);
}

static void AddJButtonButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu stick,Bitu button) {
	char buf[64];
	sprintf(buf,"jbutton_%d_%d",(int)stick,(int)button);
	CJButtonEvent * event=new CJButtonEvent(buf,stick,button);
	new CEventButton(x,y,dx,dy,title,event);
}
static void AddJButtonButton_hidden(Bitu stick,Bitu button) {
	char buf[64];
	sprintf(buf,"jbutton_%d_%d",(int)stick,(int)button);
	new CJButtonEvent(buf,stick,button);
}

static void AddJHatButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu _stick,Bitu _hat,Bitu _dir) {
	char buf[64];
	sprintf(buf,"jhat_%d_%d_%d",(int)_stick,(int)_hat,(int)_dir);
	CJHatEvent * event=new CJHatEvent(buf,_stick,_hat,_dir);
	new CEventButton(x,y,dx,dy,title,event);
}

static void AddModButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu _mod) {
	char buf[64];
	sprintf(buf,"mod_%d",(int)_mod);
	CModEvent * event=new CModEvent(buf,_mod);
	new CEventButton(x,y,dx,dy,title,event);
}

struct KeyBlock {
	const char * title;
	const char * entry;
	KBD_KEYS key;
};
static KeyBlock combo_f[12]={
	{"F1","f1",KBD_f1},		{"F2","f2",KBD_f2},		{"F3","f3",KBD_f3},		
	{"F4","f4",KBD_f4},		{"F5","f5",KBD_f5},		{"F6","f6",KBD_f6},
	{"F7","f7",KBD_f7},		{"F8","f8",KBD_f8},		{"F9","f9",KBD_f9},
	{"F10","f10",KBD_f10},	{"F11","f11",KBD_f11},	{"F12","f12",KBD_f12},
};

static KeyBlock combo_1[14]={
	{"`~","grave",KBD_grave},	{"1!","1",KBD_1},	{"2@","2",KBD_2},
	{"3#","3",KBD_3},			{"4$","4",KBD_4},	{"5%","5",KBD_5},
	{"6^","6",KBD_6},			{"7&","7",KBD_7},	{"8*","8",KBD_8},
	{"9(","9",KBD_9},			{"0)","0",KBD_0},	{"-_","minus",KBD_minus},	
	{"=+","equals",KBD_equals},	{"\x1B","bspace",KBD_backspace},
};

static KeyBlock combo_2[12]={
	{"q","q",KBD_q},			{"w","w",KBD_w},	{"e","e",KBD_e},
	{"r","r",KBD_r},			{"t","t",KBD_t},	{"y","y",KBD_y},
	{"u","u",KBD_u},			{"i","i",KBD_i},	{"o","o",KBD_o},	
	{"p","p",KBD_p},			{"[","lbracket",KBD_leftbracket},	
	{"]","rbracket",KBD_rightbracket},	
};

static KeyBlock combo_3[12]={
	{"a","a",KBD_a},			{"s","s",KBD_s},	{"d","d",KBD_d},
	{"f","f",KBD_f},			{"g","g",KBD_g},	{"h","h",KBD_h},
	{"j","j",KBD_j},			{"k","k",KBD_k},	{"l","l",KBD_l},
	{";","semicolon",KBD_semicolon},				{"'","quote",KBD_quote},
	{"\\","backslash",KBD_backslash},	
};

static KeyBlock combo_4[11]={
	{"<","lessthan",KBD_extra_lt_gt},
	{"z","z",KBD_z},			{"x","x",KBD_x},	{"c","c",KBD_c},
	{"v","v",KBD_v},			{"b","b",KBD_b},	{"n","n",KBD_n},
	{"m","m",KBD_m},			{",","comma",KBD_comma},
	{".","period",KBD_period},						{"/","slash",KBD_slash},		
};

static CKeyEvent * caps_lock_event=NULL;
static CKeyEvent * num_lock_event=NULL;

static void CreateLayout(void) {
	Bitu i;
	/* Create the buttons for the Keyboard */
#define BW 28
#define BH 18
#define DX 5
#define PX(_X_) ((_X_)*BW + DX)
#define PY(_Y_) (10+(_Y_)*BH)
	AddKeyButtonEvent(PX(0),PY(0),BW,BH,"ESC","esc",KBD_esc);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(0),BW,BH,combo_f[i].title,combo_f[i].entry,combo_f[i].key);
	for (i=0;i<14;i++) AddKeyButtonEvent(PX(  i),PY(1),BW,BH,combo_1[i].title,combo_1[i].entry,combo_1[i].key);

	AddKeyButtonEvent(PX(0),PY(2),BW*2,BH,"TAB","tab",KBD_tab);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(2),BW,BH,combo_2[i].title,combo_2[i].entry,combo_2[i].key);

	AddKeyButtonEvent(PX(14),PY(2),BW*2,BH*2,"ENTER","enter",KBD_enter);
	
	caps_lock_event=AddKeyButtonEvent(PX(0),PY(3),BW*2,BH,"CLCK","capslock",KBD_capslock);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(3),BW,BH,combo_3[i].title,combo_3[i].entry,combo_3[i].key);

	AddKeyButtonEvent(PX(0),PY(4),BW*2,BH,"SHIFT","lshift",KBD_leftshift);
	for (i=0;i<11;i++) AddKeyButtonEvent(PX(2+i),PY(4),BW,BH,combo_4[i].title,combo_4[i].entry,combo_4[i].key);
	AddKeyButtonEvent(PX(13),PY(4),BW*3,BH,"SHIFT","rshift",KBD_rightshift);

	/* Last Row */
	AddKeyButtonEvent(PX(0) ,PY(5),BW*2,BH,"CTRL","lctrl",KBD_leftctrl);
	AddKeyButtonEvent(PX(2) ,PY(5),BW*1,BH,"WIN","lwindows",KBD_lwindows);
	AddKeyButtonEvent(PX(3) ,PY(5),BW*1,BH,"ALT","lalt",KBD_leftalt);
	AddKeyButtonEvent(PX(4) ,PY(5),BW*7,BH,"SPACE","space",KBD_space);
	AddKeyButtonEvent(PX(11),PY(5),BW*1,BH,"ALT","ralt",KBD_rightalt);
	AddKeyButtonEvent(PX(12),PY(5),BW*1,BH,"WIN","rwindows",KBD_rwindows);
	AddKeyButtonEvent(PX(13),PY(5),BW*1,BH,"WMN","rwinmenu",KBD_rwinmenu);
	AddKeyButtonEvent(PX(14),PY(5),BW*2,BH,"CTRL","rctrl",KBD_rightctrl);

	/* Arrow Keys */
#define XO 17
#define YO 0

	AddKeyButtonEvent(PX(XO+0),PY(YO),BW,BH,"PRT","printscreen",KBD_printscreen);
	AddKeyButtonEvent(PX(XO+1),PY(YO),BW,BH,"SCL","scrolllock",KBD_scrolllock);
	AddKeyButtonEvent(PX(XO+2),PY(YO),BW,BH,"PAU","pause",KBD_pause);
	AddKeyButtonEvent(PX(XO+3),PY(YO),BW,BH,"NEQ","kp_equals",KBD_kpequals);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"INS","insert",KBD_insert);
	AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"HOM","home",KBD_home);
	AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"PUP","pageup",KBD_pageup);
	AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW,BH,"DEL","delete",KBD_delete);
	AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"END","end",KBD_end);
	AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"PDN","pagedown",KBD_pagedown);
	AddKeyButtonEvent(PX(XO+1),PY(YO+4),BW,BH,"\x18","up",KBD_up);
	AddKeyButtonEvent(PX(XO+0),PY(YO+5),BW,BH,"\x1B","left",KBD_left);
	AddKeyButtonEvent(PX(XO+1),PY(YO+5),BW,BH,"\x19","down",KBD_down);
	AddKeyButtonEvent(PX(XO+2),PY(YO+5),BW,BH,"\x1A","right",KBD_right);
#undef XO
#undef YO
#define XO 0
#define YO 7
	/* Numeric KeyPad */
	num_lock_event=AddKeyButtonEvent(PX(XO),PY(YO),BW,BH,"NUM","numlock",KBD_numlock);
	AddKeyButtonEvent(PX(XO+1),PY(YO),BW,BH,"/","kp_divide",KBD_kpdivide);
	AddKeyButtonEvent(PX(XO+2),PY(YO),BW,BH,"*","kp_multiply",KBD_kpmultiply);
	AddKeyButtonEvent(PX(XO+3),PY(YO),BW,BH,"-","kp_minus",KBD_kpminus);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"7","kp_7",KBD_kp7);
	AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"8","kp_8",KBD_kp8);
	AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"9","kp_9",KBD_kp9);
	AddKeyButtonEvent(PX(XO+3),PY(YO+1),BW,BH*2,"+","kp_plus",KBD_kpplus);
	AddKeyButtonEvent(PX(XO),PY(YO+2),BW,BH,"4","kp_4",KBD_kp4);
	AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"5","kp_5",KBD_kp5);
	AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"6","kp_6",KBD_kp6);
	AddKeyButtonEvent(PX(XO+0),PY(YO+3),BW,BH,"1","kp_1",KBD_kp1);
	AddKeyButtonEvent(PX(XO+1),PY(YO+3),BW,BH,"2","kp_2",KBD_kp2);
	AddKeyButtonEvent(PX(XO+2),PY(YO+3),BW,BH,"3","kp_3",KBD_kp3);
	AddKeyButtonEvent(PX(XO+3),PY(YO+3),BW,BH*2,"ENT","kp_enter",KBD_kpenter);
	AddKeyButtonEvent(PX(XO),PY(YO+4),BW*2,BH,"0","kp_0",KBD_kp0);
	AddKeyButtonEvent(PX(XO+2),PY(YO+4),BW,BH,".","kp_period",KBD_kpperiod);
#undef XO
#undef YO
#define XO 5
#define YO 7
	/* F13-F24 block */
	AddKeyButtonEvent(PX(XO+0),PY(YO+0),BW,BH,"F13","f13",KBD_f13);
	AddKeyButtonEvent(PX(XO+1),PY(YO+0),BW,BH,"F14","f14",KBD_f14);
	AddKeyButtonEvent(PX(XO+2),PY(YO+0),BW,BH,"F15","f15",KBD_f15);
	AddKeyButtonEvent(PX(XO+3),PY(YO+0),BW,BH,"F16","f16",KBD_f16);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"F17","f17",KBD_f17);
	AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"F18","f18",KBD_f18);
	AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"F19","f19",KBD_f19);
	AddKeyButtonEvent(PX(XO+3),PY(YO+1),BW,BH,"F20","f20",KBD_f20);
	AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW,BH,"F21","f21",KBD_f21);
	AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"F22","f22",KBD_f22);
	AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"F23","f23",KBD_f23);
	AddKeyButtonEvent(PX(XO+3),PY(YO+2),BW,BH,"F24","f24",KBD_f24);
#undef XO
#undef YO
#define XO 0
#define YO 13
	/* Japanese keys */
	AddKeyButtonEvent(PX(XO+0),PY(YO+0),BW*3,BH,"HANKAKU", "jp_hankaku", KBD_jp_hankaku);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW*3,BH,"MUHENKAN","jp_muhenkan",KBD_jp_muhenkan);
	AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW*3,BH,"HENKAN",  "jp_henkan",  KBD_jp_henkan);
	AddKeyButtonEvent(PX(XO+3),PY(YO+0),BW*3,BH,"HIRAGANA","jp_hiragana",KBD_jp_hiragana);
	AddKeyButtonEvent(PX(XO+6),PY(YO+0),BW*1,BH,"YEN",     "jp_yen",     KBD_jp_yen);
	AddKeyButtonEvent(PX(XO+6),PY(YO+1),BW*1,BH,"\\",      "jp_bckslash",KBD_jp_backslash);
	/* Korean */
	AddKeyButtonEvent(PX(XO+3),PY(YO+1),BW*3,BH,"HANCHA",  "kor_hancha", KBD_kor_hancha);
	AddKeyButtonEvent(PX(XO+3),PY(YO+2),BW*3,BH,"HANYONG", "kor_hanyong",KBD_kor_hanyong);
#undef XO
#undef YO
#define XO 10
#define YO 8
	/* Joystick Buttons/Texts */
	/* Buttons 1+2 of 1st Joystick */
	AddJButtonButton(PX(XO),PY(YO),BW,BH,"1" ,0,0);
	AddJButtonButton(PX(XO+2),PY(YO),BW,BH,"2" ,0,1);
	/* Axes 1+2 (X+Y) of 1st Joystick */
	CJAxisEvent * cjaxis=AddJAxisButton(PX(XO+1),PY(YO),BW,BH,"Y-",0,1,false,NULL);
	AddJAxisButton  (PX(XO+1),PY(YO+1),BW,BH,"Y+",0,1,true,cjaxis);
	cjaxis=AddJAxisButton  (PX(XO),PY(YO+1),BW,BH,"X-",0,0,false,NULL);
	AddJAxisButton  (PX(XO+2),PY(YO+1),BW,BH,"X+",0,0,true,cjaxis);

	if (joytype==JOY_2AXIS) {
		/* Buttons 1+2 of 2nd Joystick */
		AddJButtonButton(PX(XO+4),PY(YO),BW,BH,"1" ,1,0);
		AddJButtonButton(PX(XO+4+2),PY(YO),BW,BH,"2" ,1,1);
		/* Buttons 3+4 of 1st Joystick, not accessible */
		AddJButtonButton_hidden(0,2);
		AddJButtonButton_hidden(0,3);

		/* Axes 1+2 (X+Y) of 2nd Joystick */
		cjaxis=	AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",1,0,false,NULL);
				AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",1,0,true,cjaxis);
		cjaxis=	AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",1,1,false,NULL);
				AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",1,1,true,cjaxis);
		/* Axes 3+4 (X+Y) of 1st Joystick, not accessible */
		cjaxis=	AddJAxisButton_hidden(0,2,false,NULL);
				AddJAxisButton_hidden(0,2,true,cjaxis);
		cjaxis=	AddJAxisButton_hidden(0,3,false,NULL);
				AddJAxisButton_hidden(0,3,true,cjaxis);
	} else {
		/* Buttons 3+4 of 1st Joystick */
		AddJButtonButton(PX(XO+4),PY(YO),BW,BH,"3" ,0,2);
		AddJButtonButton(PX(XO+4+2),PY(YO),BW,BH,"4" ,0,3);
		/* Buttons 1+2 of 2nd Joystick, not accessible */
		AddJButtonButton_hidden(1,0);
		AddJButtonButton_hidden(1,1);

		/* Axes 3+4 (X+Y) of 1st Joystick */
		cjaxis=	AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",0,2,false,NULL);
				AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",0,2,true,cjaxis);
		cjaxis=	AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",0,3,false,NULL);
				AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",0,3,true,cjaxis);
		/* Axes 1+2 (X+Y) of 2nd Joystick , not accessible*/
		cjaxis=	AddJAxisButton_hidden(1,0,false,NULL);
				AddJAxisButton_hidden(1,0,true,cjaxis);
		cjaxis=	AddJAxisButton_hidden(1,1,false,NULL);
				AddJAxisButton_hidden(1,1,true,cjaxis);
	}

	if (joytype==JOY_CH) {
		/* Buttons 5+6 of 1st Joystick */
		AddJButtonButton(PX(XO+8),PY(YO),BW,BH,"5" ,0,4);
		AddJButtonButton(PX(XO+8+2),PY(YO),BW,BH,"6" ,0,5);
	} else {
		/* Buttons 5+6 of 1st Joystick, not accessible */
		AddJButtonButton_hidden(0,4);
		AddJButtonButton_hidden(0,5);
	}

	/* Hat directions up, left, down, right */
	AddJHatButton(PX(XO+8+1),PY(YO),BW,BH,"UP",0,0,0);
	AddJHatButton(PX(XO+8+0),PY(YO+1),BW,BH,"LFT",0,0,3);
	AddJHatButton(PX(XO+8+1),PY(YO+1),BW,BH,"DWN",0,0,2);
	AddJHatButton(PX(XO+8+2),PY(YO+1),BW,BH,"RGT",0,0,1);

	/* Labels for the joystick */
	if (joytype ==JOY_2AXIS) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Joystick 1");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Joystick 2");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
	} else if(joytype ==JOY_4AXIS || joytype == JOY_4AXIS_2) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3/4");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
	} else if(joytype == JOY_CH) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3/4");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Hat/D-pad");
	} else if ( joytype==JOY_FCS) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Hat/D-pad");
	} else if(joytype == JOY_NONE) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Disabled");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Disabled");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
	}
   
   
   
	/* The modifier buttons */
	AddModButton(PX(0),PY(17),50,20,"Mod1",1);
	AddModButton(PX(2),PY(17),50,20,"Mod2",2);
	AddModButton(PX(4),PY(17),50,20,"Mod3",3);
	/* Create Handler buttons */
	Bitu xpos=3;Bitu ypos=11;
	for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();hit++) {
		new CEventButton(PX(xpos*3),PY(ypos),BW*3,BH,(*hit)->ButtonName(),(*hit));
		xpos++;
		if (xpos>6) {
			xpos=3;ypos++;
		}
	}
    next_handler_xpos = xpos;
    next_handler_ypos = ypos;
	/* Create some text buttons */
//	new CTextButton(PX(6),0,124,20,"Keyboard Layout");
//	new CTextButton(PX(17),0,124,20,"Joystick Layout");

	bind_but.action=new CCaptionButton(180,420,0,0);

	bind_but.event_title=new CCaptionButton(0,350,0,0);
	bind_but.bind_title=new CCaptionButton(0,365,0,0);

	/* Create binding support buttons */
	
	bind_but.mod1=new CCheckButton(20,410,60,20, "mod1",BC_Mod1);
	bind_but.mod2=new CCheckButton(20,432,60,20, "mod2",BC_Mod2);
	bind_but.mod3=new CCheckButton(20,454,60,20, "mod3",BC_Mod3);
	bind_but.hold=new CCheckButton(100,410,60,20,"hold",BC_Hold);

	bind_but.next=new CBindButton(250,400,50,20,"Next",BB_Next);

	bind_but.add=new CBindButton(250,380,50,20,"Add",BB_Add);
	bind_but.del=new CBindButton(300,380,50,20,"Del",BB_Del);

	bind_but.save=new CBindButton(400,440,50,20,"Save",BB_Save);
	bind_but.exit=new CBindButton(450,440,50,20,"Exit",BB_Exit);
	bind_but.cap=new CBindButton(500,440,50,20,"Capt",BB_Capture);

	bind_but.dbg=new CCaptionButton(300,460,340,20); // right below the Save button
	bind_but.dbg->Change("(event debug)");

	bind_but.bind_title->Change("Bind Title");

    mapper_addhandler_create_buttons = true;
}

static SDL_Color map_pal[5]={
	{0x00,0x00,0x00,0x00},			//0=black
	{0xff,0xff,0xff,0x00},			//1=white
	{0xff,0x00,0x00,0x00},			//2=red
	{0x10,0x30,0xff,0x00},			//3=blue
	{0x00,0xff,0x20,0x00}			//4=green
};

static void CreateStringBind(char * line,bool loading=false) {
	line=trim(line);
	char * eventname=StripWord(line);
	CEvent * event;
	for (CEventVector_it ev_it=events.begin();ev_it!=events.end();ev_it++) {
		if (!strcasecmp((*ev_it)->GetName(),eventname)) {
			event=*ev_it;
			goto foundevent;
		}
	}

    if (loading) {
        /* NTS: StripWord() updates line pointer after ASCIIZ snipping the event name */
        pending_string_binds[eventname] = line; /* perhaps code will register it later (i.e. Herc pal change) */
    	LOG(LOG_MISC,LOG_WARN)("Can't find matching event for %s = %s yet. It may exist later when registered elsewhere in this emulator.",eventname,line);
    }
    else {
    	LOG(LOG_MISC,LOG_WARN)("Can't find matching event for %s",eventname);
    }

	return ;
foundevent:
	CBind * bind;
	for (char * bindline=StripWord(line);*bindline;bindline=StripWord(line)) {
		for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
			bind=(*it)->CreateConfigBind(bindline);
			if (bind) {
				event->AddBind(bind);
				bind->SetFlags(bindline);
				break;
			}
		}
	}
}

#if defined(C_SDL2)

static struct {
	const char * eventend;
	Bitu key;
} DefaultKeys[]={

	{"f1",SDL_SCANCODE_F1},		{"f2",SDL_SCANCODE_F2},		{"f3",SDL_SCANCODE_F3},		{"f4",SDL_SCANCODE_F4},
	{"f5",SDL_SCANCODE_F5},		{"f6",SDL_SCANCODE_F6},		{"f7",SDL_SCANCODE_F7},		{"f8",SDL_SCANCODE_F8},
	{"f9",SDL_SCANCODE_F9},		{"f10",SDL_SCANCODE_F10},	{"f11",SDL_SCANCODE_F11},	{"f12",SDL_SCANCODE_F12},

	{"1",SDL_SCANCODE_1},		{"2",SDL_SCANCODE_2},		{"3",SDL_SCANCODE_3},		{"4",SDL_SCANCODE_4},
	{"5",SDL_SCANCODE_5},		{"6",SDL_SCANCODE_6},		{"7",SDL_SCANCODE_7},		{"8",SDL_SCANCODE_8},
	{"9",SDL_SCANCODE_9},		{"0",SDL_SCANCODE_0},

	{"a",SDL_SCANCODE_A},		{"b",SDL_SCANCODE_B},		{"c",SDL_SCANCODE_C},		{"d",SDL_SCANCODE_D},
	{"e",SDL_SCANCODE_E},		{"f",SDL_SCANCODE_F},		{"g",SDL_SCANCODE_G},		{"h",SDL_SCANCODE_H},
	{"i",SDL_SCANCODE_I},		{"j",SDL_SCANCODE_J},		{"k",SDL_SCANCODE_K},		{"l",SDL_SCANCODE_L},
	{"m",SDL_SCANCODE_M},		{"n",SDL_SCANCODE_N},		{"o",SDL_SCANCODE_O},		{"p",SDL_SCANCODE_P},
	{"q",SDL_SCANCODE_Q},		{"r",SDL_SCANCODE_R},		{"s",SDL_SCANCODE_S},		{"t",SDL_SCANCODE_T},
	{"u",SDL_SCANCODE_U},		{"v",SDL_SCANCODE_V},		{"w",SDL_SCANCODE_W},		{"x",SDL_SCANCODE_X},
	{"y",SDL_SCANCODE_Y},		{"z",SDL_SCANCODE_Z},		{"space",SDL_SCANCODE_SPACE},
	{"esc",SDL_SCANCODE_ESCAPE},	{"equals",SDL_SCANCODE_EQUALS},		{"grave",SDL_SCANCODE_GRAVE},
	{"tab",SDL_SCANCODE_TAB},		{"enter",SDL_SCANCODE_RETURN},		{"bspace",SDL_SCANCODE_BACKSPACE},
	{"lbracket",SDL_SCANCODE_LEFTBRACKET},						{"rbracket",SDL_SCANCODE_RIGHTBRACKET},
	{"minus",SDL_SCANCODE_MINUS},	{"capslock",SDL_SCANCODE_CAPSLOCK},	{"semicolon",SDL_SCANCODE_SEMICOLON},
	{"quote", SDL_SCANCODE_APOSTROPHE},	{"backslash",SDL_SCANCODE_BACKSLASH},	{"lshift",SDL_SCANCODE_LSHIFT},
	{"rshift",SDL_SCANCODE_RSHIFT},	{"lalt",SDL_SCANCODE_LALT},			{"ralt",SDL_SCANCODE_RALT},
	{"lctrl",SDL_SCANCODE_LCTRL},	{"rctrl",SDL_SCANCODE_RCTRL},		{"comma",SDL_SCANCODE_COMMA},
	{"period",SDL_SCANCODE_PERIOD},	{"slash",SDL_SCANCODE_SLASH},		{"printscreen",SDL_SCANCODE_PRINTSCREEN},
	{"scrolllock",SDL_SCANCODE_SCROLLLOCK},	{"pause",SDL_SCANCODE_PAUSE},		{"pagedown",SDL_SCANCODE_PAGEDOWN},
	{"pageup",SDL_SCANCODE_PAGEUP},	{"insert",SDL_SCANCODE_INSERT},		{"home",SDL_SCANCODE_HOME},
	{"delete",SDL_SCANCODE_DELETE},	{"end",SDL_SCANCODE_END},			{"up",SDL_SCANCODE_UP},
	{"left",SDL_SCANCODE_LEFT},		{"down",SDL_SCANCODE_DOWN},			{"right",SDL_SCANCODE_RIGHT},
	{"kp_0",SDL_SCANCODE_KP_0},	{"kp_1",SDL_SCANCODE_KP_1},	{"kp_2",SDL_SCANCODE_KP_2},	{"kp_3",SDL_SCANCODE_KP_3},
	{"kp_4",SDL_SCANCODE_KP_4},	{"kp_5",SDL_SCANCODE_KP_5},	{"kp_6",SDL_SCANCODE_KP_6},	{"kp_7",SDL_SCANCODE_KP_7},
	{"kp_8",SDL_SCANCODE_KP_8},	{"kp_9",SDL_SCANCODE_KP_9},	{"numlock",SDL_SCANCODE_NUMLOCKCLEAR},
	{"kp_divide",SDL_SCANCODE_KP_DIVIDE},	{"kp_multiply",SDL_SCANCODE_KP_MULTIPLY},
	{"kp_minus",SDL_SCANCODE_KP_MINUS},		{"kp_plus",SDL_SCANCODE_KP_PLUS},
	{"kp_period",SDL_SCANCODE_KP_PERIOD},	{"kp_enter",SDL_SCANCODE_KP_ENTER},

	/* Is that the extra backslash key ("less than" key) */
	/* on some keyboards with the 102-keys layout??      */
	{"lessthan",SDL_SCANCODE_NONUSBACKSLASH},
	{0,0}
};

#else

static struct {
	const char * eventend;
	Bitu key;
} DefaultKeys[]={
	{"f1",SDLK_F1},		{"f2",SDLK_F2},		{"f3",SDLK_F3},		{"f4",SDLK_F4},
	{"f5",SDLK_F5},		{"f6",SDLK_F6},		{"f7",SDLK_F7},		{"f8",SDLK_F8},
	{"f9",SDLK_F9},		{"f10",SDLK_F10},	{"f11",SDLK_F11},	{"f12",SDLK_F12},
	{"f13",SDLK_F13},	{"f14",SDLK_F14},	{"f15",SDLK_F15},

	{"1",SDLK_1},		{"2",SDLK_2},		{"3",SDLK_3},		{"4",SDLK_4},
	{"5",SDLK_5},		{"6",SDLK_6},		{"7",SDLK_7},		{"8",SDLK_8},
	{"9",SDLK_9},		{"0",SDLK_0},

	{"a",SDLK_a},		{"b",SDLK_b},		{"c",SDLK_c},		{"d",SDLK_d},
	{"e",SDLK_e},		{"f",SDLK_f},		{"g",SDLK_g},		{"h",SDLK_h},
	{"i",SDLK_i},		{"j",SDLK_j},		{"k",SDLK_k},		{"l",SDLK_l},
	{"m",SDLK_m},		{"n",SDLK_n},		{"o",SDLK_o},		{"p",SDLK_p},
	{"q",SDLK_q},		{"r",SDLK_r},		{"s",SDLK_s},		{"t",SDLK_t},
	{"u",SDLK_u},		{"v",SDLK_v},		{"w",SDLK_w},		{"x",SDLK_x},
	{"y",SDLK_y},		{"z",SDLK_z},		{"space",SDLK_SPACE},
	{"esc",SDLK_ESCAPE},	{"equals",SDLK_EQUALS},		{"grave",SDLK_BACKQUOTE},
	{"tab",SDLK_TAB},		{"enter",SDLK_RETURN},		{"bspace",SDLK_BACKSPACE},
	{"lbracket",SDLK_LEFTBRACKET},						{"rbracket",SDLK_RIGHTBRACKET},
	{"minus",SDLK_MINUS},	{"capslock",SDLK_CAPSLOCK},	{"semicolon",SDLK_SEMICOLON},
	{"quote", SDLK_QUOTE},	{"backslash",SDLK_BACKSLASH},	{"lshift",SDLK_LSHIFT},
	{"rshift",SDLK_RSHIFT},	{"lalt",SDLK_LALT},			{"ralt",SDLK_RALT},
	{"lctrl",SDLK_LCTRL},	{"rctrl",SDLK_RCTRL},		{"comma",SDLK_COMMA},
	{"period",SDLK_PERIOD},	{"slash",SDLK_SLASH},

#if defined(C_SDL2)
    {"printscreen",SDLK_PRINTSCREEN},
    {"scrolllock",SDLK_SCROLLLOCK},
#else
    {"printscreen",SDLK_PRINT},
    {"scrolllock",SDLK_SCROLLOCK},
#endif

    {"pause",SDLK_PAUSE},		{"pagedown",SDLK_PAGEDOWN},
	{"pageup",SDLK_PAGEUP},	{"insert",SDLK_INSERT},		{"home",SDLK_HOME},
	{"delete",SDLK_DELETE},	{"end",SDLK_END},			{"up",SDLK_UP},
	{"left",SDLK_LEFT},		{"down",SDLK_DOWN},			{"right",SDLK_RIGHT},

#if defined(C_SDL2)
	{"kp_0",SDLK_KP_0},	{"kp_1",SDLK_KP_1},	{"kp_2",SDLK_KP_2},	{"kp_3",SDLK_KP_3},
	{"kp_4",SDLK_KP_4},	{"kp_5",SDLK_KP_5},	{"kp_6",SDLK_KP_6},	{"kp_7",SDLK_KP_7},
	{"kp_8",SDLK_KP_8},	{"kp_9",SDLK_KP_9},
    {"numlock",SDLK_NUMLOCKCLEAR},
#else
	{"kp_0",SDLK_KP0},	{"kp_1",SDLK_KP1},	{"kp_2",SDLK_KP2},	{"kp_3",SDLK_KP3},
	{"kp_4",SDLK_KP4},	{"kp_5",SDLK_KP5},	{"kp_6",SDLK_KP6},	{"kp_7",SDLK_KP7},
	{"kp_8",SDLK_KP8},	{"kp_9",SDLK_KP9},
    {"numlock",SDLK_NUMLOCK},
#endif

	{"kp_divide",SDLK_KP_DIVIDE},	{"kp_multiply",SDLK_KP_MULTIPLY},
	{"kp_minus",SDLK_KP_MINUS},		{"kp_plus",SDLK_KP_PLUS},
	{"kp_period",SDLK_KP_PERIOD},	{"kp_enter",SDLK_KP_ENTER},

	/* NTS: IBM PC keyboards as far as I know do not have numeric keypad equals sign.
	 *      This default assignment should allow Apple Mac users (who's keyboards DO have one)
	 *      to use theirs as a normal equals sign. */
	{"kp_equals",SDLK_KP_EQUALS},

#if defined(C_SDL2)
    // TODO??
#else
	/* Windows 95 keyboard stuff */
	{"lwindows",SDLK_LSUPER},
	{"rwindows",SDLK_RSUPER},
#endif
	{"rwinmenu",SDLK_MENU},

#if defined (MACOSX)
	/* Intl Mac keyboards in US layout actually put U+00A7 SECTION SIGN here */
	{"lessthan",SDLK_WORLD_0},
#else
	{"lessthan",SDLK_LESS},
#endif

#if defined(C_SDL2)
    // TODO??
#else
	/* hack for Japanese keyboards with \ and _ */
	{"jp_bckslash",SDLK_WORLD_10},	// FIXME: Apparently there's a name length limit in the mapper?
	/* hack for Japanese keyboards with Yen and | */
	{"jp_yen",SDLK_WORLD_11 },
	/* more */
	{"jp_hankaku", SDLK_WORLD_12 },
	{"jp_muhenkan", SDLK_WORLD_13 },
	{"jp_henkan", SDLK_WORLD_14 },
	{"jp_hiragana", SDLK_WORLD_15 },
#endif

	{0,0}
};

#endif

static void CreateDefaultBinds(void) {
	char buffer[512];
	Bitu i=0;
	while (DefaultKeys[i].eventend) {
		sprintf(buffer,"key_%s \"key %d\"",DefaultKeys[i].eventend,(int)DefaultKeys[i].key);
		CreateStringBind(buffer);
		i++;
	}

#if defined(C_SDL2)
	sprintf(buffer,"mod_1 \"key %d\"",SDL_SCANCODE_RCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_1 \"key %d\"",SDL_SCANCODE_LCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDL_SCANCODE_RALT);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDL_SCANCODE_LALT);CreateStringBind(buffer);
#else
	sprintf(buffer,"mod_1 \"key %d\"",SDLK_RCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_1 \"key %d\"",SDLK_LCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDLK_RALT);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDLK_LALT);CreateStringBind(buffer);
#endif

	for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();hit++) {
		(*hit)->MakeDefaultBind(buffer);
		CreateStringBind(buffer);
	}

	/* joystick1, buttons 1-6 */
	sprintf(buffer,"jbutton_0_0 \"stick_0 button 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_1 \"stick_0 button 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_2 \"stick_0 button 2\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_3 \"stick_0 button 3\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_4 \"stick_0 button 4\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_5 \"stick_0 button 5\" ");CreateStringBind(buffer);
	/* joystick2, buttons 1-2 */
	sprintf(buffer,"jbutton_1_0 \"stick_1 button 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_1_1 \"stick_1 button 1\" ");CreateStringBind(buffer);

	/* joystick1, axes 1-4 */
	sprintf(buffer,"jaxis_0_0- \"stick_0 axis 0 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_0+ \"stick_0 axis 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_1- \"stick_0 axis 1 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_1+ \"stick_0 axis 1 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_2- \"stick_0 axis 2 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_2+ \"stick_0 axis 2 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_3- \"stick_0 axis 3 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_3+ \"stick_0 axis 3 1\" ");CreateStringBind(buffer);
	/* joystick2, axes 1-2 */
	sprintf(buffer,"jaxis_1_0- \"stick_1 axis 0 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_0+ \"stick_1 axis 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_1- \"stick_1 axis 1 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_1+ \"stick_1 axis 1 1\" ");CreateStringBind(buffer);

	/* joystick1, hat */
	sprintf(buffer,"jhat_0_0_0 \"stick_0 hat 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_1 \"stick_0 hat 0 2\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_2 \"stick_0 hat 0 4\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_3 \"stick_0 hat 0 8\" ");CreateStringBind(buffer);
}

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname) {
	//Check if it already exists=> if so return.
	for(CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();it++)
		if(strcmp((*it)->buttonname,buttonname) == 0) return;

	char tempname[27];
	strcpy(tempname,"hand_");
	strcat(tempname,eventname);
	CHandlerEvent *event = new CHandlerEvent(tempname,handler,key,mods,buttonname);

    if (mapper_addhandler_create_buttons) {
        // and a button in the mapper UI
        {
            new CEventButton(PX(next_handler_xpos*3),PY(next_handler_ypos),BW*3,BH,buttonname,event);
            next_handler_xpos++;
            if (next_handler_xpos>6) {
                next_handler_xpos=3;next_handler_ypos++;
            }
        }

        // this event may have appeared in the user's mapper file, and been ignored.
        // now is the time to register it.
        {
            auto i = pending_string_binds.find(tempname);
            char tmp[512];

            if (i != pending_string_binds.end()) {
                LOG(LOG_MISC,LOG_WARN)("Found pending event for %s from user's file, applying now",tempname);

                snprintf(tmp,sizeof(tmp),"%s %s",tempname,i->second.c_str());

                CreateStringBind(tmp);

                pending_string_binds.erase(i);
            }
            else {
                /* use default binding.
                 * redundant? Yes! But, apparently necessary. */
                event->MakeDefaultBind(tmp);
                CreateStringBind(tmp);
            }
        }
    }

	return ;
}

static void MAPPER_SaveBinds(void) {
	FILE * savefile=fopen(mapper.filename.c_str(),"wt+");
	if (!savefile) {
		LOG_MSG("Can't open %s for saving the mappings",mapper.filename.c_str());
		return;
	}
	char buf[128];
	for (CEventVector_it event_it=events.begin();event_it!=events.end();event_it++) {
		CEvent * event=*(event_it);
		fprintf(savefile,"%s ",event->GetName());
		for (CBindList_it bind_it=event->bindlist.begin();bind_it!=event->bindlist.end();bind_it++) {
			CBind * bind=*(bind_it);
			bind->ConfigName(buf);
			bind->AddFlags(buf);
			fprintf(savefile,"\"%s\" ",buf);
		}
		fprintf(savefile,"\n");
	}
	fclose(savefile);
	change_action_text("Mapper file saved.",CLR_WHITE);
}

static bool MAPPER_LoadBinds(void) {
	FILE * loadfile=fopen(mapper.filename.c_str(),"rt");
	if (!loadfile) return false;
	char linein[512];
	while (fgets(linein,512,loadfile)) {
		CreateStringBind(linein,/*loading*/true);
	}
	fclose(loadfile);
	LOG(LOG_MISC,LOG_NORMAL)("MAPPER: Loading mapper settings from %s", mapper.filename.c_str());
	return true;
}

bool log_keyboard_scan_codes = false;

void MAPPER_CheckEvent(SDL_Event * event) {
	for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
		if ((*it)->CheckEvent(event)) return;
	}

	if (log_keyboard_scan_codes) {
		if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
			LOG_MSG("MAPPER: SDL keyboard event (%s): scancode=0x%X sym=0x%X mod=0x%X",
				event->type == SDL_KEYDOWN?"down":"up",
				event->key.keysym.scancode,event->key.keysym.sym,event->key.keysym.mod);
	}
}

void Mapper_MouseInputEvent(SDL_Event &event) {
    /* Check the press */
    for (CButton_it but_it = buttons.begin();but_it!=buttons.end();but_it++) {
        if ((*but_it)->OnTop(event.button.x,event.button.y)) {
            (*but_it)->Click();
        }
    }
}

#if defined(C_SDL2)
void Mapper_FingerInputEvent(SDL_Event &event) {
    SDL_Event ev;

    memset(&ev,0,sizeof(ev));
    ev.type = SDL_MOUSEBUTTONUP;

#if defined(WIN32)
	/* NTS: Windows versions of SDL2 do normalize the coordinates */
	ev.button.x = (Sint32)(event.tfinger.x * mapper.surface->w);
	ev.button.y = (Sint32)(event.tfinger.y * mapper.surface->h);
#else
	/* NTS: Linux versions of SDL2 don't normalize the coordinates? */
    ev.button.x = event.tfinger.x;     /* Contrary to SDL_events.h the x/y coordinates are NOT normalized to 0...1 */
    ev.button.y = event.tfinger.y;     /* Contrary to SDL_events.h the x/y coordinates are NOT normalized to 0...1 */
#endif

    Mapper_MouseInputEvent(ev);
}
#endif

void BIND_MappingEvents(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
#if defined(C_SDL2)
        case SDL_FINGERUP:
            Mapper_FingerInputEvent(event);
            break;
#endif
    	case SDL_MOUSEBUTTONUP:
#if defined(C_SDL2)
            if (event.button.which != SDL_TOUCH_MOUSEID) /* don't handle mouse events faked by touchscreen */
                Mapper_MouseInputEvent(event);
#else
            Mapper_MouseInputEvent(event);
#endif
			break;
		case SDL_QUIT:
			mapper.exit=true;
			break;
		case SDL_KEYDOWN: /* help the user determine keyboard problems by showing keyboard event, scan code, etc. */
		case SDL_KEYUP:
			{
				static int event_count = 0;
#if defined(C_SDL2)
                SDL_Keysym &s = event.key.keysym;
#else
                SDL_keysym &s = event.key.keysym;
#endif
                char tmp[256];

				// ESC is your magic key out of capture
				if (s.sym == SDLK_ESCAPE && mouselocked) GFX_CaptureMouse();

#if defined(C_SDL2)
                sprintf(tmp,"%c%02x: scan=%u sym=%u mod=%xh",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    s.scancode,
                    s.sym,
                    s.mod);
#else
                sprintf(tmp,"%c%02x: scan=%u sym=%u mod=%xh u=%xh",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    s.scancode,
                    s.sym,
                    s.mod,
                    s.unicode);
#endif

				LOG(LOG_GUI,LOG_DEBUG)("Mapper keyboard event: %s",tmp);
				bind_but.dbg->Change(tmp);
				event_count++;
			}
			/* fall through to mapper UI processing */
		default:
			if (mapper.addbind) for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
				CBind * newbind=(*it)->CreateEventBind(&event);
				if (!newbind) continue;
				mapper.aevent->AddBind(newbind);
				SetActiveEvent(mapper.aevent);
				mapper.addbind=false;
				break;
			}
		}
	}
}

static void InitializeJoysticks(void) {
	mapper.sticks.num=0;
	mapper.sticks.num_groups=0;
	if (joytype != JOY_NONE) {
		mapper.sticks.num=SDL_NumJoysticks();
		LOG(LOG_MISC,LOG_DEBUG)("Joystick type != none, SDL reports %u sticks",(unsigned int)mapper.sticks.num);
		if (joytype==JOY_AUTO) {
			// try to figure out what joystick type to select
			// depending on the number of physically attached joysticks
			if (mapper.sticks.num>1) {
				// more than one joystick present; if all are acceptable use 2axis
				// to allow emulation of two joysticks
				bool first_usable=false;
				SDL_Joystick* tmp_stick1=SDL_JoystickOpen(0);
				if (tmp_stick1) {
					if ((SDL_JoystickNumAxes(tmp_stick1)>1) || (SDL_JoystickNumButtons(tmp_stick1)>0)) {
						first_usable=true;
					}
					SDL_JoystickClose(tmp_stick1);
				}
				bool second_usable=false;
				SDL_Joystick* tmp_stick2=SDL_JoystickOpen(1);
				if (tmp_stick2) {
					if ((SDL_JoystickNumAxes(tmp_stick2)>1) || (SDL_JoystickNumButtons(tmp_stick2)>0)) {
						second_usable=true;
					}
					SDL_JoystickClose(tmp_stick2);
				}
				// choose joystick type now that we know which physical joysticks are usable
				if (first_usable) {
					if (second_usable) {
						joytype=JOY_2AXIS;
						LOG_MSG("Two or more joysticks reported, initializing with 2axis");
					} else {
						joytype=JOY_4AXIS;
						LOG_MSG("One joystick reported, initializing with 4axis");
					}
				} else if (second_usable) {
					joytype=JOY_4AXIS_2;
					LOG_MSG("One joystick reported, initializing with 4axis_2");
				}
			} else if (mapper.sticks.num) {
				// one joystick present; if it is acceptable use 4axis
				joytype=JOY_NONE;
				SDL_Joystick* tmp_stick1=SDL_JoystickOpen(0);
				if (tmp_stick1) {
					if ((SDL_JoystickNumAxes(tmp_stick1)>0) || (SDL_JoystickNumButtons(tmp_stick1)>0)) {
						joytype=JOY_4AXIS;
						LOG_MSG("One joystick reported, initializing with 4axis");
					}
				}
			} else {
				joytype=JOY_NONE;
			}
		}
	}
	else {
		LOG(LOG_MISC,LOG_DEBUG)("Joystick type none, not initializing");
	}
}

static void CreateBindGroups(void) {
	bindgroups.clear();
#if defined(C_SDL2)
    new CKeyBindGroup(SDL_NUM_SCANCODES);
#else
	new CKeyBindGroup(SDLK_LAST);
#endif
	if (joytype != JOY_NONE) {
#if defined (REDUCE_JOYSTICK_POLLING)
		// direct access to the SDL joystick, thus removed from the event handling
		if (mapper.sticks.num) SDL_JoystickEventState(SDL_DISABLE);
#else
		// enable joystick event handling
		if (mapper.sticks.num) SDL_JoystickEventState(SDL_ENABLE);
		else return;
#endif
		Bit8u joyno=0;
		switch (joytype) {
		case JOY_NONE:
			break;
		case JOY_4AXIS:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new C4AxisBindGroup(joyno,joyno);
			new CStickBindGroup(joyno+1U,joyno+1U,true);
			break;
		case JOY_4AXIS_2:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new C4AxisBindGroup(joyno+1U,joyno);
			new CStickBindGroup(joyno,joyno+1U,true);
			break;
		case JOY_FCS:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CFCSBindGroup(joyno,joyno);
			new CStickBindGroup(joyno+1U,joyno+1U,true);
			break;
		case JOY_CH:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CCHBindGroup(joyno,joyno);
			new CStickBindGroup(joyno+1U,joyno+1U,true);
			break;
		case JOY_2AXIS:
		default:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CStickBindGroup(joyno,joyno);
			if((joyno+1U) < mapper.sticks.num) {
				mapper.sticks.stick[mapper.sticks.num_groups++]=new CStickBindGroup(joyno+1U,joyno+1U);
			} else {
				new CStickBindGroup(joyno+1U,joyno+1U,true);
			}
			break;
		}
	}
}

#if defined (REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void) {
	for (Bitu i=0; i<mapper.sticks.num_groups; i++) {
		mapper.sticks.stick[i]->UpdateJoystick();
	}
}
#endif

void MAPPER_LosingFocus(void) {
	for (CEventVector_it evit=events.begin();evit!=events.end();evit++) {
		if(*evit != caps_lock_event && *evit != num_lock_event)
			(*evit)->DeActivateAll();
	}
}

void MAPPER_RunEvent(Bitu /*val*/) {
	KEYBOARD_ClrBuffer();	//Clear buffer
	GFX_LosingFocus();		//Release any keys pressed (buffer gets filled again).
	MAPPER_RunInternal();
}

void MAPPER_Run(bool pressed) {
	if (pressed)
		return;
	PIC_AddEvent(MAPPER_RunEvent,0.0001f);	//In case mapper deletes the key object that ran it
}

void MAPPER_RunInternal() {
#if defined(__WIN32__) && !defined(C_SDL2)
	if(menu.maxwindow) ShowWindow(GetHWND(), SW_RESTORE);
#endif
	int cursor = SDL_ShowCursor(SDL_QUERY);
	SDL_ShowCursor(SDL_ENABLE);
	bool mousetoggle=false;
	if(mouselocked) {
		mousetoggle=true;
		GFX_CaptureMouse();
	}

	/* Be sure that there is no update in progress */
	GFX_EndUpdate( 0 );
#if defined(C_SDL2)
    mapper.window=GFX_SetSDLSurfaceWindow(640,480);
    if (mapper.window == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());
    mapper.surface=SDL_GetWindowSurface(mapper.window);
    if (mapper.surface == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());
    mapper.draw_surface=SDL_CreateRGBSurface(0,640,480,8,0,0,0,0);
    // Needed for SDL_BlitScaled
    mapper.draw_surface_nonpaletted=SDL_CreateRGBSurface(0,640,480,32,0x0000ff00,0x00ff0000,0xff000000,0);
    mapper.draw_rect=GFX_GetSDLSurfaceSubwindowDims(640,480);
    // Sorry, but SDL_SetSurfacePalette requires a full palette.
    SDL_Palette *sdl2_map_pal_ptr = SDL_AllocPalette(256);
    SDL_SetPaletteColors(sdl2_map_pal_ptr, map_pal, 0, 6);
    SDL_SetSurfacePalette(mapper.draw_surface, sdl2_map_pal_ptr);
    if (last_clicked) {
        last_clicked->BindColor();
        last_clicked=NULL;
    }
#else
	mapper.surface=SDL_SetVideoMode(640,480,8,SDL_RESIZABLE);
	if (mapper.surface == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());

	/* Set some palette entries */
	SDL_SetPalette(mapper.surface, SDL_LOGPAL|SDL_PHYSPAL, map_pal, 0, 5);
	if (last_clicked) {
		last_clicked->SetColor(CLR_WHITE);
		last_clicked=NULL;
	}
#endif
	/* Go in the event loop */
	mapper.exit=false;	
	mapper.redraw=true;
	SetActiveEvent(0);
#if defined (REDUCE_JOYSTICK_POLLING)
	SDL_JoystickEventState(SDL_ENABLE);
#endif
	while (!mapper.exit) {
		if (mapper.redraw) {
			mapper.redraw=false;		
			DrawButtons();
        } else {
#if defined(C_SDL2)
            SDL_UpdateWindowSurface(mapper.window);
#endif
        }
		BIND_MappingEvents();
		SDL_Delay(1);
	}
#if defined(C_SDL2)
    SDL_FreeSurface(mapper.draw_surface);
    SDL_FreeSurface(mapper.draw_surface_nonpaletted);
    SDL_FreePalette(sdl2_map_pal_ptr);
#endif
#if defined (REDUCE_JOYSTICK_POLLING)
	SDL_JoystickEventState(SDL_DISABLE);
#endif
	if((mousetoggle && !mouselocked) || (!mousetoggle && mouselocked)) GFX_CaptureMouse();
	SDL_ShowCursor(cursor);
#if defined(__WIN32__) && !defined(C_SDL2)
	GUI_Shortcut(0);
#endif
#if !defined(C_SDL2)
	DOSBox_RefreshMenu();
#endif
	if(!menu_gui) {
		SDL_FreeSurface(mapper.surface);
		GFX_RestoreMode();
	}
#ifdef __WIN32__
	if(GetAsyncKeyState(0x11)) {
		INPUT ip;

		// Set up a generic keyboard event.
		ip.type = INPUT_KEYBOARD;
		ip.ki.wScan = 0; // hardware scan code for key
		ip.ki.time = 0;
		ip.ki.dwExtraInfo = 0;

		ip.ki.wVk = 0x11;
		ip.ki.dwFlags = 0; // 0 for key press
		SendInput(1, &ip, sizeof(INPUT));

		// Release the "ctrl" key
		ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
	    SendInput(1, &ip, sizeof(INPUT));
	}
#endif
	KEYBOARD_ClrBuffer();
	GFX_LosingFocus();

	void GFX_ForceRedrawScreen(void);
	GFX_ForceRedrawScreen();
}

void MAPPER_CheckKeyboardLayout() {
#if defined(WIN32)
	WORD cur_kb_layout = LOWORD(GetKeyboardLayout(0));

	isJPkeyboard = false;

	if (cur_kb_layout == 1041/*JP106*/) {
		isJPkeyboard = true;
	}
#endif
}

void MAPPER_Init(void) {
	LOG(LOG_MISC,LOG_DEBUG)("Initializing DOSBox mapper");

	MAPPER_CheckKeyboardLayout();
	InitializeJoysticks();
	CreateLayout();
	CreateBindGroups();
	if (!MAPPER_LoadBinds()) CreateDefaultBinds();
	if (SDL_GetModState()&KMOD_CAPS) {
		for (CBindList_it bit=caps_lock_event->bindlist.begin();bit!=caps_lock_event->bindlist.end();bit++) {
#if SDL_VERSION_ATLEAST(1, 2, 14)
			(*bit)->ActivateBind(32767,true,false);
			(*bit)->DeActivateBind(false);
#else
			(*bit)->ActivateBind(32767,true,true); //Skip the action itself as bios_keyboard.cpp handles the startup state.
#endif
		}
	}
	if (SDL_GetModState()&KMOD_NUM) {
		for (CBindList_it bit=num_lock_event->bindlist.begin();bit!=num_lock_event->bindlist.end();bit++) {
#if SDL_VERSION_ATLEAST(1, 2, 14)
			(*bit)->ActivateBind(32767,true,false);
			(*bit)->DeActivateBind(false);
#else
			(*bit)->ActivateBind(32767,true,true);
#endif
		}
	}
}
//Somehow including them at the top conflicts with something in setup.h
#ifdef C_X11_XKB
#include "SDL_syswm.h"
#include <X11/XKBlib.h>
#endif
void MAPPER_StartUp() {
	Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl"));
	mapper.sticks.num=0;
	mapper.sticks.num_groups=0;
	Bitu i;

	LOG(LOG_MISC,LOG_DEBUG)("MAPPER starting up");

	for (i=0; i<MAX_VJOY_BUTTONS; i++) {
		virtual_joysticks[0].button_pressed[i]=false;
		virtual_joysticks[1].button_pressed[i]=false;
		virtual_joysticks[0].hat_pressed[i]=false;
		virtual_joysticks[1].hat_pressed[i]=false;
	}
	for (i=0; i<MAX_VJOY_AXES; i++) {
		virtual_joysticks[0].axis_pos[i]=0;
		virtual_joysticks[1].axis_pos[i]=0;
	}

#if !defined(C_SDL2)
	usescancodes = false;

	if (section->Get_bool("usescancodes")) {
		usescancodes=true;

		/* Note: table has to be tested/updated for various OSs */
#if defined (MACOSX)
		/* nothing */
#elif defined(OS2)
		sdlkey_map[0x61]=SDLK_UP;
		sdlkey_map[0x66]=SDLK_DOWN;
		sdlkey_map[0x63]=SDLK_LEFT;
		sdlkey_map[0x64]=SDLK_RIGHT;
		sdlkey_map[0x60]=SDLK_HOME;
		sdlkey_map[0x65]=SDLK_END;
		sdlkey_map[0x62]=SDLK_PAGEUP;
		sdlkey_map[0x67]=SDLK_PAGEDOWN;
		sdlkey_map[0x68]=SDLK_INSERT;
		sdlkey_map[0x69]=SDLK_DELETE;
		sdlkey_map[0x5C]=SDLK_KP_DIVIDE;
		sdlkey_map[0x5A]=SDLK_KP_ENTER;
		sdlkey_map[0x5B]=SDLK_RCTRL;
		sdlkey_map[0x5F]=SDLK_PAUSE;
//		sdlkey_map[0x00]=SDLK_PRINT;
		sdlkey_map[0x5E]=SDLK_RALT;
		sdlkey_map[0x40]=SDLK_KP5;
		sdlkey_map[0x41]=SDLK_KP6;
#elif !defined (WIN32) /* => Linux & BSDs */
		bool evdev_input = false;
#ifdef C_X11_XKB
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if (SDL_GetWMInfo(&info)) {
			XkbDescPtr desc = NULL;
			if((desc = XkbGetMap(info.info.x11.display,XkbAllComponentsMask,XkbUseCoreKbd))) {
				if(XkbGetNames(info.info.x11.display,XkbAllNamesMask,desc) == 0) {
					const char* keycodes = XGetAtomName(info.info.x11.display, desc->names->keycodes);
//					const char* geom = XGetAtomName(info.info.x11.display, desc->names->geometry);
					if(keycodes) {
						LOG(LOG_MISC,LOG_NORMAL)("keyboard type %s",keycodes);
						if (strncmp(keycodes,"evdev",5) == 0) evdev_input = true;
					}
					XkbFreeNames(desc,XkbAllNamesMask,True);
				}
			XkbFreeClientMap(desc,0,True);
			}
		}
#endif
		if (evdev_input) {
			sdlkey_map[0x67]=SDLK_UP;
			sdlkey_map[0x6c]=SDLK_DOWN;
			sdlkey_map[0x69]=SDLK_LEFT;
			sdlkey_map[0x6a]=SDLK_RIGHT;
			sdlkey_map[0x66]=SDLK_HOME;
			sdlkey_map[0x6b]=SDLK_END;
			sdlkey_map[0x68]=SDLK_PAGEUP;
			sdlkey_map[0x6d]=SDLK_PAGEDOWN;
			sdlkey_map[0x6e]=SDLK_INSERT;
			sdlkey_map[0x6f]=SDLK_DELETE;
			sdlkey_map[0x62]=SDLK_KP_DIVIDE;
			sdlkey_map[0x60]=SDLK_KP_ENTER;
			sdlkey_map[0x61]=SDLK_RCTRL;
			sdlkey_map[0x77]=SDLK_PAUSE;
			sdlkey_map[0x63]=SDLK_PRINT;
			sdlkey_map[0x64]=SDLK_RALT;

			//Win-keys
			sdlkey_map[0x7d]=SDLK_LSUPER;
			sdlkey_map[0x7e]=SDLK_RSUPER;
			sdlkey_map[0x7f]=SDLK_MENU;
		} else {
			sdlkey_map[0x5a]=SDLK_UP;
			sdlkey_map[0x60]=SDLK_DOWN;
			sdlkey_map[0x5c]=SDLK_LEFT;
			sdlkey_map[0x5e]=SDLK_RIGHT;
			sdlkey_map[0x59]=SDLK_HOME;
			sdlkey_map[0x5f]=SDLK_END;
			sdlkey_map[0x5b]=SDLK_PAGEUP;
			sdlkey_map[0x61]=SDLK_PAGEDOWN;
			sdlkey_map[0x62]=SDLK_INSERT;
			sdlkey_map[0x63]=SDLK_DELETE;
			sdlkey_map[0x68]=SDLK_KP_DIVIDE;
			sdlkey_map[0x64]=SDLK_KP_ENTER;
			sdlkey_map[0x65]=SDLK_RCTRL;
			sdlkey_map[0x66]=SDLK_PAUSE;
			sdlkey_map[0x67]=SDLK_PRINT;
			sdlkey_map[0x69]=SDLK_RALT;
		}
#else
		sdlkey_map[0xc8]=SDLK_UP;
		sdlkey_map[0xd0]=SDLK_DOWN;
		sdlkey_map[0xcb]=SDLK_LEFT;
		sdlkey_map[0xcd]=SDLK_RIGHT;
		sdlkey_map[0xc7]=SDLK_HOME;
		sdlkey_map[0xcf]=SDLK_END;
		sdlkey_map[0xc9]=SDLK_PAGEUP;
		sdlkey_map[0xd1]=SDLK_PAGEDOWN;
		sdlkey_map[0xd2]=SDLK_INSERT;
		sdlkey_map[0xd3]=SDLK_DELETE;
		sdlkey_map[0xb5]=SDLK_KP_DIVIDE;
		sdlkey_map[0x9c]=SDLK_KP_ENTER;
		sdlkey_map[0x9d]=SDLK_RCTRL;
		sdlkey_map[0xc5]=SDLK_PAUSE;
		sdlkey_map[0xb7]=SDLK_PRINT;
		sdlkey_map[0xb8]=SDLK_RALT;

		//Win-keys
		sdlkey_map[0xdb]=SDLK_LMETA;
		sdlkey_map[0xdc]=SDLK_RMETA;
		sdlkey_map[0xdd]=SDLK_MENU;

#endif

		Bitu i;
		for (i=0; i<MAX_SDLKEYS; i++) scancode_map[i]=0;
		for (i=0; i<MAX_SCANCODES; i++) {
			SDLKey key=sdlkey_map[i];
			if (key<MAX_SDLKEYS) scancode_map[key]=(Bit8u)i;
		}
	}
#endif

	Prop_path* pp = section->Get_path("mapperfile");
	mapper.filename = pp->realpath;
	MAPPER_AddHandler(&MAPPER_Run,MK_f1,MMOD1,"mapper","Mapper");
}

void MAPPER_Shutdown() {
	for (size_t i=0;i < events.size();i++) {
		if (events[i] != NULL) {
			delete events[i];
			events[i] = NULL;
		}
	}
	events.clear();

	for (size_t i=0;i < buttons.size();i++) {
		if (buttons[i] != NULL) {
			delete buttons[i];
			buttons[i] = NULL;
		}
	}
	buttons.clear();

	for (size_t i=0;i < bindgroups.size();i++) {
		if (bindgroups[i] != NULL) {
			delete bindgroups[i];
			bindgroups[i] = NULL;
		}
	}
	bindgroups.clear();

	for (size_t i=0;i < handlergroup.size();i++) {
		if (handlergroup[i] != NULL) {
#if 0 /* FIXME: Is this list simply another copy of other pointers? Allowing this delete[] to commence triggers double-free warnings */
			delete handlergroup[i];
#endif
			handlergroup[i] = NULL;
		}
	}
	handlergroup.clear();
}

// save state support
void *MAPPER_RunEvent_PIC_Event = (void*)MAPPER_RunEvent;
