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

#include <string>
#include "menudef.h"
void SetVal(const std::string secname, std::string preval, const std::string val);

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
void D3D_PS(void);
void DOSBox_CheckOS(int &id, int &major, int &minor);
void MountDrive(char drive, const char drive2[DOS_PATHLENGTH]);
void MountDrive_2(char drive, const char drive2[DOS_PATHLENGTH], std::string drive_type);
void MENU_SaveState(int value);
void MENU_LoadState(int value);
void MENU_RemoveState(std::string value);
void MENU_RemoveState_All(void);
void MENU_Check_SaveState(HMENU handle, std::string real_path, int load_state, int save_state, int remove_state);
void MENU_Check_Drive(HMENU handle, int cdrom, int floppy, int local, int image, int automount, int umount, char drive);
bool MENU_SetBool(std::string secname, std::string value);
void MENU_swapstereo(bool enabled);
void UI_Shortcut(int select);
void* GetSetSDLValue(int isget, std::string target, void* setval);
void Go_Boot(const char boot_drive[_MAX_DRIVE]);
void Go_Boot2(const char boot_drive[_MAX_DRIVE]);
void OpenFileDialog(char * path_arg);
void OpenFileDialog_Img(char drive);
void GFX_SetTitle(Bit32s cycles, Bits frameskip, Bits timing, bool paused);
void change_output(int output);
void res_input(bool type, const char * res);
void res_init(void);
int Reflect_Menu(void);
extern bool DOSBox_Kor(void);

extern unsigned int hdd_defsize;
extern char hdd_size[20];
extern HWND GetHWND(void);
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

// dummy Win32 functions for less #ifdefs
#define GetHWND() (0)
#define SetMenu(a,b)
#define DragAcceptFiles(a,b)
#define GetMenu(a) (0)

// menu.cpp replacements; the optimizer will completely remove code based on these
#define DOSBox_SetMenu()
#define DOSBox_RefreshMenu()
#define DOSBox_CheckOS(a, b, c) do { (a)=0; (b)=0; (c)=0; } while(0)
#define VER_PLATFORM_WIN32_NT (1)
#define DOSBox_Kor() !strncmp("ko", getenv("LANG"), 2) // dirty hack.

#endif
