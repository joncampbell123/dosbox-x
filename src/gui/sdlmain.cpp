/** \mainpage DOSBox-X emulation
 *
 * \section i Introduction
 *
 * \section f Features
 *
 * \li Complete and accurate x86/DOS emulation
 *
*/

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

#ifdef WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
#endif

#ifdef OS2
# define INCL_DOS
# define INCL_WIN
#endif

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

int socknum=-1;
int posx = -1;
int posy = -1;
int initgl = 0;
int transparency=0;
int selsrow = -1, selscol = -1;
int selerow = -1, selecol = -1;
int middleunlock = 1;
unsigned int maincp = 0;
extern bool testerr;
extern bool blinking;
extern bool log_int21;
extern bool log_fileio;
extern bool ticksLocked;
extern bool isJPkeyboard;
extern bool enable_autosave;
extern bool use_quick_reboot;
extern bool showdbcs, skipdraw;
extern bool noremark_save_state;
extern bool ctrlbrk, dpi_aware_enable;
extern bool force_load_state, force_conversion;
extern bool pc98_force_ibm_layout, gbk, chinasea;
extern bool inshell, enable_config_as_shell_commands;
extern bool switchttf, ttfswitch, switch_output_from_ttf;
bool checkmenuwidth = false;
bool dos_kernel_disabled = true;
bool winrun=false, use_save_file=false;
bool maximize = false, tooutttf = false;
bool tonoime = false, enableime = false;
bool usesystemcursor = false, rtl = false, selmark = false;
bool mountfro[26], mountiro[26];
bool OpenGL_using(void), Direct3D_using(void);
void DOSBox_SetSysMenu(void), GFX_OpenGLRedrawScreen(void), InitFontHandle(void), DOSV_FillScreen(void), refreshExtChar(void), Add_VFiles(bool usecp), SetAlpha(double alpha), SetWindowTransparency(int trans);
void MenuBrowseProgramFile(void), OutputSettingMenuUpdate(void), aspect_ratio_menu(void), update_pc98_clock_pit_menu(void), AllocCallback1(void), AllocCallback2(void), ToggleMenu(bool pressed);
extern int tryconvertcp, Reflect_Menu(void);

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

// Tell macOS to shut up about deprecated OpenGL calls
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <algorithm> // std::transform
#include <fcntl.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#ifdef WIN32
# include <signal.h>
# include <process.h>
# if !defined(__MINGW32__) /* MinGW does not have these headers */
#  include <shcore.h>
#  include <shellscalingapi.h>
# endif
#endif

#include "control.h"
#include "dosbox.h"
#include "menudef.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "bios.h"
#include "callback.h"
#include "support.h"
#include "debug.h"
#include "ide.h"
#include "bitop.h"
#include "ptrop.h"
#include "mapper.h"
#include "sdlmain.h"
#include "zipfile.h"
#include "glidedef.h"
#include "bios_disk.h"
#include "inout.h"
#include "jfont.h"
#include "render.h"
#include "../dos/cdrom.h"
#include "../dos/drives.h"
#include "../ints/int10.h"
#if !defined(HX_DOS)
#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include "whereami.c"
#endif
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#endif
#if C_DEBUG
#include "display2.cpp"
#endif

#if (defined __i386__ || defined __x86_64__) && (defined BSD || defined LINUX)
#include "libs/passthroughio/passthroughio.h" // for dropPrivileges()
#endif

#if defined(LINUX) && defined(HAVE_ALSA)
# include <alsa/asoundlib.h>
#endif

#if defined(WIN32) && !defined(HX_DOS)
# include <shobjidl.h>
#endif

#include <output/output_direct3d.h>
#include <output/output_opengl.h>
#include <output/output_surface.h>
#include <output/output_tools.h>
#include <output/output_ttf.h>
#include <output/output_tools_xbrz.h>
static bool init_output = false;

#if defined(WIN32)
#include "resource.h"
#if !defined(HX_DOS)

BOOL CALLBACK EnumDispProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM lParam) {
    (void)hMon;
    (void)dcMon;
	xyp* xy = reinterpret_cast<xyp*>(lParam);
	curscreen++;
	if (sdl.displayNumber==curscreen) monrect=*pRcMon;
	return TRUE;

}
#endif
#endif

#if defined(WIN32) && defined(__MINGW32__) /* MinGW does not have IID_ITaskbarList3 */
/* MinGW now contains this, the older MinGW for HX-DOS does not.
 * Keep things simple and just #define around it like this */
static const GUID __my_CLSID_TaskbarList ={ 0x56FDF344,0xFD6D,0x11d0,{0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90}};
# define CLSID_TaskbarList __my_CLSID_TaskbarList

static const GUID __my_IID_ITaskbarList3 = { 0xEA1AFB91ul,0x9E28u,0x4B86u,0x90u,0xE9u,0x9Eu,0x9Fu,0x8Au,0x5Eu,0xEFu,0xAFu };
# define IID_ITaskbarList3 __my_IID_ITaskbarList3
#endif

#if defined(WIN32) && defined(__MINGW32__) /* MinGW does not have this */
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE             = 0,
    PROCESS_SYSTEM_DPI_AWARE        = 1,
    PROCESS_PER_MONITOR_DPI_AWARE   = 2
} PROCESS_DPI_AWARENESS;
#endif

#if C_EMSCRIPTEN
# include <emscripten.h>
#endif

#include "../src/libs/gui_tk/gui_tk.h"

#ifdef __WIN32__
# include "callback.h"
# include "dos_inc.h"
# include <malloc.h>
# include "Commdlg.h"
# include "windows.h"
# include "Shellapi.h"
# include <cstring>
# include <fstream>
# if defined(__MINGW32__) && !defined(HX_DOS)
#  include <imm.h> // input method editor
# endif
#endif // WIN32

#include <sstream>

#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "fpu.h"
#include "cross.h"
#include "keymap.h"
#include "voodoo.h"
#if C_OPENGL
#include "../hardware/voodoo_types.h"
#include "../hardware/voodoo_data.h"
#include "../hardware/voodoo_opengl.h"
#endif

#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void sdl1_hax_macosx_highdpi_set_enable(const bool enable);
#endif

# include "SDL_version.h"
#if !defined(C_SDL2) && !defined(RISCOS)
# ifndef SDL_DOSBOX_X_SPECIAL
#  warning It is STRONGLY RECOMMENDED to compile the DOSBox-X code using the SDL 1.x library provided in this source repository.
#  error You can ignore this by commenting out this error, but you will encounter problems if you use the unmodified SDL 1.x library.
# endif
#endif

#include "sdlmain.h"
#include "build_timestamp.h"

#if C_OPENGL
namespace gl2 {
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLSHADERSOURCEPROC_NP glShaderSource;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
}

#define glAttachShader            gl2::glAttachShader
#define glCompileShader           gl2::glCompileShader
#define glCreateProgram           gl2::glCreateProgram
#define glCreateShader            gl2::glCreateShader
#define glDeleteProgram           gl2::glDeleteProgram
#define glDeleteShader            gl2::glDeleteShader
#define glEnableVertexAttribArray gl2::glEnableVertexAttribArray
#define glGetAttribLocation       gl2::glGetAttribLocation
#define glGetProgramiv            gl2::glGetProgramiv
#define glGetProgramInfoLog       gl2::glGetProgramInfoLog
#define glGetShaderiv             gl2::glGetShaderiv
#define glGetShaderInfoLog        gl2::glGetShaderInfoLog
#define glGetUniformLocation      gl2::glGetUniformLocation
#define glLinkProgram             gl2::glLinkProgram
#define glShaderSource            gl2::glShaderSource
#define glUniform2f               gl2::glUniform2f
#define glUniform1i               gl2::glUniform1i
#define glUseProgram              gl2::glUseProgram
#define glVertexAttribPointer     gl2::glVertexAttribPointer
#endif

#ifdef MACOSX
#include <CoreGraphics/CoreGraphics.h>
extern bool has_touch_bar_support;
bool macosx_detect_nstouchbar(void);
void macosx_init_touchbar(void);
void macosx_GetWindowDPI(ScreenSizeInfo &info);
int macosx_yesno(const char *title, const char *message);
int macosx_yesnocancel(const char *title, const char *message);
std::string macosx_prompt_folder(const char *default_folder);
#endif

#if C_DIRECT3D
void d3d_init(void);
#endif
void ShutDownMemHandles(Section * sec), GFX_ReleaseMouse();
void resetFontSize(), increaseFontSize(), decreaseFontSize();
void GetMaxWidthHeight(unsigned int *pmaxWidth, unsigned int *pmaxHeight);
void MAPPER_CheckEvent(SDL_Event * event), MAPPER_CheckKeyboardLayout(), MAPPER_ReleaseAllKeys();
bool isDBCSCP(), InitCodePage();

SDL_Block sdl;
Bitu frames = 0;
unsigned int page=0;
unsigned int hostkeyalt=0;
unsigned int sendkeymap=0;
std::string configfile = "";
std::string strPasteBuffer = "";
ScreenSizeInfo screen_size_info;
void FormFeed(bool pressed), PrintText(bool pressed);
void DOSBOX_UnlockSpeed2(bool pressed), DEBUG_Enable_Handler(bool pressed);
int FileDirExistCP(const char *name), FileDirExistUTF8(std::string &localname, const char *name);
bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);

#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && defined(C_SDL2)
static std::string ime_text = "";
extern bool CodePageHostToGuestUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
extern bool IME_GetEnable();
#endif

int NonUserResizeCounter = 0;

#if defined(WIN32) && !defined(HX_DOS)
enum class CornerPreference {
    Default    = 0,
    DoNotRound = 1,
    Round      = 2,
    RoundSmall = 3,
};
bool UpdateWindows11RoundCorners(HWND hWnd, CornerPreference cornerPreference) {
    typedef HRESULT(WINAPI *PFNSETWINDOWATTRIBUTE)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
    enum DWMWINDOWATTRIBUTE {
        DWMWA_WINDOW_CORNER_PREFERENCE = 33
    };
    enum DWM_WINDOW_CORNER_PREFERENCE {
        DWMWCP_DEFAULT    = 0,
        DWMWCP_DONOTROUND = 1,
        DWMWCP_ROUND      = 2,
        DWMWCP_ROUNDSMALL = 3
    };
    HMODULE hDwmApi = ::LoadLibrary("dwmapi.dll");
    if (hDwmApi) {
        auto *pfnSetWindowAttribute = reinterpret_cast<PFNSETWINDOWATTRIBUTE>(GetProcAddress(hDwmApi, "DwmSetWindowAttribute"));
        if (pfnSetWindowAttribute) {
            auto preference = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(cornerPreference);
            HRESULT res = pfnSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(DWM_WINDOW_CORNER_PREFERENCE));
            return res == S_OK;
        }
        ::FreeLibrary(hDwmApi);
    }
    return false;
}
#endif

#if defined(WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
    switch (event) {
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
        raise(SIGTERM);
        return TRUE;
    case CTRL_C_EVENT:
    default: //pass to the next handler
        return FALSE;
    }
}
#endif

extern int bootdrive, resolveopt;
extern int dos_clipboard_device_access;
extern int aspect_ratio_x, aspect_ratio_y;
extern bool sync_time, loadlang, addovl;
extern bool bootguest, bootfast, bootvm;

std::string dosboxpath="";
std::string GetDOSBoxXPath(bool withexe=false) {
    std::string full;
#if defined(HX_DOS) || defined(_WIN32_WINDOWS) /*wai_* is problematic */ || defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
    char exepath[MAX_PATH];
    GetModuleFileName(NULL, exepath, sizeof(exepath));
    full=std::string(exepath);
#else
    int length = wai_getExecutablePath(NULL, 0, NULL);
    char *exepath = (char*)malloc(length + 1);
    wai_getExecutablePath(exepath, length, NULL);
    exepath[length] = 0;
    full=std::string(exepath);
    free(exepath);
#endif
    if (withexe)
        dosboxpath=full;
    else {
        size_t found=full.find_last_of("/\\");
        if (found!=std::string::npos)
            dosboxpath=full.substr(0, found+1);
        else
            dosboxpath="";
    }
    return dosboxpath;
}

#if defined(WIN32) && !defined(C_SDL2)
bool isVirtualBox = false; /* OpenGL never works with Windows XP inside VirtualBox */
#endif

bool OpenGL_using(void);

#if defined(WIN32) && !defined(S_ISREG)
# define S_ISREG(x) ((x & S_IFREG) == S_IFREG)
#endif

using namespace std;

void UpdateOverscanMenu(void);

const char *DKM_to_string(const unsigned int dkm) {
    switch (dkm) {
        case DKM_US:        return "us";
        case DKM_DEU:       return "ger";
        case DKM_JPN_PC98:  return "jpn_pc98";
        case DKM_JPN:       return "jpn";
        default:            break;
    }

    return "";
}

const char *DKM_to_descriptive_string(const unsigned int dkm) {
    switch (dkm) {
        case DKM_US:        return "US English";
        case DKM_DEU:       return "German";
        case DKM_JPN_PC98:  return "Japanese (PC-98)";
        case DKM_JPN:       return "Japanese";
        default:            break;
    }

    return "";
}

#if defined(WIN32) && !defined(HX_DOS)
ITaskbarList3 *winTaskbarList = NULL;
#endif

#if defined(WIN32) && !defined(HX_DOS)
void WindowsTaskbarUpdatePreviewRegion(void) {
    if (winTaskbarList != NULL) {
        /* Windows 7/8/10: Tell the taskbar which part of our window contains the DOS screen */
        RECT r;

        r.top = sdl.clip.y;
        r.left = sdl.clip.x;
        r.right = sdl.clip.x + sdl.clip.w;
        r.bottom = sdl.clip.y + sdl.clip.h;

        /* NTS: The MSDN documentation is misleading. Apparently, despite 30+ years of Windows SDK
                behavior where the "client area" is the area below the menu bar and inside the frame,
                ITaskbarList3's idea of the "client area" is the the area inside the frame INCLUDING
                the menu bar. Why? */
        if (GetMenu(GetHWND()) != NULL) {
            MENUBARINFO mb;
            int rh;

            memset(&mb, 0, sizeof(mb));
            mb.cbSize = sizeof(mb);

            GetMenuBarInfo(GetHWND(), OBJID_MENU, 0, &mb); // returns absolute screen coordinates, apparently.
            rh = mb.rcBar.bottom + 1 - mb.rcBar.top; // menu screen space is top <= y <= bottom, inclusive.

            r.top += rh;
            r.bottom += rh;
        }

        if (winTaskbarList->SetThumbnailClip(GetHWND(), &r) != S_OK)
            LOG_MSG("WARNING: ITaskbarList3::SetThumbnailClip() failed");
    }
}

void WindowsTaskbarResetPreviewRegion(void) {
    if (winTaskbarList != NULL) {
        /* Windows 7/8/10: Tell the taskbar which part of our window contains the client area (not including the menu bar) */
        RECT r;

        GetClientRect(GetHWND(), &r);

        /* NTS: The MSDN documentation is misleading. Apparently, despite 30+ years of Windows SDK
                behavior where the "client area" is the area below the menu bar and inside the frame,
                ITaskbarList3's idea of the "client area" is the the area inside the frame INCLUDING
                the menu bar. Why? */
        if (GetMenu(GetHWND()) != NULL) {
            MENUBARINFO mb;
            int rh;

            memset(&mb, 0, sizeof(mb));
            mb.cbSize = sizeof(mb);

            GetMenuBarInfo(GetHWND(), OBJID_MENU, 0, &mb); // returns absolute screen coordinates, apparently.
            rh = mb.rcBar.bottom + 1 - mb.rcBar.top; // menu screen space is top <= y <= bottom, inclusive.

            r.top += rh;
            r.bottom += rh;
        }

        if (winTaskbarList->SetThumbnailClip(GetHWND(), &r) != S_OK)
            LOG_MSG("WARNING: ITaskbarList3::SetThumbnailClip() failed");
    }
}
#endif

unsigned int mapper_keyboard_layout = DKM_US;
unsigned int host_keyboard_layout = DKM_US;

void KeyboardLayoutDetect(void) {
    unsigned int nlayout = DKM_US;

#if defined(LINUX)
    unsigned int Linux_GetKeyboardLayout(void);
    nlayout = Linux_GetKeyboardLayout();

    /* BUGFIX: The xkbmap for 'jp' in Linux/X11 has a problem that maps both
     *         Ro and Yen to backslash, which in SDL's default state makes
     *         it impossible to map them properly in the mapper. */
    if (nlayout == DKM_JPN) {
        LOG_MSG("Engaging Linux/X11 fix for jp xkbmap in order to handle Ro/Yen keys");

        void Linux_JPXKBFix(void);
        Linux_JPXKBFix();
    }
#elif defined(WIN32)
    WORD lid = LOWORD(GetKeyboardLayout(0));

    LOG_MSG("Windows keyboard layout ID is 0x%04x", lid);

    switch (lid) {
        case 0x0407:    nlayout = DKM_DEU; break;
        case 0x0409:    nlayout = DKM_US; break;
        case 0x0411:    nlayout = DKM_JPN; break;
        default:        break;
    }
#endif

    host_keyboard_layout = nlayout;

    LOG_MSG("Host keyboard layout is now %s (%s)",
        DKM_to_string(host_keyboard_layout),
        DKM_to_descriptive_string(host_keyboard_layout));

    if (IS_PC98_ARCH) {
        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        const char *layoutstr = pc98_section->Get_string("pc-98 force ibm keyboard layout");
        if (!strcasecmp(layoutstr, "auto")) {
            pc98_force_ibm_layout = host_keyboard_layout == DKM_US;
            mainMenu.get_item("pc98_use_uskb").check(pc98_force_ibm_layout).refresh_item(mainMenu);
        }
    }
}

void SetMapperKeyboardLayout(const unsigned int dkm) {
    /* TODO: Make mapper re-initialize layout. If the mapper interface is visible, redraw it. */
    mapper_keyboard_layout = dkm;

    LOG_MSG("Mapper keyboard layout is now %s (%s)",
        DKM_to_string(mapper_keyboard_layout),
        DKM_to_descriptive_string(mapper_keyboard_layout));
}

#if defined(WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" unsigned char SDL1_hax_hasLayoutChanged(void);
extern "C" void SDL1_hax_ackLayoutChanged(void);
#endif

void CheckMapperKeyboardLayout(void) {
#if defined(WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    if (SDL1_hax_hasLayoutChanged()) {
        SDL1_hax_ackLayoutChanged();
        LOG_MSG("Keyboard layout changed");
        KeyboardLayoutDetect();

        if (host_keyboard_layout == DKM_JPN && IS_PC98_ARCH)
            SetMapperKeyboardLayout(DKM_JPN_PC98);
        else
            SetMapperKeyboardLayout(host_keyboard_layout);
    }
#endif
}

/* yksoft1 says that older MinGW headers lack this value --Jonathan C. */
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

bool boot_debug_break = false;

bool window_was_maximized = false;

/* this flag is needed in order to know if we're AT the shell,
   or if we're in a program running under the shell. */
bool dos_shell_running_program = false;

Bitu userResizeWindowWidth = 0, userResizeWindowHeight = 0;
Bitu currentWindowWidth = 640, currentWindowHeight = 480;

bool setSizeButNotResize() {
    return (userResizeWindowWidth > 0 && userResizeWindowHeight > 0);
}

Bitu time_limit_ms = 0;

extern bool keep_umb_on_boot;
extern bool keep_private_area_on_boot;
bool guest_machine_power_on = false;

std::string custom_savedir;

void SHELL_Run();
void DisableINT33();
void EMS_DoShutDown();
void XMS_DoShutDown();
void DOS_DoShutDown();
void GUS_DOS_Shutdown();
void SBLASTER_DOS_Shutdown();
void DOS_ShutdownDevices(void);
void RemoveEMSPageFrame(void);
void RemoveUMBBlock();
void DOS_GetMemory_unmap();
void VFILE_Shutdown(void);
void PROGRAMS_Shutdown(void);
void DOS_UninstallMisc(void);
void CALLBACK_Shutdown(void);
void DOS_ShutdownDrives();
void VFILE_Shutdown(void);
void DOS_ShutdownFiles();
void FreeBIOSDiskList();
void GFX_ShutDown(void);
void MAPPER_Shutdown();
void SHELL_Init(void);
void CopyClipboard(int all);
void CopyAllClipboard(bool bPressed);
void PasteClipboard(bool bPressed);
void PasteClipStop(bool bPressed);
void QuickEdit(bool bPressed);
void ClipKeySelect(int sym);
bool isModifierApplied(void);
bool PasteClipboardNext(void);

#if C_DYNAMIC_X86
void CPU_Core_Dyn_X86_Shutdown(void);
#endif

void UpdateWindowMaximized(bool flag) {
    menu.maxwindow = flag;
}

void UpdateWindowDimensions(Bitu width, Bitu height)
{
    currentWindowWidth = width;
    currentWindowHeight = height;
}

void PrintScreenSizeInfo(void) {
#if 1
    const char *method = "?";

    switch (screen_size_info.method) {
        case METHOD_NONE:       method = "None";        break;
        case METHOD_X11:        method = "X11";         break;
        case METHOD_XRANDR:     method = "XRandR";      break;
        case METHOD_WIN98BASE:  method = "Win98base";   break;
        case METHOD_COREGRAPHICS:method = "CoreGraphics";break;
        default:                                                        break;
    }

    LOG_MSG("Screen report: Method '%s' (%.3f x %.3f pixels) at (%.3f x %.3f) (%.3f x %.3f mm) (%.3f x %.3f in) (%.3f x %.3f DPI)",
            method,

            screen_size_info.screen_dimensions_pixels.width,
            screen_size_info.screen_dimensions_pixels.height,

            screen_size_info.screen_position_pixels.x,
            screen_size_info.screen_position_pixels.y,

            screen_size_info.screen_dimensions_mm.width,
            screen_size_info.screen_dimensions_mm.height,

            screen_size_info.screen_dimensions_mm.width / 25.4,
            screen_size_info.screen_dimensions_mm.height / 25.4,

            screen_size_info.screen_dpi.width,
            screen_size_info.screen_dpi.height);
#endif
}

#if defined(WIN32)
void Windows_GetWindowDPI(ScreenSizeInfo &info) {
    info.clear();

# if !defined(HX_DOS)
    HMONITOR mon;
    HWND hwnd;

    info.method = METHOD_WIN98BASE;

    hwnd = GetHWND();
    if (hwnd == NULL) return;

    mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (mon == NULL) mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (mon == NULL) return;

    MONITORINFO mi;
    memset(&mi,0,sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(mon,&mi)) return;

    info.screen_position_pixels.x        = mi.rcMonitor.left;
    info.screen_position_pixels.y        = mi.rcMonitor.top;

    info.screen_dimensions_pixels.width  = mi.rcMonitor.right - mi.rcMonitor.left;
    info.screen_dimensions_pixels.height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    /* Windows 10 build 1607 and later offer a "Get DPI of window" function */
    {
        HMODULE __user32;

        __user32 = GetModuleHandle("USER32.DLL");
        if (__user32) {
            UINT (WINAPI *__GetDpiForWindow)(HWND) = NULL;

            __GetDpiForWindow = (UINT (WINAPI *)(HWND))GetProcAddress(__user32,"GetDpiForWindow");
            if (__GetDpiForWindow) {
                UINT dpi = __GetDpiForWindow(hwnd);

                if (dpi != 0) {
                    info.screen_dpi.width = dpi;
                    info.screen_dpi.height = dpi;

                    info.screen_dimensions_mm.width = (25.4 * screen_size_info.screen_dimensions_pixels.width) / dpi;
                    info.screen_dimensions_mm.height = (25.4 * screen_size_info.screen_dimensions_pixels.height) / dpi;
                }
            }
        }
    }
# endif
}
#endif

void UpdateWindowDimensions(void)
{
#if defined(C_SDL2)
    int w = 640,h = 480;
    SDL_GetWindowSize(sdl.window, &w, &h);
    UpdateWindowDimensions(w,h);

    Uint32 fl = SDL_GetWindowFlags(sdl.window);
    UpdateWindowMaximized((fl & SDL_WINDOW_MAXIMIZED) != 0);
#endif
#if defined(MACOSX)
    macosx_GetWindowDPI(/*&*/screen_size_info);
#endif
#if defined(WIN32)
    // When maximized, SDL won't actually tell us our new dimensions, so get it ourselves.
    // FIXME: Instead of GetHWND() we need to track our own handle or add something to SDL 1.x
    //        to provide the handle!
    RECT r = { 0 };

    GetClientRect(GetHWND(), &r);
    UpdateWindowDimensions(r.right, r.bottom);
    UpdateWindowMaximized(IsZoomed(GetHWND()));
    Windows_GetWindowDPI(/*&*/screen_size_info);
#endif
#if defined(LINUX)
    void UpdateWindowDimensions_Linux(void);
    UpdateWindowDimensions_Linux();
    void Linux_GetWindowDPI(ScreenSizeInfo &info);
    Linux_GetWindowDPI(/*&*/screen_size_info);
#endif
    PrintScreenSizeInfo();
}

#define MAPPERFILE              "mapper-dosbox-x.map"
#define MAPPERFILE_SDL1         "mapper-dosbox-x.sdl1.map"
#define MAPPERFILE_SDL2         "mapper-dosbox-x.sdl2.map"

void                        GUI_ResetResize(bool);
void                        GUI_LoadFonts();
void                        GUI_Run(bool);

const char*                 titlebar = NULL;
extern const char*          RunningProgram;
extern bool                 CPU_CycleAutoAdjust;
extern                      cpu_cycles_count_t CPU_CyclePercUsed;
#if !(ENVIRON_INCLUDED)
extern char**                   environ;
#endif

double                      rtdelta = 0;
bool                        emu_paused = false;
bool                        mouselocked = false; //Global variable for mapper
bool                        fullscreen_switch = true;
bool                        startup_state_numlock = false; // Global for keyboard initialisation
bool                        startup_state_capslock = false; // Global for keyboard initialisation
bool                        startup_state_scrlock = false; // Global for keyboard initialisation
int mouse_start_x=-1, mouse_start_y=-1, mouse_end_x=-1, mouse_end_y=-1, fx=-1, fy=-1, paste_speed=20, wheel_key=0, mbutton=3;
bool wheel_guest = false, clipboard_dosapi = true, clipboard_biospaste =
#if defined (WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
false;
#else
true;
#endif
const char *modifier;

#ifdef WIN32
# define STDOUT_FILE                TEXT("stdout.txt")
# define STDERR_FILE                TEXT("stderr.txt")
# define DEFAULT_CONFIG_FILE            "/dosbox-x.conf"
#elif defined(MACOSX)
# define DEFAULT_CONFIG_FILE            "/Library/Preferences/DOSBox Preferences"
#elif defined(HAIKU)
#define DEFAULT_CONFIG_FILE             "~/config/settings/dosbox-x/dosbox-x.conf"
#else /*linux freebsd*/
# define DEFAULT_CONFIG_FILE            "/.dosboxrc"
#endif

#if C_SET_PRIORITY
# include <sys/resource.h>
# define PRIO_TOTAL             (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
# include <os2.h>
#endif

#if defined(WIN32)
HWND GetHWND(void) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
# if defined(C_SDL2)
    if (sdl.window == NULL)
        return nullptr;
    if (!SDL_GetWindowWMInfo(sdl.window, &wmi))
        return nullptr;
    return wmi.info.win.window;
#else
    if(!SDL_GetWMInfo(&wmi)) {
        return NULL;
    }
    return wmi.window;
#endif
}

HWND GetSurfaceHWND(void) {
# if defined(C_SDL2)
    return GetHWND();
# else
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);

    if (!SDL_GetWMInfo(&wmi)) {
        return NULL;
    }
    return wmi.child_window;
# endif
}
#endif

void SDL_rect_cliptoscreen(SDL_Rect &r) {
    if (r.x < 0) {
        r.w += r.x;
        r.x = 0;
    }
    if (r.y < 0) {
        r.h += r.y;
        r.y = 0;
    }
    if ((r.x+r.w) > sdl.surface->w)
        r.w = sdl.surface->w - r.x;
    if ((r.y+r.h) > sdl.surface->h)
        r.h = sdl.surface->h - r.y;
    /* NTS: Apparently r.w and r.h are unsigned, therefore no need to check if negative */
//    if (r.w < 0) r.w = 0;
//    if (r.h < 0) r.h = 0;
}

Bitu GUI_JoystickCount(void) {
    return sdl.num_joysticks;
}

#if !defined(MACOSX)
/* TODO: should move to it's own file ================================================ */
static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};
#endif

#if !defined(MACOSX)
static void DOSBox_SetOriginalIcon(void) {
    SDL_Surface *logos;

#ifdef WORDS_BIGENDIAN
        logos = SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
        logos = SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif

#if defined(C_SDL2)
        SDL_SetWindowIcon(sdl.window, logos);
#else
        SDL_WM_SetIcon(logos,NULL);
#endif
}
#endif
/* =================================================================================== */

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void) {
    return sdl.using_windib;
}
#endif

void GFX_SetIcon(void) 
{
#if !defined(MACOSX)
    /* Set Icon (must be done before any sdl_setvideomode call) */
    /* But don't set it on macOS, as we use a nicer external icon there. */
    /* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
    if (menu_compatible) { DOSBox_SetOriginalIcon(); return; }
#endif

#if defined(WIN32) && !defined(C_SDL2)
    HICON hIcon1;

    hIcon1 = (HICON) LoadImage( GetModuleHandle(NULL), MAKEINTRESOURCE(dosbox_ico), IMAGE_ICON,
        16,16,LR_DEFAULTSIZE);

    SendMessage(GetHWND(), WM_SETICON, ICON_SMALL, (LPARAM) hIcon1 ); 
#endif
}

#if C_DEBUG
bool IsDebuggerActive(void);
#endif

extern std::string dosbox_title;

void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused) {
    (void)frameskip;//UNUSED
    (void)timing;//UNUSED
//  static Bits internal_frameskip=0;
    static int32_t internal_cycles=0;
//  static Bits internal_timing=0;
    char title[250] = {0};

    Section_prop *section = static_cast<Section_prop *>(control->GetSection("SDL"));
    assert(section != NULL);
    titlebar = section->Get_string("titlebar");

    if (cycles != -1) internal_cycles = cycles;
//  if (timing != -1) internal_timing = timing;
//  if (frameskip != -1) internal_frameskip = frameskip;

    bool showbasic = section->Get_bool("showbasic");
    if (showbasic) {
        sprintf(title,"%s%sDOSBox-X %s", dosbox_title.c_str(),dosbox_title.empty()?"":" - ", VERSION);

        const char *what = RunningProgram;
        if (what != NULL && *what != 0) {
            char *p = title + strlen(title); // append to end of string

            sprintf(p,": %s - ", what);
        }

        char *p = title + strlen(title); // append to end of string
        if (CPU_CycleAutoAdjust && menu.hidecycles && !menu.showrt)
            sprintf(p,"%d%%", (int)internal_cycles);
        else
            sprintf(p,"%d cycles/ms", (int)internal_cycles);
    } else
        sprintf(title,"%s%sDOSBox-X", dosbox_title.c_str(),dosbox_title.empty()?"":" - ");

    if (!menu.hidecycles) {
        char *p = title + strlen(title); // append to end of string

        sprintf(p,", FPS %2d",(int)frames);
    }

    if (menu.showrt) {
        char *p = title + strlen(title); // append to end of string

        sprintf(p,", %2d%%/RT",(int)floor((rtdelta / 10) + 0.5));
    }

    if (titlebar != NULL && *titlebar != 0) {
        char *p = title + strlen(title); // append to end of string

        sprintf(p,": %s",titlebar);
    }

    if (sdl.mouse.locked) {
        std::string get_mapper_shortcut(const char *name);
        std::string key=get_mapper_shortcut("capmouse");
        strcat(title, key.size()?(" ["+key+" releases mouse]").c_str():" [mouse locked]");
    }

    if (paused) strcat(title," PAUSED");
#if C_DEBUG
    if (IsDebuggerActive()) strcat(title," DEBUGGER");
#endif
#if defined(C_SDL2)
    SDL_SetWindowTitle(sdl.window,title);
#else
    SDL_WM_SetCaption(title,VERSION);
#endif
}

bool warn_on_mem_write = false;

bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton) {
#if !defined(HX_DOS)
    bool fs=sdl.desktop.fullscreen;
    if (fs) GFX_SwitchFullScreen();
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    bool ret=tinyfd_messageBox(aTitle, aMessage, aDialogType, aIconType, aDefaultButton);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    if (fs&&!sdl.desktop.fullscreen) GFX_SwitchFullScreen();
    return ret;
#else
    return true;
#endif
}

bool CheckQuit(void) {
#if !defined(HX_DOS)
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
    std::string warn = section->Get_string("quit warning");
    bool quit = section->Get_bool("allow quit after warning");
    if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
    if (warn == "true") {
        if (!quit) {
            systemmessagebox("Quit DOSBox-X warning","Quitting from DOSBox-X with this is currently disabled.","ok", "warning", 1);
            return false;
        } else
            return systemmessagebox("Quit DOSBox-X warning","This will quit from DOSBox-X.\nAre you sure?","yesno", "question", 1);
    } else if (warn == "false")
        return true;
    if (dos_kernel_disabled&&strcmp(RunningProgram, "DOSBOX-X")) {
        if (!quit) {
            systemmessagebox("Quit DOSBox-X warning","You cannot quit DOSBox-X while running a guest system.","ok", "warning", 1);
            return false;
        } else
            return systemmessagebox("Quit DOSBox-X warning","You are currently running a guest system.\nAre you sure to quit anyway now?","yesno", "question", 1);
    }
    if (warn == "autofile")
        for (uint8_t handle = 0; handle < DOS_FILES; handle++) {
            if (Files[handle] && (Files[handle]->GetName() == NULL || strcmp(Files[handle]->GetName(), "CON")) && (Files[handle]->GetInformation()&DeviceInfoFlags::Device) == 0) {
                if (!quit) {
                    systemmessagebox("Quit DOSBox-X warning","You cannot quit DOSBox-X while one or more files are open.","ok", "warning", 1);
                    return false;
                } else
                    return systemmessagebox("Quit DOSBox-X warning","It may be unsafe to quit from DOSBox-X right now\nbecause one or more files are currently open.\nAre you sure to quit anyway now?","yesno", "question", 1);
            }
        }
    else if (RunningProgram&&strcmp(RunningProgram, "DOSBOX-X")&&strcmp(RunningProgram, "COMMAND")&&strcmp(RunningProgram, "4DOS")) {
        if (!quit) {
            systemmessagebox("Quit DOSBox-X warning","You cannot quit DOSBox-X while running a program or game.","ok", "warning", 1);
            return false;
        } else
            return systemmessagebox("Quit DOSBox-X warning","You are currently running a program or game.\nAre you sure to quit anyway now?","yesno", "question", 1);
    }
#endif
    return true;
}

void NewInstanceEvent(bool pressed) {
    if (!pressed) return;
#if defined(MACOSX)
    pid_t p = fork();
    if (p == 0) {
        /* child process */
        char *argv[8];
        extern std::string MacOSXEXEPath;
        {
            int fd = open("/dev/null",O_RDWR);
            dup2(fd,0);
            dup2(fd,1);
            dup2(fd,2);
            close(fd);
        }
        chdir("/");
        argv[0] = (char*)MacOSXEXEPath.c_str();
        argv[1] = NULL;
        execv(argv[0],argv);
        fprintf(stderr,"Failed to exec to %s\n",argv[0]);
        _exit(1);
    }
#endif
}

void CPU_Snap_Back_To_Real_Mode();
static void KillSwitch(bool pressed) {
    if (!pressed) return;
    if (!CheckQuit()) return;
    if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
#if 0 /* Re-enable this hack IF DOSBox-X continues to have problems page-faulting on kill switch */
    CPU_Snap_Back_To_Real_Mode(); /* TEMPORARY HACK. There are portions of DOSBox that write to memory as if still running DOS. */
    /* ^ Without this hack, when running Windows NT 3.1 this Kill Switch effectively becomes the Instant Page Fault BSOD switch
     * because the DOSBox-X code attempting to write to real mode memory causes a page fault (though hitting the kill switch a
     * second time shuts DOSBox-X down properly). It's sort of the same issue behind the INT 33h emulation causing instant BSOD
     * in Windows NT the instant you moved or clicked the mouse. The purpose of this hack is that, before any of that DOSBox-X
     * code has a chance, we force the CPU back into real mode so that the code doesn't trigger funny page faults and DOSBox-X
     * shuts down properly. */
#endif
    warn_on_mem_write = true;
    throw 1;
}

void DoKillSwitch(void) {
    KillSwitch(true);
}

void BlankDisplay(void) {
    if (OpenGL_using()) {
        LOG_MSG("FIXME: BlankDisplay() not implemented for OpenGL mode");
    }
    else {
        SDL_FillRect(sdl.surface,0,0);
#if defined(C_SDL2)
        SDL_UpdateWindowSurface(sdl.window);
#else
        SDL_Flip(sdl.surface);
#endif
    }
}

void GFX_SDL_Overscan(void) {
    sdl.overscan_color=0;
    if (sdl.overscan_width) {
        Bitu border_color =  GFX_GetRGB(vga.dac.rgb[vga.attr.overscan_color].red<<2,
            vga.dac.rgb[vga.attr.overscan_color].green<<2, vga.dac.rgb[vga.attr.overscan_color].blue<<2);
        if (border_color != sdl.overscan_color) {
            sdl.overscan_color = border_color;

        // Find four rectangles forming the border
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0; rect->y = 0; rect->w = sdl.draw.width+(unsigned int)(2*sdl.clip.x); rect->h = (uint16_t)sdl.clip.y; // top
            if ((Bitu)rect->h > (Bitu)sdl.overscan_width) { rect->y += (int)(rect->h-sdl.overscan_width); rect->h = (uint16_t)sdl.overscan_width; }
            if ((Bitu)sdl.clip.x > (Bitu)sdl.overscan_width) { rect->x += (int)sdl.clip.x-(int)sdl.overscan_width; rect->w -= (uint16_t)(2*((int)sdl.clip.x-(int)sdl.overscan_width)); }
            rect = &sdl.updateRects[1];
            rect->x = 0; rect->y = sdl.clip.y; rect->w = (uint16_t)sdl.clip.x; rect->h = (uint16_t)sdl.draw.height; // left
            if ((unsigned int)rect->w > (unsigned int)sdl.overscan_width) { rect->x += (int)rect->w-(int)sdl.overscan_width; rect->w = (uint16_t)sdl.overscan_width; }
            rect = &sdl.updateRects[2];
            rect->x = (int)sdl.clip.x+(int)sdl.draw.width; rect->y = sdl.clip.y; rect->w = (uint16_t)sdl.clip.x; rect->h = (uint16_t)sdl.draw.height; // right
            if ((unsigned int)rect->w > (unsigned int)sdl.overscan_width) { rect->w = (uint16_t)sdl.overscan_width; }
            rect = &sdl.updateRects[3];
            rect->x = 0; rect->y = (int)sdl.clip.y+(int)sdl.draw.height; rect->w = sdl.draw.width+(unsigned int)(2*sdl.clip.x); rect->h = (uint16_t)sdl.clip.y; // bottom
            if ((Bitu)rect->h > (Bitu)sdl.overscan_width) { rect->h = (uint16_t)sdl.overscan_width; }
            if ((Bitu)sdl.clip.x > (Bitu)sdl.overscan_width) { rect->x += (int)sdl.clip.x-(int)sdl.overscan_width; rect->w -= (unsigned int)(2*((int)sdl.clip.x-(int)sdl.overscan_width)); }

            if (sdl.surface->format->BitsPerPixel == 8) { // SDL_FillRect seems to have some issues with palettized hw surfaces
                uint8_t* pixelptr = (uint8_t*)sdl.surface->pixels;
                Bitu linepitch = sdl.surface->pitch;
                for (Bitu i=0; i<4; i++) {
                    rect = &sdl.updateRects[i];
                    uint8_t* start = pixelptr + (unsigned int)rect->y*(unsigned int)linepitch + (unsigned int)rect->x;
                    for (Bitu j=0; j<(unsigned int)rect->h; j++) {
                        memset(start, vga.attr.overscan_color, rect->w);
                        start += linepitch;
                    }
                }
            } else {
                for (Bitu i=0; i<4; i++)
                    SDL_FillRect(sdl.surface, &sdl.updateRects[i], (Uint32)border_color);

#if defined(C_SDL2)
                SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
                SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
            }
        }
    }
}

bool DOSBox_Paused()
{
    return emu_paused;
}

bool pause_on_vsync = false;

#if defined(C_SDL2)
static bool IsFullscreen() {
    if (sdl.window == NULL) return false;
    uint32_t windowFlags = SDL_GetWindowFlags(sdl.window);
    if (windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) return true;
    return false;
}
#endif

bool is_paused = false;
bool unpause_now = false;
bool pausewithinterrupts_enable = false;
#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
int pause_menu_item_tag = -1;
#endif

void PushDummySDL(void) {
    SDL_Event event;

    memset(&event,0,sizeof(event));
    event.type = SDL_KEYUP;
    SDL_PushEvent(&event);
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion);
static void HandleMouseButton(SDL_MouseButtonEvent * button, SDL_MouseMotionEvent * motion);

#if defined(C_SDL2)
# if !defined(IGNORE_TOUCHSCREEN)
static void HandleTouchscreenFinger(SDL_TouchFingerEvent * finger);
# endif
#endif

#if defined(C_SDL2)
static const SDL_TouchID no_touch_id = (SDL_TouchID)(~0ULL);
static const SDL_FingerID no_finger_id = (SDL_FingerID)(~0ULL);
static SDL_FingerID touchscreen_finger_lock = no_finger_id;
static SDL_TouchID touchscreen_touch_lock = no_touch_id;
#endif

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
int menuwidth_atleast(int width) {
    HMENU hMenu = mainMenu.getWinMenu();
    int count = GetMenuItemCount(hMenu);
    int tWidth = 0;
    RECT r;
    for(int idx = 0; idx < count; ++idx) {
        if (GetMenuItemRect(GetHWND(), hMenu, idx, &r)) {
            int res=MapWindowPoints(NULL, GetHWND(), (LPPOINT)&r, 2);
            tWidth += r.right - r.left;
        }
    }
    tWidth += GetSystemMetrics(SM_CXBORDER)*2+(TTF_using()?24:60);
    unsigned int maxWidth = 0, maxHeight = 0;
    GetMaxWidthHeight(&maxWidth, &maxHeight);
    return tWidth>width && tWidth<=maxWidth ? tWidth : -1;
}
#endif

void HideMenu_mapper_shortcut(bool pressed) {
    if (!pressed) return;

    ToggleMenu(true);

    mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
#if defined(USE_TTF) && DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
    int last = 0;
    while (TTF_using() && !sdl.desktop.fullscreen && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0 && ttf.pointsize>last) {
        last = ttf.pointsize;
        increaseFontSize();
    }
#endif
}

void CALLBACK_Idle(void);

void PauseWithInterruptsEnabled(Bitu /*val*/) {
    /* we can ONLY do this when the CPU is either in real mode or v86 mode.
     * doing this from protected mode will only crash the game.
     * also require that interrupts are enabled before pausing. */
    if (cpu.pmode) {
        if (!(reg_flags & FLAG_VM)) {
            PIC_AddEvent(PauseWithInterruptsEnabled,0.001);
            return;
        }
    }

    if (!(reg_flags & FLAG_IF)) {
        PIC_AddEvent(PauseWithInterruptsEnabled,0.001);
        return;
    }

    while (pausewithinterrupts_enable) CALLBACK_Idle();
}

void PauseWithInterrupts_mapper_shortcut(bool pressed) {
    if (!pressed) return;

    if (!pausewithinterrupts_enable) {
        pausewithinterrupts_enable = true;
        PIC_AddEvent(PauseWithInterruptsEnabled,0.001);
    }
    else {
        pausewithinterrupts_enable = false;
    }

    mainMenu.get_item("mapper_pauseints").check(pausewithinterrupts_enable).refresh_item(mainMenu);
}

void PauseDOSBoxLoop(Bitu /*unused*/) {
    bool paused = true;
    SDL_Event event;

    /* reflect in the menu that we're paused now */
    mainMenu.get_item("mapper_pause").check(true).refresh_item(mainMenu);

    MAPPER_ReleaseAllKeys();
    GFX_ReleaseMouse();
    GFX_SetTitle(-1,-1,-1,true);
//  KEYBOARD_ClrBuffer();
    GFX_LosingFocus();
    while (SDL_PollEvent(&event)); // flush event queue.

    // reset pause conditions
    pause_on_vsync = false;

    // give mouse to win32 (ex. alt-tab)
#if defined(C_SDL2)
    SDL_SetRelativeMouseMode(SDL_FALSE);
#else
    SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif

    is_paused = true;
#if defined(WIN32)
    DOSBox_SetSysMenu();
#endif

    while (paused) {
        if (unpause_now) {
            unpause_now = false;
            break;
        }

#if C_EMSCRIPTEN
        emscripten_sleep(0);
        SDL_PollEvent(&event);
#elif C_GAMELINK
        // Keep GameLink ticking over.
        SDL_Delay(100);
        OUTPUT_GAMELINK_Transfer();
        SDL_PollEvent(&event);
#else
        SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
#endif

#if defined(WIN32) && !defined(HX_DOS)
        UINT msg=0;
        WPARAM wparam;
        if (event.type==SDL_SYSWMEVENT) {
#if defined(C_SDL2)
            msg=event.syswm.msg->msg.win.msg;
            wparam=event.syswm.msg->msg.win.wParam;
#else
            msg=event.syswm.msg->msg;
            wparam=event.syswm.msg->wParam;
#endif
        }
        if (event.type==SDL_SYSWMEVENT && msg == WM_COMMAND && (LOWORD(wparam) == ID_WIN_SYSMENU_PAUSE || LOWORD(wparam) == (mainMenu.get_item("mapper_pause").get_master_id()+DOSBoxMenu::winMenuMinimumID))) {
            paused=false;
            GFX_SetTitle(-1,-1,-1,false);   
            break;
        }
        if (event.type == SDL_SYSWMEVENT && msg == WM_SYSCOMMAND && LOWORD(wparam) == ID_WIN_SYSMENU_PAUSE) {
            paused = false;
            GFX_SetTitle(-1, -1, -1, false);
            break;
        }
#endif
        switch (event.type) {

            case SDL_QUIT: KillSwitch(true); break;
            case SDL_KEYDOWN:   // Must use Pause/Break or escape Key to resume.
            if(event.key.keysym.sym == SDLK_PAUSE || event.key.keysym.sym == SDLK_ESCAPE) {

                paused = false;
                GFX_SetTitle(-1,-1,-1,false);
                break;
            }
            else if (event.key.keysym.sym == SDLK_SPACE) { /* spacebar = single frame step */
                /* resume, but let the VGA code know to call us on vertical retrace */
                paused = false;
                pause_on_vsync = true;
                GFX_SetTitle(-1,-1,-1,false);
                break;
            }
#if defined (MACOSX) && !defined(C_SDL2)
            if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
                /* On macs, all apps exit when pressing cmd-q */
                KillSwitch(true);
                break;
            } 
#endif
        case SDL_MOUSEMOTION:
#if defined(C_SDL2)
            if (touchscreen_finger_lock == no_finger_id &&
                touchscreen_touch_lock == no_touch_id &&
                event.motion.which != SDL_TOUCH_MOUSEID) { /* don't handle mouse events faked by touchscreen */
                HandleMouseMotion(&event.motion);
            }
#else
            HandleMouseMotion(&event.motion);
#endif
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
#if defined(C_SDL2)
            if (touchscreen_finger_lock == no_finger_id &&
                touchscreen_touch_lock == no_touch_id &&
                event.button.which != SDL_TOUCH_MOUSEID) { /* don't handle mouse events faked by touchscreen */
                HandleMouseButton(&event.button,&event.motion);
            }
#else
            HandleMouseButton(&event.button,&event.motion);
#endif
            break;
#if defined(C_SDL2)
# if !defined(IGNORE_TOUCHSCREEN)
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            HandleTouchscreenFinger(&event.tfinger);
            break;
# endif
#endif
        }
    }

    // restore mouse state
    void GFX_UpdateSDLCaptureState();
    GFX_UpdateSDLCaptureState();

    MAPPER_ReleaseAllKeys();

//  KEYBOARD_ClrBuffer();
    GFX_LosingFocus();

    // redraw screen (ex. fullscreen - pause - alt+tab x2 - unpause)
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackReset );

    /* reflect in the menu that we're paused now */
    mainMenu.get_item("mapper_pause").check(false).refresh_item(mainMenu);

    is_paused = false;
#if defined(WIN32)
    DOSBox_SetSysMenu();
#endif
}

void PauseDOSBox(bool pressed) {
    if (pressed) {
        if (is_paused) {
            unpause_now = true;
            PushDummySDL();
        }
        else {
            PIC_AddEvent(PauseDOSBoxLoop,0.001);
        }
    }
}

int GetDisplayNumber(void) {
    return sdl.displayNumber;
}

void SetDisplayNumber(int display) {
    sdl.displayNumber = display;
}

#if defined(C_SDL2)
static bool SDL2_resize_enable = false;

SDL_Window* GFX_GetSDLWindow(void) {
    return sdl.window;
}

SDL_Window* GFX_SetSDLWindowMode(uint16_t width, uint16_t height, SCREEN_TYPES screenType) 
{
    static SCREEN_TYPES lastType = SCREEN_SURFACE;
    if (sdl.renderer) {
        SDL_DestroyRenderer(sdl.renderer);
        sdl.renderer=0;
    }
    if (sdl.texture.pixelFormat) {
        SDL_FreeFormat(sdl.texture.pixelFormat);
        sdl.texture.pixelFormat = 0;
    }
    if (sdl.texture.texture) {
        SDL_DestroyTexture(sdl.texture.texture);
        sdl.texture.texture=0;
    }
    sdl.window_desired_width = width;
    sdl.window_desired_height = height;
    int currWidth, currHeight;
    if (sdl.window) {
        //SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
        if (!sdl.update_window) {
            SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
            sdl.update_display_contents = ((width == currWidth) && (height == currHeight));

            currentWindowWidth = currWidth;
            currentWindowHeight = currHeight;

            return sdl.window;
        }
    }

#if C_OPENGL
    if (sdl_opengl.context) {
        SDL_GL_DeleteContext(sdl_opengl.context);
        sdl_opengl.context=0;
    }
#endif

    /* If we change screen type, recreate the window. Furthermore, if
     * it is our very first time then we simply create a new window.
     */
    if (!sdl.window
            || (lastType != screenType)
//          || (currWidth != width) || (currHeight != height)
//          || (glwindow != (0 != (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_OPENGL)))
//          || (fullscreen && (0 == (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN)))
//          || (fullscreen != (SDL_WINDOW_FULLSCREEN == (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN)))
//          || (fullscreen && ((width != currWidth) || (height != currHeight)))
       ) {
        lastType = screenType;
        if (sdl.window) {
            SDL_DestroyWindow(sdl.window);
        }

        sdl.window = SDL_CreateWindow("",
                                      SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber?sdl.displayNumber-1:0),
                                      SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber?sdl.displayNumber-1:0),
                                      width, height,
                                      (GFX_IsFullscreen() ? (sdl.desktop.full.display_res ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0)
                                      | ((screenType == SCREEN_OPENGL) ? SDL_WINDOW_OPENGL : 0) | (maximize && !TTF_using()? SDL_WINDOW_MAXIMIZED : 0)
                                      | SDL_WINDOW_SHOWN | (SDL2_resize_enable ? SDL_WINDOW_RESIZABLE : 0)
                                      | (dpi_aware_enable ? SDL_WINDOW_ALLOW_HIGHDPI : 0));
        if (sdl.window) {
            GFX_SetTitle(-1, -1, -1, false); //refresh title.
        }
        sdl.surface = SDL_GetWindowSurface(sdl.window);
        SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
        sdl.update_display_contents = ((width == currWidth) && (height == currHeight));

        currentWindowWidth = currWidth;
        currentWindowHeight = currHeight;

#if C_OPENGL
        if (screenType == SCREEN_OPENGL) {
            sdl_opengl.context = SDL_GL_CreateContext(sdl.window);
            if (sdl_opengl.context == NULL) LOG_MSG("WARNING: SDL2 unable to create GL context");
            if (SDL_GL_MakeCurrent(sdl.window, sdl_opengl.context) != 0) LOG_MSG("WARNING: SDL2 unable to make current GL context");
        }
#endif

        return sdl.window;
    }
    /* Fullscreen mode switching has its limits, and is also problematic on
     * some window managers. For now, the following may work up to some
     * level. On X11, SDL_VIDEO_X11_LEGACY_FULLSCREEN=1 can also help,
     * although it has its own issues.
     * Suggestion: Use the desktop res if possible, with output=surface
     * if one is not interested in scaling.
     * On Android, desktop res is the only way.
     */
    SDL_SetWindowResizable(sdl.window, SDL2_resize_enable ? SDL_TRUE : SDL_FALSE);
    if (GFX_IsFullscreen()) {
        SDL_DisplayMode displayMode;
        SDL_GetWindowDisplayMode(sdl.window, &displayMode);
        displayMode.w = width;
        displayMode.h = height;
        SDL_SetWindowDisplayMode(sdl.window, &displayMode);

        SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(sdl.window, 0);

        SDL_SetWindowSize(sdl.window, width, height);
    }
    /* Maybe some requested fullscreen resolution is unsupported? */
    SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
    sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
    sdl.surface = SDL_GetWindowSurface(sdl.window);

    currentWindowWidth = currWidth;
    currentWindowHeight = currHeight;

#if C_OPENGL
    if (screenType == SCREEN_OPENGL) {
        sdl_opengl.context = SDL_GL_CreateContext(sdl.window);
        if (sdl_opengl.context == NULL) LOG_MSG("WARNING: SDL2 unable to create GL context");
        if (SDL_GL_MakeCurrent(sdl.window, sdl_opengl.context) != 0) LOG_MSG("WARNING: SDL2 unable to make current GL context");
    }
#endif

    return sdl.window;
}

void GFX_SetResizeable(bool enable) {
    if (SDL2_resize_enable != enable) {
        SDL2_resize_enable = enable;

        if (sdl.window != NULL)
            SDL_SetWindowResizable(sdl.window, SDL2_resize_enable ? SDL_TRUE : SDL_FALSE);
    }
}

// Used for the mapper UI and more: Creates a fullscreen window with desktop res
// on Android, and a non-fullscreen window with the input dimensions otherwise.
SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height) {
    return GFX_SetSDLWindowMode(width, height, SCREEN_SURFACE);
}

// Returns the rectangle in the current window to be used for scaling a
// sub-window with the given dimensions, like the mapper UI.
SDL_Rect GFX_GetSDLSurfaceSubwindowDims(uint16_t width, uint16_t height) {
    SDL_Rect rect;
    rect.x=rect.y=0;
    rect.w=width;
    rect.h=height;
    return rect;
}

# if !defined(C_SDL2)
// Currently used for an initial test here
static SDL_Window * GFX_SetSDLOpenGLWindow(uint16_t width, uint16_t height) {
    return GFX_SetSDLWindowMode(width, height, SCREEN_OPENGL);
}
# endif
#endif

#if !defined(C_SDL2)
/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) 
{
    Bitu retFlags = 0;

    switch (sdl.desktop.want_type) 
    {
        case SCREEN_SURFACE:
            retFlags = OUTPUT_SURFACE_GetBestMode(flags);
            break;

#if C_OPENGL
        case SCREEN_OPENGL:
            retFlags = OUTPUT_OPENGL_GetBestMode(flags);
            break;
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            retFlags = OUTPUT_DIRECT3D_GetBestMode(flags);
            break;
#endif

#if defined(USE_TTF)
        case SCREEN_TTF:
            retFlags = GFX_CAN_32 | GFX_SCALING;
            break;
#endif

        default:
            // we should never reach here
            retFlags = 0;
            break;
    }

    if (!retFlags)
    {
        if (sdl.desktop.want_type != SCREEN_SURFACE)
        {
            // try falling back down to surface
            OUTPUT_SURFACE_Select();
            retFlags = OUTPUT_SURFACE_GetBestMode(flags);
        }
        if (retFlags == 0)
            LOG_MSG("SDL: Failed everything including falling back to surface GFX_GetBestMode"); // completely failed it seems
    }

    return retFlags;
}
#endif

/* FIXME: This prepares the SDL library to accept Win32 drag+drop events from the Windows shell.
 *        So it should be named something like EnableDragAcceptFiles() not SDL_Prepare() */
void SDL_Prepare(void) {
    if (menu_compatible) return;

#if defined(WIN32) && !defined(HX_DOS) // Microsoft Windows specific
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#if !defined(C_SDL2)
    LOG(LOG_MISC,LOG_DEBUG)("Win32: Preparing main window to accept files dragged in from the Windows shell");

    SDL_PumpEvents();
    DragAcceptFiles(GetHWND(), TRUE);
#endif
#endif
#if !defined(C_SDL2)
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
}

void GFX_ForceRedrawScreen(void) {
    GFX_Stop();
    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
    GFX_Start();
#if defined(USE_TTF)
    if (TTF_using() && CurMode->type==M_TEXT) ttf.inUse = true;
    if (ttf.inUse)
       GFX_EndTextLines(true);
#endif
}

void GFX_ResetScreen(void) {
    fullscreen_switch=false; 
	if(glide.enabled) {
		GLIDE_ResetScreen(true);
		return;
	}
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0; rect->y = 0; rect->w = 0; rect->h = 0;
#if defined(C_SDL2)
            SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
            SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
    GFX_Stop();
    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
    GFX_Start();
    CPU_Reset_AutoAdjust();
    fullscreen_switch=true;
    DOSBox_RefreshMenu(); // for menu
}

void GFX_ForceFullscreenExit(void) {
    if (sdl.desktop.lazy_fullscreen) {
        LOG_MSG("GFX LF: invalid screen change");
    } else {
        sdl.desktop.fullscreen=false;
        GFX_ResetScreen();
    }
}

uint32_t GFX_Rmask;
unsigned char GFX_Rshift;
uint32_t GFX_Gmask;
unsigned char GFX_Gshift;
uint32_t GFX_Bmask;
unsigned char GFX_Bshift;
uint32_t GFX_Amask;
unsigned char GFX_Ashift;
unsigned char GFX_bpp;

unsigned int GFX_GetBShift() {
    return sdl.surface->format->Bshift;
}

void GFX_LogSDLState(void) 
{
    LOG(LOG_MISC,LOG_DEBUG)("SDL video mode: %ux%u (clip %ux%u with upper-left at %ux%u) %ubpp",
        (unsigned)sdl.surface->w,(unsigned)sdl.surface->h,
        (unsigned)sdl.clip.w,(unsigned)sdl.clip.h,
        (unsigned)sdl.clip.x,(unsigned)sdl.clip.y,
        (unsigned)sdl.surface->format->BitsPerPixel);
    LOG(LOG_MISC,LOG_DEBUG)("   red: shift=%u mask=0x%08lx",
        (unsigned)sdl.surface->format->Rshift,
        (unsigned long)sdl.surface->format->Rmask);
    LOG(LOG_MISC,LOG_DEBUG)("   green: shift=%u mask=0x%08lx",
        (unsigned)sdl.surface->format->Gshift,
        (unsigned long)sdl.surface->format->Gmask);
    LOG(LOG_MISC,LOG_DEBUG)("   blue: shift=%u mask=0x%08lx",
        (unsigned)sdl.surface->format->Bshift,
        (unsigned long)sdl.surface->format->Bmask);
    LOG(LOG_MISC,LOG_DEBUG)("   alpha: shift=%u mask=0x%08lx",
        (unsigned)sdl.surface->format->Ashift,
        (unsigned long)sdl.surface->format->Amask);

    GFX_bpp = sdl.surface->format->BitsPerPixel;
    GFX_Rmask = sdl.surface->format->Rmask;
    GFX_Rshift = sdl.surface->format->Rshift;
    GFX_Gmask = sdl.surface->format->Gmask;
    GFX_Gshift = sdl.surface->format->Gshift;
    GFX_Bmask = sdl.surface->format->Bmask;
    GFX_Bshift = sdl.surface->format->Bshift;
    GFX_Amask = sdl.surface->format->Amask;
    GFX_Ashift = sdl.surface->format->Ashift;
}

void GFX_TearDown(void) {
    if (sdl.updating)
        GFX_EndUpdate( 0 );

    if (sdl.blit.surface) {
        SDL_FreeSurface(sdl.blit.surface);
        sdl.blit.surface=0;
    }
}

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
bool DOSBox_isMenuVisible(void);
void MenuShadeRect(int x,int y,int w,int h);
void MenuDrawRect(int x,int y,int w,int h,Bitu color);
void GFX_DrawSDLMenu(DOSBoxMenu &menu, DOSBoxMenu::displaylist &dl) {
    if (!menu.needsRedraw() || (sdl.updating && !OpenGL_using())) {
        return;
    }
    if (!DOSBox_isMenuVisible() || sdl.desktop.fullscreen) {
        // BUGFIX: If the menu is hidden then silently clear "needs redraw" to avoid excess redraw of nothing
        menu.clearRedraw();
        return;
    }

    bool mustLock = !OpenGL_using() && SDL_MUSTLOCK(sdl.surface);

    if (mustLock) {
        SDL_LockSurface(sdl.surface);
    }

    if (&dl == &menu.display_list) { /* top level menu, draw background */
        MenuDrawRect(menu.menuBox.x, menu.menuBox.y, menu.menuBox.w, menu.menuBox.h - 1, GFX_GetRGB(63, 63, 63));
        MenuDrawRect(menu.menuBox.x, menu.menuBox.y + menu.menuBox.h - 1, menu.menuBox.w, 1,
                     GFX_GetRGB(31, 31, 31));
    }

    if (mustLock) {
        SDL_UnlockSurface(sdl.surface);
    }

    int cp = dos.loaded_codepage;
    if (!cp) InitCodePage();
    if (IS_PC98_ARCH || IS_JEGA_ARCH || isDBCSCP()) InitFontHandle();
    dos.loaded_codepage = cp;
#if 0
    LOG_MSG("menudraw %u",(unsigned int)SDL_GetTicks());
#endif

    menu.clearRedraw();
    menu.display_list.DrawDisplayList(menu,/*updateScreen*/false);

    if (!OpenGL_using()) {
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects( sdl.window, &menu.menuBox, 1 );
#else
        SDL_UpdateRects(sdl.surface, 1, &menu.menuBox);
#endif
    }
}

void drawmenu(Bitu val) {
    if (menu_gui && menu.toggle) GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
}
#endif

void RENDER_Reset(void);

Bitu GFX_SetSize(Bitu width, Bitu height, Bitu flags, double scalex, double scaley, GFX_CallBack_t callback) 
{
    if ((width == 0 || height == 0) && !TTF_using()) {
        E_Exit("GFX_SetSize with width=%d height=%d zero dimensions not allowed",(int)width,(int)height);
        return 0;
    }
    bool diff = width != sdl.draw.width || height != sdl.draw.height;

    if (sdl.updating)
        GFX_EndUpdate( 0 );

    sdl.must_redraw_all = true;

    sdl.draw.width = (uint32_t)width;
    sdl.draw.height = (uint32_t)height;
#if defined(USE_TTF)
    if (TTF_using()) {
#if C_OPENGL && defined(MACOSX) && !defined(C_SDL2)
        sdl_opengl.framebuf = calloc(sdl.draw.width*sdl.draw.height, 4);
        sdl.desktop.type = SCREEN_OPENGL;
#else
        sdl.desktop.type = SCREEN_SURFACE;
#endif
        return OUTPUT_TTF_SetSize();
    }
#endif
    sdl.draw.flags = flags;
    sdl.draw.callback = callback;
    sdl.draw.scalex = scalex;
    sdl.draw.scaley = scaley;

    LOG(LOG_MISC,LOG_DEBUG)("GFX_SetSize %ux%u flags=0x%x scale=%.3fx%.3f",
        (unsigned int)width,(unsigned int)height,
        (unsigned int)flags,
        scalex,scaley);

//    Bitu bpp = 0;
    Bitu retFlags = 0;

    if (sdl.blit.surface) {
        SDL_FreeSurface(sdl.blit.surface);
        sdl.blit.surface=0;
    }

    switch (sdl.desktop.want_type) {
        case SCREEN_SURFACE:
            retFlags = OUTPUT_SURFACE_SetSize();
            break;

#if C_OPENGL
        case SCREEN_OPENGL:
            retFlags = OUTPUT_OPENGL_SetSize();
            break;
#endif

#if C_GAMELINK
        case SCREEN_GAMELINK:
            retFlags = OUTPUT_GAMELINK_SetSize();
            break;
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D: 
            retFlags = OUTPUT_DIRECT3D_SetSize();
            break;
#endif

#if defined(USE_TTF)
        case SCREEN_TTF:
            break;
#endif

        default:
            // we should never reach here
            retFlags = 0;
            break;
    }

    if (!retFlags)
    {
        if (sdl.desktop.want_type != SCREEN_SURFACE)
        {
            // try falling back down to surface
            OUTPUT_SURFACE_Select();
            retFlags = OUTPUT_SURFACE_SetSize();
        }
        if (retFlags == 0)
            LOG_MSG("SDL: Failed everything including falling back to surface in GFX_GetSize"); // completely failed it seems
    }

#if C_GAMELINK
    if (!OUTPUT_GAMELINK_InitTrackingMode() && sdl.desktop.want_type == SCREEN_GAMELINK) {
        OUTPUT_SURFACE_Select();
        retFlags = OUTPUT_SURFACE_SetSize();
    }
#endif

    // we have selected an actual desktop type
    sdl.desktop.type = sdl.desktop.want_type;

    GFX_LogSDLState();

    if (retFlags) 
        GFX_Start();

    if (!sdl.mouse.autoenable && !sdl.mouse.locked)
        SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);

#if defined(C_SDL2)
    if (diff && posx < 0 && posy < 0 && !(posx == -2 && posy == -2)) {
        if (sdl.displayNumber==0)
            SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        else {
            int bx = 0, by = 0;
            int displays = SDL_GetNumVideoDisplays();
            SDL_Rect bound;
            for( int i = 1; i <= displays; i++ ) {
                bound = SDL_Rect();
                SDL_GetDisplayBounds(i-1, &bound);
                if (i == sdl.displayNumber) {
                    bx = bound.x;
                    by = bound.y;
                    break;
                }
            }
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(sdl.displayNumber?sdl.displayNumber-1:0,&dm) == 0) {
                bx += (dm.w - sdl.draw.width - sdl.clip.x)/2;
                by += (dm.h - sdl.draw.height - sdl.clip.y)/2;
            }
            SDL_SetWindowPosition(sdl.window, bx, by);
        }
    }
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
    if ((!sdl.desktop.fullscreen && menu_gui && menu.toggle && ((width == 640 || (vga.draw.char9_set && width == 720)) && ((machine != MCH_CGA && !IS_VGA_ARCH && !IS_PC98_ARCH && height == 350) || height == 400))) || ((render.aspect || IS_DOSV) && checkmenuwidth)) {
        RECT r;
        bool res = GetWindowRect(GetHWND(), &r);
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
        int tWidth = menuwidth_atleast(r.right-r.left);
        if (tWidth>0 && tWidth>maxWidth) tWidth = maxWidth;
        int tHeight = r.bottom-r.top-height<0?(r.bottom-r.top):(double)(tWidth-((r.right-r.left-width)<0?0:(r.right-r.left-width)))*height/width+(r.bottom-r.top-height);
        if (tHeight>0 && tHeight>maxHeight) tHeight = maxHeight;
        if (res && tWidth>0 && tHeight>0) {
            MoveWindow(GetHWND(), r.left, r.top, tWidth, tHeight, true);
            LOG_MSG("SDL: Window size enlarged for the menus\n");
        }
    }
#endif
    UpdateWindowDimensions();

#if defined(WIN32) && !defined(HX_DOS)
    WindowsTaskbarUpdatePreviewRegion();
#endif

    return retFlags;
}

#if defined(WIN32) && !defined(HX_DOS)
// WARNING: Not recommended, there is danger you cannot exit emulator because mouse+keyboard are taken
static bool enable_hook_everything = false;
#endif

// Whether or not to hook the keyboard and block special keys.
// Setting this is recommended so that your keyboard is fully usable in the guest OS when you
// enable the mouse+keyboard capture. But hooking EVERYTHING is not recommended because there is a
// danger you become trapped in the DOSBox-X emulator!
static bool enable_hook_special_keys = true;

#if defined(WIN32) && !defined(HX_DOS)
// Whether or not to hook Num/Scroll/Caps lock in order to give the guest OS full control of the
// LEDs on the keyboard (i.e. the LEDs do not change until the guest OS changes their state).
// This flag also enables code to set the LEDs to guest state when setting mouse+keyboard capture,
// and restoring LED state when releasing capture.
static bool enable_hook_lock_toggle_keys = true;
#endif

#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
// and this is where we store host LED state when capture is set.
static bool on_capture_num_lock_was_on = true; // reasonable guess
static bool on_capture_scroll_lock_was_on = false;
static bool on_capture_caps_lock_was_on = false;
#endif

static bool exthook_enabled = false;
#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
static HHOOK exthook_winhook = NULL;

#if !defined(__MINGW32__)
extern "C" void SDL_DOSBox_X_Hack_Set_Toggle_Key_WM_USER_Hack(unsigned char x);
#endif

static LRESULT CALLBACK WinExtHookKeyboardHookProc(int nCode,WPARAM wParam,LPARAM lParam) {
    if (nCode == HC_ACTION) {
        HWND myHwnd = GetHWND();

        if (exthook_enabled && GetFocus() == myHwnd) { /* intercept only if DOSBox-X is the focus and the keyboard is hooked */
            if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN || wParam == WM_SYSKEYUP || wParam == WM_KEYUP) {
                KBDLLHOOKSTRUCT *st_hook = (KBDLLHOOKSTRUCT*)lParam;

                if (st_hook->flags & LLKHF_INJECTED) {
                    // injected keys are automatically allowed, especially if we are injecting keyboard input into ourself
                    // to control Num/Scroll/Caps Lock LEDs. If we don't check this we cannot control the LEDs. Injecting
                    // keydown/keyup for Num Lock is the only means provided by Windows to control those LEDs.
                }
                else if (st_hook->vkCode == VK_MENU/*alt*/ || st_hook->vkCode == VK_CONTROL ||
                    st_hook->vkCode == VK_LSHIFT || st_hook->vkCode == VK_RSHIFT) {
                    // always allow modifier keys through, so other applications are not left with state inconsistent from
                    // actual keyboard state.
                }
                else {
                    bool nopass = enable_hook_everything; // if the user wants us to hook ALL keys then that's where this signals it
                    bool alternate_message = false; // send as WM_USER+0x100 instead of WM_KEYDOWN

                    if (!nopass) {
                        // hook only certain keys Windows is likely to act on by itself.

                        // FIXME: Hooking the keyboard does NOT prevent Fn+SPACE (zoom) from triggering screen resolution
                        //        changes in Windows 10! How do we stop that?

                        // FIXME: It might be nice to let the user decide whether or not Print Screen is intercepted.

                        // TODO: We do not hook the volume up/down/mute keys. This is to be kind to the user. They may
                        // appreciate the ability to dial down the volume if a loud DOS program comes up. But
                        // if the user WANTS us to, we should allow hooking those keys.

                        // TODO: Allow (if instructed) hooking the VK_SLEEP key so pushing the sleep key (the
                        // one with the icon of the moon on Microsoft keyboards) can be sent instead to the
                        // guest OS. Also add code where if we're not hooking the key, then we should listen
                        // for signals the guest OS is suspending or hibernating and auto-disconnect the
                        // mouse capture and keyboard hook.

                        switch (st_hook->vkCode) {
                        case VK_LWIN:   // left Windows key (normally triggers Start menu)
                        case VK_RWIN:   // right Windows key (normally triggers Start menu)
                        case VK_APPS:   // Application key (normally open to the user, but just in case)
                        case VK_PAUSE:  // pause key
                        case VK_SNAPSHOT: // print screen
                        case VK_TAB:    // try to catch ALT+TAB too (not blocking VK_TAB will allow host OS to switch tasks)
                        case VK_ESCAPE: // try to catch CTRL+ESC as well (so Windows 95 Start Menu is accessible)
                        case VK_SPACE:  // and space (catching VK_ZOOM isn't enough to prevent Windows 10 from changing res)
                        // these keys have no meaning to DOSBox-X and so we hook them by default to allow the guest OS to use them
                        case VK_BROWSER_BACK: // Browser Back key
                        case VK_BROWSER_FORWARD: // Browser Forward key
                        case VK_BROWSER_REFRESH: // Browser Refresh key
                        case VK_BROWSER_STOP: // Browser Stop key
                        case VK_BROWSER_SEARCH: // Browser Search key
                        case VK_BROWSER_FAVORITES: // Browser Favorites key
                        case VK_BROWSER_HOME: // Browser Start and Home key
                        case VK_MEDIA_NEXT_TRACK: // Next Track key
                        case VK_MEDIA_PREV_TRACK: // Previous Track key
                        case VK_MEDIA_STOP: // Stop Media key
                        case VK_MEDIA_PLAY_PAUSE: // Play / Pause Media key
                        case VK_LAUNCH_MAIL: // Start Mail key
                        case VK_LAUNCH_MEDIA_SELECT: // Select Media key
                        case VK_LAUNCH_APP1: // Start Application 1 key
                        case VK_LAUNCH_APP2: // Start Application 2 key
                        case VK_PLAY: // Play key
                        case VK_ZOOM: // Zoom key (the (+) magnifying glass keyboard shortcut laptops have these days on the spacebar?)
                            nopass = true;
                            break;

                            // IME Hiragana key, otherwise inaccessible to us
                        case 0xF2:
                            nopass = true; // FIXME: This doesn't (yet) cause a SDL key event.
                            break;

                            // we allow hooking Num/Scroll/Caps Lock keys so that pressing them does not toggle the LED.
                            // we then take Num/Scroll/Caps LED state from the guest and let THAT control the LED state.
                        case VK_CAPITAL:
                        case VK_NUMLOCK:
                        case VK_SCROLL:
                            nopass = enable_hook_lock_toggle_keys;
                            alternate_message = true;
                            break;
                        }
                    }

                    if (nopass) {
                        // convert WM_KEYDOWN/WM_KEYUP if obfuscating the message to distinguish between real and injected events
                        if (alternate_message) {
                            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                                wParam = WM_USER + 0x100;
                            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                                wParam = WM_USER + 0x101;
                        }

                        DWORD lParam =
                            (st_hook->scanCode << 8U) +
                            ((st_hook->flags & LLKHF_EXTENDED) ? 0x01000000 : 0) +
                            ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) ? 0xC0000000 : 0);

                        // catch the keystroke, post it to ourself, do not pass it on
                        PostMessage(myHwnd, (UINT)wParam, st_hook->vkCode, lParam);
                        return TRUE;
                    }
                }
            }
        }
    }

    return CallNextHookEx(exthook_winhook, nCode, wParam, lParam);
}

// Microsoft doesn't have an outright "set toggle key state" call, they expect you
// to know the state and then fake input to toggle. Blegh. Fine.
void WinSetKeyToggleState(unsigned int vkCode, bool state) {
    bool curState = (GetKeyState(vkCode) & 1) ? true : false;
    INPUT inps;

    // if we're already in that state, then there is nothing to do.
    if (curState == state) return;

    // fake keyboard input.
    memset(&inps, 0, sizeof(inps));
    inps.type = INPUT_KEYBOARD;
    inps.ki.wVk = vkCode;
    inps.ki.dwFlags = KEYEVENTF_EXTENDEDKEY; // pressed, use wVk.
    SendInput(1, &inps, sizeof(INPUT));

    memset(&inps, 0, sizeof(inps));
    inps.type = INPUT_KEYBOARD;
    inps.ki.wVk = vkCode;
    inps.ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP; // release, use wVk.
    SendInput(1, &inps, sizeof(INPUT));
}
#endif

Bitu Keyboard_Guest_LED_State();
void UpdateKeyboardLEDState(Bitu led_state/* in the same bitfield arrangement as using command 0xED on PS/2 keyboards */);

void UpdateKeyboardLEDState(Bitu led_state/* in the same bitfield arrangement as using command 0xED on PS/2 keyboards */) {
    (void)led_state;//POSSIBLY UNUSED
#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS) /* Microsoft Windows */
    if (exthook_enabled) { // ONLY if ext hook is enabled, else we risk infinite loops with keyboard events
        //WinSetKeyToggleState(VK_NUMLOCK, !!(led_state & 2));
        //WinSetKeyToggleState(VK_SCROLL, !!(led_state & 1));
        //WinSetKeyToggleState(VK_CAPITAL, !!(led_state & 4));
    }
#endif
}

void DoExtendedKeyboardHook(bool enable) {
    if (exthook_enabled == enable)
        return;

#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
    if (enable) {
        if (!exthook_winhook) {
            exthook_winhook = SetWindowsHookEx(WH_KEYBOARD_LL, WinExtHookKeyboardHookProc, GetModuleHandle(NULL), 0);
            if (exthook_winhook == NULL) return;
        }

        // it's on
        exthook_enabled = enable;

        // flush out and handle pending keyboard I/O
        {
            MSG msg;

            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

#if !defined(__MINGW32__)
        // Enable the SDL hack for Win32 to handle Num/Scroll/Caps
        SDL_DOSBox_X_Hack_Set_Toggle_Key_WM_USER_Hack(1);
#endif

        // if hooking Num/Scroll/Caps Lock then record the toggle state of those keys.
        // then read from the keyboard emulation the LED state set by the guest and apply it to the host keyboard.
        if (enable_hook_lock_toggle_keys) {
            // record state
            on_capture_num_lock_was_on = (GetKeyState(VK_NUMLOCK) & 1) ? true : false;
            on_capture_scroll_lock_was_on = (GetKeyState(VK_SCROLL) & 1) ? true : false;
            on_capture_caps_lock_was_on = (GetKeyState(VK_CAPITAL) & 1) ? true : false;
            // change to guest state (FIXME: Read emulated keyboard state and apply!)
            UpdateKeyboardLEDState(Keyboard_Guest_LED_State());
        }
    }
    else {
        if (exthook_winhook) {
            if (enable_hook_lock_toggle_keys) {
                // restore state
                //WinSetKeyToggleState(VK_NUMLOCK, on_capture_num_lock_was_on);
                //WinSetKeyToggleState(VK_SCROLL, on_capture_scroll_lock_was_on);
                //WinSetKeyToggleState(VK_CAPITAL, on_capture_caps_lock_was_on);
            }

            {
                MSG msg;

                // before we disable the SDL hack make sure we flush out and handle any pending keyboard events.
                // if we don't do this the posted Num/Scroll/Caps events will stay in the queue and will be handled
                // by SDL after turning off the toggle key hack.
                Sleep(1); // make sure Windows posts the keystrokes
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

#if !defined(__MINGW32__)
            // Disable the SDL hack for Win32 to handle Num/Scroll/Caps
            SDL_DOSBox_X_Hack_Set_Toggle_Key_WM_USER_Hack(0);
#endif

            UnhookWindowsHookEx(exthook_winhook);
            exthook_winhook = NULL;
        }

        exthook_enabled = enable;
    }
#endif
}

void GFX_SetShader(const char* src) {
#if C_OPENGL
	sdl_opengl.shader_def = render.shader_def;
	if (!sdl_opengl.use_shader || src == sdl_opengl.shader_src)
		return;

	sdl_opengl.shader_src = src;
	if (sdl_opengl.program_object) {
		glDeleteProgram(sdl_opengl.program_object);
		sdl_opengl.program_object = 0;
	}
#else
    (void)src;//UNUSED
#endif
}

void GFX_ReleaseMouse(void) {
    if (sdl.mouse.locked)
        GFX_CaptureMouse();
}

void GFX_CaptureMouse(void) {
    GFX_CaptureMouse(!sdl.mouse.locked);
}

void GFX_CaptureMouse(bool capture) {
    sdl.mouse.locked=capture;
    if (sdl.mouse.locked) {
#if defined(C_SDL2)
        SDL_SetRelativeMouseMode(SDL_TRUE);
#else
        SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
        if (enable_hook_special_keys) DoExtendedKeyboardHook(true);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        DoExtendedKeyboardHook(false);
#if defined(C_SDL2)
        SDL_SetRelativeMouseMode(SDL_FALSE);
#else
        SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
        if (vmware_mouse) SDL_ShowCursor(SDL_DISABLE);
        else if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
    }
        mouselocked=sdl.mouse.locked;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
        if (sdl.mouse.locked) {
            void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
            void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

            GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
            GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);
        }
#endif

    GFX_SetTitle(-1,-1,-1,false);

    /* keep the menu updated (it might not exist yet) */
    if (mainMenu.item_exists("mapper_capmouse"))
        mainMenu.get_item("mapper_capmouse").check(sdl.mouse.locked).refresh_item(mainMenu);
}

void GFX_UpdateSDLCaptureState(void) {
    if (sdl.mouse.locked) {
#if defined(C_SDL2)
        SDL_SetRelativeMouseMode(SDL_TRUE);
#else
        SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
        if (enable_hook_special_keys) DoExtendedKeyboardHook(true);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        DoExtendedKeyboardHook(false);
#if defined(C_SDL2)
        SDL_SetRelativeMouseMode(SDL_FALSE);
#else
        SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
        if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
    }
    CPU_Reset_AutoAdjust();
    GFX_SetTitle(-1,-1,-1,false);
}

#if WIN32
void CaptureMouseNotifyWin32(bool lck)
{
    switch (sdl.mouse.autolock_feedback)
    {
    case AUTOLOCK_FEEDBACK_NONE: break;
    case AUTOLOCK_FEEDBACK_BEEP:
    {
        const auto lo = 1000;
        const auto hi = 2000;
        const auto t1 = 50;
        const auto t2 = 25;
        const auto f1 = lck ? hi : lo;
        const auto f2 = lck ? lo : hi;
        const auto tt = lck ? t1 : t2;
        Beep(f1, tt);
        Beep(f2, tt);
    }
    break;
    case AUTOLOCK_FEEDBACK_FLASH:
    {
# if !defined(HX_DOS)
        const auto cnt = lck ? 4 : 2;
        const auto tim = lck ? 80 : 40;
        const auto wnd = GetHWND();
        if (wnd != nullptr)
        {
            FLASHWINFO fi;
            fi.cbSize    = sizeof(FLASHWINFO);
            fi.hwnd      = wnd;
            fi.dwFlags   = FLASHW_CAPTION;
            fi.uCount    = cnt;
            fi.dwTimeout = tim;
            FlashWindowEx(&fi);
        }
# endif
        break;
    }
    default: ;
    }
}
#endif

void CaptureMouseNotify()
{
    CaptureMouseNotify(sdl.mouse.locked);
}

void CaptureMouseNotify(bool capture)
{
#if WIN32
    CaptureMouseNotifyWin32(capture);
#else
    (void)capture;
    // TODO
#endif
}

static void CaptureMouse(bool pressed) {
    if (!pressed || is_paused || sdl.desktop.want_type == SCREEN_GAMELINK)
        return;

    CaptureMouseNotify();
    GFX_CaptureMouse();
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore){
    static bool inited = false;
    if (!inited){
        inited = true;
        SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
    } 
    if (restore) {
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
        return;
    }
    //Get current sticky keys layout:
    STICKYKEYS s = {sizeof(STICKYKEYS), 0};
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
    if ( !(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
        s.dwFlags &= ~SKF_HOTKEYACTIVE;
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
    }
}
#else
#define sticky_keys(a)
#endif

void GetDesktopResolution(int* width, int* height)
{
#ifdef WIN32
    RECT rDdesk;
    auto hDesk = GetDesktopWindow();
    GetWindowRect(hDesk, &rDdesk);
    *width = rDdesk.right - rDdesk.left;
    *height = rDdesk.bottom - rDdesk.top;
#elif defined(LINUX)
    void Linux_GetDesktopResolution(int *width,int *height);
    Linux_GetDesktopResolution(width,height); /* this is MESSY but there's too much namespace collision going on here */
#else
    *width = 1024; // guess
    *height = 768;
#endif
}

void res_init(void) {
    Section * sec = control->GetSection("sdl");
    Section_prop * section=static_cast<Section_prop *>(sec);
    sdl.desktop.full.fixed=false;
    const char* fullresolution=section->Get_string("fullresolution");
    sdl.desktop.full.width  = 0; sdl.desktop.full.height = 0;
    if(fullresolution && *fullresolution) {
        char res[100];
        safe_strncpy( res, fullresolution, sizeof( res ));
        fullresolution = lowcase (res);//so x and X are allowed
        if (strcmp(fullresolution,"original")) {
            sdl.desktop.full.fixed = true;
            if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
                char* height = const_cast<char*>(strchr(fullresolution,'x'));
                if(height && * height) {
                    *height = 0;
                    sdl.desktop.full.height = atoi(height+1);
                    sdl.desktop.full.width  = atoi(res);
                }
            }
        }
    }

    sdl.desktop.window.width  = 0;
    sdl.desktop.window.height = 0;
    const char* windowresolution=section->Get_string("windowresolution");
    if(windowresolution && *windowresolution) {
        //if(sdl.desktop.type==SCREEN_SURFACE) return;
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if(strcmp(windowresolution,"original")) {
            char* height = const_cast<char*>(strchr(windowresolution,'x'));
            if(height && *height) {
                *height = 0;
                sdl.desktop.window.height = (uint16_t)atoi(height+1);
                sdl.desktop.window.width  = (uint16_t)atoi(res);
            }
        }
    }
    sdl.desktop.doublebuf=section->Get_bool("fulldouble");

    int width = 1024;
    int height = 768;
    
    // fullresolution == desktop -> get/set desktop size
    auto sdlSection = control->GetSection("sdl");
    auto sdlSectionProp = static_cast<Section_prop*>(sdlSection);
    auto fullRes = sdlSectionProp->Get_string("fullresolution");
    if (!strcmp(fullRes, "desktop")) GetDesktopResolution(&width, &height);
    
    if (!sdl.desktop.full.width) {
        sdl.desktop.full.width=width;
    }
    if (!sdl.desktop.full.height) {
        sdl.desktop.full.height=height;
    }
    if(sdl.desktop.type==SCREEN_SURFACE && !sdl.desktop.fullscreen) return;
    else {
        GFX_Stop();
        if (sdl.draw.callback)
            (sdl.draw.callback)( GFX_CallBackReset );
        GFX_Start();
    }
}

void res_input(bool type, const char * res) {
    Section* sec = control->GetSection("sdl");

    if(sec) {
        char win_res[11];
        strcpy(win_res,res);
        if(type) {
            std::string tmp("windowresolution="); tmp.append(win_res);
            sec->HandleInputline(tmp);
        } else {
            std::string tmp("fullresolution="); tmp.append(win_res);
            sec->HandleInputline(tmp);
        }

        res_init();
    }
}

#if !defined(HX_DOS) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void SDL_hax_SetFSWindowPosition(int x,int y,int w,int h);
#endif

void modeSwitched(bool full) {
    LOG_MSG("INFO: switched to %s mode", full ? "full screen" : "window");

#if defined (WIN32)
    // (re-)assign menu to window
    DOSBox_SetSysMenu();
#endif

    // ensure mouse capture when fullscreen || (re-)capture if user said so when windowed
    auto locked = sdl.mouse.locked;
    if ((full && !locked) || (!full && locked)) GFX_CaptureMouse();
}

void GFX_SwitchFullScreen(void)
{
#if defined(USE_TTF)
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
    if (ttf.fullScrn && ttf.inUse && !control->opt_nomenu && static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("showmenu")) {
        DOSBox_SetMenu();
        lastmenu = true;
    }
#endif

    if (ttf.inUse) {
        if (ttf.fullScrn) {
            sdl.desktop.fullscreen = false;
            if (lastfontsize>0)
                OUTPUT_TTF_Select(lastfontsize);
            else
                OUTPUT_TTF_Select(1);
            resetFontSize();
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            if (lastmenu) DOSBox_SetMenu();
#endif
#if defined(C_SDL2)
            if (posx >= 0 && posy >= 0)
                SDL_SetWindowPosition(sdl.window, posx, posy);
            else if (sdl.displayNumber==0 && !(posx == -2 && posy == -2))
                SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else if (posx != -2 || posy != -2) {
                int bx = 0, by = 0;
                int displays = SDL_GetNumVideoDisplays();
                SDL_Rect bound;
                for( int i = 1; i <= displays; i++ ) {
                    bound = SDL_Rect();
                    SDL_GetDisplayBounds(i-1, &bound);
                    if (i == sdl.displayNumber) {
                        bx = bound.x;
                        by = bound.y;
                        break;
                    }
                }
                SDL_DisplayMode dm;
                if (SDL_GetDesktopDisplayMode(sdl.displayNumber?sdl.displayNumber-1:0,&dm) == 0) {
                    bx += (dm.w - sdl.draw.width - sdl.clip.x)/2;
                    by += (dm.h - sdl.draw.height - sdl.clip.y)/2;
                }
                SDL_SetWindowPosition(sdl.window, bx, by);
            }
#endif
            resetreq = true;
            GFX_ResetScreen();
            resetFontSize();
            if (lastfontsize<1) PIC_AddEvent(ResetTTFSize,0.001);
            lastfontsize = 0;
        } else {
            lastfontsize = ttf.pointsize;
            sdl.desktop.fullscreen = true;
            OUTPUT_TTF_Select(3);
            resetFontSize();
        }
        modeSwitched(sdl.desktop.fullscreen);
        return;
    }
#endif
#if !defined(HX_DOS)
    if (sdl.desktop.prevent_fullscreen)
        return;

    if (!sdl.desktop.fullscreen) {/*is GOING fullscreen*/
        UpdateWindowDimensions();

        if (screen_size_info.screen_dimensions_pixels.width != 0 && screen_size_info.screen_dimensions_pixels.height != 0) {
            if (sdl.desktop.full.width_auto)
                sdl.desktop.full.width = (unsigned int)screen_size_info.screen_dimensions_pixels.width;
            if (sdl.desktop.full.height_auto)
                sdl.desktop.full.height = (unsigned int)screen_size_info.screen_dimensions_pixels.height;

#if !defined(C_SDL2) && defined(MACOSX)
            /* macOS has this annoying problem with their API where the System Preferences app, display settings panel
               allows setting 720p and 1080i/1080p modes on monitors who's native resolution is less than 1920x1080 but
               supports 1920x1080.

               This is a problem with HDTV sets made in the late 2000s early 2010s when "HD" apparently meant LCD displays
               with 1368x768 resolution that will nonetheless accept 1080i (and later, 1080p) and downscale on display.

               The problem is that with these monitors, macOS's display API will NOT list 1920x1080 as one of the
               display modes even when the freaking desktop is obviously set to 1920x1080 on that monitor!

               If the screen reporting code says 1920x1080 and we try to go fullscreen, SDL1 will intervene and say
               "hey wait there's no 1920x1080 in the mode list" and then fail the call.

               So to work around this, we have to go check SDL's mode list before using the screen size returned by the
               API.

               Oh and for extra fun, macOS is one of those modern systems where "setting the video mode" apparently
               means NOT setting the video hardware mode but just changing how the GPU scales the display... EXCEPT when
               talking to these older displays where if the user set it to 1080i/1080p, our request to set 1368x768
               actually DOES change the video mode. This is just wonderful considering the old HDTV set I bought back in
               2008 takes 1-2 seconds to adjust to the mode change on it's HDMI input.

               Frankly I wish I knew how to fix the SDL1 Quartz code to use whatever abracadabra magic code is required
               to enumerate these extra HDTV modes so this hack isn't necessary, except that experience says this hack is
               always necessary because it might not always work.

               This magic hidden super secret API bullshit is annoying. You're worse than Microsoft with this stuff, Apple --J.C. */
            {
                SDL_Rect **ls = SDLCALL SDL_ListModes(NULL, SDL_FULLSCREEN);
                if (ls != NULL) {
                    unsigned int maxwidth = 0,maxheight = 0;

                    for (size_t i=0;ls[i] != NULL;i++) {
                        unsigned int w = ls[i]->w;
                        unsigned int h = ls[i]->h;

                        if (maxwidth < w || maxheight < h) {
                            maxwidth = w;
                            maxheight = h;
                        }
                    }

                    if (maxwidth != 0 && maxheight != 0) {
                        LOG_MSG("macOS: Actual maximum screen resolution is %d x %d\n",maxwidth,maxheight);

                        if (sdl.desktop.full.width_auto) {
                            if (sdl.desktop.full.width > maxwidth)
                                sdl.desktop.full.width = maxwidth;
                        }
                        if (sdl.desktop.full.height_auto) {
                            if (sdl.desktop.full.height > maxheight)
                                sdl.desktop.full.height = maxheight;
                        }
                    }
                }
            }
#endif

#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
            SDL_hax_SetFSWindowPosition(
                (int)screen_size_info.screen_position_pixels.x, (int)screen_size_info.screen_position_pixels.y,
                (int)screen_size_info.screen_dimensions_pixels.width, (int)screen_size_info.screen_dimensions_pixels.height);
#endif
        }
        else {
#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
            SDL_hax_SetFSWindowPosition(0,0,0,0);
#endif
        }
    }

    menu.resizeusing = true;

    sdl.desktop.fullscreen = !sdl.desktop.fullscreen;

    auto full = sdl.desktop.fullscreen;

    // if we're going fullscreen and current scaler exceeds screen size,
    // cancel the fullscreen change -> fixes scaler crashes
    // TODO this will need further changes to accommodate different outputs (e.g. stretched)
    if (full)
    {
        int width, height;
        GetDesktopResolution(&width, &height);
        auto width1 = sdl.draw.width;
        auto height1 = sdl.draw.height;
        if ((unsigned int)width < width1 || (unsigned int)height < height1) {
            sdl.desktop.fullscreen = false;
            LOG_MSG("WARNING: full screen canceled, surface size (%ix%i) exceeds screen size (%ix%i).",
                width1, height1, width, height);
        }
    }

    modeSwitched(full);

    // disable/enable sticky keys for fullscreen/desktop
#if defined (WIN32)
    sticky_keys(!full);
#endif

	if (glide.enabled)
		GLIDE_ResetScreen();
	else
        GFX_ResetScreen();

    // set vsync to host
    // NOTE why forcing ???
#ifdef WIN32
    if (menu.startup) // NOTE should be always true I suppose ???
    {
        auto vsyncProp = static_cast<Section_prop *>(control->GetSection("vsync"));
        if (vsyncProp)
        {
            auto vsyncMode = vsyncProp->Get_string("vsyncmode");
            if (!strcmp(vsyncMode, "host")) SetVal("vsync", "vsyncmode", "host");
        }
    }
#endif
#endif
}

static void SwitchFullScreen(bool pressed) {
    if (!pressed || sdl.desktop.want_type == SCREEN_GAMELINK)
        return;

    GFX_LosingFocus();
    if (sdl.desktop.lazy_fullscreen) {
        LOG_MSG("GFX LF: fullscreen switching not supported");
    } else {
        GFX_SwitchFullScreen();
    }
}

void GFX_SwitchLazyFullscreen(bool lazy) {
    sdl.desktop.lazy_fullscreen=lazy;
    sdl.desktop.lazy_fullscreen_req=false;
}

void GFX_SwitchFullscreenNoReset(void) {
    sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
}

bool GFX_LazyFullscreenRequested(void) {
    if (sdl.desktop.lazy_fullscreen) return sdl.desktop.lazy_fullscreen_req;
    return false;
}

bool GFX_GetPreventFullscreen(void) {
    return sdl.desktop.prevent_fullscreen;
}

#if defined(WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" unsigned char SDL1_hax_RemoveMinimize;
#endif

void GFX_PreventFullscreen(bool lockout) {
    if (sdl.desktop.prevent_fullscreen != lockout) {
        sdl.desktop.prevent_fullscreen = lockout;
#if defined(WIN32)
        DOSBox_SetSysMenu();
#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
        SDL1_hax_RemoveMinimize = lockout ? 1 : 0;
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif
#endif
    }
}

void GFX_RestoreMode(void) {
    if (sdl.draw.width == 0 || sdl.draw.height == 0)
        return;

    GFX_SetSize(sdl.draw.width,sdl.draw.height,sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
    GFX_UpdateSDLCaptureState();
    GFX_ResetScreen();
}

static bool GFX_GetSurfacePtrLock = false;

unsigned char *GFX_GetSurfacePtr(size_t *pitch, unsigned int x, unsigned int y) {
    if (sdl.surface->pixels == NULL) {
        if (!GFX_GetSurfacePtrLock) {
            if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
                return NULL;

            GFX_GetSurfacePtrLock = true;
        }
    }

    *pitch = sdl.surface->pitch;
    if (sdl.surface->pixels != NULL) {
        unsigned char *p = (unsigned char*)(sdl.surface->pixels);
        p += y * sdl.surface->pitch;
        p += x * ((unsigned int)sdl.surface->format->BitsPerPixel >> 3U);
        return p;
    }

    return NULL;
}

void GFX_ReleaseSurfacePtr(void) {
    if (GFX_GetSurfacePtrLock) {
        if (SDL_MUSTLOCK(sdl.surface))
            SDL_UnlockSurface(sdl.surface);
 
        GFX_GetSurfacePtrLock = false;
    }
}

bool GFX_StartUpdate(uint8_t* &pixels,Bitu &pitch) 
{
    if (!sdl.active || sdl.updating)
        return false;

    switch (sdl.desktop.type) 
    {
        case SCREEN_SURFACE:
            return OUTPUT_SURFACE_StartUpdate(pixels, pitch);

#if C_OPENGL
        case SCREEN_OPENGL:
            return OUTPUT_OPENGL_StartUpdate(pixels, pitch);
#endif

#if C_GAMELINK
        case SCREEN_GAMELINK:
            return OUTPUT_GAMELINK_StartUpdate(pixels, pitch);
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            return OUTPUT_DIRECT3D_StartUpdate(pixels, pitch);
#endif

        default:
            break;
    }

    return false;
}

void GFX_OpenGLRedrawScreen(void) {
#if C_OPENGL
    if (OpenGL_using()) {
        if (sdl_opengl.clear_countdown > 0) {
            sdl_opengl.clear_countdown--;
            glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if (sdl_opengl.pixel_buffer_object) {
            glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
            glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
            glCallList(sdl_opengl.displaylist);
        } else {
            glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
            glCallList(sdl_opengl.displaylist);
        }
    }
#endif
}

void GFX_EndUpdate(const uint16_t *changedLines) {
#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    /* don't present our output if 3Dfx is in OpenGL mode */
    if (sdl.desktop.prevent_fullscreen)
        return;

#if C_DIRECT3D
    // we may have to do forced update in D3D case
    if (d3d && d3d->getForceUpdate());
    else
#endif
    if (((sdl.desktop.type != SCREEN_OPENGL) || !RENDER_GetForceUpdate()) && !sdl.updating)
        return;
#if C_OPENGL
    bool actually_updating = sdl.updating;
#endif
    sdl.updating = false;
#if defined(USE_TTF)
    if (ttf.inUse) {
        GFX_EndTextLines();
        return;
    }
#endif

    switch (sdl.desktop.type) 
    {
        case SCREEN_SURFACE:
            OUTPUT_SURFACE_EndUpdate(changedLines);
            break;

#if C_OPENGL
        case SCREEN_OPENGL:
            // Clear drawing area. Some drivers (on Linux) have more than 2 buffers and the screen might
            // be dirty because of other programs.
            if (!actually_updating) {
                /* Don't really update; Just increase the frame counter.
                 * If we tried to update it may have not worked so well
                 * with VSync...
                 * (Think of 60Hz on the host with 70Hz on the client.)
                 */
                sdl_opengl.actual_frame_count++;
                return;
            }
            OUTPUT_OPENGL_EndUpdate(changedLines);
            break;
#endif

#if C_GAMELINK
        case SCREEN_GAMELINK:
            OUTPUT_GAMELINK_EndUpdate(changedLines);
            break;
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            OUTPUT_DIRECT3D_EndUpdate(changedLines);
            break;
#endif

        default:
            break;
    }

#if C_GAMELINK
    OUTPUT_GAMELINK_Transfer();
#endif

    if (changedLines != NULL) 
    {
        sdl.must_redraw_all = false;

#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
        sdl.surface->flags &= ~((unsigned int)SDL_HAX_NOREFRESH);
#endif

        if (changedLines != NULL && sdl.deferred_resize) 
        {
            sdl.deferred_resize = false;
#if !defined(C_SDL2)
            void GFX_RedrawScreen(uint32_t nWidth, uint32_t nHeight);
            GFX_RedrawScreen(sdl.draw.width, sdl.draw.height);
#endif
        }
        else if (sdl.gfx_force_redraw_count > 0) 
        {
            void RENDER_CallBack( GFX_CallBackFunctions_t function );
            RENDER_CallBack(GFX_CallBackRedraw);
            sdl.gfx_force_redraw_count--;
        }
    }
}

void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
    (void)start;
    (void)count;
    (void)entries;
#if !defined(C_SDL2)
    /* I should probably not change the GFX_PalEntry :) */
    if (sdl.surface->flags & SDL_HWPALETTE) {
        if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,(int)start,(int)count)) {
            E_Exit("SDL: Cannot set palette");
        }
    } else {
        if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,(int)start,(int)count)) {
            E_Exit("SDL: Cannot set palette");
        }
    }
#endif
}

Bitu GFX_GetRGB(uint8_t red, uint8_t green, uint8_t blue) {
    switch (sdl.desktop.type) {
        case SCREEN_SURFACE:
            return SDL_MapRGB(sdl.surface->format, red, green, blue);

#if C_OPENGL
        case SCREEN_OPENGL:
# if SDL_BYTEORDER == SDL_LIL_ENDIAN && defined(MACOSX) /* macOS Intel builds use a weird RGBA order (alpha in the low 8 bits) */
            //USE BGRA
            return (((unsigned long)blue << 24ul) | ((unsigned long)green << 16ul) | ((unsigned long)red <<  8ul)) | (255ul <<  0ul);
# else
            //USE ARGB
            return (((unsigned long)blue <<  0ul) | ((unsigned long)green <<  8ul) | ((unsigned long)red << 16ul)) | (255ul << 24ul);
# endif
#endif

#if C_GAMELINK
        case SCREEN_GAMELINK:
            return (((unsigned long)blue <<  0ul) | ((unsigned long)green <<  8ul) | ((unsigned long)red << 16ul)) | (255ul << 24ul);
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            return SDL_MapRGB(sdl.surface->format, red, green, blue);
#endif

        default:
            break;
    }
    return 0;
}

void GFX_Stop() {
    if (sdl.updating)
        GFX_EndUpdate( 0 );
    sdl.active=false;
}

#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
static uint8_t im_x, im_y;
#endif

void GFX_Start() {
#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
	if(dos.im_enable_flag) {
		SDL_SetIMValues(SDL_IM_ENABLE, 1, NULL);
		im_x = -1;
	}
#endif
    sdl.active=true;
}

static void GUI_ShutDown(Section * /*sec*/) {
    GFX_Stop();
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
    if (sdl.mouse.locked) GFX_CaptureMouse();
    if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
    switch (sdl.desktop.type)
    {
        case SCREEN_SURFACE:
            OUTPUT_SURFACE_Shutdown();
            break;

#if C_OPENGL
        case SCREEN_OPENGL:
            OUTPUT_OPENGL_Shutdown();
            break;
#endif

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            OUTPUT_DIRECT3D_Shutdown();
            break;
#endif

        default:
                break;
    }

    #if C_GAMELINK
    OUTPUT_GAMELINK_Shutdown();
    #endif
}

static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest
// priority can not be set as users can only lower priority (not restore it)

    if((sdl.priority.focus != sdl.priority.nofocus ) &&
        (getuid()!=0) ) return;

#endif
    switch (level) {
#ifdef WIN32
    case PRIORITY_LEVEL_PAUSE:  // if DOSBox-X is paused, assume idle priority
    case PRIORITY_LEVEL_LOWEST:
        SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
        break;
    case PRIORITY_LEVEL_LOWER:
        SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
        break;
    case PRIORITY_LEVEL_NORMAL:
        SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
        break;
    case PRIORITY_LEVEL_HIGHER:
        SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
        break;
    case PRIORITY_LEVEL_HIGHEST:
        SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
        break;
#elif C_SET_PRIORITY
/* Linux use group as dosbox-x has multiple threads under linux */
    case PRIORITY_LEVEL_PAUSE:  // if DOSBox-X is paused, assume idle priority
    case PRIORITY_LEVEL_LOWEST:
        setpriority (PRIO_PGRP, 0,PRIO_MAX);
        break;
    case PRIORITY_LEVEL_LOWER:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
        break;
    case PRIORITY_LEVEL_NORMAL:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
        break;
    case PRIORITY_LEVEL_HIGHER:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
        break;
    case PRIORITY_LEVEL_HIGHEST:
        setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
        break;
#endif
    default:
        break;
    }
}

static void OutputString(Bitu x,Bitu y,const char * text,uint32_t color,uint32_t color2,SDL_Surface * output_surface) {
    uint32_t * draw=(uint32_t*)(((uint8_t *)output_surface->pixels)+(y*output_surface->pitch))+x;
    while (*text) {
        uint8_t * font=&int10_font_14[(*text)*14];
        Bitu i,j;
        uint32_t * draw_line=draw;
        for (i=0;i<14;i++) {
            uint8_t map=*font++;
            for (j=0;j<8;j++) {
                if (map & 0x80) *((uint32_t*)(draw_line+j))=color; else *((uint32_t*)(draw_line+j))=color2;
                map<<=1;
            }
            draw_line+=output_surface->pitch/4;
        }
        text++;
        draw+=8;
    }
}

void ResetSystem(bool pressed) {
    if (!pressed) return;
	
	if (is_paused) {
		is_paused = false;
		mainMenu.get_item("mapper_pause").check(false).refresh_item(mainMenu);
	}
	if (pausewithinterrupts_enable) {
        pausewithinterrupts_enable = false;
		mainMenu.get_item("mapper_pauseints").check(false).refresh_item(mainMenu);
	}
	bootvm=true;
    throw int(3);
}

void RebootGuest(bool pressed) {
    if (!pressed) return;

	if (is_paused) {
		is_paused = false;
		mainMenu.get_item("mapper_pause").check(false).refresh_item(mainMenu);
	}
	if (pausewithinterrupts_enable) {
        pausewithinterrupts_enable = false;
		mainMenu.get_item("mapper_pauseints").check(false).refresh_item(mainMenu);
	}
	if (!dos_kernel_disabled) {
	    if (CurMode->type==M_TEXT || IS_PC98_ARCH) {
			char msg[]="\033[2J";
			uint16_t s = (uint16_t)strlen(msg);
			DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
            throw int(6);
	    } else {
            bootfast=true;
            throw int(3);
	    }
	}
	bootguest=true;
	throw int(3);
}

void LoadMapFile(bool bPressed) {
    if (!bPressed) return;
    void Load_mapper_file();
    Load_mapper_file();
}

void QuickLaunch(bool bPressed) {
    if (!bPressed) return;
    if (dos_kernel_disabled) return;
    MenuBrowseProgramFile();
}

void SendKey(std::string key) {
    if (key == "sendkey_winlogo") {
        KEYBOARD_AddKey(KBD_lwindows, true);
        KEYBOARD_AddKey(KBD_lwindows, false);
    } else if (key == "sendkey_winmenu") {
        KEYBOARD_AddKey(KBD_rwinmenu, true);
        KEYBOARD_AddKey(KBD_rwinmenu, false);
    } else if (key == "sendkey_alttab") {
        KEYBOARD_AddKey(KBD_leftalt, true);
        KEYBOARD_AddKey(KBD_tab, true);
        KEYBOARD_AddKey(KBD_leftalt, false);
        KEYBOARD_AddKey(KBD_tab, false);
    } else if (key == "sendkey_ctrlesc") {
        KEYBOARD_AddKey(KBD_leftctrl, true);
        KEYBOARD_AddKey(KBD_esc, true);
        KEYBOARD_AddKey(KBD_leftctrl, false);
        KEYBOARD_AddKey(KBD_esc, false);
    } else if (key == "sendkey_ctrlbreak") {
        KEYBOARD_AddKey(KBD_leftctrl, true);
        KEYBOARD_AddKey(KBD_pause, true);
        KEYBOARD_AddKey(KBD_leftctrl, false);
        KEYBOARD_AddKey(KBD_pause, false);
    } else if (key == "sendkey_cad") {
        KEYBOARD_AddKey(KBD_leftctrl, true);
        KEYBOARD_AddKey(KBD_leftalt, true);
        KEYBOARD_AddKey(KBD_delete, true);
        KEYBOARD_AddKey(KBD_leftctrl, false);
        KEYBOARD_AddKey(KBD_leftalt, false);
        KEYBOARD_AddKey(KBD_delete, false);
    }
}

void Sendkeymapper(bool pressed) {
    if (!pressed) return;
    if (sendkeymap==1) SendKey("sendkey_winlogo");
    else if (sendkeymap==2) SendKey("sendkey_winmenu");
    else if (sendkeymap==3) SendKey("sendkey_alttab");
    else if (sendkeymap==4) SendKey("sendkey_ctrlesc");
    else if (sendkeymap==5) SendKey("sendkey_ctrlbreak");
    else SendKey("sendkey_cad");
}

bool has_GUI_StartUp = false;

static void GUI_StartUp() {
    DOSBoxMenu::item *item;

    if (has_GUI_StartUp) return;
    has_GUI_StartUp = true;

    LOG(LOG_GUI,LOG_DEBUG)("Starting GUI");

#if defined(C_SDL2)
    LOG(LOG_GUI,LOG_DEBUG)("This version compiled against SDL 2.x");
#else
    LOG(LOG_GUI,LOG_DEBUG)("This version compiled against SDL 1.x");
#endif

#if defined(C_SDL2)
    /* while we're here, SDL 2.0.5 has some issues with Linux/X11, encourage the user to update SDL2. */
    {
        SDL_version v;
        SDL_GetVersion(&v);
        LOG(LOG_GUI,LOG_DEBUG)("SDL2 version %u.%u.%u",v.major,v.minor,v.patch);

# if defined(LINUX)
        /* Linux/X11 2.0.5 has window positioning issues i.e. with XFCE */
        if (v.major == 2 && v.minor == 0 && v.patch == 5)
            LOG_MSG("WARNING: Your SDL2 library is known to have some issues with Linux/X11, please update your SDL2 library");
# endif
    }
#endif

    AddExitFunction(AddExitFunctionFuncPair(GUI_ShutDown));
    GUI_LoadFonts();

    sdl.active=false;
    sdl.updating=false;
#if defined(C_SDL2)
    sdl.update_window=true;
#endif

    GFX_SetIcon();

    sdl.desktop.lazy_fullscreen=false;
    sdl.desktop.lazy_fullscreen_req=false;
    sdl.desktop.prevent_fullscreen=false;

    Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl"));
    assert(section != NULL);

    maximize = section->Get_bool("maximize");

    sdl.desktop.fullscreen=false;
    sdl.wait_on_error=section->Get_bool("waitonerror");

    Prop_multival* p=section->Get_multival("priority");
    std::string focus = p->GetSection()->Get_string("active");
    std::string notfocus = p->GetSection()->Get_string("inactive");

    if      (focus == "lowest")  { sdl.priority.focus = PRIORITY_LEVEL_LOWEST;  }
    else if (focus == "lower")   { sdl.priority.focus = PRIORITY_LEVEL_LOWER;   }
    else if (focus == "normal")  { sdl.priority.focus = PRIORITY_LEVEL_NORMAL;  }
    else if (focus == "higher")  { sdl.priority.focus = PRIORITY_LEVEL_HIGHER;  }
    else if (focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

    if      (notfocus == "lowest")  { sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;  }
    else if (notfocus == "lower")   { sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;   }
    else if (notfocus == "normal")  { sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;  }
    else if (notfocus == "higher")  { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;  }
    else if (notfocus == "highest") { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST; }
    else if (notfocus == "pause")   {
        /* we only check for pause here, because it makes no sense
         * for DOSBox-X to be paused while it has focus
         */
        sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
    }

    SetPriority(sdl.priority.focus); //Assume focus on startup
    sdl.mouse.locked=false;
    mouselocked=false; //Global for mapper
    sdl.mouse.requestlock=false;
    sdl.desktop.full.fixed=false;
    const char* fullresolution=section->Get_string("fullresolution");
    sdl.desktop.full.width  = 0;
    sdl.desktop.full.height = 0;
    if(fullresolution && *fullresolution) {
        char res[100];
        safe_strncpy(res, fullresolution, sizeof(res));
        fullresolution = lowcase (res);//so x and X are allowed
        if (strcmp(fullresolution,"original")) {
            sdl.desktop.full.fixed = true;
            if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
                char* height = const_cast<char*>(strchr(fullresolution,'x'));
                if (height && * height) {
                    *height = 0;
                    sdl.desktop.full.height = (uint16_t)atoi(height+1);
                    sdl.desktop.full.width  = (uint16_t)atoi(res);
                }
            }
        }
    }

    sdl.desktop.window.width  = 0;
    sdl.desktop.window.height = 0;
    const char* windowresolution=section->Get_string("windowresolution");
    if(windowresolution && *windowresolution) {
        char res[100];
        safe_strncpy(res, windowresolution, sizeof(res));
        windowresolution = lowcase (res);//so x and X are allowed
        if(strcmp(windowresolution,"original")) {
            char* height = const_cast<char*>(strchr(windowresolution,'x'));
            if(height && *height) {
                *height = 0;
                sdl.desktop.window.height = (uint16_t)atoi(height+1);
                sdl.desktop.window.width  = (uint16_t)atoi(res);
            }
        }
    }
    sdl.desktop.doublebuf=section->Get_bool("fulldouble");
#if defined(C_SDL2)
    {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0/*FIXME display index*/,&dm) == 0) {
            if (sdl.desktop.full.width == 0) {
                sdl.desktop.full.width_auto = true;
                sdl.desktop.full.width = dm.w;
            }
            if (sdl.desktop.full.height == 0) {
                sdl.desktop.full.height_auto = true;
                sdl.desktop.full.height = dm.h;
            }
            LOG_MSG("SDL2 reports desktop display mode %u x %u",dm.w,dm.h);
        }
        else {
            LOG_MSG("SDL2 unable to determine desktop display mode, error %s",SDL_GetError());
        }
    }
#endif
#if !defined(C_SDL2)
  #if SDL_VERSION_ATLEAST(1, 2, 10)
  #ifdef WIN32
    /* NTS: This should not print any warning whatsoever because Windows builds by default will use
     *      the Windows API to disable DPI scaling of the main window, unless the user modifies the
     *      setting through dosbox-x.conf or the command line. */
    /* NTS: macOS has high DPI scaling too, though Apple is wise to enable it by default only for
     *      Macbooks with "Retina" displays. On macOS, unless otherwise wanted by the user, it is
     *      wise to let macOS scale up the DOSBox-X window by 2x so that the DOS prompt is not
     *      a teeny tiny window on the screen. */
    const SDL_VideoInfo* vidinfo = SDL_GetVideoInfo();
    if (vidinfo) {
        int sdl_w = vidinfo->current_w;
        int sdl_h = vidinfo->current_h;
        int win_w = GetSystemMetrics(SM_CXSCREEN);
        int win_h = GetSystemMetrics(SM_CYSCREEN);
        if (sdl_w != win_w && sdl_h != win_h) 
            LOG_MSG("Windows DPI/blurry apps scaling detected as it might be a large screen.\n"
				"Please see the DOSBox-X documentation for more details.\n");
        }
  #else
    if (!sdl.desktop.full.width || !sdl.desktop.full.height){
        //Can only be done on the very first call! Not restartable.
        //On windows don't use it as SDL returns the values without taking in account the dpi scaling
        const SDL_VideoInfo* vidinfo = SDL_GetVideoInfo();
        if (vidinfo) {
            if (sdl.desktop.full.width == 0) {
                sdl.desktop.full.width_auto = true;
                sdl.desktop.full.width = vidinfo->current_w;
            }
            if (sdl.desktop.full.height == 0) {
                sdl.desktop.full.height_auto = true;
                sdl.desktop.full.height = vidinfo->current_h;
            }

            LOG_MSG("SDL1 auto-detected desktop as %u x %u",
                (unsigned int)sdl.desktop.full.width,
                (unsigned int)sdl.desktop.full.height);
        }
    }
  #endif
  #endif
#endif

    if (!sdl.desktop.full.width) {
        int width=1024;
        sdl.desktop.full.width_auto = true;
        sdl.desktop.full.width=width;
    }
    if (!sdl.desktop.full.height) {
        int height=768;
        sdl.desktop.full.height_auto = true;
        sdl.desktop.full.height=height;
    }
    sdl.mouse.autoenable=section->Get_bool("autolock");
    if (!sdl.mouse.autoenable && sdl.desktop.want_type != SCREEN_GAMELINK) SDL_ShowCursor(SDL_DISABLE);
    sdl.mouse.autolock=false;

    const std::string feedback = section->Get_string("autolock_feedback");
    if (feedback == "none")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_NONE;
    else if (feedback == "beep")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_BEEP;
    else if (feedback == "flash")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_FLASH;

    const std::string munlock = section->Get_string("middle_unlock");
    if (munlock == "none")
        middleunlock = 0;
    else if (munlock == "auto")
        middleunlock = 2;
    else if (munlock == "both")
        middleunlock = 3;
    else if (munlock == "manual")
        middleunlock = 1;

    const char *clip_mouse_button = section->Get_string("clip_mouse_button");
    if (!strcmp(clip_mouse_button, "middle")) mbutton=2;
    else if (!strcmp(clip_mouse_button, "right")) mbutton=3;
    else if (!strcmp(clip_mouse_button, "arrows")) mbutton=4;
    else mbutton=0;
    modifier = section->Get_string("clip_key_modifier");
    const char *pastebios = section->Get_string("clip_paste_bios");
    if (!strcasecmp(pastebios, "true") || !strcmp(pastebios, "1")) clipboard_biospaste = true;
    else if (!strcasecmp(pastebios, "false") || !strcmp(pastebios, "0")) clipboard_biospaste = false;
    paste_speed = (unsigned int)section->Get_int("clip_paste_speed");
    wheel_key = section->Get_int("mouse_wheel_key");
    wheel_guest=wheel_key>0;
    if (wheel_key<0) wheel_key=-wheel_key;

    Prop_multival* p3 = section->Get_multival("sensitivity");
    sdl.mouse.xsensitivity = p3->GetSection()->Get_int("xsens");
    sdl.mouse.ysensitivity = p3->GetSection()->Get_int("ysens");

#if defined(C_SDL2)
    // Apply raw mouse input setting
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, section->Get_bool("raw_mouse_input") ? "0" : "1", SDL_HINT_OVERRIDE);
#endif

    /* Get some Event handlers */
    MAPPER_AddHandler(ResetSystem, MK_r, MMODHOST, "reset", "Reset DOSBox-X", &item); /* Host+R (Host+CTRL+R acts funny on my Linux system) */
    item->set_text("Reset virtual machine");

    MAPPER_AddHandler(RebootGuest, MK_b, MMODHOST, "reboot", "Reboot DOS system", &item); /* Reboot guest system or integrated DOS */
    item->set_text("Reboot guest system");

#if !defined(HX_DOS)
    MAPPER_AddHandler(LoadMapFile, MK_nothing, 0, "loadmap", "Load mapper file", &item);
    item->set_text("Load mapper file...");

    MAPPER_AddHandler(QuickLaunch, MK_q, MMODHOST, "quickrun", "Quick launch program", &item);
    item->set_text("Quick launch program...");
#endif

#if defined(MACOSX) && 0
    MAPPER_AddHandler(NewInstanceEvent, MK_nothing, 0, "newinst", "Start new instance", &item);
    item->set_text("Start new instance");
    {
        bool enable = false;
        extern std::string MacOSXEXEPath;
        if (!MacOSXEXEPath.empty()) {
            if (MacOSXEXEPath.at(0) == '/') {
                enable = true;
            }
        }
        item->enable(enable);
        item->refresh_item(mainMenu);
    }
#endif

#if !defined(C_EMSCRIPTEN)//FIXME: Shutdown causes problems with Emscripten
    MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","Quit from DOSBox-X", &item); /* KEEP: Most DOSBox-X users may have muscle memory for this */
    item->set_text("Quit from DOSBox-X");
#endif

    if (sdl.desktop.want_type != SCREEN_GAMELINK) {
        MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Capture mouse", &item); /* KEEP: Most DOSBox-X users may have muscle memory for this */
        item->set_text("Capture mouse");
    }

#if defined(C_SDL2) || defined(WIN32) || defined(MACOSX)
    MAPPER_AddHandler(QuickEdit,MK_nothing, 0,"fastedit", "Quick edit mode", &item);
    item->set_text("Quick edit: copy on select and paste text");

    MAPPER_AddHandler(CopyAllClipboard,MK_f5,MMOD1,"copyall", "Copy to clipboard", &item);
    item->set_text("Copy all text on the DOS screen");
#endif

    MAPPER_AddHandler(PasteClipboard,MK_f6,MMOD1,"paste", "Paste from clipboard", &item); //end emendelson; improved by Wengier
    item->set_text("Pasting from the clipboard");

    MAPPER_AddHandler(PasteClipStop,MK_nothing, 0,"pasteend", "Stop clipboard paste", &item);
    item->set_text("Stop clipboard pasting");

#if C_PRINTER
    MAPPER_AddHandler(&FormFeed, MK_f2 , MMOD1, "ejectpage", "Send form-feed", &item);
    item->set_text("Send form-feed");

    MAPPER_AddHandler(&PrintText, MK_nothing, 0, "printtext", "Print text screen", &item);
    item->set_text("Print DOS text screen");
#endif

    MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMODHOST, "pause", "Pause emulation");

    MAPPER_AddHandler(&PauseWithInterrupts_mapper_shortcut, MK_nothing, 0, "pauseints", "Pause with interrupt", &item);
    item->set_text("Pause with interrupts enabled");

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
    pause_menu_item_tag = mainMenu.get_item("mapper_pause").get_master_id() + DOSBoxMenu::nsMenuMinimumID;
#endif

    if (sdl.desktop.want_type != SCREEN_GAMELINK) {
        MAPPER_AddHandler(&GUI_Run, MK_c,MMODHOST, "gui", "Configuration tool", &item);
        item->set_text("Configuration tool");

        MAPPER_AddHandler(&MAPPER_Run,MK_m,MMODHOST,"mapper","Mapper editor",&item);
        item->set_accelerator(DOSBoxMenu::accelerator('m'));
        item->set_description("Bring up the mapper UI");
        item->set_text("Mapper editor");

        MAPPER_AddHandler(SwitchFullScreen,MK_f,MMODHOST,"fullscr","Toggle fullscreen", &item);
        item->set_text("Toggle fullscreen");

        MAPPER_AddHandler(&GUI_ResetResize, MK_backspace, MMODHOST, "resetsize", "Reset window size", &item);
        item->set_text("Reset window size");
    }

#if defined(USE_TTF)
    void DBCSSBCS_mapper_shortcut(bool pressed);
    MAPPER_AddHandler(&DBCSSBCS_mapper_shortcut, MK_nothing, 0, "dbcssbcs", "CJK: Switch between DBCS/SBCS modes", &item);
    item->set_text("CJK: Switch between DBCS/SBCS modes");

    void AutoBoxDraw_mapper_shortcut(bool pressed);
    MAPPER_AddHandler(&AutoBoxDraw_mapper_shortcut, MK_nothing, 0, "autoboxdraw", "CJK: Auto-detect box-drawing characters", &item);
    item->set_text("CJK: Auto-detect box-drawing characters");

    MAPPER_AddHandler(&TTF_IncreaseSize, MK_uparrow, MMODHOST, "incsize", "Increase TTF size", &item);
    item->set_text("Increase TTF font size");

    MAPPER_AddHandler(&TTF_DecreaseSize, MK_downarrow, MMODHOST, "decsize", "Decrease TTF size", &item);
    item->set_text("Decrease TTF font size");

    void ResetColors_mapper_shortcut(bool pressed);
    MAPPER_AddHandler(&ResetColors_mapper_shortcut, MK_nothing, 0, "resetcolor", "Reset color scheme", &item);
    item->set_text("Reset TTF color scheme");
#endif

    void GEN_PowerButton(bool pressed);
    MAPPER_AddHandler(&GEN_PowerButton,MK_nothing,0,"pwrbutton","APM power button", &item);
    item->set_text("APM power button");

    MAPPER_AddHandler(&HideMenu_mapper_shortcut, MK_escape, MMODHOST, "togmenu", "Toggle menu bar", &item);
    item->set_text("Hide/show menu bar");

    const int display = section->Get_int("display");
    int numscreen = GetNumScreen();
    if (display >= 0 && display <= numscreen)
        sdl.displayNumber = display;
    else {
        LOG_MSG("SDL: Display number should be between 0 and %d, fallback to default display", numscreen);
        sdl.displayNumber = 0;
    }
    std::string output=section->Get_string("output");

	if (output == "default") {
		output=GetDefaultOutput();
		LOG_MSG("The default output for the video system: %s", output.c_str());
	}

    const std::string emulation = section->Get_string("mouse_emulation");
    if (emulation == "always")
        sdl.mouse.emulation = MOUSE_EMULATION_ALWAYS;
    else if (emulation == "locked")
        sdl.mouse.emulation = MOUSE_EMULATION_LOCKED;
    else if (emulation == "integration")
        sdl.mouse.emulation = MOUSE_EMULATION_INTEGRATION;
    else if (emulation == "never")
        sdl.mouse.emulation = MOUSE_EMULATION_NEVER;

    usesystemcursor = section->Get_bool("usesystemcursor");

#if C_GAMELINK
    sdl.gamelink.enable = section->Get_bool("gamelink master");
    sdl.gamelink.snoop = section->Get_bool("gamelink snoop");
    sdl.gamelink.loadaddr = section->Get_int("gamelink load address");
#endif


#if C_XBRZ
    // initialize xBRZ parameters and check output type for compatibility
    xBRZ_Initialize();

    if (sdl_xbrz.enable) {
        // xBRZ requirements
        if ((output != "surface") && (output != "direct3d") && (output != "opengl") && (output != "openglhq")  && (output != "openglnb") && (output != "openglpp") && (output != "ttf") && (output != "gamelink"))
            output = "surface";
    }
#endif

    // output type selection
    // "overlay" and "ddraw" were removed, pre-map to Direct3D or OpenGL or surface
    if (output == "overlay" || output == "ddraw"
#if !C_DIRECT3D
       || output == "direct3d"
#endif
#if !C_GAMELINK
       || output == "gamelink"
#endif
#if !defined(USE_TTF)
       || output == "ttf"
#endif
    ) {
        if (output == "overlay" || output == "ddraw")
            LOG_MSG("The %s output has been removed.", output.c_str());
        else
            LOG_MSG("The %s output is not enabled.", output == "ttf" ? "TrueType font (TTF)":output.c_str());
#if C_DIRECT3D
        output = "direct3d";
#elif C_OPENGL
        output = "opengl";
#else
        output = "surface";
#endif
        LOG_MSG("The following output will be switched to: %s\n", output.c_str());
    }

    // FIXME: this selection of output is duplicated in change_output:
    sdl.desktop.isperfect = false; /* Reset before selection */
    if (output == "surface") 
    {
#if C_DIRECT3D
        if(!init_output) OUTPUT_DIRECT3D_Select();
#elif C_OPENGL && defined(MACOSX) // JC: This is breaking SDL1 on Linux! Limit this to MAC OS! Windows users may use this if needed.
        if(!init_output) OUTPUT_OPENGL_Select(GLBilinear); // Initialize screen before switching to TTF (required for macOS builds)     
#endif
        OUTPUT_SURFACE_Select();
        init_output = true;
#if C_OPENGL
    } 
    else if (output == "opengl" || output == "openglhq") 
    {
        OUTPUT_OPENGL_Select(GLBilinear);
    }
    else if (output == "openglnb") 
    {
        OUTPUT_OPENGL_Select(GLNearest);
    }
    else if (output == "openglpp")
    {
        OUTPUT_OPENGL_Select(GLPerfect);
#endif
#if C_GAMELINK
    } 
    else if (output == "gamelink") 
    {
        OUTPUT_GAMELINK_Select();
#endif
#if C_DIRECT3D
    } 
    else if (output == "direct3d") 
    {
        OUTPUT_DIRECT3D_Select();
#if LOG_D3D
        LOG_MSG("SDL: Direct3D activated");
#endif
#endif
    }
#if defined(USE_TTF)
    else if (output == "ttf")
    {
        LOG_MSG("SDL(sdlmain.cpp): TTF activated");
#if ((WIN32 && !defined(C_SDL2)) || MACOSX) && C_OPENGL
        if(!init_output) OUTPUT_OPENGL_Select(GLBilinear); // Initialize screen before switching to TTF (required for Windows & macOS builds)
#endif
        OUTPUT_TTF_Select(0);
        init_output = true;
    }
#endif
    else 
    {
        LOG_MSG("SDL: Unsupported output device %s, switching back to surface",output.c_str());
#if MACOSX && C_OPENGL
        if(!init_output) OUTPUT_OPENGL_Select(GLBilinear); // Initialize screen before switching to surface (required for macOS builds)     
#endif
        OUTPUT_SURFACE_Select(); // should not reach there anymore
        init_output = true;
    }

    sdl.overscan_width=(unsigned int)section->Get_int("overscan");
//  sdl.overscan_color=section->Get_int("overscancolor");

    // Getting window position (if configured)
    posx = -1;
    posy = -1;
    const char* windowposition = section->Get_string("windowposition");
    LOG_MSG("Configured windowposition: %s", windowposition);
    if (windowposition && !strcmp(windowposition, "-"))
        posx = posy = -2;
    else if (windowposition && *windowposition && strcmp(windowposition, ",")) {
        char result[100];
        safe_strncpy(result, windowposition, sizeof(result));
        char* y = strchr(result, ',');
        if (y && *y) {
            *y = 0;
            posx = atoi(result);
            posy = atoi(y + 1);
        }
    }

    // Setting SDL1 window position before a call to SDL_SetVideoMode() is made. If the user provided
    // SDL_VIDEO_WINDOW_POS environment variable then "windowposition" setting should have no effect.
    // SDL2 position is set later, using SDL_SetWindowPosition()
#if !defined(C_SDL2)
#if defined(WIN32) && !defined(HX_DOS)
    MONITORINFO info;
    if (sdl.displayNumber>0) {
        xyp xy={0};
        xy.x=-1;
        xy.y=-1;
        curscreen=0;
        EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
        HMONITOR monitor = MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST);
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &info);
        if (posx >=0 && posy >=0) {
            posx+=info.rcMonitor.left;
            posy+=info.rcMonitor.top;
        }
    }
#endif
    char pos[100];
    if (posx >= 0 && posy >= 0 && SDL_getenv("SDL_VIDEO_WINDOW_POS") == NULL) {
#if defined(WIN32)
        safe_strncpy(pos, "SDL_VIDEO_WINDOW_POS=", sizeof(pos));
        safe_strcat(pos, (std::to_string(posx)+","+std::to_string(posy)).c_str());
        SDL_putenv(pos);
#else
        setenv("SDL_VIDEO_WINDOW_POS",(std::to_string(posx)+","+std::to_string(posy)).c_str(),0);
#endif
#if defined(WIN32) && !defined(HX_DOS)
    } else if (sdl.displayNumber>0 && !(posx == -2 && posy == -2)) {
        safe_strncpy(pos, "SDL_VIDEO_WINDOW_POS=", sizeof(pos));
        safe_strcat(pos, (std::to_string(info.rcMonitor.left+200)+","+std::to_string(info.rcMonitor.top+200)).c_str());
        SDL_putenv(pos);
#endif
    } else if (posx != -2 || posy != -2)
        putenv((char*)"SDL_VIDEO_CENTERED=center");
#endif

/* Initialize screen for first time */
#if defined(C_SDL2)
    if (!initgl) {
        GFX_SetResizeable(true);
        if (!GFX_SetSDLSurfaceWindow(640,400))
            E_Exit("Could not initialize video: %s",SDL_GetError());
        sdl.surface = SDL_GetWindowSurface(sdl.window);
    }
    //SDL_Rect splash_rect=GFX_GetSDLSurfaceSubwindowDims(640,400);
    sdl.desktop.pixelFormat = SDL_GetWindowPixelFormat(sdl.window);
    LOG_MSG("SDL: Current window pixel format: %s", SDL_GetPixelFormatName(sdl.desktop.pixelFormat));
    sdl.desktop.bpp=8*SDL_BYTESPERPIXEL(sdl.desktop.pixelFormat);
    if (SDL_BITSPERPIXEL(sdl.desktop.pixelFormat) == 24)
        LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
#else
    if (!initgl) {
        sdl.surface=SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
        if (sdl.surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
    }
    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;
    sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
    if (sdl.desktop.bpp==24)
        LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
#endif

    GFX_LogSDLState();
    GFX_Stop();

#if C_DIRECT3D
    if (sdl.desktop.want_type == SCREEN_DIRECT3D)
        d3d_init();
#endif

#if defined(C_SDL2)
    SDL_SetWindowTitle(sdl.window,"DOSBox-X");
    if (posx >= 0 && posy >= 0) {
        if (sdl.displayNumber>0) {
            int displays = SDL_GetNumVideoDisplays();
            SDL_Rect bound;
            for( int i = 1; i <= displays; i++ ) {
                bound = SDL_Rect();
                SDL_GetDisplayBounds(i-1, &bound);
                if (i == sdl.displayNumber) {
                    posx += bound.x;
                    posy += bound.y;
                    break;
                }
            }
        }
        SDL_SetWindowPosition(sdl.window, posx, posy);
    }
#else
    SDL_WM_SetCaption("DOSBox-X",VERSION);
#endif

    /* Please leave the Splash screen stuff in working order in DOSBox-X. We spend a lot of time making DOSBox-X. */
    //ShowSplashScreen();   /* I will keep the splash screen alive. But now, the BIOS will do it --J.C. */

#if defined(WIN32) && !defined(HX_DOS)
    if (section->Get_bool("forcesquarecorner") && UpdateWindows11RoundCorners(GetHWND(), CornerPreference::DoNotRound))
        LOG_MSG("SDL: Windows 11 round corners will be disabled.");
#endif
    transparency = 0;
    SetWindowTransparency(section->Get_int("transparency"));
    UpdateWindowDimensions();
}

void Mouse_AutoLock(bool enable) {
#if C_GAMELINK
    if (sdl.desktop.type == SCREEN_GAMELINK) {
        // store the 'want mouse' state
        sdl.gamelink.want_mouse = enable;
        return;
    }
#endif // C_GAMELINK

    if (sdl.mouse.autolock == enable)
        return;

    sdl.mouse.autolock=enable;
    if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
    else {
        SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
        sdl.mouse.requestlock=false;
    }
}

bool Mouse_IsLocked()
{
    return sdl.mouse.locked;
}

static void RedrawScreen(uint32_t nWidth, uint32_t nHeight) {
    (void)nWidth;//UNUSED
    (void)nHeight;//UNUSED
//  int width;
//  int height;
#ifdef __WIN32__
//   width=sdl.clip.w; 
//   height=sdl.clip.h;
#else
//  width=sdl.draw.width; 
//  height=sdl.draw.height;
#endif
    void RENDER_CallBack( GFX_CallBackFunctions_t function );
#if 0
    while (sdl.desktop.fullscreen) {
        int temp_size;
        temp_size=render.scale.size;
        if(!sdl.desktop.fullscreen) { render.scale.size=temp_size; RENDER_CallBack( GFX_CallBackReset); return; }
    }
#endif
#ifdef WIN32
    if(menu.resizeusing) {
        RENDER_CallBack( GFX_CallBackReset);
        return;
    }
#endif
#if 0 /* FIXME: This code misbehaves when doublescan=false on Linux/X11 */
    if((Bitu)nWidth == (Bitu)width && (Bitu)nHeight == (Bitu)height) {
        RENDER_CallBack( GFX_CallBackReset);
        return;
    }
    Section_prop * section=static_cast<Section_prop *>(control->GetSection("sdl")); 
    if ((!strcmp(section->Get_string("windowresolution"),"original") || (!strcmp(section->Get_string("windowresolution"),"desktop"))) && (render.src.dblw && render.src.dblh)) {
        switch (render.scale.op) {
            case scalerOpNormal:
                if(!render.scale.hardware) {
                    if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) {
                        if (render.scale.size <= 4 && render.scale.size >=1) ++render.scale.size; break;
                    } else {
                        if (render.scale.size <= 5 && render.scale.size >= 2) --render.scale.size; break;
                    }
                } else {
                    if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) {
                        if (render.scale.size == 1) { render.scale.size=4; break; }
                        if (render.scale.size == 4) { render.scale.size=6; break; }
                        if (render.scale.size == 6) { render.scale.size=8; break; }
                        if (render.scale.size == 8) { render.scale.size=10; break; }
                    }
                    if((Bitu)nWidth<(Bitu)width || (Bitu)nHeight<(Bitu)height) {
                        if (render.scale.size == 10) { render.scale.size=8; break; }
                        if (render.scale.size == 8) { render.scale.size=6; break; }
                        if (render.scale.size == 6) { render.scale.size=4; break; }
                        if (render.scale.size == 4) { render.scale.size=1; break; }
                    }
                }
                break;
            case scalerOpAdvMame:
            case scalerOpHQ:
            case scalerOpAdvInterp:
            case scalerOpTV:
            case scalerOpRGB:
            case scalerOpScan:
                if((Bitu)nWidth>(Bitu)width || (Bitu)nHeight>(Bitu)height) { if (render.scale.size == 2) ++render.scale.size; }
                if((Bitu)nWidth<(Bitu)width || (Bitu)nHeight<(Bitu)height) { if (render.scale.size == 3) --render.scale.size; }
                break;
            case scalerOpSaI:
            case scalerOpSuperSaI:
            case scalerOpSuperEagle:
            default: // other scalers
                break;
        }
    }
#endif
    RENDER_CallBack( GFX_CallBackReset);
}

void GFX_RedrawScreen(uint32_t nWidth, uint32_t nHeight) {
    RedrawScreen(nWidth, nHeight);
}

bool GFX_MustActOnResize() {
    if (!GFX_IsFullscreen())
        return false;

    return true;
}

#if defined(C_SDL2)
void GFX_HandleVideoResize(int width, int height) {

    /* don't act if 3Dfx OpenGL emulation is active */
    if (GFX_GetPreventFullscreen()) {
#if C_OPENGL
        if (new_width == width && new_height == height) return;
        new_width = width;
        new_height = height;
        voodoo_ogl_update_dimensions();
#endif
        return;
    }

    /* Maybe a screen rotation has just occurred, so we simply resize.
       There may be a different cause for a forced resized, though.    */
    if (sdl.desktop.full.display_res && IsFullscreen()) {
        /* Note: We should not use GFX_ObtainDisplayDimensions
           (SDL_GetDisplayBounds) on Android after a screen rotation:
           The older values from application startup are returned. */
        sdl.desktop.full.width = width;
        sdl.desktop.full.height = height;
    }

    /* assume the resize comes from user preference UNLESS the window
     * is fullscreen or maximized */
    if (!menu.maxwindow && !sdl.desktop.fullscreen && !sdl.init_ignore && NonUserResizeCounter == 0 && !window_was_maximized) {
        UpdateWindowDimensions();
        UpdateWindowDimensions((unsigned int)width, (unsigned int)height);

        /* if the dimensions actually changed from our surface dimensions, then
           assume it's the user's input. Linux/X11 is good at doing this anyway,
           but the Windows SDL 1.x support will return us a resize event for the
           window size change resulting from SDL mode set. */
        if (width != sdl.surface->w || height != sdl.surface->h) {
            userResizeWindowWidth = (unsigned int)width;
            userResizeWindowHeight = (unsigned int)height;
        }
    }
    else {
        UpdateWindowDimensions();
        if (window_was_maximized && !menu.maxwindow) {
            userResizeWindowWidth = currentWindowWidth==sdl.surface->w?0:(unsigned int)currentWindowWidth;
            userResizeWindowHeight = currentWindowHeight==sdl.surface->h?0:(unsigned int)currentWindowHeight;
        }
    }

    /* TODO: Only if FULLSCREEN_DESKTOP */
    if (screen_size_info.screen_dimensions_pixels.width != 0 && screen_size_info.screen_dimensions_pixels.height != 0) {
        sdl.desktop.full.width = (uint16_t)screen_size_info.screen_dimensions_pixels.width;
        sdl.desktop.full.height = (uint16_t)screen_size_info.screen_dimensions_pixels.height;
    }
    else {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0/*FIXME display index*/,&dm) == 0) {
            sdl.desktop.full.width = dm.w;
            sdl.desktop.full.height = dm.h;
            LOG_MSG("SDL2 reports desktop display mode %u x %u",dm.w,dm.h);
        }
        else {
            LOG_MSG("SDL2 unable to determine desktop display mode, error %s",SDL_GetError());
        }
    }

    window_was_maximized = menu.maxwindow;
    if (NonUserResizeCounter > 0)
        NonUserResizeCounter--;

    /* Even if the new window's dimensions are actually the desired ones
     * we may still need to re-obtain a new window surface or do
     * a different thing. So we basically call GFX_SetSize, but without
     * touching the window itself (or else we may end in an infinite loop).
     *
     * Furthermore, if the new dimensions are *not* the desired ones, we
     * don't fight it. Rather than attempting to resize it back, we simply
     * keep the window as-is and disable screen updates. This is done
     * in SDL_SetSDLWindowSurface by setting sdl.update_display_contents
     * to false.
     */
    sdl.update_window = false;
    GFX_ResetScreen();
    sdl.update_window = true;
}
#else
static void HandleVideoResize(void * event) {
    if(sdl.desktop.fullscreen) return;

    /* don't act on resize events if we made the window non-resizeable.
     * especially if 3Dfx voodoo emulation is active. */
    if (!(sdl.surface->flags & SDL_RESIZABLE)) return;

    SDL_ResizeEvent* ResizeEvent = (SDL_ResizeEvent*)event;

    /* don't act if 3Dfx OpenGL emulation is active */
    if (GFX_GetPreventFullscreen()) {
#if C_OPENGL
        if (new_width == ResizeEvent->w && new_height == ResizeEvent->h) return;
        new_width = ResizeEvent->w;
        new_height = ResizeEvent->h;
        voodoo_ogl_update_dimensions();
#endif
        return;
    }

    /* assume the resize comes from user preference UNLESS the window
     * is fullscreen or maximized */
    if (!menu.maxwindow && !sdl.desktop.fullscreen && !sdl.init_ignore && NonUserResizeCounter == 0 && !window_was_maximized) {
        UpdateWindowDimensions();
        UpdateWindowDimensions((unsigned int)ResizeEvent->w, (unsigned int)ResizeEvent->h);

        /* if the dimensions actually changed from our surface dimensions, then
           assume it's the user's input. Linux/X11 is good at doing this anyway,
           but the Windows SDL 1.x support will return us a resize event for the
           window size change resulting from SDL mode set. */
        if (ResizeEvent->w != sdl.surface->w || ResizeEvent->h != sdl.surface->h) {
            userResizeWindowWidth = (unsigned int)ResizeEvent->w;
            userResizeWindowHeight = (unsigned int)ResizeEvent->h;
        }
    }
    else {
        UpdateWindowDimensions();
    }

    window_was_maximized = menu.maxwindow;
    if (NonUserResizeCounter > 0)
        NonUserResizeCounter--;

    if (sdl.updating && !GFX_MustActOnResize()) {
        /* act on resize when updating is complete */
        sdl.deferred_resize = true;
    }
    else {
        sdl.deferred_resize = false;
        RedrawScreen((unsigned int)ResizeEvent->w, (unsigned int)ResizeEvent->h);
    }
#ifdef WIN32
    menu.resizeusing=false;
#endif
}
#endif

extern unsigned int mouse_notify_mode;

bool user_cursor_locked = false;
MOUSE_EMULATION user_cursor_emulation = MOUSE_EMULATION_NEVER;
int user_cursor_x = 0,user_cursor_y = 0;
int user_cursor_sw = 640,user_cursor_sh = 480;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id) {
    (void)menu;//UNUSED
    if (mainMenu.menuUserHoverAt != item_id) {
        if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle) {
            auto &item = mainMenu.get_item(mainMenu.menuUserHoverAt);
            item.setHover(mainMenu,false);
            if (item.checkResetRedraw()) {
                item.drawMenuItem(mainMenu);
                item.updateScreenFromItem(mainMenu);
            }
        }

        mainMenu.menuUserHoverAt = item_id;

        if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle) {
            auto &item = mainMenu.get_item(mainMenu.menuUserHoverAt);
            item.setHover(mainMenu,true);
            if (item.checkResetRedraw()) {
                item.drawMenuItem(mainMenu);
                item.updateScreenFromItem(mainMenu);
            }
        }

        if (OpenGL_using())
            mainMenu.setRedraw();
    }
}

void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id) {
    (void)menu;//UNUSED
    if (mainMenu.menuUserAttentionAt != item_id) {
        if (mainMenu.menuUserAttentionAt != DOSBoxMenu::unassigned_item_handle) {
            auto &item = mainMenu.get_item(mainMenu.menuUserAttentionAt);
            item.setHilight(mainMenu,false);
            if (item.checkResetRedraw()) {
                item.drawMenuItem(mainMenu);
                item.updateScreenFromItem(mainMenu);
            }
        }

        mainMenu.menuUserAttentionAt = item_id;

        if (mainMenu.menuUserAttentionAt != DOSBoxMenu::unassigned_item_handle) {
            auto &item = mainMenu.get_item(mainMenu.menuUserAttentionAt);
            item.setHilight(mainMenu,true);
            if (item.checkResetRedraw()) {
                item.drawMenuItem(mainMenu);
                item.updateScreenFromItem(mainMenu);
            }
        }

        if (OpenGL_using())
            mainMenu.setRedraw();
    }
}
#endif

uint8_t Mouse_GetButtonState(void);

bool GFX_CursorInOrNearScreen(int wx,int wy) {
#if defined(USE_TTF)
    if (ttf.inUse) return true;
#endif
    int minx = sdl.clip.x - (sdl.clip.w / 10);
    int miny = sdl.clip.y - (sdl.clip.h / 10);
    int maxx = sdl.clip.x + sdl.clip.w + (sdl.clip.w / 10);
    int maxy = sdl.clip.y + sdl.clip.h + (sdl.clip.h / 10);

    return  (wx >= minx && wx < maxx) && (wy >= miny && wy < maxy);
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
    bool inputToScreen = false;

    /* limit mouse input to whenever the cursor is on the screen, or near the edge of the screen. */
    if (is_paused)
        inputToScreen = false;
    else if (vmware_mouse || sdl.mouse.locked || Mouse_GetButtonState() != 0)
        inputToScreen = true;
    else {
        inputToScreen = GFX_CursorInOrNearScreen(motion->x,motion->y);
#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
		if (mouse_start_x >= 0 && mouse_start_y >= 0) {
			if (fx>=0 && fy>=0)
				Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,sdl.clip.w,sdl.clip.h, false);
			Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,motion->x-sdl.clip.x,motion->y-sdl.clip.y,sdl.clip.w,sdl.clip.h, true);
			fx=motion->x;
			fy=motion->y;
		}
#endif
	}

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
    if (GFX_GetPreventFullscreen()) {
        /* do NOT draw SDL menu in OpenGL mode when 3Dfx emulation is using OpenGL */
    }
    else if (!sdl.mouse.locked && !sdl.desktop.fullscreen && mainMenu.isVisible() && motion->y < mainMenu.menuBox.h && Mouse_GetButtonState() == 0) {
        skipdraw=true;
        GFX_SDLMenuTrackHover(mainMenu,mainMenu.display_list.itemFromPoint(mainMenu,motion->x,motion->y));
        skipdraw=false;
        SDL_ShowCursor(SDL_ENABLE);

        if (OpenGL_using() && mainMenu.needsRedraw()) {
#if C_OPENGL
            sdl_opengl.menudraw_countdown = 2; // two GL buffers
            GFX_OpenGLRedrawScreen();
            GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
# if defined(C_SDL2)
            SDL_GL_SwapWindow(sdl.window);
# else
            SDL_GL_SwapBuffers();
# endif
#endif
        }
 
        return;
    }
    else {
        skipdraw=true;
        GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
        skipdraw=false;

        if (OpenGL_using() && mainMenu.needsRedraw()) {
#if C_OPENGL
            sdl_opengl.menudraw_countdown = 2; // two GL buffers
            GFX_OpenGLRedrawScreen();
            GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
# if defined(C_SDL2)
            SDL_GL_SwapWindow(sdl.window);
# else
            SDL_GL_SwapBuffers();
# endif
#endif
        }
    }
#endif

    if (!inputToScreen) {
#if defined(C_SDL2)
        if (!sdl.mouse.locked)
#else
        /* SDL1 has some sort of weird mouse warping bug in fullscreen mode no matter whether the mouse is captured or not (Windows, Linux/X11) */
        if (!sdl.mouse.locked && !sdl.desktop.fullscreen)
#endif
            SDL_ShowCursor(SDL_ENABLE);
 
        return;
    }

    user_cursor_x      = motion->x - sdl.clip.x;
    user_cursor_y      = motion->y - sdl.clip.y;
    user_cursor_locked = sdl.mouse.locked;
    user_cursor_emulation = sdl.mouse.emulation;
    user_cursor_sw     = sdl.clip.w;
    user_cursor_sh     = sdl.clip.h;

    auto xrel = static_cast<float>(motion->xrel) * sdl.mouse.xsensitivity / 100.0f;
    auto yrel = static_cast<float>(motion->yrel) * sdl.mouse.ysensitivity / 100.0f;
    auto x    = static_cast<float>(motion->x - sdl.clip.x) / (sdl.clip.w - 1) * sdl.mouse.xsensitivity / 100.0f;
    auto y    = static_cast<float>(motion->y - sdl.clip.y) / (sdl.clip.h - 1) * sdl.mouse.ysensitivity / 100.0f;
    auto emu  = sdl.mouse.locked;

    const auto inside =
        motion->x >= sdl.clip.x && motion->x < sdl.clip.x + sdl.clip.w &&
        motion->y >= sdl.clip.y && motion->y < sdl.clip.y + sdl.clip.h;

    if (mouse_notify_mode != 0)
    {
        /* for mouse integration driver */
        if (!sdl.mouse.locked)
            xrel = yrel = x = y = 0.0f;

        emu               = sdl.mouse.locked;
        const auto isdown = Mouse_GetButtonState() != 0;

#if defined(C_SDL2)
        if (!vmware_mouse && !sdl.mouse.locked)
#else
        /* SDL1 has some sort of weird mouse warping bug in fullscreen mode no matter whether the mouse is captured or not (Windows, Linux/X11) */
        if (!vmware_mouse && !sdl.mouse.locked && !sdl.desktop.fullscreen)
#endif
            SDL_ShowCursor((isdown || inside) ? SDL_DISABLE : SDL_ENABLE);
    }
    else if (!user_cursor_locked)
    {
        extern bool MOUSE_HasInterruptSub();
        extern bool MOUSE_IsBeingPolled();
        extern bool MOUSE_IsHidden();
        /* Show only when DOS app is not using mouse */

#if defined(C_SDL2)
        if (!vmware_mouse && !sdl.mouse.locked)
#else
        /* SDL1 has some sort of weird mouse warping bug in fullscreen mode no matter whether the mouse is captured or not (Windows, Linux/X11) */
        if (!vmware_mouse && !sdl.mouse.locked && !sdl.desktop.fullscreen)
#endif
            SDL_ShowCursor(((!inside) || ((MOUSE_IsHidden()) && !(MOUSE_IsBeingPolled() || MOUSE_HasInterruptSub()))) ? SDL_ENABLE : SDL_DISABLE);
    }
    Mouse_CursorMoved(xrel, yrel, x, y, emu);
    VMWARE_MousePosition(motion->x, motion->y);
}

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
void MenuFullScreenRedraw(void) {
#if defined(C_SDL2)
    SDL_UpdateWindowSurface(sdl.window);
#else
    SDL_Flip(sdl.surface);
#endif
}

static struct {
    unsigned char*      bmp = NULL;
    unsigned int        stride = 0,height = 0;
} menuSavedScreen;

void MenuSaveScreen(void) {
    if (!OpenGL_using()) {
        if (menuSavedScreen.bmp == NULL) {
            menuSavedScreen.height = (unsigned int)sdl.surface->h;
            menuSavedScreen.stride = (unsigned int)sdl.surface->pitch;
            menuSavedScreen.bmp = new unsigned char[menuSavedScreen.height * menuSavedScreen.stride];
        }

        if (SDL_MUSTLOCK(sdl.surface))
            SDL_LockSurface(sdl.surface);

        memcpy(menuSavedScreen.bmp, sdl.surface->pixels, menuSavedScreen.height * menuSavedScreen.stride);

        if (SDL_MUSTLOCK(sdl.surface))
            SDL_UnlockSurface(sdl.surface);
    }
}

void MenuRestoreScreen(void) {
    if (!OpenGL_using()) {
        if (menuSavedScreen.bmp == NULL)
            return;

        if (SDL_MUSTLOCK(sdl.surface))
            SDL_LockSurface(sdl.surface);

        memcpy(sdl.surface->pixels, menuSavedScreen.bmp, menuSavedScreen.height * menuSavedScreen.stride);

        if (SDL_MUSTLOCK(sdl.surface))
            SDL_UnlockSurface(sdl.surface);
    }
}

void MenuFreeScreen(void) {
    if (menuSavedScreen.bmp == NULL)
        return;

    delete[] menuSavedScreen.bmp;
    menuSavedScreen.bmp = NULL;
}
#endif

static void HandleMouseWheel(bool normal, int amount) {
    if (amount != 0) {
        Mouse_WheelMoved(normal ? -amount : amount);
        if (vmware_mouse) VMWARE_MouseWheel(normal ? -amount : amount);
    }
}

static void HandleMouseButton(SDL_MouseButtonEvent * button, SDL_MouseMotionEvent * motion) {
#if !defined(WIN32)
    (void)motion;
#endif
    bool inputToScreen = false;
    bool inMenu = false;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
    if (GFX_GetPreventFullscreen()) {
        /* do NOT draw SDL menu in OpenGL mode when 3Dfx emulation is using OpenGL */
        mainMenu.menuUserHoverAt = DOSBoxMenu::unassigned_item_handle;
    }
    else if (!sdl.mouse.locked && !sdl.desktop.fullscreen && mainMenu.isVisible() && button->y < mainMenu.menuBox.h) {
        GFX_SDLMenuTrackHover(mainMenu,mainMenu.display_list.itemFromPoint(mainMenu,button->x,button->y));
        inMenu = true;
    }
    else {
        GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
    }

    if (button->button == SDL_BUTTON_LEFT) {
        if (button->state == SDL_PRESSED) {
            GFX_SDLMenuTrackHilight(mainMenu,mainMenu.menuUserHoverAt);
            if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle) {
                std::vector<DOSBoxMenu::item_handle_t> popup_stack;
                DOSBoxMenu::item_handle_t choice_item;
                DOSBoxMenu::item_handle_t psel_item;
                DOSBoxMenu::item_handle_t sel_item;
                bool button_holding=true;
                bool redrawAll=false;
                bool resized=false;
                bool runloop=true;
                SDL_Rect uprect;
                SDL_Event event;

                psel_item = DOSBoxMenu::unassigned_item_handle;
                choice_item = mainMenu.menuUserHoverAt = mainMenu.menuUserAttentionAt;

                popup_stack.push_back(mainMenu.menuUserAttentionAt);

#if C_DIRECT3D
        if (sdl.desktop.want_type == SCREEN_DIRECT3D) {
            /* In output=direct3d mode, SDL still has a surface but this code ignores SDL
             * and draws directly to a Direct3D9 backbuffer which is presented to the window
             * client area. However, GDI output to the window still works, and this code
             * uses the SDL surface still. Therefore, for menus to draw correctly atop the
             * Direct3D output, this code copies the Direct3D backbuffer to the SDL surface
             * first.
             *
             * WARNING: This happens to work with Windows (even Windows 10 build 18xx as of
             * 2018/05/21) because Windows appears to permit mixing Direct3D and GDI rendering
             * to the window.
             *
             * Someday, if Microsoft should break that ability, this code will need to be
             * revised to send screen "updates" to the Direct3D backbuffer first, then
             * Present to the window client area. */
            if (d3d) d3d->UpdateRectToSDLSurface(0, 0, sdl.surface->w, sdl.surface->h);
        }
#endif

                if (OpenGL_using()) {
#if C_OPENGL
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHilight(mainMenu,false);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHover(mainMenu,false);

                    /* show the menu */
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHilight(mainMenu,true);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHover(mainMenu,true);

                    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);

                    GFX_OpenGLRedrawScreen();

                    /* give the menu bar a drop shadow */
                    MenuShadeRect(
                            (int)mainMenu.menuBox.x + (int)DOSBoxMenu::dropshadowX,
                            (int)mainMenu.menuBox.y + (int)mainMenu.menuBox.h,
                            (int)mainMenu.menuBox.w,
                            (int)DOSBoxMenu::dropshadowY - 1/*menubar border*/);

                    mainMenu.setRedraw();                  
                    GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);

                    for (auto i=popup_stack.begin();i!=popup_stack.end();i++) {
                        if (mainMenu.get_item(*i).get_type() == DOSBoxMenu::submenu_type_id) {
                            mainMenu.get_item(*i).drawBackground(mainMenu);
                            mainMenu.get_item(*i).display_list.DrawDisplayList(mainMenu,/*updateScreen*/false);
                        }
                    }

# if defined(C_SDL2)
                    SDL_GL_SwapWindow(sdl.window);
# else
                    SDL_GL_SwapBuffers();
# endif
#endif
                }
                else {
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHilight(mainMenu,false);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHover(mainMenu,false);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).drawMenuItem(mainMenu);
                    MenuSaveScreen();

                    /* give the menu bar a drop shadow */
                    MenuShadeRect(
                            (int)mainMenu.menuBox.x + (int)DOSBoxMenu::dropshadowX,
                            (int)mainMenu.menuBox.y + (int)mainMenu.menuBox.h,
                            (int)mainMenu.menuBox.w,
                            (int)DOSBoxMenu::dropshadowY - 1/*menubar border*/);

                    uprect.x = 0;
                    uprect.y = mainMenu.menuBox.y + mainMenu.menuBox.h;
                    uprect.w = mainMenu.menuBox.w;
                    uprect.h = DOSBoxMenu::dropshadowY;
#if defined(C_SDL2)
                    SDL_UpdateWindowSurfaceRects(sdl.window, &uprect, 1);
#else
                    SDL_UpdateRects( sdl.surface, 1, &uprect );
#endif

                    /* show the menu */
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHilight(mainMenu,true);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).setHover(mainMenu,true);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).drawBackground(mainMenu);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).display_list.DrawDisplayList(mainMenu,/*updateScreen*/false);
                    mainMenu.get_item(mainMenu.menuUserAttentionAt).updateScreenFromPopup(mainMenu);
                }

                /* hack */
                mainMenu.menuUserAttentionAt = DOSBoxMenu::unassigned_item_handle;

                /* fall into another loop to process the menu */
                while (runloop) {
#if C_EMSCRIPTEN
                    emscripten_sleep(0);
                    if (!SDL_PollEvent(&event)) continue;
#else
                    if (!SDL_WaitEvent(&event)) break;
#endif

#if defined(C_SDL2) && !defined(IGNORE_TOUCHSCREEN)
                    switch (event.type) {
                        case SDL_FINGERDOWN:
                            if (touchscreen_finger_lock == no_finger_id &&
                                touchscreen_touch_lock == no_touch_id) {
                                touchscreen_finger_lock = event.tfinger.fingerId;
                                touchscreen_touch_lock = event.tfinger.touchId;
                                Sint32 x,y;

                                x = (Sint32)(event.tfinger.x * currentWindowWidth);
                                y = (Sint32)(event.tfinger.y * currentWindowHeight);

                                memset(&event.button,0,sizeof(event.button));
                                event.type = SDL_MOUSEBUTTONDOWN;
                                event.button.x = x;
                                event.button.y = y;
                            }
                            else {
                                event.type = -1;
                            }
                            break;
                        case SDL_FINGERUP:
                            if (touchscreen_finger_lock == event.tfinger.fingerId &&
                                touchscreen_touch_lock == event.tfinger.touchId) {
                                touchscreen_finger_lock = no_finger_id;
                                touchscreen_touch_lock = no_touch_id;
                                Sint32 x,y;

                                x = (Sint32)(event.tfinger.x * currentWindowWidth);
                                y = (Sint32)(event.tfinger.y * currentWindowHeight);
                                
                                memset(&event.button,0,sizeof(event.button));
                                event.type = SDL_MOUSEBUTTONUP;
                                event.button.x = x;
                                event.button.y = y;
                            }
                            else {
                                event.type = -1;
                            }
                            break;
                        case SDL_FINGERMOTION:
                            if (touchscreen_finger_lock == event.tfinger.fingerId &&
                                touchscreen_touch_lock == event.tfinger.touchId) {
                                Sint32 x,y;

                                x = (Sint32)(event.tfinger.x * currentWindowWidth);
                                y = (Sint32)(event.tfinger.y * currentWindowHeight);

                                memset(&event.button,0,sizeof(event.button));
                                event.type = SDL_MOUSEMOTION;
                                event.button.x = x;
                                event.button.y = y;
                            }
                            else if (touchscreen_finger_lock != no_finger_id ||
                                     touchscreen_touch_lock != no_touch_id) {
                                event.type = -1;
                            }
                            break;
                    }
#endif

                    switch (event.type) {
                        case SDL_QUIT:
                            if (CheckQuit()) throw(0);
                            break;
                        case SDL_KEYUP:
                            if (event.key.keysym.sym == SDLK_ESCAPE) {
                                choice_item = DOSBoxMenu::unassigned_item_handle;
                                runloop = false;
                            }
                            break;
#if defined(C_SDL2)
                        case SDL_WINDOWEVENT:
                            switch (event.window.event) {
                                case SDL_WINDOWEVENT_RESIZED:
                                    GFX_HandleVideoResize(event.window.data1, event.window.data2);
                                    runloop = false;
                                    resized = true;
                                    break;
                                default:
                                    break;
                            }
#endif
#if !defined(C_SDL2)
                        case SDL_VIDEORESIZE:
                            UpdateWindowDimensions(); // FIXME: Use SDL window dimensions, except that on Windows, SDL won't tell us our actual dimensions
                            HandleVideoResize(&event.resize);

                            runloop = false;
                            resized = true;
                            break;
#endif
                        case SDL_MOUSEBUTTONDOWN:
                            button_holding=true;
                            choice_item = mainMenu.menuUserHoverAt;
                            if (choice_item != DOSBoxMenu::unassigned_item_handle) {
                                DOSBoxMenu::item &item = mainMenu.get_item(choice_item);
                                item.setHilight(mainMenu,true);
                                item.drawMenuItem(mainMenu);
                                if (OpenGL_using())
                                    redrawAll = true;
                                else
                                    item.updateScreenFromItem(mainMenu);
                            }
                            else {
                                /* clicking on nothing should dismiss */
                                runloop = false;
                            }
                            break;
                        case SDL_MOUSEBUTTONUP:
                            button_holding=false;
                            choice_item = mainMenu.menuUserHoverAt;
                            if (choice_item != DOSBoxMenu::unassigned_item_handle) {
                                if (choice_item == psel_item) { /* clicking something twice should dismiss */
                                    runloop = false;
                                }
                                else {
                                    DOSBoxMenu::item &item = mainMenu.get_item(choice_item);
                                    if (item.get_type() == DOSBoxMenu::item_type_id && item.is_enabled())
                                        runloop = false;
                                }

                                psel_item = choice_item;
                            }
                            else {
                                /* not selecting anything counts as a reason to exit */
                                runloop = false;
                            }
                            break;
                        case SDL_MOUSEMOTION:
                            {
                                sel_item = DOSBoxMenu::unassigned_item_handle;

                                auto search = popup_stack.end();
                                if (search != popup_stack.begin()) {
                                    do {
                                        search--;

                                        sel_item = mainMenu.get_item(*search).display_list.itemFromPoint(mainMenu,event.button.x,event.button.y);
                                        if (sel_item != DOSBoxMenu::unassigned_item_handle) {
                                            assert(search != popup_stack.end());
                                            search++;
                                            break;
                                        }
                                    } while (search != popup_stack.begin());
                                }

                                if (sel_item == DOSBoxMenu::unassigned_item_handle)
                                    sel_item = mainMenu.display_list.itemFromPoint(mainMenu,event.button.x,event.button.y);

                                /* at this point:
                                 *  sel_item = item under cursor, or unassigned if no item
                                 *  search = iterator just past the item's level (to remove items if changing) */

                                if (mainMenu.menuUserHoverAt != sel_item) {
                                    bool noRedrawNew = false,noRedrawOld = false;

                                    if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle) {
                                        mainMenu.get_item(mainMenu.menuUserHoverAt).setHover(mainMenu,false);
                                        if (mainMenu.get_item(mainMenu.menuUserHoverAt).get_type() == DOSBoxMenu::item_type_id)
                                            mainMenu.get_item(mainMenu.menuUserHoverAt).setHilight(mainMenu,false);
                                        else if (mainMenu.get_item(mainMenu.menuUserHoverAt).get_type() == DOSBoxMenu::submenu_type_id) {
                                            if (mainMenu.get_item(mainMenu.menuUserHoverAt).isHilight()) {
                                                noRedrawOld = true;
                                            }
                                        }
                                    }

                                    if (sel_item != DOSBoxMenu::unassigned_item_handle) {
                                        if (mainMenu.get_item(sel_item).get_type() == DOSBoxMenu::submenu_type_id) {
                                            if (!mainMenu.get_item(sel_item).isHilight()) {
                                                /* use a copy of the iterator to scan forward and un-hilight the menu items.
                                                 * then use the original iterator to erase from the vector. */
                                                for (auto ss=search;ss != popup_stack.end();ss++) {
                                                    for (auto &id : mainMenu.get_item(*ss).display_list.get_disp_list())
                                                        mainMenu.get_item(id).setHilight(mainMenu,false).setHover(mainMenu,false);

                                                    mainMenu.get_item(*ss).setHilight(mainMenu,false).setHover(mainMenu,false);
                                                }

                                                popup_stack.erase(search,popup_stack.end());
                                                mainMenu.get_item(sel_item).setHilight(mainMenu,true).setHover(mainMenu,true);
                                                popup_stack.push_back(sel_item);
                                                redrawAll = true;
                                            }
                                            else {
                                                /* no change in item state, don't bother redrawing */
                                                noRedrawNew = true;
                                            }
                                        }
                                        else {
                                            /* use a copy of the iterator to scan forward and un-hilight the menu items.
                                             * then use the original iterator to erase from the vector. */
                                            for (auto ss=search;ss != popup_stack.end();ss++) {
                                                for (auto &id : mainMenu.get_item(*ss).display_list.get_disp_list())
                                                    mainMenu.get_item(id).setHilight(mainMenu,false).setHover(mainMenu,false);

                                                mainMenu.get_item(*ss).setHilight(mainMenu,false).setHover(mainMenu,false);
                                                redrawAll = true;
                                            }

                                            popup_stack.erase(search,popup_stack.end());
                                        }

                                        if (OpenGL_using())
                                            redrawAll = true;

                                        mainMenu.get_item(sel_item).setHover(mainMenu,true);
                                        if (mainMenu.get_item(sel_item).get_type() == DOSBoxMenu::item_type_id && button_holding)
                                            mainMenu.get_item(sel_item).setHilight(mainMenu,true);
                                    }
                                    else {
                                        if (OpenGL_using())
                                            redrawAll = true;
                                    }

                                    if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle && !OpenGL_using() && !redrawAll && !noRedrawOld) {
                                        if (mainMenu.get_item(mainMenu.menuUserHoverAt).checkResetRedraw()) {
                                            mainMenu.get_item(mainMenu.menuUserHoverAt).drawMenuItem(mainMenu);
                                            mainMenu.get_item(mainMenu.menuUserHoverAt).updateScreenFromItem(mainMenu);
                                        }
                                    }

                                    mainMenu.menuUserHoverAt = sel_item;

                                    if (mainMenu.menuUserHoverAt != DOSBoxMenu::unassigned_item_handle && !OpenGL_using() && !redrawAll && !noRedrawNew) {
                                        if (mainMenu.get_item(mainMenu.menuUserHoverAt).checkResetRedraw()) {
                                            mainMenu.get_item(mainMenu.menuUserHoverAt).drawMenuItem(mainMenu);
                                            mainMenu.get_item(mainMenu.menuUserHoverAt).updateScreenFromItem(mainMenu);
                                        }
                                    }
                                }
                            }
                            break;
                    }

                    if (redrawAll) {
                        redrawAll = false;

#if 0/*DEBUG*/
                        LOG_MSG("Redraw %u",(unsigned int)SDL_GetTicks());
#endif

                        if (OpenGL_using()) {
#if C_OPENGL
                            glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
                            glClear(GL_COLOR_BUFFER_BIT);

                            GFX_OpenGLRedrawScreen();
  
                            mainMenu.setRedraw();                  
                            GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif
                        }
                        else {
                            MenuRestoreScreen();
                            mainMenu.display_list.DrawDisplayList(mainMenu,/*updateScreen*/false);
                        }

                        /* give the menu bar a drop shadow */
                        MenuShadeRect(
                                (int)mainMenu.menuBox.x + (int)DOSBoxMenu::dropshadowX,
                                (int)mainMenu.menuBox.y + (int)mainMenu.menuBox.h,
                                (int)mainMenu.menuBox.w,
                                (int)DOSBoxMenu::dropshadowY - 1/*menubar border*/);

                        for (auto i=popup_stack.begin();i!=popup_stack.end();i++) {
                            if (mainMenu.get_item(*i).get_type() == DOSBoxMenu::submenu_type_id) {
                                mainMenu.get_item(*i).drawBackground(mainMenu);
                                mainMenu.get_item(*i).display_list.DrawDisplayList(mainMenu,/*updateScreen*/false);
                            }
                        }

#if C_OPENGL
                        if (OpenGL_using()) {
# if defined(C_SDL2)
                            SDL_GL_SwapWindow(sdl.window);
# else
                            SDL_GL_SwapBuffers();
# endif
                        }
                        else
#endif
                            MenuFullScreenRedraw();
                    }
                }

#if defined(C_SDL2)
                /* force touchscreen mapping to let go */
                touchscreen_finger_lock = no_finger_id;
                touchscreen_touch_lock = no_touch_id;
#endif

                /* then return */
                GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);
                GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                if (!resized) {
                    MenuRestoreScreen();
                    if (!OpenGL_using())
                        MenuFullScreenRedraw();
                }
                MenuFreeScreen();

                while (!popup_stack.empty()) {
                    DOSBoxMenu::item &item = mainMenu.get_item(popup_stack.back());

                    for (auto &id : item.display_list.get_disp_list()) {
                        mainMenu.get_item(id).setHilight(mainMenu,false);
                        mainMenu.get_item(id).setHover(mainMenu,false);
                    }

                    item.setHilight(mainMenu,false);
                    item.setHover(mainMenu,false);
                    popup_stack.pop_back();
                }

                if (OpenGL_using()) {
#if C_OPENGL
                    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
        
                    GFX_OpenGLRedrawScreen();

                    mainMenu.setRedraw();
                    GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);

# if defined(C_SDL2)
                    SDL_GL_SwapWindow(sdl.window);
# else
                    SDL_GL_SwapBuffers();
# endif

                    sdl_opengl.clear_countdown = 2;
                    sdl_opengl.menudraw_countdown = 2; // two GL buffers
#endif
                }

                /* action! */
                if (!resized && choice_item != DOSBoxMenu::unassigned_item_handle) {
                    DOSBoxMenu::item &item = mainMenu.get_item(choice_item);

                    if (item.get_type() == DOSBoxMenu::item_type_id && item.is_enabled())
                        mainMenu.dispatchItemCommand(item);
                }

                return;
            }
        }
        else {
            GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);
        }
    }
#endif

    /* limit mouse input to whenever the cursor is on the screen, or near the edge of the screen. */
    if (is_paused)
        inputToScreen = false;
    else if (sdl.mouse.locked)
        inputToScreen = true;
    else if (!inMenu)
        inputToScreen = GFX_CursorInOrNearScreen(button->x, button->y);

    switch (button->state) {
    case SDL_PRESSED:
        if (inMenu || !inputToScreen) return;
#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
		if (!sdl.mouse.locked && button->button == SDL_BUTTON_LEFT && isModifierApplied()) {
            if (mbutton==4 && selsrow>-1 && selscol>-1) {
                Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
                selmark = false;
                selsrow = selscol = selerow = selecol = -1;
            } else if (mouse_start_x >= 0 && mouse_start_y >= 0 && fx >= 0 && fy >= 0) {
                Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,sdl.clip.w,sdl.clip.h, false);
                mouse_start_x = mouse_start_y = -1;
                mouse_end_x = mouse_end_y = -1;
                fx = fy = -1;
            }
		}
		if (!sdl.mouse.locked && ((mbutton==2 && button->button == SDL_BUTTON_MIDDLE) || (mbutton==3 && button->button == SDL_BUTTON_RIGHT)) && isModifierApplied()) {
			mouse_start_x=motion->x;
			mouse_start_y=motion->y;
			break;
		} else
#endif
        if (sdl.mouse.requestlock && !vmware_mouse && !sdl.mouse.locked && mouse_notify_mode == 0) {
            CaptureMouseNotify();
            GFX_CaptureMouse();
            // Don't pass click to mouse handler
            break;
        }
        if (((middleunlock == 1 && !sdl.mouse.autoenable) || (middleunlock == 2 && sdl.mouse.autoenable) || middleunlock == 3) && sdl.mouse.autolock && mouse_notify_mode == 0 && button->button == SDL_BUTTON_MIDDLE) {
            GFX_CaptureMouse();
            break;
        }
        switch (button->button) {
        case SDL_BUTTON_LEFT:
            Mouse_ButtonPressed(0);
            VMWARE_MouseButtonPressed(0);
            break;
        case SDL_BUTTON_RIGHT:
            Mouse_ButtonPressed(1);
            VMWARE_MouseButtonPressed(1);
            break;
        case SDL_BUTTON_MIDDLE:
            Mouse_ButtonPressed(2);
            VMWARE_MouseButtonPressed(2);
            break;
#if !defined(C_SDL2)
        case SDL_BUTTON_WHEELUP: /* Ick, really SDL? */
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
#if defined(WIN32) && !defined(HX_DOS)
                if (wheel_key<4) {
                    INPUT ip = {0};
                    ip.type = INPUT_KEYBOARD;
                    ip.ki.wScan = wheel_key==2?75:(wheel_key==3?73:72);
                    ip.ki.time = 0;
                    ip.ki.dwExtraInfo = 0;
                    ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
                    SendInput(1, &ip, sizeof(INPUT));
                    ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
                    SendInput(1, &ip, sizeof(INPUT));
                } else
#endif
                if (wheel_key==7) {
                    bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                    if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                    KEYBOARD_AddKey(KBD_w, true);
                    if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                    KEYBOARD_AddKey(KBD_w, false);
                } else {
                    bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                    if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                    KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_left:(wheel_key==3||wheel_key==6?KBD_pageup:KBD_up), true);
                    if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                    KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_left:(wheel_key==3||wheel_key==6?KBD_pageup:KBD_up), false);
                }
			} else {
                Mouse_ButtonPressed(100-1);
                HandleMouseWheel(true, 1);
            }
			break;
        case SDL_BUTTON_WHEELDOWN: /* Ick, really SDL? */
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
#if defined(WIN32) && !defined(HX_DOS)
                if (wheel_key<4) {
                    INPUT ip = {0};
                    ip.type = INPUT_KEYBOARD;
                    ip.ki.wScan = wheel_key==2?77:(wheel_key==3?81:80);
                    ip.ki.time = 0;
                    ip.ki.dwExtraInfo = 0;
                    ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
                    SendInput(1, &ip, sizeof(INPUT));
                    ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
                    SendInput(1, &ip, sizeof(INPUT));
                } else
#endif
                if (wheel_key==7) {
                    bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                    if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                    KEYBOARD_AddKey(KBD_z, true);
                    if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                    KEYBOARD_AddKey(KBD_z, false);
                } else {
                    bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                    if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                    KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_right:(wheel_key==3||wheel_key==6?KBD_pagedown:KBD_down), true);
                    if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                    KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_right:(wheel_key==3||wheel_key==6?KBD_pagedown:KBD_down), false);
                }
			} else {
                Mouse_ButtonPressed(100+1);
                HandleMouseWheel(false, 1);
            }
            break;
#endif
        }
        break;
    case SDL_RELEASED:
#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
		if (!sdl.mouse.locked && ((mbutton==2 && button->button == SDL_BUTTON_MIDDLE) || (mbutton==3 && button->button == SDL_BUTTON_RIGHT)) && mouse_start_x >= 0 && mouse_start_y >= 0) {
			mouse_end_x=motion->x;
			mouse_end_y=motion->y;
			if (mouse_start_x == mouse_end_x && mouse_start_y == mouse_end_y) {
				PasteClipboard(true);
#ifdef USE_TTF
				if (ttf.inUse) resetFontSize();
#endif
			} else {
				if (abs(mouse_end_x - mouse_start_x) + abs(mouse_end_y - mouse_start_y)<5) {
					PasteClipboard(true);
#ifdef USE_TTF
					if (ttf.inUse) resetFontSize();
#endif
				} else
					CopyClipboard(0);
				if (fx >= 0 && fy >= 0)
					Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,sdl.clip.w,sdl.clip.h, false);
			}
			mouse_start_x = -1;
			mouse_start_y = -1;
			mouse_end_x = -1;
			mouse_end_y = -1;
			fx = -1;
			fy = -1;
			break;
		}
#endif
        switch (button->button) {
        case SDL_BUTTON_LEFT:
            Mouse_ButtonReleased(0);
            VMWARE_MouseButtonReleased(0);
            break;
        case SDL_BUTTON_RIGHT:
            Mouse_ButtonReleased(1);
            VMWARE_MouseButtonReleased(1);
            break;
        case SDL_BUTTON_MIDDLE:
            Mouse_ButtonReleased(2);
            VMWARE_MouseButtonReleased(2);
            break;
#if !defined(C_SDL2)
        case SDL_BUTTON_WHEELUP: /* Ick, really SDL? */
            Mouse_ButtonReleased(100-1);
            break;
        case SDL_BUTTON_WHEELDOWN: /* Ick, really SDL? */
            Mouse_ButtonReleased(100+1);
            break;
#endif
        }
        break;
    }
}

void GFX_LosingFocus(void) {
    sdl.laltstate=SDL_KEYUP;
    sdl.raltstate=SDL_KEYUP;
    sdl.lctrlstate=SDL_KEYUP;
    sdl.rctrlstate=SDL_KEYUP;
    sdl.lshiftstate=SDL_KEYUP;
    sdl.rshiftstate=SDL_KEYUP;
    MAPPER_LosingFocus();
    DoExtendedKeyboardHook(false);
}

bool GFX_IsFullscreen(void) {
    return sdl.desktop.fullscreen;
}

bool sdl_wait_on_error() {
    return sdl.wait_on_error;
}

#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
static uint32_t last_ticks;
void SetIMPosition() {
	uint8_t x, y;
	uint8_t page = IS_PC98_ARCH?0:real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
    if (IS_PC98_ARCH) {
        uint16_t address = vga.config.cursor_start;
        x = address % 80;
        y = address / 80;
    } else
        INT10_GetCursorPos(&y, &x, page);
    /* UNUSED
    int nrows = 25, ncols = 80;
    if (IS_PC98_ARCH)
        nrows=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
    else {
        nrows=(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
        ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
    }*/
    if ((im_x != x || im_y != y) && GetTicks() - last_ticks > 100) {
        last_ticks = GetTicks();
        im_x = x;
        im_y = y;
#if defined(LINUX) || defined(MACOSX)
        y++;
#endif
        uint8_t height = IS_PC98_ARCH?16:real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
        uint8_t width = CurMode && DOSV_CheckCJKVideoMode() ? CurMode->cwidth : (height / 2);
        SDL_Rect rect;
        rect.h = 0;
#if defined(USE_TTF)
        if (ttf.inUse) {
            rect.x = x * ttf.width;
            rect.y = y * ttf.height + (ttf.height - TTF_FontAscent(ttf.SDL_font)) / 2;
#if defined(MACOSX)
            rect.h = ttf.height;
#endif
        } else {
#endif
            double sx = sdl.clip.w>0&&sdl.draw.width>0?((double)sdl.clip.w/sdl.draw.width):1;
            double sy = sdl.clip.h>0&&sdl.draw.height>0?((double)sdl.clip.h/sdl.draw.height):1;
            rect.x = x * width * sx;
            rect.y = y * height * sy - (J3_IsJapanese()?2:(IS_DOSV?-1:(DOSV_CheckCJKVideoMode()?2:0)));
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
            rect.y += mainMenu.menuBarHeight;
#endif
#if defined(MACOSX)
            rect.h = height;
#endif
#if defined(USE_TTF)
        }
#endif
        if(IS_PC98_ARCH)
#if defined(MACOSX)
            rect.y += 2;
#else
            rect.y--;
#endif
#if defined(C_SDL2)
        rect.w = 0;
        SDL_SetTextInputRect(&rect);
#else
#if defined(MACOSX)
        SDL_SetIMValues(SDL_IM_FONT_SIZE, rect.h, NULL);
#endif
        SDL_SetIMPosition(rect.x, rect.y);
#endif
	}
}
#endif

char *CodePageHostToGuest(const uint16_t *s);

#if defined(C_SDL2) && !defined(IGNORE_TOUCHSCREEN)
static void FingerToFakeMouseMotion(SDL_TouchFingerEvent * finger) {
    SDL_MouseMotionEvent fake;

    memset(&fake,0,sizeof(fake));
    /* NTS: Windows versions of SDL2 do normalize the coordinates */
    fake.x = (Sint32)(finger->x * currentWindowWidth);
    fake.y = (Sint32)(finger->y * currentWindowHeight);
    fake.xrel = (Sint32)finger->dx;
    fake.yrel = (Sint32)finger->dy;
    HandleMouseMotion(&fake);

    if (finger->type == SDL_FINGERDOWN || finger->type == SDL_FINGERUP) {
        SDL_MouseButtonEvent fakeb;

        memset(&fakeb,0,sizeof(fakeb));

        fakeb.state = (finger->type == SDL_FINGERDOWN) ? SDL_PRESSED : SDL_RELEASED;
        fakeb.button = SDL_BUTTON_LEFT;
        fakeb.x = fake.x;
        fakeb.y = fake.y;
        HandleMouseButton(&fakeb,&fake);
    }
}

static void HandleTouchscreenFinger(SDL_TouchFingerEvent * finger) {
    /* Now that SDL2 can tell my mouse from my laptop touchscreen, let's
     * map tap events to the left mouse button. Now I can use my laptop
     * touchscreen with Windows 3.11 again! --J.C. */
    /* Now let's handle The Finger (har har) */

    /* NTS: This code is written to map ONLY one finger to the mouse.
     *      If multiple fingers are touching the screen, this code will
     *      only respond to the first finger that touched the screen. */

    if (finger->type == SDL_FINGERDOWN) {
        if (touchscreen_finger_lock == no_finger_id &&
            touchscreen_touch_lock == no_touch_id) {
            touchscreen_finger_lock = finger->fingerId;
            touchscreen_touch_lock = finger->touchId;
            FingerToFakeMouseMotion(finger);
        }
    }
    else if (finger->type == SDL_FINGERUP) {
        if (touchscreen_finger_lock == finger->fingerId &&
            touchscreen_touch_lock == finger->touchId) {
            touchscreen_finger_lock = no_finger_id;
            touchscreen_touch_lock = no_touch_id;
            FingerToFakeMouseMotion(finger);
        }
    }
    else if (finger->type == SDL_FINGERMOTION) {
        if (touchscreen_finger_lock == finger->fingerId &&
            touchscreen_touch_lock == finger->touchId) {
            FingerToFakeMouseMotion(finger);
        }
    }
}
#endif

#if defined(WIN32) && !defined(HX_DOS)
void MSG_WM_COMMAND_handle(SDL_SysWMmsg &Message);
#endif

struct mouse_pos
{
    long x = 0;
    long y = 0;
} mouse_pos;

bool mouse_inside = false;

void GFX_EventsMouseProcess(const long x, const long y, const long rx, const long ry)
{
    const auto x1 = sdl.clip.x;
    const auto x2 = x1 + sdl.clip.w - 1;
    const auto y1 = sdl.clip.y;
    const auto y2 = y1 + sdl.clip.h - 1;
    const auto in = x >= x1 && x <= x2 && y >= y1 && y <= y2;

    if (mouse_inside && !in)
    {
        const auto x3 = max((int)x1, min((int)x2, (int)x));
        const auto y3 = max((int)y1, min((int)y2, (int)y));
        SDL_Event  evt;
        evt.type         = SDL_MOUSEMOTION;
        evt.motion.state = 0;
        evt.motion.which = 0;
        evt.motion.x     = x3;
        evt.motion.y     = y3;
        evt.motion.xrel  = (Sint16)((rx >= 0) ? min(rx, 32767l) : max(rx, -32768l));
        evt.motion.yrel  = (Sint16)((ry >= 0) ? min(ry, 32767l) : max(ry, -32768l));
        SDL_PushEvent(&evt);
    }

    mouse_inside = in;
}

#if defined(WIN32)
void GFX_EventsMouseWin32()
{
    /* Compute relative mouse movement */

    POINT point;

    if (!GetCursorPos(&point))
        return;

    const auto hwnd = GetSurfaceHWND();

    if (hwnd == nullptr || !ScreenToClient(hwnd, &point))
        return;

    const auto x  = point.x;
    const auto y  = point.y;
    const auto rx = x - mouse_pos.x;
    const auto ry = y - mouse_pos.y;

    mouse_pos.x = x;
    mouse_pos.y = y;

    /* Let the method do the heavy uplifting */
    GFX_EventsMouseProcess(x, y, rx, ry);
}
#endif

/**
 * \brief Processes mouse movements when outside the window.
 * 
 * This method will send an extra mouse event to the SDL pump
 * when some relative movement has occurred.
 */
void GFX_EventsMouse()
{
    if (sdl.desktop.fullscreen || sdl.mouse.locked)
        return;

#if WIN32
    GFX_EventsMouseWin32();
#else
    // TODO
#endif
}

bool eatRestoredWindow = false;

/* DOSBox SVN revision 4176:4177: For Linux/X11, Xorg 1.20.1
 * will make spurious focus gain and loss events when locking the mouse in windowed mode.
 *
 * This has not been tested with DOSBox-X yet because I do not run Xorg 1.20.1, yet */
#if defined(LINUX)
#define SDL_XORG_FIX 1
#else
#define SDL_XORG_FIX 0
#endif
/* end patch fragment */

bool gfx_in_mapper = false;

#if defined(MACOSX)
#define DB_POLLSKIP 3
#else
//Not used yet, see comment below
#define DB_POLLSKIP 1
#endif

void GFX_Events() {
    CheckMapperKeyboardLayout();
#if defined(C_SDL2) /* SDL 2.x---------------------------------- */
    //Don't poll too often. This can be heavy on the OS, especially Macs.
    //In idle mode 3000-4000 polls are done per second without this check.
    //Macs, with this code,  max 250 polls per second. (non-macs unused default max 500)
    //Currently not implemented for all platforms, given the ALT-TAB stuff for WIN32.
#if defined (MACOSX)
    static int last_check = 0;
    int current_check = GetTicks();
    if (current_check - last_check <=  DB_POLLSKIP) return;
    last_check = current_check;
#endif

    SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
    static int poll_delay = 0;
    int time = GetTicks();
    if (time - poll_delay > 20) {
        poll_delay = time;
        if (sdl.num_joysticks > 0)
        {
            SDL_JoystickUpdate();
            MAPPER_UpdateJoysticks();
        }
    }
#endif

#if C_GAMELINK
    if (sdl.desktop.type == SCREEN_GAMELINK) OUTPUT_GAMELINK_InputEvent();
#endif

#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
   if(IS_PC98_ARCH) {
       static uint32_t poll98_delay = 0;
       uint32_t time = GetTicks();
       if((int32_t)(time - poll98_delay) > 50) {
           poll98_delay = time;
           SetIMPosition();
       }
    }
#endif

    GFX_EventsMouse();

#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    while (SDL_PollEvent(&event)) {
#if defined(C_SDL2)
        /* SDL2 hack: There seems to be a problem where calling the SetWindowSize function,
           even for the same size, still causes a resize event, and sometimes for no apparent
           reason, will cause an endless feed of Windows Restored events. */
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESTORED) {
            if (eatRestoredWindow) continue;
        }
        else {
            eatRestoredWindow = false;
        }
#endif

        switch (event.type) {
#if defined(WIN32) && !defined(HX_DOS)
        case SDL_SYSWMEVENT:
            switch( event.syswm.msg->msg.win.msg ) {
                case WM_COMMAND:
                    MSG_WM_COMMAND_handle(/*&*/(*event.syswm.msg));
                    break;
                case WM_SYSCOMMAND:
                    switch (event.syswm.msg->msg.win.wParam) {
                        case ID_WIN_SYSMENU_RESTOREMENU:
                            /* prevent removing the menu in 3Dfx mode */
                            if (!GFX_GetPreventFullscreen()) {
                                DOSBox_SetMenu();
                                mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
                            }
                            break;
                        case ID_WIN_SYSMENU_TOGGLEMENU:
                            /* prevent removing the menu in 3Dfx mode */
                            if (!GFX_GetPreventFullscreen())
                            {
                                ToggleMenu(true);
                                mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
#if defined(USE_TTF) && DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                                int last = 0;
                                while (TTF_using() && !sdl.desktop.fullscreen && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0 && ttf.pointsize>last) {
                                    last = ttf.pointsize;
                                    increaseFontSize();
                                }
#endif
                            }
                            break;
                        case ID_WIN_SYSMENU_MAPPER:
                            extern void MAPPER_Run(bool pressed);
                            MAPPER_Run(false);
                            break;
                        case ID_WIN_SYSMENU_CFG_GUI:
                            extern void GUI_Run(bool pressed);
                            GUI_Run(false);
                            break;
                        case ID_WIN_SYSMENU_PAUSE:
                            void PauseDOSBox(bool pressed);
                            PauseDOSBox(true);
                            break;
                        case ID_WIN_SYSMENU_RESETSIZE:
                            void GUI_ResetResize(bool pressed);
                            GUI_ResetResize(true);
                            break;
#if defined(USE_TTF)
                        case ID_WIN_SYSMENU_TTFINCSIZE:
                            increaseFontSize();
                            break;
                        case ID_WIN_SYSMENU_TTFDECSIZE:
                            decreaseFontSize();
                            break;
#endif
                        default:
                            break;
                    }
                default:
                    break;
            }
            break;
#endif
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_MOVED:
#if defined(USE_TTF)
                if (ttf.inUse)
                   GFX_EndTextLines(true);
#endif
                continue;
            case SDL_WINDOWEVENT_RESTORED:
                GFX_ResetScreen();
                eatRestoredWindow = true;
                continue;
            case SDL_WINDOWEVENT_RESIZED:
                GFX_HandleVideoResize(event.window.data1, event.window.data2);
                continue;
            case SDL_WINDOWEVENT_EXPOSED:
                if (sdl.desktop.type == SCREEN_GAMELINK) break;
                if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
                continue;
            case SDL_WINDOWEVENT_LEAVE:
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
                void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

                skipdraw=true;
                GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                skipdraw=false;
                GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);

                GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (sdl.desktop.type == SCREEN_GAMELINK) break;
                if (IsFullscreen() && !sdl.mouse.locked)
                    GFX_CaptureMouse();
                SetPriority(sdl.priority.focus);
                CPU_Disable_SkipAutoAdjust();
#if defined(USE_TTF)
                resetFontSize();
#endif
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                if (sdl.desktop.type == SCREEN_GAMELINK) break;
                if (sdl.mouse.locked) {
                    CaptureMouseNotify();
                    GFX_CaptureMouse();
                }
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
                void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

                GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);

                GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif
                SetPriority(sdl.priority.nofocus);
                GFX_LosingFocus();
                if( sdl.priority.nofocus != PRIORITY_LEVEL_PAUSE ) {
                    CPU_Enable_SkipAutoAdjust();
                }
                break;
            default:
                ;
            }

            /* Non-focus priority is set to pause; check to see if we've lost window or input focus
             * i.e. has the window been minimised or made inactive?
             */
            if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
                if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
                    /* Window has lost focus, pause the emulator.
                     * This is similar to what PauseDOSBox() does, but the exit criteria is different.
                     * Instead of waiting for the user to hit Alt-Break, we wait for the window to
                     * regain window or input focus.
                     */
                    bool paused = true;
                    SDL_Event ev;

                    GFX_SetTitle(-1,-1,-1,true);
                    KEYBOARD_ClrBuffer();
//                  SDL_Delay(500);
//                  while (SDL_PollEvent(&ev)) {
                    // flush event queue.
//                  }

                    while (paused) {
#if C_EMSCRIPTEN
                        emscripten_sleep(0);
                        SDL_PollEvent(&ev);
#else
                        // WaitEvent waits for an event rather than polling, so CPU usage drops to zero
                        SDL_WaitEvent(&ev);
#endif

                        switch (ev.type) {
                        case SDL_QUIT:
                            if (CheckQuit()) throw(0);
                            break; // a bit redundant at linux at least as the active events gets before the quit event.
                        case SDL_WINDOWEVENT:     // wait until we get window focus back
                            if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) || (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                                // We've got focus back, so unpause and break out of the loop
                                if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                                    paused = false;
                                    GFX_SetTitle(-1,-1,-1,false);
                                }

                                /* Now poke a "release ALT" command into the keyboard buffer
                                 * we have to do this, otherwise ALT will 'stick' and cause
                                 * problems with the app running in the DOSBox-X.
                                 */
                                KEYBOARD_AddKey(KBD_leftalt, false);
                                KEYBOARD_AddKey(KBD_rightalt, false);
                                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                                    // We may need to re-create a texture and more
                                    GFX_ResetScreen();
                                }
                            }
                            break;
                        }
                    }
                }
            }
            break;
        case SDL_MOUSEMOTION:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
#if defined(C_SDL2)
            if (touchscreen_finger_lock == no_finger_id &&
                touchscreen_touch_lock == no_touch_id &&
                event.motion.which != SDL_TOUCH_MOUSEID) { /* don't handle mouse events faked by touchscreen */
                HandleMouseMotion(&event.motion);
            }
#else
            HandleMouseMotion(&event.motion);
#endif
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
#if defined(C_SDL2)
            if (touchscreen_finger_lock == no_finger_id &&
                touchscreen_touch_lock == no_touch_id &&
                event.button.which != SDL_TOUCH_MOUSEID) { /* don't handle mouse events faked by touchscreen */
                HandleMouseButton(&event.button,&event.motion);
            }
#else
            HandleMouseButton(&event.button,&event.motion);
#endif
            break;
#if !defined(IGNORE_TOUCHSCREEN)
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
            HandleTouchscreenFinger(&event.tfinger);
            break;
#endif
        case SDL_QUIT:
            if (CheckQuit()) throw(0);
            break;
		case SDL_MOUSEWHEEL:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
				if(event.wheel.y > 0) {
#if defined (WIN32) && !defined(HX_DOS)
                    if (wheel_key<4) {
                        INPUT ip = {0};
                        ip.type = INPUT_KEYBOARD;
                        ip.ki.wScan = wheel_key==2?75:(wheel_key==3?73:72);
                        ip.ki.time = 0;
                        ip.ki.dwExtraInfo = 0;
                        ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
                        SendInput(1, &ip, sizeof(INPUT));
                        ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
                        SendInput(1, &ip, sizeof(INPUT));
                    } else
#endif
                    if (wheel_key==7) {
                        bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                        if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                        KEYBOARD_AddKey(KBD_w, true);
                        if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                        KEYBOARD_AddKey(KBD_w, false);
                    } else {
                        bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                        if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                        KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_left:(wheel_key==3||wheel_key==6?KBD_pageup:KBD_up), true);
                        if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                        KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_left:(wheel_key==3||wheel_key==6?KBD_pageup:KBD_up), false);
                    }
				} else if(event.wheel.y < 0) {
#if defined (WIN32) && !defined(HX_DOS)
                    if (wheel_key<4) {
                        INPUT ip = {0};
                        ip.type = INPUT_KEYBOARD;
                        ip.ki.wScan = wheel_key==2?77:(wheel_key==3?81:80);
                        ip.ki.time = 0;
                        ip.ki.dwExtraInfo = 0;
                        ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
                        SendInput(1, &ip, sizeof(INPUT));
                        ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
                        SendInput(1, &ip, sizeof(INPUT));
                    } else
#endif
                    if (wheel_key==7) {
                        bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                        if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                        KEYBOARD_AddKey(KBD_z, true);
                        if (ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                        KEYBOARD_AddKey(KBD_z, false);
                    } else {
                        bool ctrlup = sdl.lctrlstate==SDL_KEYUP && sdl.rctrlstate==SDL_KEYUP;
                        if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, true);
                        KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_right:(wheel_key==3||wheel_key==6?KBD_pagedown:KBD_down), true);
                        if (wheel_key >= 4 && wheel_key <= 6 && ctrlup) KEYBOARD_AddKey(KBD_leftctrl, false);
                        KEYBOARD_AddKey(wheel_key==2||wheel_key==5?KBD_right:(wheel_key==3||wheel_key==6?KBD_pagedown:KBD_down), false);
                    }
				}
			} else
                HandleMouseWheel(event.wheel.direction == SDL_MOUSEWHEEL_NORMAL, event.wheel.y);
			break;
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
        case SDL_TEXTEDITING:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
            ime_text = event.edit.text;
            break;
        case SDL_TEXTINPUT:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
            {
                int len = strlen(event.text.text);
                if(ime_text == event.text.text || len > 1) {
                    uint8_t* buff;
                    if((buff = (uint8_t *)malloc(len * 2)) != NULL) {
                        if(CodePageHostToGuestUTF8((char *)buff, event.text.text)) {
                            for(int no = 0 ; buff[no] != 0 ; no++) {
                                if (IS_PC98_ARCH || isDBCSCP()) {
                                    if(dos.loaded_codepage == 932 && isKanji1(buff[no]) && isKanji2(buff[no + 1])) {
#if defined(MACOSX)
                                        if (buff[no] == 0x81 && buff[no + 1] == 0x40) no++;
                                        else
#endif
                                        {
                                        BIOS_AddKeyToBuffer(0xf100 | buff[no++]);
                                        BIOS_AddKeyToBuffer(0xf000 | buff[no]);
                                        }
                                    } else {
                                        BIOS_AddKeyToBuffer(buff[no]);
                                    }
                                } else {
                                    BIOS_AddKeyToBuffer(buff[no]);
                                }
                            }
                        }
                        free(buff);
                    }
                    SetIMPosition();
                }
                ime_text = "";
            }
            break;
#endif
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
#if defined (WIN32) || defined(MACOSX) || defined(C_SDL2)
            if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LCTRL) sdl.lctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RCTRL) sdl.rctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LSHIFT) sdl.lshiftstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RSHIFT) sdl.rshiftstate = event.key.type;
            if (event.type == SDL_KEYDOWN && isModifierApplied())
                ClipKeySelect(event.key.keysym.sym);
            if(dos.im_enable_flag) {
#if defined(MACOSX)
                if(event.type == SDL_KEYDOWN && IME_GetEnable()) {
                    // Enter, BS, TAB, <-, ->
                    if(event.key.keysym.sym == 0x0d || event.key.keysym.sym == 0x08 || event.key.keysym.sym == 0x09 || (event.key.keysym.scancode >= 0x4f && event.key.keysym.scancode <= 0x52)) {
                        if(ime_text.size() != 0) {
                            break;
                        }
                    } else {
                        if(event.key.keysym.scancode == 0x2c && ime_text.size() == 0 && dos.loaded_codepage == 932) {
                            if((event.key.keysym.mod & 0x03) == 0) {
                                // Zenkaku space
                                BIOS_AddKeyToBuffer(0xf100 | 0x81);
                                BIOS_AddKeyToBuffer(0xf000 | 0x40);
                                break;
                            }
                        }
                        else break;
                    }
                }
#endif
                // Hankaku/Zenkaku
                if(event.key.keysym.scancode == 0x35) {
                    MAPPER_CheckKeyboardLayout();
                    if (isJPkeyboard) break;
                }
            }
#endif
#if defined (MACOSX)
            /* On macs CMD-Q is the default key to close an application */
            if (event.key.keysym.sym == SDLK_q &&
                    (event.key.keysym.mod == KMOD_RGUI ||
                     event.key.keysym.mod == KMOD_LGUI)
               ) {
                KillSwitch(true);
                break;
            }
#endif
        default:
            if (sdl.desktop.type == SCREEN_GAMELINK) break;
            gfx_in_mapper = true;
            if (ticksLocked && event.type == SDL_KEYDOWN && static_cast<Section_prop *>(control->GetSection("cpu"))->Get_bool("stop turbo on key")) DOSBOX_UnlockSpeed2(true);
            MAPPER_CheckEvent(&event);
            gfx_in_mapper = false;
        }
    }
#else /* SDL 1.x---------------------------------- */
    SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
    static uint32_t poll_delay=0;
    uint32_t time=GetTicks();
    if ((int32_t)(time-poll_delay)>20) {
        poll_delay=time;
        if (sdl.num_joysticks>0)
        {
            SDL_JoystickUpdate();
            MAPPER_UpdateJoysticks();
        }
    }
#endif
#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
   if(IS_PC98_ARCH) {
       static uint32_t poll98_delay = 0;
       uint32_t time = GetTicks();
       if((int32_t)(time - poll98_delay) > 50) {
           poll98_delay = time;
           SetIMPosition();
       }
    }
#endif

    GFX_EventsMouse();

#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    while (SDL_PollEvent(&event)) {
        /* DOSBox SVN revision 4176:4177: For Linux/X11, Xorg 1.20.1
         * will make spurious focus gain and loss events when locking the mouse in windowed mode.
         *
         * This has not been tested with DOSBox-X yet because I do not run Xorg 1.20.1, yet */
#if SDL_XORG_FIX
        // Special code for broken SDL with Xorg 1.20.1, where pairs of inputfocus gain and loss events are generated
        // when locking the mouse in windowed mode.
        if (event.type == SDL_ACTIVEEVENT && event.active.state == SDL_APPINPUTFOCUS && event.active.gain == 0) {
            SDL_Event test; //Check if the next event would undo this one.
            if (SDL_PeepEvents(&test,1,SDL_PEEKEVENT,SDL_ACTIVEEVENTMASK) == 1 && test.active.state == SDL_APPINPUTFOCUS && test.active.gain == 1) {
                // Skip both events.
                SDL_PeepEvents(&test,1,SDL_GETEVENT,SDL_ACTIVEEVENTMASK);
                continue;
            }
        }
#endif
        /* end patch fragment */

        switch (event.type) {
#ifdef __WIN32__
        case SDL_SYSWMEVENT : {
            switch( event.syswm.msg->msg ) {
#if !defined(HX_DOS)
                case WM_COMMAND:
                    MSG_WM_COMMAND_handle(/*&*/(*event.syswm.msg));
                    break;
#endif
                case WM_SYSCOMMAND:
                    switch (event.syswm.msg->wParam) {
                        case 0xF032: // FIXME: What is this?
                        case SC_MAXIMIZE:
                            userResizeWindowWidth = 0;
                            userResizeWindowHeight = 0;
                            menu.maxwindow = true;
                            break;
                        case 0xF122: // FIXME: What is this?
                        case SC_RESTORE:
                            if (sdl.desktop.fullscreen)
                                GFX_SwitchFullScreen();
                            menu.maxwindow = false;
                            UpdateWindowDimensions();
                            RENDER_Reset();
                            if (OpenGL_using()) {
                                UpdateWindowDimensions();
                                RENDER_Reset();
                            }
                            break;
                        case ID_WIN_SYSMENU_RESTOREMENU:
                            /* prevent removing the menu in 3Dfx mode */
                            if (!GFX_GetPreventFullscreen()) {
                                DOSBox_SetMenu();
                                mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
                            }
                            break;
                        case ID_WIN_SYSMENU_TOGGLEMENU:
                            /* prevent removing the menu in 3Dfx mode */
                            if (!GFX_GetPreventFullscreen())
                            {
                                if (menu.toggle) DOSBox_NoMenu(); else DOSBox_SetMenu();
                                mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
#if defined(USE_TTF) && DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                                int last = 0;
                                while (TTF_using() && !sdl.desktop.fullscreen && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0 && ttf.pointsize>last) {
                                    last = ttf.pointsize;
                                    increaseFontSize();
                                }
#endif
                            }
                            break;
#if !defined(HX_DOS)
                        case ID_WIN_SYSMENU_MAPPER:
                            extern void MAPPER_Run(bool pressed);
                            MAPPER_Run(false);
                            break;
                        case ID_WIN_SYSMENU_CFG_GUI:
                            extern void GUI_Run(bool pressed);
                            GUI_Run(false);
                            break;
                        case ID_WIN_SYSMENU_PAUSE:
                            void PauseDOSBox(bool pressed);
                            PauseDOSBox(true);
                            break;
                        case ID_WIN_SYSMENU_RESETSIZE:
                            void GUI_ResetResize(bool pressed);
                            GUI_ResetResize(true);
                            break;
#if defined(USE_TTF)
                        case ID_WIN_SYSMENU_TTFINCSIZE:
                            increaseFontSize();
                            break;
                        case ID_WIN_SYSMENU_TTFDECSIZE:
                            decreaseFontSize();
                            break;
#endif
#endif
                    }
                default:
                    break;
            }
        }
#endif
        case SDL_ACTIVEEVENT:
                if (event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
                    if (event.active.gain) {
#ifdef WIN32
                        if (!sdl.desktop.fullscreen) sdl.focus_ticks = GetTicks();
#endif
                        if (sdl.desktop.fullscreen && !sdl.mouse.locked)
                            GFX_CaptureMouse();
                        SetPriority(sdl.priority.focus);
                        CPU_Disable_SkipAutoAdjust();
                        if (strcmp(RunningProgram, "LOADLIN")) {
                            BIOS_SynchronizeNumLock();
                            BIOS_SynchronizeCapsLock();
                            BIOS_SynchronizeScrollLock();
                        }
                    } else {
                        if (sdl.mouse.locked)
                        {
                            CaptureMouseNotify();
                            GFX_CaptureMouse();
                        }

#if defined(WIN32)
                        if (sdl.desktop.fullscreen)
                            GFX_ForceFullscreenExit();
#endif

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                        void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
                        void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

                        GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                        GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);
#endif

                        SetPriority(sdl.priority.nofocus);
                        GFX_LosingFocus();

                        if( sdl.priority.nofocus != PRIORITY_LEVEL_PAUSE ) {
                            CPU_Enable_SkipAutoAdjust();
                        }
                    }
                }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                if (event.active.state & SDL_APPMOUSEFOCUS) {
                    if (!(event.active.gain & SDL_APPMOUSEFOCUS)) {
                        /* losing focus or moving the mouse outside the window should un-hilight the currently selected menu item */
                        void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
                        void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

                        GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                        GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);

                    }
                }

                GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif

                /* Non-focus priority is set to pause; check to see if we've lost window or input focus
                 * i.e. has the window been minimised or made inactive?
                 */
                if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
                    if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
                    /* Window has lost focus, pause the emulator.
                     * This is similar to what PauseDOSBox() does, but the exit criteria is different.
                     * Instead of waiting for the user to hit Alt-Break, we wait for the window to
                     * regain window or input focus.
                     */
                    bool paused = true;
                    SDL_Event ev;

                    GFX_SetTitle(-1,-1,-1,true);
                    KEYBOARD_ClrBuffer();
//                  SDL_Delay(500);
//                  while (SDL_PollEvent(&ev)) {
                        // flush event queue.
//                  }

                    while (paused) {
#if C_EMSCRIPTEN
                        emscripten_sleep(0);
                        SDL_PollEvent(&ev);
#else
                        // WaitEvent waits for an event rather than polling, so CPU usage drops to zero
                        SDL_WaitEvent(&ev);
#endif

                        switch (ev.type) {
                        case SDL_QUIT: if (CheckQuit()) throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
                        case SDL_ACTIVEEVENT:     // wait until we get window focus back
                            if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
                                // We've got focus back, so unpause and break out of the loop
                                if (ev.active.gain) {
                                    paused = false;
                                    GFX_SetTitle(-1,-1,-1,false);
                                }

                                /* Now poke a "release ALT" command into the keyboard buffer
                                 * we have to do this, otherwise ALT will 'stick' and cause
                                 * problems with the app running in the DOSBox-X.
                                 */
                                KEYBOARD_AddKey(KBD_leftalt, false);
                                KEYBOARD_AddKey(KBD_rightalt, false);
                            }
                            break;
                        }
                    }
                }
            }
            break;
        case SDL_MOUSEMOTION:
            HandleMouseMotion(&event.motion);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            HandleMouseButton(&event.button,&event.motion);
            break;
        case SDL_VIDEORESIZE:
            UpdateWindowDimensions(); // FIXME: Use SDL window dimensions, except that on Windows, SDL won't tell us our actual dimensions
            HandleVideoResize(&event.resize);
            break;
        case SDL_QUIT:
            if (CheckQuit()) throw(0);
            break;
        case SDL_VIDEOEXPOSE:
            if (sdl.draw.callback && !glide.enabled) sdl.draw.callback( GFX_CallBackRedraw );
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // ignore event alt+tab
            if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LCTRL) sdl.lctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RCTRL) sdl.rctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LSHIFT) sdl.lshiftstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RSHIFT) sdl.rshiftstate = event.key.type;
#if defined(LINUX) && C_X11
			if (event.type == SDL_KEYDOWN) {
				if((IS_PC98_ARCH || isDBCSCP()) && event.key.keysym.unicode != 0) {
					SetIMPosition();
                    char chars[10];
                    uint16_t uname[2];
                    uname[0]=event.key.keysym.unicode;
                    uname[1]=0;
                    if (CodePageHostToGuestUTF16(chars, uname)) {
                        for (size_t i=0; i<strlen(chars); i++) {
                            if (dos.loaded_codepage == 932 && strlen(chars) == 2 && isKanji1(chars[0]))
                                BIOS_AddKeyToBuffer((i==0?0xf100:0xf000) | (unsigned char)chars[i]);
                            else
                                BIOS_AddKeyToBuffer((unsigned char)chars[i]);
                        }
                    } else
                        BIOS_AddKeyToBuffer((unsigned char)uname[0]);
                    break;
                }
			}
#endif
#if defined(WIN32)
            if (event.type == SDL_KEYDOWN && isModifierApplied())
                ClipKeySelect(event.key.keysym.sym);
            if ((event.key.keysym.sym==SDLK_TAB) &&
                ((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN))) { MAPPER_LosingFocus(); break; }
            // This can happen as well.
            if ((event.key.keysym.sym == SDLK_TAB) && (event.key.keysym.mod & KMOD_ALT)) break;
            // ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
            if ((event.key.keysym.sym == SDLK_TAB) && (GetTicks() - sdl.focus_ticks < 2)) break;
            if (GetACP() == 932 && GetKeyboardType(0) != 7) {
                // If the Windows code page is 932 and you are using a non-Japanese keyboard
                if(event.key.keysym.scancode == 0x0d) event.key.keysym.sym = SDLK_EQUALS;
                else if(event.key.keysym.scancode == 0x2b) event.key.keysym.sym = SDLK_BACKSLASH;
                else if(event.key.keysym.scancode == 0x27) event.key.keysym.sym = SDLK_SEMICOLON;
                else if(event.key.keysym.scancode == 0x28) event.key.keysym.sym = SDLK_QUOTE;
            }
#if !defined(HX_DOS) && defined(SDL_DOSBOX_X_SPECIAL)
			int onoff;
			if(SDL_GetIMValues(SDL_IM_ONOFF, &onoff, NULL) == NULL) {
				if(onoff != 0 && event.type == SDL_KEYDOWN) {
					if(event.key.keysym.sym == 0x0d) {
						if(sdl.ime_ticks != 0) {
							if(GetTicks() - sdl.ime_ticks < 10) {
								sdl.ime_ticks = 0;
								break;
							}
						}
					}
				}
			}
			sdl.ime_ticks = 0;
			if(event.key.keysym.scancode == 0 && event.key.keysym.sym == 0) {
				int len;
				char chars[10];
				if((len = SDL_FlushIMString(NULL))) {
					uint16_t *buff = (uint16_t *)malloc((len + 1)*sizeof(uint16_t)), uname[2];
					SDL_FlushIMString(buff);
					SetIMPosition();
					for(int no = 0 ; no < len ; no++) {
                        unsigned char ch = (unsigned char)buff[no];
                        if ((IS_PC98_ARCH || isDBCSCP()) && ch != buff[no]) {
                            uname[0]=buff[no];
                            uname[1]=0;
                            if (CodePageHostToGuestUTF16(chars, uname)) {
                                for (size_t i=0; i<strlen(chars); i++) {
                                    if (dos.loaded_codepage == 932 && strlen(chars) == 2 && isKanji1(chars[0]))
                                        BIOS_AddKeyToBuffer((i==0?0xf100:0xf000) | (unsigned char)chars[i]);
                                    else
                                        BIOS_AddKeyToBuffer((unsigned char)chars[i]);
                                }
                            } else
                                BIOS_AddKeyToBuffer(ch);
                        } else
                                BIOS_AddKeyToBuffer(ch);
					}
					free(buff);
					sdl.ime_ticks = GetTicks();
				}
			}
#endif
#endif
#if defined (MACOSX) &&  defined(SDL_DOSBOX_X_SPECIAL)
			bool ime_key;
			int onoff;
			if(SDL_GetIMValues(SDL_IM_ONOFF, &onoff, NULL) == NULL) {
				if(onoff != 0 && event.type == SDL_KEYDOWN) {
					if(event.key.keysym.sym == 0x0d) {
						if(sdl.ime_ticks != 0) {
							if(GetTicks() - sdl.ime_ticks < 10) {
								sdl.ime_ticks = 0;
								break;
							}
						}
					}
				}
			}
			ime_key = false;
			sdl.ime_ticks = 0;
			if(event.key.keysym.scancode == 0 && event.key.keysym.sym == 0) {
				int len;
				if(len = SDL_FlushIMString(NULL)) {
					int flag = 0;
					unsigned char *buff = (unsigned char *)malloc((len + 2)); 

					SDL_FlushIMString(buff);
					for(int no = 0 ; no < len ; no++) {
						if(flag == 0) {
							if(isKanji1(buff[no])) {
								flag = 1;
								BIOS_AddKeyToBuffer(0xf000 | buff[no]);
							} else {
								if(buff[no] == 0x1c) {
									event.key.keysym.scancode = 0x7b;
									event.key.keysym.sym = SDLK_LEFT;
									ime_key = true;
								} else if(buff[no] == 0x1d) {
									event.key.keysym.scancode = 0x7c;
									event.key.keysym.sym = SDLK_RIGHT;
									ime_key = true;
								} else if(buff[no] == 0x1e) {
									event.key.keysym.scancode = 0x7e;
									event.key.keysym.sym = SDLK_UP;
									ime_key = true;
								} else if(buff[no] == 0x1f) {
									event.key.keysym.scancode = 0x7d;
									event.key.keysym.sym = SDLK_DOWN;
									ime_key = true;
								} else if(buff[no] == 0x08) {
									event.key.keysym.scancode = 0x33;
									event.key.keysym.sym = SDLK_BACKSPACE;
									ime_key = true;
								} else {
									BIOS_AddKeyToBuffer(buff[no]);
								}
							}
						} else {
							BIOS_AddKeyToBuffer(0xf100 | buff[no]);
							flag = 0;
						}
					}
					free(buff);
					sdl.ime_ticks = GetTicks();
				}
			}
            if (event.type == SDL_KEYDOWN && isModifierApplied())
                ClipKeySelect(event.key.keysym.sym);
            /* On macs CMD-Q is the default key to close an application */
            if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
                KillSwitch(true);
                break;
            } 
#endif
        default:
#if defined(WIN32) && !defined(HX_DOS) && defined(SDL_DOSBOX_X_SPECIAL)
            if(dos.im_enable_flag) {
                if(event.key.keysym.scancode == 0x94) {
                    break;
                } else if(event.key.keysym.scancode == 0x29) {
                    MAPPER_CheckKeyboardLayout();
                    if (isJPkeyboard) break;
                } else if(event.key.keysym.scancode == 0x70) {
                    event.type = SDL_KEYDOWN;
                }
            }
#endif
            if (ticksLocked && event.type == SDL_KEYDOWN && static_cast<Section_prop *>(control->GetSection("cpu"))->Get_bool("stop turbo on key")) DOSBOX_UnlockSpeed2(true);
            MAPPER_CheckEvent(&event);
#if defined (MACOSX) &&  defined(SDL_DOSBOX_X_SPECIAL)
            if(ime_key) {
                event.type = SDL_KEYUP;
                MAPPER_CheckEvent(&event);
            }
#endif
        }
    }
#endif
    // start emendelson from dbDOS; improved by Wengier
    // Disabled multiple characters per dispatch b/c occasionally
    // keystrokes get lost in the spew. (Prob b/c of DI usage on Win32, sadly..)
    // while (PasteClipboardNext());
    // Doesn't really matter though, it's fast enough as it is...
	if (paste_speed < 0) paste_speed = 30;

    static Bitu iPasteTicker = 0;
    if (paste_speed && (iPasteTicker++ % paste_speed) == 0) // emendelson: was 20 - good for WP51; Wengier: changed to 30 for better compatibility
        PasteClipboardNext();   // end added emendelson from dbDOS; improved by Wengier
}

void Null_Init(Section *sec);

void SDL_SetupConfigSection() {
    Section_prop * sdl_sec=control->AddSection_prop("sdl",&Null_Init);

    Prop_bool* Pbool;
    Prop_string* Pstring;
    Prop_int* Pint;
    Prop_multival* Pmulti;

    Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,false);
    Pbool->Set_help("Start DOSBox-X directly in fullscreen. (Press [F11/F12]+F to go back)");
    Pbool->SetBasic(true);

    Pbool = sdl_sec->Add_bool("fulldouble",Property::Changeable::Always,false);
    Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox-X.");
    Pbool->SetBasic(true);

    //Pbool = sdl_sec->Add_bool("sdlresize",Property::Changeable::Always,false);
    //Pbool->Set_help("Makes window resizable (depends on scalers)");

    Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"desktop");
    Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
                      "  Using your monitor's native resolution with aspect=true might give the best results.\n"
              "  If you end up with small window on a large screen, try an output different from surface.");
    Pstring->SetBasic(true);

    Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"original");
    Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
                      "  (output=surface does not!)");
    Pstring->SetBasic(true);

    Pstring = sdl_sec->Add_string("windowposition", Property::Changeable::Always, "-");
    Pstring->Set_help("Set the window position at startup in the positionX,positionY format (e.g.: 1300,200).\n"
                      "The window will be centered with \",\" (or empty), and will be in the original position with \"-\".");
    Pstring->SetBasic(true);

    const char* outputs[] = {
        "default", "surface", "overlay", "ttf",
#if C_OPENGL
        "opengl", "openglnb", "openglhq", "openglpp",
#endif
#if C_GAMELINK
        "gamelink",
#endif
        "ddraw", "direct3d",
        0 };

    Pint = sdl_sec->Add_int("display", Property::Changeable::Always, 0);
    Pint->Set_help("Specify a screen/display number to use for a multi-screen setup (0 = default).");
    Pint->SetBasic(true);

    Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "default");
    Pstring->Set_help("What video system to use for output (openglnb = OpenGL nearest; openglpp = OpenGL perfect; ttf = TrueType font output).");
    Pstring->Set_values(outputs);
    Pstring->SetBasic(true);

    Pstring = sdl_sec->Add_string("videodriver",Property::Changeable::OnlyAtStart, "");
    Pstring->Set_help("Forces a video driver (e.g. windib/windows, directx, x11, fbcon, dummy, etc) for the SDL library to use.");
    Pstring->SetBasic(true);

    Pint = sdl_sec->Add_int("transparency", Property::Changeable::WhenIdle, 0);
    Pint->Set_help("Set the transparency of the DOSBox-X screen (both windowed and full-screen modes, on SDL2 and Windows SDL1 builds).\n"
		"The valid value is from 0 (no transparency, the default setting) to 90 (high transparency).");
    Pint->SetMinMax(0,90);
    Pint->SetBasic(true);

    Pbool = sdl_sec->Add_bool("maximize",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("If set, the DOSBox-X window will be maximized at start (SDL2 and Windows SDL1 builds only; use fullscreen for TTF output).");
    Pbool->SetBasic(true);

    Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::WhenIdle, false);
    Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");
    Pbool->SetBasic(true);

    const char* feeds[] = { "none", "beep", "flash", nullptr};
    Pstring = sdl_sec->Add_string("autolock_feedback", Property::Changeable::Always, feeds[1]);
    Pstring->Set_help("Autolock status feedback type, i.e. visual, auditive, none.");
    Pstring->Set_values(feeds);
    Pstring->SetBasic(true);

    const char* unlocks[] = { "none", "manual", "auto", "both", nullptr};
    Pstring = sdl_sec->Add_string("middle_unlock",Property::Changeable::Always, unlocks[1]);
    Pstring->Set_help("Whether you can press the middle mouse button to unlock the mouse when the mouse has been locked.\n"
        "If set to \"manual\", it works only with \"autolock=false\"; if set to \"auto\", it works only with \"autolock=true\".");
    Pstring->Set_values(unlocks);
    Pstring->SetBasic(true);

	const char* clipboardbutton[] = { "none", "middle", "right", "arrows", 0};
	Pstring = sdl_sec->Add_string("clip_mouse_button",Property::Changeable::Always, "right");
	Pstring->Set_values(clipboardbutton);
	Pstring->Set_help("Select the mouse button or use arrow keys for the shared clipboard copy/paste function.\n"
		"The default mouse button is \"right\", which means using the right mouse button to select text, copy to and paste from the host clipboard.\n"
		"Set to \"middle\" to use the middle mouse button, \"arrows\" to use arrow keys instead of a mouse button, or \"none\" to disable this feature.\n"
		"For \"arrows\", press Home key (or Fn+Shift+Left on Mac laptops) to start selection, and End key (or Fn+Shift+Right on Mac laptops) to end selection.");
    Pstring->SetBasic(true);

	const char* clipboardmodifier[] = { "none", "ctrl", "lctrl", "rctrl", "alt", "lalt", "ralt", "shift", "lshift", "rshift", "ctrlalt", "ctrlshift", "altshift", "lctrlalt", "lctrlshift", "laltshift", "rctrlalt", "rctrlshift", "raltshift", 0};
	Pstring = sdl_sec->Add_string("clip_key_modifier",Property::Changeable::Always, "shift");
	Pstring->Set_values(clipboardmodifier);
	Pstring->Set_help("Change the keyboard modifier for the shared clipboard copy/paste function using a mouse button or arrow keys.\n"
		"The default modifier is \"shift\" (both left and right shift keys). Set to \"none\" if no modifier is desired.");
    Pstring->SetBasic(true);

	const char* truefalsedefaultopt[] = { "true", "false", "1", "0", "default", 0};
	Pstring = sdl_sec->Add_string("clip_paste_bios",Property::Changeable::WhenIdle, "default");
	Pstring->Set_values(truefalsedefaultopt);
	Pstring->Set_help("Specify whether to use BIOS keyboard functions for the clipboard pasting instead of the keystroke method.\n"
		"For pasting clipboard text into Windows 3.x/9x applications (e.g. Notepad), make sure to use the keystroke method.");
    Pstring->SetBasic(true);

    Pint = sdl_sec->Add_int("clip_paste_speed", Property::Changeable::WhenIdle, 30);
    Pint->Set_help("Set keyboard speed for pasting text from the shared clipboard.\n"
        "If the default setting of 30 causes lost keystrokes, increase the number.\n"
        "Or experiment with decreasing the number for applications that accept keystrokes quickly.");
    Pint->SetBasic(true);

    Pmulti = sdl_sec->Add_multi("sensitivity",Property::Changeable::Always, ",");
    Pmulti->Set_help("Mouse sensitivity. The optional second parameter specifies vertical sensitivity (e.g. 100,-50).");
    Pmulti->SetValue("100");
    Pmulti->SetBasic(true);
    Pint = Pmulti->GetSection()->Add_int("xsens",Property::Changeable::Always,100);
    Pint->SetMinMax(-1000,1000);
    Pint = Pmulti->GetSection()->Add_int("ysens",Property::Changeable::Always,100);
    Pint->SetMinMax(-1000,1000);

#if defined(C_SDL2)
    Pbool = sdl_sec->Add_bool("raw_mouse_input", Property::Changeable::OnlyAtStart, false);
    Pbool->Set_help("Enable this setting to bypass your operating system's mouse acceleration and sensitivity settings.\n"
        "This works in fullscreen or when the mouse is captured in window mode (SDL2 builds only).");
#endif

    Pbool = sdl_sec->Add_bool("usesystemcursor",Property::Changeable::OnlyAtStart,false);
    Pbool->Set_help("Use the mouse cursor of the host system instead of drawing a DOS mouse cursor. Activated when the mouse is not locked.");
    Pbool->SetBasic(true);

    const char * emulation[] = {"integration", "locked", "always", "never", nullptr};
    Pstring  = sdl_sec->Add_string("mouse_emulation", Property::Changeable::Always, emulation[1]);
    Pstring->Set_help(
        "When is mouse emulated ?\n"
        "integration: when not locked\n"
        "locked:      when locked\n"
        "always:      every time\n"
        "never:       at no time\n"
        "If disabled, the mouse position in DOSBox-X is exactly where the host OS reports it.\n"
        "When using a high DPI mouse, the emulation of mouse movement can noticeably reduce the\n"
        "sensitiveness of your device, i.e. the mouse is slower but more precise.");
    Pstring->Set_values(emulation);
    Pstring->SetBasic(true);

    Pint = sdl_sec->Add_int("mouse_wheel_key", Property::Changeable::WhenIdle, -1);
    Pint->SetMinMax(-7,7);
    Pint->Set_help("Convert mouse wheel movements into keyboard presses such as arrow keys.\n"
        "0: disabled; 1: up/down arrows; 2: left/right arrows; 3: PgUp/PgDn keys.\n"
        "4: Ctrl+up/down arrows; 5: Ctrl+left/right arrows; 6: Ctrl+PgUp/PgDn keys.\n"
        "7: Ctrl+W/Z, as supported by text editors like WordStar and MS-DOS EDIT.\n"
        "Putting a minus sign in front will disable the conversion for guest systems.");
    Pint->SetBasic(true);

    Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
    Pbool->Set_help("Wait before closing the console if DOSBox-X has an error.");
    Pbool->SetBasic(true);

    Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
    Pmulti->SetValue("higher,normal",/*init*/true);
    Pmulti->Set_help("Priority levels for DOSBox-X. Second entry behind the comma is for when DOSBox-X is not focused/minimized.\n"
                     "  pause is only valid for the second entry.");
    Pmulti->SetBasic(true);

    const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
    Pstring = Pmulti->GetSection()->Add_string("active",Property::Changeable::Always,"higher");
    Pstring->Set_values(actt);

    const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
    Pstring = Pmulti->GetSection()->Add_string("inactive",Property::Changeable::Always,"normal");
    Pstring->Set_values(inactt);

    Pstring = sdl_sec->Add_path("mapperfile",Property::Changeable::Always,MAPPERFILE);
    Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");
    Pstring->SetBasic(true);

    Pstring = sdl_sec->Add_path("mapperfile_sdl1",Property::Changeable::Always,"");
    Pstring->Set_help("File used to load/save the key/event mappings from DOSBox-X SDL1 builds. If set it will override \"mapperfile\" for SDL1 builds.");

    Pstring = sdl_sec->Add_path("mapperfile_sdl2",Property::Changeable::Always,"");
    Pstring->Set_help("File used to load/save the key/event mappings from DOSBox-X SDL2 builds. If set it will override \"mapperfile\" for SDL2 builds.");

    Pbool = sdl_sec->Add_bool("forcesquarecorner", Property::Changeable::OnlyAtStart, true);
    Pbool->Set_help("If set, DOSBox-X will force square corners (instead of round corners) for the DOSBox-X window when running in Windows 11.");

	const char* truefalseautoopt[] = { "true", "false", "1", "0", "auto", 0};
    Pstring = sdl_sec->Add_string("usescancodes",Property::Changeable::OnlyAtStart,"auto");
    Pstring->Set_values(truefalseautoopt);
    Pstring->Set_help("Avoid usage of symkeys, in favor of scancodes. Might not work on all operating systems.\n"
        "If set to \"auto\" (default), it is enabled when using non-US keyboards in SDL1 builds.");
    Pstring->SetBasic(true);

#if C_GAMELINK
    Pbool = sdl_sec->Add_bool("gamelink master", Property::Changeable::OnlyAtStart, false);
    Pbool->Set_help("Allow Game Link connections e.g. from Grid Cartographer; required for output=gamelink, otherwise optional.");
    Pbool = sdl_sec->Add_bool("gamelink snoop", Property::Changeable::OnlyAtStart, false);
    Pbool->Set_help("Connect to an existing Game Link session and output link data instead of sending own data. Compares memory contents to find a suitable memory offset of peeks.");
    Pint = sdl_sec->Add_int("gamelink load address", Property::Changeable::Always, 0);
    Pbool->Set_help("Configure the original load address of the software (when running in plain DOSBox) so that gamelink accesses are adjusted for different load addresses.");
#endif

    Pint = sdl_sec->Add_int("overscan",Property::Changeable::Always, 0);
    Pint->SetMinMax(0,10);
    Pint->Set_help("Width of the overscan border (0 to 10) for the \"surface\" output.");
    Pint->SetBasic(true);

    Pstring = sdl_sec->Add_string("titlebar", Property::Changeable::Always, "");
    Pstring->Set_help("Change the string displayed in the DOSBox-X title bar.");
    Pstring->SetBasic(true);

    Pbool = sdl_sec->Add_bool("showbasic", Property::Changeable::Always, true);
    Pbool->Set_help("If set, DOSBox-X will show basic information including the DOSBox-X version number and current running speed in the title bar.");
    Pbool->SetBasic(true);

    Pbool = sdl_sec->Add_bool("showdetails", Property::Changeable::Always, false);
    Pbool->Set_help("If set, DOSBox-X will show the cycles count (FPS) and emulation speed relative to realtime in the title bar.");
    Pbool->SetBasic(true);

    Pbool = sdl_sec->Add_bool("showmenu", Property::Changeable::Always, true);
    Pbool->Set_help("Whether to show the menu bar (if supported). Default true.");
    Pbool->SetBasic(true);

//  Pint = sdl_sec->Add_int("overscancolor",Property::Changeable::Always, 0);
//  Pint->SetMinMax(0,1000);
//  Pint->Set_help("Value of overscan color.");
}

void SDL_OnSectionPropChange(Section *x) {
    (void)x;//UNUSED
    Section_prop * section = static_cast<Section_prop *>(control->GetSection("sdl"));

    {
        bool cfg_want_menu = section->Get_bool("showmenu");

        /* -- -- decide whether to set menu */
        if (menu_gui && !control->opt_nomenu && cfg_want_menu)
            DOSBox_SetMenu();
        else
            DOSBox_NoMenu();
    }
}

static void show_warning(char const * const message) {
    bool textonly = true;
#ifdef WIN32
    textonly = false;
    if ( !sdl.inited && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0 ) textonly = true;
    sdl.inited = true;
#endif
    LOG_MSG( "Warning: %s", message);
    if(textonly) return;
#if defined(C_SDL2)
    if (!sdl.window)
        if (!GFX_SetSDLSurfaceWindow(640,400)) return;
    sdl.surface = SDL_GetWindowSurface(sdl.window);
#else
    if(!sdl.surface) sdl.surface = SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;
#endif
    if(!sdl.surface) return;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    uint32_t rmask = 0xff000000;
    uint32_t gmask = 0x00ff0000;
    uint32_t bmask = 0x0000ff00;
#else
    uint32_t rmask = 0x000000ff;
    uint32_t gmask = 0x0000ff00;                    
    uint32_t bmask = 0x00ff0000;
#endif
    SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
    if (!splash_surf) return;

    int x = 120,y = 20;
    std::string m(message),m2;
    std::string::size_type a,b,c,d;
   
    while(m.size()) { //Max 50 characters. break on space before or on a newline
        c = m.find('\n');
        d = m.rfind(' ',50);
        if(c>d) a=b=d; else a=b=c;
        if( a != std::string::npos) b++; 
        m2 = m.substr(0,a); m.erase(0,b);
        OutputString((unsigned int)x,(unsigned int)y,m2.c_str(),0xffffffffu,0,splash_surf);
        y += 20;
    }
   
    SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
#if defined(C_SDL2)
    SDL_UpdateWindowSurface(sdl.window);
#else
    SDL_Flip(sdl.surface);
#endif
    SDL_Delay(12000);
}
   
static void launcheditor(std::string edit) {
    if (control->configfiles.size() && control->configfiles.front().size())
        execlp(edit.c_str(),edit.c_str(),control->configfiles.front().c_str(),(char*) 0);
    std::string path,file;
    Cross::CreatePlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
    FILE* f = fopen(path.c_str(),"r");
    if(!f && !control->PrintConfig(path.c_str())) {
        printf("tried creating %s. but failed.\n",path.c_str());
        exit(1);
    }
    if(f) fclose(f);

    execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);

    //if you get here the launching failed!
    printf("can't find editor(s) specified at the command line.\n");
    exit(1);
}
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

static void launchcaptures(std::string const& edit) {
    std::string path,file;
    struct stat cstat;
    Section* t = control->GetSection("dosbox");
    if(t) file = t->GetPropValue("captures");
    if(!t || file == NO_SUCH_PROPERTY) {
        printf("Config system messed up.\n");
        exit(1);
    }
    path = ".";
    path += CROSS_FILESPLIT;
    path += file;

    stat(path.c_str(),&cstat);
    if(cstat.st_mode & S_IFDIR) {
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    } else {
        path = "";
        Cross::CreatePlatformConfigDir(path);
        path += file;
        Cross::CreateDir(path);
        stat(path.c_str(),&cstat);
        if((cstat.st_mode & S_IFDIR) == 0) {
            printf("%s doesn't exist or isn't a directory.\n",path.c_str());
            exit(1);
        }
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    }
}

static void launchsaves(std::string const& edit) {
    std::string path,file;
    struct stat cstat;
    file="SAVE";
    path = ".";
    path += CROSS_FILESPLIT;
    path += file;
    stat(path.c_str(),&cstat);
    if(cstat.st_mode & S_IFDIR) {
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    } else {
        path = "";
        Cross::CreatePlatformConfigDir(path);
        path += file;
        Cross::CreateDir(path);
        stat(path.c_str(),&cstat);
        if((cstat.st_mode & S_IFDIR) == 0) {
            printf("%s doesn't exists or isn't a directory.\n",path.c_str());
            exit(1);
        }
        execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
        //if you get here the launching failed!
        printf("can't find filemanager %s\n",edit.c_str());
        exit(1);
    }
}

static void printconfiglocation() {
    std::string path,file;
    Cross::CreatePlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
     
    FILE* f = fopen(path.c_str(),"r");
    if(!f && !control->PrintConfig(path.c_str())) {
        printf("tried creating %s. but failed",path.c_str());
        exit(1);
    }
    if(f) fclose(f);
    printf("%s\n",path.c_str());
    exit(0);
}

static void eraseconfigfile() {
    FILE* f = fopen("dosbox-x.conf","r");
	if (!f) f = fopen("dosbox.conf","r");
    if (f) {
        fclose(f);
        show_warning("Warning: dosbox-x.conf (or dosbox.conf) exists in current working directory.\nThis will override the configuration file at runtime.\n");
    }
    std::string path,file;
    Cross::GetPlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
    f = fopen(path.c_str(),"r");
    if (!f) exit(0);
    fclose(f);
    unlink(path.c_str());
    exit(0);
}

static void erasemapperfile() {
    FILE* g = fopen("dosbox-x.conf","r");
	if (!g) g = fopen("dosbox.conf","r");
    if (g) {
        fclose(g);
        show_warning("Warning: dosbox-x.conf (or dosbox.conf) exists in current working directory.\nKeymapping might not be properly reset.\n"
                     "Please reset configuration as well and delete the dosbox-x.conf (or dosbox.conf).\n");
    }

    std::string path,file=MAPPERFILE;
    Cross::GetPlatformConfigDir(path);
    path += file;
    FILE* f = fopen(path.c_str(),"r");
    if (!f) exit(0);
    fclose(f);
    unlink(path.c_str());
    exit(0);
}

void SetNumLock(void) {
#ifdef WIN32
    if (control->opt_disable_numlock_check)
        return;

    // Simulate a key press
    keybd_event(VK_NUMLOCK,0x45,KEYEVENTF_EXTENDEDKEY | 0,0);

    // Simulate a key release
    keybd_event(VK_NUMLOCK,0x45,KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP,0);
#endif
}

void CheckNumLockState(void) {
#ifdef WIN32
    BYTE keyState[256];

    GetKeyboardState((LPBYTE)(&keyState));
    if (keyState[VK_NUMLOCK] & 1) {
        startup_state_numlock = true;
    }
#endif
}

void CheckCapsLockState(void) {
#ifdef WIN32
    BYTE keyState[256];

    GetKeyboardState((LPBYTE)(&keyState));
    if (keyState[VK_CAPITAL] & 1) {
        startup_state_capslock = true;
    }
#endif
}

void CheckScrollLockState(void) {
#ifdef WIN32
    BYTE keyState[256];

    GetKeyboardState((LPBYTE)(&keyState));
    if (keyState[VK_SCROLL] & 1) {
        startup_state_scrlock = true;
    }
#endif
}

extern bool log_keyboard_scan_codes;

bool showconsole_init = false;

#if C_DEBUG
bool DEBUG_IsDebuggerConsoleVisible(void);
#endif

void DOSBox_ShowConsole() {
#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD crd;
    HWND hwnd;

#if C_DEBUG
    /* if the debugger has already taken the console, do nothing */
    if (DEBUG_IsDebuggerConsoleVisible())
        return;
#endif

    /* if WE have already opened the console, do nothing */
    if (showconsole_init)
        return;

    /* Microsoft Windows: Allocate a console and begin spewing to it.
       DOSBox-X is compiled on Windows platforms as a Win32 application, and therefore, no console. */
    /* FIXME: What about "file handles" 0/1/2 emulated by C library, for use with _open/_close/_lseek/etc? */
    AllocConsole();

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    crd = csbi.dwSize;
    crd.X = 130;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), crd);

    hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_MAXIMIZE);

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    showconsole_init = true;
#endif
}

void DOSBox_ConsolePauseWait() {
    char c;

    printf("Press ENTER key to continue\n");
    do {
        if (fread(&c, 1, 1, stdin) != 1) break;
    } while (!(c == 13 || c == 10)); /* wait for Enter key */
}

bool usecfgdir = false;
bool DOSBOX_parse_argv() {
    std::string tmp,optname,localname;

    assert(control != NULL);
    assert(control->cmdline != NULL);

	control->cmdline->ChangeOptStyle(CommandLine::either_except);
    control->cmdline->BeginOpt(true/*eat argv*/);
    while (control->cmdline->GetOpt(optname)) {
        std::transform(optname.begin(), optname.end(), optname.begin(), ::tolower);

        if (optname == "v" || optname == "ver" || optname == "version") {
            DOSBox_ShowConsole();

            fprintf(stderr,"\nDOSBox-X version %s %s, copyright 2011-%s The DOSBox-X Team.\n",VERSION,SDL_STRING,COPYRIGHT_END_YEAR);
            fprintf(stderr,"DOSBox-X project maintainer: joncampbell123 (The Great Codeholio)\n\n");
            fprintf(stderr,"DOSBox-X comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
            fprintf(stderr,"and you are welcome to redistribute it under certain conditions;\n");
            fprintf(stderr,"please read the COPYING file thoroughly before doing so.\n\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        }
        else if (optname == "?" || optname == "h" || optname == "help") {
            DOSBox_ShowConsole();

            fprintf(stderr,"\nDOSBox-X version %s %s, copyright 2011-%s The DOSBox-X Team.\n",VERSION,SDL_STRING,COPYRIGHT_END_YEAR);
            fprintf(stderr,"DOSBox-X project maintainer: joncampbell123 (The Great Codeholio)\n\n");
            fprintf(stderr,"dosbox-x [name] [options]\n\n");
            fprintf(stderr,"Options can be started with either \"-\" or \"/\" (e.g. \"-help\" or \"/help\"):\n\n");
            fprintf(stderr,"  -?, -h, -help                           Show this help screen\n");
            fprintf(stderr,"  -v, -ver, -version                      Display DOSBox-X version information\n");
            fprintf(stderr,"  -fs, -fullscreen                        Start DOSBox-X in fullscreen mode\n");
            fprintf(stderr,"  -conf <configfile>                      Start DOSBox-X with the specific config file\n");
            fprintf(stderr,"  -editconf <editor>                      Edit the config file with the specific editor\n");
            fprintf(stderr,"  -userconf                               Create user level config file\n");
            fprintf(stderr,"  -printconf                              Print config file location\n");
            fprintf(stderr,"  -eraseconf (or -resetconf)              Erase loaded config file (or user config file and exit)\n");
            fprintf(stderr,"  -erasemapper (or -resetmapper)          Erase loaded mapper file (or user mapper file and exit)\n");
            fprintf(stderr,"  -opencaptures <param>                   Launch captures\n");
            fprintf(stderr,"  -opensaves <param>                      Launch saves\n");
            fprintf(stderr,"  -startui, -startgui, -starttool         Start DOSBox-X with GUI configuration tool\n");
            fprintf(stderr,"  -startmapper                            Start DOSBox-X with the mapper editor\n");
            fprintf(stderr,"  -promptfolder                           Prompt for the working directory when DOSBox-X starts\n");
            fprintf(stderr,"  -nopromptfolder                         Do not prompt for the working directory when DOSBox-X starts\n");
            fprintf(stderr,"  -nogui                                  Do not show GUI\n");
            fprintf(stderr,"  -nomenu                                 Do not show menu\n");
            fprintf(stderr,"  -showcycles                             Show cycles count (FPS) in the title\n");
            fprintf(stderr,"  -showrt                                 Show emulation speed relative to realtime in the title\n");
            fprintf(stderr,"  -socket <socketnum>                     Specify the socket number for null-modem emulation\n");
            fprintf(stderr,"  -savedir <path>                         Set path for the save slots\n");
            fprintf(stderr,"  -defaultdir <path>                      Set the default working path for DOSBox-X\n");
            fprintf(stderr,"  -defaultconf                            Use the default config settings for DOSBox-X\n");
            fprintf(stderr,"  -defaultmapper                          Use the default key mappings for DOSBox-X\n");
#if defined(WIN32)
            fprintf(stderr,"  -disable-numlock-check                  Disable NumLock check (Windows version only)\n");
#endif
            fprintf(stderr,"  -date-host-forced                       Force synchronization of date with host\n");
#if C_DEBUG
            fprintf(stderr,"  -display2 <color>                       Enable standard & monochrome dual-screen mode with <color>\n");
#endif
            fprintf(stderr,"  -lang (or -langcp) <message file>       Use specific message file instead of language= setting\n");
            fprintf(stderr,"  -machine <type>                         Start DOSBox-X with a specific machine <type>\n");
            fprintf(stderr,"  -nodpiaware                             Ignore (do not signal) Windows DPI awareness\n");
            fprintf(stderr,"  -securemode                             Enable secure mode (no drive mounting etc)\n");
            fprintf(stderr,"  -prerun                                 If [name] is given, run it before AUTOEXEC.BAT config section\n");
#if defined(WIN32) && !defined(HX_DOS) || defined(MACOSX) || defined(LINUX)
            fprintf(stderr,"  -hostrun                                Enable running host program via START command and LFN support\n");
#endif
            fprintf(stderr,"  -noconfig                               Do not execute CONFIG.SYS config section\n");
            fprintf(stderr,"  -noautoexec                             Do not execute AUTOEXEC.BAT config section\n");
            fprintf(stderr,"  -exit                                   Exit after executing AUTOEXEC.BAT\n");
            fprintf(stderr,"  -silent                                 Run DOSBox-X silently and exit after executing AUTOEXEC.BAT.\n");
            fprintf(stderr,"  -o <option string>                      Provide command-line option(s) for [name] if specified.\n");
            fprintf(stderr,"  -c <command string>                     Execute this command in addition to AUTOEXEC.BAT.\n");
            fprintf(stderr,"                                          Make sure to surround the command in quotes to cover spaces.\n");
            fprintf(stderr,"  -set <section property=value>           Set the config option (overriding the config file).\n");
            fprintf(stderr,"                                          Make sure to surround the string in quotes to cover spaces.\n");
            fprintf(stderr,"  -time-limit <n>                         Kill the emulator after 'n' seconds\n");
            fprintf(stderr,"  -fastlaunch                             Fast launch mode (skip the BIOS logo and welcome banner)\n");
#if C_DEBUG
            fprintf(stderr,"  -helpdebug                              Show debug-related options\n");
#endif
            fprintf(stderr,"\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        } else if (optname == "helpdebug") {
            DOSBox_ShowConsole();

            fprintf(stderr,"\ndosbox-x [options]\n");
            fprintf(stderr,"\nDOSBox-X version %s %s, copyright 2011-%s The DOSBox-X Team.\n",VERSION,SDL_STRING,COPYRIGHT_END_YEAR);
            fprintf(stderr,"DOSBox-X project maintainer: joncampbell123 (The Great Codeholio)\n\n");
            fprintf(stderr,"Based on DOSBox by the DOSBox Team (See AUTHORS file)\n\n");
            fprintf(stderr,"Debugging options:\n\n");
            fprintf(stderr,"  -debug                                  Set all logging levels to debug\n");
            fprintf(stderr,"  -early-debug                            Log early initialization messages in DOSBox-X (implies -console)\n");
            fprintf(stderr,"  -keydbg                                 Log all SDL key events\n");
            fprintf(stderr,"  -break-start                            Break into debugger at startup\n");
            fprintf(stderr,"  -console                                Show logging console (Windows builds only)\n");
            fprintf(stderr,"  -noconsole                              Do not show logging console (Windows debug builds only)\n");
            fprintf(stderr,"  -log-con                                Log CON output to a log file\n");
            fprintf(stderr,"  -log-int21                              Log calls to INT 21h (debug level)\n");
            fprintf(stderr,"  -log-fileio                             Log file I/O through INT 21h (debug level)\n");
            fprintf(stderr,"  -nolog                                  Do not log anything to log file\n");
            fprintf(stderr,"  -tests                                  Run unit tests to test the DOSBox-X code\n");
            fprintf(stderr,"  -print-ticks                            (Debug) Print emulator time and SDL_GetTicks()\n");
            fprintf(stderr,"\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        }
        else if (optname == "o") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_o.push_back(tmp);
        }
        else if (optname == "c") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_c.push_back(tmp);
        }
        else if (optname == "set") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_set.push_back(tmp);
        }
        else if (optname == "log-con") {
            control->opt_log_con = true;
        }
        else if (optname == "nolog") {
            control->opt_nolog = true;
        }
        else if (optname == "machine") {
            if (!control->cmdline->NextOptArgv(control->opt_machine)) return false;
        }
        else if (optname == "time-limit") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_time_limit = atof(tmp.c_str());
        }
        else if (optname == "break-start") {
            control->opt_break_start = true;
        }
        else if (optname == "silent") {
            putenv(const_cast<char*>("SDL_AUDIODRIVER=dummy"));
            putenv(const_cast<char*>("SDL_VIDEODRIVER=dummy"));
            control->opt_exit = true;
            control->opt_silent = true;
            control->opt_nomenu = true;
            control->opt_fastlaunch = true;
        }
        else if (optname == "test" || optname == "tests" || optname == "gtest_list_tests") {
            putenv(const_cast<char*>("SDL_VIDEODRIVER=dummy"));
            control->opt_test = true;
            control->opt_nolog = true;
            control->opt_noconsole = false;
            control->opt_console = true;
            control->opt_nomenu = true;
            control->opt_fastlaunch = true;
        }
        else if (optname == "exit") {
            control->opt_exit = true;
        }
        else if (optname == "noconfig") {
            control->opt_noconfig = true;
        }
        else if (optname == "noautoexec") {
            control->opt_noautoexec = true;
        }
        else if (optname == "prerun") {
            control->opt_prerun = true;
        }
#if defined(WIN32) && !defined(HX_DOS) || defined(MACOSX) || defined(LINUX)
        else if (optname == "hostrun") {
            winrun = true;
        }
#endif
#if defined(WIN32) && !defined(HX_DOS)
        else if (optname == "winrun") {
            winrun = true;
        }
#endif
        else if (optname == "securemode") {
            control->opt_securemode = true;
        }
        else if (optname == "nodpiaware") {
            control->opt_disable_dpi_awareness = true;
        }
        else if (optname == "keydbg") {
            log_keyboard_scan_codes = true;
        }
        else if (optname == "date-host-forced" || optname == "date_host_forced") {
            control->opt_date_host_forced = true;
        }
        else if (optname == "showrt") {
            control->opt_showrt = true;
        }
        else if (optname == "showcycles") {
            control->opt_showcycles = true;
        }
        else if (optname == "startmapper") {
            control->opt_startmapper = true;
        }
        else if (optname == "fs" || optname == "fullscreen") {
            control->opt_fullscreen = true;
        }
        else if (optname == "startui" || optname == "startgui" || optname == "starttool") {
            control->opt_startui = true;
        }
        else if (optname == "disable-numlock-check" || optname == "disable_numlock_check") {
            /* mainline DOSBox expects -disable_numlock_check so we support that here too */
            control->opt_disable_numlock_check = true;
        }
        else if (optname == "savedir") {
            if (!control->cmdline->NextOptArgv(custom_savedir)) return false;
#if defined(WIN32) && defined(C_SDL2)
            localname = custom_savedir;
            if (!FileDirExistCP(custom_savedir.c_str()) && FileDirExistUTF8(localname, custom_savedir.c_str()) == 2)
                custom_savedir = localname;
#endif
        }
        else if (optname == "defaultdir") {
            control->opt_used_defaultdir = true;
            if (control->cmdline->NextOptArgv(tmp)) {
                localname = tmp;
                if (FileDirExistCP(tmp.c_str()) == 2)
                    chdir(tmp.c_str());
#if defined(WIN32) && defined(C_SDL2)
                else if (FileDirExistUTF8(localname, tmp.c_str()) == 2)
                    chdir(localname.c_str());
#endif
            } else
                usecfgdir = true;
        }
        else if (optname == "userconf") {
            control->opt_userconf = true;
        }
        else if (optname == "lang" || optname == "langcp") {
            if (!control->cmdline->NextOptArgv(control->opt_lang)) return false;
            if (optname == "langcp") control->opt_langcp = true;
        }
        else if (optname == "fastbioslogo") {
            control->opt_fastbioslogo = true;
        }
        else if (optname == "fastlaunch") {
            control->opt_fastlaunch = true;
        }
        else if (optname == "conf") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->config_file_list.push_back(tmp);
        }
        else if (optname == "defaultconf") {
            control->opt_defaultconf = true;
        }
        else if (optname == "editconf") {
            if (!control->cmdline->NextOptArgv(control->opt_editconf)) return false;
        }
        else if (optname == "opencaptures") {
            if (!control->cmdline->NextOptArgv(control->opt_opencaptures)) {
#if defined(LINUX)
                control->opt_opencaptures = "xdg-open";
#else
                return false;
#endif
	    }
        }
        else if (optname == "opensaves") {
            if (!control->cmdline->NextOptArgv(control->opt_opensaves)) {
#if defined(LINUX)
                control->opt_opensaves = "xdg-open";
#else
                return false;
#endif
	    }
        }
        else if (optname == "eraseconf") {
            control->opt_eraseconf = true;
        }
        else if (optname == "resetconf") {
            control->opt_resetconf = true;
        }
        else if (optname == "printconf") {
            control->opt_printconf = true;
        }
        else if (optname == "defaultmapper") {
            control->opt_defaultmapper = true;
        }
        else if (optname == "erasemapper") {
            control->opt_erasemapper = true;
        }
        else if (optname == "resetmapper") {
            control->opt_resetmapper = true;
        }
        else if (optname == "log-int21") {
            control->opt_logint21 = true;
        }
        else if (optname == "log-fileio") {
            control->opt_logfileio = true;
        }
        else if (optname == "noconsole") {
            control->opt_noconsole = true;
            control->opt_console = false;
        }
        else if (optname == "console") {
            control->opt_noconsole = false;
            control->opt_console = true;
        }
#if C_DEBUG
        else if (optname == "display2") {
        uint8_t disp2_color = 0;
            if (control->cmdline->NextOptArgv(tmp)) {
                if (strcasecmp(tmp.c_str(),"amber")==0) disp2_color=1;
                else if (strcasecmp(tmp.c_str(),"green")==0) disp2_color=2;
            }
            DISP2_Init(disp2_color);
            control->opt_display2 = true;
        }
#endif
        else if (optname == "nomenu") {
            control->opt_nomenu = true;
        }
        else if (optname == "nogui") {
            control->opt_nogui = true;
        }
        else if (optname == "nopromptfolder") {
            control->opt_promptfolder = 0;
        }
        else if (optname == "promptfolder") {
            control->opt_promptfolder = 2;
        }
        else if (optname == "debug") {
            control->opt_debug = true;
        }
        else if (optname == "early-debug") {
            control->opt_earlydebug = true;
            control->opt_console = true;
        }
        else if (optname == "print-ticks") {
            control->opt_print_ticks = true;
            control->opt_console = true;
        }
        else if (optname == "socket") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            socknum = std::stoi(tmp);
        } else {
            printf("WARNING: Unknown option %s (first parsing stage)\n",optname.c_str());
        }
    }
    if (control->opt_display2) {
        control->opt_break_start = false;
        control->opt_console = false;
    }

    /* now that the above loop has eaten all the options from the command
     * line, scan the command line for batch files to run.
     * https://github.com/joncampbell123/dosbox-x/issues/369 */
    control->cmdline->BeginOpt(/*don't eat*/false);
    while (!control->cmdline->CurrentArgvEnd()) {
        control->cmdline->GetCurrentArgv(tmp);
        trim(tmp);
        localname = tmp;
        int rescp = FileDirExistCP(tmp.c_str()), resutf8 = rescp||!tmp.size()?0:FileDirExistUTF8(localname, tmp.c_str());
        if (!rescp && resutf8) {
            tmp = localname;
            rescp = resutf8;
        }
        const char *ext = strrchr(tmp.c_str(),'.'); /* if it looks like a file... with an extension */
        if (rescp) {
            if (rescp == 2 || (ext != NULL && rescp == 1 && (!strcasecmp(ext,".zip") || !strcasecmp(ext,".7z")))) {
                control->auto_bat_additional.push_back("@mount c: \""+tmp+"\" -nl");
                control->cmdline->EatCurrentArgv();
                continue;
            } else if (ext != NULL && rescp == 1 && (!strcasecmp(ext,".bat") || !strcasecmp(ext,".exe") || !strcasecmp(ext,".com"))) { /* .BAT files given on the command line trigger automounting C: to run it */
                control->auto_bat_additional.push_back(tmp);
                control->cmdline->EatCurrentArgv();
                continue;
            }
        }

        control->cmdline->NextArgv();
    }

    return true;
}

void MSG_Init();
void DOSBOX_RealInit();
void DOSBOX_InitTickLoop();
void TIMER_ShutdownTickHandlers();
void DOSBOX_SetupConfigSections(void);
void PAGING_Init();
void IO_Init();
void Init_VGABIOS();
void Init_AddressLimitAndGateMask();
void Init_RAM();
void Init_MemHandles();
void Init_MemoryAccessArray();
void Init_PCJR_CartridgeROM();
void Init_PS2_Port_92h();
void Init_A20_Gate();
void HARDWARE_Init();
void CAPTURE_Init();
void ROMBIOS_Init();
void CALLBACK_Init();
void Init_DMA();
void Init_PIC();
void PCIBUS_Init();
void PROGRAMS_Init();
void RENDER_Init();
void TIMER_Init();
void CMOS_Init();
void VGA_Init();
void CPU_Init();
void ISAPNP_Cfg_Init();
#if C_FPU
void FPU_Init();
#endif
void KEYBOARD_Init();
void VOODOO_Init();
void GLIDE_Init();
void MIXER_Init();
void MIDI_Init();

/* Init all the sections */
void MPU401_Init();
#if C_DEBUG
void DEBUG_Init();
#endif
void SBLASTER_Init();
void GUS_Init();
void IMFC_Init();
void INNOVA_Init();
void PCSPEAKER_Init();
void TANDYSOUND_Init();
void DISNEY_Init();
void PS1SOUND_Init();
void BIOS_Init();
void INT10_Init();
void JOYSTICK_Init();
void SERIAL_Init();
#if C_PRINTER
void PRINTER_Init();
#endif
void PARALLEL_Init();
void DONGLE_Init();
void DOS_Init();
void XMS_Init();
void EMS_Init();
void MOUSE_Init();
void DOS_KeyboardLayout_Init();
void CDROM_Image_Init();
void MSCDEX_Init();
void DRIVES_Init();
void IPX_Init();
void IDE_Init();
void NE2K_Init();
void FDC_Primary_Init();
void AUTOEXEC_Init();
void DOS_InitClock();

#if defined(WIN32)
// NTS: I intend to add code that not only indicates High DPI awareness but also queries the monitor DPI
//      and then factor the DPI into DOSBox-X's scaler and UI decisions.
void Windows_DPI_Awareness_Init() {
    // if the user says not to from the command line, or disables it from dosbox-x.conf, then don't enable DPI awareness.
    if (!dpi_aware_enable || control->opt_disable_dpi_awareness)
        return;

    /* log it */
    LOG(LOG_MISC,LOG_DEBUG)("Win32: I will announce High DPI awareness to Windows to eliminate upscaling");

    // turn off DPI scaling so DOSBox-X doesn't look so blurry on Windows 8 & Windows 10.
    // use GetProcAddress and LoadLibrary so that these functions are not hard dependencies that prevent us from
    // running under Windows 7 or XP.
    HRESULT (WINAPI *__SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS) = NULL; // windows 8.1
    BOOL (WINAPI *__SetProcessDPIAware)(void) = NULL; // vista/7/8/10
    HMODULE __user32;
    HMODULE __shcore;

    __user32 = GetModuleHandle("USER32.DLL");
    __shcore = GetModuleHandle("SHCORE.DLL");

    if (__user32)
        __SetProcessDPIAware = (BOOL(WINAPI *)(void))GetProcAddress(__user32, "SetProcessDPIAware");
    if (__shcore)
        __SetProcessDpiAwareness = (HRESULT (WINAPI *)(PROCESS_DPI_AWARENESS))GetProcAddress(__shcore, "SetProcessDpiAwareness");

    if (__SetProcessDpiAwareness) {
        LOG(LOG_MISC,LOG_DEBUG)("SHCORE.DLL exports SetProcessDpiAwareness function, calling it to signal we are DPI aware.");
        if (__SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) != S_OK)
            LOG(LOG_MISC,LOG_DEBUG)("SetProcessDpiAwareness failed");
    }
    if (__SetProcessDPIAware) {
        LOG(LOG_MISC,LOG_DEBUG)("USER32.DLL exports SetProcessDPIAware function, calling it to signal we are DPI aware.");
        __SetProcessDPIAware();
    }
}
#endif

bool VM_Boot_DOSBox_Kernel() {
    if (!dos_kernel_disabled) {
        RemoveEMSPageFrame();
        RemoveUMBBlock();
        DisableINT33();
        DOS_GetMemory_unmap();
        VFILE_Shutdown();
        PROGRAMS_Shutdown();
        DOS_UninstallMisc();
        SBLASTER_DOS_Shutdown();
        GUS_DOS_Shutdown();
        EMS_DoShutDown();
        XMS_DoShutDown();
        DOS_DoShutDown();

        DispatchVMEvent(VM_EVENT_DOS_SURPRISE_REBOOT); // <- apparently we rebooted without any notification (such as jmp'ing to FFFF:0000)

        dos_kernel_disabled = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif
    }

    if (dos_kernel_disabled) {
        /* in case of reboot */
        Init_MemHandles();

        DispatchVMEvent(VM_EVENT_DOS_BOOT); // <- just starting the DOS kernel now

        /* DOS kernel init */
        dos_kernel_disabled = false; // FIXME: DOS_Init should install VM callback handler to set this
        void DOS_Startup(Section* sec);
        DOS_Startup(NULL);
        maincp = 0;

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        void update_pc98_function_row(unsigned char setting,bool force_redraw=false);

        void DRIVES_Startup(Section *s);
        DRIVES_Startup(NULL);

        /* NEC's function key row seems to be deeply embedded in the CON driver. Am I wrong? */
        if (IS_PC98_ARCH) update_pc98_function_row(1);

        DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY); // <- kernel is ready

        /* Date/time */
        DOS_InitClock();

        /* keyboard mapping, at this point in CONFIG.SYS parsing, right? */
        void DOS_KeyboardLayout_Startup(Section* sec);
        DOS_KeyboardLayout_Startup(NULL);

        /* Most MS-DOS installations have a DEVICE=C:\HIMEM.SYS somewhere near the top of their CONFIG.SYS */
        void XMS_Startup(Section *sec);
        XMS_Startup(NULL);

        /* And then after that, usually a DEVICE=C:\EMM386.EXE just after HIMEM.SYS */
        void EMS_Startup(Section* sec);
        EMS_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_CONFIG_SYS_DONE); // <- we just finished executing CONFIG.SYS
        SHELL_Init(); // <- NTS: this will change CPU instruction pointer!
        DispatchVMEvent(VM_EVENT_DOS_INIT_SHELL_READY); // <- we just finished loading the shell (COMMAND.COM)

        /* it's time to init parsing AUTOEXEC.BAT */
        void AUTOEXEC_Startup(Section *sec);
        AUTOEXEC_Startup(NULL);

        /* Most MS-DOS installations run MSCDEX.EXE from somewhere in AUTOEXEC.BAT. We do the same here, in a fashion. */
        /* TODO: Can we make this an OPTION if the user doesn't want to make MSCDEX.EXE resident? */
        /* TODO: When we emulate executing AUTOEXEC.BAT between INIT_SHELL_READY and AUTOEXEC_BAT_DONE, can we make a fake MSCDEX.EXE within drive Z:\
         *       and auto-add a Z:\DOS\MSCDEX.EXE to the top of AUTOEXEC.BAT, command line switches and all. if the user has not already added it? */
        void MSCDEX_Startup(Section* sec);
        MSCDEX_Startup(NULL);

        /* Some installations load the MOUSE.COM driver from AUTOEXEC.BAT as well */
        /* TODO: Can we make this an option? Can we add a fake MOUSE.COM to the Z: drive as well? */
        void MOUSE_Startup(Section *sec);
        MOUSE_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE); // <- we just finished executing AUTOEXEC.BAT
        DispatchVMEvent(VM_EVENT_DOS_INIT_AT_PROMPT); // <- now, we're at the DOS prompt
        SHELL_Run();
    }

    return true;
}

bool VM_PowerOn() {
    if (!guest_machine_power_on) {
        // powering on means power on event, followed by reset assert, then reset deassert
        guest_machine_power_on = true;
        DispatchVMEvent(VM_EVENT_POWERON);
        DispatchVMEvent(VM_EVENT_RESET);
        DispatchVMEvent(VM_EVENT_RESET_END);
    }

    return true;
}

#if defined(LINUX) && C_X11
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include "SDL_syswm.h"
#endif

#if !defined(C_EMSCRIPTEN)
void update_capture_fmt_menu(void);
#endif

void SetWindowTransparency(int trans) {
    if (trans == -1) trans = transparency;
    else if (trans == transparency) return;
    double alpha = (double)(100-trans)/100;
#if defined(C_SDL2)
    SDL_SetWindowOpacity(sdl.window,alpha);
#elif defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    SetWindowLong(GetHWND(), GWL_EXSTYLE, GetWindowLong(GetHWND(), GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(GetHWND(), 0, 255 * alpha, LWA_ALPHA);
#elif defined(MACOSX)
    SetAlpha(alpha);
#elif defined(LINUX) && C_X11
    Display *dpy;
    Window window;
    SDL_SysWMinfo wminfo;
    memset(&wminfo,0,sizeof(wminfo));
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWMInfo(&wminfo) >= 0) {
        dpy = wminfo.info.x11.display;
        if (!dpy) return;
        if (wminfo.info.x11.wmwindow) window = wminfo.info.x11.wmwindow;
    } else {
        dpy = XOpenDisplay(NULL);
        if (!dpy) return;
        window = DefaultRootWindow(dpy);
    }
    unsigned long opacity = (unsigned long)(0xFFFFFFFFul * alpha);
    Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    XChangeProperty(dpy, window, atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1L);
#endif
    transparency = trans;
}

void GetDrawWidthHeight(unsigned int *pdrawWidth, unsigned int *pdrawHeight) {
    *pdrawWidth = sdl.draw.width;
    *pdrawHeight = sdl.draw.height;
}

void GetMaxWidthHeight(unsigned int *pmaxWidth, unsigned int *pmaxHeight) {
    unsigned int maxWidth = sdl.desktop.full.width;
    unsigned int maxHeight = sdl.desktop.full.height;

#if defined(C_SDL2)
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(sdl.displayNumber?sdl.displayNumber-1:0,&dm) == 0) {
        maxWidth = dm.w;
        maxHeight = dm.h;
    }
#elif defined(WIN32)
    maxWidth = GetSystemMetrics(SM_CXSCREEN);
    maxHeight = GetSystemMetrics(SM_CYSCREEN);
#elif defined(MACOSX)
    auto mainDisplayId = CGMainDisplayID();
    maxWidth = CGDisplayPixelsWide(mainDisplayId);
    maxHeight = CGDisplayPixelsHigh(mainDisplayId);
#elif defined(LINUX) && C_X11
    Display *dpy = XOpenDisplay(NULL);
    if (dpy) {
        int snum = DefaultScreen(dpy);
        maxWidth = DisplayWidth(dpy, snum);
        maxHeight = DisplayHeight(dpy, snum);
    }
#endif
    *pmaxWidth = maxWidth;
    *pmaxHeight = maxHeight;
}

#if defined(LINUX)
bool x11_on_top = false;
#endif

#if defined(MACOSX)
bool macosx_on_top = false;
#endif

bool is_always_on_top(void) {
#if defined(_WIN32)
    DWORD dwExStyle = ::GetWindowLong(GetHWND(), GWL_EXSTYLE);
    return !!(dwExStyle & WS_EX_TOPMOST);
#elif defined(MACOSX)
    return macosx_on_top;
#elif defined(LINUX)
    return x11_on_top;
#else
    return false;
#endif
}

bool custom_bios = false;

// OK why isn't this being set for Linux??
#ifndef SDL_MAIN_NOEXCEPT
#define SDL_MAIN_NOEXCEPT
#endif

#if defined(WIN32) && !defined(HX_DOS)
#include "Shlobj.h"
int CALLBACK FolderBrowserCallback(HWND h_Dlg, UINT uMsg, LPARAM lParam, LPARAM lpData) {
    (void)lParam;
    if (uMsg == BFFM_INITIALIZED)
        SendMessageW(h_Dlg, BFFM_SETEXPANDED, TRUE, lpData);
    return 0;
}

std::wstring win32_prompt_folder(const char *default_folder) {
    std::wstring res;
    const WCHAR text[] = L"Select folder where to run emulation, which will become DOSBox-X's working directory:";
    const size_t size = default_folder == NULL? 0 : strlen(default_folder)+1;
    wchar_t* wfolder = default_folder == NULL ? NULL : new wchar_t[size];
    if (default_folder != NULL) mbstowcs (wfolder, default_folder, size);

    //fix memory leaks
    std::wstring wsfolder = wfolder ? wfolder : L"";
    if(wfolder) delete[] wfolder;
    wfolder = &wsfolder[0];

#if 0 // Browse for folder using SHBrowseForFolder, which works on Windows XP and higher
    WCHAR szDir[MAX_PATH];
    BROWSEINFOW bInfo;
    bInfo.hwndOwner = GetHWND();
    bInfo.pidlRoot = NULL;
    bInfo.pszDisplayName = szDir;
    bInfo.lpszTitle = text;
    bInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    if (wfolder != NULL) {
        bInfo.lpfn   = FolderBrowserCallback;
        bInfo.lParam = (LPARAM)wfolder;
    } else {
        bInfo.lpfn = NULL;
        bInfo.lParam = 0;
    }
    LPITEMIDLIST lpItem = SHBrowseForFolderW(&bInfo);
    if (lpItem != NULL) {
        SHGetPathFromIDListW(lpItem, szDir);
        res = std::wstring(szDir);
        CoTaskMemFree(lpItem);
    } else
        return std::wstring();
#else // Use IFileDialog (Visual Studio builds) or OPENFILENAME (MinGW builds)
# if !defined(__MINGW32__) /* MinGW does not have these headers */
    IFileDialog* ifd; /* Windows Vista file/folder picker interface COM object (shobjidl_core.h) */
    /* Try the new picker first (Windows Vista or higher) which makes it possible to pick a folder */
    if(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_IFileDialog, (void**)(&ifd)) == S_OK) {
        HRESULT hr;
        HMODULE __shell32 = GetModuleHandle("SHELL32.DLL");
        if (wfolder != NULL && __shell32) {
            PIDLIST_ABSOLUTE pidl = NULL;
            SHParseDisplayName(wfolder, NULL, &pidl, 0, NULL);
            HRESULT (WINAPI *__SHCreateItemFromIDList)(PCIDLIST_ABSOLUTE, REFIID, void**) = NULL;
            __SHCreateItemFromIDList = (HRESULT (WINAPI *)(PCIDLIST_ABSOLUTE, REFIID, void**))GetProcAddress(__shell32,"SHCreateItemFromIDList");
            if (pidl != NULL && __SHCreateItemFromIDList) {
                IShellItem *item = NULL;
                __SHCreateItemFromIDList(pidl, IID_IShellItem, (LPVOID*)&item);
                if(item != NULL) {
                    ifd->SetDefaultFolder(item);
                    item->Release();
                }
                CoTaskMemFree(pidl);
            }
        }
        ifd->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_DONTADDTORECENT);
        ifd->SetTitle(text);
        ifd->SetOkButtonLabel(L"Choose");
        hr = ifd->Show(NULL);
        if(hr == S_OK) {
            IShellItem* sh = NULL;
            if(ifd->GetFolder(&sh) == S_OK) {
                LPWSTR str = NULL;

                if(sh->GetDisplayName(SIGDN_FILESYSPATH, &str) == S_OK) {
                    res = str;
                    CoTaskMemFree(str);
                }

                sh->Release();
            }

            ifd->Release();
            return res;
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            /* the user clicked cancel, sorry */
            ifd->Release();
            return std::wstring();
        }
        ifd->Release();
        /* didn't work, try the other method below for Windows XP and below */
    }
# endif
    OPENFILENAMEW of;
    WCHAR tmp[1024];
    tmp[0] = 0;
    memset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of);
    of.lpstrFile = tmp;
    of.nMaxFile = sizeof(tmp) / sizeof(tmp[0]); // Size in CHARACTERS not bytes
    of.lpstrInitialDir = wfolder;
    of.lpstrTitle = text;
    of.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    of.lpstrFilter = L"DOSBox-X configuration file\0" L"dosbox-x.conf;dosbox.conf\0";
    if (GetOpenFileNameW(&of)) {
        if (of.nFileOffset >= sizeof(tmp)) return std::wstring();
        while ((of.nFileOffset > 0 && tmp[of.nFileOffset - 1] == '/') || tmp[of.nFileOffset - 1] == '\\') of.nFileOffset--;
        if (of.nFileOffset == 0) return std::wstring();
        res = std::wstring(tmp, (size_t)of.nFileOffset);
    }
#endif

    return res;
}
#endif

void DISP2_Init(uint8_t color), DISP2_Shut();
//extern void UI_Init(void);
void grGlideShutdown(void);
int main(int argc, char* argv[]) SDL_MAIN_NOEXCEPT {
    CommandLine com_line(argc,argv);
    Config myconf(&com_line);
    bool saved_opt_test;

    control=&myconf;

#if defined(WIN32) && !defined(HX_DOS)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
#endif

    /* -- parse command line arguments */
    if (!DOSBOX_parse_argv()) return 1;

    if (control->opt_time_limit > 0)
        time_limit_ms = (Bitu)(control->opt_time_limit * 1000);

    if (control->opt_console)
        DOSBox_ShowConsole();

    /* -- Handle some command line options */
    if (control->opt_resetconf)
        eraseconfigfile();
    if (control->opt_printconf)
        printconfiglocation();
    if (control->opt_resetmapper)
        erasemapperfile();

    /* -- Early logging init, in case these details are needed to debug problems at this level */
    /*    If --early-debug was given this opens up logging to STDERR until Log::Init() */
    LOG::EarlyInit();

    /* -- Init the configuration system and add default values */
    CheckNumLockState();
    CheckCapsLockState();
    CheckScrollLockState();

    /* -- setup the config sections for config parsing */
    SDL_SetupConfigSection();
    LOG::SetupConfigSection();
    DOSBOX_SetupConfigSections();

#if 0 /* VGA_Draw_2 self test: dot clock */
    {
        const double start_time = 0;
        signed long long count = 0;
        VGA_Draw_2 t;

        fprintf(stderr,"VGA_Draw_2: 1000Hz\n");

        t.dotclock.set_rate(1000,start_time);/*hz*/
        for (int i=0;i < 10000;i++) {
            t.dotclock.update(start_time + i);

            if (labs(t.dotclock.ticks - (signed long long)i) > 1ll) { /* NTS: Expect possible +/- 1 error due to floating point */
                fprintf(stderr,"* TEST FAILURE:\n");
                fprintf(stderr,"   ticks=%lld ticks_prev=%lld\n",(signed long long)i,(signed long long)t.dotclock.ticks);
                return 1;
            }
        }

        t.dotclock.reset(start_time);
        assert(t.dotclock.base == start_time);
        assert(t.dotclock.ticks_prev == 0);
        assert(t.dotclock.ticks == 0);

        fprintf(stderr,"VGA_Draw_2: 1000Hz incremental\n");

        count = 0;
        t.dotclock.set_rate(1000,start_time);/*hz*/
        for (int i=0;i < 10000;i++) {
            t.dotclock.update(start_time + i);
            count += t.dotclock.delta_get();
            assert(t.dotclock.ticks == t.dotclock.ticks_prev);

            if (labs(count - (signed long long)i) > 1ll) { /* NTS: Expect possible +/- 1 error due to floating point */
                fprintf(stderr,"* TEST FAILURE:\n");
                fprintf(stderr,"   count=%lld ticks=%lld ticks_prev=%lld\n",count,(signed long long)i,(signed long long)t.dotclock.ticks);
                return 1;
            }

            signed long long rc = t.dotclock.ticks2pic(count);
            if (labs(rc - (signed long long)i) > 1ll) { /* NTS: Expect possible +/- 1 error due to floating point */
                fprintf(stderr,"* TEST FAILURE:\n");
                fprintf(stderr,"   count=%lld ticks=%lld ticks_prev=%lld rc=%lld\n",count,(signed long long)i,(signed long long)t.dotclock.ticks,rc);
                return 1;
            }
        }

        fprintf(stderr,"VGA_Draw_2: 1000Hz inc then 100Hz inc\n");

        count = 0;
        t.dotclock.set_rate(100,start_time + 1000);/*hz, rate change*/
        for (int i=0;i < 10000;i++) {
            t.dotclock.update(start_time + 1000 + (i * 10));/*1ms * 10 = 10ms = 100Hz */
            count += t.dotclock.delta_get();
            assert(t.dotclock.ticks == t.dotclock.ticks_prev);

            if (labs(count - (signed long long)i) > 1ll) { /* NTS: Expect possible +/- 1 error due to floating point */
                fprintf(stderr,"* TEST FAILURE:\n");
                fprintf(stderr,"   count=%lld ticks=%lld ticks_prev=%lld\n",count,(signed long long)i,(signed long long)t.dotclock.ticks);
                return 1;
            }

            signed long long rc = t.dotclock.ticks2pic(count);
            if (labs(rc - ((signed long long)(i * 10) + 1000 + start_time)) > 1ll) { /* NTS: Expect possible +/- 1 error due to floating point */
                fprintf(stderr,"* TEST FAILURE:\n");
                fprintf(stderr,"   count=%lld ticks=%lld ticks_prev=%lld rc=%lld\n",count,(signed long long)i,(signed long long)t.dotclock.ticks,rc);
                return 1;
            }
        }

        return 0;
    }
#endif

#if C_EMSCRIPTEN
    control->opt_debug = true;
    control->opt_earlydebug = true;
#endif

    bitop::self_test();
    ptrop::self_test();

    // initialize output libraries
    OUTPUT_SURFACE_Initialize();
#if C_OPENGL
    OUTPUT_OPENGL_Initialize();
#endif
#if C_DIRECT3D
    OUTPUT_DIRECT3D_Initialize();
#endif

    // initialize some defaults in SDL structure here
    sdl.srcAspect.x = aspect_ratio_x>0?aspect_ratio_x:4;
    sdl.srcAspect.y = aspect_ratio_y>0?aspect_ratio_y:3;
    sdl.srcAspect.xToY = (double)sdl.srcAspect.x / sdl.srcAspect.y;
    sdl.srcAspect.yToX = (double)sdl.srcAspect.y / sdl.srcAspect.x;

    std::string exepath = GetDOSBoxXPath(true);

#if defined(MACOSX)
    /* The resource system of DOSBox-X relies on being able to locate the Resources subdirectory
       within the DOSBox-X .app bundle. To do this, we have to first know where our own executable
       is, which macOS helpfully puts int argv[0] for us */
    /* NTS: Experimental testing shows that when we are run from the desktop (double-clicking on
            the .app bundle from the Finder) the current working directory is / (fs root). */
    extern std::string MacOSXEXEPath;
    extern std::string MacOSXResPath;
    MacOSXEXEPath = argv[0];

    /* The path should be something like /blah/blah/dosbox-x.app/Contents/MacOS/DosBox */
    /* If that's true, then we can move one level up the tree and look for */
    /* /blah/blah/dosbox-x.app/Contents/Resources */
    {
        const char *ref = argv[0];
        const char *s = strrchr(ref,'/');
        if (s != NULL) {
            if (s > ref) s--;
            while (s > ref && *s != '/') s--;
            if (!strncasecmp(s,"/MacOS/",7)) {
                MacOSXResPath = std::string(ref,(size_t)(s-ref)) + "/Resources";
            }
        }
        if (!MacOSXResPath.size() && exepath.size()) {
            char *path = (char*)malloc(exepath.size() + 1);
            if (path) {
                strcpy(path, exepath.c_str());
                const char *s = strrchr(path,'/');
                if (s != NULL) {
                    if (s > path) s--;
                    while (s > path && *s != '/') s--;
                    if (!strncasecmp(s,"/MacOS/",7)) {
                        MacOSXResPath = std::string(path,(size_t)(s-path)) + "/Resources";
                        struct stat st;
                        if (stat(MacOSXResPath.c_str(), &st)) MacOSXResPath = "";
                    }
                }
                free(path);
            }
        }
    }
#endif

    configfile = "";
    exepath = GetDOSBoxXPath();
    std::string workdiropt = "default";
    std::string workdirdef = "";
    struct stat st;
    if (!control->opt_defaultconf && control->config_file_list.empty() && stat("dosbox-x.conf", &st) && stat("dosbox.conf", &st)) {
        /* load the global config file first */
        std::string tmp,config_path,config_combined;

        /* -- Parse configuration files */
        Cross::GetPlatformConfigDir(config_path);
        Cross::GetPlatformConfigName(tmp);

        if (exepath.size()) {
            control->ParseConfigFile((exepath + "dosbox-x.conf").c_str());
            if (!control->configfiles.size()) control->ParseConfigFile((exepath + "dosbox.conf").c_str());
        }

        config_combined = config_path + "dosbox-x.conf";
        if (!control->configfiles.size() && stat(config_combined.c_str(),&st) == 0 && S_ISREG(st.st_mode))
            control->ParseConfigFile(config_combined.c_str());

        config_combined = config_path + tmp;
        if (!control->configfiles.size() && stat(config_combined.c_str(),&st) == 0 && S_ISREG(st.st_mode))
            control->ParseConfigFile(config_combined.c_str());

        if (control->configfiles.size()) configfile = control->configfiles.front();

        Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        workdiropt = section->Get_string("working directory option");
        workdirdef = section->Get_path("working directory default")->realpath;
        std::string resolvestr = section->Get_string("resolve config path");
        resolveopt = resolvestr=="true"||resolvestr=="1"?1:(resolvestr=="dosvar"?2:(resolvestr=="tilde"?3:0));
        void ResolvePath(std::string& in);
        ResolvePath(workdirdef);

        control->ClearExtraData();
        control->configfiles.clear();
    }

    if (workdiropt == "prompt" && control->opt_promptfolder < 0) control->opt_promptfolder = 1;
    else if (((workdiropt == "custom" && !control->opt_used_defaultdir) || workdiropt == "force") && workdirdef.size()) {
        if (chdir(workdirdef.c_str()) == -1) {
            LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'custom' or 'force'.");
        }
        control->opt_used_defaultdir = true;
        usecfgdir = false;
    } else if (workdiropt == "userconfig") {
        std::string config_path;
        Cross::GetPlatformConfigDir(config_path);
        if (config_path.size()) {
            if (chdir(config_path.c_str()) == -1) {
                LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'userconfig'.");
            }
        }
        control->opt_used_defaultdir = true;
        usecfgdir = false;
    } else if (workdiropt == "program") {
        std::string exepath = GetDOSBoxXPath();
        if (exepath.size()) {
            if (chdir(exepath.c_str()) == -1) {
                LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'program'.");
            }
        }
        control->opt_used_defaultdir = true;
        usecfgdir = false;
    } else if (workdiropt == "config") {
        control->opt_used_defaultdir = true;
        usecfgdir = true;
    }

    /* default do not prompt if -set, -conf, -userconf, -defaultconf, or -defaultdir is used */
    if (control->opt_promptfolder < 0 && (!control->config_file_list.empty() || !control->opt_set.empty() || control->opt_userconf || control->opt_defaultconf || control->opt_used_defaultdir || control->opt_fastlaunch || control->opt_test || workdiropt == "noprompt")) {
        control->opt_promptfolder = 0;
    }

    int workdirsave = 0;
    std::string workdirsaveas = "";
#if defined(MACOSX) || defined(LINUX) || (defined(WIN32) && !defined(HX_DOS))
    {
        char cwd[512] = {0};
        if(getcwd(cwd, sizeof(cwd) - 1) == NULL) {
            LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to get the current working directory.");
        }

        if(control->opt_promptfolder < 0) {
#if !defined(MACOSX)
            struct stat st;

            /* if dosbox.conf or dosbox-x.conf already exists in the current working directory, then skip folder prompt */
            if(stat("dosbox-x.conf", &st) == 0 || stat("dosbox.conf", &st) == 0) {
                if(S_ISREG(st.st_mode)) {
                    control->opt_promptfolder = 0;
                }
            }
#endif
            std::string res_path;
            Cross::GetPlatformResDir(res_path);
            if(stat((res_path + "dosbox-x.conf").c_str(), &st) == 0) {
                if(S_ISREG(st.st_mode)) {
                    control->opt_promptfolder = 0;
                }
            }
        }

#if defined(WIN32)
        /* A Windows application cannot detect with isatty() if run from the command prompt.
        *  isatty() returns true even though STDIN/STDOUT/STDERR do not exist even if run from the command prompt. */
        if (control->opt_promptfolder < 0)
            control->opt_promptfolder = 1;
#else
        if (control->opt_promptfolder < 0)
            control->opt_promptfolder = (!isatty(0) || !strcmp(cwd,"/")) ? 1 : 0;
#endif
        if (control->opt_promptfolder == 1 && workdiropt == "default" && workdirdef.size()) {
            control->opt_promptfolder = 0;
            if(chdir(workdirdef.c_str()) == -1) {
                LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'default'.");
            }
            control->opt_used_defaultdir = true;
            usecfgdir = false;
        }

        /* When we're run from the Finder, the current working directory is often / (the
           root filesystem) and there is no terminal. What to run, what directory to run
           it from, and the dosbox-x.conf to read, is not obvious. If run from the Finder,
           prompt the user where to run from, and then set it as the current working
           directory and continue. If they cancel, then exit. */
        /* Assume that if STDIN is not a TTY, or if the current working directory is "/",
           that we were started by the Finder */
        /* FIXME: Is there a better way to detect whether we were started by the Finder
                  or any other part of the macOS desktop? */
        if (control->opt_promptfolder > 0) {
#if defined(MACOSX)
            /* NTS: Do NOT call macosx_prompt_folder() to show a modal NSOpenPanel without
               first initializing the SDL video subsystem. SDL1 must initialize
               the Cocoa NS objects first, or else strange things happen, like
               DOSBox-X as an NSWindow with no apparent icon in the task manager,
               inability to change or maintain the menu at the top, etc. */
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
                E_Exit("Can't init SDL %s",SDL_GetError());
#endif

            char folder[512], *default_folder=folder;
            std::string dir = workdirdef.empty()?(exepath.size()?exepath:""):workdirdef;
            if (dir.size()&&dir.size()<512)
                strcpy(default_folder, dir.c_str());
            else
                default_folder = NULL;
            const char *confirmstr = "Do you want to use the selected folder as the DOSBox-X working directory in future sessions?\n\nIf you select Yes, DOSBox-X will not prompt for a folder again.\nIf you select No, DOSBox-X will always prompt for a folder when it runs.\nIf you select Cancel, DOSBox-X will ask this question again next time.";
            const char *quitstr = "You have not selected a valid path. Do you want to run DOSBox-X with the current path as the DOSBox-X working directory?\n\nDOSBox-X will exit if you select No.";
#if defined(MACOSX)
            std::string path = macosx_prompt_folder(default_folder);
            if (path.empty()) {
                if (!macosx_yesno("Run DOSBox-X?", quitstr)) {
                    fprintf(stderr,"No path chosen by user, exiting\n");
                    return 1;
                }
            } else if (workdiropt == "default") {
                int ans=macosx_yesnocancel("DOSBox-X working directory", confirmstr);
                if (ans == 1) {workdirsave=1;workdirsaveas=path;}
                else if (ans == 0) workdirsave=2;
            }
#elif defined(WIN32) && !defined(HX_DOS)
            std::wstring path = win32_prompt_folder(default_folder);
            if (path.empty()) {
                if (MessageBox(NULL, quitstr, "Run DOSBox-X?", MB_YESNO)==IDNO) {
                    fprintf(stderr, "No path chosen by user, exiting\n");
                    return 1;
                }
            } else if (workdiropt == "default") {
                int ans=MessageBox(NULL, confirmstr, "DOSBox-X working directory",  MB_YESNOCANCEL);
                const wchar_t *input = path.c_str();
                size_t size = (wcslen(input) + 1) * sizeof(wchar_t);
                char *buffer = new char[size];
                wcstombs(buffer, input, size);
                if (ans == IDYES) {workdirsave=1;workdirsaveas=buffer;}
                else if (ans == IDNO) workdirsave=2;
            }
#else
            char *cpath = tinyfd_selectFolderDialog("Select folder where to run emulation, which will become the DOSBox-X working directory:",default_folder);
            std::string path = (cpath != NULL) ? cpath : "";
            if (path.empty()) {
                if (!systemmessagebox("Run DOSBox-X?", quitstr, "yesno","question", 1)) {
                    fprintf(stderr,"No path chosen by user, exiting\n");
                    return 1;
                }
            } else if (workdiropt == "default") {
                confirmstr = "Do you want to use the selected folder as the DOSBox-X working directory in future sessions?\n\nIf you select Yes, DOSBox-X will not prompt for a folder again.\nIf you select No, DOSBox-X will always prompt for a folder when it runs.";
                int ans=systemmessagebox("DOSBox-X working directory",confirmstr,"yesno", "question", 1);
                if (ans == 1) {workdirsave=1;workdirsaveas=path;}
                else if (ans == 0) workdirsave=2;
            }
#endif

#if defined(MACOSX)
            /* Thank you, no longer needed */
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif

            if(!path.empty()) {
#if defined(WIN32) && !defined(HX_DOS)
                /* our stat override makes wstat impossible */
                if(_wchdir(path.c_str())) {
                    MessageBoxW(NULL, path.c_str(), L"Failed to change to the selected path.", MB_OK);
                    return 1;
                }
#else
                struct stat st;
                if (stat(path.c_str(),&st)) {
                    fprintf(stderr,"Unable to stat path '%s'\n",path.c_str());
                    return 1;
                }
                if (!S_ISDIR(st.st_mode)) {
                    fprintf(stderr,"Path '%s' is not S_ISDIR\n",path.c_str());
                    return 1;
                }
                if (chdir(path.c_str())) {
                    fprintf(stderr,"Failed to chdir() to path '%s', errno=%s\n",path.c_str(),strerror(errno));
                    return 1;
                }
#endif
                LOG_MSG("User selected folder '%s', making that the current working directory.\n",path.c_str());
                control->opt_used_defaultdir = true;
            }
        }
    }
#endif

    {
        std::string tmp,config_path,res_path,config_combined;

#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
        {
            DISPLAY_DEVICE dd;
            unsigned int i = 0;

            do {
                memset(&dd, 0, sizeof(dd));
                dd.cb = sizeof(dd);
                if (!EnumDisplayDevices(NULL, i, &dd, 0)) break;
                LOG_MSG("Win32 EnumDisplayDevices #%d: name=%s string=%s", i, dd.DeviceName, dd.DeviceString);
                i++;

                if (strstr(dd.DeviceString, "VirtualBox") != NULL)
                    isVirtualBox = true;
            } while (1);
        }
#endif
        if (workdirsave>0&&configfile.size()) {
            control->ParseConfigFile(configfile.c_str());
            if (control->configfiles.size()) {
                Section* tsec = control->GetSection("dosbox");
                if (workdirsave==1 && workdirsaveas.size()) {
                    //tsec->HandleInputline("working directory option=custom");
                    tsec->HandleInputline(("working directory default="+workdirsaveas).c_str());
                } else if (workdirsave==2)
                    tsec->HandleInputline("working directory option=autoprompt");
                if (control->PrintConfig(configfile.c_str(), static_cast<Section_prop *>(tsec)->Get_bool("show advanced options")?1:-1)) {
                    workdirsave=0;
                    LOG_MSG("Saved the DOSBox-X working directory to %s", configfile.c_str());
                }
                control->ClearExtraData();
                control->configfiles.clear();
            }
        }

        /* -- Parse configuration files */
        Cross::GetPlatformConfigDir(config_path);
        Cross::GetPlatformResDir(res_path);

        /* -- -- first the user config file */
        if (control->opt_userconf || workdirsave>0) {
            tmp.clear();
            Cross::GetPlatformConfigDir(config_path);
            Cross::GetPlatformConfigName(tmp);
            config_combined = config_path + tmp;

            LOG(LOG_MISC,LOG_DEBUG)("Loading config file according to -userconf from %s",config_combined.c_str());
            control->ParseConfigFile(config_combined.c_str());
            if (!control->configfiles.size()) {
                if (workdirsave>0) {
                    Section* tsec = control->GetSection("dosbox");
                    if (workdirsave==1 && workdirsaveas.size()) {
                        //tsec->HandleInputline("working directory option=custom");
                        tsec->HandleInputline(("working directory default="+workdirsaveas).c_str());
                    } else if (workdirsave==2)
                        tsec->HandleInputline("working directory option=autoprompt");
                }
                //Try to create the userlevel configfile.
                tmp.clear();
                Cross::CreatePlatformConfigDir(config_path);
                Cross::GetPlatformConfigName(tmp);
                config_combined = config_path + tmp;

                LOG(LOG_MISC,LOG_DEBUG)("Attempting to write config file according to -userconf, to %s",config_combined.c_str());
                if (control->PrintConfig(config_combined.c_str())) {
                    workdirsave = 0;
                    if (!control->opt_userconf) LOG_MSG("Saved the DOSBox-X working directory to %s", config_combined.c_str());
                    LOG(LOG_MISC,LOG_NORMAL)("Generating default configuration. Writing it to %s",config_combined.c_str());
                    //Load them as well. Makes relative paths much easier
                    control->ParseConfigFile(config_combined.c_str());
                }
                if (!control->opt_userconf) {control->ClearExtraData();control->configfiles.clear();}
            }
        }
        if (workdirsave>0) LOG_MSG("Unable to save the DOSBox-X working directory.");

        /* -- -- second the -conf switches from the command line */
        for (size_t si=0;si < control->config_file_list.size();si++) {
            std::string &cfg = control->config_file_list[si];
#if defined(WIN32) && defined(C_SDL2)
            std::string localname = cfg;
            if (!FileDirExistCP(cfg.c_str()) && FileDirExistUTF8(localname, cfg.c_str())) cfg = localname;
#endif
            if (!control->ParseConfigFile(cfg.c_str())) {
                // try to load it from the user directory
                if (!control->ParseConfigFile((config_path + cfg).c_str())) {
                    LOG_MSG("CONFIG: Can't open specified config file: %s",cfg.c_str());
                } else if (control->opt_eraseconf) {
                    LOG_MSG("Erase config file: %s\n", (config_path + cfg).c_str());
                    unlink((config_path + cfg).c_str());
                }
            } else if (control->opt_eraseconf) {
                LOG_MSG("Erase config file: %s\n", cfg.c_str());
                unlink(cfg.c_str());
            }
        }

    if (!control->opt_defaultconf) {
        /* -- -- if none found, use dosbox-x.conf or dosbox.conf */
        if (!control->configfiles.size()) control->ParseConfigFile("dosbox-x.conf");
        if (!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");
        if (!control->configfiles.size()) {
            std::string exepath=GetDOSBoxXPath();
            if (exepath.size()) {
                control->ParseConfigFile((exepath + "dosbox-x.conf").c_str());
                if (!control->configfiles.size()) control->ParseConfigFile((exepath + "dosbox.conf").c_str());
            }
        }

        /* -- -- if none found, use resource level conf */
        if (!control->configfiles.size()) control->ParseConfigFile((res_path + "dosbox-x.conf").c_str());

        /* -- -- if none found, use userlevel conf */
        if (!control->configfiles.size()) control->ParseConfigFile((config_path + "dosbox-x.conf").c_str());
        if (!control->configfiles.size()) {
            tmp.clear();
            Cross::GetPlatformConfigName(tmp);
            control->ParseConfigFile((config_path + tmp).c_str());
        }

        if (control->configfiles.size()) {
            if (control->opt_eraseconf&&control->config_file_list.empty()) {
                LOG_MSG("Erase config file: %s\n", control->configfiles.front().c_str());
                unlink(control->configfiles.front().c_str());
            }
            if (usecfgdir) {
                std::string configpath=control->configfiles.front();
                size_t found=configpath.find_last_of("/\\");
                if(found != string::npos) {
                    if(chdir(configpath.substr(0, found + 1).c_str()) == -1) {
                        LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for .conf file.");
                    }
                }
            }
        }

		// Redirect existing PC-98 related settings from other sections to the [pc98] section if the latter is empty
		Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
		assert(pc98_section != NULL);
		const char * extra = const_cast<char*>(pc98_section->data.c_str());
		if (!extra||!strlen(extra)) {
			char linestr[CROSS_LEN+1], *p;
			Section_prop * section = static_cast<Section_prop *>(control->GetSection("dosbox"));
			extra = const_cast<char*>(section->data.c_str());
			if (extra&&strlen(extra)) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (strncasecmp(line.c_str(), "pc-98 ", 6)) continue;
					if (line.length()>CROSS_LEN) {
						strncpy(linestr, line.c_str(), CROSS_LEN);
						linestr[CROSS_LEN]=0;
					} else
						strcpy(linestr, line.c_str());
					p=strchr(linestr, '=');
					if (p!=NULL&&pc98_section->HandleInputline(line)) {
						*p=0;
						LOG_MSG("Redirected \"%s\" from [dosbox] to [pc98] section\n", trim(linestr));
					}
				}
			}
			section = static_cast<Section_prop *>(control->GetSection("dos"));
			extra = const_cast<char*>(section->data.c_str());
			if (extra&&strlen(extra)) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (strncasecmp(line.c_str(), "pc-98 ", 6)) continue;
					if (line.length()>CROSS_LEN) {
						strncpy(linestr, line.c_str(), CROSS_LEN);
						linestr[CROSS_LEN]=0;
					} else
						strcpy(linestr, line.c_str());
					p=strchr(linestr, '=');
					if (p!=NULL&&pc98_section->HandleInputline(line)) {
						*p=0;
						LOG_MSG("Redirected \"%s\" from [dos] to [pc98] section\n", trim(linestr));
					}
				}
			}
		}

		// Redirect existing TTF related settings from [render] section to the [ttf] section if the latter is empty
		Section_prop * ttf_section = static_cast<Section_prop *>(control->GetSection("ttf"));
		assert(ttf_section != NULL);
		extra = const_cast<char*>(ttf_section->data.c_str());
		if (!extra||!strlen(extra)) {
			char linestr[CROSS_LEN+1], *p;
			Section_prop * section = static_cast<Section_prop *>(control->GetSection("render"));
			extra = const_cast<char*>(section->data.c_str());
			if (extra&&strlen(extra)) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (strncasecmp(line.c_str(), "ttf.", 4)) continue;
					if (line.length()>CROSS_LEN) {
						strncpy(linestr, line.substr(4).c_str(), CROSS_LEN);
						linestr[CROSS_LEN]=0;
					} else
						strcpy(linestr, line.substr(4).c_str());
					p=strchr(linestr, '=');
					if (p!=NULL&&ttf_section->HandleInputline(line.substr(4))) {
						*p=0;
						LOG_MSG("Redirected \"%s\" (\"ttf.%s\") from [render] to [ttf] section\n", trim(linestr), trim(linestr));
					}
				}
			}
		}

		// Redirect existing video related settings from [dosbox] section to the [video] section if the latter is empty
		Section_prop * video_section = static_cast<Section_prop *>(control->GetSection("video"));
		assert(video_section != NULL);
		extra = const_cast<char*>(video_section->data.c_str());
		if (!extra||!strlen(extra)) {
			char linestr[CROSS_LEN+1], *p;
			Section_prop * section = static_cast<Section_prop *>(control->GetSection("dosbox"));
			extra = const_cast<char*>(section->data.c_str());
			if (extra&&strlen(extra)) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (!strstr(line.c_str(), "vmem")&&!strstr(line.c_str(), "vga")&&!strstr(line.c_str(), "video")&&!strstr(line.c_str(), "dac")&&!strstr(line.c_str(), "cga")&&!strstr(line.c_str(), "CGA")&&!strstr(line.c_str(), "vesa")&&!strstr(line.c_str(), "hpel")&&!strstr(line.c_str(), "hretrace")&&!strstr(line.c_str(), "debug line")&&!strstr(line.c_str(), "memory bit")&&!strstr(line.c_str(), "forcerate")&&!strstr(line.c_str(), "double-buffered")&&!strstr(line.c_str(), "vblank")&&!strstr(line.c_str(), "setmode")) continue;
					if (line.length()>CROSS_LEN) {
						strncpy(linestr, line.c_str(), CROSS_LEN);
						linestr[CROSS_LEN]=0;
					} else
						strcpy(linestr, line.c_str());
					p=strchr(linestr, '=');
					if (p!=NULL&&video_section->HandleInputline(line)) {
						*p=0;
						LOG_MSG("Redirected \"%s\" from [dosbox] to [video] section\n", trim(linestr));
					}
				}
			}
		}

		// Redirect existing files= setting from [dos] section to the [config] section if the latter is empty
		Section_prop * config_section = static_cast<Section_prop *>(control->GetSection("config"));
		assert(config_section != NULL);
		extra = const_cast<char*>(config_section->data.c_str());
		if (!extra||!strlen(extra)) {
			char linestr[CROSS_LEN+1], *p;
			Section_prop * section = static_cast<Section_prop *>(control->GetSection("dos"));
			extra = const_cast<char*>(section->data.c_str());
			if (extra&&strlen(extra)) {
				std::istringstream in(extra);
				if (in)	for (std::string line; std::getline(in, line); ) {
					if (strncasecmp(line.c_str(), "files", 5)&&strncasecmp(line.c_str(), "dos in hma", 10)) continue;
					if (line.length()>CROSS_LEN) {
						strncpy(linestr, line.c_str(), CROSS_LEN);
						linestr[CROSS_LEN]=0;
					} else
						strcpy(linestr, line.c_str());
					p=strchr(linestr, '=');
					if (p!=NULL) {
						if (!strncasecmp(line.c_str(), "dos in hma", 10)) {
							if (!strcasecmp(trim(p+1), "true")) line="dos=high";
							else if (!strcasecmp(trim(p+1), "false")) line="dos=low";
						}
						if (config_section->HandleInputline(line)) {
							*p=0;
							LOG_MSG("Redirected \"%s\" (\"%s\") from [dos] to [config] section\n", "dos", trim(linestr));
						}
					}
				}
			}
		}

		// Redirect NE2000 realnic settings to pcap section
		Section_prop * ne2000_section = static_cast<Section_prop *>(control->GetSection("ne2000"));
		Section_prop * pcap_section = static_cast<Section_prop *>(control->GetSection("ethernet, pcap"));
		assert(ne2000_section != NULL);
		assert(pcap_section != NULL);
		std::istringstream in(ne2000_section->data.c_str());
		if (in)	for (std::string line; std::getline(in, line); ) {
			size_t pcaptimeout_pos = line.find("pcaptimeout");
			size_t realnic_pos = line.find("realnic");
			size_t eq_pos = line.find("=");
			if (pcaptimeout_pos != std::string::npos &&
			    eq_pos != std::string::npos &&
			    pcaptimeout_pos < eq_pos) {
				pcap_section->HandleInputline("timeout=" + line.substr(eq_pos + 1, std::string::npos));
				LOG_MSG("Migrated pcaptimeout from [ne2000] to [ethernet, pcap] section");
			}
			if (realnic_pos != std::string::npos &&
			    eq_pos != std::string::npos &&
			    realnic_pos < eq_pos) {
				pcap_section->HandleInputline(line);
				LOG_MSG("Migrated realnic from [ne2000] to [ethernet, pcap] section");
				if (ne2000_section->Get_bool("ne2000")) {
					LOG_MSG("Set ne2000 backend to pcap during migration");
					Prop_string* backend_prop = static_cast<Prop_string*>(ne2000_section->Get_prop("backend"));
					assert(backend_prop != NULL);
					backend_prop->SetValue("pcap");
				}
			}
		}
    }

		MSG_Add("PROGRAM_CONFIG_PROPERTY_ERROR","No such section or property.\n");
		MSG_Add("PROGRAM_CONFIG_NO_PROPERTY","There is no property %s in section %s.\n");
		MSG_Add("PROGRAM_CONFIG_SET_SYNTAX","The syntax for -set option is incorrect.\n");
		for (auto it=control->opt_set.begin(); it!=control->opt_set.end();it++) { /* -set switches */
			// add rest of command
			std::string rest;
			std::vector<std::string> pvars;
			pvars.clear();
			pvars.push_back(trim((char *)(*it).c_str()));
			if (!strlen(pvars[0].c_str())) continue;
			if (pvars[0][0]=='%'||pvars[0][0]=='\0'||pvars[0][0]=='#'||pvars[0][0]=='\n') continue;

			Section* sec = control->GetSectionFromProperty(pvars[0].c_str());
			// attempt to split off the first word
			std::string::size_type spcpos = pvars[0].find_first_of(' ');
			if (spcpos>1&&pvars[0].c_str()[spcpos-1]==',')
				spcpos=pvars[0].find_first_of(' ', spcpos+1);

			std::string::size_type equpos = pvars[0].find_first_of('=');
            if (equpos != std::string::npos) {
                std::string p = pvars[0];
                p.erase(equpos);
                sec = control->GetSectionFromProperty(p.c_str());
            }

			if ((equpos != std::string::npos) && 
				((spcpos == std::string::npos) || (equpos < spcpos) || sec)) {
				// If we have a '=' possibly before a ' ' split on the =
				pvars.insert(pvars.begin()+1,pvars[0].substr(equpos+1));
				pvars[0].erase(equpos);
				// As we had a = the first thing must be a property now
				sec=control->GetSectionFromProperty(pvars[0].c_str());
				if (!sec&&pvars[0].size()>4&&!strcasecmp(pvars[0].substr(0, 4).c_str(), "ttf.")) {
					pvars[0].erase(0,4);
					sec = control->GetSectionFromProperty(pvars[0].c_str());
				}
				if (sec) pvars.insert(pvars.begin(),std::string(sec->GetName()));
				else {
					LOG_MSG("%s", MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
					continue;
				}
				// order in the vector should be ok now
			} else {
				if (equpos != std::string::npos && spcpos < equpos) {
					// ' ' before a possible '=', split on the ' '
					pvars.insert(pvars.begin()+1,pvars[0].substr(spcpos+1));
					pvars[0].erase(spcpos);
				}
				// check if the first parameter is a section or property
				sec = control->GetSection(pvars[0].c_str());
				if (!sec) {
					// not a section: little duplicate from above
					sec=control->GetSectionFromProperty(pvars[0].c_str());
					if (sec) pvars.insert(pvars.begin(),std::string(sec->GetName()));
					else {
						LOG_MSG("%s", MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"));
						continue;
					}
				} else {
					// first of pvars is most likely a section, but could still be gus
					// have a look at the second parameter
					if (pvars.size() < 2) {
						LOG_MSG("%s", MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
						continue;
					}
					std::string::size_type equpos2 = pvars[1].find_first_of('=');
					if (equpos2 != std::string::npos) {
						// split on the =
						pvars.insert(pvars.begin()+2,pvars[1].substr(equpos2+1));
						pvars[1].erase(equpos2);
					}
					// is this a property?
					Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
					if (!sec2) {
						// not a property, 
						Section* sec3 = control->GetSectionFromProperty(pvars[0].c_str());
						if (sec3 && !(equpos != std::string::npos && spcpos != std::string::npos && equpos > spcpos)) {
							// section and property name are identical
							pvars.insert(pvars.begin(),pvars[0]);
						} // else has been checked above already
					}
				}
			}
			if(pvars.size() < 3) {
				LOG_MSG("%s", MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				continue;
			}
			// check if the property actually exists in the section
			Section* sec2 = control->GetSectionFromProperty(pvars[1].c_str());
			if (!sec2) {
				LOG_MSG(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
					pvars[1].c_str(),pvars[0].c_str());
				continue;
			}
			// Input has been parsed (pvar[0]=section, [1]=property, [2]=value)
			// now execute
			Section* tsec = control->GetSection(pvars[0]);
			std::string value(pvars[2]);
			//Due to parsing there can be a = at the start of value.
			while (value.size() && (value.at(0) ==' ' ||value.at(0) =='=') ) value.erase(0,1);
			for(Bitu i = 3; i < pvars.size(); i++) value += (std::string(" ") + pvars[i]);
			std::string inputline = pvars[1] + "=" + value;

			bool change_success = tsec->HandleInputline(inputline.c_str());
			if (!change_success&&!value.empty()) LOG_MSG("Cannot set \"%s\"\n", inputline.c_str());
		}

    {
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        workdiropt = section->Get_string("working directory option");
        workdirdef = section->Get_path("working directory default")->realpath;
        std::string resolvestr = section->Get_string("resolve config path");
        resolveopt = resolvestr=="true"||resolvestr=="1"?1:(resolvestr=="dosvar"?2:(resolvestr=="tilde"?3:0));
        void ResolvePath(std::string& in);
        ResolvePath(workdirdef);
        if (((workdiropt == "custom" && !control->opt_used_defaultdir) || workdiropt == "force") && workdirdef.size()) {
            if(chdir(workdirdef.c_str()) == -1) {
                LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'custom' or 'force'.");
            }
        } else if (workdiropt == "userconfig") {
            std::string config_path;
            Cross::GetPlatformConfigDir(config_path);
            if(config_path.size()) {
                if(chdir(config_path.c_str()) == -1) {
                    LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'userconfig'.");
                }
            }
        } else if (workdiropt == "program" && exepath.size()) {
            if(chdir(exepath.c_str()) == -1) {
                LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'program'.");
            }
        } else if (workdiropt == "config" && control->configfiles.size()) {
            std::string configpath=control->configfiles.front();
            size_t found=configpath.find_last_of("/\\");
            if(found != string::npos) {
                if(chdir(configpath.substr(0, found + 1).c_str()) == -1) {
                    LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to change directories for workdiropt 'config'.");
                }
            }
        }

        char cwd[512] = {0};
        if(getcwd(cwd, sizeof(cwd) - 1))
            LOG_MSG("DOSBox-X's working directory: %s\n", cwd);
        else
            LOG(LOG_GUI, LOG_ERROR)("sdlmain.cpp main() failed to get the current working directory.");

    const char *imestr = section->Get_string("ime");
    enableime = !strcasecmp(imestr, "true") || !strcasecmp(imestr, "1");
    if (!strcasecmp(imestr, "auto")) {
        const char *machine = section->Get_string("machine");
        if (!strcasecmp(machine, "pc98") || !strcasecmp(machine, "pc9801") || !strcasecmp(machine, "pc9821") || !strcasecmp(machine, "jega") || strcasecmp(static_cast<Section_prop *>(control->GetSection("dosv"))->Get_string("dosv"), "off")) enableime = true;
        else {
            force_conversion = true;
            int cp=dos.loaded_codepage;
            if (InitCodePage() && isDBCSCP()) enableime = true;
            else if (control->opt_langcp) tonoime = true;
            force_conversion = false;
            dos.loaded_codepage=cp;
#if defined (WIN32)
            if (!enableime&&!tonoime) {
                const Section_prop* section = static_cast<Section_prop*>(control->GetSection("dos"));
                const char * layoutname=section->Get_string("keyboardlayout");
                WORD cur_kb_layout = LOWORD(GetKeyboardLayout(0));
                if (!strcmp(layoutname, "jp") || !strcmp(layoutname, "ko") || !strcmp(layoutname, "cn") || !strcmp(layoutname, "tw") || !strcmp(layoutname, "hk") || !strcmp(layoutname, "zh") || !strcmp(layoutname, "zhs") || !strcmp(layoutname, "zht") || (!strcmp(layoutname, "auto") && (cur_kb_layout == 1028 || cur_kb_layout == 1041 || cur_kb_layout == 1042 || cur_kb_layout == 2052 || cur_kb_layout == 3076))) enableime = true;
            }
#endif
        }
    }
#if defined(WIN32) && !defined(HX_DOS)
        if (!enableime&&!tonoime) ImmDisableIME((DWORD)(-1));
#endif
    }

#if (ENVIRON_LINKED)
        /* -- parse environment block (why?) */
        control->ParseEnv(environ);
#endif

        /* -- initialize logging first, so that higher level inits can report problems to the log file */
        LOG::Init();

#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
        {
            DISPLAY_DEVICE dd;
            unsigned int i = 0;

            do {
                memset(&dd, 0, sizeof(dd));
                dd.cb = sizeof(dd);
                if (!EnumDisplayDevices(NULL, i, &dd, 0)) break;
                LOG_MSG("Win32 EnumDisplayDevices #%d: name=%s string=%s", i, dd.DeviceName, dd.DeviceString);
                i++;
            } while (1);
        }

        if (isVirtualBox) LOG_MSG("Win32 VirtualBox graphics adapter detected");
#endif

        /* -- Welcome to DOSBox-X! */
        LOG_MSG("DOSBox-X version %s ("
#if defined(WIN32)
                "Windows"
#elif defined(HX_DOS)
                "DOS"
#elif defined(LINUX)
                "Linux"
#elif defined(MACOSX)
                "macOS"
#else
                ""
#endif
        " %s)",VERSION,SDL_STRING);
        LOG(LOG_MISC,LOG_NORMAL)("Copyright 2011-%s The DOSBox-X Team. Project maintainer: joncampbell123 (The Great Codeholio). DOSBox-X published under GNU GPL.",std::string(COPYRIGHT_END_YEAR).c_str());

#if defined(MACOSX)
        LOG_MSG("macOS EXE path: %s",MacOSXEXEPath.c_str());
        LOG_MSG("macOS Resource path: %s",MacOSXResPath.c_str());
#endif

        /* -- [debug] setup console */
#if C_DEBUG
# if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
        /* Can't disable the console with debugger enabled */
        if (control->opt_noconsole) {
            LOG(LOG_MISC,LOG_DEBUG)("-noconsole: hiding Win32 console window");
            ShowWindow(GetConsoleWindow(), SW_HIDE);
            DestroyWindow(GetConsoleWindow());
        }
# endif
#endif

#if defined(WIN32)
        /* -- Windows: set console control handler */
        SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

#if !defined(C_SDL2)
        {
            int id, major, minor;

            DOSBox_CheckOS(id, major, minor);
            //if (id == 1) menu.compatible=true; //?

            /* use all variables to shut up the compiler about unused vars */
            LOG(LOG_MISC,LOG_DEBUG)("DOSBox-X CheckOS results: id=%u major=%u minor=%u",id,major,minor);
        }
#endif

        /* -- SDL init hackery */
#if SDL_VERSION_ATLEAST(1, 2, 14)
        /* hack: On debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
         * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version of libsdl */
        putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
        LOG(LOG_GUI,LOG_DEBUG)("SDL 1.2.14 hack: SDL_DISABLE_LOCK_KEYS=1");
#endif

        std::string videodriver = static_cast<Section_prop *>(control->GetSection("sdl"))->Get_string("videodriver");
        if (videodriver.size()) {
            videodriver = "SDL_VIDEODRIVER="+videodriver;
            putenv((char *)videodriver.c_str());
        }

#ifdef WIN32
        /* hack: Encourage SDL to use windib if not otherwise specified */
        if (getenv("SDL_VIDEODRIVER") == NULL) {
#if defined(C_SDL2)
            LOG(LOG_GUI, LOG_DEBUG)("Win32 hack: setting SDL_VIDEODRIVER=windows because environ variable is not set");
            putenv("SDL_VIDEODRIVER=windows");
#else
            LOG(LOG_GUI,LOG_DEBUG)("Win32 hack: setting SDL_VIDEODRIVER=windib because environ variable is not set");
            putenv("SDL_VIDEODRIVER=windib");
#endif
            sdl.using_windib=true;
        }
#endif

#if defined(WIN32) && defined(C_SDL2)
        /* HACK: WASAPI output on Windows 10 isn't working... */
        if (getenv("SDL_AUDIODRIVER") == NULL) {
            LOG(LOG_GUI, LOG_DEBUG)("Win32: using directsound audio driver");
            putenv("SDL_AUDIODRIVER=directsound");
        }
        if (getenv("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4") == NULL)
            putenv("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4=1");
#endif

        sdl.init_ignore = true;

    {
        Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
        assert(section != NULL);

        // boot-time option whether or not to report ourself as "DPI aware" to Windows so the
        // DWM doesn't upscale our window for backwards compat.
		{
			std::string dpiw = section->Get_string("dpi aware");

			if (dpiw == "1" || dpiw == "true") {
				dpi_aware_enable = true;
			}
			else if (dpiw == "0" || dpiw == "false") {
				dpi_aware_enable = false;
			}
			else { /* auto */
#if defined(MACOSX)
				/* Try not to look extra tiny on Retina displays */
				dpi_aware_enable = false;
#elif defined(WIN32) && !defined(HX_DOS)
				dpi_aware_enable = true;
#if !defined(C_SDL2)
                if (!control->opt_fullscreen && !static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("fullscreen"))
#endif
                {
				UINT dpi=0;
				HMODULE __user32 = GetModuleHandle("USER32.DLL");
				if (__user32) {
					DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#ifndef DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE      ((DPI_AWARENESS_CONTEXT)-2)
#endif
					DPI_AWARENESS_CONTEXT (WINAPI *__SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT) = NULL;
					UINT (WINAPI *__GetDpiForSystem)() = NULL;
					__SetThreadDpiAwarenessContext = (DPI_AWARENESS_CONTEXT (WINAPI *)(DPI_AWARENESS_CONTEXT))GetProcAddress(__user32,"SetThreadDpiAwarenessContext");
					__GetDpiForSystem = (UINT (WINAPI *)())GetProcAddress(__user32,"GetDpiForSystem");
					if (__SetThreadDpiAwarenessContext && __GetDpiForSystem) {
						DPI_AWARENESS_CONTEXT previousDpiContext = __SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
						dpi=__GetDpiForSystem();
						if (previousDpiContext!=NULL) __SetThreadDpiAwarenessContext(previousDpiContext);
					}
				}
				if (dpi&&dpi/96>1) dpi_aware_enable = false;
                }
#else
				dpi_aware_enable = true;
#endif
			}
		}
	}

#ifdef WIN32
        /* Windows Vista/7/8/10 DPI awareness. If we don't tell Windows we're high DPI aware, the DWM will
         * upscale our window to emulate a 96 DPI display which on high res screen will make our UI look blurry.
         * But we obey the user if they don't want us to do that. */
        Windows_DPI_Awareness_Init();
#endif
#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    /* Our SDL1 in-tree library has a High DPI awareness function for macOS now */
        if (!control->opt_disable_dpi_awareness)
            sdl1_hax_macosx_highdpi_set_enable(dpi_aware_enable);
#endif

#ifdef MACOSX
        macosx_detect_nstouchbar();/*assigns to has_touch_bar_support*/
        if (has_touch_bar_support) {
            LOG_MSG("macOS: NSTouchBar support detected in system");
            macosx_init_touchbar();
        }

        extern void macosx_init_dock_menu(void);
        macosx_init_dock_menu();

        void qz_set_match_monitor_cb(void);
        qz_set_match_monitor_cb();
#endif

        /* -- SDL init */
        if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE) >= 0)
            sdl.inited = true;
        else
            E_Exit("Can't init SDL %s",SDL_GetError());
#if defined(C_SDL2)
        SDL_version sdl_version;
        SDL_GetVersion(&sdl_version);
        LOG_MSG("SDL: version %d.%d.%d, Video %s, Audio %s)",
            sdl_version.major, sdl_version.minor, sdl_version.patch,
            SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());
#endif //defined (C_SDL2)
#if defined(__MINGW32__) && defined(C_DEBUG)
        LOG_MSG("EXPERIMENTAL: Debugger enabled for MinGW build, DOSBox-X crashes depending on the terminal software you use. Launching from command prompt (cmd.exe) is recommended.");
#endif
        /* -- -- decide whether to show menu in GUI */
        if (control->opt_nogui || menu.compatible)
            menu.gui=false;

        /* -- -- helpful advice */
        LOG(LOG_GUI,LOG_NORMAL)("Press Ctrl-F10 to capture/release mouse, Alt-F10 for configuration.");

        /* -- -- other steps to prepare SDL window/output */
        SDL_Prepare();

        /* -- NOW it is safe to send change events to SDL */
        {
            Section_prop *sdl_sec = static_cast<Section_prop*>(control->GetSection("sdl"));
            sdl_sec->onpropchange.push_back(&SDL_OnSectionPropChange);
        }

        /* -- -- Keyboard layout detection and setup */
        KeyboardLayoutDetect();
        SetMapperKeyboardLayout(host_keyboard_layout);

        /* -- -- Initialise Joystick and CD-ROM separately. This way we can warn when it fails instead of exiting the application */
        LOG(LOG_MISC,LOG_DEBUG)("Initializing SDL joystick subsystem...");
        glide.fullscreen = &sdl.desktop.fullscreen;
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) >= 0) {
            sdl.num_joysticks = (Bitu)SDL_NumJoysticks();
            LOG(LOG_MISC,LOG_DEBUG)("SDL reports %u joysticks",(unsigned int)sdl.num_joysticks);
        }
        else {
            LOG(LOG_GUI,LOG_WARN)("Failed to init joystick support");
            sdl.num_joysticks = 0;
        }

#if defined(C_SDL2)
        if (SDL_CDROMInit() < 0) {
#else
        if (SDL_InitSubSystem(SDL_INIT_CDROM) < 0) {
#endif
            LOG(LOG_GUI,LOG_WARN)("Failed to init CD-ROM support");
        }

        /* must redraw after modeset */
        sdl.must_redraw_all = true;
        sdl.deferred_resize = false;

        /* assume L+R ALT keys are up */
        sdl.laltstate = SDL_KEYUP;
        sdl.raltstate = SDL_KEYUP;
        sdl.lctrlstate = SDL_KEYUP;
        sdl.rctrlstate = SDL_KEYUP;
        sdl.lshiftstate = SDL_KEYUP;
        sdl.rshiftstate = SDL_KEYUP;

#if defined(WIN32) && defined(C_SDL2)
        char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
        if (sdl_videodrv != NULL && !strcmp(sdl_videodrv,"windows")) sdl.using_windib = true;
#elif defined(WIN32) && !defined(C_SDL2)
# if SDL_VERSION_ATLEAST(1, 2, 10)
        sdl.using_windib=true;
# else
        sdl.using_windib=false;
# endif

        if (getenv("SDL_VIDEODRIVER")==NULL) {
            putenv("SDL_VIDEODRIVER=windib");
            if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
            sdl.using_windib=true;
        } else {
            char* sdl_videodrv = getenv("SDL_VIDEODRIVER");

            LOG(LOG_MISC,LOG_DEBUG)("Win32: SDL_VIDEODRIVER is '%s', so I will obey it",sdl_videodrv);
            if (strcmp(sdl_videodrv,"directx")==0) sdl.using_windib = false;
            else if (strcmp(sdl_videodrv,"windib")==0) sdl.using_windib = true;
        }
#endif

        /* GUI init */
        GUI_StartUp();

        /* FIXME: We need a more general "init list", outside of the section-based design,
         *        that we then execute serially here. */
        /* TODO: Each section currently uses "AddDestroyFunction" per section. We need to
         *       change over that code to a global destroy callback list instead. */
        /* TODO: Get rid of "init" and "destroy" callback lists per section. */
        /* TODO: Add a global (within the Config object) init and destroy callback list.
         *       On each call, init functions are added to the end of the list, and
         *       destroy functions added to the beginning of the list. That way, init
         *       is lowest level to highest, destroy is highest level to lowest. */
        /* TODO: Config object should also have a "reset" callback list. On system
         *       reset each device would be notified so that it can emulate hardware
         *       reset (the RESET line on ISA/PCI bus), lowest level to highest. */
        /* TODO: Each "init" function should do the work of getting the section object,
         *       whatever section it wants to read, instead of us doing the work. When
         *       that's complete, the call to init should be without parameters (void).
         *       The hope is that the init functions can read whatever sections it wants,
         *       both newer DOSBox-X sections and existing DOSBox (mainline) compatible
         *       sections. */

        /* The order is important here:
         * Init functions are called low-level first to high level last,
         * because some init functions rely on others. */

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        AllocCallback1();
        
        if (control->opt_editconf.length() != 0)
            launcheditor(control->opt_editconf);
        if (control->opt_opencaptures.length() != 0)
            launchcaptures(control->opt_opencaptures);
        if (control->opt_opensaves.length() != 0)
            launchsaves(control->opt_opensaves);

        {
#if defined(WIN32) && !defined(C_SDL2)
            Section_prop *sec = static_cast<Section_prop *>(control->GetSection("dosbox"));
            enable_hook_special_keys = sec->Get_bool("keyboard hook");
#endif

            /* Some extra SDL Functions */
            Section_prop* sdl_sec = static_cast<Section_prop*>(control->GetSection("sdl"));

            if ((control->opt_fullscreen || sdl_sec->Get_bool("fullscreen")) && sdl.desktop.want_type != SCREEN_GAMELINK) {
                LOG(LOG_MISC, LOG_DEBUG)("Going fullscreen immediately, during startup");

#if defined(WIN32)
                DOSBox_SetSysMenu();
#endif
                //only switch if not already in fullscreen
                if (!sdl.desktop.fullscreen) GFX_SwitchFullScreen();

                /* Setup Mouse correctly if fullscreen */
                if(sdl.desktop.fullscreen&&sdl.mouse.autoenable) GFX_CaptureMouse();
            }

            // Shows menu bar (window)
            menu.startup = true;

            menu.showrt = control->opt_showrt||sdl_sec->Get_bool("showdetails");
            menu.hidecycles = (control->opt_showcycles||sdl_sec->Get_bool("showdetails") ? false : true);
        }

        /* Start up main machine */

        MAPPER_StartUp();
        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();

        /* at this point: If the machine type is PC-98, and the mapper keyboard layout was "Japanese",
         * then change the mapper layout to "Japanese PC-98" */
        if (host_keyboard_layout == DKM_JPN && IS_PC98_ARCH)
            SetMapperKeyboardLayout(DKM_JPN_PC98);

        RENDER_Init();
        CAPTURE_Init();
        IO_Init();
        HARDWARE_Init();
        Init_AddressLimitAndGateMask(); /* <- need to init address mask so Init_RAM knows the maximum amount of RAM possible */
        Init_MemoryAccessArray(); /* <- NTS: In DOSBox-X this is the "cache" of devices that responded to memory access */
        Init_A20_Gate(); // FIXME: Should be handled by motherboard!
        Init_PS2_Port_92h(); // FIXME: Should be handled by motherboard!
        Init_RAM();
        Init_DMA();
        Init_PIC();
        TIMER_Init();
        PCIBUS_Init();
        PAGING_Init(); /* <- NTS: At this time, must come before memory init because paging is so well integrated into emulation code */
        CMOS_Init();
        ROMBIOS_Init();
        CALLBACK_Init(); /* <- NTS: This relies on ROM BIOS allocation and it must happen AFTER ROMBIOS init */
#if C_DEBUG
        DEBUG_Init(); /* <- NTS: Relies on callback system */
#endif
        Init_VGABIOS();
        VOODOO_Init();
        GLIDE_Init();
        PROGRAMS_Init(); /* <- NTS: Does not init programs, it inits the callback used later when creating the .COM programs on drive Z: */
        PCSPEAKER_Init();
        TANDYSOUND_Init();
        MPU401_Init();
        MIXER_Init();
        MIDI_Init();
        {
            DOSBoxMenu::item *item;

            MAPPER_AddHandler(Sendkeymapper, MK_delete, MMODHOST, "sendkey_mapper", "Send special key", &item);
            item->set_text("Send special key");
        }
        CPU_Init();
#if C_FPU
        FPU_Init();
#endif
        VGA_Init();
        ISAPNP_Cfg_Init();
        FDC_Primary_Init();
        KEYBOARD_Init();
        SBLASTER_Init();
        JOYSTICK_Init();
        PS1SOUND_Init();
        DISNEY_Init();
        GUS_Init();
        IDE_Init();
        IMFC_Init();
        INNOVA_Init();
        BIOS_Init();
        INT10_Init();
        SERIAL_Init();
        DONGLE_Init();
#if C_PRINTER
        PRINTER_Init();
#endif
        PARALLEL_Init();
        NE2K_Init();

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        /* If PCjr emulation, map cartridge ROM */
        if (machine == MCH_PCJR)
            Init_PCJR_CartridgeROM();

        /* let's assume motherboards are sane on boot because A20 gate is ENABLED on first boot */
        MEM_A20_Enable(true);

#if C_DEBUG
        {
            DOSBoxMenu::item *item;
            /* Add some keyhandlers */
            MAPPER_AddHandler(DEBUG_Enable_Handler,
#if defined(MACOSX)
            // MacOS     NOTE: ALT-F12 to launch debugger. pause maps to F16 on macOS,
            // which is not easy to input on a modern mac laptop
            MK_f12
#else
            MK_pause
#endif
            ,MMOD2,"debugger","Show debugger",&item);
            item->set_text("Start DOSBox-X Debugger");

#if defined(MACOSX) || defined(LINUX)
            /* Mac OS X does not have a console for us to just allocate on a whim like Windows does.
               So the debugger interface is useless UNLESS the user has started us from a terminal
               (whether over SSH or from the Terminal app).

               Linux/X11 also does not have a console we can allocate on a whim. You either run
               this program from XTerm for the debugger, or not. */
            bool allow = true;

            if (!isatty(0) || !isatty(1) || !isatty(2))
                allow = false;

            mainMenu.get_item("mapper_debugger").enable(allow).refresh_item(mainMenu);
#endif
        }
#endif

        /* OS init now */
        DOS_Init();
        DRIVES_Init();
        DOS_KeyboardLayout_Init();
        MOUSE_Init(); // FIXME: inits INT 15h and INT 33h at the same time. Also uses DOS_GetMemory() which is why DOS_Init must come first
        XMS_Init();
        EMS_Init();
        AUTOEXEC_Init();
#if C_IPX
        IPX_Init();
#endif
        MSCDEX_Init();
        CDROM_Image_Init();

        /* Init memhandle system. This part is used by DOSBox-X's XMS/EMS emulation to associate handles
         * per page. FIXME: I would like to push this down to the point that it's never called until
         * XMS/EMS emulation needs it. I would also like the code to free the mhandle array immediately
         * upon booting into a guest OS, since memory handles no longer have meaning in the guest OS
         * memory layout. */
        Init_MemHandles();

        /* finally, the mapper */
        MAPPER_Init();
        AllocCallback2();
        MSG_Init();

        /* stop at this point, and show the configuration tool/mapper editor, if instructed */
        if (control->opt_startui) {
            LOG(LOG_MISC,LOG_DEBUG)("Running Configuration Tool, during startup, as instructed");
            int cp=dos.loaded_codepage;
            if (!cp) InitCodePage();
            GUI_Run(false);
            dos.loaded_codepage=cp;
        }
        if (control->opt_startmapper) {
            LOG(LOG_MISC,LOG_DEBUG)("Running Mapper Editor, during startup, as instructed");
            int cp=dos.loaded_codepage;
            if (!cp) InitCodePage();
            MAPPER_RunInternal();
            dos.loaded_codepage=cp;
        }

        char name[6]="slot0";
        if (!control->opt_silent)
        for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
            name[4]='0'+i;
            std::string command=SaveState::instance().getName(page*SaveState::SLOT_COUNT+i);
            std::string str=std::string(MSG_Get("SLOT"))+" "+to_string(page*SaveState::SLOT_COUNT+i+1)+(command==""?"":" "+command);
            mainMenu.get_item(name).set_text(str.c_str());
        }
        mainMenu.get_item("wheel_updown").check(wheel_key==1).refresh_item(mainMenu);
        mainMenu.get_item("wheel_leftright").check(wheel_key==2).refresh_item(mainMenu);
        mainMenu.get_item("wheel_pageupdown").check(wheel_key==3).refresh_item(mainMenu);
        mainMenu.get_item("wheel_ctrlupdown").check(wheel_key==4).refresh_item(mainMenu);
        mainMenu.get_item("wheel_ctrlleftright").check(wheel_key==5).refresh_item(mainMenu);
        mainMenu.get_item("wheel_ctrlpageupdown").check(wheel_key==6).refresh_item(mainMenu);
        mainMenu.get_item("wheel_ctrlwz").check(wheel_key==7).refresh_item(mainMenu);
        mainMenu.get_item("wheel_none").check(wheel_key==0).refresh_item(mainMenu);
        mainMenu.get_item("wheel_guest").check(wheel_guest).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_winlogo").check(sendkeymap==1).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_winmenu").check(sendkeymap==2).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_alttab").check(sendkeymap==3).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_ctrlesc").check(sendkeymap==4).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_ctrlbreak").check(sendkeymap==5).refresh_item(mainMenu);
        mainMenu.get_item("sendkey_mapper_cad").check(!sendkeymap).refresh_item(mainMenu);
        mainMenu.get_item("hostkey_ctrlalt").check(hostkeyalt==1).refresh_item(mainMenu);
        mainMenu.get_item("hostkey_ctrlshift").check(hostkeyalt==2).refresh_item(mainMenu);
        mainMenu.get_item("hostkey_altshift").check(hostkeyalt==3).refresh_item(mainMenu);
        std::string mapper_keybind = mapper_event_keybind_string("host");
        if (mapper_keybind.empty()) mapper_keybind = "unbound";
        std::string text=mainMenu.get_item("hostkey_mapper").get_text();
        std::size_t found = text.find(":");
        if (found!=std::string::npos) text = text.substr(0, found);
        mainMenu.get_item("hostkey_mapper").check(hostkeyalt==0).set_text(text+": "+mapper_keybind).refresh_item(mainMenu);

        bool MENU_get_swapstereo(void);
        mainMenu.get_item("mixer_swapstereo").check(MENU_get_swapstereo()).refresh_item(mainMenu);

        bool MENU_get_mute(void);
        mainMenu.get_item("mixer_mute").check(MENU_get_mute()).refresh_item(mainMenu);

        mainMenu.get_item("mapper_fscaler").check(render.scale.forced).refresh_item(mainMenu);
        mainMenu.get_item("3dfx_voodoo").check(strcasecmp(static_cast<Section_prop *>(control->GetSection("voodoo"))->Get_string("voodoo_card"), "false")).refresh_item(mainMenu);
        mainMenu.get_item("3dfx_glide").check(addovl).refresh_item(mainMenu);

        mainMenu.get_item("debug_logint21").check(log_int21).refresh_item(mainMenu);
        mainMenu.get_item("debug_logfileio").check(log_fileio).refresh_item(mainMenu);

        mainMenu.get_item("sync_host_datetime").enable(!IS_PC98_ARCH);
        mainMenu.get_item("vga_9widetext").enable(!IS_PC98_ARCH);
        mainMenu.get_item("doublescan").enable(!IS_PC98_ARCH);

        blinking=static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("high intensity blinking");
        mainMenu.get_item("text_background").enable(!IS_PC98_ARCH&&machine!=MCH_CGA).check(!blinking).refresh_item(mainMenu);
        mainMenu.get_item("text_blinking").enable(!IS_PC98_ARCH&&machine!=MCH_CGA).check(blinking).refresh_item(mainMenu);
        mainMenu.get_item("line_80x25").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x43").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x50").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x60").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x25").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x43").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x50").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x60").enable(!IS_PC98_ARCH);
#if defined(USE_TTF)
        mainMenu.get_item("mapper_aspratio").enable(!TTF_using());
        mainMenu.get_item("video_ratio_1_1").enable(!TTF_using());
        mainMenu.get_item("video_ratio_3_2").enable(!TTF_using());
        mainMenu.get_item("video_ratio_4_3").enable(!TTF_using());
        mainMenu.get_item("video_ratio_16_9").enable(!TTF_using());
        mainMenu.get_item("video_ratio_16_10").enable(!TTF_using());
        mainMenu.get_item("video_ratio_18_10").enable(!TTF_using());
        mainMenu.get_item("video_ratio_original").enable(!TTF_using());
        mainMenu.get_item("video_ratio_set").enable(!TTF_using());
        mainMenu.get_item("mapper_incsize").enable(TTF_using());
        mainMenu.get_item("mapper_decsize").enable(TTF_using());
        mainMenu.get_item("mapper_resetcolor").enable(TTF_using());
        mainMenu.get_item("ttf_showbold").enable(TTF_using()).check(showbold);
        mainMenu.get_item("ttf_showital").enable(TTF_using()).check(showital);
        mainMenu.get_item("ttf_showline").enable(TTF_using()).check(showline);
        mainMenu.get_item("ttf_showsout").enable(TTF_using()).check(showsout);
        mainMenu.get_item("ttf_wpno").enable(TTF_using()).check(!wpType);
        mainMenu.get_item("ttf_wpwp").enable(TTF_using()).check(wpType==1);
        mainMenu.get_item("ttf_wpws").enable(TTF_using()).check(wpType==2);
        mainMenu.get_item("ttf_wpxy").enable(TTF_using()).check(wpType==3);
        mainMenu.get_item("ttf_wpfe").enable(TTF_using()).check(wpType==4);
        mainMenu.get_item("ttf_blinkc").enable(TTF_using()).check(blinkCursor>-1);
        mainMenu.get_item("ttf_right_left").enable(TTF_using()).check(rtl);
#if C_PRINTER
        mainMenu.get_item("ttf_printfont").enable(TTF_using()).check(printfont);
#endif
        mainMenu.get_item("mapper_dbcssbcs").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(dbcs_sbcs||IS_PC98_ARCH||IS_JEGA_ARCH);
        mainMenu.get_item("mapper_autoboxdraw").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(autoboxdraw||IS_PC98_ARCH||IS_JEGA_ARCH);
        mainMenu.get_item("ttf_halfwidthkana").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(halfwidthkana||IS_PC98_ARCH||IS_JEGA_ARCH);
        refreshExtChar();
#endif
#if C_PRINTER
        mainMenu.get_item("mapper_printtext").enable(!IS_PC98_ARCH);
#endif
        mainMenu.get_item("pc98_5mhz_gdc").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_allow_200scanline").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_allow_4partitions").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_enable_egc").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_enable_grcg").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_enable_analog").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_enable_analog256").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_enable_188user").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_clear_text").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_clear_graphics").enable(IS_PC98_ARCH);
        mainMenu.get_item("pc98_use_uskb").enable(IS_PC98_ARCH);
        mainMenu.get_item("dos_pc98_pit_4mhz").enable(IS_PC98_ARCH);
        mainMenu.get_item("dos_pc98_pit_5mhz").enable(IS_PC98_ARCH);

        extern bool Mouse_Vertical;
        extern bool Mouse_Drv;

        mainMenu.get_item("dos_mouse_enable_int33").check(Mouse_Drv).refresh_item(mainMenu);
        mainMenu.get_item("dos_mouse_y_axis_reverse").check(Mouse_Vertical).refresh_item(mainMenu);

#ifdef C_OPENGL
        mainMenu.get_item("load_glsl_shader").enable(OpenGL_using()&&initgl==2);
#endif
#ifdef C_DIRECT3D
        mainMenu.get_item("load_d3d_shader").enable(Direct3D_using());
#endif
#ifdef USE_TTF
        mainMenu.get_item("load_ttf_font").enable(TTF_using());
#endif

#if !defined(C_EMSCRIPTEN)
        mainMenu.get_item("show_console").check(showconsole_init).refresh_item(mainMenu);
        mainMenu.get_item("clear_console").check(false).enable(showconsole_init).refresh_item(mainMenu);
        if (control->opt_display2) {
            mainMenu.get_item("show_console").enable(false).refresh_item(mainMenu);
            mainMenu.get_item("wait_on_error").enable(false).refresh_item(mainMenu);
        }
#endif
#if C_DEBUG
        if (control->opt_display2) mainMenu.get_item("mapper_debugger").enable(false).refresh_item(mainMenu);
#endif
        mainMenu.get_item("mapper_speedlock2").check(ticksLocked).refresh_item(mainMenu);

        OutputSettingMenuUpdate();
        aspect_ratio_menu();
        update_pc98_clock_pit_menu();
#if !defined(C_EMSCRIPTEN)
        update_capture_fmt_menu();
#endif

        /* The machine just "powered on", and then reset finished */
        if (!VM_PowerOn()) E_Exit("VM failed to power on");

#if (defined __i386__ || defined __x86_64__) && (defined BSD || defined LINUX)
        /*
          Drop root privileges after they are no longer needed, which is a good
          practice if the executable is setuid root.
          dropPrivileges() is called by PARPORTS::PARPORTS() after contructing
          CDirectLPT instances, but only if the constant HAS_CDIRECTLPT is
          non-zero. dropPrivileges() should be called regardless (if
          initPassthroughIO() is used anywhere else).
        */
        dropPrivileges(); // Ignore whether we could actually drop privileges.
#endif

        /* go! */
        sdl.init_ignore = false;
        UpdateWindowDimensions();
        userResizeWindowWidth = 0;
        userResizeWindowHeight = 0;

        UpdateOverscanMenu();

        void GUI_ResetResize(bool pressed);
        GUI_ResetResize(true);

        void ConstructMenu(void);
        ConstructMenu();

#if 0
        mainMenu.dump_log_debug(); /*DEBUG*/
#endif
        mainMenu.rebuild();

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
#if defined(LINUX)
        if (IS_PC98_ARCH || IS_JEGA_ARCH || isDBCSCP()) InitFontHandle();
#endif
        mainMenu.screenWidth = (unsigned int)sdl.surface->w;
        mainMenu.screenHeight = (unsigned int)sdl.surface->h;
        mainMenu.updateRect();
#endif
#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
        /* Windows 7 taskbar extension support */
        {
            HRESULT hr;

            hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_SERVER, IID_ITaskbarList3, (LPVOID*)(&winTaskbarList));
            if (hr == S_OK) {
                LOG_MSG("Windows: IID_ITaskbarList3 is available");

#if !defined(C_SDL2)
                THUMBBUTTON buttons[8];
                int buttoni = 0;

                {
                    THUMBBUTTON &b = buttons[buttoni++];
                    memset(&b, 0, sizeof(b));
                    b.iId = ID_WIN_SYSMENU_MAPPER;
                    b.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAPPER));
                    b.dwMask = (THUMBBUTTONMASK)(THB_TOOLTIP | THB_FLAGS | THB_ICON);
                    b.dwFlags = (THUMBBUTTONFLAGS)(THBF_ENABLED | THBF_DISMISSONCLICK);
                    wcscpy(b.szTip, L"Mapper editor");
                }

                {
                    THUMBBUTTON &b = buttons[buttoni++];
                    memset(&b, 0, sizeof(b));
                    b.iId = ID_WIN_SYSMENU_CFG_GUI;
                    b.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_CFG_GUI));
                    b.dwMask = (THUMBBUTTONMASK)(THB_TOOLTIP | THB_FLAGS | THB_ICON);
                    b.dwFlags = (THUMBBUTTONFLAGS)(THBF_ENABLED | THBF_DISMISSONCLICK);
                    wcscpy(b.szTip, L"Configuration tool");
                }

                {
                    THUMBBUTTON &b = buttons[buttoni++];
                    memset(&b, 0, sizeof(b));
                    b.iId = 1;
                    b.dwMask = (THUMBBUTTONMASK)THB_FLAGS;
                    b.dwFlags = (THUMBBUTTONFLAGS)(THBF_DISABLED | THBF_NONINTERACTIVE | THBF_NOBACKGROUND);
                }

                {
                    THUMBBUTTON &b = buttons[buttoni++];
                    memset(&b, 0, sizeof(b));
                    b.iId = ID_WIN_SYSMENU_PAUSE;
                    b.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PAUSE));
                    b.dwMask = (THUMBBUTTONMASK)(THB_TOOLTIP | THB_FLAGS | THB_ICON);
                    b.dwFlags = (THUMBBUTTONFLAGS)THBF_ENABLED;
                    wcscpy(b.szTip, L"Pause emulation");
                }

                winTaskbarList->ThumbBarAddButtons(GetHWND(), buttoni, buttons);
#endif
            }
        }
#endif
        {
            Section_prop *section = static_cast<Section_prop *>(control->GetSection("SDL"));
            assert(section != NULL);

            bool cfg_want_menu = section->Get_bool("showmenu");

            /* -- -- decide whether to set menu */
            if (menu_gui && !control->opt_nomenu && cfg_want_menu) {
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                if (!TTF_using() || !sdl.desktop.fullscreen)
#endif
                    DOSBox_SetMenu();
            }
            else
                DOSBox_NoMenu();

#if defined(WIN32) && !defined(C_SDL2)
            if (maximize && !TTF_using() && !GFX_IsFullscreen()) ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        bool reboot_dos;
        bool run_machine;
        bool wait_debugger;
        bool reboot_machine;
        bool dos_kernel_shutdown;

fresh_boot:
        reboot_dos = false;
        run_machine = false;
        wait_debugger = false;
        reboot_machine = false;
        dos_kernel_shutdown = false;
        guest_msdos_mcb_chain = (uint16_t)(~0u);

#if C_DEBUG
        if (control->opt_test) ::testing::InitGoogleTest(&argc, argv);
#endif

        /* NTS: CPU reset handler, and BIOS init, has the instruction pointer poised to run through BIOS initialization,
         *      which will then "boot" into the DOSBox-X kernel, and then the shell, by calling VM_Boot_DOSBox_Kernel() */
        /* FIXME: throwing int() is a stupid and nondescriptive way to signal shutdown/reset. */
        try {
#if C_DEBUG
            if (control->opt_break_start) DEBUG_EnableDebugger();
#endif
            DOSBOX_RunMachine();
        } catch (int x) {
            if (x == 2) { /* booting a guest OS. "boot" has already done the work to load the image and setup CPU registers */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to boot guest OS");

                run_machine = true; /* make note. don't run the whole shebang from an exception handler! */
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else if (x == 3) { /* reboot the system */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to reboot the system");

                reboot_machine = true;
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else if (x == 5) { /* go to PC-98 mode */
                E_Exit("Obsolete int signal");
            }
            else if (x == 6) { /* reboot DOS kernel */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to reboot DOS kernel");

                reboot_dos = true;
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else if (x == 7) { /* DOS kernel corruption error (need to restart the DOS kernel) */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to reboot DOS kernel");

                reboot_dos = true;
                wait_debugger = true;
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else if (x == 8) { /* Booting to a BIOS, shutting down DOSBox-X BIOS */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw a signal to boot into BIOS image");

                reboot_machine = true;
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else if (x == 9) { /* BIOS caught a JMP to F000:FFF0 without any other hardware reset signal */
                LOG(LOG_MISC,LOG_DEBUG)("Emulation detected JMP to BIOS POST routine");

                reboot_machine = true;
                dos_kernel_shutdown = !dos_kernel_disabled; /* only if DOS kernel enabled */
            }
            else {
                LOG(LOG_MISC,LOG_DEBUG)("Emulation threw DOSBox-X kill switch signal");

                // kill switch (see instances of throw(0) and throw(1) elsewhere in DOSBox)
                run_machine = false;
                dos_kernel_shutdown = false;
            }
        }
        catch (...) {
            throw;
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        checkmenuwidth = ctrlbrk = false;
        if (dos_kernel_shutdown) {
            tryconvertcp = 0;
            inshell = false;
            maincp = dos.loaded_codepage;
            if (!IS_PC98_ARCH&&!IS_JEGA_ARCH&&!IS_J3100&&dos.loaded_codepage!=437) dos.loaded_codepage=437;
            if (ticksLocked && !static_cast<Section_prop *>(control->GetSection("cpu"))->Get_bool("turbo")) DOSBOX_UnlockSpeed2(true);

            /* NTS: we take different paths depending on whether we're just shutting down DOS
             *      or doing a hard reboot. */

            if (wait_debugger) {
#if C_DEBUG
                Bitu DEBUG_EnableDebugger(void);
                void DEBUG_WaitNoExecute(void);

                LOG_MSG("Starting debugger.");
                DEBUG_EnableDebugger();
                DEBUG_WaitNoExecute();
#endif
            }

            /* new code: fire event */
            if (reboot_machine)
                DispatchVMEvent(VM_EVENT_DOS_EXIT_REBOOT_BEGIN);
            else
                DispatchVMEvent(VM_EVENT_DOS_EXIT_BEGIN);

            /* older shutdown code */
            RemoveEMSPageFrame();

            /* remove UMB block */
            if (!keep_umb_on_boot) RemoveUMBBlock();

            /* disable INT 33h mouse services. it can interfere with guest OS paging and control of the mouse */
            DisableINT33();

            /* unmap the DOSBox-X kernel private segment. if the user told us not to,
             * but the segment exists below 640KB, then we must, because the guest OS
             * will trample it and assume control of that region of RAM. */
            if (!keep_private_area_on_boot || reboot_machine)
                DOS_GetMemory_unmap();
            else if (DOS_PRIVATE_SEGMENT < 0xA000)
                DOS_GetMemory_unmap();

            /* revector some dos-allocated interrupts */
            if (!reboot_machine) {
                real_writed(0,0x01*4,(uint32_t)BIOS_DEFAULT_HANDLER_LOCATION);
                real_writed(0,0x03*4,(uint32_t)BIOS_DEFAULT_HANDLER_LOCATION);
            }

            if (Voodoo_OGL_Active())
                Voodoo_Output_Enable(false);

            grGlideShutdown();

            /* shutdown DOSBox-X's virtual drive Z */
            VFILE_Shutdown();
            /* shutdown the programs */
            PROGRAMS_Shutdown();        /* FIXME: Is this safe? Or will this cause use-after-free bug? */

            unsigned int cpbak = dos.loaded_codepage;
            dos.loaded_codepage = maincp;
            Add_VFiles(false);
            dos.loaded_codepage = cpbak;

            /* remove environment variables for some components */
            DOS_UninstallMisc();
            SBLASTER_DOS_Shutdown();
            GUS_DOS_Shutdown();
            /* disable Expanded Memory. EMM is a DOS API, not a BIOS API */
            EMS_DoShutDown();
            /* and XMS, also a DOS API */
            XMS_DoShutDown();
            /* and the DOS API in general */
            DOS_DoShutDown();

            /* mem handles too */
            ShutDownMemHandles(NULL);

            /* set the "disable DOS kernel" flag so other parts of this program
             * do not attempt to manipulate now-defunct parts of the kernel
             * such as the environment block */
            dos_kernel_disabled = true;

            std::string core(static_cast<Section_prop *>(control->GetSection("cpu"))->Get_string("core"));
            if (!strcmp(RunningProgram, "LOADLIN") && core == "auto") {
                cpudecoder=&CPU_Core_Normal_Run;
                mainMenu.get_item("mapper_normal").check(true).refresh_item(mainMenu);
#if (C_DYNAMIC_X86) || (C_DYNREC)
                mainMenu.get_item("mapper_dynamic").check(false).refresh_item(mainMenu);
#endif
            }

            /* new code: fire event */
            if (reboot_machine)
                DispatchVMEvent(VM_EVENT_DOS_EXIT_REBOOT_KERNEL);
            else
                DispatchVMEvent(VM_EVENT_DOS_EXIT_KERNEL);

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            Reflect_Menu();
#endif
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

#if defined(USE_TTF)
        if (ttfswitch || switch_output_from_ttf) {
            tooutttf = true;
            showdbcs = switchttf = ttfswitch = switch_output_from_ttf = false;
            mainMenu.get_item("output_ttf").enable(true).refresh_item(mainMenu);
        }
#endif

        if (run_machine) {
            bootguest = false;
            bool disable_a20 = static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_bool("turn off a20 gate on boot");

            /* if instructed, turn off A20 at boot */
            if (disable_a20) MEM_A20_Enable(false);

            /* PC-98: hide the cursor */
            if (IS_PC98_ARCH) {
                void PC98_show_cursor(bool show);
                PC98_show_cursor(false);
            }

            /* new code: fire event */
            DispatchVMEvent(VM_EVENT_GUEST_OS_BOOT);

            LOG_MSG("Alright: DOS kernel shutdown, booting a guest OS\n");
            LOG_MSG("  CS:IP=%04x:%04x SS:SP=%04x:%04x AX=%04x BX=%04x CX=%04x DX=%04x\n",
                SegValue(cs),reg_ip,
                SegValue(ss),reg_sp,
                reg_ax,reg_bx,reg_cx,reg_dx);

#if C_DEBUG
            if (boot_debug_break) {
                boot_debug_break = false;

                Bitu DEBUG_EnableDebugger(void);
                DEBUG_EnableDebugger();
            }
#endif

            /* run again */
            goto fresh_boot;
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        if (reboot_machine) {
            LOG_MSG("Rebooting the system\n");

            boothax = BOOTHAX_NONE;
            guest_msdos_LoL = 0;
            guest_msdos_mcb_chain = 0;

            void CPU_Snap_Back_Forget();
            /* Shutdown everything. For shutdown to work properly we must force CPU to real mode */
            CPU_Snap_Back_To_Real_Mode();
            CPU_Snap_Back_Forget();

            /* new code: fire event */
            DispatchVMEvent(VM_EVENT_RESET);

            /* force the mapper to let go of all keys so that the host key is not stuck (Issue #1320) */
            MAPPER_ReleaseAllKeys();
            void MAPPER_LosingFocus(void);
            MAPPER_LosingFocus();

            if (custom_bios) {
                /* need to relocate BIOS allocations */
                void ROMBIOS_InitForCustomBIOS(void);
                ROMBIOS_InitForCustomBIOS();

                void CALLBACK_Init();
                CALLBACK_Init();

#if C_DEBUG
                void DEBUG_ReinitCallback(void);
                DEBUG_ReinitCallback();
#endif
            }

            DispatchVMEvent(VM_EVENT_RESET_END);

            /* HACK: EGA/VGA modes will need VGA BIOS mapped in, ready to go */
            if (IS_EGAVGA_ARCH) {
                void INT10_Startup(Section *sec);
                INT10_Startup(NULL);
            }

#if C_DEBUG
            if (boot_debug_break) {
                boot_debug_break = false;

                Bitu DEBUG_EnableDebugger(void);
                DEBUG_EnableDebugger();
            }
#endif

            /* run again */
            goto fresh_boot;
        }
        else if (reboot_dos) { /* typically (at this time) to enter/exit PC-98 mode */
            LOG_MSG("Rebooting DOS\n");

            void CPU_Snap_Back_Forget();
            /* Shutdown everything. For shutdown to work properly we must force CPU to real mode */
            CPU_Snap_Back_To_Real_Mode();
            CPU_Snap_Back_Forget();

            /* all hardware devices need to know to reregister themselves PC-98 style */

            /* begin booting DOS again. */
            void BIOS_Enter_Boot_Phase(void);
            BIOS_Enter_Boot_Phase();

            /* run again */
            goto fresh_boot;
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        /* and then shutdown */
        GFX_ShutDown();

        void CPU_Snap_Back_Forget();
        /* Shutdown everything. For shutdown to work properly we must force CPU to real mode */
        CPU_Snap_Back_To_Real_Mode();
        CPU_Snap_Back_Forget();

        void CALLBACK_Dump(void);
        CALLBACK_Dump();

        /* GUI font registry shutdown */
#if !defined(C_SDL2)
        GUI::Font::registry_freeall();
#endif
        DOS_ShutdownDrives();
        DOS_ShutdownFiles();
        DOS_ShutdownDevices();
        CALLBACK_Shutdown();
#if C_DYNAMIC_X86
        CPU_Core_Dyn_X86_Shutdown();
#endif
        FreeBIOSDiskList();
        MAPPER_Shutdown();
        VFILE_Shutdown();
        PROGRAMS_Shutdown();
        TIMER_ShutdownTickHandlers();
#if C_DEBUG
        DEBUG_ShutDown(NULL);
#endif

        sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt

#if (defined(WIN32) && !defined(HX_DOS) || defined(LINUX) && C_X11 || defined(MACOSX)) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
        if (!control->opt_silent) {
                SDL_SetIMValues(SDL_IM_ONOFF, 0, NULL);
                SDL_SetIMValues(SDL_IM_ENABLE, 0, NULL);
        }
#elif (defined(WIN32) && !defined(HX_DOS) || defined(MACOSX)) && defined(C_SDL2)
        if (!control->opt_silent) {
                SDL_StopTextInput();
        }
#endif

        //Force visible mouse to end user. Somehow this sometimes doesn't happen
#if defined(C_SDL2)
        SDL_SetRelativeMouseMode(SDL_FALSE);
#else
        SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
        SDL_ShowCursor(SDL_ENABLE);

        /* Exit functions */
        if (machine != MCH_AMSTRAD) // FIXME
                while (!exitfunctions.empty()) {
                        Function_wrapper &ent = exitfunctions.front();

                        LOG(LOG_MISC,LOG_DEBUG)("Calling exit function (%p) '%s'",(void*)((uintptr_t)ent.function),ent.name.c_str());
                        ent.function(NULL);
                        exitfunctions.pop_front();
                }

        LOG::Exit();

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU && defined(WIN32) && !defined(HX_DOS) && (!defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL) || defined(C_SDL2))
        ShowWindow(GetHWND(), SW_HIDE);
        SDL1_hax_SetMenu(NULL);/* detach menu from window, or else Windows will destroy the menu out from under the C++ class */
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
        void sdl_hax_macosx_setmenu(void *nsMenu);
        sdl_hax_macosx_setmenu(NULL);
#endif
#if C_DEBUG
        DISP2_Shut();
#endif

#if defined(C_SDL2)
        SDL_CDROMQuit();
#endif
        SDL_Quit();//Let's hope sdl will quit as well when it catches an exception

#if defined(WIN32) && !defined(HX_DOS)
        if (winTaskbarList != NULL) {
                winTaskbarList->Release();
                winTaskbarList = NULL;
        }
#endif

        mainMenu.unbuild();
        mainMenu.clear_all_menu_items();

#if defined(LINUX) && defined(HAVE_ALSA)
        // force ALSA to release global cache, so that it's one less leak reported by Valgrind
        snd_config_update_free_global();
#endif

	/* the return statement below needs control->opt_test, save it before Config
	 * goes out of context and is deleted */
	saved_opt_test = control->opt_test;

        /* NTS: The "control" object destructor is called here because the "myconf" object leaves scope.
         * The destructor calls all section destroy functions here. After this point, all sections have
         * freed resources. */
        control = NULL; /* any deref past this point and you deserve to segfault */
    }

    return saved_opt_test&&testerr?1:0;
}

void GFX_GetSizeAndPos(int &x,int &y,int &width, int &height, bool &fullscreen) {
    x = sdl.clip.x;
    y = sdl.clip.y;
    width = sdl.clip.w; // draw.width
    height = sdl.clip.h; // draw.height
    fullscreen = sdl.desktop.fullscreen;
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
    width = sdl.clip.w; // draw.width
    height = sdl.clip.h; // draw.height
    fullscreen = sdl.desktop.fullscreen;
}

void GFX_ShutDown(void) {
    LOG(LOG_MISC,LOG_DEBUG)("Shutting down GFX renderer");
    GFX_Stop();
    if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
    if (sdl.mouse.locked) GFX_CaptureMouse();
    if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}

bool Direct3D_using(void) {
#if C_DIRECT3D
    return (sdl.desktop.want_type==SCREEN_DIRECT3D?true:false);
#else
    return false;
#endif
}

bool OpenGL_using(void) {
#if C_OPENGL
    return (sdl.desktop.want_type==SCREEN_OPENGL?true:false);
#else
    return false;
#endif
}

bool TTF_using(void) {
#if defined(USE_TTF)
    return (sdl.desktop.want_type==SCREEN_TTF?true:false);
#else
    return false;
#endif
}

bool Get_Custom_SaveDir(std::string& savedir) {
    if (custom_savedir.length() != 0) {
        savedir=custom_savedir;
        return true;
    }
    return false;
}

void GUI_ResetResize(bool pressed) {
#if defined(USE_TTF)
    if (TTF_using()) {
        resetFontSize();
        return;
    }
#endif

    void RENDER_CallBack( GFX_CallBackFunctions_t function );

    if (!pressed) return;
    userResizeWindowWidth = 0;
    userResizeWindowHeight = 0;

    if (GFX_GetPreventFullscreen())
        return;

    if (sdl.updating && !GFX_MustActOnResize()) {
        /* act on resize when updating is complete */
        sdl.deferred_resize = true;
    }
    else {
        sdl.deferred_resize = false;
        RENDER_CallBack(GFX_CallBackReset);
    }
}

bool MOUSE_IsLocked()
{
    return sdl.mouse.locked;
}

#if defined(C_SDL2) && defined(C_OPENGL)/*HACK*/
void SDL_GL_SwapBuffers(void) {
    SDL_GL_SwapWindow(sdl.window);
}
#endif

// save state support
void POD_Save_Sdlmain( std::ostream& stream )
{
	// - pure data
	WRITE_POD( &sdl.mouse.autolock, sdl.mouse.autolock );
	WRITE_POD( &sdl.mouse.requestlock, sdl.mouse.requestlock );
}

void POD_Load_Sdlmain( std::istream& stream )
{
	// - pure data
	READ_POD( &sdl.mouse.autolock, sdl.mouse.autolock );
	READ_POD( &sdl.mouse.requestlock, sdl.mouse.requestlock );
}
