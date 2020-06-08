/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
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
#include "mouse.h"
#include "pic.h"
#include "control.h"
#include "joystick.h"
#include "util_math.h"
#include "keymap.h"
#include "support.h"
#include "mapper.h"
#include "setup.h"
#include "menu.h"

#include "SDL_syswm.h"

#if C_EMSCRIPTEN
# include <emscripten.h>
#endif

#include <map>

#define BMOD_Mod1               0x0001
#define BMOD_Mod2               0x0002
#define BMOD_Mod3               0x0004
#define BMOD_Host               0x0008

#define BFLG_Hold               0x0001
#define BFLG_Repeat             0x0004


#define MAXSTICKS               8
#define MAXACTIVE               16
#define MAXBUTTON               32
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
    BB_Capture
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
    Bit16s                                      axis_pos[MAX_VJOY_AXES];
    bool                                        hat_pressed[MAX_VJOY_HATS];
} virtual_joysticks[2];

static struct CMapper {
#if defined(C_SDL2)
    SDL_Window*                                 window;
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
    std::string                                 filename;
} mapper;

static struct {
    CCaptionButton*                             event_title;
    CCaptionButton*                             bind_title;
    CCaptionButton*                             selected;
    CCaptionButton*                             action;
    CCaptionButton*                             dbg2;
    CCaptionButton*                             dbg;
    CBindButton*                                save;
    CBindButton*                                exit;   
    CBindButton*                                cap;
    CBindButton*                                add;
    CBindButton*                                del;
    CBindButton*                                next;
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

extern Bit8u                                    int10_font_14[256 * 14];

std::map<std::string,std::string>               pending_string_binds;

static int                                      mapper_esc_count = 0;

Bitu                                            next_handler_xpos = 0;
Bitu                                            next_handler_ypos = 0;

bool                                            mapper_addhandler_create_buttons = false;

bool                                            isJPkeyboard = false;

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

static void                                     SetActiveEvent(CEvent * event);
static void                                     SetActiveBind(CBind * _bind);
static void                                     change_action_text(const char* text,Bit8u col);
static void                                     DrawText(Bitu x,Bitu y,const char * text,Bit8u color,Bit8u bkcolor=CLR_BLACK);
static void                                     MAPPER_SaveBinds(void);

CEvent*                                         get_mapper_event_by_name(const std::string &x);
bool                                            MAPPER_DemoOnly(void);

Bitu                                            GUI_JoystickCount(void);                // external
bool                                            GFX_GetPreventFullscreen(void);         // external
void                                            GFX_ForceRedrawScreen(void);            // external
#if defined(WIN32) && !defined(HX_DOS)
void                                            WindowsTaskbarUpdatePreviewRegion(void);// external
void                                            WindowsTaskbarResetPreviewRegion(void); // external
#endif

//! \brief Base CEvent class for mapper events
class CEvent {
public:
    //! \brief Type of CEvent class, if the code needs to use the specific type
    //!
    //! \description This is used by other parts of the mapper if it needs to retrieve
    //!              additional information that is only provided by the handler event class
    enum event_type {
        event_t=0,
        handler_event_t
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
    char entry[16];

    //! \brief Current value of the event (such as joystick position)
    Bits current_value;
};

//! \brief class for events which can be ON/OFF only: key presses, joystick buttons, joystick hat
class CTriggeredEvent : public CEvent {
public:
    //! \brief Constructor, with event name
    CTriggeredEvent(char const * const _entry) : CEvent(_entry) {}

    // methods below this line have sufficient documentation inherited from the base class

    virtual ~CTriggeredEvent() {}

    virtual bool IsTrigger(void) {
        return true;
    }

    virtual void ActivateEvent(bool ev_trigger,bool skip_action) {
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

    virtual void DeActivateEvent(bool /*ev_trigger*/) {
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

    virtual ~CContinuousEvent() {}

    virtual bool IsTrigger(void) {
        return false;
    }

    virtual void ActivateEvent(bool ev_trigger,bool skip_action) {
        if (ev_trigger) {
            activity++;
            if (!skip_action) Active(true);
        } else {
            /* test if no trigger-activity is present, this cares especially
               about activity of the opposite-direction joystick axis for example */
            if (!GetActivityCount()) Active(true);
        }
    }

    virtual void DeActivateEvent(bool ev_trigger) {
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
        event=0;
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
            if (flags & BFLG_Hold) {
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
    Bit16s value;

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
typedef char assert_right_size [MAX_SCANCODES == (sizeof(sdlkey_map)/sizeof(sdlkey_map[0])) ? 1 : -1];

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
//  Z,Z,Z,Z, Z,Z,Z,Z, Z,Z,Z,Z, Z,Z//,Z,Z

};
#endif

#undef Z


SDLKey MapSDLCode(Bitu skey) {
//  LOG_MSG("MapSDLCode %d %X",skey,skey);
    if (usescancodes) {
        if (skey<MAX_SCANCODES) return sdlkey_map[skey];
        else return SDLK_UNKNOWN;
    } else return (SDLKey)skey;
}

Bitu GetKeyCode(SDL_keysym keysym) {
//  LOG_MSG("GetKeyCode %X %X %X",keysym.scancode,keysym.sym,keysym.mod);
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
            case 0x1c:  // ENTER
            case 0x1d:  // CONTROL
            case 0x35:  // SLASH
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

class CKeyBind : public CBind {
public:
    CKeyBind(CBindList * _list,SDLKey _key) : CBind(_list, CBind::keybind_t) {
        key = _key;
    }
    virtual ~CKeyBind() {}
    virtual void BindName(char * buf) override {
#if defined(C_SDL2)
        sprintf(buf,"Key %s",SDL_GetScancodeName(key));
#else
        sprintf(buf,"Key %s",SDL_GetKeyName(MapSDLCode((Bitu)key)));
#endif
    }
    virtual void ConfigName(char * buf) override {
#if defined(C_SDL2)
        sprintf(buf,"key %d",key);
#else
        sprintf(buf,"key %d",MapSDLCode((Bitu)key));
#endif
    }
    virtual std::string GetBindMenuText(void) override {
        const char *s;
        std::string r,m;

#if defined(C_SDL2)
        s = SDL_GetScancodeName(key);
        if (s != NULL) r = s;
#else
        s = SDL_GetKeyName(MapSDLCode((Bitu)key));
		if (s != NULL) {
			r = s;
			if (r.length()>0) {
				r[0]=toupper(r[0]);
				char *c=(char *)strstr(r.c_str(), " ctrl");
				if (c==NULL) c=(char *)strstr(r.c_str(), " alt");
				if (c==NULL) c=(char *)strstr(r.c_str(), " shift");
				if (c!=NULL) *(c+1)=toupper(*(c+1));
			}
		}
#endif

        m = GetModifierText();
        if (!m.empty()) r = m + "+" + r;

        return r;
    }
public:
    SDLKey key;
};

std::string CEvent::GetBindMenuText(void) {
    std::string r;

    if (bindlist.empty())
        return std::string();

    for (auto i=bindlist.begin();i!=bindlist.end();i++) {
        CBind *b = *i;
        if (b == NULL) continue;
        if (b->type != CBind::keybind_t) continue;

        CKeyBind *kb = reinterpret_cast<CKeyBind*>(b);
        if (kb == NULL) continue;

        r += kb->GetBindMenuText();
        break;
    }

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
    CBind * CreateConfigBind(char *& buf) {
        if (strncasecmp(buf,configname,strlen(configname))) return 0;
        StripWord(buf);char * num=StripWord(buf);
        Bitu code=(Bitu)ConvDecWord(num);
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
//      LOG_MSG("key type %i is %x [%x %x]",event->type,key,event->key.keysym.sym,event->key.keysym.scancode);

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
    Bit8u dir;
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

    CBind * CreateConfigBind(char *& buf) {
        if (strncasecmp(configname,buf,strlen(configname))) return 0;
        StripWord(buf);char * type=StripWord(buf);
        CBind * bind=0;
        if (!strcasecmp(type,"axis")) {
            Bitu ax=(Bitu)ConvDecWord(StripWord(buf));
            bool pos=ConvDecWord(StripWord(buf)) > 0;
            bind=CreateAxisBind(ax,pos);
        } else if (!strcasecmp(type,"button")) {
            Bitu but=(Bitu)ConvDecWord(StripWord(buf));           
            bind=CreateButtonBind(but);
        } else if (!strcasecmp(type,"hat")) {
            Bitu hat=(Bitu)ConvDecWord(StripWord(buf));           
            Bit8u dir=(Bit8u)ConvDecWord(StripWord(buf));           
            bind=CreateHatBind(hat,dir);
        }
        return bind;
    }
    CBind * CreateEventBind(SDL_Event * event) {
        if (event->type==SDL_JOYAXISMOTION) {
            if ((unsigned int)event->jaxis.which!=(unsigned int)stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
            if (event->jaxis.axis>=axes) return 0;
#endif
            if (abs(event->jaxis.value)<25000) return 0;
            return CreateAxisBind(event->jaxis.axis,event->jaxis.value>0);
        } else if (event->type==SDL_JOYBUTTONDOWN) {
            if ((unsigned int)event->jbutton.which!=(unsigned int)stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
            return CreateButtonBind(event->jbutton.button%button_wrap);
#else
            return CreateButtonBind(event->jbutton.button);
#endif
        } else if (event->type==SDL_JOYHATMOTION) {
            if ((unsigned int)event->jhat.which!=(unsigned int)stick) return 0;
            if (event->jhat.value==0) return 0;
            if (event->jhat.value>(SDL_HAT_UP|SDL_HAT_RIGHT|SDL_HAT_DOWN|SDL_HAT_LEFT)) return 0;
            return CreateHatBind(event->jhat.hat,event->jhat.value);
        } else return 0;
    }

    virtual bool CheckEvent(SDL_Event * event) {
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

    static void ProcessInput(Bit16s x, Bit16s y, float deadzone, DOSBox_Vector2& joy)
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
        Bit16s x = virtual_joysticks[joystick].axis_pos[xAxis];
        Bit16s y = virtual_joysticks[joystick].axis_pos[yAxis];
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
        float x1 = (sgn(v.X) * fabs(pow(v.X, response)));
        float y1 = (sgn(v.Y) * fabs(pow(v.Y, response)));
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

    bool CheckEvent(SDL_Event * event) {
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
    Bit16u button_state;
};

void CBindGroup::ActivateBindList(CBindList * list,Bits value,bool ev_trigger) {
    Bitu validmod=0;
    CBindList_it it;
    for (it=list->begin();it!=list->end();++it) {
        if (((*it)->mods & mapper.mods) == (*it)->mods) {
            if (validmod<(*it)->mods) validmod=(*it)->mods;
        }
    }
    for (it=list->begin();it!=list->end();++it) {
        if (validmod==(*it)->mods) (*it)->ActivateBind(value,ev_trigger);
    }
}

void CBindGroup::DeactivateBindList(CBindList * list,bool ev_trigger) {
    CBindList_it it;
    for (it=list->begin();it!=list->end();++it) {
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
    }
    virtual void Draw(void) {
        Bit8u bg;

        if (!enabled) return;

        if (!invert)
            bg = press ? Bit8u(CLR_DARKGREEN) : bkcolor;
        else
            bg = color;

#if defined(C_SDL2)
        Bit8u * point=((Bit8u *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
        Bit8u * point=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
#endif
        for (Bitu lines=0;lines<dy;lines++)  {
            if (lines==0 || lines==(dy-1)) {
                for (Bitu cols=0;cols<dx;cols++) *(point+cols)=color;
            } else {
                for (Bitu cols=1;cols<(dx-1);cols++) *(point+cols)=bg;
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
    void SetInvert(bool inv) {
        invert=inv;
        mapper.redraw=true;
    }
    void SetPress(bool p) {
        press=p;
        mapper.redraw=true;
    }
    virtual void RebindRedraw(void) {}
    void SetColor(Bit8u _col) { color=_col; }
protected:
    Bitu x,y,dx,dy;
    Bit8u color;
    Bit8u bkcolor;
    bool press;
    bool invert;
    bool enabled;
};

static CButton *press_select = NULL;

class CTextButton : public CButton {
public:
    CTextButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text) : CButton(_x,_y,_dx,_dy) { text=_text; invertw=0; }
    virtual ~CTextButton() {}
    void Draw(void) {
        Bit8u fg,bg;

        if (!enabled) return;

        if (!invert) {
            fg = color;
            bg = press ? Bit8u(CLR_DARKGREEN) : bkcolor;
        }
        else {
            fg = bkcolor;
            bg = color;
        }

        CButton::Draw();
        DrawText(x+2,y+2,text,fg,bg);

#if defined(C_SDL2)
        Bit8u * point=((Bit8u *)mapper.draw_surface->pixels)+(y*mapper.draw_surface->w)+x;
#else
        Bit8u * point=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
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
        }
    }
    void SetText(const char *_text) {
        text = _text;
    }
    void SetPartialInvert(double a) {
        if (a < 0) a = 0;
        if (a > 1) a = 1;
        invertw = (Bitu)floor((a * (dx - 2)) + 0.5);
        if (invertw > (dx - 2)) invertw = dx - 2;
        mapper.redraw=true;
    }
protected:
    const char * text;
    Bitu invertw;
};

class CEventButton;
static CEventButton * last_clicked = NULL;

class CEventButton : public CTextButton {
public:
    CEventButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,CEvent * _event) 
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
        event=_event;   
    }
    virtual ~CEventButton() {}
    void BindColor(void) {
        this->SetColor(event->bindlist.begin() == event->bindlist.end() ? CLR_GREY : CLR_WHITE);
    }
    void Click(void) {
        if (last_clicked) last_clicked->BindColor();
        this->SetColor(event->bindlist.begin() == event->bindlist.end() ? CLR_DARKGREEN : CLR_GREEN);
        SetActiveEvent(event);
        last_clicked=this;
    }
    void RebindRedraw(void) {
        Click();//HACK!
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

void RedrawMapperBindButton(CEvent *ev);

class CBindButton : public CTextButton {
public: 
    CBindButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BB_Types _type) 
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
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
            RedrawMapperBindButton(mapper.aevent);
            break;
        case BB_Next:
            if (mapper.abindit!=mapper.aevent->bindlist.end()) 
                ++mapper.abindit;
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
    : CTextButton(_x,_y,_dx,_dy,_text)  { 
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

    virtual ~CKeyEvent() {}

    virtual void Active(bool yesno) {
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

    virtual void RebindRedraw(void) {
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
	CMouseButtonEvent(char const * const _entry,Bit8u _button) : CTriggeredEvent(_entry) {
		button=_button;
        notify_button=NULL;
	}
	void Active(bool yesno) {
		if (yesno)
			Mouse_ButtonPressed(button);
		else
			Mouse_ButtonReleased(button);
	}
    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    virtual void RebindRedraw(void) {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Text button in the mapper UI to indicate our status by
    CTextButton *notify_button;

	Bit8u button;
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

    virtual ~CJAxisEvent() {}

    virtual void Active(bool /*moved*/) {
        if (notify_button != NULL)
            notify_button->SetPartialInvert(GetValue()/32768.0);

        virtual_joysticks[stick].axis_pos[axis]=(Bit16s)(GetValue()*(positive?1:-1));
    }

    virtual Bitu GetActivityCount(void) {
        return activity|opposite_axis->activity;
    }

    virtual void RepostActivity(void) {
        /* caring for joystick movement into the opposite direction */
        opposite_axis->Active(true);
    }

    //! \brief Associate this object with a text button in the mapper GUI so that joystick position can be displayed at all times
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    virtual void RebindRedraw(void) {
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

    virtual ~CJButtonEvent() {}
    
    virtual void Active(bool pressed) {
        if (notify_button != NULL)
            notify_button->SetInvert(pressed);

        virtual_joysticks[stick].button_pressed[button]=pressed;
        active=pressed;
    }
    
    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    virtual void RebindRedraw(void) {
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

    virtual ~CJHatEvent() {}

    virtual void Active(bool pressed) {
        if (notify_button != NULL)
            notify_button->SetInvert(pressed);
        virtual_joysticks[stick].hat_pressed[(hat<<2)+dir]=pressed;
    }
    void notifybutton(CTextButton *n)
    {
        notify_button = n;
    }

    virtual void RebindRedraw(void) {
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

//! \brief Modifier trigger event, for modifier keys. This permits the user to change modifier key bindings.
class CModEvent : public CTriggeredEvent {
public:
    //! \brief Constructor to provide entry name and the index of the modifier button
    CModEvent(char const * const _entry,Bitu _wmod) : CTriggeredEvent(_entry), notify_button(NULL) {
        wmod=_wmod;
    }

    virtual ~CModEvent() {}

    virtual void Active(bool yesno) {
        if (notify_button != NULL)
            notify_button->SetInvert(yesno);

        if (yesno) mapper.mods|=((Bitu)1u << (wmod-1u));
        else mapper.mods&=~((Bitu)1u << (wmod-1u));
    };

    //! \brief Associate this object with a text button in the mapper UI
    void notifybutton(CTextButton *n) {
        notify_button = n;
    }

    virtual void RebindRedraw(void) {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    //! \brief Mapper UI text button to indicate status by
    CTextButton *notify_button;
protected:
    //! \brief Modifier button index
    Bitu wmod;
};

std::string CBind::GetModifierText(void) {
    std::string r,t;

    for (size_t m=4u/*Host key first*/;m >= 1u;m--) {
        if ((mods & ((Bitu)1u << (m - 1u))) && mod_event[m] != NULL) {
            t = mod_event[m]->GetBindMenuText();
            if (!r.empty()) r += "+";
            r += t;
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
        buttonname=_buttonname;
        handlergroup.push_back(this);
        type = handler_event_t;
    }

    virtual ~CHandlerEvent() {}

    virtual void RebindRedraw(void) {
        if (notify_button != NULL)
            notify_button->RebindRedraw();
    }

    virtual void Active(bool yesno) {
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
    const char * ButtonName(void) {
        return buttonname;
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
        case MK_return:
            key=SDL_SCANCODE_RETURN;
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
        case MK_c:
            key=SDL_SCANCODE_C;
            break;
        case MK_d:
            key=SDL_SCANCODE_D;
            break;
        case MK_f:
            key=SDL_SCANCODE_F;
            break;
        case MK_m:
            key=SDL_SCANCODE_M;
            break;
        case MK_r:
            key=SDL_SCANCODE_R;
            break;
        case MK_s:
            key=SDL_SCANCODE_S;
            break;
        case MK_v:
            key=SDL_SCANCODE_V;
            break;
        case MK_w:
            key=SDL_SCANCODE_W;
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
        case MK_return:
            key=SDLK_RETURN;
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
        case MK_c:
            key=SDLK_c;
            break;
        case MK_d:
            key=SDLK_d;
            break;
        case MK_f:
            key=SDLK_f;
            break;
        case MK_m:
            key=SDLK_m;
            break;
        case MK_r:
            key=SDLK_r;
            break;
        case MK_s:
            key=SDLK_s;
            break;
        case MK_v:
            key=SDLK_v;
            break;
        case MK_w:
            key=SDLK_w;
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
    const char * buttonname;
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

static void DrawText(Bitu x,Bitu y,const char * text,Bit8u color,Bit8u bkcolor/*=CLR_BLACK*/) {
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
#if defined(C_SDL2)
    SDL_FillRect(mapper.draw_surface,0,0);
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
    CEventButton *button=new CEventButton(x,y,dx,dy,title,event);
    event->notifybutton(button);
    return event;
}

static CMouseButtonEvent * AddMouseButtonEvent(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,char const * const entry,Bit8u key) {
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

    assert(_mod < 8);
    mod_event[_mod] = event;
}

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

    if (IS_PC98_ARCH) {
        for (i=0;i<14;i++) AddKeyButtonEvent(PX(  i),PY(1),BW,BH,combo_1_pc98[i].title,combo_1_pc98[i].entry,combo_1_pc98[i].key);
    }
    else {
        for (i=0;i<14;i++) AddKeyButtonEvent(PX(  i),PY(1),BW,BH,combo_1[i].title,combo_1[i].entry,combo_1[i].key);
    }

    AddKeyButtonEvent(PX(0),PY(2),BW*2,BH,"TAB","tab",KBD_tab);
    for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(2),BW,BH,combo_2[i].title,combo_2[i].entry,combo_2[i].key);

    AddKeyButtonEvent(PX(14),PY(2),BW*2,BH*2,"ENTER","enter",KBD_enter);
    
    caps_lock_event=AddKeyButtonEvent(PX(0),PY(3),BW*2,BH,"CLCK","capslock",KBD_capslock);

    if (IS_PC98_ARCH) {
        for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(3),BW,BH,combo_3_pc98[i].title,combo_3_pc98[i].entry,combo_3_pc98[i].key);
    }
    else {
        for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(3),BW,BH,combo_3[i].title,combo_3[i].entry,combo_3[i].key);
    }

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
#define XO 18
#define YO 0

    AddKeyButtonEvent(PX(XO+0),PY(YO),BW,BH,"PRT","printscreen",KBD_printscreen);
    AddKeyButtonEvent(PX(XO+1),PY(YO),BW,BH,"SCL","scrolllock",KBD_scrolllock);
    AddKeyButtonEvent(PX(XO+2),PY(YO),BW,BH,"PAU","pause",KBD_pause);
    AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"INS","insert",KBD_insert);
    AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"HOM","home",KBD_home);
    AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"PUP","pageup",KBD_pageup);
    AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW,BH,"DEL","delete",KBD_delete);
    AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"END","end",KBD_end);
    AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"PDN","pagedown",KBD_pagedown);
    AddKeyButtonEvent(PX(XO-4),PY(YO),BW,BH,"NEQ","kp_equals",KBD_kpequals);
    AddKeyButtonEvent(PX(XO-2),PY(YO),BW,BH,"\x18 U","up",KBD_up);
    AddKeyButtonEvent(PX(XO-3),PY(YO+1),BW,BH,"\x1B L","left",KBD_left);
    AddKeyButtonEvent(PX(XO-2),PY(YO+1),BW,BH,"\x19 D","down",KBD_down);
    AddKeyButtonEvent(PX(XO-1),PY(YO+1),BW,BH,"\x1A R","right",KBD_right);
#undef XO
#undef YO
#define XO 18
#define YO 5
	/* Mouse Buttons */
	new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Mouse keys");
	AddMouseButtonEvent(PX(XO+0),PY(YO),BW,BH,"L","left",0);
	AddMouseButtonEvent(PX(XO+1),PY(YO),BW,BH,"M","middle",2);
	AddMouseButtonEvent(PX(XO+2),PY(YO),BW,BH,"R","right",1);
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
    if (IS_PC98_ARCH) {
        AddKeyButtonEvent(PX(XO+0),PY(YO+4),BW*1,BH,"0","kp_0",KBD_kp0);
        AddKeyButtonEvent(PX(XO+1),PY(YO+4),BW*1,BH,",","kp_comma",KBD_kpcomma);
    }
    else {
        AddKeyButtonEvent(PX(XO+0),PY(YO+4),BW*2,BH,"0","kp_0",KBD_kp0);
    }
    AddKeyButtonEvent(PX(XO+2),PY(YO+4),BW,BH,".","kp_period",KBD_kpperiod);
#undef XO
#undef YO
#define XO 5
#define YO 7
    if (IS_PC98_ARCH) {
        /* PC-98 extra keys */
        AddKeyButtonEvent(PX(XO+0),PY(YO+0),BW*2,BH,"STOP","stop",KBD_stop);
        AddKeyButtonEvent(PX(XO+2),PY(YO+0),BW*2,BH,"HELP","help",KBD_help);

        AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW*2,BH,"COPY","copy",KBD_copy);
        AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW*2,BH,"KANA","kana",KBD_kana);

        AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW*2,BH,"NFER","nfer",KBD_nfer);
        AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW*2,BH,"XFER","xfer",KBD_xfer);

        AddKeyButtonEvent(PX(XO+2),PY(YO+3),BW*2,BH,"Ro / _","jp_ro",KBD_jp_ro);

        AddKeyButtonEvent(PX(XO+0),PY(YO+3),BW*1,BH,"VF1","vf1",KBD_vf1);
        AddKeyButtonEvent(PX(XO+1),PY(YO+3),BW*1,BH,"VF2","vf2",KBD_vf2);
        AddKeyButtonEvent(PX(XO+0),PY(YO+4),BW*1,BH,"VF3","vf3",KBD_vf3);
        AddKeyButtonEvent(PX(XO+1),PY(YO+4),BW*1,BH,"VF4","vf4",KBD_vf4);
        AddKeyButtonEvent(PX(XO+2),PY(YO+4),BW*1,BH,"VF5","vf5",KBD_vf5);
    }
    else {
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
    }
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
    AddKeyButtonEvent(PX(XO+6),PY(YO+2),BW*1,BH,":*",      "colon",      KBD_colon);
    AddKeyButtonEvent(PX(XO+7),PY(YO+0),BW*1,BH,"^`",      "caret",      KBD_caret);
    AddKeyButtonEvent(PX(XO+7),PY(YO+1),BW*1,BH,"@~",      "atsign",     KBD_atsign);
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
        cjaxis= AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",1,0,false,NULL);
                AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",1,0,true,cjaxis);
        cjaxis= AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",1,1,false,NULL);
                AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",1,1,true,cjaxis);
        /* Axes 3+4 (X+Y) of 1st Joystick, not accessible */
        cjaxis= AddJAxisButton_hidden(0,2,false,NULL);
                AddJAxisButton_hidden(0,2,true,cjaxis);
        cjaxis= AddJAxisButton_hidden(0,3,false,NULL);
                AddJAxisButton_hidden(0,3,true,cjaxis);
    } else {
        /* Buttons 3+4 of 1st Joystick */
        AddJButtonButton(PX(XO+4),PY(YO),BW,BH,"3" ,0,2);
        AddJButtonButton(PX(XO+4+2),PY(YO),BW,BH,"4" ,0,3);
        /* Buttons 1+2 of 2nd Joystick, not accessible */
        AddJButtonButton_hidden(1,0);
        AddJButtonButton_hidden(1,1);

        /* Axes 3+4 (X+Y) of 1st Joystick */
        cjaxis= AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",0,2,false,NULL);
                AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",0,2,true,cjaxis);
        cjaxis= AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",0,3,false,NULL);
                AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",0,3,true,cjaxis);
        /* Axes 1+2 (X+Y) of 2nd Joystick , not accessible*/
        cjaxis= AddJAxisButton_hidden(1,0,false,NULL);
                AddJAxisButton_hidden(1,0,true,cjaxis);
        cjaxis= AddJAxisButton_hidden(1,1,false,NULL);
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
    CTextButton* btn;
    if (joytype ==JOY_2AXIS) {
        new CTextButton(PX(XO+0),PY(YO-1),3*BW,BH,"Joystick 1");
        new CTextButton(PX(XO+4),PY(YO-1),3*BW,BH,"Joystick 2");
        btn = new CTextButton(PX(XO + 8), PY(YO - 1), 3 * BW, BH, "Disabled");
        btn->SetColor(CLR_GREY);
    } else if(joytype ==JOY_4AXIS || joytype == JOY_4AXIS_2) {
        new CTextButton(PX(XO+0),PY(YO-1),3*BW,BH,"Axis 1/2");
        new CTextButton(PX(XO+4),PY(YO-1),3*BW,BH,"Axis 3/4");
        btn = new CTextButton(PX(XO + 8), PY(YO - 1), 3 * BW, BH, "Disabled");
        btn->SetColor(CLR_GREY);
    } else if(joytype == JOY_CH) {
        new CTextButton(PX(XO+0),PY(YO-1),3*BW,BH,"Axis 1/2");
        new CTextButton(PX(XO+4),PY(YO-1),3*BW,BH,"Axis 3/4");
        new CTextButton(PX(XO+8),PY(YO-1),3*BW,BH,"Hat/D-pad");
    } else if ( joytype==JOY_FCS) {
        new CTextButton(PX(XO+0),PY(YO-1),3*BW,BH,"Axis 1/2");
        new CTextButton(PX(XO+4),PY(YO-1),3*BW,BH,"Axis 3");
        new CTextButton(PX(XO+8),PY(YO-1),3*BW,BH,"Hat/D-pad");
    } else if(joytype == JOY_NONE) {
        btn = new CTextButton(PX(XO + 0), PY(YO - 1), 3 * BW, BH, "Disabled");
        btn->SetColor(CLR_GREY);
        btn = new CTextButton(PX(XO + 4), PY(YO - 1), 3 * BW, BH, "Disabled");
        btn->SetColor(CLR_GREY);
        btn = new CTextButton(PX(XO + 8), PY(YO - 1), 3 * BW, BH, "Disabled");
        btn->SetColor(CLR_GREY);
    }
#undef XO
#undef YO
   
    /* The modifier buttons */
    AddModButton(PX(0),PY(17),50,BH,"Mod1",1);
    AddModButton(PX(2),PY(17),50,BH,"Mod2",2);
    AddModButton(PX(4),PY(17),50,BH,"Mod3",3);
    AddModButton(PX(6),PY(17),50,BH,"Host",4);
    /* Create Handler buttons */
    Bitu xpos=3;Bitu ypos=11;
    for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();++hit) {
        unsigned int columns = ((unsigned int)strlen((*hit)->ButtonName()) + 9U) / 10U;
        if ((xpos+columns-1)>6) {
            xpos=3;ypos++;
        }
        CEventButton *button=new CEventButton(PX(xpos*3),PY(ypos),BW*3*columns,BH,(*hit)->ButtonName(),(*hit));
        (*hit)->notifybutton(button);
        xpos += columns;
        if (xpos>6) {
            xpos=3;ypos++;
        }
    }
    next_handler_xpos = xpos;
    next_handler_ypos = ypos;
    /* Create some text buttons */
//  new CTextButton(PX(6),0,124,BH,"Keyboard Layout");
//  new CTextButton(PX(17),0,124,BH,"Joystick Layout");

    bind_but.action=new CCaptionButton(180,426,0,0);

    bind_but.event_title=new CCaptionButton(0,350,0,0);
    bind_but.bind_title=new CCaptionButton(0,365,0,0);

    /* Create binding support buttons */
    
    bind_but.mod1=new CCheckButton(20,410,60,BH, "mod1",BC_Mod1);
    bind_but.mod2=new CCheckButton(20,432,60,BH, "mod2",BC_Mod2);
    bind_but.mod3=new CCheckButton(20,454,60,BH, "mod3",BC_Mod3);
    bind_but.host=new CCheckButton(100,410,60,BH,"host",BC_Host);
    bind_but.hold=new CCheckButton(100,432,60,BH,"hold",BC_Hold);

    bind_but.add=new CBindButton(20,384,50,BH,"Add",BB_Add);
    bind_but.del=new CBindButton(70,384,50,BH,"Del",BB_Del);
    bind_but.next=new CBindButton(120,384,50,BH,"Next",BB_Next);

    bind_but.save=new CBindButton(180,444,50,BH,"Save",BB_Save);
    bind_but.exit=new CBindButton(230,444,50,BH,"Exit",BB_Exit);
    bind_but.cap=new CBindButton(280,444,50,BH,"Capt",BB_Capture);

    bind_but.dbg = new CCaptionButton(180, 462, 460, 20); // right below the Save button
    bind_but.dbg->Change("(event debug)");

    bind_but.dbg2 = new CCaptionButton(330, 444, 310, 20); // right next to the Save button
    bind_but.dbg2->Change("%s", "");

    bind_but.bind_title->Change("Bind Title");

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

#if defined(C_SDL2)
    {"printscreen",SDLK_PRINTSCREEN},
    {"scrolllock",SDLK_SCROLLLOCK},
#else
    {"printscreen",SDLK_PRINT},
    {"scrolllock",SDLK_SCROLLOCK},
#endif

    {"pause",SDLK_PAUSE},       {"pagedown",SDLK_PAGEDOWN},
    {"pageup",SDLK_PAGEUP}, {"insert",SDLK_INSERT},     {"home",SDLK_HOME},
    {"delete",SDLK_DELETE}, {"end",SDLK_END},           {"up",SDLK_UP},
    {"left",SDLK_LEFT},     {"down",SDLK_DOWN},         {"right",SDLK_RIGHT},

#if defined(C_SDL2)
    {"kp_0",SDLK_KP_0}, {"kp_1",SDLK_KP_1}, {"kp_2",SDLK_KP_2}, {"kp_3",SDLK_KP_3},
    {"kp_4",SDLK_KP_4}, {"kp_5",SDLK_KP_5}, {"kp_6",SDLK_KP_6}, {"kp_7",SDLK_KP_7},
    {"kp_8",SDLK_KP_8}, {"kp_9",SDLK_KP_9},
    {"numlock",SDLK_NUMLOCKCLEAR},
#else
    {"kp_0",SDLK_KP0},  {"kp_1",SDLK_KP1},  {"kp_2",SDLK_KP2},  {"kp_3",SDLK_KP3},
    {"kp_4",SDLK_KP4},  {"kp_5",SDLK_KP5},  {"kp_6",SDLK_KP6},  {"kp_7",SDLK_KP7},
    {"kp_8",SDLK_KP8},  {"kp_9",SDLK_KP9},
    {"numlock",SDLK_NUMLOCK},
#endif

    {"kp_divide",SDLK_KP_DIVIDE},   {"kp_multiply",SDLK_KP_MULTIPLY},
    {"kp_minus",SDLK_KP_MINUS},     {"kp_plus",SDLK_KP_PLUS},
    {"kp_period",SDLK_KP_PERIOD},   {"kp_enter",SDLK_KP_ENTER},

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
    FILE * savefile=fopen(mapper.filename.c_str(),"wt+");
    if (!savefile) {
        LOG_MSG("Can't open %s for saving the mappings",mapper.filename.c_str());
        return;
    }
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
    emscripten_sleep_with_yield(0);
#endif

    while (SDL_PollEvent(&event)) {
#if C_EMSCRIPTEN
        emscripten_sleep_with_yield(0);
#endif

        switch (event.type) {
#if !defined(C_SDL2) && defined(_WIN32) && !defined(HX_DOS)
        case SDL_SYSWMEVENT : {
            switch ( event.syswm.msg->msg ) {
                case WM_COMMAND:
# if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                    if (GetMenu(GetHWND())) {
                        if (mapperMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->wParam))) return;
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
                tmpl = (size_t)sprintf(tmp,"%c%02x: scan=%d sym=%d mod=%xh name=%s",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    s.scancode,
                    s.sym,
                    s.mod,
                    SDL_GetScancodeName(s.scancode));
#else
                tmpl = (size_t)sprintf(tmp,"%c%02x: scan=%u sym=%u mod=%xh u=%xh name=%s",
                    (event.type == SDL_KEYDOWN ? 'D' : 'U'),
                    event_count&0xFF,
                    s.scancode,
                    s.sym,
                    s.mod,
                    s.unicode,
                    SDL_GetKeyName((SDLKey)MapSDLCode((Bitu)s.sym)));
#endif
                while (tmpl < (440/8)) tmp[tmpl++] = ' ';
                assert(tmpl < sizeof(tmp));
                tmp[tmpl] = 0;

                LOG(LOG_GUI,LOG_DEBUG)("Mapper keyboard event: %s",tmp);
                bind_but.dbg->Change("%s",tmp);

                tmpl = 0;
#if defined(WIN32)
# if defined(C_SDL2)
# else
                {
                    char nm[256];

                    nm[0] = 0;
#if !defined(HX_DOS) /* I assume HX DOS doesn't bother with keyboard scancode names */
                    GetKeyNameText(s.scancode << 16,nm,sizeof(nm)-1);
#endif

                    tmpl = sprintf(tmp, "Win32: VK=0x%x kn=%s",(unsigned int)s.win32_vk,nm);
                }
# endif
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
# if defined(C_SDL2)
# else
                {
                    char *LinuxX11_KeySymName(Uint32 x);

                    char *name;

                    name = LinuxX11_KeySymName(s.x11_sym);
                    tmpl = (size_t)sprintf(tmp,"X11: Sym=0x%x sn=%s",(unsigned int)s.x11_sym,name ? name : "");
                }
# endif
#endif
                while (tmpl < (310 / 8)) tmp[tmpl++] = ' ';
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
    KEYBOARD_ClrBuffer();   //Clear buffer
    GFX_LosingFocus();      //Release any keys pressed (buffer gets filled again).
    MAPPER_RunInternal();
}

void MAPPER_Run(bool pressed) {
    if (pressed)
        return;
    PIC_AddEvent(MAPPER_RunEvent,0.0001f);  //In case mapper deletes the key object that ran it
}

void MAPPER_RunInternal() {
    MAPPER_ReleaseAllKeys();

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    mapperMenu.rebuild();
#endif

    /* Sorry, the MAPPER screws up 3Dfx OpenGL emulation.
     * Remove this block when fixed. */
    if (GFX_GetPreventFullscreen()) {
        LOG_MSG("MAPPER ui is not available while 3Dfx OpenGL emulation is running");
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
    GFX_EndUpdate( 0 );
#if defined(C_SDL2)
    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(false);
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
    SDL_SetPaletteColors(sdl2_map_pal_ptr, map_pal, 0, 7);
    SDL_SetSurfacePalette(mapper.draw_surface, sdl2_map_pal_ptr);
    if (last_clicked) {
        last_clicked->SetColor(CLR_WHITE);
        last_clicked=NULL;
    }
#else
    mapper.surface=SDL_SetVideoMode(640,480,8,0);
    if (mapper.surface == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());

    /* Set some palette entries */
    SDL_SetPalette(mapper.surface, SDL_LOGPAL|SDL_PHYSPAL, map_pal, 0, 7);
    if (last_clicked) {
        last_clicked->BindColor();
        last_clicked=NULL;
    }
#endif

#if defined(WIN32) && !defined(HX_DOS)
    WindowsTaskbarResetPreviewRegion();
#endif

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    DOSBox_SetMenu(mapperMenu);
#endif

#if defined(MACOSX)
    void osx_reload_touchbar(void);
    osx_reload_touchbar();
#endif

    /* Go in the event loop */
    mapper.exit=false;  
    mapper.redraw=true;
    SetActiveEvent(0);
#if defined (REDUCE_JOYSTICK_POLLING)
    SDL_JoystickEventState(SDL_ENABLE);
#endif
    while (!mapper.exit) {
#if C_EMSCRIPTEN
        emscripten_sleep_with_yield(0);
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
#endif
#if defined (REDUCE_JOYSTICK_POLLING)
    SDL_JoystickEventState(SDL_DISABLE);
#endif
    if((mousetoggle && !mouselocked) || (!mousetoggle && mouselocked)) GFX_CaptureMouse();
    SDL_ShowCursor(cursor);
#if !defined(C_SDL2)
    DOSBox_RefreshMenu();
#endif
    if(!menu_gui) GFX_RestoreMode();
#if defined(__WIN32__) && !defined(HX_DOS)
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

#if defined(WIN32) && !defined(HX_DOS)
    WindowsTaskbarUpdatePreviewRegion();
#endif

//  KEYBOARD_ClrBuffer();
    GFX_LosingFocus();

    /* and then the menu items need to be updated */
    for (auto &ev : events) {
        if (ev != NULL) ev->update_menu_shortcut();
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.rebuild();
#endif

    GFX_ForceRedrawScreen();

    mapper.running = false;

#if defined(MACOSX)
    void osx_reload_touchbar(void);
    osx_reload_touchbar();
#endif

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    DOSBox_SetMenu(mainMenu);
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

void MAPPER_Init(void) {
    LOG(LOG_MISC,LOG_DEBUG)("Initializing DOSBox mapper");

    mapper.exit=true;

    MAPPER_CheckKeyboardLayout();
    InitializeJoysticks();
    CreateLayout();
    CreateBindGroups();
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
    for (auto &ev : events) {
        if (ev != NULL) ev->update_menu_shortcut();
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

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    {
        mapperMenu.alloc_item(DOSBoxMenu::separator_type_id,"_separator_");
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MapperMenu");
        item.set_text("Mapper");
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::item_type_id,"ExitMapper");
        item.set_callback_function(mapper_menu_exit);
        item.set_text("Exit mapper");
    }

    {
        DOSBoxMenu::item &item = mapperMenu.alloc_item(DOSBoxMenu::item_type_id,"SaveMapper");
        item.set_callback_function(mapper_menu_save);
        item.set_text("Save mapper file");
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
    usescancodes = false;

    if (section->Get_bool("usescancodes")) {
        usescancodes=true;

        /* Note: table has to be tested/updated for various OSs */
#if defined (MACOSX)
        /* nothing */
#elif defined(HAIKU) || defined(RISCOS)
        usescancodes = false;
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
    Prop_path* pp;
#if defined(C_SDL2)
	pp = section->Get_path("mapperfile_sdl2");
    mapper.filename = pp->realpath;
	if (mapper.filename=="") pp = section->Get_path("mapperfile");
#else
    pp = section->Get_path("mapperfile");
#endif
    mapper.filename = pp->realpath;

    {
        DOSBoxMenu::item *itemp = NULL;

        MAPPER_AddHandler(&MAPPER_Run,MK_m,MMODHOST,"mapper","Mapper",&itemp);
        itemp->set_accelerator(DOSBoxMenu::accelerator('m'));
        itemp->set_description("Bring up the mapper UI");
        itemp->set_text("Mapper editor");
    }
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

