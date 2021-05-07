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
 *
 *  New commands & heavy improvements to existing commands by the DOSBox-X Team
 *  With major works from joncampbell123, Wengier, and rderooy
 *  AUTOTYPE command Copyright (C) 2020 the DOSBox Staging Team
 *  FLAGSAVE command Copyright PogoMan361 and Wengier
 */

#include "dosbox.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "programs.h"
#include "support.h"
#include "drives.h"
#include "cross.h"
#include "regs.h"
#include "ide.h"
#include "cpu.h"
#include "callback.h"
#include "cdrom.h"
#include "bios_disk.h"
#include "dos_system.h"
#include "dos_inc.h"
#include "bios.h"
#include "inout.h"
#include "dma.h"
#include "bios_disk.h"
#include "qcow2_disk.h"
#include "setup.h"
#include "control.h"
#include <time.h>
#include "menu.h"
#include "render.h"
#include "mouse.h"
#include "../ints/int10.h"
#if !defined(HX_DOS)
#include "../libs/tinyfiledialogs/tinyfiledialogs.c"
#endif
#if defined(WIN32)
#include <VersionHelpers.h>
# if defined(__MINGW32__)
#  define ht_stat_t struct _stat
#  define ht_stat(x,y) _wstat(x,y)
# else
#  define ht_stat_t struct _stat64
#  define ht_stat(x,y) _wstat64(x,y)
# endif
typedef wchar_t host_cnv_char_t;
host_cnv_char_t *CodePageGuestToHost(const char *s);
#if !defined(S_ISREG)
# define S_ISREG(x) ((x & S_IFREG) == S_IFREG)
#endif
#include "../dos/cdrom.h"
#include <ShlObj.h>
#else
#include <libgen.h>
#endif
int freesizecap = 1;
bool Mouse_Drv=true;
bool Mouse_Vertical = false;
bool force_nocachedir = false;
bool lockmount = true;
bool wpcolon = true;
bool startcmd = false;
bool startwait = true;
bool startquiet = false;
bool mountwarning = true;
bool qmount = false;
bool nowarn = false;
extern bool mountfro[26], mountiro[26];

void DOS_EnableDriveMenu(char drv);
void runBoot(const char *str), runMount(const char *str), runImgmount(const char *str), runRescan(const char *str);

#if defined(OS2)
#define INCL DOSFILEMGR
#define INCL_DOSERRORS
#include "os2.h"
#endif

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

#if defined(RISCOS)
#include <unixlib/local.h>
#include <limits.h>
#endif

#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

class MOUSE : public Program {
public:
    void Run(void);
};

void MOUSE::Run(void) {
    if (cmd->FindExist("/?",false) || cmd->FindExist("/h",false)) {
        WriteOut(MSG_Get("PROGRAM_MOUSE_HELP"));
        return;
    }
	if (!Mouse_Drv) {
        if (cmd->FindExist("/u",false))
            WriteOut(MSG_Get("PROGRAM_MOUSE_NOINSTALLED"));
        else {
            Mouse_Drv = true;
            mainMenu.get_item("dos_mouse_enable_int33").check(Mouse_Drv).refresh_item(mainMenu);
            WriteOut(MSG_Get("PROGRAM_MOUSE_INSTALL"));
            if (cmd->FindExist("/v",false)) {
                Mouse_Vertical = true;
                WriteOut(MSG_Get("PROGRAM_MOUSE_VERTICAL"));
            } else {
                Mouse_Vertical = false;
            }
            mainMenu.get_item("dos_mouse_y_axis_reverse").check(Mouse_Vertical).refresh_item(mainMenu);
        }
    }
	else {
        if (cmd->FindExist("/u",false)) {
            Mouse_Drv = false;
            mainMenu.get_item("dos_mouse_enable_int33").check(Mouse_Drv).refresh_item(mainMenu);
            WriteOut(MSG_Get("PROGRAM_MOUSE_UNINSTALL"));
        } else
            if (cmd->FindExist("/v",false)) {
                if(!Mouse_Vertical) {
                    Mouse_Vertical = true;
                    WriteOut(MSG_Get("PROGRAM_MOUSE_VERTICAL"));
                } else {
                    Mouse_Vertical = false;
                    WriteOut(MSG_Get("PROGRAM_MOUSE_VERTICAL_BACK"));
                }
                mainMenu.get_item("dos_mouse_y_axis_reverse").check(Mouse_Vertical).refresh_item(mainMenu);
            } else
                WriteOut(MSG_Get("PROGRAM_MOUSE_ERROR"));
	}
}

static void MOUSE_ProgramStart(Program * * make) {
    *make=new MOUSE;
}

void DetachFromBios(imageDisk* image) {
    if (image) {
        for (int index = 0; index < MAX_DISK_IMAGES; index++) {
            if (imageDiskList[index] == image) {
                if (index > 1) IDE_Hard_Disk_Detach(index);
                imageDiskList[index]->Release();
                imageDiskChange[index] = true;
                imageDiskList[index] = NULL;
            }
        }
    }
}

extern std::string hidefiles;
extern int swapInDisksSpecificDrive;
extern bool dos_kernel_disabled, clearline;
void MSCDEX_SetCDInterface(int intNr, int forceCD);
bool FDC_UnassignINT13Disk(unsigned char drv);
bool bootguest=false, use_quick_reboot=false;
int bootdrive=-1;
uint8_t ZDRIVE_NUM = 25;
std::string msgget;
static const char* UnmountHelper(char umount) {
    int i_drive;
    if (umount < '0' || umount > 3+'0')
        i_drive = toupper(umount) - 'A';
    else
        i_drive = umount - '0';

    if (i_drive >= DOS_DRIVES || i_drive < 0)
        return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");

    if (i_drive < MAX_DISK_IMAGES && Drives[i_drive] == NULL && imageDiskList[i_drive] == NULL)
        return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");

    if (i_drive >= MAX_DISK_IMAGES && Drives[i_drive] == NULL)
        return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");

    if (i_drive <= 1)
        FDC_UnassignINT13Disk(i_drive);

    msgget=MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS");
    if (Drives[i_drive]) {
        const fatDrive* drive = dynamic_cast<fatDrive*>(Drives[i_drive]);
        imageDisk* image = drive ? drive->loadedDisk : NULL;
        const isoDrive* cdrom = dynamic_cast<isoDrive*>(Drives[i_drive]);
        switch (DriveManager::UnmountDrive(i_drive)) {
            case 1: return MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL");
            case 2: return MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS");
        }
        if (image) DetachFromBios(image);
        if (cdrom) IDE_CDROM_Detach(i_drive);
        Drives[i_drive] = 0;
        DOS_EnableDriveMenu(i_drive+'A');
        mem_writeb(Real2Phys(dos.tables.mediaid)+(unsigned int)i_drive*dos.tables.dpb_size,0);
        if (i_drive == DOS_GetDefaultDrive())
            DOS_SetDrive(ZDRIVE_NUM);
        if (cdrom)
            for (int drv=0; drv<2; drv++)
                if (Drives[drv]) {
                    fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drv]);
                    if (fdp&&fdp->opts.mounttype==1&&toupper(umount)==fdp->el.CDROM_drive) {
                        msgget+=UnmountHelper('A'+drv);
                        size_t found=msgget.rfind("%c");
                        if (found!=std::string::npos)
                            msgget.replace(found, 2, std::string(1, 'A'+drv));
                    }
                }
    }

    if (i_drive < MAX_DISK_IMAGES && imageDiskList[i_drive]) {
        delete imageDiskList[i_drive];
        imageDiskList[i_drive] = NULL;
    }
    if (swapInDisksSpecificDrive == i_drive) {
        for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
            if (diskSwap[si] != NULL) {
                diskSwap[si]->Release();
                diskSwap[si] = NULL;
            }
        }
        swapInDisksSpecificDrive = -1;
    }

    return msgget.c_str();
}

void MountHelper(char drive, const char drive2[DOS_PATHLENGTH], std::string drive_type) {
	std::vector<std::string> options;
	DOS_Drive * newdrive;
	std::string temp_line;
	std::string str_size;
	uint16_t sizes[4];
	uint8_t mediaid;

	if(drive_type=="CDROM") {
		mediaid=0xF8;		/* Hard Disk */
        str_size="2048,1,65535,0";
	} else {
		if(drive_type=="FLOPPY") {
			mediaid=0xF0;			/* Floppy 1.44 media */
			str_size="512,1,2880,2880";	/* All space free */
		} else if(drive_type=="LOCAL") {
			mediaid=0xF8;
			str_size="512,32,0,0";
		}
	}

	char number[20]; const char * scan=str_size.c_str();
	Bitu index=0; Bitu count=0;
		while (*scan) {
			if (*scan==',') {
				number[index]=0;sizes[count++]=atoi(number);
				index=0;
			} else number[index++]=*scan;
			scan++;
		}
	number[index]=0; sizes[count++]=atoi(number);

	temp_line = drive2;
	if(temp_line.size() > 3 && temp_line[temp_line.size()-1]=='\\') temp_line.erase(temp_line.size()-1,1);
	if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
	uint8_t bit8size=(uint8_t) sizes[1];

	if(drive_type=="CDROM") {
		int num = -1;
		int error;

		int id, major, minor;
		DOSBox_CheckOS(id, major, minor);
		if ((id==VER_PLATFORM_WIN32_NT) && (major>5)) {
			// Vista/above
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
		} else {
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
		}
		newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error,options);
		std::string errmsg;
		switch (error) {
			case 0  :   errmsg=MSG_Get("MSCDEX_SUCCESS");                break;
			case 1  :   errmsg=MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS");  break;
			case 2  :   errmsg=MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED");    break;
			case 3  :   errmsg=MSG_Get("MSCDEX_ERROR_PATH");             break;
			case 4  :   errmsg=MSG_Get("MSCDEX_TOO_MANY_DRIVES");        break;
			case 5  :   errmsg=MSG_Get("MSCDEX_LIMITED_SUPPORT");        break;
			default :   errmsg=MSG_Get("MSCDEX_UNKNOWN_ERROR");          break;
		}
		if (error) {
#if !defined(HX_DOS)
            tinyfd_messageBox(error==5?"Warning":"Error",errmsg.c_str(),"ok","error", 1);
#endif
			if (error!=5) {
				delete newdrive;
				return;
			}
		}
	} else {
        newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,options);
        newdrive->readonly = mountfro[drive-'A'];
    }

	if (!newdrive) E_Exit("DOS:Can't create drive");
	Drives[drive-'A']=newdrive;
	DOS_EnableDriveMenu(drive);
	mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,mediaid);
	if(drive_type=="CDROM")
		LOG_MSG("GUI: Drive %c is mounted as CD-ROM",drive);
	else
		LOG_MSG("GUI: Drive %c is mounted as local directory",drive);
    if(drive == drive2[0] && strlen(drive2) == 3) {
        // automatic mount
    } else {
        if(drive_type=="CDROM") return;
        std::string label;
        label = drive;
        if(drive_type=="LOCAL")
            label += "_DRIVE";
        else
            label += "_FLOPPY";
        newdrive->SetLabel(label.c_str(),false,true);
    }
}

#if defined(WIN32)
void MenuMountDrive(char drive, const char drive2[DOS_PATHLENGTH]) {
	std::vector<std::string> options;
	std::string str(1, drive);
	std::string drive_warn;
	if (Drives[drive-'A']) {
		drive_warn="Drive "+str+": is already mounted. Unmount it first, and then try again.";
		MessageBox(GetHWND(),drive_warn.c_str(),"Error",MB_OK);
		return;
	}
	if(control->SecureMode()) {
		MessageBox(GetHWND(),MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"Error",MB_OK);
		return;
	}
	DOS_Drive * newdrive;
	std::string temp_line;
	std::string str_size;
	uint16_t sizes[4];
	uint8_t mediaid;
	drive_warn="Do you really want to give DOSBox-X access to";
	int type=GetDriveType(drive2);
	if(type==DRIVE_NO_ROOT_DIR) {
		MessageBox(GetHWND(),("Drive "+str+": does not exist in the system.").c_str(),"Error",MB_OK);
		return;
	} else if(type==DRIVE_CDROM)
		drive_warn += " your real CD-ROM drive ";
	else if(type==DRIVE_REMOVABLE)
		drive_warn += drive=='A'||drive=='B'?" your real floppy drive ":" your real removable drive ";
	else if(type==DRIVE_REMOTE)
		drive_warn += " your real network drive ";
	else
		drive_warn += " everything on your real drive ";

	if (mountwarning && MessageBox(GetHWND(),(drive_warn+str+"?").c_str(),"Warning",MB_YESNO)==IDNO) return;

	if(type==DRIVE_CDROM) {
		mediaid=0xF8;		/* Hard Disk */
        str_size="2048,1,65535,0";
	} else if(type==DRIVE_REMOVABLE && (drive=='A'||drive=='B')) {
		mediaid=0xF0;			/* Floppy 1.44 media */
		str_size="512,1,2880,2880";	/* All space free */
	} else {
		mediaid=0xF8;
		str_size="512,32,0,0";
	}

	char number[20]; const char * scan=str_size.c_str();
	Bitu index=0; Bitu count=0;
		while (*scan) {
			if (*scan==',') {
				number[index]=0;sizes[count++]=atoi(number);
				index=0;
			} else number[index++]=*scan;
			scan++;
		}
	number[index]=0; sizes[count++]=atoi(number);
	uint8_t bit8size=(uint8_t) sizes[1];

	temp_line = drive2;
	int error, num = -1;
	if(type==DRIVE_CDROM) {
		int id, major, minor;
		DOSBox_CheckOS(id, major, minor);

		if ((id==VER_PLATFORM_WIN32_NT) && (major>5)) {
			// Vista/above
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
		} else {
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
		}
		newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error,options);
		std::string errmsg;
		switch (error) {
			case 0  :   errmsg=MSG_Get("MSCDEX_SUCCESS");                break;
			case 1  :   errmsg=MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS");  break;
			case 2  :   errmsg=MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED");    break;
			case 3  :   errmsg=MSG_Get("MSCDEX_ERROR_PATH");             break;
			case 4  :   errmsg=MSG_Get("MSCDEX_TOO_MANY_DRIVES");        break;
			case 5  :   errmsg=MSG_Get("MSCDEX_LIMITED_SUPPORT");        break;
			default :   errmsg=MSG_Get("MSCDEX_UNKNOWN_ERROR");          break;
		}
		if (error) {
			MessageBox(GetHWND(),errmsg.c_str(),error==5?"Warning":"Error",MB_OK);
			if (error!=5) {
				delete newdrive;
				return;
			}
		}
	} else {
        newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,options);
        newdrive->readonly = mountfro[drive-'A'];
    }

	if (!newdrive) E_Exit("DOS:Can't create drive");
	if(error && (type==DRIVE_CDROM)) return;
	Drives[drive-'A']=newdrive;
	DOS_EnableDriveMenu(drive);
	mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,mediaid);
	if(type==DRIVE_CDROM) LOG_MSG("GUI: Drive %c is mounted as CD-ROM %c:\\",drive,drive);
	else LOG_MSG("GUI: Drive %c is mounted as local directory %c:\\",drive,drive);
    if(drive == drive2[0] && strlen(drive2) == 3) {
        // automatic mount
    } else {
        if(type == DRIVE_CDROM) return;
        std::string label;
        label = drive;
        if(type==DRIVE_REMOVABLE && (drive=='A'||drive=='B'))
            label += "_FLOPPY";
        else
            label += "_DRIVE";
        newdrive->SetLabel(label.c_str(),false,true);
    }
}
#endif

std::string newstr="";
std::string GetNewStr(const char *str) {
    newstr = str?std::string(str):"";
#if defined(WIN32)
    if (str&&dos.loaded_codepage!=437) {
        char *temp = NULL;
        wchar_t* wstr = NULL;
        int reqsize = MultiByteToWideChar(CP_UTF8, 0, str, (int)(strlen(str)+1), NULL, 0);
        if (reqsize>0 && (wstr = new wchar_t[reqsize]) && MultiByteToWideChar(CP_UTF8, 0, str, (int)(strlen(str)+1), wstr, reqsize)==reqsize) {
            reqsize = WideCharToMultiByte(dos.loaded_codepage==808?866:(dos.loaded_codepage==872?855:dos.loaded_codepage), WC_NO_BEST_FIT_CHARS, wstr, -1, NULL, 0, "\x07", NULL);
            if (reqsize > 1 && (temp = new char[reqsize]) && WideCharToMultiByte(dos.loaded_codepage==808?866:(dos.loaded_codepage==872?855:dos.loaded_codepage), WC_NO_BEST_FIT_CHARS, wstr, -1, (LPSTR)temp, reqsize, "\x07", NULL) == reqsize)
                newstr = std::string(temp);
        }
    }
#endif
    return newstr;
}

void MenuBrowseImageFile(char drive, bool arc, bool boot, bool multiple) {
	std::string str(1, drive);
	std::string drive_warn;
	if (Drives[drive-'A']&&!boot) {
		drive_warn="Drive "+str+": is already mounted. Unmount it first, and then try again.";
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
#endif
		return;
	}
	if(control->SecureMode()) {
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"ok","error", 1);
#endif
		return;
	}
	if (dos_kernel_disabled)
		return;
#if !defined(HX_DOS)
    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    char const * lTheOpenFileName;
    std::string files="", fname="";
    if (arc) {
        const char *lFilterPatterns[] = {"*.zip","*.7z","*.ZIP","*.7Z"};
        const char *lFilterDescription = "Archive files (*.zip, *.7z)";
        lTheOpenFileName = tinyfd_openFileDialog(("Select an archive file for Drive "+str+":").c_str(),"",4,lFilterPatterns,lFilterDescription,0);
        if (lTheOpenFileName) fname = GetNewStr(lTheOpenFileName);
    } else {
        const char *lFilterPatterns[] = {"*.ima","*.img","*.vhd","*.hdi","*.iso","*.cue","*.bin","*.chd","*.mdf","*.gog","*.ins","*.IMA","*.IMG","*.VHD","*.HDI","*.ISO","*.CUE","*.BIN","*.CHD","*.MDF","*.GOG","*.INS"};
        const char *lFilterDescription = "Disk/CD image files (*.ima, *.img, *.vhd, *.hdi, *.iso, *.cue, *.bin, *.chd, *.mdf, *.gog, *.ins)";
        lTheOpenFileName = tinyfd_openFileDialog(((multiple?"Select image file(s) for Drive ":"Select an image file for Drive ")+str+":").c_str(),"",22,lFilterPatterns,lFilterDescription,multiple?1:0);
        if (lTheOpenFileName) fname = GetNewStr(lTheOpenFileName);
        if (multiple&&fname.size()) {
            files += "\"";
            for (int i=0; i<fname.size(); i++)
                files += fname[i]=='|'?"\" \"":std::string(1,fname[i]);
            files += "\" ";
        }
        while (multiple&&lTheOpenFileName&&tinyfd_messageBox("Mount image files","Do you want to mount more image file(s)?","yesno", "question", 1)) {
            lTheOpenFileName = tinyfd_openFileDialog(("Select image file(s) for Drive "+str+":").c_str(),"",20,lFilterPatterns,lFilterDescription,multiple?1:0);
            if (lTheOpenFileName) {
                fname = GetNewStr(lTheOpenFileName);
                files += "\"";
                for (int i=0; i<fname.size(); i++)
                    files += fname[i]=='|'?"\" \"":std::string(1,fname[i]);
                files += "\" ";
            }
        }
    }

    if (fname.size()||files.size()) {
        char type[15];
        if (!arc&&!files.size()) {
            char ext[5] = "";
            if (fname.size()>4)
                strcpy(ext, fname.substr(fname.size()-4).c_str());
            if(!strcasecmp(ext,".ima"))
                strcpy(type,"-t floppy ");
            else if((!strcasecmp(ext,".iso")) || (!strcasecmp(ext,".cue")) || (!strcasecmp(ext,".bin")) || (!strcasecmp(ext,".chd")) || (!strcasecmp(ext,".mdf")) || (!strcasecmp(ext,".gog")) || (!strcasecmp(ext,".ins")))
                strcpy(type,"-t iso ");
            else
                strcpy(type,"");
        } else
            *type=0;
		char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
		strcpy(mountstring,type);
		char temp_str[3] = { 0,0,0 };
		temp_str[0]=drive;
		temp_str[1]=' ';
		strcat(mountstring,temp_str);
		if (!multiple) strcat(mountstring,"\"");
		strcat(mountstring,files.size()?files.c_str():fname.c_str());
		if (!multiple) strcat(mountstring,"\"");
		if (mountiro[drive-'A']) strcat(mountstring," -ro");
		if (boot) strcat(mountstring," -u");
        if (arc) {
            strcat(mountstring," -q");
            runMount(mountstring);
        } else {
            qmount=true;
            runImgmount(mountstring);
            qmount=false;
        }
		chdir( Temp_CurrentDir );
		if (!Drives[drive-'A']) {
			drive_warn="Drive "+str+": failed to mount.";
			tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
			return;
		} else if (boot) {
			char str[] = "-Q A:";
			str[3]=drive;
			runBoot(str);
			std::string drive_warn="Drive "+std::string(1, drive)+": failed to boot.";
			tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
		} else if (multiple) {
			tinyfd_messageBox("Information",("Mounted disk images to Drive "+std::string(1,drive)+(dos.loaded_codepage==437?":\n"+files:".")+(mountiro[drive-'A']?"\n(Read-only mode)":"")).c_str(),"ok","info", 1);
		} else if (lTheOpenFileName) {
			tinyfd_messageBox("Information",(std::string(arc?"Mounted archive":"Mounted disk image")+" to Drive "+std::string(1,drive)+":\n"+std::string(lTheOpenFileName)+(arc||mountiro[drive-'A']?"\n(Read-only mode)":"")).c_str(),"ok","info", 1);
		}
	}
	chdir( Temp_CurrentDir );
#endif
}

void MenuBrowseFolder(char drive, std::string drive_type) {
    std::string str(1, drive);
	if (Drives[drive-'A']) {
		std::string drive_warn="Drive "+str+": is already mounted. Unmount it first, and then try again.";
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
#endif
		return;
	}
	if(control->SecureMode()) {
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"ok","error", 1);
#endif
		return;
	}
#if !defined(HX_DOS)
    std::string title = "Select a drive/directory to mount for Drive "+str+":";
    if(drive_type=="CDROM")
        title += " CD-ROM\nMounting a directory as CD-ROM gives an limited support";
    else if(drive_type=="FLOPPY")
        title += " as Floppy";
    else if(drive_type=="LOCAL")
        title += " as Local";
    char const * lTheSelectFolderName = tinyfd_selectFolderDialog(title.c_str(), NULL);
    if (lTheSelectFolderName) {
        MountHelper(drive,GetNewStr(lTheSelectFolderName).c_str(),drive_type);
        if (Drives[drive-'A']) tinyfd_messageBox("Information",("Drive "+std::string(1,drive)+" is now mounted to:\n"+std::string(lTheSelectFolderName)).c_str(),"ok","info", 1);
    }
#endif
}

void MenuUnmountDrive(char drive) {
	if (!Drives[drive-'A']) {
		std::string drive_warn="Drive "+std::string(1, drive)+": is not yet mounted.";
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
#endif
		return;
	}
    UnmountHelper(drive);
}

void MenuBootDrive(char drive) {
	if(control->SecureMode()) {
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"ok","error", 1);
#endif
		return;
	}
	char str[] = "-Q A:";
	str[3]=drive;
	runBoot(str);
	std::string drive_warn="Drive "+std::string(1, drive)+": failed to boot.";
#if !defined(HX_DOS)
    tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
#endif
}

void MenuBrowseProgramFile() {
	if(control->SecureMode()) {
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"),"ok","error", 1);
#endif
		return;
	}
	if (dos_kernel_disabled)
		return;
    std::string drive_warn;
	DOS_MCB mcb(dos.psp()-1);
	static char psp_name[9];
	mcb.GetFileName(psp_name);
	if(strlen(psp_name)&&strcmp(psp_name, "COMMAND")) {
        drive_warn=strcmp(psp_name, "4DOS")?"Another program is already running.":"Another shell is currently running.";
#if !defined(HX_DOS)
        tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
#endif
        return;
    }

#if !defined(HX_DOS)
    char drv=' ';
    for (int i=2; i<DOS_DRIVES-1; i++) {
        if (!Drives[i]) {
            drv='A'+i;
            break;
        }
    }
    if (drv==' ') {
        for (int i=0; i<2; i++) {
            if (!Drives[i]) {
                drv='A'+i;
                break;
            }
        }
    }
    if (drv==' ') { // Fallback to C: if no free drive found
        drive_warn="Quick launch automatically mounts drive C in DOSBox-X.\nDrive C has already been mounted. Do you want to continue?";
        if (!tinyfd_messageBox("Warning",drive_warn.c_str(),"yesno", "question", 1)) {return;}
        drv='C';
    }
    mainMenu.get_item("mapper_quickrun").enable(false).refresh_item(mainMenu);

    char CurrentDir[512];
    char * Temp_CurrentDir = CurrentDir;
    getcwd(Temp_CurrentDir, 512);
    const char *lFilterPatterns[] = {"*.com","*.exe","*.bat","*.COM","*.EXE","*.BAT"};
    const char *lFilterDescription = "Executable files (*.com, *.exe, *.bat)";
    char const * lTheOpenFileName = tinyfd_openFileDialog("Select an executable file to launch","",6,lFilterPatterns,lFilterDescription,0);

    if (lTheOpenFileName) {
        const char *ext = strrchr(lTheOpenFileName,'.');
        struct stat st;
        std::string full = std::string(lTheOpenFileName);
        if (stat(lTheOpenFileName, &st) || !S_ISREG(st.st_mode)) {
            if(ext!=NULL) {
                tinyfd_messageBox("Error","Executable file not found.","ok","error", 1);
                return;
            }
            full=std::string(lTheOpenFileName)+".com";
            if (stat(full.c_str(), &st) || !S_ISREG(st.st_mode)) {
                full=std::string(lTheOpenFileName)+".exe";
                if (stat(full.c_str(), &st) || !S_ISREG(st.st_mode)) {
                    full=std::string(lTheOpenFileName)+".bat";
                    if (stat(full.c_str(), &st) || !S_ISREG(st.st_mode)) {
                        tinyfd_messageBox("Error","Executable file not found.","ok","error", 1);
                        return;
                    }
                }
            }
        }
        if(ext==NULL) {
            tinyfd_messageBox("Error","Executable file not found.","ok","error", 1);
            return;
        }
        std::size_t found = full.find_last_of("/\\");
        std::string pathname = full.substr(0,found), filename = full.substr(found+1);
        clearline=true;
        bool exist=Drives[drv-'A'];
		char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
		char temp_str[3] = { 0,0,0 };
		temp_str[0]=drv;
		temp_str[1]=' ';
		strcpy(mountstring,temp_str);
		strcat(mountstring,"\"");
		strcat(mountstring,pathname.c_str());
#if defined(WIN32)
		if (pathname.size()==2&&pathname.back()==':') strcat(mountstring,"\\");
#endif
		strcat(mountstring," \"");
		strcat(mountstring," -Q -U");
		runMount(mountstring);
		if (!Drives[drv-'A']) {
			drive_warn="Drive "+std::string(1, drv)+": failed to mount.";
            tinyfd_messageBox("Error",drive_warn.c_str(),"ok","error", 1);
            if (!dos_kernel_disabled) mainMenu.get_item("mapper_quickrun").enable(true).refresh_item(mainMenu);
			return;
        }
        uint8_t olddrv=DOS_GetDefaultDrive();
		DOS_SetDefaultDrive(drv-'A');

		char name1[DOS_PATHLENGTH+2], name2[DOS_PATHLENGTH+4], name3[DOS_PATHLENGTH+4];
        std::string msg="\r\nLaunching ";
		strcpy(name1,filename.c_str());
        msg+=std::string(name1)+"...\r\n";
        bool filename_not_8x3(const char *n);
        if (filename_not_8x3(name1)) {
            bool olduselfn=uselfn;
            uselfn=true;
            sprintf(name3,"\"%s\"",filename.c_str());
            if (DOS_GetSFNPath(name3,name2,false)) {
                char *p=strrchr(name2, '\\');
                strcpy(name1,p==NULL?name2:p+1);
            }
            uselfn=olduselfn;
        }

        uint16_t n = (uint16_t)msg.size();
        DOS_WriteFile(STDERR,(uint8_t*)msg.c_str(),&n);

		chdir( Temp_CurrentDir );
        DOS_Shell shell;
        shell.Execute(name1," ");
        if (!strcasecmp(ext,".bat")) {
            bool echo=shell.echo;
            shell.echo=false;
            shell.RunInternal();
            shell.echo=echo;
        }
        if (!exist) {
            for (int i=0; i<1000; i++) CALLBACK_Idle();
            drive_warn="Program has finished execution. Do you want to unmount Drive "+std::string(1, drv)+" now?";
            if (tinyfd_messageBox("Warning",drive_warn.c_str(),"yesno", "question", 1)) {
                if (Drives[olddrv]) DOS_SetDefaultDrive(olddrv);
                char temp_str[3] = { 0,0,0 };
                temp_str[0]=drv;
                temp_str[1]=' ';
                strcpy(mountstring,temp_str);
                strcat(mountstring," -Q -U");
                runMount(mountstring);
            }
        }
        if (strcasecmp(ext,".bat")) {
            n=1;
            uint8_t c='\r';
            DOS_WriteFile(STDOUT,&c,&n);
            c='\n';
            DOS_WriteFile(STDOUT,&c,&n);
        }
		shell.ShowPrompt();
	}

	chdir( Temp_CurrentDir );
    if (!dos_kernel_disabled) mainMenu.get_item("mapper_quickrun").enable(true).refresh_item(mainMenu);
#endif
}

class MOUNT : public Program {
public:
    std::vector<std::string> options;
    void Move_Z(char new_z) {
        char newz_drive = (char)toupper(new_z);
        int i_newz = (int)newz_drive - (int)'A';
        if (Drives[i_newz])
            WriteOut("Drive %c is already in use\n", new_z);
        else if (i_newz >= 0 && i_newz < DOS_DRIVES) {
            /* remap drives */
            Drives[i_newz] = Drives[ZDRIVE_NUM];
            Drives[ZDRIVE_NUM] = 0;
            DOS_EnableDriveMenu(i_newz + 'A');
            DOS_EnableDriveMenu(ZDRIVE_NUM + 'A');
            if (!first_shell) return; //Should not be possible
            /* Update environment */
            std::string line = "";
            char ppp[2] = { newz_drive,0 };
            std::string tempenv = ppp; tempenv += ":\\";
            std::string tempenvZ = std::string(1, 'A'+ZDRIVE_NUM); tempenvZ += ":\\";
            std::string tempenvz = std::string(1, 'a'+ZDRIVE_NUM); tempenvz += ":\\";
            if (first_shell->GetEnvStr("PATH", line)) {
                std::string::size_type idx = line.find('=');
                std::string value = line.substr(idx + 1, std::string::npos);
                while ((idx = value.find(tempenvZ)) != std::string::npos ||
                    (idx = value.find(tempenvz)) != std::string::npos)
                    value.replace(idx, 3, tempenv);
                line = value;
            }
            if (!line.size()) line = tempenv;
            first_shell->SetEnv("PATH", line.c_str());
            tempenv += "COMMAND.COM";
            first_shell->SetEnv("COMSPEC", tempenv.c_str());

            /* Update batch file if running from Z: (very likely: autoexec) */
            if (first_shell->bf) {
                std::string& name = first_shell->bf->filename;
                if (name.length() > 2 && name[0] == 'A'+ZDRIVE_NUM && name[1] == ':') name[0] = newz_drive;
            }
            /* Change the active drive */
            if (DOS_GetDefaultDrive() == ZDRIVE_NUM) DOS_SetDrive(i_newz);
            ZDRIVE_NUM = i_newz;
        }
    }
    void ListMounts(void) {
        char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
        uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
        /* Command uses dta so set it to our internal dta */
        RealPt save_dta = dos.dta();
        dos.dta(dos.tables.tempdta);
        DOS_DTA dta(dos.dta());

        WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
        WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),"Drive","Type","Label");
        int cols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
        if (!cols) cols=80;
        for(int p = 0;p < cols;p++) WriteOut("-");
        bool none=true;
        for (int d = 0;d < DOS_DRIVES;d++) {
            if (!Drives[d]) continue;

            char root[7] = {(char)('A'+d),':','\\','*','.','*',0};
            bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
            if (ret) {
                dta.GetResult(name,lname,size,date,time,attr);
                DOS_FindNext(); //Mark entry as invalid
            } else name[0] = 0;

            /* Change 8.3 to 11.0 */
            const char* dot = strchr(name, '.');
            if(dot && (dot - name == 8) ) { 
                name[8] = name[9];name[9] = name[10];name[10] = name[11];name[11] = 0;
            }

            root[1] = 0; //This way, the format string can be reused.
            WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),root, Drives[d]->GetInfo(),name);
            none=false;
        }
        if (none) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
        dos.dta(save_dta);
    }

    void Run(void) {
        DOS_Drive *newdrive = NULL;
        std::string label;
        std::string umount;
        std::string newz;
        bool quiet=false;
        char drive;

        //Hack To allow long commandlines
        ChangeToLongCmd();
        /* Parse the command line */
        /* if the command line is empty show current mounts */
        if (!cmd->GetCount()) {
            ListMounts();
            return;
        }

        /* In secure mode don't allow people to change mount points. 
         * Neither mount nor unmount */
        if(control->SecureMode()) {
            WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
            return;
        }
		if (cmd->FindExist("/examples")||cmd->FindExist("-examples")) {
#if defined (WIN32) || defined(OS2)
            WriteOut(MSG_Get("PROGRAM_MOUNT_EXAMPLE"),"d:\\dosprogs","d:\\dosprogs","\"d:\\dos games\"","\"d:\\dos games\"","d:\\dosprogs","d:\\dosprogs","d:\\dosprogs","d:\\dosprogs","d:\\dosprogs","d:\\dosprogs","d:\\overlaydir");
#else
            WriteOut(MSG_Get("PROGRAM_MOUNT_EXAMPLE"),"~/dosprogs","~/dosprogs","\"~/dos games\"","\"~/dos games\"","~/dosprogs","~/dosprogs","~/dosprogs","~/dosprogs","~/dosprogs","~/dosprogs","~/overlaydir");
#endif
			return;
		}
        bool path_relative_to_last_config = false;
        if (cmd->FindExist("-pr",true)) path_relative_to_last_config = true;

        if (cmd->FindExist("-q",true))
            quiet = true;

        /* Check for unmounting */
        if (cmd->FindString("-u",umount,false)) {
            const char *msg=UnmountHelper(umount[0]);
            if (!quiet) WriteOut(msg, toupper(umount[0]));
            return;
        }

        //look for -o options
        {
            std::string s;

            while (cmd->FindString("-o", s, true))
                options.push_back(s);
        }

        /* Check for moving Z: */
        /* Only allowing moving it once. It is merely a convenience added for the wine team */
        if (cmd->FindString("-z", newz,false)) {
            if (ZDRIVE_NUM != newz[0]-'A') Move_Z(newz[0]);
            return;
        }
        /* Show list of cdroms */
        if (cmd->FindExist("-cd",false)) {
            int num = SDL_CDNumDrives();
            if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_CDROMS_FOUND"),num);
            for (int i=0; i<num; i++) {
                if (!quiet) WriteOut("%2d. %s\n",i,SDL_CDName(i));
            }
            return;
        }

        bool nocachedir = false;

        if (force_nocachedir)
            nocachedir = true;

        if (cmd->FindExist("-nocachedir",true))
            nocachedir = true;

        bool readonly = false;
        if (cmd->FindExist("-ro",true))
            readonly = true;
        if (cmd->FindExist("-rw",true))
            readonly = false;

        std::string type="dir";
        cmd->FindString("-t",type,true);
		std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        bool iscdrom = (type =="cdrom"); //Used for mscdex bug cdrom label name emulation
        bool exist = false, removed = false;
        if (type=="floppy" || type=="dir" || type=="cdrom" || type =="overlay") {
            uint16_t sizes[4] = { 0 };
            uint8_t mediaid;
            std::string str_size = "";
            if (type=="floppy") {
                str_size="512,1,2880,2880";
                mediaid=0xF0;       /* Floppy 1.44 media */
            } else if (type=="dir" || type == "overlay") {
                // 512*32*32765==~500MB total size
                // 512*32*16000==~250MB total free size
                str_size="512,32,0,0";
                mediaid=0xF8;       /* Hard Disk */
            } else if (type=="cdrom") {
                str_size="2048,1,65535,0";
                mediaid=0xF8;       /* Hard Disk */
            } else {
                if (!quiet) WriteOut(MSG_Get("PROGAM_MOUNT_ILL_TYPE"),type.c_str());
                return;
            }
            /* Parse the free space in mb's (kb's for floppies) */
            std::string mb_size;
            if(cmd->FindString("-freesize",mb_size,true)) {
                char teststr[1024];
                uint16_t freesize = static_cast<uint16_t>(atoi(mb_size.c_str()));
                if (type=="floppy") {
                    // freesize in kb
                    sprintf(teststr,"512,1,2880,%d",freesize*1024/(512*1)>2880?2880:freesize*1024/(512*1));
                } else {
					if (freesize>1919) freesize=1919;
					uint16_t numc=type=="cdrom"?1:32;
                    uint32_t total_size_cyl=32765;
					uint32_t tmp=(uint32_t)freesize*1024*1024/(type=="cdrom"?2048*1:512*32);
					if (tmp>65534) numc=type=="cdrom"?(tmp+65535)/65536:64;
                    uint32_t free_size_cyl=(uint32_t)freesize*1024*1024/(numc*(type=="cdrom"?2048:512));
                    if (free_size_cyl>65534) free_size_cyl=65534;
                    if (total_size_cyl<free_size_cyl) total_size_cyl=free_size_cyl+10;
                    if (total_size_cyl>65534) total_size_cyl=65534;
                    sprintf(teststr,type=="cdrom"?"2048,%u,%u,%u":"512,%u,%u,%u",numc,total_size_cyl,free_size_cyl);
                }
                str_size=teststr;
            }
           
            cmd->FindString("-size",str_size,true);
            char number[21] = { 0 }; const char* scan = str_size.c_str();
            Bitu index = 0; Bitu count = 0;
            /* Parse the str_size string */
            while (*scan && index < 20 && count < 4) {
                if (*scan==',') {
                    number[index] = 0;
                    sizes[count++] = atoi(number);
                    index = 0;
                } else number[index++] = *scan;
                scan++;
            }
            if (count < 4) {
                number[index] = 0; //always goes correct as index is max 20 at this point.
                sizes[count] = atoi(number);
            }
        
            // get the drive letter
            cmd->FindCommand(1,temp_line);
            if ((temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) goto showusage;
            int i_drive = toupper(temp_line[0]);
            if (!isalpha(i_drive)) goto showusage;
            if ((i_drive - 'A') >= DOS_DRIVES || (i_drive - 'A') < 0) goto showusage;
            if (!cmd->FindCommand(2,temp_line)) goto showusage;
            if (!temp_line.size()) goto showusage;
			if (cmd->FindExist("-u",true)) {
                bool curdrv = toupper(i_drive)-'A' == DOS_GetDefaultDrive();
                const char *msg=UnmountHelper(i_drive);
				if (!quiet) WriteOut(msg, toupper(i_drive));
				if (!cmd->FindCommand(2,temp_line)||!temp_line.size()) return;
                if (curdrv && toupper(i_drive)-'A' != DOS_GetDefaultDrive()) removed = true;
			}
            drive = static_cast<char>(i_drive);
            if (type == "overlay") {
                //Ensure that the base drive exists:
                if (!Drives[drive-'A']) {
                    if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_NO_BASE"));
                    return;
                }
            } else if (Drives[drive-'A']) {
                if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),drive,Drives[drive-'A']->GetInfo());
                return;
            }

            temp_line.erase(std::find_if(temp_line.rbegin(), temp_line.rend(), [](unsigned char ch) {return !std::isspace(ch);}).base(), temp_line.end());
            if(path_relative_to_last_config && control->configfiles.size() && !Cross::IsPathAbsolute(temp_line)) {
		        std::string lastconfigdir(control->configfiles[control->configfiles.size()-1]);
                std::string::size_type pos = lastconfigdir.rfind(CROSS_FILESPLIT);
                if(pos == std::string::npos) pos = 0; //No directory then erase string
                lastconfigdir.erase(pos);
                if (lastconfigdir.length())	temp_line = lastconfigdir + CROSS_FILESPLIT + temp_line;
            }
            bool is_physfs = temp_line.find(':',((temp_line[0]|0x20) >= 'a' && (temp_line[0]|0x20) <= 'z')?2:0) != std::string::npos;
            struct stat test;
            //Win32 : strip tailing backslashes
            //os2: some special drive check
            //rest: substitute ~ for home
            bool failed = false;

            (void)failed;// MAY BE UNUSED

#if defined (RISCOS)
            // If the user provided a RISC OS style path, convert it to a Unix style path
            // TODO: Disable UnixLib's automatic path conversion and use RISC OS style paths internally?
            if (temp_line.find('$',0) != std::string::npos) {
                char fname[PATH_MAX];
                is_physfs = false;
                __unixify_std(temp_line.c_str(), fname, sizeof(fname), 0);
		temp_line = fname;
            }
#endif

#if defined (WIN32) || defined(OS2)
            // Windows: Workaround for LaunchBox
            if (is_physfs && temp_line.size()>4 && temp_line[0]=='\'' && toupper(temp_line[1])>='A' && toupper(temp_line[1])<='Z' && temp_line[2]==':' && (temp_line[3]=='/' || temp_line[3]=='\\') && temp_line.back()=='\'') {
                temp_line = temp_line.substr(1, temp_line.size()-2);
                is_physfs = temp_line.find(':',((temp_line[0]|0x20) >= 'a' && (temp_line[0]|0x20) <= 'z')?2:0) != std::string::npos;
            } else if (is_physfs && temp_line.size()>3 && temp_line[0]=='\'' && toupper(temp_line[1])>='A' && toupper(temp_line[1])<='Z' && temp_line[2]==':' && (temp_line[3]=='/' || temp_line[3]=='\\')) {
                std::string line=trim((char *)cmd->GetRawCmdline().c_str());
                std::size_t space=line.find(' ');
                if (space!=std::string::npos) {
                    line=trim((char *)line.substr(space).c_str());
                    std::size_t found=line.back()=='\''?line.find_last_of('\''):line.rfind("' ");
                    if (found!=std::string::npos&&found>2) {
                        temp_line=line.substr(1, found-1);
                        is_physfs = temp_line.find(':',((temp_line[0]|0x20) >= 'a' && (temp_line[0]|0x20) <= 'z')?2:0) != std::string::npos;
                    }
                }
            }
#else
            // Linux: Convert backslash to forward slash
            if (!is_physfs && temp_line.size() > 0) {
                for (size_t i=0;i < temp_line.size();i++) {
                    if (temp_line[i] == '\\')
                        temp_line[i] = '/';
                }
            }
#endif

            bool useh = false;
#if defined (WIN32)
            ht_stat_t htest;
#else
            struct stat htest;
#endif
#if defined (WIN32) || defined(OS2)
            /* Removing trailing backslash if not root dir so stat will succeed */
            if(temp_line.size() > 3 && temp_line[temp_line.size()-1]=='\\') temp_line.erase(temp_line.size()-1,1);
			if(temp_line.size() > 4 && temp_line[0]=='\\' && temp_line[1]=='\\' && temp_line[2]!='\\' && std::count(temp_line.begin()+3, temp_line.end(), '\\')==1) temp_line.append("\\");
            if (!is_physfs && stat(temp_line.c_str(),&test)) {
#endif
#if defined(WIN32)
                const host_cnv_char_t* host_name = CodePageGuestToHost(temp_line.c_str());
                if (host_name == NULL || ht_stat(host_name, &htest)) failed = true;
                useh = true;
            }
#elif defined (OS2)
                if (temp_line.size() <= 2) // Seems to be a drive.
                {
                    failed = true;
                    HFILE cdrom_fd = 0;
                    ULONG ulAction = 0;

                    APIRET rc = DosOpen((unsigned char*)temp_line.c_str(), &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
                        OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
                    DosClose(cdrom_fd);
                    if (rc != NO_ERROR && rc != ERROR_NOT_READY)
                    {
                        failed = true;
                    } else {
                        failed = false;
                    }
                }
            }
#else
            if (!is_physfs && stat(temp_line.c_str(),&test)) {
                failed = true;
                Cross::ResolveHomedir(temp_line);
                //Try again after resolving ~
                if(!stat(temp_line.c_str(),&test)) failed = false;
            }
#endif
            if(failed) {
                if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
                return;
            }
            /* Not a switch so a normal directory/file */
            if (!is_physfs && !S_ISDIR(useh?htest.st_mode:test.st_mode)) {
#ifdef OS2
                HFILE cdrom_fd = 0;
                ULONG ulAction = 0;

                APIRET rc = DosOpen((unsigned char*)temp_line.c_str(), &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
                    OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
                DosClose(cdrom_fd);
                if (rc != NO_ERROR && rc != ERROR_NOT_READY)
#endif
                {
                    is_physfs = true;
                    temp_line.insert(0, 1, ':');
                    /*if (!quiet) {
                        WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
                        if (temp_line.length()>4) {
                            char ext[5];
                            strncpy(ext, temp_line.substr(temp_line.length()-4).c_str(), 4);
                            ext[4]=0;
                            if (!strcasecmp(ext, ".iso")||!strcasecmp(ext, ".cue")||!strcasecmp(ext, ".bin")||!strcasecmp(ext, ".chd")||!strcasecmp(ext, ".mdf")||!strcasecmp(ext, ".ima")||!strcasecmp(ext, ".img")||!strcasecmp(ext, ".vhd")||!strcasecmp(ext, ".hdi"))
                                WriteOut(MSG_Get("PROGRAM_MOUNT_IMGMOUNT"),temp_line.c_str());
                        }
                    }
                    return;*/
                }
            }

            if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
            uint8_t bit8size=(uint8_t) sizes[1];
            exist = drive - 'A' < DOS_DRIVES && drive - 'A' >= 0 && Drives[drive - 'A'];
            if (type=="cdrom") {
                int num = -1;
                cmd->FindInt("-usecd",num,true);
                int error = 0;
                if (cmd->FindExist("-aspi",false)) {
                    MSCDEX_SetCDInterface(CDROM_USE_ASPI, num);
                } else if (cmd->FindExist("-ioctl_dio",false)) {
                    MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
                } else if (cmd->FindExist("-ioctl_dx",false)) {
                    MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
#if defined (WIN32)
                } else if (cmd->FindExist("-ioctl_mci",false)) {
                    MSCDEX_SetCDInterface(CDROM_USE_IOCTL_MCI, num);
#endif
                } else if (cmd->FindExist("-noioctl",false)) {
                    MSCDEX_SetCDInterface(CDROM_USE_SDL, num);
                } else {
#if defined (WIN32)
                    // Check OS
                    if (IsWindowsVistaOrGreater()) {
                        MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
                    } else {
                        MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
                    }
#else
                    MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
#endif
                }
                if (is_physfs)
					newdrive  = new physfscdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error,options);
                else
                    newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,error,options);
                // Check Mscdex, if it worked out...
                if (!quiet)
                switch (error) {
                    case 0  :   WriteOut(MSG_Get("MSCDEX_SUCCESS"));                break;
                    case 1  :   WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));  break;
                    case 2  :   WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));    break;
                    case 3  :   WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));             break;
                    case 4  :   WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));        break;
                    case 5  :   WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));        break;
                    case 10 :   WriteOut(MSG_Get("PROGRAM_MOUNT_PHYSFS_ERROR"));    break;
                    default :   WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));          break;
                }
                if (error && error!=5) {
                    delete newdrive;
                    return;
                }
            } else {
                /* Give a warning when mount c:\ or the / */
                if (mountwarning && !quiet && !nowarn) {
#if defined (WIN32) || defined(OS2)
                    if( (temp_line == "c:\\") || (temp_line == "C:\\") ||
                        (temp_line == "c:/") || (temp_line == "C:/")    )
                        WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
#else
                    if(temp_line == "/") WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_OTHER"));
#endif
                }
                if (is_physfs) {
                    int error = 0;
					newdrive=new physfsDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,error,options);
                    if (error) {
                        if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_PHYSFS_ERROR"));
                        delete newdrive;
                        return;
                    }
                } else if(type == "overlay") {
                  physfsDrive* pdp = dynamic_cast<physfsDrive*>(Drives[drive-'A']);
                  physfscdromDrive* pcdp = dynamic_cast<physfscdromDrive*>(Drives[drive-'A']);
                  if (pdp && !pcdp) {
                      if (pdp->setOverlaydir(temp_line.c_str()))
                          WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),(temp_line+(temp_line.size()&&temp_line.back()!=CROSS_FILESPLIT?std::string(1, CROSS_FILESPLIT):"")+std::string(1, drive)+"_DRIVE").c_str(),drive);
                      else
                          WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_ERROR"));
                      return;
                  }
                  localDrive* ldp = dynamic_cast<localDrive*>(Drives[drive-'A']);
                  cdromDrive* cdp = dynamic_cast<cdromDrive*>(Drives[drive-'A']);
                  if (!ldp || cdp || pcdp) {
					  if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE"));
                      return;
                  }
                  std::string base = ldp->getBasedir();
                  uint8_t o_error = 0;
                  newdrive = new Overlay_Drive(base.c_str(),temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,o_error,options);
                  //Erase old drive on succes
                  if (newdrive) {
                      if (o_error) {
                          if (quiet) {delete newdrive;return;}
                          if (o_error == 1) WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_MIXED_BASE"));
                          else if (o_error == 2) WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE"));
                          else WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_ERROR"));
                          delete newdrive;
                          return;
                      } else {
						  Overlay_Drive* odrive=dynamic_cast<Overlay_Drive*>(newdrive);
						  if (odrive!=NULL) {
							odrive->ovlnocachedir = nocachedir;
							odrive->ovlreadonly = readonly;
						  }
					  }

						//Copy current directory if not marked as deleted.
						if (newdrive->TestDir(ldp->curdir)) {
							strcpy(newdrive->curdir,ldp->curdir);
						}

                      delete Drives[drive-'A'];
                      Drives[drive-'A'] = 0;
                  } else { 
                      if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_ERROR"));
                      return;
                  }
              } else {
                    newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,options);
                    newdrive->nocachedir = nocachedir;
                    newdrive->readonly = readonly;
                }
            }
        } else {
            if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
            return;
        }
        if (!newdrive) E_Exit("DOS:Can't create drive");
        Drives[drive-'A']=newdrive;
        if (removed && !exist) DOS_SetDefaultDrive(drive-'A');
        DOS_EnableDriveMenu(drive);
        /* Set the correct media byte in the table */
        mem_writeb(Real2Phys(dos.tables.mediaid)+((unsigned int)drive-'A')*dos.tables.dpb_size,newdrive->GetMediaByte());
        if (!quiet) {
			if (type != "overlay") WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,newdrive->GetInfo());
			else WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),temp_line.c_str(),drive);
		}
        /* check if volume label is given and don't allow it to updated in the future */
        if (cmd->FindString("-label",label,true)) newdrive->SetLabel(label.c_str(),iscdrom,false);
        /* For hard drives set the label to DRIVELETTER_Drive.
         * For floppy drives set the label to DRIVELETTER_Floppy.
         * This way every drive except cdroms should get a label.*/
        else if(type == "dir" || type == "overlay") { 
#if defined (WIN32) || defined(OS2)
            if(temp_line.size()==3 && toupper(drive) == toupper(temp_line[0]))  {
                // automatic mount
            } else {
                label = drive; label += "_DRIVE";
                newdrive->SetLabel(label.c_str(),iscdrom,false);
            }
#endif
        } else if(type == "floppy") {
#if defined (WIN32) || defined(OS2)
            if(temp_line.size()==3 && toupper(drive) == toupper(temp_line[0]))  {
                // automatic mount
            } else {
                label = drive; label += "_FLOPPY";
                newdrive->SetLabel(label.c_str(),iscdrom,true);
            }
#endif
        }
        if(type == "floppy") incrementFDD();
        return;
showusage:
        WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"));
        return;
    }
};

static void MOUNT_ProgramStart(Program * * make) {
    *make=new MOUNT;
}

void runMount(const char *str) {
	MOUNT mount;
	mount.cmd=new CommandLine("MOUNT", str);
	mount.Run();
}

void GUI_Run(bool pressed);

class CFGTOOL : public Program {
public:
    void Run(void) {
        if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
			WriteOut("Starts DOSBox-X's graphical configuration tool.\n\nCFGTOOL\n\nNote: You can also use CONFIG command for command-line configurations.\n");
            return;
		}
        GUI_Run(false); /* So that I don't have to run the keymapper on every setup of mine just to get the GUI --J.C */
    }
};

static void CFGTOOL_ProgramStart(Program * * make) {
    *make=new CFGTOOL;
}

extern bool custom_bios;
extern uint32_t floppytype;
extern bool boot_debug_break;
extern Bitu BIOS_bootfail_code_offset;

void DisableINT33();
void EMS_DoShutDown();
void XMS_DoShutDown();
void DOS_DoShutDown();
void GUS_DOS_Shutdown();
void SBLASTER_DOS_Shutdown();

unsigned char PC98_ITF_ROM[0x8000];
bool PC98_ITF_ROM_init = false;
unsigned char PC98_BANK_Select = 0x12;

#include "mem.h"
#include "paging.h"

class PC98ITFPageHandler : public PageHandler {
public:
    PC98ITFPageHandler() : PageHandler(PFLAG_READABLE|PFLAG_HASROM) {}
    PC98ITFPageHandler(Bitu flags) : PageHandler(flags) {}
    HostPt GetHostReadPt(Bitu phys_page) {
        return PC98_ITF_ROM+(phys_page&0x7)*MEM_PAGESIZE;
    }
    HostPt GetHostWritePt(Bitu phys_page) {
        return PC98_ITF_ROM+(phys_page&0x7)*MEM_PAGESIZE;
    }
    void writeb(PhysPt addr,uint8_t val){
        LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
    }
    void writew(PhysPt addr,uint16_t val){
        LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
    }
    void writed(PhysPt addr,uint32_t val){
        LOG(LOG_CPU,LOG_ERROR)("Write %x to rom at %x",(int)val,(int)addr);
    }
};

PC98ITFPageHandler          mem_itf_rom;

bool FDC_AssignINT13Disk(unsigned char drv);
void MEM_RegisterHandler(Bitu phys_page,PageHandler * handler,Bitu page_range);
void MEM_ResetPageHandler_Unmapped(Bitu phys_page, Bitu pages);
bool MEM_map_ROM_physmem(Bitu start,Bitu end);
PageHandler &Get_ROM_page_handler(void);

// Normal BIOS is in the BIOS memory area
// ITF is in it's own buffer, served by mem_itf_rom
void PC98_BIOS_Bank_Switch(void) {
    if (PC98_BANK_Select == 0x00) {
        MEM_RegisterHandler(0xF8,&mem_itf_rom,0x8);
    }
    else {
        MEM_RegisterHandler(0xF8,&Get_ROM_page_handler(),0x8);
    }

    PAGING_ClearTLB();
}

// BIOS behavior suggests a reset signal puts the BIOS back
void PC98_BIOS_Bank_Switch_Reset(void) {
    LOG_MSG("PC-98 43Dh mapping BIOS back into top of RAM");
    PC98_BANK_Select = 0x12;
    PC98_BIOS_Bank_Switch();
#if 0
    Bitu DEBUG_EnableDebugger(void);
    DEBUG_EnableDebugger();
#endif
}

void pc98_43d_write(Bitu port,Bitu val,Bitu iolen) {
    (void)port;
    (void)iolen;

    LOG_MSG("PC-98 43Dh BIOS bank switching write: 0x%02x",(unsigned int)val);

    switch (val) {
        case 0x00: // ITF
        case 0x10:
        case 0x18:
            PC98_BANK_Select = 0x00;
            PC98_BIOS_Bank_Switch();
            break;
        case 0x12: // BIOS
            PC98_BANK_Select = 0x12;
            PC98_BIOS_Bank_Switch();
            break;
        default:
            LOG_MSG("PC-98 43Dh BIOS bank switching write: 0x%02x unknown value",(unsigned int)val);
            break;
    }
}

#if defined(WIN32)
#include <fcntl.h>
#else
#if defined(MACOSX)
#define _DARWIN_C_SOURCE
#endif
#include <sys/file.h>
#endif
FILE *retfile = NULL;
FILE * fopen_lock(const char * fname, const char * mode) {
    if (lockmount && strlen(mode)>1&&mode[strlen(mode)-1]=='+') {
#if defined(WIN32)
        HANDLE hFile = CreateFile(fname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            const host_cnv_char_t* host_name = CodePageGuestToHost(fname);
            if (host_name != NULL) hFile = CreateFileW(host_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (hFile == INVALID_HANDLE_VALUE) return NULL;
        int nHandle = _open_osfhandle((intptr_t)hFile, _O_RDONLY);
        if (nHandle == -1) {CloseHandle(hFile);return NULL;}
        retfile = _fdopen(nHandle, mode);
        if(!retfile) {CloseHandle(hFile);return NULL;}
        LockFile(hFile, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
#else
        retfile = fopen64(fname, mode);
        if (retfile == NULL) return NULL; /* did you know fopen returns NULL if it cannot open the file? */
        int lock = flock(fileno(retfile), LOCK_EX | LOCK_NB); /* did you know fileno() assumes retfile != NULL and you will segfault if that is wrong? */
        if (lock < 0) {
            fclose(retfile); /* don't leak file handles on failure to flock() */
            return NULL;
        }
#endif
    } else {
        retfile = fopen64(fname, mode);
#if defined(WIN32)
        if (retfile == NULL) {
            const host_cnv_char_t* host_name = CodePageGuestToHost(fname);
            if (host_name != NULL) {
                const size_t size = strlen(mode)+1;
                wchar_t* wmode = new wchar_t[size];
                mbstowcs (wmode, mode, size);
                retfile = _wfopen(host_name, wmode);
            }
        }
#endif
    }
    return retfile;
}

/*! \brief          BOOT.COM utility to boot a floppy or hard disk device.
 *
 *  \description    Users will use this command to boot a guest operating system from
 *                  a disk image. Options are provided to specify the device to boot
 *                  from (if the image is already assigned) or a floppy disk image
 *                  specified on the command line.
 */
class BOOT : public Program {
public:
    BOOT() {
        for (size_t i=0;i < MAX_SWAPPABLE_DISKS;i++) newDiskSwap[i] = NULL;
    }
    virtual ~BOOT() {
        for (size_t i=0;i < MAX_SWAPPABLE_DISKS;i++) {
            if (newDiskSwap[i] != NULL) {
                newDiskSwap[i]->Release();
                newDiskSwap[i] = NULL;
            }
        }
    }
    /*! \brief      Array of disk images to add to floppy swaplist
     */
    imageDisk* newDiskSwap[MAX_SWAPPABLE_DISKS] = {};

private:

    /*! \brief      Open a file as a disk image and return FILE* handle and size
     */
    FILE *getFSFile_mounted(char const* filename, uint32_t *ksize, uint32_t *bsize, uint8_t *error) {
        //if return NULL then put in error the errormessage code if an error was requested
        bool tryload = (*error)?true:false;
        *error = 0;
        uint8_t drive;
        char fullname[DOS_PATHLENGTH];

        localDrive* ldp=0;
		bool readonly=wpcolon&&strlen(filename)>1&&filename[0]==':';
        if (!DOS_MakeName(const_cast<char*>(readonly?filename+1:filename),fullname,&drive)) return NULL;

        try {       
            ldp=dynamic_cast<localDrive*>(Drives[drive]);
            if(!ldp) return NULL;

            FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
            if(tmpfile == NULL) {
                if (!tryload) *error=1;
                return NULL;
            }

            // get file size
            fseek(tmpfile,0L, SEEK_END);
            *ksize = uint32_t(ftell(tmpfile) / 1024);
            *bsize = uint32_t(ftell(tmpfile));
            fclose(tmpfile);

			if (!readonly)
				tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
            if(readonly || tmpfile == NULL) {
//              if (!tryload) *error=2;
//              return NULL;
                WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
                tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
                if(tmpfile == NULL) {
                    if (!tryload) *error=1;
                    return NULL;
                }
            }

            return tmpfile;
        }
        catch(...) {
            return NULL;
        }
    }

    /*! \brief      Open a file as a disk image and return FILE* handle and size
     */
    FILE *getFSFile(char const * filename, uint32_t *ksize, uint32_t *bsize,bool tryload=false) {
        uint8_t error = tryload?1:0;
        FILE* tmpfile = getFSFile_mounted(filename,ksize,bsize,&error);
        if(tmpfile) return tmpfile;
        //File not found on mounted filesystem. Try regular filesystem
        std::string filename_s(filename);
        Cross::ResolveHomedir(filename_s);
		bool readonly=wpcolon&&filename_s.length()>1&&filename_s[0]==':';
		if (!readonly)
			tmpfile = fopen_lock(filename_s.c_str(),"rb+");
        if(readonly || !tmpfile) {
            if( (tmpfile = fopen(readonly?filename_s.c_str()+1:filename_s.c_str(),"rb")) ) {
                //File exists; So can't be opened in correct mode => error 2
//              fclose(tmpfile);
//              if(tryload) error = 2;
                WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
                fseek(tmpfile,0L, SEEK_END);
                *ksize = uint32_t(ftell(tmpfile) / 1024);
                *bsize = uint32_t(ftell(tmpfile));
                return tmpfile;
            }
            // Give the delayed errormessages from the mounted variant (or from above)
            if(error == 1) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
            if(error == 2) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
            return NULL;
        }
        fseek(tmpfile,0L, SEEK_END);
        *ksize = uint32_t(ftell(tmpfile) / 1024);
        *bsize = uint32_t(ftell(tmpfile));
        return tmpfile;
    }

    /*! \brief      Utility function to print generic boot error
     */
    void printError(void) {
        WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"));
    }

public:
   
    /*! \brief      Program entry point, when the command is run
     */
    void Run(void) {
        std::string bios;
        std::string boothax_str;
        bool pc98_640x200 = true;
        bool pc98_show_graphics = false;
        bool bios_boot = false;
        bool swaponedrive = false;
        bool force = false;
        int quiet = 0;

        //Hack To allow long commandlines
        ChangeToLongCmd();

        boot_debug_break = false;
        if (cmd->FindExist("-debug",true))
            boot_debug_break = true;

        if (cmd->FindExist("-swap-one-drive",true))
            swaponedrive = true;

        // debugging options
        if (cmd->FindExist("-pc98-640x200",true))
            pc98_640x200 = true;
        if (cmd->FindExist("-pc98-640x400",true))
            pc98_640x200 = false;
        if (cmd->FindExist("-pc98-graphics",true))
            pc98_show_graphics = true;

        if (cmd->FindExist("-q",true))
            quiet = 1;
        if (cmd->FindExist("-qq",true))
            quiet = 2;

        if (cmd->FindExist("-force",true))
            force = true;

        if (cmd->FindString("-bios",bios,true))
            bios_boot = true;

        cmd->FindString("-boothax",boothax_str,true);

        if (boothax_str == "msdos") // WARNING: For MS-DOS only, or the real-mode portion of Windows 95/98/ME.
            boothax = BOOTHAX_MSDOS; // do NOT use while in the graphical portion of Windows 95/98/ME especially a DOS VM.
        else if (boothax_str == "")
            boothax = BOOTHAX_NONE;
        else {
            if (!quiet) WriteOut("Unknown boothax mode");
            return;
        }

        /* In secure mode don't allow people to boot stuff. 
         * They might try to corrupt the data on it */
        if(control->SecureMode()) {
            if (!quiet) WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
            return;
        }

        if (bios_boot) {
            uint32_t isz1,isz2;

            if (bios.empty()) {
                if (!quiet) WriteOut("Must specify BIOS image to boot\n");
                return;
            }

            // NOTES:
            // 
            // Regarding PC-98 mode, you should use an older BIOS image.
            // The PC-9821 ROM image(s) I have appear to rely on bank
            // switching parts of itself to boot up and operate.
            //
            // Update: I found some PC-9801 ROM BIOS images online, which
            //         ALSO seem to have a BIOS.ROM, ITF.ROM, etc...
            //
            //         So, this command will not be able to run those
            //         images until port 43Dh (the I/O port used for
            //         bank switching) is implemented in DOSBox-X.
            //
            // In IBM PC/AT mode, this should hopefully allow using old
            // 386/486 BIOSes in DOSBox-X.

            /* load it */
            FILE *romfp = getFSFile(bios.c_str(), &isz1, &isz2);
            if (romfp == NULL) {
                if (!quiet) WriteOut("Unable to open BIOS image\n");
                return;
            }
            Bitu loadsz = (isz2 + 0xFU) & (~0xFU);
            if (loadsz == 0) loadsz = 0x10;
            if (loadsz > (IS_PC98_ARCH ? 0x18000u : 0x20000u)) loadsz = (IS_PC98_ARCH ? 0x18000u : 0x20000u);
            Bitu segbase = 0x100000 - loadsz;
            LOG_MSG("Loading BIOS image %s to 0x%lx, 0x%lx bytes",bios.c_str(),(unsigned long)segbase,(unsigned long)loadsz);
            fseek(romfp, 0, SEEK_SET);
            size_t readResult = fread(GetMemBase()+segbase,loadsz,1,romfp);
            fclose(romfp);
            if (readResult != 1) {
                LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                return;
            }

            // The PC-98 BIOS has a bank switching system where at least the last 32KB
            // can be switched to an Initial Firmware Test BIOS, which initializes the
            // system then switches back to the full 96KB visible during runtime.
            //
            // We can emulate the same if a file named ITF.ROM exists in the same directory
            // as the BIOS image we were given.
            //
            // To enable multiple ITFs per ROM image, we first try <bios filename>.itf.rom
            // before trying itf.rom, for the user's convenience.
            FILE *itffp;

                               itffp = getFSFile((bios + ".itf.rom").c_str(), &isz1, &isz2);
            if (itffp == NULL) itffp = getFSFile((bios + ".ITF.ROM").c_str(), &isz1, &isz2);
            if (itffp == NULL) itffp = getFSFile("itf.rom", &isz1, &isz2);
            if (itffp == NULL) itffp = getFSFile("ITF.ROM", &isz1, &isz2);

            if (itffp != NULL && isz2 <= 0x8000ul) {
                LOG_MSG("Found ITF (initial firmware test) BIOS image (0x%lx bytes)",(unsigned long)isz2);

                memset(PC98_ITF_ROM,0xFF,sizeof(PC98_ITF_ROM));
                readResult = fread(PC98_ITF_ROM,isz2,1,itffp);
                fclose(itffp);
                if (readResult != 1) {
                    LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                    return;
                }
                PC98_ITF_ROM_init = true;
            }

            IO_RegisterWriteHandler(0x43D,pc98_43d_write,IO_MB);

            custom_bios = true;

            /* boot it */
            throw int(8);
        }

        bool bootbyDrive=false;
        FILE *usefile_1=NULL;
        FILE *usefile_2=NULL;
        Bitu i=0; 
        uint32_t floppysize=0;
        uint32_t rombytesize_1=0;
        uint32_t rombytesize_2=0;
        uint8_t drive = 'A';
        std::string cart_cmd="";
        Bitu max_seg;

        /* IBM PC:
         *    CS:IP = 0000:7C00     Load = 07C0:0000
         *    SS:SP = ???
         *
         * PC-98:
         *    CS:IP = 1FE0:0000     Load = 1FE0:0000
         *    SS:SP = 0030:00D8
         */
        Bitu stack_seg=IS_PC98_ARCH ? 0x0030 : 0x7000;
        Bitu load_seg;//=IS_PC98_ARCH ? 0x1FE0 : 0x07C0;

        if (MEM_TotalPages() > 0x9C)
            max_seg = 0x9C00;
        else
            max_seg = MEM_TotalPages() << (12 - 4);

        if ((stack_seg+0x20) > max_seg)
            stack_seg = max_seg - 0x20;

        if(!cmd->GetCount()) {
            uint8_t drv = dos_kernel_disabled?26:DOS_GetDefaultDrive();
            if (drv < 4 && Drives[drv] && !strncmp(Drives[drv]->GetInfo(), "fatDrive ", 9)) {
                drive = 'A' + drv;
                bootbyDrive = true;
            } else {
                printError();
                return;
            }
        } else if (cmd->GetCount()==1) {
			cmd->FindCommand(1, temp_line);
			if (temp_line.length()==2&&toupper(temp_line[0])>='A'&&toupper(temp_line[0])<='Z'&&temp_line[1]==':') {
				drive=toupper(temp_line[0]);
				if ((drive != 'A') && (drive != 'C') && (drive != 'D')) {
					printError();
					return;
				}
				bootbyDrive = true;
			}
		}

		if (!bootbyDrive)
		while(i<cmd->GetCount()) {
            if(cmd->FindCommand((unsigned int)(i+1), temp_line)) {
				if ((temp_line == "/?") || (temp_line == "-?")) {
					printError();
					return;
				}
                if((temp_line == "-l") || (temp_line == "-L")) {
                    /* Specifying drive... next argument then is the drive */
                    bootbyDrive = true;
                    i++;
                    if(cmd->FindCommand((unsigned int)(i+1), temp_line)) {
						if (temp_line.length()==1&&isdigit(temp_line[0]))
							drive='A'+(temp_line[0]-'0');
						else
							drive=toupper(temp_line[0]);
                        if ((drive != 'A') && (drive != 'C') && (drive != 'D')) {
                            printError();
                            return;
                        }

                    } else {
                        printError();
                        return;
                    }
                    i++;
                    continue;
                }

                if((temp_line == "-e") || (temp_line == "-E")) {
                    /* Command mode for PCJr cartridges */
                    i++;
                    if(cmd->FindCommand((unsigned int)(i + 1), temp_line)) {
                        for(size_t ct = 0;ct < temp_line.size();ct++) temp_line[ct] = toupper(temp_line[ct]);
                        cart_cmd = temp_line;
                    } else {
                        printError();
                        return;
                    }
                    i++;
                    continue;
                }

                if (i >= MAX_SWAPPABLE_DISKS) {
                    return; //TODO give a warning.
                }

                uint32_t rombytesize=0;
				bool readonly=wpcolon&&temp_line.length()>1&&temp_line[0]==':';
                if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), readonly?temp_line.c_str()+1:temp_line.c_str());
                FILE *usefile = getFSFile(temp_line.c_str(), &floppysize, &rombytesize);
                if(usefile != NULL) {
                    char tmp[256];

                    if (newDiskSwap[i] != NULL) {
                        newDiskSwap[i]->Release();
                        newDiskSwap[i] = NULL;
                    }

                    fseeko64(usefile, 0L, SEEK_SET);
                    size_t readResult = fread(tmp,256,1,usefile); // look for magic signatures
                    if (readResult != 1) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                        return;
                    }

                    const char *ext = strrchr(temp_line.c_str(),'.'), *fname=readonly?temp_line.c_str()+1:temp_line.c_str();

                    if (ext != NULL && !strcasecmp(ext, ".d88")) {
                        newDiskSwap[i] = new imageDiskD88(usefile, (uint8_t *)fname, floppysize, false);
                    }
                    else if (!memcmp(tmp,"VFD1.",5)) { /* FDD files */
                        newDiskSwap[i] = new imageDiskVFD(usefile, (uint8_t *)fname, floppysize, false);
                    }
                    else if (!memcmp(tmp,"T98FDDIMAGE.R0\0\0",16)) {
                        newDiskSwap[i] = new imageDiskNFD(usefile, (uint8_t *)fname, floppysize, false, 0);
                    }
                    else if (!memcmp(tmp,"T98FDDIMAGE.R1\0\0",16)) {
                        newDiskSwap[i] = new imageDiskNFD(usefile, (uint8_t *)fname, floppysize, false, 1);
                    }
                    else {
                        newDiskSwap[i] = new imageDisk(usefile, fname, floppysize, false);
                    }
                    newDiskSwap[i]->Addref();
                    if (newDiskSwap[i]->active && !newDiskSwap[i]->hardDrive) incrementFDD(); //moved from imageDisk constructor

                    if (usefile_1==NULL) {
                        usefile_1=usefile;
                        rombytesize_1=rombytesize;
                    } else {
                        usefile_2=usefile;
                        rombytesize_2=rombytesize;
                    }
                } else {
                    if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), readonly?temp_line.c_str()+1:temp_line.c_str());
                    return;
                }

            }
            i++;
        }

        if (!bootbyDrive) {
            if (i == 0) {
                if (!quiet) WriteOut("No images specified");
                return;
            }

            if (i > 1) {
                /* if more than one image is given, then this drive becomes the focus of the swaplist */
                if (swapInDisksSpecificDrive >= 0 && swapInDisksSpecificDrive != (drive - 65)) {
                    if (!quiet) WriteOut("Multiple disk images specified and another drive is already connected to the swap list");
                    return;
                }
                else if (swapInDisksSpecificDrive < 0 && swaponedrive) {
                    swapInDisksSpecificDrive = drive - 65;
                }

                /* transfer to the diskSwap array */
                for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                    if (diskSwap[si] != NULL) {
                        diskSwap[si]->Release();
                        diskSwap[si] = NULL;
                    }

                    diskSwap[si] = newDiskSwap[si];
                    newDiskSwap[si] = NULL;
                }

                swapPosition = 0;
                swapInDisks(-1);
            }
            else {
                if (swapInDisksSpecificDrive == (drive - 65)) {
                    /* if we're replacing the diskSwap drive clear it now */
                    for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                        if (diskSwap[si] != NULL) {
                            diskSwap[si]->Release();
                            diskSwap[si] = NULL;
                        }
                    }

                    swapInDisksSpecificDrive = -1;
                }

                /* attach directly without using the swap list */
                if (imageDiskList[drive-65] != NULL) {
                    imageDiskChange[drive-65] = true;
                    imageDiskList[drive-65]->Release();
                    imageDiskList[drive-65] = NULL;
                }

                imageDiskList[drive-65] = newDiskSwap[0];
                newDiskSwap[0] = NULL;
            }
        }

        if(imageDiskList[drive-65]==NULL) {
            if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
            return;
        }

        // .D88 images come from PC-88 which usually means the boot sector is full
        // of Z80 executable code, therefore it's very unlikely the boot sector will
        // work with our x86 emulation!
        //
        // If the user is REALLY REALLY SURE they want to try executing Z80 bootsector
        // code as x86, they're free to use --force.
        //
        // However PC-98 games are also distributed as .D88 images and therefore
        // we probably CAN boot the image.
        //
        // It depends on the fd_type field of the image.
        if (!force && imageDiskList[drive-65]->class_id == imageDisk::ID_D88) {
            if (reinterpret_cast<imageDiskD88*>(imageDiskList[drive-65])->fd_type_major == imageDiskD88::DISKTYPE_2D) {
                if (!quiet) WriteOut("The D88 image appears to target PC-88 and cannot be booted.");
                return;
            }
        }


        bootSector bootarea;

        if (imageDiskList[drive-65]->getSectSize() > sizeof(bootarea)) {
            if (!quiet) WriteOut("Bytes/sector too large");
            return;
        }

        /* clear the disk change flag.
         * Most OSes don't expect the disk change error signal when they first boot up */
        imageDiskChange[drive-65] = false;

        bool has_read = false;
        bool pc98_sect128 = false;
        unsigned int bootsize = imageDiskList[drive-65]->getSectSize();

        if (!has_read && IS_PC98_ARCH && drive < 'C') {
            /* this may be one of those odd FDD images where track 0, head 0 is all 128-byte sectors
             * and the rest of the disk is 256-byte sectors. */
            if (imageDiskList[drive - 65]->Read_Sector(0, 0, 1, (uint8_t *)&bootarea, 128) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 128, 128) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 256, 128) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 384, 128) == 0) {
                LOG_MSG("First sector is 128 byte/sector. Booting from first four sectors.");
                has_read = true;
                bootsize = 512; // 128 x 4
                pc98_sect128 = true;
            }
        }

        if (!has_read && IS_PC98_ARCH && drive < 'C') {
            /* another nonstandard one with track 0 having 256 bytes/sector while the rest have 1024 bytes/sector */
            if (imageDiskList[drive - 65]->Read_Sector(0, 0, 1, (uint8_t *)&bootarea,       256) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 256, 256) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 512, 256) == 0 &&
                imageDiskList[drive - 65]->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 768, 256) == 0) {
                LOG_MSG("First sector is 256 byte/sector. Booting from first two sectors.");
                has_read = true;
                bootsize = 1024; // 256 x 4
                pc98_sect128 = true;
            }
        }

        /* NTS: Load address is 128KB - sector size */
        load_seg=IS_PC98_ARCH ? (0x2000 - (bootsize/16U)) : 0x07C0;

        if (!has_read) {
            if (imageDiskList[drive - 65]->Read_Sector(0, 0, 1, (uint8_t *)&bootarea) != 0) {
                if (!quiet) WriteOut("Error reading drive");
                return;
            }
        }

        Bitu pcjr_hdr_length = 0;
        uint8_t pcjr_hdr_type = 0; // not a PCjr cartridge
        if ((bootarea.rawdata[0]==0x50) && (bootarea.rawdata[1]==0x43) && (bootarea.rawdata[2]==0x6a) && (bootarea.rawdata[3]==0x72)) {
            pcjr_hdr_type = 1; // JRipCart
            pcjr_hdr_length = 0x200;
        } else if ((bootarea.rawdata[56]==0x50) && (bootarea.rawdata[57]==0x43) && (bootarea.rawdata[58]==0x4a) && (bootarea.rawdata[59]==0x52)) {
            pcjr_hdr_type = 2; // PCJRCart
            pcjr_hdr_length = 0x80;
        }
        
        if (pcjr_hdr_type > 0) {
            if (machine!=MCH_PCJR&&!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
            else {
                uint8_t rombuf[65536];
                Bits cfound_at=-1;
                if (cart_cmd!="") {
                    /* read cartridge data into buffer */
                    fseek(usefile_1, (long)pcjr_hdr_length, SEEK_SET);
                    size_t readResult = fread(rombuf, 1, rombytesize_1-pcjr_hdr_length, usefile_1);
                    if (readResult != rombytesize_1 - pcjr_hdr_length) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                        return;
                    }

                    char cmdlist[1024];
                    cmdlist[0]=0;
                    Bitu ct=6;
                    Bits clen=rombuf[ct];
                    char buf[257];
                    if (cart_cmd=="?") {
                        while (clen!=0) {
                            safe_strncpy(buf,(char*)&rombuf[ct+1],clen);
                            buf[clen]=0;
                            upcase(buf);
                            strcat(cmdlist," ");
                            strcat(cmdlist,buf);
                            ct+=1u+(Bitu)clen+3u;
                            if (ct>sizeof(cmdlist)) break;
                            clen=rombuf[ct];
                        }
                        if (ct>6) {
                            if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
                        } else {
                            if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
                        }
                        for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
                            if(diskSwap[dct]!=NULL) {
                                diskSwap[dct]->Release();
                                diskSwap[dct]=NULL;
                            }
                        }
                        //fclose(usefile_1); //delete diskSwap closes the file
                        return;
                    } else {
                        while (clen!=0) {
                            safe_strncpy(buf,(char*)&rombuf[ct+1],clen);
                            buf[clen]=0;
                            upcase(buf);
                            strcat(cmdlist," ");
                            strcat(cmdlist,buf);
                            ct+=1u+(Bitu)clen;

                            if (cart_cmd==buf) {
                                cfound_at=(Bits)ct;
                                break;
                            }

                            ct+=3;
                            if (ct>sizeof(cmdlist)) break;
                            clen=rombuf[ct];
                        }
                        if (cfound_at<=0) {
                            if (ct>6) {
                                if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
                            } else {
                                if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
                            }
                            for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
                                if(diskSwap[dct]!=NULL) {
                                    diskSwap[dct]->Release();
                                    diskSwap[dct]=NULL;
                                }
                            }
                            //fclose(usefile_1); //Delete diskSwap closes the file
                            return;
                        }
                    }
                }

                void PreparePCJRCartRom(void);
                PreparePCJRCartRom();

                if (usefile_1==NULL) return;

                uint32_t sz1,sz2;
                FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
                if (tfile!=NULL) {
                    fseek(tfile, 0x3000L, SEEK_SET);
                    uint32_t drd=(uint32_t)fread(rombuf, 1, 0xb000, tfile);
                    if (drd==0xb000) {
                        for(i=0;i<0xb000;i++) phys_writeb((PhysPt)(0xf3000+i),rombuf[i]);
                    }
                    fclose(tfile);
                }

                if (usefile_2!=NULL) {
                    unsigned int romseg_pt=0;

                    fseek(usefile_2, 0x0L, SEEK_SET);
                    size_t readResult = fread(rombuf, 1, pcjr_hdr_length, usefile_2);
                    if (readResult != pcjr_hdr_length) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                        return;
                    }

                    if (pcjr_hdr_type == 1) {
                        romseg_pt=host_readw(&rombuf[0x1ce]);
                    } else {
                        fseek(usefile_2, 0x61L, SEEK_SET);
                        int scanResult = fscanf(usefile_2, "%4x", &romseg_pt);
                        if (scanResult == 0) {
                            LOG(LOG_IO, LOG_ERROR) ("Scanning error in Run\n");
                            return;
                        }
                    }
                    /* read cartridge data into buffer */
                    fseek(usefile_2, (long)pcjr_hdr_length, SEEK_SET);
                    readResult = fread(rombuf, 1, rombytesize_2-pcjr_hdr_length, usefile_2);
                    if (readResult != rombytesize_2 - pcjr_hdr_length) {
                        LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                        return;
                    }
                    //fclose(usefile_2); //usefile_2 is in diskSwap structure which should be deleted to close the file

                    /* write cartridge data into ROM */
                    for(i=0;i<rombytesize_2-pcjr_hdr_length;i++) phys_writeb((PhysPt)((romseg_pt<<4)+i),rombuf[i]);
                }

                unsigned int romseg=0;

                fseek(usefile_1, 0x0L, SEEK_SET);
                size_t readResult = fread(rombuf, 1, pcjr_hdr_length, usefile_1);
                if (readResult != pcjr_hdr_length) {
                    LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                    return;
                }

                if (pcjr_hdr_type == 1) {
                    romseg=host_readw(&rombuf[0x1ce]);
                } else {
                    fseek(usefile_1, 0x61L, SEEK_SET);
                    int scanResult = fscanf(usefile_1, "%4x", &romseg);
                    if (scanResult == 0) {
                        LOG(LOG_IO, LOG_ERROR) ("Scanning error in Run\n");
                        return;
                    }
                }
                /* read cartridge data into buffer */
                fseek(usefile_1,(long)pcjr_hdr_length, SEEK_SET);
                readResult = fread(rombuf, 1, rombytesize_1-pcjr_hdr_length, usefile_1);
                if (readResult != rombytesize_1 - pcjr_hdr_length) {
                    LOG(LOG_IO, LOG_ERROR) ("Reading error in Run\n");
                    return;
                }

                //fclose(usefile_1); //usefile_1 is in diskSwap structure which should be deleted to close the file
                /* write cartridge data into ROM */
                for(i=0;i<rombytesize_1-pcjr_hdr_length;i++) phys_writeb((PhysPt)((romseg<<4)+i),rombuf[i]);

                //Close cardridges
                for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
                    if(diskSwap[dct]!=NULL) {
                        diskSwap[dct]->Release();
                        diskSwap[dct]=NULL;
                    }
                }


                if (cart_cmd=="") {
                    uint32_t old_int18=mem_readd(0x60);
                    /* run cartridge setup */
                    SegSet16(ds,romseg);
                    SegSet16(es,romseg);
                    SegSet16(ss,0x8000);
                    reg_esp=0xfffe;
                    CALLBACK_RunRealFar(romseg,0x0003);

                    uint32_t new_int18=mem_readd(0x60);
                    if (old_int18!=new_int18) {
                        /* boot cartridge (int18) */
                        SegSet16(cs,RealSeg(new_int18));
                        reg_ip = RealOff(new_int18);
                    } 
                } else {
                    if (cfound_at>0) {
                        /* run cartridge setup */
                        SegSet16(ds,dos.psp());
                        SegSet16(es,dos.psp());
                        CALLBACK_RunRealFar((uint16_t)romseg,(uint16_t)cfound_at);
                    }
                }
            }
        } else {
            extern const char* RunningProgram;

            if (max_seg < 0x0800) {
                /* TODO: For the adventerous, add a configuration option or command line switch to "BOOT"
                 *       that allows us to boot the guest OS anyway in a manner that is non-standard. */
                if (!quiet) WriteOut("32KB of RAM is required to boot a guest OS\n");
                return;
            }

            /* Other versions of MS-DOS/PC-DOS have their own requirements about memory:
             *    - IBM PC-DOS 1.0/1.1: not too picky, will boot with as little as 32KB even though
             *                          it was intended for the average model with 64KB of RAM.
             *
             *    - IBM PC-DOS 2.1: requires at least 44KB of RAM. will crash on boot otherwise.
             *
             *    - MS-DOS 3.2: requires at least 64KB to boot without crashing, 80KB to make it
             *                  to the command line without halting at "configuration too big for
             *                  memory"*/

            /* TODO: Need a configuration option or a BOOT command option where the user can
             *       dictate where we put the stack: if we put it at 0x7000 or top of memory
             *       (default) or just below the boot sector, or... */

            if((bootarea.rawdata[0]==0) && (bootarea.rawdata[1]==0)) {
                if (!quiet) WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
                return;
            }

			if (quiet<2) {
				char msg[30];
				const uint8_t page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
				strcpy(msg, CURSOR_POS_COL(page)>0?"\r\n":"");
				strcat(msg, "Booting from drive ");
				strcat(msg, std::string(1, drive).c_str());
				strcat(msg, "...\r\n");
				uint16_t s = (uint16_t)strlen(msg);
				DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
			}

            if (IS_PC98_ARCH) {
                for(i=0;i<bootsize;i++) real_writeb((uint16_t)load_seg, (uint16_t)i, bootarea.rawdata[i]);
            }
            else {
                for(i=0;i<bootsize;i++) real_writeb(0, (uint16_t)((load_seg<<4) + i), bootarea.rawdata[i]);
            }

            /* debug */
            LOG_MSG("Booting guest OS stack_seg=0x%04x load_seg=0x%04x\n",(int)stack_seg,(int)load_seg);
            RunningProgram = "Guest OS";

            if (drive == 'A' || drive == 'B') {
                FDC_AssignINT13Disk(drive - 'A');
                if (!IS_PC98_ARCH) incrementFDD();
            }

            /* NTS: IBM PC and PC-98 both use DMA channel 2 for the floppy, though according to
             *      Neko Project II source code, DMA 3 is used for the double density drives (but we don't emulate that yet) */
            /* create appearance of floppy drive DMA usage (Demon's Forge) */
            if (IS_PC98_ARCH) {
                GetDMAChannel(2)->tcount=true;
                GetDMAChannel(3)->tcount=true;
            }
            else {
                if (!IS_TANDY_ARCH && floppysize!=0) GetDMAChannel(2)->tcount=true;
            }

            /* standard method */
            if (IS_PC98_ARCH) {
                /* Based on a CPU register dump at boot time on a real PC-9821:
                 *
                 * DUMP:
                 *
                 * SP: 00D8 SS: 0030 ES: 1FE0 DS: 0000 CS: 1FE0 FL: 0246 BP: 0000
                 * DI: 055C SI: 1FE0 DX: 0001 CX: 0200 BX: 0200 AX: 0030 IP: 0000
                 *
                 * So:
                 *
                 * Stack at 0030:00D8
                 *
                 * CS:IP at load_seg:0000
                 *
                 * load_seg at 0x1FE0 which on the original 128KB PC-98 puts it at the top of memory
                 *
                 */
                SegSet16(cs, (uint16_t)load_seg);
                SegSet16(ds, 0x0000);
                SegSet16(es, (uint16_t)load_seg);
                reg_ip = 0;
                reg_ebx = 0x200;
                reg_esp = 0xD8;
                /* set up stack at a safe place */
                SegSet16(ss, (uint16_t)stack_seg);
                reg_esi = (uint32_t)load_seg;
                reg_ecx = 0x200;
                reg_ebp = 0;
                reg_eax = 0x30;
                reg_edx = 0x1;

                /* It seems 640x200 8-color digital mode is the state of the graphics hardware when the
                 * BIOS boots the OS, and some games like Ys II assume the hardware is in this state.
                 *
                 * If I am wrong, you can pass --pc98-640x400 as a command line option to disable this. */
                if (pc98_640x200) {
                    reg_eax = 0x4200;   // setup 640x200 graphics
                    reg_ecx = 0x8000;   // lower
                    CALLBACK_RunRealInt(0x18);
                }
                else {
                    reg_eax = 0x4200;   // setup 640x400 graphics
                    reg_ecx = 0xC000;   // full
                    CALLBACK_RunRealInt(0x18);
                }

                /* Some HDI images of Orange House games need this option because it assumes NEC MOUSE.COM
                 * behavior where mouse driver init and reset show the graphics layer. Unfortunately the HDI
                 * image uses QMOUSE which does not show the graphics layer. Use this option with those
                 * HDI images to make them playable anyway. */
                if (pc98_show_graphics) {
                    reg_eax = 0x4000;   // show graphics
                    CALLBACK_RunRealInt(0x18);
                }
                else {
                    reg_eax = 0x4100;   // hide graphics (normal state of graphics layer on startup). INT 33h emulation might have enabled it.
                    CALLBACK_RunRealInt(0x18);
                }

                /* PC-98 MS-DOS boot sector behavior suggests that the BIOS does a CALL FAR
                 * to the boot sector, and the boot sector can RETF back to the BIOS on failure. */
                CPU_Push16((uint16_t)(BIOS_bootfail_code_offset >> 4)); /* segment */
                CPU_Push16(BIOS_bootfail_code_offset & 0xF); /* offset */

                /* clear the text layer */
                for (i=0;i < (80*25*2);i += 2) {
                    mem_writew((PhysPt)(0xA0000+i),0x0000);
                    mem_writew((PhysPt)(0xA2000+i),0x00E1);
                }

                /* hide the cursor */
                void PC98_show_cursor(bool show);
                PC98_show_cursor(false);

                /* There is a byte at 0x584 that describes the boot drive + type.
                 * This is confirmed in Neko Project II source and by the behavior
                 * of an MS-DOS boot disk formatted by a PC-98 system.
                 *
                 * There are three values for three different floppy formats, and
                 * one for hard drives */
                uint32_t heads,cyls,sects,ssize;

                imageDiskList[drive-65]->Get_Geometry(&heads,&cyls,&sects,&ssize);

                uint8_t RDISK_EQUIP = 0; /* 488h (ref. PC-9800 Series Technical Data Book - BIOS 1992 page 233 */
                /* bits [7:4] = 640KB FD drives 3:0
                 * bits [3:0] = 1MB FD drives 3:0 */
                uint8_t F2HD_MODE = 0; /* 493h (ref. PC-9800 Series Technical Data Book - BIOS 1992 page 233 */
                /* bits [7:4] = 640KB FD drives 3:0 ??
                 * bits [3:0] = 1MB FD drives 3:0 ?? */
                uint8_t F2DD_MODE = 0; /* 5CAh (ref. PC-9800 Series Technical Data Book - BIOS 1992 page 233 */
                /* bits [7:4] = 640KB FD drives 3:0 ??
                 * bits [3:0] = 1MB FD drives 3:0 ?? */
                uint16_t disk_equip = 0, disk_equip_144 = 0;
                uint8_t scsi_equip = 0;

                /* FIXME: MS-DOS appears to be able to see disk image B: but only
                 *        if the disk format is the same, for some reason.
                 *
                 *        So, apparently you cannot put a 1.44MB image in drive A:
                 *        and a 1.2MB image in drive B: */

                for (i=0;i < 2;i++) {
                    if (imageDiskList[i] != NULL) {
                        disk_equip |= (0x0111u << i); /* 320KB[15:12] 1MB[11:8] 640KB[7:4] unit[1:0] */
                        disk_equip_144 |= (1u << i);
                        F2HD_MODE |= (0x11u << i);
                    }
                }

                for (i=0;i < 2;i++) {
                    if (imageDiskList[i+2] != NULL) {
                        scsi_equip |= (1u << i);

                        uint16_t m = 0x460u + ((uint16_t)i * 4u);

                        mem_writeb(m+0u,sects);
                        mem_writeb(m+1u,heads);
                        mem_writew(m+2u,(cyls & 0xFFFu) + (ssize == 512u ? 0x1000u : 0u) + (ssize == 1024u ? 0x2000u : 0) + 0x8000u/*NP2:hwsec*/);
                    }
                }

                mem_writew(0x55C,disk_equip);   /* disk equipment (drive 0 is present) */
                mem_writew(0x5AE,disk_equip_144);   /* disk equipment (drive 0 is present, 1.44MB) */
                mem_writeb(0x482,scsi_equip);
                mem_writeb(0x488,RDISK_EQUIP); /* RAM disk equip */
                mem_writeb(0x493,F2HD_MODE);
                mem_writeb(0x5CA,F2DD_MODE);

                if (drive >= 'C') {
                    /* hard drive */
                    mem_writeb(0x584,0xA0/*type*/ + (drive - 'C')/*drive*/);
                }
                else if ((ssize == 1024 && heads == 2 && cyls == 77 && sects == 8) || pc98_sect128) {
                    mem_writeb(0x584,0x90/*type*/ + (drive - 65)/*drive*/); /* 1.2MB 3-mode */
                }
                else if (ssize == 512 && heads == 2 && cyls == 80 && sects == 18) {
                    mem_writeb(0x584,0x30/*type*/ + (drive - 65)/*drive*/); /* 1.44MB */
                }
                else {
                    // FIXME
                    LOG_MSG("PC-98 boot: Unable to determine boot drive type for ssize=%u heads=%u cyls=%u sects=%u. Guessing.",
                        ssize,heads,cyls,sects);

                    mem_writeb(0x584,(ssize < 1024 ? 0x30 : 0x90)/*type*/ + (drive - 65)/*drive*/);
                }
            }
            else {
                SegSet16(cs, 0);
                SegSet16(ds, 0);
                SegSet16(es, 0);
                reg_ip = (uint16_t)(load_seg<<4);
                reg_ebx = (uint32_t)(load_seg<<4); //Real code probably uses bx to load the image
                reg_esp = 0x100;
                /* set up stack at a safe place */
                SegSet16(ss, (uint16_t)stack_seg);
                reg_esi = 0;
                reg_ecx = 1;
                reg_ebp = 0;
                reg_eax = 0;
                reg_edx = 0; //Head 0
                if (drive >= 'A' && drive <= 'B')
                    reg_edx += (unsigned int)(drive-'A');
                else if (drive >= 'C' && drive <= 'Z')
                    reg_edx += 0x80u+(unsigned int)(drive-'C');
            }
#ifdef __WIN32__
            // let menu know it boots
            menu.boot=true;
#endif
            bootguest=false;
            bootdrive=drive-65;

            /* forcibly exit the shell, the DOS kernel, and anything else by throwing an exception */
            throw int(2);
        }
    }
};

static void BOOT_ProgramStart(Program * * make) {
    *make=new BOOT;
}

void runBoot(const char *str) {
	BOOT boot;
	boot.cmd=new CommandLine("BOOT", str);
	boot.Run();
}

class LOADROM : public Program {
public:
    void Run(void) {
		if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
			WriteOut(MSG_Get("PROGRAM_LOADROM_HELP"));
			return;
		}

        if (!(cmd->FindCommand(1, temp_line))) {
            WriteOut(MSG_Get("PROGRAM_LOADROM_SPECIFY_FILE"));
            return;
        }

        uint8_t drive;
        char fullname[DOS_PATHLENGTH];
        localDrive* ldp=0;
        if (!DOS_MakeName((char *)temp_line.c_str(),fullname,&drive)) return;

        try {
            /* try to read ROM file into buffer */
            ldp=dynamic_cast<localDrive*>(Drives[drive]);
            if(!ldp) return;

            FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
            if(tmpfile == NULL) {
                WriteOut(MSG_Get("PROGRAM_LOADROM_CANT_OPEN"));
                return;
            }
            fseek(tmpfile, 0L, SEEK_END);
            if (ftell(tmpfile)>0x8000) {
                WriteOut(MSG_Get("PROGRAM_LOADROM_TOO_LARGE"));
                fclose(tmpfile);
                return;
            }
            fseek(tmpfile, 0L, SEEK_SET);
            uint8_t rom_buffer[0x8000];
            Bitu data_read = fread(rom_buffer, 1, 0x8000, tmpfile);
            fclose(tmpfile);

            /* try to identify ROM type */
            PhysPt rom_base = 0;
            if (data_read >= 0x4000 && rom_buffer[0] == 0x55 && rom_buffer[1] == 0xaa &&
                (rom_buffer[3] & 0xfc) == 0xe8 && strncmp((char*)(&rom_buffer[0x1e]), "IBM", 3) == 0) {

                if (!IS_EGAVGA_ARCH) {
                    WriteOut(MSG_Get("PROGRAM_LOADROM_INCOMPATIBLE"));
                    return;
                }
                rom_base = PhysMake(0xc000, 0); // video BIOS
            }
            else if (data_read == 0x8000 && rom_buffer[0] == 0xe9 && rom_buffer[1] == 0x8f &&
                rom_buffer[2] == 0x7e && strncmp((char*)(&rom_buffer[0x4cd4]), "IBM", 3) == 0) {

                rom_base = PhysMake(0xf600, 0); // BASIC
            }

            if (rom_base) {
                /* write buffer into ROM */
                for (Bitu i=0; i<data_read; i++) phys_writeb((PhysPt)(rom_base + i), rom_buffer[i]);

                if (rom_base == 0xc0000) {
                    /* initialize video BIOS */
                    phys_writeb(PhysMake(0xf000, 0xf065), 0xcf);
                    reg_flags &= ~FLAG_IF;
                    CALLBACK_RunRealFar(0xc000, 0x0003);
                    LOG_MSG("Video BIOS ROM loaded and initialized.");
                }
                else WriteOut(MSG_Get("PROGRAM_LOADROM_BASIC_LOADED"));
            }
            else WriteOut(MSG_Get("PROGRAM_LOADROM_UNRECOGNIZED"));
        }
        catch(...) {
            return;
        }
    }
};

static void LOADROM_ProgramStart(Program * * make) {
    *make=new LOADROM;
}

#if C_DEBUG
class BIOSTEST : public Program {
public:
    void Run(void) {
        if (!(cmd->FindCommand(1, temp_line))) {
            WriteOut("Must specify BIOS file to load.\n");
            return;
        }
        if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
            WriteOut(MSG_Get("PROGRAM_BIOSTEST_HELP"));
            return;
        }

        uint8_t drive;
        char fullname[DOS_PATHLENGTH];
        localDrive* ldp = 0;
        if (!DOS_MakeName((char*)temp_line.c_str(), fullname, &drive)) return;

        try {
            /* try to read ROM file into buffer */
            ldp = dynamic_cast<localDrive*>(Drives[drive]);
            if (!ldp) return;

            FILE* tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
            if (tmpfile == NULL) {
                WriteOut("Can't open a file");
                return;
            }
            fseek(tmpfile, 0L, SEEK_END);
            if (ftell(tmpfile) > 64 * 1024) {
                WriteOut("BIOS File too large");
                fclose(tmpfile);
                return;
            }
            fseek(tmpfile, 0L, SEEK_SET);
            uint8_t buffer[64 * 1024];
            Bitu data_read = fread(buffer, 1, sizeof(buffer), tmpfile);
            fclose(tmpfile);

            uint32_t rom_base = PhysMake(0xf000, 0); // override regular dosbox bios
                    /* write buffer into ROM */
            for (Bitu i = 0; i < data_read; i++) phys_writeb((PhysPt)(rom_base + i), buffer[i]);

            //Start executing this bios
            memset(&cpu_regs, 0, sizeof(cpu_regs));
            memset(&Segs, 0, sizeof(Segs));

            SegSet16(cs, 0xf000);
            reg_eip = 0xfff0;
        }
        catch (...) {
            return;
        }
    }
};

static void BIOSTEST_ProgramStart(Program** make) {
    *make = new BIOSTEST;
}
#endif

const uint8_t freedos_mbr[] = {
    0x33,0xC0,0x8E,0xC0,0x8E,0xD8,0x8E,0xD0,0xBC,0x00,0x7C,0xFC,0x8B,0xF4,0xBF,0x00, 
    0x06,0xB9,0x00,0x01,0xF2,0xA5,0xEA,0x67,0x06,0x00,0x00,0x8B,0xD5,0x58,0xA2,0x4F, // 10h
    0x07,0x3C,0x35,0x74,0x23,0xB4,0x10,0xF6,0xE4,0x05,0xAE,0x04,0x8B,0xF0,0x80,0x7C, // 20h
    0x04,0x00,0x74,0x44,0x80,0x7C,0x04,0x05,0x74,0x3E,0xC6,0x04,0x80,0xE8,0xDA,0x00, 
    0x8A,0x74,0x01,0x8B,0x4C,0x02,0xEB,0x08,0xE8,0xCF,0x00,0xB9,0x01,0x00,0x32,0xD1, // 40h
    0xBB,0x00,0x7C,0xB8,0x01,0x02,0xCD,0x13,0x72,0x1E,0x81,0xBF,0xFE,0x01,0x55,0xAA, 
    0x75,0x16,0xEA,0x00,0x7C,0x00,0x00,0x80,0xFA,0x81,0x74,0x02,0xB2,0x80,0x8B,0xEA, 
    0x42,0x80,0xF2,0xB3,0x88,0x16,0x41,0x07,0xBF,0xBE,0x07,0xB9,0x04,0x00,0xC6,0x06, 
    0x34,0x07,0x31,0x32,0xF6,0x88,0x2D,0x8A,0x45,0x04,0x3C,0x00,0x74,0x23,0x3C,0x05, // 80h
    0x74,0x1F,0xFE,0xC6,0xBE,0x31,0x07,0xE8,0x71,0x00,0xBE,0x4F,0x07,0x46,0x46,0x8B, 
    0x1C,0x0A,0xFF,0x74,0x05,0x32,0x7D,0x04,0x75,0xF3,0x8D,0xB7,0x7B,0x07,0xE8,0x5A, 
    0x00,0x83,0xC7,0x10,0xFE,0x06,0x34,0x07,0xE2,0xCB,0x80,0x3E,0x75,0x04,0x02,0x74, 
    0x0B,0xBE,0x42,0x07,0x0A,0xF6,0x75,0x0A,0xCD,0x18,0xEB,0xAC,0xBE,0x31,0x07,0xE8, 
    0x39,0x00,0xE8,0x36,0x00,0x32,0xE4,0xCD,0x1A,0x8B,0xDA,0x83,0xC3,0x60,0xB4,0x01, 
    0xCD,0x16,0xB4,0x00,0x75,0x0B,0xCD,0x1A,0x3B,0xD3,0x72,0xF2,0xA0,0x4F,0x07,0xEB, 
    0x0A,0xCD,0x16,0x8A,0xC4,0x3C,0x1C,0x74,0xF3,0x04,0xF6,0x3C,0x31,0x72,0xD6,0x3C, 
    0x35,0x77,0xD2,0x50,0xBE,0x2F,0x07,0xBB,0x1B,0x06,0x53,0xFC,0xAC,0x50,0x24,0x7F, //100h
    0xB4,0x0E,0xCD,0x10,0x58,0xA8,0x80,0x74,0xF2,0xC3,0x56,0xB8,0x01,0x03,0xBB,0x00, //110h
    0x06,0xB9,0x01,0x00,0x32,0xF6,0xCD,0x13,0x5E,0xC6,0x06,0x4F,0x07,0x3F,0xC3,0x0D, //120h
    0x8A,0x0D,0x0A,0x46,0x35,0x20,0x2E,0x20,0x2E,0x20,0x2E,0xA0,0x64,0x69,0x73,0x6B, 
    0x20,0x32,0x0D,0x0A,0x0A,0x44,0x65,0x66,0x61,0x75,0x6C,0x74,0x3A,0x20,0x46,0x31, //140h
    0xA0,0x00,0x01,0x00,0x04,0x00,0x06,0x03,0x07,0x07,0x0A,0x0A,0x63,0x0E,0x64,0x0E, 
    0x65,0x14,0x80,0x19,0x81,0x19,0x82,0x19,0x83,0x1E,0x93,0x24,0xA5,0x2B,0x9F,0x2F, 
    0x75,0x33,0x52,0x33,0xDB,0x36,0x40,0x3B,0xF2,0x41,0x00,0x44,0x6F,0xF3,0x48,0x70, 
    0x66,0xF3,0x4F,0x73,0xB2,0x55,0x6E,0x69,0xF8,0x4E,0x6F,0x76,0x65,0x6C,0xEC,0x4D, //180h
    0x69,0x6E,0x69,0xF8,0x4C,0x69,0x6E,0x75,0xF8,0x41,0x6D,0x6F,0x65,0x62,0xE1,0x46, 
    0x72,0x65,0x65,0x42,0x53,0xC4,0x42,0x53,0x44,0xE9,0x50,0x63,0x69,0xF8,0x43,0x70, 
    0xED,0x56,0x65,0x6E,0x69,0xF8,0x44,0x6F,0x73,0x73,0x65,0xE3,0x3F,0xBF,0x00,0x00, //1B0h
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x55,0xAA
    };
#ifdef WIN32
#include <winioctl.h>
#endif

class IMGMAKE : public Program {
public:
#ifdef WIN32
    bool OpenDisk(HANDLE* f, OVERLAPPED* o, uint8_t* name) {
        o->hEvent = INVALID_HANDLE_VALUE;
        *f = CreateFile( (LPCSTR)name, GENERIC_READ | GENERIC_WRITE,
            0,    // exclusive access 
            NULL, // default security attributes 
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL );

        if (*f == INVALID_HANDLE_VALUE) return false;

        // init OVERLAPPED 
        o->Internal = 0;
        o->InternalHigh = 0;
        o->Offset = 0;
        o->OffsetHigh = 0;
        o->hEvent = CreateEvent(
            NULL,   // default security attributes 
            TRUE,   // manual-reset event 
            FALSE,  // not signaled 
            NULL    // no name
            );
        return true;
    }

    void CloseDisk(HANDLE f, OVERLAPPED* o) {
        if(f != INVALID_HANDLE_VALUE) CloseHandle(f);
        if(o->hEvent != INVALID_HANDLE_VALUE) CloseHandle(o->hEvent);
    }

    bool StartReadDisk(HANDLE f, OVERLAPPED* o, uint8_t* buffer, Bitu offset, Bitu size) { 
        o->Offset = (DWORD)offset;
        if (!ReadFile(f, buffer, (DWORD)size, NULL, o) &&
            (GetLastError()==ERROR_IO_PENDING)) return true;
        return false;
    }

    // 0=still waiting, 1=catastrophic faliure, 2=success, 3=sector not found, 4=crc error 
    Bitu CheckDiskReadComplete(HANDLE f, OVERLAPPED* o) {
        DWORD numret;
        BOOL b = GetOverlappedResult( f, o, &numret,false); 
        if(b) return 2;
        else {
            int error = GetLastError();
            if(error==ERROR_IO_INCOMPLETE)          return 0;
            if(error==ERROR_FLOPPY_UNKNOWN_ERROR)   return 5;
            if(error==ERROR_CRC)                    return 4;
            if(error==ERROR_SECTOR_NOT_FOUND)       return 3;
            return 1;   
        }
    }

    Bitu ReadDisk(FILE* f, uint8_t driveletter, Bitu retries_max) {
        unsigned char data[36*2*512];
        HANDLE hFloppy;
        DWORD numret;
        OVERLAPPED o;
        DISK_GEOMETRY geom;

        uint8_t drivestring[] = "\\\\.\\x:"; drivestring[4]=driveletter;
        if(!OpenDisk(&hFloppy, &o, drivestring)) return false;

        // get drive geom
        DeviceIoControl( hFloppy, IOCTL_DISK_GET_DRIVE_GEOMETRY,NULL,0,
        &geom,sizeof(DISK_GEOMETRY),&numret,NULL);

        switch(geom.MediaType) {
            case F5_1Pt2_512: case F3_1Pt44_512: case F3_2Pt88_512: case F3_720_512:
            case F5_360_512: case F5_320_512: case F5_180_512: case F5_160_512:
                break;
            default:
                CloseDisk(hFloppy,&o);
                return false;
        }
        Bitu total_sect_per_cyl = geom.SectorsPerTrack * geom.TracksPerCylinder;
        Bitu cyln_size = 512 * total_sect_per_cyl;
        
        WriteOut(MSG_Get("PROGRAM_IMGMAKE_FLREAD"),
            geom.Cylinders.LowPart,geom.TracksPerCylinder,
            geom.SectorsPerTrack,(cyln_size*geom.Cylinders.LowPart)/1024);
        WriteOut(MSG_Get("PROGRAM_IMGMAKE_FLREAD2"));
            
        for(Bitu i = 0; i < geom.Cylinders.LowPart; i++) {
            Bitu result;
            // for each cylinder
            WriteOut("%2u",i);

            if(!StartReadDisk(hFloppy, &o, &data[0], cyln_size*i, cyln_size)){
                CloseDisk(hFloppy,&o);
                return false;
            }
            do {
                result = CheckDiskReadComplete(hFloppy, &o);
                CALLBACK_Idle();
            }
            while (result==0);

            switch(result) {
            case 1:
                CloseDisk(hFloppy,&o);
                return false;
            case 2: // success
                for(Bitu m=0; m < cyln_size/512; m++) WriteOut("\xdb");
                break;
            case 3:
            case 4: // data errors
            case 5:
                for(Bitu k=0; k < total_sect_per_cyl; k++) {
                    Bitu retries=retries_max;
restart_int:
                    StartReadDisk(hFloppy, &o, &data[512*k], cyln_size*i + 512*k, 512);
                    do {
                        result = CheckDiskReadComplete(hFloppy, &o);
                        CALLBACK_Idle();
                    }
                    while (result==0);
                                        
                    switch(result) {
                    case 1: // bad error
                        CloseDisk(hFloppy,&o);
                        return false;
                    case 2: // success
                        if(retries==retries_max) WriteOut("\xdb");
                        else WriteOut("\b\b\b\xb1");
                        break;
                    case 3:
                    case 4: // read errors
                    case 5:
                        if(retries!=retries_max) WriteOut("\b\b\b");
                        retries--;
                        switch(result) {
                            case 3: WriteOut("x");
                            case 4: WriteOut("!");
                            case 5: WriteOut("?");
                        }
                        WriteOut("%2d",retries);

                        if(retries) goto restart_int;
                        const uint8_t badfood[]="IMGMAKE BAD FLOPPY SECTOR \xBA\xAD\xF0\x0D";
                        for(uint8_t z = 0; z < 512/32; z++)
                            memcpy(&data[512*k+z*32],badfood,31);
                        WriteOut("\b\b");
                        break;
                    }
                }
                break;
            }
            fwrite(data, 512, total_sect_per_cyl, f);
            WriteOut("%2x%2x\n", data[0], data[1]);
        }
        // seek to 0
        StartReadDisk(hFloppy, &o, &data[0], 0, 512);
        CloseDisk(hFloppy,&o);
        return true;
    }
#endif

    void Run(void) {
        std::string disktype;
        std::string src;
        std::string filename;
        std::string dpath;
        std::string tmp;

        unsigned int c, h, s, sectors; 
        uint64_t size = 0;

        if(control->SecureMode()) {
            WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
            return;
        }
        if(cmd->FindExist("-?")) {
            printHelp();
            return;
        }
		if (cmd->FindExist("/examples")||cmd->FindExist("-examples")) {
			WriteOut(MSG_Get("PROGRAM_IMGMAKE_EXAMPLE"));
			return;
		}

/*
        this stuff is too frustrating

        // when only a filename is passed try to create the file on the current DOS path
        // if directory+filename are passed first see if directory is a host path, if not
        // maybe it is a DOSBox path.
        
        // split filename and path
        std::string path = "";
        Bitu spos = temp_line.rfind('\\');
        if(spos==std::string::npos) {
            temp_line.rfind('/');
        }

        if(spos==std::string::npos) {
            // no path separator
            filename=temp_line;
        } else {
            path=temp_line.substr(0,spos);
            filename=temp_line.substr(spos+1,std::string::npos);
        }
        if(filename=="") 

        char tbuffer[DOS_PATHLENGTH]= { 0 };
        if(path=="") {
            if(!DOS_GetCurrentDir(DOS_GetDefaultDrive()+1,tbuffer)){
                printHelp();
                return;
            }
            dpath=(std::string)tbuffer;
        }       
        WriteOut("path %s, filename %s, dpath %s",
            path.c_str(),filename.c_str(),dpath.c_str());
        return;
*/

        bool ForceOverwrite = false;
        if (cmd->FindExist("-force",true))
            ForceOverwrite = true;
#ifdef WIN32
        // read from real floppy?
        if(cmd->FindString("-source",src,true)) {
            int retries = 10;
            cmd->FindInt("-retries",retries,true);
            if((retries < 1)||(retries > 99))  {
                printHelp();
                return;
            }
            if((src.length()!=1) || !isalpha(src.c_str()[0])) {
                // only one letter allowed
                printHelp();
                return;
            }

            /* temp_line is the given filename */
            if (!(cmd->FindCommand(1, temp_line)))
                temp_line = "IMGMAKE.IMG";

            bool setdir=false;
            char dirCur[512], dirNew[512];
            if (!dos_kernel_disabled&&getcwd(dirCur, 512)!=NULL&&(!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"local ",6)||!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"CDRom ",6))) {
                Overlay_Drive *ddp = dynamic_cast<Overlay_Drive*>(Drives[DOS_GetDefaultDrive()]);
                strcpy(dirNew, ddp!=NULL?ddp->getOverlaydir():Drives[DOS_GetDefaultDrive()]->GetBaseDir());
                strcat(dirNew, Drives[DOS_GetDefaultDrive()]->curdir);
                if (chdir(dirNew)==0) setdir=true;
            }

            FILE* f = fopen(temp_line.c_str(),"r");
            if (f){
                fclose(f);
                if (!ForceOverwrite) {
                    WriteOut(MSG_Get("PROGRAM_IMGMAKE_FILE_EXISTS"),temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
            }
            f = fopen(temp_line.c_str(),"wb+");
            if (!f) {
                WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANNOT_WRITE"),temp_line.c_str());
                if (setdir) chdir(dirCur);
                return;
            }
            if (setdir) chdir(dirCur);
            // maybe delete f if it failed?
            if(!ReadDisk(f, src.c_str()[0],retries))
                WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANT_READ_FLOPPY"));
            fclose(f);
            if (setdir) runRescan("-Q");
            return;
        }
#endif
        // disk type
        if (!(cmd->FindString("-t",disktype,true))) {
            printHelp();
            return;
        }
		std::transform(disktype.begin(), disktype.end(), disktype.begin(), ::tolower);

        uint8_t mediadesc = 0xF8; // media descriptor byte; also used to differ fd and hd
        uint16_t root_ent = 512; // FAT root directory entries: 512 is for harddisks
        if(disktype=="fd_160") {
            c = 40; h = 1; s = 8; mediadesc = 0xFE; root_ent = 56; // root_ent?
        } else if(disktype=="fd_180") {
            c = 40; h = 1; s = 9; mediadesc = 0xFC; root_ent = 56; // root_ent?
        } else if(disktype=="fd_200") {
            c = 40; h = 1; s = 10; mediadesc = 0xFC; root_ent = 56; // root_ent?
        } else if(disktype=="fd_320") {
            c = 40; h = 2; s = 8; mediadesc = 0xFF; root_ent = 112; // root_ent?
        } else if(disktype=="fd_360") {
            c = 40; h = 2; s = 9; mediadesc = 0xFD; root_ent = 112;
        } else if(disktype=="fd_400") {
            c = 40; h = 2; s = 10; mediadesc = 0xFD; root_ent = 112; // root_ent?
        } else if(disktype=="fd_720") {
            c = 80; h = 2; s = 9; mediadesc = 0xF9; root_ent = 112;
        } else if(disktype=="fd_1200") {
            c = 80; h = 2; s = 15; mediadesc = 0xF9; root_ent = 224;
        } else if(disktype=="fd_1440"||disktype=="fd"||disktype=="floppy") {
            c = 80; h = 2; s = 18; mediadesc = 0xF0; root_ent = 224;
        } else if(disktype=="fd_2880") {
            c = 80; h = 2; s = 36; mediadesc = 0xF0; root_ent = 512; // root_ent?
        } else if(disktype=="hd_250") {
            c = 489; h = 16; s = 63;
        } else if(disktype=="hd_520") {
            c = 1023; h = 16; s = 63;
        } else if(disktype=="hd_1gig") {
            c = 1023; h = 32; s = 63;
        } else if(disktype=="hd_2gig") {
            c = 1023; h = 64; s = 63;
        } else if(disktype=="hd_4gig") { // fseek only supports 2gb
            c = 1023; h = 130; s = 63;
        } else if(disktype=="hd_8gig") { // fseek only supports 2gb
            c = 1023; h = 255; s = 63;
        } else if(disktype=="hd_st251") { // old 40mb drive
            c = 820; h = 6; s = 17;
        } else if(disktype=="hd_st225") { // even older 20mb drive
            c = 615; h = 4; s = 17;
        } else if(disktype=="hd") {
            // get size from parameter
            std::string isize;
            if (!(cmd->FindString("-size",isize,true))) {
                // maybe -chs?
                if (!(cmd->FindString("-chs",isize,true))){
                        // user forgot -size and -chs
                        printHelp();
                        return;
                    }
                else {
                    // got chs data: -chs 1023,16,63
                    if(sscanf(isize.c_str(),"%u,%u,%u",&c,&h,&s) != 3) {
                        printHelp();
                        return;
                    }
                    // sanity-check chs values
                    if((h>255)||(c>1023)||(s>63)) {
                        printHelp();
                        return;
                    }
                    size = (unsigned long long)c * (unsigned long long)h * (unsigned long long)s * 512ULL;
                    if((size < 3u*1024u*1024u) || (size > 0x1FFFFFFFFLL)/*8GB*/) {
                        // user picked geometry resulting in wrong size
                        printHelp();
                        return;
                    }
                }
            } else {
                // got -size
                std::istringstream stream(isize);
                stream >> size;
                size *= 1024*1024LL; // size in megabytes
                // low limit: 3 megs, high limit: 2 terabytes
                // Int13 limit would be 8 gigs
                if((size < 3*1024*1024LL) || (size > 0x1FFFFFFFFFFLL)/*2TB*/) {
                    // wrong size
                    printHelp();
                    return;
                }
                sectors = (unsigned int)(size / 512);

                // Now that we finally have the proper size, figure out good CHS values
                if (size > 0xFFFFFFFFLL/*4GB*/) {
                    /* beyond that point it's easier to just map like LBA and be done with it */
                    h=255;
                    s=63;
                    c=sectors/(h*s);
                }
                else {
                    h=2;
                    while(h*1023*63 < sectors) h <<= 1;
                    if(h>255) h=255;
                    s=8;
                    while(h*s*1023 < sectors) s *= 2;
                    if(s>63) s=63;
                    c=sectors/(h*s);
                    if(c>1023) c=1023;
                }
            }
        } else {
            // user passed a wrong -t argument
            printHelp();
            return;
        }

        std::string t2 = "";
        if(cmd->FindExist("-bat",true)) {
            t2 = "-bat";
        }

        size = (unsigned long long)c * (unsigned long long)h * (unsigned long long)s * 512ULL;
        Bits bootsect_pos = 0; // offset of the boot sector in clusters
        if(cmd->FindExist("-nofs",true)) {
            bootsect_pos = -1;
        }

        /* beyond this point clamp c */
        if (c > 1023) c = 1023;

        /* temp_line is the given filename */
        if (!(cmd->FindCommand(1, temp_line)))
            temp_line = "IMGMAKE.IMG";

        bool setdir=false;
        char dirCur[512], dirNew[512];
        if (!dos_kernel_disabled&&getcwd(dirCur, 512)!=NULL&&!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"local directory", 15)) {
            Overlay_Drive *ddp = dynamic_cast<Overlay_Drive*>(Drives[DOS_GetDefaultDrive()]);
            strcpy(dirNew, ddp!=NULL?ddp->getOverlaydir():Drives[DOS_GetDefaultDrive()]->GetBaseDir());
            strcat(dirNew, Drives[DOS_GetDefaultDrive()]->curdir);
            if (chdir(dirNew)==0) setdir=true;
        }

#if !defined(WIN32) && !defined(OS2)
        if (setdir&&temp_line[0]!='/'&&!(temp_line[0]=='~'&&temp_line[1]=='/'))
            std::replace(temp_line.begin(), temp_line.end(), '\\', '/');
        pref_struct_stat test;
        std::string homedir(temp_line);
        Cross::ResolveHomedir(homedir);
        std::string homepath=homedir;
        if (!pref_stat(dirname((char *)homepath.c_str()), &test) && test.st_mode & S_IFDIR)
            temp_line = homedir;
#endif
        FILE* f = fopen(temp_line.c_str(),"r");
        if (f){
            fclose(f);
            if (!ForceOverwrite) {
                if (!dos_kernel_disabled) WriteOut(MSG_Get("PROGRAM_IMGMAKE_FILE_EXISTS"),temp_line.c_str());
                if (setdir) chdir(dirCur);
                return;
            }
        }

        if (!dos_kernel_disabled) WriteOut(MSG_Get("PROGRAM_IMGMAKE_PRINT_CHS"),temp_line.c_str(),c,h,s);
        LOG_MSG(MSG_Get("PROGRAM_IMGMAKE_PRINT_CHS"),temp_line.c_str(),c,h,s);

        // do it again for fixed chs values
        sectors = (unsigned int)(size / 512);

        // create the image file
        f = fopen64(temp_line.c_str(),"wb+");
        if (!f) {
            if (!dos_kernel_disabled) WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANNOT_WRITE"),temp_line.c_str());
            if (setdir) chdir(dirCur);
            return;
        }

#if defined (_MSC_VER) && (_MSC_VER >= 1400)
        if(fseeko64(f,(__int64)(size - 1ull),SEEK_SET)) {
#else
        if(fseeko64(f,static_cast<off_t>(size - 1ull),SEEK_SET)) {
#endif
            if (!dos_kernel_disabled) WriteOut(MSG_Get("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE"),size);
            fclose(f);
            unlink(temp_line.c_str());
            if (setdir) chdir(dirCur);
            return;
        }
        uint8_t bufferbyte=0;
        if(fwrite(&bufferbyte,1,1,f)!=1) {
            if (!dos_kernel_disabled) WriteOut(MSG_Get("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE"),size);
            fclose(f);
            unlink(temp_line.c_str());
            if (setdir) chdir(dirCur);
            return;
        }

        // Format the image if not unrequested (and image size<2GB)
        if(bootsect_pos > -1) {
            unsigned int reserved_sectors = 1; /* 1 for the boot sector + BPB. FAT32 will require more */
            unsigned int sectors_per_cluster = 0;
            unsigned int vol_sectors = 0;
            unsigned int fat_copies = 2; /* number of copies of the FAT. always 2. TODO: Allow the user to specify */
            unsigned int fatlimitmin;
            unsigned int fatlimit;
            int FAT = -1;

            /* FAT filesystem, user choice */
            if (cmd->FindString("-fat",tmp,true)) {
                FAT = atoi(tmp.c_str());
                if (!(FAT == 12 || FAT == 16 || FAT == 32)) {
                    WriteOut("Invalid -fat option. Must be 12, 16, or 32\n");
                    fclose(f);
                    unlink(temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
            }

            /* FAT copies, user choice */
            if (cmd->FindString("-fatcopies",tmp,true)) {
                fat_copies = atoi(tmp.c_str());
                if (fat_copies < 1u || fat_copies > 4u) {
                    WriteOut("Invalid -fatcopies option\n");
                    fclose(f);
                    unlink(temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
            }

            /* Sectors per cluster, user choice */
            if (cmd->FindString("-spc",tmp,true)) {
                sectors_per_cluster = atoi(tmp.c_str());
                if (sectors_per_cluster < 1u || sectors_per_cluster > 128u) {
                    WriteOut("Invalid -spc option, out of range\n");
                    fclose(f);
                    unlink(temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
                if ((sectors_per_cluster & (sectors_per_cluster - 1u)) != 0u) {
                    WriteOut("Invalid -spc option, must be a power of 2\n");
                    fclose(f);
                    unlink(temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
            }

            /* Root directory count, user choice.
             * Does not apply to FAT32, which makes the root directory an allocation chain like any other directory/file. */
            if (cmd->FindString("-rootdir",tmp,true)) {
                root_ent = atoi(tmp.c_str());
                if (root_ent < 1u || root_ent > 4096u) {
                    WriteOut("Invalid -rootdir option\n");
                    fclose(f);
                    unlink(temp_line.c_str());
                    if (setdir) chdir(dirCur);
                    return;
                }
            }

            /* decide partition placement */
            if (mediadesc == 0xF8) {
                bootsect_pos = (Bits)s;
                vol_sectors = sectors - (unsigned int)bootsect_pos;
            }
            else {
                bootsect_pos = 0;
                vol_sectors = sectors;
            }

            /* auto-decide FAT system */
            if (FAT < 0) {
                bool dosver_fat32 = (dos.version.major >= 8) || (dos.version.major == 7 && dos.version.minor >= 10);

                if (vol_sectors >= 4194304 && !dosver_fat32) /* 2GB or larger */
                    FAT = 32;
                else if (vol_sectors >= 1048576 && dosver_fat32) /* 512MB or larger */
                    FAT = 32;
                else if (vol_sectors >= 24576) /* 12MB or larger */
                    FAT = 16;
                else
                    FAT = 12;
            }

            /* highest cluster number + 1 */
            switch (FAT) {
                case 32:
                    fatlimit = 0x0FFFFFF6;
                    fatlimitmin = 0xFFF6;
                    break;
                case 16:
                    fatlimit = 0xFFF6;
                    fatlimitmin = 0xFF6;
                    break;
                case 12:
                    fatlimit = 0xFF6;
                    fatlimitmin = 0;
                    break;
                default:
                    abort();
                    break;
            }

            /* FAT32 increases reserved area to at least 7. Microsoft likes to use 32 */
            if (FAT >= 32)
                reserved_sectors = 32;

            uint8_t sbuf[512];
            if(mediadesc == 0xF8) {
                // is a harddisk: write MBR
                memcpy(sbuf,freedos_mbr,512);
                // active partition
                sbuf[0x1be]=0x80;
                // start head - head 0 has the partition table, head 1 first partition
                sbuf[0x1bf]=1;
                // start sector with bits 8-9 of start cylinder in bits 6-7
                sbuf[0x1c0]=1;
                // start cylinder bits 0-7
                sbuf[0x1c1]=0;
                // OS indicator
                if (FAT < 32 && (bootsect_pos+vol_sectors) < 65536) { /* 32MB or smaller */
                    if (FAT >= 16)
                        sbuf[0x1c2]=0x04; /* FAT16 within the first 32MB */
                    else
                        sbuf[0x1c2]=0x01; /* FAT12 within the first 32MB */
                }
                else if ((bootsect_pos+vol_sectors) < 8388608) { /* 4GB or smaller */
                    if (FAT >= 32)
                        sbuf[0x1c2]=0x0B; /* FAT32 C/H/S */
                    else
                        sbuf[0x1c2]=0x06; /* FAT12/FAT16 C/H/S */
                }
                else {
                    if (FAT >= 32)
                        sbuf[0x1c2]=0x0C; /* FAT32 LBA */
                    else
                        sbuf[0x1c2]=0x0E; /* FAT12/FAT16 LBA */
                }
                // end head (0-based)
                sbuf[0x1c3]= h-1;
                // end sector with bits 8-9 of end cylinder (0-based) in bits 6-7
                sbuf[0x1c4]=s|(((c-1)&0x300)>>2);
                // end cylinder (0-based) bits 0-7
                sbuf[0x1c5]=(c-1)&0xFF;
                // sectors preceding partition1 (one head)
                host_writed(&sbuf[0x1c6],(uint32_t)bootsect_pos);
                // length of partition1, align to chs value
                host_writed(&sbuf[0x1ca],vol_sectors);

                // write partition table
                fseeko64(f,0,SEEK_SET);
                fwrite(&sbuf,512,1,f);
            }

            // set boot sector values
            memset(sbuf,0,512);
            // TODO boot code jump
            if (FAT >= 32) {
                sbuf[0]=0xEB; sbuf[1]=0x58; sbuf[2]=0x90; // Windows 98 values
            }
            else {
                sbuf[0]=0xEB; sbuf[1]=0x3c; sbuf[2]=0x90;
            }
            // OEM
            if (FAT >= 32) {
                sprintf((char*)&sbuf[0x03],"MSWIN4.1");
            } else {
                sprintf((char*)&sbuf[0x03],"MSDOS5.0");
            }
            // bytes per sector: always 512
            host_writew(&sbuf[0x0b],512);
            // sectors per cluster: 1,2,4,8,16,...
            // NOTES: SCANDISK.EXE will hang if you ask it to check a FAT12 filesystem with 128 sectors/cluster.
            if (sectors_per_cluster == 0) {
                sectors_per_cluster = 1;
                /* one sector per cluster on anything larger than 200KB is a bit wasteful (large FAT tables).
                 * Improve capacity by starting from a larger value.*/
                if (vol_sectors >= 400) {
                    unsigned int tmp_fatlimit;

                    /* Windows 98 likes multiples of 4KB, which is actually reasonable considering
                     * that it keeps FAT32 efficient. Also, Windows 98 SETUP will crash if sectors/cluster
                     * is too small. Ref: [https://github.com/joncampbell123/dosbox-x/issues/1553#issuecomment-651880604]
                     * and [http://www.helpwithwindows.com/windows98/fat32.html] */
                    if (FAT >= 32) {
                        if (vol_sectors >= 67108864/*32GB*/)
                            sectors_per_cluster = 64; /* 32KB (64*512) */
                        else if (vol_sectors >= 33554432/*16GB*/)
                            sectors_per_cluster = 32; /* 16KB (32*512) */
                        else if (vol_sectors >= 16777216/*8GB*/)
                            sectors_per_cluster = 16; /* 8KB (16*512) */
                        else
                            sectors_per_cluster = 8; /* 4KB (8*512) */
                    }
                    else {
                        /* 1 sector per cluster is very inefficent */
                        if (vol_sectors >= 6144000/*3000MB*/)
                            sectors_per_cluster = 8;
                        else if (vol_sectors >= 1048576/*512MB*/)
                            sectors_per_cluster = 4;
                        else if (vol_sectors >= 131072/*64MB*/)
                            sectors_per_cluster = 2;
                    }

                    /* no more than 5% of the disk */
                    switch (FAT) {
                        case 12:    tmp_fatlimit = ((((vol_sectors / 20u) * (512u / fat_copies)) / 3u) * 2u) + 2u; break;
                        case 16:    tmp_fatlimit =  (((vol_sectors / 20u) * (512u / fat_copies)) / 2u)       + 2u; break;
                        case 32:    tmp_fatlimit =  (((vol_sectors / 20u) * (512u / fat_copies)) / 4u)       + 2u; break;
                        default:    abort(); break;
                    }

                    while ((vol_sectors/sectors_per_cluster) >= (tmp_fatlimit - 2u) && sectors_per_cluster < 0x80u) sectors_per_cluster <<= 1;
                }
            }
            while ((vol_sectors/sectors_per_cluster) >= (fatlimit - 2u) && sectors_per_cluster < 0x80u) sectors_per_cluster <<= 1;
            sbuf[0x0d]=(uint8_t)sectors_per_cluster;
            // TODO small floppys have 2 sectors per cluster?
            // reserverd sectors
            host_writew(&sbuf[0x0e],reserved_sectors);
            // Number of FATs
            sbuf[0x10] = fat_copies;
            // Root entries if not FAT32
            if (FAT < 32) host_writew(&sbuf[0x11],root_ent);
            // sectors (under 32MB) if not FAT32 and less than 65536
            if (FAT < 32 && vol_sectors < 65536ul) host_writew(&sbuf[0x13],vol_sectors);
            // sectors (32MB or larger or FAT32)
            if (FAT >= 32 || vol_sectors >= 65536ul) host_writed(&sbuf[0x20],vol_sectors);
            // media descriptor
            sbuf[0x15]=mediadesc;
            // sectors per FAT
            // needed entries: (sectors per cluster)
            Bitu sect_per_fat=0;
            Bitu clusters = vol_sectors / sectors_per_cluster; // initial estimate

            if (FAT >= 32)          sect_per_fat = ((clusters*4u)+511u)/512u;
            else if (FAT >= 16)     sect_per_fat = ((clusters*2u)+511u)/512u;
            else                    sect_per_fat = ((((clusters+1u)/2u)*3u)+511u)/512u;

            if (FAT < 32 && sect_per_fat >= 65536u) {
                WriteOut("Error: Generated filesystem has more than 64KB sectors per FAT and is not FAT32\n");
                fclose(f);
                unlink(temp_line.c_str());
                if (setdir) chdir(dirCur);
                return;
            }

            Bitu data_area = vol_sectors - reserved_sectors - (sect_per_fat * fat_copies);
            if (FAT < 32) data_area -= ((root_ent * 32u) + 511u) / 512u;
            clusters = data_area / sectors_per_cluster;
            if (FAT < 32) host_writew(&sbuf[0x16],(uint16_t)sect_per_fat);

            /* Too many or to few clusters can foul up FAT12/FAT16/FAT32 detection and cause corruption! */
            if ((clusters+2u) < fatlimitmin) {
                WriteOut("Error: Generated filesystem has too few clusters given the parameters\n");
                fclose(f);
                unlink(temp_line.c_str());
                if (setdir) chdir(dirCur);
                return;
            }
            if ((clusters+2u) > fatlimit) {
                clusters = fatlimit-2u;
                WriteOut("Warning: Cluster count is too high given the volume size. Reporting a\n");
                WriteOut("         smaller sector count.\n");
                /* Well, if the user wants an oversized partition, hack the total sectors fields to make it work */
                unsigned int adj_vol_sectors =
                    (unsigned int)(reserved_sectors + (sect_per_fat * fat_copies) +
                    (((root_ent * 32u) + 511u) / 512u) + (clusters * sectors_per_cluster));

                // sectors (under 32MB) if not FAT32 and less than 65536
                if (adj_vol_sectors < 65536ul) host_writew(&sbuf[0x13],adj_vol_sectors);
                // sectors (32MB or larger or FAT32)
                if (adj_vol_sectors >= 65536ul) host_writed(&sbuf[0x20],adj_vol_sectors);
            }

            // sectors per track
            host_writew(&sbuf[0x18],s);
            // heads
            host_writew(&sbuf[0x1a],h);
            // hidden sectors
            host_writed(&sbuf[0x1c],(uint32_t)bootsect_pos);
            /* after 0x24, FAT12/FAT16 and FAT32 diverge in structure */
            if (FAT >= 32) {
                host_writed(&sbuf[0x24],(uint32_t)sect_per_fat);
                sbuf[0x28] = 0x00; // FAT is mirrored at runtime because that is what DOSBox-X's FAT driver does
                host_writew(&sbuf[0x2A],0x0000); // FAT32 version 0.0
                host_writed(&sbuf[0x2C],2); // root directory starting cluster
                host_writew(&sbuf[0x30],1); // sector number in reserved area of FSINFO structure
                host_writew(&sbuf[0x32],6); // sector number in reserved area of backup boot sector
                // BIOS drive
                if(mediadesc == 0xF8) sbuf[0x40]=0x80;
                else sbuf[0x40]=0x00;
                // ext. boot signature
                sbuf[0x42]=0x29;
                // volume serial number
                // let's use the BIOS time (cheap, huh?)
                host_writed(&sbuf[0x43],mem_readd(BIOS_TIMER));
                // Volume label
                sprintf((char*)&sbuf[0x47],"NO NAME    ");
                // file system type
                sprintf((char*)&sbuf[0x52],"FAT32   ");
            }
            else { /* FAT12/FAT16 */
                // BIOS drive
                if(mediadesc == 0xF8) sbuf[0x24]=0x80;
                else sbuf[0x24]=0x00;
                // ext. boot signature
                sbuf[0x26]=0x29;
                // volume serial number
                // let's use the BIOS time (cheap, huh?)
                host_writed(&sbuf[0x27],mem_readd(BIOS_TIMER));
                // Volume label
                sprintf((char*)&sbuf[0x2b],"NO NAME    ");
                // file system type
                if (FAT >= 16)  sprintf((char*)&sbuf[0x36],"FAT16   ");
                else            sprintf((char*)&sbuf[0x36],"FAT12   ");
            }
            // boot sector signature
            host_writew(&sbuf[0x1fe],0xAA55);

            // write the boot sector
            fseeko64(f,bootsect_pos*512,SEEK_SET);
            fwrite(&sbuf,512,1,f);

            // FAT32: Write backup copy too.
            //        The BPB we wrote says sector 6 from start of volume
            if (FAT >= 32) {
                fseeko64(f,(bootsect_pos+6u)*512,SEEK_SET);
                fwrite(&sbuf,512,1,f);
            }

            // FAT32: Write FSInfo sector too at sector 1 from start of volume.
            //        Windows 98 behavior shows that the FSInfo is duplicated
            //        along with the boot sector.
            if (FAT >= 32) {
                memset(sbuf,0,512);
                host_writed(&sbuf[0x000],0x41615252); /* "RRaA" */
                host_writed(&sbuf[0x1e4],0x61417272); /* "rrAa" */
                host_writed(&sbuf[0x1e8],(uint32_t)(clusters-1)); /* Last known free cluster count */
                host_writed(&sbuf[0x1ec],3);          /* Next free cluster. We used 2 for the root dir, so 3 is next */
                host_writed(&sbuf[0x1fc],0xAA550000); /* signature */
                fseeko64(f,(bootsect_pos+1u)*512,SEEK_SET);
                fwrite(&sbuf,512,1,f);
                fseeko64(f,(bootsect_pos+6u+1u)*512,SEEK_SET);
                fwrite(&sbuf,512,1,f);
            }

            // write FATs
            memset(sbuf,0,512);
            if (FAT >= 32) {
                host_writed(&sbuf[0],0x0FFFFF00 | mediadesc);
                host_writed(&sbuf[4],0x0FFFFFFF);

                /* The code above marks cluster 2 as the start of the root directory. */
                host_writed(&sbuf[8],0x0FFFFFFF);
            }
            else if (FAT >= 16)
                host_writed(&sbuf[0],0xFFFFFF00 | mediadesc);
            else
                host_writed(&sbuf[0],0xFFFF00 | mediadesc);

            for (unsigned int fat=0;fat < fat_copies;fat++) {
                fseeko64(f,(off_t)(((unsigned long long)bootsect_pos+reserved_sectors+(unsigned long long)sect_per_fat*(unsigned long long)fat)*512ull),SEEK_SET);
                fwrite(&sbuf,512,1,f);
            }

            // warning
            if ((sectors_per_cluster*512ul) >= 65536ul)
                WriteOut("WARNING: Cluster sizes >= 64KB are not compatible with MS-DOS and SCANDISK\n");
        }
        // write VHD footer if requested, largely copied from RAW2VHD program, no license was included
        if((mediadesc == 0xF8) && (temp_line.find(".vhd")) != std::string::npos) {
            int i;
            uint8_t footer[512];
            // basic information
            memcpy(footer,"conectix" "\0\0\0\2\0\1\0\0" "\xff\xff\xff\xff\xff\xff\xff\xff" "????rawv" "\0\1\0\0Wi2k",40);
            memset(footer+40,0,512-40);
            // time
            struct tm tm20000101 = { /*sec*/0,/*min*/0,/*hours*/0, /*day of month*/1,/*month*/0,/*year*/100, /*wday*/0,/*yday*/0,/*isdst*/0 };
            time_t basetime = mktime(&tm20000101);
            time_t vhdtime = time(NULL) - basetime;
#if defined (_MSC_VER)
            *(uint32_t*)(footer+0x18) = SDL_SwapBE32((__time32_t)vhdtime);
#else
            *(uint32_t*)(footer+0x18) = uint32_t(SDL_SwapBE32((Uint32)vhdtime));
#endif
            // size and geometry
            *(uint64_t*)(footer+0x30) = *(uint64_t*)(footer+0x28) = SDL_SwapBE64(size);

            *(uint16_t*)(footer+0x38) = SDL_SwapBE16(c);
            *(uint8_t*)( footer+0x3A) = h;
            *(uint8_t*)( footer+0x3B) = s;
            *(uint32_t*)(footer+0x3C) = SDL_SwapBE32(2);

            // generate UUID
            for (i=0; i<16; ++i) {
                *(footer+0x44+i) = (uint8_t)(rand()>>4);
            }

            // calculate checksum
            uint32_t sum;
            for (i=0,sum=0; i<512; ++i) {
                sum += footer[i];
            }

            *(uint32_t*)(footer+0x40) = SDL_SwapBE32(~sum);

            // write footer
            fseeko64(f, 0L, SEEK_END);
            fwrite(&footer,512,1,f);
        }
        fclose(f);

        // create the batch file
        if(t2 == "-bat") {
            if(temp_line.length() > 3) {
                t2 = temp_line.substr(0,temp_line.length()-4);
                t2 = t2.append(".bat");
            } else {
                t2 = temp_line.append(".bat");
            }
            WriteOut("%s\n",t2.c_str());
            f = fopen(t2.c_str(),"wb+");
            if (!f) {
                WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANNOT_WRITE"),t2.c_str());
                if (setdir) {
                    chdir(dirCur);
                    runRescan("-Q");
                }
                return;
            }
            fprintf(f,"imgmount c %s -size 512,%u,%u,%u\r\n",temp_line.c_str(),s,h,c);
            fclose(f);
        }
        if (setdir) {
            chdir(dirCur);
            runRescan("-Q");
        }
        return;
    }
    void printHelp() { // maybe hint parameter?
        WriteOut(MSG_Get("PROGRAM_IMGMAKE_SYNTAX"));
    }
};

static void IMGMAKE_ProgramStart(Program * * make) {
    *make=new IMGMAKE;
}

void runImgmake(const char *str) {
	IMGMAKE imgmake;
	imgmake.cmd=new CommandLine("IMGMAKE", str);
	imgmake.Run();
}

// LOADFIX

class LOADFIX : public Program {
public:
    void Run(void);
};

bool XMS_Active(void);
Bitu XMS_AllocateMemory(Bitu size, uint16_t& handle);
Bitu XMS_FreeMemory(Bitu handle);
uint8_t EMM_AllocateMemory(uint16_t pages,uint16_t & dhandle,bool can_allocate_zpages);
uint8_t EMM_ReleaseMemory(uint16_t handle);
bool EMS_Active(void);

/* HIMEM.SYS does not store who owns what block, so for -D or -F to work,
 * we need to keep track of handles ourself */
std::vector<uint16_t>       LOADFIX_xms_handles;
std::vector<uint16_t>       LOADFIX_ems_handles;

void LOADFIX_OnDOSShutdown(void) {
    LOADFIX_xms_handles.clear();
    LOADFIX_ems_handles.clear();
}

void LOADFIX::Run(void) 
{
    uint16_t commandNr  = 1;
    Bitu kb             = 64;
    bool xms            = false;
    bool ems            = false;
    bool opta           = false;

    if (cmd->FindExist("-xms",true) || cmd->FindExist("/xms",true)) {
        xms = true;
        kb = 1024;
    }

    if (cmd->FindExist("-ems",true) || cmd->FindExist("/ems",true)) {
        ems = true;
        kb = 1024;
    }

    if (cmd->FindExist("-a",true) || cmd->FindExist("/a",true))
        opta = true;

    if (cmd->GetCount()==1 && (cmd->FindExist("-?", false) || cmd->FindExist("/?", false))) {
        WriteOut(MSG_Get("PROGRAM_LOADFIX_HELP"));
        return;
    }

    if (cmd->FindCommand(commandNr,temp_line)) {
        if (temp_line[0]=='-' || (temp_line[0]=='/')) {
            char ch = temp_line[1];
            if ((*upcase(&ch)=='D') || (*upcase(&ch)=='F')) {
                // Deallocate all
                if (ems) {
                    for (auto i=LOADFIX_ems_handles.begin();i!=LOADFIX_ems_handles.end();i++) {
                        if (EMM_ReleaseMemory(*i))
                            WriteOut("XMS handle %u: unable to free",*i);
                    }
                    LOADFIX_ems_handles.clear();
                    WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
                }
                else if (xms) {
                    for (auto i=LOADFIX_xms_handles.begin();i!=LOADFIX_xms_handles.end();i++) {
                        if (XMS_FreeMemory(*i))
                            WriteOut("XMS handle %u: unable to free",*i);
                    }
                    LOADFIX_xms_handles.clear();
                    WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
                }
                else {
                    DOS_FreeProcessMemory(0x40);
                    WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
                }
                return;
            } else {
                // Set mem amount to allocate
                kb = (Bitu)atoi(temp_line.c_str()+1);
                if (kb==0) kb=xms?1024:64;
                commandNr++;
            }
        }
    }

    // Allocate Memory
    if (ems) {
        if (EMS_Active()) {
            uint16_t handle;
            Bitu err;

            /* EMS allocates in 16kb increments */
            kb = (kb + 15u) & (~15u);

            err = EMM_AllocateMemory((uint16_t)(kb/16u)/*16KB pages*/,/*&*/handle,false);
            if (err == 0) {
                WriteOut("EMS block allocated (%uKB)\n",kb);
                LOADFIX_ems_handles.push_back(handle);
            }
            else {
                WriteOut("Unable to allocate EMS block\n");
            }
        }
        else {
            WriteOut("EMS not active\n");
        }
    }
    else if (xms) {
        if (XMS_Active()) {
            uint16_t handle;
            Bitu err;

            err = XMS_AllocateMemory(kb,/*&*/handle);
            if (err == 0) {
                WriteOut("XMS block allocated (%uKB)\n",kb);
                LOADFIX_xms_handles.push_back(handle);
            }
            else {
                WriteOut("Unable to allocate XMS block\n");
            }
        }
        else {
            WriteOut("XMS not active\n");
        }
    }
    else {
        uint16_t segment;
        uint16_t blocks = (uint16_t)(kb*1024/16);
        if (DOS_AllocateMemory(&segment,&blocks)) {
            DOS_MCB mcb((uint16_t)(segment-1));
            if (opta) {
                if (segment < 0x1000) {
                    uint16_t needed = 0x1000 - segment;
                    if (DOS_ResizeMemory(segment,&needed))
                        kb=needed*16/1024;
                }
                else {
                    DOS_FreeMemory(segment);
                    WriteOut("Lowest MCB is above 64KB, nothing allocated\n");
                    return;
                }
            }
            mcb.SetPSPSeg(0x40);            // use fake segment
            WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
            // Prepare commandline...
            if (cmd->FindCommand(commandNr++,temp_line)) {
                // get Filename
                char filename[128];
                safe_strncpy(filename,temp_line.c_str(),128);
                // Setup commandline
                char args[256 + 1];
                args[0] = 0;
                bool found = cmd->FindCommand(commandNr++, temp_line);
                while (found) {
                    if (strlen(args) + temp_line.length() + 1 > 256) break;
                    strcat(args, temp_line.c_str());
                    found = cmd->FindCommand(commandNr++, temp_line);
                    if (found) strcat(args, " ");
                }
                // Use shell to start program
                DOS_Shell shell;
                shell.Execute(filename,args);
                DOS_FreeMemory(segment);        
                WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
            }
        } else {
            WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);  
        }
    }
}

static void LOADFIX_ProgramStart(Program * * make) {
    *make=new LOADFIX;
}

// RESCAN

class RESCAN : public Program {
public:
    void Run(void);
};

void RESCAN::Run(void)
{
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		WriteOut("Clears the caches of a mounted drive.\n\nRESCAN [/A] [/Q]\nRESCAN [drive:] [/Q]\n\n  [/A]\t\tRescan all drives\n  [/Q]\t\tEnable quiet mode\n  [drive:]\tThe drive to rescan\n\nType RESCAN with no parameters to rescan the current drive.\n");
		return;
	}
    bool all = false, quiet = false;
    if (cmd->FindExist("-q",true) || cmd->FindExist("/q",true))
        quiet = true;

    uint8_t drive = DOS_GetDefaultDrive();
    if(cmd->FindCommand(1,temp_line)) {
        //-A -All /A /All 
        if(temp_line.size() >= 2 && (temp_line[0] == '-' ||temp_line[0] =='/')&& (temp_line[1] == 'a' || temp_line[1] =='A') ) all = true;
        else if(temp_line.size() == 2 && temp_line[1] == ':') {
            lowcase(temp_line);
            drive  = temp_line[0] - 'a';
        }
    }
    // Get current drive
    if (all) {
        for(Bitu i =0; i<DOS_DRIVES;i++) {
            if (Drives[i]) Drives[i]->EmptyCache();
        }
        if (!quiet) WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
    } else {
        if (drive < DOS_DRIVES && Drives[drive]) {
            Drives[drive]->EmptyCache();
            if (!quiet) WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
        } else
            if (!quiet) WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"), 'A'+drive);
    }
}

static void RESCAN_ProgramStart(Program * * make) {
    *make=new RESCAN;
}

void runRescan(const char *str) {
	RESCAN rescan;
	rescan.cmd=new CommandLine("RESCAN", str);
	rescan.Run();
}

/* TODO: This menu code sucks. Write a better one. */
class INTRO : public Program {
public:
    void DisplayMount(void) {
        /* Basic mounting has a version for each operating system.
         * This is done this way so both messages appear in the language file*/
        WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#if (WIN32)
        WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else           
        WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
        WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
    }
    void DisplayUsage(void) {
        uint8_t c;uint16_t n=1;
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_TOP"));
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_1"));
        DOS_ReadFile (STDIN,&c,&n);
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_TOP"));
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_2"));
        DOS_ReadFile (STDIN,&c,&n);
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_TOP"));
        WriteOut(MSG_Get("PROGRAM_INTRO_USAGE_3"));
        DOS_ReadFile (STDIN,&c,&n);
    }
    void DisplayIntro(void) {
        WriteOut(MSG_Get("PROGRAM_INTRO"));
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_UP"));
    }
    void DisplayMenuBefore(void) { WriteOut("\033[44m\033[K\033[33m\033[1m   \033[0m "); }
    void DisplayMenuCursorStart(void) {
        if (machine == MCH_PC98) {
            WriteOut("\033[44m\033[K\033[1m\033[33;44m  \x1C\033[0m\033[5;37;44m ");
        } else {
            WriteOut("\033[44m\033[K\033[1m\033[33;44m  \x10\033[0m\033[5;37;44m ");
        }
    }
    void DisplayMenuCursorEnd(void) { WriteOut("\033[0m\n"); }
    void DisplayMenuNone(void) { WriteOut("\033[44m\033[K\033[0m\n"); }

    bool CON_IN(uint8_t * data) {
        uint8_t c;
        uint16_t n=1;

        /* handle arrow keys vs normal input,
         * with special conditions for PC-98 and IBM PC */
        if (!DOS_ReadFile(STDIN,&c,&n) || n == 0) return false;

        if (IS_PC98_ARCH) {
            /* translate PC-98 arrow keys to IBM PC escape for the caller */
                 if (c == 0x0B)
                *data = 0x48 | 0x80;    /* IBM extended code up arrow */
            else if (c == 0x0A)
                *data = 0x50 | 0x80;    /* IBM extended code down arrow */
            else
                *data = c;
        }
        else {
            if (c == 0) {
                if (!DOS_ReadFile(STDIN,&c,&n) || n == 0) return false;
                *data = c | 0x80; /* extended code */
            }
            else {
                *data = c;
            }
        }

        return true;
    }

    void Run(void) {
		if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
			WriteOut("A full-screen introduction to DOSBox-X.\n\nINTRO [/RUN] [CDROM|MOUNT|USAGE]\n");
			return;
		}
        std::string menuname = "BASIC"; // default
        /* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
        if (!cmd->FindExist("-run", true)&&!cmd->FindExist("/run", true)&&DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
        if(cmd->FindExist("cdrom",false)) {
            WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
            return;
        }
        if(cmd->FindExist("mount",false)) {
            WriteOut("\033[2J");//Clear screen before printing
            DisplayMount();
            return;
        }

        if(cmd->FindExist("usage",false)) { DisplayUsage(); return; }
        uint8_t c;uint16_t n=1;

#define CURSOR(option) \
    if (menuname==option) DisplayMenuCursorStart(); \
    else DisplayMenuBefore(); \
    WriteOut(MSG_Get("PROGRAM_INTRO_MENU_" option "")); \
    if (menuname==option) DisplayMenuCursorEnd(); \
    else WriteOut("\n");

        /* Intro */

menufirst:
        DisplayIntro();
        CURSOR("BASIC")
        CURSOR("CDROM")
        CURSOR("USAGE")
        DisplayMenuNone(); // None
        CURSOR("INFO")
        CURSOR("QUIT")
        DisplayMenuNone(); // None

        if (menuname=="BASIC") goto basic;
        else if (menuname=="CDROM") goto cdrom;
        else if (menuname=="USAGE") goto usage;
        else if (menuname=="INFO") goto info;
        else if (menuname=="QUIT") goto quit;
        else if (menuname=="GOTO_EXIT") goto goto_exit;

goto_exit:
        WriteOut("\n"); // Give a line
        return;

basic:
        menuname="BASIC";
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_BASIC_HELP")); 
        CON_IN(&c);
        do switch (c) {
            case 0x48|0x80: menuname="QUIT"; goto menufirst; // Up
            case 0x50|0x80: menuname="CDROM"; goto menufirst; // Down
            case 0xD:   // Run
                WriteOut("\033[2J");
                WriteOut(MSG_Get("PROGRAM_INTRO"));
                WriteOut("\n");
                DisplayMount();
                DOS_ReadFile (STDIN,&c,&n);
                goto menufirst;
        } while (CON_IN(&c));

cdrom:
        menuname="CDROM";
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_CDROM_HELP")); 
        CON_IN(&c);
        do switch (c) {
            case 0x48|0x80: menuname="BASIC"; goto menufirst; // Up
            case 0x50|0x80: menuname="USAGE"; goto menufirst; // Down
            case 0xD:   // Run
                WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
                DOS_ReadFile (STDIN,&c,&n);
                goto menufirst;
        } while (CON_IN(&c));

usage:
        menuname="USAGE";
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_USAGE_HELP")); 
        CON_IN(&c);
        do switch (c) {
            case 0x48|0x80: menuname="CDROM"; goto menufirst; // Down
            case 0x50|0x80: menuname="INFO"; goto menufirst; // Down
            case 0xD:   // Run
                DisplayUsage();
                goto menufirst;
        } while (CON_IN(&c));

info:
        menuname="INFO";
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_INFO_HELP"));
        CON_IN(&c);
        do switch (c) {
            case 0x48|0x80: menuname="USAGE"; goto menufirst; // Up
            case 0x50|0x80: menuname="QUIT"; goto menufirst; // Down
            case 0xD:   // Run
                WriteOut("\033[2J");
                WriteOut(MSG_Get("PROGRAM_INTRO"));
                WriteOut("\n");
                WriteOut(MSG_Get("PROGRAM_INTRO_INFO"));
                DOS_ReadFile (STDIN,&c,&n);
                goto menufirst;
        } while (CON_IN(&c));

quit:
        menuname="QUIT";
        WriteOut(MSG_Get("PROGRAM_INTRO_MENU_QUIT_HELP")); 
        CON_IN(&c);
        do switch (c) {
            case 0x48|0x80: menuname="INFO"; goto menufirst; // Up
            case 0x50|0x80: menuname="BASIC"; goto menufirst; // Down
            case 0xD:   // Run
                menuname="GOTO_EXIT";
                goto menufirst;
        } while (CON_IN(&c));
    }   
};

bool ElTorito_ChecksumRecord(unsigned char *entry/*32 bytes*/) {
    unsigned int chk=0,i;

    for (i=0;i < 16;i++) {
        unsigned int word = ((unsigned int)entry[0]) + ((unsigned int)entry[1] << 8);
        chk += word;
        entry += 2;
    }
    chk &= 0xFFFF;
    return (chk == 0);
}

static void INTRO_ProgramStart(Program * * make) {
    *make=new INTRO;
}

bool ElTorito_ScanForBootRecord(CDROM_Interface *drv,unsigned long &boot_record,unsigned long &el_torito_base) {
    unsigned char buffer[2048];
    unsigned int sec;

    for (sec=16;sec < 32;sec++) {
        if (!drv->ReadSectorsHost(buffer,false,sec,1))
            break;

        /* stop at terminating volume record */
        if (buffer[0] == 0xFF) break;

        /* match boot record and whether it conforms to El Torito */
        if (buffer[0] == 0x00 && memcmp(buffer+1,"CD001",5) == 0 && buffer[6] == 0x01 &&
            memcmp(buffer+7,"EL TORITO SPECIFICATION\0\0\0\0\0\0\0\0\0",32) == 0) {
            boot_record = sec;
            el_torito_base = (unsigned long)buffer[71] +
                    ((unsigned long)buffer[72] << 8UL) +
                    ((unsigned long)buffer[73] << 16UL) +
                    ((unsigned long)buffer[74] << 24UL);

            return true;
        }
    }

    return false;
}

imageDiskMemory* CreateRamDrive(Bitu sizes[], const int reserved_cylinders, const bool forceFloppy, Program* obj) {
    imageDiskMemory* dsk = NULL;
    //if chs not specified
    if (sizes[1] == 0) {
        uint32_t imgSizeK = (uint32_t)sizes[0];
        //default to 1.44mb floppy
        if (forceFloppy && imgSizeK == 0) imgSizeK = 1440;
        //search for floppy geometry that matches specified size in KB
        int index = 0;
        while (DiskGeometryList[index].cylcount != 0) {
            if (DiskGeometryList[index].ksize == imgSizeK) {
                //create floppy
                dsk = new imageDiskMemory(DiskGeometryList[index]);
                break;
            }
            index++;
        }
        if (dsk == NULL) {
            //create hard drive
            if (forceFloppy) {
                if (obj!=NULL) obj->WriteOut("Floppy size not recognized\n");
                return NULL;
            }

            // The fatDrive class is hard-coded to assume that disks 2880KB or smaller are floppies,
            //   whether or not they are attached to a floppy controller.  So, let's enforce a minimum
            //   size of 4096kb for hard drives.  Use the other constructor for floppy drives.
            // Note that a size of 0 means to auto-select a size
            if (imgSizeK < 4096) imgSizeK = 4096;

            dsk = new imageDiskMemory(imgSizeK);
        }
    }
    else {
        //search for floppy geometry that matches specified geometry
        int index = 0;
        while (DiskGeometryList[index].cylcount != 0) {
            if (DiskGeometryList[index].cylcount == sizes[3] &&
                DiskGeometryList[index].headscyl == sizes[2] &&
                DiskGeometryList[index].secttrack == sizes[1] &&
                DiskGeometryList[index].bytespersect == sizes[0]) {
                //create floppy
                dsk = new imageDiskMemory(DiskGeometryList[index]);
                break;
            }
            index++;
        }
        if (dsk == NULL) {
            //create hard drive
            if (forceFloppy) {
                if (obj!=NULL) obj->WriteOut("Floppy size not recognized\n");
                return NULL;
            }
            dsk = new imageDiskMemory((uint16_t)sizes[3], (uint16_t)sizes[2], (uint16_t)sizes[1], (uint16_t)sizes[0]);
        }
    }
    if (!dsk->active) {
        if (obj!=NULL) obj->WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
        delete dsk;
        return NULL;
    }
    dsk->Set_Reserved_Cylinders((Bitu)reserved_cylinders);
    return dsk;
}

bool AttachToBiosByIndex(imageDisk* image, const unsigned char bios_drive_index) {
    if (bios_drive_index >= MAX_DISK_IMAGES) return false;
    if (imageDiskList[bios_drive_index] != NULL) {
        /* Notify IDE ATA emulation if a drive is already there */
        if (bios_drive_index >= 2) IDE_Hard_Disk_Detach(bios_drive_index);
        imageDiskList[bios_drive_index]->Release();
    }
    imageDiskList[bios_drive_index] = image;
    imageDiskChange[bios_drive_index] = true;
    image->Addref();

    // let FDC know if we mounted a floppy
    if (bios_drive_index <= 1) {
        FDC_AssignINT13Disk(bios_drive_index);
        incrementFDD();
    }

    return true;
}

bool AttachToBiosAndIdeByIndex(imageDisk* image, const unsigned char bios_drive_index, const unsigned char ide_index, const bool ide_slave) {
    if (!AttachToBiosByIndex(image, bios_drive_index)) return false;
    //if hard drive image, and if ide controller is specified
    if (bios_drive_index >= 2 && bios_drive_index < MAX_DISK_IMAGES) {
        IDE_Hard_Disk_Attach((signed char)ide_index, ide_slave, bios_drive_index);
        updateDPT();
    }
    return true;
}

bool AttachToBiosByLetter(imageDisk* image, const char drive) {
    if (image->hardDrive) {
        //for hard drives, mount hard drives at first available index
        for (int index = 2; index < MAX_DISK_IMAGES; index++) {
            if (imageDiskList[index] == NULL) {
                return AttachToBiosByIndex(image, index);
            }
        }
    }
    else if (IS_PC98_ARCH) {
        //for pc-98 machines, mount floppies at first available index
        for (int index = 0; index < 2; index++) {
            if (imageDiskList[index] == NULL) {
                return AttachToBiosByIndex(image, index);
            }
        }
    }
    else if ((drive - 'A') < 2) {
        //for PCs, mount floppies only if A: or B: is specified, and then if so, at specified index
        return AttachToBiosByIndex(image, drive - 'A');
    }
    return false;
}

bool AttachToBiosAndIdeByLetter(imageDisk* image, const char drive, const unsigned char ide_index, const bool ide_slave) {
    if (image->hardDrive) {
        //for hard drives, mount hard drives at first available index
        for (int index = 2; index < MAX_DISK_IMAGES; index++) {
            if (imageDiskList[index] == NULL) {
                return AttachToBiosAndIdeByIndex(image, index, ide_index, ide_slave);
            }
        }
    }
    else if (IS_PC98_ARCH) {
        //for pc-98 machines, mount floppies at first available index
        for (int index = 0; index < 2; index++) {
            if (imageDiskList[index] == NULL) {
                return AttachToBiosByIndex(image, index);
            }
        }
    } else if ((drive - 'A') < 2) {
        //for PCs, mount floppies only if A: or B: is specified, and then if so, at specified index
        return AttachToBiosByIndex(image, drive - 'A');
    }
    return false;
}

char * GetIDEPosition(unsigned char bios_disk_index);
class IMGMOUNT : public Program {
public:
    std::vector<std::string> options;
    void ListImgMounts(void) {
        char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
        uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
        /* Command uses dta so set it to our internal dta */
        RealPt save_dta = dos.dta();
        dos.dta(dos.tables.tempdta);
        DOS_DTA dta(dos.dta());

        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_1"));
        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_FORMAT"),"Drive","Type","Label","Swap slot");
        int cols=IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
        if (!cols) cols=80;
        for(int p = 0;p < cols;p++) WriteOut("-");
        char swapstr[50];
        bool none=true;
        for (int d = 0;d < DOS_DRIVES;d++) {
            if (!Drives[d] || (strncmp(Drives[d]->GetInfo(), "fatDrive ", 9) && strncmp(Drives[d]->GetInfo(), "isoDrive ", 9))) continue;
            char root[7] = {(char)('A'+d),':','\\','*','.','*',0};
            bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
            if (ret) {
                dta.GetResult(name,lname,size,date,time,attr);
                DOS_FindNext(); //Mark entry as invalid
            } else name[0] = 0;

            /* Change 8.3 to 11.0 */
            const char* dot = strchr(name, '.');
            if(dot && (dot - name == 8) ) { 
                name[8] = name[9];name[9] = name[10];name[10] = name[11];name[11] = 0;
            }

            root[1] = 0; //This way, the format string can be reused.
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_FORMAT"),root, Drives[d]->GetInfo(),name,DriveManager::GetDrivePosition(d));
            none=false;
        }
        if (none) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
		WriteOut("\n");
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_2"));
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NUMBER_FORMAT"),"Drive number","Disk name","IDE position","Swap slot");
        for(int p = 0;p < cols;p++) WriteOut("-");
        none=true;
		for (int index = 0; index < MAX_DISK_IMAGES; index++)
			if (imageDiskList[index]) {
                int swaps=0;
                if (swapInDisksSpecificDrive == index) {
                    for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++)
                        if (diskSwap[si] != NULL)
                            swaps++;
                }
                if (!swaps) swaps=1;
                sprintf(swapstr, "%d / %d", swaps==1?1:swapPosition+1, swaps);
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NUMBER_FORMAT"), std::to_string(index).c_str(), dynamic_cast<imageDiskElToritoFloppy *>(imageDiskList[index])!=NULL?"El Torito floppy drive":imageDiskList[index]->diskname.c_str(), GetIDEPosition(index), swapstr);
                none=false;
            }
        if (none) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
        dos.dta(save_dta);
    }
    void Run(void) {
        //Hack To allow long commandlines
        ChangeToLongCmd();
        /* In secure mode don't allow people to change imgmount points. 
         * Neither mount nor unmount */
        if(control->SecureMode()) {
            WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
            return;
        }
        imageDisk * newImage;
        char drive;
        std::vector<std::string> paths;
        if (!cmd->GetCount()) {
            ListImgMounts();
            return;
        }
        //show help if /? or -?
        if (cmd->FindExist("/?", true) || cmd->FindExist("-?", true) || cmd->FindExist("?", true) || cmd->FindExist("-help", true)) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_HELP"));
            return;
        }
		if (cmd->FindExist("/examples")||cmd->FindExist("-examples")) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_EXAMPLE"));
			return;
		}
        /* Check for unmounting */
        std::string umount;
        if (cmd->FindString("-u",umount,false)) {
            Unmount(umount[0]);
            return;
        }

        bool roflag = false;
        if (cmd->FindExist("-ro",true))
            roflag = true;

        //initialize more variables
        unsigned long el_torito_floppy_base=~0UL;
        unsigned char el_torito_floppy_type=0xFF;
        bool ide_slave = false;
        signed char ide_index = -1;
        char el_torito_cd_drive = 0;
        std::string el_torito;
        std::string ideattach="auto";
        std::string type="hdd";

        //this code simply sets default type to "floppy" if mounting at A: or B: --- nothing else
        // get first parameter - which is probably the drive letter to mount at (A-Z or A:-Z:) - and check it if is A or B or A: or B:
        // default to floppy for drive letters A and B and numbers 0 and 1
        if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) ||
            ((temp_line.size()>1) && (temp_line[1]!=':'))) {
            // drive not valid
        } else {
            uint8_t tdr = toupper(temp_line[0]);
            if(tdr=='A'||tdr=='B'||tdr=='0'||tdr=='1') type="floppy";
        }

		if (temp_line.size() == 1 && isdigit(temp_line[0]) && temp_line[0]>='0' && temp_line[0]<MAX_DISK_IMAGES+'0' && cmd->FindExist("-u",true)) {
			Unmount(temp_line[0]);
			std::string templine;
			if (!cmd->FindCommand(2,templine)||!templine.size()) return;
		}

        //get the type
        bool rtype=cmd->FindString("-t", type, true);
		std::transform(type.begin(), type.end(), type.begin(), ::tolower);

        if (type == "cdrom") type = "iso"; //Tiny hack for people who like to type -t cdrom
        if (!(type == "floppy" || type == "hdd" || type == "iso" || type == "ram")) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED"), type.c_str());
            return;
        }

        //look for -o options
        {
            std::string s;

            while (cmd->FindString("-o", s, true))
                options.push_back(s);
        }

        //look for -el-torito parameter and remove it from the command line
        cmd->FindString("-el-torito",el_torito,true);
		if (el_torito == "") cmd->FindString("-bootcd",el_torito,true);
        if (el_torito != "") {
            //get el-torito floppy from cdrom mounted at drive letter el_torito_cd_drive
            el_torito_cd_drive = toupper(el_torito[0]);
            //validate the el_torito loading (look for boot image on the cdrom, etc), and
            //  find the el_torito_floppy_base and el_torito_floppy_type values
            if (!PrepElTorito(type, el_torito_cd_drive, el_torito_floppy_base, el_torito_floppy_type)) return;
        }

        //default fstype is fat
        std::string fstype="fat";
        bool rfstype=cmd->FindString("-fs",fstype,true);
		std::transform(fstype.begin(), fstype.end(), fstype.begin(), ::tolower);
        
        Bitu sizes[4] = { 0,0,0,0 };
        int reserved_cylinders=0;
        std::string reservecyl;

        /* DOSBox-X: to please certain 32-bit drivers like Windows 3.1 WDCTRL, or to emulate older h/w configurations,
            *           we allow the user or script to specify the number of reserved cylinders. older BIOSes were known
            *           to subtract 1 or 2 additional cylinders from the total in the fixed disk param table. the -reservecyl
            *           option allows the number we subtract from the total in INT 13H to be set */
        cmd->FindString("-reservecyl",reservecyl,true);
        if (reservecyl != "") reserved_cylinders = atoi(reservecyl.c_str());

        /* DOSBox-X: we allow "-ide" to allow controlling which IDE controller and slot to attach the hard disk/CD-ROM to */
        cmd->FindString("-ide",ideattach,true);
		std::transform(ideattach.begin(), ideattach.end(), ideattach.begin(), ::tolower);

        if (ideattach == "auto") {
            if (type != "floppy") {
                IDE_Auto(ide_index,ide_slave);
            }
                
            LOG_MSG("IDE: index %d slave=%d",ide_index,ide_slave?1:0);
        }
        else if (ideattach != "none" && isdigit(ideattach[0]) && ideattach[0] > '0') { /* takes the form [controller]<m/s> such as: 1m for primary master */
            ide_index = ideattach[0] - '1';
            if (ideattach.length() >= 2) ide_slave = (ideattach[1] == 's');
            LOG_MSG("IDE: index %d slave=%d",ide_index,ide_slave?1:0);
        }

        //if floppy, don't attach to ide controller
        //if cdrom, file system is iso
        if (type=="floppy") {
            ideattach="none";
        } else if (type=="iso") {
            //str_size=="2048,1,60000,0";   // ignored, see drive_iso.cpp (AllocationInfo)
            fstype = "iso";
        } 

        //load the size parameter
        //auto detect hard drives if not specified
        std::string str_size;
        std::string str_chs;
        cmd->FindString("-size", str_size, true);
        cmd->FindString("-chs", str_chs, true);
        if (!ReadSizeParameter(str_size, str_chs, type, sizes)) return;

        if (!rfstype&&isdigit(temp_line[0])) fstype="none";

        //for floppies, hard drives, and cdroms, require a drive letter
        //for -fs none, require a number indicating where to mount at
        if(fstype=="fat" || fstype=="iso") {
            // get the drive letter
            if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) {
                WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
                return;
            }
            int i_drive = toupper(temp_line[0]);
            if (!isalpha(i_drive) || (i_drive - 'A') >= DOS_DRIVES || (i_drive - 'A') < 0) {
                WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
                return;
            }
            drive = static_cast<char>(i_drive);
        } else if (fstype=="none") {
            cmd->FindCommand(1,temp_line);
            if ((temp_line.size() > 1) || (!isdigit(temp_line[0]))) {
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"), MAX_DISK_IMAGES-1);
                return;
            }
            drive=temp_line[0];
            if ((drive<'0') || (drive>=MAX_DISK_IMAGES+'0')) {
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"), MAX_DISK_IMAGES-1);
                return;
            }
			int index = drive - '0';
			if (imageDiskList[index]) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED_NUMBER"),index);
                return;
			}
        } else {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"),fstype.c_str());
            return;
        }

        // find all file parameters, assuming that all option parameters have been removed
        bool removed=ParseFiles(temp_line, paths, el_torito != "" || type == "ram");

        // some generic checks
        if (el_torito != "") {
            if (paths.size() != 0) {
                WriteOut("Do not specify files when mounting floppy drives from El Torito bootable CDs\n");
                return;
            }
        }
        else if (type == "ram") {
            if (paths.size() != 0) {
                WriteOut("Do not specify files when mounting RAM drives\n");
                return;
            }
        }
        else {
            if (paths.size() == 0) {
                if (strcasecmp(temp_line.c_str(), "-u")&&!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
                return; 
            }
			if (!rtype&&!rfstype&&fstype!="none"&&paths[0].length()>4) {
				char ext[5];
				strncpy(ext, paths[0].substr(paths[0].length()-4).c_str(), 4);
				ext[4]=0;
				if (!strcasecmp(ext, ".iso")||!strcasecmp(ext, ".cue")||!strcasecmp(ext, ".bin")||!strcasecmp(ext, ".chd")||!strcasecmp(ext, ".mdf")||!strcasecmp(ext, ".gog")||!strcasecmp(ext, ".ins")) {
					type="iso";
					fstype="iso";
				} else if (!strcasecmp(ext, ".ima")) {
					type="floppy";
					ideattach="none";
				}
			}
        }

        int i_drive = drive - 'A';
        bool exist = i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive];
        //====== call the proper subroutine ======
        if(fstype=="fat") {
            //mount floppy or hard drive
            if (el_torito != "") {
                if (!MountElToritoFat(drive, sizes, el_torito_cd_drive, el_torito_floppy_base, el_torito_floppy_type)) return;
            }
            else if (type == "ram") {
                if (!MountRam(sizes, drive, ide_index, ide_slave, roflag)) return;
            }
            else {
                //supports multiple files
                if (!MountFat(sizes, drive, type == "hdd", str_size, paths, ide_index, ide_slave, reserved_cylinders, roflag)) return;
            }
            if (removed && !exist && i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive]) DOS_SetDefaultDrive(i_drive);
        } else if (fstype=="iso") {
            if (el_torito != "") {
                WriteOut("El Torito bootable CD: -fs iso mounting not supported\n"); /* <- NTS: Will never implement, either */
                return;
            }
            //supports multiple files
            if (!MountIso(drive, paths, ide_index, ide_slave)) return;
            if (removed && !exist && i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive]) DOS_SetDefaultDrive(i_drive);
        } else if (fstype=="none") {
            unsigned char driveIndex = drive - '0';

            if (paths.size() > 1) {
                if (driveIndex <= 1) {
                    if (swapInDisksSpecificDrive >= 0 && swapInDisksSpecificDrive <= 1 &&
                        swapInDisksSpecificDrive != driveIndex) {
                        WriteOut("Multiple images given and another drive already uses multiple images\n");
                        return;
                    }
                }
                else {
                    WriteOut("Multiple disk images not supported for that drive\n");
                    return;
                }
            }

            if (el_torito != "") {
                newImage = new imageDiskElToritoFloppy((unsigned char)el_torito_cd_drive, el_torito_floppy_base, el_torito_floppy_type);
            }
            else if (type == "ram") {
                newImage = MountImageNoneRam(sizes, reserved_cylinders, driveIndex < 2);
            }
            else {
                newImage = MountImageNone(paths[0].c_str(), NULL, sizes, reserved_cylinders, roflag);
            }
            if (newImage == NULL) return;
            newImage->Addref();
            if (newImage->hardDrive && (driveIndex < 2)) {
                WriteOut("Cannot mount hard drive in floppy position.\n");
            }
            else if (!newImage->hardDrive && (driveIndex >= 2)) {
                WriteOut("Cannot mount floppy in hard drive position.\n");
            }
            else {
                if (AttachToBiosAndIdeByIndex(newImage, (unsigned char)driveIndex, (unsigned char)ide_index, ide_slave)) {
                    WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"), drive - '0', (!paths.empty()) ? (wpcolon&&paths[0].length()>1&&paths[0].c_str()[0]==':'?paths[0].c_str()+1:paths[0].c_str()) : (el_torito != ""?"El Torito floppy drive":(type == "ram"?"RAM drive":"-")));
                    if (swapInDisksSpecificDrive == driveIndex || swapInDisksSpecificDrive == -1) {
                        for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                            if (diskSwap[si] != NULL) {
                                diskSwap[si]->Release();
                                diskSwap[si] = NULL;
                            }
                        }
                        swapInDisksSpecificDrive = -1;
                        if (paths.size() > 1) {
                            /* slot 0 is the image we already assigned */
                            diskSwap[0] = newImage;
                            diskSwap[0]->Addref();
                            swapPosition = 0;
                            swapInDisksSpecificDrive = driveIndex;

                            for (size_t si=1;si < MAX_SWAPPABLE_DISKS && si < paths.size();si++) {
                                imageDisk *img = MountImageNone(paths[si].c_str(), NULL, sizes, reserved_cylinders, roflag);

                                if (img != NULL) {
                                    diskSwap[si] = img;
                                    diskSwap[si]->Addref();
                                }
                            }
                        }
                    }
                }
                else {
                    WriteOut("Invalid mount number\n");
                }
            }
            newImage->Release();
            return;
        }
        else {
            WriteOut("Invalid fstype\n");
            return;
        }

        return;
    }

private:
    bool ReadSizeParameter(const std::string &str_size, const std::string &str_chs, const std::string &type, Bitu sizes[]) {
        bool isCHS = false;
        const char * scan;
        if (str_chs.size() != 0) {
            if (str_size.size() != 0) {
                WriteOut("Size and chs parameter cannot both be specified\n");
                return false;
            }
            isCHS = true;
            scan = str_chs.c_str();
        }
        else if (str_size.size() != 0) {
            scan = str_size.c_str();
        }
        else {
            //nothing specified, so automatic size detection
            return true;
        }
        char number[20];
        Bitu index = 0;
        Bitu count = 0;
        int val;

        //scan through input string
        while (*scan) {
            //separate string by ','
            if (*scan == ',') {
                number[index] = 0; //add null char
                val = atoi(number);
                if (val <= 0) {
                    //out of range
                    WriteOut("Invalid size parameter\n");
                    return false;
                }
                sizes[count++] = (unsigned int)val;
                index = 0;
                if (count == 4) {
                    //too many commas
                    WriteOut("Invalid size parameter\n");
                    return false;
                }
            }
            else if (index >= 19) {
                //number too large (too many characters, anyway)
                WriteOut("Invalid size parameter\n");
                return false;
            }
            else {
                number[index++] = *scan;
            }
            scan++;
        }
        number[index] = 0;
        val = atoi(number);
        if (val <= 0) {
            //out of range
            WriteOut("Invalid size parameter\n");
            return false;
        }
        sizes[count++] = (unsigned int)val;
        if (isCHS) {
            if (count == 3) sizes[count++] = 512; //set sector size automatically
            if (count != 4) {
                WriteOut("Invalid chs parameter\n");
                return false;
            }
            Bitu temp = sizes[3]; //hold on to sector size temporarily
            sizes[3] = sizes[0]; //put cylinders in the right spot
            sizes[0] = temp; //put sector size in the right spot
            temp = sizes[2]; //hold on to sectors temporarily
            sizes[2] = sizes[1]; //put heads in the right spot
            sizes[1] = temp; //put sectors in the right spot
        }
        if (!((type == "ram" && count == 1) || count == 4)) {
            //ram drives require 1 or 4 numbers
            //other drives require 4 numbers
            WriteOut("Invalid size parameter\n");
            return false;
        }
        return true;
    }
    bool ParseFiles(std::string &commandLine, std::vector<std::string> &paths, bool nodef) {
		char drive=commandLine[0];
        bool nocont=false;
        while (!nocont&&cmd->FindCommand((unsigned int)(paths.size() + 1), commandLine)) {
			bool usedef=false;
			if (!cmd->FindCommand((unsigned int)(paths.size() + 2), commandLine) || !commandLine.size()) {
				if (!nodef && !paths.size()) {
					commandLine="IMGMAKE.IMG";
					usedef=true;
				}
				else break;
			}
#if defined (WIN32) || defined(OS2)
            // Windows: Workaround for LaunchBox
            if (commandLine.size()>4 && commandLine[0]=='\'' && toupper(commandLine[1])>='A' && toupper(commandLine[1])<='Z' && commandLine[2]==':' && (commandLine[3]=='/' || commandLine[3]=='\\') && commandLine.back()=='\'')
                commandLine = commandLine.substr(1, commandLine.size()-2);
            else if (!paths.size() && commandLine.size()>3 && commandLine[0]=='\'' && toupper(commandLine[1])>='A' && toupper(commandLine[1])<='Z' && commandLine[2]==':' && (commandLine[3]=='/' || commandLine[3]=='\\')) {
                std::string line=trim((char *)cmd->GetRawCmdline().c_str());
                std::size_t space=line.find(' ');
                if (space!=std::string::npos) {
                    line=trim((char *)line.substr(space).c_str());
                    std::size_t found=line.back()=='\''?line.find_last_of('\''):line.rfind("' ");
                    if (found!=std::string::npos&&found>2) {
                        commandLine=line.substr(1, found-1);
                        nocont=true;
                        if (line.size()>3 && !strcasecmp(line.substr(line.size()-3).c_str(), " -u")) Unmount(drive);
                    }
                }
            }
#else
            // Linux: Convert backslash to forward slash
            if (commandLine.size() > 0) {
                for (size_t i = 0; i < commandLine.size(); i++) {
                    if (commandLine[i] == '\\')
                        commandLine[i] = '/';
                }
            }
#endif

			if (!strcasecmp(commandLine.c_str(), "-u")) {
                bool exist = toupper(drive) - 'A' == DOS_GetDefaultDrive();
				Unmount(drive);
				return exist && drive - 'A' != DOS_GetDefaultDrive();
			}

			char fullname[CROSS_LEN];
			char tmp[CROSS_LEN];
			safe_strncpy(tmp, wpcolon&&commandLine.length()>1&&commandLine[0]==':'?commandLine.c_str()+1:commandLine.c_str(), CROSS_LEN);
            bool useh = false;
            pref_struct_stat test;
#if defined(WIN32)
            ht_stat_t htest;
            const host_cnv_char_t* host_name = CodePageGuestToHost(tmp);
            if (pref_stat(tmp, &test) && (host_name == NULL || ht_stat(host_name, &htest))) {
                if (pref_stat(tmp, &test) && host_name != NULL) useh = true;
#else
            pref_struct_stat htest;
            if (pref_stat(tmp, &test)) {
#endif
                //See if it works if the ~ are written out
                std::string homedir(commandLine);
                Cross::ResolveHomedir(homedir);
                if (!pref_stat(homedir.c_str(), &test)) {
                    commandLine = homedir;
                }
                else {
                    // convert dosbox-x filename to system filename
                    uint8_t dummy;
                    if (!DOS_MakeName(tmp, fullname, &dummy) || strncmp(Drives[dummy]->GetInfo(), "local directory", 15)) {
                        if (!qmount) WriteOut(MSG_Get(usedef?"PROGRAM_IMGMOUNT_DEFAULT_NOT_FOUND":"PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
                        return false;
                    }

                    localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
                    if (ldp == NULL) {
                        if (!qmount) WriteOut(MSG_Get(usedef?"PROGRAM_IMGMOUNT_DEFAULT_NOT_FOUND":"PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
                        return false;
                    }
					bool readonly=wpcolon&&commandLine.length()>1&&commandLine[0]==':';
                    ldp->GetSystemFilename(readonly?tmp+1:tmp, fullname);
					if (readonly) tmp[0]=':';
                    commandLine = tmp;

                    if (pref_stat(readonly?tmp+1:tmp, &test)) {
                        if (!qmount) WriteOut(MSG_Get(usedef?"PROGRAM_IMGMOUNT_DEFAULT_NOT_FOUND":"PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
                        return false;
                    }
                }
            }
            if (S_ISDIR(useh?htest.st_mode:test.st_mode)&&!usedef) {
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT"));
                return false;
            }
            paths.push_back(commandLine);
        }
        return false;
    }

    bool Unmount(char &letter) {
        letter = toupper(letter);
        if (isalpha(letter)) { /* if it's a drive letter, then traditional usage applies */
            int i_drive = letter - 'A';
            if (i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive]) {
                //if drive A: or B:
                if (i_drive <= 1)
                    FDC_UnassignINT13Disk(i_drive);

                //get reference to image and cdrom before they are possibly destroyed
                const fatDrive* drive = dynamic_cast<fatDrive*>(Drives[i_drive]);
                imageDisk* image = drive ? drive->loadedDisk : NULL;
                const isoDrive* cdrom = dynamic_cast<isoDrive*>(Drives[i_drive]);

                switch (DriveManager::UnmountDrive(i_drive)) {
                case 0: //success
                {
                    //detatch hard drive or floppy drive from bios and ide controller
                    if (image) DetachFromBios(image);
                    /* If the drive letter is also a CD-ROM drive attached to IDE, then let the IDE code know */
                    if (cdrom) IDE_CDROM_Detach(i_drive);
                    Drives[i_drive] = NULL;
                    DOS_EnableDriveMenu(i_drive+'A');
                    if (i_drive == DOS_GetDefaultDrive())
                        DOS_SetDrive(toupper('Z') - 'A');
                    if (!qmount) WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS"), letter);
                    if (cdrom)
                        for (int drv=0; drv<2; drv++)
                            if (Drives[drv]) {
                                fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[drv]);
                                if (fdp&&fdp->opts.mounttype==1&&letter==fdp->el.CDROM_drive) {
                                    char drive='A'+drv;
                                    Unmount(drive);
                                }
                            }
                    if (i_drive < MAX_DISK_IMAGES && imageDiskList[i_drive]) {
                        delete imageDiskList[i_drive];
                        imageDiskList[i_drive] = NULL;
                    }
                    if (swapInDisksSpecificDrive == i_drive) {
                        for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                            if (diskSwap[si] != NULL) {
                                diskSwap[si]->Release();
                                diskSwap[si] = NULL;
                            }
                        }
                        swapInDisksSpecificDrive = -1;
                    }
                    return true;
                }
                case 1:
                    if (!qmount) WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
                    return false;
                case 2:
                    if (!qmount) WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
                    return false;
                default:
                    return false;
                }
            }
            else {
                if (!qmount) WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"), letter);
                return false;
            }
        }
        else if (isdigit(letter)) { /* DOSBox-X: drives mounted by number (INT 13h) can be unmounted this way */
            int index = letter - '0';

            //detatch hard drive or floppy drive from bios and ide controller
            if (index < MAX_DISK_IMAGES && imageDiskList[index]) {
                if (index > 1) IDE_Hard_Disk_Detach(index);
                imageDiskList[index]->Release();
                imageDiskList[index] = NULL;
                imageDiskChange[index] = true;
                if (swapInDisksSpecificDrive == index) {
                    for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                        if (diskSwap[si] != NULL) {
                            diskSwap[si]->Release();
                            diskSwap[si] = NULL;
                        }
                    }
                    swapInDisksSpecificDrive = -1;
                }
				WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NUMBER_SUCCESS"), letter);
                return true;
            }
            WriteOut("Drive number %d is not mounted.\n", index);
            return false;
        }
        else {
            WriteOut("Incorrect IMGMOUNT unmount usage.\n");
            return false;
        }
    }

    bool PrepElTorito(const std::string& type, const char &el_torito_cd_drive, unsigned long &el_torito_floppy_base, unsigned char &el_torito_floppy_type) {
        el_torito_floppy_base = ~0UL;
        el_torito_floppy_type = 0xFF;

        unsigned char entries[2048], *entry, ent_num = 0;
        int header_platform = -1, header_count = 0;
        bool header_final = false;
        int header_more = -1;

        /* must be valid drive letter, C to Z */
        if (!isalpha(el_torito_cd_drive) || el_torito_cd_drive < 'C') {
            WriteOut("El Torito emulation requires a proper CD-ROM drive letter\n");
            return false;
        }

        /* drive must not exist (as a hard drive) */
        if (imageDiskList[el_torito_cd_drive - 'A'] != NULL) {
            WriteOut("El Torito CD-ROM drive specified already exists as a non-CD-ROM device\n");
            return false;
        }

        bool GetMSCDEXDrive(unsigned char drive_letter, CDROM_Interface **_cdrom);

        /* get the CD-ROM drive */
        CDROM_Interface *src_drive = NULL;
        if (!GetMSCDEXDrive(el_torito_cd_drive - 'A', &src_drive)) {
            WriteOut("El Torito CD-ROM drive specified is not actually a CD-ROM drive\n");
            return false;
        }

        /* FIXME: We only support the floppy emulation mode at this time.
        *        "Superfloppy" or hard disk emulation modes are not yet implemented */
        if (type != "floppy") {
            WriteOut("El Torito emulation must be used with -t floppy at this time\n");
            return false;
        }

        /* Okay. Step #1: Scan the volume descriptors for the Boot Record. */
        unsigned long el_torito_base = 0, boot_record_sector = 0;
        if (!ElTorito_ScanForBootRecord(src_drive, boot_record_sector, el_torito_base)) {
            WriteOut("El Torito CD-ROM boot record not found\n");
            return false;
        }

        LOG_MSG("El Torito emulation: Found ISO 9660 Boot Record in sector %lu, pointing to sector %lu\n",
            boot_record_sector, el_torito_base);

        /* Step #2: Parse the records. Each one is 32 bytes long */
        if (!src_drive->ReadSectorsHost(entries, false, el_torito_base, 1)) {
            WriteOut("El Torito entries unreadable\n");
            return false;
        }

        /* for more information about what this loop is doing, read:
        * http://download.intel.com/support/motherboards/desktop/sb/specscdrom.pdf
        */
        /* FIXME: Somebody find me an example of a CD-ROM with bootable code for both x86, PowerPC, and Macintosh.
        *        I need an example of such a CD since El Torito allows multiple "headers" */
        /* TODO: Is it possible for this record list to span multiple sectors? */
        for (ent_num = 0; ent_num < (2048 / 0x20); ent_num++) {
            entry = entries + (ent_num * 0x20);

            if (memcmp(entry, "\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0", 32) == 0)
                break;

            if (entry[0] == 0x01/*header*/) {
                if (!ElTorito_ChecksumRecord(entry)) {
                    LOG_MSG("Warning: El Torito checksum error in header(0x01) entry\n");
                    continue;
                }

                if (header_count != 0) {
                    LOG_MSG("Warning: El Torito has more than one Header/validation entry\n");
                    continue;
                }

                if (header_final) {
                    LOG_MSG("Warning: El Torito has an additional header past the final header\n");
                    continue;
                }

                header_more = -1;
                header_platform = entry[1];
                LOG_MSG("El Torito entry: first header platform=0x%02x\n", header_platform);
                header_count++;
            }
            else if (entry[0] == 0x90/*header, more follows*/ || entry[0] == 0x91/*final header*/) {
                if (header_final) {
                    LOG_MSG("Warning: El Torito has an additional header past the final header\n");
                    continue;
                }

                header_final = (entry[0] == 0x91);
                header_more = (int)(((unsigned int)entry[2]) + (((unsigned int)entry[3]) << 8u));
                header_platform = entry[1];
                LOG_MSG("El Torito entry: first header platform=0x%02x more=%u final=%u\n", header_platform, header_more, header_final);
                header_count++;
            }
            else {
                if (header_more == 0) {
                    LOG_MSG("El Torito entry: Non-header entry count expired, ignoring record 0x%02x\n", entry[0]);
                    continue;
                }
                else if (header_more > 0) {
                    header_more--;
                }

                if (entry[0] == 0x44) {
                    LOG_MSG("El Torito entry: ignoring extension record\n");
                }
                else if (entry[0] == 0x00/*non-bootable*/) {
                    LOG_MSG("El Torito entry: ignoring non-bootable record\n");
                }
                else if (entry[0] == 0x88/*bootable*/) {
                    if (header_platform == 0x00/*x86*/) {
                        unsigned char mediatype = entry[1] & 0xF;
                        unsigned short load_segment = ((unsigned int)entry[2]) + (((unsigned int)entry[3]) << 8);
                        unsigned char system_type = entry[4];
                        unsigned short sector_count = ((unsigned int)entry[6]) + (((unsigned int)entry[7]) << 8);
                        unsigned long load_rba = ((unsigned int)entry[8]) + (((unsigned int)entry[9]) << 8) +
                            (((unsigned int)entry[10]) << 16) + (((unsigned int)entry[11]) << 24);

                        LOG_MSG("El Torito entry: bootable x86 record mediatype=%u load_segment=0x%04x "
                            "system_type=0x%02x sector_count=%u load_rba=%lu\n",
                            mediatype, load_segment, system_type, sector_count, load_rba);

                        /* already chose one, ignore */
                        if (el_torito_floppy_base != ~0UL)
                            continue;

                        if (load_segment != 0 && load_segment != 0x7C0)
                            LOG_MSG("El Torito boot warning: load segments other than 0x7C0 not supported yet\n");
                        if (sector_count != 1)
                            LOG_MSG("El Torito boot warning: sector counts other than 1 are not supported yet\n");

                        if (mediatype < 1 || mediatype > 3) {
                            LOG_MSG("El Torito boot entry: media types other than floppy emulation not supported yet\n");
                            continue;
                        }

                        el_torito_floppy_base = load_rba;
                        el_torito_floppy_type = mediatype;
                    }
                    else {
                        LOG_MSG("El Torito entry: ignoring bootable non-x86 (platform_id=0x%02x) record\n", header_platform);
                    }
                }
                else {
                    LOG_MSG("El Torito entry: ignoring unknown record ID %02x\n", entry[0]);
                }
            }
        }

        if (el_torito_floppy_type == 0xFF || el_torito_floppy_base == ~0UL) {
            WriteOut("El Torito bootable floppy not found\n");
            return false;
        }

        return true;
    }

    bool MountElToritoFat(const char drive, const Bitu sizes[], const char el_torito_cd_drive, const unsigned long el_torito_floppy_base, const unsigned char el_torito_floppy_type) {
        unsigned char driveIndex = drive - 'A';

        (void)sizes;//UNUSED

        if (driveIndex > 1) {
            WriteOut("Invalid drive letter");
            return false;
        }

        if (Drives[driveIndex]) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
            return false;
        }

        imageDisk * newImage = new imageDiskElToritoFloppy((unsigned char)el_torito_cd_drive, el_torito_floppy_base, el_torito_floppy_type);
        newImage->Addref();

        DOS_Drive* newDrive = new fatDrive(newImage, options);
        newImage->Release(); //fatDrive calls Addref, and this will release newImage if fatDrive doesn't use it
        if (!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
            return false;
        }

        AddToDriveManager(drive, newDrive, 0xF0);
        AttachToBiosByLetter(newImage, drive);
        DOS_EnableDriveMenu(drive);

        WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_ELTORITO"), drive);

        return true;
    }

    bool MountFat(Bitu sizes[], const char drive, const bool isHardDrive, const std::string &str_size, const std::vector<std::string> &paths, const signed char ide_index, const bool ide_slave, const int reserved_cylinders, bool roflag) {
        if (Drives[drive - 'A']) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
            return false;
        }

        bool imgsizedetect = isHardDrive && sizes[0] == 0;
        
        std::vector<DOS_Drive*> imgDisks;
        std::vector<std::string>::size_type i;
        std::vector<DOS_Drive*>::size_type ct;
        FILE *diskfiles[MAX_SWAPPABLE_DISKS];
        for (i = 0; i < MAX_SWAPPABLE_DISKS; i++) diskfiles[i]=NULL;

        for (i = 0; i < paths.size(); i++) {
            const char* errorMessage = NULL;
            imageDisk* vhdImage = NULL;
            bool ro=false;

            //detect hard drive geometry
            if (imgsizedetect) {
                bool skipDetectGeometry = false;
                sizes[0] = 0;
                sizes[1] = 0;
                sizes[2] = 0;
                sizes[3] = 0;

                /* .HDI images contain the geometry explicitly in the header. */
                if (str_size.size() == 0) {
                    const char *ext = strrchr(paths[i].c_str(), '.');
                    if (ext != NULL) {
                        if (!strcasecmp(ext, ".hdi")) {
                            skipDetectGeometry = true;
                        }
                        if (!strcasecmp(ext, ".nhd")) {
                            skipDetectGeometry = true;
                        }
                        if (!strcasecmp(ext, ".nfd")) {
                            skipDetectGeometry = true;
                        }
                        //for all vhd files where the system will autodetect the chs values,
                        if (!strcasecmp(ext, ".vhd")) {
                            ro=wpcolon&&paths[i].length()>1&&paths[i].c_str()[0]==':';
                            //load the file with imageDiskVHD, which supports fixed/dynamic/differential disks
                            imageDiskVHD::ErrorCodes ret = imageDiskVHD::Open(ro?paths[i].c_str()+1:paths[i].c_str(), ro||roflag, &vhdImage);
                            switch (ret) {
                            case imageDiskVHD::OPEN_SUCCESS: {
                                //upon successful, go back to old code if using a fixed disk, which patches chs values for incorrectly identified disks
                                skipDetectGeometry = true;
                                const imageDiskVHD* vhdDisk = dynamic_cast<imageDiskVHD*>(vhdImage);
                                if (vhdDisk == NULL || vhdDisk->vhdType == imageDiskVHD::VHD_TYPE_FIXED) { //fixed disks would be null here
                                    delete vhdDisk;
                                    vhdDisk = 0;
                                    skipDetectGeometry = false;
                                }
                                break;
                            }
                            case imageDiskVHD::ERROR_OPENING: 
                                errorMessage = (char*)MSG_Get("VHD_ERROR_OPENING"); break;
                            case imageDiskVHD::INVALID_DATA: 
                                errorMessage = (char*)MSG_Get("VHD_INVALID_DATA"); break;
                            case imageDiskVHD::UNSUPPORTED_TYPE: 
                                errorMessage = (char*)MSG_Get("VHD_UNSUPPORTED_TYPE"); break;
                            case imageDiskVHD::ERROR_OPENING_PARENT: 
                                errorMessage = (char*)MSG_Get("VHD_ERROR_OPENING_PARENT"); break;
                            case imageDiskVHD::PARENT_INVALID_DATA: 
                                errorMessage = (char*)MSG_Get("VHD_PARENT_INVALID_DATA"); break;
                            case imageDiskVHD::PARENT_UNSUPPORTED_TYPE: 
                                errorMessage = (char*)MSG_Get("VHD_PARENT_UNSUPPORTED_TYPE"); break;
                            case imageDiskVHD::PARENT_INVALID_MATCH: 
                                errorMessage = (char*)MSG_Get("VHD_PARENT_INVALID_MATCH"); break;
                            case imageDiskVHD::PARENT_INVALID_DATE: 
                                errorMessage = (char*)MSG_Get("VHD_PARENT_INVALID_DATE"); break;
                            default: break;
                            }
                        }
                    }
                }
                if (!skipDetectGeometry && !DetectGeometry(NULL, paths[i].c_str(), sizes)) {
                    errorMessage = (char*)("Unable to detect geometry\n");
                }
            }

            if (!errorMessage) {
                DOS_Drive* newDrive = NULL;
                if (vhdImage) {
                    newDrive = new fatDrive(vhdImage, options);
                    strcpy(newDrive->info, "fatDrive ");
                    strcat(newDrive->info, ro?paths[i].c_str()+1:paths[i].c_str());
                    vhdImage = NULL;
                }
                else {
                    if (roflag) options.push_back("readonly");
                    newDrive = new fatDrive(paths[i].c_str(), (uint32_t)sizes[0], (uint32_t)sizes[1], (uint32_t)sizes[2], (uint32_t)sizes[3], options);
                }
                imgDisks.push_back(newDrive);
				fatDrive* fdrive=dynamic_cast<fatDrive*>(newDrive);
                if (!fdrive->created_successfully) {
                    errorMessage = (char*)MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE");
					if (fdrive->req_ver_major>0) {
						static char ver_msg[150];
						sprintf(ver_msg, "Mounting this image file requires a reported DOS version of %u.%u or higher.\n%s", fdrive->req_ver_major, fdrive->req_ver_minor, errorMessage);
						errorMessage = ver_msg;
					}
                } else {
                    diskfiles[i]=fdrive->loadedDisk->diskimg;
                    if ((vhdImage&&ro)||roflag) fdrive->readonly=true;
                }
            }
            if (errorMessage) {
                if (!qmount) WriteOut(errorMessage);
                for (ct = 0; ct < imgDisks.size(); ct++) {
                    delete imgDisks[ct];
                }
                return false;
            }
        }

        AddToDriveManager(drive, imgDisks, isHardDrive ? 0xF8 : 0xF0);
        DOS_EnableDriveMenu(drive);

        std::string tmp(wpcolon&&paths[0].length()>1&&paths[0].c_str()[0]==':'?paths[0].substr(1):paths[0]);
        for (i = 1; i < paths.size(); i++) {
            tmp += "; " + (wpcolon&&paths[i].length()>1&&paths[i].c_str()[0]==':'?paths[i].substr(1):paths[i]);
        }
        if (!qmount) WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());

        unsigned char driveIndex = drive-'A';
        if (imgDisks.size() == 1 || (imgDisks.size() > 1 && driveIndex < 2 && (swapInDisksSpecificDrive == driveIndex || swapInDisksSpecificDrive == -1))) {
            imageDisk* image = ((fatDrive*)imgDisks[0])->loadedDisk;
            if (AttachToBiosAndIdeByLetter(image, drive, (unsigned char)ide_index, ide_slave)) {
                if (swapInDisksSpecificDrive == driveIndex || swapInDisksSpecificDrive == -1) {
                    for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
                        if (diskSwap[si] != NULL) {
                            diskSwap[si]->Release();
                            diskSwap[si] = NULL;
                        }
                    }
                    swapInDisksSpecificDrive = -1;
                    if (paths.size() > 1) {
                        /* slot 0 is the image we already assigned */
                        diskSwap[0] = image;
                        diskSwap[0]->Addref();
                        swapPosition = 0;
                        swapInDisksSpecificDrive = driveIndex;

                        for (size_t si=1;si < MAX_SWAPPABLE_DISKS && si < paths.size();si++) {
                            imageDisk *img = MountImageNone(paths[si].c_str(), diskfiles[si], sizes, reserved_cylinders, roflag);

                            if (img != NULL) {
                                diskSwap[si] = img;
                                diskSwap[si]->Addref();
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    imageDisk* MountImageNoneRam(Bitu sizes[], const int reserved_cylinders, const bool forceFloppy) {
        imageDiskMemory* dsk = CreateRamDrive(sizes, reserved_cylinders, forceFloppy, this);
        if (dsk == NULL) return NULL;
        //formatting might fail; just log the failure and continue
        uint8_t ret = dsk->Format();
        if (ret != 0x00) {
            LOG_MSG("Warning: could not format RAM drive - error code %u\n", (unsigned int)ret);
        }
        return dsk;
    }

    bool MountRam(Bitu sizes[], const char drive, const signed char ide_index, const bool ide_slave, bool roflag) {
        if (Drives[drive - 'A']) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
            return false;
        }

        //by default, make a floppy disk if A: or B: is specified (still makes a hard drive if not a recognized size)
        imageDiskMemory* dsk = CreateRamDrive(sizes, 0, (drive - 'A') < 2 && sizes[0] == 0, this);
        if (dsk == NULL) return false;
        if (dsk->Format() != 0x00) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
            delete dsk;
            return false;
        }
        dsk->Addref();
        DOS_Drive* newDrive = new fatDrive(dsk, options);
        if (roflag) newDrive->readonly=true;
        dsk->Release();
        if (!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
            delete newDrive; //this executes dsk.Release() which executes delete dsk
            return false;
        }

        AddToDriveManager(drive, newDrive, dsk->hardDrive ? 0xF8 : 0xF0);
        DOS_EnableDriveMenu(drive);

        WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_RAMDRIVE"), drive);

        AttachToBiosAndIdeByLetter(dsk, drive, (unsigned char)ide_index, ide_slave);

        return true;
    }

    void AddToDriveManager(const char drive, DOS_Drive* imgDisk, const uint8_t mediaid) {
        std::vector<DOS_Drive*> imgDisks = { imgDisk };
        AddToDriveManager(drive, imgDisks, mediaid);
    }

    void AddToDriveManager(const char drive, const std::vector<DOS_Drive*> &imgDisks, const uint8_t mediaid) {
        std::vector<DOS_Drive*>::size_type ct;

        // Update DriveManager
        for (ct = 0; ct < imgDisks.size(); ct++) {
            DriveManager::AppendDisk(drive - 'A', imgDisks[ct]);
        }
        DriveManager::InitializeDrive(drive - 'A');

        // Set the correct media byte in the table 
        mem_writeb(Real2Phys(dos.tables.mediaid) + ((unsigned int)drive - 'A') * dos.tables.dpb_size, mediaid);

        /* Command uses dta so set it to our internal dta */
        RealPt save_dta = dos.dta();
        dos.dta(dos.tables.tempdta);

        for (ct = 0; ct < imgDisks.size(); ct++) {
            DriveManager::CycleDisks(drive - 'A', (ct == (imgDisks.size() - 1)));

            char root[7] = { drive, ':', '\\', '*', '.', '*', 0 };
            DOS_FindFirst(root, DOS_ATTR_VOLUME); // force obtaining the label and saving it in dirCache
        }
        dos.dta(save_dta);

    }

    bool DetectGeometry(FILE * file, const char* fileName, Bitu sizes[]) {
        bool yet_detected = false, readonly = wpcolon&&strlen(fileName)>1&&fileName[0]==':';
        FILE * diskfile = file==NULL?fopen64(readonly?fileName+1:fileName, "rb"):file;
#if defined(WIN32)
        if (!diskfile && file==NULL) {
            const host_cnv_char_t* host_name = CodePageGuestToHost(readonly?fileName+1:fileName);
            if (host_name != NULL) diskfile = _wfopen(host_name, L"rb");
        }
#endif
        if (!diskfile) {
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
            return false;
        }
        fseeko64(diskfile, 0L, SEEK_END);
        uint32_t fcsize = (uint32_t)(ftello64(diskfile) / 512L);
        uint8_t buf[512];
        // check for vhd signature
        fseeko64(diskfile, -512, SEEK_CUR);
        if (fread(buf, sizeof(uint8_t), 512, diskfile)<512) {
            fclose(diskfile);
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
            return false;
        }
        if (!strcmp((const char*)buf, "conectix")) {
            fcsize--;   // skip footer (512 bytes)
            sizes[0] = 512; // sector size
            sizes[1] = buf[0x3b];   // sectors
            sizes[2] = buf[0x3a];   // heads
            sizes[3] = SDL_SwapBE16((uint16_t)(*(int16_t*)(buf + 0x38)));    // cylinders

                                                                // Do translation (?)
            while ((sizes[2] < 128u) && (sizes[3] > 1023u)) {
                sizes[2] <<= 1u;
                sizes[3] >>= 1u;
            }

            if (sizes[3]>1023) {
                // Set x/255/63
                sizes[2] = 255;
                sizes[3] = fcsize / sizes[2] / sizes[1];
            }

            LOG_MSG("VHD image detected: %u,%u,%u,%u",
                (unsigned int)sizes[0], (unsigned int)sizes[1], (unsigned int)sizes[2], (unsigned int)sizes[3]);
            if (sizes[3]>1023) LOG_MSG("WARNING: cylinders>1023, INT13 will not work unless extensions are used");
            yet_detected = true;
        }

        fseeko64(diskfile, 0L, SEEK_SET);
        if (fread(buf, sizeof(uint8_t), 512, diskfile)<512) {
            fclose(diskfile);
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
            return false;
        }
        if (file==NULL) fclose(diskfile);
        // check it is not dynamic VHD image
        if (!strcmp((const char*)buf, "conectix")) {
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_DYNAMIC_VHD_UNSUPPORTED"));
            return false;
        }
        // check MBR signature for unknown images
        if (!yet_detected && ((buf[510] != 0x55) || (buf[511] != 0xaa))) {
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
            return false;
        }
        // check MBR partition entry 1
        if (!yet_detected)
            yet_detected = DetectMFMsectorPartition(buf, fcsize, sizes);

        // Try bximage disk geometry
        // bximage flat images should already be detected by
        // DetectMFMSectorPartition(), not sure what this adds...
        if (!yet_detected) {
            yet_detected = DetectBximagePartition(fcsize, sizes);
        }
        
        uint8_t ptype = buf[0x1c2]; // Location of DOS 3.3+ partition type
	bool assume_lba = false;

	/* If the first partition is a Windows 95 FAT32 (LBA) type partition, and we failed to detect,
	 * then assume LBA and make up a geometry */
	if (!yet_detected && (ptype == 0x0C/*FAT32+LBA*/ || ptype == 0x0E/*FAT16+LBA*/)) {
		yet_detected = 1;
		assume_lba = true;
		LOG_MSG("Failed to autodetect geometry, assuming LBA approximation based on first partition type (FAT with LBA)");
	}

	/* If the MBR has only a partition table, but the part that normally contains executable
	 * code is all zeros. To avoid false negatives, check only the first 0x20 bytes since
	 * at boot time executable code must reside there to do something, and many of these
	 * disk images while they ARE mostly zeros, do have some extra nonzero bytes immediately
	 * before the partition table at 0x1BE.
	 *
	 * Modern FAT32 generator tools and older digital cameras will format FAT32 like this.
	 * These tools are unlikely to support non-LBA disks.
	 *
	 * To avoid false positives, the partition type has to be something related to FAT */
	if (!yet_detected && (ptype == 0x01 || ptype == 0x04 || ptype == 0x06 || ptype == 0x0B || ptype == 0x0C || ptype == 0x0E)) {
		/* buf[] still contains MBR */
		unsigned int i=0;
		while (i < 0x20 && buf[i] == 0) i++;
		if (i == 0x20) {
			yet_detected = 1;
			assume_lba = true;
			LOG_MSG("Failed to autodetect geometry, assuming LBA approximation based on first partition type (FAT-related) and lack of executable code in the MBR");
		}
	}

	/* If we failed to detect, but the disk image is 4GB or larger, make up a geometry because
	 * IDE drives by that point were pure LBA anyway and supported C/H/S for the sake of
	 * backward compatibility anyway. fcsize is in 512-byte sectors. */
	if (!yet_detected && fcsize >= ((4ull*1024ull*1024ull*1024ull)/512ull)) {
		yet_detected = 1;
		assume_lba = true;
		LOG_MSG("Failed to autodetect geometry, assuming LBA approximation based on size");
	}

	if (yet_detected && assume_lba) {
		sizes[0] = 512;
		sizes[1] = 63;
		sizes[2] = 255;
		{
			const Bitu d = sizes[1]*sizes[2];
			sizes[3] = (fcsize + d - 1) / d; /* round up */
		}
	}

	if (yet_detected) {
            //"Image geometry auto detection: -size %u,%u,%u,%u\r\n",
            //sizes[0],sizes[1],sizes[2],sizes[3]);
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_AUTODET_VALUES"), sizes[0], sizes[1], sizes[2], sizes[3]);
            return true;
        }
        else {
            if (!qmount) WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
            return false;
        }
    }

    bool DetectMFMsectorPartition(uint8_t buf[], uint32_t fcsize, Bitu sizes[]) {
        // This is used for plain MFM sector format as created by IMGMAKE
        // It tries to find the first partition. Addressing is in CHS format.
        /* Offset   | Length    | Description
         * +0       | 1 byte    | 80 hex = active, 00 = inactive
         * +1       | 3 bytes   | CHS of first sector in partition
         * +4       | 1 byte    | partition type
         * +5       | 3 bytes   | CHS of last sector in partition
         * +8       | 4 bytes   | LBA of first sector in partition
         * +C       | 4 bytes   | Number of sectors in partition. 0 may mean, use LBA
         */
        uint8_t starthead = 0; // start head of partition
        uint8_t startsect = 0; // start sector of partition
        uint16_t startcyl = 0; // start cylinder of partition
        uint8_t ptype = 0;     // Partition Type
        uint16_t endcyl = 0;   // end cylinder of partition
        uint8_t heads = 0;     // heads in partition
        uint8_t sectors = 0;   // sectors per track in partition
        uint32_t pe1_size = host_readd(&buf[0x1ca]);
        if ((uint32_t)host_readd(&buf[0x1fa]) != 0) {     // DOS 2.0-3.21 partition table
            pe1_size = host_readd(&buf[0x1fa]);
            starthead = buf[0x1ef];
            startsect = (buf[0x1f0] & 0x3fu) - 1u;
            startcyl = (unsigned char)buf[0x1f1] | (unsigned int)((buf[0x1f0] & 0xc0) << 2u);
            endcyl = (unsigned char)buf[0x1f5] | (unsigned int)((buf[0x1f4] & 0xc0) << 2u);
            ptype = buf[0x1f2];
            heads = buf[0x1f3] + 1u;
            sectors = buf[0x1f4] & 0x3fu;
        } else if (pe1_size != 0) {                     // DOS 3.3+ partition table, starting at 0x1BE
            starthead = buf[0x1bf];
            startsect = (buf[0x1c0] & 0x3fu) - 1u;
            startcyl = (unsigned char)buf[0x1c1] | (unsigned int)((buf[0x1c0] & 0xc0) << 2u);
            endcyl = (unsigned char)buf[0x1c5] | (unsigned int)((buf[0x1c4] & 0xc0) << 2u);
            ptype = buf[0x1c2];
            heads = buf[0x1c3] + 1u;
            sectors = buf[0x1c4] & 0x3fu;
        }
        (void)ptype;//GCC: Set but not used. Assume it will be used someday --J.C.
        if (pe1_size != 0) {
            uint32_t part_start = startsect + sectors * starthead +
                startcyl * sectors * heads;
            uint32_t part_end = heads * sectors * endcyl;
            uint32_t part_len = part_end - part_start;
            // partition start/end sanity check
            // partition length should not exceed file length
            // real partition size can be a few cylinders less than pe1_size
            // if more than 1023 cylinders see if first partition fits
            // into 1023, else bail.
            if (/*(part_len<0) always false because unsigned || */(part_len > pe1_size) || (pe1_size > fcsize) ||
                ((pe1_size - part_len) / (sectors*heads)>2u) ||
                ((pe1_size / (heads*sectors))>1023u)) {
                //LOG_MSG("start(c,h,s) %u,%u,%u",startcyl,starthead,startsect);
                //LOG_MSG("endcyl %u heads %u sectors %u",endcyl,heads,sectors);
                //LOG_MSG("psize %u start %u end %u",pe1_size,part_start,part_end);
            } else {
                sizes[0] = 512; sizes[1] = sectors;
                sizes[2] = heads; sizes[3] = (uint16_t)(fcsize / (heads*sectors));
                if (sizes[3]>1023) sizes[3] = 1023;
                return true;
            }
        }
        return false;
    }
    
    bool DetectBximagePartition(uint32_t fcsize, Bitu sizes[]) {
        // Try bximage disk geometry
        uint32_t cylinders = fcsize / (16 * 63);
        // Int13 only supports up to 1023 cylinders
        // For mounting unknown images we could go up with the heads to 255
        if ((cylinders * 16 * 63 == fcsize) && (cylinders<1024)) {
            sizes[0] = 512; sizes[1] = 63; sizes[2] = 16; sizes[3] = cylinders;
            return true;
        }
        return false;
    }
    
    bool MountIso(const char drive, const std::vector<std::string> &paths, const signed char ide_index, const bool ide_slave) {
        //mount cdrom
        if (Drives[drive - 'A']) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
            return false;
        }
        uint8_t mediaid = 0xF8;
        MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
        // create new drives for all images
        std::vector<DOS_Drive*> isoDisks;
        std::vector<std::string>::size_type i;
        std::vector<DOS_Drive*>::size_type ct;
        for (i = 0; i < paths.size(); i++) {
            int error = -1;
            DOS_Drive* newDrive = new isoDrive(drive, wpcolon&&paths[i].length()>1&&paths[i].c_str()[0]==':'?paths[i].c_str()+1:paths[i].c_str(), mediaid, error);
            isoDisks.push_back(newDrive);
            if (!qmount)
            switch (error) {
            case 0: break;
            case 1: WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));  break;
            case 2: WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));    break;
            case 3: WriteOut(MSG_Get("MSCDEX_ERROR_OPEN"));             break;
            case 4: WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));        break;
            case 5: WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));        break;
            case 6: WriteOut(MSG_Get("MSCDEX_INVALID_FILEFORMAT"));     break;
            default: WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));         break;
            }
            // error: clean up and leave
            if (error) {
                for (ct = 0; ct < isoDisks.size(); ct++) {
                    delete isoDisks[ct];
                }
                return false;
            }
        }
        // Update DriveManager
        for (ct = 0; ct < isoDisks.size(); ct++) {
            DriveManager::AppendDisk(drive - 'A', isoDisks[ct]);
        }
        DriveManager::InitializeDrive(drive - 'A');
        DOS_EnableDriveMenu(drive);

        // Set the correct media byte in the table 
        mem_writeb(Real2Phys(dos.tables.mediaid) + ((unsigned int)drive - 'A') * dos.tables.dpb_size, mediaid);

        // If instructed, attach to IDE controller as ATAPI CD-ROM device
        if (ide_index >= 0) IDE_CDROM_Attach(ide_index, ide_slave, drive - 'A');

        // Print status message (success)
        if (!qmount) WriteOut(MSG_Get("MSCDEX_SUCCESS"));
        std::string tmp(wpcolon&&paths[0].length()>1&&paths[0].c_str()[0]==':'?paths[0].substr(1):paths[0]);
        for (i = 1; i < paths.size(); i++) {
            tmp += "; " + (wpcolon&&paths[i].length()>1&&paths[i].c_str()[0]==':'?paths[i].substr(1):paths[i]);
        }
        if (!qmount) WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());
        return true;
    }

    imageDisk* MountImageNone(const char* fileName, FILE* file, const Bitu sizesOriginal[], const int reserved_cylinders, bool roflag) {
        imageDisk* newImage = 0;
        Bitu sizes[4];
        sizes[0] = sizesOriginal[0];
        sizes[1] = sizesOriginal[1];
        sizes[2] = sizesOriginal[2];
        sizes[3] = sizesOriginal[3];

        //check for VHD files
        if (sizes[0] == 0 /* auto detect size */) {
            const char *ext = strrchr(fileName, '.');
            if (ext != NULL) {
                if (!strcasecmp(ext, ".vhd")) {
                    bool ro=wpcolon&&strlen(fileName)>1&&fileName[0]==':';
                    imageDiskVHD::ErrorCodes ret = imageDiskVHD::Open(ro?fileName+1:fileName, ro||roflag, &newImage);
                    switch (ret) {
                    case imageDiskVHD::ERROR_OPENING: WriteOut(MSG_Get("VHD_ERROR_OPENING")); break;
                    case imageDiskVHD::INVALID_DATA: WriteOut(MSG_Get("VHD_INVALID_DATA")); break;
                    case imageDiskVHD::UNSUPPORTED_TYPE: WriteOut(MSG_Get("VHD_UNSUPPORTED_TYPE")); break;
                    case imageDiskVHD::ERROR_OPENING_PARENT: WriteOut(MSG_Get("VHD_ERROR_OPENING_PARENT")); break;
                    case imageDiskVHD::PARENT_INVALID_DATA: WriteOut(MSG_Get("VHD_PARENT_INVALID_DATA")); break;
                    case imageDiskVHD::PARENT_UNSUPPORTED_TYPE: WriteOut(MSG_Get("VHD_PARENT_UNSUPPORTED_TYPE")); break;
                    case imageDiskVHD::PARENT_INVALID_MATCH: WriteOut(MSG_Get("VHD_PARENT_INVALID_MATCH")); break;
                    case imageDiskVHD::PARENT_INVALID_DATE: WriteOut(MSG_Get("VHD_PARENT_INVALID_DATE")); break;
                    default: break;
                    }
                    return newImage;
                }
            }
        }

        uint32_t imagesize;
        /* auto-fill: sector size */
        if (sizes[0] == 0) sizes[0] = 512;

		bool readonly = wpcolon&&strlen(fileName)>1&&fileName[0]==':';
		const char* fname=readonly?fileName+1:fileName;
        FILE *newDisk = file==NULL?fopen_lock(fname, readonly||roflag?"rb":"rb+"):file;
        if (!newDisk) {
            if (!qmount) WriteOut("Unable to open '%s'\n", fname);
            return NULL;
        }

        QCow2Image::QCow2Header qcow2_header = QCow2Image::read_header(newDisk);

        uint64_t sectors;
        if (qcow2_header.magic == QCow2Image::magic && (qcow2_header.version == 2 || qcow2_header.version == 3)) {
            uint32_t cluster_size = 1u << qcow2_header.cluster_bits;
            if ((sizes[0] < 512) || ((cluster_size % sizes[0]) != 0)) {
                WriteOut("Sector size must be larger than 512 bytes and evenly divide the image cluster size of %lu bytes.\n", cluster_size);
                return 0;
            }
            sectors = (uint64_t)qcow2_header.size / (uint64_t)sizes[0];
            imagesize = (uint32_t)(qcow2_header.size / 1024L);
            setbuf(newDisk, NULL);
            newImage = new QCow2Disk(qcow2_header, newDisk, (uint8_t *)fname, imagesize, (uint32_t)sizes[0], (imagesize > 2880));
        }
        else {
            char tmp[256];

            fseeko64(newDisk, 0L, SEEK_SET);
            size_t readResult = fread(tmp, 256, 1, newDisk); // look for magic signatures
            if (readResult != 1) {
                LOG(LOG_IO, LOG_ERROR) ("Reading error in MountImageNone\n");
                return NULL;
            }

            const char *ext = strrchr(fname,'.');

            if (ext != NULL && !strcasecmp(ext, ".d88")) {
                fseeko64(newDisk, 0L, SEEK_END);
                sectors = (uint64_t)ftello64(newDisk) / (uint64_t)sizes[0];
                imagesize = (uint32_t)(sectors / 2); /* orig. code wants it in KBs */
                setbuf(newDisk, NULL);
                newImage = new imageDiskD88(newDisk, (uint8_t *)fname, imagesize, (imagesize > 2880));
            }
            else if (!memcmp(tmp, "VFD1.", 5)) { /* FDD files */
                fseeko64(newDisk, 0L, SEEK_END);
                sectors = (uint64_t)ftello64(newDisk) / (uint64_t)sizes[0];
                imagesize = (uint32_t)(sectors / 2); /* orig. code wants it in KBs */
                setbuf(newDisk, NULL);
                newImage = new imageDiskVFD(newDisk, (uint8_t *)fname, imagesize, (imagesize > 2880));
            }
            else if (!memcmp(tmp,"T98FDDIMAGE.R0\0\0",16)) {
                fseeko64(newDisk, 0L, SEEK_END);
                sectors = (uint64_t)ftello64(newDisk) / (uint64_t)sizes[0];
                imagesize = (uint32_t)(sectors / 2); /* orig. code wants it in KBs */
                setbuf(newDisk, NULL);
                newImage = new imageDiskNFD(newDisk, (uint8_t *)fname, imagesize, (imagesize > 2880), 0);
            }
            else if (!memcmp(tmp,"T98FDDIMAGE.R1\0\0",16)) {
                fseeko64(newDisk, 0L, SEEK_END);
                sectors = (uint64_t)ftello64(newDisk) / (uint64_t)sizes[0];
                imagesize = (uint32_t)(sectors / 2); /* orig. code wants it in KBs */
                setbuf(newDisk, NULL);
                newImage = new imageDiskNFD(newDisk, (uint8_t *)fname, imagesize, (imagesize > 2880), 1);
            }
            else {
                fseeko64(newDisk, 0L, SEEK_END);
                sectors = (uint64_t)ftello64(newDisk) / (uint64_t)sizes[0];
                imagesize = (uint32_t)(sectors / 2); /* orig. code wants it in KBs */
                setbuf(newDisk, NULL);
                newImage = new imageDisk(newDisk, fname, imagesize, (imagesize > 2880));
            }
        }

        /* sometimes imageDisk is able to determine geometry automatically (HDI images) */
        if (newImage) {
            if (newImage->sectors != 0 && newImage->heads != 0 && newImage->cylinders != 0 && newImage->sector_size != 0) {
                /* prevent the code below from changing the geometry */
                sizes[0] = newImage->sector_size;
                sizes[1] = newImage->sectors;
                sizes[2] = newImage->heads;
                sizes[3] = newImage->cylinders;
            }
        }

        /* try auto-detect */
        if (sizes[3] == 0 && sizes[2] == 0) {
            DetectGeometry(newDisk, fname, sizes);
        }

        /* auto-fill: sector/track count */
        if (sizes[1] == 0) sizes[1] = 63;
        /* auto-fill: head/cylinder count */
        if (sizes[3]/*cylinders*/ == 0 && sizes[2]/*heads*/ == 0) {
            sizes[2] = 16; /* typical hard drive, unless a very old drive */
            sizes[3]/*cylinders*/ = (Bitu)((uint64_t)sectors / (uint64_t)sizes[2]/*heads*/ / (uint64_t)sizes[1]/*sectors/track*/);

            if (IS_PC98_ARCH) {
                /* TODO: PC-98 has it's own issues with a 4096-cylinder limit */
            }
            else {
                /* INT 13h mapping, deal with 1024-cyl limit */
                while (sizes[3] > 1024) {
                    if (sizes[2] >= 255) break; /* nothing more we can do */

                    /* try to generate head count 16, 32, 64, 128, 255 */
                    sizes[2]/*heads*/ *= 2;
                    if (sizes[2] >= 256) sizes[2] = 255;

                    /* and recompute cylinders */
                    sizes[3]/*cylinders*/ = (Bitu)((uint64_t)sectors / (uint64_t)sizes[2]/*heads*/ / (uint64_t)sizes[1]/*sectors/track*/);
                }
            }
        }

        LOG(LOG_DOSMISC, LOG_NORMAL)("Mounting image as C/H/S %u/%u/%u with %u bytes/sector",
            (unsigned int)sizes[3], (unsigned int)sizes[2], (unsigned int)sizes[1], (unsigned int)sizes[0]);

        if (imagesize > 2880) newImage->Set_Geometry((uint32_t)sizes[2], (uint32_t)sizes[3], (uint32_t)sizes[1], (uint32_t)sizes[0]);
        if (reserved_cylinders > 0) newImage->Set_Reserved_Cylinders((Bitu)reserved_cylinders);

        return newImage;
    }
};

void IMGMOUNT_ProgramStart(Program * * make) {
    *make=new IMGMOUNT;
}

void runImgmount(const char *str) {
	IMGMOUNT imgmount;
	imgmount.cmd=new CommandLine("IMGMOUNT", str);
	imgmount.Run();
}

Bitu DOS_SwitchKeyboardLayout(const char* new_layout, int32_t& tried_cp);
Bitu DOS_LoadKeyboardLayout(const char * layoutname, int32_t codepage, const char * codepagefile);
const char* DOS_GetLoadedLayout(void);

class KEYB : public Program {
public:
    void Run(void);
};

void KEYB::Run(void) {
    // codepage 949 start
    std::string temp_codepage;
    temp_codepage="949";
    if (cmd->FindString("ko",temp_codepage,false)) {
        dos.loaded_codepage=949;
        const char* layout_name = DOS_GetLoadedLayout();
        WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
        return;
    }
    // codepage 949 end
    if (cmd->FindCommand(1,temp_line)) {
        if (cmd->FindString("?",temp_line,false)) {
            WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
        } else {
            /* first parameter is layout ID */
            Bitu keyb_error=0;
            std::string cp_string;
            int32_t tried_cp = -1;
            if (cmd->FindCommand(2,cp_string)) {
                /* second parameter is codepage number */
                tried_cp=atoi(cp_string.c_str());
                char cp_file_name[256];
                if (cmd->FindCommand(3,cp_string)) {
                    /* third parameter is codepage file */
                    strcpy(cp_file_name, cp_string.c_str());
                } else {
                    /* no codepage file specified, use automatic selection */
                    strcpy(cp_file_name, "auto");
                }

                keyb_error=DOS_LoadKeyboardLayout(temp_line.c_str(), tried_cp, cp_file_name);
            } else {
                keyb_error=DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
            }
            switch (keyb_error) {
                case KEYB_NOERROR:
                    WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"),temp_line.c_str(),dos.loaded_codepage);
                    runRescan("-A -Q");
                    break;
                case KEYB_FILENOTFOUND:
                    if (temp_line!="/?"&&temp_line!="-?") WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
                    WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
                    break;
                case KEYB_INVALIDFILE:
                    WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"),temp_line.c_str());
                    break;
                case KEYB_LAYOUTNOTFOUND:
                    WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),temp_line.c_str(),tried_cp);
                    break;
                case KEYB_INVALIDCPFILE:
                    WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"),temp_line.c_str());
                    WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
                    break;
                default:
                    LOG(LOG_DOSMISC,LOG_ERROR)("KEYB:Invalid returncode %x",(int)keyb_error);
                    break;
            }
        }
    } else {
        /* no parameter in the command line, just output codepage info and possibly loaded layout ID */
        const char* layout_name = DOS_GetLoadedLayout();
        if (layout_name==NULL) {
            WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
        } else {
            WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
        }
    }
}

static void KEYB_ProgramStart(Program * * make) {
    *make=new KEYB;
}

// MODE

class MODE : public Program {
public:
    void Run(void);
};

bool setlines(const char *mname);
void MODE::Run(void) {
    uint16_t rate=0,delay=0,cols=0,lines=0,mode;
    if (!cmd->FindCommand(1,temp_line) || temp_line=="/?") {
        WriteOut(MSG_Get("PROGRAM_MODE_USAGE"));
        return;
    }
    else if (strcasecmp(temp_line.c_str(),"con")==0 || strcasecmp(temp_line.c_str(),"con:")==0) {
        if (IS_PC98_ARCH) return;
        int LINES = 25, COLS = 80;
        LINES=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
        COLS=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
        if (cmd->GetCount()<2) {
            WriteOut("Status for device CON:\n----------------------\nColumns=%d\nLines=%d\n\nCode page operation not supported on this device\n", COLS, LINES);
            return;
        }
        if (cmd->FindStringBegin("rate=", temp_line,false)) rate=atoi(temp_line.c_str());
        if (cmd->FindStringBegin("delay=", temp_line,false)) delay=atoi(temp_line.c_str());
        if (cmd->FindStringBegin("cols=", temp_line,false)) cols=atoi(temp_line.c_str()); else cols=COLS;
        if (cmd->FindStringBegin("lines=",temp_line,false)) lines=atoi(temp_line.c_str()); else lines=LINES;
        bool optr=cmd->FindStringBegin("rate=", temp_line,true), optd=cmd->FindStringBegin("delay=",temp_line,true), optc=cmd->FindStringBegin("cols=", temp_line,true), optl=cmd->FindStringBegin("lines=",temp_line,true);
        if (optr&&!optd||optd&&!optr) {
            WriteOut("Rate and delay must be specified together\n");
            return;
        }
        if (cmd->GetCount()>1) goto modeparam;
        if (optr&&optd) {
            if (rate<1 || rate>32 || delay<1 || delay>4) goto modeparam;
            IO_Write(0x60,0xf3); IO_Write(0x60,(uint8_t)(((delay-1)<<5)|(32-rate)));
        }
        if ((optc||optl)&&(cols!=COLS||lines!=LINES)) {
            std::string cmd="line_"+std::to_string(cols)+"x"+std::to_string(lines);
            if (!setlines(cmd.c_str())) goto modeparam;
        }
        return;
    }
    else if (cmd->GetCount()>1) goto modeparam;
    else if (strcasecmp(temp_line.c_str(),"mono")==0) mode=7;
    else if (machine==MCH_HERC || machine==MCH_MDA) goto modeparam;
    else if (strcasecmp(temp_line.c_str(),"co80")==0) mode=3;
    else if (strcasecmp(temp_line.c_str(),"bw80")==0) mode=2;
    else if (strcasecmp(temp_line.c_str(),"co40")==0) mode=1;
    else if (strcasecmp(temp_line.c_str(),"bw40")==0) mode=0;
    else goto modeparam;
    mem_writeb(BIOS_CONFIGURATION,(mem_readb(BIOS_CONFIGURATION)&0xcf)|((mode==7)?0x30:0x20));
    reg_ax=mode;
    CALLBACK_RunRealInt(0x10);
    return;
modeparam:
    WriteOut(MSG_Get("PROGRAM_MODE_INVALID_PARAMETERS"));
    return;
}

static void MODE_ProgramStart(Program * * make) {
    *make=new MODE;
}
/*
// MORE
class MORE : public Program {
public:
    void Run(void);
};

void MORE::Run(void) {
    if (cmd->GetCount()) {
        WriteOut(MSG_Get("PROGRAM_MORE_USAGE"));
        return;
    }
    uint16_t ncols=mem_readw(BIOS_SCREEN_COLUMNS);
    uint16_t nrows=(uint16_t)mem_readb(BIOS_ROWS_ON_SCREEN_MINUS_1);
    uint16_t col=1,row=1;
    uint8_t c;uint16_t n=1;
    WriteOut("\n");
    while (n) {
        DOS_ReadFile(STDIN,&c,&n);
        if (n==0 || c==0x1a) break; // stop at EOF
        switch (c) {
            case 0x07: break;
            case 0x08: if (col>1) col--; break;
            case 0x09: col=((col+7)&~7)+1; break;
            case 0x0a: row++; break;
            case 0x0d: col=1; break;
            default: col++; break;
        }
        if (col>ncols) {col=1;row++;}
        DOS_WriteFile(STDOUT,&c,&n);
        if (row>=nrows) {
            WriteOut(MSG_Get("PROGRAM_MORE_MORE"));
            DOS_ReadFile(STDERR,&c,&n);
            if (c==0) DOS_ReadFile(STDERR,&c,&n); // read extended key
            WriteOut("\n\n");
            col=row=1;
        }
    }
}

static void MORE_ProgramStart(Program * * make) {
    *make=new MORE;
}
*/

void REDOS_ProgramStart(Program * * make);
void A20GATE_ProgramStart(Program * * make);
void PC98UTIL_ProgramStart(Program * * make);
void VESAMOED_ProgramStart(Program * * make);

#if defined C_DEBUG
class NMITEST : public Program {
public:
    void Run(void) {
        if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
			WriteOut("Generates a non-maskable interrupt (NMI).\n\nNMITEST\n\nNote: This is a debugging tool to test if the interrupt handler works properly.\n");
            return;
		}
        WriteOut("Generating a non-maskable interrupt (NMI)...\n");
        CPU_Raise_NMI();
    }
};

static void NMITEST_ProgramStart(Program * * make) {
    *make=new NMITEST;
}
#endif

class CAPMOUSE : public Program
{
public:
	void Run() override
    {
        auto val = 0;
        auto tmp = std::string("");

        if(cmd->GetCount() == 0 || cmd->FindExist("/?", true))
            val = 0;
        else if(cmd->FindExist("/C", false))
            val = 1;
        else if(cmd->FindExist("/R", false))
            val = 2;

        auto cap = false;
        switch(val)
        {
        case 2:
            break;
        case 1:
            cap = true;
            break;
        case 0:
        default:
            WriteOut("Captures or releases the mouse inside DOSBox-X.\n\n");
            WriteOut("CAPMOUSE [/C|/R]\n");
            WriteOut("  /C Capture the mouse\n");
            WriteOut("  /R Release the mouse\n");
            return;
        }

        CaptureMouseNotify(!cap);
        GFX_CaptureMouse(cap);
        std::string msg;
        msg.append("Mouse ");
        msg.append(Mouse_IsLocked() ? "captured" : "released");
        msg.append("\n");
        WriteOut(msg.c_str());
    }
};

void CAPMOUSE_ProgramStart(Program** make)
{
	*make = new CAPMOUSE;
}

class LABEL : public Program
{
public:
    void Help() {
        WriteOut("Creates, changes, or deletes the volume label of a drive.\n\nLABEL [drive:][label]\n\n  [drive:]\tSpecifies the drive letter\n  [label]\tSpecifies the volume label\n");
    }
	void Run() override
    {
        /* MS-DOS behavior: If no label provided at the command line, prompt for one.
         *
         * LABEL [drive:] [label]
         *
         * No options are supported in MS-DOS, and the label can have spaces in it.
         * This is valid, apparently:
         *
         * LABEL H E L L O
         *
         * Will set the volume label to "H E L L O"
         *
         * Label /? will print help.
         */
        std::string label;
    	uint8_t drive = DOS_GetDefaultDrive();
        const char *raw = cmd->GetRawCmdline().c_str();

        /* skip space */
        while (*raw == ' ') raw++;

        /* options */
        if (raw[0] == '/') {
            raw++;
            if (raw[0] == '?') {
                Help();
                return;
            }
        }

        /* is the next part a drive letter? */
        if (raw[0] != 0 && raw[1] != 0) {
            if (isalpha(raw[0]) && raw[1] == ':') {
                drive = tolower(raw[0]) - 'a';
                raw += 2;
                while (*raw == ' ') raw++;
            }
        }

        /* then the label. MS-DOS behavior is to treat the rest of the command line, spaces and all, as the label */
        if (*raw != 0) {
            label = raw;
        }

        /* if the label is longer than 11 chars or contains a dot, MS-DOS will reject it and then prompt for another label */
        if (label.length() > 11) {
            WriteOut("Label is too long (more than 11 chars)\n");
            label.clear();
        }
        else if (label.find_first_of(".:/\\") != std::string::npos) {
            WriteOut("Label has invalid chars.\n");
            label.clear();
        }

        /* if no label provided, MS-DOS will display the current label and serial number and prompt the user to type in a new label. */
        if (label.empty()) {
            std::string clabel = Drives[drive]->GetLabel();

            if (!clabel.empty())
                WriteOut("Volume in drive %c is %s\n",drive+'A',clabel.c_str());
            else
                WriteOut("Volume in drive %c has no label\n",drive+'A');
        }

        /* If no label is provided, MS-DOS will prompt the user whether to delete the label. */
        if (label.empty()) {
            uint8_t c,ans=0;
            uint16_t s;

            do {
                WriteOut("Delete the volume label (Y/N)? ");
                s = 1;
                DOS_ReadFile(STDIN,&c,&s);
                WriteOut("\n");
                if (s != 1) return;
                ans = uint8_t(tolower(char(c)));
            } while (!(ans == 'y' || ans == 'n'));

            if (ans != 'y') return;
        }

        /* delete then create the label */
		Drives[drive]->SetLabel("",false,true);
		Drives[drive]->SetLabel(label.c_str(),false,true);
    }
};

void LABEL_ProgramStart(Program** make)
{
	*make = new LABEL;
}

std::vector<std::string> MAPPER_GetEventNames(const std::string &prefix);
void MAPPER_AutoType(std::vector<std::string> &sequence, const uint32_t wait_ms, const uint32_t pacing_ms);

class AUTOTYPE : public Program {
public:
	void Run();

private:
	void PrintUsage();
	void PrintKeys();
	bool ReadDoubleArg(const std::string &name,
	                   const char *flag,
	                   const double &def_value,
	                   const double &min_value,
	                   const double &max_value,
	                   double &value);
};

void AUTOTYPE_ProgramStart(Program **make);

void AUTOTYPE::PrintUsage()
{
	constexpr const char *msg =
	        "Performs scripted keyboard entry into a running DOS program.\n\n"
	        "AUTOTYPE [-list] [-w WAIT] [-p PACE] button_1 [button_2 [...]]\n\n"
	        "Where:\n"
	        "  -list:   prints all available button names.\n"
	        "  -w WAIT: seconds before typing begins. Two second default; max of 30.\n"
	        "  -p PACE: seconds between each keystroke. Half-second default; max of 10.\n\n"
	        "  The sequence is comprised of one or more space-separated buttons.\n"
	        "  Autotyping begins after WAIT seconds, and each button is entered\n"
	        "  every PACE seconds. The , character inserts an extra PACE delay.\n\n"
	        "Some examples:\n"
	        "  \033[32;1mAUTOTYPE -w 1 -p 0.3 up enter , right enter\033[0m\n"
	        "  \033[32;1mAUTOTYPE -p 0.2 f1 kp_8 , , enter\033[0m\n"
	        "  \033[32;1mAUTOTYPE -w 1.3 esc enter , p l a y e r enter\033[0m\n";
	WriteOut_NoParsing(msg);
}

// Prints the key-names for the mapper's currently-bound events.
void AUTOTYPE::PrintKeys()
{
	const std::vector<std::string> names = MAPPER_GetEventNames("key_");

	// Keep track of the longest key name
	size_t max_length = 0;
	for (const auto &name : names)
		max_length = (std::max)(name.length(), max_length);

	// Sanity check to avoid dividing by 0
	if (!max_length) {
		WriteOut_NoParsing(
		        "AUTOTYPE: The mapper has no key bindings\n");
		return;
	}

	// Setup our rows and columns
	const size_t wrap_width = 72; // confortable columns not pushed to the edge
	const size_t columns = wrap_width / max_length;
	const size_t rows = ceil_udivide(names.size(), columns);

	// Build the string output by rows and columns
	auto name = names.begin();
	for (size_t row = 0; row < rows; ++row) {
		for (size_t i = row; i < names.size(); i += rows)
			WriteOut("  %-*s", static_cast<int>(max_length), name[i].c_str());
		WriteOut_NoParsing("\n");
	}
}

/*
 *  Converts a string to a finite number (such as float or double).
 *  Returns the number or quiet_NaN, if it could not be parsed.
 *  This function does not attemp to capture exceptions that may
 *  be thrown from std::stod(...)
 */
template<typename T>
T to_finite(const std::string& input) {
	// Defensively set NaN from the get-go
	T result = std::numeric_limits<T>::quiet_NaN();
	size_t bytes_read = 0;
	try {
		const double interim = std::stod(input, &bytes_read);
		if (!input.empty() && bytes_read == input.size())
			result = static_cast<T>(interim);
	}
	// Capture expected exceptions stod may throw
    catch (std::invalid_argument& e) { (void)e;  }
    catch (std::out_of_range& e) { (void)e; }
	return result;
}

/*
 *  Reads a floating point argument from command line, where:
 *  - name is a human description for the flag, ie: DELAY
 *  - flag is the command-line flag, ie: -d or -delay
 *  - default is the default value if the flag doesn't exist
 *  - value will be populated with the default or provided value
 *
 *  Returns:
 *    true if 'value' is set to the default or read from the arg.
 *    false if the argument was used but could not be parsed.
 */
bool AUTOTYPE::ReadDoubleArg(const std::string &name,
                             const char *flag,
                             const double &def_value,
                             const double &min_value,
                             const double &max_value,
                             double &value)
{
	bool result = false;
	std::string str_value;

	// Is the user trying to set this flag?
	if (cmd->FindString(flag, str_value, true)) {
		// Can the user's value be parsed?
		const double user_value = to_finite<double>(str_value);
#if defined(MACOSX) || defined(EMSCRIPTEN)
		if (isfinite(user_value)) { /* *sigh* Really, clang, really? */
#else
		if (std::isfinite(user_value)) {
#endif
			result = true;

			// Clamp the user's value if needed
			value = clamp(user_value, min_value, max_value);

			// Inform them if we had to clamp their value
			if (fabs(user_value - value) >
			    std::numeric_limits<double>::epsilon())
				WriteOut("AUTOTYPE: bounding %s value of %.2f "
				         "to %.2f\n",
				         name.c_str(), user_value, value);

		} else { // Otherwise we couldn't parse their value
			WriteOut("AUTOTYPE: %s value '%s' is not a valid "
			         "floating point number\n",
			         name.c_str(), str_value.c_str());
		}
	} else { // Otherwise thay haven't passed this flag
		value = def_value;
		result = true;
	}
	return result;
}

void AUTOTYPE::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (!cmd->GetCount()||(cmd->GetCount()==1 && (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)))) {
		PrintUsage();
		return;
	}

	// Print available keys
	if (cmd->FindExist("-list", false)) {
		PrintKeys();
		return;
	}

	// Get the wait delay in milliseconds
	double wait_s;
	constexpr double def_wait_s = 2.0;
	constexpr double min_wait_s = 0.0;
	constexpr double max_wait_s = 30.0;
	if (!ReadDoubleArg("WAIT", "-w", def_wait_s, min_wait_s, max_wait_s, wait_s))
		return;
	const auto wait_ms = static_cast<uint32_t>(wait_s * 1000);

	// Get the inter-key pacing in milliseconds
	double pace_s;
	constexpr double def_pace_s = 0.5;
	constexpr double min_pace_s = 0.0;
	constexpr double max_pace_s = 10.0;
	if (!ReadDoubleArg("PACE", "-p", def_pace_s, min_pace_s, max_pace_s, pace_s))
		return;
	const auto pace_ms = static_cast<uint32_t>(pace_s * 1000);

	// Get the button sequence
	std::vector<std::string> sequence;
	cmd->FillVector(sequence);
	if (sequence.empty()) {
		WriteOut_NoParsing("AUTOTYPE: button sequence is empty\n");
		return;
	}
	MAPPER_AutoType(sequence, wait_ms, pace_ms);
}

void AUTOTYPE_ProgramStart(Program **make)
{
	*make = new AUTOTYPE;
}

class ADDKEY : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
            "Generates artificial keypresses.\n\nADDKEY [pmsec] [key]\n\n"
            "For example, the command below types \"dir\" followed by ENTER after 1 second:\n\nADDKEY p1000 d i r enter\n\n"
            "You could also try AUTOTYPE command instead of this command to perform\nscripted keyboard entry into a running DOS program.\n";
        WriteOut(msg);
	}
};

void ADDKEY::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
	DOS_Shell temp;
	temp.CMD_ADDKEY(args);
}

static void ADDKEY_ProgramStart(Program * * make) {
    *make=new ADDKEY;
}

class LS : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
            "Lists directory contents.\n\nLS [drive:][path][filename] [/A] [/L] [/P] [/Z]\n\n"
            "  /A\tLists hidden and system files also.\n"
            "  /L\tLists names one per line.\n"
            "  /P\tPauses after each screenful of information.\n"
            "  /Z\tDisplays short names even if LFN support is available.\n";
        WriteOut(msg);
	}
};

void LS::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
	DOS_Shell temp;
	temp.CMD_LS(args);
}

static void LS_ProgramStart(Program * * make) {
    *make=new LS;
}

class DELTREE : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
           "Deletes a directory and all the subdirectories and files in it.\n\n"
           "To delete one or more files and directories:\n"
           "DELTREE [/Y] [drive:]path [[drive:]path[...]]\n\n"
           "  /Y              Suppresses prompting to confirm you want to delete\n"
           "                  the subdirectory.\n"
           "  [drive:]path    Specifies the name of the directory you want to delete.\n\n"
           "Note: Use DELTREE cautiously. Every file and subdirectory within the\n"
           "specified directory will be deleted.\n";
        WriteOut(msg);
	}
};

void DELTREE::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
	DOS_Shell temp;
	temp.CMD_DELTREE(args);
}

static void DELTREE_ProgramStart(Program * * make) {
    *make=new DELTREE;
}

class TREE : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
           "Graphically displays the directory structure of a drive or path.\n\n"
           "TREE [drive:][path] [/F] [/A]\n\n"
           "  /F   Displays the names of the files in each directory.\n"
           "  /A   Uses ASCII instead of extended characters.\n";
        WriteOut(msg);
	}
};

void TREE::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
	DOS_Shell temp;
	temp.CMD_TREE(args);
}

static void TREE_ProgramStart(Program * * make) {
    *make=new TREE;
}

class COLOR : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
           "Sets the default console foreground and background colors.\n\n"
           "COLOR [attr]\n\n"
           "  attr        Specifies color attribute of console output\n\n"
           "Color attributes are specified by TWO hex digits -- the first\n"
           "corresponds to the background; the second to the foreground.\n"
           "Each digit can be any of the following values:\n\n"
           "    0 = Black       8 = Gray\n"
           "    1 = Blue        9 = Light Blue\n"
           "    2 = Green       A = Light Green\n"
           "    3 = Aqua        B = Light Aqua\n"
           "    4 = Red         C = Light Red\n"
           "    5 = Purple      D = Light Purple\n"
           "    6 = Yellow      E = Light Yellow\n"
           "    7 = White       F = Bright White\n\n"
           "If no argument is given, this command restores the original color.\n\n"
           "Example: \"COLOR fc\" produces light red on bright white\n";
        WriteOut(msg);
	}
};

void COLOR::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
    bool back=false;
    char fg, bg;
    int fgc=0, bgc=0;
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
    if (strlen(args)==2) {
        bg=args[0];
        fg=args[1];
        if (fg=='0'||fg=='8')
            fgc=30;
        else if (fg=='1'||fg=='9')
            fgc=34;
        else if (fg=='2'||tolower(fg)=='a')
            fgc=32;
        else if (fg=='3'||tolower(fg)=='b')
            fgc=36;
        else if (fg=='4'||tolower(fg)=='c')
            fgc=31;
        else if (fg=='5'||tolower(fg)=='d')
            fgc=35;
        else if (fg=='6'||tolower(fg)=='e')
            fgc=32;
        else if (fg=='7'||tolower(fg)=='f')
            fgc=37;
        else
            back=true;
        if (bg=='0'||bg=='8')
            bgc=40;
        else if (bg=='1'||bg=='9')
            bgc=44;
        else if (bg=='2'||tolower(bg)=='a')
            bgc=42;
        else if (bg=='3'||tolower(bg)=='b')
            bgc=46;
        else if (bg=='4'||tolower(bg)=='c')
            bgc=41;
        else if (bg=='5'||tolower(bg)=='d')
            bgc=45;
        else if (bg=='6'||tolower(bg)=='e')
            bgc=42;
        else if (bg=='7'||tolower(bg)=='f')
            bgc=47;
        else
            back=true;
    } else
       back=true;
    if (back)
        WriteOut("[0m");
    else {
        bool fgl=fg>='0'&&fg<='7', bgl=bg>='0'&&bg<='7';
        WriteOut(("["+std::string(fgl||bgl?"0;":"")+std::string(fgl?"":"1;")+std::string(bgl?"":"5;")+std::to_string(fgc)+";"+std::to_string(bgc)+"m").c_str());
    }
}

static void COLOR_ProgramStart(Program * * make) {
    *make=new COLOR;
}

#if defined(USE_TTF)
typedef struct {uint8_t red; uint8_t green; uint8_t blue; uint8_t alpha;} alt_rgb;
alt_rgb altBGR[16], *rgbcolors = (alt_rgb*)render.pal.rgb;
extern alt_rgb altBGR1[16];
extern bool colorChanged;
bool setColors(const char *colorArray, int n);
void resetFontSize();

class SETCOLOR : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
            "Views or changes the text-mode color scheme settings.\n\nSETCOLOR [color# [value]]\n\nFor example:\n\n  SETCOLOR 1 (50,50,50)\n\nChange Color #1 to the specified color value\n\n  SETCOLOR 7 -\n\nReturn Color #7 to the default color value\n\n  SETCOLOR MONO\n\nDisplay current MONO mode status\n\nTo change the current background and foreground colors, use COLOR command.\n";
        WriteOut(msg);
	}
};

void SETCOLOR::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	if (*args) {
		args=trim(args);
		char *p = strchr(args, ' ');
		if (p!=NULL)
			*p=0;
		int i=atoi(args);
		if (!strcasecmp(args,"MONO")) {
			if (p==NULL)
				WriteOut("MONO mode status: %s (video mode %d)\n",CurMode->mode==7?"active":CurMode->mode==3?"inactive":"unavailable",CurMode->mode);
			else if (!strcmp(trim(p+1),"+")) {
				if (CurMode->mode!=7) INT10_SetVideoMode(7);
				WriteOut(CurMode->mode==7?"MONO mode status => active (video mode 7)\n":"Failed to change MONO mode\n");
			} else if (!strcmp(trim(p+1),"-")) {
				if (CurMode->mode!=3) INT10_SetVideoMode(3);
				WriteOut(CurMode->mode==3?"MONO mode status => inactive (video mode 3)\n":"Failed to change MONO mode\n");
			} else
				WriteOut("Must be + or - for MONO: %s\n",trim(p+1));
		} else if (!strcmp(args,"0")||!strcmp(args,"00")||!strcmp(args,"+0")||!strcmp(args,"-0")||i>0&&i<16) {
			if (p==NULL) {
                altBGR[i].red = colorChanged&&!IS_VGA_ARCH?altBGR1[i].red:rgbcolors[i].red;
                altBGR[i].green = colorChanged&&!IS_VGA_ARCH?altBGR1[i].green:rgbcolors[i].green;
                altBGR[i].blue = colorChanged&&!IS_VGA_ARCH?altBGR1[i].blue:rgbcolors[i].blue;
                WriteOut("Color %d: (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR[i].red,altBGR[i].green,altBGR[i].blue,altBGR[i].red,altBGR[i].green,altBGR[i].blue);
            }
		} else {
			WriteOut("Invalid color number - %s\n", trim(args));
			DOS_SetError(DOSERR_DATA_INVALID);
			return;
		} if (p!=NULL&&strcasecmp(args,"MONO")) {
			char value[128];
			if (strcmp(trim(p+1),"-")) {
				strncpy(value,trim(p+1),127);
				value[127]=0;
			} else
				strcpy(value,i==0?"#000000":i==1?"#0000aa":i==2?"#00aa00":i==3?"#00aaaa":i==4?"#aa0000":i==5?"#aa00aa":i==6?"#aa5500":i==7?"#aaaaaa":i==8?"#555555":i==9?"#5555ff":i==10?"#55ff55":i==11?"#55ffff":i==12?"#ff5555":i==13?"#ff55ff":i==14?"#ffff55":"#ffffff");
            if (!ttf.inUse)
				WriteOut("Changing color scheme is only supported for the TrueType font output.\n");
			else if (setColors(value,i)) {
                altBGR[i].red = colorChanged&&!IS_VGA_ARCH?altBGR1[i].red:rgbcolors[i].red;
                altBGR[i].green = colorChanged&&!IS_VGA_ARCH?altBGR1[i].green:rgbcolors[i].green;
                altBGR[i].blue = colorChanged&&!IS_VGA_ARCH?altBGR1[i].blue:rgbcolors[i].blue;
				WriteOut("Color %d => (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR[i].red,altBGR[i].green,altBGR[i].blue,altBGR[i].red,altBGR[i].green,altBGR[i].blue);
				resetFontSize();
			} else
				WriteOut("Invalid color value - %s\n",value);
			}
	} else {
		WriteOut("MONO mode status: %s (video mode %d)\n",CurMode->mode==7?"active":CurMode->mode==3?"inactive":"unavailable",CurMode->mode);
		for (int i = 0; i < 16; i++) {
            altBGR[i].red = colorChanged&&!IS_VGA_ARCH?altBGR1[i].red:rgbcolors[i].red;
            altBGR[i].green = colorChanged&&!IS_VGA_ARCH?altBGR1[i].green:rgbcolors[i].green;
            altBGR[i].blue = colorChanged&&!IS_VGA_ARCH?altBGR1[i].blue:rgbcolors[i].blue;
			WriteOut("Color %d: (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR[i].red,altBGR[i].green,altBGR[i].blue,altBGR[i].red,altBGR[i].green,altBGR[i].blue);
        }
	}
}

static void SETCOLOR_ProgramStart(Program * * make) {
    *make=new SETCOLOR;
}
#endif

#if C_DEBUG
extern Bitu int2fdbg_hook_callback;
class INT2FDBG : public Program {
public:
    void Run(void);
private:
	void PrintUsage() {
        constexpr const char *msg =
            "Hooks INT 2Fh for debugging purposes.\n\nINT2FDBG [option]\n  /I      Installs hook\n\nIt will hook INT 2Fh at the top of the call chain for debugging information.\n\nType INT2FDBG without a parameter to show the current hook status.\n";
        WriteOut(msg);
	}
};

void INT2FDBG::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

    if (!cmd->GetCount()) {
        if (int2fdbg_hook_callback == 0)
            WriteOut("INT 2Fh hook has not been set.\n");
        else
            WriteOut("INT 2Fh hook has already been set.\n");
        return;
    }

	// Usage
	if (!cmd->GetCount() || cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
		PrintUsage();
		return;
	}
	char *args=(char *)cmd->GetRawCmdline().c_str();
	args=trim(args);
	DOS_Shell temp;
	temp.CMD_INT2FDBG(args);
}

static void INT2FDBG_ProgramStart(Program * * make) {
    *make=new INT2FDBG;
}
#endif

#if defined (WIN32) && !defined(HX_DOS)
#include <sstream>
#include <shellapi.h>
extern bool ctrlbrk;
extern std::string startincon;
SHELLEXECUTEINFO lpExecInfo;

void EndStartProcess() {
    if(lpExecInfo.hProcess!=NULL) {
        DWORD exitCode;
        GetExitCodeProcess(lpExecInfo.hProcess, &exitCode);
        if (exitCode==STILL_ACTIVE)
            TerminateProcess(lpExecInfo.hProcess, 0);
    }
    ctrlbrk=false;
}

class START : public Program {
public:
    void Run() {
        if(control->SecureMode()) {
            WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
            return;
        }

        // Hack To allow long commandlines
        ChangeToLongCmd();

        // Usage
        if (!cmd->GetCount()||(cmd->GetCount()==1 && (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)))) {
            PrintUsage();
            return;
        }
        char *args=(char *)cmd->GetRawCmdline().c_str();
        args=trim(args);
        char *cmd = strlen(args)?args:NULL;
        if (cmd!=NULL&&strlen(cmd)>1&&cmd[0]=='"'&&cmd[1]==' ') {
            cmd++;
            while (cmd[0]==' ') cmd++;
            cmd--;
            cmd[0]='"';
        }
        char *cmdstr = cmd==NULL?NULL:(char *)strstr(cmd, cmd[0]=='"'?"\" ":" ");
        char buf[CROSS_LEN], dir[CROSS_LEN+15], str[CROSS_LEN*2];
        int k=0;
        if (cmdstr!=NULL) {
            if (*cmdstr=='\"') cmdstr++;
            while (*cmdstr==' ') {k++;*cmdstr++=0;}
        }
        int state = cmd==NULL?0:!strcmp(cmd,"-")||!strcasecmp(cmd,"/min")||!strcasecmp(cmd,"-min")?1:!strcmp(cmd,"+")||!strcasecmp(cmd,"/max")||!strcasecmp(cmd,"-max")?2:!strcasecmp(cmd,"_")||!strcasecmp(cmd,"/hid")||!strcasecmp(cmd,"-hid")?3:0;
        if (state > 0) {
            k=0;
            cmd = cmdstr;
            if (cmd!=NULL&&strlen(cmd)>1&&cmd[0]=='"'&&cmd[1]==' ') {
                cmd++;
                while (cmd[0]==' ') cmd++;
                cmd--;
                cmd[0]='"';
            }
            if ((cmdstr = cmd==NULL?NULL:(char *)strstr(cmd, cmd[0]=='"'?"\" ":" "))!=NULL) {
                if (*cmdstr=='\"') cmdstr++;
                while (*cmdstr==' ') {k++;*cmdstr++=0;}
            }
        }
        if (cmd!=NULL) {
            char *ret, *ret0, *ret1, *ret2, *ret3, *ret4;
            ret0 = strchr(cmd, '/');
            ret1 = strchr(cmd, '|');
            ret2 = strchr(cmd, '<');
            ret3 = strchr(cmd, '>');
            ret4 = strchr(cmd, ' ');
            ret = ret0>cmd?ret0:NULL;
            if (ret1!=NULL && (ret == NULL || ret1<ret0)) ret=ret1;
            if (ret2!=NULL && (ret == NULL || ret2<ret0)) ret=ret2;
            if (ret3!=NULL && (ret == NULL || ret3<ret0)) ret=ret3;
            if (ret4!=NULL && ret!=NULL && ret4<ret) ret=ret4;
            if (ret!=NULL&&!(ret==ret0&&ret>cmd&&*(ret-1)==':')) {
                strcpy(buf, cmdstr==NULL?"":cmdstr);
                strcpy(str, ret);
                if (k<1) k=1;
                for (int i=0; i<k; i++) strcat(str, " ");
                strcat(str, buf);
                cmdstr=str;
                *ret='\0';
                if (*cmd=='"'&&strlen(cmdstr)>0&&cmdstr[strlen(cmdstr)-2]=='"') {
                    cmd++;
                    cmdstr[strlen(cmdstr)-2]='\0';
                }
            }
        }
        if (cmd==NULL || !strlen(cmd) || !strcmp(cmd,"?") || !strcmp(cmd,"/") || !strcmp(cmd,"/?") || !strcmp(cmd,"-?")) {
            PrintUsage();
            DOS_SetError(0);
            return;
        }
        int sw = state==0?SW_SHOW:state==1?SW_MINIMIZE:state==2?SW_MAXIMIZE:SW_HIDE;
        bool match=false;
        std::istringstream in(startincon);
        if (in)	for (std::string command; in >> command; ) {
            if (!strcasecmp(cmd,command.c_str())||!strcasecmp(cmd,("\""+command+"\"").c_str())) {
                match=true;
                break;
            }
        }
        lpExecInfo.cbSize  = sizeof(SHELLEXECUTEINFO);
        lpExecInfo.fMask=SEE_MASK_DOENVSUBST|SEE_MASK_NOCLOSEPROCESS;
        lpExecInfo.hwnd = NULL;
        lpExecInfo.lpVerb = "open";
        lpExecInfo.lpDirectory = NULL;
        lpExecInfo.nShow = sw;
        lpExecInfo.hInstApp = (HINSTANCE) SE_ERR_DDEFAIL;
        if (match) {
            strcpy(dir, strcasecmp(cmd,"for")?"/C \"":"/C \"(");
            strcat(dir, cmd);
            strcat(dir, " ");
            if (cmdstr!=NULL) strcat(dir, cmdstr);
            if (!strcasecmp(cmd,"for")) strcat(dir, ")");
            strcat(dir, " & echo( & echo The command execution is completed. & pause\"");
            lpExecInfo.lpFile = "CMD.EXE";
            lpExecInfo.lpParameters = dir;
        } else {
            lpExecInfo.lpFile = cmd;
            lpExecInfo.lpParameters = cmdstr;
        }
        bool setdir=false;
        char winDirCur[512], winDirNew[512];
        if (GetCurrentDirectory(512, winDirCur)&&(!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"local ",6)||!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"CDRom ",6))) {
            Overlay_Drive *ddp = dynamic_cast<Overlay_Drive*>(Drives[DOS_GetDefaultDrive()]);
            strcpy(winDirNew, ddp!=NULL?ddp->getOverlaydir():Drives[DOS_GetDefaultDrive()]->GetBaseDir());
            strcat(winDirNew, Drives[DOS_GetDefaultDrive()]->curdir);
            if (SetCurrentDirectory(winDirNew)) setdir=true;
        }
        if (!startquiet) WriteOut("Starting %s...\n", cmd);
        ShellExecuteEx(&lpExecInfo);
        int ErrorCode = GetLastError();
        if (setdir) SetCurrentDirectory(winDirCur);
        if (startwait && lpExecInfo.hProcess!=NULL) {
            DWORD exitCode;
            BOOL ret;
            int count=0;
            ctrlbrk=false;
            do {
                ret=GetExitCodeProcess(lpExecInfo.hProcess, &exitCode);
                CALLBACK_Idle();
                if (ctrlbrk) {
                    uint8_t c;uint16_t n=1;
                    DOS_ReadFile (STDIN,&c,&n);
                    if (c == 3) WriteOut("^C\n");
                    EndStartProcess();
                    exitCode=0;
                    break;
                }
                if (++count==20000&&ret&&exitCode==STILL_ACTIVE&&!startquiet) WriteOut("(Press Ctrl+C to exit immediately)\n");
            } while (ret!=0&&exitCode==STILL_ACTIVE);
            ErrorCode = GetLastError();
            CloseHandle(lpExecInfo.hProcess);
        }
        DOS_SetError(ErrorCode);
    }

private:
    void PrintUsage() {
        constexpr const char *msg =
            "Starts a separate window to run a specified program or command.\n\n"
            "START [+|-|_] command [arguments]\n\n"
            "  [+|-|_]: To maximize/minimize/hide the program.\n"
            "  The options /MAX, /MIN, /HID are also accepted.\n"
            "  command: The command, program or file to start.\n"
            "  arguments: Arguments to pass to the application.\n\n"
            "START opens the Windows command prompt automatically to run these commands\n"
            "and wait for a key press before exiting (specified by \"startincon\" option):\n"
            "%s\n\nNote: The path specified in this command is the path on the Windows host.\n";
        WriteOut(msg, startincon.c_str());
    }
};

void START_ProgramStart(Program **make)
{
	*make = new START;
}
#endif

#define MAX_FLAGS 512
char *g_flagged_files[MAX_FLAGS]; //global array to hold flagged files
int my_minizip(char ** savefile, char ** savefile2, char* savename);
int my_miniunz(char ** savefile, const char * savefile2, const char * savedir, char* savename);
int flagged_backup(char *zip)
{
    char zipfile[CROSS_LEN], ziptmp[CROSS_LEN+4];
    int i;
    int ret = 0;

    strcpy(zipfile, zip);
    strcpy(ziptmp, zip);
    if (strstr(zipfile, ".sav")) {
        strcpy(strstr(zipfile, ".sav"), ".dat");
        strcpy(strstr(ziptmp, ".sav"), ".tmp");
    } else
        strcat(ziptmp, ".tmp");
    bool first=true;
    for (i = 0; i < MAX_FLAGS; i++)
    {
        if (g_flagged_files[i])
        {
            if (first) {
                first=false;
                std::ofstream file (zipfile);
                file << "";
                file.close();
            }
            uint16_t handle;
            if (DOS_FindDevice(("\""+std::string(g_flagged_files[i])+"\"").c_str()) != DOS_DEVICES || !DOS_OpenFile(("\""+std::string(g_flagged_files[i])+"\"").c_str(),0,&handle)) {
                LOG_MSG(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),g_flagged_files[i]);
                continue;
            }
            uint8_t c;uint16_t n=1;
            std::string out="";
            while (n) {
                DOS_ReadFile(handle,&c,&n);
                if (n==0) break;
                out+=std::string(1, c);
            }
            DOS_CloseFile(handle);
            std::ofstream outfile (ziptmp, std::ofstream::binary);
            outfile << out;
            outfile.close();
            my_minizip((char**)zipfile, (char**)ziptmp, g_flagged_files[i]);
            ret++;
        }
    }
    remove(ziptmp);
    return ret;
}

int flagged_restore(char* zip)
{
    char zipfile[MAX_FLAGS], ziptmp[CROSS_LEN+4];
    int i;
    int ret = 0;

    strcpy(zipfile, zip);
    strcpy(ziptmp, zip);
    if (strstr(zipfile, ".sav")) {
        strcpy(strstr(zipfile, ".sav"), ".dat");
        strcpy(strstr(ziptmp, ".sav"), ".tmp");
    } else
        strcat(ziptmp, ".tmp");
    for (i = 0; i < MAX_FLAGS; i++)
    {
        if (g_flagged_files[i])
        {
            if (DOS_FindDevice(("\""+std::string(g_flagged_files[i])+"\"").c_str()) != DOS_DEVICES) {
                LOG_MSG(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),g_flagged_files[i]);
                continue;
            }
            char savedir[CROSS_LEN], savename[CROSS_LEN];
            char *p=strrchr(ziptmp, CROSS_FILESPLIT);
            if (p==NULL) {
                strcpy(savedir, ".");
                strcpy(savename, ziptmp);
            } else {
                strcpy(savename, p+1);
                *p=0;
                strcpy(savedir, ziptmp);
                *p=CROSS_FILESPLIT;
            }
            my_miniunz((char**)zipfile, g_flagged_files[i], savedir, savename);
            std::ifstream ifs(ziptmp, std::ios::in | std::ios::binary | std::ios::ate);
            std::ifstream::pos_type fileSize = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            std::vector<char> bytes(fileSize);
            ifs.read(bytes.data(), fileSize);
            std::string str(bytes.data(), fileSize);
            uint16_t handle, size;
            if (DOS_CreateFile(("\""+std::string(g_flagged_files[i])+"\"").c_str(),0,&handle)) {
                for (uint64_t i=0; i<=ceil(fileSize/UINT16_MAX); i++) {
                    size=(uint64_t)fileSize-UINT16_MAX*i>UINT16_MAX?UINT16_MAX:(uint16_t)((uint64_t)fileSize-UINT16_MAX*i);
                    DOS_WriteFile(handle,(uint8_t *)str.substr(i*UINT16_MAX, size).c_str(),&size);
                }
                DOS_CloseFile(handle);
            }
            ret++;
        }
    }
    remove(ziptmp);
    return ret;
}

class FLAGSAVE : public Program
{
public:

    void Run(void)
    {
        std::string file_to_flag;
        int i, lf;
        bool force=false, remove=false;

        if (cmd->FindExist("-?", false) || cmd->FindExist("/?", false)) {
            printHelp();
            return;
        }
        if (cmd->FindExist("/f", true))
            force=true;
        if (cmd->FindExist("/r", true))
            remove=true;
        if (cmd->FindExist("/u", true))
        {
            for (i = 0; i < MAX_FLAGS; i++)
            {
                if (g_flagged_files[i] != NULL)
                    g_flagged_files[i] = NULL;
            }
            WriteOut("All files unflagged for saving.\n");
            return;
        }
        else if (cmd->GetCount())
        {
            for (unsigned int i=1; i<=cmd->GetCount(); i++) {
                cmd->FindCommand(i,temp_line);
                uint8_t drive;
                char fullname[DOS_PATHLENGTH], flagfile[CROSS_LEN];

                strcpy(flagfile, temp_line.c_str());
                if (*flagfile&&DOS_MakeName(((flagfile[0]!='\"'?"\"":"")+std::string(flagfile)+(flagfile[strlen(flagfile)-1]!='\"'?"\"":"")).c_str(), fullname, &drive))
                {
                    sprintf(flagfile, "%c:\\%s", drive+'A', fullname);
                    if (remove) {
                        for (lf = 0; lf < MAX_FLAGS; lf++)
                        {
                            if (g_flagged_files[lf] != NULL && !strcasecmp(g_flagged_files[lf], flagfile))
                            {
                                WriteOut("File %s unflagged for saving.\n", g_flagged_files[lf]);
                                free(g_flagged_files[lf]);
                                g_flagged_files[lf] = NULL;
                                break;
                            }
                        }
                        continue;
                    }
                    if (!force && !DOS_FileExists(("\""+std::string(flagfile)+"\"").c_str())) {
                        WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"), flagfile);
                        continue;
                    }
                    bool found=false;
                    for (lf = 0; lf < MAX_FLAGS; lf++)
                    {
                        if (g_flagged_files[lf] == NULL)
                            continue;
                        if (!strcasecmp(g_flagged_files[lf], flagfile))
                        {
                            WriteOut("File already flagged for saving - %s\n", flagfile);
                            found=true;
                        }
                    }
                    if (found) continue;
                    for (lf = 0; lf < MAX_FLAGS; lf++)
                    {
                        if (g_flagged_files[lf] == NULL)
                            break;
                    }
                    if (lf == MAX_FLAGS)
                    {
                        WriteOut("Too many files to flag for saving.\n");
                        return;
                    }
                    g_flagged_files[lf] = (char*)malloc(strlen(flagfile) + 1);
                    strcpy(g_flagged_files[lf], flagfile);
                    WriteOut("File %s flagged for saving\n", g_flagged_files[lf]);
                } else
                    WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"), flagfile);
            }
            return;
        }
        else
        {
            WriteOut("Files flagged for saving:\n");
            for (i = 0; i < MAX_FLAGS; i++)
            {
                if (g_flagged_files[i])
                    WriteOut("%s\n", g_flagged_files[i]);
            }
            return;
        }
        return;
    }
    void printHelp()
    {
        WriteOut( "Marks or flags files to be saved for the save state feature.\n\n"
                "FLAGSAVE [file(s) [/F] [/R]] [/U]\n\n"
                "  file(s)     Specifies one or more files to be flagged for saving.\n"
                "  /F          Forces to flag the file(s) even if they are not found.\n"
                "  /R          Removes flags from the specified file(s).\n"
                "  /U          Removes flags from all flagged files.\n\n"
                "Type FLAGSAVE without a parameter to list flagged files.\n");
    }
};

static void FLAGSAVE_ProgramStart(Program** make)
{
    *make = new FLAGSAVE;
}

void DOS_SetupPrograms(void) {
    /*Add Messages */

    MSG_Add("PROGRAM_MOUSE_INSTALL","Installed at PS/2 port.\n");
    MSG_Add("PROGRAM_MOUSE_VERTICAL","Reverse Y-axis enabled.\n");
    MSG_Add("PROGRAM_MOUSE_VERTICAL_BACK","Reverse Y-axis disabled.\n");
    MSG_Add("PROGRAM_MOUSE_UNINSTALL","Driver successfully unloaded...\n");
    MSG_Add("PROGRAM_MOUSE_ERROR","Already installed at PS/2 port.\n");
    MSG_Add("PROGRAM_MOUSE_NOINSTALLED","Driver is not installed.\n");
    MSG_Add("PROGRAM_MOUSE_HELP","Turns on/off mouse.\n\nMOUSE [/?] [/U] [/V]\n  /U: Uninstall\n  /V: Reverse Y-axis\n");
    MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
    MSG_Add("PROGRAM_MOUNT_STATUS_FORMAT","%-5s  %-58s %-12s\n");
    MSG_Add("PROGRAM_MOUNT_STATUS_ELTORITO", "Drive %c is mounted as El Torito floppy drive\n");
    MSG_Add("PROGRAM_MOUNT_STATUS_RAMDRIVE", "Drive %c is mounted as RAM drive\n");
    MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
    MSG_Add("PROGRAM_MOUNT_STATUS_1","The currently mounted drives are:\n");
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_FORMAT","%-5s  %-47s  %-12s  %s\n");
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_NUMBER_FORMAT","%-12s  %-40s  %-12s  %s\n");
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_2","The currently mounted drive numbers are:\n");
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_1","The currently mounted FAT/ISO drives are:\n");
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_NONE","No drive available\n");
    MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s does not exist.\n");
    MSG_Add("PROGRAM_MOUNT_ERROR_2","%s is not a directory\n");
    MSG_Add("PROGRAM_MOUNT_IMGMOUNT","To mount image files, use the \033[34;1mIMGMOUNT\033[0m command, not the \033[34;1mMOUNT\033[0m command.\n");
    MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
    MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
    MSG_Add("PROGRAM_MOUNT_USAGE",
        "Mounts directories or drives in the host system as DOSBox-X drives.\n"
        "Usage: \033[34;1m\033[32;1mMOUNT\033[0m \033[37;1mdrive\033[0m \033[36;1mlocal_directory\033[0m [option]\033[0m\n"
        " \033[37;1mdrive\033[0m            Drive letter where the directory or drive will be mounted.\n"
        " \033[36;1mlocal_directory\033[0m  Local directory or drive in the host system to be mounted.\n"
        " [option]         Option(s) for mounting. The following options are accepted:\n"
        " -t               Specify the drive type the mounted drive to behave as.\n"
        "                  Supported drive type: dir, floppy, cdrom, overlay\n"
        " (Note that 'overlay' redirects writes for mounted drive to another directory)\n"
        " -label [name]    Set the volume label name of the drive (all upper case)\n"
        " -ro              Mount the drive in read-only mode.\n"
        " -cd              Generate a list of local CD drive's \"drive #\" values.\n"
        " -usecd [drive #] For direct hardware emulation such as audio playback.\n"
        " -ioctl           Use lowest level hardware access (following -usecd option).\n"
        " -aspi            Use the installed ASPI layer (following -usecd option).\n"
        " -freesize [size] Specify the free disk space of drive in MB (KB for floppies).\n"
        " -nocachedir      Enable real-time update and do not cache the drive.\n"
        " -z drive         Move virtual drive Z: to a different letter.\n"
        " -o               Report the drive as: local, remote.\n"
        " -q               Quiet mode (no message output).\n"
        " -u               Unmount the drive.\n"
        " \033[32;1m-examples        Show some usage examples.\033[0m\n"
        "Type MOUNT with no parameters to display a list of mounted drives.\n");
    MSG_Add("PROGRAM_MOUNT_EXAMPLE",
        "A basic example of MOUNT command:\n\n"
        "\033[32;1mMOUNT c %s\033[0m\n\n"
        "This makes the directory %s act as the C: drive inside DOSBox-X.\n"
        "The directory has to exist in the host system. If the directory contains\n"
        "space(s), be sure to properly quote the directory with double quotes,\n"
        "e.g. %s\n\n"
        "Some other usage examples of MOUNT:\n\n"
#if defined (WIN32) || defined(OS2)
        "\033[32;1mMOUNT\033[0m                             - list all mounted drives\n"
        "\033[32;1mMOUNT -cd\033[0m                         - list all local CD drives\n"
#else
        "\033[32;1mMOUNT\033[0m                            - list all mounted drives\n"
        "\033[32;1mMOUNT -cd\033[0m                        - list all local CD drives\n"
#endif
        "\033[32;1mMOUNT d %s\033[0m            - mount the D: drive to the directory\n"
        "\033[32;1mMOUNT c %s -t cdrom\033[0m      - mount the C: drive as a CD-ROM drive\n"
        "\033[32;1mMOUNT c %s -ro\033[0m           - mount the C: drive in read-only mode\n"
        "\033[32;1mMOUNT c %s -label TEST\033[0m   - mount the C: drive with the label TEST\n"
        "\033[32;1mMOUNT c %s -nocachedir \033[0m  - mount C: without caching the drive\n"
        "\033[32;1mMOUNT c %s -freesize 128\033[0m - mount C: with 128MB free disk space\n"
        "\033[32;1mMOUNT c %s -u\033[0m            - force mount C: drive even if it's mounted\n"
        "\033[32;1mMOUNT c %s -t overlay\033[0m  - mount C: with overlay directory on top\n"
#if defined (WIN32) || defined(OS2)
        "\033[32;1mMOUNT c -u\033[0m                        - unmount the C: drive\n"
#else
        "\033[32;1mMOUNT c -u\033[0m                       - unmount the C: drive\n"
#endif
        );
    MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c is not mounted.\n");
    MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
    MSG_Add("PROGRAM_MOUNT_UMOUNT_NUMBER_SUCCESS","Drive number %c has successfully been removed.\n");
    MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
    MSG_Add("PROGRAM_MOUNT_WARNING_WIN","Warning: Mounting C:\\ is not recommended.\n");
    MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","Warning: Mounting / is not recommended.\n");
	MSG_Add("PROGRAM_MOUNT_PHYSFS_ERROR","Failed to mount the PhysFS drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE","Please MOUNT a normal directory first before adding an overlay on top.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE","The overlay is NOT compatible with the drive that is specified.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE","The overlay needs to be specified using the same addressing as the underlying drive. No mixing of relative and absolute paths.");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE","The overlay directory can not be the same as underlying drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_ERROR","An error occurred when trying to create an overlay drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS","Overlay %s on drive %c mounted.\n");

    MSG_Add("PROGRAM_LOADFIX_ALLOC","%d kb allocated.\n");
    MSG_Add("PROGRAM_LOADFIX_DEALLOC","%d kb freed.\n");
    MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
    MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");
    MSG_Add("PROGRAM_LOADFIX_HELP",
        "Reduces the amount of available conventional or XMS memory.\n\n"
        "LOADFIX [-xms] [-ems] [-{ram}] [{program}] [{options}]\n"
        "LOADFIX -f [-xms] [-ems]\n\n"
        "  -xms        Allocates memory from XMS rather than conventional memory\n"
        "  -ems        Allocates memory from EMS rather than conventional memory\n"
        "  -{ram}      Specifies the amount of memory to allocate in KB\n"
        "                 Defaults to 64kb for conventional memory; 1MB for XMS memory\n"
        "  -a          Auto allocates enough memory to fill the lowest 64KB memory\n"
        "  -f (or -d)  Frees previously allocated memory\n"
        "  {program}   Runs the specified program\n"
        "  {options}   Program options (if any)\n\n"
        "Examples:\n"
        "  \033[32;1mLOADFIX game.exe\033[0m     Allocates 64KB of conventional memory and runs game.exe\n"
        "  \033[32;1mLOADFIX -a\033[0m           Auto-allocates enough memory conventional memory\n"
        "  \033[32;1mLOADFIX -128\033[0m         Allocates 128KB of conventional memory\n"
        "  \033[32;1mLOADFIX -xms\033[0m         Allocates 1MB of XMS memory\n"
        "  \033[32;1mLOADFIX -f\033[0m           Frees allocated conventional memory\n");

    MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
    MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CD-ROM drives have to be continuous.\n");
    MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
    MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Specified location is not a CD-ROM drive.\n");
    MSG_Add("MSCDEX_ERROR_OPEN","MSCDEX: Failure: Invalid file or unable to open.\n");
    MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CD-ROM drives (max: 5). MSCDEX Installation failed.\n");
    MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
    MSG_Add("MSCDEX_INVALID_FILEFORMAT","MSCDEX: Failure: File is either no ISO/CUE image or contains errors.\n");
    MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");

    MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

    MSG_Add("PROGRAM_INTRO",
        "\033[2J\033[32;1mWelcome to DOSBox-X\033[0m, an open-source x86 emulator with sound and graphics.\n"
        "DOSBox-X creates a shell for you which looks just like the plain DOS.\n"
        "\n"
        "\033[31;1mDOSBox-X will stop/exit without a warning if an error occurred!\033[0m\n"
        "\n"
        "\n" );
    if (machine == MCH_PC98) {
        MSG_Add("PROGRAM_INTRO_MENU_UP",
            "\033[44m\033[K\033[0m\n"
            "\033[44m\033[K\033[1m\033[1m\t\t\t\t\t\t\t DOSBox-X Introduction\033[0m\n"
            "\033[44m\033[K\033[1m\033[1m \x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\033[0m\n"
            "\033[44m\033[K\033[0m\n"
            );
    } else {
        MSG_Add("PROGRAM_INTRO_MENU_UP",
            "\033[44m\033[K\033[0m\n"
            "\033[44m\033[K\033[1m\033[1m\t\t\t\t\t\t\t DOSBox-X Introduction \033[0m\n"
            "\033[44m\033[K\033[1m\033[1m \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\033[0m\n"
            "\033[44m\033[K\033[0m\n"
            );
    }
    MSG_Add("PROGRAM_INTRO_MENU_BASIC","Basic mount");
    MSG_Add("PROGRAM_INTRO_MENU_CDROM","CD-ROM support");
    MSG_Add("PROGRAM_INTRO_MENU_USAGE","Usage");
    MSG_Add("PROGRAM_INTRO_MENU_INFO","Information");
    MSG_Add("PROGRAM_INTRO_MENU_QUIT","Quit");
    MSG_Add("PROGRAM_INTRO_MENU_BASIC_HELP","\n\033[1m   \033[1m\033[KMOUNT allows you to connect real hardware to DOSBox-X's emulated PC.\033[0m\n");
    MSG_Add("PROGRAM_INTRO_MENU_CDROM_HELP","\n\033[1m   \033[1m\033[KTo mount your CD-ROM in DOSBox-X, you need to specify additional options\n   when mounting the CD-ROM.\033[0m\n");
    MSG_Add("PROGRAM_INTRO_MENU_USAGE_HELP","\n\033[1m   \033[1m\033[KAn overview of the command line options you can give to DOSBox-X.\033[0m\n");
    MSG_Add("PROGRAM_INTRO_MENU_INFO_HELP","\n\033[1m   \033[1m\033[KHow to get more information about DOSBox-X.\033[0m\n");
    MSG_Add("PROGRAM_INTRO_MENU_QUIT_HELP","\n\033[1m   \033[1m\033[KExit from Intro.\033[0m\n");
    MSG_Add("PROGRAM_INTRO_USAGE_TOP",
        "\033[2J\033[32;1mAn overview of the command line options you can give to DOSBox-X.\033[0m\n"
        "Windows users must open cmd.exe or edit the shortcut to DOSBox-X.exe for this.\n\n"
        "dosbox-x [name] [-exit] [-version] [-fastlaunch] [-fullscreen]\n"
        "         [-conf congfigfile] [-lang languagefile] [-machine machinetype]\n"
        "         [-startmapper] [-noautoexec] [-scaler scaler | -forcescaler scaler]\n"
        "         [-noconsole] [-c command] [-set <section property=value>]\n\n"
        );
    MSG_Add("PROGRAM_INTRO_USAGE_1",
        "\033[33;1m  name\033[0m\n"
        "\tIf name is a directory it will mount that as the C: drive.\n"
        "\tIf name is an executable it will mount the directory of name\n"
        "\tas the C: drive and execute name.\n\n"
        "\033[33;1m  -exit\033[0m\n"
        "\tDOSBox-X will close itself when the DOS application name ends.\n\n"
        "\033[33;1m  -version\033[0m\n"
        "\tOutputs version information and exit. Useful for frontends.\n\n"
        "\033[33;1m  -fastlaunch\033[0m\n"
        "\tEnables fast launch mode (skip BIOS logo and welcome banner).\n\n"
        "\033[33;1m  -fullscreen\033[0m\n"
        "\tStarts DOSBox-X in fullscreen mode.\n"
        );
    MSG_Add("PROGRAM_INTRO_USAGE_2",
        "\033[33;1m  -conf\033[0m configfile\n"
        "\tStart DOSBox-X with the options specified in configfile.\n"
        "\tSee the documentation for more details.\n\n"
        "\033[33;1m  -lang\033[0m languagefile\n"
        "\tStart DOSBox-X using the language specified in languagefile.\n\n"
        "\033[33;1m  -noconsole\033[0m (Windows Only)\n"
        "\tStart DOSBox-X without showing the console window. Output will\n"
        "\tbe redirected to stdout.txt and stderr.txt\n\n"
        "\033[33;1m  -machine\033[0m machinetype\n"
        "\tSetup DOSBox-X to emulate a specific type of machine. Valid choices:\n"
        "\thercules, cga, cga_mono, mcga, mda, pcjr, tandy, ega, vga, vgaonly,\n"
        "\tpc98, vesa_nolfb, vesa_oldvbe, svga_paradise, svga_s3 (default).\n"
        "\tThe machinetype affects both the video card and available sound cards.\n"
        );
    MSG_Add("PROGRAM_INTRO_USAGE_3",
        "\033[33;1m  -startmapper\033[0m\n"
        "\tEnter the keymapper directly on startup. Useful for people with\n"
        "\tkeyboard problems.\n\n"
        "\033[33;1m  -noautoexec\033[0m\n"
        "\tSkips the [autoexec] section of the loaded configuration file.\n\n"
        "\033[33;1m  -c\033[0m command\n"
        "\tRuns the specified command before running name. Multiple commands\n"
        "\tcan be specified. Each command should start with -c, though.\n"
        "\tA command can be: an Internal Program, a DOS command or an executable\n"
        "\ton a mounted drive.\n\n"
        "\033[33;1m  -set\033[0m <section property=value>\n"
        "\tSets the config option (overriding the config file). Multiple options\n"
        "\tcan be specified. Each option should start with -set, though.\n"
        );
    MSG_Add("PROGRAM_INTRO_INFO",
        "\033[32;1mInformation:\033[0m\n\n"
        "For information about basic mount, type \033[34;1mintro mount\033[0m\n"
        "For information about CD-ROM support, type \033[34;1mintro cdrom\033[0m\n"
        "For information about special keys, type \033[34;1mintro special\033[0m\n"
        "For information about usage, type \033[34;1mintro usage\033[0m\n\n"
        "For the latest version of DOSBox-X, go to its GitHub site:\033[34;1m\n"
        "\n"
        "\033[34;1mhttps://github.com/joncampbell123/dosbox-x\033[0m\n"
        "\n"
        "For more information about DOSBox-X, please take a look at its Wiki:\n"
        "\n"
        "\033[34;1mhttps://dosbox-x.com/wiki\033[0m\n"
        );
    MSG_Add("PROGRAM_INTRO_MOUNT_START",
        "\033[32;1mHere are some commands to get you started:\033[0m\n"
        "Before you can use the files located on your own filesystem,\n"
        "you have to mount the directory containing the files.\n"
        "\n"
        );
    if (machine == MCH_PC98) {
        MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
            "\033[44;1m\x86\x52\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x56\n"
            "\x86\x46 \033[32mmount a c:\\dosgames\\\033[37m will create an A drive with c:\\dosgames as contents.\x86\x46\n"
            "\x86\x46                                                                          \x86\x46\n"
            "\x86\x46 \033[32mc:\\dosgames\\\033[37m is an example. Replace it with your own games directory.  \033[37m  \x86\x46\n"
            "\x86\x5A\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x5E\033[0m\n"
            );
        MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
            "\033[44;1m\x86\x52\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x56\n"
            "\x86\x46 \033[32mmount a ~/dosgames\033[37m will create an A drive with ~/dosgames as contents.\x86\x46\n"
            "\x86\x46                                                                       \x86\x46\n"
            "\x86\x46 \033[32m~/dosgames\033[37m is an example. Replace it with your own games directory. \033[37m  \x86\x46\n"
            "\x86\x5A\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44"
            "\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x5E\033[0m\n"
            );
        MSG_Add("PROGRAM_INTRO_MOUNT_END",
            "When the mount has successfully completed you can type \033[34;1ma:\033[0m to go to your freshly\n"
            "mounted A-drive. Typing \033[34;1mdir\033[0m there will show its contents."
            " \033[34;1mcd\033[0m will allow you to\n"
            "enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
            "You can run programs/files which end with \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
            );
    } else {
        MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
            "\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
            "\xBA \033[32mmount c c:\\dosgames\\\033[37m will create a C drive with c:\\dosgames as contents.\xBA\n"
            "\xBA                                                                         \xBA\n"
            "\xBA \033[32mc:\\dosgames\\\033[37m is an example. Replace it with your own games directory.  \033[37m \xBA\n"
            "\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
            );
        MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
            "\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
            "\xBA \033[32mmount c ~/dosgames\033[37m will create a C drive with ~/dosgames as contents.\xBA\n"
            "\xBA                                                                      \xBA\n"
            "\xBA \033[32m~/dosgames\033[37m is an example. Replace it with your own games directory.\033[37m  \xBA\n"
            "\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
            );
        MSG_Add("PROGRAM_INTRO_MOUNT_END",
            "When the mount has successfully completed you can type \033[34;1mc:\033[0m to go to your freshly\n"
            "mounted C: drive. Typing \033[34;1mdir\033[0m there will show its contents."
            " \033[34;1mcd\033[0m will allow you to\n"
            "enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
            "You can run programs/files which end with \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
            );
    }
    MSG_Add("PROGRAM_INTRO_CDROM",
        "\033[2J\033[32;1mHow to mount a Real/Virtual CD-ROM Drive in DOSBox-X:\033[0m\n"
        "DOSBox-X provides CD-ROM emulation on several levels.\n"
        "\n"
        "The \033[33mbasic\033[0m level works on all CD-ROM drives and normal directories.\n"
        "It installs MSCDEX and marks the files read-only.\n"
        "Usually this is enough for most games:\n"
        "\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom\033[0m   or   \033[34;1mmount d C:\\example -t cdrom\033[0m\n"
        "If it doesn't work you might have to tell DOSBox-X the label of the CD-ROM:\n"
        "\033[34;1mmount d C:\\example -t cdrom -label CDLABEL\033[0m\n"
        "\n"
        "The \033[33mnext\033[0m level adds some low-level support.\n"
        "Therefore only works on CD-ROM drives:\n"
        "\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0\033[0m\n"
        "\n"
        "The \033[33mlast\033[0m level of support depends on your Operating System:\n"
        "For \033[1mWindows 2000\033[0m, \033[1mWindows XP\033[0m and \033[1mLinux\033[0m:\n"
        "\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-ioctl\033[0m\n"
        "For \033[1mWindows 9x\033[0m with a ASPI layer installed:\n"
        "\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0 \033[34m-aspi\033[0m\n"
        "\n"
        "Replace \033[0;31mD:\\\033[0m with the location of your CD-ROM.\n"
        "Replace the \033[33;1m0\033[0m in \033[34;1m-usecd \033[33m0\033[0m with the number reported for your CD-ROM if you type:\n"
        "\033[34;1mmount -cd\033[0m\n"
        );
    MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
    MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
    MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Boot in write-protected mode.\n");
    MSG_Add("PROGRAM_BOOT_PRINT_ERROR","This command boots DOSBox-X from either a floppy or hard disk image.\n\n"
        "For this command, one can specify a succession of floppy disks swappable\n"
        "by the menu command, and drive: specifies the mounted drive to boot from.\n"
        "If no drive letter is specified, this defaults to boot from the A drive.\n"
        "If no parameter is specified, it will try to boot from the current drive.\n"
        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
        "drive (C or D), ensure the image is already mounted by \033[34;1mIMGMOUNT\033[0m command.\n\n"
        "The syntax of this command is one of the following:\n\n"
        "\033[34;1mBOOT [driveletter:]\033[0m\n\n"
        "\033[34;1mBOOT diskimg1.img [diskimg2.img ...] [-L driveletter]\033[0m\n\n"
        "Note: An image file with a leading colon (:) will be booted in write-protected\n"
		"mode if the \"leading colon write protect image\" option is enabled.\n\n"
        "Examples:\n\n"
        "\033[32;1mBOOT A:\033[0m       - boot from drive A: if it is mounted and bootable.\n"
        "\033[32;1mBOOT :DOS.IMG\033[0m - boot from floppy image DOS.IMG in write-protected mode.\n"
        );
    MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c.\n");
    MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
    MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s\n");
    MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
    MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS","Available PCjr cartridge commandos:%s");
    MSG_Add("PROGRAM_BOOT_CART_NO_CMDS","No PCjr cartridge commandos found");

    MSG_Add("PROGRAM_LOADROM_HELP","Loads the specified ROM image file.\n\nLOADROM ROM_file\n");
    MSG_Add("PROGRAM_LOADROM_HELP","Must specify ROM file to load.\n");
    MSG_Add("PROGRAM_LOADROM_SPECIFY_FILE","Must specify ROM file to load.\n");
    MSG_Add("PROGRAM_LOADROM_CANT_OPEN","ROM file not accessible.\n");
    MSG_Add("PROGRAM_LOADROM_TOO_LARGE","ROM file too large.\n");
    MSG_Add("PROGRAM_LOADROM_INCOMPATIBLE","Video BIOS not supported by machine type.\n");
    MSG_Add("PROGRAM_LOADROM_UNRECOGNIZED","ROM file not recognized.\n");
    MSG_Add("PROGRAM_LOADROM_BASIC_LOADED","BASIC ROM loaded.\n");
    MSG_Add("PROGRAM_BIOSTEST_HELP","Boots into a BIOS image for running CPU tester BIOS.\n\nBIOSTEST image_file\n");

    MSG_Add("VHD_ERROR_OPENING", "Could not open the specified VHD file.\n");
    MSG_Add("VHD_INVALID_DATA", "The specified VHD file is corrupt and cannot be opened.\n");
    MSG_Add("VHD_UNSUPPORTED_TYPE", "The specified VHD file is of an unsupported type.\n");
    MSG_Add("VHD_ERROR_OPENING_PARENT", "The parent of the specified VHD file could not be found.\n");
    MSG_Add("VHD_PARENT_INVALID_DATA", "The parent of the specified VHD file is corrupt and cannot be opened.\n");
    MSG_Add("VHD_PARENT_UNSUPPORTED_TYPE", "The parent of the specified VHD file is of an unsupported type.\n");
    MSG_Add("VHD_PARENT_INVALID_MATCH", "The parent of the specified VHD file does not contain the expected identifier.\n");
    MSG_Add("VHD_PARENT_INVALID_DATE", "The parent of the specified VHD file has been changed and cannot be loaded.\n");

    MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE","Must specify drive letter to mount image at.\n");
    MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2","Must specify drive number (0 to %d) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
    MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
        "For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
        "\n"
        "For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
        "bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
        "\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
    MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
        "Check that the path is correct and the image is accessible.\n");
    MSG_Add("PROGRAM_IMGMOUNT_DYNAMIC_VHD_UNSUPPORTED", "Dynamic VHD files are not supported.\n");
    MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
        "Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
    MSG_Add("PROGRAM_IMGMOUNT_AUTODET_VALUES","Image geometry auto detection: -size %u,%u,%u,%u\n");
    MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED","Type \"%s\" is unsupported. Specify \"hdd\" or \"floppy\" or \"iso\".\n");
    MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
    MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify image file(s) to mount.\n");
    MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
    MSG_Add("PROGRAM_IMGMOUNT_DEFAULT_NOT_FOUND","Image file not found: IMGMAKE.IMG.\n");
    MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
    MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
    MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED_NUMBER","Drive number %d already mounted.\n");
    MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Cannot create drive from file.\n");
    MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE_PHYSFS","Cannot create PhysFS drive.\n");
    MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
    MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host, local or network drive.\n");
    MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");

    MSG_Add("PROGRAM_IMGMOUNT_HELP",
        "Mounts floppy, hard drive and optical disc images.\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdrive\033[0m \033[36;1mfile\033[0m [-ro] [-t floppy] [-fs fat] [-size ss,s,h,c]\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdrive\033[0m \033[36;1mfile\033[0m [-ro] [-t hdd] [-fs fat] [-size ss,s,h,c] [-ide controller]\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdriveNum\033[0m \033[36;1mfile\033[0m [-ro] [-fs none] [-size ss,s,h,c] [-reservecyl #]\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdrive\033[0m \033[36;1mfile\033[0m [-t iso] [-fs iso]\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdrive\033[0m [-t floppy] -bootcd cdDrive (or -el-torito cdDrive)\n"
        "\033[32;1mIMGMOUNT\033[0m \033[37;1mdrive\033[0m -t ram -size size\n"
        "\033[32;1mIMGMOUNT\033[0m -u \033[37;1mdrive|driveNum\033[0m (or \033[32;1mIMGMOUNT\033[0m \033[37;1mdrive|driveNum\033[0m \033[36;1mfile\033[0m [options] -u)\n"
        " \033[37;1mdrive\033[0m               Drive letter to mount the image at.\n"
        " \033[37;1mdriveNum\033[0m            Drive number to mount, where 0-1 are FDDs, 2-5 are HDDs.\n"
        " \033[36;1mfile\033[0m                Image filename(s), or \033[33;1mIMGMAKE.IMG\033[0m if not specified.\n"
        " -t iso              Image type is optical disc iso or cue / bin image.\n"
        " -t hdd              Image type is hard disk; VHD and HDI files are supported.\n"
        " -t floppy|ram       Image type is floppy drive|RAM drive.\n"
        " -fs iso             Filesystem is ISO 9660 (auto-assumed for .iso/.cue files).\n"
        " -fs fat             Filesystem is FAT - FAT12, FAT16 and FAT32 are supported.\n"
        " -fs none            Do not detect filesystem (auto-assumed for drive numbers).\n"
        " -reservecyl #       Report # number of cylinders less than actual in BIOS.\n"
        " -ide controller     Specify IDE controller (1m, 1s, 2m, 2s) to mount drive.\n"
        " -size size|ss,s,h,c Specify the size in KB, or sector size and CHS geometry.\n"
        " -bootcd cdDrive     Specify the CD drive to load the bootable floppy from.\n"
        " -ro                 Mount image(s) read-only (or leading ':' for read-only).\n"
        " -u                  Unmount the drive or drive number.\n"
        " \033[32;1m-examples           Show some usage examples.\033[0m"
    );
    MSG_Add("PROGRAM_IMGMOUNT_EXAMPLE",
        "Some usage examples of IMGMOUNT:\n\n"
        "  \033[32;1mIMGMOUNT\033[0m                       - list mounted FAT/ISO drives & drive numbers\n"
        "  \033[32;1mIMGMOUNT C\033[0m                     - mount hard disk image \033[33;1mIMGMAKE.IMG\033[0m as C:\n"
#ifdef WIN32
        "  \033[32;1mIMGMOUNT C c:\\image.img\033[0m        - mount hard disk image c:\\image.img as C:\n"
        "  \033[32;1mIMGMOUNT D c:\\files\\game.iso\033[0m   - mount CD image c:\\files\\game.iso as D:\n"
#else
        "  \033[32;1mIMGMOUNT C ~/image.img\033[0m         - mount hard disk image ~/image.img as C:\n"
        "  \033[32;1mIMGMOUNT D ~/files/game.iso\033[0m    - mount CD image ~/files/game.iso as D:\n"
#endif
        "  \033[32;1mIMGMOUNT D cdaudio.cue\033[0m         - mount cue file of a cue/bin pair as CD drive\n"
        "  \033[32;1mIMGMOUNT 0 dos.ima\033[0m             - mount floppy image dos.ima as drive number 0\n"
        "                                   (\033[33;1mBOOT A:\033[0m will boot from drive if bootable)\n"
        "  \033[32;1mIMGMOUNT A -ro dos.ima\033[0m         - mount floppy image dos.ima as A: read-only\n"
        "  \033[32;1mIMGMOUNT A :dsk1.img dsk2.img\033[0m  - mount floppy images dsk1.img and dsk2.img as\n"
        "                                   A:, swappable via menu item \"Swap floppy\",\n"
        "                                   with dsk1.img read-only (but not dsk2.img)\n"
        "  \033[32;1mIMGMOUNT A -bootcd D\033[0m           - mount bootable floppy A: from CD drive D:\n"
        "  \033[32;1mIMGMOUNT C -t ram -size 10000\033[0m  - mount hard drive C: as a 10MB RAM drive\n"
        "  \033[32;1mIMGMOUNT C disk.img -u\033[0m         - force mount hard disk image disk.img as C:,\n"
        "                                   auto-unmount drive beforehand if necessary\n"
        "  \033[32;1mIMGMOUNT A -u\033[0m                  - unmount previously-mounted drive A:\n"
        );
    MSG_Add("PROGRAM_IMGMAKE_SYNTAX",
        "Creates floppy or hard disk images.\n"
        "Usage: \033[34;1mIMGMAKE [file] [-t type] [[-size size] | [-chs geometry]] [-spc] [-nofs]\033[0m\n"
        "  \033[34;1m[-bat] [-fat] [-fatcopies] [-rootdir] [-force]"
#ifdef WIN32
        " [-source source] [-r retries]"
#endif
        "\033[0m\n"
        "  file: Image file to create (or \033[33;1mIMGMAKE.IMG\033[0m if not set) - \033[31;1mpath on the host\033[0m\n"
        "  -t: Type of image.\n"
        "    \033[33;1mFloppy disk templates\033[0m (names resolve to floppy sizes in KB or fd=fd_1440):\n"
        "     fd_160 fd_180 fd_200 fd_320 fd_360 fd_400 fd_720 fd_1200 fd_1440 fd_2880\n"
        "    \033[33;1mHard disk templates:\033[0m\n"
        "     hd_250: 250MB image, hd_520: 520MB image, hd_1gig: 1GB image\n"
        "     hd_2gig: 2GB image, hd_4gig: 4GB image, hd_8gig: 8GB image\n"
        "     hd_st251: 40MB image, hd_st225: 20MB image (geometry from old drives)\n"
        "    \033[33;1mCustom hard disk images:\033[0m hd (requires -size or -chs)\n"
        "  -size: Size of a custom hard disk image in MB.\n"
        "  -chs: Disk geometry in cylinders(1-1023),heads(1-255),sectors(1-63).\n"
        "  -nofs: Add this parameter if a blank image should be created.\n"
        "  -force: Force to overwrite the existing image file.\n"
        "  -bat: Create a .bat file with the IMGMOUNT command required for this image.\n"
        "  -fat: FAT filesystem type (12, 16, or 32).\n"
        "  -spc: Sectors per cluster override. Must be a power of 2.\n"
        "  -fatcopies: Override number of FAT table copies.\n"
        "  -rootdir: Size of root directory in entries. Ignored for FAT32.\n"
#ifdef WIN32
        "  -source: drive letter - if specified the image is read from a floppy disk.\n"
        "  -retries: how often to retry when attempting to read a bad floppy disk(1-99).\n"
#endif
        "  \033[32;1m-examples: Show some usage examples.\033[0m"
        );
    MSG_Add("PROGRAM_IMGMAKE_EXAMPLE",
        "Some usage examples of IMGMAKE:\n\n"
        "  \033[32;1mIMGMAKE -t fd\033[0m                   - create a 1.44MB floppy image \033[33;1mIMGMAKE.IMG\033[0m\n"
        "  \033[32;1mIMGMAKE -t fd_1440 -force\033[0m       - force to create a floppy image \033[33;1mIMGMAKE.IMG\033[0m\n"
        "  \033[32;1mIMGMAKE dos.img -t fd_2880\033[0m      - create a 2.88MB floppy image named dos.img\n"
#ifdef WIN32
        "  \033[32;1mIMGMAKE c:\\disk.img -t hd -size 50\033[0m      - create a 50MB HDD image c:\\disk.img\n"
        "  \033[32;1mIMGMAKE c:\\disk.img -t hd_520 -nofs\033[0m     - create a 520MB blank HDD image\n"
        "  \033[32;1mIMGMAKE c:\\disk.img -t hd_2gig -fat 32\033[0m  - create a 2GB FAT32 HDD image\n"
        "  \033[32;1mIMGMAKE c:\\disk.img -t hd -chs 130,2,17\033[0m - create a HDD image of specified CHS\n"
        "  \033[32;1mIMGMAKE c:\\disk.img -source a\033[0m           - read image from physical drive A:\n"
#else
        "  \033[32;1mIMGMAKE ~/disk.img -t hd -size 50\033[0m       - create a 50MB HDD image ~/disk.img\n"
        "  \033[32;1mIMGMAKE ~/disk.img -t hd_520 -nofs\033[0m      - create a 520MB blank HDD image\n"
        "  \033[32;1mIMGMAKE ~/disk.img -t hd_2gig -fat 32\033[0m   - create a 2GB FAT32 HDD image\n"
        "  \033[32;1mIMGMAKE ~/disk.img -t hd -chs 130,2,17\033[0m  - create a HDD image of specified CHS\n"
#endif
        );

#ifdef WIN32
    MSG_Add("PROGRAM_IMGMAKE_FLREAD",
        "Disk geometry: %d Cylinders, %d Heads, %d Sectors, %d Kilobytes\n\n");
    MSG_Add("PROGRAM_IMGMAKE_FLREAD2",
        "\xdb =good, \xb1 =good after retries, ! =CRC error, x =sector not found, ? =unknown\n\n");
#endif
    MSG_Add("PROGRAM_IMGMAKE_FILE_EXISTS","The file \"%s\" already exists. You can specify \"-force\" to overwrite.\n");
    MSG_Add("PROGRAM_IMGMAKE_CANNOT_WRITE","The file \"%s\" cannot be opened for writing.\n");
    MSG_Add("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE","Not enough space availible for the image file. Need %u bytes.\n");
    MSG_Add("PROGRAM_IMGMAKE_PRINT_CHS","Creating image file \"%s\" with %u cylinders, %u heads and %u sectors\n");
    MSG_Add("PROGRAM_IMGMAKE_CANT_READ_FLOPPY","\n\nUnable to read floppy.");

    MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
    MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
    MSG_Add("PROGRAM_KEYB_SHOWHELP","Configures a keyboard for a specific language.\n\n"
        "KEYB [keyboard layout ID [codepage number [codepage file]]]\n\n"
        "Some examples:\n"
        "  \033[32;1mKEYB\033[0m          Display currently loaded codepage.\n"
        "  \033[32;1mKEYB sp\033[0m       Load the Spanish (SP) layout, use an appropriate codepage.\n"
        "  \033[32;1mKEYB sp 850\033[0m   Load the Spanish (SP) layout, use codepage 850.\n"
        "  \033[32;1mKEYB sp 850 mycp.cpi\033[0m Same as above, but use file mycp.cpi.\n");
    MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
    MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
    MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
    MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
    MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");
    MSG_Add("PROGRAM_MODE_USAGE","Configures system devices.\n\n"
            "\033[34;1mMODE\033[0m display-type       :display-type codes are "
            "\033[1mCO80\033[0m, \033[1mBW80\033[0m, \033[1mCO40\033[0m, \033[1mBW40\033[0m, or \033[1mMONO\033[0m\n"
            "\033[34;1mMODE CON COLS=\033[0mc \033[34;1mLINES=\033[0mn :columns and lines, c=80 or 132, n=25, 43, 50, or 60\n"
            "\033[34;1mMODE CON RATE=\033[0mr \033[34;1mDELAY=\033[0md :typematic rates, r=1-32 (32=fastest), d=1-4 (1=lowest)\n");
    MSG_Add("PROGRAM_MODE_INVALID_PARAMETERS","Invalid parameter(s).\n");

    const Section_prop * dos_section=static_cast<Section_prop *>(control->GetSection("dos"));
    hidefiles = dos_section->Get_string("drive z hide files");

    /*regular setup*/
    VFILE_Register("BIN", 0, 0, "/");
    VFILE_Register("DOS", 0, 0, "/");
    VFILE_Register("DEBUG", 0, 0, "/");
    VFILE_Register("SYSTEM", 0, 0, "/");

    PROGRAMS_MakeFile("BOOT.COM",BOOT_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("IMGMAKE.COM", IMGMAKE_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("INTRO.COM",INTRO_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("RE-DOS.COM",REDOS_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("RESCAN.COM",RESCAN_ProgramStart,"/SYSTEM/");
#if defined(WIN32) && !defined(HX_DOS)
    if (startcmd) PROGRAMS_MakeFile("START.COM", START_ProgramStart,"/SYSTEM/");
#endif

    void CGASNOW_ProgramStart(Program * * make), VFRCRATE_ProgramStart(Program * * make);
    if (machine == MCH_CGA) PROGRAMS_MakeFile("CGASNOW.COM",CGASNOW_ProgramStart,"/DEBUG/");
    PROGRAMS_MakeFile("VFRCRATE.COM",VFRCRATE_ProgramStart,"/DEBUG/");

    if (!IS_PC98_ARCH)
        PROGRAMS_MakeFile("LOADROM.COM", LOADROM_ProgramStart,"/DEBUG/");

#if C_DEBUG
    PROGRAMS_MakeFile("BIOSTEST.COM", BIOSTEST_ProgramStart,"/DEBUG/");
#endif

    if (!IS_PC98_ARCH) {
        PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart,"/DOS/");
        PROGRAMS_MakeFile("MODE.COM", MODE_ProgramStart,"/DOS/");
        PROGRAMS_MakeFile("MOUSE.COM", MOUSE_ProgramStart,"/DOS/");
#if defined(USE_TTF)
        PROGRAMS_MakeFile("SETCOLOR.COM", SETCOLOR_ProgramStart,"/DOS/");
#endif
	}

    PROGRAMS_MakeFile("LS.COM",LS_ProgramStart,"/BIN/");
    PROGRAMS_MakeFile("LOADFIX.COM",LOADFIX_ProgramStart,"/DOS/");
    PROGRAMS_MakeFile("ADDKEY.COM",ADDKEY_ProgramStart,"/BIN/");
    PROGRAMS_MakeFile("A20GATE.COM",A20GATE_ProgramStart,"/DEBUG/");
    PROGRAMS_MakeFile("CFGTOOL.COM",CFGTOOL_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("COLOR.COM",COLOR_ProgramStart,"/DOS/");
    PROGRAMS_MakeFile("DELTREE.EXE",DELTREE_ProgramStart,"/DOS/");
    PROGRAMS_MakeFile("FLAGSAVE.COM", FLAGSAVE_ProgramStart,"/SYSTEM/");
#if defined C_DEBUG
    PROGRAMS_MakeFile("INT2FDBG.COM",INT2FDBG_ProgramStart,"/DEBUG/");
    PROGRAMS_MakeFile("NMITEST.COM",NMITEST_ProgramStart,"/DEBUG/");
#endif

    if (IS_VGA_ARCH && svgaCard != SVGA_None)
        PROGRAMS_MakeFile("VESAMOED.COM",VESAMOED_ProgramStart,"/DEBUG/");

    if (IS_PC98_ARCH)
        PROGRAMS_MakeFile("PC98UTIL.COM",PC98UTIL_ProgramStart,"/BIN/");

    PROGRAMS_MakeFile("CAPMOUSE.COM", CAPMOUSE_ProgramStart,"/SYSTEM/");
    PROGRAMS_MakeFile("LABEL.COM", LABEL_ProgramStart,"/DOS/");
    PROGRAMS_MakeFile("TREE.COM", TREE_ProgramStart,"/DOS/");
    PROGRAMS_MakeFile("AUTOTYPE.COM", AUTOTYPE_ProgramStart,"/BIN/");
}
