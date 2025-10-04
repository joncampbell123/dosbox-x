/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <vector>
#include <list>
#include <chrono>
#include <thread>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>

// for std::max
#include <algorithm>
#if defined(WIN32)
#define NOMINMAX
#endif

#include "SDL.h"

#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "video.h"
#include "keyboard.h"
#include "mouse.h"
#include "pic.h"
#include "control.h"
#include "joystick.h"
#include "util_math.h"
#include "keymap.h"
#include "support.h"
#include "mapper.h"
#include "render.h"
#include "setup.h"
#include "menu.h"
#include "../ints/int10.h"

#include "SDL_syswm.h"
#include "sdlmain.h"
#include "shell.h"
#include "jfont.h"

#if C_EMSCRIPTEN
# include <emscripten.h>
#endif

#include <map>
#include <cctype>

#include <output/output_ttf.h>

#if defined(MACOSX)
#include <Carbon/Carbon.h> 
#endif

#define BMOD_Mod1               0x0001
#define BMOD_Mod2               0x0002
#define BMOD_Mod3               0x0004
#define BMOD_Host               0x0008

#define BFLG_Hold               0x0001
#define BFLG_Hold_Temporary     0x0002 /* Emendelson alternate ctrl+alt host key combinations. Keep it SEPARATE so it does not disturb user changes to the mapper */
#define BFLG_Repeat             0x0004


#define MAXSTICKS               8
#define MAXACTIVE               16
#define MAXBUTTON               96
#define MAXBUTTON_CAP           16
#define MAXAXIS                 8
#define MAXHAT                  2


#define MAX_VJOY_BUTTONS        8
#define MAX_VJOY_HATS           16
#define MAX_VJOY_AXES           8


class CEvent;
class CKeyEvent;
class CHandlerEvent;
class CButton;
class CBind;
class CBindGroup;
class CJAxisBind;
class CJButtonBind;
class CJHatBind;
class CKeyBind;
class CKeyBindGroup;
class CStickBindGroup;
class CCaptionButton;
class CCheckButton;
class CBindButton;
class CModEvent;

#if defined(OS2) && defined(C_SDL2)
#undef CLR_BLACK
#undef CLR_WHITE
#undef CLR_RED
#undef CLR_BLUE
#undef CLR_GREEN
#undef CLR_DARKGREEN
#endif

enum {
    CLR_BLACK = 0,
    CLR_GREY = 1,
    CLR_WHITE = 2,
    CLR_RED = 3,
    CLR_BLUE = 4,
    CLR_GREEN = 5,
    CLR_DARKGREEN = 6
};

enum BB_Types {
    BB_Next,
    BB_Add,
    BB_Del,
    BB_Save,
    BB_Exit,
    BB_Capture,
    BB_Prevpage,
    BB_Nextpage
};

enum BC_Types {
    BC_Mod1,
    BC_Mod2,
    BC_Mod3,
    BC_Host,
    BC_Hold
};

typedef std::list<CBind *>                      CBindList;
typedef std::list<CEvent *>::iterator           CEventList_it;
typedef std::list<CBind *>::iterator            CBindList_it;
typedef std::vector<CButton *>::iterator        CButton_it;
typedef std::vector<CEvent *>::iterator         CEventVector_it;
typedef std::vector<CHandlerEvent *>::iterator  CHandlerEventVector_it;
typedef std::vector<CBindGroup *>::iterator     CBindGroup_it;

static struct {
    bool                                        button_pressed[MAX_VJOY_BUTTONS];
    int16_t                                      axis_pos[MAX_VJOY_AXES];
    bool                                        hat_pressed[MAX_VJOY_HATS];
} virtual_joysticks[2];

static struct {
    CCaptionButton*                             event_title;
    CCaptionButton*                             bind_title;
    CCaptionButton*                             selected;
    CCaptionButton*                             action;
    CCaptionButton*                             dbg2;
    CCaptionButton*                             dbg1;
    CBindButton*                                save;
    CBindButton*                                exit;   
    CBindButton*                                cap;
    CBindButton*                                add;
    CBindButton*                                del;
    CBindButton*                                next;
    CBindButton*                                prevpage;
    CBindButton*                                nextpage;
    CCaptionButton*                             pagestat;
    CCheckButton                                *mod1, *mod2, *mod3, *host, *hold;
} bind_but;

struct KeyBlock {
    const char*                                 title;
    const char*                                 entry;
    KBD_KEYS                                    key;
};

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
static DOSBoxMenu                               mapperMenu;
#endif

extern unsigned int                             hostkeyalt, maincp;

std::map<std::string,std::string>               pending_string_binds;

static int                                      mapper_esc_count = 0;

Bitu                                            next_handler_xpos = 0;
Bitu                                            next_handler_ypos = 0;

bool                                            mapper_addhandler_create_buttons = false;

bool                                            isJPkeyboard = false;
bool                                            dotype = false;

//! \brief joystick autofire config option
bool                                            autofire = false;

//! \brief log scancodes for debugging config option
bool                                            log_keyboard_scan_codes = false;

//! \brief map of joystick 1 axes
int                                             joy1axes[8];

//! \brief map of joystick 2 axes
int                                             joy2axes[8];

static std::vector<CEvent *>                    events;
static std::vector<CButton *>                   buttons;
static std::vector<CBindGroup *>                bindgroups;
static std::vector<CHandlerEvent *>             handlergroup;

static CModEvent*                               mod_event[8] = {NULL};

static CKeyEvent*                               caps_lock_event = NULL;
static CKeyEvent*                               num_lock_event = NULL;

static std::map<std::string, size_t>            name_to_events;
static std::map<std::string, std::string>       event_map;

static SDL_Color                                map_pal[7] =
{
    {0x00,0x00,0x00,0x00},          //0=black
    {0x7f,0x7f,0x7f,0x00},          //1=grey
    {0xff,0xff,0xff,0x00},          //2=white
    {0xff,0x00,0x00,0x00},          //3=red
    {0x10,0x30,0xff,0x00},          //4=blue
    {0x00,0xff,0x20,0x00},          //5=green
    {0x00,0x7f,0x10,0x00}           //6=dark green
};

static KeyBlock combo_f[12] =
{
    {"F1","f1",KBD_f1},     {"F2","f2",KBD_f2},     {"F3","f3",KBD_f3},
    {"F4","f4",KBD_f4},     {"F5","f5",KBD_f5},     {"F6","f6",KBD_f6},
    {"F7","f7",KBD_f7},     {"F8","f8",KBD_f8},     {"F9","f9",KBD_f9},
    {"F10","f10",KBD_f10},  {"F11","f11",KBD_f11},  {"F12","f12",KBD_f12},
};

static KeyBlock combo_1[14] =
{
    {"`~","grave",KBD_grave},   {"1!","1",KBD_1},   {"2@","2",KBD_2},
    {"3#","3",KBD_3},           {"4$","4",KBD_4},   {"5%","5",KBD_5},
    {"6^","6",KBD_6},           {"7&","7",KBD_7},   {"8*","8",KBD_8},
    {"9(","9",KBD_9},           {"0)","0",KBD_0},   {"-_","minus",KBD_minus},
    {"=+","equals",KBD_equals}, {"\x1B","bspace",KBD_backspace},
};

static KeyBlock combo_1_pc98[14] =
{
    {"`~","grave",KBD_grave},   {"1!","1",KBD_1},   {"2\"","2",KBD_2},
    {"3#","3",KBD_3},           {"4$","4",KBD_4},   {"5%","5",KBD_5},
    {"6&","6",KBD_6},           {"7'","7",KBD_7},   {"8(","8",KBD_8},
    {"9)","9",KBD_9},           {"0","0",KBD_0},    {"-=","minus",KBD_minus},
    {"=+","equals",KBD_equals}, {"\x1B","bspace",KBD_backspace},
};

static KeyBlock combo_2[12] =
{
    {"Q","q",KBD_q},            {"W","w",KBD_w},    {"E","e",KBD_e},
    {"R","r",KBD_r},            {"T","t",KBD_t},    {"Y","y",KBD_y},
    {"U","u",KBD_u},            {"I","i",KBD_i},    {"O","o",KBD_o},
    {"P","p",KBD_p},            {"[{","lbracket",KBD_leftbracket},
    {"]}","rbracket",KBD_rightbracket},
};

static KeyBlock combo_3[12] =
{
    {"A","a",KBD_a},            {"S","s",KBD_s},    {"D","d",KBD_d},
    {"F","f",KBD_f},            {"G","g",KBD_g},    {"H","h",KBD_h},
    {"J","j",KBD_j},            {"K","k",KBD_k},    {"L","l",KBD_l},
    {";:","semicolon",KBD_semicolon},               {"'\"","quote",KBD_quote},
    {"\\|","backslash",KBD_backslash},
};

static KeyBlock combo_3_pc98[12] =
{
    {"A","a",KBD_a},            {"S","s",KBD_s},    {"D","d",KBD_d},
    {"F","f",KBD_f},            {"G","g",KBD_g},    {"H","h",KBD_h},
    {"J","j",KBD_j},            {"K","k",KBD_k},    {"L","l",KBD_l},
    {";:+","semicolon",KBD_semicolon},              {"'\"","quote",KBD_quote},
    {"\\|","backslash",KBD_backslash},
};

static KeyBlock combo_4[11] =
{
    {"<>","lessthan",KBD_extra_lt_gt},
    {"Z","z",KBD_z},            {"X","x",KBD_x},    {"C","c",KBD_c},
    {"V","v",KBD_v},            {"B","b",KBD_b},    {"N","n",KBD_n},
    {"M","m",KBD_m},            {",<","comma",KBD_comma},
    {".>","period",KBD_period},                     {"/?","slash",KBD_slash},
};

static const char* LabelMod1 = "Mod1";
static const char* LabelMod2 = "Mod2";
static const char* LabelMod3 = "Mod3";
static const char* LabelHost = "Host";
static const char* LabelHold = "Hold";

static bool initjoy=true;
static int cpage=1, maxpage=1;

static void                                     SetActiveEvent(CEvent * event);
static void                                     SetActiveBind(CBind * _bind);
static void                                     change_action_text(const char* text,uint8_t col);
static void                                     DrawText(Bitu x,Bitu y,const char * text,uint8_t color,uint8_t bkcolor=CLR_BLACK);
static void                                     MAPPER_SaveBinds(void);

CEvent*                                         get_mapper_event_by_name(const std::string &x);
bool                                            MAPPER_DemoOnly(void);

#if defined(USE_TTF)
void                                            resetFontSize(void);
#endif

Bitu                                            GUI_JoystickCount(void);                // external
bool                                            OpenGL_using(void);
bool                                            GFX_GetPreventFullscreen(void);         // external
void                                            GFX_ForceRedrawScreen(void);            // external
#if defined(WIN32) && !defined(HX_DOS)
void                                            DOSBox_SetSysMenu(void);
void                                            WindowsTaskbarUpdatePreviewRegion(void);// external
void                                            WindowsTaskbarResetPreviewRegion(void); // external
#endif

#if defined(MACOSX)
void                        macosx_reload_touchbar(void);
#endif

bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);

//! \brief Base CEvent class for mapper events
class CEvent {
public:
    //! \brief Type of CEvent class, if the code needs to use the specific type
    //!
    //! \description This is used by other parts of the mapper if it needs to retrieve
    //!              additional information that is only provided by the handler event class
    enum event_type {
        event_t=0,
        handler_event_t,
        mod_event_t
    };
public:
    //! \brief CEvent constructor
    //!
    //! \description This constructor takes a mapper entry name and event type.
    //!              Subclasses will call down to this constructor as well.
    //!              The handler event class will fill in the _type field to
    //!              identify itself.
    CEvent(char const * const _entry,const enum event_type _type = event_t) {
        safe_strncpy(entry,_entry,sizeof(entry));

        {
            if (name_to_events.find(entry) != name_to_events.end())
                E_Exit("Mapper: Event \"%s\" already defined",entry);
        }

        name_to_events[entry] = events.size();

        events.push_back(this);
        bindlist.clear();
        active=false;
        activity=0;
        current_value=0;
        type = _type;

        assert(get_mapper_event_by_name(entry) == this);
    }

    //! \brief Retrieve binding string for display in the menu
    //!
    //! \description Retrieve text string to show as the assigned mapper binding in a
    //!              menu item's displayable area so that the user knows what keyboard
    //!              input will trigger the shortcut.
    virtual std::string GetBindMenuText(void);

    //! \brief Update the menu item for the mapper shortcut with the latest text and keyboard shortcut
    void update_menu_shortcut(void) {
        if (!eventname.empty()) {
            DOSBoxMenu::item& item = mainMenu.get_item(std::string("mapper_") + std::string(eventname));
            std::string str = GetBindMenuText();
            item.set_shortcut_text(str);
            item.refresh_item(mainMenu);
//            LOG_MSG("%s",str.c_str());
        }
    }

    //! \brief Add binding to the bindlist
    void AddBind(CBind * bind);
	void ClearBinds();

    virtual ~CEvent();

    //! \brief Change whether the event is activated or not
    virtual void Active(bool yesno) {
        active = yesno;
    }

    //! \brief Activate the event, act on it
    virtual void ActivateEvent(bool ev_trigger,bool skip_action)=0;

    //! \brief Deactivate the event
    virtual void DeActivateEvent(bool ev_trigger)=0;

    //! \brief Deactivate all bindings 
    void DeActivateAll(void);

    //! \brief Set the value of the event (such as joystick position)
    void SetValue(Bits value){
        current_value=value;
    }

    //! \brief Get the value of the event
    Bits GetValue(void) {
        return current_value;
    }

    //! \brief Retrieve the name of the event
    char * GetName(void) { return entry; }

    //! \brief Indicate whether the event is a trigger or continuous input
    virtual bool IsTrigger(void)=0;

    /// TODO
    virtual void RebindRedraw(void) {}

    //! \brief Event name
    std::string eventname;

    //! \brief event type
    enum event_type type;

    //! \brief Bind list to trigger on activation/deactivation
    CBindList bindlist;

    //! \brief Whether the event is active or not
    bool active;
protected:
    //! \brief Activity counter
    Bitu activity;

    //! \brief Mapper entry name
    char entry[20];

    //! \brief Current value of the event (such as joystick position)
    Bits current_value;
};

//! \brief class for events which can be ON/OFF only: key presses, joystick buttons, joystick hat
class CTriggeredEvent : public CEvent {
public:
    //! \brief Constructor, with event name
    CTriggeredEvent(char const * const _entry) : CEvent(_entry) {}

    // methods below this line have sufficient documentation inherited from the base class

    ~CTriggeredEvent() {}

    bool IsTrigger(void) override {
        return true;
    }

    void ActivateEvent(bool ev_trigger,bool skip_action) override {
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

    void DeActivateEvent(bool /*ev_trigger*/) override {
        if (activity > 0) activity--;
        if (!activity) Active(false);
    }
};

//! \brief class for events which have a non-boolean state: joystick axis movement
class CContinuousEvent : public CEvent {
public:
    //! \brief Constructor, with event name
    CContinuousEvent(char const * const _entry) : CEvent(_entry) {}

    // methods below this line have sufficient documentation inherited from the base class

    ~CContinuousEvent() {}

    bool IsTrigger(void) override {
        return false;
    }

    void ActivateEvent(bool ev_trigger,bool skip_action) override {
        if (ev_trigger) {
            activity++;
            if (!skip_action) Active(true);
        } else {
            /* test if no trigger-activity is present, this cares especially
               about activity of the opposite-direction joystick axis for example */
            if (!GetActivityCount()) Active(true);
        }
    }

    void DeActivateEvent(bool ev_trigger) override {
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

    //! \brief Retrieve activity counter
    virtual Bitu GetActivityCount(void) {
        return activity;
    }

    //! \brief Re-post activity
    virtual void RepostActivity(void) {}
};

//! \brief Base C++ class for a binding assigned in the mapper interface (or by default settings)
class CBind {
public:
    //! \brief Bind class type, for runtime detection
    enum CBindType {
        bind_t=0,
        keybind_t
    };
public:
    virtual ~CBind () {
        list->remove(this);
    }

    //! \brief Constructor, to define the binding and type. This constructor adds the CBind object itself to the list
    CBind(CBindList * _list,enum CBindType _type = bind_t) {
        list=_list;
        _list->push_back(this);
        mods=flags=0;
        event = nullptr;
        active=holding=false;
        type = _type;
        value = 0;
    }

    //! \brief Get modifier text
    virtual std::string GetModifierText(void);

    //! \brief Get binding text, for display in the menu item
    virtual std::string GetBindMenuText(void) {
        return GetModifierText();
    }

    //! \brief Append modifier text to a string, for use in recording bindings to the mapper file
    void AddFlags(char * buf) {
        if (mods & BMOD_Mod1) strcat(buf," mod1");
        if (mods & BMOD_Mod2) strcat(buf," mod2");
        if (mods & BMOD_Mod3) strcat(buf," mod3");
        if (mods & BMOD_Host) strcat(buf," host");
        if (flags & BFLG_Hold) strcat(buf," hold");
    }

    //! \brief Read modifier flags from a string, for use in parsing bindings from the mapper file
    void SetFlags(char * buf) {
        char * word;
        while (*(word=StripWord(buf))) {
            if (!strcasecmp(word,"mod1")) mods|=BMOD_Mod1;
            if (!strcasecmp(word,"mod2")) mods|=BMOD_Mod2;
            if (!strcasecmp(word,"mod3")) mods|=BMOD_Mod3;
            if (!strcasecmp(word,"host")) mods|=BMOD_Host;
            if (!strcasecmp(word,"hold")) flags|=BFLG_Hold;
        }
    }

    //! \brief Activate bindings
    virtual void ActivateBind(Bits _value,bool ev_trigger,bool skip_action=false) {
        if (event->IsTrigger()) {
            /* use value-boundary for on/off events */
            if (_value>25000) {
                event->SetValue(_value);
                if (active) return;
                if (!holding) event->ActivateEvent(ev_trigger,skip_action);
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

    //! \brief Deactivate bindings
    void DeActivateBind(bool ev_trigger) {
        if (event->IsTrigger()) {
            if (!active) return;
            active=false;
            if (flags & (BFLG_Hold|BFLG_Hold_Temporary)) {
                if (!holding) {
                    holding=true;
                    return;
                } else {
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

    //! \brief Get configuration name, for use in writing the mapper file
    virtual void ConfigName(char * buf)=0;

    //! \brief Get binding name, for display in the mapper UI
    virtual void BindName(char * buf)=0;

    //! \brief Modifiers (shift, ctrl, alt)
    Bitu mods;

    //! \brief Flags (hold)
    Bitu flags;

    //! \brief Binding value (TODO?)
    int16_t value;

    //! \brief Event object this binding is bound to (for visual UI purposes)
    CEvent * event;

    //! \brief List that this object is part of
    CBindList * list;

    //! \brief Active status
    bool active;

    //! \brief Holding status
    bool holding;

    //! \brief Binding type
    enum CBindType type;
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
void CEvent::ClearBinds() {
	for (CBind *bind : bindlist) {
		delete bind;
	}
	bindlist.clear();
}
void CEvent::DeActivateAll(void) {
    for (CBindList_it bit=bindlist.begin();bit!=bindlist.end();++bit) {
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

void MAPPER_TriggerEvent(const CEvent *event, const bool deactivation_state) {
	assert(event);
	for (auto &bind : event->bindlist) {
		bind->ActivateBind(32767, true, false);
		bind->DeActivateBind(deactivation_state);
	}
}

#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
/* TODO: This is fine, but it should not call MAPPER functions from a separate thread.
 *       These functions are not necessarily reentrant and can cause screw ups when
 *       called from multiple threads.
 *
 *       Also the HX-DOS builds cannot use this code because the older MinGW lacks
 *       std::thread.
 *
 *       Replace thread with PIC_AddEvent() to callback. */
class Typer {
	public:
		Typer() = default;
		Typer(const Typer&) = delete; // prevent copy
		Typer& operator=(const Typer&) = delete; // prevent assignment
		~Typer() {
			Stop();
		}
		void Start(std::vector<CEvent*>     *ext_events,
		           std::vector<std::string> &ext_sequence,
                   const uint32_t           wait_ms,
                   const uint32_t           pace_ms,
                   bool                     choice) {
			// Guard against empty inputs
			if (!ext_events || ext_sequence.empty())
				return;
			Wait();
			m_events = ext_events;
			m_sequence = std::move(ext_sequence);
			m_wait_ms = wait_ms;
			m_pace_ms = pace_ms;
			m_choice = choice;
			m_stop_requested = false;
			m_instance = std::thread(&Typer::Callback, this);
		}
		void Wait() {
			if (m_instance.joinable())
				m_instance.join();
		}
		void Stop() {
			m_stop_requested = true;
			Wait();
		}
	private:
		CEvent *GetLShiftEvent()
		{
			static CEvent *lshift_event = nullptr;
			for (auto &event : *m_events) {
				if (std::string("key_lshift") == event->GetName()) {
					lshift_event = event;
					break;
				}
			}
			assert(lshift_event);
			return lshift_event;
		}
		void Callback() {
			// quit before our initial wait time
			if (m_stop_requested || (m_choice && !dotype))
				return;
			std::this_thread::sleep_for(std::chrono::milliseconds(m_wait_ms));
			for (const auto &button : m_sequence) {
				bool found = false;
				// comma adds an extra pause, similar to the pause used in a phone number
				if (m_choice && !dotype)
					return;
				if (button == ",") {
					found = true;
					 // quit before the pause
					if (m_stop_requested)
						return;
					std::this_thread::sleep_for(std::chrono::milliseconds(m_pace_ms));
				// Otherwise trigger the matching button if we have one
				} else {
					const auto is_cap = button.length() == 1 && isupper(button[0]);
					const auto maybe_lshift = is_cap ? GetLShiftEvent() : nullptr;
					const std::string lbutton = is_cap ? std::string(1, tolower(button[0])) : button;
					const std::string bind_name = "key_" + lbutton;
					for (auto &event : *m_events) {
						if (bind_name == event->GetName()) {
							found = true;
							if (maybe_lshift) maybe_lshift->Active(true);
							event->Active(true);
							std::this_thread::sleep_for(std::chrono::milliseconds(50));
							event->Active(false);
							if (maybe_lshift) maybe_lshift->Active(false);
							break;
						}
					}
				}
				/*
				*  Terminate the sequence for safety reasons if we can't find a button.
				*  For example, we don't want DEAL becoming DEL, or 'rem' becoming 'rm'
				*/
				if (!found) {
					LOG_MSG("MAPPER: Couldn't find a button named '%s', stopping.",
							button.c_str());
					return;
				}
				if (m_stop_requested || (m_choice && !dotype)) // quit before the pacing delay
					return;
				std::this_thread::sleep_for(std::chrono::milliseconds(m_pace_ms));
			}
		}
		std::thread              m_instance;
		std::vector<std::string> m_sequence;
		std::vector<CEvent*>     *m_events = nullptr;
		uint32_t                 m_wait_ms = 0;
		uint32_t                 m_pace_ms = 0;
		bool                     m_choice = false;
		bool                     m_stop_requested = false;
};
#endif

static struct CMapper {
#if defined(C_SDL2)
    SDL_Window*                                 window;
    uint32_t                                    window_scale;
    SDL_Rect                                    draw_rect;
    SDL_Surface*                                draw_surface_nonpaletted;
    SDL_Surface*                                draw_surface;
#endif
    SDL_Surface*                                surface = NULL;
    bool                                        exit = false;
    CEvent*                                     aevent = NULL;                     //Active Event
    CBind*                                      abind = NULL;                      //Active Bind
    CBindList_it                                abindit;                    //Location of active bind in list
    bool                                        redraw = false;
    bool                                        addbind = false;
    bool                                        running = false;
    Bitu                                        mods = 0;
    struct {
        Bitu                                    num_groups,num;
        CStickBindGroup*                        stick[MAXSTICKS];
    } sticks = {};
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
	Typer										typist;
#endif
    std::string                                 filename;
} mapper;

#if defined(C_SDL2) /* SDL 2.x */

/* HACK */
typedef SDL_Scancode SDLKey;

#else /* !defined(C_SDL2) SDL 1.x */

#define MAX_SDLKEYS 323

static int usescancodes=-1;
static uint8_t scancode_map[MAX_SDLKEYS];

#define Z SDLK_UNKNOWN

#if defined (MACOSX)
static SDLKey sdlkey_map[]={
    /* Main block printables */
    /*00-05*/ Z, SDLK_s, SDLK_d, SDLK_f, SDLK_h, SDLK_g,
    /*06-0B*/ SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_BACKQUOTE /* 'Section' key */, SDLK_b,
    /*0C-11*/ SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_y, SDLK_t, 
    /*12-17*/ SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_6, SDLK_5, 
    /*18-1D*/ SDLK_EQUALS, SDLK_9, SDLK_7, SDLK_MINUS, SDLK_8, SDLK_0, 
    /*1E-21*/ SDLK_RIGHTBRACKET, SDLK_o, SDLK_u, SDLK_LEFTBRACKET, 
    /*22-23*/ SDLK_i, SDLK_p,
    /*24-29*/ SDLK_RETURN, SDLK_l, SDLK_j, SDLK_QUOTE, SDLK_k, SDLK_SEMICOLON, 
    /*2A-2F*/ SDLK_BACKSLASH, SDLK_COMMA, SDLK_SLASH, SDLK_n, SDLK_m, SDLK_PERIOD,

    /* Spaces, controls, modifiers (dosbox uses LMETA only for
     * hotkeys, it's not really mapped to an emulated key) */
    /*30-33*/ SDLK_TAB, SDLK_SPACE, SDLK_BACKQUOTE, SDLK_BACKSPACE,
    /*34-37*/ Z, SDLK_ESCAPE, SDLK_RSUPER, SDLK_LSUPER,
    /*38-3B*/ SDLK_LSHIFT, SDLK_CAPSLOCK, SDLK_LALT, SDLK_LCTRL,
    /*3C-40*/ SDLK_RSHIFT, SDLK_RALT, SDLK_RCTRL, Z, SDLK_F17,

    /* Keypad (KP_EQUALS not supported, NUMLOCK used on what is CLEAR
     * in Mac OS X) */
    /*41-46*/ SDLK_KP_PERIOD, Z, SDLK_KP_MULTIPLY, Z, SDLK_KP_PLUS, Z,
    /*47-4A*/ SDLK_NUMLOCK /*==SDLK_CLEAR*/, Z, Z, Z,
    /*4B-4D*/ SDLK_KP_DIVIDE, SDLK_KP_ENTER, Z,
    /*4E-51*/ SDLK_KP_MINUS, SDLK_F18, SDLK_F19, SDLK_KP_EQUALS,
    /*52-57*/ SDLK_KP0, SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5, 
    /*58-5C*/ SDLK_KP6, SDLK_KP7, Z, SDLK_KP8, SDLK_KP9, 

	/*5D-5F*/ SDLK_JP_YEN, SDLK_JP_RO, SDLK_KP_COMMA,
    
    /* Function keys and cursor blocks (F13 not supported, F14 =>
     * PRINT[SCREEN], F15 => SCROLLOCK, F16 => PAUSE, HELP => INSERT) */
    /*60-64*/ SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F3, SDLK_F8,
    /*65-6A*/ SDLK_F9, SDLK_WORLD_17, SDLK_F11, SDLK_WORLD_18, SDLK_F13, SDLK_PAUSE /*==SDLK_F16*/,
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
typedef char assert_right_size [MAX_SCANCODES == (sizeof(sdlkey_map)/sizeof(sdlkey_map[0])) ? 1 : -1];

#elif defined (__linux__)
#define MAX_SCANCODES 0x90
#include "../../vs/sdl/include/SDL_keysym.h"
static SDLKey sdlkey_map[MAX_SCANCODES] = { // Convert hardware scancode (XKB = evdev + 8) to SDL virtual keycode
    /* Refer to https://chromium.googlesource.com/chromium/src/+/lkgr/ui/events/keycodes/keyboard_code_conversion_x.cc */
    SDLK_UNKNOWN,//0x00
    Z,Z,Z,Z,Z,Z,Z,Z, //0x01-0x08 unknown
    SDLK_ESCAPE,//0x09
    /*0x0a-0x13 1-9,0*/
    SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0,
    SDLK_MINUS, //0x14
    SDLK_EQUALS, //0x15
    SDLK_BACKSPACE,//0x16
    SDLK_TAB,//0x17
    SDLK_q,//0x18
    SDLK_w,//0x19
    SDLK_e,//0x1a
    SDLK_r,//0x1b
    SDLK_t,//0x1c
    SDLK_y,//0x1d
    SDLK_u,//0x1e
    SDLK_i,//0x1f
    SDLK_o,//0x20
    SDLK_p,//0x21
    SDLK_LEFTBRACKET,//0x22  [ and {
    SDLK_RIGHTBRACKET,//0x23 ] and }
    SDLK_RETURN,//0x24 Enter
    SDLK_LCTRL,//0x25  Left-Control
    SDLK_a,//0x26
    SDLK_s,//0x27
    SDLK_d,//0x28
    SDLK_f,//0x29
    SDLK_g,//0x2a
    SDLK_h,//0x2b
    SDLK_j,//0x2c
    SDLK_k,//0x2d
    SDLK_l,//0x2e
    SDLK_SEMICOLON,//0x2f  ; and :
    SDLK_QUOTE,//0x30 ' and "
    SDLK_BACKQUOTE,//0x31 Grave Accent and Tilde
    SDLK_LSHIFT,//0x32 left-shift
    SDLK_BACKSLASH,//0x33 \ and |
    SDLK_z,//0x34
    SDLK_x,//0x35
    SDLK_c,//0x36
    SDLK_v,//0x37
    SDLK_b,//0x38
    SDLK_n,//0x39
    SDLK_m,//0x3a
    SDLK_COMMA,//0x3b  , and <
    SDLK_PERIOD,//0x3c . and >
    SDLK_SLASH,//0x3d / and ?
    SDLK_RSHIFT,//0x3e Right-Shift
    SDLK_KP_MULTIPLY,//0x3f Keypad *
    SDLK_LALT,//0x40 Left-Alt
    SDLK_SPACE,//0x41 Spacebar
    SDLK_CAPSLOCK,//0x42 CapsLock
    /*0x43-0x4c F1-F10*/
    SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10,
    SDLK_NUMLOCK, //0x4d Keypad Num Lock and Clear
    SDLK_SCROLLOCK, //0x4e Scroll-lock
    SDLK_KP7, //0x4f Keypad 7 and Home
    SDLK_KP8, //0x50 Keypad 8 and Up Arrow
    SDLK_KP9, //0x51 Keypad 9 and PageUp
    SDLK_KP_MINUS, //0x52 Keypad -
    SDLK_KP4, //0x53 Keypad 4 Left-arrow
    SDLK_KP5, //0x54 Keypad 5
    SDLK_KP6, //0x55 Keypad 6 Right-arrow
    SDLK_KP_PLUS, //0x56 Keypad +
    SDLK_KP1, //0x57 Keypad 1 and End
    SDLK_KP2, //0x58 Keypad 2 and Down Arrow
    SDLK_KP3, //0x59 Keypad 3 and PageDn
    SDLK_KP0, //0x5a Keypad 0 and Insert
    SDLK_KP_PERIOD, //0x5b Keypad . and Delete
    Z, //0x5c unknown
    SDLK_WORLD_12, //0x5d Zenkaku/Hankaku
    SDLK_LESS, //0x5e Non-US \ and |
    SDLK_F11, //0x5f
    SDLK_F12, //0x60
    SDLK_JP_RO, //0x61
    Z, Z, //0x62-0x63 unknown
    SDLK_WORLD_14, //0x64 Henkan
    SDLK_WORLD_15, //0x65 Hiragana/Katakana
    SDLK_WORLD_13, //0x66 Muhenkan
    Z,SDLK_KP_ENTER,SDLK_RCTRL,SDLK_KP_DIVIDE, //0x67-0x6a
    Z, //SDLK_PRINTSCREEN, //0x6b
    SDLK_RALT,  //0x6c
    Z, //0x6d unknown
    SDLK_HOME,  //0x6e
    SDLK_UP,    //0x6f
    SDLK_PAGEUP,//0x70
    SDLK_LEFT,  //0x71
    SDLK_RIGHT, //0x72
    SDLK_END,   //0x73
    SDLK_DOWN,  //0x74
    SDLK_PAGEDOWN,//0x75
    SDLK_INSERT,//0x76
    SDLK_DELETE,//0x77
    Z,Z,Z,Z,Z,Z,Z, //0x78-0x7e
    SDLK_PAUSE, // 0x7F
    /* 0x80: */
    Z,Z, //0x80-0x81 unknown
    Z, //0x82 Hanguel
    Z, //0x83 Hanja
    SDLK_JP_YEN,//0x84
    SDLK_LSUPER, //0x85
    SDLK_RSUPER, //0x86
    SDLK_MENU,  //0x87
    Z,Z,Z,Z,Z,Z,Z,Z //0x88-0x8f unknown
    /* 0x90-0x9f unknown */
    //Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z
};

#else // !MACOSX && !Linux
#if defined(__FreeBSD__) || defined(OS2)
// Todo: recheck sdl mapping
#define SDLK_JP_RO (SDLKey)0x73
#define SDLK_JP_YEN (SDLKey)0x7d
#if defined(OS2)
#define SDLK_KP_COMMA SDLK_COMMA
#endif
#endif
#define MAX_SCANCODES 0xdf
static SDLKey sdlkey_map[MAX_SCANCODES] = {
    /* Refer to http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf PS/2 Set 1 Make */
    SDLK_UNKNOWN,//0x00
	SDLK_ESCAPE,//0x01
    /*0x02-0x0b 1-9,0*/
	SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0,
	/* 0x0c: */
	SDLK_MINUS, //0x0c
	SDLK_EQUALS, //0x0d
	SDLK_BACKSPACE,//0x0e
	SDLK_TAB,//0x0f
	SDLK_q,//0x10
	SDLK_w,//0x11
	SDLK_e,//0x12
	SDLK_r,//0x13
	SDLK_t,//0x14
	SDLK_y,//0x15
	SDLK_u,//0x16
	SDLK_i,//0x17
	SDLK_o,//0x18
	SDLK_p,//0x19
	SDLK_LEFTBRACKET,//0x1a  [ and {
	SDLK_RIGHTBRACKET,//0x1b ] and }
	SDLK_RETURN,//0x1c Enter
	SDLK_LCTRL,//0x1d  Left-Control
	SDLK_a,//0x1e
	SDLK_s,//0x1f
	SDLK_d,//0x20
	SDLK_f,//0x21
	SDLK_g,//0x22
	SDLK_h,//0x23
	SDLK_j,//0x24
	SDLK_k,//0x25
	SDLK_l,//0x26
	SDLK_SEMICOLON,//0x27  ; and :
	SDLK_QUOTE,//0x28 ' and "
	SDLK_BACKQUOTE,//0x29 Grave Accent and Tilde
	SDLK_LSHIFT,//0x2a left-shift
	SDLK_BACKSLASH,//0x2b \ and |
	SDLK_z,//0x2c
	SDLK_x,//0x2d
	SDLK_c,//0x2e
	SDLK_v,//0x2f
	SDLK_b,//0x30
	SDLK_n,//0x31
	SDLK_m,//0x32
	/* 0x33: */
	SDLK_COMMA,//0x33  , and <
	SDLK_PERIOD,//0x34 . and >
	SDLK_SLASH,//0x35 / and ?
	SDLK_RSHIFT,//0x36 Right-Shift
	SDLK_KP_MULTIPLY,//0x37 Keypad *
	SDLK_LALT,//0x38 Left-Alt
	SDLK_SPACE,//0x39 Spacebar
	SDLK_CAPSLOCK,//0x3a CapsLock
	/*0x3b-0x44 F1-F10*/
	SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10,
	/* 0x45: */
	SDLK_NUMLOCK, //0x45 Keypad Num Lock and Clear
	SDLK_SCROLLOCK, //0x46 Scroll-lock and Break
	SDLK_KP7, //0x47 Keypad 7 and Home
	SDLK_KP8, //0x48 Keypad 8 and Up Arrow
	SDLK_KP9, //0x49 Keypad 9 and PageUp
	SDLK_KP_MINUS, //0x4a Keypad -
	SDLK_KP4, //0x4b Keypad 4 Left-arrow
	SDLK_KP5, //0x4c Keypad 5
	SDLK_KP6, //0x4d Keypad 6 Right-arrow
	SDLK_KP_PLUS, //0x4e Keypad +
	SDLK_KP1, //0x4f Keypad 1 and End
	SDLK_KP2, //0x50 Keypad 2 and Down Arrow
	SDLK_KP3, //0x51 Keypad 3 and PageDn
	SDLK_KP0, //0x52 Keypad 0 and Insert
	SDLK_KP_PERIOD, //0x53 Keypad . and Delete
	Z, //0x54
	Z, //0x55
	SDLK_LESS, //0x56 Non-US \ and |
	SDLK_F11, //0x57
	SDLK_F12, //0x58
	SDLK_KP_EQUALS, //0x59 Keypad =
	Z, Z, //0x5a-0x5b unknown
	Z, //0x5c Keyboard International6
	Z, Z, Z, //0x5d-0x5f unknown
	/* 0x60: */
	Z, Z, Z, Z,	//0x60-0x63 unknown
	/* 0x64-0x6e F13-F23 */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	Z, // 0x6F unknown
	/* 0x70: */
	SDLK_WORLD_15,//0x70 Keyboard International2 Hiragana/Katakana
	Z, Z, //0x71-0x72 unknown
	SDLK_JP_RO, //0x73 Keyboard International1 \ and _
	Z, Z,//0x74-0x75 unknown
	Z, //0x76 F24 or Keyboard LANG5
	Z, //0x77 Keyboard Lang4
	Z, //0x78 Keyboard Lang3
	SDLK_WORLD_14, //0x79 Keyboard International4 Henkan
	Z, //0x7a unknown
	SDLK_WORLD_13, //0x7b Keyboard International5 Muhenkan
	Z, //0x7c unknown
	SDLK_JP_YEN, //0x7d Keyboard International3 \ and |
	SDLK_KP_COMMA, //0x7e Keypad Comma
	Z, //0x7f unknown
	/* 0x80-0x8f unknown */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	/* 0x90: */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	/* 0xA0: */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	/* 0xB0: */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	/* 0xC0: */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	/* 0xD0: */
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, Z//,Z,Z,
	/* 0xE0: */
	//Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z,
	/* 0xF0: */
	//Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z//,Z,Z

};
#endif

#undef Z

unsigned int Linux_GetKeyboardLayout(void); // defined in sdlmain_linux.cpp

#if !defined(C_SDL2)
void setScanCode(Section_prop * section) {
	usescancodes = -1;
	const char *usesc = section->Get_string("usescancodes");
	if (!strcasecmp(usesc, "true")||!strcmp(usesc, "1"))
		usescancodes = 1;
	else if (!strcasecmp(usesc, "false")||!strcmp(usesc, "0"))
		usescancodes = 0;
#if defined(WIN32)
    else {
        WORD cur_kb_layout = LOWORD(GetKeyboardLayout(0));
        if(cur_kb_layout == 1033) { /* Locale ID: en-us */
            usescancodes = 0;
            LOG_MSG("SDL_mapper: US keyboard detected, set usescancodes=false");
        }
        else {
            usescancodes = 1;
            LOG_MSG("SDL_mapper: non-US keyboard detected, set usescancodes=true");
        }
    }
#elif defined(__linux__)
    else {
        if(Linux_GetKeyboardLayout() == DKM_US) { /* Locale ID: en-us */
            usescancodes = 0;
            LOG_MSG("SDL_mapper: US keyboard detected, set usescancodes=false");
        }
        else {
            usescancodes = 1;
            LOG_MSG("SDL_mapper: non-US keyboard detected, set usescancodes=true");
        }
    }
#elif defined(MACOSX)
    else {
        char layout[128];
        memset(layout, '\0', sizeof(layout));
        TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
        // get input source id - kTISPropertyInputSourceID
        // get layout name - kTISPropertyLocalizedName
        CFStringRef layoutID = CFStringRef(TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
        CFStringGetCString(layoutID, layout, sizeof(layout), kCFStringEncodingUTF8);
        //LOG_MSG("SDL_mapper: %s\n", layout);
        if(!strcasecmp(layout, "com.apple.keylayout.US")) { /* US keyboard layout */
            usescancodes = 0;
            LOG_MSG("SDL_mapper: US keyboard detected, set usescancodes=false");
        }
        else {
            usescancodes = 1;
            LOG_MSG("SDL_mapper: non-US keyboard detected, set usescancodes=true");
        }    
    }
#endif //defined(MACOSX)
}
void loadScanCode();
const char* DOS_GetLoadedLayout(void);
bool load=false;
bool prev_ret;
#endif // !defined(C_SDL2)

bool useScanCode() {
#if defined(C_SDL2)
	return false;
#else
	if (usescancodes==1)
		return true;
	else if (!usescancodes)
		return false;
	else {
		const char* layout_name = DOS_GetLoadedLayout();
		bool ret = layout_name != NULL && !IS_PC98_ARCH;
		if (!load)
			prev_ret=ret;
		else if (ret != prev_ret) {
			prev_ret=ret;
			loadScanCode();
			GFX_LosingFocus();
			MAPPER_Init();
			load=true;
		}
		return ret;
	}
#endif
}

SDLKey MapSDLCode(Bitu skey) {
//  LOG_MSG("MapSDLCode %d %X",skey,skey);
#if defined(C_SDL2)
	return (SDLKey)skey;
#else
	if (useScanCode()) {
		if (skey <= SDLK_LAST) return (SDLKey)skey;
        else return SDLK_UNKNOWN;
    } else return (SDLKey)skey;
#endif
}

Bitu GetKeyCode(SDL_keysym keysym) {
    // LOG_MSG("GetKeyCode %X %X %X",keysym.scancode,keysym.sym,keysym.mod);
    if (useScanCode()) {
        SDLKey key = SDLK_UNKNOWN;

#if defined (MACOSX)
        // zero value makes the keyboard crazy
        // Hack for 'a' key (QWERTY, QWERTZ), 'u' key (Turkish-F), 'q' key (AZERTY) on Mac
        // (FIX ME if there are other keys returning scancode 0)
        if((keysym.scancode == 0) &&
            ((keysym.sym == 'a') || (keysym.sym == 'u') || (keysym.sym == 'q'))) return 'a';
#endif

        if (keysym.scancode==0
#if defined (MACOSX)
            /* On Mac on US keyboards, scancode 0 is actually the 'a'
             * key.  For good measure exclude all printables from this
             * condition. */
             && (keysym.sym < SDLK_SPACE || keysym.sym > SDLK_WORLD_95)
#endif
			 ) {
                /* try to retrieve key from keysym.sym as scancode is zero */
                if (keysym.sym) {
				    key = (keysym.sym <= SDLK_LAST ? keysym.sym : SDLK_UNKNOWN);
			    }
        }
        else {
#if defined (WIN32)
#if(_WIN32_WINNT >= 0x0500)
            if(keysym.win32_vk >= 0xa6 && keysym.win32_vk <= 0xb7) return SDLK_UNKNOWN; // Ignore all media keys
#endif
            switch(keysym.scancode) {
            case 0x46:  // Scroll Lock
                // LOG_MSG("Scroll_lock scancode=%x, vk_key=%x", keysym.scancode, keysym.win32_vk);
                return (keysym.win32_vk == VK_CANCEL ? SDLK_BREAK : SDLK_SCROLLOCK);
            case 0x35:  // SLASH
                if(keysym.sym != SDLK_KP_DIVIDE) {
                    return SDLK_SLASH; // Various characters are allocated to this key in European keyboards
                }
            case 0x1c:  // ENTER
            case 0x1d:  // CONTROL
            case 0x37:  // PRINTSCREEN
            case 0x38:  // ALT
            case 0x45:  // PAUSE
            case 0x47:  // HOME
            case 0x48:  // cursor UP
            case 0x49:  // PAGE UP
            case 0x4b:  // cursor LEFT
            case 0x4d:  // cursor RIGHT
            case 0x4f:  // END
            case 0x50:  // cursor DOWN
            case 0x51:  // PAGE DOWN
            case 0x52:  // INSERT
            case 0x53:  // DELETE
            case 0x5b:  // Left Windows
            case 0x5c:  // Right Windows
            case 0x5d:  // App
                return keysym.sym; //Hardware scancodes of these keys are not unique, so virtual keycodes are used instead
                break;
            default:
#elif defined (__linux__)
            switch(keysym.scancode) { // Workaround for some of the keys return incorrect virtual keycodes 
            case 0x5E:
                return SDLK_LESS;    //DE Keyboard on Debian GNU/Linux 12 (bookworm) Debian 1:6.1.63-1+rpt1 (2023-11-24) aarch64 Linux 6.1.0-rpi7-rpi-v8
            case 0x60:
                return SDLK_F12;
            case 0x61:
                return SDLK_JP_RO;    //Hack: JP Keyboards
            case 0x64:
                return SDLK_WORLD_14; //Hack: JP Keyboards
            case 0x66:
                return SDLK_WORLD_13; //Hack: JP Keyboards
            case 0x68:
                return SDLK_KP_ENTER;
            case 0x69:
                return SDLK_RCTRL;
            case 0x6a:
                return SDLK_KP_DIVIDE;
            //case 0x6b:
            //    return SDLK_PRINTSCREEN;
            case 0x6c:
                return SDLK_RALT;
            case 0x6e:
                return SDLK_HOME;
            case 0x6f:
                return SDLK_UP;
            case 0x77:
                return SDLK_DELETE;
            case 0x7f:
                return SDLK_PAUSE;
            default:
#endif
                key = (keysym.scancode < MAX_SCANCODES ? sdlkey_map[keysym.scancode] : SDLK_UNKNOWN);
#if defined (WIN32) || defined (__linux__)
                break;
            }
#endif
        }
#if defined (WIN32)
		/* Special handling for JP Keyboard */
		if (key == SDLK_BACKQUOTE){
			key = (isJPkeyboard ? SDLK_WORLD_12 : SDLK_BACKQUOTE); // Set to Hankaku key if JP Keyboard
		}
#endif
		return key;
    } else {
#if defined (WIN32)
        /* special handling of 102-key under windows */
        if((keysym.sym == SDLK_BACKSLASH) && (keysym.scancode == 0x56)) return (Bitu)SDLK_LESS;
        /* special case of the \ _ key on Japanese 106-keyboards. seems to be the same code whether or not you've told Windows it's a 106-key */
        /* NTS: SDL source on Win32 maps this key (VK_OEM_102) to SDLK_LESS */
        else if(isJPkeyboard && (keysym.sym == 0) && (keysym.scancode == 0x73)) return (Bitu)SDLK_WORLD_10; //FIXME: There's no SDLK code for that key! Re-use one of the world keys!
        /* another hack, for the Yen \ pipe key on Japanese 106-keyboards.
           sym == 0 if English layout, sym == 0x5C if Japanese layout */
        else if(keysym.win32_vk == VK_CANCEL) return (Bitu)SDLK_BREAK;
#if(_WIN32_WINNT >= 0x0500)
        else if(keysym.win32_vk >= 0xa6 && keysym.win32_vk <= 0xb7) return (Bitu)SDLK_UNKNOWN; // Ignore all media keys
#endif
        if (isJPkeyboard && (keysym.sym == 0 || keysym.sym == 0x5C) && (keysym.scancode == 0x7D)) return (Bitu)SDLK_WORLD_11; //FIXME: There's no SDLK code for that key! Re-use one of the world keys!
        /* what is ~ ` on American keyboards is "Hankaku" on Japanese keyboards. Same scan code. */
		if (keysym.scancode == 0x29) return (Bitu) (isJPkeyboard ? SDLK_WORLD_12 : SDLK_BACKQUOTE); //if JP106 keyboard Hankaku else Backquote(grave)  
        /* Muhenkan */
        if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x7B) return (Bitu)SDLK_WORLD_13;
        /* Henkan/Zenkouho */
        if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x79) return (Bitu)SDLK_WORLD_14;
        /* Hiragana/Katakana */
        if (isJPkeyboard && keysym.sym == 0 && keysym.scancode == 0x70) return (Bitu)SDLK_WORLD_15;
#elif defined (__linux__)
        if ((keysym.sym == SDLK_BACKSLASH) && (keysym.scancode == 0x84)) return (Bitu)SDLK_JP_YEN;
        if ((keysym.sym == SDLK_BACKSLASH) && (keysym.scancode == 0x61)) return (Bitu)SDLK_JP_RO;
        if ((keysym.sym == SDLK_UNKNOWN) && (keysym.scancode == 0x31)) return (Bitu)SDLK_WORLD_12;
#endif
        if (keysym.sym) return (Bitu) (keysym.sym < SDLK_LAST ? keysym.sym : SDLK_UNKNOWN);
        else return (Bitu)(keysym.scancode < MAX_SCANCODES ? sdlkey_map[(keysym.scancode)] : SDLK_UNKNOWN);
    }
}

#endif /* !defined(C_SDL2) */

class CKeyBind : public CBind {
public:
    CKeyBind(CBindList * _list,SDLKey _key) : CBind(_list, CBind::keybind_t) {
        key = _key;
    }
    virtual ~CKeyBind() {}

    std::string CamelCase(const char* input)
    {
        auto text = std::string(input);

        auto caps = true;

        for(auto& c : text)
        {
            if(std::isalpha(c))
            {
                if(caps)
                {
                    c = std::toupper(c);
                    caps = false;
                }
                else
                {
                    c = std::tolower(c);
                }
            }
            else if(c == ' ')
            {
                caps = true;
            }
        }

        return text;
    }

    virtual void BindName(char * buf) override {
#if defined(C_SDL2)
        sprintf(buf,"Key %s",SDL_GetScancodeName(key));
#else
        const char *r=SDL_GetKeyName(key);
        if (!strcmp(r, "left super")) r = "left Windows";
        else if (!strcmp(r, "right super")) r = "right Windows";
        else if (!strcmp(r, "left meta")) r = "left Command";
        else if (!strcmp(r, "right meta")) r = "right Command";
        else if (!strcmp(r, "left ctrl")) r = "Left Ctrl";
        else if (!strcmp(r, "right ctrl")) r = "Right Ctrl";
        else if (!strcmp(r, "left alt")) r = "Left Alt";
        else if (!strcmp(r, "right alt")) r = "Right Alt";
        else if (!strcmp(r, "left shift")) r = "Left Shift";
        else if (!strcmp(r, "right shift")) r = "Right Shift";
		//LOG_MSG("Key %s", r);
		const auto str = CamelCase(r);
		sprintf(buf,"%s key",str.c_str());
#endif
    }
    virtual void ConfigName(char * buf) override {
#if defined(C_SDL2)
        sprintf(buf,"key %d",key);
#else
        sprintf(buf,"key %d",key);
#endif
    }
    virtual std::string GetBindMenuText(void) override {
        const char *s;
        std::string r,m;

#if defined(C_SDL2)
        s = SDL_GetScancodeName(key);
        if (s != NULL) r = s;
#else
        s = SDL_GetKeyName(key);
		if (s != NULL) {
			r = s;
			if (r.length()>0) {
				r[0]=toupper(r[0]);
				char *c=(char *)strstr(r.c_str(), " ctrl");
				if (c==NULL) c=(char *)strstr(r.c_str(), " alt");
				if (c==NULL) c=(char *)strstr(r.c_str(), " shift");
				if (c!=NULL) *(c+1)=toupper(*(c+1));
                else if (r=="Left super") r = "Left Windows";
                else if (r=="Right super") r = "Right Windows";
                else if (r=="Left meta") r = "Left Command";
                else if (r=="Right meta") r = "Right Command";
			}
		}
#endif

        m = event->type == CEvent::mod_event_t ? "" : GetModifierText();
        if (!m.empty()) r = m + "+" + r;

        return r;
    }
public:
    SDLKey key;
};

std::string CEvent::GetBindMenuText(void) {
    std::string r="", s="", t="";

    if (bindlist.empty())
        return std::string();

    bool first=true;
    for (auto i=bindlist.begin();i!=bindlist.end();i++) {
        CBind *b = *i;
        if (b == NULL) continue;
        if (b->type != CBind::keybind_t) continue;

        CKeyBind *kb = reinterpret_cast<CKeyBind*>(b);
        if (kb == NULL) continue;

        t = kb->GetBindMenuText();
        if (first) {
            first=false;
            r += t;
        }
        if (t!="Right Windows"&&t!="Left Windows"&&t!="Right Command"&&t!="Left Command"&&t!="Right Ctrl"&&t!="Left Ctrl"&&t!="Right Alt"&&t!="Left Alt"&&t!="Right Shift"&&t!="Left Shift") break;
        s += t;
    }
    if (s=="Right WindowsLeft Windows"||s=="Left WindowsRight Windows") r="Windows";
    else if (s=="Right CommandLeft Command"||s=="Left CommandRight Command") r="Command";
    else if (s=="Right CtrlLeft Ctrl"||s=="Left CtrlRight Ctrl") r="Ctrl";
    else if (s=="Right AltLeft Alt"||s=="Left AltRight Alt") r="Alt";
    else if (s=="Right ShiftLeft Shift"||s=="Left ShiftRight Shift") r="Shift";

    return r;
}

class CKeyBindGroup : public  CBindGroup {
public:
    CKeyBindGroup(Bitu _keys) : CBindGroup (){
        lists=new CBindList[_keys];
        for (Bitu i=0;i<_keys;i++) lists[i].clear();
        keys=_keys;
        configname="key";
    }
    virtual ~CKeyBindGroup() { delete[] lists; }
    CBind * CreateConfigBind(char *& buf) override {
        if (strncasecmp(buf,configname,strlen(configname))) return nullptr;
        StripWord(buf);char * num=StripWord(buf);
        Bitu code=(Bitu)ConvDecWord(num);
#if defined(C_SDL2)
        CBind * bind=CreateKeyBind((SDL_Scancode)code);
#else
        //if (useScanCode()) {
        //    if (code<MAX_SDLKEYS) code=scancode_map[code];
        //    else code=0;
        //}
        CBind * bind=CreateKeyBind((SDLKey)code);
#endif
        return bind;
    }
    CBind * CreateEventBind(SDL_Event * event) override {
        if (event->type!=SDL_KEYDOWN) return nullptr;
#if defined(C_SDL2)
        SDL_Scancode key = event->key.keysym.scancode;
#if defined(WIN32)
        if(key == SDL_SCANCODE_NONUSBACKSLASH) { // Special consideration for JP Keyboard
            key = (isJPkeyboard ? SDL_SCANCODE_INTERNATIONAL1 : SDL_SCANCODE_NONUSBACKSLASH);
        }
#endif
        return CreateKeyBind(key);
#else
	return CreateKeyBind((SDLKey)GetKeyCode(event->key.keysym));
#endif
    };
    bool CheckEvent(SDL_Event * event) override {
        if (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP) return false;
#if defined(C_SDL2)
        Bitu key = event->key.keysym.scancode;
#if defined(WIN32)
        if(key == SDL_SCANCODE_NONUSBACKSLASH) { // Special consideration for JP Keyboard
            key = (isJPkeyboard ? SDL_SCANCODE_INTERNATIONAL1 : SDL_SCANCODE_NONUSBACKSLASH);
        }
#endif
#else
		Bitu key;

		//key = (event->key.keysym.sym ? GetKeyCode(event->key.keysym) : sdlkey_map[(Bitu)(event->key.keysym.scancode)]);
		key = GetKeyCode(event->key.keysym);
		//assert(key < keys);
        if(key >= keys) {
            key = SDLK_UNKNOWN; // a test to avoid assertion failure (key < keys)
            // LOG_MSG("assertion failed: key: %x [keysym.sym:%x keysym.scancode: %x]", key, event->key.keysym.sym, event->key.keysym.scancode);
        }
#endif
//      LOG_MSG("key type %i is %x [%x %x]",event->type,key,event->key.keysym.sym,event->key.keysym.scancode);

#if defined(WIN32)
        /* HACK: When setting up the Japanese keyboard layout, I'm seeing some bizarre keyboard handling
                 from within Windows when pressing the ~ ` (grave) aka Hankaku key. I know it's not hardware
                 because when you switch back to English the key works normally as the tilde/grave key.
                 What I'm seeing is that pressing the key only sends the "down" event (even though you're
                 not holding the key down). When you press the key again, an "up" event is sent immediately
                 followed by a "down" event. This is confusing to the mapper, so we work around it here. */

#if defined(C_SDL2)
        if (isJPkeyboard && key == 0x35 && event->key.keysym.sym == 0x60)
#else
        //if (isJPkeyboard && event->key.keysym.scancode == 0x29 /*Hankaku*/ || (useScanCode() && key == 0x29 && event->key.keysym.sym == 0))
        if (isJPkeyboard && (key == SDLK_BACKQUOTE || key == SDLK_WORLD_12))
#endif
        {
            if (event->type == SDL_KEYDOWN) {
                // send down, then up (right?)
                ActivateBindList(&lists[key], 0x7fff, true);
                DeactivateBindList(&lists[key], true);
            }
            return 0; // ignore up event
        }
#endif

#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
        /* HACK: As requested on the issue tracker, on US keyboards, remap the Windows menu key
         *       to the "Ro" key.
         *
         *       If the user has un-bound the Ro key, then allow the menu key to go through
         *       unmodified.
         *
         * TODO: I understand that later PC-9821 systems (post Windows 95, prior to the platform's
         *       demise) actually did have Windows and Windows Menu keys.
         *
         *       I also understand from "Undocumented PC-98" that the actual keyboards for backwards
         *       compat reasons will not send these key codes unless a byte is sent to the keyboard
         *       on Windows 95 startup to tell the keyboard that it can send those keys.
         *
         *       This would explain why if you boot a PC-9821 system into Windows 95 "Safe mode"
         *       the Windows key does not work.
         *
         *       So later on, this remap should disable itself IF that byte is sent to signal
         *       Windows 95 is running and the Windows keys should be enabled. */
        if (IS_PC98_ARCH && host_keyboard_layout != DKM_JPN && key == SDLK_MENU) {
            CEvent *x = get_mapper_event_by_name("key_jp_ro");
            bool RoMenuRemap = false;

            if (x != NULL) {
                if (!x->bindlist.empty()) {
                    RoMenuRemap = true;

                    /* but, if the Ro key has been bound to Menu, do not remap */
                    auto &i = x->bindlist.front();
                    if (i->type == CBind::keybind_t) {
                        auto ik = reinterpret_cast<CKeyBind*>(i);
                        RoMenuRemap = false;
                        if (ik->key == SDLK_JP_RO)
                            RoMenuRemap = true;
                    }
                }
            }

            if (RoMenuRemap) key = SDLK_JP_RO;
        }
#endif

        if (event->type==SDL_KEYDOWN) ActivateBindList(&lists[key],0x7fff,true);
        else DeactivateBindList(&lists[key],true);
        return 0;
    }
    CBind * CreateKeyBind(SDLKey _key) {
#if !defined(C_SDL2)
        if (!useScanCode()) assert((Bitu)_key<keys);
#endif
        return new CKeyBind(&lists[(Bitu)_key],_key);
    }
private:
    const char * ConfigStart(void) override {
        return configname;
    }
    const char * BindStart(void) override {
        return "Key";
    }
protected:
    const char * configname;
    CBindList * lists;
    Bitu keys;
};

class CJAxisBind : public CBind {
public:
    CJAxisBind(CBindList * _list,CBindGroup * _group, Bitu _joystick, Bitu _axis,bool _positive) : CBind(_list){
        group = _group;
        axis = _axis;
        positive = _positive;
        joystick = _joystick;
    }
    virtual ~CJAxisBind() {}
    virtual void ConfigName(char * buf) override {
        sprintf(buf,"%s axis %d %d",group->ConfigStart(),(int)axis,positive ? 1 : 0);
    }
    virtual void BindName(char * buf) override {
        sprintf(buf,"%s Axis %d%s",group->BindStart(),(int)axis,positive ? "+" : "-");
    }

    //! \brief Gets the joystick index for this instance.
    Bitu GetJoystick() const { return joystick; };

    //! \brief Gets the axis index for this instance.
    Bitu GetAxis() const { return axis; }

    //! \brief Gets the axis direction for this instance.
    bool GetPositive() const { return positive; }

    //! \brief Gets the deadzone for a joystick axis direction.
    static int GetJoystickDeadzone(int joystick, int axis, bool positive)
    {
        auto section = control->GetSection("mapper");
        auto prop = static_cast<Section_prop*>(section);
        auto name = "joy" + std::to_string(joystick + 1) + "deadzone" + std::to_string(axis) + (positive ? "+" : "-");
        auto value = prop->Get_double(name);
        auto deadzone = static_cast<int>(value * 32767.0);
        return deadzone;
    }

    void ActivateBind(Bits _value, bool ev_trigger, bool skip_action = false) override
    {
        /* Since codebase is flawed, we do a simple hack:
         * If user-set deadzone exceeds hard-coded value of 25000 we just set it to 25001.
         * Other code works as usual, CTriggeredEvent does not have to check if it handles a joy axis.
         */

        // activate if we exceed user-defined deadzone
        const auto deadzone = GetJoystickDeadzone((int)this->GetJoystick(), (int)this->GetAxis(), this->GetPositive());

        if (_value > deadzone && event->IsTrigger()) 
            _value = 25000 + 1;

        CBind::ActivateBind(_value, ev_trigger, skip_action);
    }

protected:
    CBindGroup * group;
    Bitu axis;
    bool positive;
    Bitu joystick;
};

class CJButtonBind : public CBind {
public:
    CJButtonBind(CBindList * _list,CBindGroup * _group,Bitu _button) : CBind(_list) {
        group = _group;
        button=_button;
    }
    virtual ~CJButtonBind() {}
    virtual void ConfigName(char * buf) override {
        sprintf(buf,"%s button %d",group->ConfigStart(),(int)button);
    }
    virtual void BindName(char * buf) override {
        sprintf(buf,"%s Button %d",group->BindStart(),(int)button);
    }
protected:
    CBindGroup * group;
    Bitu button;
};

class CJHatBind : public CBind {
public:
    CJHatBind(CBindList * _list,CBindGroup * _group,Bitu _hat,uint8_t _dir) : CBind(_list) {
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
    virtual void ConfigName(char * buf) override {
        sprintf(buf,"%s hat %d %d",group->ConfigStart(),(int)hat,(int)dir);
    }
    virtual void BindName(char * buf) override {
        sprintf(buf,"%s Hat %d %s",group->BindStart(),(int)hat,(dir==SDL_HAT_UP)?"up":
                                                        ((dir==SDL_HAT_RIGHT)?"right":
                                                        ((dir==SDL_HAT_DOWN)?"down":"left")));
    }
protected:
    CBindGroup * group;
    Bitu hat;
    uint8_t dir;
};

class CStickBindGroup : public  CBindGroup {
public:
    CStickBindGroup(Bitu _stick,Bitu _emustick,bool _dummy=false) : CBindGroup (){
        stick=_stick;       // the number of the physical device (SDL numbering|)
        emustick=_emustick; // the number of the emulated device
        sprintf(configname,"stick_%d",(int)emustick);

        sdl_joystick=NULL;
        axes=0; buttons=0; hats=0;
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

        sdl_joystick=SDL_JoystickOpen((int)_stick);
        if (sdl_joystick==NULL) {
            button_wrap=emulated_buttons;
            return;
        }

        axes=(Bitu)SDL_JoystickNumAxes(sdl_joystick);
        if (axes > MAXAXIS) axes = MAXAXIS;
        axes_cap=emulated_axes;
        if (axes_cap>axes) axes_cap=axes;

        hats=(Bitu)SDL_JoystickNumHats(sdl_joystick);
        if (hats > MAXHAT) hats = MAXHAT;
        hats_cap=emulated_hats;
        if (hats_cap>hats) hats_cap=hats;

        buttons=(Bitu)SDL_JoystickNumButtons(sdl_joystick);
        button_wrap=buttons;
        button_cap=buttons;
        if (button_wrapping_enabled) {
            button_wrap=emulated_buttons;
            if (buttons>MAXBUTTON_CAP) button_cap = MAXBUTTON_CAP;
        }
        if (button_wrap > MAXBUTTON) button_wrap = MAXBUTTON;

#if defined(C_SDL2)
        LOG_MSG("Using joystick %s with %d axes, %d buttons and %d hat(s)",SDL_JoystickNameForIndex((int)stick),(int)axes,(int)buttons,(int)hats);
#else
        LOG_MSG("Using joystick %s with %d axes, %d buttons and %d hat(s)",SDL_JoystickName((int)stick),(int)axes,(int)buttons,(int)hats);
#endif

        // fetching these at every call simply freezes DOSBox at times so we do it once
        // (game tested : Terminal Velocity @ joystick calibration page)
        joy1dz1 = static_cast<float>(GetAxisDeadzone(0, 0));
        joy1rs1 = static_cast<float>(GetAxisResponse(0, 0));
        joy1dz2 = static_cast<float>(GetAxisDeadzone(0, 1));
        joy1rs2 = static_cast<float>(GetAxisResponse(0, 1));
        joy2dz1 = static_cast<float>(GetAxisDeadzone(1, 0));
        joy2rs1 = static_cast<float>(GetAxisResponse(1, 0));
    }
    virtual ~CStickBindGroup() {
        SDL_JoystickClose(sdl_joystick);
        if (pos_axis_lists != NULL) delete[] pos_axis_lists;
        if (neg_axis_lists != NULL) delete[] neg_axis_lists;
        if (button_lists != NULL) delete[] button_lists;
        if (hat_lists != NULL) delete[] hat_lists;
    }

    CBind * CreateConfigBind(char *& buf) override {
        if (is_dummy) return nullptr;
        if (strncasecmp(configname,buf,strlen(configname))) return nullptr;
        StripWord(buf);char * type=StripWord(buf);
        CBind * bind = nullptr;
        if (!strcasecmp(type,"axis")) {
            Bitu ax=(Bitu)ConvDecWord(StripWord(buf));
            bool pos=ConvDecWord(StripWord(buf)) > 0;
            bind=CreateAxisBind(ax,pos);
        } else if (!strcasecmp(type,"button")) {
            Bitu but=(Bitu)ConvDecWord(StripWord(buf));           
            bind=CreateButtonBind(but);
        } else if (!strcasecmp(type,"hat")) {
            Bitu hat=(Bitu)ConvDecWord(StripWord(buf));           
            uint8_t dir=(uint8_t)ConvDecWord(StripWord(buf));           
            bind=CreateHatBind(hat,dir);
        }
        return bind;
    }
    CBind * CreateEventBind(SDL_Event * event) override {
        if (event->type==SDL_JOYAXISMOTION) {
            if ((unsigned int)event->jaxis.which!=(unsigned int)stick) return nullptr;
#if defined (REDUCE_JOYSTICK_POLLING)
            if (event->jaxis.axis>=axes) return nullptr;
#endif
            if (abs(event->jaxis.value)<25000) return nullptr;
            return CreateAxisBind(event->jaxis.axis,event->jaxis.value>0);
        } else if (event->type==SDL_JOYBUTTONDOWN) {
            if ((unsigned int)event->jbutton.which!=(unsigned int)stick) return nullptr;
#if defined (REDUCE_JOYSTICK_POLLING)
            return CreateButtonBind(event->jbutton.button%button_wrap);
#else
            return CreateButtonBind(event->jbutton.button);
#endif
        } else if (event->type==SDL_JOYHATMOTION) {
            if ((unsigned int)event->jhat.which!=(unsigned int)stick) return nullptr;
            if (event->jhat.value==0) return nullptr;
            if (event->jhat.value>(SDL_HAT_UP|SDL_HAT_RIGHT|SDL_HAT_DOWN|SDL_HAT_LEFT)) return nullptr;
            return CreateHatBind(event->jhat.hat,event->jhat.value);
        } else return nullptr;
    }

    bool CheckEvent(SDL_Event * event) override {
        SDL_JoyAxisEvent * jaxis = NULL;
        SDL_JoyButtonEvent * jbutton = NULL;

        switch(event->type) {
            case SDL_JOYAXISMOTION:
                jaxis = &event->jaxis;
                if((unsigned int)jaxis->which == (unsigned int)stick) {
                    if(jaxis->axis == 0)
                        JOYSTICK_Move_X(emustick,(float)(jaxis->value/32768.0));
                    else if(jaxis->axis == 1)
                        JOYSTICK_Move_Y(emustick,(float)(jaxis->value/32768.0));
                }
                break;
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                if (emulated_buttons != 0) {
                    jbutton = &event->jbutton;
                    bool state;
                    state=jbutton->type==SDL_JOYBUTTONDOWN;
                    Bitu but = jbutton->button % emulated_buttons;
                    if ((unsigned int)jbutton->which == (unsigned int)stick) {
                        JOYSTICK_Button(emustick, but, state);
                    }
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

        auto v = GetJoystickVector((int)emustick, 0, 0, 1);
        JOYSTICK_Move_X(emustick, v.X);
        JOYSTICK_Move_Y(emustick, v.Y);
    }

    void ActivateJoystickBoundEvents() {
        if (GCC_UNLIKELY(sdl_joystick==NULL)) return;

        Bitu i;

        bool button_pressed[MAXBUTTON];
        for (i=0; i<MAXBUTTON; i++) button_pressed[i]=false;
        /* read button states */
        for (i=0; i<button_cap; i++) {
            if (SDL_JoystickGetButton(sdl_joystick,(int)i))
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
        
        int* axis_map = stick == 0 ? &joy1axes[0] : &joy2axes[0];
        for (i=0; i<axes; i++) {
            int i1 = axis_map[i];
            Sint16 caxis_pos=SDL_JoystickGetAxis(sdl_joystick,i1);
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
            Uint8 chat_state=SDL_JoystickGetHat(sdl_joystick,(int)i);

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
    float joy1dz1 = 0, joy1rs1 = 0, joy1dz2 = 0, joy1rs2 = 0, joy2dz1 = 0, joy2rs1 = 0;
    CBind * CreateAxisBind(Bitu axis,bool positive) {
        if (axis<axes) {
            if (positive) return new CJAxisBind(&pos_axis_lists[axis],this,stick,axis,positive);
            else return new CJAxisBind(&neg_axis_lists[axis],this,stick,axis,positive);
        }
        return NULL;
    }
    CBind * CreateButtonBind(Bitu button) {
        if (button<button_wrap) 
            return new CJButtonBind(&button_lists[button],this,button);
        return NULL;
    }
    CBind * CreateHatBind(Bitu hat,uint8_t value) {
        if (hat < hats_cap) return NULL;
        Bitu hat_dir;
        if (value&SDL_HAT_UP) hat_dir=0;
        else if (value&SDL_HAT_RIGHT) hat_dir=1;
        else if (value&SDL_HAT_DOWN) hat_dir=2;
        else if (value&SDL_HAT_LEFT) hat_dir=3;
        else return NULL;
        return new CJHatBind(&hat_lists[(hat<<2)+hat_dir],this,hat,value);
    }
    const char * ConfigStart(void) override {
        return configname;
    }
    const char * BindStart(void) override {
#if defined(C_SDL2)
        if (sdl_joystick!=NULL) return SDL_JoystickNameForIndex((int)stick);
#else
        if (sdl_joystick!=NULL) return SDL_JoystickName((int)stick);
#endif
        else return "[missing joystick]";
    }

    static float GetAxisDeadzone(int joystick, int thumbStick)
    {
        auto section = control->GetSection("joystick");
        auto prop = static_cast<Section_prop*>(section);
        auto name = "joy" + std::to_string(joystick + 1) + "deadzone" + std::to_string(thumbStick + 1);
        auto deadzone = static_cast<float>(prop->Get_double(name));
        return deadzone;
    }
    
    static float GetAxisResponse(int joystick, int thumbStick)
    {
        auto section = control->GetSection("joystick");
        auto prop = static_cast<Section_prop*>(section);
        auto name = "joy" + std::to_string(joystick + 1) + "response" + std::to_string(thumbStick + 1);
        auto response = static_cast<float>(prop->Get_double(name));
        return response;
    }

    static void ProcessInput(int16_t x, int16_t y, float deadzone, DOSBox_Vector2& joy)
    {
        // http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html

        joy = DOSBox_Vector2((x + 0.5f) / 32767.5f, (y + 0.5f) / 32767.5f);

        float m = joy.magnitude();
        DOSBox_Vector2 n = joy.normalized();
        joy = m < deadzone ? DOSBox_Vector2() : n * ((m - deadzone) / (1.0f - deadzone));

        DOSBox_Vector2 min = DOSBox_Vector2(-1.0f, -1.0f);
        DOSBox_Vector2 max = DOSBox_Vector2(+1.0f, +1.0f);
        joy = joy.clamp(min, max);
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
    Bitu button_autofire[MAXBUTTON] = {};
    bool old_button_state[MAXBUTTON] = {};
    bool old_pos_axis_state[MAXAXIS] = {};
    bool old_neg_axis_state[MAXAXIS] = {};
    Uint8 old_hat_state[16] = {};
    bool is_dummy;

    DOSBox_Vector2 GetJoystickVector(int joystick, int thumbStick, int xAxis, int yAxis) const
    {
        int16_t x = virtual_joysticks[joystick].axis_pos[xAxis];
        int16_t y = virtual_joysticks[joystick].axis_pos[yAxis];
        float deadzone;
        float response;
        if (joystick == 0)
        {
            if (thumbStick == 0)
            {
                deadzone = joy1dz1;
                response = joy1rs1;
            }
            else
            {
                deadzone = joy1dz2;
                response = joy1rs2;
            }
        }
        else
        {
            deadzone = joy2dz1;
            response = joy2rs1;
        }
        DOSBox_Vector2 v;
        ProcessInput(x, y, deadzone, v);
        float x1 = (sgn(v.X) * pow(fabs(v.X), response));
        float y1 = (sgn(v.Y) * pow(fabs(v.Y), response));
        DOSBox_Vector2 v1(x1, y1);
        return v1;
    }
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

    bool CheckEvent(SDL_Event * event) override {
        SDL_JoyAxisEvent * jaxis = NULL;
        SDL_JoyButtonEvent * jbutton = NULL;
        Bitu but = 0;

        switch(event->type) {
            case SDL_JOYAXISMOTION:
                jaxis = &event->jaxis;
                if((unsigned int)jaxis->which == (unsigned int)stick && jaxis->axis < 4) {
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
                if ((unsigned int)jbutton->which == (unsigned int)stick) {
                    JOYSTICK_Button((but >> 1),(but & 1),state);
                }
                break;
        }
        return false;
    }

    void UpdateJoystick() override {
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

        auto v1 = GetJoystickVector(0, 0, 0, 1);
        auto v2 = GetJoystickVector(0, 1, 2, 3);
        JOYSTICK_Move_X(0, v1.X);
        JOYSTICK_Move_Y(0, v1.Y);
        JOYSTICK_Move_X(1, v2.X);
        JOYSTICK_Move_Y(1, v2.Y);
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
        //hats_cap=emulated_hats;
        //if (hats_cap>hats) hats_cap=hats;

        JOYSTICK_Enable(1,true);
        JOYSTICK_Move_Y(1,1.0);
    }
    virtual ~CFCSBindGroup() {}

    bool CheckEvent(SDL_Event * event) override {
        SDL_JoyAxisEvent * jaxis = NULL;
        SDL_JoyButtonEvent * jbutton = NULL;
        SDL_JoyHatEvent * jhat = NULL;
        Bitu but = 0;

        switch(event->type) {
            case SDL_JOYAXISMOTION:
                jaxis = &event->jaxis;
                if((unsigned int)jaxis->which == (unsigned int)stick) {
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
                if((unsigned int)jhat->which == (unsigned int)stick) DecodeHatPosition(jhat->value);
                break;
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                jbutton = &event->jbutton;
                bool state;
                state=jbutton->type==SDL_JOYBUTTONDOWN;
                but = jbutton->button % emulated_buttons;
                if ((unsigned int)jbutton->which == (unsigned int)stick) {
                        JOYSTICK_Button((but >> 1),(but & 1),state);
                }
                break;
        }
        return false;
    }

    void UpdateJoystick() override {
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

        auto v1 = GetJoystickVector(0, 0, 0, 1);
        auto v2 = GetJoystickVector(0, 1, 2, 3);
        JOYSTICK_Move_X(0, v1.X);
        JOYSTICK_Move_Y(0, v1.Y);
        JOYSTICK_Move_X(1, v2.X);

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

    bool CheckEvent(SDL_Event * event) override {
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
                if((unsigned int)jaxis->which == (unsigned int)stick && jaxis->axis < 4) {
                    if(jaxis->axis & 1)
                        JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
                    else
                        JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
                }
                break;
            case SDL_JOYHATMOTION:
                jhat = &event->jhat;
                if((unsigned int)jhat->which == (unsigned int)stick && jhat->hat < 2) {
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
                if ((unsigned int)jbutton->which == (unsigned int)stick)
                    button_state|=button_magic[but];
                break;
            case SDL_JOYBUTTONUP:
                jbutton = &event->jbutton;
                but = jbutton->button % emulated_buttons;
                if ((unsigned int)jbutton->which == (unsigned int)stick)
                    button_state&=~button_magic[but];
                break;
        }

        unsigned i;
        uint16_t j;
        j=button_state;
        for(i=0;i<16;i++) if (j & 1) break; else j>>=1;
        JOYSTICK_Button(0,0,i&1);
        JOYSTICK_Button(0,1,(i>>1)&1);
        JOYSTICK_Button(1,0,(i>>2)&1);
        JOYSTICK_Button(1,1,(i>>3)&1);
        return false;
    }

    void UpdateJoystick() override {
        static unsigned const button_priority[6]={7,11,13,14,5,6};
        static unsigned const hat_priority[2][4]={{0,1,2,3},{8,9,10,12}};

        /* query SDL joystick and activate bindings */
        ActivateJoystickBoundEvents();

        auto v1 = GetJoystickVector(0, 0, 0, 1);
        auto v2 = GetJoystickVector(0, 1, 2, 3);
        JOYSTICK_Move_X(0, v1.X);
        JOYSTICK_Move_X(0, v1.Y);
        JOYSTICK_Move_X(1, v2.X);
        JOYSTICK_Move_X(1, v2.Y);

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
    uint16_t button_state;
};

void CBindGroup::ActivateBindList(CBindList * list,Bits value,bool ev_trigger) {
	assert(list);
    Bitu validmod=0;
    CBindList_it it;
    for (it=list->begin();it!=list->end();++it) {
        if (((*it)->mods & mapper.mods) == (*it)->mods) {
            if (validmod<(*it)->mods) validmod=(*it)->mods;
        }
    }
    for (it=list->begin();it!=list->end();++it) {
        if ((*it)->mods==MMODHOST) {
            if ((!hostkeyalt&&validmod==(*it)->mods)||(hostkeyalt==1&&(sdl.lctrlstate==SDL_KEYDOWN||sdl.rctrlstate==SDL_KEYDOWN)&&(sdl.laltstate==SDL_KEYDOWN||sdl.raltstate==SDL_KEYDOWN))||(hostkeyalt==2&&(sdl.lctrlstate==SDL_KEYDOWN||sdl.rctrlstate==SDL_KEYDOWN)&&(sdl.lshiftstate==SDL_KEYDOWN||sdl.rshiftstate==SDL_KEYDOWN))||(hostkeyalt==3&&(sdl.laltstate==SDL_KEYDOWN||sdl.raltstate==SDL_KEYDOWN)&&(sdl.lshiftstate==SDL_KEYDOWN||sdl.rshiftstate==SDL_KEYDOWN))) {
                if (hostkeyalt != 0) /* only IF using an alternate host key */
                    (*it)->flags|=BFLG_Hold_Temporary;

                (*it)->ActivateBind(value,ev_trigger);
            }
        } else if (validmod==(*it)->mods)
            (*it)->ActivateBind(value,ev_trigger);
    }
}

void CBindGroup::DeactivateBindList(CBindList * list,bool ev_trigger) {
	assert(list);
    CBindList_it it;
    for (it=list->begin();it!=list->end();++it) {
        (*it)->flags&=~BFLG_Hold_Temporary;
        (*it)->DeActivateBind(ev_trigger);
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
        invert=false;
        press=false;
        page=1;
        SetCanClick(true);
    }
    virtual void Draw(bool background, bool border) {
        uint8_t bg;

        if (!enabled) return;

        if (!invert)
            bg = press ? uint8_t(CLR_DARKGREEN) : bkcolor;
        else
            bg = color;

#if defined(C_SDL2)
        uint8_t * point=((uint8_t *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
        uint8_t * point=((uint8_t *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
        for (Bitu lines=0;lines<dy;lines++)  {
            if (lines==0 || lines==(dy-1)) {
                if (border)
                {
                    for (Bitu cols=0;cols<dx;cols++)
                    {
                        *(point+cols)=color;
                    }
                }
            } else {
                if (background)
                {
                    for (Bitu cols=1;cols<(dx-1);cols++)
                    {
                        *(point+cols)=bg;
                    }
                }

                if (border)
                {
                    *point=color;
                    *(point+dx-1)=color;
                }
            }
#if defined(C_SDL2)
            point+=mapper.draw_surface->w;
#else
            point+=mapper.surface->pitch;
#endif
        }
    }
    virtual void Draw(void) {
        Draw(true, true);
    }
    virtual bool OnTop(Bitu _x,Bitu _y) {
#if defined(C_SDL2)
        const auto scale = mapper.window_scale;
        _x /= scale;
        _y /= scale;
#endif
        return ( enabled && (_x>=x) && (_x<x+dx) && (_y>=y) && (_y<y+dy));
    }
    virtual void BindColor(void) {}

    virtual void Click(void)
    {
        if(clickable)
        {
            ClickImpl();
        }
    }

    virtual void ClickImpl(void) {}

    bool GetCanClick(void)
    {
        return clickable;
    }

    void SetCanClick(bool value)
    {
        clickable = value;
        SetColor(value ? CLR_WHITE : CLR_GREY); // TODO don't hardcode
    }

    uint8_t Page(uint8_t p) {
        if (p>0) page=p;
        return page;
    }
    void Enable(bool yes) {
        enabled=yes; 
        mapper.redraw=true;
    }
    void SetInvert(bool inv) {
        invert=inv;
        mapper.redraw=true;
    }
    void SetPress(bool p) {
        press=p;
        mapper.redraw=true;
    }
    virtual void RebindRedraw(void) {}
    void SetColor(uint8_t _col) { color=_col; }
protected:
    Bitu x,y,dx,dy;
    uint8_t page;
    uint8_t color;
    uint8_t bkcolor;
    bool press;
    bool invert;
    bool enabled;
    bool clickable;

    void DrawTextAuto(const char* text, bool centered, uint8_t foreground, uint8_t background)
    {
        const auto size = strlen(text);
        const auto wide = dx / 8;
        const auto data = dx > 0 && size > wide ? std::string(text, wide - 3) + std::string("...") : std::string(text);

        if(centered)
        {
            const auto size = data.size();
            const auto xPos = std::max(x, (Bitu) (x + dx / 2 - size * 8 / 2));
            const auto yPos = std::max(y, y + dy / 2 - 14 / 2);
            DrawText(1 + xPos, yPos, data.c_str(), foreground, background);
        }
        else
        {
            DrawText(x + 2, y + 2, data.c_str(), foreground, background);
        }
    }
};

static CButton *press_select = NULL;

class CTextButton : public CButton {
public:
    CTextButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text) : CButton(_x,_y,_dx,_dy) {
        if (strlen(_text)<100) strcpy(text, _text);
        else {strncpy(text, _text, 99);text[99]=0;}
        invertw=0;
    }
    ~CTextButton() {}
    void Draw(void) override {
        uint8_t fg,bg;

        if (!enabled) return;

        if (!invert) {
            fg = color;
            bg = press ? uint8_t(CLR_DARKGREEN) : bkcolor;
        }
        else {
            fg = bkcolor;
            bg = color;
        }

        CButton::Draw();

        DrawTextAuto(text, true, fg, bg); // TODO add the ability to center text for this class

#if defined(C_SDL2)
        uint8_t * point=((uint8_t *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
        uint8_t * point=((uint8_t *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
        for (Bitu lines=0;lines<(dy-1);lines++) {
            if (lines != 0) {
                for (Bitu cols=1;cols<=invertw;cols++) {
                    if (*(point+cols) == color)
                        *(point+cols) = bkcolor;
                    else
                        *(point+cols) = color;
                }
            }
#if defined(C_SDL2)
            point+=mapper.draw_surface->w;
#else
            point+=mapper.surface->pitch;
#endif
            // draw the borders a second time to prevent the text from overwriting them
            CButton::Draw(false, true);
        }
    }
    void SetText(const char *_text) {
        if (strlen(_text)<100) strcpy(text, _text);
        else {strncpy(text, _text, 99);text[99]=0;}
    }
    void SetPartialInvert(double a) {
        if (a < 0) a = 0;
        if (a > 1) a = 1;
        invertw = (Bitu)floor((a * (dx - 2)) + 0.5);
        if (invertw > (dx - 2)) invertw = dx - 2;
        mapper.redraw=true;
    }
protected:
    char text[100];
    Bitu invertw;
};

class CEventButton;
static CEventButton * hostbutton = NULL;
static CEventButton * last_clicked = NULL;
static std::vector<CEventButton *> ceventbuttons;

class CEventButton : public CTextButton {
public:
    CEventButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,CEvent * _event) 
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
        event=_event;   
    }
    ~CEventButton() {}
    void BindColor(void) override {
        this->SetColor(event->bindlist.begin() == event->bindlist.end() ? CLR_GREY : CLR_WHITE);
    }
    void ClickImpl(void) override {
        if (last_clicked) last_clicked->BindColor();
        this->SetColor(event->bindlist.begin() == event->bindlist.end() ? CLR_DARKGREEN : CLR_GREEN);
        SetActiveEvent(event);
        last_clicked=this;
    }
    CEvent *GetEvent() {
        return event;
    }
    void RebindRedraw(void) override {
        Click();//HACK!
    }
protected:
    CEvent * event;
};

class CCaptionButton : public CButton {
public:
    CCaptionButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy, bool centered) : CButton(_x,_y,_dx,_dy){
        caption[0]=0;
        center = centered;
    }
    virtual ~CCaptionButton() {}
    void Change(const char * format,...) GCC_ATTRIBUTE(__format__(__printf__,2,3));

    void Draw(void) override
    {
        if(!enabled)
            return;

        DrawTextAuto(caption, center, color, CLR_BLACK);
    }

protected:
    char caption[128];
private:
    bool center;
};

void CCaptionButton::Change(const char * format,...) {
    va_list msg;
    va_start(msg,format);
    vsprintf(caption,format,msg);
    va_end(msg);
    mapper.redraw=true;
}       

void RedrawMapperBindButton(CEvent *ev), RedrawMapperEventButtons();
class CBindButton : public CTextButton {
public: 
    CBindButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BB_Types _type) 
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
        type=_type;
    }
    ~CBindButton() {}
    void ClickImpl(void) override {
        switch (type) {
        case BB_Add: 
            mapper.addbind=true;
            SetActiveBind(nullptr);
            change_action_text(MSG_Get("PRESS_JOYSTICK_KEY"),CLR_RED);
            break;
        case BB_Del:
            assert(mapper.aevent != NULL);
            if (mapper.abindit!=mapper.aevent->bindlist.end())  {
                delete (*mapper.abindit);
                mapper.abindit=mapper.aevent->bindlist.erase(mapper.abindit);
                if (mapper.abindit==mapper.aevent->bindlist.end()) 
                    mapper.abindit=mapper.aevent->bindlist.begin();
            }
            if (mapper.abindit!=mapper.aevent->bindlist.end()) SetActiveBind(*(mapper.abindit));
            else SetActiveBind(nullptr);
            RedrawMapperBindButton(mapper.aevent);
            break;
        case BB_Next:
            assert(mapper.aevent != NULL);
            if (mapper.abindit!=mapper.aevent->bindlist.end()) 
                ++mapper.abindit;
            if (mapper.abindit==mapper.aevent->bindlist.end()) 
                mapper.abindit=mapper.aevent->bindlist.begin();
            SetActiveBind(*(mapper.abindit));
            break;
        case BB_Prevpage:
            if (cpage<2) break;
            cpage--;
            RedrawMapperEventButtons();
            break;
        case BB_Nextpage:
            if (cpage>=maxpage) break;
            cpage++;
            RedrawMapperEventButtons();
            break;
        case BB_Save:
            MAPPER_SaveBinds();
            break;
        case BB_Exit:   
            mapper.exit=true;
            break;
        case BB_Capture:
            GFX_CaptureMouse();
            if (mouselocked) change_action_text(MSG_Get("CAPTURE_ENABLED"),CLR_WHITE);
            break;
        }
    }
protected:
    BB_Types type;
};

//! \brief Modifier trigger event, for modifier keys. This permits the user to change modifier key bindings.
class CModEvent : public CTriggeredEvent {
public:
    //! \brief Constructor to provide entry name and the index of the modifier button
    CModEvent(char const * const _entry,Bitu _wmod) : CTriggeredEvent(_entry), notify_button(NULL) {
        wmod=_wmod;
        type = wmod==4?event_t:mod_event_t;
    }

    ~CModEvent() {}

    void Active(bool yesno) override {
        if (notify_button != NULL)
            notify_button->SetInvert(yesno);

        if (yesno) mapper.mods|=((Bitu)1u << (wmod-1u));
        else mapper.mods&=~((Bitu)1u << (wmod-1u));
    };

    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Mapper UI text button to indicate status by
    CTextButton *notify_button;
protected:
    //! \brief Modifier button index
    Bitu wmod;
};

class CCheckButton : public CTextButton {
public: 
    CCheckButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BC_Types _type) 
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
        type=_type;
    }
    ~CCheckButton() {}
    void Draw(void) override {
        if (!enabled) return;
        bool checked=false;
        std::string str = "";
        switch (type) {
        case BC_Mod1:
            checked=(mapper.abind->mods&BMOD_Mod1)>0;
            str = checked && mod_event[1] != NULL ? mod_event[1]->GetBindMenuText() : LabelMod1;
            strcpy(text, str.size()>8?LabelMod1:str.c_str());
            break;
        case BC_Mod2:
            checked=(mapper.abind->mods&BMOD_Mod2)>0;
            str = checked && mod_event[2] != NULL ? mod_event[2]->GetBindMenuText() : LabelMod2;
            strcpy(text, str.size()>8?LabelMod2:str.c_str());
            break;
        case BC_Mod3:
            checked=(mapper.abind->mods&BMOD_Mod3)>0;
            str = checked && mod_event[3] != NULL ? mod_event[3]->GetBindMenuText() : LabelMod3;
            strcpy(text, str.size()>8?LabelMod3:str.c_str());
            break;
        case BC_Host:
            checked=(mapper.abind->mods&BMOD_Host)>0;
            break;
        case BC_Hold:
            checked=(mapper.abind->flags&BFLG_Hold)>0;
            break;
        }
        CTextButton::Draw();
        if (checked) {
#if defined(C_SDL2)
            uint8_t * point=((uint8_t *)mapper.draw_surface->pixels)+((y+2)*mapper.draw_surface->pitch)+x+dx-dy+2;
#else
            uint8_t * point=((uint8_t *)mapper.surface->pixels)+((y+2)*mapper.surface->pitch)+x+dx-dy+2;
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
    }
    void ClickImpl(void) override {
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
        case BC_Host:
            mapper.abind->mods^=BMOD_Host;
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

//! \brief Keyboard key trigger event
class CKeyEvent : public CTriggeredEvent {
public:
    //! \brief Constructor to specify mapper event, and KBD_* key enumeration constant
    CKeyEvent(char const * const _entry,KBD_KEYS _key) : CTriggeredEvent(_entry), notify_button(NULL) {
        key=_key;
    }

    ~CKeyEvent() {}

    void Active(bool yesno) override {
        if (MAPPER_DemoOnly()) {
            if (notify_button != NULL)
                notify_button->SetInvert(yesno);
        }
        else {
            KEYBOARD_AddKey(key,yesno);
        }

        active=yesno;
    };

    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Text button in the mapper UI to indicate our status by
    CTextButton *notify_button;

    //! \brief KBD_* key enumeration value to transmit to keyboard emulation
    KBD_KEYS key;
};

class CMouseButtonEvent : public CTriggeredEvent {
public:
	CMouseButtonEvent(char const * const _entry,uint8_t _button) : CTriggeredEvent(_entry) {
		button=_button;
        notify_button=NULL;
	}
	void Active(bool yesno) override {
		if (yesno)
			Mouse_ButtonPressed(button);
		else
			Mouse_ButtonReleased(button);
	}
    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Text button in the mapper UI to indicate our status by
    CTextButton *notify_button;

	uint8_t button;
};

//! \brief Joystick axis event handling for the mapper
class CJAxisEvent : public CContinuousEvent {
public:
    //! \brief Constructor, to describe entry, joystick, axis, direction, and the opposing axis
    CJAxisEvent(char const * const _entry,Bitu _stick,Bitu _axis,bool _positive,CJAxisEvent * _opposite_axis) : CContinuousEvent(_entry) {
        notify_button=NULL;
        stick=_stick;
        axis=_axis;
        positive=_positive;
        opposite_axis=_opposite_axis;
        if (_opposite_axis) {
            _opposite_axis->SetOppositeAxis(this);
        }
    }

    ~CJAxisEvent() {}

    void Active(bool /*moved*/) override {
        if (notify_button != NULL)
            notify_button->SetPartialInvert(GetValue()/32768.0);

        virtual_joysticks[stick].axis_pos[axis]=(int16_t)(GetValue()*(positive?1:-1));
    }

    Bitu GetActivityCount(void) override {
        return activity|opposite_axis->activity;
    }

    void RepostActivity(void) override {
        /* caring for joystick movement into the opposite direction */
        opposite_axis->Active(true);
    }

    //! \brief Associate this object with a text button in the mapper GUI so that joystick position can be displayed at all times
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Text button to use to display joystick position
    CTextButton *notify_button;
protected:
    //! \brief Associate this object with the opposing joystick axis
    void SetOppositeAxis(CJAxisEvent * _opposite_axis) {
        opposite_axis=_opposite_axis;
    }

    //! \brief Joystick to follow
    Bitu stick;

    //! \brief Joystick axis to track
    Bitu axis;

    //! \brief Whether joystick axis is positive or negative
    bool positive;

    //! \brief Opposing joystick axis object
    CJAxisEvent * opposite_axis;
};

//! \brief Joystick button trigger
class CJButtonEvent : public CTriggeredEvent {
public:
    //! \brief Constructor, describing mapper event, joystick, and which button
    CJButtonEvent(char const * const _entry,Bitu _stick,Bitu _button) : CTriggeredEvent(_entry) {
        stick=_stick;
        button=_button;
        notify_button=NULL;
    }

    ~CJButtonEvent() {}
    
    void Active(bool pressed) override {
        if (notify_button != NULL)
            notify_button->SetInvert(pressed);

        virtual_joysticks[stick].button_pressed[button]=pressed;
        active=pressed;
    }
    
    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Text button in the mapper UI to indicate our status by
    CTextButton *notify_button;
protected:
    //! \brief Which joystick
    Bitu stick;

    //! \brief Which button
    Bitu button;
};

//! \brief Joystick hat event
class CJHatEvent : public CTriggeredEvent {
public:
    //! \brief Constructor to describe mapper event, joystick, hat, and direction
    CJHatEvent(char const * const _entry,Bitu _stick,Bitu _hat,Bitu _dir) : CTriggeredEvent(_entry) {
        stick=_stick;
        hat=_hat;
        dir=_dir;
        notify_button = NULL;
    }

    ~CJHatEvent() {}

    void Active(bool pressed) override {
        if (notify_button != NULL)
            notify_button->SetInvert(pressed);
        virtual_joysticks[stick].hat_pressed[(hat<<2)+dir]=pressed;
    }
    void notifybutton(CTextButton *n)
    {
        notify_button = n;
    }

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    CTextButton *notify_button;
protected:
    //! \brief Which joystick
    Bitu stick;

    //! \brief Which hat
    Bitu hat;

    //! \brief Direction of hat
    Bitu dir;
};

std::string CBind::GetModifierText(void) {
    std::string r,t;

    for (size_t m=4u/*Host key first*/;m >= 1u;m--) {
        if ((mods & ((Bitu)1u << (m - 1u))) && mod_event[m] != NULL) {
            t = mod_event[m]->GetBindMenuText();
            if (!r.empty()) r += "+";
            r += m==4?(hostkeyalt==1?"Ctrl+Alt":(hostkeyalt==2?"Ctrl+Shift":(hostkeyalt==3?"Alt+Shift":t))):t;
        }
    }

    return r;
}

//! \brief Mapper shortcut event. Keyboard triggerable only.
class CHandlerEvent : public CTriggeredEvent {
public:
    //! \brief Constructor, to specify the entry, handler (callback), key (according to MapKeys enumeration), and text to display for the shortcut in the mapper UO
    CHandlerEvent(char const * const _entry,MAPPER_Handler * _handler,MapKeys _key,Bitu _mod,char const * const _buttonname) : CTriggeredEvent(_entry), notify_button(NULL) {
        handler=_handler;
        defmod=_mod;
        defkey=_key;
        if (strlen(_buttonname)<100) strcpy(buttonname, _buttonname);
        else {strncpy(buttonname, _buttonname, 99);buttonname[99]=0;}
        handlergroup.push_back(this);
        type = handler_event_t;
    }

    ~CHandlerEvent() {}

    void RebindRedraw(void) override {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    void Active(bool yesno) override {
        if (MAPPER_DemoOnly()) {
            if (notify_button != NULL)
                notify_button->SetInvert(yesno);
        }
        else {
            (*handler)(yesno);
        }

        active=yesno;
    };

    //! \brief Retrieve the button name (for display in the mapper UI)
    const char * ButtonName() {
        return buttonname;
    }

    void SetButtonName(const char *name) {
        if (strlen(name)<100) strcpy(buttonname, name);
        else {strncpy(buttonname, name, 99);buttonname[99]=0;}
    }

    //! \brief Generate a default binding from the MapKeys enumeration
#if defined(C_SDL2)
    void MakeDefaultBind(char * buf) {
        Bitu key=0;
        switch (defkey) {
        case MK_nothing: *buf = 0; return;
        case MK_f1:case MK_f2:case MK_f3:case MK_f4:
        case MK_f5:case MK_f6:case MK_f7:case MK_f8:
        case MK_f9:case MK_f10:case MK_f11:case MK_f12: 
            key=SDL_SCANCODE_F1+(defkey-MK_f1);
            break;
        case MK_rightarrow:
            key=SDL_SCANCODE_RIGHT;
            break;
        case MK_leftarrow:
            key=SDL_SCANCODE_LEFT;
            break;
        case MK_uparrow:
            key=SDL_SCANCODE_UP;
            break;
        case MK_downarrow:
            key=SDL_SCANCODE_DOWN;
            break;
        case MK_return:
            key=SDL_SCANCODE_RETURN;
            break;
        case MK_tab:
            key=SDL_SCANCODE_TAB;
            break;
        case MK_slash:
            key=SDL_SCANCODE_SLASH;
            break;
        case MK_backslash:
            key=SDL_SCANCODE_BACKSLASH;
            break;
        case MK_space:
            key=SDL_SCANCODE_SPACE;
            break;
        case MK_backspace:
            key=SDL_SCANCODE_BACKSPACE;
            break;
        case MK_delete:
            key=SDL_SCANCODE_DELETE;
            break;
        case MK_insert:
            key=SDL_SCANCODE_INSERT;
            break;
        case MK_semicolon:
            key=SDL_SCANCODE_SEMICOLON;
            break;
        case MK_quote:
            key=SDL_SCANCODE_APOSTROPHE;
            break;
        case MK_grave:
            key=SDL_SCANCODE_GRAVE;
            break;
        case MK_kpminus:
            key=SDL_SCANCODE_KP_MINUS;
            break;
        case MK_kpplus:
            key=SDL_SCANCODE_KP_PLUS;
            break;
        case MK_minus:
            key=SDL_SCANCODE_MINUS;
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
        case MK_end:
            key=SDL_SCANCODE_END;
            break;
        case MK_pageup:
            key=SDL_SCANCODE_PAGEUP;
            break;
        case MK_pagedown:
            key=SDL_SCANCODE_PAGEDOWN;
            break;
        case MK_comma:
            key=SDL_SCANCODE_COMMA;
            break;
        case MK_period:
            key=SDL_SCANCODE_PERIOD;
            break;
        case MK_0:
            key=SDL_SCANCODE_0;
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
        case MK_5:
            key=SDL_SCANCODE_5;
            break;
        case MK_6:
            key=SDL_SCANCODE_6;
            break;
        case MK_7:
            key=SDL_SCANCODE_7;
            break;
        case MK_8:
            key=SDL_SCANCODE_8;
            break;
        case MK_9:
            key=SDL_SCANCODE_9;
            break;
        case MK_a:
            key=SDL_SCANCODE_A;
            break;
        case MK_b:
            key=SDL_SCANCODE_B;
            break;
        case MK_c:
            key=SDL_SCANCODE_C;
            break;
        case MK_d:
            key=SDL_SCANCODE_D;
            break;
        case MK_e:
            key=SDL_SCANCODE_E;
            break;
        case MK_f:
            key=SDL_SCANCODE_F;
            break;
        case MK_g:
            key=SDL_SCANCODE_G;
            break;
        case MK_h:
            key=SDL_SCANCODE_H;
            break;
        case MK_i:
            key=SDL_SCANCODE_I;
            break;
        case MK_j:
            key=SDL_SCANCODE_J;
            break;
        case MK_k:
            key=SDL_SCANCODE_K;
            break;
        case MK_l:
            key=SDL_SCANCODE_L;
            break;
        case MK_m:
            key=SDL_SCANCODE_M;
            break;
        case MK_n:
            key=SDL_SCANCODE_N;
            break;
        case MK_o:
            key=SDL_SCANCODE_O;
            break;
        case MK_p:
            key=SDL_SCANCODE_P;
            break;
        case MK_q:
            key=SDL_SCANCODE_Q;
            break;
        case MK_r:
            key=SDL_SCANCODE_R;
            break;
        case MK_s:
            key=SDL_SCANCODE_S;
            break;
        case MK_t:
            key=SDL_SCANCODE_T;
            break;
        case MK_u:
            key=SDL_SCANCODE_U;
            break;
        case MK_v:
            key=SDL_SCANCODE_V;
            break;
        case MK_w:
            key=SDL_SCANCODE_W;
            break;
        case MK_x:
            key=SDL_SCANCODE_X;
            break;
        case MK_y:
            key=SDL_SCANCODE_Y;
            break;
        case MK_z:
            key=SDL_SCANCODE_Z;
            break;
        case MK_escape:
            key=SDL_SCANCODE_ESCAPE;
            break;
        case MK_lbracket:
            key=SDL_SCANCODE_LEFTBRACKET;
            break;
        case MK_rbracket:
            key=SDL_SCANCODE_RIGHTBRACKET;
            break;
        default:
            break;
        }
        sprintf(buf,"%s \"key %d%s%s%s%s\"",
            entry,
            (int)key,
            (defmod & 1) ? " mod1" : "",
            (defmod & 2) ? " mod2" : "",
            (defmod & 4) ? " mod3" : "",
            (defmod & 8) ? " host" : ""
        );
    }
#else
    void MakeDefaultBind(char * buf) {
        Bitu key=0;
        switch (defkey) {
        case MK_nothing: *buf = 0; return;
        case MK_f1:case MK_f2:case MK_f3:case MK_f4:
        case MK_f5:case MK_f6:case MK_f7:case MK_f8:
        case MK_f9:case MK_f10:case MK_f11:case MK_f12: 
            key=(Bitu)(SDLK_F1+(defkey-MK_f1));
            break;
        case MK_rightarrow:
            key=SDLK_RIGHT;
            break;
        case MK_leftarrow:
            key=SDLK_LEFT;
            break;
        case MK_uparrow:
            key=SDLK_UP;
            break;
        case MK_downarrow:
            key=SDLK_DOWN;
            break;
        case MK_return:
            key=SDLK_RETURN;
            break;
        case MK_tab:
            key=SDLK_TAB;
            break;
        case MK_slash:
            key=SDLK_SLASH;
            break;
        case MK_backslash:
            key=SDLK_BACKSLASH;
            break;
        case MK_space:
            key=SDLK_SPACE;
            break;
        case MK_backspace:
            key=SDLK_BACKSPACE;
            break;
        case MK_delete:
            key=SDLK_DELETE;
            break;
        case MK_insert:
            key=SDLK_INSERT;
            break;
        case MK_semicolon:
            key=SDLK_SEMICOLON;
            break;
        case MK_quote:
            key=SDLK_QUOTE;
            break;
        case MK_grave:
            key=SDLK_BACKQUOTE;
            break;
        case MK_kpminus:
            key=SDLK_KP_MINUS;
            break;
        case MK_kpplus:
            key=SDLK_KP_PLUS;
            break;
        case MK_minus:
            key=SDLK_MINUS;
            break;
        case MK_equals:
            key=SDLK_EQUALS;
            break;
        case MK_scrolllock:
            key=SDLK_SCROLLOCK;
            break;
        case MK_pause:
            key=SDLK_PAUSE;
            break;
        case MK_printscreen:
            key=SDLK_PRINT;
            break;
        case MK_home:
            key=SDLK_HOME; 
            break;
        case MK_end:
            key=SDLK_END;
            break;
        case MK_pageup:
            key=SDLK_PAGEUP;
            break;
        case MK_pagedown:
            key=SDLK_PAGEDOWN;
            break;
        case MK_comma:
            key=SDLK_COMMA;
            break;
        case MK_period:
            key=SDLK_PERIOD;
            break;
        case MK_0:
            key=SDLK_0;
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
        case MK_5:
            key=SDLK_5;
            break;
        case MK_6:
            key=SDLK_6;
            break;
        case MK_7:
            key=SDLK_7;
            break;
        case MK_8:
            key=SDLK_8;
            break;
        case MK_9:
            key=SDLK_9;
            break;
        case MK_a:
            key=SDLK_a;
            break;
        case MK_b:
            key=SDLK_b;
            break;
        case MK_c:
            key=SDLK_c;
            break;
        case MK_d:
            key=SDLK_d;
            break;
        case MK_e:
            key=SDLK_e;
            break;
        case MK_f:
            key=SDLK_f;
            break;
        case MK_g:
            key=SDLK_g;
            break;
        case MK_h:
            key=SDLK_h;
            break;
        case MK_i:
            key=SDLK_i;
            break;
        case MK_j:
            key=SDLK_j;
            break;
        case MK_k:
            key=SDLK_k;
            break;
        case MK_l:
            key=SDLK_l;
            break;
        case MK_m:
            key=SDLK_m;
            break;
        case MK_n:
            key=SDLK_n;
            break;
        case MK_o:
            key=SDLK_o;
            break;
        case MK_p:
            key=SDLK_p;
            break;
        case MK_q:
            key=SDLK_q;
            break;
        case MK_r:
            key=SDLK_r;
            break;
        case MK_s:
            key=SDLK_s;
            break;
        case MK_t:
            key=SDLK_t;
            break;
        case MK_u:
            key=SDLK_u;
            break;
        case MK_v:
            key=SDLK_v;
            break;
        case MK_w:
            key=SDLK_w;
            break;
        case MK_x:
            key=SDLK_x;
            break;
        case MK_y:
            key=SDLK_y;
            break;
        case MK_z:
            key=SDLK_z;
            break;
        case MK_escape:
            key=SDLK_ESCAPE;
            break;
        case MK_lbracket:
            key=SDLK_LEFTBRACKET;
            break;
        case MK_rbracket:
            key=SDLK_RIGHTBRACKET;
            break;
        default:
            *buf = 0;
            return;
        }
        sprintf(buf,"%s \"key %d%s%s%s%s\"",
            entry,
            (int)key,
            defmod & 1 ? " mod1" : "",
            defmod & 2 ? " mod2" : "",
            defmod & 4 ? " mod3" : "",
            defmod & 8 ? " host" : ""
        );
    }
#endif
    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    //! \brief Text button in the mapper UI to indicate status by
    CTextButton *notify_button;

    //! \brief Mapper handler shortcut
    MAPPER_Handler * handler;
protected:
    //! \brief MapKeys enumeration for keyboard shortcut
    MapKeys defkey;

    //! \brief Default modifiers
    Bitu defmod;
public:
    //! \brief Button name
    char buttonname[100];
};

/* whether to run keystrokes through system but only to show how it comes out.
 * otherwise, do full mapper processing. */
bool MAPPER_DemoOnly(void) {
    return !mapper.exit;
}

CEvent *get_mapper_event_by_name(const std::string &x) {
    auto i = name_to_events.find(x);

    if (i != name_to_events.end()) {
        if (i->second >= events.size())
            E_Exit("Mapper: name to events contains out of range index for \"%s\"",x.c_str());

        return events[i->second];
    }

    return NULL;
}

unsigned char prvmc = 0;
extern bool font_14_init, loadlang;
uint8_t *GetDbcs14Font(Bitu code, bool &is14);
bool isDBCSCP();
static void DrawText(Bitu x,Bitu y,const char * text,uint8_t color,uint8_t bkcolor/*=CLR_BLACK*/) {
#if defined(C_SDL2)
    uint8_t * draw=((uint8_t *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
    uint8_t * draw=((uint8_t *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
    unsigned int cpbak = dos.loaded_codepage;
    if (dos_kernel_disabled&&maincp) dos.loaded_codepage = maincp;
    while (*text) {
        unsigned char c = *text;
        uint8_t * font;
        bool is14 = false;
        if (IS_PC98_ARCH || IS_JEGA_ARCH || isDBCSCP()) {
            if (isKanji1(c) && prvmc == 0) {
                prvmc = c;
                text++;
                continue;
            } else if (isKanji2(c) && prvmc > 1) {
                font = GetDbcs14Font(prvmc*0x100+c, is14);
                prvmc = 1;
            } else if (prvmc < 0x81)
                prvmc = 0;
        } else
            prvmc = 0;
        if (font_14_init&&dos.loaded_codepage&&dos.loaded_codepage!=437&&prvmc!=1)
            font = &int10_font_14_init[c*14];
        else if (prvmc!=1)
            font = &int10_font_14[c*14];
        Bitu i,j;
        for (int k=0; k<(prvmc?2:1); k++) {
            uint8_t * draw_line = draw;
            for (i=0;i<14;i++) {
                uint8_t map=*(font+(prvmc==1?(i*2+k):i));
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
            draw+=8;
        }
        text++;
        prvmc = 0;
    }
    dos.loaded_codepage = cpbak;
    prvmc = 0;
}

void RedrawMapperEventButtons() {
    bind_but.prevpage->SetCanClick(cpage > 1);
    bind_but.nextpage->SetCanClick(cpage < maxpage);
    bind_but.pagestat->Change("%2u / %-2u", cpage, maxpage);
    for (std::vector<CEventButton *>::iterator it = ceventbuttons.begin(); it != ceventbuttons.end(); ++it) {
        CEventButton *button = (CEventButton *)*it;
        button->Enable(button->Page(0)==cpage);
    }
}

void MAPPER_TriggerEventByName(const std::string& name) {
    CEvent *event = get_mapper_event_by_name(name);
    if (event != NULL) {
        if (event->type == CEvent::handler_event_t) {
            CHandlerEvent *he = reinterpret_cast<CHandlerEvent*>(event);
            if (he->handler != NULL) {
                he->handler(true);
                he->handler(false);
            }
        }
    }
}

static void change_action_text(const char* text,uint8_t col) {
    bind_but.action->Change(text,"");
    bind_but.action->SetColor(col);
}


static void SetActiveBind(CBind * _bind) {
    mapper.abind=_bind;
    if (_bind) {
        bind_but.bind_title->Enable(true);
        char buf[256];_bind->BindName(buf);
        bind_but.bind_title->Change("INPUT: %s",buf);
        bind_but.bind_title->SetColor(CLR_GREEN);
        bind_but.del->Enable(true);
        bind_but.next->Enable(true);
        bind_but.mod1->Enable(true);
        bind_but.mod2->Enable(true);
        bind_but.mod3->Enable(true);
        bind_but.host->Enable(true);
        bind_but.hold->Enable(true);
    } else {
        bind_but.bind_title->Enable(false);
        bind_but.del->Enable(false);
        bind_but.next->Enable(false);
        bind_but.mod1->Enable(false);
        bind_but.mod2->Enable(false);
        bind_but.mod3->Enable(false);
        bind_but.host->Enable(false);
        bind_but.hold->Enable(false);
    }
}

static void SetActiveEvent(CEvent * event) {
    mapper.aevent=event;
    mapper.redraw=true;
    mapper.addbind=false;
    bind_but.event_title->Change("EVENT: %s",event ? event->GetName(): "none");
    if (!event) {
        change_action_text(MSG_Get("SELECT_EVENT"),CLR_WHITE);
        bind_but.add->Enable(false);
        SetActiveBind(nullptr);
    } else {
        change_action_text(MSG_Get("SELECT_DIFFERENT_EVENT"),CLR_WHITE);
        mapper.abindit=event->bindlist.begin();
        if (mapper.abindit!=event->bindlist.end()) {
            SetActiveBind(*(mapper.abindit));
        } else SetActiveBind(nullptr);
        bind_but.add->Enable(true);
    }

    bind_but.cap->SetCanClick(event != nullptr);
}

#if defined(C_SDL2)
extern SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height);
extern SDL_Rect GFX_GetSDLSurfaceSubwindowDims(uint16_t width, uint16_t height);
extern void GFX_UpdateDisplayDimensions(int width, int height);
#endif

static void DrawButtons(void) {
#if defined(C_SDL2)
    SDL_FillRect(mapper.draw_surface, nullptr, 0);
#else
    SDL_FillRect(mapper.surface,0,CLR_BLACK);
    SDL_LockSurface(mapper.surface);
#endif
    for (CButton_it but_it = buttons.begin();but_it!=buttons.end();++but_it) {
        (*but_it)->Draw();
    }
#if defined(C_SDL2)
    // We can't just use SDL_BlitScaled (say for Android) in one step
    SDL_BlitSurface(mapper.draw_surface, NULL, mapper.draw_surface_nonpaletted, NULL);
    SDL_BlitScaled(mapper.draw_surface_nonpaletted, NULL, mapper.surface, &mapper.draw_rect);
//    SDL_BlitSurface(mapper.draw_surface, NULL, mapper.surface, NULL);
    if (SDL_UpdateWindowSurface(mapper.window) != 0)
    {
        E_Exit("Couldn't update window surface for mapper: %s", SDL_GetError());
    }
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
    CEventButton *button=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(button);
    return event;
}

static CMouseButtonEvent * AddMouseButtonEvent(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,char const * const entry,uint8_t key) {
	char buf[64];
	strcpy(buf,"mouse_");
	strcat(buf,entry);
	CMouseButtonEvent * event=new CMouseButtonEvent(buf,key);
	CEventButton *button=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(button);
	return event;
}

static CJAxisEvent * AddJAxisButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu stick,Bitu axis,bool positive,CJAxisEvent * opposite_axis) {
    char buf[64];
    sprintf(buf,"jaxis_%d_%d%s",(int)stick,(int)axis,positive ? "+" : "-");
    CJAxisEvent * event=new CJAxisEvent(buf,stick,axis,positive,opposite_axis);
    CEventButton *button=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(button);
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
    CEventButton *evbutton=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(evbutton);
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
    CEventButton* evbutton = new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(evbutton);
}

static void AddModButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu _mod) {
    char buf[64];

    if (_mod == 4)
        sprintf(buf,"host");
    else
        sprintf(buf,"mod_%d",(int)_mod);

    CModEvent * event=new CModEvent(buf,_mod);
    CEventButton *button=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(button);
    if (_mod == 4) {
        button->Enable(hostkeyalt == 0);
        hostbutton=button;
    }

    assert(_mod < 8);
    mod_event[_mod] = event;
}

bool SortHandlers(CHandlerEvent* a, CHandlerEvent* b)
{
    auto str1 = std::string(a->ButtonName());
    auto str2 = std::string(b->ButtonName());
    auto sort = str1.compare(str2) < 0;
    return sort;
}

static void CreateLayout(void) {

#define DX 0
#define DY 10
#define MX 1
#define MY 1
#define BW 27
#define BH 18
#define CX (BW / 2)
#define CY (BH / 2)
#define BU(_X_) (BW * (_X_) + MX * ((_X_) - 1))
#define BV(_Y_) (BH * (_Y_) + MY * ((_Y_) - 1))
#define PX(_X_) (DX + (_X_) * (BW + MX))
#define PY(_Y_) (DY + (_Y_) * (BH + MY))

    Bitu i;

#pragma region Keyboard

#undef XO
#undef YO
#define XO 2
#define YO 0

    if(!IS_PC98_ARCH)
    {
        AddKeyButtonEvent(PX(XO + 0), PY(YO), BU(1), BV(1), "F13", "f13", KBD_f13);
        AddKeyButtonEvent(PX(XO + 1), PY(YO), BU(1), BV(1), "F14", "f14", KBD_f14);
        AddKeyButtonEvent(PX(XO + 2), PY(YO), BU(1), BV(1), "F15", "f15", KBD_f15);
        AddKeyButtonEvent(PX(XO + 3), PY(YO), BU(1), BV(1), "F16", "f16", KBD_f16);
        AddKeyButtonEvent(PX(XO + 4), PY(YO), BU(1), BV(1), "F17", "f17", KBD_f17);
        AddKeyButtonEvent(PX(XO + 5), PY(YO), BU(1), BV(1), "F18", "f18", KBD_f18);
        AddKeyButtonEvent(PX(XO + 6), PY(YO), BU(1), BV(1), "F19", "f19", KBD_f19);
        AddKeyButtonEvent(PX(XO + 7), PY(YO), BU(1), BV(1), "F20", "f20", KBD_f20);
        AddKeyButtonEvent(PX(XO + 8), PY(YO), BU(1), BV(1), "F21", "f21", KBD_f21);
        AddKeyButtonEvent(PX(XO + 9), PY(YO), BU(1), BV(1), "F22", "f22", KBD_f22);
        AddKeyButtonEvent(PX(XO + 10), PY(YO), BU(1), BV(1), "F23", "f23", KBD_f23);
        AddKeyButtonEvent(PX(XO + 11), PY(YO), BU(1), BV(1), "F24", "f24", KBD_f24);
    }

#undef XO
#undef YO
#define XO 0
#define YO 1
    AddKeyButtonEvent(PX(XO), PY(YO), BU(2), BV(1), "Esc", "esc", KBD_esc);

    for(i = 0; i < 12; i++)
    {
        AddKeyButtonEvent(PX(XO + 2 + i), PY(YO), BU(1), BV(1), combo_f[i].title, combo_f[i].entry, combo_f[i].key);
    }

#undef XO
#undef YO
#define XO 0
#define YO 2

    for(i = 0; i < 14; i++)
    {
        const auto block = IS_PC98_ARCH ? combo_1_pc98 : combo_1;
        const auto width = i == 0 || i == 13 ? 2 : 1;
        AddKeyButtonEvent(PX(XO + (i > 0 ? i + 1 : i)), PY(YO), BU(width), BV(1), block[i].title, block[i].entry, block[i].key);
    }

#undef XO
#undef YO
#define XO 0
#define YO 3

    AddKeyButtonEvent(PX(XO), PY(YO), BU(2), BV(1), "Tab", "tab", KBD_tab);

    for(i = 0; i < 12; i++)
    {
        AddKeyButtonEvent(PX(XO + 2 + i), PY(YO), BU(1), BV(1), combo_2[i].title, combo_2[i].entry, combo_2[i].key);
    }

    AddKeyButtonEvent(PX(XO + 14), PY(YO), BU(2), BV(2), "Enter", "enter", KBD_enter);

#undef XO
#undef YO
#define XO 0
#define YO 4

    caps_lock_event = AddKeyButtonEvent(PX(XO), PY(YO), BU(2), BV(1), "Caps", "capslock", KBD_capslock);

    for(i = 0; i < 12; i++)
    {
        const auto block = IS_PC98_ARCH ? combo_3_pc98 : combo_3;
        AddKeyButtonEvent(PX(XO + 2 + i), PY(YO), BU(1), BV(1), block[i].title, block[i].entry, block[i].key);
    }

#undef XO
#undef YO
#define XO 0
#define YO 5

    AddKeyButtonEvent(PX(XO), PY(YO), BU(2), BV(1), "Shift", "lshift", KBD_leftshift);

    for(i = 0; i < 11; i++)
    {
        AddKeyButtonEvent(PX(XO + 2 + i), PY(YO), BU(1), BV(1), combo_4[i].title, combo_4[i].entry, combo_4[i].key);
    }

    AddKeyButtonEvent(PX(XO + 13), PY(YO), BU(3), BV(1), "Shift", "rshift", KBD_rightshift);

#undef XO
#undef YO
#define XO 0
#define YO 6

    AddKeyButtonEvent(PX(XO + 00), PY(YO), BU(2), BV(1), "Ctrl", "lctrl", KBD_leftctrl);
    AddKeyButtonEvent(PX(XO + 02), PY(YO), BU(1), BV(1), "Win", "lwindows", KBD_lwindows);
    AddKeyButtonEvent(PX(XO + 03), PY(YO), BU(1), BV(1), "Alt", "lalt", KBD_leftalt);
    AddKeyButtonEvent(PX(XO + 04), PY(YO), BU(7), BV(1), "Space", "space", KBD_space);
    AddKeyButtonEvent(PX(XO + 11), PY(YO), BU(1), BV(1), "Alt", "ralt", KBD_rightalt);
    AddKeyButtonEvent(PX(XO + 12), PY(YO), BU(1), BV(1), "Win", "rwindows", KBD_rwindows);
    AddKeyButtonEvent(PX(XO + 13), PY(YO), BU(1), BV(1), "App", "rwinmenu", KBD_rwinmenu);
    AddKeyButtonEvent(PX(XO + 14), PY(YO), BU(2), BV(1), "Ctrl", "rctrl", KBD_rightctrl);

#undef XO
#undef YO
#define XO 16
#define YO 0

    AddKeyButtonEvent(PX(XO + 0), PY(YO + 0), BU(1), BV(1), "Prn", "printscreen", KBD_printscreen);
    AddKeyButtonEvent(PX(XO + 1), PY(YO + 0), BU(1), BV(1), "Scr", "scrolllock", KBD_scrolllock);
    AddKeyButtonEvent(PX(XO + 2), PY(YO + 0), BU(1), BV(1), "Brk", "pause", KBD_pause);

    AddKeyButtonEvent(PX(XO + 0), PY(YO + 2), BU(1), BV(1), "Ins", "insert", KBD_insert);
    AddKeyButtonEvent(PX(XO + 1), PY(YO + 2), BU(1), BV(1), "Hom", "home", KBD_home);
    AddKeyButtonEvent(PX(XO + 2), PY(YO + 2), BU(1), BV(1), "Pg\x18", "pageup", KBD_pageup);

    AddKeyButtonEvent(PX(XO + 0), PY(YO + 3), BU(1), BV(1), "Del", "delete", KBD_delete);
    AddKeyButtonEvent(PX(XO + 1), PY(YO + 3), BU(1), BV(1), "End", "end", KBD_end);
    AddKeyButtonEvent(PX(XO + 2), PY(YO + 3), BU(1), BV(1), "Pg\x19", "pagedown", KBD_pagedown);

    AddKeyButtonEvent(PX(XO + 1), PY(YO + 5), BU(1), BV(1), "\x18", "up", KBD_up);
    AddKeyButtonEvent(PX(XO + 0), PY(YO + 6), BU(1), BV(1), "\x1B", "left", KBD_left);
    AddKeyButtonEvent(PX(XO + 1), PY(YO + 6), BU(1), BV(1), "\x19", "down", KBD_down);
    AddKeyButtonEvent(PX(XO + 2), PY(YO + 6), BU(1), BV(1), "\x1A", "right", KBD_right);

#undef XO
#undef YO
#define XO 19
#define YO 2

    AddKeyButtonEvent(PX(XO + 3) - 3, PY(YO - 1), BU(1) - 1, BV(1), "KP=", "kp_equals", KBD_kpequals);
    if(!IS_PC98_ARCH) {
        AddKeyButtonEvent(PX(XO + 2) - 3, PY(YO - 1), BU(1) - 1, BV(1), "KP,", "kp_comma", KBD_kpcomma);
    }
    num_lock_event =
    AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 0), BU(1) - 1, BV(1), "Num", "numlock", KBD_numlock);
    AddKeyButtonEvent(PX(XO + 1) - 1, PY(YO + 0), BU(1) - 1, BV(1), "/", "kp_divide", KBD_kpdivide);
    AddKeyButtonEvent(PX(XO + 2) - 2, PY(YO + 0), BU(1) - 1, BV(1), "*", "kp_multiply", KBD_kpmultiply);
    AddKeyButtonEvent(PX(XO + 3) - 3, PY(YO + 0), BU(1) - 1, BV(1), "-", "kp_minus", KBD_kpminus);

    AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 1), BU(1) - 1, BV(1), "7", "kp_7", KBD_kp7);
    AddKeyButtonEvent(PX(XO + 1) - 1, PY(YO + 1), BU(1) - 1, BV(1), "8", "kp_8", KBD_kp8);
    AddKeyButtonEvent(PX(XO + 2) - 2, PY(YO + 1), BU(1) - 1, BV(1), "9", "kp_9", KBD_kp9);
    AddKeyButtonEvent(PX(XO + 3) - 3, PY(YO + 1), BU(1) - 1, BV(2), "+", "kp_plus", KBD_kpplus);
                                     
    AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 2), BU(1) - 1, BV(1), "4", "kp_4", KBD_kp4);
    AddKeyButtonEvent(PX(XO + 1) - 1, PY(YO + 2), BU(1) - 1, BV(1), "5", "kp_5", KBD_kp5);
    AddKeyButtonEvent(PX(XO + 2) - 2, PY(YO + 2), BU(1) - 1, BV(1), "6", "kp_6", KBD_kp6);
                                     
    AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 3), BU(1) - 1, BV(1), "1", "kp_1", KBD_kp1);
    AddKeyButtonEvent(PX(XO + 1) - 1, PY(YO + 3), BU(1) - 1, BV(1), "2", "kp_2", KBD_kp2);
    AddKeyButtonEvent(PX(XO + 2) - 2, PY(YO + 3), BU(1) - 1, BV(1), "3", "kp_3", KBD_kp3);

    AddKeyButtonEvent(PX(XO + 3) - 3, PY(YO + 3), BU(1) - 1, BV(2), "Ent", "kp_enter", KBD_kpenter);

    if(IS_PC98_ARCH) // TODO
    {
        AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 4), BU(1) - 1, BV(1), "0", "kp_0", KBD_kp0);
        AddKeyButtonEvent(PX(XO + 1) - 1, PY(YO + 4), BU(1) - 1, BV(1), ",", "kp_comma", KBD_kpcomma);
    }
    else
    {
        AddKeyButtonEvent(PX(XO + 0) - 0, PY(YO + 4), BU(2) - 2, BV(1), "0", "kp_0", KBD_kp0);
    }

    AddKeyButtonEvent(PX(XO + 2) - 2, PY(YO + 4), BU(1) - 1, BV(1), ".", "kp_period", KBD_kpperiod);

#pragma endregion

#pragma region Mouse

#undef XO
#undef YO
#define XO 0
#define YO 9
    new CTextButton(PX(XO + 0) + CX * 1, PY(YO - 1), BU(3), BV(1), "Mouse");
    AddMouseButtonEvent(PX(XO + 0) + CX * 1, PY(YO), BU(1), BV(1), "L", "left", 0);
    AddMouseButtonEvent(PX(XO + 1) + CX * 1, PY(YO), BU(1), BV(1), "M", "middle", 2);
    AddMouseButtonEvent(PX(XO + 2) + CX * 1, PY(YO), BU(1), BV(1), "R", "right", 1);

#pragma endregion

#pragma region Joysticks

#undef XO
#undef YO
#define XO 11
#define YO 8

    switch(joytype)
    {
    case JOY_NONE:
        (new CTextButton(PX(XO + 0), PY(YO + 0) - CY, BU(3), BV(1), "Disabled"))->SetCanClick(false);
        (new CTextButton(PX(XO + 4), PY(YO + 0) - CY, BU(3), BV(1), "Disabled"))->SetCanClick(false);
        (new CTextButton(PX(XO + 8), PY(YO + 0) - CY, BU(3), BV(1), "Disabled"))->SetCanClick(false);
        break;
    case JOY_2AXIS:
        (new CTextButton(PX(XO + 0), PY(YO + 0) - CY, BU(3), BV(1), "Joystick 1"));
        (new CTextButton(PX(XO + 4), PY(YO + 0) - CY, BU(3), BV(1), "Joystick 2"));
        (new CTextButton(PX(XO + 8), PY(YO + 0) - CY, BU(3), BV(1), "Disabled"))->SetCanClick(false);
        break;
    case JOY_4AXIS:
    case JOY_4AXIS_2:
        (new CTextButton(PX(XO + 0), PY(YO + 0) - CY, BU(3), BV(1), "Axis 1/2"));
        (new CTextButton(PX(XO + 4), PY(YO + 0) - CY, BU(3), BV(1), "Axis 3/4"));
        (new CTextButton(PX(XO + 8), PY(YO + 0) - CY, BU(3), BV(1), "Disabled"))->SetCanClick(false);
        break;
    case JOY_FCS:
        (new CTextButton(PX(XO + 0), PY(YO + 0) - CY, BU(3), BV(1), "Axis 1/2"));
        (new CTextButton(PX(XO + 4), PY(YO + 0) - CY, BU(3), BV(1), "Axis 3"));
        (new CTextButton(PX(XO + 8), PY(YO + 0) - CY, BU(3), BV(1), "Hat/D-pad"));
        break;
    case JOY_CH:
        (new CTextButton(PX(XO + 0), PY(YO + 0) - CY, BU(3), BV(1), "Axis 1/2"));
        (new CTextButton(PX(XO + 4), PY(YO + 0) - CY, BU(3), BV(1), "Axis 3/4"));
        (new CTextButton(PX(XO + 8), PY(YO + 0) - CY, BU(3), BV(1), "Hat/D-pad"));
        break;
    default:
        break;
    }

    {
        AddJButtonButton(PX(XO + 0), PY(YO + 1) - CY, BU(1), BV(1), "1", 0, 0);
        AddJButtonButton(PX(XO + 2), PY(YO + 1) - CY, BU(1), BV(1), "2", 0, 1);

        auto x1 = AddJAxisButton(PX(XO + 0), PY(YO + 2) - CY, BU(1), BV(1), "X-", 0, 0, false, nullptr);
        auto x2 = AddJAxisButton(PX(XO + 2), PY(YO + 2) - CY, BU(1), BV(1), "X+", 0, 0, true, x1); (void)x2;//unused
        auto y1 = AddJAxisButton(PX(XO + 1), PY(YO + 1) - CY, BU(1), BV(1), "Y-", 0, 1, false, nullptr);
        auto y2 = AddJAxisButton(PX(XO + 1), PY(YO + 2) - CY, BU(1), BV(1), "Y+", 0, 1, true, y1); (void)y2;//unused
    }

    if(joytype == JOY_2AXIS)
    {
        AddJButtonButton(PX(XO + 4), PY(YO + 1) - CY, BU(1), BV(1), "1", 1, 0);
        AddJButtonButton(PX(XO + 6), PY(YO + 1) - CY, BU(1), BV(1), "2", 1, 1);
        AddJButtonButton_hidden(0, 2);
        AddJButtonButton_hidden(0, 3);

        auto x1 = AddJAxisButton(PX(XO + 4), PY(YO + 2) - CY, BW, BH, "X-", 1, 0, false, nullptr);
        auto x2 = AddJAxisButton(PX(XO + 6), PY(YO + 2) - CY, BW, BH, "X+", 1, 0, true, x1); (void)x2;//unused
        auto y1 = AddJAxisButton(PX(XO + 5), PY(YO + 1) - CY, BW, BH, "Y-", 1, 1, false, nullptr);
        auto y2 = AddJAxisButton(PX(XO + 5), PY(YO + 2) - CY, BW, BH, "Y+", 1, 1, true, y1); (void)y2;//unused
        auto z1 = AddJAxisButton_hidden(0, 2, false, nullptr);
        auto z2 = AddJAxisButton_hidden(0, 3, false, nullptr);

        AddJAxisButton_hidden(0, 2, true, z1);
        AddJAxisButton_hidden(0, 3, true, z2);
    }
    else
    {
        AddJButtonButton(PX(XO + 4), PY(YO + 1) - CY, BU(1), BV(1), "3", 0, 2);
        AddJButtonButton(PX(XO + 6), PY(YO + 1) - CY, BU(1), BV(1), "4", 0, 3);
        AddJButtonButton_hidden(1, 0);
        AddJButtonButton_hidden(1, 1);

        auto x1 = AddJAxisButton(PX(XO + 4), PY(YO + 2) - CY, BU(1), BV(1), "X-", 0, 2, false, nullptr);
        auto x2 = AddJAxisButton(PX(XO + 6), PY(YO + 2) - CY, BU(1), BV(1), "X+", 0, 2, true, x1); (void)x2;//unused
        auto y1 = AddJAxisButton(PX(XO + 5), PY(YO + 1) - CY, BU(1), BV(1), "Y-", 0, 3, false, nullptr);
        auto y2 = AddJAxisButton(PX(XO + 5), PY(YO + 2) - CY, BU(1), BV(1), "Y+", 0, 3, true, y1); (void)y2;//unused
        auto z1 = AddJAxisButton_hidden(1, 0, false, nullptr);
        auto z2 = AddJAxisButton_hidden(1, 1, false, nullptr);

        AddJAxisButton_hidden(1, 0, true, z1);
        AddJAxisButton_hidden(1, 1, true, z2);
    }

    if(joytype == JOY_CH)
    {
        AddJButtonButton(PX(XO +  8), PY(YO + 1) - CY, BU(1), BV(1), "5", 0, 4);
        AddJButtonButton(PX(XO + 10), PY(YO + 1) - CY, BU(1), BV(1), "6", 0, 5);
    }
    else
    {
        AddJButtonButton_hidden(0, 4);
        AddJButtonButton_hidden(0, 5);
    }

    AddJHatButton(PX(XO +  9), PY(YO + 1) - CY, BU(1), BV(1), "\x18", 0, 0, 0); // U
    AddJHatButton(PX(XO +  8), PY(YO + 2) - CY, BU(1), BV(1), "\x1B", 0, 0, 3); // L
    AddJHatButton(PX(XO +  9), PY(YO + 2) - CY, BU(1), BV(1), "\x19", 0, 0, 2); // D
    AddJHatButton(PX(XO + 10), PY(YO + 2) - CY, BU(1), BV(1), "\x1A", 0, 0, 1); // R

#pragma endregion

#pragma region Japanese, Korean

#undef XO
#undef YO
#define XO 0
#define YO 11

    AddKeyButtonEvent(PX(XO + 0) + CX, PY(YO + 0), BU(3), BV(1), "Hankaku", "jp_hankaku", KBD_jp_hankaku);
    AddKeyButtonEvent(PX(XO + 0) + CX, PY(YO + 1), BU(3), BV(1), "Muhenkan","jp_muhenkan",KBD_jp_muhenkan);
    AddKeyButtonEvent(PX(XO + 0) + CX, PY(YO + 2), BU(3), BV(1), "Henkan",  "jp_henkan",  KBD_jp_henkan);
    AddKeyButtonEvent(PX(XO + 0) + CX, PY(YO + 3), BU(3), BV(1), "Hiragana","jp_hiragana",KBD_jp_hiragana);
    AddKeyButtonEvent(PX(XO + 0) + CX, PY(YO + 4), BU(3), BV(1), "Yen",     "jp_yen",     KBD_jp_yen);
    AddKeyButtonEvent(PX(XO + 4) + CX, PY(YO + 0), BU(1), BV(1), "\\",      "jp_bckslash",KBD_jp_backslash);
    AddKeyButtonEvent(PX(XO + 5) + CX, PY(YO + 0), BU(1), BV(1), ":*",      "colon",      KBD_colon);
    AddKeyButtonEvent(PX(XO + 6) + CX, PY(YO + 0), BU(1), BV(1), "^`",      "caret",      KBD_caret);
    AddKeyButtonEvent(PX(XO + 7) + CX, PY(YO + 0), BU(1), BV(1), "@~",      "atsign",     KBD_atsign);
    AddKeyButtonEvent(PX(XO + 4) + CX, PY(YO + 3), BU(3), BV(1), "Hancha",  "kor_hancha", KBD_kor_hancha);
    AddKeyButtonEvent(PX(XO + 4) + CX, PY(YO + 4), BU(3), BV(1), "Hanyong", "kor_hanyong",KBD_kor_hanyong);

#pragma endregion

#pragma region Modifiers & Bindings

    AddModButton(PX(0) + CX, PY(17), BU(2), BV(1), LabelMod1, 1);
    AddModButton(PX(2) + CX, PY(17), BU(2), BV(1), LabelMod2, 2);
    AddModButton(PX(4) + CX, PY(17), BU(2), BV(1), LabelMod3, 3);
    AddModButton(PX(6) + CX, PY(17), BU(3), BV(1), LabelHost, 4);

    bind_but.event_title = new CCaptionButton(PX(0) + CX, PY(18), 0, 0, false);
    bind_but.bind_title  = new CCaptionButton(PX(0) + CX, PY(19), 0, 0, false);

    bind_but.add  = new CBindButton(PX(0) + CX, PY(20), BU(3), BV(1), MSG_Get("ADD"), BB_Add);
    bind_but.del  = new CBindButton(PX(3) + CX, PY(20), BU(3), BV(1), MSG_Get("DEL"), BB_Del);
    bind_but.next = new CBindButton(PX(6) + CX, PY(20), BU(3), BV(1), MSG_Get("NEXT"), BB_Next);

    bind_but.mod1 = new CCheckButton(PX(0) + CX, PY(21) + CY, BU(3), BV(1), LabelMod1, BC_Mod1);
    bind_but.mod2 = new CCheckButton(PX(0) + CX, PY(22) + CY, BU(3), BV(1), LabelMod2, BC_Mod2);
    bind_but.mod3 = new CCheckButton(PX(0) + CX, PY(23) + CY, BU(3), BV(1), LabelMod3, BC_Mod3);
    bind_but.host = new CCheckButton(PX(3) + CX, PY(21) + CY, BU(3), BV(1), LabelHost, BC_Host);
    bind_but.hold = new CCheckButton(PX(3) + CX, PY(22) + CY, BU(3), BV(1), LabelHold, BC_Hold);

#pragma endregion

#pragma region PC-98 extra keys

    if(IS_PC98_ARCH)
    {

#undef XO
#undef YO
#define XO 0
#define YO 0

        AddKeyButtonEvent(PX(XO +  0), PY(YO), BU(2), BV(1), "STOP", "stop", KBD_stop);
        AddKeyButtonEvent(PX(XO +  2), PY(YO), BU(2), BV(1), "HELP", "help", KBD_help);
        AddKeyButtonEvent(PX(XO +  4), PY(YO), BU(2), BV(1), "COPY", "copy", KBD_copy);
        AddKeyButtonEvent(PX(XO +  6), PY(YO), BU(2), BV(1), "KANA", "kana", KBD_kana);
        AddKeyButtonEvent(PX(XO +  8), PY(YO), BU(2), BV(1), "NFER", "nfer", KBD_nfer);
        AddKeyButtonEvent(PX(XO + 10), PY(YO), BU(2), BV(1), "XFER", "xfer", KBD_xfer);
        AddKeyButtonEvent(PX(XO + 12), PY(YO), BU(2), BV(1), "RO/_", "jp_ro", KBD_jp_ro);

#undef XO
#undef YO
#define XO 3
#define YO 11

        AddKeyButtonEvent(PX(XO + 1) + CX, PY(YO + 1), BU(1), BV(1), "VF1", "vf1", KBD_vf1);
        AddKeyButtonEvent(PX(XO + 2) + CX, PY(YO + 1), BU(1), BV(1), "VF2", "vf2", KBD_vf2);
        AddKeyButtonEvent(PX(XO + 3) + CX, PY(YO + 1), BU(1), BV(1), "VF3", "vf3", KBD_vf3);
        AddKeyButtonEvent(PX(XO + 4) + CX, PY(YO + 1), BU(1), BV(1), "VF4", "vf4", KBD_vf4);
        AddKeyButtonEvent(PX(XO + 5) + CX, PY(YO + 1), BU(1), BV(1), "VF5", "vf5", KBD_vf5);
    }

#pragma endregion

#pragma region Bindables

#undef XO
#undef YO

    Bitu    xpos = 0;
    Bitu    ypos = 0;
    uint8_t page = cpage;

    ceventbuttons.clear();

    std::sort(handlergroup.begin(), handlergroup.end(), SortHandlers);

    for(CHandlerEventVector_it hit = handlergroup.begin(); hit != handlergroup.end(); ++hit)
    {
        maxpage = page;

        auto button = new CEventButton(PX(10) + CX, PY(11 + ypos), BU(12), BV(1), (*hit)->ButtonName(), (*hit));

        ceventbuttons.push_back(button);
        (*hit)->notifybutton(button);
        button->Enable(page == cpage);
        button->Page(page);

        ypos++;

        if(ypos >= 7)
        {
            ypos = 0;
            page++;
        }
    }

    bind_but.prevpage =
        new CBindButton(PX(11), PY(19) - CY - (CY / 2), BU(4), BV(1), "<-", BB_Prevpage);

    bind_but.pagestat = new CCaptionButton(PX(14), PY(19) - CY - (CY / 2), BU(5), BV(1), true);

    bind_but.nextpage =
        new CBindButton(PX(18), PY(19) - CY - (CY / 2), BU(4), BV(1), "->", BB_Nextpage);

    bind_but.pagestat->Change("%2u / %-2u", cpage, maxpage);

    bind_but.prevpage->SetCanClick(false);

    next_handler_xpos = xpos; // TODO is this useless, commenting it out changes nothing!?
    next_handler_ypos = ypos; // TODO is this useless, commenting it out changes nothing!?

#pragma endregion

#pragma region Capture/Save/Exit

    bind_but.cap  = new CBindButton(PX(12), PY(20), BU(3), BV(1), MSG_Get("CAPTURE"), BB_Capture);
    bind_but.save = new CBindButton(PX(15), PY(20), BU(3), BV(1), MSG_Get("SAVE"), BB_Save);
    bind_but.exit = new CBindButton(PX(18), PY(20), BU(3), BV(1), MSG_Get("EXIT"), BB_Exit);
    bind_but.cap->SetCanClick(false);

#pragma endregion

#pragma region Status/Info

    // NOTE: screen budget is really tight down there, more than that and drawing crashes

    bind_but.action = new CCaptionButton(PX(7), PY(22) - CY, BU(15), BV(1), false);
    bind_but.dbg1   = new CCaptionButton(PX(7), PY(23) - CY, BU(16), BV(1), false);
    bind_but.dbg2   = new CCaptionButton(PX(7), PY(24) - CY, BU(16), BV(1), false);

    bind_but.dbg1->Change("%s", "");
    bind_but.dbg2->Change("%s", "");

#pragma endregion

    mapper_addhandler_create_buttons = true;
}

static void CreateStringBind(char * line,bool loading=false) {
    std::string o_line = line;

    line=trim(line);
    if (*line == 0) return;
    char * eventname=StripWord(line);
    CEvent * event;
    for (CEventVector_it ev_it=events.begin();ev_it!=events.end();++ev_it) {
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
        for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();++it) {
            bind=(*it)->CreateConfigBind(bindline);
            if (bind) {
                event->AddBind(bind);
                bind->SetFlags(bindline);
                event->update_menu_shortcut();
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

    {"f1",SDL_SCANCODE_F1},     {"f2",SDL_SCANCODE_F2},     {"f3",SDL_SCANCODE_F3},     {"f4",SDL_SCANCODE_F4},
    {"f5",SDL_SCANCODE_F5},     {"f6",SDL_SCANCODE_F6},     {"f7",SDL_SCANCODE_F7},     {"f8",SDL_SCANCODE_F8},
    {"f9",SDL_SCANCODE_F9},     {"f10",SDL_SCANCODE_F10},   {"f11",SDL_SCANCODE_F11},   {"f12",SDL_SCANCODE_F12},

    {"1",SDL_SCANCODE_1},       {"2",SDL_SCANCODE_2},       {"3",SDL_SCANCODE_3},       {"4",SDL_SCANCODE_4},
    {"5",SDL_SCANCODE_5},       {"6",SDL_SCANCODE_6},       {"7",SDL_SCANCODE_7},       {"8",SDL_SCANCODE_8},
    {"9",SDL_SCANCODE_9},       {"0",SDL_SCANCODE_0},

    {"a",SDL_SCANCODE_A},       {"b",SDL_SCANCODE_B},       {"c",SDL_SCANCODE_C},       {"d",SDL_SCANCODE_D},
    {"e",SDL_SCANCODE_E},       {"f",SDL_SCANCODE_F},       {"g",SDL_SCANCODE_G},       {"h",SDL_SCANCODE_H},
    {"i",SDL_SCANCODE_I},       {"j",SDL_SCANCODE_J},       {"k",SDL_SCANCODE_K},       {"l",SDL_SCANCODE_L},
    {"m",SDL_SCANCODE_M},       {"n",SDL_SCANCODE_N},       {"o",SDL_SCANCODE_O},       {"p",SDL_SCANCODE_P},
    {"q",SDL_SCANCODE_Q},       {"r",SDL_SCANCODE_R},       {"s",SDL_SCANCODE_S},       {"t",SDL_SCANCODE_T},
    {"u",SDL_SCANCODE_U},       {"v",SDL_SCANCODE_V},       {"w",SDL_SCANCODE_W},       {"x",SDL_SCANCODE_X},
    {"y",SDL_SCANCODE_Y},       {"z",SDL_SCANCODE_Z},       {"space",SDL_SCANCODE_SPACE},
    {"esc",SDL_SCANCODE_ESCAPE},    {"equals",SDL_SCANCODE_EQUALS},     {"grave",SDL_SCANCODE_GRAVE},
    {"tab",SDL_SCANCODE_TAB},       {"enter",SDL_SCANCODE_RETURN},      {"bspace",SDL_SCANCODE_BACKSPACE},
    {"lbracket",SDL_SCANCODE_LEFTBRACKET},                      {"rbracket",SDL_SCANCODE_RIGHTBRACKET},
    {"minus",SDL_SCANCODE_MINUS},   {"capslock",SDL_SCANCODE_CAPSLOCK}, {"semicolon",SDL_SCANCODE_SEMICOLON},
    {"quote", SDL_SCANCODE_APOSTROPHE}, {"backslash",SDL_SCANCODE_BACKSLASH},   {"lshift",SDL_SCANCODE_LSHIFT},
    {"rshift",SDL_SCANCODE_RSHIFT}, {"lalt",SDL_SCANCODE_LALT},         {"ralt",SDL_SCANCODE_RALT},
    {"lctrl",SDL_SCANCODE_LCTRL},   {"rctrl",SDL_SCANCODE_RCTRL},       {"comma",SDL_SCANCODE_COMMA},
    {"period",SDL_SCANCODE_PERIOD}, {"slash",SDL_SCANCODE_SLASH},       {"printscreen",SDL_SCANCODE_PRINTSCREEN},
    {"scrolllock",SDL_SCANCODE_SCROLLLOCK}, {"pause",SDL_SCANCODE_PAUSE},       {"pagedown",SDL_SCANCODE_PAGEDOWN},
    {"pageup",SDL_SCANCODE_PAGEUP}, {"insert",SDL_SCANCODE_INSERT},     {"home",SDL_SCANCODE_HOME},
    {"delete",SDL_SCANCODE_DELETE}, {"end",SDL_SCANCODE_END},           {"up",SDL_SCANCODE_UP},
    {"left",SDL_SCANCODE_LEFT},     {"down",SDL_SCANCODE_DOWN},         {"right",SDL_SCANCODE_RIGHT},
    {"kp_0",SDL_SCANCODE_KP_0}, {"kp_1",SDL_SCANCODE_KP_1}, {"kp_2",SDL_SCANCODE_KP_2}, {"kp_3",SDL_SCANCODE_KP_3},
    {"kp_4",SDL_SCANCODE_KP_4}, {"kp_5",SDL_SCANCODE_KP_5}, {"kp_6",SDL_SCANCODE_KP_6}, {"kp_7",SDL_SCANCODE_KP_7},
    {"kp_8",SDL_SCANCODE_KP_8}, {"kp_9",SDL_SCANCODE_KP_9}, {"numlock",SDL_SCANCODE_NUMLOCKCLEAR},
    {"kp_divide",SDL_SCANCODE_KP_DIVIDE},   {"kp_multiply",SDL_SCANCODE_KP_MULTIPLY},
    {"kp_minus",SDL_SCANCODE_KP_MINUS},     {"kp_plus",SDL_SCANCODE_KP_PLUS},
    {"kp_period",SDL_SCANCODE_KP_PERIOD},   {"kp_enter",SDL_SCANCODE_KP_ENTER},
    {"kp_equals",SDL_SCANCODE_KP_EQUALS},   {"kp_comma",SDL_SCANCODE_KP_COMMA},

    /* Is that the extra backslash key ("less than" key) */
    /* on some keyboards with the 102-keys layout??      */
    {"lessthan",SDL_SCANCODE_NONUSBACKSLASH},
#if defined(MACOSX)
    {"jp_bckslash",SDL_SCANCODE_INTERNATIONAL1},
    {"jp_yen",SDL_SCANCODE_INTERNATIONAL3},
#elif (defined (WIN32) || defined (__linux__))
    /* Special handling for JP Keyboards */
    {"jp_bckslash",SDL_SCANCODE_INTERNATIONAL1}, // SDL2 returns same code as SDL_SCANCODE_NONUSBACKSLASH?
    {"jp_yen",SDL_SCANCODE_INTERNATIONAL3},
    {"jp_muhenkan", SDL_SCANCODE_INTERNATIONAL5},
    {"jp_henkan", SDL_SCANCODE_INTERNATIONAL4},
    {"jp_hiragana", SDL_SCANCODE_INTERNATIONAL2},
#endif //(defined (WIN32) || defined (__linux__))
#if 0
#ifdef SDL_DOSBOX_X_SPECIAL
    /* hack for Japanese keyboards with \ and _ */
    {"jp_bckslash",SDLK_JP_RO}, // Same difference
    {"jp_ro",SDLK_JP_RO}, // DOSBox proprietary
    /* hack for Japanese keyboards with Yen and | */
    {"jp_yen",SDLK_JP_YEN },
#endif
    /* more */
    {"jp_hankaku", SDLK_WORLD_12 },
    {"jp_muhenkan", SDLK_WORLD_13 },
    {"jp_henkan", SDLK_WORLD_14 },
    {"jp_hiragana", SDLK_WORLD_15 },
    {"colon", SDLK_COLON },
    {"caret", SDLK_CARET },
    {"atsign", SDLK_AT },
#endif
    {nullptr, 0}
};
#else

static struct {
    const char * eventend;
    Bitu key;
} DefaultKeys[]={
    {"f1",SDLK_F1},     {"f2",SDLK_F2},     {"f3",SDLK_F3},     {"f4",SDLK_F4},
    {"f5",SDLK_F5},     {"f6",SDLK_F6},     {"f7",SDLK_F7},     {"f8",SDLK_F8},
    {"f9",SDLK_F9},     {"f10",SDLK_F10},   {"f11",SDLK_F11},   {"f12",SDLK_F12},

    {"1",SDLK_1},       {"2",SDLK_2},       {"3",SDLK_3},       {"4",SDLK_4},
    {"5",SDLK_5},       {"6",SDLK_6},       {"7",SDLK_7},       {"8",SDLK_8},
    {"9",SDLK_9},       {"0",SDLK_0},

    {"a",SDLK_a},       {"b",SDLK_b},       {"c",SDLK_c},       {"d",SDLK_d},
    {"e",SDLK_e},       {"f",SDLK_f},       {"g",SDLK_g},       {"h",SDLK_h},
    {"i",SDLK_i},       {"j",SDLK_j},       {"k",SDLK_k},       {"l",SDLK_l},
    {"m",SDLK_m},       {"n",SDLK_n},       {"o",SDLK_o},       {"p",SDLK_p},
    {"q",SDLK_q},       {"r",SDLK_r},       {"s",SDLK_s},       {"t",SDLK_t},
    {"u",SDLK_u},       {"v",SDLK_v},       {"w",SDLK_w},       {"x",SDLK_x},
    {"y",SDLK_y},       {"z",SDLK_z},       {"space",SDLK_SPACE},
    {"esc",SDLK_ESCAPE},    {"equals",SDLK_EQUALS},     {"grave",SDLK_BACKQUOTE},
    {"tab",SDLK_TAB},       {"enter",SDLK_RETURN},      {"bspace",SDLK_BACKSPACE},
    {"lbracket",SDLK_LEFTBRACKET},                      {"rbracket",SDLK_RIGHTBRACKET},
    {"minus",SDLK_MINUS},   {"capslock",SDLK_CAPSLOCK}, {"semicolon",SDLK_SEMICOLON},
    {"quote", SDLK_QUOTE},  {"backslash",SDLK_BACKSLASH},   {"lshift",SDLK_LSHIFT},
    {"rshift",SDLK_RSHIFT}, {"lalt",SDLK_LALT},         {"ralt",SDLK_RALT},
    {"lctrl",SDLK_LCTRL},   {"rctrl",SDLK_RCTRL},       {"comma",SDLK_COMMA},
    {"period",SDLK_PERIOD}, {"slash",SDLK_SLASH},

    {"printscreen",SDLK_PRINT},
    {"scrolllock",SDLK_SCROLLOCK},

    {"pause",SDLK_PAUSE},       {"pagedown",SDLK_PAGEDOWN},
    {"pageup",SDLK_PAGEUP}, {"insert",SDLK_INSERT},     {"home",SDLK_HOME},
    {"delete",SDLK_DELETE}, {"end",SDLK_END},           {"up",SDLK_UP},
    {"left",SDLK_LEFT},     {"down",SDLK_DOWN},         {"right",SDLK_RIGHT},

    {"kp_0",SDLK_KP0},  {"kp_1",SDLK_KP1},  {"kp_2",SDLK_KP2},  {"kp_3",SDLK_KP3},
    {"kp_4",SDLK_KP4},  {"kp_5",SDLK_KP5},  {"kp_6",SDLK_KP6},  {"kp_7",SDLK_KP7},
    {"kp_8",SDLK_KP8},  {"kp_9",SDLK_KP9},
    {"numlock",SDLK_NUMLOCK},

    {"kp_divide",SDLK_KP_DIVIDE},   {"kp_multiply",SDLK_KP_MULTIPLY},
    {"kp_minus",SDLK_KP_MINUS},     {"kp_plus",SDLK_KP_PLUS},
    {"kp_period",SDLK_KP_PERIOD},   {"kp_enter",SDLK_KP_ENTER},

    /* NTS: IBM PC keyboards as far as I know do not have numeric keypad equals sign.
     *      This default assignment should allow Apple Mac users (who's keyboards DO have one)
     *      to use theirs as a normal equals sign. */
    {"kp_equals",SDLK_KP_EQUALS},
    {"kp_comma", SDLK_KP_COMMA},

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
#ifdef SDL_DOSBOX_X_SPECIAL
#if defined(MACOSX)
    {"jp_bckslash",SDLK_JP_RO},
    {"jp_yen",SDLK_JP_YEN },
#else
    /* hack for Japanese keyboards with \ and _ */
    {"jp_bckslash",SDLK_JP_RO}, // Same difference
    //{"jp_ro",SDLK_JP_RO}, // DOSBox proprietary
    /* hack for Japanese keyboards with Yen and | */
    {"jp_yen",SDLK_JP_YEN },
#endif
#endif
    /* more */
    {"jp_hankaku", SDLK_WORLD_12 },
    {"jp_muhenkan", SDLK_WORLD_13 },
    {"jp_henkan", SDLK_WORLD_14 },
    {"jp_hiragana", SDLK_WORLD_15 },
    {"colon", SDLK_COLON },
    {"caret", SDLK_CARET },
    {"atsign", SDLK_AT },
#endif

    {0,0}
};

#endif

static void ClearAllBinds(void) {
	for (CEvent *event : events)
		event->ClearBinds();
}

static void CreateDefaultBinds(void) {
	ClearAllBinds();
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
    sprintf(buffer,"mod_3 \"key %d\"",SDL_SCANCODE_RSHIFT);CreateStringBind(buffer);
    sprintf(buffer,"mod_3 \"key %d\"",SDL_SCANCODE_LSHIFT);CreateStringBind(buffer);
# if defined(WIN32) && !defined(C_HX_DOS) /* F12 is not a good modifier key in Windows: https://stackoverflow.com/questions/18997754/how-to-disable-f12-to-debug-application-in-visual-studio-2012 */
    sprintf(buffer,"host \"key %d\"",SDL_SCANCODE_F11);CreateStringBind(buffer);
# else
    sprintf(buffer,"host \"key %d\"",SDL_SCANCODE_F12);CreateStringBind(buffer);
# endif
#else
    sprintf(buffer,"mod_1 \"key %d\"",SDLK_RCTRL);CreateStringBind(buffer);
    sprintf(buffer,"mod_1 \"key %d\"",SDLK_LCTRL);CreateStringBind(buffer);
    sprintf(buffer,"mod_2 \"key %d\"",SDLK_RALT);CreateStringBind(buffer);
    sprintf(buffer,"mod_2 \"key %d\"",SDLK_LALT);CreateStringBind(buffer);
    sprintf(buffer,"mod_3 \"key %d\"",SDLK_RSHIFT);CreateStringBind(buffer);
    sprintf(buffer,"mod_3 \"key %d\"",SDLK_LSHIFT);CreateStringBind(buffer);
# if defined(WIN32) && !defined(C_HX_DOS) /* F12 is not a good modifier key in Windows: https://stackoverflow.com/questions/18997754/how-to-disable-f12-to-debug-application-in-visual-studio-2012 */
    sprintf(buffer,"host \"key %d\"",SDLK_F11);CreateStringBind(buffer);
# else
    sprintf(buffer,"host \"key %d\"",SDLK_F12);CreateStringBind(buffer);
# endif
#endif

    for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();++hit) {
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

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char const * const eventname,char const * const buttonname,DOSBoxMenu::item **ret_menuitem) {
    if (ret_menuitem != NULL)
        *ret_menuitem = NULL;

    char tempname[27];
    strcpy(tempname,"hand_");
    strcat(tempname,eventname);

    //Check if it already exists=> if so return.
    for(CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();++it) {
        if(strcmp((*it)->buttonname,buttonname) == 0) {
            if (ret_menuitem != NULL)
                *ret_menuitem = &mainMenu.get_item(std::string("mapper_") + std::string(eventname));

            return;
        }
    }

    CHandlerEvent *event = new CHandlerEvent(tempname,handler,key,mods,buttonname);
    event->eventname = eventname;

    /* The mapper now automatically makes menu items for mapper events */
    DOSBoxMenu::item &menuitem = mainMenu.alloc_item(DOSBoxMenu::item_type_id, std::string("mapper_") + std::string(eventname));
    menuitem.set_mapper_event(tempname);

    if (ret_menuitem == NULL)
        menuitem.set_text(buttonname);
    else
        *ret_menuitem = &menuitem;

    if (mapper_addhandler_create_buttons) {
        // and a button in the mapper UI
        {
            unsigned int columns = ((unsigned int)strlen(buttonname) + 9U) / 10U;
            if ((next_handler_xpos+columns-1)>6) {
                next_handler_xpos=3;next_handler_ypos++;
            }
            CEventButton *button=new CEventButton(PX(next_handler_xpos*3),PY(next_handler_ypos),BW*3*columns,BH,buttonname,event);
            event->notifybutton(button);
            next_handler_xpos += columns;
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

            // color of the button needs to reflect binding
            event->notify_button->BindColor();
        }
    }

    return ;
}

static void MAPPER_SaveBinds(void) {
    std::string content="";
    FILE * loadfile=fopen(mapper.filename.c_str(),"rt");
    if (loadfile) {
        char secname[512], linein[512], *line=linein;
        strcpy(secname, "");
        while (fgets(linein,512,loadfile)) {
            line=trim(line);
            if (strlen(line)>2 && line[0]=='[' && line[strlen(line)-1]==']') {
                linein[strlen(line)-1] = 0;
                strcpy(secname, line+1);
                if (strcasecmp(secname, SDL_STRING)) content+=std::string(line)+"]\n";
            } else if (strlen(secname)&&strcasecmp(secname, SDL_STRING))
                content+=std::string(linein)+"\n";
        }
        fclose(loadfile);
    }

    FILE * savefile=fopen(mapper.filename.c_str(),"wt+");
    if (!savefile) {
        LOG_MSG("Can't open %s for saving the mappings",mapper.filename.c_str());
        return;
    }
    fprintf(savefile,"[%s]\n",SDL_STRING);
    char buf[128];
    for (CEventVector_it event_it=events.begin();event_it!=events.end();++event_it) {
        CEvent * event=*(event_it);
        fprintf(savefile,"%s ",event->GetName());
        for (CBindList_it bind_it=event->bindlist.begin();bind_it!=event->bindlist.end();++bind_it) {
            CBind * bind=*(bind_it);
            bind->ConfigName(buf);
            bind->AddFlags(buf);
            fprintf(savefile,"\"%s\" ",buf);
        }
        fprintf(savefile,"\n");
    }
    if (content.size()) {
        fprintf(savefile,"\n");
        std::istringstream f(content);
        std::string line;
        while (std::getline(f, line))
            fprintf(savefile,"%s \n",line.c_str());
    }
    fclose(savefile);
#if defined(WIN32)
    char path[MAX_PATH];
    if (GetFullPathName(mapper.filename.c_str(), MAX_PATH, path, NULL)) LOG_MSG("Saved mapper file: %s", path);
#elif defined(HAVE_REALPATH)
    char path[PATH_MAX];
    if (realpath(mapper.filename.c_str(), path) != NULL) LOG_MSG("Saved mapper file: %s", path);
#endif
    std::string str = mapper.filename.substr(mapper.filename.find_last_of("/\\") + 1);
    std::string msg = MSG_Get("MAPPER_FILE_SAVED")+std::string(": ")+str;
    change_action_text(msg.c_str(),CLR_WHITE);
}

static bool MAPPER_LoadBinds(void) {
    FILE * loadfile=fopen(mapper.filename.c_str(),"rt");
    if (!loadfile) return false;
	ClearAllBinds();
    bool othersec=false, hasbind=false;
    char secname[512], linein[512], *line=linein;
    strcpy(secname, "");
    while (fgets(linein,512,loadfile)) {
        line=trim(line);
        if (strlen(line)>2 && line[0]=='[' && line[strlen(line)-1]==']') {
            linein[strlen(line)-1] = 0;
            strcpy(secname, line+1);
            if (strcasecmp(secname, SDL_STRING)) othersec=true;
        } else if (!strlen(secname)||!strcasecmp(secname, SDL_STRING)) {
            hasbind=true;
            CreateStringBind(linein,/*loading*/true);
        }
    }
    fclose(loadfile);
    if (othersec&&!hasbind) return false;
    LOG(LOG_MISC,LOG_NORMAL)("MAPPER: Loading mapper settings from %s", mapper.filename.c_str());
    return true;
}

void MAPPER_CheckEvent(SDL_Event * event) {
    for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();++it) {
        if ((*it)->CheckEvent(event)) return;
    }

    if (log_keyboard_scan_codes) {
        if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
            LOG_MSG("MAPPER: SDL keyboard event (%s): scancode=0x%X sym=0x%X mod=0x%X",
                event->type == SDL_KEYDOWN?"down":"up",
                event->key.keysym.scancode,event->key.keysym.sym,event->key.keysym.mod);
    }
}

void PressRelease(void) {
    if (press_select != NULL) {
        press_select->SetPress(false);
        press_select = NULL;
    }
}

void PressSelect(CButton *b) {
    if (press_select != b)
        PressRelease();

    if (b != NULL) {
        press_select = b;
        press_select->SetPress(true);
    }
}

void Mapper_MousePressEvent(SDL_Event &event) {
    /* Check the press */
    for (CButton_it but_it = buttons.begin();but_it!=buttons.end();++but_it) {
        if ((*but_it)->OnTop(event.button.x,event.button.y)) {
            PressSelect(*but_it);
            break;
        }
    }
}

void Mapper_MouseInputEvent(SDL_Event &event) {
    PressRelease();

    /* Check the press */
    for (CButton_it but_it = buttons.begin();but_it!=buttons.end();++but_it) {
        if ((*but_it)->OnTop(event.button.x,event.button.y)) {
            (*but_it)->Click();
        }
    }
}

#if defined(C_SDL2)
void Mapper_FingerPressEvent(SDL_Event &event) {
    SDL_Event ev;

    memset(&ev,0,sizeof(ev));
    ev.type = SDL_MOUSEBUTTONDOWN;

    /* NTS: Windows versions of SDL2 do normalize the coordinates */
    ev.button.x = (Sint32)(event.tfinger.x * mapper.surface->w);
    ev.button.y = (Sint32)(event.tfinger.y * mapper.surface->h);

    Mapper_MousePressEvent(ev);
}

void Mapper_FingerInputEvent(SDL_Event &event) {
    SDL_Event ev;

    memset(&ev,0,sizeof(ev));
    ev.type = SDL_MOUSEBUTTONUP;

    /* NTS: Windows versions of SDL2 do normalize the coordinates */
    ev.button.x = (Sint32)(event.tfinger.x * mapper.surface->w);
    ev.button.y = (Sint32)(event.tfinger.y * mapper.surface->h);

    Mapper_MouseInputEvent(ev);
}
#endif

void BIND_MappingEvents(void) {
    SDL_Event event;

    if (GUI_JoystickCount()>0) SDL_JoystickUpdate();
    MAPPER_UpdateJoysticks();

#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    while (SDL_PollEvent(&event)) {
#if C_EMSCRIPTEN
        emscripten_sleep(0);
#endif

        switch (event.type) {
#if defined(_WIN32) && !defined(HX_DOS)
        case SDL_SYSWMEVENT : {
            switch ( event.syswm.msg->
#if defined(C_SDL2)
            msg.win.
#endif
            msg ) {
                case WM_COMMAND:
# if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                    if (GetMenu(GetHWND())) {
# if defined(C_SDL2)
                        if (mapperMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->msg.win.wParam))) return;
# else
                        if (mapperMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->wParam))) return;
# endif
                    }
# endif
                    break;
            }
        } break;
#endif
#if defined(C_SDL2) && !defined(IGNORE_TOUCHSCREEN)
        case SDL_FINGERDOWN:
            Mapper_FingerPressEvent(event);
            break;
#endif
#if defined(C_SDL2) && !defined(IGNORE_TOUCHSCREEN)
        case SDL_FINGERUP:
            Mapper_FingerInputEvent(event);
            break;
#endif
        case SDL_MOUSEBUTTONDOWN:
#if defined(C_SDL2)
            if (event.button.which != SDL_TOUCH_MOUSEID) /* don't handle mouse events faked by touchscreen */
                Mapper_MousePressEvent(event);
#else
            Mapper_MousePressEvent(event);
#endif
            break;
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
                if (s.sym == SDLK_ESCAPE) {
                    if (mouselocked) {
                        GFX_CaptureMouse();
                        mapper_esc_count = 0;
                        change_action_text(MSG_Get("SELECT_EVENT"), CLR_WHITE);
                    }
                    else {
                        if (event.type == SDL_KEYUP) {
                            if (++mapper_esc_count == 3) {
                                void MAPPER_ReleaseAllKeys(void);
                                MAPPER_ReleaseAllKeys();
                                mapper.exit=true;
                            }
                        }
                    }
                }
                else {
                    mapper_esc_count = 0;
                }

                size_t tmpl;
#if defined(C_SDL2)
                SDL_Scancode key = s.scancode;
#if defined(WIN32)
                if(key == SDL_SCANCODE_NONUSBACKSLASH) { // Special consideration for JP Keyboard
                    key = (isJPkeyboard ? SDL_SCANCODE_INTERNATIONAL1 : SDL_SCANCODE_NONUSBACKSLASH);
                }
#endif
                tmpl = (size_t)sprintf(tmp,"%c%02x: scan=%d sym=%d mod=%xh name=%s",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    key,
                    s.sym,
                    s.mod,
                    SDL_GetScancodeName(key));
#else
                tmpl = (size_t)sprintf(tmp,"%c%02x: scan=%u sym=%u mod=%xh u=%xh name=%s",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    s.scancode,
                    s.sym,
                    s.mod,
                    s.unicode,
					SDL_GetKeyName((SDLKey)GetKeyCode(s)));
					//(s.sym ? SDL_GetKeyName((SDLKey)MapSDLCode((Bitu)s.sym)) : SDL_GetKeyName((SDLKey)MapSDLCode((Bitu)sdlkey_map[(s.scancode ? s.scancode : event.key.keysym.scancode)]))));
#endif
                while (tmpl < (440/8)) tmp[tmpl++] = ' ';
                assert(tmpl < sizeof(tmp));
                tmp[tmpl] = 0;

                LOG(LOG_GUI,LOG_DEBUG)("Mapper keyboard event: %s",tmp);
                bind_but.dbg1->Change("%s",tmp);

                tmpl = 0;
#if defined(WIN32)
# if defined(C_SDL2)
# elif defined(SDL_DOSBOX_X_SPECIAL)
                {
                    char nm[256];

                    nm[0] = 0;
#if !defined(HX_DOS) /* I assume HX DOS doesn't bother with keyboard scancode names */
					GetKeyNameText((s.scancode ? s.scancode : event.key.keysym.scancode) << 16, nm, sizeof(nm)-1);
#endif

                    tmpl = sprintf(tmp, "Win32: VK=0x%x kn=%s",(unsigned int)s.win32_vk,nm);
                }
# endif
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
# if defined(C_SDL2)
# elif defined(SDL_DOSBOX_X_SPECIAL) && defined(LINUX)
                {
                    char *LinuxX11_KeySymName(Uint32 x);

                    char *name;

                    name = LinuxX11_KeySymName(s.x11_sym);
                    tmpl = (size_t)sprintf(tmp,"X11: Sym=0x%x sn=%s",(unsigned int)s.x11_sym,name ? name : "");
                }
# endif
#endif
                while (tmpl < (250 / 8)) tmp[tmpl++] = ' ';
                assert(tmpl < sizeof(tmp));
                tmp[tmpl] = 0;
                bind_but.dbg2->Change("%s", tmp);

                event_count++;
            }
            /* fall through to mapper UI processing */
        default:
            if (mapper.addbind) {
                for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();++it) {
                    CBind * newbind=(*it)->CreateEventBind(&event);
                    if (!newbind) continue;
                    assert(mapper.aevent != NULL);
                    mapper.aevent->AddBind(newbind);
                    SetActiveEvent(mapper.aevent);
                    mapper.addbind=false;
                    mapper.aevent->update_menu_shortcut();
                    RedrawMapperBindButton(mapper.aevent);
                    break;
                }
            }

            void MAPPER_CheckEvent(SDL_Event * event);
            MAPPER_CheckEvent(&event);
        }
    }
}

static void InitializeJoysticks(void) {
    mapper.sticks.num=0;
    mapper.sticks.num_groups=0;
    if (joytype != JOY_NONE) {
        mapper.sticks.num=(Bitu)SDL_NumJoysticks();
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
        initjoy=false;
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
        uint8_t joyno=0;
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
    for (CEventVector_it evit=events.begin();evit!=events.end();++evit) {
        if(*evit != caps_lock_event && *evit != num_lock_event)
            (*evit)->DeActivateAll();
    }
}

void MAPPER_ReleaseAllKeys(void) {
    for (CEventVector_it evit=events.begin();evit!=events.end();++evit) {
        if ((*evit)->active) {
            LOG_MSG("Release");
            (*evit)->Active(false);
        }
    }
}

void MAPPER_RunEvent(Bitu /*val*/) {
    if (hostbutton != NULL) hostbutton->Enable(hostkeyalt == 0);
    KEYBOARD_ClrBuffer();   //Clear buffer
    GFX_LosingFocus();      //Release any keys pressed (buffer gets filled again).
    MAPPER_RunInternal();
}

void MAPPER_Run(bool pressed) {
    if (pressed)
        return;
    PIC_AddEvent(MAPPER_RunEvent,0.0001f);  //In case mapper deletes the key object that ran it
}

void update_all_shortcuts() {
    for (auto &ev : events)
        if (ev != NULL) ev->update_menu_shortcut();
}

void UpdateMapperSurface()
{
#if defined(C_SDL2)
    mapper.surface = SDL_GetWindowSurface(mapper.window);

    if (mapper.surface == nullptr)
    {
        const auto error = SDL_GetError();

        E_Exit("Could not initialize video mode for mapper: %s", error);
    }
#endif
}

void GetDisplaySize(int* w, int* h)
{
#if defined(C_SDL2)
    SDL_DisplayMode mode = { };
    SDL_GetCurrentDisplayMode(0, &mode);
    *w = mode.w;
    *h = mode.h;
#endif
}

void GetWindowSize(int* w, int* h)
{
#if defined(C_SDL2)
    SDL_GetWindowSize(mapper.window, w, h);
#endif
}

void CenterWindow()
{
#if defined(C_SDL2)
    int dsp_w, dsp_h, win_w, win_h;

    GetDisplaySize(&dsp_w, &dsp_h);

    GetWindowSize(&win_w, &win_h);

    SDL_SetWindowPosition(mapper.window, dsp_w / 2 - win_w / 2, dsp_h / 2 - win_h / 2);
#endif
}

int GetMapperRenderWidth()
{
    return 640;
}

int GetMapperRenderHeight()
{
    return 480;
}

int GetMapperScaleFactor()
{
#if defined(C_SDL2)
    SDL_DisplayMode mode = { };

    const auto rw = GetMapperRenderWidth();
    const auto rh = GetMapperRenderHeight();
    const auto sf = SDL_GetCurrentDisplayMode(0, &mode) != 0 ? 1 : std::max(1, std::min(mode.w / rw, mode.h / rh));

    return sf;
#else
    return 1;
#endif
}

void MAPPER_RunInternal() {
    MAPPER_ReleaseAllKeys();

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    {
        DOSBoxMenu::item &item = mapperMenu.get_item("MapperMenu");
        item.set_text(mainMenu.get_item("mapper_mapper").get_text());
    }

    {
        DOSBoxMenu::item &item = mapperMenu.get_item("ExitMapper");
        item.set_text(MSG_Get("MAPPER_EDITOR_EXIT"));
    }

    {
        DOSBoxMenu::item &item = mapperMenu.get_item("SaveMapper");
        item.set_text(MSG_Get("SAVE_MAPPER_FILE"));
    }
# if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU || DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
    if (loadlang) mapperMenu.unbuild();
# endif
    mapperMenu.rebuild();
#endif

    /* Sorry, the MAPPER screws up 3Dfx OpenGL emulation.
     * Remove this block when fixed. */
    if (GFX_GetPreventFullscreen()) {
        systemmessagebox("Mapper Editor", MSG_Get("MAPPEREDITOR_NOT_AVAILABLE"),"ok", "info", 1);
        LOG_MSG("Mapper Editor is not available while 3Dfx OpenGL emulation is running");
        return;
    }

    mapper_esc_count = 0;
    mapper.running = true;

#if defined(__WIN32__) && !defined(C_SDL2) && !defined(C_HX_DOS)
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
    GFX_EndUpdate(nullptr);

    /* scale mapper according display resolution */
    const auto source_w = GetMapperRenderWidth();
    const auto source_h = GetMapperRenderHeight();
    const auto scale_by = GetMapperScaleFactor();
    const auto target_w = source_w * scale_by;
    const auto target_h = source_h * scale_by;

#if defined(C_SDL2)
    mapper.window_scale = scale_by;
    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(false);
    mapper.window = OpenGL_using() ? GFX_SetSDLWindowMode(target_w,target_h,SCREEN_OPENGL) : GFX_SetSDLSurfaceWindow(target_w,target_h);
    if (mapper.window == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());
    CenterWindow();
    UpdateMapperSurface();
    mapper.draw_surface=SDL_CreateRGBSurface(0,source_w,source_h,8,0,0,0,0);
    // Needed for SDL_BlitScaled
    mapper.draw_surface_nonpaletted=SDL_CreateRGBSurface(0,source_w,source_h,32,0x0000ff00,0x00ff0000,0xff000000,0);
    mapper.draw_rect=GFX_GetSDLSurfaceSubwindowDims(target_w,target_h);
    // Sorry, but SDL_SetSurfacePalette requires a full palette.
    SDL_Palette *sdl2_map_pal_ptr = SDL_AllocPalette(256);
    SDL_SetPaletteColors(sdl2_map_pal_ptr, map_pal, 0, 7);
    SDL_SetSurfacePalette(mapper.draw_surface, sdl2_map_pal_ptr);
    if (last_clicked) {
        last_clicked->SetColor(CLR_WHITE);
        last_clicked=NULL;
    }
#else
    mapper.surface=SDL_SetVideoMode(source_w, source_h,8,0);
    if (mapper.surface == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());

    /* Set some palette entries */
    SDL_SetPalette(mapper.surface, SDL_LOGPAL|SDL_PHYSPAL, map_pal, 0, 7);
    if (last_clicked) {
        last_clicked->BindColor();
        last_clicked=NULL;
    }
#endif

#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    WindowsTaskbarResetPreviewRegion();
#endif

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    DOSBox_SetMenu(mapperMenu);
#endif
    ApplyPreventCap();

    UpdateMapperSurface(); // update again because of DOSBox_SetMenu
#if defined(MACOSX)
    macosx_reload_touchbar();
#endif

    /* Go in the event loop */
    mapper.exit=false;  
    mapper.redraw=true;
    SetActiveEvent(nullptr);
#if defined (REDUCE_JOYSTICK_POLLING)
    SDL_JoystickEventState(SDL_ENABLE);
#endif
    while (!mapper.exit) {
#if C_EMSCRIPTEN
        emscripten_sleep(0);
#endif

        if (mapper.redraw) {
            mapper.redraw=false;        
            DrawButtons();
        } else {
#if defined(C_SDL2)
//            SDL_UpdateWindowSurface(mapper.window);
#endif
        }
        BIND_MappingEvents();
        SDL_Delay(1);
    }
#if defined(C_SDL2)
    SDL_FreeSurface(mapper.draw_surface);
    SDL_FreeSurface(mapper.draw_surface_nonpaletted);
    SDL_FreePalette(sdl2_map_pal_ptr);
    GFX_SetResizeable(true);
#elif C_DIRECT3D
    bool Direct3D_using(void);
    if (Direct3D_using()
# if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        && !IS_VGA_ARCH && !IS_PC98_ARCH
# endif
    ) {
        change_output(0);
        change_output(6);
    }
#endif
#if defined (REDUCE_JOYSTICK_POLLING)
    SDL_JoystickEventState(SDL_DISABLE);
#endif
    if((mousetoggle && !mouselocked) || (!mousetoggle && mouselocked)) GFX_CaptureMouse();
    SDL_ShowCursor(cursor);
    DOSBox_RefreshMenu();
    CenterWindow();
    if(!menu_gui) GFX_RestoreMode();
#if defined(__WIN32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
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

#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    WindowsTaskbarUpdatePreviewRegion();
#endif

//  KEYBOARD_ClrBuffer();
    GFX_LosingFocus();

    /* and then the menu items need to be updated */
    update_all_shortcuts();

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.rebuild();
#endif
    std::string mapper_keybind = mapper_event_keybind_string("host");
    if (mapper_keybind.empty()) mapper_keybind = "unbound";
#if __APPLE__ && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
     /* Avoid conflict in check() macro */
     mainMenu.get_item("hostkey_mapper").check2(hostkeyalt==0).set_text("Mapper-defined: "+mapper_keybind).refresh_item(mainMenu);
#else
     mainMenu.get_item("hostkey_mapper").check(hostkeyalt==0).set_text("Mapper-defined: "+mapper_keybind).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    if (!TTF_using() || ttf.inUse)
#endif
    {
        GFX_Stop();
        if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackReset );
        GFX_Start();
#if defined(USE_TTF)
        if (ttf.inUse) resetFontSize();
#endif
    }

    mapper.running = false;

#if defined(MACOSX)
    macosx_reload_touchbar();
#endif

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    DOSBox_SetMenu(mainMenu);
#endif
#if defined(WIN32) && !defined(HX_DOS)
    DOSBox_SetSysMenu();
#endif
}

bool MAPPER_IsRunning(void) {
    return mapper.running;
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

bool mapper_menu_exit(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    mapper.exit=true;
    return true;
}

bool mapper_menu_save(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_SaveBinds();
    return true;
}

std::vector<std::string> MAPPER_GetEventNames(const std::string &prefix) {
	std::vector<std::string> key_names;
	key_names.reserve(events.size());
	for (auto & e : events) {
		const std::string name = e->GetName();
		const std::size_t found = name.find(prefix);
		if (found != std::string::npos) {
			const std::string key_name = name.substr(found + prefix.length());
			key_names.push_back(key_name);
		}
	}
	return key_names;
}

void MAPPER_AutoType(std::vector<std::string> &sequence, const uint32_t wait_ms, const uint32_t pace_ms, bool choice) {
#if !defined(HX_DOS) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
	mapper.typist.Start(&events, sequence, wait_ms, pace_ms, choice);
#endif
}

void MAPPER_Init(void) {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing DOSBox-X mapper");

    mapper.exit=true;

    MAPPER_CheckKeyboardLayout();
    if (initjoy) InitializeJoysticks();
    if (buttons.empty()) CreateLayout();
    if (bindgroups.empty()) CreateBindGroups();
    if (!MAPPER_LoadBinds()) CreateDefaultBinds();
    for (CButton_it but_it = buttons.begin(); but_it != buttons.end(); ++but_it) {
        (*but_it)->BindColor();
    }
    if (SDL_GetModState()&KMOD_CAPS) {
        for (CBindList_it bit=caps_lock_event->bindlist.begin();bit!=caps_lock_event->bindlist.end();++bit) {
#if SDL_VERSION_ATLEAST(1, 2, 14)
            (*bit)->ActivateBind(32767,true,false);
            (*bit)->DeActivateBind(false);
#else
            (*bit)->ActivateBind(32767,true,true); //Skip the action itself as bios_keyboard.cpp handles the startup state.
#endif
        }
    }
    if (SDL_GetModState()&KMOD_NUM) {
        for (CBindList_it bit=num_lock_event->bindlist.begin();bit!=num_lock_event->bindlist.end();++bit) {
#if SDL_VERSION_ATLEAST(1, 2, 14)
            (*bit)->ActivateBind(32767,true,false);
            (*bit)->DeActivateBind(false);
#else
            (*bit)->ActivateBind(32767,true,true);
#endif
        }
    }

    /* and then the menu items need to be updated */
    update_all_shortcuts();
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.rebuild();
#endif
#if defined(WIN32) && !defined(HX_DOS)
    DOSBox_SetSysMenu();
#endif
}

std::string GetDOSBoxXPath(bool withexe=false);
void ResolvePath(std::string& in);
void ReloadMapper(Section_prop *section, bool init) {
    if (!init&&control->opt_defaultmapper) return;
    Prop_path* pp;
#if defined(C_SDL2)
	pp = section->Get_path("mapperfile_sdl2");
#else
	pp = section->Get_path("mapperfile_sdl1");
#endif
    mapper.filename = pp->realpath;
	if (mapper.filename=="") pp = section->Get_path("mapperfile");
    mapper.filename = pp->realpath;
    ResolvePath(mapper.filename);
    bool appendmap=false;
    FILE * loadfile=fopen(mapper.filename.c_str(),"rt");
    if (!loadfile) {
        loadfile=fopen((mapper.filename+".map").c_str(),"rt");
        if (loadfile) appendmap=true;
    }
    if (!loadfile) {
        std::string exepath=GetDOSBoxXPath(), config_path, res_path;
        Cross::GetPlatformConfigDir(config_path), Cross::GetPlatformResDir(res_path);
        if (mapper.filename.size() && exepath.size()) {
            loadfile=fopen((exepath+mapper.filename).c_str(),"rt");
            if (!loadfile) {
                loadfile=fopen((exepath+mapper.filename+".map").c_str(),"rt");
                if (loadfile) appendmap=true;
            }
            if (loadfile) {
                mapper.filename = exepath+mapper.filename+(appendmap?".map":"");
                fclose(loadfile);
                if (control->opt_erasemapper) {
                    LOG_MSG("Erase mapper file: %s\n", mapper.filename.c_str());
                    unlink(mapper.filename.c_str());
                }
            }
        }
        if (!loadfile && mapper.filename.size() && config_path.size()) {
            loadfile=fopen((config_path+mapper.filename).c_str(),"rt");
            if (!loadfile) {
                loadfile=fopen((config_path+mapper.filename+".map").c_str(),"rt");
                if (loadfile) appendmap=true;
            }
            if (loadfile) {
                mapper.filename = config_path+mapper.filename+(appendmap?".map":"");
                fclose(loadfile);
                if (control->opt_erasemapper) {
                    LOG_MSG("Erase mapper file: %s\n", mapper.filename.c_str());
                    unlink(mapper.filename.c_str());
                }
            }
        }
        if (!loadfile && mapper.filename.size() && res_path.size()) {
            loadfile=fopen((res_path+mapper.filename).c_str(),"rt");
            if (!loadfile) {
                loadfile=fopen((res_path+mapper.filename+".map").c_str(),"rt");
                if (loadfile) appendmap=true;
            }
            if (loadfile) {
                mapper.filename = res_path+mapper.filename+(appendmap?".map":"");
                fclose(loadfile);
                if (control->opt_erasemapper) {
                    LOG_MSG("Erase mapper file: %s\n", mapper.filename.c_str());
                    unlink(mapper.filename.c_str());
                }
            }
        }
    } else {
        mapper.filename = mapper.filename+(appendmap?".map":"");
        fclose(loadfile);
        if (control->opt_erasemapper) {
            LOG_MSG("Erase mapper file: %s\n", mapper.filename.c_str());
            unlink(mapper.filename.c_str());
        }
    }
	if (init) {
		GFX_LosingFocus(); //Release any keys pressed, or else they'll get stuck.
		MAPPER_Init();
	}
}

//Somehow including them at the top conflicts with something in setup.h
#ifdef C_X11_XKB
#include "SDL_syswm.h"
#include <X11/XKBlib.h>
#endif

#if !defined(C_SDL2)
void loadScanCode() {

	load=false;
	if (useScanCode()) {

        /* Note: table has to be tested/updated for various OSs */
#if defined (MACOSX)
        /* nothing */
#elif defined(HAIKU) || defined(RISCOS)
        usescancodes = 0;
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
//      sdlkey_map[0x00]=SDLK_PRINT;
        sdlkey_map[0x5E]=SDLK_RALT;
        sdlkey_map[0x40]=SDLK_KP5;
        sdlkey_map[0x41]=SDLK_KP6;
#elif !defined (WIN32) /* => Linux & BSDs */
        bool evdev_input = false;
#ifdef SDL_VIDEO_DRIVER_X11
//SDL needs to be compiled to use it, else the next makes no sense.
#ifdef C_X11_XKB
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWMInfo(&info)) {
            XkbDescPtr desc = NULL;
            if((desc = XkbGetMap(info.info.x11.display,XkbAllComponentsMask,XkbUseCoreKbd))) {
                if(XkbGetNames(info.info.x11.display,XkbAllNamesMask,desc) == 0) {
                    const char* keycodes = XGetAtomName(info.info.x11.display, desc->names->keycodes);
//                  const char* geom = XGetAtomName(info.info.x11.display, desc->names->geometry);
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
#else //defined(WIN32) Is this really required?
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
            if (key<MAX_SDLKEYS) scancode_map[key]=(uint8_t)i;
        }
    }
}
#endif

void MAPPER_StartUp() {
    Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl"));
    mapper.sticks.num=0;
    mapper.sticks.num_groups=0;

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    {
        mapperMenu.alloc_item(DOSBoxMenu::separator_type_id,"_separator_");
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MapperMenu");
        item.set_text(mainMenu.get_item("mapper_mapper").get_text());
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::item_type_id,"ExitMapper");
        item.set_callback_function(mapper_menu_exit);
        item.set_text(MSG_Get("MAPPER_EDITOR_EXIT"));
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::item_type_id,"SaveMapper");
        item.set_callback_function(mapper_menu_save);
        item.set_text(MSG_Get("SAVE_MAPPER_FILE"));
    }

    mapperMenu.displaylist_clear(mapperMenu.display_list);

    mapperMenu.displaylist_append(
        mapperMenu.display_list,
        mapperMenu.get_item_id_by_name("MapperMenu"));

    {
        mapperMenu.displaylist_append(
            mapperMenu.get_item("MapperMenu").display_list, mapperMenu.get_item_id_by_name("ExitMapper"));

        mapperMenu.displaylist_append(
            mapperMenu.get_item("MapperMenu").display_list, mapperMenu.get_item_id_by_name("_separator_"));

        mapperMenu.displaylist_append(
            mapperMenu.get_item("MapperMenu").display_list, mapperMenu.get_item_id_by_name("SaveMapper"));
    }
#endif

    LOG(LOG_MISC,LOG_DEBUG)("MAPPER starting up");

    memset(&virtual_joysticks,0,sizeof(virtual_joysticks));

#if !defined(C_SDL2)
	setScanCode(section);
	loadScanCode();
#endif

	ReloadMapper(section, false);
#if !defined(C_SDL2)
	load=true;
#endif
}

void MAPPER_Shutdown() {
    for (size_t i=0;i < events.size();i++) {
        if (events[i] != NULL) {
            delete events[i];
            events[i] = NULL;
        }
    }
    name_to_events.clear();
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
    initjoy=true;
}

void ext_signal_host_key(bool enable) {
    CEvent *x = get_mapper_event_by_name("host");
    if (x != NULL) {
        if (enable) {
            x->SetValue(32767);//HACK
            x->ActivateEvent(true,false);
        }
        else {
            x->SetValue(0);
            x->DeActivateEvent(true);
        }
    }
    else {
        fprintf(stderr,"WARNING: No host mapper event\n");
    }
}

void MapperCapCursorToggle(void) {
    MAPPER_TriggerEventByName("hand_capmouse");
}

void RedrawMapperBindButton(CEvent *ev) {
    if (ev != NULL) ev->RebindRedraw();
}

std::string mapper_event_keybind_string(const std::string &x) {
    CEvent *ev = get_mapper_event_by_name(x);
    if (ev != NULL) return ev->GetBindMenuText();
    return std::string();
}

std::string get_mapper_shortcut(const char *name) {
    for (CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();++it)
        if ((*it)!=NULL&&!strcmp(name, (*it)->eventname.c_str()))
            return (*it)->GetBindMenuText();
    return "";
}

std::map<std::string,std::string> get_event_map() {
    event_map.clear();
    for (CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();++it)
        event_map.insert(std::pair<std::string,std::string>((*it)->eventname,(*it)->ButtonName()));
    return event_map;
}

void update_bindbutton_text() {
    if (bind_but.prevpage) bind_but.prevpage->SetText("<-");
    if (bind_but.nextpage) bind_but.nextpage->SetText("->");
    if (bind_but.add) bind_but.add->SetText(MSG_Get("ADD"));
    if (bind_but.del) bind_but.del->SetText(MSG_Get("DEL"));
    if (bind_but.next) bind_but.next->SetText(MSG_Get("NEXT"));
    if (bind_but.save) bind_but.save->SetText(MSG_Get("SAVE"));
    if (bind_but.exit) bind_but.exit->SetText(MSG_Get("EXIT"));
    if (bind_but.cap) bind_but.cap->SetText(MSG_Get("CAPTURE"));
}

void set_eventbutton_text(const char *eventname, const char *buttonname) {
    for (CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();++it)
        if ((*it)!=NULL&&!strcmp(eventname, (*it)->eventname.c_str())) {
            (*it)->SetButtonName(buttonname);
            break;
        }
    for (std::vector<CEventButton *>::iterator it = ceventbuttons.begin(); it != ceventbuttons.end(); ++it) {
        CEventButton *button = (CEventButton *)*it;
        if (button!=NULL&&!strcmp(eventname, button->GetEvent()->eventname.c_str())) {
            button->SetText(buttonname);
            break;
        }
    }
}
