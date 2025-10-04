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

#if defined(_MSC_VER)
#pragma warning(disable:4065) /* Please do not warn on default case without other case statements */
#endif

#include "SDL.h"

#include "dosbox.h"
#include "menu.h"
#include "../libs/gui_tk/gui_tk.h"

#include "build_timestamp.h"
#include "keyboard.h"
#include "video.h"
#include "render.h"
#include "mapper.h"
#include "setup.h"
#include "control.h"
#include "paging.h"
#include "shell.h"
#include "cpu.h"
#include "pic.h"
#include "midi.h"
#include "bios_disk.h"
#include "../dos/drives.h"
#include "../ints/int10.h"

#if C_OPENGL
#include "voodoo.h"
#include "../hardware/voodoo_types.h"
#include "../hardware/voodoo_data.h"
#include "../hardware/voodoo_opengl.h"
#endif

#if defined(WIN32)
#include "shellapi.h"
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <functional>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

#include "SDL_syswm.h"
#include "sdlmain.h"
#include "version_string.h"

#if !defined(HX_DOS)
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#endif

#include <unordered_map>
#include <output/output_ttf.h>

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
static DOSBoxMenu guiMenu, nullMenu;
#endif

/* helper class for command execution */
class VirtualBatch : public BatchFile {
public:
                            VirtualBatch(DOS_Shell *host, const std::string& cmds);
    bool                    ReadLine(char *line) override;
protected:
    std::istringstream      lines;
};

extern uint32_t             GFX_Rmask;
extern unsigned char        GFX_Rshift;
extern uint32_t             GFX_Gmask;
extern unsigned char        GFX_Gshift;
extern uint32_t             GFX_Bmask;
extern unsigned char        GFX_Bshift;

extern unsigned int         maincp;
extern int                  aspect_ratio_x, aspect_ratio_y;
extern int                  statusdrive, swapInDisksSpecificDrive;
extern bool                 ttfswitch, switch_output_from_ttf, loadlang;
extern bool                 dos_kernel_disabled, swapad, confres, font_14_init;
extern Bitu                 currentWindowWidth, currentWindowHeight;
extern std::string          strPasteBuffer, langname;

extern void                 resetFontSize(void);
extern void                 MAPPER_ReleaseAllKeys(void);
extern void                 PasteClipboard(bool bPressed);
extern bool                 MSG_Write(const char *, const char *);
extern void                 LoadMessageFile(const char * fname);
extern void                 GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);

static int                  cursor;
static bool                 running;
static int                  saved_bpp;
static bool                 shell_idle;
static bool                 in_gui = false;
static bool                 resetcfg = false;
#if !defined(C_SDL2)
static int                  old_unicode;
#endif
static bool                 mousetoggle;
static bool                 shortcut=false;
static SDL_Surface*         screenshot = NULL;
static SDL_Surface*         background = NULL;
#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
static bool                 gui_menu_init = true, null_menu_init = true;
#endif
int                         shortcutid = -1;
bool                        GFX_GetPreventFullscreen(void);
void                        GFX_GetSizeAndPos(int &x,int &y,int &width, int &height, bool &fullscreen);

#if defined(WIN32) && !defined(HX_DOS)
void                        DOSBox_SetSysMenu(void);
void                        WindowsTaskbarUpdatePreviewRegion(void);
void                        WindowsTaskbarResetPreviewRegion(void);
#endif

#if defined(MACOSX)
void                        macosx_reload_touchbar(void);
#endif

std::list<std::string> proplist = {};
GUI::Checkbox *advopt, *saveall, *imgfd360, *imgfd400, *imgfd720, *imgfd1200, *imgfd1440, *imgfd2880, *imghd250, *imghd520, *imghd1gig, *imghd2gig, *imghd4gig, *imghd8gig;

// user pick of 'show advanced options' for the session
bool advOptUser = false;

std::string GetDOSBoxXPath(bool withexe);
static std::map< std::vector<GUI::Char>, GUI::ToplevelWindow* > cfg_windows_active;
void getlogtext(std::string &str), getcodetext(std::string &text), ApplySetting(std::string pvar, std::string inputline, bool quiet), GUI_Run(bool pressed);
void ttf_switch_on(bool ss=true), ttf_switch_off(bool ss=true), setAspectRatio(Section_prop * section), GFX_ForceRedrawScreen(void), SetWindowTransparency(int trans);
bool CheckQuit(void), OpenGL_using(void);
char tmp1[CROSS_LEN*2], tmp2[CROSS_LEN];
const char *aboutmsg = "DOSBox-X ver." VERSION " (" OS_PLATFORM " " SDL_STRING " " OS_BIT "-bit)\n" \
                       "Build date/time: " UPDATED_STR "\nCopyright 2011-" COPYRIGHT_END_YEAR \
                       " The DOSBox-X Team\nProject maintainer: joncampbell123\nDOSBox-X homepage: https://dosbox-x.com";

void RebootConfig(std::string filename, bool confirm=false) {
    std::string exepath=GetDOSBoxXPath(true), para="-conf \""+filename+"\"";
    if ((!confirm||CheckQuit())&&exepath.size()) {
#if defined(WIN32)
        ShellExecute(NULL, "open", exepath.c_str(), para.c_str(), NULL, SW_NORMAL);
#else
        system((exepath+" "+para+ " &").c_str());
#endif
        throw(0);
    }
}

void RebootLanguage(std::string filename, bool confirm=false) {
    std::string exepath=GetDOSBoxXPath(true), tmpconfig = "~dbxtemp.conf", para=(filename.size()?"-langcp \""+filename+"\"":"");
#if defined(USE_TTF)
    para+=ttfswitch||switch_output_from_ttf?" -set output=ttf":"";
#endif
    struct stat st;
    if ((!confirm||CheckQuit())&&exepath.size()) {
        if (!stat(tmpconfig.c_str(), &st)) remove(tmpconfig.c_str());
        if (control->PrintConfig(tmpconfig.c_str(),false,true)&&!stat(tmpconfig.c_str(), &st)) para="-conf "+tmpconfig+" -eraseconf "+para;
#if defined(WIN32)
        ShellExecute(NULL, "open", exepath.c_str(), para.c_str(), NULL, SW_NORMAL);
#else
        system((exepath+" "+para+ " &").c_str());
#endif
        throw(0);
    }
}

/* Prepare screen for UI */
void GUI_LoadFonts(void) {
    GUI::Font::addFont("default",new GUI::BitmapFont(font_14_init&&dos.loaded_codepage&&dos.loaded_codepage!=437?int10_font_14_init:int10_font_14,14,10));
}

static void getPixel(Bits x, Bits y, int &r, int &g, int &b, int shift)
{
    if (x >= (Bits)render.src.width) x = (Bits)render.src.width-1;
    if (y >= (Bits)render.src.height) x = (Bits)render.src.height-1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    uint8_t* src = (uint8_t *)&scalerSourceCache;
    uint32_t pixel;
    switch (render.scale.inMode) {
        case scalerMode8:
            pixel = *((unsigned int)x+(uint8_t*)(src+(unsigned int)y*(unsigned int)render.scale.cachePitch));
            r += (int)((unsigned int)render.pal.rgb[pixel].red >> (unsigned int)shift);
            g += (int)((unsigned int)render.pal.rgb[pixel].green >> (unsigned int)shift);
            b += (int)((unsigned int)render.pal.rgb[pixel].blue >> (unsigned int)shift);
            break;
        case scalerMode15:
            pixel = *((unsigned int)x+(uint16_t*)(src+(unsigned int)y*(unsigned int)render.scale.cachePitch));
            r += (int)((pixel >> (7u+(unsigned int)shift)) & (0xf8u >> (unsigned int)shift));
            g += (int)((pixel >> (2u+(unsigned int)shift)) & (0xf8u >> (unsigned int)shift));
            b += (int)((pixel << (3u-(unsigned int)shift)) & (0xf8u >> (unsigned int)shift));
            break;
        case scalerMode16:
            pixel = *((unsigned int)x+(uint16_t*)(src+(unsigned int)y*(unsigned int)render.scale.cachePitch));
            r += (int)((pixel >> (8u+(unsigned int)shift)) & (0xf8u >> shift));
            g += (int)((pixel >> (3u+(unsigned int)shift)) & (0xfcu >> shift));
            b += (int)((pixel << (3u-(unsigned int)shift)) & (0xf8u >> shift));
            break;
        case scalerMode32:
            pixel = *((unsigned int)x+(uint32_t*)(src+(unsigned int)y*(unsigned int)render.scale.cachePitch));
            r += (int)(((pixel & GFX_Rmask) >> (GFX_Rshift + shift)) & (0xffu >> shift));
            g += (int)(((pixel & GFX_Gmask) >> (GFX_Gshift + shift)) & (0xffu >> shift));
            b += (int)(((pixel & GFX_Bmask) >> (GFX_Bshift + shift)) & (0xffu >> shift));
            break;
    }
}

bool gui_menu_exit(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    running = false;
    return true;
}

extern bool toscale;
extern const char* RunningProgram;
static GUI::ScreenSDL *UI_Startup(GUI::ScreenSDL *screen) {
    in_gui = true;

    GFX_EndUpdate(nullptr);
    GFX_SetTitle(-1,-1,-1,true);
    if(!screen) { //Coming from DOSBox. Clean up the keyboard buffer.
        KEYBOARD_ClrBuffer();//Clear buffer
    }
    GFX_LosingFocus();//Release any keys pressed (buffer gets filled again). (could be in above if, but clearing the mapper input when exiting the mapper is sensible as well
    SDL_Delay(20);

    unsigned int cpbak = dos.loaded_codepage;
    if (dos_kernel_disabled&&maincp) dos.loaded_codepage = maincp;
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
    LoadMessageFile(section->Get_string("language"));
    if (font_14_init) GUI_LoadFonts();
    dos.loaded_codepage = cpbak;

    // Comparable to the code of intro.com, but not the same! (the code of intro.com is called from within a com file)
    shell_idle = !dos_kernel_disabled && strcmp(RunningProgram, "LOADLIN") && first_shell && (DOS_PSP(dos.psp()).GetSegment() == DOS_PSP(dos.psp()).GetParent());

    int sx, sy, sw, sh, scalex, scaley, scale;
    bool fs;
    GFX_GetSizeAndPos(sx, sy, sw, sh, fs);

    int dw,dh;
#if defined(C_SDL2)
    {
        dw = 640; dh = 480;

        SDL_Window* GFX_GetSDLWindow(void);
        SDL_Window *w = GFX_GetSDLWindow();
        SDL_GetWindowSize(w,&dw,&dh);
    }
#elif defined(C_HX_DOS)
    /* FIXME: HX DOS builds are not updating the window dimensions vars.. */
    /*        However our window is always fullscreen (maximized) */
    {
        dw = GetSystemMetrics(SM_CXSCREEN);
        dh = GetSystemMetrics(SM_CYSCREEN);
    }
#else
    void UpdateWindowDimensions(void);
    UpdateWindowDimensions();
    dw = (int)currentWindowWidth;
    dh = (int)currentWindowHeight;
#endif

    if (dw < 640) dw = 640;
    if (dh < 350) dh = 350;
    scalex = dw / 640; /* maximum horizontal scale */
    scaley = dh / 350; /* maximum vertical   scale */
    if( scalex > scaley ) scale = scaley;
    else                  scale = scalex;
    if (!toscale) scale = 1;

    assert(sx < dw);
    assert(sy < dh);

    int sw_draw = sw,sh_draw = sh;

    if ((sx+sw_draw) > dw) sw_draw = dw-sx;
    if ((sy+sh_draw) > dh) sh_draw = dh-sy;

    assert((sx+sw_draw) <= dw);
    assert((sy+sh_draw) <= dh);

    assert(sw_draw <= sw);
    assert(sh_draw <= sh);

#if !defined(C_SDL2)
    old_unicode = SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
#endif

    if (sw_draw > 0 && sh_draw > 0) {
        screenshot = SDL_CreateRGBSurface(SDL_SWSURFACE, dw, dh, 32, GUI::Color::RedMask, GUI::Color::GreenMask, GUI::Color::BlueMask, 0);
        SDL_FillRect(screenshot, nullptr, 0);

        unsigned int rs = screenshot->format->Rshift, gs = screenshot->format->Gshift, bs = screenshot->format->Bshift;

        // create screenshot for fade effect
        for (unsigned int y = 0; (int)y < sh_draw; y++) {
            uint32_t *bg = (uint32_t*)((y+(unsigned int)sy)*(unsigned int)screenshot->pitch + (char*)screenshot->pixels) + (unsigned int)sx;
            for (unsigned int x = 0; (int)x < sw_draw; x++) {
                int r = 0, g = 0, b = 0;
                getPixel((int)(x*(unsigned int)render.src.width/(unsigned int)sw),
                        (int)(y*(unsigned int)render.src.height/(unsigned int)sh),
                        r, g, b, 0);
                bg[x] = ((unsigned int)r << (unsigned int)rs) |
                    ((unsigned int)g << (unsigned int)gs) |
                    ((unsigned int)b << (unsigned int)bs);
            }
        }

        background = SDL_CreateRGBSurface(SDL_SWSURFACE, dw, dh, 32, GUI::Color::RedMask, GUI::Color::GreenMask, GUI::Color::BlueMask, 0);
        SDL_FillRect(background, nullptr, 0);
        for (int y = 0; y < sh_draw; y++) {
            uint32_t *bg = (uint32_t*)((unsigned int)(y+sy)*(unsigned int)background->pitch + (char*)background->pixels) + sx;
            for (int x = 0; x < sw_draw; x++) {
                int r = 0, g = 0, b = 0;
                getPixel(x    *(int)render.src.width/sw, y    *(int)render.src.height/sh, r, g, b, 3); 
                getPixel((x-1)*(int)render.src.width/sw, y    *(int)render.src.height/sh, r, g, b, 3); 
                getPixel(x    *(int)render.src.width/sw, (y-1)*(int)render.src.height/sh, r, g, b, 3); 
                getPixel((x-1)*(int)render.src.width/sw, (y-1)*(int)render.src.height/sh, r, g, b, 3); 
                getPixel((x+1)*(int)render.src.width/sw, y    *(int)render.src.height/sh, r, g, b, 3); 
                getPixel(x    *(int)render.src.width/sw, (y+1)*(int)render.src.height/sh, r, g, b, 3); 
                getPixel((x+1)*(int)render.src.width/sw, (y+1)*(int)render.src.height/sh, r, g, b, 3); 
                getPixel((x-1)*(int)render.src.width/sw, (y+1)*(int)render.src.height/sh, r, g, b, 3); 
                int r1, g1, b1;
#if defined(USE_TTF)
                if (ttf.inUse && confres) {
                    std::string theme = section->Get_string("bannercolortheme");
                    if (theme == "black") {
                        r1 = 0; g1 = 0; b1 = 0;
                    } else if (theme == "red") {
                        r1 = 170; g1 = 0; b1 = 0;
                    } else if (theme == "green") {
                        r1 = 0; g1 = 170; b1 = 0;
                    } else if (theme == "yellow") {
                        r1 = 170; g1 = 85; b1 = 0;
                    } else if (theme == "magenta") {
                        r1 = 170; g1 = 0; b1 = 170;
                    } else if (theme == "cyan") {
                        r1 = 0; g1 = 170; b1 = 170;
                    } else if (theme == "white") {
                        r1 = 170; g1 = 170; b1 = 170;
                    } else {
                        r1 = 0; g1 = 0; b1 = 170;
                    }
                } else
#endif
                {
                    r1 = (int)((r * 393 + g * 769 + b * 189) / 1351); // 1351 -- tweak colors
                    g1 = (int)((r * 349 + g * 686 + b * 168) / 1503); // 1203 -- for a nice
                    b1 = (int)((r * 272 + g * 534 + b * 131) / 2340); // 2140 -- golden hue
                }
                bg[x] = ((unsigned int)r1 << (unsigned int)rs) |
                    ((unsigned int)g1 << (unsigned int)gs) |
                    ((unsigned int)b1 << (unsigned int)bs); 
            }
        }
    }

    cursor = SDL_ShowCursor(SDL_QUERY);
    SDL_ShowCursor(SDL_ENABLE);

    mousetoggle = mouselocked;
    if (mouselocked) GFX_CaptureMouse();

#if defined(C_SDL2)
    extern SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height);

    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(false);

    SDL_Window* window = OpenGL_using() ? GFX_SetSDLWindowMode(dw, dh, SCREEN_OPENGL) : GFX_SetSDLSurfaceWindow(dw, dh);
    if (window == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());
    SDL_Surface* sdlscreen = SDL_GetWindowSurface(window);
    if (sdlscreen == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());

    if (screenshot != NULL && background != NULL) {
        // fade out
        // Jonathan C: do it FASTER!
        SDL_Event event;
        SDL_SetSurfaceBlendMode(screenshot, SDL_BLENDMODE_BLEND);
        for (int i = 0xff; i > 0; i -= 0x40) {
            SDL_SetSurfaceAlphaMod(screenshot, i); 
            SDL_BlitSurface(background, NULL, sdlscreen, NULL); 
            SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL);
            SDL_Window* GFX_GetSDLWindow(void);
            SDL_UpdateWindowSurface(GFX_GetSDLWindow());
            while (SDL_PollEvent(&event)); 
            SDL_Delay(40); 
        } 
        SDL_SetSurfaceBlendMode(screenshot, SDL_BLENDMODE_NONE);
    }
#else
    SDL_Surface* sdlscreen = SDL_SetVideoMode(dw, dh, 32, SDL_SWSURFACE|(fs?SDL_FULLSCREEN:0));
    if (sdlscreen == NULL) E_Exit("Could not initialize video mode %ix%ix32 for UI: %s", dw, dh, SDL_GetError());

    if (screenshot != NULL && background != NULL) {
        // fade out
        // Jonathan C: do it FASTER!
        SDL_Event event; 
        for (int i = 0xff; i > 0; i -= 0x40) {
            SDL_SetAlpha(screenshot, SDL_SRCALPHA, i); 
            SDL_BlitSurface(background, NULL, sdlscreen, NULL); 
            SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL); 
            SDL_UpdateRect(sdlscreen, 0, 0, 0, 0); 
            while (SDL_PollEvent(&event)); 
            SDL_Delay(40); 
        }
    }
#endif
 
    if (screenshot != NULL && background != NULL)
        SDL_BlitSurface(background, NULL, sdlscreen, NULL);
#if defined(C_SDL2)
    SDL_Window* GFX_GetSDLWindow(void);
    SDL_UpdateWindowSurface(GFX_GetSDLWindow());
#else   
    SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
#endif

#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    WindowsTaskbarResetPreviewRegion();
#endif

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    if (gui_menu_init) {
        gui_menu_init = false;

        {
            DOSBoxMenu::item &item = guiMenu.alloc_item(DOSBoxMenu::submenu_type_id,"ConfigGuiMenu");
            item.set_text(mainMenu.get_item("mapper_gui").get_text());
        }

        {
            DOSBoxMenu::item &item = guiMenu.alloc_item(DOSBoxMenu::item_type_id,"ExitGUI");
            item.set_callback_function(gui_menu_exit);
            item.set_text(MSG_Get("CONFIG_TOOL_EXIT"));
        }

        guiMenu.displaylist_clear(guiMenu.display_list);

        guiMenu.displaylist_append(
                guiMenu.display_list,
                guiMenu.get_item_id_by_name("ConfigGuiMenu"));

        {
            guiMenu.displaylist_append(
                    guiMenu.get_item("ConfigGuiMenu").display_list, guiMenu.get_item_id_by_name("ExitGUI"));
        }
    } else if (!shortcut || shortcutid<16) {
        {
            DOSBoxMenu::item &item = guiMenu.get_item("ConfigGuiMenu");
            item.set_text(mainMenu.get_item("mapper_gui").get_text());
        }
        {
            DOSBoxMenu::item &item = guiMenu.get_item("ExitGUI");
            item.set_text(MSG_Get("CONFIG_TOOL_EXIT"));
        }
# if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU || DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
        if (loadlang) guiMenu.unbuild();
# endif
    }

    if (null_menu_init) {
        null_menu_init = false;

        {
            DOSBoxMenu::item &item = nullMenu.alloc_item(DOSBoxMenu::submenu_type_id,"ConfigGuiMenu");
            item.set_text("");
        }

        nullMenu.displaylist_clear(nullMenu.display_list);

        nullMenu.displaylist_append(
                nullMenu.display_list,
                nullMenu.get_item_id_by_name("ConfigGuiMenu"));
    }

    if (!shortcut || shortcutid<16) {
        guiMenu.rebuild();
        DOSBox_SetMenu(guiMenu);
    } else {
        nullMenu.rebuild();
        DOSBox_SetMenu(nullMenu);
    }
#endif

    if (screen) screen->setSurface(sdlscreen);
    else screen = new GUI::ScreenSDL(sdlscreen, scale); // CONF:SCALE

    saved_bpp = (int)render.src.bpp;
    render.src.bpp = 0;
    running = true;

#if defined(MACOSX)
    macosx_reload_touchbar();
#endif

    return screen;
}

/* Restore screen */
static void UI_Shutdown(GUI::ScreenSDL *screen) {
    SDL_Surface *sdlscreen = screen->getSurface();
    render.src.bpp = (Bitu)saved_bpp;

#ifdef DOSBOXMENU_EXTERNALLY_MANAGED
    DOSBox_SetMenu(mainMenu);
#endif

#if defined(MACOSX)
    macosx_reload_touchbar();
#endif

#if defined(C_SDL2)
    if (screenshot != NULL && background != NULL) {
        // fade in
        // Jonathan C: do it FASTER!
        SDL_Event event;
        SDL_SetSurfaceBlendMode(screenshot, SDL_BLENDMODE_BLEND);
        for (unsigned int i = 0x00; i < 0xff; i += 0x60) {
            SDL_SetSurfaceAlphaMod(screenshot, i); 
            SDL_BlitSurface(background, NULL, sdlscreen, NULL); 
            SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL);
            SDL_Window* GFX_GetSDLWindow(void);
            SDL_UpdateWindowSurface(GFX_GetSDLWindow());
            while (SDL_PollEvent(&event)); 
            SDL_Delay(40); 
        } 
        SDL_SetSurfaceBlendMode(screenshot, SDL_BLENDMODE_NONE);
    }

    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(true);
#else
    if (screenshot != NULL && background != NULL) {
        // fade in
        // Jonathan C: do it FASTER!
        SDL_Event event;
        for (unsigned int i = 0x00; i < 0xff; i += 0x60) {
            SDL_SetAlpha(screenshot, SDL_SRCALPHA, i);
            SDL_BlitSurface(background, NULL, sdlscreen, NULL);
            SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL);
            SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
            while (SDL_PollEvent(&event)) {}
            SDL_Delay(40); 
        }
    }
#endif

    // clean up
    if (mousetoggle) GFX_CaptureMouse();
    SDL_ShowCursor(cursor);
    if (background != NULL) {
        SDL_FreeSurface(background);
        background = NULL;
    }
    if (screenshot != NULL) {
        SDL_FreeSurface(screenshot);
        screenshot = NULL;
    }
    SDL_FreeSurface(sdlscreen);
    screen->setSurface(NULL);
#ifdef WIN32
    void res_init(void);
    void change_output(int output);
    res_init();
    change_output(8);
#else
#if 1
    GFX_RestoreMode();
#else
    GFX_ResetScreen();
#endif
#endif

#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    DOSBox_SetSysMenu();
    WindowsTaskbarUpdatePreviewRegion();
#endif

#if !defined(C_SDL2)
    SDL_EnableUNICODE(old_unicode);
    SDL_EnableKeyRepeat(0,0);
#endif
    GFX_SetTitle(-1,-1,-1,false);

    void GFX_ForceRedrawScreen(void);
#if defined(USE_TTF)
    if (!TTF_using() || ttf.inUse)
#endif
    {
        GFX_ForceRedrawScreen();
#if defined(USE_TTF)
        if (ttf.inUse) resetFontSize();
#endif
    }

    in_gui = false;
    if (resetcfg) {
        resetcfg = false;
        std::string sec, line;
        while (proplist.size()>1) {
            sec = proplist.front();
            proplist.pop_front();
            line = proplist.front();
            proplist.pop_front();
            ApplySetting(sec, line, true);
        }
        GUI_Run(false);
    }
}

bool GUI_IsRunning(void) {
    return in_gui;
}

static void UI_RunCommands(GUI::ScreenSDL *s, const std::string &cmds) {
    DOS_Shell temp;
    temp.call = true;
    UI_Shutdown(s);
    uint16_t n=1; uint8_t c='\n';
    DOS_WriteFile(STDOUT,&c,&n);
    temp.bf = new VirtualBatch(&temp, cmds);
    temp.RunInternal();
    temp.ShowPrompt();
    UI_Startup(s);
}

VirtualBatch::VirtualBatch(DOS_Shell *host, const std::string& cmds) : BatchFile(host, "CON", "", ""), lines(cmds) {
}

bool VirtualBatch::ReadLine(char *line) {
    std::string l;

    if (!std::getline(lines,l)) {
        delete this;
        return false;
    }

    strcpy(line,l.c_str());
    return true;
}

/* stringification and conversion from the c++ FAQ */
class BadConversion : public std::runtime_error {
public: BadConversion(const std::string& s) : std::runtime_error(s) { }
};

template<typename T> inline std::string stringify(const T& x, std::ios_base& ( *pf )(std::ios_base&) = NULL) {
    std::ostringstream o;
    if (pf) o << pf;
    if (!(o << x)) throw BadConversion(std::string("stringify(") + typeid(x).name() + ")");
    return o.str();
}

template<typename T> inline void convert(const std::string& s, T& x, bool failIfLeftoverChars = true, std::ios_base& ( *pf )(std::ios_base&) = NULL) {
    std::istringstream i(s);
    if (pf) i >> pf;
    char c;
    if (!(i >> x) || (failIfLeftoverChars && i.get(c))) throw BadConversion(s);
}

/*****************************************************************************************************************************************/
/* UI classes */

class PropertyEditor : public GUI::Window, public GUI::ActionEventSource_Callback {
protected:
    Section_prop * section;
    Property *prop;

    static constexpr auto RightMarginWindow    = 10; // TODO find origin, fix
    static constexpr auto RightMarginBool      = 55; // TODO find origin, fix
    static constexpr auto RightMarginText      = 40; // TODO find origin, fix

    int GetHostWindowWidth() const
    {
        const int width = parent->getParent()->getWidth();

        return width;
    }

    void SetupUI(const bool opts, GUI::Input*& input, GUI::Button*& infoButton)
    {
        const auto windowWidth = GetHostWindowWidth();

        constexpr auto optionsWidth = 42;

        const auto defaultSpacing = static_cast<int>(GUI::CurrentTheme.DefaultSpacing);

        const auto inputWidth = 235 - (opts ? optionsWidth + defaultSpacing : 0);

        const auto optionsPos = windowWidth - optionsWidth - RightMarginText - defaultSpacing;

        const auto inputPos = opts
                                  ? optionsPos - defaultSpacing - inputWidth
                                  : windowWidth - RightMarginText - defaultSpacing - inputWidth;

        input = new GUI::Input(this, inputPos, 0, inputWidth, static_cast<int>(GUI::CurrentTheme.ButtonHeight));

        if(opts)
        {
            infoButton = new GUI::Button(this, optionsPos, 0, "...", optionsWidth);
            infoButton->addActionHandler(this);
        }
    }

public:
    PropertyEditor(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        Window(parent, x, y, parent->getParent()->getWidth() - RightMarginWindow, 25), section(section), prop(prop) { (void)opts; }

    virtual bool prepare(std::string &buffer) = 0;

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        // HACK: Attempting to cast a String to void causes "forming reference to void" errors when building with GCC 4.7
        (void)arg.size();//UNUSED
        std::string line;
        if (prepare(line)) {
            prop->SetValue(GUI::String(line));
            if (!(!strcmp(section->GetName(),"dosbox")&&prop->propname=="language")) {
                proplist.push_back(section->GetName());
                proplist.push_back(prop->propname+"="+line);
                //ApplySetting(section->GetName(), prop->propname+"="+line, true);
            }
        }
    }

    void paintVisGuideLineBetween(GUI::Drawable &d,const GUI::Window *lm,const GUI::Window *rm,const GUI::Window *pm) const {
        int sx = lm->getX() + lm->getWidth();
        int ex = rm->getX();
        int y = pm->getHeight() / 2;

        sx += 4;
        ex -= 4;

        d.setColor(GUI::CurrentTheme.Shadow3D);
        d.drawDotLine(sx,y,ex,y);
    }
};

class PropertyEditorBool : public PropertyEditor {
    GUI::Checkbox *input;
    GUI::Label *label;
public:
    PropertyEditorBool(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        PropertyEditor(parent, x, y, section, prop, opts) {
        label = new GUI::Label(this, 0, 5, prop->propname);
        constexpr auto inputWidth = 3;
        input = new GUI::Checkbox(this, GetHostWindowWidth() - inputWidth - RightMarginBool, inputWidth, "");
        input->setChecked(static_cast<bool>(prop->GetValue()));
    }

    bool prepare(std::string &buffer) override {
        if (input->isChecked() == static_cast<bool>(prop->GetValue())) return false;
        buffer.append(input->isChecked()?"true":"false");
        return true;
    }

    /// Paint label
	void paint(GUI::Drawable &d) const override {
        paintVisGuideLineBetween(d,label,input,this);
    }
};

class ShowOptions : public GUI::MessageBox2 {
protected:
    GUI::Input *name, *inp;
    GUI::Radiobox *opt[200];
    std::vector<Value> pv;
    std::vector<GUI::Char> cfg_sname;
    GUI::WindowInWindow * wiw = NULL;
public:
    ShowOptions(GUI::Screen *parent, int x, int y, const char *title, const char *msg, Property *prop, GUI::Input *input) :
        MessageBox2(parent, x, y, 310, title, msg) { // 740
            inp = input;
            pv = prop->GetValues();
            Bitu k, j = 0;
            int allowed_dialog_y = parent->getHeight() - 25 - (border_top + border_bottom) - 120;
            int scroll_h = pv.size() * 20;
            if (scroll_h > allowed_dialog_y) scroll_h = allowed_dialog_y;
            scroll_h += 6;
            wiw = new GUI::WindowInWindow(this, 5, 80, 290, scroll_h);
            bool found = false;
            for(k = 0; k < pv.size(); k++) if (pv[k].ToString().size()) {
                opt[k] = new GUI::Radiobox(wiw, 5, j*20+5, pv[k].ToString().c_str());
                if (GUI::String(pv[k].ToString())==inp->getText()) {
                    found = true;
                    opt[k]->setChecked(true);
                } else
                    opt[k]->setChecked(false);
                opt[k]->addActionHandler(this);
                j++;
            }
            if (!found)
                for(k = 0; k < pv.size(); k++)
                    if (pv[k].ToString().size() && pv[k].ToString()==prop->GetValue().ToString())
                        opt[k]->setChecked(true);
            scroll_h -= (k-j) * 20;
            wiw->resize(290, scroll_h);
            if (wiw->scroll_pos_h != 0) {
                wiw->enableScrollBars(false/*h*/,true/*v*/);
                wiw->enableBorder(true);
            } else {
                wiw->enableScrollBars(false/*h*/,false/*v*/);
                wiw->enableBorder(false);
            }
            (new GUI::Button(this, 70, scroll_h+90, MSG_Get("OK"), 70))->addActionHandler(this);
            close->move(155,scroll_h+90);
            resize(310, scroll_h+156);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            /* first child is first tabbable */
            if (pv.size() > 0) {
                Window *w = opt[0];
                if (w) w->first_tabbable = true;
            }

            /* last child is first tabbable */
            if (pv.size() > 0) {
                Window *w = opt[pv.size()-1];
                if (w) w->last_tabbable = true;
            }

            /* the FIRST field needs to come first when tabbed to */
            if (pv.size() > 0) {
                Window *w = opt[0];
                if (w) w->raise();
            }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        unsigned int j, k;
        for(k = 0; k < pv.size(); k++) if (pv[k].ToString().size()) {
            if (arg == pv[k].ToString() && opt[k]->isChecked())
                for(j = 0; j < pv.size(); j++)
                    if (pv[j].ToString().size() && j!=k) opt[j]->setChecked(false);
        }
        if (arg == MSG_Get("OK")) {
            for(k = 0; k < pv.size(); k++)
                if (pv[k].ToString().size() && opt[k]->isChecked()) {
                    inp->setText(pv[k].ToString());
                    break;
                }
            ToplevelWindow::actionExecuted(b, GUI::String(MSG_Get("CLOSE")));
        }
        else if (arg == MSG_Get("CLOSE")) {
            ToplevelWindow::actionExecuted(b, arg);
        }
    }

    ~ShowOptions() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

class PropertyEditorString : public PropertyEditor {
protected:
    GUI::Input *input;
    GUI::Label *label;
    GUI::Button *infoButton = NULL;
public:
    PropertyEditorString(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        PropertyEditor(parent, x, y, section, prop, opts) {
        std::string title(section->GetName());
        if (title=="4dos"&&!strcmp(prop->propname.c_str(), "rem"))
            input = new GUI::Input(this, 30, 0, 470);
        else {
            SetupUI(opts, input, infoButton);
        }
        std::string temps = prop->GetValue().ToString();
        input->setText(stringify(temps));
        label = new GUI::Label(this, 0, 5, prop->propname);
	scan_tabbing = true;

        /* first child is first tabbable */
        {
            Window *w = this->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = this->getChild(this->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = this->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == "...")
            new ShowOptions(getScreen(), 300, 300, MSG_Get("SELECT_VALUE"), ("Property: \033[31m" + prop->propname + "\033[0m\n\n"+(prop->Get_Default_Value().ToString().size()?"Default value: \033[32m"+prop->Get_Default_Value().ToString()+"\033[0m\n\n":"")+"Possible values to select:\n").c_str(), prop, input);
        else
            PropertyEditor::actionExecuted(b, arg);
    }

    bool prepare(std::string &buffer) override {
        std::string temps = prop->GetValue().ToString();
        if (input->getText() == GUI::String(temps)) return false;
        buffer.append(static_cast<const std::string&>(input->getText()));
        return true;
    }

    /// Paint label
	void paint(GUI::Drawable &d) const override {
        paintVisGuideLineBetween(d,label,input,this);
    }
};

class PropertyEditorFloat : public PropertyEditor {
protected:
    GUI::Input *input;
    GUI::Label *label;
    GUI::Button *infoButton = NULL;
public:
    PropertyEditorFloat(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        PropertyEditor(parent, x, y, section, prop, opts) {
        SetupUI(opts, input, infoButton);
        input->setText(stringify((double)prop->GetValue()));
        label = new GUI::Label(this, 0, 5, prop->propname);
	scan_tabbing = true;

        /* first child is first tabbable */
        {
            Window *w = this->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = this->getChild(this->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = this->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == "...")
            new ShowOptions(getScreen(), 300, 300, ("Values for " + prop->propname).c_str(), ("Property: \033[31m" + prop->propname + "\033[0m\n\n"+(prop->Get_Default_Value().ToString().size()?"Default value: \033[32m"+prop->Get_Default_Value().ToString()+"\033[0m\n\n":"")+"Possible values to select:\n").c_str(), prop, input);
        else
            PropertyEditor::actionExecuted(b, arg);
    }

    bool prepare(std::string &buffer) override {
        double val;
        convert(input->getText(), val, false);
        if (val == (double)prop->GetValue()) return false;
        buffer.append(stringify(val));
        return true;
    }

    /// Paint label
	void paint(GUI::Drawable &d) const override {
        paintVisGuideLineBetween(d,label,input,this);
    }
};

class PropertyEditorHex : public PropertyEditor {
protected:
    GUI::Input *input;
    GUI::Label *label;
    GUI::Button *infoButton = NULL;
public:
    PropertyEditorHex(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        PropertyEditor(parent, x, y, section, prop, opts) {
        SetupUI(opts, input, infoButton);
        std::string temps = prop->GetValue().ToString();
        input->setText(temps.c_str());
        label = new GUI::Label(this, 0, 5, prop->propname);
	scan_tabbing = true;

        /* first child is first tabbable */
        {
            Window *w = this->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = this->getChild(this->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = this->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == "...")
            new ShowOptions(getScreen(), 300, 300, ("Values for " + prop->propname).c_str(), ("Property: \033[31m" + prop->propname + "\033[0m\n\n"+(prop->Get_Default_Value().ToString().size()?"Default value: \033[32m"+prop->Get_Default_Value().ToString()+"\033[0m\n\n":"")+"Possible values to select:\n").c_str(), prop, input);
        else
            PropertyEditor::actionExecuted(b, arg);
    }

    bool prepare(std::string &buffer) override {
        int val;
        convert(input->getText(), val, false, std::hex);
        if ((Hex)val ==  prop->GetValue()) return false;
        buffer.append(stringify(val, std::hex));
        return true;
    }

    /// Paint label
	virtual void paint(GUI::Drawable &d) const override {
        paintVisGuideLineBetween(d,label,input,this);
    }
};

class PropertyEditorInt : public PropertyEditor {
protected:
    GUI::Input *input;
    GUI::Label *label;
    GUI::Button *infoButton = NULL;
public:
    PropertyEditorInt(Window *parent, int x, int y, Section_prop *section, Property *prop, bool opts) :
        PropertyEditor(parent, x, y, section, prop, opts) {
        SetupUI(opts, input, infoButton);
        //Maybe use ToString() of Value
        input->setText(stringify(static_cast<int>(prop->GetValue())));
        label = new GUI::Label(this, 0, 5, prop->propname);
	scan_tabbing = true;

        /* first child is first tabbable */
        {
            Window *w = this->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = this->getChild(this->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = this->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }
    };

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == "...")
            new ShowOptions(getScreen(), 300, 300, ("Values for " + prop->propname).c_str(), ("Property: \033[31m" + prop->propname + "\033[0m\n\n"+(prop->Get_Default_Value().ToString().size()?"Default value: \033[32m"+prop->Get_Default_Value().ToString()+"\033[0m\n\n":"")+"Possible values to select:\n").c_str(), prop, input);
        else
            PropertyEditor::actionExecuted(b, arg);
    }

    bool prepare(std::string &buffer) override {
        int val;
        convert(input->getText(), val, false);
        if (val == static_cast<int>(prop->GetValue())) return false;
        buffer.append(stringify(val));
        return true;
    };

    /// Paint label
	void paint(GUI::Drawable &d) const override {
        paintVisGuideLineBetween(d,label,input,this);
    }
};

std::string dispname="";
std::string CapName(std::string name) {
    dispname = name;
    if (name=="sdl"||name=="cpu"||name=="midi"||name=="gus"||name=="dos"||name=="ipx"||name=="ne2000")
        std::transform(dispname.begin(), dispname.end(), dispname.begin(), ::toupper);
    else if (name=="dosbox")
        dispname="Main";
    else if (name=="pc98")
        dispname="PC-98";
    else if (name=="dosv")
        dispname="DOS/V";
    else if (name=="ttf")
        dispname="TrueType";
    else if (name=="vsync")
        dispname="VSync";
    else if (name=="4dos")
        dispname="4DOS.INI";
    else if (name=="config")
        dispname="CONFIG.SYS";
    else if (name=="autoexec")
        dispname="AUTOEXEC.BAT";
    else if (name=="sblaster")
        dispname="SB";
    else if (name=="sblaster2")
        dispname="SB #2";
    else if (name=="speaker")
        dispname="PC Speaker";
    else if (name=="imfc")
        dispname="IMFC";
    else if (name=="innova")
        dispname="SSI-2001";
    else if (name=="serial")
        dispname="Serial";
    else if (name=="parallel")
        dispname="Parallel";
    else if (name=="fdc, primary")
        dispname="Floppy";
    else if (name=="ide, primary")
        dispname="IDE #1";
    else if (name=="ide, secondary")
        dispname="IDE #2";
    else if (name=="ide, tertiary")
        dispname="IDE #3";
    else if (name=="ide, quaternary")
        dispname="IDE #4";
    else if (name=="ide, quinternary")
        dispname="IDE #5";
    else if (name=="ide, sexternary")
        dispname="IDE #6";
    else if (name=="ide, septernary")
        dispname="IDE #7";
    else if (name=="ide, octernary")
        dispname="IDE #8";
    else if (name=="ethernet, pcap")
        dispname="pcap";
    else if (name=="ethernet, slirp")
        dispname="SLiRP";
    else
        dispname[0] = std::toupper(name[0]);
    return dispname;
}

std::string RestoreName(std::string name) {
    dispname = name;
    if (name=="Main")
        dispname="dosbox";
    else if (name=="PC-98")
        dispname="pc98";
    else if (name=="DOS/V")
        dispname="dosv";
    else if (name=="TrueType")
        dispname="ttf";
    else if (name=="VSync")
        dispname="vsync";
    else if (name=="4DOS.INI")
        dispname="4dos";
    else if (name=="CONFIG.SYS")
        dispname="config";
    else if (name=="AUTOEXEC.BAT")
        dispname="autoexec";
    else if (name=="SB")
        dispname="sblaster";
    else if (name=="SB #2")
        dispname="sblaster2";
    else if (name=="PC Speaker")
        dispname="speaker";
    else if (name=="IMFC")
        dispname="imfc";
    else if (name=="SSI-2001")
        dispname="innova";
    else if (name=="Serial")
        dispname="serial";
    else if (name=="Parallel")
        dispname="parallel";
    else if (name=="Floppy")
        dispname="fdc, primary";
    else if (name=="IDE #1")
        dispname="ide, primary";
    else if (name=="IDE #2")
        dispname="ide, secondary";
    else if (name=="IDE #3")
        dispname="ide, tertiary";
    else if (name=="IDE #4")
        dispname="ide, quaternary";
    else if (name=="IDE #5")
        dispname="ide, quinternary";
    else if (name=="IDE #6")
        dispname="ide, sexternary";
    else if (name=="IDE #7")
        dispname="ide, septernary";
    else if (name=="IDE #8")
        dispname="ide, octernary";
    else if (name=="pcap")
        dispname="ethernet, pcap";
    else if (name=="SLiRP")
        dispname="ethernet, slirp";
    return dispname;
}

#if C_DEBUG
bool ParseCommand(char* str);
int logwin = true;
GUI::MessageBox3 *npwin = NULL;
class EnterDebuggerCommand : public GUI::ToplevelWindow {
protected:
    GUI::Input *cmd;
    GUI::Button *okButton = NULL, *cancelButton = NULL;
    std::string str = "";
public:
    EnterDebuggerCommand(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 110 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, "Enter debugger command:");
        cmd = new GUI::Input(this, 5, 30, width - 10 - border_left - border_right);
        cmd->setText("");
        okButton=new GUI::Button(this, 100, 65, MSG_Get("OK"), 90);
        okButton->addActionHandler(this);
        cancelButton=new GUI::Button(this, 200, 65, MSG_Get("CANCEL"), 90);
        cancelButton->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
        cmd->raise();
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            ParseCommand(cmd->getText());
            if (npwin) {
                if (logwin) getlogtext(str);
                else getcodetext(str);
                npwin->setText(str);
                npwin->wiw->scroll_pos_y = npwin->wiw->scroll_pos_h;
            }
        }
        if (arg == MSG_Get("OK") || arg == MSG_Get("CANCEL"))
            close();
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Enter) {
            okButton->executeAction();
            return true;
        }

        if (key.special == GUI::Key::Escape) {
            cancelButton->executeAction();
            return true;
        }

        return false;
    }
};

class LogWindow : public GUI::MessageBox3 {
public:
    std::vector<GUI::Char> cfg_sname;
    std::string str = "";
public:
    LogWindow(GUI::Screen *parent, int x, int y) :
        MessageBox3(parent, x, y, 630, "", "") { // 740
        setTitle(MSG_Get("LOGGING_OUTPUT"));
        getlogtext(str);
        setText(str.size()?str:"No logging output available.");
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    };

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("DEBUGCMD")) {
            logwin = true;
            auto *np = new EnterDebuggerCommand(static_cast<GUI::Screen*>(parent), 90, 100, "Debugger command");
            np->raise();
        } else if (arg == MSG_Get("CLOSE")) {
            if(shortcut) running=false;
            npwin = NULL;
        }
    }

    ~LogWindow() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

class CodeWindow : public GUI::MessageBox3 {
public:
    std::vector<GUI::Char> cfg_sname;
    std::string str = "";
public:
    CodeWindow(GUI::Screen *parent, int x, int y) :
        MessageBox3(parent, x, y, 630, "", "") { // 740
        setTitle(MSG_Get("CODE_OVERVIEW"));
        getcodetext(str);
        setText(str);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    };

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("DEBUGCMD")) {
            logwin = false;
            auto *np = new EnterDebuggerCommand(static_cast<GUI::Screen*>(parent), 90, 100, "Debugger command");
            np->raise();
        } else if (arg == MSG_Get("CLOSE")) {
            if(shortcut) running=false;
            npwin = NULL;
        }
    }

    ~CodeWindow() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

#endif

class HelpWindow : public GUI::MessageBox2 {
public:
    std::vector<GUI::Char> cfg_sname;
public:
    HelpWindow(GUI::Screen *parent, int x, int y, Section *section) :
        MessageBox2(parent, x, y, 580, "", "") { // 740
        if (section == NULL) {
            LOG_MSG("BUG: HelpWindow constructor called with section == NULL\n");
            return;
        }

        std::string title(section->GetName());
        sprintf(tmp1, MSG_Get("HELP_FOR"), CapName(title).c_str());
        setTitle(tmp1);
        title[0] = std::toupper(title[0]);

        Section_prop* sec = dynamic_cast<Section_prop*>(title.substr(0, 4)=="Ide,"?control->GetSection("ide, primary"):section);
        if (sec) {
            std::string msg;

            Property *property;
            std::vector<Property*> properties;
            auto propertyIndex = 0;
            while ((property = sec->Get_prop(propertyIndex++)))
            {
                properties.push_back(property);
            }

            std::sort(properties.begin(), properties.end(),[](const Property* a, const Property* b)
            {
                return a->propname < b->propname;
            });

            for(const auto& p : properties)
            {
                std::string help=title=="4dos"&&p->propname=="rem"?"This is the 4DOS.INI file (if you use 4DOS as the command shell).":p->Get_help();
                if (title!="4dos" && title!="Config" && title!="Autoexec" && !advopt->isChecked() && !p->basic()) continue;
                std::string propvalues = "";
                std::vector<Value> pv = p->GetValues();
                if (p->Get_type()==Value::V_BOOL) {
                    // possible values for boolean are true, false
                    propvalues += "true, false";
                } else if (p->Get_type()==Value::V_INT) {
                    // print min, max for integer values if used
                    Prop_int* pint = dynamic_cast <Prop_int*>(p);
                    if (pint==NULL) E_Exit("Int property dynamic cast failed.");
                    if (pint->getMin() != pint->getMax()) {
                        std::ostringstream oss;
                        oss << pint->getMin();
                        oss << "..";
                        oss << pint->getMax();
                        propvalues += oss.str();
                    }
                }
                for(Bitu k = 0; k < pv.size(); k++) {
                    if (pv[k].ToString() =="%u")
                        propvalues += MSG_Get("PROGRAM_CONFIG_HLP_POSINT");
                    else propvalues += pv[k].ToString();
                    if ((k+1) < pv.size() && (title != "Config" || p->propname != "numlock" || pv[k+1].ToString() != "")) propvalues += ", ";
                }
                msg += std::string("\033[31m")+p->propname+":\033[0m\n"+help+(propvalues==""?"":"\nPossible values: \033[32m"+propvalues+"\033[0m")+(p->Get_Default_Value().ToString().size()?"\nDefault value: \033[32m"+p->Get_Default_Value().ToString():"")+"\033[0m"+(p->getChange()==Property::Changeable::OnlyAtStart?"\n(Not changeable at runtime)":"")+"\n\n";
            }
            if (!msg.empty()) msg.replace(msg.end()-1,msg.end(),"");
            setText(msg);
        } else {
            std::string name = section->GetName();
            std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::toupper);
            name += "_CONFIGFILE_HELP";
            setText(MSG_Get(name.c_str()));
        }
        wiw->raise(); /* focus on the message, not the close button, so the user can immediately arrow up/down */
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    };

    ~HelpWindow() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

class SectionEditor : public GUI::ToplevelWindow {
    Section_prop * section;
    GUI::Button * closeButton = NULL;
    GUI::WindowInWindow * wiw = NULL;
public:
    std::vector<GUI::Char> cfg_sname;
public:
    SectionEditor(GUI::Screen *parent, int x, int y, Section_prop *section) :
        ToplevelWindow(parent, x, y, 0, 0, ""), section(section) {
        if (section == NULL) {
            LOG_MSG("BUG: SectionEditor constructor called with section == NULL\n");
            return;
        }
        proplist = {};

        int first_row_y = 5;
        int row_height = 25;
        int column_width = 600;
        int button_row_h = (int)GUI::CurrentTheme.ButtonHeight;
        int button_row_padding_y = static_cast<int>(GUI::CurrentTheme.DefaultSpacing);

        int num_prop = 0, k=0;
        while (section->Get_prop(num_prop) != NULL) {
            if (advopt->isChecked() || section->Get_prop(num_prop)->basic()) k++;
            num_prop++;
        }

        int allowed_dialog_y = parent->getHeight() - 25 - (border_top + border_bottom) - 50;

        int items_per_col = k;
        int columns = 1;

        int scroll_h = items_per_col * row_height;
        if (scroll_h > allowed_dialog_y)
            scroll_h = allowed_dialog_y;

        scroll_h += 2; /* border */

        wiw = new GUI::WindowInWindow(this, 5, 5, width-border_left-border_right-10, scroll_h);

        int button_row_y = first_row_y + scroll_h + 5;
        int button_w = 90;
        int button_pad_w = 10;
        int button_row_w = ((button_pad_w + button_w) * 3) - button_pad_w;
        int button_row_cx = (((columns * column_width) - button_row_w) / 2) + 5;

        resize((columns * column_width) + border_left + border_right + 2/*wiw border*/ + wiw->vscroll_display_width/*scrollbar*/ + 10,
               button_row_y + button_row_h + button_row_padding_y + border_top + border_bottom);

        if ((this->y + this->getHeight()) > parent->getHeight())
            move(this->x,parent->getHeight() - this->getHeight());

        setTitle(CapName(std::string(section->GetName())).c_str());

        GUI::Button *b = new GUI::Button(this, button_row_cx, button_row_y, mainMenu.get_item("HelpMenu").get_text().c_str(), button_w);
        b->addActionHandler(this);

        b = new GUI::Button(this, button_row_cx + (button_w + button_pad_w), button_row_y, MSG_Get("OK"), button_w);

        int i = 0, j = 0;
        Property *property;
        std::vector<Property*> properties;
        auto propertyIndex = 0;
        
        while ((property = section->Get_prop(propertyIndex++)))
        {
            properties.push_back(property);
        }

        std::sort(properties.begin(), properties.end(),[](const Property* a, const Property* b)
        {
            return a->propname < b->propname;
        });
        
        for(const auto & prop : properties)
        {
            if (!advopt->isChecked() && !prop->basic()) {i++;continue;}
            Prop_bool   *pbool   = dynamic_cast<Prop_bool*>(prop);
            Prop_int    *pint    = dynamic_cast<Prop_int*>(prop);
            Prop_double  *pdouble  = dynamic_cast<Prop_double*>(prop);
            Prop_hex    *phex    = dynamic_cast<Prop_hex*>(prop);
            Prop_string *pstring = dynamic_cast<Prop_string*>(prop);
            Prop_multival* pmulti = dynamic_cast<Prop_multival*>(prop);
            Prop_multival_remain* pmulti_remain = dynamic_cast<Prop_multival_remain*>(prop);
            bool opts = !prop->suggested_values.empty()&&prop->GetValues().size()>1;

            PropertyEditor *p;
            if (pbool) p = new PropertyEditorBool(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (phex) p = new PropertyEditorHex(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (pint) p = new PropertyEditorInt(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (pdouble) p = new PropertyEditorFloat(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (pstring) p = new PropertyEditorString(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (pmulti) p = new PropertyEditorString(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else if (pmulti_remain) p = new PropertyEditorString(wiw, column_width*(j/items_per_col), (j%items_per_col)*row_height, section, prop, opts);
            else { i++; continue; }
            b->addActionHandler(p);
            i++;
            j++;
        }
        b->addActionHandler(this);

        b = new GUI::Button(this, button_row_cx + (button_w + button_pad_w)*2, button_row_y, MSG_Get("CANCEL"), button_w);
        b->addActionHandler(this);
	scan_tabbing = true;
        closeButton = b;

        /* first child is first tabbable */
        {
            Window *w = wiw->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = wiw->getChild(wiw->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = wiw->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }

        wiw->resize((columns * column_width) + 2/*border*/ + wiw->vscroll_display_width, scroll_h);

        if (wiw->scroll_pos_h != 0) {
            wiw->enableScrollBars(false/*h*/,true/*v*/);
            wiw->enableBorder(true);
        }
        else {
            wiw->enableScrollBars(false/*h*/,false/*v*/);
            wiw->enableBorder(false);

            resize((columns * column_width) + border_left + border_right + 2/*wiw border*/ + /*wiw->vscroll_display_width*//*scrollbar*/ + 10,
                    button_row_y + button_row_h + button_row_padding_y + border_top + border_bottom);
        }
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    ~SectionEditor() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        strcpy(tmp1, mainMenu.get_item("HelpMenu").get_text().c_str());
        if (arg == MSG_Get("OK") && proplist.size()) { close(); running=false; if(!shortcut) resetcfg=true; }
        else if (arg == MSG_Get("OK") || arg == MSG_Get("CANCEL") || arg == MSG_Get("CLOSE")) { close(); if(shortcut) running=false; }
        else if (arg == tmp1) {
            std::vector<GUI::Char> new_cfg_sname;

            if (!cfg_sname.empty()) {
//              new_cfg_sname = "help_";
                new_cfg_sname.resize(5);
                new_cfg_sname[0] = 'h';
                new_cfg_sname[1] = 'e';
                new_cfg_sname[2] = 'l';
                new_cfg_sname[3] = 'p';
                new_cfg_sname[4] = '_';
                new_cfg_sname.insert(new_cfg_sname.end(),cfg_sname.begin(),cfg_sname.end());
            }

            auto lookup = cfg_windows_active.find(new_cfg_sname);
            if (lookup == cfg_windows_active.end()) {
                int nx = getX() - 10;
                int ny = getY() - 10;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                auto *np = new HelpWindow(static_cast<GUI::Screen*>(parent), nx, ny, section);
                cfg_windows_active[new_cfg_sname] = np;
                np->cfg_sname = new_cfg_sname;
                np->raise();
            }
            else {
                lookup->second->raise();
            }
        }
        else
            ToplevelWindow::actionExecuted(b, arg);
    }

    bool keyDown(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyDown(key)) return true;
        return false;
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }

};

class ConfigEditor : public GUI::ToplevelWindow {
    GUI::Button *closeButton = NULL;
    Section_prop * section;
    GUI::Input *content = NULL;
    GUI::WindowInWindow * wiw = NULL;
public:
    std::vector<GUI::Char> cfg_sname;
public:
    ConfigEditor(GUI::Screen *parent, int x, int y, Section_prop *section) :
        ToplevelWindow(parent, x, y, 510, 260 + GUI::titlebar_y_stop, ""), section(section) {
        if (section == NULL) {
            LOG_MSG("BUG: ConfigEditor constructor called with section == NULL\n");
            return;
        }

        int first_row_y = 5;
        int row_height = 25;
        int column_width = 500;
        int button_row_h = 26;
        int button_row_padding_y = 5 + 5;

        int num_prop = 0;
        while (section->Get_prop(num_prop) != NULL) num_prop++;

        int allowed_dialog_y = parent->getHeight() - 25 - (border_top + border_bottom) - 50;

        int items_per_col = num_prop;
        int columns = 1;

        int scroll_h = items_per_col * row_height;
        if (scroll_h > allowed_dialog_y)
            scroll_h = allowed_dialog_y;

        setTitle(CapName(std::string(section->GetName())).c_str());

		char extra_data[4096] = { 0 };
		const char * extra = const_cast<char*>(section->data.c_str());
		if (extra) {
			std::istringstream in(extra);
			char linestr[CROSS_LEN+1], cmdstr[CROSS_LEN], valstr[CROSS_LEN];
			char *cmd=cmdstr, *val=valstr, *p;
			if (in)	for (std::string line; std::getline(in, line); ) {
				if (line.length()>CROSS_LEN) {
					strncpy(linestr, line.c_str(), CROSS_LEN);
					linestr[CROSS_LEN]=0;
				} else
					strcpy(linestr, line.c_str());
				p=strchr(linestr, '=');
				if (p!=NULL) {
					*p=0;
					strcpy(cmd, linestr);
					strcpy(val, p+1);
					cmd=trim(cmd);
					val=trim(val);
					if (strcasecmp(cmd, "rem")&&strlen(extra_data)+strlen(cmd)+strlen(val)+3<4096&&(title=="4dos"||!strncasecmp(cmd, "set ", 4)||!strcasecmp(cmd, "install")||!strcasecmp(cmd, "installhigh")||!strcasecmp(cmd, "device")||!strcasecmp(cmd, "devicehigh"))) {
						strcat(extra_data, cmd);
						strcat(extra_data, "=");
						strcat(extra_data, val);
						strcat(extra_data, "\r\n");
					}
				} else if (title=="Config"&&!strncasecmp(line.c_str(), "rem ", 4)) {
					strcat(extra_data, line.c_str());
					strcat(extra_data, "\r\n");
				}
			}
		}

        int height=title=="Config"?100:81;
        scroll_h += height + 2; /* border */

        wiw = new GUI::WindowInWindow(this, 5, 5, width-border_left-border_right-10, scroll_h);

        int button_row_y = first_row_y + scroll_h + 5;
        int button_w = 90;
        int button_pad_w = 10;
        int button_row_w = ((button_pad_w + button_w) * 4 + button_w) - button_pad_w;
        int button_row_cx = (((columns * column_width) - button_row_w) / 2) + 5;

        resize((columns * column_width) + border_left + border_right + 2/*wiw border*/ + wiw->vscroll_display_width/*scrollbar*/ + 10,
               button_row_y + button_row_h + button_row_padding_y + border_top + border_bottom);

        if ((this->y + this->getHeight()) > parent->getHeight())
            move(this->x,parent->getHeight() - this->getHeight());

        new GUI::Label(this, 5, button_row_y-height, MSG_Get(title=="Config"?"ADDITION_CONTENT":"CONTENT"));
        content = new GUI::Input(this, 5, button_row_y-height+20, 510 - border_left - border_right, height-25);
        content->setText(extra_data);

        GUI::Button *b = new GUI::Button(this, button_row_cx, button_row_y, MSG_Get("PASTE_CLIPBOARD"), button_w*2);
        b->addActionHandler(this);

        b = new GUI::Button(this, button_row_cx + button_w + (button_w + button_pad_w), button_row_y, mainMenu.get_item("HelpMenu").get_text().c_str(), button_w);
        b->addActionHandler(this);

        b = new GUI::Button(this, button_row_cx + button_w + (button_w + button_pad_w)*2, button_row_y, MSG_Get("OK"), button_w);

        int i = 0;
        Property *prop;
        while ((prop = section->Get_prop(i))) {
            Prop_bool   *pbool   = dynamic_cast<Prop_bool*>(prop);
            Prop_int    *pint    = dynamic_cast<Prop_int*>(prop);
            Prop_double  *pdouble  = dynamic_cast<Prop_double*>(prop);
            Prop_hex    *phex    = dynamic_cast<Prop_hex*>(prop);
            Prop_string *pstring = dynamic_cast<Prop_string*>(prop);
            Prop_multival* pmulti = dynamic_cast<Prop_multival*>(prop);
            Prop_multival_remain* pmulti_remain = dynamic_cast<Prop_multival_remain*>(prop);
            bool opts = !prop->suggested_values.empty()&&prop->GetValues().size()>1;

            PropertyEditor *p;
            if (pbool) p = new PropertyEditorBool(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (phex) p = new PropertyEditorHex(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (pint) p = new PropertyEditorInt(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (pdouble) p = new PropertyEditorFloat(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (pstring) p = new PropertyEditorString(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (pmulti) p = new PropertyEditorString(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else if (pmulti_remain) p = new PropertyEditorString(wiw, column_width*(i/items_per_col), (i%items_per_col)*row_height, section, prop, opts);
            else { i++; continue; }
            b->addActionHandler(p);
            i++;
        }
        b->addActionHandler(this);

        b = new GUI::Button(this, button_row_cx + button_w + (button_w + button_pad_w)*3, button_row_y, MSG_Get("CANCEL"), button_w);
        b->addActionHandler(this);
	scan_tabbing = true;
        closeButton = b;

        /* first child is first tabbable */
        {
            Window *w = wiw->getChild(0);
            if (w) w->first_tabbable = true;
        }

        /* last child is first tabbable */
        {
            Window *w = wiw->getChild(wiw->getChildCount()-1);
            if (w) w->last_tabbable = true;
        }

        /* the FIRST field needs to come first when tabbed to */
        {
            Window *w = wiw->getChild(0);
            if (w) w->raise(); /* NTS: This CHANGES the child element order, getChild(0) will return something else */
        }

        wiw->resize((columns * column_width) + 2/*border*/ + wiw->vscroll_display_width, scroll_h);

        if (wiw->scroll_pos_h != 0) {
            wiw->enableScrollBars(false/*h*/,true/*v*/);
            wiw->enableBorder(true);
        }
        else {
            wiw->enableScrollBars(false/*h*/,false/*v*/);
            wiw->enableBorder(false);

            resize((columns * column_width) + border_left + border_right + 2/*wiw border*/ + /*wiw->vscroll_display_width*//*scrollbar*/ + 10,
                    button_row_y + button_row_h + button_row_padding_y + border_top + border_bottom);
        }
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    ~ConfigEditor() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == MSG_Get("OK")) section->data = *(std::string*)content->getText();
        std::string lines = *(std::string*)content->getText();
        strcpy(tmp1, mainMenu.get_item("HelpMenu").get_text().c_str());
        if (arg == MSG_Get("OK") || arg == MSG_Get("CANCEL") || arg == MSG_Get("CLOSE")) { close(); if(shortcut) running=false; }
        else if (arg == tmp1) {
            std::vector<GUI::Char> new_cfg_sname;

            if (!cfg_sname.empty()) {
//              new_cfg_sname = "help_";
                new_cfg_sname.resize(5);
                new_cfg_sname[0] = 'h';
                new_cfg_sname[1] = 'e';
                new_cfg_sname[2] = 'l';
                new_cfg_sname[3] = 'p';
                new_cfg_sname[4] = '_';
                new_cfg_sname.insert(new_cfg_sname.end(),cfg_sname.begin(),cfg_sname.end());
            }

            auto lookup = cfg_windows_active.find(new_cfg_sname);
            if (lookup == cfg_windows_active.end()) {
                int nx = getX() - 10;
                int ny = getY() - 10;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                auto *np = new HelpWindow(static_cast<GUI::Screen*>(parent), nx, ny, section);
                cfg_windows_active[new_cfg_sname] = np;
                np->cfg_sname = new_cfg_sname;
                np->raise();
            }
            else {
                lookup->second->raise();
            }
		} else if (arg == MSG_Get("PASTE_CLIPBOARD")) {
			strPasteBuffer="";
			swapad=false;
			PasteClipboard(true);
			swapad=true;
			unsigned char head;
            GUI::Key *key;
            GUI::Key::Special ksym = (GUI::Key::Special)0;
			while (strPasteBuffer.length()) {
                key = NULL;
				head = strPasteBuffer[0];
				if (head == 9) for (int i=0; i<8; i++) key = new GUI::Key(' ', ksym, false, false, false, false);
				else if (head == 13) key = new GUI::Key(0, GUI::Key::Enter, false, false, false, false);
				else if (head > 31) key = new GUI::Key(head, ksym, false, false, false, false);
                if (key != NULL) content->keyDown(*key);
				strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
			}
            return;
        } else ToplevelWindow::actionExecuted(b, arg);
    }

    bool keyDown(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyDown(key)) return true;
        return false;
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }

};

class AutoexecEditor : public GUI::ToplevelWindow {
    GUI::Button *closeButton = NULL, *pasteButton = NULL, *appendButton = NULL;
    Section_line * section;
    GUI::Input *content = NULL;
public:
    std::vector<GUI::Char> cfg_sname;
public:
    AutoexecEditor(GUI::Screen *parent, int x, int y, Section_line *section) :
        ToplevelWindow(parent, x, y, 550, 310 + GUI::titlebar_y_stop, ""), section(section) {
        if (section == NULL) {
            LOG_MSG("BUG: AutoexecEditor constructor called with section == NULL\n");
            return;
        }

        std::string title(section->GetName());
        title[0] = std::toupper(title[0]);
        sprintf(tmp1, MSG_Get("EDIT_FOR"), title.c_str());
        setTitle(tmp1);
        new GUI::Label(this, 5, 10, MSG_Get("CONTENT"));
        content = new GUI::Input(this, 5, 30, 550 - 10 - border_left - border_right, 185);
        content->setText(section->data);
        (pasteButton = new GUI::Button(this, 5, 220, MSG_Get("PASTE_CLIPBOARD")))->addActionHandler(this);
        int last = 25 + pasteButton->getWidth();
        if (first_shell) {
            (appendButton = new GUI::Button(this, last, 220, MSG_Get("APPEND_HISTORY")))->addActionHandler(this);
            last += 20 + appendButton->getWidth();
        }
        if (shell_idle) (new GUI::Button(this, last, 220, MSG_Get("EXECUTE_NOW")))->addActionHandler(this);
        (new GUI::Button(this, 180, 260, MSG_Get("OK"), 90))->addActionHandler(this);
        (closeButton = new GUI::Button(this, 285, 260, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    ~AutoexecEditor() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        if (arg == MSG_Get("OK")) section->data = *(std::string*)content->getText();
        if (arg == MSG_Get("OK") || arg == MSG_Get("CANCEL") || arg == MSG_Get("CLOSE")) { close(); if(shortcut) running=false; }
        else if (arg == MSG_Get("PASTE_CLIPBOARD")) {
			strPasteBuffer="";
			swapad=false;
			PasteClipboard(true);
			swapad=true;
			unsigned char head;
            GUI::Key *key;
            GUI::Key::Special ksym = (GUI::Key::Special)0;
			while (strPasteBuffer.length()) {
                key = NULL;
				head = strPasteBuffer[0];
				if (head == 9) for (int i=0; i<8; i++) key = new GUI::Key(' ', ksym, false, false, false, false);
				else if (head == 13) key = new GUI::Key(0, GUI::Key::Enter, false, false, false, false);
				else if (head > 31) key = new GUI::Key(head, ksym, false, false, false, false);
                if (key != NULL) content->keyDown(*key);
				strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
			}
			return;
		} else if (arg == MSG_Get("APPEND_HISTORY")) {
            std::list<std::string>::reverse_iterator i = first_shell->l_history.rbegin();
            std::string lines = *(std::string*)content->getText();
            while (i != first_shell->l_history.rend()) {
                lines += "\n";
                lines += *i;
                ++i;
            }
            content->setText(lines);
        } else if (arg == MSG_Get("EXECUTE_NOW")) {
            UI_RunCommands(dynamic_cast<GUI::ScreenSDL*>(getScreen()), content->getText());
        } else ToplevelWindow::actionExecuted(b, arg);
    }

    bool keyDown(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyDown(key)) return true;
        return false;
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }

};

class SaveDialog : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
    GUI::Button *saveButton = NULL, *closeButton = NULL;
public:
    SaveDialog(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 620, 160 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, MSG_Get("CONFIG_SAVETO"));
        name = new GUI::Input(this, 5, 30, width - 10 - border_left - border_right);
        std::string fullpath;
        if (control->configfiles.size())
            fullpath = control->configfiles[0];
        else
            fullpath = "dosbox-x.conf";
        name->setText(fullpath.c_str());
        (new GUI::Button(this, 5, 60, MSG_Get("USE_PRIMARYCONFIG"), 200))->addActionHandler(this);
        (new GUI::Button(this, 210, 60, MSG_Get("USE_PORTABLECONFIG"), 210))->addActionHandler(this);
        (new GUI::Button(this, 425, 60, MSG_Get("USE_USERCONFIG"), 180))->addActionHandler(this);
        Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
        saveall = new GUI::Checkbox(this, 5, 95, MSG_Get("CONFIG_SAVEALL"));
        saveall->setChecked(section->Get_bool("show advanced options"));
        (saveButton = new GUI::Button(this, 128, 120, MSG_Get("SAVE"), 90))->addActionHandler(this);
        (new GUI::Button(this, 220, 120, MSG_Get("SAVE_RESTART"), 170))->addActionHandler(this);
        (closeButton = new GUI::Button(this, 392, 120, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("USE_PORTABLECONFIG")) {
            name->setText("dosbox-x.conf");
            return;
        }
        if (arg == MSG_Get("USE_PRIMARYCONFIG")) {
            if (control->configfiles.size())
                name->setText(control->configfiles[0]);
            return;
        }
        if (arg == MSG_Get("USE_USERCONFIG")) {
            std::string config_path;
            Cross::GetPlatformConfigDir(config_path);
            std::string fullpath,file;
            Cross::GetPlatformConfigName(file);
            const size_t last_slash_idx = config_path.find_last_of("\\/");
            if (std::string::npos != last_slash_idx) {
                fullpath = config_path.substr(0, last_slash_idx);
                fullpath += CROSS_FILESPLIT;
                fullpath += file;
            } else
                fullpath = file;
            name->setText(fullpath);
            return;
        }
        if (arg == MSG_Get("SAVE") || arg == MSG_Get("SAVE_RESTART")) control->PrintConfig(name->getText(), saveall->isChecked()?1:-1);
        if (arg == MSG_Get("SAVE_RESTART")) RebootConfig((const char*)name->getText(), true);
        close();
        if(shortcut) running=false;
    }

    virtual bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Enter) {
            saveButton->executeAction();
            return true;
        }

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }
};

class SaveLangDialog : public GUI::ToplevelWindow {
protected:
    GUI::Input *name, *lang;
    GUI::Button *saveButton = NULL, *closeButton = NULL;
public:
    SaveLangDialog(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 150 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, MSG_Get("LANG_FILENAME"));
        name = new GUI::Input(this, 5, 30, width - 10 - border_left - border_right);
        name->setText(control->opt_lang != "" ? control->opt_lang.c_str() : "messages.lng");
        new GUI::Label(this, 5, 60, MSG_Get("LANG_LANGNAME"));
        lang = new GUI::Input(this, 5, 80, width - 10 - border_left - border_right);
        lang->setText(langname.c_str());
        (saveButton = new GUI::Button(this, 100, 110, MSG_Get("OK"), 90))->addActionHandler(this);
        (closeButton = new GUI::Button(this, 200, 110, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) MSG_Write(name->getText(), lang->getText());
        close();
        if(shortcut) running=false;
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Enter) {
            saveButton->executeAction();
            return true;
        }

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }
};

// override Input field with one that responds to the Enter key as a keyboard-based cue to click "OK"
class InputWithEnterKey : public GUI::Input {
public:
                                        InputWithEnterKey(Window *parent, int x, int y, int w, int h = 0) : GUI::Input(parent,x,y,w,h) { };
public:
    void                                set_trigger_target(GUI::ToplevelWindow *_who) { trigger_who = _who; };
protected:
    GUI::ToplevelWindow*                trigger_who = NULL;
public:
    std::string                         trigger_enter = MSG_Get("OK");
    std::string                         trigger_esc = MSG_Get("CANCEL");
public:
    bool keyDown(const GUI::Key &key) override {
        if (key.special == GUI::Key::Special::Enter) {
            if (trigger_who != NULL && !trigger_enter.empty())
                trigger_who->actionExecuted(this, trigger_enter);

            return true;
        }
        else if (key.special == GUI::Key::Special::Escape) {
            if (trigger_who != NULL && !trigger_esc.empty())
                trigger_who->actionExecuted(this, trigger_esc);

            return true;
        }
        else {
            return GUI::Input::keyDown(key);
        }
    }
};

extern double vga_force_refresh_rate;
void SetRate(char *x);
class SetRefreshRate : public GUI::ToplevelWindow {
protected:
    InputWithEnterKey *name;
public:
    SetRefreshRate(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 100 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, "Enter video refresh rate (0 = unlocked):");
        name = new InputWithEnterKey(this, 5, 30, width - 10 - border_left - border_right);
        name->set_trigger_target(this);
        std::ostringstream str;
        str << (vga_force_refresh_rate < 0 ? 0 : vga_force_refresh_rate);

        std::string rates=str.str();
        name->setText(rates.c_str());
        (new GUI::Button(this, 100, 60, MSG_Get("OK"), 90))->addActionHandler(this);
        (new GUI::Button(this, 200, 60, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            char *str = (char*)name->getText();
            if (str == NULL || !strcmp(str, "0"))
                vga_force_refresh_rate = -1;
            else
                SetRate(str);
            if (vga_force_refresh_rate > 0)
                LOG_MSG("Video refresh rate is locked to %.3f fps.",vga_force_refresh_rate);
            else
                LOG_MSG("Video refresh rate is unlocked.");
        }
        close();
        if(shortcut) running=false;
    }
};

class SetSensitivity : public GUI::ToplevelWindow {
protected:
    InputWithEnterKey *name;
public:
    SetSensitivity(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 100 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, "Enter mouse sensitivity (x-sen,y-sen):");
//      name = new GUI::Input(this, 5, 30, 350);
        name = new InputWithEnterKey(this, 5, 30, width - 10 - border_left - border_right);
        name->set_trigger_target(this);
        std::ostringstream str;
        str << sdl.mouse.xsensitivity << "," << sdl.mouse.ysensitivity;


        std::string cycles=str.str();
        name->setText(cycles.c_str());
        (new GUI::Button(this, 100, 60, MSG_Get("OK"), 90))->addActionHandler(this);
        (new GUI::Button(this, 200, 60, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            Section* sec = control->GetSection("sdl");
            if (sec) {
                std::string tmp("sensitivity=");
                tmp.append((const char*)(name->getText()));
                sec->HandleInputline(tmp);
                Prop_multival* p3 = static_cast<Section_prop *>(sec)->Get_multival("sensitivity");
                sdl.mouse.xsensitivity = p3->GetSection()->Get_int("xsens");
                sdl.mouse.ysensitivity = p3->GetSection()->Get_int("ysens");
            }
        }
        close();
        if(shortcut) running=false;
    }
};

extern bool enable_autosave;
extern int autosave_second, autosave_count, autosave_start[10], autosave_end[10], autosave_last[10];
extern std::string autosave_name[10];
class SetAutoSave : public GUI::ToplevelWindow {
protected:
    GUI::Input *name[10], *start[10], *end[10];
public:
    SetAutoSave(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 630, 400, title) {
        new GUI::Label(this, 5, 15, "Time interval (secs)");
        name[0] = new GUI::Input(this, 175, 10, 80);
        name[0]->setText(std::to_string(autosave_second).c_str());
        new GUI::Label(this, 270, 15, "Start slot");
        start[0] = new GUI::Input(this, 360, 10, 35);
        start[0]->setText(std::to_string(autosave_start[0]).c_str());
        new GUI::Label(this, 410, 15, "End slot (optional)");
        end[0] = new GUI::Input(this, 575, 10, 35);
        end[0]->setText(std::to_string(autosave_end[0]).c_str());
        for (int i=1; i<10; i++) {
            new GUI::Label(this, 5, 15+i*30, "Program "+std::to_string(i)+" (Optional)");
            name[i] = new GUI::Input(this, 175, 10+i*30, 80);
            name[i]->setText(autosave_name[i].c_str());
            new GUI::Label(this, 270, 15+i*30, "Start slot");
            start[i] = new GUI::Input(this, 360, 10+i*30, 35);
            start[i]->setText(std::to_string(autosave_start[i]).c_str());
            new GUI::Label(this, 410, 15+i*30, "End slot (optional)");
            end[i] = new GUI::Input(this, 575, 10+i*30, 35);
            end[i]->setText(std::to_string(autosave_end[i]).c_str());
        }
        new GUI::Label(this, 15, 315, "Note: 0 for start slot = use current slot; -1 for start slot = skip saving");
        (new GUI::Button(this, 225, 335, MSG_Get("OK"), 90))->addActionHandler(this);
        (new GUI::Button(this, 325, 335, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            autosave_second = atoi(name[0]->getText());
            autosave_start[0] = atoi(start[0]->getText());
            autosave_end[0] = atoi(end[0]->getText());
            for (int i=1; i<10; i++) {
                autosave_name[i] = (const char*)name[i]->getText();
                if (autosave_name[i].size()) autosave_count=i;
                autosave_start[i] = atoi(start[i]->getText());
                if (autosave_start[i]<-1) autosave_start[i]=-1;
                autosave_end[i] = atoi(end[i]->getText());
                if (autosave_end[i]<autosave_start[i]) autosave_end[i]=0;
                if ((autosave_start[i]>1&&autosave_start[i]<=100&&autosave_last[i]<autosave_start[i])||(autosave_last[i]>(autosave_end[i]>=autosave_start[i]&&autosave_end[i]<=100?autosave_end[i]:autosave_start[i]))) autosave_last[i]=-1;
            }
            if (!mainMenu.get_item("enable_autosave").is_enabled()&&autosave_second) enable_autosave = autosave_second>0;
            if (autosave_second<0) autosave_second=-autosave_second;
            mainMenu.get_item("enable_autosave").check(enable_autosave).enable(autosave_second>0).refresh_item(mainMenu);
            mainMenu.get_item("lastautosaveslot").enable(autosave_second>0).refresh_item(mainMenu);
        }
        close();
        if(shortcut) running=false;
    }
};

class SetCycles : public GUI::ToplevelWindow {
protected:
    InputWithEnterKey *name;
public:
    SetCycles(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 100 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, "Enter CPU cycles (or 'max' for max cycles):");
//      name = new GUI::Input(this, 5, 30, 350);
        name = new InputWithEnterKey(this, 5, 30, width - 10 - border_left - border_right);
        name->set_trigger_target(this);
        std::ostringstream str;
        str << "fixed " << CPU_CycleMax;

        std::string cycles=str.str();
        name->setText(cycles.c_str());
        (new GUI::Button(this, 100, 60, MSG_Get("OK"), 90))->addActionHandler(this);
        (new GUI::Button(this, 200, 60, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            Section* sec = control->GetSection("cpu");
            if (sec) {
                std::string tmp("cycles=");
                tmp.append((const char*)(name->getText()));
                sec->HandleInputline(tmp);
            }
        }
        close();
        if(shortcut) running=false;
    }
};

class SetVsyncrate : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    SetVsyncrate(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 100 + GUI::titlebar_y_stop, title) {
        new GUI::Label(this, 5, 10, "Enter vertical syncrate (Hz):");
        name = new GUI::Input(this, 5, 30, width - 10 - border_left - border_right);
        Section_prop * sec = static_cast<Section_prop *>(control->GetSection("vsync"));
        if (sec)
            name->setText(sec->Get_string("vsyncrate"));
        else
            name->setText("");
        (new GUI::Button(this, 100, 60, MSG_Get("OK"), 90))->addActionHandler(this);
        (new GUI::Button(this, 200, 60, MSG_Get("CANCEL"), 90))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

        name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
        name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        Section_prop * sec = static_cast<Section_prop *>(control->GetSection("vsync"));
        if (arg == MSG_Get("OK")) {
            if (sec) {
                std::string s((const char *)name->getText());
                if (s.size()>20) s=s.substr(0,20);
                std::string tmp("vsyncrate=");
                tmp.append(s);
                sec->HandleInputline(tmp);
            }
        }
        if (sec) LOG_MSG("GUI: Current Vertical Sync Rate: %s Hz", sec->Get_string("vsyncrate"));
        close();
        if(shortcut) running=false;
    }
};

class SetLocalSize : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    SetLocalSize(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 450, 150, title) {
            new GUI::Label(this, 5, 10, "Enter default local freesize (MB, min=0, max=1024):");
            name = new GUI::Input(this, 5, 30, 400);
            extern unsigned int hdd_defsize;
            unsigned int human_readable = 512u * 32u * hdd_defsize / 1024u / 1024u;
            char buffer[6];
            sprintf(buffer, "%u", human_readable);
            name->setText(buffer);
            (new GUI::Button(this, 100, 70, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 200, 70, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
            name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            extern unsigned int hdd_defsize;
            int human_readable = atoi(name->getText());
            if (human_readable < 0)
                hdd_defsize = 0u;
            else if (human_readable > 1024)
                hdd_defsize = 256000u;
            else
                hdd_defsize = (unsigned int)human_readable * 1024u * 1024u / 512u / 32u;
            LOG_MSG("GUI: Current default freesize for local disk: %dMB", 512u * 32u * hdd_defsize / 1024u / 1024u);
        }
        close();
        if (shortcut) running = false;
    }
};

bool set_ver(char *s);
void dos_ver_menu(bool start);
class SetDOSVersion : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    SetDOSVersion(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 410, 140, title) {
            new GUI::Label(this, 5, 10, "Enter reported DOS version:");
            name = new GUI::Input(this, 5, 30, 390);
            char buffer[8];
            sprintf(buffer, "%d.%02d", dos.version.major,dos.version.minor);
            name->setText(buffer);
            (new GUI::Button(this, 100, 70, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 200, 70, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
            name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            if (set_ver(name->getText()))
                dos_ver_menu(false);
        }
        close();
        if (shortcut) running = false;
    }
};

class SetAspectRatio : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    SetAspectRatio(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 410, 140, title) {
            new GUI::Label(this, 5, 10, "Enter aspect ratio (w:h, -1:-1 = original ratio):");
            name = new GUI::Input(this, 5, 30, 390);
            char buffer[8];
            sprintf(buffer, "%d:%d", aspect_ratio_x,aspect_ratio_y);
            name->setText(buffer);
            (new GUI::Button(this, 100, 70, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 200, 70, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
            name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            SetVal("render", "aspect_ratio", name->getText());
            setAspectRatio(static_cast<Section_prop *>(control->GetSection("render")));
            //if (render.aspect) GFX_ForceRedrawScreen();
        }
        close();
        if (shortcut) running = false;
    }
};

extern std::string dosbox_title;
class SetTitleText : public GUI::ToplevelWindow {
protected:
    GUI::Input *title1, *title2;
    Section_prop *sec1, *sec2;
    std::string t1, t2;
public:
    SetTitleText(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 410, 180, title) {
            sec1 = static_cast<Section_prop *>(control->GetSection("dosbox"));
            sec2 = static_cast<Section_prop *>(control->GetSection("sdl"));
            t1 = sec1->Get_string("title");
            t2 = sec2->Get_string("titlebar");
            trim(t1);
            trim(t2);
            new GUI::Label(this, 5, 10, "Prepend text in title bar:");
            title1 = new GUI::Input(this, 5, 30, 390);
            title1->setText(t1);
            new GUI::Label(this, 5, 60, "Append text in title bar:");
            title2 = new GUI::Input(this, 5, 80, 390);
            title2->setText(t2);
            (new GUI::Button(this, 100, 110, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 200, 110, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            title1->raise(); /* make sure keyboard focus is on the text field, ready for the user */
            title1->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            dosbox_title = trim(title1->getText());
            SetVal("dosbox", "title", dosbox_title);
            SetVal("sdl", "titlebar", trim(title2->getText()));
            GFX_SetTitle(-1,-1,-1,false);
        }
        close();
        if (shortcut) running = false;
    }
};

class SetTransparency : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
	Section_prop * sec;
public:
    SetTransparency(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 410, 140, title) {
			sec = static_cast<Section_prop *>(control->GetSection("sdl"));
            new GUI::Label(this, 5, 10, "Enter transparency (0-90; from low to high):");
            name = new GUI::Input(this, 5, 30, 390);
			std::ostringstream str;
			str << sec->Get_int("transparency");
			std::string transtr=str.str();
            name->setText(transtr.c_str());
            (new GUI::Button(this, 100, 70, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 200, 70, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);

            name->raise(); /* make sure keyboard focus is on the text field, ready for the user */
            name->posToEnd(); /* position the cursor at the end where the user is most likely going to edit */
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("OK")) {
            SetVal("sdl", "transparency", name->getText());
			sec = static_cast<Section_prop *>(control->GetSection("sdl"));
            SetWindowTransparency(sec->Get_int("transparency"));
        }
        close();
        if (shortcut) running = false;
    }
};

class ShowMixerInfo : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowMixerInfo(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 350, 270, title) {
            extern std::string mixerinfo();
            std::istringstream in(mixerinfo().c_str());
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 140, r+30, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            resize(350, r+110);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

std::string GetSBtype(), GetSBbase(), GetSBirq(), GetSBldma(), GetSBhdma();

class ShowSBInfo : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowSBInfo(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 320, 230, title) {
            std::string sbinfo = "Sound Blaster type: "+GetSBtype()+"\nSound Blaster base: "+GetSBbase()+"\nSound Blaster IRQ: "+GetSBirq()+"\nSound Blaster Low DMA: "+GetSBldma()+"\nSound Blaster High DMA: "+GetSBhdma();
            std::istringstream in(sbinfo.c_str());
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 130, r+30, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

extern DB_Midi midi;
extern std::string sffile;
class ShowMidiDevice : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowMidiDevice(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 320, 260, title) {
            std::string name=!midi.handler||!midi.handler->GetName()?"-":midi.handler->GetName();
            std::string sf_rom{};
            if (name.size()) {
                if(name == "mt32") {
                    name = "MT32";
                    sf_rom = "\nMT32 ROM path:\n" + sffile;
                }
                else if(name == "fluidsynth") {
                    name = "FluidSynth";
                    sf_rom = "\nSoundFont file:\n" + sffile;
                }
                else name[0]=toupper(name[0]);
            }
            extern std::string getoplmode(), getoplemu();
            std::string midiinfo = "MIDI available: "+std::string(midi.available?"Yes":"No")+"\nMIDI device: "+name+sf_rom
                +"\nOPL mode: "+getoplmode()+"\nOPL emulation: "+getoplemu()
                + (sf_rom.size()?"":"\n\n\n");
            std::istringstream in(midiinfo.c_str());
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 130, r+30, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

class ShowDriveInfo : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowDriveInfo(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 400, 280, title) {
            char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
            uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
            /* Command uses dta so set it to our internal dta */
            RealPt save_dta = dos.dta();
            dos.dta(dos.tables.tempdta);
            DOS_DTA dta(dos.dta());
            if (Drives[statusdrive]) {
                char root[7] = {(char)('A'+statusdrive),':','\\','*','.','*',0};
                bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
                if (ret) {
                    dta.GetResult(name,lname,size,hsize,date,time,attr);
                    DOS_FindNext(); //Mark entry as invalid
                } else name[0] = 0;

                /* Change 8.3 to 11.0 */
                const char* dot = strchr(name, '.');
                if(dot && (dot - name == 8) ) {
                    name[8] = name[9];name[9] = name[10];name[10] = name[11];name[11] = 0;
                }

                root[3] = 0; //This way, the format string can be reused.
                std::string type, path, swappos="-", overlay="-";
                bool readonly=false;
                const char *info = Drives[statusdrive]->GetInfo();
                if (!strncmp(info, "fatDrive ", 9) || !strncmp(info, "isoDrive ", 9)) {
                    type=strncmp(info, "isoDrive ", 9)?"fatDrive":"isoDrive";
                    path=info+9;
                    if (type=="isoDrive")
                        readonly=true;
                    else {
                        readonly=Drives[statusdrive]->readonly;
                        if (!path.size()) {
                            fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[statusdrive]);
                            if (fdp!=NULL&&fdp->opts.mounttype==1)
                                path="El Torito floppy drive";
                            else if (fdp!=NULL&&fdp->opts.mounttype==2)
                                path="RAM drive";
                        }
                    }
                    swappos=DriveManager::GetDrivePosition(statusdrive);
                } else if (!strncmp(info, "PhysFS directory ", 17)) {
                    type="PhysFS directory";
                    path=info+17;
                    readonly=true;
                    physfsDrive *pdp = dynamic_cast<physfsDrive*>(Drives[statusdrive]);
                    if (pdp!=NULL) {
                        const char *wdir = pdp->getOverlaydir();
                        if (wdir!=NULL&&strlen(wdir)) {
                            readonly=false;
                            overlay=std::string(wdir)+(wdir[strlen(wdir)-1]!=CROSS_FILESPLIT?std::string(1, CROSS_FILESPLIT):"")+std::string(1, 'A'+statusdrive)+"_DRIVE";
                        }
                    }
                } else if (!strncmp(info, "PhysFS CDRom ", 13)) {
                    type="PhysFS CDRom";
                    path=info+13;
                    readonly=true;
                } else if (!strncmp(info, "local directory ", 16)) {
                    type="local directory";
                    path=info+16;
                    readonly=Drives[statusdrive]->readonly;
                    Overlay_Drive *ddp = dynamic_cast<Overlay_Drive*>(Drives[statusdrive]);
                    if (ddp!=NULL) {
                        readonly=ddp->ovlreadonly;
                        overlay=ddp->getOverlaydir();
                    }
                } else if (!strncmp(info, "CDRom ", 6)) {
                    type="CDRom";
                    path=info+6;
                    readonly=true;
                } else {
                    type=info;
                    path="";
                    readonly=true;
                }
                if (path=="") path="-";
                new GUI::Label(this, 40, 25, "Drive root: "+std::string(root));
                new GUI::Label(this, 40, 50, "Drive type: "+type);
                new GUI::Label(this, 40, 75, "Mounted as: "+path);
                new GUI::Label(this, 40, 100, "Overlay at: "+overlay);
                new GUI::Label(this, 40, 125, "Disk label: "+std::string(name));
                new GUI::Label(this, 40, 150, "Read only : "+std::string(readonly?"Yes":"No"));
                new GUI::Label(this, 40, 175, "Swap slot : "+swappos);
            }
            dos.dta(save_dta);
            (new GUI::Button(this, 165, 205, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get(MSG_Get("CLOSE")))
            close();
        if (shortcut) running = false;
    }
};

std::string GetIDEPosition(unsigned char bios_disk_index);
class ShowDriveNumber : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowDriveNumber(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 480, 260, title) {
        std::string str;
		for (int index = 0; index < MAX_DISK_IMAGES; index++) {
			if (imageDiskList[index]) {
                int swaps=0;
                if (swapInDisksSpecificDrive == index) {
                    for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++)
                        if (diskSwap[si] != NULL)
                            swaps++;
                }
                if (!swaps) swaps=1;
                if (index<2)
                    str = "Swap position: " + std::to_string(swaps==1?1:swapPosition+1) + "/" + std::to_string(swaps) + " - " + (dynamic_cast<imageDiskElToritoFloppy *>(imageDiskList[index])!=NULL?"El Torito floppy drive":imageDiskList[index]->diskname);
                else {
                    str = GetIDEPosition(index);
                    str = "IDE controller: " + (str.size()?str:"NA") + " - " + imageDiskList[index]->diskname;
                }
            } else
                str = "Not yet mounted";
            new GUI::Label(this, 40, 25*(index+1), std::to_string(index) + " - " + str);
        }
        (new GUI::Button(this, 190, 25*(MAX_DISK_IMAGES+1)+5, MSG_Get("CLOSE"), 70))->addActionHandler(this);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

class ShowIDEInfo : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowIDEInfo(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 300, 210, title) {
            extern std::string GetIDEInfo();
            std::istringstream in(GetIDEInfo().c_str());
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 110, r+30, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            resize(300, r+110);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

class ShowLoadWarning : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowLoadWarning(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 430, 120, MSG_Get("WARNING")) {
            new GUI::Label(this, strncmp(title, "DOSBox-X ", 9)?30:10, 20, title);
            (new GUI::Button(this, 140, 50, MSG_Get("YES"), 70))->addActionHandler(this);
            (new GUI::Button(this, 230, 50, MSG_Get("NO"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("YES"))
            confres=true;
        if (arg == MSG_Get("NO"))
            confres=false;
        close();
        if (shortcut) running = false;
    }
};

class MakeDiskImage : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    MakeDiskImage(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 500, 300, title) {
            new GUI::Label(this, 100, 30, "Select a floppy disk image size:");
            imgfd360 = new GUI::Checkbox(this, 110, 60, "360KB");
            imgfd360->addActionHandler(this);
            imgfd400 = new GUI::Checkbox(this, 210, 60, "400KB");
            imgfd400->addActionHandler(this);
            imgfd720 = new GUI::Checkbox(this, 310, 60, "720KB");
            imgfd720->addActionHandler(this);
            imgfd1200 = new GUI::Checkbox(this, 110, 90, "1.2MB");
            imgfd1200->addActionHandler(this);
            imgfd1440 = new GUI::Checkbox(this, 210, 90, "1.44MB");
            imgfd1440->addActionHandler(this);
            imgfd2880 = new GUI::Checkbox(this, 310, 90, "2.88MB");
            imgfd2880->addActionHandler(this);
            new GUI::Label(this, 100, 120, "Select a hard disk image size:");
            imghd250 = new GUI::Checkbox(this, 110, 150, "250MB");
            imghd250->addActionHandler(this);
            imghd520 = new GUI::Checkbox(this, 210, 150, "520MB");
            imghd520->addActionHandler(this);
            imghd1gig = new GUI::Checkbox(this, 310, 150, "1GB");
            imghd1gig->addActionHandler(this);
            imghd2gig = new GUI::Checkbox(this, 110, 180, "2GB");
            imghd2gig->addActionHandler(this);
            imghd4gig = new GUI::Checkbox(this, 210, 180, "4GB");
            imghd4gig->addActionHandler(this);
            imghd8gig = new GUI::Checkbox(this, 310, 180, "8GB");
            imghd8gig->addActionHandler(this);
            (new GUI::Button(this, 135, 220, MSG_Get("OK"), 90))->addActionHandler(this);
            (new GUI::Button(this, 255, 220, MSG_Get("CANCEL"), 90))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == "360KB" && imgfd360->isChecked()) {
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "720KB" && imgfd720->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "400KB" && imgfd400->isChecked()) {
            imgfd360->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "1.2MB" && imgfd1200->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "1.44MB" && imgfd1440->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "2.88MB" && imgfd2880->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "250MB" && imghd250->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "520MB" && imghd520->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "1GB" && imghd1gig->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "2GB" && imghd2gig->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd4gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "4GB" && imghd4gig->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd8gig->setChecked(false);
        } else if (arg == "8GB" && imghd8gig->isChecked()) {
            imgfd360->setChecked(false);
            imgfd400->setChecked(false);
            imgfd720->setChecked(false);
            imgfd1200->setChecked(false);
            imgfd1440->setChecked(false);
            imgfd2880->setChecked(false);
            imghd250->setChecked(false);
            imghd520->setChecked(false);
            imghd1gig->setChecked(false);
            imghd2gig->setChecked(false);
            imghd4gig->setChecked(false);
        }
        if (arg == MSG_Get("OK")) {
            std::string temp="";
            if (imgfd360->isChecked())
               temp="fd_360";
            else if (imgfd400->isChecked())
               temp="fd_400";
            else if (imgfd720->isChecked())
               temp="fd_720";
            else if (imgfd1200->isChecked())
               temp="fd_1200";
            else if (imgfd1440->isChecked())
               temp="fd_1440";
            else if (imgfd2880->isChecked())
               temp="fd_2880";
            else if (imghd250->isChecked())
               temp="hd_250";
            else if (imghd520->isChecked())
               temp="hd_520";
            else if (imghd1gig->isChecked())
               temp="hd_1gig";
            else if (imghd2gig->isChecked())
               temp="hd_2gig";
            else if (imghd4gig->isChecked())
               temp="hd_4gig";
            else if (imghd8gig->isChecked())
               temp="hd_8gig";
            if (temp.size()) {
#if defined(HX_DOS)
                char const * lTheSaveFileName = "IMGMAKE.IMG";
#else
                char CurrentDir[512];
                char * Temp_CurrentDir = CurrentDir;
                getcwd(Temp_CurrentDir, 512);
                const char *lFilterPatterns[] = {"*.img","*.IMG"};
                const char *lFilterDescription = "Disk image files (*.img)";
                char const * lTheSaveFileName = tinyfd_saveFileDialog("Select a disk image file","IMGMAKE.IMG",2,lFilterPatterns,lFilterDescription);
#endif
                if (lTheSaveFileName!=NULL) {
                    temp="-force -t "+temp+" \""+std::string(lTheSaveFileName)+"\"";
                    void runImgmake(const char *str);
                    runImgmake(temp.c_str());
                    if (!dos_kernel_disabled && strcmp(RunningProgram, "LOADLIN")) {
                        DOS_Shell shell;
                        shell.ShowPrompt();
                    }
                }
#if !defined(HX_DOS)
                chdir( Temp_CurrentDir );
#endif
            }
            if (shortcut) running = false;
        }
        else if (arg == MSG_Get("CLOSE") || arg == MSG_Get("CANCEL")) {
            close();
            if (shortcut) running = false;
        }
    }
};

class ShowHelpIntro : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowHelpIntro(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 610, 190, title) {
            std::istringstream in(MSG_Get("INTRO_MESSAGE"));
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 260, 110, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

std::string prtlist="Printer support is not enabled. Check [printer] section of the configuration.";
class ShowHelpPRT : public GUI::MessageBox2 {
public:
    std::vector<GUI::Char> cfg_sname;
public:
    ShowHelpPRT(GUI::Screen *parent, int x, int y) :
        MessageBox2(parent, x, y, 630, "", "") { // 740
        setTitle(MSG_Get("PRINTER_LIST"));
        setText(prtlist);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    };

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
         if (arg == MSG_Get("CLOSE"))
         if(shortcut) running=false;
    }

    ~ShowHelpPRT() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

std::string niclist="The pcap networking for NE2000 Ethernet emulation is not currently active.\nPlease check [ne2000] and [ethernet, pcap] sections of the configuration.";
class ShowHelpNIC : public GUI::MessageBox2 {
public:
    std::vector<GUI::Char> cfg_sname;
public:
    ShowHelpNIC(GUI::Screen *parent, int x, int y) :
        MessageBox2(parent, x, y, 630, "", "") { // 740
        setTitle(MSG_Get("NETWORK_LIST"));
        setText(niclist);
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    };

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
         if (arg == MSG_Get("CLOSE"))
         if(shortcut) running=false;
    }

    ~ShowHelpNIC() {
        if (!cfg_sname.empty()) {
            auto i = cfg_windows_active.find(cfg_sname);
            if (i != cfg_windows_active.end()) cfg_windows_active.erase(i);
        }
    }
};

class ShowHelpAbout : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowHelpAbout(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 420, 230, title) {
            std::istringstream in(aboutmsg);
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 180, 155, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

extern std::string helpcmd;
char *str_replace(const char *orig, const char *rep, const char *with);
class ShowHelpCommand : public GUI::ToplevelWindow {
protected:
    GUI::Input *name;
public:
    ShowHelpCommand(GUI::Screen *parent, int x, int y, const char *title) :
        ToplevelWindow(parent, x, y, 750, 270, title) {
            if (helpcmd=="CD") helpcmd="CHDIR";
            else if (helpcmd=="DEL") helpcmd="DELETE";
            else if (helpcmd=="LH") helpcmd="LOADHIGH";
            else if (helpcmd=="MD") helpcmd="MKDIR";
            else if (helpcmd=="RD") helpcmd="RMDIR";
            else if (helpcmd=="REN") helpcmd="RENAME";
            std::string helpinfo=std::string(MSG_Get(("SHELL_CMD_"+helpcmd+"_HELP").c_str()))+"\n"+std::string(MSG_Get(("SHELL_CMD_"+helpcmd+"_HELP_LONG").c_str()));
            std::istringstream in(str_replace(str_replace(str_replace(str_replace(helpinfo.c_str(), "%%", "%"), "\033[0m", ""), "\033[33;1m", ""), "\033[37;1m", ""));
            int r=0;
            if (in)	for (std::string line; std::getline(in, line); ) {
                r+=25;
                new GUI::Label(this, 40, r, line.c_str());
            }
            (new GUI::Button(this, 350, r+40, MSG_Get("CLOSE"), 70))->addActionHandler(this);
            resize(750, r+120);
            move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        (void)b;//UNUSED
        if (arg == MSG_Get("CLOSE"))
            close();
        if (shortcut) running = false;
    }
};

struct ThemePresetEntry {
	ThemePresetEntry(const std::string &_name,const GUI::ThemeWindows31 &&_theme,const unsigned int _special=0) : name(_name), theme(_theme), special(_special) {}

	std::string			name;
	GUI::ThemeWindows31		theme;
	unsigned int			special;

	static constexpr unsigned int	PREPEND_HLINE = 1u << 0u;
	static constexpr unsigned int	PREPEND_VLINE = 1u << 1u;
};

#define ENTRY(x) ThemePresetEntry( GUI::x::GetName(), GUI::x() )
#define ENTRYS(x,s) ThemePresetEntry( GUI::x::GetName(), GUI::x(), s )
static const ThemePresetEntry theme_presets[] = {
	ENTRY(  ThemeWindows31WindowsDefault),
	ENTRYS( ThemeWindows31Arizona, ThemePresetEntry::PREPEND_HLINE),
	ENTRY(  ThemeWindows31Bordeaux),
	ENTRY(  ThemeWindows31Cinnamon),
	ENTRY(  ThemeWindows31Designer),
	ENTRY(  ThemeWindows31EmeraldCity),
	ENTRY(  ThemeWindows31Fluorescent),
	ENTRY(  ThemeWindows31HotDogStand),
	ENTRY(  ThemeWindows31LCDDefaultScreenSettings),
	ENTRY(  ThemeWindows31LCDReversedDark),
	ENTRY(  ThemeWindows31LCDReversedLight),
	ENTRY(  ThemeWindows31BlackLeatherJacket),
	ENTRYS( ThemeWindows31Mahogany, ThemePresetEntry::PREPEND_VLINE),
	ENTRY(  ThemeWindows31Monochrome),
	ENTRY(  ThemeWindows31Ocean),
	ENTRY(  ThemeWindows31Pastel),
	ENTRY(  ThemeWindows31Patchwork),
	ENTRY(  ThemeWindows31PlasmaPowerSaver),
	ENTRY(  ThemeWindows31Rugby),
	ENTRY(  ThemeWindows31TheBlues),
	ENTRY(  ThemeWindows31Tweed),
	ENTRY(  ThemeWindows31Valentine),
	ENTRY(  ThemeWindows31Wingtips)
};

class ConfigurationWindow : public GUI::ToplevelWindow {
public:
    GUI::Button *saveButton, *closeButton;
    ConfigurationWindow(GUI::Screen *parent, GUI::Size x, GUI::Size y, GUI::String& title) :
        GUI::ToplevelWindow(parent, (int)x, (int)y, 30/*initial*/, 30/*initial*/, title) {
        cfg_windows_active.clear();

        GUI::Menubar *bar = new GUI::Menubar(this, 0, 0, getWidth()/*initial*/);
        bar->addMenu(MSG_Get("CONFIGURATION"));
        strcpy(tmp1, (MSG_Get("SAVE")+std::string("...")).c_str());
        bar->addItem(0,tmp1);
        strcpy(tmp1, (MSG_Get("SAVE_LANGUAGE")+std::string("...")).c_str());
        bar->addItem(0,tmp1);
        bar->addItem(0,"");
        bar->addItem(0,MSG_Get("CLOSE"));
        bar->addMenu(MSG_Get("SETTINGS"));
        
        // theme menu
        bar->addMenu("Theme"); // TODO MSG_Get("THEME")
        for (size_t ti=0;ti < (sizeof(theme_presets)/sizeof(theme_presets[0]));ti++) {
            if (theme_presets[ti].special & ThemePresetEntry::PREPEND_HLINE)
                bar->addItem(2, "");
            if (theme_presets[ti].special & ThemePresetEntry::PREPEND_VLINE)
                bar->addItem(2, "|");

            bar->addItem(2, theme_presets[ti].name);
        }
        
        bar->addMenu(mainMenu.get_item("HelpMenu").get_text().c_str());
        bar->addItem(3,MSG_Get("VISIT_HOMEPAGE"));
        bar->addItem(3,"");
        if (!dos_kernel_disabled) {
            /* these do not work until shell help text is registered */
            bar->addItem(3,MSG_Get("GET_STARTED"));
            bar->addItem(3,MSG_Get("CDROM_SUPPORT"));
            bar->addItem(3,"");
        }
        bar->addItem(3,MSG_Get("INTRODUCTION"));
        bar->addItem(3,mainMenu.get_item("help_about").get_text().c_str());
        bar->addActionHandler(this);

        int gridbtnwidth = 130;
        int gridbtnheight = GUI::CurrentTheme.ButtonHeight;
        int gridbtnx = 12;
        int gridbtny = 25;
        int btnperrow = 4;
        int i = 0;

        constexpr auto margin = 3;
        
        const auto xSpace = gridbtnwidth + margin;
        const auto ySpace = gridbtnheight + margin;
        
        std::function< std::pair<int,int>(const int) > gridfunc = [&/*access to locals here*/](const int i){
            return std::pair<int,int>(gridbtnx+(i%btnperrow)*xSpace, gridbtny+(i/btnperrow)*ySpace);
        };

        std::vector<Section*> sections; // sorted by relevance

        // actual sorting
        {
            std::vector<std::string> sectionNames = {
                "log", "dosbox", "render", "sdl",
                "video", "voodoo", "vsync", "ttf",
                "dos", "dosv", "pc98", "4dos",
                "autoexec", "config", "cpu", "speaker",
                "sblaster", "sblaster2", "gus", "midi", "mixer",
                "innova", "imfc", "joystick", "mapper",
                "keyboard", "serial", "parallel", "printer",
                "ipx", "ne2000", "ethernet, pcap", "ethernet, slirp",
                "ide, primary", "ide, secondary", "ide, tertiary", "ide, quaternary",
                "ide, quinternary", "ide, sexternary", "ide, septernary", "ide, octernary",
                "fdc, primary",
            };

            Section* section;

            auto temp1 = 0;

            while((section = control->GetSection(temp1++)))
            {
                sections.push_back(section);
            }

            std::unordered_map<std::string, int> sectionMap;

            auto temp2 = 0;

            for(const auto& name : sectionNames)
            {
                sectionMap[name] = temp2++;
            }

            std::sort(
                      sections.begin(), sections.end(),
                      [&sectionMap](const Section* a, const Section* b)
                      {
                          const auto itA = sectionMap.find(a->GetName());
                          const auto itB = sectionMap.find(b->GetName());
                          if(itA != sectionMap.end() && itB != sectionMap.end())
                          {
                              return itA->second < itB->second;
                          }
                          if(itA != sectionMap.end())
                          {
                              return true;
                          }
                          if(itB != sectionMap.end())
                          {
                              return false;
                          }
                          return false;
                      });
        }
        
        for(const auto & sec : sections)
        {
            if (i != 0 && (i%16) == 0) bar->addItem(1, "|");
            std::string sectionTitle = CapName(std::string(sec->GetName()));
            const auto sz = gridfunc(i);
            GUI::Button *b = new GUI::Button(this, sz.first, sz.second, sectionTitle, gridbtnwidth, gridbtnheight);
            b->addActionHandler(this);
            bar->addItem(1, sectionTitle);
            i++;
        }

        const auto finalgridpos = gridfunc(i - 1);
        int closerow_y = finalgridpos.second + 5 + ySpace;

        advopt = new GUI::Checkbox(this, gridbtnx, closerow_y, MSG_Get("SHOW_ADVOPT"));
        Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
        advopt->setChecked(section->Get_bool("show advanced options") || advOptUser);

        // apply theme
        {
            auto theme = std::string(section->Get_string("configuration tool theme"));

            if(theme.empty())
            {
                bool dark = false;

#if defined(WIN32) && !defined(HX_DOS)
                // ReSharper disable once CppDeclaratorDisambiguatedAsFunction
                bool HostDarkMode(); // NOLINT(clang-diagnostic-vexing-parse)
                dark = HostDarkMode();
#endif
                TryApplyTheme(
                              dark
                                  ? GUI::ThemeWindows31LCDReversedDark::GetName()
                                  : GUI::ThemeWindows31WindowsDefault::GetName());
            }
            else
            {
                TryApplyTheme(theme);
            }
        }

        strcpy(tmp1, (MSG_Get("SAVE")+std::string("...")).c_str());

        const auto xSave = gridbtnx + (gridbtnwidth + margin) * 2;
        const auto xExit = gridbtnx + (gridbtnwidth + margin) * 3;

        (saveButton  = new GUI::Button(this, xSave, closerow_y, tmp1, gridbtnwidth, gridbtnheight))->addActionHandler(this);
        (closeButton = new GUI::Button(this, xExit, closerow_y, MSG_Get("CLOSE"), gridbtnwidth, gridbtnheight))->addActionHandler(this);

        resize(gridbtnx + (xSpace * btnperrow) + gridbtnx + border_left + border_right,
               closerow_y + closeButton->getHeight() + 8 + border_top + border_bottom);

        bar->resize(getWidth(),bar->getHeight());
        move(parent->getWidth()>this->getWidth()?(parent->getWidth()-this->getWidth())/2:0,parent->getHeight()>this->getHeight()?(parent->getHeight()-this->getHeight())/2:0);
    }

    ~ConfigurationWindow() { running = false; cfg_windows_active.clear(); }

    bool keyDown(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyDown(key)) return true;
        return false;
    }

    bool keyUp(const GUI::Key &key) override {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Escape) {
            closeButton->executeAction();
            return true;
        }

        return false;
    }

    static bool TryApplyTheme(const GUI::String& name)
    {
        for (size_t ti=0;ti < (sizeof(theme_presets)/sizeof(theme_presets[0]));ti++) {
            if (name == theme_presets[ti].name) {
                GUI::DefaultTheme = theme_presets[ti].theme;
                return true;
            }
        }

        return false;
    }

    void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) override {
        advOptUser = advopt->isChecked();
        if (TryApplyTheme(arg)) { // TODO MSG_Get("THEME"), save to config
            Section_prop * section=static_cast<Section_prop *>(control->GetSection("dosbox"));
            section->Get_prop("configuration tool theme")->SetValue(arg);
        }
        GUI::String sname = RestoreName(arg);
        sname.at(0) = (unsigned int)std::tolower((int)sname.at(0));
        Section *sec;
        strcpy(tmp1, mainMenu.get_item("help_about").get_text().c_str());
        strcpy(tmp2, (MSG_Get("SAVE")+std::string("...")).c_str());
        if (arg == MSG_Get("OK") || arg == MSG_Get("CANCEL") || arg == MSG_Get("CLOSE")) {
            running = false;
        } else if (sname == "autoexec") {
            auto lookup = cfg_windows_active.find(sname);
            if (lookup == cfg_windows_active.end()) {
                Section_line *section = static_cast<Section_line *>(control->GetSection((const char *)sname));
                auto *np = new AutoexecEditor(getScreen(), 50, 30, section);
                cfg_windows_active[sname] = np;
                np->cfg_sname = sname;
                np->raise();
            }
            else {
                lookup->second->raise();
            }
        } else if (sname == "config" || sname == "4dos") {
            auto lookup = cfg_windows_active.find(sname);
            if (lookup == cfg_windows_active.end()) {
                Section_prop *section = static_cast<Section_prop *>(control->GetSection((const char *)sname));
                auto *np = new ConfigEditor(getScreen(), 50, 30, section);
                cfg_windows_active[sname] = np;
                np->cfg_sname = sname;
                np->raise();
            }
            else {
                lookup->second->raise();
            }
        } else if ((sec = control->GetSection((const char *)sname))) {
            auto lookup = cfg_windows_active.find(sname);
            if (lookup == cfg_windows_active.end()) {
                Section_prop *section = static_cast<Section_prop *>(sec);
                auto *np = new SectionEditor(getScreen(), 50, 30, section);
                cfg_windows_active[sname] = np;
                np->cfg_sname = sname;
                np->raise();
            }
            else {
                lookup->second->raise();
            }
        } else if (arg == MSG_Get("VISIT_HOMEPAGE")) {
            std::string url = "https://dosbox-x.com/";
#if defined(WIN32)
            ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(LINUX)
            system(("xdg-open "+url).c_str());
#elif defined(MACOSX)
            system(("open "+url).c_str());
#endif
        } else if (arg == tmp1) {
            //new GUI::MessageBox2(getScreen(), 100, 150, 330, "About DOSBox-X", aboutmsg);
            new GUI::MessageBox2(getScreen(), getScreen()->getWidth()>350?(parent->getWidth()-350)/2:0, 150, 350, mainMenu.get_item("help_about").get_text().c_str(), aboutmsg);
        } else if (arg == MSG_Get("INTRODUCTION")) {
            //new GUI::MessageBox2(getScreen(), 20, 50, 540, "Introduction", intromsg);
            new GUI::MessageBox2(getScreen(), getScreen()->getWidth()>540?(parent->getWidth()-540)/2:0, 150, 540, mainMenu.get_item("help_intro").get_text().c_str(), MSG_Get("INTRO_MESSAGE"));
        } else if (arg == MSG_Get("GET_STARTED")) {
            std::string msg = MSG_Get("PROGRAM_INTRO_MOUNT_START");
#ifdef WIN32
            msg += MSG_Get("PROGRAM_INTRO_MOUNT_EXST_WINDOWS")+std::string("\n\n")+MSG_Get("PROGRAM_INTRO_MOUNT_EXEN_WINDOWS");
#else
            msg += MSG_Get("PROGRAM_INTRO_MOUNT_EXST_OTHER")+std::string("\n\n")+MSG_Get("PROGRAM_INTRO_MOUNT_EXEN_OTHER");
#endif
            msg += std::string("\n\n")+MSG_Get("PROGRAM_INTRO_MOUNT_END");

            //new GUI::MessageBox2(getScreen(), 0, 50, 680, std::string("Getting Started"), msg);
            new GUI::MessageBox2(getScreen(), getScreen()->getWidth()>680?(parent->getWidth()-680)/2:0, 50, 680, MSG_Get("GET_STARTED"), msg.c_str());
        } else if (arg == MSG_Get("CDROM_SUPPORT")) {
            //new GUI::MessageBox2(getScreen(), 20, 50, 640, "CD-ROM Support", MSG_Get("PROGRAM_INTRO_CDROM"));
            new GUI::MessageBox2(getScreen(), getScreen()->getWidth()>640?(parent->getWidth()-640)/2:0, 50, 640, MSG_Get("CDROM_SUPPORT"), MSG_Get("PROGRAM_INTRO_CDROM"));
        } else if (arg == tmp2) {
            strcpy(tmp1, (MSG_Get("SAVE_CONFIGURATION")+std::string("...")).c_str());
            auto *np = new SaveDialog(getScreen(), 50, 100, tmp1);
            np->raise();
        } else {
            strcpy(tmp2, (MSG_Get("SAVE_LANGUAGE")+std::string("...")).c_str());
            if (arg == tmp2) {
                auto *np = new SaveLangDialog(getScreen(), 90, 100, tmp2);
                np->raise();
            } else
                return ToplevelWindow::actionExecuted(b, arg);
        }
    }
};

/*********************************************************************************************************************/
/* UI control functions */

static void UI_Execute(GUI::ScreenSDL *screen) {
    SDL_Surface *sdlscreen;
    SDL_Event event;
    GUI::String configString = GUI::String(MSG_Get("CONFIG_TOOL"));

    sdlscreen = screen->getSurface();
    auto *cfg_wnd = new ConfigurationWindow(screen, 40, 10, configString);
    cfg_wnd->raise();

    // event loop
    while (running) {
        while (SDL_PollEvent(&event)) {
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
                                if (guiMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->msg.win.wParam)))
# else
                                if (guiMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->wParam)))
# endif
                                    return;
                            }
# endif
                            break;
                    }
                } break;
#endif
                default:
                    break;
            }

            if (!screen->event(event)) {
                if (event.type == SDL_QUIT) running = false;
            }
        }

        //Selecting keyboard will create a new surface.
        screen->watchTime();
        sdlscreen = screen->getSurface();

        if (background)
            SDL_BlitSurface(background, NULL, sdlscreen, NULL);
        else
            SDL_FillRect(sdlscreen, nullptr, 0);

        screen->update(screen->getTime());

#if defined(C_SDL2)
        SDL_Window* GFX_GetSDLWindow(void);
        SDL_UpdateWindowSurface(GFX_GetSDLWindow());
#else
        SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
#endif

        SDL_Delay(40);
    }
}

static void UI_Select(GUI::ScreenSDL *screen, int select) {
    SDL_Surface *sdlscreen = NULL;
    Section_line *section2 = NULL;
    Section_prop *section = NULL;
    Section *sec = NULL;
    SDL_Event event;
    GUI::String configString = GUI::String(MSG_Get("CONFIG_TOOL"));

    sdlscreen = screen->getSurface();
    switch (select) {
        case 0:
            new GUI::MessageBox2(screen, 200, 150, 280, "", "");
            running=false;
            break;
        case 1: {
            strcpy(tmp1, (MSG_Get("SAVE_CONFIGURATION")+std::string("...")).c_str());
            auto *np = new SaveDialog(screen, 90, 100, tmp1);
            np->raise();
            } break;
        case 2: {
            sec = control->GetSection("sdl");
            section=static_cast<Section_prop *>(sec); 
            auto *p = new SectionEditor(screen,50,30,section);
            p->raise();
            } break;
        case 3:
            sec = control->GetSection("dosbox");
            section=static_cast<Section_prop *>(sec); 
            new SectionEditor(screen,50,30,section);
            break;
        case 4:
            sec = control->GetSection("mixer");
            section=static_cast<Section_prop *>(sec); 
            new SectionEditor(screen,50,30,section);
            break;
        case 5:
            sec = control->GetSection("serial");
            section=static_cast<Section_prop *>(sec); 
            new SectionEditor(screen,50,30,section);
            break;
        case 6:
            sec = control->GetSection("ne2000");
            section=static_cast<Section_prop *>(sec); 
            new SectionEditor(screen,50,30,section);
            break;
        case 7:
            section2 = static_cast<Section_line *>(control->GetSection("autoexec"));
            new AutoexecEditor(screen, 50, 30, section2);
            break;
        case 8:
            sec = control->GetSection("glide");
            section=static_cast<Section_prop *>(sec); 
            new SectionEditor(screen,50,30,section);
            break;
        case 9: {
            strcpy(tmp1, (MSG_Get("SAVE_LANGUAGE")+std::string("...")).c_str());
            auto *np = new SaveLangDialog(screen, 90, 100, tmp1);
            np->raise();
            } break;
        case 10: {
            auto *np = new ConfigurationWindow(screen, 40, 10, configString);
            np->raise();
            } break;
        case 11:
            sec = control->GetSection("parallel");
            section=static_cast<Section_prop *>(sec);
            new SectionEditor(screen,50,30,section);
            break;
        case 12:
            sec = control->GetSection("printer");
            section=static_cast<Section_prop *>(sec);
            new SectionEditor(screen,50,30,section);
            break;
        case 13:
            sec = control->GetSection("cpu");
            section=static_cast<Section_prop *>(sec);
            new SectionEditor(screen,50,30,section);
            break;
        case 14:
            sec = control->GetSection("dos");
            section=static_cast<Section_prop *>(sec);
            new SectionEditor(screen,50,30,section);
            break;
        case 15:
            sec = control->GetSection("midi");
            section=static_cast<Section_prop *>(sec);
            new SectionEditor(screen,50,30,section);
            break;
        case 16: {
            auto *np1 = new SetCycles(screen, 90, 100, "Set CPU Cycles...");
            np1->raise();
            } break;
        case 17: {
            auto *np2 = new SetVsyncrate(screen, 90, 100, "Set vertical syncrate...");
            np2->raise();
            } break;
        case 18: {
            auto *np3 = new SetLocalSize(screen, 90, 100, "Set Default Local Freesize...");
            np3->raise();
            } break;
        case 19: {
            auto *np4 = new SetDOSVersion(screen, 90, 100, "Edit reported DOS version...");
            np4->raise();
            } break;
        case 20: {
            auto *np5 = new SetAspectRatio(screen, 90, 100, "Set aspect ratio...");
            np5->raise();
            } break;
        case 21: {
            auto *np6 = new SetTitleText(screen, 90, 130, mainMenu.get_item("set_titletext").get_text().c_str());
            np6->raise();
            } break;
        case 22: {
            auto *np6 = new SetTransparency(screen, 90, 100, mainMenu.get_item("set_transparency").get_text().c_str());
            np6->raise();
            } break;
        case 23: {
            auto *np7 = new ShowLoadWarning(screen, 150, 120, "DOSBox-X version mismatch. Load the state anyway?");
            np7->raise();
            } break;
        case 24: {
            auto *np7 = new ShowLoadWarning(screen, 150, 120, "Program name mismatch. Load the state anyway?");
            np7->raise();
            } break;
        case 25: {
            auto *np7 = new ShowLoadWarning(screen, 150, 120, "Memory size mismatch. Load the state anyway?");
            np7->raise();
            } break;
        case 26: {
            auto *np7 = new ShowLoadWarning(screen, 150, 120, "Machine type mismatch. Load the state anyway?");
            np7->raise();
            } break;
        case 27: {
            auto *np7 = new ShowLoadWarning(screen, 150, 120, "Are you sure to remove the state in this slot?");
            np7->raise();
            } break;
        case 28: {
            auto *np7 = new SetSensitivity(screen, 90, 100, "Set mouse sensitivity...");
            np7->raise();
            } break;
        case 29: {
            auto *np7 = new SetAutoSave(screen, 0, 0, "Auto-save settings...");
            np7->raise();
            } break;
        case 30: {
            auto *np7 = new SetRefreshRate(screen, 0, 0, mainMenu.get_item("refresh_rate").get_text().c_str());
            np7->raise();
            } break;
        case 31: if (statusdrive>-1 && statusdrive<DOS_DRIVES && Drives[statusdrive]) {
            auto *np8 = new ShowDriveInfo(screen, 120, 50, MSG_Get("DRIVE_INFORMATION"));
            np8->raise();
            } break;
        case 32: {
            auto *np9 = new ShowDriveNumber(screen, 110, 70, MSG_Get("MOUNTED_DRIVE_NUMBER"));
            np9->raise();
            } break;
        case 33: {
            auto *np10 = new ShowIDEInfo(screen, 150, 70, MSG_Get("IDE_CONTROLLER_ASSIGNMENT"));
            np10->raise();
            } break;
        case 34: {
            auto *np11 = new ShowHelpIntro(screen, 70, 70, mainMenu.get_item("help_intro").get_text().c_str());
            np11->raise();
            } break;
        case 35: {
            auto *np12 = new ShowHelpAbout(screen, 110, 70, mainMenu.get_item("help_about").get_text().c_str());
            np12->raise();
            } break;
        case 36: {
            auto *np13 = new ShowHelpCommand(screen, 150, 120, (MSG_Get("HELP_COMMAND")+std::string(": ")+helpcmd).c_str());
            np13->raise();
            } break;
        case 37: {
            auto *np14 = new MakeDiskImage(screen, 110, 70, MSG_Get("CREATE_IMAGE"));
            np14->raise();
            } break;
        case 38: {
            auto *np15 = new ShowHelpNIC(screen, 70, 70);
            np15->raise();
            } break;
        case 39: {
            auto *np15 = new ShowHelpPRT(screen, 70, 70);
            np15->raise();
            } break;
        case 40: {
            auto *np16 = new ShowMixerInfo(screen, 90, 70, MSG_Get("CURRENT_VOLUME"));
            np16->raise();
            } break;
        case 41: {
            auto *np16 = new ShowSBInfo(screen, 150, 100, MSG_Get("CURRENT_SBCONFIG"));
            np16->raise();
            } break;
        case 42: {
            auto *np16 = new ShowMidiDevice(screen, 150, 100, MSG_Get("CURRENT_MIDICONFIG"));
            np16->raise();
            } break;
#if C_DEBUG
        case 43: {
            npwin = new LogWindow(screen, 70, 70);
            npwin->raise();
            } break;
        case 44: {
            npwin = new CodeWindow(screen, 70, 70);
            npwin->raise();
            } break;
#endif
        default:
            break;
    }

    // event loop
    while (running) {
        while (SDL_PollEvent(&event)) {
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
                                if (guiMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->msg.win.wParam)))
# else
                                if (guiMenu.mainMenuWM_COMMAND((unsigned int)LOWORD(event.syswm.msg->wParam)))
# endif
                                    return;
                            }
# endif
                            break;
                    }
                } break;
#endif
                default:
                    break;
            }

            if (!screen->event(event)) {
                if (event.type == SDL_QUIT) running = false;
            }
        }

        if (background)
            SDL_BlitSurface(background, NULL, sdlscreen, NULL);
        else
            SDL_FillRect(sdlscreen, nullptr, 0);

        screen->update(4);
#if defined(C_SDL2)
        SDL_Window* GFX_GetSDLWindow(void);
        SDL_UpdateWindowSurface(GFX_GetSDLWindow());
#else   
        SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
#endif
        SDL_Delay(20);
    }
}

int sel = -1;
bool switchttf = false, gofs = false;
void RunCfgTool(Bitu val) {
    (void)val;//unused
    gofs=false;
#if defined(USE_TTF)
    if (!ttf.inUse && switchttf) {
        ttf_switch_on();
        if (sel==36&&!GFX_IsFullscreen()) {gofs=true;GFX_SwitchFullScreen();}
    }
    switchttf = false;
#endif
    GUI::ScreenSDL *screen = UI_Startup(NULL);
    if (sel<0)
        UI_Execute(screen);
    else
        UI_Select(screen,sel);
    UI_Shutdown(screen);
    delete screen;
    if (sel>-1) {
        if (gofs&&GFX_IsFullscreen()) GFX_SwitchFullScreen();
        gofs=false;
        toscale=true;
        shortcut=false;
        shortcutid=-1;
        statusdrive=-1;
        helpcmd = "";
        MAPPER_ReleaseAllKeys();
        GFX_LosingFocus();
        sel = -1;
    }
    if (GFX_GetPreventFullscreen()) {
#if C_OPENGL
        voodoo_ogl_update_dimensions();
#endif
    }
}

void GUI_Shortcut(int select) {
    if(!select || running) return;

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

    shortcutid=select;
    shortcut=true;
    sel = select;
#if defined(USE_TTF)
    if (ttf.inUse && !confres) {
        ttf_switch_off();
        GFX_EndUpdate(nullptr);
        switchttf = true;
        PIC_AddEvent(RunCfgTool, 100);
    } else
#endif
    RunCfgTool(0);
}

void GUI_Run(bool pressed) {
    if (pressed || running) return;

    sel = -1;
#if defined(USE_TTF)
    if (ttf.inUse) {
        ttf_switch_off();
        GFX_EndUpdate(nullptr);
        switchttf = true;
        PIC_AddEvent(RunCfgTool, 100);
    } else
#endif
    RunCfgTool(0);
}
