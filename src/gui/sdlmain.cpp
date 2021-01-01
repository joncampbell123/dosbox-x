/** \mainpage DOSBox-X emulation
 *
 * \section i Introduction
 *
 * \section f Features
 *
 * \li Accurate x86 emulation
 *
*/

/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

int socknum=-1;
int posx = -1;
int posy = -1;
int initgl = 0;
int switchoutput=-1;
int selsrow = -1, selscol = -1;
int selerow = -1, selecol = -1;
bool selmark = false;
extern int enablelfn;
extern bool blinking;
extern bool dpi_aware_enable;
extern bool log_int21;
extern bool log_fileio;
extern bool noremark_save_state;
extern bool force_load_state;
extern bool use_quick_reboot;
extern bool pc98_force_ibm_layout;
extern bool enable_config_as_shell_commands;
bool winrun=false, use_save_file=false;
bool direct_mouse_clipboard=false;
bool mountfro[26], mountiro[26];

bool OpenGL_using(void), Direct3D_using(void);
void GFX_OpenGLRedrawScreen(void);

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
#ifdef WIN32
# include <signal.h>
# include <sys/stat.h>
# include <process.h>
# if !defined(__MINGW32__) /* MinGW does not have these headers */
#  include <shcore.h>
#  include <shellscalingapi.h>
# endif
#endif

#include "dosbox.h"
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
#include "shell.h"
#include "glidedef.h"
#include "inout.h"
#include "../dos/cdrom.h"
#include "../ints/int10.h"
#if !defined(HX_DOS)
#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include "whereami.c"
#endif
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#endif
#if defined(USE_TTF)
#include "sdl_ttf.c"
#include "DOSBoxTTF.h"
#endif

#if defined(LINUX) && defined(HAVE_ALSA)
# include <alsa/asoundlib.h>
#endif

#if defined(WIN32) && !defined(HX_DOS)
# include <shobjidl.h>
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
# include "shell.h"
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

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

#if !defined(C_SDL2) && !defined(RISCOS)
# include "SDL_version.h"
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
bool osx_detect_nstouchbar(void);
void osx_init_touchbar(void);
#endif

bool TTF_using(void);
void ShutDownMemHandles(Section * sec);
void resetFontSize(), decreaseFontSize();
void GetMaxWidthHeight(int *pmaxWidth, int *pmaxHeight);
int GetNumScreen();
extern SHELL_Cmd cmd_list[];

SDL_Block sdl;
Bitu frames = 0;
unsigned int page=0;
unsigned int hostkeyalt=0;
unsigned int sendkeymap=0;
ScreenSizeInfo          screen_size_info;

#if defined(USE_TTF)
Render_ttf ttf;
bool char512 = true;
bool showbold = true;
bool showital = true;
bool showline = true;
bool showsout = false;
int outputswitch = -1;
int wpType = 0;
int wpVersion = 0;
int wpBG = -1;
bool wpExtChar = false;

static unsigned long ttfSize = sizeof(DOSBoxTTFbi), ttfSizeb = 0, ttfSizei = 0, ttfSizebi = 0;
static void * ttfFont = DOSBoxTTFbi, * ttfFontb = NULL, * ttfFonti = NULL, * ttfFontbi = NULL;
extern bool resetreq;
extern uint16_t cpMap[512];
static SDL_Color ttf_fgColor = {0, 0, 0, 0};
static SDL_Color ttf_bgColor = {0, 0, 0, 0};
static SDL_Rect ttf_textRect = {0, 0, 0, 0};
static SDL_Rect ttf_textClip = {0, 0, 0, 0};

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;		// unused
} alt_rgb;
alt_rgb altBGR0[16], altBGR1[16];
static int prev_sline = -1;
static alt_rgb *rgbColors = (alt_rgb*)render.pal.rgb;
static bool blinkCursor = false, blinkstate = false;
bool colorChanged = false, justChanged = false;
#endif
#if defined(WIN32)
#if !defined(HX_DOS)
int curscr;
RECT monrect;
typedef struct {
	int	x, y;
} xyp;
BOOL CALLBACK EnumDispProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM lParam) {
	xyp* xy = reinterpret_cast<xyp*>(lParam);
	curscr++;
	if (sdl.displayNumber==curscr) monrect=*pRcMon;
	return TRUE;
}
#endif
extern int dos_clipboard_device_access;
#endif
extern bool dos_kernel_disabled;
extern bool sync_time, manualtime;
extern bool bootguest, bootfast, bootvm;
extern int bootdrive;

void MenuBrowseFolder(char drive, std::string drive_type);
void MenuBrowseImageFile(char drive, bool arc, bool boot, bool multiple);
void MenuBootDrive(char drive);
void MenuUnmountDrive(char drive);
void MenuBrowseProgramFile(void);
void SetGameState_Run(int value);
size_t GetGameState_Run(void);

bool save_slot_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"slot",4)&&isdigit(*(mname+4)))
        SetGameState_Run(page*10+std::stoi(mname+4));
    return true;
}

#if defined(WIN32)
void MenuMountDrive(char drive, const char drive2[DOS_PATHLENGTH]);
bool drive_mountauto_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

	char root[4]="A:\\";
	root[0]=drive+'A';
    MenuMountDrive(drive+'A', root);

    return true;
}

#endif

bool drive_mounthd_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseFolder(drive+'A', "LOCAL");

    return true;
}

bool drive_mountcd_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseFolder(drive+'A', "CDROM");

    return true;
}

bool drive_mountfd_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseFolder(drive+'A', "FLOPPY");

    return true;
}

bool drive_mountfro_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    const char *mname = menuitem->get_name().c_str();
    int drive = mname[6] - 'A';
    mountfro[drive] = !mountfro[drive];
    mainMenu.get_item("drive_" + std::string(1, drive + 'A') + "_mountfro").check(mountfro[drive]).refresh_item(mainMenu);

    return true;
}

bool drive_mountarc_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseImageFile(drive+'A', true, false, false);

    return true;
}

bool drive_mountimg_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseImageFile(drive+'A', false, false, false);

    return true;
}

bool drive_mountimgs_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseImageFile(drive+'A', false, false, true);

    return true;
}

bool drive_mountiro_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    const char *mname = menuitem->get_name().c_str();
    int drive = mname[6] - 'A';
    mountiro[drive] = !mountiro[drive];
    mainMenu.get_item("drive_" + std::string(1, drive + 'A') + "_mountiro").check(mountiro[drive]).refresh_item(mainMenu);

    return true;
}

bool drive_bootimg_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive != 0 && drive != 2 && drive != 3) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBrowseImageFile(drive+'A', false, true, false);

    return true;
}

bool drive_unmount_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuUnmountDrive(drive+'A');

    return true;
}

void swapInDrive(int drive);
bool drive_swap_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    if (drive < DOS_DRIVES && Drives[drive]) {
        LOG(LOG_DOSMISC,LOG_DEBUG)("Triggering swap on drive %c",drive+'A');
        swapInDrive(drive);
    }

    return true;
}

bool drive_rescan_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    if (drive < DOS_DRIVES && Drives[drive]) {
        LOG(LOG_DOSMISC,LOG_DEBUG)("Triggering rescan on drive %c",drive+'A');
        Drives[drive]->EmptyCache();
    }

    return true;
}

int statusdrive=-1;
void MAPPER_ReleaseAllKeys();
bool drive_info_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive < 0 || drive >= DOS_DRIVES) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    statusdrive=drive;
    GUI_Shortcut(31);
    statusdrive=-1;
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

    return true;
}

bool drive_boot_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    /* menu item has name "drive_A_" ... */
    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6)) {
        drive = mname[6] - 'A';
        if (drive != 0 && drive != 2 && drive != 3) return false;
    }
    else {
        return false;
    }

    if (dos_kernel_disabled) return true;

    MenuBootDrive(drive+'A');

    return true;
}

const DOSBoxMenu::callback_t drive_callbacks[] = {
#if defined(WIN32)
    drive_mountauto_menu_callback,
#endif
    drive_mounthd_menu_callback,
    drive_mountcd_menu_callback,
    drive_mountfd_menu_callback,
    drive_mountfro_menu_callback,
    NULL,
    drive_mountarc_menu_callback,
    drive_mountimg_menu_callback,
    drive_mountimgs_menu_callback,
    drive_mountiro_menu_callback,
    NULL,
    drive_unmount_menu_callback,
    drive_rescan_menu_callback,
    drive_swap_menu_callback,
    drive_info_menu_callback,
    NULL,
    drive_boot_menu_callback,
    drive_bootimg_menu_callback,
    NULL
};

bool make_diskimage_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GUI_Shortcut(38);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return true;
}

bool list_drivenum_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GUI_Shortcut(32);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return true;
}

std::string GetIDEInfo();
bool list_ideinfo_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GUI_Shortcut(33);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return true;
}

const char *drive_opts[][2] = {
#if defined(WIN32)
	{ "mountauto",              "Auto-mount Windows drive" },
#endif
	{ "mounthd",                "Mount folder as hard drive" },
	{ "mountcd",                "Mount folder as CD drive" },
	{ "mountfd",                "Mount folder as floppy drive" },
	{ "mountfro",               "Option: mount folder read-only" },
	{ "div1",                   "--" },
	{ "mountarc",               "Mount an archive file (ZIP/7Z)" },
	{ "mountimg",               "Mount a disk or CD image file" },
	{ "mountimgs",              "Mount multiple disk/CD images" },
	{ "mountiro",               "Option: mount images read-only" },
    { "div2",                   "--" },
    { "unmount",                "Unmount drive" },
    { "rescan",                 "Rescan drive" },
    { "swap",                   "Swap disk" },
    { "info",                   "Drive information" },
    { "div3",                   "--" },
    { "boot",                   "Boot from drive" },
    { "bootimg",                "Boot from disk image" },
    { NULL, NULL }
};

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

const char *scaler_menu_opts[][2] = {
    { "none",                   "None" },
    { "normal2x",               "Normal 2X" },
    { "normal3x",               "Normal 3X" },
    { "normal4x",               "Normal 4X" },
    { "normal5x",               "Normal 5X" },
    { "hardware_none",          "Hardware None" },
    { "hardware2x",             "Hardware 2X" },
    { "hardware3x",             "Hardware 3X" },
    { "hardware4x",             "Hardware 4X" },
    { "hardware5x",             "Hardware 5X" },
    { "gray",                   "Grayscale" },
    { "gray2x",                 "Grayscale 2X" },
    { "tv2x",                   "TV 2X" },
    { "tv3x",                   "TV 3X" },
    { "scan2x",                 "Scan 2X" },
    { "scan3x",                 "Scan 3X" },
    { "rgb2x",                  "RGB 2X" },
    { "rgb3x",                  "RGB 3X" },
    { "advmame2x",              "Advanced MAME 2X" },
    { "advmame3x",              "Advanced MAME 3X" },
    { "hq2x",                   "HQ 2X" },
    { "hq3x",                   "HQ 3X" },
    { "advinterp2x",            "Advanced Interpolation 2X" },
    { "advinterp3x",            "Advanced Interpolation 3X" },
    { "2xsai",                  "2xSai" },
    { "super2xsai",             "Super2xSai" },
    { "supereagle",             "SuperEagle" },
#if C_XBRZ
    { "xbrz",                   "xBRZ" },
    { "xbrz_bilinear",          "xBRZ Bilinear" },
#endif
    { NULL, NULL }
};

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

int NonUserResizeCounter = 0;

Bitu time_limit_ms = 0;

extern bool keep_umb_on_boot;
extern bool keep_private_area_on_boot;
extern bool dos_kernel_disabled;
bool guest_machine_power_on = false;

std::string custom_savedir, savefilename = "";

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

#if C_DYNAMIC_X86
void CPU_Core_Dyn_X86_Shutdown(void);
#endif

std::string dosboxpath="";
std::string GetDOSBoxXPath() {
    std::string full;
#if defined(HX_DOS) || defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
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
    size_t found=full.find_last_of("/\\");
    if (found!=string::npos)
        dosboxpath=full.substr(0, found+1);
    else
        dosboxpath="";
    return dosboxpath;
}

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
    void MacOSX_GetWindowDPI(ScreenSizeInfo &info);
    MacOSX_GetWindowDPI(/*&*/screen_size_info);
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
extern const char*              RunningProgram;
extern bool                 CPU_CycleAutoAdjust;
extern                      cpu_cycles_count_t CPU_CyclePercUsed;
#if !(ENVIRON_INCLUDED)
extern char**                   environ;
#endif

double                      rtdelta = 0;
bool                        emu_paused = false;
bool                        mouselocked = false; //Global variable for mapper
bool                        fullscreen_switch = true;
bool                        dos_kernel_disabled = true;
bool                        startup_state_numlock = false; // Global for keyboard initialisation
bool                        startup_state_capslock = false; // Global for keyboard initialisation
bool                        startup_state_scrlock = false; // Global for keyboard initialisation
int mouse_start_x=-1, mouse_start_y=-1, mouse_end_x=-1, mouse_end_y=-1, fx=-1, fy=-1, paste_speed=20, wheel_key=0, mbutton=3;
bool wheel_guest = false, clipboard_dosapi = true;
const char *modifier;

#if defined(WIN32) && !defined(C_SDL2)
extern "C" void SDL1_hax_SetMenu(HMENU menu);
#endif

#ifdef WIN32
# include <windows.h>
#endif

#ifdef WIN32
# define STDOUT_FILE                TEXT("stdout.txt")
# define STDERR_FILE                TEXT("stderr.txt")
# define DEFAULT_CONFIG_FILE            "/dosbox-x.conf"
#elif defined(MACOSX)
# define DEFAULT_CONFIG_FILE            "/Library/Preferences/DOSBox Preferences"
#elif defined(HAIKU)
#define DEFAULT_CONFIG_FILE "~/config/settings/dosbox-x/dosbox-x.conf"
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

#if defined(C_SDL2)
# if defined(WIN32)
HWND GetHWND()
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (sdl.window == NULL)
        return nullptr;
    if (!SDL_GetWindowWMInfo(sdl.window, &wmi))
        return nullptr;
    return wmi.info.win.window;
}

HWND GetSurfaceHWND()
{
    return GetHWND();
}
# endif
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
    /* But don't set it on OS X, as we use a nicer external icon there. */
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

    if (CPU_CycleAutoAdjust) {
        sprintf(title,"%s%sDOSBox-X %s, %d%%",
            dosbox_title.c_str(),dosbox_title.empty()?"":": ",
            VERSION,(int)internal_cycles);
    }
    else {
        sprintf(title,"%s%sDOSBox-X %s, %d cycles/ms",
            dosbox_title.c_str(),dosbox_title.empty()?"":": ",
            VERSION,(int)internal_cycles);
    }

    {
        const char *what = (titlebar != NULL && *titlebar != 0) ? titlebar : RunningProgram;

        if (what != NULL && *what != 0) {
            char *p = title + strlen(title); // append to end of string

            sprintf(p,", %s",what);
        }
    }

    if (!menu.hidecycles) {
        char *p = title + strlen(title); // append to end of string

        sprintf(p,", FPS %2d",(int)frames);
    }

    if (menu.showrt) {
        char *p = title + strlen(title); // append to end of string

        sprintf(p,", %2d%%/RT",(int)floor((rtdelta / 10) + 0.5));
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

void GFX_ReleaseMouse();
void CPU_Snap_Back_To_Real_Mode();
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton) {
#if !defined(HX_DOS)
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    bool ret=tinyfd_messageBox(aTitle, aMessage, aDialogType, aIconType, aDefaultButton);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return ret;
#else
    return true;
#endif
}
bool CheckQuit(void) {
#if !defined(HX_DOS)
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("dosbox"));
    std::string warn = section->Get_string("quit warning");
    if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
    if (warn == "true")
        return systemmessagebox("Quit DOSBox-X warning","This will quit from DOSBox-X.\nAre you sure?","yesno", "question", 1);
    else if (warn == "false")
        return true;
    if (dos_kernel_disabled)
        return systemmessagebox("Quit DOSBox-X warning","You are currently running a guest system.\nAre you sure to quit anyway now?","yesno", "question", 1);
    if (warn == "autofile")
        for (uint8_t handle = 0; handle < DOS_FILES; handle++) {
            if (Files[handle] && (Files[handle]->GetName() == NULL || strcmp(Files[handle]->GetName(), "CON")) && (Files[handle]->GetInformation()&0x8000) == 0)
                return systemmessagebox("Quit DOSBox-X warning","It may be unsafe to quit from DOSBox-X right now\nbecause one or more files are currently open.\nAre you sure to quit anyway now?","yesno", "question", 1);
        }
    else if (RunningProgram&&strcmp(RunningProgram, "DOSBOX-X")&&strcmp(RunningProgram, "COMMAND")&&strcmp(RunningProgram, "4DOS"))
        return systemmessagebox("Quit DOSBox-X warning","You are currently running a program or game.\nAre you sure to quit anyway now?","yesno", "question", 1);
#endif
    return true;
}

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

void PauseDOSBoxLoop(Bitu /*unused*/) {
    bool paused = true;
    SDL_Event event;

    /* reflect in the menu that we're paused now */
    mainMenu.get_item("mapper_pause").check(true).refresh_item(mainMenu);

    MAPPER_ReleaseAllKeys();

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

    while (paused) {
        if (unpause_now) {
            unpause_now = false;
            break;
        }

#if C_EMSCRIPTEN
        emscripten_sleep(0);
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
                                      | ((screenType == SCREEN_OPENGL) ? SDL_WINDOW_OPENGL : 0) | SDL_WINDOW_SHOWN
                                      | (SDL2_resize_enable ? SDL_WINDOW_RESIZABLE : 0)
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

#if C_OPENGL && DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
unsigned int SDLDrawGenFontTextureUnitPerRow = 16;
unsigned int SDLDrawGenFontTextureRows = 16;
unsigned int SDLDrawGenFontTextureWidth = SDLDrawGenFontTextureUnitPerRow * 8;
unsigned int SDLDrawGenFontTextureHeight = SDLDrawGenFontTextureRows * 16;
bool SDLDrawGenFontTextureInit = false;
GLuint SDLDrawGenFontTexture = (GLuint)(~0UL);
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
            LOG_MSG("SDL:Failed everything including falling back to surface GFX_GetBestMode"); // completely failed it seems
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
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
#endif
}

void GFX_ForceRedrawScreen(void) {
    GFX_Stop();
    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
    GFX_Start();
#if defined(USE_TTF)
    if (TTF_using() && CurMode->type==M_TEXT) ttf.inUse = true;
    if (ttf.inUse) {
       void GFX_EndTextLines(bool force);
       GFX_EndTextLines(true);
    }
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
#if !defined(C_SDL2)
    if (!sdl.desktop.fullscreen) DOSBox_RefreshMenu(); // for menu
#endif
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
void MenuShadeRect(int x,int y,int w,int h) {
    if (OpenGL_using()) {
#if C_OPENGL
        glShadeModel (GL_FLAT);
        glBlendFunc(GL_ONE, GL_SRC_ALPHA);
        glDisable (GL_DEPTH_TEST);
        glDisable (GL_LIGHTING);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_FOG);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_TEXTURE_2D);

        glColor4ub(0, 0, 0, 64);
        glBegin(GL_QUADS);
        glVertex2i(x  ,y  );
        glVertex2i(x+w,y  );
        glVertex2i(x+w,y+h);
        glVertex2i(x  ,y+h);
        glEnd();

        glBlendFunc(GL_ONE, GL_ZERO);
        glEnable(GL_TEXTURE_2D);
#endif
    }
    else {
        if (x < 0) {
            w += x;
            x = 0;
        }
        if (y < 0) {
            h += y;
            y = 0;
        }
        if ((x+w) > sdl.surface->w)
            w = sdl.surface->w - x;
        if ((y+h) > sdl.surface->h)
            h = sdl.surface->h - y;
        if (w <= 0 || h <= 0)
            return;

        if (sdl.surface->format->BitsPerPixel == 32) {
            unsigned char *scan;
            uint32_t mask;

            mask = ((sdl.surface->format->Rmask >> 2) & sdl.surface->format->Rmask) |
                ((sdl.surface->format->Gmask >> 2) & sdl.surface->format->Gmask) |
                ((sdl.surface->format->Bmask >> 2) & sdl.surface->format->Bmask);

            assert(sdl.surface->pixels != NULL);

            scan  = (unsigned char*)sdl.surface->pixels;
            scan += y * sdl.surface->pitch;
            scan += x * 4;
            while (h-- > 0) {
                uint32_t *row = (uint32_t*)scan;
                scan += sdl.surface->pitch;
                for (unsigned int c=0;c < (unsigned int)w;c++) row[c] = (row[c] >> 2) & mask;
            }
        }
        else if (sdl.surface->format->BitsPerPixel == 16) {
            unsigned char *scan;
            uint16_t mask;

            mask = ((sdl.surface->format->Rmask >> 2) & sdl.surface->format->Rmask) |
                ((sdl.surface->format->Gmask >> 2) & sdl.surface->format->Gmask) |
                ((sdl.surface->format->Bmask >> 2) & sdl.surface->format->Bmask);

            assert(sdl.surface->pixels != NULL);

            scan  = (unsigned char*)sdl.surface->pixels;
            scan += y * sdl.surface->pitch;
            scan += x * 2;
            while (h-- > 0) {
                uint16_t *row = (uint16_t*)scan;
                scan += sdl.surface->pitch;
                for (unsigned int c=0;c < (unsigned int)w;c++) row[c] = (row[c] >> 2) & mask;
            }
        }
        else {
            /* TODO */
        }
    }
}

void MenuDrawRect(int x,int y,int w,int h,Bitu color) {
    if (OpenGL_using()) {
#if C_OPENGL
        glShadeModel (GL_FLAT);
        glBlendFunc(GL_ONE, GL_ZERO);
        glDisable (GL_DEPTH_TEST);
        glDisable (GL_LIGHTING);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_FOG);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_TEXTURE_2D);

        glColor3ub((color >> 16UL) & 0xFF,(color >> 8UL) & 0xFF,(color >> 0UL) & 0xFF);
        glBegin(GL_QUADS);
        glVertex2i(x  ,y  );
        glVertex2i(x+w,y  );
        glVertex2i(x+w,y+h);
        glVertex2i(x  ,y+h);
        glEnd();

        glBlendFunc(GL_ONE, GL_ZERO);
        glEnable(GL_TEXTURE_2D);
#endif
    }
    else {
        if (x < 0) {
            w += x;
            x = 0;
        }
        if (y < 0) {
            h += y;
            y = 0;
        }
        if ((x+w) > sdl.surface->w)
            w = sdl.surface->w - x;
        if ((y+h) > sdl.surface->h)
            h = sdl.surface->h - y;
        if (w <= 0 || h <= 0)
            return;

        if (sdl.surface->format->BitsPerPixel == 32) {
            unsigned char *scan;

            assert(sdl.surface->pixels != NULL);

            scan  = (unsigned char*)sdl.surface->pixels;
            scan += y * sdl.surface->pitch;
            scan += x * 4;
            while (h-- > 0) {
                uint32_t *row = (uint32_t*)scan;
                scan += sdl.surface->pitch;
                for (unsigned int c=0;c < (unsigned int)w;c++) row[c] = (uint32_t)color;
            }
        }
        else if (sdl.surface->format->BitsPerPixel == 16) {
            unsigned char *scan;

            assert(sdl.surface->pixels != NULL);

            scan  = (unsigned char*)sdl.surface->pixels;
            scan += y * sdl.surface->pitch;
            scan += x * 2;
            while (h-- > 0) {
                uint16_t *row = (uint16_t*)scan;
                scan += sdl.surface->pitch;
                for (unsigned int c=0;c < (unsigned int)w;c++) row[c] = (uint16_t)color;
            }
        }
        else {
            /* TODO */
        }
    }
}

extern uint8_t int10_font_14[256 * 14];
extern uint8_t int10_font_16[256 * 16];

void MenuDrawTextChar(int x,int y,unsigned char c,Bitu color) {
    static const unsigned int fontHeight = 16;

    if (x < 0 || y < 0 ||
        (unsigned int)(x+8) > (unsigned int)sdl.surface->w ||
        (unsigned int)(y+(int)fontHeight) > (unsigned int)sdl.surface->h)
        return;

    unsigned char *bmp = (unsigned char*)int10_font_16 + (c * fontHeight);

    if (OpenGL_using()) {
#if C_OPENGL
        unsigned int tx = (c % 16u) * 8u;
        unsigned int ty = (c / 16u) * 16u;

        /* MenuDrawText() has prepared OpenGL state for us */
        glBegin(GL_QUADS);
        // lower left
        glTexCoord2i((int)tx+0,    (int)ty                ); glVertex2i((int)x,  (int)y                );
        // lower right
        glTexCoord2i((int)tx+8,    (int)ty                ); glVertex2i((int)x+8,(int)y                );
        // upper right
        glTexCoord2i((int)tx+8,    (int)ty+(int)fontHeight); glVertex2i((int)x+8,(int)y+(int)fontHeight);
        // upper left
        glTexCoord2i((int)tx+0,    (int)ty+(int)fontHeight); glVertex2i((int)x,  (int)y+(int)fontHeight);
        glEnd();
#endif
    }
    else {
        unsigned char *scan;

        assert(sdl.surface->pixels != NULL);

        if (x < 0 || y < 0)
            return;
        if ((x + 8) > sdl.surface->w)
            return;
        if ((y + (int)fontHeight) > sdl.surface->h)
            return;

        scan  = (unsigned char*)sdl.surface->pixels;
        scan += (unsigned int)y * (unsigned int)sdl.surface->pitch;
        scan += (unsigned int)x * (((unsigned int)sdl.surface->format->BitsPerPixel+7u)/8u);

        for (unsigned int row=0;row < fontHeight;row++) {
            unsigned char rb = bmp[row];

            if (sdl.surface->format->BitsPerPixel == 32) {
                uint32_t *dp = (uint32_t*)scan;
                for (unsigned int colm=0x80;colm != 0;colm >>= 1) {
                    if (rb & colm) *dp = (uint32_t)color;
                    dp++;
                }
            }
            else if (sdl.surface->format->BitsPerPixel == 16) {
                uint16_t *dp = (uint16_t*)scan;
                for (unsigned int colm=0x80;colm != 0;colm >>= 1) {
                    if (rb & colm) *dp = (uint16_t)color;
                    dp++;
                }
            }

            scan += (size_t)sdl.surface->pitch;
        }
    }
}

void MenuDrawTextChar2x(int x,int y,unsigned char c,Bitu color) {
    static const unsigned int fontHeight = 16;

    if (x < 0 || y < 0 ||
        (unsigned int)(x+8) > (unsigned int)sdl.surface->w ||
        (unsigned int)(y+(int)fontHeight) > (unsigned int)sdl.surface->h)
        return;

    unsigned char *bmp = (unsigned char*)int10_font_16 + (c * fontHeight);

    if (OpenGL_using()) {
#if C_OPENGL
        unsigned int tx = (c % 16u) * 8u;
        unsigned int ty = (c / 16u) * 16u;

        /* MenuDrawText() has prepared OpenGL state for us */
        glBegin(GL_QUADS);
        // lower left
        glTexCoord2i((int)tx+0,    (int)ty                ); glVertex2i(x,      y                    );
        // lower right
        glTexCoord2i((int)tx+8,    (int)ty                ); glVertex2i(x+(8*2),y                    );
        // upper right
        glTexCoord2i((int)tx+8,    (int)ty+(int)fontHeight); glVertex2i(x+(8*2),y+((int)fontHeight*2));
        // upper left
        glTexCoord2i((int)tx+0,    (int)ty+(int)fontHeight); glVertex2i(x,      y+((int)fontHeight*2));
        glEnd();
#endif
    }
    else { 
        unsigned char *scan;

        assert(sdl.surface->pixels != NULL);

        if (x < 0 || y < 0)
            return;
        if ((x + 16) > sdl.surface->w)
            return;
        if ((y + ((int)fontHeight * 2)) > sdl.surface->h)
            return;

        scan  = (unsigned char*)sdl.surface->pixels;
        scan += y * sdl.surface->pitch;
        scan += x * ((sdl.surface->format->BitsPerPixel+7)/8);

        for (unsigned int row=0;row < (fontHeight*2);row++) {
            unsigned char rb = bmp[row>>1U];

            if (sdl.surface->format->BitsPerPixel == 32) {
                uint32_t *dp = (uint32_t*)scan;
                for (unsigned int colm=0x80;colm != 0;colm >>= 1) {
                    if (rb & colm) {
                        *dp++ = (uint32_t)color;
                        *dp++ = (uint32_t)color;
                    }
                    else {
                        dp += 2;
                    }
                }
            }
            else if (sdl.surface->format->BitsPerPixel == 16) {
                uint16_t *dp = (uint16_t*)scan;
                for (unsigned int colm=0x80;colm != 0;colm >>= 1) {
                    if (rb & colm) {
                        *dp++ = (uint16_t)color;
                        *dp++ = (uint16_t)color;
                    }
                    else {
                        dp += 2;
                    }
                }
            }

            scan += sdl.surface->pitch;
        }
    }
}

void MenuDrawText(int x,int y,const char *text,Bitu color) {
#if C_OPENGL
    if (OpenGL_using()) {
        glBindTexture(GL_TEXTURE_2D,SDLDrawGenFontTexture);

        glPushMatrix();

        glMatrixMode (GL_TEXTURE);
        glLoadIdentity ();
        glScaled(1.0 / SDLDrawGenFontTextureWidth, 1.0 / SDLDrawGenFontTextureHeight, 1.0);

        glColor4ub((color >> 16UL) & 0xFF,(color >> 8UL) & 0xFF,(color >> 0UL) & 0xFF,0xFF);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
    }
#endif

    while (*text != 0) {
        if (mainMenu.fontCharScale >= 2)
            MenuDrawTextChar2x(x,y,(unsigned char)(*text++),color);
        else
            MenuDrawTextChar(x,y,(unsigned char)(*text++),color);

        x += (int)mainMenu.fontCharWidth;
    }

#if C_OPENGL
    if (OpenGL_using()) {
        glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glBlendFunc(GL_ONE, GL_ZERO);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_TEXTURE_2D);

        glPopMatrix();

        glBindTexture(GL_TEXTURE_2D,sdl_opengl.texture);
    }
#endif
}

void DOSBoxMenu::item::drawMenuItem(DOSBoxMenu &menu) {
    (void)menu;//UNUSED

    Bitu bgcolor = GFX_GetRGB(63, 63, 63);
    Bitu fgcolor = GFX_GetRGB(191, 191, 191);
    Bitu fgshortcolor = GFX_GetRGB(127, 127, 191);
    Bitu fgcheckcolor = GFX_GetRGB(191, 191, 127);

    if (type >= separator_type_id) {
        /* separators never change visual state on hover/select */
    }
    else if (!status.enabled) {
        if (itemHover)
            bgcolor = GFX_GetRGB(79, 79, 79);

        fgcolor = GFX_GetRGB(144, 144, 144);
        fgshortcolor = GFX_GetRGB(63, 63, 144);
        fgcheckcolor = GFX_GetRGB(144, 144, 63);
    }
    else if (itemHilight) {
        bgcolor = GFX_GetRGB(0, 0, 63);
        fgcolor = GFX_GetRGB(255, 255, 255);
        fgshortcolor = GFX_GetRGB(191, 191, 255);
    }
    else if (itemHover) {
        bgcolor = GFX_GetRGB(127, 127, 127);
        fgcolor = GFX_GetRGB(255, 255, 255);
        fgshortcolor = GFX_GetRGB(191, 191, 255);
    }

    itemHoverDrawn = itemHover;
    itemHilightDrawn = itemHilight;

    if (SDL_MUSTLOCK(sdl.surface))
        SDL_LockSurface(sdl.surface);

    MenuDrawRect(screenBox.x, screenBox.y, screenBox.w, screenBox.h, bgcolor);
    if (checkBox.w != 0 && checkBox.h != 0) {
        const char *str = status.checked ? "\xFB" : " ";

        MenuDrawText(screenBox.x+checkBox.x, screenBox.y+checkBox.y, str, fgcheckcolor);
    }
    if (textBox.w != 0 && textBox.h != 0)
        MenuDrawText(screenBox.x+textBox.x, screenBox.y+textBox.y, text.c_str(), fgcolor);
    if (shortBox.w != 0 && shortBox.h != 0)
        MenuDrawText(screenBox.x+shortBox.x, screenBox.y+shortBox.y, shortcut_text.c_str(), fgshortcolor);

    if (type == submenu_type_id && borderTop/*not toplevel*/)
        MenuDrawText((int)((int)screenBox.x+(int)screenBox.w - (int)mainMenu.fontCharWidth - 1), (int)((int)screenBox.y+(int)textBox.y), "\x10", fgcheckcolor);

    if (type == separator_type_id)
        MenuDrawRect((int)screenBox.x, (int)screenBox.y + ((int)screenBox.h/2), (int)screenBox.w, 1, fgcolor);
    else if (type == vseparator_type_id)
        MenuDrawRect((int)screenBox.x + ((int)screenBox.w/2), (int)screenBox.y, 1, (int)screenBox.h, fgcolor);

    if (SDL_MUSTLOCK(sdl.surface))
        SDL_UnlockSurface(sdl.surface);
}

void DOSBoxMenu::displaylist::DrawDisplayList(DOSBoxMenu &menu,bool updateScreen) {
    for (auto &id : disp_list) {
        DOSBoxMenu::item &item = menu.get_item(id);

        item.drawMenuItem(menu);
        if (updateScreen) item.updateScreenFromItem(menu);
    }
}

bool DOSBox_isMenuVisible(void);

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
#endif

void RENDER_Reset(void);

#if defined(USE_TTF)
bool firstsize = true;
static Bitu OUTPUT_TTF_SetSize() {
    bool text=CurMode&&(CurMode->type==0||CurMode->type==2||CurMode->type==M_TEXT||IS_PC98_ARCH);
    if (text) {
        sdl.clip.x = sdl.clip.y = 0;
        sdl.draw.width = sdl.clip.w = ttf.cols*ttf.width;
        sdl.draw.height = sdl.clip.h = ttf.lins*ttf.height;
        ttf.inUse = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
        if (mainMenu.isVisible() && !ttf.fullScrn) {
            sdl.clip.y += mainMenu.menuBox.h;
            // hack
            ttf.offX = sdl.clip.x;
            ttf.offY = sdl.clip.y;
        }
#endif
    } else
        ttf.inUse = false;
#if defined(C_SDL2)
    int bx = 0, by = 0;
    if (sdl.displayNumber>0) {
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
    }
#endif
    if (ttf.inUse && ttf.fullScrn) {
        int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(C_SDL2)
        GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(maxWidth, maxHeight);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        if (sdl.displayNumber==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        else SDL_SetWindowPosition(sdl.window, bx, by);
#else
        sdl.surface = SDL_SetVideoMode(maxWidth, maxHeight, 32, SDL_SWSURFACE|
#if defined(WIN32)
        SDL_NOFRAME
#else
        SDL_FULLSCREEN
#endif
        );
#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS)
        if (sdl.displayNumber>0) {
            xyp xy={0};
            xy.x=-1;
            xy.y=-1;
            curscr=0;
            EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
            HMONITOR monitor = MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info;
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(monitor, &info);
            MoveWindow(GetHWND(), info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right-info.rcMonitor.left, info.rcMonitor.bottom-info.rcMonitor.top, true);
        }
#endif
#endif
    } else {
#if defined(C_SDL2)
        GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(sdl.draw.width + sdl.clip.x, sdl.draw.height + sdl.clip.y);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        if (firstsize && (posx < 0 || posy < 0) && text) {
            firstsize=false;
            if (sdl.displayNumber==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else SDL_SetWindowPosition(sdl.window, bx, by);
        }
#else
        sdl.surface = SDL_SetVideoMode(sdl.draw.width + sdl.clip.x, sdl.draw.height + sdl.clip.y, 32, SDL_SWSURFACE);
#endif
    }
	if (!sdl.surface)
		E_Exit("SDL: Failed to create surface");
	SDL_ShowCursor(!ttf.fullScrn);
	sdl.active = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)sdl.surface->w;
    mainMenu.screenHeight = (size_t)sdl.surface->h;
    mainMenu.updateRect();
    mainMenu.setRedraw();
#endif

    return GFX_CAN_32 | GFX_SCALING;
}
#endif

Bitu GFX_SetSize(Bitu width, Bitu height, Bitu flags, double scalex, double scaley, GFX_CallBack_t callback) 
{
    if ((width == 0 || height == 0) && !TTF_using()) {
        E_Exit("GFX_SetSize with width=%d height=%d zero dimensions not allowed",(int)width,(int)height);
        return 0;
    }

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
            LOG_MSG("SDL:Failed everything including falling back to surface in GFX_GetSize"); // completely failed it seems
    }

    // we have selected an actual desktop type
    sdl.desktop.type = sdl.desktop.want_type;

    GFX_LogSDLState();

    if (retFlags) 
        GFX_Start();

    if (!sdl.mouse.autoenable && !sdl.mouse.locked)
        SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);

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
        if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
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
    if (!pressed)
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

#if defined(USE_TTF)
void GFX_SelectFontByPoints(int ptsize);
bool lastmenu = true, initttf = false;
int lastfontsize = 0;
void setVGADAC() {
    if (ttf.inUse&&CurMode&&IS_VGA_ARCH) {
        std::map<uint8_t,int> imap;
        for (uint8_t i = 0; i < 0x10; i++) {
            IO_ReadB(mem_readw(BIOS_VIDEO_PORT)+6);
            IO_WriteB(VGAREG_ACTL_ADDRESS, i+32);
            imap[i]=IO_ReadB(VGAREG_ACTL_READ_DATA);
            IO_WriteB(VGAREG_DAC_WRITE_ADDRESS, imap[i]);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].red*63/255);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].green*63/255);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].blue*63/255);
        }
    }
}

bool setColors(const char *colorArray, int n) {
    if (IS_PC98_ARCH) return false;
    if (!colorChanged)
        for (uint8_t i = 0; i < 0x10; i++) {
            altBGR1[i].red=rgbColors[i].red;
            altBGR1[i].green=rgbColors[i].green;
            altBGR1[i].blue=rgbColors[i].blue;
        }
	const char * nextRGB = colorArray;
	uint8_t * altPtr = (uint8_t *)altBGR1;
	int rgbVal[3] = {-1,-1,-1};
	for (int colNo = 0; colNo < (n>-1?1:16); colNo++) {
		if (n>-1) altPtr+=4*n;
		if (sscanf(nextRGB, " ( %d , %d , %d)", &rgbVal[0], &rgbVal[1], &rgbVal[2]) == 3) {	// Decimal: (red,green,blue)
			for (int i = 0; i< 3; i++) {
				if (rgbVal[i] < 0 || rgbVal[i] > 255)
					return false;
				altPtr[i] = rgbVal[i];
			}
			while (*nextRGB != ')')
				nextRGB++;
			nextRGB++;
		} else if (sscanf(nextRGB, " #%6x", &rgbVal[0]) == 1) {							// Hexadecimal
			if (rgbVal[0] < 0)
				return false;
			for (int i = 0; i < 3; i++) {
				altPtr[2-i] = rgbVal[0]&255;
				rgbVal[0] >>= 8;
			}
			nextRGB = strchr(nextRGB, '#') + 7;
		} else
			return false;
		altPtr += 4;
	}
	for (int i = n>-1?n:0; i < (n>-1?n+1:16); i++) {
		altBGR0[i].blue = (altBGR1[i].blue*2 + 128)/4;
		altBGR0[i].green = (altBGR1[i].green*2 + 128)/4;
		altBGR0[i].red = (altBGR1[i].red*2 + 128)/4;
	}
    setVGADAC();
    colorChanged=justChanged=true;
	return true;
}

std::string failName="";
bool readTTF(const char *fName, bool bold, bool ital) {
	FILE * ttf_fh = NULL;
	std::string exepath = "";
	char ttfPath[1024];

	strcpy(ttfPath, fName);													// Try to load it from working directory
	strcat(ttfPath, ".ttf");
    ttf_fh = fopen(ttfPath, "rb");

    if (!ttf_fh) {
        exepath=GetDOSBoxXPath();
        if (exepath.size()) {
            strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);	// Try to load it from where DOSBox-X was started
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
        }
	}
	if (!ttf_fh) {
		strcpy(ttfPath, fName);
		ttf_fh = fopen(ttfPath, "rb");
	}
	if (!ttf_fh) {
		std::string config_path;
		Cross::GetPlatformConfigDir(config_path);
		struct stat info;
		if (!stat(config_path.c_str(), &info) && (info.st_mode & S_IFDIR)) {
            strcpy(ttfPath, config_path.c_str());
            strcat(ttfPath, fName);
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(ttfPath, config_path.c_str());
                strcat(ttfPath, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        exepath=GetDOSBoxXPath();
        if (exepath.size()) {
            strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);
            ttf_fh = fopen(ttfPath, "rb");
        }
	}
    if (!ttf_fh) {
        char fontdir[300];
#if defined(WIN32)
        strcpy(fontdir, "C:\\WINDOWS\\fonts\\");
        struct stat wstat;
        if(stat(fontdir,&wstat) || !(wstat.st_mode & S_IFDIR)) {
            char dir[MAX_PATH];
            if (GetWindowsDirectory(dir, MAX_PATH)) {
                strcpy(fontdir, dir);
                strcat(fontdir, "\\fonts\\");
            }
        }
#elif defined(LINUX)
        strcpy(fontdir, "/usr/share/fonts/truetype/");
#elif defined(MACOSX)
        strcpy(fontdir, "/Library/Fonts/");
#else
        strcpy(fontdir, "/fonts/");
#endif
        strcpy(ttfPath, fontdir);
        strcat(ttfPath, fName);
        strcat(ttfPath, ".ttf");
        ttf_fh = fopen(ttfPath, "rb");
        if (!ttf_fh) {
            strcpy(ttfPath, fontdir);
            strcat(ttfPath, fName);
            ttf_fh = fopen(ttfPath, "rb");
        }
    }
	if (ttf_fh) {
		if (!fseek(ttf_fh, 0, SEEK_END))
			if (((bold&&ital?ttfSizebi:(bold&&!ital?ttfSizeb:(!bold&&ital?ttfSizei:ttfSize))) = ftell(ttf_fh)) != -1L)
				if ((bold&&ital?ttfFontbi:(bold&&!ital?ttfFontb:(!bold&&ital?ttfFonti:ttfFont))) = malloc((size_t)(bold&&ital?ttfSizebi:(bold&&!ital?ttfSizeb:(!bold&&ital?ttfSizei:ttfSize)))))
					if (!fseek(ttf_fh, 0, SEEK_SET))
						if (fread((bold&&ital?ttfFontbi:(bold&&!ital?ttfFontb:(!bold&&ital?ttfFonti:ttfFont))), 1, (size_t)(bold&&ital?ttfSizebi:(bold&&!ital?ttfSizeb:(!bold&&ital?ttfSizei:ttfSize))), ttf_fh) == (size_t)(bold&&ital?ttfSizebi:(bold&&!ital?ttfSizeb:(!bold&&ital?ttfSizei:ttfSize)))) {
							fclose(ttf_fh);
							return true;
						}
		fclose(ttf_fh);
	}
    if (!failName.size()||failName.compare(fName)) {
        failName=std::string(fName);
        std::string message="Could not load font file: "+std::string(fName)+(strlen(fName)<5||strcasecmp(fName+strlen(fName)-4, ".ttf")?".ttf":"");
        systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
    }
	return false;
}

bool firstset=true;
void OUTPUT_TTF_Select(int fsize=-1) {
    if (!initttf&&TTF_Init()) {											// Init SDL-TTF
        std::string message = "Could not init SDL-TTF: " + std::string(SDL_GetError());
        systemmessagebox("Error", message.c_str(), "ok","error", 1);
        sdl.desktop.want_type = SCREEN_SURFACE;
        return;
    }
    int fontSize = 0;
    int winPerc = 0;
    if (fsize==2)
        winPerc = 100;
    else if (fsize>9)
        fontSize = fsize;
    else {
        Section_prop * render_section=static_cast<Section_prop *>(control->GetSection("render"));
        const char * fName = render_section->Get_string("ttf.font");
        const char * fbName = render_section->Get_string("ttf.fontbold");
        const char * fiName = render_section->Get_string("ttf.fontital");
        const char * fbiName = render_section->Get_string("ttf.fontboit");
        LOG_MSG("SDL:TTF activated %s", fName);
        if (!*fName||!readTTF(fName, false, false)) {
            ttf.DOSBox = true;
            std::string message="";
            if (*fbName)
                message="A valid ttf.font setting is required for the ttf.fontbold setting: "+std::string(fbName);
            else if (*fiName)
                message="A valid ttf.font setting is required for the ttf.fontital setting: "+std::string(fiName);
            else if (*fbiName)
                message="A valid ttf.font setting is required for the ttf.fontboit setting: "+std::string(fbiName);
            if (fsize==0&&message.size())
                systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
        } else {
            if (!*fbName||!readTTF(fbName, true, false))
                ttfSizeb = 0;
            if (!*fiName||!readTTF(fiName, false, true))
                ttfSizei = 0;
            if (!*fbiName||!readTTF(fbiName, true, true))
                ttfSizebi = 0;
        }
        const char * colors = render_section->Get_string("ttf.colors");
        if (*colors) {
            if (!setColors(colors,-1)) {
                LOG_MSG("Incorrect color scheme: %s", colors);
                //setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);
            }
        }
        blinkCursor = render_section->Get_bool("ttf.blinkc");
        const char *wpstr=render_section->Get_string("ttf.wp");
        wpType=0;
        wpVersion=0;
        if (strlen(wpstr)>1) {
            if (!strncasecmp(wpstr, "WP", 2)) wpType=1;
            else if (!strncasecmp(wpstr, "WS", 2)) wpType=2;
            else if (!strncasecmp(wpstr, "XY", 3)) wpType=3;
            if (strlen(wpstr)>2&&wpstr[2]>='1'&&wpstr[2]<='9') wpVersion=wpstr[2]-'0';
        }
        wpBG = render_section->Get_int("ttf.wpbg");
        winPerc = render_section->Get_int("ttf.winperc");
        if (winPerc>100||(fsize!=1&&(control->opt_fullscreen||static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("fullscreen")))) winPerc=100;
        else if (winPerc<25) winPerc=25;
        if (fsize==1&&winPerc==100) winPerc=60;
        fontSize = render_section->Get_int("ttf.ptsize");
        char512 = render_section->Get_bool("ttf.char512");
        showbold = render_section->Get_bool("ttf.bold");
        showital = render_section->Get_bool("ttf.italic");
        showline = render_section->Get_bool("ttf.underline");
        showsout = render_section->Get_bool("ttf.strikeout");
        const char *outputstr=render_section->Get_string("ttf.outputswitch");
#if C_DIRECT3D
        if (!strcasecmp(outputstr, "direct3d"))
            switchoutput = 5;
        else
#endif
#if C_OPENGL
        if (!strcasecmp(outputstr, "openglnb"))
            switchoutput = 4;
        else if (!strcasecmp(outputstr, "opengl")||!strcasecmp(outputstr, "openglnq"))
            switchoutput = 3;
        else
#endif
        if (!strcasecmp(outputstr, "surface"))
            switchoutput = 0;
        else
            switchoutput = -1;

        ttf.lins = render_section->Get_int("ttf.lins");
        ttf.cols = render_section->Get_int("ttf.cols");
        if ((!CurMode||CurMode->type!=M_TEXT)&&!IS_PC98_ARCH) {
            if (ttf.cols<1) ttf.cols=80;
            if (ttf.lins<1) ttf.lins=25;
            ttf.lins = MAX(24, MIN(txtMaxLins, ttf.lins));
            ttf.cols = MAX(40, MIN(txtMaxCols, ttf.cols));
        } else if (firstset) {
            bool alter_vmode=false;
            uint16_t c=0, r=0;
            if (IS_PC98_ARCH) {
                c=80;
                r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
            } else {
                c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                r=(uint16_t)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
            }
            if (ttf.lins<1||ttf.cols<1)	{
                if (ttf.cols<1)
                    ttf.cols = c;
                else {
                    ttf.cols = MAX(40, MIN(txtMaxCols, ttf.cols));
                    if (ttf.cols != c) alter_vmode = true;
                }
                if (ttf.lins<1)
                    ttf.lins = r;
                else {
                    ttf.lins = MAX(24, MIN(txtMaxLins, ttf.lins));
                    if (ttf.lins != r) alter_vmode = true;
                }
            } else {
                ttf.lins = MAX(24, MIN(txtMaxLins, ttf.lins));
                ttf.cols = MAX(40, MIN(txtMaxCols, ttf.cols));
                if (ttf.cols != c || ttf.lins != r) alter_vmode = true;
            }
            if (alter_vmode) {
                for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++) {								// Set the cols and lins in video mode 2,3,7
                    ModeList_VGA[i].twidth = ttf.cols;
                    ModeList_VGA[i].theight = ttf.lins;
                }
                if (!IS_PC98_ARCH) {
                    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_COLS,ttf.cols);
                    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,ttf.lins-1);
                }
            }
            firstset=false;
        } else {
            uint16_t c=0, r=0;
            if (IS_PC98_ARCH) {
                c=80;
                r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
            } else {
                c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
                r=(uint16_t)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
            }
            ttf.cols=c;
            ttf.lins=r;
        }
    }

    if (winPerc == 100) {
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        lastmenu = menu.toggle;
        menu.toggle=false;
        NonUserResizeCounter=1;
        SDL1_hax_SetMenu(NULL);
        void RENDER_CallBack( GFX_CallBackFunctions_t function );
        RENDER_CallBack( GFX_CallBackReset );
#endif
        void DOSBox_SetSysMenu(void);
        DOSBox_SetSysMenu();
        ttf.fullScrn = true;
    } else
        ttf.fullScrn = false;

    int maxWidth, maxHeight;
    GetMaxWidthHeight(&maxWidth, &maxHeight);

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
    maxHeight -= mainMenu.menuBarHeightBase * 2;
#endif

#if defined(WIN32)
    if (!ttf.fullScrn) {																			// 3D borders
        maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
        maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
    }
#endif
    int	curSize = fontSize>9?fontSize:30;															// no clear idea what would be a good starting value
    int lastGood = -1;
    int trapLoop = 0;

    if (fontSize<10) {
        while (curSize != lastGood) {
            GFX_SelectFontByPoints(curSize);
            if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {				// if it fits on screen
                lastGood = curSize;
                float coveredPerc = float(100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight);
                if (trapLoop++ > 4 && coveredPerc <= winPerc)										// we can get into a +/-/+/-... loop!
                    break;
                curSize = (int)(curSize*sqrt((float)winPerc/coveredPerc));							// rounding down is ok
                if (curSize < 10)																	// minimum size = 10
                    curSize = 10;
            } else if (--curSize < 10)																// silly, but OK, one never can tell..
                E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
        }
        if (ttf.DOSBox)																				// make it even for DOSBox-X internal font (a bit nicer)
            curSize &= ~1;
    }
    GFX_SelectFontByPoints(curSize);
    if (fontSize>9 && 100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight > 100)
        E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
    resetreq=false;
    sdl.desktop.want_type = SCREEN_TTF;
}
#endif

void change_output(int output) {
    GFX_Stop();
    Section * sec = control->GetSection("sdl");
    Section_prop * section=static_cast<Section_prop *>(sec);
    sdl.overscan_width=(unsigned int)section->Get_int("overscan");
    UpdateOverscanMenu();

    switch (output) {
    case 0:
        OUTPUT_SURFACE_Select();
        break;
    case 1:
        OUTPUT_SURFACE_Select();
        break;
    case 2: /* do nothing */
        break;
    case 3:
#if C_OPENGL
        change_output(2);
        OUTPUT_OPENGL_Select();
        sdl_opengl.bilinear = true;
#endif
        break;
    case 4:
#if C_OPENGL
        change_output(2);
        OUTPUT_OPENGL_Select();
        sdl_opengl.bilinear = false; //NB
#endif
        break;
#if C_DIRECT3D
    case 5:
        OUTPUT_DIRECT3D_Select();
        break;
#endif
    case 6:
        break;
    case 7:
        // do not set want_type
        break;
    case 8:
#if C_DIRECT3D
        if (sdl.desktop.want_type == SCREEN_DIRECT3D) 
            OUTPUT_DIRECT3D_Select();
#endif
        break;
#if defined(USE_TTF)
    case 9:
        OUTPUT_TTF_Select();
    case 10:
        sdl.desktop.want_type = SCREEN_TTF;
        ttf.inUse = true;
        break;
#endif

    default:
        LOG_MSG("SDL:Unsupported output device %d, switching back to surface",output);
        OUTPUT_SURFACE_Select();
        break;
    }

#if C_OPENGL
    if (sdl.desktop.want_type != SCREEN_OPENGL) mainMenu.get_item("load_glsl_shader").enable(false).refresh_item(mainMenu);
#endif
#if C_DIRECT3D
    if (sdl.desktop.want_type != SCREEN_DIRECT3D) mainMenu.get_item("load_d3d_shader").enable(false).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    if (sdl.desktop.want_type != SCREEN_TTF) {
        mainMenu.get_item("load_ttf_font").enable(false).refresh_item(mainMenu);
        resetreq = false;
        ttf.inUse = false;
    }
#endif

    const char* windowresolution=section->Get_string("windowresolution");
    if (windowresolution && *windowresolution) 
    {
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if (strcmp(windowresolution,"original")) 
        {
            if (output == 0) 
            {
                std::string tmp("windowresolution=original");
                sec->HandleInputline(tmp);
            }
        }
    }
    res_init();

    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
#ifdef C_OPENGL
        mainMenu.get_item("load_glsl_shader").enable(sdl.desktop.want_type==SCREEN_OPENGL&&initgl==2).refresh_item(mainMenu);
#endif
#ifdef C_DIRECT3D
        mainMenu.get_item("load_d3d_shader").enable(sdl.desktop.want_type==SCREEN_DIRECT3D).refresh_item(mainMenu);
#endif
#ifdef USE_TTF
        mainMenu.get_item("load_ttf_font").enable(sdl.desktop.want_type==SCREEN_TTF).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    if ((output==9||output==10)&&ttf.inUse) {
        resetFontSize();
        resetreq = true;
    }
#if defined(WIN32) && !defined(HX_DOS)
    HMENU sysmenu = GetSystemMenu(GetHWND(), TRUE);
    if (sysmenu != NULL) {
        EnableMenuItem(sysmenu, ID_WIN_SYSMENU_TTFINCSIZE, MF_BYCOMMAND|(TTF_using()?MF_ENABLED:MF_DISABLED));
        EnableMenuItem(sysmenu, ID_WIN_SYSMENU_TTFDECSIZE, MF_BYCOMMAND|(TTF_using()?MF_ENABLED:MF_DISABLED));
    }
#endif
    mainMenu.get_item("mapper_ttf_incsize").enable(TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("mapper_ttf_decsize").enable(TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showbold").enable(TTF_using()).check(showbold).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showital").enable(TTF_using()).check(showital).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showline").enable(TTF_using()).check(showline).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showsout").enable(TTF_using()).check(showsout).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpno").enable(TTF_using()).check(!wpType).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpwp").enable(TTF_using()).check(wpType==1).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpws").enable(TTF_using()).check(wpType==2).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpxy").enable(TTF_using()).check(wpType==3).refresh_item(mainMenu);
#endif

    if (output != 7) GFX_SetTitle((int32_t)(CPU_CycleAutoAdjust?CPU_CyclePercUsed:CPU_CycleMax),-1,-1,false);
    GFX_LogSDLState();

    UpdateWindowDimensions();
}

#if !defined(HX_DOS) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void SDL_hax_SetFSWindowPosition(int x,int y,int w,int h);
#endif

void GFX_SwitchFullScreen(void)
{
#if defined(USE_TTF)
    if (ttf.inUse) {
        if (ttf.fullScrn) {
            sdl.desktop.fullscreen = false;
            if (lastfontsize>0)
                OUTPUT_TTF_Select(lastfontsize);
            else
                OUTPUT_TTF_Select(1);
            resetFontSize();
            lastfontsize = 0;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            if (lastmenu) DOSBox_SetMenu();
#endif
#if defined(C_SDL2)
            if (posx >= 0 && posy >= 0)
                SDL_SetWindowPosition(sdl.window, posx, posy);
            else if (sdl.displayNumber==0)
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
#endif
            resetreq = true;
            GFX_ResetScreen();
        } else {
            lastfontsize = ttf.pointsize;
            sdl.desktop.fullscreen = true;
            OUTPUT_TTF_Select(2);
            resetFontSize();
        }
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
               always necesary because it might not always work.

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
                        LOG_MSG("OS X: Actual maximum screen resolution is %d x %d\n",maxwidth,maxheight);

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
    // TODO this will need further changes to accomodate different outputs (e.g. stretched)
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
            return;
        }
    }

    LOG_MSG("INFO: switched to %s mode", full ? "full screen" : "window");

#if defined (WIN32)
    // (re-)assign menu to window
    void DOSBox_SetSysMenu(void);
    DOSBox_SetSysMenu();
#endif

    // ensure mouse capture when fullscreen || (re-)capture if user said so when windowed
    auto locked = sdl.mouse.locked;
    if ((full && !locked) || (!full && locked)) GFX_CaptureMouse();

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
    if (!pressed)
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
        void DOSBox_SetSysMenu(void);
        DOSBox_SetSysMenu();
#if !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
        SDL1_hax_RemoveMinimize = lockout ? 1 : 0;
        int Reflect_Menu(void);
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

#if !defined(C_SDL2)
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
#endif

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

#if defined(USE_TTF)
static int charSet;
void processWP(uint8_t *pcolorBG, uint8_t *pcolorFG) {
    charSet = 0;
    if (!wpType) return;
    uint8_t colorBG = *pcolorBG, colorFG = *pcolorFG;
    int style = 0;
    if (CurMode->mode == 7) {														// Mono (Hercules)
        style = showline && (colorFG&7) == 1 ? TTF_STYLE_UNDERLINE : TTF_STYLE_NORMAL;
        if ((colorFG&0xa) == colorFG && (colorBG&15) == 0)
            colorFG = 8;
        else if (colorFG&7)
            colorFG |= 7;
    }
    else if (wpType == 1) {															// WordPerfect
        if (showital && colorFG == 0xe && (colorBG&15) == (wpBG > -1 ? wpBG : 1)) {
            style = TTF_STYLE_ITALIC;
            colorFG = 7;
        }
        else if (showline && (colorFG == 1 || colorFG == 0xf) && (colorBG&15) == 7) {
            style = TTF_STYLE_UNDERLINE;
            colorBG = 1;
            colorFG = colorFG == 1 ? 7 : 0xf;
        }
        else if (showsout && colorFG == 0 && (colorBG&15) == 3) {
            style = TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : 1;
            colorFG = 7;
        }
    } else if (wpType == 2) {														// WordStar
        if (colorBG&8) {
            if (showline && colorBG&1)
                style |= TTF_STYLE_UNDERLINE;
            if (showital && colorBG&2)
                style |= TTF_STYLE_ITALIC;
            if (showsout && colorBG&4)
                style |= TTF_STYLE_STRIKETHROUGH;
            if (style)
                colorBG = wpBG > -1 ? wpBG : 0;
        }
    } else if (wpType == 3) {														// XyWrite
        if (showital && (colorFG == 10 || colorFG == 14) && colorBG != 12 && !(!showline && colorBG == 3)) {
            style = TTF_STYLE_ITALIC;
            if (colorBG == 3) {
                style |= TTF_STYLE_UNDERLINE;
                colorBG = wpBG > -1 ? wpBG : 1;
            }
            colorFG = colorFG == 10 ? 7:15;
        }
        else if (showline && (colorFG == 3 || colorFG == 0xb)) {
            style = TTF_STYLE_UNDERLINE;
            colorFG = colorFG == 3 ? 7:15;
        }
        else if (!showsout || colorBG != 4)
            style = TTF_STYLE_NORMAL;
        if (showsout && colorBG == 4 && colorFG != 12 && colorFG != 13) {
            style |= TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : (wpVersion < 4 ? 0 : 1);
            if (colorFG != 15) colorFG = 7;
        }
    }
    if (char512 && wpType == 1) {
        if (wpExtChar && (colorFG&8)) {	// WordPerfect high bit = second character set (if 512char active)
            charSet = 1;
            colorFG &= 7;
        }
    }
    if (showbold && (colorFG == 15 || (wpType == 1 && (wpVersion < 1 || wpVersion > 5 ) && colorFG == 3 && (colorBG&15) == (wpBG > -1 ? wpBG : 1)))) {
        if (!(style&TTF_STYLE_ITALIC)) style |= TTF_STYLE_BOLD;
        colorFG = 7;
    }
    if (style)
        TTF_SetFontStyle(ttf.SDL_font, style);
    else
        TTF_SetFontStyle(ttf.SDL_font, TTF_STYLE_NORMAL);
    *pcolorBG = colorBG;
    *pcolorFG = colorFG;
}

void GFX_EndTextLines(bool force=false) {
    if (!force&&!IS_PC98_ARCH&&((!CurMode||CurMode->type!=M_TEXT))) return;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    if (!ttf.fullScrn) {
        mainMenu.setRedraw();
        GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
    }
#endif
    static uint8_t bcount = 0;
	Uint16 unimap[txtMaxCols+1];							// max+1 charaters in a line
	int xmin = ttf.cols;									// keep track of changed area
	int ymin = ttf.lins;
	int xmax = -1;
	int ymax = -1;
	ttf_cell *curAC = curAttrChar;							// pointer to old/current buffer
	ttf_cell *newAC = newAttrChar;							// pointer to new/changed buffer

	if (ttf.fullScrn && (ttf.offX || ttf.offY)) {
        int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = 0; rect->y = 0; rect->w = maxWidth; rect->h = ttf.offY;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->w = ttf.offX; rect->h = maxHeight;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->x = 0; rect->y = sdl.draw.height + ttf.offY; rect->w = maxWidth; rect->h = maxHeight - sdl.draw.height - ttf.offY;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->x = sdl.draw.width + ttf.offX; rect->y = 0; rect->w = maxWidth - sdl.draw.width - ttf.offX; rect->h = maxHeight;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
    }

	if (ttf.cursor >= 0 && ttf.cursor < ttf.cols*ttf.lins)	// hide/restore (previous) cursor-character if we had one

//		if (cursor_enabled && (vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15))
//		if (ttf.cursor != vga.draw.cursor.address>>1 || (vga.draw.cursor.enabled !=  cursor_enabled) || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15)
		if (ttf.cursor != vga.draw.cursor.address>>1 || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15) {
			curAC[ttf.cursor] = newAC[ttf.cursor];
            curAC[ttf.cursor].chr ^= 0xf0f0;	// force redraw (differs)
        }

	ttf_textClip.h = ttf.height;
	ttf_textClip.y = 0;
	for (int y = 0; y < ttf.lins; y++) {
		bool draw = false;
		ttf_textRect.y = ttf.offY+y*ttf.height;
		for (int x = 0; x < ttf.cols; x++) {
			if ((newAC[x] != curAC[x] || newAC[x].selected != curAC[x].selected || (colorChanged && (justChanged || draw)) || force) && !(newAC[x].skipped)) {
				draw = true;
				xmin = min(x, xmin);
				ymin = min(y, ymin);
				ymax = y;

				bool dw = false;
				const int x1 = x;
				uint8_t colorBG = newAC[x].bg;
				uint8_t colorFG = newAC[x].fg;
				processWP(&colorBG, &colorFG);
                if (newAC[x].selected) {
                    uint8_t color = colorBG;
                    colorBG = colorFG;
                    colorFG = color;
                }
				ttf_textRect.x = ttf.offX+x*ttf.width;
				ttf_bgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].red:rgbColors[colorBG].red;
				ttf_bgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].green:rgbColors[colorBG].green;
				ttf_bgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue;
				ttf_fgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].red:rgbColors[colorFG].red;
				ttf_fgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].green:rgbColors[colorFG].green;
				ttf_fgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue;

                if (newAC[x].unicode) {
                    dw = newAC[x].doublewide;
                    unimap[x-x1] = newAC[x].chr;
                    curAC[x] = newAC[x];
                    x++;

                    if (dw) {
                        curAC[x] = newAC[x];
                        x++;
                    }
                }
                else {
                    uint8_t ascii = newAC[x].chr&255;

                    curAC[x] = newAC[x];
                    if (ascii > 175 && ascii < 179 && !IS_PC98_ARCH) {	// special: shade characters 176-178 unless PC-98
                        ttf_bgColor.b = (ttf_bgColor.b*(179-ascii) + ttf_fgColor.b*(ascii-175))>>2;
                        ttf_bgColor.g = (ttf_bgColor.g*(179-ascii) + ttf_fgColor.g*(ascii-175))>>2;
                        ttf_bgColor.r = (ttf_bgColor.r*(179-ascii) + ttf_fgColor.r*(ascii-175))>>2;
                        unimap[x-x1] = ' ';							// shaded space
                    } else {
                        unimap[x-x1] = cpMap[ascii+charSet*256];
                    }

                    x++;
                }

                {
                    unimap[x-x1] = 0;
                    xmax = max(x-1, xmax);

                    SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
                    ttf_textClip.w = (x-x1)*ttf.width;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
                    x--;
                }
			}
		}
		curAC += ttf.cols;
		newAC += ttf.cols;
	}
    if (!force) justChanged = false;
    // NTS: Additional fix is needed for the cursor in PC-98 mode; also expect further cleanup
	bcount++;
	if (vga.draw.cursor.enabled && vga.draw.cursor.sline <= vga.draw.cursor.eline && vga.draw.cursor.sline < 16) {		// Draw cursor?
		int newPos = vga.draw.cursor.address>>1;
		if (newPos >= 0 && newPos < ttf.cols*ttf.lins) {								// If on screen
			int y = newPos/ttf.cols;
			int x = newPos%ttf.cols;
			vga.draw.cursor.count++;

			if (IS_PC98_ARCH) /* default cursor blinking is slower on PC-98 systems */
				vga.draw.cursor.blinkon = (vga.draw.cursor.count & 64) ? true : false;
			else
				vga.draw.cursor.blinkon = (vga.draw.cursor.count & 16) ? true : false;

			if (ttf.cursor != newPos || vga.draw.cursor.sline != prev_sline || ((blinkstate != vga.draw.cursor.blinkon) && blinkCursor)) {				// If new position or shape changed, forse draw
				if (blinkCursor && blinkstate == vga.draw.cursor.blinkon) {
					vga.draw.cursor.count = 4;
					vga.draw.cursor.blinkon = true;
				}
				prev_sline = vga.draw.cursor.sline;
				xmin = min(x, xmin);
				xmax = max(x, xmax);
				ymin = min(y, ymin);
				ymax = max(y, ymax);
			}
			blinkstate = vga.draw.cursor.blinkon;
			ttf.cursor = newPos;
//			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax  && (GetTickCount()&0x400))	// If overdrawn previuosly (or new shape)
			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax && !newAttrChar[ttf.cursor].skipped) {							// If overdrawn previuosly (or new shape)
				uint8_t colorBG = newAttrChar[ttf.cursor].bg;
				uint8_t colorFG = newAttrChar[ttf.cursor].fg;
				processWP(&colorBG, &colorFG);

				/* Don't do this in PC-98 mode, the bright pink cursor in EDIT.COM looks awful and not at all how the cursor is supposed to look --J.C. */
				if (!IS_PC98_ARCH && blinking && colorBG&8) {
					colorBG-=8;
					if ((bcount/8)%2)
						colorFG=colorBG;
				}
				bool dw = newAttrChar[ttf.cursor].unicode && newAttrChar[ttf.cursor].doublewide;
				ttf_bgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].red:rgbColors[colorBG].red;
				ttf_bgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].green:rgbColors[colorBG].green;
				ttf_bgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue;
				ttf_fgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].red:rgbColors[colorFG].red;
				ttf_fgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].green:rgbColors[colorFG].green;
				ttf_fgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue;
				unimap[0] = newAttrChar[ttf.cursor].unicode?newAttrChar[ttf.cursor].chr:cpMap[newAttrChar[ttf.cursor].chr&255];
                if (dw) {
                    unimap[1] = newAttrChar[ttf.cursor].chr;
                    unimap[2] = 0;
                    xmax = max(x+1, xmax);
                } else
                    unimap[1] = 0;
				// first redraw character
				SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
				ttf_textClip.w = ttf.width*(dw?2:1);
				ttf_textRect.x = ttf.offX+x*ttf.width;
				ttf_textRect.y = ttf.offY+y*ttf.height;
				SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
				SDL_FreeSurface(textSurface);
				if ((vga.draw.cursor.blinkon || !blinkCursor)) {
                    // second reverse lower lines
                    textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_bgColor, ttf_fgColor, ttf.width*(dw?2:1));
                    ttf_textClip.y = (ttf.height*vga.draw.cursor.sline)>>4;
                    ttf_textClip.h = ttf.height - ttf_textClip.y;								// for now, cursor to bottom
                    ttf_textRect.y = ttf.offY+y*ttf.height + ttf_textClip.y;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
				}
			}
		}
	}
	if (xmin <= xmax) {												// if any changes
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = ttf.offX+xmin*ttf.width; rect->y = ttf.offY+ymin*ttf.height; rect->w = (xmax-xmin+1)*ttf.width; rect->h = (ymax-ymin+1)*ttf.height;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
    }
}
#endif

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
    bool actually_updating = sdl.updating;
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

#if C_DIRECT3D
        case SCREEN_DIRECT3D:
            OUTPUT_DIRECT3D_EndUpdate(changedLines);
            break;
#endif

        default:
            break;
    }

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
            E_Exit("SDL:Can't set palette");
        }
    } else {
        if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,(int)start,(int)count)) {
            E_Exit("SDL:Can't set palette");
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

void GFX_Start() {
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
/* Linux use group as dosbox-x has mulitple threads under linux */
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

extern uint8_t int10_font_14[256 * 14];
static void OutputString(Bitu x,Bitu y,const char * text,uint32_t color,uint32_t color2,SDL_Surface * output_surface) {
    uint32_t * draw=(uint32_t*)(((uint8_t *)output_surface->pixels)+((y)*output_surface->pitch))+x;
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
			char msg[]="[2J";
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

#if defined(USE_TTF)

static const uint16_t cpMap_PC98[256] = {
//  0       1       2       3       4       5       6       7
    0x0020, 0x2401, 0x2402, 0x2403, 0x2404, 0x2405, 0x2406, 0x2407,                 // 0x00-0x07   Except NUL, control chars have small letters spelling out the ASCII control code
    0x2408, 0x2409, 0x240A, 0x240B, 0x240C, 0x240D, 0x240E, 0x240F,                 // 0x08-0x0F   i.e. CR is ␍. The mapping here does not EXACTLY render as it does on real hardware.
    0x2410, 0x2411, 0x2412, 0x2413, 0x2414, 0x2415, 0x2416, 0x2417,                 // 0x10-0x17
    0x2418, 0x2419, 0x241A, 0x241B, 0x2192, 0x2190, 0x2191, 0x2193,                 // 0x18-0x1F   The last 4 are single-wide arrows
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,                 // 0x20-0x27
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,                 // 0x28-0x2F
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,                 // 0x30-0x37
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,                 // 0x38-0x3F
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,                 // 0x40-0x47
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,                 // 0x48-0x4F
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,                 // 0x50-0x57
    0x0058, 0x0059, 0x005A, 0x005B, 0x00A5, 0x005D, 0x005E, 0x005F,                 // 0x58-0x5F   0x5C = Yen
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,                 // 0x60-0x67
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,                 // 0x68-0x6F
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,                 // 0x70-0x77
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0020,                 // 0x78-0x7F   0x7F = Does not display as anything
    0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588,                 // 0x80-0x87   Block characters, vertical 1/8 to 8/8
    0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589, 0x253C,                 // 0x88-0x8F   Block characters, horizontal 1/8 to 7/8
    0x2534, 0x252C, 0x2524, 0x251C, 0x0020, 0x2500, 0x2502, 0x2595,                 // 0x90-0x97
    0x250C, 0x2510, 0x2514, 0x2518, 0x256D, 0x256E, 0x2570, 0x256F,                 // 0x98-0x9F
    0x0020, 0xFF61, 0xFF62, 0xFF63, 0xFF64, 0xFF65, 0xFF66, 0xFF67,                 // 0xA0-0xA7   0xA0 = Nothing. 0xA1-0xDF = Single-wide Katakana
    0xFF68, 0xFF69, 0xFF6A, 0xFF6B, 0xFF6C, 0xFF6D, 0xFF6E, 0xFF6F,                 // 0xA8-0xAF
    0xFF70, 0xFF71, 0xFF72, 0xFF73, 0xFF74, 0xFF75, 0xFF76, 0xFF77,                 // 0xB0-0xB7
    0xFF78, 0xFF79, 0xFF7A, 0xFF7B, 0xFF7C, 0xFF7D, 0xFF7E, 0xFF7F,                 // 0xB8-0xBF
    0xFF80, 0xFF81, 0xFF82, 0xFF83, 0xFF84, 0xFF85, 0xFF86, 0xFF87,                 // 0xC0-0xC7
    0xFF88, 0xFF89, 0xFF8A, 0xFF8B, 0xFF8C, 0xFF8D, 0xFF8E, 0xFF8F,                 // 0xC8-0xCF
    0xFF90, 0xFF91, 0xFF92, 0xFF93, 0xFF94, 0xFF95, 0xFF96, 0xFF97,                 // 0xD0-0xD7
    0xFF98, 0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0xFF9E, 0xFF9F,                 // 0xD8-0xDF
    0x2550, 0x255E, 0x256A, 0x2561, 0x0020, 0x0020, 0x0020, 0x0020,                 // 0xE0-0xE7   FIXME: E0h-E3h is an approximation. Find Unicode approx. for E4h-E7h
    0x2660, 0x2665, 0x2666, 0x2663, 0x25CF, 0x25CB, 0x2571, 0x2572,                 // 0xE8-0xEF
    0x2573, 0x0020, 0x5E74, 0x6708, 0x65E5, 0x0020, 0x5206, 0x0020,                 // 0xF0-0xF7   FIXME: What is F1h, F5h, F7h?
    0x0020, 0x0020, 0x0020, 0x0020, 0x005C, 0x0020, 0x0020, 0x0020                  // 0xF8-0xFF   0xFC = Backslash
};

extern int eurAscii;
bool CodePageGuestToHostUint16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
void setTTFCodePage() {
    if (IS_PC98_ARCH) {
        static_assert(sizeof(cpMap[0])*256 >= sizeof(cpMap_PC98), "sizeof err 1");
        static_assert(sizeof(cpMap[0]) == sizeof(cpMap_PC98[0]), "sizeof err 2");
        memcpy(cpMap,cpMap_PC98,sizeof(cpMap[0])*256);
        return;
    }

    int cp = dos.loaded_codepage;
    if (cp) {
        LOG_MSG("Loaded system codepage: %d\n", cp);
        char text[2];
        uint16_t uname[4], wcTest[256];
        for (int i = 0; i < 256; i++) {
            text[0]=i;
            text[1]=0;
            uname[0]=0;
            uname[1]=0;
            CodePageGuestToHostUint16(uname,text);
            wcTest[i] = uname[1]==0?uname[0]:i;
        }
        uint16_t unimap;
        bool notMapped = false;
        for (int y = 8; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                unimap = wcTest[y*16+x];
                if (!TTF_GlyphIsProvided(ttf.SDL_font, unimap)) {
                    if (!notMapped) {
                        LOG_MSG("ASCII Unicode Fixed");
                        notMapped = true;
                    }
                    LOG_MSG("  %3d    %4x  %4x", y*16+x, unimap, cpMap[y*16+x]);
                } else
                    cpMap[y*16+x] = unimap;
            }
        if (eurAscii != -1 && TTF_GlyphIsProvided(ttf.SDL_font, 0x20ac))
            cpMap[eurAscii] = 0x20ac;
    }
}

void GFX_SelectFontByPoints(int ptsize) {
	bool initCP = true;
	if (ttf.SDL_font != 0) {
		TTF_CloseFont(ttf.SDL_font);
		initCP = false;
	}
	if (ttf.SDL_fontb != 0) TTF_CloseFont(ttf.SDL_fontb);
	if (ttf.SDL_fonti != 0) TTF_CloseFont(ttf.SDL_fonti);
	if (ttf.SDL_fontbi != 0) TTF_CloseFont(ttf.SDL_fontbi);
	SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFont, (int)ttfSize);
	ttf.SDL_font = TTF_OpenFontRW(rwfont, 1, ptsize);
    if (ttfSizeb>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontb, (int)ttfSizeb);
        ttf.SDL_fontb = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontb = NULL;
    if (ttfSizei>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFonti, (int)ttfSizei);
        ttf.SDL_fonti = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fonti = NULL;
    if (ttfSizebi>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontbi, (int)ttfSizebi);
        ttf.SDL_fontbi = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontbi = NULL;
	ttf.pointsize = ptsize;
	TTF_GlyphMetrics(ttf.SDL_font, 65, NULL, NULL, NULL, NULL, &ttf.width);
	ttf.height = TTF_FontAscent(ttf.SDL_font)-TTF_FontDescent(ttf.SDL_font);
	if (ttf.fullScrn) {
        int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
		ttf.offX = (maxWidth-ttf.width*ttf.cols)/2;
		ttf.offY = (maxHeight-ttf.height*ttf.lins)/2;
	}
	else
		ttf.offX = ttf.offY = 0;
	if (initCP) setTTFCodePage();
}

void resetFontSize() {
	if (ttf.inUse) {
		GFX_SelectFontByPoints(ttf.pointsize);
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
			SDL_FillRect(sdl.surface, NULL, 0);
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0; rect->y = 0; rect->w = 0; rect->h = 0;
#if defined(C_SDL2)
            SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
            SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
		}
		GFX_EndTextLines(true);
	}
}

void decreaseFontSize() {
	if (ttf.inUse && ttf.pointsize > 10) {
		GFX_SelectFontByPoints(ttf.pointsize - (ttf.DOSBox ? 2 : 1));
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
            ttf.fullScrn = false;
            resetFontSize();
        } else
            GFX_EndTextLines(true);
	}
}

void increaseFontSize() {
	if (ttf.inUse) {																	// increase fontsize
        int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(WIN32)
		if (!ttf.fullScrn) {															// 3D borders
			maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
			maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
		}
#endif
		GFX_SelectFontByPoints(ttf.pointsize + (ttf.DOSBox ? 2 : 1));
		if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {		// if it fits on screen
			GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
			wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);						// force redraw of complete window
			GFX_EndTextLines(true);
		} else
			GFX_SelectFontByPoints(ttf.pointsize - (ttf.DOSBox ? 2 : 1));
	}
}

void TTF_IncreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    increaseFontSize();
    return;
}

void TTF_DecreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    decreaseFontSize();
    return;
}
#endif

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

    sdl.desktop.fullscreen=section->Get_bool("fullscreen");
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
    if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
    sdl.mouse.autolock=false;

    const std::string feedback = section->Get_string("autolock_feedback");
    if (feedback == "none")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_NONE;
    else if (feedback == "beep")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_BEEP;
    else if (feedback == "flash")
        sdl.mouse.autolock_feedback = AUTOLOCK_FEEDBACK_FLASH;

    const char *clip_mouse_button = section->Get_string("clip_mouse_button");
    if (!strcmp(clip_mouse_button, "middle")) mbutton=2;
    else if (!strcmp(clip_mouse_button, "right")) mbutton=3;
    else if (!strcmp(clip_mouse_button, "arrows")) mbutton=4;
    else mbutton=0;
    modifier = section->Get_string("clip_key_modifier");
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

    const int display = section->Get_int("display");
    int numscreen = GetNumScreen();
    if (display >= 0 && display <= numscreen)
        sdl.displayNumber = display;
    else {
        LOG_MSG("SDL: Display number should be between 0 and %d, fallback to default display", numscreen);
        sdl.displayNumber = 0;
    }
    std::string output=section->Get_string("output");
    std::string mtype(static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_string("machine"));

    //if (output == "ttf" && (mtype == "tandy" || mtype == "pcjr"))
        //output = "default";
	if (output == "default") {
#ifdef __WIN32__
# if defined(HX_DOS)
        output ="surface"; /* HX DOS should stick to surface */
# elif defined(__MINGW32__) && !(C_DIRECT3D) && !defined(C_SDL2)
        /* NTS: OpenGL output never seems to work in VirtualBox under Windows XP */
        output = isVirtualBox ? "surface" : "opengl"; /* MinGW builds do not yet have Direct3D */
# elif C_DIRECT3D
        output = "direct3d";
# elif C_OPENGL && !defined(C_SDL2)
        output = isVirtualBox ? "surface" : "opengl";
# elif C_OPENGL
        output = "opengl";
# else
        output = "surface";
# endif
#elif defined(MACOSX) && defined(C_OPENGL) && !defined(C_SDL2)
        /* NTS: Lately, especially on Macbooks with Retina displays, OpenGL gives better performance
                than the CG bitmap-based "surface" output.

                On my dev system, "top" shows a 20% CPU load doing 1:1 CG Bitmap output to the screen
                on a High DPI setup, while ignoring High DPI and rendering through Cocoa at 2x size
                pegs the CPU core DOSBox-X is running on at 100% and emulation stutters.

                So for the best experience, default to OpenGL.

                Note that "surface" yields good performance with SDL2 and OS X because SDL2 doesn't
                use the CGBitmap system, it uses OpenGL or Metal underneath automatically. */
        output = "opengl";
#else
        output = "surface";
#endif
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

    /* Setup Mouse correctly if fullscreen */
    if(sdl.desktop.fullscreen) GFX_CaptureMouse();

#if C_XBRZ
    // initialize xBRZ parameters and check output type for compatibility
    xBRZ_Initialize();

    if (sdl_xbrz.enable) {
        // xBRZ requirements
        if ((output != "surface") && (output != "direct3d") && (output != "opengl") && (output != "openglhq") && (output != "openglnb"))
            output = "surface";
    }
#endif

    // output type selection
    // "overlay" was removed, pre-map to Direct3D or OpenGL or surface
    if (output == "overlay") 
    {
#if C_DIRECT3D
        output = "direct3d";
#elif C_OPENGL
        output = "opengl";
#else
        output = "surface";
#endif
    }

    if (output == "surface") 
    {
        OUTPUT_SURFACE_Select();
    } 
    else if (output == "ddraw") 
    {
        OUTPUT_SURFACE_Select();
#if C_OPENGL
    } 
    else if (output == "opengl" || output == "openglhq") 
    {
        OUTPUT_OPENGL_Select();
        sdl_opengl.bilinear = true;
    }
    else if (output == "openglnb") 
    {
        OUTPUT_OPENGL_Select();
        sdl_opengl.bilinear = false;
#endif
#if C_DIRECT3D
    } 
    else if (output == "direct3d") 
    {
        OUTPUT_DIRECT3D_Select();
#if LOG_D3D
        LOG_MSG("SDL:Direct3D activated");
#endif
#endif
    }
#if defined(USE_TTF)
    else if (output == "ttf")
    {
        OUTPUT_TTF_Select(0);
    }
#endif
    else 
    {
        LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output.c_str());
        OUTPUT_SURFACE_Select(); // should not reach there anymore
    }

    sdl.overscan_width=(unsigned int)section->Get_int("overscan");
//  sdl.overscan_color=section->Get_int("overscancolor");

    // Getting window position (if configured)
    posx = -1;
    posy = -1;
    const char* windowposition = section->Get_string("windowposition");
    LOG_MSG("Configured windowposition: %s", windowposition);
    if (windowposition && *windowposition) {
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
        curscr=0;
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
        safe_strncpy(pos, "SDL_VIDEO_WINDOW_POS=", sizeof(pos));
        safe_strcat(pos, (std::to_string(posx)+","+std::to_string(posy)).c_str());
        SDL_putenv(pos);
#if defined(WIN32) && !defined(HX_DOS)
    } else if (sdl.displayNumber>0) {
        safe_strncpy(pos, "SDL_VIDEO_WINDOW_POS=", sizeof(pos));
        safe_strcat(pos, (std::to_string(info.rcMonitor.left+200)+","+std::to_string(info.rcMonitor.top+200)).c_str());
        SDL_putenv(pos);
#endif
    } else
        putenv("SDL_VIDEO_CENTERED=center");
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
    LOG_MSG("SDL:Current window pixel format: %s", SDL_GetPixelFormatName(sdl.desktop.pixelFormat));
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
        LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
#endif

    GFX_LogSDLState();
    GFX_Stop();


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

    /* Get some Event handlers */
    MAPPER_AddHandler(ResetSystem, MK_r, MMODHOST, "reset", "Reset DOSBox-X", &item); /* Host+R (Host+CTRL+R acts funny on my Linux system) */
    item->set_text("Reset virtual machine");

    MAPPER_AddHandler(RebootGuest, MK_b, MMODHOST, "reboot", "Reboot guest system", &item); /* Reboot guest system or integrated DOS */
    item->set_text("Reboot guest system");

#if !defined(HX_DOS)
    MAPPER_AddHandler(LoadMapFile, MK_nothing, 0, "loadmap", "Load mapper file", &item);
    item->set_text("Load mapper file...");

    MAPPER_AddHandler(QuickLaunch, MK_q, MMODHOST, "quickrun", "Quick launch program", &item);
    item->set_text("Quick launch program...");
#endif

#if !defined(C_EMSCRIPTEN)//FIXME: Shutdown causes problems with Emscripten
    MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","Quit from DOSBox-X", &item); /* KEEP: Most DOSBox-X users may have muscle memory for this */
    item->set_text("Quit");
#endif

    MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Capture mouse", &item); /* KEEP: Most DOSBox-X users may have muscle memory for this */
    item->set_text("Capture mouse");

    MAPPER_AddHandler(SwitchFullScreen,MK_f,MMODHOST,"fullscr","Toggle fullscreen", &item);
    item->set_text("Toggle fullscreen");

#if defined(C_SDL2) || defined(WIN32)
    MAPPER_AddHandler(QuickEdit,MK_nothing, 0,"fastedit", "Quick edit mode", &item);
    item->set_text("Quick edit: copy on select and paste text");

    MAPPER_AddHandler(CopyAllClipboard,MK_f5,MMOD1,"copyall", "Copy to clipboard", &item);
    item->set_text("Copy all text on the DOS screen");
#endif

#if defined(C_SDL2) || defined(WIN32) || defined(MACOSX) || defined(LINUX) && C_X11
    MAPPER_AddHandler(PasteClipboard,MK_f6,MMOD1,"paste", "Paste from clipboard", &item); //end emendelson; improved by Wengier
    item->set_text("Pasting from the clipboard");

    MAPPER_AddHandler(PasteClipStop,MK_nothing, 0,"pasteend", "Stop clipboard paste", &item);
    item->set_text("Stop clipboard pasting");
#endif

    MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMODHOST, "pause", "Pause emulation");

#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
    pause_menu_item_tag = mainMenu.get_item("mapper_pause").get_master_id() + DOSBoxMenu::nsMenuMinimumID;
#endif

    MAPPER_AddHandler(&GUI_Run, MK_c,MMODHOST, "gui", "Configuration tool", &item);
    item->set_text("Configuration tool");

    MAPPER_AddHandler(&GUI_ResetResize, MK_backspace, MMODHOST, "resetsize", "Reset window size", &item);
    item->set_text("Reset window size");

#if defined(USE_TTF)
    MAPPER_AddHandler(&TTF_IncreaseSize, MK_uparrow, MMODHOST, "ttf_incsize", "Increase TTF size", &item);
    item->set_text("Increase TTF font size");

    MAPPER_AddHandler(&TTF_DecreaseSize, MK_downarrow, MMODHOST, "ttf_decsize", "Decrease TTF size", &item);
    item->set_text("Decrease TTF font size");
#endif

    UpdateWindowDimensions();
}

void Mouse_AutoLock(bool enable) {
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

    /* don't act if 3Dfx OpenGL emulation is active */
    if (GFX_GetPreventFullscreen()) return;

    SDL_ResizeEvent* ResizeEvent = (SDL_ResizeEvent*)event;

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
DOSBoxMenu::item_handle_t DOSBoxMenu::displaylist::itemFromPoint(DOSBoxMenu &menu,int x,int y) {
    for (auto &id : disp_list) {
        DOSBoxMenu::item &item = menu.get_item(id);
        if (x >= item.screenBox.x && y >= item.screenBox.y) {
            int sx = x - item.screenBox.x;
            int sy = y - item.screenBox.y;
            int adj = (this != &menu.display_list && item.get_type() == DOSBoxMenu::submenu_type_id) ? 2 : 0;
            if (sx < (item.screenBox.w+adj) && sy < item.screenBox.h)
                return id;
        }
    }

    return unassigned_item_handle;
}

void DOSBoxMenu::item::updateScreenFromItem(DOSBoxMenu &menu) {
    (void)menu;//UNUSED
    if (!OpenGL_using()) {
        SDL_Rect uprect = screenBox;

        SDL_rect_cliptoscreen(uprect);

#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, &uprect, 1);
#else
        SDL_UpdateRects( sdl.surface, 1, &uprect );
#endif
    }
}

void DOSBoxMenu::item::updateScreenFromPopup(DOSBoxMenu &menu) {
    (void)menu;//UNUSED
    if (!OpenGL_using()) {
        SDL_Rect uprect = popupBox;

        uprect.w += DOSBoxMenu::dropshadowX;
        uprect.h += DOSBoxMenu::dropshadowY;
        SDL_rect_cliptoscreen(uprect);

#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, &uprect, 1);
#else
        SDL_UpdateRects( sdl.surface, 1, &uprect );
#endif
    }
}

void DOSBoxMenu::item::drawBackground(DOSBoxMenu &menu) {
    (void)menu;//UNUSED
    Bitu bordercolor = GFX_GetRGB(31, 31, 31);
    Bitu bgcolor = GFX_GetRGB(63, 63, 63);

    if (popupBox.w <= 1 || popupBox.h <= 1)
        return;

    MenuDrawRect(popupBox.x, popupBox.y, popupBox.w, popupBox.h, bgcolor);

    if (borderTop)
        MenuDrawRect(popupBox.x, popupBox.y, popupBox.w, 1, bordercolor);

    MenuDrawRect(popupBox.x, popupBox.y + popupBox.h - 1, popupBox.w, 1, bordercolor);

    MenuDrawRect(popupBox.x, popupBox.y, 1, popupBox.h, bordercolor);
    MenuDrawRect(popupBox.x + popupBox.w - 1, popupBox.y, 1, popupBox.h, bordercolor);

    if (type == DOSBoxMenu::submenu_type_id) {
        MenuShadeRect((int)popupBox.x + (int)popupBox.w, (int)popupBox.y + (int)DOSBoxMenu::dropshadowY,
                      (int)DOSBoxMenu::dropshadowX, (int)popupBox.h);
        MenuShadeRect((int)popupBox.x + (int)DOSBoxMenu::dropshadowX, (int)popupBox.y + (int)popupBox.h,
                      (int)popupBox.w - (int)DOSBoxMenu::dropshadowX, (int)DOSBoxMenu::dropshadowY);
    }
}
#endif

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
    else if (sdl.mouse.locked || Mouse_GetButtonState() != 0)
        inputToScreen = true;
    else {
        inputToScreen = GFX_CursorInOrNearScreen(motion->x,motion->y);
#if defined(WIN32) || defined(C_SDL2)
		if (mouse_start_x >= 0 && mouse_start_y >= 0) {
			if (fx>=0 && fy>=0)
				Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), false);
			Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,motion->x-sdl.clip.x,motion->y-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), true);
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
        GFX_SDLMenuTrackHover(mainMenu,mainMenu.display_list.itemFromPoint(mainMenu,motion->x,motion->y));
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
        GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);

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
        if (!sdl.mouse.locked)
#else
        /* SDL1 has some sort of weird mouse warping bug in fullscreen mode no matter whether the mouse is captured or not (Windows, Linux/X11) */
        if (!sdl.mouse.locked && !sdl.desktop.fullscreen)
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
        if (!sdl.mouse.locked)
#else
        /* SDL1 has some sort of weird mouse warping bug in fullscreen mode no matter whether the mouse is captured or not (Windows, Linux/X11) */
        if (!sdl.mouse.locked && !sdl.desktop.fullscreen)
#endif
            SDL_ShowCursor(((!inside) || ((MOUSE_IsHidden()) && !(MOUSE_IsBeingPolled() || MOUSE_HasInterruptSub()))) ? SDL_ENABLE : SDL_DISABLE);
    }
    Mouse_CursorMoved(xrel, yrel, x, y, emu);
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

#if defined(WIN32) || defined(C_SDL2)
bool isModifierApplied() {
    return direct_mouse_clipboard || !strcmp(modifier,"none") ||
    ((!strcmp(modifier,"ctrl") || !strcmp(modifier,"lctrl")) && sdl.lctrlstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrl") || !strcmp(modifier,"rctrl")) && sdl.rctrlstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"alt") || !strcmp(modifier,"lalt")) && sdl.laltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"alt") || !strcmp(modifier,"ralt")) && sdl.raltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"shift") || !strcmp(modifier,"lshift")) && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"shift") || !strcmp(modifier,"rshift")) && sdl.rshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlalt") || !strcmp(modifier,"lctrlalt")) && sdl.lctrlstate==SDL_KEYDOWN && sdl.laltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlalt") || !strcmp(modifier,"rctrlalt")) && sdl.rctrlstate==SDL_KEYDOWN && sdl.raltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlshift") || !strcmp(modifier,"lctrlshift")) && sdl.lctrlstate==SDL_KEYDOWN && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlshift") || !strcmp(modifier,"rctrlshift")) && sdl.rctrlstate==SDL_KEYDOWN && sdl.rshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"altshift") || !strcmp(modifier,"laltshift")) && sdl.laltstate==SDL_KEYDOWN && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"altshift") || !strcmp(modifier,"raltshift")) && sdl.raltstate==SDL_KEYDOWN && sdl.rshiftstate==SDL_KEYDOWN);
}

void ClipKeySelect(int sym) {
    if (sym==SDLK_ESCAPE) {
        if (mbutton==4 && selsrow>-1 && selscol>-1) {
            Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
            selmark = false;
            selsrow = selscol = selerow = selecol = -1;
        } else if (mouse_start_x >= 0 && mouse_start_y >= 0 && fx >= 0 && fy >= 0) {
            Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), false);
            mouse_start_x = mouse_start_y = -1;
            mouse_end_x = mouse_end_y = -1;
            fx = fy = -1;
        }
        return;
    }
    if (mbutton!=4 || (CurMode->type!=M_TEXT && !IS_PC98_ARCH)) return;
    if (sym==SDLK_HOME) {
        if (selsrow==-1 || selscol==-1) {
            int p=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
            selscol = CURSOR_POS_COL(p);
            selsrow = CURSOR_POS_ROW(p);
        } else {
            if (selsrow>-1 && selscol>-1) Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
            if (selmark) {
                selscol = selecol;
                selsrow = selerow;
            }
        }
        selmark = true;
        selecol = selscol;
        selerow = selsrow;
        Mouse_Select(selscol, selsrow, selecol, selerow, -1, -1, true);
    } else if (sym==SDLK_END) {
        if (selmark) {
            if (selsrow>-1 && selscol>-1 && selerow > -1 && selecol > -1) {
                Mouse_Select(selscol, selsrow, selecol, selerow, -1, -1, false);
                CopyClipboard(1);
            }
            selmark = false;
            selsrow = selscol = selerow = selecol = -1;
        }
    } else if (sym==SDLK_LEFT || sym==SDLK_RIGHT || sym==SDLK_UP || sym==SDLK_DOWN) {
        if (selsrow==-1 || selscol==-1) {
            int p=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
            selscol = CURSOR_POS_COL(p);
            selsrow = CURSOR_POS_ROW(p);
            selerow = selecol = -1;
            selmark = false;
        } else
            Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
        if (sym==SDLK_LEFT && (selmark?selecol:selscol)>0) (selmark?selecol:selscol)--;
        else if (sym==SDLK_RIGHT && (selmark?selecol:selscol)<(IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS))-1) (selmark?selecol:selscol)++;
        else if (sym==SDLK_UP && (selmark?selerow:selsrow)>0) (selmark?selerow:selsrow)--;
        else if (sym==SDLK_DOWN && (selmark?selerow:selsrow)<(IS_PC98_ARCH?(real_readb(0x60,0x113)&0x01?25:20)-1:real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS))) (selmark?selerow:selsrow)++;
        Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, true);
    }
}
#endif

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
#if defined(WIN32) || defined(C_SDL2)
		if (!sdl.mouse.locked && button->button == SDL_BUTTON_LEFT && isModifierApplied()) {
            if (mbutton==4 && selsrow>-1 && selscol>-1) {
                Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
                selmark = false;
                selsrow = selscol = selerow = selecol = -1;
            } else if (mouse_start_x >= 0 && mouse_start_y >= 0 && fx >= 0 && fy >= 0) {
                Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), false);
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
        if (sdl.mouse.requestlock && !sdl.mouse.locked && mouse_notify_mode == 0) {
            CaptureMouseNotify();
            GFX_CaptureMouse();
            // Don't pass click to mouse handler
            break;
        }
        if (!sdl.mouse.autoenable && sdl.mouse.autolock && mouse_notify_mode == 0 && button->button == SDL_BUTTON_MIDDLE) {
            GFX_CaptureMouse();
            break;
        }
        switch (button->button) {
        case SDL_BUTTON_LEFT:
            Mouse_ButtonPressed(0);
            break;
        case SDL_BUTTON_RIGHT:
            Mouse_ButtonPressed(1);
            break;
        case SDL_BUTTON_MIDDLE:
            Mouse_ButtonPressed(2);
            break;
#if !defined(C_SDL2)
        case SDL_BUTTON_WHEELUP: /* Ick, really SDL? */
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
#if defined(WIN32) && !defined(HX_DOS)
				INPUT ip = {0};
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = wheel_key==2?75:(wheel_key==3?73:72);
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
#else
                KEYBOARD_AddKey(wheel_key==2?KBD_left:(wheel_key==3?KBD_pageup:KBD_up), true);
                KEYBOARD_AddKey(wheel_key==2?KBD_left:(wheel_key==3?KBD_pageup:KBD_up), false);
#endif
			} else
				Mouse_ButtonPressed(100-1);
			break;
        case SDL_BUTTON_WHEELDOWN: /* Ick, really SDL? */
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
#if defined(WIN32) && !defined(HX_DOS)
				INPUT ip = {0};
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = wheel_key==2?77:(wheel_key==3?81:80);
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
#else
                KEYBOARD_AddKey(wheel_key==2?KBD_right:(wheel_key==3?KBD_pagedown:KBD_down), true);
                KEYBOARD_AddKey(wheel_key==2?KBD_right:(wheel_key==3?KBD_pagedown:KBD_down), false);
#endif
			} else
				Mouse_ButtonPressed(100+1);
            break;
#endif
        }
        break;
    case SDL_RELEASED:
#if defined(WIN32) || defined(C_SDL2)
		if (!sdl.mouse.locked && ((mbutton==2 && button->button == SDL_BUTTON_MIDDLE) || (mbutton==3 && button->button == SDL_BUTTON_RIGHT)) && mouse_start_x >= 0 && mouse_start_y >= 0) {
			mouse_end_x=motion->x;
			mouse_end_y=motion->y;
			if (mouse_start_x == mouse_end_x && mouse_start_y == mouse_end_y)
				PasteClipboard(true);
			else {
				if (fx >= 0 && fy >= 0)
					Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), false);
				if (abs(mouse_end_x - mouse_start_x) + abs(mouse_end_y - mouse_start_y)<5) {
					PasteClipboard(true);
				} else
					CopyClipboard(0);
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
            break;
        case SDL_BUTTON_RIGHT:
            Mouse_ButtonReleased(1);
            break;
        case SDL_BUTTON_MIDDLE:
            Mouse_ButtonReleased(2);
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

static bool PasteClipboardNext(); // added emendelson from dbDOS; improved by Wengier

bool GFX_IsFullscreen(void) {
    return sdl.desktop.fullscreen;
}

bool sdl_wait_on_error() {
    return sdl.wait_on_error;
}

void* GetSetSDLValue(int isget, std::string& target, void* setval) {
    if (target == "wait_on_error") {
        if (isget) return (void*) sdl.wait_on_error;
        else sdl.wait_on_error = setval;
    }
    else if (target == "opengl.bilinear") {
#if C_OPENGL
        if (isget) return (void*) sdl_opengl.bilinear;
        else sdl_opengl.bilinear = setval;
#else
        if (isget) return (void*) 0;
#endif
/*
    } else if (target == "draw.callback") {
        if (isget) return (void*) sdl.draw.callback;
        else sdl.draw.callback = *static_cast<GFX_CallBack_t*>(setval);
    } else if (target == "desktop.full.width") {
        if (isget) return (void*) sdl.desktop.full.width;
        else sdl.desktop.full.width = *static_cast<uint16_t*>(setval);
    } else if (target == "desktop.full.height") {
        if (isget) return (void*) sdl.desktop.full.height;
        else sdl.desktop.full.height = *static_cast<uint16_t*>(setval);
    } else if (target == "desktop.full.fixed") {
        if (isget) return (void*) sdl.desktop.full.fixed;
        else sdl.desktop.full.fixed = setval;
    } else if (target == "desktop.window.width") {
        if (isget) return (void*) sdl.desktop.window.width;
        else sdl.desktop.window.width = *static_cast<uint16_t*>(setval);
    } else if (target == "desktop.window.height") {
        if (isget) return (void*) sdl.desktop.window.height;
        else sdl.desktop.window.height = *static_cast<uint16_t*>(setval);
*/
    } else if (target == "desktop.fullscreen") {
        if (isget) return (void*) sdl.desktop.fullscreen;
        else sdl.desktop.fullscreen = setval;
    } else if (target == "desktop.doublebuf") {
        if (isget) return (void*) sdl.desktop.doublebuf;
        else sdl.desktop.doublebuf = setval;
/*
    } else if (target == "desktop.type") {
        if (isget) return (void*) sdl.desktop.type;
        else sdl.desktop.type = *static_cast<SCREEN_TYPES*>(setval);
*/
    } else if (target == "desktop.want_type") {
        if (isget) return (void*) sdl.desktop.want_type;
        else sdl.desktop.want_type = *static_cast<SCREEN_TYPES*>(setval);
/*
    } else if (target == "surface") {
        if (isget) return (void*) sdl.surface;
        else sdl.surface = static_cast<SDL_Surface*>(setval);
    } else if (target == "overlay") {
        if (isget) return (void*) sdl.overlay;
        else sdl.overlay = static_cast<SDL_Overlay*>(setval);
*/
    } else if (target == "mouse.autoenable") {
        if (isget) return (void*) sdl.mouse.autoenable;
        else sdl.mouse.autoenable = setval;
/*
    } else if (target == "overscan_width") {
        if (isget) return (void*) sdl.overscan_width;
        else sdl.overscan_width = *static_cast<Bitu*>(setval);
*/
#if defined (WIN32)
    } else if (target == "using_windib") {
        if (isget) return (void*) sdl.using_windib;
        else sdl.using_windib = setval;
#endif
    }

    return NULL;
}

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

/* DOSBox SVN revision 4176:4177: For Linux/X11, Xorg 1.20.1
 * will make spurious focus gain and loss events when locking the mouse in windowed mode.
 *
 * This has not been tested with DOSBox-X yet becaus I do not run Xorg 1.20.1, yet */
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

    GFX_EventsMouse();

#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
#if defined(WIN32) && !defined(HX_DOS)
        case SDL_SYSWMEVENT:
            switch( event.syswm.msg->msg.win.msg ) {
                case WM_COMMAND:
                    MSG_WM_COMMAND_handle(/*&*/(*event.syswm.msg));
                    break;
                case WM_SYSCOMMAND:
                    switch (event.syswm.msg->msg.win.wParam) {
                        case ID_WIN_SYSMENU_TOGGLEMENU:
                            /* prevent removing the menu in 3Dfx mode */
                            if (!GFX_GetPreventFullscreen())
                            {
                                void ToggleMenu(bool pressed);
                                ToggleMenu(true);
                                mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
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
                continue;
            case SDL_WINDOWEVENT_RESIZED:
                GFX_HandleVideoResize(event.window.data1, event.window.data2);
                continue;
            case SDL_WINDOWEVENT_EXPOSED:
                if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
                continue;
            case SDL_WINDOWEVENT_LEAVE:
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                void GFX_SDLMenuTrackHover(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);
                void GFX_SDLMenuTrackHilight(DOSBoxMenu &menu,DOSBoxMenu::item_handle_t item_id);

                GFX_SDLMenuTrackHover(mainMenu,DOSBoxMenu::unassigned_item_handle);
                GFX_SDLMenuTrackHilight(mainMenu,DOSBoxMenu::unassigned_item_handle);

                GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (IsFullscreen() && !sdl.mouse.locked)
                    GFX_CaptureMouse();
                SetPriority(sdl.priority.focus);
                CPU_Disable_SkipAutoAdjust();
#if defined(USE_TTF)
                resetFontSize();
#endif
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
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
                CPU_Enable_SkipAutoAdjust();
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
#if !defined(IGNORE_TOUCHSCREEN)
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            HandleTouchscreenFinger(&event.tfinger);
            break;
#endif
        case SDL_QUIT:
            if (CheckQuit()) throw(0);
            break;
		case SDL_MOUSEWHEEL:
			if (wheel_key && (wheel_guest || !dos_kernel_disabled)) {
				if(event.wheel.y > 0) {
#if defined (WIN32) && !defined(HX_DOS)
					INPUT ip = {0};
					ip.type = INPUT_KEYBOARD;
					ip.ki.wScan = wheel_key==2?75:(wheel_key==3?73:72);
					ip.ki.time = 0;
					ip.ki.dwExtraInfo = 0;
					ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
					SendInput(1, &ip, sizeof(INPUT));
					ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
					SendInput(1, &ip, sizeof(INPUT));
#else
                    KEYBOARD_AddKey(wheel_key==2?KBD_left:(wheel_key==3?KBD_pageup:KBD_up), true);
                    KEYBOARD_AddKey(wheel_key==2?KBD_left:(wheel_key==3?KBD_pageup:KBD_up), false);
#endif
				} else if(event.wheel.y < 0) {
#if defined (WIN32) && !defined(HX_DOS)
					INPUT ip = {0};
					ip.type = INPUT_KEYBOARD;
					ip.ki.wScan = wheel_key==2?77:(wheel_key==3?81:80);
					ip.ki.time = 0;
					ip.ki.dwExtraInfo = 0;
					ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
					SendInput(1, &ip, sizeof(INPUT));
					ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
					SendInput(1, &ip, sizeof(INPUT));
#else
                    KEYBOARD_AddKey(wheel_key==2?KBD_right:(wheel_key==3?KBD_pagedown:KBD_down), true);
                    KEYBOARD_AddKey(wheel_key==2?KBD_right:(wheel_key==3?KBD_pagedown:KBD_down), false);
#endif
				}
			}
			break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
#if defined (WIN32) || defined(C_SDL2)
            if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LCTRL) sdl.lctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RCTRL) sdl.rctrlstate = event.key.type;
            if (event.key.keysym.sym==SDLK_LSHIFT) sdl.lshiftstate = event.key.type;
            if (event.key.keysym.sym==SDLK_RSHIFT) sdl.rshiftstate = event.key.type;
            if (event.type == SDL_KEYDOWN && isModifierApplied())
                ClipKeySelect(event.key.keysym.sym);
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
            gfx_in_mapper = true;
            void MAPPER_CheckEvent(SDL_Event * event);
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

    GFX_EventsMouse();

#if C_EMSCRIPTEN
    emscripten_sleep(0);
#endif

    while (SDL_PollEvent(&event)) {
        /* DOSBox SVN revision 4176:4177: For Linux/X11, Xorg 1.20.1
         * will make spurious focus gain and loss events when locking the mouse in windowed mode.
         *
         * This has not been tested with DOSBox-X yet becaus I do not run Xorg 1.20.1, yet */
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
                        CPU_Enable_SkipAutoAdjust();
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
#if defined(WIN32)
            if (event.type == SDL_KEYDOWN && isModifierApplied())
                ClipKeySelect(event.key.keysym.sym);
            if (((event.key.keysym.sym==SDLK_TAB)) &&
                ((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN))) { MAPPER_LosingFocus(); break; }
            // This can happen as well.
            if (((event.key.keysym.sym == SDLK_TAB )) && (event.key.keysym.mod & KMOD_ALT)) break;
            // ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
            if ((event.key.keysym.sym == SDLK_TAB) && (GetTicks() - sdl.focus_ticks < 2)) break;
#endif
#if defined (MACOSX)            
            /* On macs CMD-Q is the default key to close an application */
            if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
                KillSwitch(true);
                break;
            } 
#endif
        default:
            void MAPPER_CheckEvent(SDL_Event * event);
            MAPPER_CheckEvent(&event);
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
    if (paste_speed && (iPasteTicker++ % paste_speed) == 0) // emendelson: was %2, %20 is good for WP51
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

    Pstring = sdl_sec->Add_string("windowposition", Property::Changeable::OnlyAtStart, "");
    Pstring->Set_help("Set the window position at startup in the positionX,positionY format (e.g.: 1300,200)");
    Pstring->SetBasic(true);

    const char* outputs[] = {
        "default", "surface", "overlay",
#if C_OPENGL
        "opengl", "openglnb", "openglhq",
#endif
        "ddraw",
#if C_DIRECT3D
        "direct3d",
#endif
#if defined(USE_TTF)
        "ttf",
#endif
        0 };

    Pint = sdl_sec->Add_int("display", Property::Changeable::OnlyAtStart, 0);
    Pint->Set_help("Specify a screen/display number to use for a multi-screen setup (0 = default).");
    Pint->SetBasic(true);

    Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "default");
    Pstring->Set_help("What video system to use for output.");
    Pstring->Set_values(outputs);
    Pstring->SetBasic(true);

    Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always, false);
    Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");
    Pbool->SetBasic(true);

    const char* feeds[] = { "none", "beep", "flash", nullptr};
    Pstring = sdl_sec->Add_string("autolock_feedback", Property::Changeable::Always, feeds[1]);
    Pstring->Set_help("Autolock status feedback type, i.e. visual, auditive, none.");
    Pstring->Set_values(feeds);

	const char* clipboardbutton[] = { "none", "middle", "right", "arrows", 0};
	Pstring = sdl_sec->Add_string("clip_mouse_button",Property::Changeable::Always, "right");
	Pstring->Set_values(clipboardbutton);
	Pstring->Set_help("Select the mouse button or use arrow keys for the shared clipboard copy/paste function.\n"
		"The default mouse button is \"right\", which means using the right mouse button to select text, copy to and paste from the host clipboard.\n"
		"Set to \"middle\" to use the middle mouse button, \"arrows\" to use arrow keys instead of a mouse button, or \"none\" to disable this feature.");
    Pstring->SetBasic(true);

	const char* clipboardmodifier[] = { "none", "ctrl", "lctrl", "rctrl", "alt", "lalt", "ralt", "shift", "lshift", "rshift", "ctrlalt", "ctrlshift", "altshift", "lctrlalt", "lctrlshift", "laltshift", "rctrlalt", "rctrlshift", "raltshift", 0};
	Pstring = sdl_sec->Add_string("clip_key_modifier",Property::Changeable::Always, "shift");
	Pstring->Set_values(clipboardmodifier);
	Pstring->Set_help("Change the keyboard modifier for the shared clipboard copy/paste function using a mouse button or arrow keys.\n"
		"The default modifier is \"shift\" (both left and right shift keys). Set to \"none\" if no modifier is desired.");
    Pstring->SetBasic(true);

    Pint = sdl_sec->Add_int("clip_paste_speed", Property::Changeable::WhenIdle, 30);
    Pint->Set_help("Set keyboard speed for pasting from the shared clipboard.\n"
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
    Pint->SetMinMax(-3,3);
    Pint->Set_help("Convert mouse wheel movements into keyboard presses such as arrow keys.\n"
        "0: disabled; 1: up/down arrows; 2: left/right arrows; 3: PgUp/PgDn keys.\n"
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

	const char* truefalseautoopt[] = { "true", "false", "auto", 0};
    Pstring = sdl_sec->Add_string("usescancodes",Property::Changeable::OnlyAtStart,"auto");
    Pstring->Set_values(truefalseautoopt);
    Pstring->Set_help("Avoid usage of symkeys, might not work on all operating systems.\n"
        "If set to \"auto\" (default), it is enabled for SDL1 and non-US keyboards.");
    Pstring->SetBasic(true);

    Pint = sdl_sec->Add_int("overscan",Property::Changeable::Always, 0);
    Pint->SetMinMax(0,10);
    Pint->Set_help("Width of overscan border (0 to 10). (works only if output=surface)");

    Pstring = sdl_sec->Add_string("titlebar", Property::Changeable::Always, "");
    Pstring->Set_help("Change the string displayed in the DOSBox-X title bar.");
    Pstring->SetBasic(true);

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

// added emendelson from dbDos; improved by Wengier
#if defined(WIN32) && !defined(C_SDL2) && !defined(__MINGW32__)
#include <cassert>

// Ripped from SDL's SDL_dx5events.c, since there's no API to access it...
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#ifndef DIK_PAUSE
#define DIK_PAUSE   0xC5
#endif
#ifndef DIK_OEM_102
#define DIK_OEM_102 0x56    /* < > | on UK/Germany keyboards */
#endif
static SDLKey aryScanCodeToSDLKey[0xFF];
static bool   bScanCodeMapInited = false;
static void PasteInitMapSCToSDLKey()
{
    /* Map the DIK scancodes to SDL keysyms */
    for (int i = 0; i<SDL_arraysize(aryScanCodeToSDLKey); ++i)
        aryScanCodeToSDLKey[i] = SDLK_UNKNOWN;

    /* Defined DIK_* constants */
    aryScanCodeToSDLKey[DIK_ESCAPE] = SDLK_ESCAPE;
    aryScanCodeToSDLKey[DIK_1] = SDLK_1;
    aryScanCodeToSDLKey[DIK_2] = SDLK_2;
    aryScanCodeToSDLKey[DIK_3] = SDLK_3;
    aryScanCodeToSDLKey[DIK_4] = SDLK_4;
    aryScanCodeToSDLKey[DIK_5] = SDLK_5;
    aryScanCodeToSDLKey[DIK_6] = SDLK_6;
    aryScanCodeToSDLKey[DIK_7] = SDLK_7;
    aryScanCodeToSDLKey[DIK_8] = SDLK_8;
    aryScanCodeToSDLKey[DIK_9] = SDLK_9;
    aryScanCodeToSDLKey[DIK_0] = SDLK_0;
    aryScanCodeToSDLKey[DIK_MINUS] = SDLK_MINUS;
    aryScanCodeToSDLKey[DIK_EQUALS] = SDLK_EQUALS;
    aryScanCodeToSDLKey[DIK_BACK] = SDLK_BACKSPACE;
    aryScanCodeToSDLKey[DIK_TAB] = SDLK_TAB;
    aryScanCodeToSDLKey[DIK_Q] = SDLK_q;
    aryScanCodeToSDLKey[DIK_W] = SDLK_w;
    aryScanCodeToSDLKey[DIK_E] = SDLK_e;
    aryScanCodeToSDLKey[DIK_R] = SDLK_r;
    aryScanCodeToSDLKey[DIK_T] = SDLK_t;
    aryScanCodeToSDLKey[DIK_Y] = SDLK_y;
    aryScanCodeToSDLKey[DIK_U] = SDLK_u;
    aryScanCodeToSDLKey[DIK_I] = SDLK_i;
    aryScanCodeToSDLKey[DIK_O] = SDLK_o;
    aryScanCodeToSDLKey[DIK_P] = SDLK_p;
    aryScanCodeToSDLKey[DIK_LBRACKET] = SDLK_LEFTBRACKET;
    aryScanCodeToSDLKey[DIK_RBRACKET] = SDLK_RIGHTBRACKET;
    aryScanCodeToSDLKey[DIK_RETURN] = SDLK_RETURN;
    aryScanCodeToSDLKey[DIK_LCONTROL] = SDLK_LCTRL;
    aryScanCodeToSDLKey[DIK_A] = SDLK_a;
    aryScanCodeToSDLKey[DIK_S] = SDLK_s;
    aryScanCodeToSDLKey[DIK_D] = SDLK_d;
    aryScanCodeToSDLKey[DIK_F] = SDLK_f;
    aryScanCodeToSDLKey[DIK_G] = SDLK_g;
    aryScanCodeToSDLKey[DIK_H] = SDLK_h;
    aryScanCodeToSDLKey[DIK_J] = SDLK_j;
    aryScanCodeToSDLKey[DIK_K] = SDLK_k;
    aryScanCodeToSDLKey[DIK_L] = SDLK_l;
    aryScanCodeToSDLKey[DIK_SEMICOLON] = SDLK_SEMICOLON;
    aryScanCodeToSDLKey[DIK_APOSTROPHE] = SDLK_QUOTE;
    aryScanCodeToSDLKey[DIK_GRAVE] = SDLK_BACKQUOTE;
    aryScanCodeToSDLKey[DIK_LSHIFT] = SDLK_LSHIFT;
    aryScanCodeToSDLKey[DIK_BACKSLASH] = SDLK_BACKSLASH;
    aryScanCodeToSDLKey[DIK_OEM_102] = SDLK_LESS;
    aryScanCodeToSDLKey[DIK_Z] = SDLK_z;
    aryScanCodeToSDLKey[DIK_X] = SDLK_x;
    aryScanCodeToSDLKey[DIK_C] = SDLK_c;
    aryScanCodeToSDLKey[DIK_V] = SDLK_v;
    aryScanCodeToSDLKey[DIK_B] = SDLK_b;
    aryScanCodeToSDLKey[DIK_N] = SDLK_n;
    aryScanCodeToSDLKey[DIK_M] = SDLK_m;
    aryScanCodeToSDLKey[DIK_COMMA] = SDLK_COMMA;
    aryScanCodeToSDLKey[DIK_PERIOD] = SDLK_PERIOD;
    aryScanCodeToSDLKey[DIK_SLASH] = SDLK_SLASH;
    aryScanCodeToSDLKey[DIK_RSHIFT] = SDLK_RSHIFT;
    aryScanCodeToSDLKey[DIK_MULTIPLY] = SDLK_KP_MULTIPLY;
    aryScanCodeToSDLKey[DIK_LMENU] = SDLK_LALT;
    aryScanCodeToSDLKey[DIK_SPACE] = SDLK_SPACE;
    aryScanCodeToSDLKey[DIK_CAPITAL] = SDLK_CAPSLOCK;
    aryScanCodeToSDLKey[DIK_F1] = SDLK_F1;
    aryScanCodeToSDLKey[DIK_F2] = SDLK_F2;
    aryScanCodeToSDLKey[DIK_F3] = SDLK_F3;
    aryScanCodeToSDLKey[DIK_F4] = SDLK_F4;
    aryScanCodeToSDLKey[DIK_F5] = SDLK_F5;
    aryScanCodeToSDLKey[DIK_F6] = SDLK_F6;
    aryScanCodeToSDLKey[DIK_F7] = SDLK_F7;
    aryScanCodeToSDLKey[DIK_F8] = SDLK_F8;
    aryScanCodeToSDLKey[DIK_F9] = SDLK_F9;
    aryScanCodeToSDLKey[DIK_F10] = SDLK_F10;
    aryScanCodeToSDLKey[DIK_NUMLOCK] = SDLK_NUMLOCK;
    aryScanCodeToSDLKey[DIK_SCROLL] = SDLK_SCROLLOCK;
    aryScanCodeToSDLKey[DIK_NUMPAD7] = SDLK_KP7;
    aryScanCodeToSDLKey[DIK_NUMPAD8] = SDLK_KP8;
    aryScanCodeToSDLKey[DIK_NUMPAD9] = SDLK_KP9;
    aryScanCodeToSDLKey[DIK_SUBTRACT] = SDLK_KP_MINUS;
    aryScanCodeToSDLKey[DIK_NUMPAD4] = SDLK_KP4;
    aryScanCodeToSDLKey[DIK_NUMPAD5] = SDLK_KP5;
    aryScanCodeToSDLKey[DIK_NUMPAD6] = SDLK_KP6;
    aryScanCodeToSDLKey[DIK_ADD] = SDLK_KP_PLUS;
    aryScanCodeToSDLKey[DIK_NUMPAD1] = SDLK_KP1;
    aryScanCodeToSDLKey[DIK_NUMPAD2] = SDLK_KP2;
    aryScanCodeToSDLKey[DIK_NUMPAD3] = SDLK_KP3;
    aryScanCodeToSDLKey[DIK_NUMPAD0] = SDLK_KP0;
    aryScanCodeToSDLKey[DIK_DECIMAL] = SDLK_KP_PERIOD;
    aryScanCodeToSDLKey[DIK_F11] = SDLK_F11;
    aryScanCodeToSDLKey[DIK_F12] = SDLK_F12;

    aryScanCodeToSDLKey[DIK_F13] = SDLK_F13;
    aryScanCodeToSDLKey[DIK_F14] = SDLK_F14;
    aryScanCodeToSDLKey[DIK_F15] = SDLK_F15;

    aryScanCodeToSDLKey[DIK_NUMPADEQUALS] = SDLK_KP_EQUALS;
    aryScanCodeToSDLKey[DIK_NUMPADENTER] = SDLK_KP_ENTER;
    aryScanCodeToSDLKey[DIK_RCONTROL] = SDLK_RCTRL;
    aryScanCodeToSDLKey[DIK_DIVIDE] = SDLK_KP_DIVIDE;
    aryScanCodeToSDLKey[DIK_SYSRQ] = SDLK_PRINT;
    aryScanCodeToSDLKey[DIK_RMENU] = SDLK_RALT;
    aryScanCodeToSDLKey[DIK_PAUSE] = SDLK_PAUSE;
    aryScanCodeToSDLKey[DIK_HOME] = SDLK_HOME;
    aryScanCodeToSDLKey[DIK_UP] = SDLK_UP;
    aryScanCodeToSDLKey[DIK_PRIOR] = SDLK_PAGEUP;
    aryScanCodeToSDLKey[DIK_LEFT] = SDLK_LEFT;
    aryScanCodeToSDLKey[DIK_RIGHT] = SDLK_RIGHT;
    aryScanCodeToSDLKey[DIK_END] = SDLK_END;
    aryScanCodeToSDLKey[DIK_DOWN] = SDLK_DOWN;
    aryScanCodeToSDLKey[DIK_NEXT] = SDLK_PAGEDOWN;
    aryScanCodeToSDLKey[DIK_INSERT] = SDLK_INSERT;
    aryScanCodeToSDLKey[DIK_DELETE] = SDLK_DELETE;
    aryScanCodeToSDLKey[DIK_LWIN] = SDLK_LMETA;
    aryScanCodeToSDLKey[DIK_RWIN] = SDLK_RMETA;
    aryScanCodeToSDLKey[DIK_APPS] = SDLK_MENU;

    bScanCodeMapInited = true;
}

static std::string strPasteBuffer;
// Just in case, to keep us from entering an unexpected KB state
const  size_t      kPasteMinBufExtra = 4;
/// Sightly inefficient, but who cares
static void GenKBStroke(const UINT uiScanCode, const bool bDepressed, const SDLMod keymods)
{
    const SDLKey sdlkey = aryScanCodeToSDLKey[uiScanCode];
    if (sdlkey == SDLK_UNKNOWN)
        return;

    SDL_Event evntKeyStroke = { 0 };
    evntKeyStroke.type = bDepressed ? SDL_KEYDOWN : SDL_KEYUP;
    evntKeyStroke.key.keysym.scancode = (unsigned char)LOBYTE(uiScanCode);
    evntKeyStroke.key.keysym.sym = sdlkey;
    evntKeyStroke.key.keysym.mod = keymods;
    evntKeyStroke.key.keysym.unicode = 0;
    evntKeyStroke.key.state = bDepressed ? SDL_PRESSED : SDL_RELEASED;
    SDL_PushEvent(&evntKeyStroke);
}

static bool PasteClipboardNext()
{
    if (strPasteBuffer.length() == 0)
        return false;

    if (!bScanCodeMapInited)
        PasteInitMapSCToSDLKey();
	
	if (IS_PC98_ARCH) {
		BIOS_AddKeyToBuffer(strPasteBuffer[0]);
		strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
		return true;
	}

    const char cKey = strPasteBuffer[0];
    SHORT shVirKey = VkKeyScan(cKey); // If it fails then MapVirtK will also fail, so no bail yet
    UINT uiScanCode = MapVirtualKey(LOBYTE(shVirKey), MAPVK_VK_TO_VSC);
    if (uiScanCode)
    {
        const bool   bModShift = ((shVirKey & 0x0100) != 0);
        const bool   bModCntrl = ((shVirKey & 0x0200) != 0);
        const bool   bModAlt = ((shVirKey & 0x0400) != 0);
        const SDLMod sdlmModsOn = SDL_GetModState();
        const bool   bModShiftOn = ((sdlmModsOn & (KMOD_LSHIFT | KMOD_RSHIFT)) > 0);
        const bool   bModCntrlOn = ((sdlmModsOn & (KMOD_LCTRL | KMOD_RCTRL)) > 0);
        const bool   bModAltOn = ((sdlmModsOn & (KMOD_LALT | KMOD_RALT)) > 0);
        const UINT   uiScanCodeShift = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
        const UINT   uiScanCodeCntrl = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
        const UINT   uiScanCodeAlt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
        const SDLMod sdlmMods = (SDLMod)((sdlmModsOn & ~(KMOD_LSHIFT | KMOD_RSHIFT |
            KMOD_LCTRL | KMOD_RCTRL |
            KMOD_LALT | KMOD_RALT)) |
            (bModShiftOn ? KMOD_LSHIFT : 0) |
            (bModCntrlOn ? KMOD_LCTRL : 0) |
            (bModAltOn ? KMOD_LALT : 0));

        /// \note Currently pasteing a character is a two step affair, because if
        ///       you do it too quickly DI can miss a key press/release.
        // Could be made more efficient, but would require tracking of more state,
        // so let's forgot that for now...
        size_t sStrokesRequired = 2; // At least the key & up/down
        if (bModShift != bModShiftOn) sStrokesRequired += 2; // To press/release Shift
        if (bModCntrl != bModCntrlOn) sStrokesRequired += 2; // To press/release Control
        if (bModAlt != bModAltOn) sStrokesRequired += 2; // To press/release Alt
        /// \fixme Should check if key is already pressed or not so it can toggle press
        ///        but since we don't actually have any mappings from VK/SC to DI codes
        ///        (which SDL (can) use(s) internally as actually scancodes), we can't
        ///        actually check that ourselves, sadly...
        if (KEYBOARD_BufferSpaceAvail() < (sStrokesRequired + kPasteMinBufExtra))
            return false;

        if (bModShift != bModShiftOn) GenKBStroke(uiScanCodeShift, !bModShiftOn, sdlmMods);
        if (bModCntrl != bModCntrlOn) GenKBStroke(uiScanCodeCntrl, !bModCntrlOn, sdlmMods);
        if (bModAlt != bModAltOn) GenKBStroke(uiScanCodeAlt, !bModAltOn, sdlmMods);
        GenKBStroke(uiScanCode, true, sdlmMods);
        GenKBStroke(uiScanCode, false, sdlmMods);
        if (bModShift != bModShiftOn) GenKBStroke(uiScanCodeShift, bModShiftOn, sdlmMods);
        if (bModCntrl != bModCntrlOn) GenKBStroke(uiScanCodeCntrl, bModCntrlOn, sdlmMods);
        if (bModAlt != bModAltOn) GenKBStroke(uiScanCodeAlt, bModAltOn, sdlmMods);
        //putchar(cKey); // For debugging dropped strokes
    } else {
		if (KEYBOARD_BufferSpaceAvail() < (10+kPasteMinBufExtra)) // For simplicity, we just mimic Alt+<3 digits>
			return false;

		const SDLMod sdlmModsOn = SDL_GetModState();
		const UINT   uiScanCodeAlt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
		const bool bModShiftOn = ((sdlmModsOn & (KMOD_LSHIFT|KMOD_RSHIFT)) > 0);
		const bool bModCntrlOn = ((sdlmModsOn & (KMOD_LCTRL|KMOD_RCTRL )) > 0);
		const bool bModAltOn = ((sdlmModsOn&(KMOD_LALT|KMOD_RALT)) > 0);
		const SDLMod sdlmMods = (SDLMod)((sdlmModsOn&~(KMOD_LSHIFT|KMOD_RSHIFT|KMOD_LCTRL|KMOD_RCTRL|KMOD_LALT|KMOD_RALT)) |
												 (bModShiftOn ? KMOD_LSHIFT : 0) |
												 (bModCntrlOn ? KMOD_LCTRL  : 0) |
												 (bModAltOn   ? KMOD_LALT   : 0));
	   
		if (!bModAltOn)                                                // Alt down if not already down
			GenKBStroke(uiScanCodeAlt, true, sdlmMods);
		   
		uint8_t ansiVal = cKey;
		for (int i = 100; i; i /= 10)
			{
			int numKey = ansiVal/i;                                    // High digit of Alt+ASCII number combination
			ansiVal %= i;
			UINT uiScanCode = MapVirtualKey(numKey+VK_NUMPAD0, MAPVK_VK_TO_VSC);
			GenKBStroke(uiScanCode, true, sdlmMods);
			GenKBStroke(uiScanCode, false, sdlmMods);
			}
		GenKBStroke(uiScanCodeAlt, false, sdlmMods);                // Alt up
		if (bModAltOn)                                                // Alt down if is was already down
			GenKBStroke(uiScanCodeAlt, true, sdlmMods);
    }

    // Pop head. Could be made more efficient, but this is neater.
    strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length()); // technically -1, but it clamps by itself anyways...
    return true;
}

extern uint8_t* clipAscii;
extern uint32_t clipSize;
extern void Unicode2Ascii(const uint16_t* unicode);

void PasteClipboard(bool bPressed)
{
    if (!bPressed) return;
    SDL_SysWMinfo wmiInfo;
    SDL_VERSION(&wmiInfo.version);

    if (SDL_GetWMInfo(&wmiInfo) != 1||!OpenClipboard(wmiInfo.window)) return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {CloseClipboard();return;}

    HANDLE hContents = GetClipboardData(CF_UNICODETEXT);
    if (!hContents) {CloseClipboard();return;}

    uint16_t *szClipboard = (uint16_t *)GlobalLock(hContents);
    if (szClipboard)
    {
		clipSize=0;
		Unicode2Ascii(szClipboard);
        unsigned long j=0;
        for (size_t i = 0; i < clipSize; ++i) if (clipAscii[i] == 9) j++;
        // Create a copy of the string, and filter out Linefeed characters (ASCII '10')
        char* szFilteredText = reinterpret_cast<char*>(alloca(clipSize + j*3 + 1));
        char* szFilterNextChar = szFilteredText;
        for (size_t i = 0; i < clipSize; ++i)
            if (clipAscii[i] == 9) // Tab to spaces
                for (int k=0; k<4; k++) {
                    *szFilterNextChar = ' ';
                    ++szFilterNextChar;
                }
            else if (clipAscii[i] != 0x0A) // Skip linefeeds
            {
                *szFilterNextChar = clipAscii[i];
                ++szFilterNextChar;
            }
        *szFilterNextChar = '\0'; // Cap it.

        strPasteBuffer.append(szFilteredText);
        GlobalUnlock(hContents);
		clipSize=0;
    }
    CloseClipboard();
}
/// TODO: add menu items here 
#else // end emendelson from dbDOS; improved by Wengier
static std::string strPasteBuffer;
#if defined(WIN32) // SDL2, MinGW / Added by Wengier
extern uint8_t* clipAscii;
extern uint32_t clipSize;
extern void Unicode2Ascii(const uint16_t* unicode);
void PasteClipboard(bool bPressed) {
	if (!bPressed||!OpenClipboard(NULL)) return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {CloseClipboard();return;}
    HANDLE hContents = GetClipboardData(CF_UNICODETEXT);
    if (!hContents) {CloseClipboard();return;}
    uint16_t *szClipboard = (uint16_t *)GlobalLock(hContents);
    if (szClipboard)
    {
		clipSize=0;
		Unicode2Ascii(szClipboard);
        unsigned long j=0;
        for (size_t i = 0; i < clipSize; ++i) if (clipAscii[i] == 9) j++;
        char* szFilteredText = reinterpret_cast<char*>(alloca(clipSize + j*3 + 1));
        char* szFilterNextChar = szFilteredText;
        for (size_t i = 0; i < clipSize; ++i)
            if (clipAscii[i] == 9) // Tab to spaces
                for (int k=0; k<4; k++) {
                    *szFilterNextChar = ' ';
                    ++szFilterNextChar;
                }
            else if (clipAscii[i] != 0x0A) // Skip linefeeds
            {
                *szFilterNextChar = clipAscii[i];
                ++szFilterNextChar;
            }
        *szFilterNextChar = '\0'; // Cap it.

        strPasteBuffer.append(szFilteredText);
        GlobalUnlock(hContents);
		clipSize=0;
    }
	CloseClipboard();
}

bool PasteClipboardNext() {
    if (strPasteBuffer.length() == 0) return false;
    if (strPasteBuffer[0]==13) {
        KEYBOARD_AddKey(KBD_enter, true);
        KEYBOARD_AddKey(KBD_enter, false);
    } else
        BIOS_AddKeyToBuffer(strPasteBuffer[0]<0?strPasteBuffer[0]&0xff:strPasteBuffer[0]);
    strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
	return true;
}
#elif defined(C_SDL2) || defined(MACOSX)
typedef char host_cnv_char_t;
char *CodePageHostToGuest(const host_cnv_char_t *s);
void GetClipboard(std::string* result);
void PasteClipboard(bool bPressed) {
	if (!bPressed) return;
    char *text;
#if defined(C_SDL2)
    text = SDL_GetClipboardText();
#else
    std::string clip="";
    GetClipboard(&clip);
    text = new char[clip.size()+1];
    strcpy(text, clip.c_str());
#endif
    std::string result="", pre="";
    for (unsigned int i=0; i<strlen(text); i++) {
        if (text[i]==0x0A&&(i==0||text[i-1]!=0x0D)) text[i]=0x0D;
        if (text[i]==9) result+="    ";
        else if (text[i]<0) {
            char c=text[i];
            int n=1;
            if ((c & 0xe0) == 0xc0) n=2;
            else if ((c & 0xf0) == 0xe0) n=3;
            else if ((c & 0xf8) == 0xf0) n=4;
            pre="";
            bool exit=false;
            for (int k=0; k<n; k++) {
                if (n>2&&i>=strlen(text)) {exit=true;break;}
                else if (i>=strlen(text)) {i++;break;}
                if (text[i]>=0) {exit=true;break;}
                pre+=std::string(1, text[i]);
                i++;
            }
            if (exit) continue;
            const char* asc = CodePageHostToGuest(pre.c_str());
            result+=asc!=NULL?std::string(asc):(n>2?"":pre);
            i--;
        } else {
            const char* asc = CodePageHostToGuest(std::string(1, text[i]).c_str());
            result+=asc != NULL?std::string(asc):std::string(1, text[i]);
        }
    }
    delete text;
    strPasteBuffer.append(result.c_str());
}

bool PasteClipboardNext() {
    if (strPasteBuffer.length() == 0) return false;
    if (strPasteBuffer[0]==13) {
        KEYBOARD_AddKey(KBD_enter, true);
        KEYBOARD_AddKey(KBD_enter, false);
    } else
        BIOS_AddKeyToBuffer(strPasteBuffer[0]<0?strPasteBuffer[0]&0xff:strPasteBuffer[0]);
    strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
	return true;
}
#elif defined(LINUX) && C_X11
#include <X11/Xlib.h>
typedef char host_cnv_char_t;
char *CodePageHostToGuest(const host_cnv_char_t *s);
void paste_utf8_prop(Display *dpy, Window w, Atom p)
{
    Atom da, incr, type;
    int di;
    unsigned long size, dul;
    unsigned char *prop_ret = NULL;

    XGetWindowProperty(dpy, w, p, 0, 0, False, AnyPropertyType, &type, &di, &dul, &size, &prop_ret);
    XFree(prop_ret);

    incr = XInternAtom(dpy, "INCR", False);
    if (type == incr) return;

    XGetWindowProperty(dpy, w, p, 0, size, False, AnyPropertyType, &da, &di, &dul, &dul, &prop_ret);
    char *text=(char *)prop_ret;
    std::string result="", pre="";
    for (unsigned int i=0; i<strlen(text); i++) {
        if (text[i]==0x0A&&(i==0||text[i-1]!=0x0D)) text[i]=0x0D;
        if (text[i]==9) result+="    ";
        else if (text[i]==0x0A) continue;
        else if (text[i]<0) {
            char c=text[i];
            int n=1;
            if ((c & 0xe0) == 0xc0) n=2;
            else if ((c & 0xf0) == 0xe0) n=3;
            else if ((c & 0xf8) == 0xf0) n=4;
            pre="";
            bool exit=false;
            for (int k=0; k<n; k++) {
                if (n>2&&i>=strlen(text)) {exit=true;break;}
                else if (i>=strlen(text)) {i++;break;}
                if (text[i]>=0) {exit=true;break;}
                pre+=std::string(1, text[i]);
                i++;
            }
            if (exit) continue;
            const char* asc = CodePageHostToGuest(pre.c_str());
            result+=asc!=NULL?std::string(asc):(n>2?"":pre);
            i--;
        } else {
            const char* asc = CodePageHostToGuest(std::string(1, text[i]).c_str());
            result+=asc != NULL?std::string(asc):std::string(1, text[i]);
        }
    }
    strPasteBuffer.append(result.c_str());
    fflush(stdout);
    XFree(prop_ret);
    XDeleteProperty(dpy, w, p);
}

void PasteClipboard(bool bPressed) {
	if (!bPressed) return;
    Display *dpy;
    Window owner, target_window, root;
    int screen;
    Atom sel, target_property, utf8;
    XEvent ev;
    XSelectionEvent *sev;

    dpy = XOpenDisplay(NULL);
    if (!dpy) return;

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    sel = XInternAtom(dpy, "CLIPBOARD", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);

    owner = XGetSelectionOwner(dpy, sel);
    if (owner == None) return;

    target_window = XCreateSimpleWindow(dpy, root, -10, -10, 1, 1, 0, 0, 0);
    target_property = XInternAtom(dpy, "PENGUIN", False);
    XConvertSelection(dpy, sel, utf8, target_property, target_window,
                      CurrentTime);
    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case SelectionNotify:
                sev = (XSelectionEvent*)&ev.xselection;
                if (sev->property == None) return;
                else {
                    paste_utf8_prop(dpy, target_window, target_property);
                    return;
                }
                break;
        }
    }
}

bool PasteClipboardNext() {
    if (strPasteBuffer.length() == 0) return false;
    if (strPasteBuffer[0]==13) {
        KEYBOARD_AddKey(KBD_enter, true);
        KEYBOARD_AddKey(KBD_enter, false);
    } else
        BIOS_AddKeyToBuffer(strPasteBuffer[0]<0?strPasteBuffer[0]&0xff:strPasteBuffer[0]);
    strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
	return true;
}
#else
void PasteClipboard(bool bPressed) {
    (void)bPressed;//UNUSED
    // stub
}

bool PasteClipboardNext() {
    // stub
    return false;
}
#endif
#endif

#if defined (WIN32)
void CopyClipboard(int all) {
	uint16_t len=0;
	const char* text = (char *)(all==2?Mouse_GetSelected(0,0,currentWindowWidth-1-sdl.clip.x,currentWindowHeight-1-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len):(all==1?Mouse_GetSelected(selscol, selsrow, selecol, selerow, -1, -1, &len):Mouse_GetSelected(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,mouse_end_x-sdl.clip.x,mouse_end_y-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len)));
	if (OpenClipboard(NULL)&&EmptyClipboard()) {
		HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, (len+1)*2);
		LPWSTR buffer = static_cast<LPWSTR>(GlobalLock(clipbuffer));
		if (buffer!=NULL) {
			int reqsize = MultiByteToWideChar(dos.loaded_codepage, 0, text, len+1, NULL, 0);
			if (reqsize>0 && MultiByteToWideChar(dos.loaded_codepage, 0, text, len+1, buffer, reqsize)==reqsize) {
				GlobalUnlock(clipbuffer);
				SetClipboardData(CF_UNICODETEXT,clipbuffer);
			}
		}
		CloseClipboard();
	}
}

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

#elif defined(C_SDL2)
typedef char host_cnv_char_t;
host_cnv_char_t *CodePageGuestToHost(const char *s);
void CopyClipboard(int all) {
	uint16_t len=0;
	char* text = (char *)(all==2?Mouse_GetSelected(0,0,currentWindowWidth-1-sdl.clip.x,currentWindowHeight-1-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len):(all==1?Mouse_GetSelected(selscol, selsrow, selecol, selerow, -1, -1, &len):Mouse_GetSelected(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,mouse_end_x-sdl.clip.x,mouse_end_y-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len)));
    unsigned int k=0;
    for (unsigned int i=0; i<len; i++)
        if (text[i]&&text[i]!=13)
            text[k++]=text[i];
    text[k]=0;
    std::string result="";
    std::istringstream iss(text);
    for (std::string token; std::getline(iss, token); ) {
        char* uname = CodePageGuestToHost(token.c_str());
        result+=(uname!=NULL?std::string(uname):token)+std::string(1, 10);
    }
    if (result.size()&&result.back()==10) result.pop_back();
    SDL_SetClipboardText(result.c_str());
}
#endif

#if defined(C_SDL2) || defined (WIN32) || defined(MACOSX) || defined(LINUX) && C_X11
void PasteClipStop(bool bPressed) {
    if (!bPressed) return;
    strPasteBuffer = "";
}
#endif

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
    if(f) {
        fclose(f);
        show_warning("Warning: dosbox-x.conf (or dosbox.conf) exists in current working directory.\nThis will override the configuration file at runtime.\n");
    }
    std::string path,file;
    Cross::GetPlatformConfigDir(path);
    Cross::GetPlatformConfigName(file);
    path += file;
    f = fopen(path.c_str(),"r");
    if(!f) exit(0);
    fclose(f);
    unlink(path.c_str());
    exit(0);
}

static void erasemapperfile() {
    FILE* g = fopen("dosbox-x.conf","r");
	if (!g) g = fopen("dosbox.conf","r");
    if(g) {
        fclose(g);
        show_warning("Warning: dosbox-x.conf (or dosbox.conf) exists in current working directory.\nKeymapping might not be properly reset.\n"
                     "Please reset configuration as well and delete the dosbox-x.conf (or dosbox.conf).\n");
    }

    std::string path,file=MAPPERFILE;
    Cross::GetPlatformConfigDir(path);
    path += file;
    FILE* f = fopen(path.c_str(),"r");
    if(!f) exit(0);
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
#if defined(WIN32) && !defined(HX_DOS)
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

bool DOSBOX_parse_argv() {
    std::string optname,tmp;

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

            fprintf(stderr,"\ndosbox-x [options]\n");
            fprintf(stderr,"\nDOSBox-X version %s %s, copyright 2011-%s The DOSBox-X Team.\n",VERSION,SDL_STRING,COPYRIGHT_END_YEAR);
            fprintf(stderr,"DOSBox-X project maintainer: joncampbell123 (The Great Codeholio)\n\n");
            fprintf(stderr,"Options can be started with either \"-\" or \"/\" (e.g. \"-help\" or \"/help\"):\n\n");
            fprintf(stderr,"  -?, -h, -help                           Show this help screen\n");
            fprintf(stderr,"  -v, -ver, -version                      Display DOSBox-X version information\n");
            fprintf(stderr,"  -fs, -fullscreen                        Start DOSBox-X in fullscreen mode\n");
            fprintf(stderr,"  -conf <configfile>                      Start DOSBox-X with the specific config file\n");
            fprintf(stderr,"  -editconf <editor>                      Edit the config file with the specific editor\n");
            fprintf(stderr,"  -userconf                               Create user level config file\n");
            fprintf(stderr,"  -printconf                              Print config file location\n");
            fprintf(stderr,"  -eraseconf (or -resetconf)              Erase config file\n");
            fprintf(stderr,"  -erasemapper (or -resetmapper)          Erase mapper file\n");
            fprintf(stderr,"  -opencaptures <param>                   Launch captures\n");
            fprintf(stderr,"  -opensaves <param>                      Launch saves\n");
            fprintf(stderr,"  -startui, -startgui, -starttool         Start DOSBox-X with GUI configuration tool\n");
            fprintf(stderr,"  -startmapper                            Start DOSBox-X with the mapper editor\n");
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
            fprintf(stderr,"  -lang <message file>                    Use specific message file instead of language= setting\n");
            fprintf(stderr,"  -nodpiaware                             Ignore (do not signal) Windows DPI awareness\n");
            fprintf(stderr,"  -securemode                             Enable secure mode\n");
#if defined(WIN32) && !defined(HX_DOS)
            fprintf(stderr,"  -winrun                                 Enable START command and CLIP$ device (Windows version only)\n");
            fprintf(stderr,"                                          Windows programs can be launched directly to run on the host.\n");
#endif
            fprintf(stderr,"  -noconfig                               Do not execute CONFIG.SYS config section\n");
            fprintf(stderr,"  -noautoexec                             Do not execute AUTOEXEC.BAT config section\n");
            fprintf(stderr,"  -exit                                   Exit after executing AUTOEXEC.BAT\n");
            fprintf(stderr,"  -silent                                 Run DOSBox-X silently and exit after executing AUTOEXEC.BAT.\n");
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
            fprintf(stderr,"  -log-fileio                             Log file I/O through INT 21h (debug level)\n\n");

#if defined(WIN32)
            DOSBox_ConsolePauseWait();
#endif

            return 0;
        }
        else if (optname == "c") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_c.push_back(tmp);
        }
        else if (optname == "set") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_set.push_back(tmp);
        }
        else if (optname == "alt-vga") {
            control->opt_alt_vga_render = true;
        }
        else if (optname == "log-con") {
            control->opt_log_con = true;
        }
        else if (optname == "time-limit") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            control->opt_time_limit = atof(tmp.c_str());
        }
        else if (optname == "break-start") {
            control->opt_break_start = true;
        }
        else if (optname == "silent") {
            putenv(const_cast<char*>("SDL_VIDEODRIVER=dummy"));
            control->opt_exit = true;
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
#if defined(WIN32)
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
        }
        else if (optname == "defaultdir") {
            if (control->cmdline->NextOptArgv(tmp)) {
                struct stat st;
                if (stat(tmp.c_str(), &st) == 0 && st.st_mode & S_IFDIR)
                    if (chdir(tmp.c_str()) < 0)
                        return false;
            }
        }
        else if (optname == "userconf") {
            control->opt_userconf = true;
        }
        else if (optname == "lang") {
            if (!control->cmdline->NextOptArgv(control->opt_lang)) return false;
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
            if (!control->cmdline->NextOptArgv(control->opt_opencaptures)) return false;
        }
        else if (optname == "opensaves") {
            if (!control->cmdline->NextOptArgv(control->opt_opensaves)) return false;
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
        else if (optname == "nomenu") {
            control->opt_nomenu = true;
        }
        else if (optname == "nogui") {
            control->opt_nogui = true;
        }
        else if (optname == "debug") {
            control->opt_debug = true;
        }
        else if (optname == "early-debug") {
            control->opt_earlydebug = true;
            control->opt_console = true;
        }
        else if (optname == "socket") {
            if (!control->cmdline->NextOptArgv(tmp)) return false;
            socknum = std::stoi(tmp);
        } else {
            printf("WARNING: Unknown option %s (first parsing stage)\n",optname.c_str());
        }
    }

    /* now that the above loop has eaten all the options from the command
     * line, scan the command line for batch files to run.
     * https://github.com/joncampbell123/dosbox-x/issues/369 */
    control->cmdline->BeginOpt(/*don't eat*/false);
    while (!control->cmdline->CurrentArgvEnd()) {
        control->cmdline->GetCurrentArgv(tmp);
        trim(tmp);
        {
            struct stat st;
            const char *ext = strrchr(tmp.c_str(),'.'); /* if it looks like a file... with an extension */
            if (stat(tmp.c_str(), &st) == 0) {
                if (st.st_mode & S_IFDIR) {
                    control->auto_bat_additional.push_back("@mount c: \""+tmp+"\"");
                    control->cmdline->EatCurrentArgv();
                    continue;
                } else if (ext != NULL && S_ISREG(st.st_mode) && (!strcasecmp(ext,".bat") || !strcasecmp(ext,".exe") || !strcasecmp(ext,".com"))) { /* .BAT files given on the command line trigger automounting C: to run it */
                    control->auto_bat_additional.push_back(tmp);
                    control->cmdline->EatCurrentArgv();
                    continue;
                }
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

#if defined(WIN32)
// NTS: I intend to add code that not only indicates High DPI awareness but also queries the monitor DPI
//      and then factor the DPI into DOSBox's scaler and UI decisions.
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

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
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

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
        Reflect_Menu();
#endif

        void update_pc98_function_row(unsigned char setting,bool force_redraw=false);

        void DRIVES_Startup(Section *s);
        DRIVES_Startup(NULL);

        /* NEC's function key row seems to be deeply embedded in the CON driver. Am I wrong? */
        if (IS_PC98_ARCH) update_pc98_function_row(1);

        DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY); // <- kernel is ready

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
         *       and auto-add a Z:\MSCDEX.EXE to the top of AUTOEXEC.BAT, command line switches and all. if the user has not already added it? */
        void MSCDEX_Startup(Section* sec);
        MSCDEX_Startup(NULL);

        /* Some installations load the MOUSE.COM driver from AUTOEXEC.BAT as well */
        /* TODO: Can we make this an option? Can we add a fake MOUSE.COM to the Z:\ drive as well? */
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

#if !defined(C_EMSCRIPTEN)
void update_capture_fmt_menu(void);
#endif

bool capture_fmt_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);

void update_pc98_clock_pit_menu(void) {
    Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));

    int pc98rate = pc98_section->Get_int("pc-98 timer master frequency");
    if (pc98rate > 6) pc98rate /= 2;
    if (pc98rate == 0) pc98rate = 5; /* Pick the most likely to work with DOS games (FIXME: This is a GUESS!! Is this correct?) */
    else if (pc98rate < 5) pc98rate = 4;
    else pc98rate = 5;

    mainMenu.get_item("dos_pc98_pit_4mhz").check(pc98rate == 4).refresh_item(mainMenu);
    mainMenu.get_item("dos_pc98_pit_5mhz").check(pc98rate == 5).refresh_item(mainMenu);
}

bool dos_pc98_clock_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void TIMER_OnPowerOn(Section*);
    void TIMER_OnEnterPC98_Phase2_UpdateBDA(void);

    const char *ts = menuitem->get_name().c_str();
    if (!strncmp(ts,"dos_pc98_pit_",13))
        ts += 13;
    else
        return true;

    std::string tmp = "pc-98 timer master frequency=";

    {
        char tmp1[64];
        sprintf(tmp1,"%d",atoi(ts));
        tmp += tmp1;
    }

    Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
    pc98_section->HandleInputline(tmp.c_str());

    TIMER_OnPowerOn(NULL);
    TIMER_OnEnterPC98_Phase2_UpdateBDA();

    update_pc98_clock_pit_menu();
    return true;
}

void update_dos_ems_menu(void) {
    Section_prop * dos_section = static_cast<Section_prop *>(control->GetSection("dos"));
    const char *ems = dos_section->Get_string("ems");
    if (ems == NULL) return;
    mainMenu.get_item("dos_ems_true").check(!strcmp(ems, "true")||!strcmp(ems, "1")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_board").check(!strcmp(ems, "emsboard")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_emm386").check(!strcmp(ems, "emm386")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_false").check(!strcmp(ems, "false")||!strcmp(ems, "0")).enable(true).refresh_item(mainMenu);
}

bool dos_ems_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    std::string tmp="";
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "dos_ems_true")) tmp="ems=true";
    else if (!strcmp(mname, "dos_ems_board")) tmp="ems=emsboard";
    else if (!strcmp(mname, "dos_ems_emm386")) tmp="ems=emm386";
    else if (!strcmp(mname, "dos_ems_false")) tmp="ems=false";
    if (tmp.size()) {
        Section_prop * dos_section = static_cast<Section_prop *>(control->GetSection("dos"));
        dos_section->HandleInputline(tmp.c_str());
        EMS_DoShutDown();
        void EMS_Startup(Section* sec);
        EMS_Startup(NULL);
        update_dos_ems_menu();
    }
    return true;
}

bool dos_debug_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    const auto &ts = menuitem->get_name();

    if (ts == "debug_logint21") {
        log_int21 = !log_int21;
        mainMenu.get_item("debug_logint21").check(log_int21).refresh_item(mainMenu);
    }
    else if (ts == "debug_logfileio") {
        log_fileio = !log_fileio;
        mainMenu.get_item("debug_logfileio").check(log_fileio).refresh_item(mainMenu);
    }

    return true;
}

void SetScaleForced(bool forced);
void OutputSettingMenuUpdate(void);

bool scaler_forced_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    SetScaleForced(!render.scale.forced);
    return true;
}

void MENU_swapstereo(bool enabled);
bool MENU_get_swapstereo(void);

bool mixer_swapstereo_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MENU_swapstereo(!MENU_get_swapstereo());
    return true;
}

void MENU_mute(bool enabled);
bool MENU_get_mute(void);

bool mixer_mute_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MENU_mute(!MENU_get_mute());
    return true;
}

bool mixer_info_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(20);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
    return true;
}

bool sb_device_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(21);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
    return true;
}

bool midi_device_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(22);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
    return true;
}

bool cpu_speed_emulate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    int cyclemu = 0;
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "cpu88-4"))
        cyclemu = 240;
    else if (!strcmp(mname, "cpu286-8"))
        cyclemu = 750;
    else if (!strcmp(mname, "cpu286-12"))
        cyclemu = 1510;
    else if (!strcmp(mname, "cpu286-25"))
        cyclemu = 3300;
    else if (!strcmp(mname, "cpu386-25"))
        cyclemu = 4595;
    else if (!strcmp(mname, "cpu386-33"))
        cyclemu = 6075;
    else if (!strcmp(mname, "cpu486-33"))
        cyclemu = 12010;
    else if (!strcmp(mname, "cpu486-66"))
        cyclemu = 23880;
    else if (!strcmp(mname, "cpu486-100"))
        cyclemu = 33445;
    else if (!strcmp(mname, "cpu486-133"))
        cyclemu = 47810;
    else if (!strcmp(mname, "cpu586-60"))
        cyclemu = 31545;
    else if (!strcmp(mname, "cpu586-66"))
        cyclemu = 35620;
    else if (!strcmp(mname, "cpu586-75"))
        cyclemu = 43500;
    else if (!strcmp(mname, "cpu586-90"))
        cyclemu = 52000;
    else if (!strcmp(mname, "cpu586-100"))
        cyclemu = 60000;
    else if (!strcmp(mname, "cpu586-120"))
        cyclemu = 74000;
    else if (!strcmp(mname, "cpu586-133"))
        cyclemu = 80000;
    else if (!strcmp(mname, "cpu586-166"))
        cyclemu = 97240;
    if (cyclemu>0) {
        Section* sec = control->GetSection("cpu");
        if (sec) {
            double perc = static_cast<Section_prop *>(sec)->Get_int("cycle emulation percentage adjust");
            cyclemu*=(int)((100+perc)/100);
            std::string tmp("cycles="+std::to_string(cyclemu));
            sec->HandleInputline(tmp);
        }
    }
    return true;
}

bool dos_mouse_enable_int33_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool Mouse_Drv;
    Mouse_Drv = !Mouse_Drv;
    mainMenu.get_item("dos_mouse_enable_int33").check(Mouse_Drv).refresh_item(mainMenu);
    return true;
}

bool dos_mouse_y_axis_reverse_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool Mouse_Vertical;
    Mouse_Vertical = !Mouse_Vertical;
    mainMenu.get_item("dos_mouse_y_axis_reverse").check(Mouse_Vertical).refresh_item(mainMenu);
    return true;
}

bool dos_mouse_sensitivity_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(28);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
    return true;
}

void dos_ver_menu(bool start) {
    mainMenu.get_item("dos_ver_330").check(dos.version.major==3&&dos.version.minor==30).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_500").check(dos.version.major==5&&dos.version.minor==00).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_622").check(dos.version.major==6&&dos.version.minor==22).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_710").check(dos.version.major==7&&dos.version.minor==10).enable(true).refresh_item(mainMenu);
    if (start || enablelfn != -2) uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && (dos.version.major>6 || winrun));
}

bool dos_ver_set_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "dos_ver_330")) {
        dos.version.major = 3;
        dos.version.minor = 30;
    } else if (!strcmp(mname, "dos_ver_500")) {
        dos.version.major = 5;
        dos.version.minor = 0;
    } else if (!strcmp(mname, "dos_ver_622")) {
        dos.version.major = 6;
        dos.version.minor = 22;
    } else if (!strcmp(mname, "dos_ver_710")) {
        dos.version.major = 7;
        dos.version.minor = 10;
    }
    dos_ver_menu(false);
    return true;
}

bool dos_ver_edit_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(19);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
    return true;
}

bool dos_lfn_auto_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    enablelfn = -1;
	uselfn = dos.version.major>6 || winrun;
    mainMenu.get_item("dos_lfn_auto").check(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_enable").check(false).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_disable").check(false).refresh_item(mainMenu);
    return true;
}

bool dos_lfn_enable_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    enablelfn = 1;
	uselfn = true;
    mainMenu.get_item("dos_lfn_auto").check(false).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_enable").check(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_disable").check(false).refresh_item(mainMenu);
    return true;
}

bool dos_lfn_disable_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    enablelfn = 0;
	uselfn = false;
    mainMenu.get_item("dos_lfn_auto").check(false).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_enable").check(false).refresh_item(mainMenu);
    mainMenu.get_item("dos_lfn_disable").check(true).refresh_item(mainMenu);
    return true;
}

#if defined(WIN32) && !defined(HX_DOS)
bool dos_win_autorun_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool winautorun;
    winautorun = !winautorun;
    mainMenu.get_item("dos_win_autorun").check(winautorun).refresh_item(mainMenu);
    return true;
}

bool dos_win_wait_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool startwait;
    startwait = !startwait;
    mainMenu.get_item("dos_win_wait").check(startwait).refresh_item(mainMenu);
    return true;
}

bool dos_win_quiet_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool startquiet;
    startquiet = !startquiet;
    mainMenu.get_item("dos_win_quiet").check(startquiet).refresh_item(mainMenu);
    return true;
}
#endif

bool wheel_updown_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    wheel_key = 1;
    mainMenu.get_item("wheel_updown").check(true).refresh_item(mainMenu);
    mainMenu.get_item("wheel_leftright").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_pageupdown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_none").check(false).refresh_item(mainMenu);
    return true;
}

bool wheel_leftright_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    wheel_key = 2;
    mainMenu.get_item("wheel_updown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_leftright").check(true).refresh_item(mainMenu);
    mainMenu.get_item("wheel_pageupdown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_none").check(false).refresh_item(mainMenu);
    return true;
}

bool wheel_pageupdown_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    wheel_key = 3;
    mainMenu.get_item("wheel_updown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_leftright").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_pageupdown").check(true).refresh_item(mainMenu);
    mainMenu.get_item("wheel_none").check(false).refresh_item(mainMenu);
    return true;
}

bool wheel_none_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    wheel_key = 0;
    mainMenu.get_item("wheel_updown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_leftright").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_pageupdown").check(false).refresh_item(mainMenu);
    mainMenu.get_item("wheel_none").check(true).refresh_item(mainMenu);
    return true;
}

bool wheel_guest_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    wheel_guest = !wheel_guest;
    mainMenu.get_item("wheel_guest").check(wheel_guest).refresh_item(mainMenu);
    return true;
}

extern bool                         gdc_5mhz_mode_initial;

bool vid_pc98_5mhz_gdc_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (IS_PC98_ARCH) {
        void gdc_5mhz_mode_update_vars(void);
        extern bool gdc_5mhz_mode;
        extern bool gdc_clock_1;
        extern bool gdc_clock_2;

        gdc_5mhz_mode = !gdc_5mhz_mode;
        gdc_5mhz_mode_update_vars();

        // this is the user's command to change GDC setting, so it should appear
        // as if the initial setting in the dip switches
        gdc_5mhz_mode_initial = gdc_5mhz_mode;

        gdc_clock_1 = gdc_5mhz_mode;
        gdc_clock_2 = gdc_5mhz_mode;

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (gdc_5mhz_mode)
            pc98_section->HandleInputline("pc-98 start gdc at 5mhz=1");
        else
            pc98_section->HandleInputline("pc-98 start gdc at 5mhz=0");

        mainMenu.get_item("pc98_5mhz_gdc").check(gdc_5mhz_mode).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_200scanline_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (IS_PC98_ARCH) {
        extern bool pc98_allow_scanline_effect;

        pc98_allow_scanline_effect = !pc98_allow_scanline_effect;

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (pc98_allow_scanline_effect)
            pc98_section->HandleInputline("pc-98 allow scanline effect=1");
        else
            pc98_section->HandleInputline("pc-98 allow scanline effect=0");

        mainMenu.get_item("pc98_allow_200scanline").check(pc98_allow_scanline_effect).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_4parts_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (IS_PC98_ARCH) {
        extern bool pc98_allow_4_display_partitions;
        void updateGDCpartitions4(bool enable);

        updateGDCpartitions4(!pc98_allow_4_display_partitions);

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (pc98_allow_4_display_partitions)
            pc98_section->HandleInputline("pc-98 allow 4 display partition graphics=1");
        else
            pc98_section->HandleInputline("pc-98 allow 4 display partition graphics=0");

        mainMenu.get_item("pc98_allow_4partitions").check(pc98_allow_4_display_partitions).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_enable_188user_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void gdc_egc_enable_update_vars(void);
//    extern bool enable_pc98_egc;
//    extern bool enable_pc98_grcg;
//    extern bool enable_pc98_16color;
    extern bool enable_pc98_188usermod;

    if(IS_PC98_ARCH) {
        enable_pc98_188usermod = !enable_pc98_188usermod;
        gdc_egc_enable_update_vars();

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (enable_pc98_188usermod)
            pc98_section->HandleInputline("pc-98 enable 188 user cg=1");
        else
            pc98_section->HandleInputline("pc-98 enable 188 user cg=0");

        mainMenu.get_item("pc98_enable_188user").check(enable_pc98_188usermod).refresh_item(mainMenu);
    }
    
    return true;
}

bool vid_pc98_enable_egc_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void gdc_egc_enable_update_vars(void);
    extern bool enable_pc98_egc;
    extern bool enable_pc98_grcg;
    extern bool enable_pc98_16color;
//    extern bool enable_pc98_188usermod;

    if(IS_PC98_ARCH) {
        enable_pc98_egc = !enable_pc98_egc;
        gdc_egc_enable_update_vars();

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (enable_pc98_egc) {
            pc98_section->HandleInputline("pc-98 enable egc=1");

            if(!enable_pc98_grcg) { //Also enable GRCG if GRCG is disabled when enabling EGC
                enable_pc98_grcg = !enable_pc98_grcg;
                mem_writeb(0x54C,(enable_pc98_grcg ? 0x02 : 0x00) | (enable_pc98_16color ? 0x04 : 0x00));   
                pc98_section->HandleInputline("pc-98 enable grcg=1");
            }
        }
        else
            pc98_section->HandleInputline("pc-98 enable egc=0");

        mainMenu.get_item("pc98_enable_egc").check(enable_pc98_egc).refresh_item(mainMenu);
        mainMenu.get_item("pc98_enable_grcg").check(enable_pc98_grcg).refresh_item(mainMenu);
    }
    
    return true;
}

bool vid_pc98_enable_grcg_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    extern bool enable_pc98_grcg;
    extern bool enable_pc98_egc;
    void gdc_grcg_enable_update_vars(void);

    if(IS_PC98_ARCH) {
        enable_pc98_grcg = !enable_pc98_grcg;
        gdc_grcg_enable_update_vars();

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (enable_pc98_grcg)
            pc98_section->HandleInputline("pc-98 enable grcg=1");
        else
            pc98_section->HandleInputline("pc-98 enable grcg=0");

        if ((!enable_pc98_grcg) && enable_pc98_egc) { // Also disable EGC if switching off GRCG
            void gdc_egc_enable_update_vars(void);
            enable_pc98_egc = !enable_pc98_egc;
            gdc_egc_enable_update_vars();   
            pc98_section->HandleInputline("pc-98 enable egc=0");
        }

        mainMenu.get_item("pc98_enable_egc").check(enable_pc98_egc).refresh_item(mainMenu);
        mainMenu.get_item("pc98_enable_grcg").check(enable_pc98_grcg).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_enable_analog_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    //NOTE: I thought that even later PC-9801s and some PC-9821s could use EGC features in digital 8-colors mode? 
    extern bool enable_pc98_16color;
    void gdc_16color_enable_update_vars(void);

    if(IS_PC98_ARCH) {
        enable_pc98_16color = !enable_pc98_16color;
        gdc_16color_enable_update_vars();

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (enable_pc98_16color)
            pc98_section->HandleInputline("pc-98 enable 16-color=1");
        else
            pc98_section->HandleInputline("pc-98 enable 16-color=0");

        mainMenu.get_item("pc98_enable_analog").check(enable_pc98_16color).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_enable_analog256_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    //NOTE: I thought that even later PC-9801s and some PC-9821s could use EGC features in digital 8-colors mode? 
    extern bool enable_pc98_256color;
    void gdc_16color_enable_update_vars(void);

    if(IS_PC98_ARCH) {
        enable_pc98_256color = !enable_pc98_256color;
        gdc_16color_enable_update_vars();

        Section_prop * pc98_section = static_cast<Section_prop *>(control->GetSection("pc98"));
        if (enable_pc98_256color)
            pc98_section->HandleInputline("pc-98 enable 256-color=1");
        else
            pc98_section->HandleInputline("pc-98 enable 256-color=0");

        mainMenu.get_item("pc98_enable_analog256").check(enable_pc98_256color).refresh_item(mainMenu);
    }

    return true;
}

bool vid_pc98_cleartext_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void pc98_clear_text(void);
    if (IS_PC98_ARCH) pc98_clear_text();
    return true;
}

#ifdef C_D3DSHADERS
extern bool informd3d;
bool vid_select_pixel_shader_menu_callback(DOSBoxMenu* const menu, DOSBoxMenu::item* const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    OPENFILENAME ofn;
    char filenamebuf[300] = { 0 };
    char cwd[1024]; /* to prevent the dialog box from browsing relative to the Documents folder */

    GetCurrentDirectory(sizeof(cwd) - 16,cwd);

    std::string forced_setting;
    std::string o_cwd = cwd; /* GetOpenFileName() will change the current working directory! */

    strcat(cwd, "\\shaders"); /* DOSBox "D3D patch" compat: File names are assumed to exist relative to <cwd>\shaders */

    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    assert(section != NULL);
    {
        Prop_multival* prop = section->Get_multival("pixelshader");
        const char *path = prop->GetSection()->Get_string("type");
        forced_setting = prop->GetSection()->Get_string("force");

        if (path != NULL && strcmp(path, "none") != 0) {
            filenamebuf[sizeof(filenamebuf) - 1] = 0;
            strncpy(filenamebuf, path, sizeof(filenamebuf) - 1);
        }
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetHWND();
    ofn.lpstrFilter =
        "D3D shaders\0"                 "*.fx\0"
        "\0"                            "\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filenamebuf;
    ofn.nMaxFile = sizeof(filenamebuf);
    ofn.lpstrTitle = "Select D3D shader";
    ofn.lpstrInitialDir = cwd;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES;

    if (GetOpenFileName(&ofn)) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the pixelshader setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = ofn.lpstrFile;

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd, strlen(cwd))) {
            name += strlen(cwd);
            while (*name == '\\') name++;
        }

        /* the shader set included with the source code includes none.fx which is empty.
           if that was chosen then just change it to "none" so the D3D shader code does
           not waste it's time. */
        {
            const char* n = strrchr(name, '\\');
            if (n == NULL) n = name;

            if (!strcasecmp(n, "none.fx"))
                name = "none";
        }

        /* SetVal just forces the interpreter to parse name=value and pixelshader is a multivalue. */
        std::string tmp = name;
        tmp += " ";
        tmp += forced_setting;
        SetVal("render", "pixelshader", tmp);

        /* GetOpenFileName() probably changed the current directory.
           This must be done before reinit of GFX because pixelshader might be relative path. */
        SetCurrentDirectory(o_cwd.c_str());
        informd3d=true;
        /* force reinit */
        GFX_ForceRedrawScreen();
        informd3d=false;
    }
    else {
        /* GetOpenFileName() probably changed the current directory */
        SetCurrentDirectory(o_cwd.c_str());
    }

    return true;
}
#endif

#ifdef C_OPENGL
extern bool reloadshader;
extern std::string shader_src;
std::string LoadGLShader(Section_prop * section);
bool vid_select_glsl_shader_menu_callback(DOSBoxMenu* const menu, DOSBoxMenu::item* const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    assert(section != NULL);
    //Prop_path *sh = section->Get_path("glshader");

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT+"glshaders"+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.glsl","*.GLSL"};
    const char *lFilterDescription = "OpenGL shader files (*.glsl)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select OpenGL shader",cwd.c_str(),2,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the pixelshader setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;
        std::string tmp = "";

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        /* the shader set included with the source code includes none.glsl which is empty.
           if that was chosen then just change it to "none" so the GLSL shader code does
           not waste it's time. */
        {
            const char* n = strrchr(name, CROSS_FILESPLIT);
            if (n == NULL) n = name;
            else n++;

            if (!strcasecmp(n, "none.glsl"))
                tmp = "default";
            else if (!strcasecmp(n, "advinterp2x.glsl") || !strcasecmp(n, "advinterp3x.glsl") ||
                !strcasecmp(n, "advmame2x.glsl") || !strcasecmp(n, "advmame3x.glsl") ||
                !strcasecmp(n, "rgb2x.glsl") || !strcasecmp(n, "rgb3x.glsl") ||
                !strcasecmp(n, "scan2x.glsl") || !strcasecmp(n, "scan3x.glsl") ||
                !strcasecmp(n, "tv2x.glsl") || !strcasecmp(n, "tv3x.glsl") ||
                !strcasecmp(n, "sharp.glsl") || !strcasecmp(n, "default.glsl")) {
                    tmp = n;
                    tmp.erase(tmp.size()-5, string::npos);
            } else
                tmp = name;
        }

        if (tmp.size()) {
            SetVal("render", "glshader", tmp);
            LoadGLShader(section);

            /* force reinit */
            GFX_ForceRedrawScreen();
        }
    }
    chdir( Temp_CurrentDir );
#endif

    return true;
}
#endif

#ifdef USE_TTF
void ttf_reset(void) {
    OUTPUT_TTF_Select();
    resetFontSize();
}

bool vid_select_ttf_font_menu_callback(DOSBoxMenu* const menu, DOSBoxMenu::item* const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    assert(section != NULL);

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.ttf","*.TTF"};
    const char *lFilterDescription = "TrueType font files (*.ttf)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select TrueType font",cwd.c_str(),2,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the pixelshader setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;
        std::string tmp = "";

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        if (*name) {
            SetVal("render", "ttf.font", name);
            void ttf_reset(void);
            ttf_reset();
        }
    }
    chdir( Temp_CurrentDir );
#endif

    return true;
}
#endif

void Load_mapper_file() {
    Section_prop* section = static_cast<Section_prop*>(control->GetSection("sdl"));
    assert(section != NULL);

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.map","*.MAP"};
    const char *lFilterDescription = "Mapper files (*.map)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select mapper file",cwd.c_str(),2,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the pixelshader setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;
        std::string tmp = "";

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        if (*name) {
            Prop_path* pp;
#if defined(C_SDL2)
            pp = section->Get_path("mapperfile_sdl2");
#else
            pp = section->Get_path("mapperfile_sdl1");
#endif
            if (pp->realpath=="")
                SetVal("sdl", "mapperfile", name);
            else {
#if defined(C_SDL2)
                SetVal("sdl", "mapperfile_sdl2", name);
#else
                SetVal("sdl", "mapperfile_sdl1", name);
#endif
            }
            void ReloadMapper(Section_prop *sec, bool init);
            ReloadMapper(section,true);
        }
    }
    chdir( Temp_CurrentDir );
#endif
}

bool vid_pc98_graphics_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void pc98_clear_graphics(void);
    if (IS_PC98_ARCH) pc98_clear_graphics();
    return true;
}

bool overscan_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    int f = atoi(menuitem->get_text().c_str()); /* Off becomes 0 */
    char tmp[64];

    sprintf(tmp,"%d",f);
    SetVal("sdl", "overscan", tmp);
    change_output(7);
    return true;
}

void UpdateOverscanMenu(void) {
    for (size_t i=0;i <= 10;i++) {
        char tmp[64];
        sprintf(tmp,"overscan_%zu",i);
        mainMenu.get_item(tmp).check(sdl.overscan_width == i).refresh_item(mainMenu);
    }
}

bool vsync_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
#if !defined(C_SDL2)
    const char *val = menuitem->get_name().c_str();
    if (!strncmp(val,"vsync_",6))
        val += 6;
    else
        return true;

    SetVal("vsync", "vsyncmode", val);

    void change_output(int output);
    change_output(8);

    VGA_Vsync VGA_Vsync_Decode(const char *vsyncmodestr);
    void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode);
    VGA_VsyncUpdateMode(VGA_Vsync_Decode(val));
#endif
    return true;
}

bool vsync_set_syncrate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
#if !defined(C_SDL2)
    GUI_Shortcut(17);
#endif
    return true;
}

bool output_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    const char *what = menuitem->get_name().c_str();

    if (!strncmp(what,"output_",7))
        what += 7;
    else
        return true;

    bool reset=false;
#if defined(USE_TTF)
    if (TTF_using()) reset=true;
#endif

    if (!strcmp(what,"surface")) {
        if (sdl.desktop.want_type == SCREEN_SURFACE) return true;
        change_output(0);
        RENDER_Reset();
    }
    else if (!strcmp(what,"opengl")) {
#if C_OPENGL
        if (sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.bilinear) return true;
        change_output(3);
#endif
    }
    else if (!strcmp(what,"openglnb")) {
#if C_OPENGL
        if (sdl.desktop.want_type == SCREEN_OPENGL && !sdl_opengl.bilinear) return true;
        change_output(4);
#endif
    }
    else if (!strcmp(what,"direct3d")) {
#if C_DIRECT3D
        if (sdl.desktop.want_type == SCREEN_DIRECT3D) return true;
        change_output(5);
#endif
    }
    else if (!strcmp(what,"ttf")) {
#if defined(USE_TTF)
        if (sdl.desktop.want_type == SCREEN_TTF || (CurMode->type!=M_TEXT && !IS_PC98_ARCH)) return true;
#if C_OPENGL && defined(MACOSX) && !defined(C_SDL2)
        if (sdl.desktop.want_type == SCREEN_SURFACE) {
            sdl_opengl.framebuf = calloc(sdl.draw.width*sdl.draw.height, 4);
            sdl.desktop.type = SCREEN_OPENGL;
        }
#endif
        if (window_was_maximized&&!GFX_IsFullscreen()) {
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_RESTORE);
#else
            // Todo: How about Linux and macOS
#endif
        }
#if !defined(C_SDL2)
        putenv("SDL_VIDEO_CENTERED=center");
#endif
        firstset=false;
        change_output(9);
#endif
    }
    if (reset) RENDER_Reset();

    SetVal("sdl", "output", what);
    OutputSettingMenuUpdate();
    return true;
}

bool clear_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064)
        return true;
    if (CurMode->type==M_TEXT || dos_kernel_disabled) {
        const auto rows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS);
        const auto cols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
        INT10_ScrollWindow(0, 0, rows, static_cast<uint8_t>(cols), -rows, 0x7, 0xff);
        INT10_SetCursorPos(0, 0, 0);
    } else if (IS_PC98_ARCH) {
        char msg[]="[2J";
        uint16_t s = (uint16_t)strlen(msg);
        DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
    } else {
        uint16_t oldax=reg_ax;
        reg_ax=(uint16_t)CurMode->mode;
        CALLBACK_RunRealInt(0x10);
        reg_ax = oldax;
    }
    if (!strcmp(mname, "clear_screen") && !dos_kernel_disabled && !strcmp(RunningProgram, "COMMAND")) {
        DOS_Shell temp;
        temp.exit = true;
        temp.ShowPrompt();
    }
    return true;
}

#include "inout.h"
bool intensity_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    uint16_t oldax=reg_ax, oldbx=reg_bx;
    if (IS_PC98_ARCH||(CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064))
        return true;
    if (!strcmp(mname, "text_background"))
        reg_bl = 0;
    else
        reg_bl = 1;
	reg_ax = 0x1003;
    reg_bh = 0;
	CALLBACK_RunRealInt(0x10);
    reg_ax = oldax;
    reg_bx = oldbx;
    return true;
}

int GetNumScreen() {
    int numscreen = 1;
#if defined(C_SDL2)
    numscreen = SDL_GetNumVideoDisplays();
#elif defined(WIN32) && !defined(HX_DOS)
    xyp xy={0};
    xy.x=-1;
    xy.y=-1;
    curscr=0;
    EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
    numscreen = curscr;
#elif defined(MACOSX)
    CGDisplayCount nDisplays;
    CGGetActiveDisplayList(0,0, &nDisplays);
    numscreen = (int)nDisplays;
#elif defined(LINUX) && C_X11
    Display *dpy = XOpenDisplay(NULL);
    if (dpy) numscreen = XScreenCount(dpy);
#endif
    return numscreen;
}

#if defined(USE_TTF)
void GetMaxWidthHeight(int *pmaxWidth, int *pmaxHeight) {
    int maxWidth = sdl.desktop.full.width;
    int maxHeight = sdl.desktop.full.height;

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

void ttf_setlines(int cols, int lins) {
    SetVal("render", "ttf.cols", std::to_string(cols));
    SetVal("render", "ttf.lins", std::to_string(lins));
    firstset=true;
    ttf_reset();
    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_COLS,ttf.cols);
    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,ttf.lins-1);
    vga.draw.address_add = ttf.cols * 2;
}
#endif

bool setlines(const char *mname) {
    uint16_t oldax=reg_ax, oldbx=reg_bx, oldcx=reg_cx;
    if (!strcmp(mname, "line_80x25")) {
        reg_ax = 0x0003;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 25);
#endif
    } else if (!strcmp(mname, "line_80x43")) {
        reg_ax = 0x0019;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 43);
#endif
    } else if (!strcmp(mname, "line_80x50")) {
        reg_ax = 0x1202;
        reg_bl = 0x30;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 3;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x1112;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x100;
        reg_cx = 0x808;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 50);
#endif
    } else if (!strcmp(mname, "line_80x60")) {
        reg_ax = 0x0043;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 60);
#endif
    } else if (!strcmp(mname, "line_132x25")) {
        reg_ax = 0x0055;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 25);
#endif
    } else if (!strcmp(mname, "line_132x43")) {
        reg_ax = 0x0054;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 43);
#endif
    } else if (!strcmp(mname, "line_132x50")) {
        reg_ax = 0x0055;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x1112;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x100;
        reg_cx = 0x808;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 50);
#endif
    } else if (!strcmp(mname, "line_132x60")) {
        reg_ax = 0x0064;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 60);
#endif
    } else
        return false;
    reg_ax = oldax;
    reg_bx = oldbx;
    reg_cx = oldcx;
    return true;
}

#if defined(USE_TTF)
bool ttf_style_change_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "ttf_showbold")) {
        showbold=!showbold;
        mainMenu.get_item(mname).check(showbold).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showital")) {
        showital=!showital;
        mainMenu.get_item(mname).check(showital).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showline")) {
        showline=!showline;
        mainMenu.get_item(mname).check(showline).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showsout")) {
        showsout=!showsout;
        mainMenu.get_item(mname).check(showsout).refresh_item(mainMenu);
    } else
        return true;
    resetFontSize();
    return true;
}

bool ttf_wp_change_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "ttf_wpno")) {
        wpType=0;
    } else if (!strcmp(mname, "ttf_wpwp")) {
        wpType=1;
    } else if (!strcmp(mname, "ttf_wpws")) {
        wpType=2;
    } else if (!strcmp(mname, "ttf_wpxy")) {
        wpType=3;
    } else
        return true;
    mainMenu.get_item("ttf_wpno").check(!wpType).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpwp").check(wpType==1).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpws").check(wpType==2).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpxy").check(wpType==3).refresh_item(mainMenu);
    resetFontSize();
    return true;
}
#endif

bool lines_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    clear_menu_callback(menu, menuitem);
    const char *mname = menuitem->get_name().c_str();
    if (IS_PC98_ARCH||(CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064))
        return true;
    if (!setlines(mname)) return true;
    if (!dos_kernel_disabled && !strcmp(RunningProgram, "COMMAND")) {
        DOS_Shell temp;
        temp.exit = true;
        temp.ShowPrompt();
    }
    return true;
}

bool MENU_SetBool(std::string secname, std::string value);

bool vga_9widetext_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MENU_SetBool("render", "char9");
    return true;
}

bool doublescan_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    MENU_SetBool("render", "doublescan");
    return true;
}

bool scaler_set_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    const char *scaler = menuitem->get_name().c_str();
    if (!strncmp(scaler,"scaler_set_",11))
        scaler += 11;
    else
        abort();

    auto value = std::string(scaler) + (render.scale.forced ? " forced" : "");
    SetVal("render", "scaler", value);

    void RENDER_UpdateFromScalerSetting(void);
    RENDER_UpdateFromScalerSetting();

    void RENDER_UpdateScalerMenu(void);
    RENDER_UpdateScalerMenu();

    void RENDER_CallBack( GFX_CallBackFunctions_t function );
    RENDER_CallBack(GFX_CallBackReset);

    return true;
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

bool video_frameskip_common_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    int f = atoi(menuitem->get_text().c_str()); /* Off becomes 0 */
    char tmp1[64], tmp2[64];

    sprintf(tmp1,"%d",f);
    SetVal("render", "frameskip", tmp1);
    for (unsigned int i=0;i<=10;i++) {
        sprintf(tmp2,"frameskip_%u",i);
        mainMenu.get_item(tmp2).check((unsigned int)f==i).refresh_item(mainMenu);
    }
    return true;
}

bool show_console_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
#if !defined(C_EMSCRIPTEN) && defined(WIN32) && !defined(HX_DOS)
#if C_DEBUG
    if (DEBUG_IsDebuggerConsoleVisible())
        return true;
#endif
    HWND hwnd = GetConsoleWindow();
    if (hwnd == NULL)
        DOSBox_ShowConsole();
    else if (IsWindowVisible(hwnd))
        ShowWindow(hwnd, SW_HIDE);
    else
        ShowWindow(hwnd, SW_SHOW);
    mainMenu.get_item("show_console").check(IsWindowVisible(hwnd)).refresh_item(mainMenu);
#endif
    return true;
}

bool wait_on_error_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
#if !defined(C_EMSCRIPTEN)
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    sdl.wait_on_error = !sdl.wait_on_error;
    mainMenu.get_item("wait_on_error").check(sdl.wait_on_error).refresh_item(mainMenu);
#endif
    return true;
}

bool autolock_mouse_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    sdl.mouse.autoenable = !sdl.mouse.autoenable;
    mainMenu.get_item("auto_lock_mouse").check(sdl.mouse.autoenable).refresh_item(mainMenu);
    return true;
}

#if defined (WIN32) || defined(C_SDL2)
bool arrow_keys_clipboard_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    mbutton = 4;
    mainMenu.get_item("clipboard_right").check(false).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_middle").check(false).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_arrows").check(true).refresh_item(mainMenu);
    return true;
}

bool right_mouse_clipboard_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    mbutton = 3;
    mainMenu.get_item("clipboard_right").check(true).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_middle").check(false).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_arrows").check(false).refresh_item(mainMenu);
    return true;
}

bool middle_mouse_clipboard_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    mbutton = 2;
    mainMenu.get_item("clipboard_right").check(false).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_middle").check(true).refresh_item(mainMenu);
    mainMenu.get_item("clipboard_arrows").check(false).refresh_item(mainMenu);
    return true;
}

bool screen_to_clipboard_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    CopyClipboard(2);
    return true;
}

void QuickEdit(bool bPressed)
{
    if (!bPressed) return;
    direct_mouse_clipboard = !direct_mouse_clipboard;
    mainMenu.get_item("mapper_fastedit").check(direct_mouse_clipboard).refresh_item(mainMenu);
}

void CopyAllClipboard(bool bPressed) {
    if (!bPressed) return;
    CopyClipboard(2);
}
#else
void CopyAllClipboard(bool bPressed) {
    if (!bPressed) return;
    // STUB
}
#endif

#if defined (WIN32)
bool dos_clipboard_api_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    clipboard_dosapi = !clipboard_dosapi;
    if (control->SecureMode()) clipboard_dosapi = false;
    mainMenu.get_item("clipboard_dosapi").check(clipboard_dosapi).refresh_item(mainMenu);
    return true;
}

bool dos_clipboard_device_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (dos_clipboard_device_access == 4) dos_clipboard_device_access=1;
    else if (dos_clipboard_device_access) dos_clipboard_device_access=4;
    mainMenu.get_item("clipboard_device").check(dos_clipboard_device_access==4&&!control->SecureMode()).refresh_item(mainMenu);
    return true;
}
#endif

bool pc98_force_uskb_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    pc98_force_ibm_layout = !pc98_force_ibm_layout;
    mainMenu.get_item("pc98_use_uskb").check(pc98_force_ibm_layout).refresh_item(mainMenu);
    return true;
}

bool doublebuf_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    std::string doubleBufString = std::string("desktop.doublebuf");
    SetVal("sdl", "fulldouble", (GetSetSDLValue(1, doubleBufString, 0)) ? "false" : "true"); res_init();
    mainMenu.get_item("doublebuf").check(!!GetSetSDLValue(1, doubleBufString, 0)).refresh_item(mainMenu);
    return true;
}

bool quick_reboot_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    use_quick_reboot = !use_quick_reboot;
    mainMenu.get_item("quick_reboot").check(use_quick_reboot).refresh_item(mainMenu);
    return true;
}

bool sync_host_datetime_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (!sync_time) sync_time=true;
    else if (!manualtime) sync_time=false;
    manualtime=false;
    mainMenu.get_item("sync_host_datetime").check(sync_time).refresh_item(mainMenu);
    return true;
}

bool shell_config_commands_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    enable_config_as_shell_commands = !enable_config_as_shell_commands;
    mainMenu.get_item("shell_config_commands").check(enable_config_as_shell_commands).refresh_item(mainMenu);
    return true;
}

bool noremark_savestate_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    noremark_save_state = !noremark_save_state;
    mainMenu.get_item("noremark_savestate").check(noremark_save_state).refresh_item(mainMenu);
    return true;
}

bool force_loadstate_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    force_load_state = !force_load_state;
    mainMenu.get_item("force_loadstate").check(force_load_state).refresh_item(mainMenu);
    return true;
}

bool browse_save_file_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "browsesavefile")&&!use_save_file) return false;

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    const char *lFilterPatterns[] = {"*.sav","*.SAV"};
    const char *lFilterDescription = "Save files (*.sav)";
    char const * lTheSaveFileName = tinyfd_saveFileDialog("Select a save file","",2,lFilterPatterns,lFilterDescription);
    if (lTheSaveFileName!=NULL) {
        savefilename = std::string(lTheSaveFileName);
        mainMenu.get_item("usesavefile").set_text("Use save file"+(savefilename.size()?" ("+savefilename+")":"")).refresh_item(mainMenu);
    }
	chdir( Temp_CurrentDir );
#endif
    return true;
}

bool use_save_file_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (use_save_file) use_save_file=false;
    else if (savefilename.size()) use_save_file=true;
    else {
        browse_save_file_menu_callback(menu, menuitem);
        if (savefilename.size()) use_save_file=true;
    }
    mainMenu.get_item("usesavefile").check(use_save_file).refresh_item(mainMenu);
    mainMenu.get_item("browsesavefile").enable(use_save_file).refresh_item(mainMenu);
    std::string slot="";
    for (int i=0; i<=9; i++) {
        slot="slot"+std::to_string(i);
        mainMenu.get_item(slot).enable(!use_save_file).refresh_item(mainMenu);
    }
    return true;
}

void refresh_slots() {
    mainMenu.get_item("current_page").set_text("Current page: "+to_string(page+1)+"/10").refresh_item(mainMenu);
	for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
		char name[6]="slot0";
		name[4]='0'+i;
		std::string command=SaveState::instance().getName(page*SaveState::SLOT_COUNT+i);
		std::string str="Slot "+to_string(page*SaveState::SLOT_COUNT+i+1)+(command==""?"":" "+command);
		mainMenu.get_item(name).set_text(str.c_str()).refresh_item(mainMenu);
	}
}

bool refresh_slots_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    refresh_slots();
    return true;
}

bool remove_state_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    SaveState::instance().removeState(GetGameState_Run());
    return true;
}

bool first_page_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    if (page>0) {
        char name[6]="slot0";
        name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
        mainMenu.get_item(name).check(false).refresh_item(mainMenu);
        page=0;
        if (GetGameState_Run()/SaveState::SLOT_COUNT==page) {
            name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
            mainMenu.get_item(name).check(true).refresh_item(mainMenu);
        }
        refresh_slots_menu_callback(menu, menuitem);
    }
    return true;
}

bool last_page_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    if (page<SaveState::MAX_PAGE-1) {
        char name[6]="slot0";
        name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
        mainMenu.get_item(name).check(false).refresh_item(mainMenu);
        page=SaveState::MAX_PAGE-1;
        if (GetGameState_Run()/SaveState::SLOT_COUNT==page) {
            name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
            mainMenu.get_item(name).check(true).refresh_item(mainMenu);
        }
        refresh_slots_menu_callback(menu, menuitem);
    }
    return true;
}

bool prev_page_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
	char name[6]="slot0";
	name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
	mainMenu.get_item(name).check(false).refresh_item(mainMenu);
    page=(page+SaveState::MAX_PAGE-1)%SaveState::MAX_PAGE;
    if (GetGameState_Run()/SaveState::SLOT_COUNT==page) {
        name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
        mainMenu.get_item(name).check(true).refresh_item(mainMenu);
    }
    refresh_slots_menu_callback(menu, menuitem);
    return true;
}

bool next_page_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
	char name[6]="slot0";
	name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
	mainMenu.get_item(name).check(false).refresh_item(mainMenu);
    page=(page+1)%SaveState::MAX_PAGE;
    if (GetGameState_Run()/SaveState::SLOT_COUNT==page) {
        name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
        mainMenu.get_item(name).check(true).refresh_item(mainMenu);
    }
    refresh_slots_menu_callback(menu, menuitem);
    return true;
}

#if defined(LINUX)
bool x11_on_top = false;
#endif

#if defined(MACOSX) && !defined(C_SDL2)
bool macosx_on_top = false;
#endif

bool is_always_on_top(void) {
#if defined(_WIN32) && !defined(C_SDL2)
    DWORD dwExStyle = ::GetWindowLong(GetHWND(), GWL_EXSTYLE);
    return !!(dwExStyle & WS_EX_TOPMOST);
#elif defined(MACOSX) && !defined(C_SDL2)
    return macosx_on_top;
#elif defined(LINUX)
    return x11_on_top;
#else
    return false;
#endif
}

#if defined(_WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void sdl1_hax_set_topmost(unsigned char topmost);
#endif
#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void sdl1_hax_set_topmost(unsigned char topmost);
#endif
#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
extern "C" void sdl1_hax_macosx_highdpi_set_enable(const bool enable);
#endif

void toggle_always_on_top(void) {
    bool cur = is_always_on_top();
#if defined(_WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    sdl1_hax_set_topmost(!cur);
#elif defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    sdl1_hax_set_topmost(macosx_on_top = (!cur));
#elif defined(LINUX)
    void LinuxX11_OnTop(bool f);
    LinuxX11_OnTop(x11_on_top = (!cur));
#else
    (void)cur;
#endif
}

void BlankDisplay(void);

bool refreshtest_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)menuitem;
    (void)xmenu;

    BlankDisplay();

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif

    return true;
}

bool showdetails_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)xmenu;//UNUSED
    (void)menuitem;//UNUSED
    menu.showrt = !(menu.hidecycles = !menu.hidecycles);
    GFX_SetTitle((int32_t)(CPU_CycleAutoAdjust?CPU_CyclePercUsed:CPU_CycleMax), -1, -1, false);
    mainMenu.get_item("showdetails").check(!menu.hidecycles).refresh_item(mainMenu);
    return true;
}

bool alwaysontop_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    toggle_always_on_top();
    mainMenu.get_item("alwaysontop").check(is_always_on_top()).refresh_item(mainMenu);
    return true;
}

bool highdpienable_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    void RENDER_CallBack( GFX_CallBackFunctions_t function );

    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

#if defined(MACOSX) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    dpi_aware_enable = !dpi_aware_enable;
    if (!control->opt_disable_dpi_awareness) {
        sdl1_hax_macosx_highdpi_set_enable(dpi_aware_enable);
        RENDER_CallBack(GFX_CallBackReset);
    }
#endif

    mainMenu.get_item("highdpienable").check(dpi_aware_enable).refresh_item(mainMenu);
    return true;
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

bool sendkey_mapper_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    if (menuitem->get_name()=="sendkey_mapper_winlogo") sendkeymap=1;
    else if (menuitem->get_name()=="sendkey_mapper_winmenu") sendkeymap=2;
    else if (menuitem->get_name()=="sendkey_mapper_alttab") sendkeymap=3;
    else if (menuitem->get_name()=="sendkey_mapper_ctrlesc") sendkeymap=4;
    else if (menuitem->get_name()=="sendkey_mapper_ctrlbreak") sendkeymap=5;
    else if (menuitem->get_name()=="sendkey_mapper_cad") sendkeymap=0;
    mainMenu.get_item("sendkey_mapper_winlogo").check(menuitem->get_name()=="sendkey_mapper_winlogo").refresh_item(mainMenu);
    mainMenu.get_item("sendkey_mapper_winmenu").check(menuitem->get_name()=="sendkey_mapper_winmenu").refresh_item(mainMenu);
    mainMenu.get_item("sendkey_mapper_alttab").check(menuitem->get_name()=="sendkey_mapper_alttab").refresh_item(mainMenu);
    mainMenu.get_item("sendkey_mapper_ctrlesc").check(menuitem->get_name()=="sendkey_mapper_ctrlesc").refresh_item(mainMenu);
    mainMenu.get_item("sendkey_mapper_ctrlbreak").check(menuitem->get_name()=="sendkey_mapper_ctrlbreak").refresh_item(mainMenu);
    mainMenu.get_item("sendkey_mapper_cad").check(menuitem->get_name()=="sendkey_mapper_cad").refresh_item(mainMenu);
    return true;
}

bool sendkey_preset_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    SendKey(menuitem->get_name());
    return true;
}

void update_all_shortcuts(), DOSBox_SetSysMenu();
bool hostkey_preset_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    if (menuitem->get_name()=="hostkey_ctrlalt") hostkeyalt=1;
    else if (menuitem->get_name()=="hostkey_ctrlshift") hostkeyalt=2;
    else if (menuitem->get_name()=="hostkey_altshift") hostkeyalt=3;
    else hostkeyalt=0;
    mainMenu.get_item("hostkey_ctrlalt").check(hostkeyalt==1).refresh_item(mainMenu);
    mainMenu.get_item("hostkey_ctrlshift").check(hostkeyalt==2).refresh_item(mainMenu);
    mainMenu.get_item("hostkey_altshift").check(hostkeyalt==3).refresh_item(mainMenu);
    mainMenu.get_item("hostkey_mapper").check(hostkeyalt==0).refresh_item(mainMenu);
    update_all_shortcuts();
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.rebuild();
#endif
#if defined(WIN32) && !defined(HX_DOS)
    DOSBox_SetSysMenu();
#endif
    return true;
}

bool help_open_url_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    std::string url="";
    if (menuitem->get_name() == "help_homepage")
        url="https://dosbox-x.com/";
    else if (menuitem->get_name() == "help_wiki")
        url="https://dosbox-x.com/wiki";
    else if (menuitem->get_name() == "help_issue")
        url="https://github.com/joncampbell123/dosbox-x/issues";
    if (url.size()) {
#if defined(WIN32)
      ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(LINUX)
      int ret = system(("xdg-open "+url).c_str());
      return WIFEXITED(ret) && WEXITSTATUS(ret);
#elif defined(MACOSX)
      int ret = system(("open "+url).c_str());
      return WIFEXITED(ret) && WEXITSTATUS(ret);
#endif
    }

    return true;
}

bool help_intro_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(34);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    return true;
}

bool help_about_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(35);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    return true;
}

std::string helpcmd="";
bool help_command_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const menuitem) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    bool switchfs=false;
#if defined(C_SDL2)
    int x=-1, y=-1;
#endif
    if (!GFX_IsFullscreen()) {
        switchfs=true;
#if defined(C_SDL2)
        SDL_GetWindowPosition(sdl.window, &x, &y);
#endif
        GFX_SwitchFullScreen();
    }
    helpcmd = menuitem->get_name().substr(8);
    GUI_Shortcut(36);
    helpcmd = "";
    if (switchfs) {
        GFX_SwitchFullScreen();
#if defined(C_SDL2)
        if (x>-1&&y>-1) SDL_SetWindowPosition(sdl.window, x, y);
#endif
    }

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    return true;
}

extern std::string niclist;
bool help_nic_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    bool switchfs=false;
#if defined(C_SDL2)
    int x=-1, y=-1;
#endif
    if (niclist.find("-------------")!=std::string::npos&&!GFX_IsFullscreen()) {
        switchfs=true;
#if defined(C_SDL2)
        SDL_GetWindowPosition(sdl.window, &x, &y);
#endif
        GFX_SwitchFullScreen();
    }
    GUI_Shortcut(37);
    if (switchfs) {
        GFX_SwitchFullScreen();
#if defined(C_SDL2)
        if (x>-1&&y>-1) SDL_SetWindowPosition(sdl.window, x, y);
#endif
    }

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    return true;
}

void SetCyclesCount_mapper_shortcut_RunInternal(void) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    GUI_Shortcut(16);

    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();
}

void SetCyclesCount_mapper_shortcut_RunEvent(Bitu /*val*/) {
    KEYBOARD_ClrBuffer();   //Clear buffer
    GFX_LosingFocus();      //Release any keys pressed (buffer gets filled again).
    SetCyclesCount_mapper_shortcut_RunInternal();
}

void SetCyclesCount_mapper_shortcut(bool pressed) {
    if (!pressed) return;
    PIC_AddEvent(SetCyclesCount_mapper_shortcut_RunEvent, 0.0001f); //In case mapper deletes the key object that ran it
}

void AspectRatio_mapper_shortcut(bool pressed) {
    if (!pressed) return;

    if (!GFX_GetPreventFullscreen()) {
        SetVal("render", "aspect", render.aspect ? "false" : "true");
    }
}

void HideMenu_mapper_shortcut(bool pressed) {
    if (!pressed) return;

    void ToggleMenu(bool pressed);
    ToggleMenu(true);

    mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
}

void OutputSettingMenuUpdate(void) {
    mainMenu.get_item("output_surface").check(sdl.desktop.want_type == SCREEN_SURFACE).refresh_item(mainMenu);
#if C_DIRECT3D
    mainMenu.get_item("output_direct3d").check(sdl.desktop.want_type == SCREEN_DIRECT3D).refresh_item(mainMenu);
#endif
#if C_OPENGL
    mainMenu.get_item("output_opengl").check(sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.bilinear).refresh_item(mainMenu);
    mainMenu.get_item("output_openglnb").check(sdl.desktop.want_type == SCREEN_OPENGL && !sdl_opengl.bilinear).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    mainMenu.get_item("output_ttf").check(sdl.desktop.want_type == SCREEN_TTF).refresh_item(mainMenu);
#endif
}

bool custom_bios = false;

// OK why isn't this being set for Linux??
#ifndef SDL_MAIN_NOEXCEPT
#define SDL_MAIN_NOEXCEPT
#endif

//extern void UI_Init(void);
void grGlideShutdown(void);
int main(int argc, char* argv[]) SDL_MAIN_NOEXCEPT {
    CommandLine com_line(argc,argv);
    Config myconf(&com_line);

#if defined(WIN32) && !defined(HX_DOS)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
#endif

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
    sdl.srcAspect.x = 4; sdl.srcAspect.y = 3; 
    sdl.srcAspect.xToY = (double)sdl.srcAspect.x / sdl.srcAspect.y;
    sdl.srcAspect.yToX = (double)sdl.srcAspect.y / sdl.srcAspect.x;

    control=&myconf;
#if defined(WIN32) && !defined(HX_DOS)
    /* Microsoft's IME does not play nice with DOSBox */
    ImmDisableIME((DWORD)(-1));
#endif

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
    }

    /* If we were launched by the Finder, the current working directory will usually be
       the root of the filesystem (/) which is useless. If we see that, change instead
       to the user's home directory */
    {
        char *home = getenv("HOME");
        char cwd[512];

        cwd[0]=0;
        getcwd(cwd,sizeof(cwd)-1);

        if (!strcmp(cwd,"/")) {
            /* Only the Finder would do that.
               Even if the user somehow did this from the Terminal app, it's still
               worth changing to the home directory because certain directories
               including / are locked readonly even for sudo in macOS */
            /* NTS: HOME is usually an absolute path */
            if (home != NULL) chdir(home);
        }
    }
#endif

    {
        std::string tmp,config_path,config_combined;

        /* -- parse command line arguments */
        if (!DOSBOX_parse_argv()) return 1;

        if (control->opt_time_limit > 0)
            time_limit_ms = (Bitu)(control->opt_time_limit * 1000);

        if (control->opt_console)
            DOSBox_ShowConsole();

        /* -- Handle some command line options */
        if (control->opt_eraseconf || control->opt_resetconf)
            eraseconfigfile();
        if (control->opt_printconf)
            printconfiglocation();
        if (control->opt_erasemapper || control->opt_resetmapper)
            erasemapperfile();

        /* -- Early logging init, in case these details are needed to debug problems at this level */
        /*    If --early-debug was given this opens up logging to STDERR until Log::Init() */
        LOG::EarlyInit();

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

        /* -- Init the configuration system and add default values */
        CheckNumLockState();
        CheckCapsLockState();
        CheckScrollLockState();

        /* -- setup the config sections for config parsing */
        SDL_SetupConfigSection();
        LOG::SetupConfigSection();
        DOSBOX_SetupConfigSections();

        /* -- Parse configuration files */
        Cross::GetPlatformConfigDir(config_path);

        /* -- -- first the user config file */
        if (control->opt_userconf) {
            tmp.clear();
            Cross::GetPlatformConfigDir(config_path);
            Cross::GetPlatformConfigName(tmp);
            config_combined = config_path + tmp;

            LOG(LOG_MISC,LOG_DEBUG)("Loading config file according to -userconf from %s",config_combined.c_str());
            control->ParseConfigFile(config_combined.c_str());
            if (!control->configfiles.size()) {
                //Try to create the userlevel configfile.
                tmp.clear();
                Cross::CreatePlatformConfigDir(config_path);
                Cross::GetPlatformConfigName(tmp);
                config_combined = config_path + tmp;

                LOG(LOG_MISC,LOG_DEBUG)("Attempting to write config file according to -userconf, to %s",config_combined.c_str());
                if (control->PrintConfig(config_combined.c_str())) {
                    LOG(LOG_MISC,LOG_NORMAL)("Generating default configuration. Writing it to %s",config_combined.c_str());
                    //Load them as well. Makes relative paths much easier
                    control->ParseConfigFile(config_combined.c_str());
                }
            }
        }

        /* -- -- second the -conf switches from the command line */
        for (size_t si=0;si < control->config_file_list.size();si++) {
            std::string &cfg = control->config_file_list[si];
            if (!control->ParseConfigFile(cfg.c_str())) {
                // try to load it from the user directory
                control->ParseConfigFile((config_path + cfg).c_str());
                if (!control->ParseConfigFile((config_path + cfg).c_str())) {
                LOG_MSG("CONFIG: Can't open specified config file: %s",cfg.c_str());
                }
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

        /* -- -- if none found, use userlevel conf */
        if (!control->configfiles.size()) {
            tmp.clear();
            Cross::GetPlatformConfigName(tmp);
            control->ParseConfigFile((config_path + tmp).c_str());
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
							LOG_MSG("Redirected \"%s\" from [dos] to [config] section\n", trim(linestr));
						}
					}
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


			// attempt to split off the first word
			std::string::size_type spcpos = pvars[0].find_first_of(' ');
			if (spcpos>1&&pvars[0].c_str()[spcpos-1]==',')
				spcpos=pvars[0].find_first_of(' ', spcpos+1);

			std::string::size_type equpos = pvars[0].find_first_of('=');

			if ((equpos != std::string::npos) && 
				((spcpos == std::string::npos) || (equpos < spcpos))) {
				// If we have a '=' possibly before a ' ' split on the =
				pvars.insert(pvars.begin()+1,pvars[0].substr(equpos+1));
				pvars[0].erase(equpos);
				// As we had a = the first thing must be a property now
				Section* sec=control->GetSectionFromProperty(pvars[0].c_str());
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
				Section* sec = control->GetSection(pvars[0].c_str());
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
						if (sec3) {
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
        LOG(LOG_MISC,LOG_NORMAL)(("Copyright 2011-%s The DOSBox-X Team. Project maintainer: joncampbell123 (The Great Codeholio). DOSBox-X published under GNU GPL."),std::string(COPYRIGHT_END_YEAR).c_str());

#if defined(MACOSX)
        LOG_MSG("macOS EXE path: %s",MacOSXEXEPath.c_str());
        LOG_MSG("macOS Resource path: %s",MacOSXResPath.c_str());
#endif

        /* -- [debug] setup console */
#if C_DEBUG
# if defined(WIN32) && !defined(HX_DOS)
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
            if (id == 1) menu.compatible=true;

            /* use all variables to shut up the compiler about unused vars */
            LOG(LOG_MISC,LOG_DEBUG)("DOSBox_CheckOS results: id=%u major=%u minor=%u",id,major,minor);
        }
#endif

        /* -- SDL init hackery */
#if SDL_VERSION_ATLEAST(1, 2, 14)
        /* hack: On debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
         * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version of libsdl */
        putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
        LOG(LOG_GUI,LOG_DEBUG)("SDL 1.2.14 hack: SDL_DISABLE_LOCK_KEYS=1");
#endif

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
        osx_detect_nstouchbar();/*assigns to has_touch_bar_support*/
        if (has_touch_bar_support) {
            LOG_MSG("macOS: NSTouchBar support detected in system");
            osx_init_touchbar();
        }

        extern void osx_init_dock_menu(void);
        osx_init_dock_menu();

        void qz_set_match_monitor_cb(void);
        qz_set_match_monitor_cb();
#endif

        /* -- SDL init */
        if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE) >= 0)
            sdl.inited = true;
        else
            E_Exit("Can't init SDL %s",SDL_GetError());

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

        /* -- -- Initialise Joystick and CD-ROM seperately. This way we can warn when it fails instead of exiting the application */
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
        if (Compat_SDL_CDROMInit() < 0) {
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

#if defined(WIN32) && !defined(C_SDL2)
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

#if !defined(C_SDL2)
# if defined(WIN32)
        Reflect_Menu();
# endif
#endif

        /* stock top-level menu items */
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainMenu");
            item.set_text("Main");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainSendKey");
                item.set_text("Send special key");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainHostKey");
                item.set_text("Select host key");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"WheelToArrow");
                item.set_text("Mouse wheel movements");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"SharedClipboard");
                item.set_text("Shared clipboard functions");
            }
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuMenu");
            item.set_text("CPU");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuCoreMenu");
                item.set_text("CPU core");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuTypeMenu");
                item.set_text("CPU type");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuSpeedMenu");
                item.set_text("Emulate CPU speed");
            }
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu88-4").set_text("8088 XT 4.77MHz (~240 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-8").set_text("286 8MHz (~750 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-12").set_text("286 12MHz (~1510 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-25").set_text("286 25MHz (~3300 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu386-25").set_text("386DX 25MHz (~4595 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu386-33").set_text("386DX 33MHz (~6075 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-33").set_text("486DX 33MHz (~12010 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-66").set_text("486DX2 66MHz (~23880 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-100").set_text("486DX4 100MHz (~33445 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-133").set_text("486DX5 133MHz (~47810 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-60").set_text("Pentium 60MHz (~31545 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-66").set_text("Pentium 66MHz (~35620 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-75").set_text("Pentium 75MHz (~43500 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-90").set_text("Pentium 90MHz (~52000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-100").set_text("Pentium 100MHz (~60000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-120").set_text("Pentium 120MHz (~74000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-133").set_text("Pentium 133MHz (~80000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-166").set_text("Pentium 166MHz MMX (~97240 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoMenu");
            item.set_text("Video");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoFrameskipMenu");
                item.set_text("Frameskip");
        
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"frameskip_0").set_text("Off").
                    set_callback_function(video_frameskip_common_menu_callback);

                for (unsigned int f=1;f <= 10;f++) {
                    char tmp1[64],tmp2[64];

                    sprintf(tmp1,"frameskip_%u",f);
                    sprintf(tmp2,"%u frame",f);

                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,tmp1).set_text(tmp2).
                        set_callback_function(video_frameskip_common_menu_callback);
                }
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoScalerMenu");
                item.set_text("Scaler");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"scaler_forced").set_text("Force scaler").
                    set_callback_function(scaler_forced_menu_callback);

                for (size_t i=0;scaler_menu_opts[i][0] != NULL;i++) {
                    const std::string name = std::string("scaler_set_") + scaler_menu_opts[i][0];

                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(scaler_menu_opts[i][1]).
                        set_callback_function(scaler_set_menu_callback);
                }
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoOutputMenu");
                item.set_text("Output");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_surface").set_text("Surface").
                    set_callback_function(output_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_direct3d").set_text("Direct3D").
                    set_callback_function(output_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_opengl").set_text("OpenGL").
                    set_callback_function(output_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_openglnb").set_text("OpenGL NB").
                    set_callback_function(output_menu_callback);
#if defined(USE_TTF)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_ttf").set_text("TrueType font").
                    set_callback_function(output_menu_callback);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"doublescan").set_text("Doublescan").
                    set_callback_function(doublescan_menu_callback);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoVsyncMenu");
                item.set_text("V-Sync");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_on").set_text("On").
                    set_callback_function(vsync_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_force").set_text("Force").
                    set_callback_function(vsync_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_host").set_text("Host").
                    set_callback_function(vsync_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_off").set_text("Off").
                    set_callback_function(vsync_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_set_syncrate").set_text("Set syncrate").
                    set_callback_function(vsync_set_syncrate_menu_callback);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoOverscanMenu");
                item.set_text("Overscan");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"overscan_0").set_text("Off").
                    set_callback_function(overscan_menu_callback);

                for (size_t i=1;i <= 10;i++) {
                    char tmp1[64],tmp2[64];

                    sprintf(tmp1,"overscan_%zu",i);
                    sprintf(tmp2,"%zu",i);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,tmp1).set_text(tmp2).
                        set_callback_function(overscan_menu_callback);
                }
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoTextmodeMenu");
                item.set_text("Text-mode");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clear_screen").set_text("Clear the screen").
                    set_callback_function(clear_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vga_9widetext").set_text("Allow 9-pixel wide fonts").
                    set_callback_function(vga_9widetext_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"text_background").set_text("High intensity: background color").
                    set_callback_function(intensity_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"text_blinking").set_text("High intensity: blinking text").
                    set_callback_function(intensity_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x25").set_text("Screen: 80 columns x 25 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x43").set_text("Screen: 80 columns x 43 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x50").set_text("Screen: 80 columns x 50 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x60").set_text("Screen: 80 columns x 60 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x25").set_text("Screen: 132 columns x 25 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x43").set_text("Screen: 132 columns x 43 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x50").set_text("Screen: 132 columns x 50 lines").
                    set_callback_function(lines_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x60").set_text("Screen: 132 columns x 60 lines").
                    set_callback_function(lines_menu_callback);
            }
#if defined(USE_TTF)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoTTFMenu");
                item.set_text("TTF options");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showbold").set_text("Display bold text in TTF").
                    set_callback_function(ttf_style_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showital").set_text("Display italic text in TTF").
                    set_callback_function(ttf_style_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showline").set_text("Display underlined text in TTF").
                    set_callback_function(ttf_style_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showsout").set_text("Display strikeout text in TTF").
                    set_callback_function(ttf_style_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpno").set_text("TTF word processor: None").
                    set_callback_function(ttf_wp_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpwp").set_text("TTF word processor: WordPerfect").
                    set_callback_function(ttf_wp_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpws").set_text("TTF word processor: WordStar").
                    set_callback_function(ttf_wp_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpxy").set_text("TTF word processor: XyWrite").
                    set_callback_function(ttf_wp_change_callback);
            }
#endif
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoPC98Menu");
                item.set_text("PC-98 options");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_5mhz_gdc").set_text("5MHz GDC clock").
                    set_callback_function(vid_pc98_5mhz_gdc_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_allow_200scanline").set_text("Allow 200-line scanline effect").
                    set_callback_function(vid_pc98_200scanline_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_allow_4partitions").set_text("Allow 4 display partitions in graphics layer").
                    set_callback_function(vid_pc98_4parts_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_egc").set_text("Enable EGC").
                    set_callback_function(vid_pc98_enable_egc_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_grcg").set_text("Enable GRCG").
                    set_callback_function(vid_pc98_enable_grcg_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_analog").set_text("Enable analog display").
                    set_callback_function(vid_pc98_enable_analog_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_analog256").set_text("Enable analog 256-color display").
                    set_callback_function(vid_pc98_enable_analog256_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_188user").set_text("Enable 188+ user CG cells").
                    set_callback_function(vid_pc98_enable_188user_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_clear_text").set_text("Clear text layer").
                    set_callback_function(vid_pc98_cleartext_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_clear_graphics").set_text("Clear graphics layer").
                    set_callback_function(vid_pc98_graphics_menu_callback);
            }
#ifdef C_D3DSHADERS
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_d3d_shader").set_text("Select Direct3D pixel shader...").
                    set_callback_function(vid_select_pixel_shader_menu_callback);
            }
#endif
#ifdef C_OPENGL
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_glsl_shader").set_text("Select OpenGL (GLSL) shader...").
                    set_callback_function(vid_select_glsl_shader_menu_callback);
            }
#endif
#ifdef USE_TTF
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_ttf_font").set_text("Select TrueType font (TTF)...").
                    set_callback_function(vid_select_ttf_font_menu_callback);
            }
#endif
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"SoundMenu");
            item.set_text("Sound");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_swapstereo").set_text("Swap stereo").
                    set_callback_function(mixer_swapstereo_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_mute").set_text("Mute").
                    set_callback_function(mixer_mute_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_info").set_text("Show sound mixer volumes").
                    set_callback_function(mixer_info_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sb_info").set_text("Show Sound Blaster configuration").
                    set_callback_function(sb_device_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"midi_info").set_text("Show MIDI device configuration").
                    set_callback_function(midi_device_menu_callback);
            }
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSMenu");
            item.set_text("DOS");

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSMouseMenu");
                item.set_text("Mouse emulation");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_enable_int33").set_text("Internal Emulation").
                        set_callback_function(dos_mouse_enable_int33_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_y_axis_reverse").set_text("Y-axis Reverse").
                        set_callback_function(dos_mouse_y_axis_reverse_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_sensitivity").set_text("Sensitivity").
                        set_callback_function(dos_mouse_sensitivity_menu_callback);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSVerMenu");
                item.set_text("Reported DOS version");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_330").set_text("3.30").
                        set_callback_function(dos_ver_set_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_500").set_text("5.00").
                        set_callback_function(dos_ver_set_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_622").set_text("6.22").
                        set_callback_function(dos_ver_set_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_710").set_text("7.10").
                        set_callback_function(dos_ver_set_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_edit").set_text("Edit").
                        set_callback_function(dos_ver_edit_menu_callback);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSLFNMenu");
                item.set_text("Long filename support");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_auto").set_text("Auto per reported DOS version").
                        set_callback_function(dos_lfn_auto_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_enable").set_text("Enable long filename emulation").
                        set_callback_function(dos_lfn_enable_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_disable").set_text("Disable long filename emulation").
                        set_callback_function(dos_lfn_disable_menu_callback);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSPC98Menu");
                item.set_text("PC-98 PIT master clock");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_pc98_pit_4mhz").set_text("4MHz/8MHz PIT master clock").
                        set_callback_function(dos_pc98_clock_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_pc98_pit_5mhz").set_text("5MHz/10MHz PIT master clock").
                        set_callback_function(dos_pc98_clock_menu_callback);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSEMSMenu");
                item.set_text("Expanded memory (EMS)");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_true").set_text("Enable EMS emulation").
                        set_callback_function(dos_ems_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_board").set_text("EMS board emulation").
                        set_callback_function(dos_ems_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_emm386").set_text("EMM386 emulation").
                        set_callback_function(dos_ems_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_false").set_text("Disable EMS emulation").
                        set_callback_function(dos_ems_menu_callback);
                }
            }

#if defined(WIN32) && !defined(HX_DOS)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSWinMenu");
                item.set_text("Windows host applications");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_autorun").set_text("Launch to run on the Windows host").
                        set_callback_function(dos_win_autorun_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_wait").set_text("Wait for the application if possible").
                        set_callback_function(dos_win_wait_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_quiet").set_text("Quiet mode - no start messages").
                        set_callback_function(dos_win_quiet_menu_callback);
                }
            }
#endif
        }
#if !defined(C_EMSCRIPTEN)
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CaptureMenu");
            item.set_text("Capture");
        }
#endif
# if (C_SSHOT)
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CaptureFormatMenu");
            item.set_text("Capture format");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"capture_fmt_avi_zmbv").set_text("AVI + ZMBV").
                    set_callback_function(capture_fmt_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"capture_fmt_mpegts_h264").set_text("MPEG-TS + H.264").
                    set_callback_function(capture_fmt_menu_callback).
#  if (C_AVCODEC)
                enable(true);
#  else
                enable(false);
#  endif
            }
        }
# endif
		{
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"saveoptionmenu");
            item.set_text("Save/load options");
        }
		{
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"saveslotmenu");
            item.set_text("Select save slot");
		}
        {
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"noremark_savestate").set_text("No remark when saving state").set_callback_function(noremark_savestate_menu_callback).check(noremark_save_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"force_loadstate").set_text("No warning when loading state").set_callback_function(force_loadstate_menu_callback).check(force_load_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"removestate").set_text("Remove state in slot").set_callback_function(remove_state_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"refreshslot").set_text("Refresh display status").set_callback_function(refresh_slots_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "usesavefile").set_text("Use save file instead of save slot").set_callback_function(use_save_file_menu_callback).check(use_save_file);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "browsesavefile").set_text("Browse save file...").set_callback_function(browse_save_file_menu_callback).enable(use_save_file);
            {
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"current_page").set_text("Current page: 1/10").enable(false).set_callback_function(refresh_slots_menu_callback);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"prev_page").set_text("Previous page").set_callback_function(prev_page_menu_callback);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"next_page").set_text("Next page").set_callback_function(next_page_menu_callback);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"first_page").set_text("Go to first page").set_callback_function(first_page_menu_callback);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"last_page").set_text("Go to last page").set_callback_function(last_page_menu_callback);
				char name[6]="slot0";
				for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
					name[4]='0'+i;
					std::string command=SaveState::instance().getName(page*SaveState::SLOT_COUNT+i);
					std::string str="Slot "+to_string(page*SaveState::SLOT_COUNT+i+1)+(command==""?"":" "+command);
					mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(str.c_str()).set_callback_function(save_slot_callback);
				}
            }
            if (page!=GetGameState_Run()/SaveState::SLOT_COUNT) {
                page=(unsigned int)(GetGameState_Run()/SaveState::SLOT_COUNT);
                refresh_slots();
            }
			char name[6]="slot0";
			name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DriveMenu");
            item.set_text("Drive");

            for (char c='A';c <= 'Z';c++) {
                mountfro[c-'A']=false;
                mountiro[c-'A']=false;
                std::string dmenu = "Drive";
                dmenu += c;

                std::string dmenut;
                dmenut = c;

                DOSBoxMenu::item &ditem = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,dmenu.c_str());
                ditem.set_text(dmenut.c_str());

                for (size_t i=0;drive_opts[i][0] != NULL;i++) {
                    const std::string name = std::string("drive_") + c + "_" + drive_opts[i][0];
                    if ((!strcmp(drive_opts[i][0], "boot")||!strcmp(drive_opts[i][0], "bootimg")||!strcmp(drive_opts[i][0], "div3"))&&(c!='A'&&c!='C'&&c!='D'))
                        continue;
                    if (!strcmp(drive_opts[i][1], "--"))
                        mainMenu.alloc_item(DOSBoxMenu::separator_type_id,name);
                    else
                        mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(drive_opts[i][1]).set_callback_function(drive_callbacks[i]);
                }
            }
        }

        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpMenu");
            item.set_text("Help");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_intro").set_text("Introduction").
                    set_callback_function(help_intro_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_homepage").set_text("DOSBox-X homepage").
                    set_callback_function(help_open_url_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_wiki").set_text("DOSBox-X Wiki guide").
                    set_callback_function(help_open_url_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_issue").set_text("DOSBox-X support").
                    set_callback_function(help_open_url_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_nic").set_text("List network interfaces").
                    set_callback_function(help_nic_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_about").set_text("About DOSBox-X").
                    set_callback_function(help_about_callback);
#if !defined(C_EMSCRIPTEN)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_console").set_text("Show logging console").set_callback_function(show_console_menu_callback);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wait_on_error").set_text("Console wait on error").set_callback_function(wait_on_error_menu_callback).check(sdl.wait_on_error);
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpCommandMenu");

                item.set_text("DOS commands");

                {
                    uint32_t cmd_index=0;
                    while (cmd_list[cmd_index].name) {
                        if (!cmd_list[cmd_index].flags)
                            mainMenu.alloc_item(DOSBoxMenu::item_type_id,("command_"+std::string(cmd_list[cmd_index].name)).c_str()).set_text(cmd_list[cmd_index].name).set_callback_function(help_command_callback);
                        cmd_index++;
                    }
                    cmd_index=0;
                    while (cmd_list[cmd_index].name) {
                        if (cmd_list[cmd_index].flags && strcmp(cmd_list[cmd_index].name, "CHDIR") && strcmp(cmd_list[cmd_index].name, "ERASE") && strcmp(cmd_list[cmd_index].name, "LOADHIGH") && strcmp(cmd_list[cmd_index].name, "MKDIR") && strcmp(cmd_list[cmd_index].name, "RMDIR") && strcmp(cmd_list[cmd_index].name, "RENAME") && strcmp(cmd_list[cmd_index].name, "DX-CAPTURE") && strcmp(cmd_list[cmd_index].name, "DEBUGBOX"))
                            mainMenu.alloc_item(DOSBoxMenu::item_type_id,("command_"+std::string(cmd_list[cmd_index].name)).c_str()).set_text(cmd_list[cmd_index].name).set_callback_function(help_command_callback);
                        cmd_index++;
                    }
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpDebugMenu");

                item.set_text("Debugging options");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_blankrefreshtest").set_text("Refresh test (blank display)").set_callback_function(refreshtest_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logint21").set_text("Log INT 21h calls").
                        set_callback_function(dos_debug_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logfileio").set_text("Log file I/O").
                        set_callback_function(dos_debug_menu_callback);
                }
            }
        }

        if (control->opt_startui)
            GUI_Run(false);
        if (control->opt_editconf.length() != 0)
            launcheditor(control->opt_editconf);
        if (control->opt_opencaptures.length() != 0)
            launchcaptures(control->opt_opencaptures);
        if (control->opt_opensaves.length() != 0)
            launchsaves(control->opt_opensaves);

        {
            /* Some extra SDL Functions */
            Section_prop* sdl_sec = static_cast<Section_prop*>(control->GetSection("sdl"));

            if (control->opt_fullscreen || sdl_sec->Get_bool("fullscreen")) {
                LOG(LOG_MISC, LOG_DEBUG)("Going fullscreen immediately, during startup");

#if defined(WIN32)
                void DOSBox_SetSysMenu(void);
                DOSBox_SetSysMenu();
#endif
                //only switch if not already in fullscreen
                if (!sdl.desktop.fullscreen) GFX_SwitchFullScreen();
            }

            // Shows menu bar (window)
            menu.startup = true;

            menu.showrt = control->opt_showrt||sdl_sec->Get_bool("showdetails");
            menu.hidecycles = (control->opt_showcycles||sdl_sec->Get_bool("showdetails") ? false : true);
        }

        /* Start up main machine */

#if defined(WIN32) && !defined(C_SDL2)
        {
            Section_prop *sec = static_cast<Section_prop *>(control->GetSection("dosbox"));
            enable_hook_special_keys = sec->Get_bool("keyboard hook");
        }
#endif

        MSG_Init();
        MAPPER_StartUp();
        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();

        /* at this point: If the machine type is PC-98, and the mapper keyboard layout was "Japanese",
         * then change the mapper layout to "Japanese PC-98" */
        if (host_keyboard_layout == DKM_JPN && IS_PC98_ARCH)
            SetMapperKeyboardLayout(DKM_JPN_PC98);

        /* more */
        {
            DOSBoxMenu::item *item;

            MAPPER_AddHandler(&HideMenu_mapper_shortcut, MK_escape, MMODHOST, "togmenu", "Toggle menu bar", &item);
            item->set_text("Hide/show menu bar");

            MAPPER_AddHandler(&PauseWithInterrupts_mapper_shortcut, MK_nothing, 0, "pauseints", "Pause with interrupt", &item);
            item->set_text("Pause with interrupts enabled");
        }

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
        INNOVA_Init();
        BIOS_Init();
        INT10_Init();
        SERIAL_Init();
        DONGLE_Init();
#if C_PRINTER
        PRINTER_Init();
#endif
        PARALLEL_Init();
#if C_NE2000
        NE2K_Init();
#endif

#if defined(WIN32) && !defined(C_SDL2)
        Reflect_Menu();
#endif

        /* If PCjr emulation, map cartridge ROM */
        if (machine == MCH_PCJR)
            Init_PCJR_CartridgeROM();

        /* let's assume motherboards are sane on boot because A20 gate is ENABLED on first boot */
        MEM_A20_Enable(true);

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

        /* Init memhandle system. This part is used by DOSBox's XMS/EMS emulation to associate handles
         * per page. FIXME: I would like to push this down to the point that it's never called until
         * XMS/EMS emulation needs it. I would also like the code to free the mhandle array immediately
         * upon booting into a guest OS, since memory handles no longer have meaning in the guest OS
         * memory layout. */
        Init_MemHandles();

        /* finally, the mapper */
        MAPPER_Init();
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"hostkey_mapper").set_text("Mapper-defined").set_callback_function(hostkey_preset_menu_callback);

        /* stop at this point, and show the mapper, if instructed */
        if (control->opt_startmapper) {
            LOG(LOG_MISC,LOG_DEBUG)("Running mapper interface, during startup, as instructed");
            MAPPER_RunInternal();
        }

        /* more */
        std::string doubleBufString = std::string("desktop.doublebuf");
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"showdetails").set_text("Show FPS and RT speed in title bar").set_callback_function(showdetails_menu_callback).check(!menu.hidecycles && menu.showrt);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"auto_lock_mouse").set_text("Autolock mouse").set_callback_function(autolock_mouse_menu_callback).check(sdl.mouse.autoenable);
#if defined (WIN32) || defined(C_SDL2)
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_right").set_text("Via right mouse button").set_callback_function(right_mouse_clipboard_menu_callback).check(mbutton==3);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_middle").set_text("Via middle mouse button").set_callback_function(middle_mouse_clipboard_menu_callback).check(mbutton==2);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_arrows").set_text("Via arrow keys (Home=start, End=end)").set_callback_function(arrow_keys_clipboard_menu_callback).check(mbutton==4);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"screen_to_clipboard").set_text("Copy all text on the DOS screen").set_callback_function(screen_to_clipboard_menu_callback);
#endif
#if defined (WIN32)
        if (control->SecureMode()) clipboard_dosapi = false;
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_device").set_text("Enable DOS clipboard device access").set_callback_function(dos_clipboard_device_menu_callback).check(dos_clipboard_device_access==4&&!control->SecureMode());
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_dosapi").set_text("Enable DOS clipboard API for applications").set_callback_function(dos_clipboard_api_menu_callback).check(clipboard_dosapi);
#endif
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_winlogo").set_text("Send logo key").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_winmenu").set_text("Send menu key").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_alttab").set_text("Send Alt+Tab").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_ctrlesc").set_text("Send Ctrl+Esc").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_ctrlbreak").set_text("Send Ctrl+Break").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_cad").set_text("Send Ctrl+Alt+Del").set_callback_function(sendkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"hostkey_ctrlalt").set_text("Ctrl+Alt").set_callback_function(hostkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"hostkey_ctrlshift").set_text("Ctrl+Shift").set_callback_function(hostkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"hostkey_altshift").set_text("Alt+Shift").set_callback_function(hostkey_preset_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_winlogo").set_text("Mapper \"Send special key\": logo key").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_winmenu").set_text("Mapper \"Send special key\": menu key").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_alttab").set_text("Mapper \"Send special key\": Alt+Tab").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_ctrlesc").set_text("Mapper \"Send special key\": Ctrl+Esc").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_ctrlbreak").set_text("Mapper \"Send special key\": Ctrl+Break").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sendkey_mapper_cad").set_text("Mapper \"Send special key\": Ctrl+Alt+Del").set_callback_function(sendkey_mapper_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_updown").set_text("Convert to up/down arrows").set_callback_function(wheel_updown_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_leftright").set_text("Convert to left/right arrows").set_callback_function(wheel_leftright_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_pageupdown").set_text("Convert to PgUp/PgDn keys").set_callback_function(wheel_pageupdown_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_none").set_text("Do not convert to arrow keys").set_callback_function(wheel_none_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_guest").set_text("Enable for guest systems also").set_callback_function(wheel_guest_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"doublebuf").set_text("Double Buffering (Fullscreen)").set_callback_function(doublebuf_menu_callback).check(!!GetSetSDLValue(1, doubleBufString, 0));
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"alwaysontop").set_text("Always on top").set_callback_function(alwaysontop_menu_callback).check(is_always_on_top());
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"highdpienable").set_text("High DPI enable").set_callback_function(highdpienable_menu_callback).check(dpi_aware_enable);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sync_host_datetime").set_text("Synchronize host date/time").set_callback_function(sync_host_datetime_menu_callback).check(sync_time);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"shell_config_commands").set_text("Config options as commands").set_callback_function(shell_config_commands_menu_callback).check(enable_config_as_shell_commands);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"quick_reboot").set_text("Enable quick reboot").set_callback_function(quick_reboot_menu_callback).check(use_quick_reboot);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"make_diskimage").set_text("Create blank disk image...").set_callback_function(make_diskimage_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"list_drivenum").set_text("Show mounted drive numbers").set_callback_function(list_drivenum_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"list_ideinfo").set_text("Show IDE disk or CD status").set_callback_function(list_ideinfo_menu_callback);

        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_use_uskb").set_text("Use US keyboard layout").set_callback_function(pc98_force_uskb_menu_callback).check(pc98_force_ibm_layout);

        mainMenu.get_item("wheel_updown").check(wheel_key==1).refresh_item(mainMenu);
        mainMenu.get_item("wheel_leftright").check(wheel_key==2).refresh_item(mainMenu);
        mainMenu.get_item("wheel_pageupdown").check(wheel_key==3).refresh_item(mainMenu);
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
        mainMenu.get_item("hostkey_mapper").check(hostkeyalt==0).set_text("Mapper-defined: "+mapper_keybind).refresh_item(mainMenu);

        bool MENU_get_swapstereo(void);
        mainMenu.get_item("mixer_swapstereo").check(MENU_get_swapstereo()).refresh_item(mainMenu);

        bool MENU_get_mute(void);
        mainMenu.get_item("mixer_mute").check(MENU_get_mute()).refresh_item(mainMenu);

        mainMenu.get_item("scaler_forced").check(render.scale.forced);

        mainMenu.get_item("debug_logint21").check(log_int21);
        mainMenu.get_item("debug_logfileio").check(log_fileio);

        mainMenu.get_item("sync_host_datetime").enable(!IS_PC98_ARCH);
        mainMenu.get_item("vga_9widetext").enable(!IS_PC98_ARCH);
        mainMenu.get_item("doublescan").enable(!IS_PC98_ARCH);

        blinking=static_cast<Section_prop *>(control->GetSection("video"))->Get_bool("high intensity blinking");
        mainMenu.get_item("text_background").enable(!IS_PC98_ARCH).check(!blinking).refresh_item(mainMenu);
        mainMenu.get_item("text_blinking").enable(!IS_PC98_ARCH).check(blinking).refresh_item(mainMenu);
        mainMenu.get_item("line_80x25").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x43").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x50").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_80x60").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x25").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x43").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x50").enable(!IS_PC98_ARCH);
        mainMenu.get_item("line_132x60").enable(!IS_PC98_ARCH);
#if defined(USE_TTF)
        mainMenu.get_item("mapper_ttf_incsize").enable(TTF_using());
        mainMenu.get_item("mapper_ttf_decsize").enable(TTF_using());
        mainMenu.get_item("ttf_showbold").enable(TTF_using()).check(showbold);
        mainMenu.get_item("ttf_showital").enable(TTF_using()).check(showital);
        mainMenu.get_item("ttf_showline").enable(TTF_using()).check(showline);
        mainMenu.get_item("ttf_showsout").enable(TTF_using()).check(showsout);
        mainMenu.get_item("ttf_wpno").enable(TTF_using()).check(!wpType);
        mainMenu.get_item("ttf_wpwp").enable(TTF_using()).check(wpType==1);
        mainMenu.get_item("ttf_wpws").enable(TTF_using()).check(wpType==2);
        mainMenu.get_item("ttf_wpxy").enable(TTF_using()).check(wpType==3);
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
#endif

        OutputSettingMenuUpdate();
        update_pc98_clock_pit_menu();
#if !defined(C_EMSCRIPTEN)
        update_capture_fmt_menu();
#endif

        /* The machine just "powered on", and then reset finished */
        if (!VM_PowerOn()) E_Exit("VM failed to power on");

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
        mainMenu.screenWidth = (unsigned int)sdl.surface->w;
        mainMenu.screenHeight = (unsigned int)sdl.surface->h;
        mainMenu.updateRect();
#endif
#if defined(WIN32) && !defined(HX_DOS)
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
            if (menu_gui && !control->opt_nomenu && cfg_want_menu)
                DOSBox_SetMenu();
            else
                DOSBox_NoMenu();
        }

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        int Reflect_Menu(void);
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

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
        Reflect_Menu();
#endif

        if (dos_kernel_shutdown) {

            if (!IS_PC98_ARCH&&dos.loaded_codepage!=437) dos.loaded_codepage=437;

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

            grGlideShutdown();
            /* shutdown DOSBox-X's virtual drive Z */
            VFILE_Shutdown();

            /* shutdown the programs */
            PROGRAMS_Shutdown();        /* FIXME: Is this safe? Or will this cause use-after-free bug? */

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
            if (!strcmp(RunningProgram, "LOADLIN") && core == "auto")
                cpudecoder=&CPU_Core_Normal_Run;

            /* new code: fire event */
            if (reboot_machine)
                DispatchVMEvent(VM_EVENT_DOS_EXIT_REBOOT_KERNEL);
            else
                DispatchVMEvent(VM_EVENT_DOS_EXIT_KERNEL);

#if defined(WIN32) && !defined(C_SDL2)
            int Reflect_Menu(void);
            Reflect_Menu();
#endif
        }

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
        Reflect_Menu();
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

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
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

#if defined(WIN32) && !defined(C_SDL2)
        int Reflect_Menu(void);
        Reflect_Menu();
#endif

        /* and then shutdown */
        GFX_ShutDown();

        void CPU_Snap_Back_Forget();
        /* Shutdown everything. For shutdown to work properly we must force CPU to real mode */
        CPU_Snap_Back_To_Real_Mode();
        CPU_Snap_Back_Forget();

        /* NTS: The "control" object destructor is called here because the "myconf" object leaves scope.
         * The destructor calls all section destroy functions here. After this point, all sections have
         * freed resources. */
    }

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

    //Force visible mouse to end user. Somehow this sometimes doesn't happen
#if defined(C_SDL2)
    SDL_SetRelativeMouseMode(SDL_FALSE);
#else
    SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
    SDL_ShowCursor(SDL_ENABLE);

    /* Exit functions */
    while (!exitfunctions.empty()) {
        Function_wrapper &ent = exitfunctions.front();

        LOG(LOG_MISC,LOG_DEBUG)("Calling exit function (%p) '%s'",(void*)((uintptr_t)ent.function),ent.name.c_str());
        ent.function(NULL);
        exitfunctions.pop_front();
    }

    LOG::Exit();

#if defined(WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
# if !defined(HX_DOS)
    ShowWindow(GetHWND(), SW_HIDE);
    SDL1_hax_SetMenu(NULL);/* detach menu from window, or else Windows will destroy the menu out from under the C++ class */
# endif
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_NSMENU
    void sdl_hax_macosx_setmenu(void *nsMenu);
    sdl_hax_macosx_setmenu(NULL);
#endif

#if defined(C_SDL2)
	Compat_SDL_CDROMQuit();
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

    return 0;
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
