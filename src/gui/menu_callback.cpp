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

#include <assert.h>

#include "dosbox.h"
#include "menu.h"
#include "menudef.h"
#include "sdlmain.h"
#include "control.h"
#include "logging.h"
#include "callback.h"
#include "keyboard.h"
#include "bios_disk.h"
#include "render.h"
#include "shell.h"
#include "jfont.h"
#include "inout.h"
#include "regs.h"
#include "cpu.h"
#include "../dos/drives.h"
#include "../ints/int10.h"
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#ifdef WIN32
# include "Commdlg.h"
# include "windows.h"
# include "Shellapi.h"
#endif
#ifdef MACOSX
#include <CoreGraphics/CoreGraphics.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <output/output_opengl.h>
#include <output/output_ttf.h>

int oldblinkc = -1;
std::string savefilename = "";

extern SHELL_Cmd cmd_list[];
extern unsigned int page, hostkeyalt, sendkeymap;
extern int posx, posy, wheel_key, mbutton, enablelfn, dos_clipboard_device_access, aspect_ratio_x, aspect_ratio_y, disk_data_rate, floppy_data_rate, lastmsgcp;
extern bool addovl, clearline, pcibus_enable, winrun, window_was_maximized, wheel_guest, clipboard_dosapi, clipboard_biospaste, direct_mouse_clipboard, sync_time, manualtime, pausewithinterrupts_enable, enable_autosave, enable_config_as_shell_commands, noremark_save_state, force_load_state, use_quick_reboot, use_save_file, dpi_aware_enable, pc98_force_ibm_layout, log_int21, log_fileio, x11_on_top, macosx_on_top, rtl, gbk, chinasea, uselangcp;
extern bool mountfro[26], mountiro[26];
extern struct BuiltinFileBlob bfb_GLIDE2X_OVL;
extern const char* RunningProgram;
extern bool video_debug_overlay;

void MSG_Init(void);
void SendKey(std::string key);
void MAPPER_ReleaseAllKeys(void);
void RENDER_Reset(void);
void resetFontSize(void);
void EMS_DoShutDown(void);
void DOSV_FillScreen(void);
void CopyClipboard(int all);
void res_init(void), change_output(int output);
void VFILE_Remove(const char *name,const char *dir = "");
void VOODOO_Destroy(Section* /*sec*/), VOODOO_OnPowerOn(Section* /*sec*/);
void GLIDE_ShutDown(Section* sec), GLIDE_PowerOn(Section* sec);
void DOSBox_ShowConsole(void);
void Load_Language(std::string name);
void RebootLanguage(std::string filename, bool confirm=false);
void MenuBrowseFolder(char drive, std::string const& drive_type);
void MenuBrowseImageFile(char drive, bool arc, bool boot, bool multiple);
void MenuBootDrive(char drive);
void MenuUnmountDrive(char drive);
void DOSBox_SetSysMenu(void);
void makestdcp950table(void);
void makeseacp951table(void);
void SetGameState_Run(int value);
void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
void GFX_LosingFocus(void);
void GFX_ReleaseMouse(void);
void GFX_ForceRedrawScreen(void);
bool GFX_GetPreventFullscreen(void);
bool isDBCSCP(void), toOutput(const char *what), saveDiskImage(imageDisk *image, const char *name);
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
int setTTFMap(bool changecp), FileDirExistCP(const char *name), FileDirExistUTF8(std::string &localname, const char *name);
size_t GetGameState_Run(void);
void DBCSSBCS_mapper_shortcut(bool pressed);
void AutoBoxDraw_mapper_shortcut(bool pressed);
extern std::string langname, GetDOSBoxXPath(bool withexe=false);

void* GetSetSDLValue(int isget, std::string& target, void* setval) {
    if (target == "wait_on_error") {
        if (isget) return (void*) sdl.wait_on_error;
        else sdl.wait_on_error = setval;
    }
    else if (target == "opengl.kind") {
#if C_OPENGL
        if (isget) return (void*) sdl_opengl.kind;
        else sdl_opengl.kind = (GLKind)(intptr_t)setval;
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

bool capture_fmt_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseFolder(drive+'A', "LOCAL");
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseFolder(drive+'A', "CDROM");
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseFolder(drive+'A', "FLOPPY");
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseImageFile(drive+'A', true, false, false);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseImageFile(drive+'A', false, false, false);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseImageFile(drive+'A', false, false, true);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

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

    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    MenuBrowseImageFile(drive+'A', false, true, false);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();

    return true;
}

bool drive_saveimg_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    int drive;
    const char *mname = menuitem->get_name().c_str();
    if (!strncmp(mname,"drive_",6))
        drive = mname[6] - 'A';
    else
        return false;
    if (drive < 0 || drive>=DOS_DRIVES) return false;
    if (!Drives[drive] || dynamic_cast<fatDrive*>(Drives[drive])) {
        systemmessagebox("Error", "Drive does not exist or is mounted from disk image.", "ok","error", 1);
        return false;
    }

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("drive_saveimg_menu_callback failed to get the current working directory.");
        return false;
    }
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] =  {IS_PC98_ARCH?"*.hdi":"*.img",IS_PC98_ARCH?"*.HDI":"*.hdi"};
    const char *lFilterDescription = IS_PC98_ARCH ? "Disk image (*.hdi)" : "Disk image (*.img)";
    char const * lTheSaveFileName = tinyfd_saveFileDialog("Save image file...","",2,lFilterPatterns,lFilterDescription);
    if (lTheSaveFileName==NULL) return false;

    for (int i=0; i<MAX_DISK_IMAGES; i++)
        if (imageDiskList[i] && imageDiskList[i]->ffdd && imageDiskList[i]->drvnum == drive) {
            if (!saveDiskImage(imageDiskList[i], lTheSaveFileName)) systemmessagebox("Error", "Failed to save disk image.", "ok","error", 1);
            chdir(Temp_CurrentDir);
            return true;
        }
    if (dos_kernel_disabled || !strcmp(RunningProgram, "LOADLIN")) return false;
    Section_prop *sec = static_cast<Section_prop *>(control->GetSection("dosbox"));
    uint32_t freeMB = sec->Get_int("convert fat free space"), timeout = sec->Get_int("convert fat timeout");
    imageDisk *imagedrv = new imageDisk(Drives[drive], drive, freeMB, timeout);
    if (!saveDiskImage(imagedrv, lTheSaveFileName)) systemmessagebox("Error", "Failed to save disk image.", "ok","error", 1);
    if (imagedrv) delete imagedrv;

    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("drive_saveimg_menu_callback failed to change directories.");
        return false;
    }
#endif

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

void swapInDrive(int drive, unsigned int position=0);
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

    statusdrive=drive;
    GUI_Shortcut(31);
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
    drive_saveimg_menu_callback,
    NULL
};

void MEM_A20_Enable(bool enabled);
bool MEM_A20_Enabled(void);
bool a20gate_on_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    bool bef = MEM_A20_Enabled();
    MEM_A20_Enable(!bef);
    bool aft = MEM_A20_Enabled();
    if (bef != aft)
        mainMenu.get_item("enable_a20gate").check(MEM_A20_Enabled()).refresh_item(mainMenu);
    else {
        std::string msg = "The A20 gate may be locked and cannot be "+std::string(bef?"disabled":"enabled")+".";
        systemmessagebox("Warning",msg.c_str(),"ok", "warning", 1);
    }
    return true;
}

void Get_IDECD_drives(std::vector<int> &v), MenuBrowseCDImage(char drive, int num), MenuBrowseFDImage(char drive, int num, int type);
bool change_currentcd_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    std::vector<int> v = {};
    if (dos_kernel_disabled) Get_IDECD_drives(v);
    int num=0;
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    for (unsigned int idrive=2; idrive<DOS_DRIVES; idrive++) {
        if (dos_kernel_disabled && std::find(v.begin(), v.end(), idrive) == v.end()) continue;
        if (dynamic_cast<const isoDrive*>(Drives[idrive]) == NULL) continue;
        MenuBrowseCDImage('A'+idrive, ++num);
    }
#if !defined(HX_DOS)
    if (!num) tinyfd_messageBox("Error","No CD drive is currently available.","ok","error", 1);
#endif
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return true;
}

bool change_currentfd_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    int num=0;
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GFX_ReleaseMouse();
    for (unsigned int idrive=0; idrive<2; idrive++) {
        if (Drives[idrive]) {
            fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[idrive]);
            if (fdp == NULL || fdp->opts.bytesector || fdp->opts.cylsector || fdp->opts.headscyl || fdp->opts.cylinders) continue;
            MenuBrowseFDImage('A'+idrive, ++num, fdp->opts.mounttype);
        } else if (imageDiskList[idrive])
            MenuBrowseFDImage('A'+idrive, ++num, -1);
    }
#if !defined(HX_DOS)
    if (!num) tinyfd_messageBox("Error","No floppy drive is currently available.","ok","error", 1);
#endif
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    return true;
}

bool make_diskimage_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if(control->SecureMode()) {
#if !defined(HX_DOS)
        MAPPER_ReleaseAllKeys();
        GFX_LosingFocus();
        GFX_ReleaseMouse();
        tinyfd_messageBox("Error",MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"ok","error", 1);
        MAPPER_ReleaseAllKeys();
        GFX_LosingFocus();
#endif
    } else
        GUI_Shortcut(37);
    return true;
}

bool list_drivenum_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(32);
    return true;
}

bool list_ideinfo_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(33);
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
    { "saveimg",                "Save to disk image" },
    { NULL, NULL }
};

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

bool dos_hdd_rate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    Section_prop * section = static_cast<Section_prop *>(control->GetSection("dos"));
    if (disk_data_rate == 0) {
        if (pcibus_enable)
            disk_data_rate = 8333333; /* Probably an average IDE data rate for mid 1990s PCI IDE controllers in PIO mode */
        else
            disk_data_rate = 3500000; /* Probably an average IDE data rate for early 1990s ISA IDE controllers in PIO mode */
    } else
        disk_data_rate = 0;
    std::string tmp = std::to_string(disk_data_rate);
    SetVal("dos", "hard drive data rate limit", tmp);
    mainMenu.get_item("limit_hdd_rate").check(disk_data_rate).refresh_item(mainMenu);
    return true;
}

bool dos_floppy_rate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

    Section_prop * section = static_cast<Section_prop *>(control->GetSection("dos"));
    if(floppy_data_rate == 0)
        floppy_data_rate = 22400; // 175 kbps
    else
        floppy_data_rate = 0;
    std::string tmp = std::to_string(floppy_data_rate);
    SetVal("dos", "floppy drive data rate limit", tmp);
    mainMenu.get_item("limit_floppy_rate").check(floppy_data_rate).refresh_item(mainMenu);
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

void OutputSettingMenuUpdate(void);
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
    GUI_Shortcut(40);
    return true;
}

bool sb_device_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(41);
    return true;
}

bool midi_device_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(42);
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
    else if (!strcmp(mname, "cpuak6-166"))
        cyclemu = 110000;
    else if (!strcmp(mname, "cpuak6-200"))
        cyclemu = 130000;
    else if (!strcmp(mname, "cpuak6-300"))
        cyclemu = 193000;
    else if (!strcmp(mname, "cpuath-600"))
        cyclemu = 306000;
    else if (!strcmp(mname, "cpu686-866"))
        cyclemu = 407000;
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
    GUI_Shortcut(28);
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
    GUI_Shortcut(19);
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

#if defined(WIN32) && !defined(HX_DOS) || defined(LINUX) || defined(MACOSX)
extern bool winautorun, startwait, startquiet, starttranspath;
bool dos_win_autorun_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    winautorun = !winautorun;
    mainMenu.get_item("dos_win_autorun").check(winautorun).refresh_item(mainMenu);
    return true;
}

bool dos_win_transpath_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    starttranspath = !starttranspath;
    mainMenu.get_item("dos_win_transpath").check(starttranspath).refresh_item(mainMenu);
    return true;
}

bool dos_win_wait_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    startwait = !startwait;
    mainMenu.get_item("dos_win_wait").check(startwait).refresh_item(mainMenu);
    return true;
}

bool dos_win_quiet_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    startquiet = !startquiet;
    mainMenu.get_item("dos_win_quiet").check(startquiet).refresh_item(mainMenu);
    return true;
}
#endif

bool wheel_move_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "wheel_none")) wheel_key = 0;
    else if (!strcmp(mname, "wheel_updown")) wheel_key = 1;
    else if (!strcmp(mname, "wheel_leftright")) wheel_key = 2;
    else if (!strcmp(mname, "wheel_pageupdown")) wheel_key = 3;
    else if (!strcmp(mname, "wheel_ctrlupdown")) wheel_key = 4;
    else if (!strcmp(mname, "wheel_ctrlleftright")) wheel_key = 5;
    else if (!strcmp(mname, "wheel_ctrlpageupdown")) wheel_key = 6;
    else if (!strcmp(mname, "wheel_ctrlwz")) wheel_key = 7;
    mainMenu.get_item("wheel_updown").check(wheel_key==1).refresh_item(mainMenu);
    mainMenu.get_item("wheel_leftright").check(wheel_key==2).refresh_item(mainMenu);
    mainMenu.get_item("wheel_pageupdown").check(wheel_key==3).refresh_item(mainMenu);
    mainMenu.get_item("wheel_ctrlupdown").check(wheel_key==4).refresh_item(mainMenu);
    mainMenu.get_item("wheel_ctrlleftright").check(wheel_key==5).refresh_item(mainMenu);
    mainMenu.get_item("wheel_ctrlpageupdown").check(wheel_key==6).refresh_item(mainMenu);
    mainMenu.get_item("wheel_ctrlwz").check(wheel_key==7).refresh_item(mainMenu);
    mainMenu.get_item("wheel_none").check(wheel_key==0).refresh_item(mainMenu);
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
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("vid_select_glsl_shader_menu_callback failed to get the current working directory.");
        return false;
    }
    struct stat st;
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT+"glshaders"+CROSS_FILESPLIT;
# if defined(MACOSX)
    /* Hey, Mac OS! When I ask for files that end in *.glsl I expect your finder
       to actually let users select files that end in *.glsl! What gives? 2022/06/29 */
    int nFilterPatterns = 0;
    const char **lFilterPatterns = NULL;
    const char *lFilterDescription = NULL;
# else
    int nFilterPatterns = 2;
    const char *lFilterPatterns[] = {"*.glsl","*.GLSL"};
    const char *lFilterDescription = "OpenGL shader files (*.glsl)";
# endif

    /* Mac OS Monterey: osascript will refuse to present any dialog box if the path does not exist.
       Make sure we give it something that exists */
    if (stat(cwd.c_str(),&st) != 0)
        cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT+"contrib/glshaders"+CROSS_FILESPLIT;
    if (stat(cwd.c_str(),&st) != 0)
        cwd = std::string(Temp_CurrentDir);

    char const * lTheOpenFileName = tinyfd_openFileDialog("Select OpenGL shader",cwd.c_str(),nFilterPatterns,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the GLSL shader setting unless it's just
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
                    tmp.erase(tmp.size()-5, std::string::npos);
            } else {
                std::string localname = name;
                if (!FileDirExistCP(name) && FileDirExistUTF8(localname, name))
                    tmp = localname;
                else
                    tmp = name;
            }
        }

        if (tmp.size()) {
            SetVal("render", "glshader", tmp);
            LoadGLShader(section);

            /* force reinit */
            GFX_ForceRedrawScreen();
        }
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("vid_select_glsl_shader_menu_callback failed to change directories.");
        return false;
    }
#endif

    return true;
}
#endif

#if defined(USE_TTF)
bool vid_select_ttf_font_menu_callback(DOSBoxMenu* const menu, DOSBoxMenu::item* const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("vid_select_ttf_font_menu_callback failed to get the current working directory.");
        return false;
    }
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.ttf","*.TTF","*.ttc","*.TTC","*.otf","*.OTF","*.fon","*.FON"};
    const char *lFilterDescription = "TrueType font files (*.ttf, *.ttc, *.otf, *.fon)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select TrueType font",cwd.c_str(),8,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the TrueType font setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        if (*name) {
            std::string localname = name;
            if (!FileDirExistCP(name) && FileDirExistUTF8(localname, name))
                SetVal("ttf", "font", localname.c_str());
            else
                SetVal("ttf", "font", name);
            ttf_reset();
            if (!IS_PC98_ARCH) setTTFMap(false);
#if C_PRINTER
            if (TTF_using() && printfont) UpdateDefaultPrinterFont();
#endif
        }
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("vid_select_ttf_font_menu_callback failed to change directories.");
        return false;
    }
#endif

    return true;
}

bool ttf_blinking_cursor_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (blinkCursor>-1) {
        oldblinkc=blinkCursor;
        blinkCursor=-1;
        SetVal("ttf", "blinkc", "false");
        mainMenu.get_item("ttf_blinkc").check(false).refresh_item(mainMenu);
    } else {
        blinkCursor=oldblinkc>-1?oldblinkc:(IS_PC98_ARCH?6:4);
        SetVal("ttf", "blinkc", "true");
        mainMenu.get_item("ttf_blinkc").check(true).refresh_item(mainMenu);
    }
    resetFontSize();
    return true;
}

bool ttf_right_left_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    rtl=!rtl;
    SetVal("ttf", "righttoleft", rtl?"true":"false");
    mainMenu.get_item("ttf_right_left").check(rtl).refresh_item(mainMenu);
    resetFontSize();
    return true;
}

bool ttf_halfwidth_katakana_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (!isDBCSCP()||dos.loaded_codepage!=932) {
        systemmessagebox("Warning", "This function is only available for the Japanese code page (932).", "ok","warning", 1);
        return true;
    }
    halfwidthkana=!halfwidthkana;
    SetVal("ttf", "halfwidthkana", halfwidthkana?"true":"false");
    mainMenu.get_item("ttf_halfwidthkana").check(halfwidthkana).refresh_item(mainMenu);
    setTTFCodePage();
    resetFontSize();
    return true;
}

bool ttf_extend_charset_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (!isDBCSCP()||(dos.loaded_codepage!=936&&dos.loaded_codepage!=950&&dos.loaded_codepage!=951)) {
        systemmessagebox("Warning", "This function is only available for the Chinese code pages (936 or 950).", "ok","warning", 1);
        return true;
    }
    if (dos.loaded_codepage==936) {
        gbk=!gbk;
        SetVal("ttf", "gbk", gbk?"true":"false");
        mainMenu.get_item("ttf_extcharset").check(gbk).refresh_item(mainMenu);
    } else if (dos.loaded_codepage==950) {
        chinasea=!chinasea;
        if (!chinasea) makestdcp950table();
        SetVal("ttf", "chinasea", chinasea?"true":"false");
        mainMenu.get_item("ttf_extcharset").check(chinasea).refresh_item(mainMenu);
    } else if (dos.loaded_codepage==951) {
        chinasea=!chinasea;
        if (chinasea) makeseacp951table();
        SetVal("ttf", "chinasea", chinasea?"true":"false");
        mainMenu.get_item("ttf_extcharset").check(chinasea).refresh_item(mainMenu);
        MSG_Init();
    }
    resetFontSize();
    return true;
}

#if C_PRINTER
bool ttf_print_font_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    printfont=!printfont;
    SetVal("ttf", "printfont", printfont?"true":"false");
    mainMenu.get_item("ttf_printfont").check(printfont).refresh_item(mainMenu);
    UpdateDefaultPrinterFont();
    return true;
}
#endif

bool ttf_style_change_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "ttf_showbold")) {
        showbold=!showbold;
        SetVal("ttf", "bold", showbold?"true":"false");
        mainMenu.get_item(mname).check(showbold).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showital")) {
        showital=!showital;
        SetVal("ttf", "italic", showital?"true":"false");
        mainMenu.get_item(mname).check(showital).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showline")) {
        showline=!showline;
        SetVal("ttf", "underline", showline?"true":"false");
        mainMenu.get_item(mname).check(showline).refresh_item(mainMenu);
    } else if (!strcmp(mname, "ttf_showsout")) {
        showsout=!showsout;
        SetVal("ttf", "strikeout", showsout?"true":"false");
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
        SetVal("ttf", "wp", "");
        wpType=0;
    } else if (!strcmp(mname, "ttf_wpwp")) {
        SetVal("ttf", "wp", "wp");
        wpType=1;
    } else if (!strcmp(mname, "ttf_wpws")) {
        SetVal("ttf", "wp", "ws");
        wpType=2;
    } else if (!strcmp(mname, "ttf_wpxy")) {
        SetVal("ttf", "wp", "xy");
        wpType=3;
    } else if (!strcmp(mname, "ttf_wpfe")) {
        SetVal("ttf", "wp", "fe");
        wpType=4;
    } else
        return true;
    mainMenu.get_item("ttf_wpno").check(!wpType).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpwp").check(wpType==1).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpws").check(wpType==2).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpxy").check(wpType==3).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpfe").check(wpType==4).refresh_item(mainMenu);
    resetFontSize();
    return true;
}
#endif

void Load_mapper_file() {
    Section_prop* section = static_cast<Section_prop*>(control->GetSection("sdl"));
    assert(section != NULL);

#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("Load_mapper_file failed to get the current working directory.");
        return;
    }
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.map","*.MAP"};
    const char *lFilterDescription = "Mapper files (*.map)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select mapper file",cwd.c_str(),2,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the mapper file setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;

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
            std::string localname = name;
            if (pp->realpath=="")
                SetVal("sdl", "mapperfile", !FileDirExistCP(name) && FileDirExistUTF8(localname, name) ? localname.c_str() : name);
            else {
#if defined(C_SDL2)
                SetVal("sdl", "mapperfile_sdl2", !FileDirExistCP(name) && FileDirExistUTF8(localname, name) ? localname.c_str() : name);
#else
                SetVal("sdl", "mapperfile_sdl1", !FileDirExistCP(name) && FileDirExistUTF8(localname, name) ? localname.c_str() : name);
#endif
            }
            void ReloadMapper(Section_prop *sec, bool init);
            ReloadMapper(section,true);
        }
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("Load_mapper_file failed to change directories.");
    }
#endif
}

void Restart_config_file() {
#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("Restart_config_file failed to get the current working directory.");
        return;
    }
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.conf","*.CONF","*.cfg","*.CFG"};
    const char *lFilterDescription = "DOSBox-X config files (*.conf, *.cfg)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select config file",cwd.c_str(),4,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the config file setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        if (*name) {
            void RebootConfig(std::string filename, bool confirm=false);
            std::string localname = name;
            if (!FileDirExistCP(name) && FileDirExistUTF8(localname, name))
                RebootConfig(localname.c_str(), true);
            else
                RebootConfig(name, true);
        }
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("Restart_config_file failed to change directories.");
    }
#endif
}

void Load_language_file() {
#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("Load_language_file failed to get the current working directory.");
        return;
    }
    struct stat st;
    std::string res_path, exepath = GetDOSBoxXPath();
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT+"languages"+CROSS_FILESPLIT;
    Cross::GetPlatformResDir(res_path);
    if (stat(cwd.c_str(),&st) != 0 && exepath.size())
        cwd = exepath+(exepath.back()==CROSS_FILESPLIT?"":std::string(1, CROSS_FILESPLIT))+"languages"+CROSS_FILESPLIT;
    if (stat(cwd.c_str(),&st) != 0 && res_path.size())
        cwd = res_path+(res_path.back()==CROSS_FILESPLIT?"":std::string(1, CROSS_FILESPLIT))+"languages"+CROSS_FILESPLIT;
    if (stat(cwd.c_str(),&st) != 0)
        cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.lng","*.LNG","*.txt","*.TXT"};
    const char *lFilterDescription = "DOSBox-X language files (*.lng, *.txt)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select language file",cwd.c_str(),4,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        /* Windows will fill lpstrFile with the FULL PATH.
           The full path should be given to the language file setting unless it's just
           the same base path it was given: <cwd>\shaders in which case just cut it
           down to the filename. */
        const char* name = lTheOpenFileName;

        /* filenames in Windows are case insensitive so do the comparison the same */
        if (!strncasecmp(name, cwd.c_str(), cwd.size())) {
            name += cwd.size();
            while (*name == CROSS_FILESPLIT) name++;
        }

        if (*name) {
            std::string localname = name, newname = !FileDirExistCP(name) && FileDirExistUTF8(localname, name) ? localname : name;
            if (dos_kernel_disabled)
                RebootLanguage(newname.c_str(), true);
            else {
                uselangcp = true;
                SetVal("dosbox", "language", newname);
                Load_Language(newname);
                uselangcp = false;
                if (lastmsgcp == dos.loaded_codepage && langname.size()) {
                    std::string msg = "DOSBox-X has successfully loaded the following language:\n\n" + langname + " [code page " + std::to_string(lastmsgcp) + "]\n\nMessages from this language will be applied from this point.";
                    systemmessagebox("DOSBox-X language file", msg.c_str(), "ok","info", 2);
                }
            }
        }
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("Load_language_file failed to change directories.");
    }
#endif
}

bool vid_pc98_graphics_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void pc98_clear_graphics(void);
    if (IS_PC98_ARCH) pc98_clear_graphics();
    return true;
}

bool voodoo_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("voodoo"));
    if (section == NULL) return false;
    const char *voodoostr = section->Get_string("voodoo_card");
    bool voodooon = strcasecmp(voodoostr, "false");
    SetVal("voodoo", "voodoo_card", voodooon?"false":"auto");
    VOODOO_Destroy(section);
    VOODOO_OnPowerOn(section);
    mainMenu.get_item("3dfx_voodoo").check(!voodooon).refresh_item(mainMenu);
    return true;
}

bool glide_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    Section_prop *section = static_cast<Section_prop *>(control->GetSection("voodoo"));
    if (section == NULL) return false;
    bool glideon = addovl;
    SetVal("voodoo", "glide", glideon?"false":"true");
    addovl=false;
    GLIDE_ShutDown(section);
    GLIDE_PowerOn(section);
    if (addovl) VFILE_RegisterBuiltinFileBlob(bfb_GLIDE2X_OVL, "/SYSTEM/");
    else {
        VFILE_Remove("GLIDE2X.OVL","SYSTEM");
        if (!glideon) systemmessagebox("Warning", "Glide passthrough cannot be enabled. Check the Glide wrapper installation.", "ok","warning", 1);
    }
    mainMenu.get_item("3dfx_glide").check(addovl).refresh_item(mainMenu);
    return true;
}

bool overscan_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    int f = atoi(menuitem->get_text().c_str()); /* Off becomes 0 */
    char tmp[64];

    sprintf(tmp,"%d",f);
    SetVal("sdl", "overscan", tmp);
    change_output(8);
    return true;
}

void UpdateOverscanMenu(void) {
    for (size_t i=0;i <= 10;i++) {
        char tmp[64];
        sprintf(tmp,"overscan_%zu",i);
        mainMenu.get_item(tmp).check(sdl.overscan_width == i).refresh_item(mainMenu);
    }
}

void aspect_ratio_menu() {
    mainMenu.get_item("video_ratio_1_1").check(aspect_ratio_x==1&&aspect_ratio_y==1).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_3_2").check(aspect_ratio_x==3&&aspect_ratio_y==2).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_4_3").check((aspect_ratio_x==4&&aspect_ratio_y==3)||!aspect_ratio_x||!aspect_ratio_y).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_16_9").check(aspect_ratio_x==16&&aspect_ratio_y==9).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_16_10").check(aspect_ratio_x==16&&aspect_ratio_y==10).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_18_10").check(aspect_ratio_x==18&&aspect_ratio_y==10).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_original").check(aspect_ratio_x==-1&&aspect_ratio_y==-1).enable(true).refresh_item(mainMenu);
}

bool aspect_ratio_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    if (!strcmp(mname, "video_ratio_1_1")) {
        aspect_ratio_x = 1;
        aspect_ratio_y = 1;
        SetVal("render", "aspect_ratio", "1:1");
    } else if (!strcmp(mname, "video_ratio_3_2")) {
        aspect_ratio_x = 3;
        aspect_ratio_y = 2;
        SetVal("render", "aspect_ratio", "3:2");
    } else if (!strcmp(mname, "video_ratio_4_3")) {
        aspect_ratio_x = 4;
        aspect_ratio_y = 3;
        SetVal("render", "aspect_ratio", "4:3");
    } else if (!strcmp(mname, "video_ratio_16_9")) {
        aspect_ratio_x = 16;
        aspect_ratio_y = 9;
        SetVal("render", "aspect_ratio", "16:9");
    } else if (!strcmp(mname, "video_ratio_16_10")) {
        aspect_ratio_x = 16;
        aspect_ratio_y = 10;
        SetVal("render", "aspect_ratio", "16:10");
    } else if (!strcmp(mname, "video_ratio_18_10")) {
        aspect_ratio_x = 18;
        aspect_ratio_y = 10;
        SetVal("render", "aspect_ratio", "18:10");
    } else if (!strcmp(mname, "video_ratio_original")) {
        aspect_ratio_x = -1;
        aspect_ratio_y = -1;
        SetVal("render", "aspect_ratio", "-1:-1");
    }
    aspect_ratio_menu();
    if (render.aspect) GFX_ForceRedrawScreen();
    return true;
}

bool aspect_ratio_edit_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(20);
    return true;
}

bool vsync_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    const char *val = menuitem->get_name().c_str();
    if (!strncmp(val,"vsync_",6))
        val += 6;
    else
        return true;

    SetVal("vsync", "vsyncmode", val);

    change_output(9);

    VGA_Vsync VGA_Vsync_Decode(const char *vsyncmodestr);
    void VGA_VsyncUpdateMode(VGA_Vsync vsyncmode);
    VGA_VsyncUpdateMode(VGA_Vsync_Decode(val));
    return true;
}

bool vsync_set_syncrate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(17);
    return true;
}

bool set_titletext_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(21);
    return true;
}

bool set_transparency_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(22);
    return true;
}

bool refresh_rate_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(30);
    return true;
}

bool output_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED

    const char *what = menuitem->get_name().c_str();

    if (!strncmp(what,"output_",7))
        what += 7;
    else
        return true;

    if (toOutput(what)) SetVal("sdl", "output", what);
    return true;
}

bool clear_screen() {
    if (CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064)
        return false;
    if (CurMode->type==M_TEXT || dos_kernel_disabled) {
        const auto rows = (IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS):24);
        const auto cols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
        INT10_ScrollWindow(0, 0, rows, static_cast<uint8_t>(cols), -rows, 0x7, 0xff);
        INT10_SetCursorPos(0, 0, 0);
    } else if (IS_PC98_ARCH) {
        char msg[]="\033[2J";
        uint16_t s = (uint16_t)strlen(msg);
        DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
    } else {
        uint16_t oldax=reg_ax;
        reg_ax=(uint16_t)CurMode->mode;
        CALLBACK_RunRealInt(0x10);
        if (IS_DOSV && DOSV_CheckCJKVideoMode()) DOSV_FillScreen();
        reg_ax = oldax;
    }
    clearline=true;
    return true;
}

void show_prompt() {
    if (!dos_kernel_disabled && !strcmp(RunningProgram, "COMMAND")) {
        DOS_Shell temp;
        temp.exit = true;
        temp.ShowPrompt();
    }
}

bool clear_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    if (!clear_screen()) return true;
    show_prompt();
    return true;
}

bool intensity_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    const char *mname = menuitem->get_name().c_str();
    uint16_t oldax=reg_ax, oldbx=reg_bx;
    if (IS_PC98_ARCH||machine==MCH_CGA||(CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064)) {
        systemmessagebox("Warning", "High intensity is not supported for the current video mode.", "ok","warning", 1);
        return true;
    }
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

#if defined(LINUX) && C_X11
# include <X11/Xlib.h>
# include <X11/Xatom.h>
#endif

int GetNumScreen() {
    int numscreen = 1;
#if defined(C_SDL2)
    numscreen = SDL_GetNumVideoDisplays();
#elif defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    xyp xy={0};
    xy.x=-1;
    xy.y=-1;
    curscreen=0;
    EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
    numscreen = curscreen;
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

bool lines_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    clear_screen();
    const char *mname = menuitem->get_name().c_str();
    if (IS_PC98_ARCH||(CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064))
        return true;
    if (!setlines(mname)) return true;
    show_prompt();
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
#if !defined(C_EMSCRIPTEN) && defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
#if C_DEBUG
    bool DEBUG_IsDebuggerConsoleVisible(void);
    if (DEBUG_IsDebuggerConsoleVisible())
        return true;
#endif
    auto window = GetForegroundWindow();

    auto console = GetConsoleWindow();

    if (console == nullptr)
        DOSBox_ShowConsole();

    auto visible = IsWindowVisible(console);

    ShowWindow(console, visible ? SW_HIDE : SW_SHOW);

    SetForegroundWindow(window);

    if (console == nullptr)
        console = GetConsoleWindow();

    visible = IsWindowVisible(console);

    mainMenu.get_item("show_console").check(visible).refresh_item(mainMenu);
    mainMenu.get_item("clear_console").check(false).enable(visible).refresh_item(mainMenu);
#endif
    return true;
}

bool clear_console_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
#if !defined(C_EMSCRIPTEN) && defined(WIN32) && !defined(HX_DOS)
#if C_DEBUG
    bool DEBUG_IsDebuggerConsoleVisible(void);
    if (DEBUG_IsDebuggerConsoleVisible())
        return true;
#endif
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

    constexpr COORD position = { 0, 0 };

    DWORD written;

    CONSOLE_SCREEN_BUFFER_INFO info;

    if(!GetConsoleScreenBufferInfo(handle, &info))
        return false;

    const DWORD size = info.dwSize.X * info.dwSize.Y;

    if(!FillConsoleOutputCharacter(handle, ' ', size, position, &written))
        return false;

    if(!GetConsoleScreenBufferInfo(handle, &info))
        return false;

    if(!FillConsoleOutputAttribute(handle, info.wAttributes, size, position, &written))
        return false;

    SetConsoleCursorPosition(handle, position);
#endif
    // TODO clear console on other platforms
    return true;
}

bool save_logas_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("save_logas_menu_callback failed to get the current working directory.");
        return false;
    }
    std::string cwd = std::string(Temp_CurrentDir)+CROSS_FILESPLIT;
    const char *lFilterPatterns[] = {"*.log","*.LOG"};
    const char *lFilterDescription = "Log files (*.log)";
    char const * lTheSaveFileName = tinyfd_saveFileDialog("Save log file...","",2,lFilterPatterns,lFilterDescription);
    if (lTheSaveFileName==NULL) return false;
#if C_DEBUG
    bool savetologfile(const char *name);
    if (!savetologfile(lTheSaveFileName)) systemmessagebox("Warning", ("Cannot save to the file: "+std::string(lTheSaveFileName)).c_str(), "ok","warning", 1);
#endif
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("save_logas_menu_callback failed to change directories.");
        return false;
    }
#endif
    return true;
}

bool show_logtext_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(43);
    return true;
}

bool show_codetext_menu_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    GUI_Shortcut(44);
    return true;
}

bool video_debug_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    video_debug_overlay = !video_debug_overlay;
    mainMenu.get_item("video_debug_overlay").check(video_debug_overlay).refresh_item(mainMenu);

    if (!vga.draw.vga_override)
        RENDER_SetSize(vga.draw.width,vga.draw.height,render.src.bpp,render.src.fps,render.src.scrn_ratio);

    return true;
}

bool disable_log_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    control->opt_nolog = !control->opt_nolog;
    mainMenu.get_item("disable_logging").check(control->opt_nolog).refresh_item(mainMenu);
    return true;
}

bool wait_on_error_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    sdl.wait_on_error = !sdl.wait_on_error;
    mainMenu.get_item("wait_on_error").check(sdl.wait_on_error).refresh_item(mainMenu);
    return true;
}

#if C_DEBUG
extern bool tohide;
extern int debugrunmode, debuggerrun;
void DEBUG_Enable_Handler(bool pressed);
bool IsDebuggerActive(void), IsDebuggerRunwatch(void), IsDebuggerRunNormal(void);
bool debugger_rundebug_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    debugrunmode=0;
    mainMenu.get_item("debugger_rundebug").check(true).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runnormal").check(false).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runwatch").check(false).refresh_item(mainMenu);
    if (IsDebuggerActive()||IsDebuggerRunwatch()||IsDebuggerRunNormal()) {
        tohide=false;
        DEBUG_Enable_Handler(true);
        tohide=true;
    }
    return true;
}

bool debugger_runnormal_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    debugrunmode=1;
    mainMenu.get_item("debugger_rundebug").check(false).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runnormal").check(true).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runwatch").check(false).refresh_item(mainMenu);
    if (IsDebuggerActive()||IsDebuggerRunwatch()||IsDebuggerRunNormal()) {
        tohide=false;
        DEBUG_Enable_Handler(true);
        tohide=true;
    }
    return true;
}

bool debugger_runwatch_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    debugrunmode=2;
    mainMenu.get_item("debugger_rundebug").check(false).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runnormal").check(false).refresh_item(mainMenu);
    mainMenu.get_item("debugger_runwatch").check(true).refresh_item(mainMenu);
    if (IsDebuggerActive()||IsDebuggerRunwatch()||IsDebuggerRunNormal()) {
        tohide=false;
        DEBUG_Enable_Handler(true);
        tohide=true;
    }
    return true;
}
#endif

bool autolock_mouse_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    sdl.mouse.autoenable = !sdl.mouse.autoenable;
    mainMenu.get_item("auto_lock_mouse").check(sdl.mouse.autoenable).refresh_item(mainMenu);
    return true;
}

#if defined (WIN32) || defined(MACOSX) || defined(C_SDL2)
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

bool clipboard_bios_paste_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    clipboard_biospaste = !clipboard_biospaste;
    mainMenu.get_item("clipboard_biospaste").check(clipboard_biospaste).refresh_item(mainMenu);
    return true;
}

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

bool enable_autosave_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    enable_autosave = !enable_autosave;
    mainMenu.get_item("enable_autosave").check(enable_autosave).refresh_item(mainMenu);
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
    if(getcwd(Temp_CurrentDir, 512) == NULL) {
        LOG(LOG_GUI, LOG_ERROR)("browse_save_file_menu_callback failed to get the current working directory.");
        return false;
    }
    const char *lFilterPatterns[] = {"*.sav","*.SAV"};
    const char *lFilterDescription = "Save files (*.sav)";
    char const * lTheSaveFileName = tinyfd_saveFileDialog("Select a save file","",2,lFilterPatterns,lFilterDescription);
    if (lTheSaveFileName!=NULL) {
        savefilename = std::string(lTheSaveFileName);
        mainMenu.get_item("usesavefile").set_text("Use save file"+(savefilename.size()?" ("+savefilename+")":"")).refresh_item(mainMenu);
    }
    if(chdir(Temp_CurrentDir) == -1) {
        LOG(LOG_GUI, LOG_ERROR)("browse_save_file_menu_callback failed to change to the current working directory.");
        return false;
    }
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

bool auto_save_setting_menu_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    GUI_Shortcut(29);
    return true;
}

void refresh_slots() {
    std::string text=mainMenu.get_item("current_page").get_text();
    std::size_t found = text.find(":");
    if (found!=std::string::npos) text = text.substr(0, found);
    mainMenu.get_item("current_page").set_text(text+": "+std::to_string(page+1)+"/10").refresh_item(mainMenu);
	for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
		char name[6]="slot0";
		name[4]='0'+i;
		std::string command=SaveState::instance().getName(page*SaveState::SLOT_COUNT+i);
		std::string str=std::string(MSG_Get("SLOT"))+" "+std::to_string(page*SaveState::SLOT_COUNT+i+1)+(command==""?"":" "+command);
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

bool last_autosave_slot_menu_callback(DOSBoxMenu * const menu, DOSBoxMenu::item * const menuitem) {
    (void)menu;//UNUSED
    (void)menuitem;//UNUSED
    void LastAutoSaveSlot_Run(void);
    LastAutoSaveSlot_Run();
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

void BlankDisplay(void);

bool refreshtest_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)menuitem;
    (void)xmenu;

    BlankDisplay();

#if defined(USE_TTF)
    if (TTF_using()) resetFontSize();
#endif
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
#endif

    return true;
}

bool generatenmi_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)menuitem;
    (void)xmenu;

    CPU_Raise_NMI();

    return true;
}

Bitu int2fdbg_hook_callback = 0;
bool int2fhook_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)menuitem;
    (void)xmenu;

#if C_DEBUG
    if (int2fdbg_hook_callback == 0) {
        void Int2fhook();
        Int2fhook();
        systemmessagebox("Success", "The INT 2Fh hook has been successfully set.", "ok","info", 1);
    } else
        systemmessagebox("Warning", "The INT 2Fh hook was already set up.", "ok","warning", 1);
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

bool restartinst_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)xmenu;//UNUSED
    (void)menuitem;//UNUSED
    RebootLanguage("", true);
    return true;
}

bool restartconf_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)xmenu;//UNUSED
    (void)menuitem;//UNUSED
    Restart_config_file();
    return true;
}

bool loadlang_menu_callback(DOSBoxMenu * const xmenu, DOSBoxMenu::item * const menuitem) {
    (void)xmenu;//UNUSED
    (void)menuitem;//UNUSED
    Load_language_file();
    return true;
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

bool is_always_on_top(void);

void toggle_always_on_top(void) {
    bool cur = is_always_on_top();
#if defined(_WIN32) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
    sdl1_hax_set_topmost(!cur);
#elif defined(_WIN32)
    SetWindowPos(GetHWND(), cur?HWND_NOTOPMOST:HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#elif defined(MACOSX) && (defined(C_SDL2) || defined(SDL_DOSBOX_X_SPECIAL))
    void sdl1_hax_set_topmost(unsigned char topmost);
    sdl1_hax_set_topmost(macosx_on_top = (!cur));
#elif defined(LINUX)
    void LinuxX11_OnTop(bool f);
    LinuxX11_OnTop(x11_on_top = (!cur));
#else
    (void)cur;
#endif
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

void update_all_shortcuts();
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
    GUI_Shortcut(34);
    return true;
}

bool help_about_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    GUI_Shortcut(35);
    return true;
}

bool toscale=true;
std::string helpcmd="";
bool help_command_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const menuitem) {
    MAPPER_ReleaseAllKeys();

    GFX_LosingFocus();

    bool switchfs=false;
#if defined(C_SDL2)
    int x=-1, y=-1;
#endif
    if (!GFX_IsFullscreen()&&!TTF_using()&&!window_was_maximized) {
        switchfs=true;
#if defined(C_SDL2)
        SDL_GetWindowPosition(sdl.window, &x, &y);
#endif
        GFX_SwitchFullScreen();
    }
    toscale=false;
    helpcmd = menuitem->get_name().substr(8);
    GUI_Shortcut(36);
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

bool help_nic_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    GUI_Shortcut(38);
    return true;
}

bool help_prt_callback(DOSBoxMenu * const /*menu*/, DOSBoxMenu::item * const /*menuitem*/) {
    GUI_Shortcut(39);
    return true;
}

void AllocCallback1() {
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
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuak6-166").set_text("AMD K6 166MHz (~110000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuak6-200").set_text("AMD K6 200MHz (~130000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuak6-300").set_text("AMD K6-2 300MHz (~193000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuath-600").set_text("AMD Athlon 600MHz (~306000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu686-866").set_text("Pentium III 866MHz EB (~407000 cycles)").set_callback_function(cpu_speed_emulate_menu_callback);
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
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoRatioMenu");
                item.set_text("Aspect ratio");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_1_1").set_text("1:1").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_3_2").set_text("3:2").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_4_3").set_text("4:3").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_16_9").set_text("16:9").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_16_10").set_text("16:10").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_18_10").set_text("18:10").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_original").set_text("Original ratio").
                    set_callback_function(aspect_ratio_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_set").set_text("Set ratio").
                    set_callback_function(aspect_ratio_edit_menu_callback);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoScalerMenu");
                item.set_text("Scaler");

                for (size_t i=0;scaler_menu_opts[i][0] != NULL;i++) {
                    const std::string name = std::string("scaler_set_") + scaler_menu_opts[i][0];

                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(scaler_menu_opts[i][1]).
                        set_callback_function(scaler_set_menu_callback);
                }

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"set_titletext").set_text("Set title bar text...").
                    set_callback_function(set_titletext_menu_callback);

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"set_transparency").set_text("Set transparency...").
                    set_callback_function(set_transparency_menu_callback);

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"refresh_rate").set_text("Adjust refresh rate...").
                    set_callback_function(refresh_rate_menu_callback);
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
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_openglnb").set_text("OpenGL nearest").
                    set_callback_function(output_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_openglpp").set_text("OpenGL perfect").
                    set_callback_function(output_menu_callback);
#if defined(USE_TTF)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_ttf").set_text("TrueType font").
                    set_callback_function(output_menu_callback);
#endif
#if C_GAMELINK
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_gamelink").set_text("Game Link").
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
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpfe").set_text("TTF word processor: FastEdit").
                    set_callback_function(ttf_wp_change_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_blinkc").set_text("Display TTF blinking cursor").
                    set_callback_function(ttf_blinking_cursor_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_right_left").set_text("Display text from right to left").
                    set_callback_function(ttf_right_left_callback);
#if C_PRINTER
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_printfont").set_text("Use current TTF font for printing").
                    set_callback_function(ttf_print_font_callback);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_halfwidthkana").set_text("CJK: Allow half-width Japanese Katakana").
                    set_callback_function(ttf_halfwidth_katakana_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_extcharset").set_text("CJK: Enable extended Chinese character set").
                    set_callback_function(ttf_extend_charset_callback);
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
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"Video3dfxMenu");
                item.set_text("3dfx emulation");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"3dfx_voodoo").set_text("Internal Voodoo card").
                    set_callback_function(voodoo_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"3dfx_glide").set_text("Glide passthrough").
                    set_callback_function(glide_menu_callback);
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
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_ttf_font").set_text("Select TrueType font (TTF/OTF)...").
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
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"enable_a20gate").set_text("Enable A20 gate").
                    set_callback_function(a20gate_on_menu_callback).check(MEM_A20_Enabled());
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"change_currentcd").set_text("Change current CD image...").
                    set_callback_function(change_currentcd_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"change_currentfd").set_text("Change current floppy image...").
                    set_callback_function(change_currentfd_menu_callback);
            }

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

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSDiskRateMenu");
                item.set_text("Limit disk transfer speed");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"limit_hdd_rate").set_text("Limit hard disk data rate").
                        set_callback_function(dos_hdd_rate_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"limit_floppy_rate").set_text("Limit floppy disk data rate").
                        set_callback_function(dos_floppy_rate_menu_callback);
                }
            }

#if defined(WIN32) && !defined(HX_DOS) || defined(LINUX) || defined(MACOSX)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSWinMenu");
                item.set_text("Host system applications");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_autorun").set_text("Launch to run on the Windows host").
                        set_callback_function(dos_win_autorun_menu_callback)
#if defined(LINUX) || defined(MACOSX)
                    .enable(false)
#endif
                    ;
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_transpath").set_text("Translate paths to host system paths").
                        set_callback_function(dos_win_transpath_menu_callback);
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
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"enable_autosave").set_text("Enable auto-saving state").set_callback_function(enable_autosave_menu_callback).enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"noremark_savestate").set_text("No remark when saving state").set_callback_function(noremark_savestate_menu_callback).check(noremark_save_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"force_loadstate").set_text("No warning when loading state").set_callback_function(force_loadstate_menu_callback).check(force_load_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"removestate").set_text("Remove state in slot").set_callback_function(remove_state_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"refreshslot").set_text("Refresh display status").set_callback_function(refresh_slots_menu_callback);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"lastautosaveslot").set_text("Select last auto-saved slot").set_callback_function(last_autosave_slot_menu_callback).enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "usesavefile").set_text("Use save file instead of save slot").set_callback_function(use_save_file_menu_callback).check(use_save_file);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "autosavecfg").set_text("Auto-save settings...").set_callback_function(auto_save_setting_menu_callback);
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
					std::string str=std::string(MSG_Get("SLOT"))+" "+std::to_string(page*SaveState::SLOT_COUNT+i+1);
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
#if C_PCAP
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_nic").set_text("List network interfaces").
                    set_callback_function(help_nic_callback);
#endif
#if C_PRINTER && defined(WIN32)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_prt").set_text("List printer devices").
                    set_callback_function(help_prt_callback);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_about").set_text("About DOSBox-X").
                    set_callback_function(help_about_callback);
#if !defined(C_EMSCRIPTEN)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_console").set_text("Show logging console").set_callback_function(show_console_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clear_console").set_text("Clear logging console").set_callback_function(clear_console_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"disable_logging").set_text("Disable logging output").set_callback_function(disable_log_menu_callback).check(control->opt_nolog);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wait_on_error").set_text("Console wait on error").set_callback_function(wait_on_error_menu_callback).check(sdl.wait_on_error);
#endif
#if C_DEBUG
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_codetext").set_text("Show code overview").set_callback_function(show_codetext_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_logtext").set_text("Show logging text").set_callback_function(show_logtext_menu_callback);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"save_logas").set_text("Save logging as...").set_callback_function(save_logas_menu_callback);

                debugrunmode = debuggerrun;
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_rundebug").set_text("Debugger option: Run debugger").set_callback_function(debugger_rundebug_menu_callback).check(debugrunmode==0);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_runnormal").set_text("Debugger option: Run normal").set_callback_function(debugger_runnormal_menu_callback).check(debugrunmode==1);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_runwatch").set_text("Debugger option: Run watch").set_callback_function(debugger_runwatch_menu_callback).check(debugrunmode==2);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_debug_overlay").set_text("Video debug overlay").set_callback_function(video_debug_callback).check(video_debug_overlay);
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
#if C_DEBUG
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DebugMenu");
                item.set_text("Debug");
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"HelpDebugMenu")
#else
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpDebugMenu");
                item
#endif
                .set_text("Logging console");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_blankrefreshtest").set_text("Refresh test (blank display)").set_callback_function(refreshtest_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_generatenmi").set_text("Generate NMI interrupt").set_callback_function(generatenmi_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_int2fhook").set_text("Hook INT 2Fh calls").set_callback_function(int2fhook_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logint21").set_text("Log INT 21h calls").
                        set_callback_function(dos_debug_menu_callback);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logfileio").set_text("Log file I/O").
                        set_callback_function(dos_debug_menu_callback);
                }
            }
        }
}

void AllocCallback2() {
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"hostkey_mapper").set_text("Mapper-defined").set_callback_function(hostkey_preset_menu_callback);

        /* more */
        std::string doubleBufString = std::string("desktop.doublebuf");
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"showdetails").set_text("Show FPS and RT speed in title bar").set_callback_function(showdetails_menu_callback).check(!menu.hidecycles && menu.showrt);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"restartinst").set_text("Restart DOSBox-X instance").set_callback_function(restartinst_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"restartconf").set_text("Restart DOSBox-X with config file...").set_callback_function(restartconf_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"loadlang").set_text("Load language file...").set_callback_function(loadlang_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"auto_lock_mouse").set_text("Autolock mouse").set_callback_function(autolock_mouse_menu_callback).check(sdl.mouse.autoenable);
#if defined (WIN32) || defined(MACOSX) || defined(C_SDL2)
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_right").set_text("Via right mouse button").set_callback_function(right_mouse_clipboard_menu_callback).check(mbutton==3);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_middle").set_text("Via middle mouse button").set_callback_function(middle_mouse_clipboard_menu_callback).check(mbutton==2);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_arrows").set_text("Via arrow keys (Home=start, End=end)").set_callback_function(arrow_keys_clipboard_menu_callback).check(mbutton==4);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"screen_to_clipboard").set_text("Copy all text on the DOS screen").set_callback_function(screen_to_clipboard_menu_callback);
#endif
        if (control->SecureMode()) clipboard_dosapi = false;
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_device").set_text("Enable DOS clipboard device access").set_callback_function(dos_clipboard_device_menu_callback).check(dos_clipboard_device_access==4&&!control->SecureMode());
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_dosapi").set_text("Enable DOS clipboard API for applications").set_callback_function(dos_clipboard_api_menu_callback).check(clipboard_dosapi);
        if (IS_PC98_ARCH) clipboard_biospaste = true;
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clipboard_biospaste").set_text("Use BIOS function for clipboard pasting").set_callback_function(clipboard_bios_paste_menu_callback).check(clipboard_biospaste).enable(!IS_PC98_ARCH);
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
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_none").set_text("Do not convert to arrow keys").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_updown").set_text("Convert to up/down arrows").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_leftright").set_text("Convert to left/right arrows").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_pageupdown").set_text("Convert to PgUp/PgDn keys").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_ctrlupdown").set_text("Convert to Ctrl+up/down arrows").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_ctrlleftright").set_text("Convert to Ctrl+left/right arrows").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_ctrlpageupdown").set_text("Convert to Ctrl+PgUp/PgDn keys").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_ctrlwz").set_text("Convert to Ctrl+W/Z keys").set_callback_function(wheel_move_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wheel_guest").set_text("Enable for guest systems also").set_callback_function(wheel_guest_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"doublebuf").set_text("Double buffering (fullscreen)").set_callback_function(doublebuf_menu_callback).check(!!GetSetSDLValue(1, doubleBufString, 0));
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"alwaysontop").set_text("Always on top").set_callback_function(alwaysontop_menu_callback).check(is_always_on_top());
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"highdpienable").set_text("High DPI enable").set_callback_function(highdpienable_menu_callback).check(dpi_aware_enable);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sync_host_datetime").set_text("Synchronize host date/time").set_callback_function(sync_host_datetime_menu_callback).check(sync_time);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"shell_config_commands").set_text("Config options as commands").set_callback_function(shell_config_commands_menu_callback).check(enable_config_as_shell_commands);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"quick_reboot").set_text("Enable quick reboot").set_callback_function(quick_reboot_menu_callback).check(use_quick_reboot);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"make_diskimage").set_text("Create blank disk image...").set_callback_function(make_diskimage_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"list_drivenum").set_text("Show mounted drive numbers").set_callback_function(list_drivenum_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"list_ideinfo").set_text("Show IDE disk or CD status").set_callback_function(list_ideinfo_menu_callback);
        mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_use_uskb").set_text("Use US keyboard layout").set_callback_function(pc98_force_uskb_menu_callback).check(pc98_force_ibm_layout);
}
