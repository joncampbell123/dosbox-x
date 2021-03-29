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

#include <string>
#include "config.h"
#include "menudef.h"
void SetVal(const std::string& secname, const std::string& preval, const std::string& val);

#include <SDL_video.h>

#ifdef __WIN32__
#include "programs.h"

void ToggleMenu(bool pressed);
void mem_conf(std::string memtype, int option);
void UnMount(int i_drive);
void BrowseFolder( char drive , std::string drive_type );
void Mount_Img(char drive, std::string realpath);
void Mount_Img_Floppy(char drive, std::string realpath);
void Mount_Img_HDD(char drive, std::string realpath);
void DOSBox_SetMenu(void);
void DOSBox_NoMenu(void);
void DOSBox_RefreshMenu(void);
void ToggleMenu(bool pressed);
void DOSBox_CheckOS(int &id, int &major, int &minor);
void MountDrive(char drive, const char drive2[DOS_PATHLENGTH]);
void MountDrive_2(char drive, const char drive2[DOS_PATHLENGTH], std::string drive_type);
void MENU_Check_Drive(HMENU handle, int cdrom, int floppy, int local, int image, int automount, int umount, char drive);
bool MENU_SetBool(std::string secname, std::string value);
void MENU_swapstereo(bool enabled);
void* GetSetSDLValue(int isget, std::string& target, void* setval);
void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
void change_output(int output);
void res_input(bool type, const char * res);
void res_init(void);
int Reflect_Menu(void);
extern bool DOSBox_Kor(void);

extern unsigned int hdd_defsize;
extern char hdd_size[20];
extern HWND GetHWND(void);
extern HWND GetSurfaceHWND(void);
extern void GetDefaultSize(void);
#define SCALER(opscaler,opsize) \
	if ((render.scale.op==opscaler) && (render.scale.size==opsize))

#define SCALER_SW(opscaler,opsize) \
	if ((render.scale.op==opscaler) && (render.scale.size==opsize) && (!render.scale.hardware))

#define SCALER_HW(opscaler,opsize) \
	if ((render.scale.op==opscaler) && (render.scale.size==opsize) && (render.scale.hardware))

#define SCALER_2(opscaler,opsize) \
	((render.scale.op==opscaler) && (render.scale.size==opsize))

#define SCALER_SW_2(opscaler,opsize) \
	((render.scale.op==opscaler) && (render.scale.size==opsize) && (!render.scale.hardware))

#define SCALER_HW_2(opscaler,opsize) \
	((render.scale.op==opscaler) && (render.scale.size==opsize) && (render.scale.hardware))

#define AUTOMOUNT(name,name2) \
	(((GetDriveType(name) == 2) || (GetDriveType(name) == 3) || (GetDriveType(name) == 4) || (GetDriveType(name) == 5) || (GetDriveType(name) == 6)))&&(!Drives[name2-'A'])

#else

void DOSBox_CheckOS(int &id, int &major, int &minor);
void DOSBox_RefreshMenu(void);
void DOSBox_SetMenu(void);
void DOSBox_NoMenu(void);

// dummy Win32 functions for less #ifdefs
#define GetHWND() (0)
#define SetMenu(a,b)
#define DragAcceptFiles(a,b)
#define GetMenu(a) (0)

// menu.cpp replacements; the optimizer will completely remove code based on these
#define VER_PLATFORM_WIN32_NT (1)
#define DOSBox_Kor() !strncmp("ko", getenv("LANG"), 2) // dirty hack.

#endif

/* menu interface mode */
#define DOSBOXMENU_NULL     (0)     /* nothing */
#define DOSBOXMENU_HMENU    (1)     /* Windows HMENU resources */
#define DOSBOXMENU_NSMENU   (2)     /* Mac OS X NSMenu / NSMenuItem resources */
#define DOSBOXMENU_SDLDRAW  (3)     /* menus that WE draw on the SDL surface */

#if C_FORCE_MENU_SDLDRAW /* Programmer/Dev wants to compile with SDL drawn menus even if host OS offers menus (shrug) Ok */
# define DOSBOXMENU_TYPE    DOSBOXMENU_SDLDRAW
#elif defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
# define DOSBOXMENU_TYPE    DOSBOXMENU_HMENU
#elif defined(MACOSX)
# define DOSBOXMENU_TYPE    DOSBOXMENU_NSMENU
#elif defined(C_SDL2) /* SDL 2.x only */
# define DOSBOXMENU_TYPE    DOSBOXMENU_SDLDRAW
#elif !defined(C_SDL2) /* SDL 1.x only */
# define DOSBOXMENU_TYPE    DOSBOXMENU_SDLDRAW
#else
# define DOSBOXMENU_TYPE    DOSBOXMENU_NULL
#endif

/* Whether or not the menu exists, and is NOT drawn by ourself (Windows and Mac OS X) */
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU || DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
# define DOSBOXMENU_EXTERNALLY_MANAGED
#endif

void GUI_Shortcut(int select);

#define DOSBOXMENU_ACCELMARK_STR        "\x01"
#define DOSBOXMENU_ACCELMARK_CHAR       '\x01'

#include <map>
#include <vector>

#ifndef MENU_DOSBOXMENU_H
#define MENU_DOSBOXMENU_H

class DOSBoxMenu {
    public:
        DOSBoxMenu(const DOSBoxMenu &src) = delete;             /* don't copy me */
        DOSBoxMenu(const DOSBoxMenu &&src) = delete;            /* don't move me */
    public:
        class item;
    public:
        enum item_type_t {
            item_type_id=0,
            submenu_type_id,
            separator_type_id,
            vseparator_type_id,

            MAX_id
        };
    public:
        typedef uint16_t                item_handle_t;
        typedef bool                  (*callback_t)(DOSBoxMenu * const,item * const);
        typedef std::string             mapper_event_t;     /* event name */
    public:
        class displaylist {
            friend DOSBoxMenu;

            public:
                                        displaylist();
                                        ~displaylist();
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
            public:
                void                    DrawDisplayList(DOSBoxMenu &menu,bool updateScreen=true);
                item_handle_t           itemFromPoint(DOSBoxMenu &menu,int x,int y);
#endif
            protected:
                bool                    items_changed = false;
                bool                    order_changed = false;
                std::vector<item_handle_t> disp_list;
            public:
                const std::vector<item_handle_t> &get_disp_list(void) const {
                    return disp_list;
                }
        };
    public:
        static constexpr item_handle_t  unassigned_item_handle = ((item_handle_t)(0xFFFFU)); 
        static constexpr callback_t     unassigned_callback = NULL;
        static const mapper_event_t     unassigned_mapper_event; /* empty std::string */
    public:
        struct accelerator {
                                        accelerator() { }
                                        accelerator(char _key,unsigned char _instance=0) : key(_key), key_instance(_instance) { }

            char                        key = 0;            /* ascii code i.e. 'g' */
            unsigned char               key_instance = 0;   /* which occurrence of the letter in the text */
        };
    public:
        class item {
            friend DOSBoxMenu;

            public:
                                        item();
                                        ~item();
            protected:
                std::string             name;               /* item name */
                std::string             text;               /* item text */
                std::string             shortcut_text;      /* shortcut text on the right */
                std::string             description;        /* description text */
                struct accelerator      accelerator;        /* menu accelerator */
            protected:
                item_handle_t           parent_id = unassigned_item_handle;
                item_handle_t           master_id = unassigned_item_handle;
                enum item_type_t        type = item_type_id;
            protected:
                struct status {
                                        status() : changed(false), allocated(false),
                                                   enabled(true), checked(false),
                                                   in_use(false) { };

                    unsigned int        changed:1;
                    unsigned int        allocated:1;
                    unsigned int        enabled:1;
                    unsigned int        checked:1;
                    unsigned int        in_use:1;
                } status = {};
            protected:
                callback_t              callback_func = unassigned_callback;
                mapper_event_t          mapper_event = unassigned_mapper_event;
            public:
                displaylist             display_list;
            public:
                uint64_t                user_defined = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU /* Windows menu handle */
            protected:
                HMENU                   winMenu = NULL;
            protected:
                void                    winAppendMenu(HMENU handle);
                std::string             winConstructMenuText(void);
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X menu handle */
            protected:
                void*                   nsMenuItem = NULL;
                void*                   nsMenu = NULL;
            protected:
                void                    nsAppendMenu(void *nsMenu);
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
            protected:
                SDL_Rect                screenBox = {0,0,0,0};      /* absolute screen coords */
                SDL_Rect                checkBox = {0,0,0,0};       /* relative to screenbox */
                SDL_Rect                textBox = {0,0,0,0};        /* relative to screenbox */
                SDL_Rect                shortBox = {0,0,0,0};       /* relative to screenbox */
                SDL_Rect                popupBox = {0,0,0,0};       /* absolute screen coords */
                bool                    boxInit = false;
                bool                    itemHover = false;
                bool                    needRedraw = false;
                bool                    itemHilight = false;
                bool                    itemVisible = false;
                bool                    itemHoverDrawn = false;
                bool                    itemHilightDrawn = false;
                bool                    borderTop = false;
            public:
                void                    removeFocus(DOSBoxMenu &menu);
                void                    removeHover(DOSBoxMenu &menu);
                void                    drawMenuItem(DOSBoxMenu &menu);
                void                    showItem(DOSBoxMenu &menu,bool show=true);
                item&                   setHover(DOSBoxMenu &menu,bool ho=true);
                item&                   setHilight(DOSBoxMenu &menu,bool hi=true);
                void                    placeItem(DOSBoxMenu &menu,int x,int y,bool isTopLevel=false);
                void                    placeItemFinal(DOSBoxMenu &menu,int finalwidth,bool isTopLevel=false);
                void                    layoutSubmenu(DOSBoxMenu &menu, bool isTopLevel=false);
                void                    updateScreenFromPopup(DOSBoxMenu &menu);
                void                    updateScreenFromItem(DOSBoxMenu &menu);
                void                    drawBackground(DOSBoxMenu &menu);
            public:
                inline bool isHilight(void) const {
                    return itemHilight;
                }
#endif
            protected:
                item&                   allocate(const DOSBoxMenu::item_handle_t id, const enum item_type_t new_type, const std::string& new_name);
                void                    deallocate(void);
            public:
                inline bool checkResetRedraw(void) {
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
					bool r = needRedraw || (itemHilight != itemHilightDrawn) || (itemHover != itemHoverDrawn);
                    needRedraw = false;
                    return r;
#else
					return false;
#endif
                }
                inline const std::string &get_name(void) const {
                    return name;
                }
                inline item_handle_t get_master_id(void) const {
                    return master_id;
                }
                inline bool is_allocated(void) const {
                    return master_id != unassigned_item_handle;
                }
                inline bool has_vis_text(void) const {
                    return type <= submenu_type_id;
                }
                inline bool has_vis_shortcut_text(void) const {
                    return type <= item_type_id;
                }
                inline bool has_vis_description(void) const {
                    return false;
                }
                inline bool has_vis_accelerator(void) const {
                    return type <= item_type_id;
                }
                inline bool has_vis_enabled(void) const {
                    return type <= submenu_type_id;
                }
                inline bool can_enable(void) const {
                    return type <= submenu_type_id;
                }
                inline bool has_vis_checked(void) const {
                    return type <= item_type_id;
                }
                inline bool can_check(void) const {
                    return type <= item_type_id;
                }
            public:
                void refresh_item(DOSBoxMenu &menu);
                inline bool has_changed(void) const {
                    return status.changed;
                }
                void clear_changed(void) {
                    status.changed = false;
                }
            public:
                inline item &check(const bool f=true) {
                    if (status.checked != f) {
                        status.checked  = f;
                        if (can_check() && has_vis_checked())
                            status.changed = 1;
                    }

                    return *this;
                }
                inline bool is_checked(void) const {
                    return status.checked;
                }
            public:
                inline item &enable(const bool f=true) {
                    if (status.enabled != f) {
                        status.enabled  = f;
                        if (can_enable() && has_vis_enabled())
                            status.changed = 1;
                    }

                    return *this;
                }
                inline bool is_enabled(void) const {
                    return status.enabled;
                }
            public:
                inline item_type_t get_type(void) const {
                    return type;
                }
                void set_type(const item_type_t t) {
                    if (type >= separator_type_id && t >= separator_type_id)
                        type = t;
                }
            public:
                inline callback_t get_callback_function(void) const {
                    return callback_func;
                }
                inline item &set_callback_function(const callback_t f) {
                    callback_func = f;
                    return *this;
                }
            public:
                inline mapper_event_t get_mapper_event(void) const {
                    return mapper_event;
                }
                inline item& set_mapper_event(const mapper_event_t& e) {
                    mapper_event = e;
                    return *this;
                }
            public:
                inline const std::string &get_text(void) const {
                    return text;
                }
                inline item &set_text(const std::string &str) {
                    if (has_vis_text() && text != str)
                        status.changed = 1;

                    text = str;
                    return *this;
                }
            public:
                inline const std::string &get_shortcut_text(void) const {
                    return shortcut_text;
                }
                inline item &set_shortcut_text(const std::string &str) {
                    if (has_vis_shortcut_text() && shortcut_text != str)
                        status.changed = 1;

                    shortcut_text = str;
                    return *this;
                }
            public:
                inline const std::string &get_description(void) const {
                    return description;
                }
                inline item &set_description(const std::string &str) {
                    if (has_vis_description() && description != str)
                        status.changed = 1;

                    description = str;
                    return *this;
                }
            public:
                inline const struct accelerator &get_accelerator(void) const {
                    return accelerator;
                }
                inline item &set_accelerator(const struct accelerator &str) {
                    if (has_vis_accelerator()/* && accelerator != str*//*TODO*/)
                        status.changed = 1;

                    accelerator = str;
                    return *this;
                }
        };
    public:
                                        DOSBoxMenu();
                                        ~DOSBoxMenu();
    public:
        bool                            item_exists(const item_handle_t i);
        bool                            item_exists(const std::string &name);
        std::vector<item>               get_master_list(void);
        item&                           get_item(const item_handle_t i);
        item&                           get_item(const std::string &name);
        item_handle_t                   get_item_id_by_name(const std::string &name);
        item&                           alloc_item(const enum item_type_t type,const std::string &name);
        void                            delete_item(const item_handle_t i);
        void                            clear_all_menu_items(void);
        void                            dump_log_debug(void);
        void                            dump_log_displaylist(DOSBoxMenu::displaylist &ls, unsigned int indent);
        const char*                     TypeToString(const enum item_type_t type);
        void                            rebuild(void);
        void                            unbuild(void);
    public:
        displaylist                     display_list;
    protected:
        std::vector<item>               master_list;
        std::map<std::string,item_handle_t> name_map;
        item_handle_t                   master_list_alloc = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU /* Windows menu handle */
    protected:
        HMENU                           winMenu = NULL;
        bool                            winMenuInit(void);
        void                            winMenuDestroy(void);
        bool                            winMenuSubInit(DOSBoxMenu::item &item);
    public:
        HMENU                           getWinMenu(void) const;
        bool                            mainMenuWM_COMMAND(unsigned int id);
#endif
    public:
        static constexpr unsigned int   winMenuMinimumID = 0x1000;
#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU /* Mac OS X NSMenu / NSMenuItem handle */
    protected:
        void*                           nsMenu = NULL;
        bool                            nsMenuInit(void);
        void                            nsMenuDestroy(void);
        bool                            nsMenuSubInit(DOSBoxMenu::item &item);
    public:
        void*                           getNsMenu(void) const;
        bool                            mainMenuAction(unsigned int id);
    public:
        static constexpr unsigned int   nsMenuMinimumID = 0x1000;
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    public:
        bool                            needRedraw = false;
        bool                            menuVisible = false;
        item_handle_t                   menuUserAttentionAt = unassigned_item_handle;
        item_handle_t                   menuUserHoverAt = unassigned_item_handle;
    public:
        SDL_Rect                        menuBox = {0,0,0,0};
    public:
        inline bool isVisible(void) const {
            return menuVisible;
        }
        inline bool needsRedraw(void) const {
            return needRedraw;
        }
        inline void setRedraw(void) {
            needRedraw = true;
        }
        inline void clearRedraw(void) {
            needRedraw = false;
        }
    public:
        void                            showMenu(bool show=true);
        void                            setScale(size_t s);
        void                            removeHover(void);
        void                            removeFocus(void);
        void                            updateRect(void);
        void                            layoutMenu(void);
    public:
        static constexpr size_t         menuBarHeightBase = (16 + 1);
        size_t                          menuBarHeight = menuBarHeightBase;
    public:
        size_t                          screenWidth = 640;
        size_t                          screenHeight = 400;
    public:
        static constexpr size_t         fontCharWidthBase = 8;
        static constexpr size_t         fontCharHeightBase = 16;
        static constexpr size_t         dropshadowX = 8;
        static constexpr size_t         dropshadowY = 8;
    public:
        size_t                          fontCharScale = 1;
        size_t                          fontCharWidth = fontCharWidthBase;
        size_t                          fontCharHeight = fontCharHeightBase;
#endif
    public:
        void                            dispatchItemCommand(item &item);
    public:
        static constexpr size_t         master_list_limit = 4096;
    public:
        void                            displaylist_append(displaylist &ls,const DOSBoxMenu::item_handle_t item_id);
        void                            displaylist_clear(displaylist &ls);
};

extern DOSBoxMenu mainMenu;

void DOSBox_SetMenu(DOSBoxMenu &altMenu);

#endif /* MENU_DOSBOXMENU_H */

