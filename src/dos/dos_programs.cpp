/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include "dosbox.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
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
bool Mouse_Drv=true;
bool Mouse_Vertical = false;

#if defined(OS2)
#define INCL DOSFILEMGR
#define INCL_DOSERRORS
#include "os2.h"
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
	switch (Mouse_Drv) {
	case 0:
		if (cmd->FindExist("/u",false))
			WriteOut(MSG_Get("PROGRAM_MOUSE_NOINSTALLED"));
		else {
			Mouse_Drv = true;
			WriteOut(MSG_Get("PROGRAM_MOUSE_INSTALL"));
			if (cmd->FindExist("/v",false)) {
				Mouse_Vertical = true;
				WriteOut(MSG_Get("PROGRAM_MOUSE_VERTICAL"));
			} else {
				Mouse_Vertical = false;
			}
		}
		break;
	case 1:
		if (cmd->FindExist("/u",false)) {
			Mouse_Drv = false;
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
			} else
				WriteOut(MSG_Get("PROGRAM_MOUSE_ERROR"));
		break;
	}
	return;
}

static void MOUSE_ProgramStart(Program * * make) {
	*make=new MOUSE;
}

void MSCDEX_SetCDInterface(int intNr, int forceCD);
Bitu ZDRIVE_NUM = 25;

class MOUNT : public Program {
public:
	void ListMounts(void) {
		char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
		/* Command uses dta so set it to our internal dta */
		RealPt save_dta = dos.dta();
		dos.dta(dos.tables.tempdta);
		DOS_DTA dta(dos.dta());

		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),"Drive","Type","Label");
		for(int p = 0;p < 8;p++) WriteOut("----------");

		for (int d = 0;d < DOS_DRIVES;d++) {
			if (!Drives[d]) continue;

			char root[4] = {(char)('A'+d),':','\\',0};
			bool ret = DOS_FindFirst(root,DOS_ATTR_VOLUME);
			if (ret) {
				dta.GetResult(name,size,date,time,attr);
				DOS_FindNext(); //Mark entry as invalid
			} else name[0] = 0;

			/* Change 8.3 to 11.0 */
			char* dot = strchr(name,'.');
			if(dot && (dot - name == 8) ) { 
				name[8] = name[9];name[9] = name[10];name[10] = name[11];name[11] = 0;
			}

			root[1] = 0; //This way, the format string can be reused.
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_FORMAT"),root, Drives[d]->GetInfo(),name);		
		}
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

		if (cmd->FindExist("-q",false))
			quiet = true;

		/* Check for unmounting */
		if (cmd->FindString("-u",umount,false)) {
			umount[0] = toupper(umount[0]);
			int i_drive = umount[0]-'A';
				if (i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive]) {
					switch (DriveManager::UnmountDrive(i_drive)) {
					case 0:
						Drives[i_drive] = 0;
						if(i_drive == DOS_GetDefaultDrive()) 
							DOS_SetDrive(ZDRIVE_NUM);
						if (!quiet)
                            WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS"),umount[0]);
						break;
					case 1:
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
						break;
					case 2:
						WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
						break;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"),umount[0]);
				}
			return;
		}
		
		/* Check for moving Z: */
		/* Only allowing moving it once. It is merely a convenience added for the wine team */
		if (ZDRIVE_NUM == 25 && cmd->FindString("-z", newz,false)) {
			newz[0] = toupper(newz[0]);
			int i_newz = newz[0] - 'A';
			if (i_newz >= 0 && i_newz < DOS_DRIVES-1 && !Drives[i_newz]) {
				ZDRIVE_NUM = i_newz;
				/* remap drives */
				Drives[i_newz] = Drives[25];
				Drives[25] = 0;
				DOS_Shell *fs = static_cast<DOS_Shell *>(first_shell); //dynamic ?				
				/* Update environment */
				std::string line = "";
				char ppp[2] = {newz[0],0};
				std::string tempenv = ppp; tempenv += ":\\";
				if (fs->GetEnvStr("PATH",line)){
					std::string::size_type idx = line.find('=');
					std::string value = line.substr(idx +1 , std::string::npos);
					while ( (idx = value.find("Z:\\")) != std::string::npos ||
					        (idx = value.find("z:\\")) != std::string::npos  )
						value.replace(idx,3,tempenv);
					line = value;
				}
				if (!line.size()) line = tempenv;
				fs->SetEnv("PATH",line.c_str());
				tempenv += "COMMAND.COM";
				fs->SetEnv("COMSPEC",tempenv.c_str());

				/* Update batch file if running from Z: (very likely: autoexec) */
				if(fs->bf) {
					std::string &name = fs->bf->filename;
					if(name.length() >2 &&  name[0] == 'Z' && name[1] == ':') name[0] = newz[0];
				}
				/* Change the active drive */
				if (DOS_GetDefaultDrive() == 25) DOS_SetDrive(i_newz);
			}
			return;
		}
		/* Show list of cdroms */
		if (cmd->FindExist("-cd",false)) {
#if !defined(C_SDL2)
			int num = SDL_CDNumDrives();
   			WriteOut(MSG_Get("PROGRAM_MOUNT_CDROMS_FOUND"),num);
			for (int i=0; i<num; i++) {
				WriteOut("%2d. %s\n",i,SDL_CDName(i));
			};
#endif
			return;
		}

        bool nocachedir = false;
        if (cmd->FindExist("-nocachedir",true))
            nocachedir = true;

        bool readonly = false;
        if (cmd->FindExist("-ro",true))
            readonly = true;
        if (cmd->FindExist("-rw",true))
            readonly = false;

		std::string type="dir";
		cmd->FindString("-t",type,true);
		bool iscdrom = (type =="cdrom"); //Used for mscdex bug cdrom label name emulation
		if (type=="floppy" || type=="dir" || type=="cdrom") {
			Bit16u sizes[4];
			Bit8u mediaid;
			std::string str_size;
			if (type=="floppy") {
				str_size="512,1,2880,2880";/* All space free */
				mediaid=0xF0;		/* Floppy 1.44 media */
			} else if (type=="dir") {
				// 512*32*32765==~500MB total size
				// 512*32*16000==~250MB total free size
#if defined(__WIN32__) && !defined(C_SDL2)
				GetDefaultSize();
				str_size=hdd_size;
#else
				str_size="512,32,32765,16000";
#endif
				mediaid=0xF8;		/* Hard Disk */
			} else if (type=="cdrom") {
				str_size="2048,1,65535,0";
				mediaid=0xF8;		/* Hard Disk */
			} else {
				WriteOut(MSG_Get("PROGAM_MOUNT_ILL_TYPE"),type.c_str());
				return;
			}
			/* Parse the free space in mb's (kb's for floppies) */
			std::string mb_size;
			if(cmd->FindString("-freesize",mb_size,true)) {
				char teststr[1024];
				Bit16u freesize = static_cast<Bit16u>(atoi(mb_size.c_str()));
				if (type=="floppy") {
					// freesize in kb
					sprintf(teststr,"512,1,2880,%d",freesize*1024/(512*1));
				} else {
					Bit32u total_size_cyl=32765;
					Bit32u free_size_cyl=(Bit32u)freesize*1024*1024/(512*32);
					if (free_size_cyl>65534) free_size_cyl=65534;
					if (total_size_cyl<free_size_cyl) total_size_cyl=free_size_cyl+10;
					if (total_size_cyl>65534) total_size_cyl=65534;
					sprintf(teststr,"512,32,%d,%d",total_size_cyl,free_size_cyl);
				}
				str_size=teststr;
			}
		   
			cmd->FindString("-size",str_size,true);
			char number[20];const char * scan=str_size.c_str();
			Bitu index=0;Bitu count=0;
			/* Parse the str_size string */
			while (*scan) {
				if (*scan==',') {
					number[index]=0;sizes[count++]=atoi(number);
					index=0;
				} else number[index++]=*scan;
				scan++;
			}
			number[index]=0;sizes[count++]=atoi(number);
		
			// get the drive letter
			cmd->FindCommand(1,temp_line);
			if ((temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) goto showusage;
			drive=toupper(temp_line[0]);
			if (!isalpha(drive)) goto showusage;

			if (!cmd->FindCommand(2,temp_line)) goto showusage;
			if (!temp_line.size()) goto showusage;
			bool is_physfs = temp_line.find(':',((temp_line[0]|0x20) >= 'a' && (temp_line[0]|0x20) <= 'z')?2:0) != std::string::npos;
			struct stat test;
			//Win32 : strip tailing backslashes
			//os2: some special drive check
			//rest: substiture ~ for home
			bool failed = false;

#if defined (WIN32) || defined(OS2)
			/* nothing */
#else
			// Linux: Convert backslash to forward slash
			if (!is_physfs && temp_line.size() > 0) {
				for (size_t i=0;i < temp_line.size();i++) {
					if (temp_line[i] == '\\')
						temp_line[i] = '/';
				}
			}
#endif

#if defined (WIN32) || defined(OS2)
			/* Removing trailing backslash if not root dir so stat will succeed */
			if(temp_line.size() > 3 && temp_line[temp_line.size()-1]=='\\') temp_line.erase(temp_line.size()-1,1);
			if (!is_physfs && stat(temp_line.c_str(),&test)) {
#endif
#if defined(WIN32)
// Nothing to do here.
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
			if (failed) {
#else
			if (!is_physfs && stat(temp_line.c_str(),&test)) {
				failed = true;
				Cross::ResolveHomedir(temp_line);
				//Try again after resolving ~
				if(!stat(temp_line.c_str(),&test)) failed = false;
			}
			if(failed) {
#endif
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
				return;
			}
			/* Not a switch so a normal directory/file */
			if (!is_physfs && !(test.st_mode & S_IFDIR)) {
#ifdef OS2
				HFILE cdrom_fd = 0;
				ULONG ulAction = 0;

				APIRET rc = DosOpen((unsigned char*)temp_line.c_str(), &cdrom_fd, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
					OPEN_FLAGS_DASD | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0L);
				DosClose(cdrom_fd);
				if (rc != NO_ERROR && rc != ERROR_NOT_READY) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
			}
#else
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
#endif

			}

			if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
			Bit8u bit8size=(Bit8u) sizes[1];
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
					OSVERSIONINFO osi;
					osi.dwOSVersionInfoSize = sizeof(osi);
					GetVersionEx(&osi);
					if ((osi.dwPlatformId==VER_PLATFORM_WIN32_NT) && (osi.dwMajorVersion>5)) {
						// Vista/above
						MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
					} else {
						MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
					}
#else
					MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
#endif
				}
				if (is_physfs) {
					LOG_MSG("ERROR:This build does not support physfs");
				} else {
					newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
				}
				// Check Mscdex, if it worked out...
				switch (error) {
					case 0  :	WriteOut(MSG_Get("MSCDEX_SUCCESS"));				break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));				break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
				};
				if (error && error!=5) {
					delete newdrive;
					return;
				}
			} else {
				/* Give a warning when mount c:\ or the / */
#if defined (WIN32) || defined(OS2)
				if( (temp_line == "c:\\") || (temp_line == "C:\\") || 
				    (temp_line == "c:/") || (temp_line == "C:/")    )	
					WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
#else
				if(temp_line == "/") WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_OTHER"));
#endif
				if (is_physfs) {
					LOG_MSG("ERROR:This build does not support physfs");
				} else {
					newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
                    newdrive->nocachedir = nocachedir;
                    newdrive->readonly = readonly;
				}
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
			return;
		}
		if (Drives[drive-'A']) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),drive,Drives[drive-'A']->GetInfo());
			if (newdrive) delete newdrive;
			return;
		}
		if (!newdrive) E_Exit("DOS:Can't create drive");
		Drives[drive-'A']=newdrive;
		/* Set the correct media byte in the table */
		mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,newdrive->GetMediaByte());
		if (!quiet) WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,newdrive->GetInfo());
		/* check if volume label is given and don't allow it to updated in the future */
		if (cmd->FindString("-label",label,true)) newdrive->SetLabel(label.c_str(),iscdrom,false);
		/* For hard drives set the label to DRIVELETTER_Drive.
		 * For floppy drives set the label to DRIVELETTER_Floppy.
		 * This way every drive except cdroms should get a label.*/
		else if(type == "dir") { 
#if defined (WIN32) || defined(OS2)
            if(temp_line.size()==3 && toupper(drive) == toupper(temp_line[0]))  {
                // automatic mount
            } else {
    			label = drive; label += "_DRIVE";
    			newdrive->SetLabel(label.c_str(),iscdrom,true);
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
#if defined (WIN32) || defined(OS2)
	   WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"),"d:\\dosprogs","d:\\dosprogs");
#else
	   WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"),"~/dosprogs","~/dosprogs");		   
#endif
		return;
	}
};

static void MOUNT_ProgramStart(Program * * make) {
	*make=new MOUNT;
}

#if !defined(C_SDL2)
void GUI_Run(bool pressed);

class SHOWGUI : public Program {
public:
	void Run(void) {
		GUI_Run(false); /* So that I don't have to run the keymapper on every setup of mine just to get the GUI --J.C */
	}
};

static void SHOWGUI_ProgramStart(Program * * make) {
	*make=new SHOWGUI;
}
#endif

extern Bit32u floppytype;
extern bool dos_kernel_disabled;
extern bool boot_debug_break;

void DisableINT33();
void EMS_DoShutDown();
void XMS_DoShutDown();
void DOS_DoShutDown();
void GUS_DOS_Shutdown();
void SBLASTER_DOS_Shutdown();

class BOOT : public Program {
private:
   
	FILE *getFSFile_mounted(char const* filename, Bit32u *ksize, Bit32u *bsize, Bit8u *error) {
		//if return NULL then put in error the errormessage code if an error was requested
		bool tryload = (*error)?true:false;
		*error = 0;
		Bit8u drive;
		FILE *tmpfile;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName(const_cast<char*>(filename),fullname,&drive)) return NULL;

		try {		
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if(!ldp) return NULL;

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if(tmpfile == NULL) {
				if (!tryload) *error=1;
				return NULL;
			}

			// get file size
			fseek(tmpfile,0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			fclose(tmpfile);

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
			if(tmpfile == NULL) {
//				if (!tryload) *error=2;
//				return NULL;
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
   
	FILE *getFSFile(char const * filename, Bit32u *ksize, Bit32u *bsize,bool tryload=false) {
		Bit8u error = tryload?1:0;
		FILE* tmpfile = getFSFile_mounted(filename,ksize,bsize,&error);
		if(tmpfile) return tmpfile;
		//File not found on mounted filesystem. Try regular filesystem
		std::string filename_s(filename);
		Cross::ResolveHomedir(filename_s);
		tmpfile = fopen(filename_s.c_str(),"rb+");
		if(!tmpfile) {
			if( (tmpfile = fopen(filename_s.c_str(),"rb")) ) {
				//File exists; So can't be opened in correct mode => error 2
//				fclose(tmpfile);
//				if(tryload) error = 2;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				fseek(tmpfile,0L, SEEK_END);
				*ksize = (ftell(tmpfile) / 1024);
				*bsize = ftell(tmpfile);
				return tmpfile;
			}
			// Give the delayed errormessages from the mounted variant (or from above)
			if(error == 1) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
			if(error == 2) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
			return NULL;
		}
		fseek(tmpfile,0L, SEEK_END);
		*ksize = (ftell(tmpfile) / 1024);
		*bsize = ftell(tmpfile);
		return tmpfile;
	}

	void printError(void) {
		WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"));
	}

	void disable_umb_ems_xms(void) {
		Section* dos_sec = control->GetSection("dos");
		char test[20];
		strcpy(test,"umb=false");
		dos_sec->HandleInputline(test);
		strcpy(test,"xms=false");
		dos_sec->HandleInputline(test);
		strcpy(test,"ems=false");
		dos_sec->HandleInputline(test);
	}

public:
   
	void Run(void) {
        bool force = false;

        boot_debug_break = false;
        if (cmd->FindExist("-debug",true))
            boot_debug_break = true;

        if (cmd->FindExist("-force",true))
            force = true;

        if (IS_PC98_ARCH && !force) {
            WriteOut("Booting from PC-98 mode is not supported yet\n");
            return;
        }

		//Hack To allow long commandlines
		ChangeToLongCmd();
		/* In secure mode don't allow people to boot stuff. 
		 * They might try to corrupt the data on it */
		if(control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}

		FILE *usefile_1=NULL;
		FILE *usefile_2=NULL;
		Bitu i=0; 
		Bit32u floppysize=0;
		Bit32u rombytesize_1=0;
		Bit32u rombytesize_2=0;
		Bit8u drive = 'A';
		std::string cart_cmd="";
		Bitu stack_seg=0x7000,max_seg,load_seg=0x07C0;

		if (MEM_TotalPages() > 0x9C)
			max_seg = 0x9C00;
		else
			max_seg = MEM_TotalPages() << (12 - 4);

		if ((stack_seg+0x20) > max_seg)
			stack_seg = max_seg - 0x20;

		if(!cmd->GetCount()) {
			printError();
			return;
		}
		while(i<cmd->GetCount()) {
			if(cmd->FindCommand(i+1, temp_line)) {
				if((temp_line == "-l") || (temp_line == "-L")) {
					/* Specifying drive... next argument then is the drive */
					i++;
					if(cmd->FindCommand(i+1, temp_line)) {
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
					if(cmd->FindCommand(i + 1, temp_line)) {
						for(size_t ct = 0;ct < temp_line.size();ct++) temp_line[ct] = toupper(temp_line[ct]);
						cart_cmd = temp_line;
					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				Bit32u rombytesize=0;
				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), temp_line.c_str());
				FILE *usefile = getFSFile(temp_line.c_str(), &floppysize, &rombytesize);
				if(usefile != NULL) {
					if(diskSwap[i] != NULL) diskSwap[i]->Release();
					diskSwap[i] = new imageDisk(usefile, (Bit8u *)temp_line.c_str(), floppysize, false);
					diskSwap[i]->Addref();

					if (usefile_1==NULL) {
						usefile_1=usefile;
						rombytesize_1=rombytesize;
					} else {
						usefile_2=usefile;
						rombytesize_2=rombytesize;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}

			}
			i++;
		}

		swapPosition = 0;

		swapInDisks();

		if(imageDiskList[drive-65]==NULL) {
			WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
			return;
		}

		bootSector bootarea;

        if (imageDiskList[drive-65]->getSectSize() > sizeof(bootarea)) {
            WriteOut("Bytes/sector too large");
            return;
        }

		imageDiskList[drive-65]->Read_Sector(0,0,1,(Bit8u *)&bootarea);

		Bitu pcjr_hdr_length = 0;
		Bit8u pcjr_hdr_type = 0; // not a PCjr cartridge
		if ((bootarea.rawdata[0]==0x50) && (bootarea.rawdata[1]==0x43) && (bootarea.rawdata[2]==0x6a) && (bootarea.rawdata[3]==0x72)) {
			pcjr_hdr_type = 1; // JRipCart
			pcjr_hdr_length = 0x200;
		} else if ((bootarea.rawdata[56]==0x50) && (bootarea.rawdata[57]==0x43) && (bootarea.rawdata[58]==0x4a) && (bootarea.rawdata[59]==0x52)) {
			pcjr_hdr_type = 2; // PCJRCart
			pcjr_hdr_length = 0x80;
		}
		
		if (pcjr_hdr_type > 0) {
			if (machine!=MCH_PCJR) WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
			else {
				Bit8u rombuf[65536];
				Bits cfound_at=-1;
				if (cart_cmd!="") {
					/* read cartridge data into buffer */
					fseek(usefile_1, (long)pcjr_hdr_length, SEEK_SET);
					fread(rombuf, 1, rombytesize_1-pcjr_hdr_length, usefile_1);

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
							ct+=1+clen+3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (ct>6) {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
						} else {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
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
							ct+=1+clen;

							if (cart_cmd==buf) {
								cfound_at=ct;
								break;
							}

							ct+=3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (cfound_at<=0) {
							if (ct>6) {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
							} else {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
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

				disable_umb_ems_xms();
				void PreparePCJRCartRom(void);
				PreparePCJRCartRom();

				if (usefile_1==NULL) return;

				Bit32u sz1,sz2;
				FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
				if (tfile!=NULL) {
					fseek(tfile, 0x3000L, SEEK_SET);
					Bit32u drd=(Bit32u)fread(rombuf, 1, 0xb000, tfile);
					if (drd==0xb000) {
						for(i=0;i<0xb000;i++) phys_writeb(0xf3000+i,rombuf[i]);
					}
					fclose(tfile);
				}

				if (usefile_2!=NULL) {
					unsigned int romseg_pt=0;

					fseek(usefile_2, 0x0L, SEEK_SET);
					fread(rombuf, 1, pcjr_hdr_length, usefile_2);
					if (pcjr_hdr_type == 1) {
						romseg_pt=host_readw(&rombuf[0x1ce]);
					} else {
						fseek(usefile_2, 0x61L, SEEK_SET);
						fscanf(usefile_2, "%4x", &romseg_pt);
					}
					/* read cartridge data into buffer */
					fseek(usefile_2, (long)pcjr_hdr_length, SEEK_SET);
					fread(rombuf, 1, rombytesize_2-pcjr_hdr_length, usefile_2);
					//fclose(usefile_2); //usefile_2 is in diskSwap structure which should be deleted to close the file

					/* write cartridge data into ROM */
					for(i=0;i<rombytesize_2-pcjr_hdr_length;i++) phys_writeb((romseg_pt<<4)+i,rombuf[i]);
				}

				unsigned int romseg=0;

				fseek(usefile_1, 0x0L, SEEK_SET);
				fread(rombuf, 1, pcjr_hdr_length, usefile_1);
				if (pcjr_hdr_type == 1) {
					romseg=host_readw(&rombuf[0x1ce]);
				} else {
					fseek(usefile_1, 0x61L, SEEK_SET);
					fscanf(usefile_1, "%4x", &romseg);
				}
				/* read cartridge data into buffer */
				fseek(usefile_1,(long)pcjr_hdr_length, SEEK_SET);
				fread(rombuf, 1, rombytesize_1-pcjr_hdr_length, usefile_1);
				//fclose(usefile_1); //usefile_1 is in diskSwap structure which should be deleted to close the file
				/* write cartridge data into ROM */
				for(i=0;i<rombytesize_1-pcjr_hdr_length;i++) phys_writeb((romseg<<4)+i,rombuf[i]);

				//Close cardridges
				for(Bitu dct=0;dct<MAX_SWAPPABLE_DISKS;dct++) {
					if(diskSwap[dct]!=NULL) {
						diskSwap[dct]->Release();
						diskSwap[dct]=NULL;
					}
				}


				if (cart_cmd=="") {
					Bit32u old_int18=mem_readd(0x60);
					/* run cartridge setup */
					SegSet16(ds,romseg);
					SegSet16(es,romseg);
					SegSet16(ss,0x8000);
					reg_esp=0xfffe;
					CALLBACK_RunRealFar(romseg,0x0003);

					Bit32u new_int18=mem_readd(0x60);
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
						CALLBACK_RunRealFar(romseg,cfound_at);
					}
				}
			}
		} else {
            extern const char* RunningProgram;

			if (max_seg < 0x0800) {
				/* TODO: For the adventerous, add a configuration option or command line switch to "BOOT"
				 *       that allows us to boot the guest OS anyway in a manner that is non-standard. */
				WriteOut("32KB of RAM is required to boot a guest OS\n");
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
				WriteOut_NoParsing("PROGRAM_BOOT_UNABLE");
				return;
			}

			disable_umb_ems_xms();

			WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);
			for(i=0;i<512;i++) real_writeb(0, (load_seg<<4) + i, bootarea.rawdata[i]);

			/* debug */
			LOG_MSG("Booting guest OS stack_seg=0x%04x load_seg=0x%04x\n",(int)stack_seg,(int)load_seg);
            RunningProgram = "Guest OS";
 
			/* create appearance of floppy drive DMA usage (Demon's Forge) */
			if (!IS_TANDY_ARCH && floppysize!=0) GetDMAChannel(2)->tcount=true;

			/* standard method */
			SegSet16(cs, 0);
			SegSet16(ds, 0);
			SegSet16(es, 0);
			reg_ip = load_seg<<4;
			reg_ebx = load_seg<<4; //Real code probably uses bx to load the image
			reg_esp = 0x100;
			/* set up stack at a safe place */
			SegSet16(ss, stack_seg);
			reg_esi = 0;
			reg_ecx = 1;
			reg_ebp = 0;
			reg_eax = 0;
			reg_edx = 0; //Head 0
			if (drive >= 'A' && drive <= 'B')
				reg_edx += (drive-'A');
			else if (drive >= 'C' && drive <= 'Z')
				reg_edx += 0x80+(drive-'C');
#ifdef __WIN32__
			// let menu know it boots
			menu.boot=true;
#endif

			/* forcibly exit the shell, the DOS kernel, and anything else by throwing an exception */
			throw int(2);
		}
	}
};

static void BOOT_ProgramStart(Program * * make) {
	*make=new BOOT;
}

class LDGFXROM : public Program {
public:
	void Run(void) {
		if (!(cmd->FindCommand(1, temp_line))) return;

		Bit8u drive;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName((char *)temp_line.c_str(),fullname,&drive)) return;

		try {		
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if(!ldp) return;

			FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if(tmpfile == NULL) {
				LOG_MSG("BIOS file not accessible.");
				return;
			}
			fseek(tmpfile, 0L, SEEK_END);
			if (ftell(tmpfile)>0x10000) {
				LOG_MSG("BIOS file too large.");
				return;
			}
			fseek(tmpfile, 0L, SEEK_SET);

			PhysPt rom_base=PhysMake(0xc000,0);

			Bit8u vga_buffer[0x10000];
			Bitu data_written=0;
			Bitu data_read = (Bitu)fread(vga_buffer, 1, 0x10000, tmpfile);
			for (Bitu ct=0; ct<data_read; ct++) {
				phys_writeb(rom_base+(data_written++),vga_buffer[ct]);
			}
			fclose(tmpfile);

			rom_base=PhysMake(0xf000,0);
			phys_writeb(rom_base+0xf065,0xcf);
		}
		catch(...) {
			return;
		}

		reg_flags&=~FLAG_IF;
		CALLBACK_RunRealFar(0xc000,0x0003);
	}
};

static void LDGFXROM_ProgramStart(Program * * make) {
	*make=new LDGFXROM;
}

const Bit8u freedos_mbr[] = {
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
	bool OpenDisk(HANDLE* f, OVERLAPPED* o, Bit8u* name) {
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

	bool StartReadDisk(HANDLE f, OVERLAPPED* o, Bit8u* buffer, Bitu offset, Bitu size) { 
		o->Offset = offset;
		if (!ReadFile(f, buffer, size, NULL, o) && 
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
			if(error==ERROR_IO_INCOMPLETE)			return 0;
			if(error==ERROR_FLOPPY_UNKNOWN_ERROR)	return 5;
			if(error==ERROR_CRC)					return 4;
			if(error==ERROR_SECTOR_NOT_FOUND)		return 3;
			return 1;	
		}
	}

	Bitu ReadDisk(FILE* f, Bit8u driveletter, Bitu retries_max) {
		unsigned char data[36*2*512];
		HANDLE hFloppy;
		DWORD numret;
		OVERLAPPED o;
		DISK_GEOMETRY geom;

		Bit8u drivestring[] = "\\\\.\\x:"; drivestring[4]=driveletter;
		if(!OpenDisk(&hFloppy, &o, drivestring)) return false;

		// get drive geom
		DeviceIoControl( hFloppy, IOCTL_DISK_GET_DRIVE_GEOMETRY,NULL,0,
		&geom,sizeof(DISK_GEOMETRY),&numret,NULL);

		switch(geom.MediaType) {
			case F5_1Pt2_512: case F3_1Pt44_512: case F3_2Pt88_512:	case F3_720_512:
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

						if(retries)	goto restart_int;
						const Bit8u badfood[]="IMGMAKE BAD FLOPPY SECTOR \xBA\xAD\xF0\x0D";
						for(Bitu z = 0; z < 512/32; z++)
							memcpy(&data[512*k+z*32],badfood,32);
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
		std::string path = "";
		std::string dpath;

		unsigned int c, h, s, sectors; 
		Bit64u size = 0;

		if(cmd->FindExist("-?")) {
			printHelp();
			return;
		}

/*
		this stuff is too frustrating

		// when only a filename is passed try to create the file on the current DOS path
		// if directory+filename are passed first see if directory is a host path, if not
		// maybe it is a DOSBox path.
		
		// split filename and path
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

			// temp_line is the filename
			if (!(cmd->FindCommand(1, temp_line))) {
				printHelp();
				return;
			}

			// don't trash user's files
			FILE* f = fopen(temp_line.c_str(),"r");
			if(f) {
				fclose(f);
				WriteOut(MSG_Get("PROGRAM_IMGMAKE_FILE_EXISTS"),temp_line.c_str());
				return;
			}
			f = fopen(temp_line.c_str(),"wb+");
			if (!f) {
				WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANNOT_WRITE"),temp_line.c_str());
				return;
			}
			// maybe delete f if it failed?
			if(!ReadDisk(f, src.c_str()[0],retries))
				WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANT_READ_FLOPPY"));
            fclose(f);
			return;
		}
#endif
		// disk type
		if (!(cmd->FindString("-t",disktype,true))) {
			printHelp();
			return;
		}

		Bit8u mediadesc = 0xF8; // media descriptor byte; also used to differ fd and hd
		Bitu root_ent = 512; // FAT root directory entries: 512 is for harddisks
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
		} else if(disktype=="fd_1440") {
			c = 80; h = 2; s = 18; mediadesc = 0xF0; root_ent = 224;
		} else if(disktype=="fd_2880") {
			c = 80; h = 2; s = 36; mediadesc = 0xF0; root_ent = 512; // root_ent?
		} else if(disktype=="hd_250") {
			c = 489; h = 16; s = 63;
		} else if(disktype=="hd_520") {
			c = 1023; h = 16; s = 63;
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
					size = c*h*s*512LL;
					if((size < 3*1024*1024) || (size > 0x1FFFFFFFFLL)) {
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
				// low limit: 3 megs, high limit: 2 gigs
				// Int13 limit would be 8 gigs
				if((size < 3*1024*1024LL) || (size > 0x1FFFFFFFFLL)) {
					// wrong size
					printHelp();
					return;
				}
				sectors = (Bitu)(size / 512);

				// Now that we finally have the proper size, figure out good CHS values
				h=2;
				while(h*1023*63 < sectors) h <<= 1;
				if(h>255) h=255;
				s=8;
				while(h*s*1023 < sectors) s *= 2;
				if(s>63) s=63;
				c=sectors/(h*s);
				if(c>1023) c=1023;
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

		size = c*h*s*512LL;
		Bits bootsect_pos = 0; // offset of the boot sector in clusters
		if(cmd->FindExist("-nofs",true) || (size>(2048*1024*1024LL))) {
			bootsect_pos = -1;
		}

		// temp_line is the filename
		if (!(cmd->FindCommand(1, temp_line))) {
			printHelp();
			return;
		}

		// don't trash user's files
		FILE* f = fopen(temp_line.c_str(),"r");
		if(f) {
			fclose(f);
			WriteOut(MSG_Get("PROGRAM_IMGMAKE_FILE_EXISTS"),temp_line.c_str());
			return;
		}

		WriteOut(MSG_Get("PROGRAM_IMGMAKE_PRINT_CHS"),c,h,s);
		WriteOut("%s\r\n",temp_line.c_str());
		LOG_MSG(MSG_Get("PROGRAM_IMGMAKE_PRINT_CHS"),c,h,s);

		// do it again for fixed chs values
		sectors = (Bitu)(size / 512);

		// create the image file
		f = fopen64(temp_line.c_str(),"wb+");
		if (!f) {
			WriteOut(MSG_Get("PROGRAM_IMGMAKE_CANNOT_WRITE"),temp_line.c_str());
			return;
		}
		if(fseeko64(f,size-1,SEEK_SET)) {
			WriteOut(MSG_Get("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE"),size);
			return;
		}
		Bit8u bufferbyte=0;
		if(fwrite(&bufferbyte,1,1,f)!=1) {
			WriteOut(MSG_Get("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE"),size);
			return;
		}

		// Format the image if not unrequested (and image size<2GB)
		if(bootsect_pos > -1) {
			Bit8u sbuf[512];
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
				// OS indicator: DOS what else ;)
				sbuf[0x1c2]=0x06;
				// end head (0-based)
				sbuf[0x1c3]= h-1;
				// end sector with bits 8-9 of end cylinder (0-based) in bits 6-7
				sbuf[0x1c4]=s|(((c-1)&0x300)>>2);
				// end cylinder (0-based) bits 0-7
				sbuf[0x1c5]=(c-1)&0xFF;
				// sectors preceding partition1 (one head)
				host_writed(&sbuf[0x1c6],s);
				// length of partition1, align to chs value
				host_writed(&sbuf[0x1ca],((c-1)*h+(h-1))*s);

				// write partition table
				fseeko64(f,0,SEEK_SET);
				fwrite(&sbuf,512,1,f);
				bootsect_pos = s;
			}

			// set boot sector values
			memset(sbuf,0,512);
			// TODO boot code jump
			sbuf[0]=0xEB; sbuf[1]=0x3c; sbuf[2]=0x90;
			// OEM
			sprintf((char*)&sbuf[0x03],"MSDOS5.0");
			// bytes per sector: always 512
			host_writew(&sbuf[0x0b],512);
			// sectors per cluster: 1,2,4,8,16,...
			if(mediadesc == 0xF8) {
				Bitu cval = 1;
				while((sectors/cval) >= 65525) cval <<= 1;
				sbuf[0x0d]=cval;
			} else sbuf[0x0d]=sectors/0x1000 + 1; // FAT12 can hold 0x1000 entries TODO
			// TODO small floppys have 2 sectors per cluster?
			// reserverd sectors: 1 ( the boot sector)
			host_writew(&sbuf[0x0e],1);
			// Number of FATs - always 2
			sbuf[0x10] = 2;
			// Root entries - how are these made up? - TODO
			host_writew(&sbuf[0x11],root_ent);
			// sectors (under 32MB) - will OSes be sore if all HD's use large size?
			if(mediadesc != 0xF8) host_writew(&sbuf[0x13],c*h*s);
			// media descriptor
			sbuf[0x15]=mediadesc;
			// sectors per FAT
			// needed entries: (sectors per cluster)
			Bitu sect_per_fat=0;
			Bitu clusters = (sectors-1)/sbuf[0x0d]; // TODO subtract root dir too maybe
			if(mediadesc == 0xF8) sect_per_fat = (clusters*2)/512+1;
			else sect_per_fat = ((clusters*3)/2)/512+1;
			host_writew(&sbuf[0x16],sect_per_fat);
			// sectors per track
			host_writew(&sbuf[0x18],s);
			// heads
			host_writew(&sbuf[0x1a],h);
			// hidden sectors
			host_writed(&sbuf[0x1c],bootsect_pos);
			// sectors (large disk) - this is the same as partition length in MBR
			if(mediadesc == 0xF8) host_writed(&sbuf[0x20],sectors-s);
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
			if(mediadesc == 0xF8) sprintf((char*)&sbuf[0x36],"FAT16   ");
			else sprintf((char*)&sbuf[0x36],"FAT12   ");
			// boot sector signature
			host_writew(&sbuf[0x1fe],0xAA55);

			// write the boot sector
			fseeko64(f,bootsect_pos*512,SEEK_SET);
			fwrite(&sbuf,512,1,f);
			// write FATs
			memset(sbuf,0,512);
			if(mediadesc == 0xF8) host_writed(&sbuf[0],0xFFFFFFF8);
			else host_writed(&sbuf[0],0xFFFFF0);
			// 1st FAT
			fseeko64(f,(bootsect_pos+1)*512,SEEK_SET);
			fwrite(&sbuf,512,1,f);
			// 2nd FAT
			fseeko64(f,(bootsect_pos+1+sect_per_fat)*512,SEEK_SET);
			fwrite(&sbuf,512,1,f);
		}
		// write VHD footer if requested, largely copied from RAW2VHD program, no license was included
		if((mediadesc = 0xF8) && (temp_line.find(".vhd"))) {
			int i;
			Bit8u footer[512];
			// basic information
			memcpy(footer,"conectix" "\0\0\0\2\0\1\0\0" "\xff\xff\xff\xff\xff\xff\xff\xff" "????rawv" "\0\1\0\0Wi2k",40);
			memset(footer+40,0,512-40);
			// time
			struct tm tm20000101 = { 0,0,0, 1,0,100, 0,0,0 };
			time_t basetime = mktime(&tm20000101);
			time_t vhdtime = time(NULL) - basetime;
#if defined (_MSC_VER)
			*(Bit32u*)(footer+0x18) = SDL_SwapBE32((__time32_t)vhdtime);
#else
			*(Bit32u*)(footer+0x18) = SDL_SwapBE32((long)vhdtime);
#endif
			// size and geometry
			*(Bit64u*)(footer+0x30) = *(Bit64u*)(footer+0x28) = SDL_SwapBE64(size);

			*(Bit16u*)(footer+0x38) = SDL_SwapBE16(c);
			*(Bit8u*)( footer+0x3A) = h;
			*(Bit8u*)( footer+0x3B) = s;
			*(Bit32u*)(footer+0x3C) = SDL_SwapBE32(2);

			// generate UUID
			for (i=0; i<16; ++i) {
				*(footer+0x44+i) = (Bit8u)(rand()>>4);
			}

			// calculate checksum
			Bit32u sum;
			for (i=0,sum=0; i<512; ++i) {
				sum += footer[i];
			}

			*(Bit32u*)(footer+0x40) = SDL_SwapBE32(~sum);

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
				return;
			}
			fprintf(f,"imgmount c %s -size 512,%u,%u,%u\r\n",temp_line.c_str(),s,h,c);
			fclose(f);
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

// LOADFIX

class LOADFIX : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void) 
{
	Bit16u commandNr	= 1;
	Bit16u kb			= 64;
	if (cmd->FindCommand(commandNr,temp_line)) {
		if (temp_line[0]=='-') {
			char ch = temp_line[1];
			if ((*upcase(&ch)=='D') || (*upcase(&ch)=='F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	Bit16u segment;
	Bit16u blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		DOS_MCB mcb((Bit16u)(segment-1));
		mcb.SetPSPSeg(0x40);			// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			safe_strncpy(filename,temp_line.c_str(),128);
			// Setup commandline
			bool ok;
			char args[256];
			args[0] = 0;
			do {
				ok = cmd->FindCommand(commandNr++,temp_line);
				if(sizeof(args)-strlen(args)-1 < temp_line.length()+1)
					break;
				strcat(args,temp_line.c_str());
				strcat(args," ");
			} while (ok);			
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
	bool all = false;
	
	Bit8u drive = DOS_GetDefaultDrive();
	
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
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	} else {
		if (drive < DOS_DRIVES && Drives[drive]) {
			Drives[drive]->EmptyCache();
			WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
		}
	}
}

static void RESCAN_ProgramStart(Program * * make) {
	*make=new RESCAN;
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
		Bit8u c;Bit16u n=1;
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

    bool CON_IN(Bit8u * data) {
        Bit8u c;
        Bit16u n=1;

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
		std::string menuname = "BASIC"; // default
		/* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
		if(DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
		if(cmd->FindExist("cdrom",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
			return;
		}
		if(cmd->FindExist("mount",false)) {
			WriteOut("\033[2J");//Clear screen before printing
			DisplayMount();
			return;
		}
		if(cmd->FindExist("special",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
			return;
		}

		if(cmd->FindExist("usage",false)) { DisplayUsage(); return; }
		Bit8u c;Bit16u n=1;

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
		CURSOR("SPECIAL")
		CURSOR("USAGE")
		DisplayMenuNone(); // None
		CURSOR("INFO")
		CURSOR("QUIT")
		DisplayMenuNone(); // None

		if (menuname=="BASIC") goto basic;
		else if (menuname=="CDROM") goto cdrom;
		else if (menuname=="SPECIAL") goto special;
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
			case 0xD:	// Run
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
			case 0x50|0x80: menuname="SPECIAL"; goto menufirst; // Down
			case 0xD:	// Run
				WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
				DOS_ReadFile (STDIN,&c,&n);
				goto menufirst;
		} while (CON_IN(&c));

special:
		menuname="SPECIAL";
		WriteOut(MSG_Get("PROGRAM_INTRO_MENU_SPECIAL_HELP")); 
        CON_IN(&c);
		do switch (c) {
			case 0x48|0x80: menuname="CDROM"; goto menufirst; // Up
			case 0x50|0x80: menuname="USAGE"; goto menufirst; // Down
			case 0xD:	// Run
				WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
				DOS_ReadFile (STDIN,&c,&n);
				goto menufirst;
		} while (CON_IN(&c));

usage:
		menuname="USAGE";
		WriteOut(MSG_Get("PROGRAM_INTRO_MENU_USAGE_HELP")); 
        CON_IN(&c);
		do switch (c) {
			case 0x48|0x80: menuname="SPECIAL"; goto menufirst; // Up
			case 0x50|0x80: menuname="INFO"; goto menufirst; // Down
			case 0xD:	// Run
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
			case 0xD:	// Run
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
			case 0xD:	// Run
				menuname="GOTO_EXIT";
				return;
		} while (CON_IN(&c));
	}	
};

bool ElTorito_ChecksumRecord(unsigned char *entry/*32 bytes*/) {
	unsigned int word,chk=0,i;

	for (i=0;i < 16;i++) {
		word = ((unsigned int)entry[0]) + ((unsigned int)entry[1] << 8);
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
	char buffer[2048];
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


/* C++ class implementing El Torito floppy emulation */
class imageDiskElToritoFloppy : public imageDisk {
public:
	/* Read_Sector and Write_Sector take care of geometry translation for us,
	 * then call the absolute versions. So, we override the absolute versions only */
	virtual Bit8u Read_AbsoluteSector(Bit32u sectnum, void * data) {
		unsigned char buffer[2048];

		bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);

		CDROM_Interface *src_drive=NULL;
		if (!GetMSCDEXDrive(CDROM_drive-'A',&src_drive)) return 0x05;

		if (!src_drive->ReadSectorsHost(buffer,false,cdrom_sector_offset+(sectnum>>2)/*512 byte/sector to 2048 byte/sector conversion*/,1))
			return 0x05;

		memcpy(data,buffer+((sectnum&3)*512),512);
		return 0x00;
	}
	virtual Bit8u Write_AbsoluteSector(Bit32u sectnum, void * data) {
		return 0x05; /* fail, read only */
	}
	imageDiskElToritoFloppy(unsigned char new_CDROM_drive,unsigned long new_cdrom_sector_offset,unsigned char floppy_emu_type) : imageDisk(NULL,NULL,0,false) {
		diskimg = NULL;
		sector_size = 512;
		CDROM_drive = new_CDROM_drive;
		cdrom_sector_offset = new_cdrom_sector_offset;
		class_id = ID_EL_TORITO_FLOPPY;

		if (floppy_emu_type == 1) { /* 1.2MB */
			heads = 2;
			cylinders = 80;
			sectors = 15;
		}
		else if (floppy_emu_type == 2) { /* 1.44MB */
			heads = 2;
			cylinders = 80;
			sectors = 18;
		}
		else if (floppy_emu_type == 3) { /* 2.88MB */
			heads = 2;
			cylinders = 80;
			sectors = 36; /* FIXME: right? */
		}
		else {
			heads = 2;
			cylinders = 69;
			sectors = 14;
			LOG_MSG("BUG! unsupported floppy_emu_type in El Torito floppy object\n");
		}

        diskSizeK = (((heads * cylinders * sectors * sector_size) + 1023) / 1024);
		active = true;
	}
	virtual ~imageDiskElToritoFloppy() {
	}

	unsigned long cdrom_sector_offset;
	unsigned char CDROM_drive;
/*
	int class_id;

	bool hardDrive;
	bool active;
	FILE *diskimg;
	std::string diskname;
	Bit8u floppytype;

	Bit32u sector_size;
	Bit32u heads,cylinders,sectors;
	Bit32u reserved_cylinders;
	Bit64u current_fpos; */
};

bool FDC_AssignINT13Disk(unsigned char drv);
bool FDC_UnassignINT13Disk(unsigned char drv);

class IMGMOUNT : public Program {
public:
	void Run(void) {
		//Hack To allow long commandlines
		ChangeToLongCmd();
		/* In secure mode don't allow people to change imgmount points. 
		 * Neither mount nor unmount */
		if(control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}
		DOS_Drive * newdrive = NULL;
		imageDisk * newImage = NULL;
		Bit32u imagesize;
		char drive;
		std::string label;
		std::vector<std::string> paths;
		std::string umount;
		if (cmd->GetCount() == 0 || cmd->FindExist("-?", true) || cmd->FindExist("-help", true)) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_HELP"));
			return;
		}
		/* Check for unmounting */
		if (cmd->FindString("-u",umount,false)) {
			umount[0] = toupper(umount[0]);
			if (isalpha(umount[0])) { /* if it's a drive letter, then traditional usage applies */
				int i_drive = umount[0]-'A';
				if (i_drive < DOS_DRIVES && i_drive >= 0 && Drives[i_drive]) {
					if (i_drive <= 1)
						FDC_UnassignINT13Disk(i_drive);

					switch (DriveManager::UnmountDrive(i_drive)) {
					case 0:
						/* TODO: If the drive letter is also a CD-ROM drive attached to IDE, then let the
							 IDE code know */
						Drives[i_drive] = 0;
						if (i_drive == DOS_GetDefaultDrive()) 
							DOS_SetDrive(toupper('Z') - 'A');
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS"),umount[0]);
						break;
					case 1:
						WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL"));
						break;
					case 2:
						WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
						break;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED"),umount[0]);
				}
			}
			else if (isdigit(umount[0])) { /* DOSBox-X: drives mounted by number (INT 13h) can be unmounted this way */
				WriteOut("Unmounting imgmount by number (INT13h) is not yet implemented");
			}
			else {
				WriteOut("Unknown imgmount unmount usage");
			}
			return;
		}

		unsigned long el_torito_floppy_base=~0UL;
		unsigned char el_torito_floppy_type=0xFF;
		bool ide_slave = false;
		signed char ide_index = -1;
		char el_torito_cd_drive = 0;
		std::string el_torito;
		std::string ideattach="auto";
		std::string type="hdd";
		// default to floppy for drive letters A and B and numbers 0 and 1
		if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) ||
			((temp_line.size()>1) && (temp_line[1]!=':'))) {
			// drive not valid
		} else {
			Bit8u tdr = toupper(temp_line[0]);
			if(tdr=='A'||tdr=='B'||tdr=='0'||tdr=='1') type="floppy";
		}

		cmd->FindString("-el-torito",el_torito,true);
		if (el_torito != "") {
			unsigned char entries[2048],*entry,ent_num=0;
			int header_platform = -1,header_count=0;
			bool header_final = false;
			int header_more = -1;

			el_torito_cd_drive = toupper(el_torito[0]);

			/* must be valid drive letter, C to Z */
			if (!isalpha(el_torito_cd_drive) || el_torito_cd_drive < 'C') {
				WriteOut("-el-torito requires a proper drive letter corresponding to your CD-ROM drive\n");
				return;
			}

			/* drive must not exist (as a hard drive) */
			if (imageDiskList[el_torito_cd_drive-'C'] != NULL) {
				WriteOut("-el-torito CD-ROM drive specified already exists as a non-CD-ROM device\n");
				return;
			}

			bool GetMSCDEXDrive(unsigned char drive_letter,CDROM_Interface **_cdrom);

			/* get the CD-ROM drive */
			CDROM_Interface *src_drive=NULL;
			if (!GetMSCDEXDrive(el_torito_cd_drive-'A',&src_drive)) {
				WriteOut("-el-torito CD-ROM drive specified is not actually a CD-ROM drive\n");
				return;
			}

			/* FIXME: We only support the floppy emulation mode at this time.
			 *        "Superfloppy" or hard disk emulation modes are not yet implemented */
			if (type != "floppy") {
				WriteOut("-el-torito must be used with -t floppy at this time\n");
				return;
			}

			/* Okay. Step #1: Scan the volume descriptors for the Boot Record. */
			unsigned long el_torito_base = 0,boot_record_sector = 0;
			if (!ElTorito_ScanForBootRecord(src_drive,boot_record_sector,el_torito_base)) {
				WriteOut("El Torito boot record not found\n");
				return;
			}

			LOG_MSG("El Torito emulation: Found ISO 9660 Boot Record in sector %lu, pointing to sector %lu\n",
				boot_record_sector,el_torito_base);

			/* Step #2: Parse the records. Each one is 32 bytes long */
			if (!src_drive->ReadSectorsHost(entries,false,el_torito_base,1)) {
				WriteOut("El Torito entries unreadable\n");
				return;
			}

			/* for more information about what this loop is doing, read:
			 * http://download.intel.com/support/motherboards/desktop/sb/specscdrom.pdf
			 */
			/* FIXME: Somebody find me an example of a CD-ROM with bootable code for both x86, PowerPC, and Macintosh.
			 *        I need an example of such a CD since El Torito allows multiple "headers" */
			/* TODO: Is it possible for this record list to span multiple sectors? */
			for (ent_num=0;ent_num < (2048/0x20);ent_num++) {
				entry = entries + (ent_num*0x20);

				if (memcmp(entry,"\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0""\0\0\0\0",32) == 0)
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
					LOG_MSG("El Torito entry: first header platform=0x%02x\n",header_platform);
					header_count++;
				}
				else if (entry[0] == 0x90/*header, more follows*/ || entry[0] == 0x91/*final header*/) {
					if (header_final) {
						LOG_MSG("Warning: El Torito has an additional header past the final header\n");
						continue;
					}

					header_final = (entry[0] == 0x91);
					header_more = ((unsigned int)entry[2]) + (((unsigned int)entry[3]) << 8);
					header_platform = entry[1];
					LOG_MSG("El Torito entry: first header platform=0x%02x more=%u final=%u\n",header_platform,header_more,header_final);
					header_count++;
				}
				else {
					if (header_more == 0) {
						LOG_MSG("El Torito entry: Non-header entry count expired, ignoring record 0x%02x\n",entry[0]);
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
							unsigned char mediatype = entry[1]&0xF;
							unsigned short load_segment = ((unsigned int)entry[2]) + (((unsigned int)entry[3]) << 8);
							unsigned char system_type = entry[4];
							unsigned short sector_count = ((unsigned int)entry[6]) + (((unsigned int)entry[7]) << 8);
							unsigned long load_rba = ((unsigned int)entry[8]) + (((unsigned int)entry[9]) << 8) +
								(((unsigned int)entry[10]) << 16) + (((unsigned int)entry[11]) << 24);

							LOG_MSG("El Torito entry: bootable x86 record mediatype=%u load_segment=0x%04x "
								"system_type=0x%02x sector_count=%u load_rba=%lu\n",
								mediatype,load_segment,system_type,sector_count,load_rba);

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
							LOG_MSG("El Torito entry: ignoring bootable non-x86 (platform_id=0x%02x) record\n",header_platform);
						}
					}
					else {
						LOG_MSG("El Torito entry: ignoring unknown record ID %02x\n",entry[0]);
					}
				}
			}

			if (el_torito_floppy_type == 0xFF || el_torito_floppy_base == ~0UL) {
				WriteOut("El Torito bootable floppy not found\n");
				return;
			}
		}

		std::string fstype="fat";
		cmd->FindString("-t",type,true);
		cmd->FindString("-fs",fstype,true);
		if(type == "cdrom") type = "iso"; //Tiny hack for people who like to type -t cdrom
		Bit8u mediaid;
		if (type=="floppy" || type=="hdd" || type=="iso" || type=="ram") {
			Bitu sizes[4] = { 0,0,0,0 };
			bool imgsizedetect=false;
			int reserved_cylinders=0;
			std::string reservecyl;
			std::string str_size;
			mediaid=0xF8;

			/* DOSBox-X: to please certain 32-bit drivers like Windows 3.1 WDCTRL, or to emulate older h/w configurations,
			 *           we allow the user or script to specify the number of reserved cylinders. older BIOSes were known
			 *           to subtract 1 or 2 additional cylinders from the total in the fixed disk param table. the -reservecyl
			 *           option allows the number we subtract from the total in INT 13H to be set */
			cmd->FindString("-reservecyl",reservecyl,true);
			if (reservecyl != "") reserved_cylinders = atoi(reservecyl.c_str());

			/* DOSBox-X: we allow "-ide" to allow controlling which IDE controller and slot to attach the hard disk/CD-ROM to */
			cmd->FindString("-ide",ideattach,true);

			if (ideattach == "auto") {
				if (type == "floppy") {
				}
				else {
					IDE_Auto(ide_index,ide_slave);
				}
				
				LOG_MSG("IDE: index %d slave=%d",ide_index,ide_slave?1:0);
			}
			else if (ideattach != "none" && isdigit(ideattach[0]) && ideattach[0] > '0') { /* takes the form [controller]<m/s> such as: 1m for primary master */
				ide_index = ideattach[0] - '1';
				if (ideattach.length() >= 1) ide_slave = (ideattach[1] == 's');
				LOG_MSG("IDE: index %d slave=%d",ide_index,ide_slave?1:0);
			}

			if (type=="floppy") {
				mediaid=0xF0;
				ideattach="none";
			} else if (type=="iso") {
				str_size=="2048,1,60000,0";	// ignored, see drive_iso.cpp (AllocationInfo)
				mediaid=0xF8;		
				fstype = "iso";
			} 
			cmd->FindString("-size",str_size,true);
			if ((type=="hdd") && (str_size.size()==0)) {
				imgsizedetect=true;
			}
			else if (type == "ram") {
				const char * conv = str_size.c_str();
				sizes[0] = atoi(conv);
			} else {
				char number[20];
				const char * scan=str_size.c_str();
				Bitu index=0;Bitu count=0;
				
				while (*scan) {
					if (*scan==',') {
						number[index]=0;sizes[count++]=atoi(number);
						index=0;
					} else number[index++]=*scan;
					scan++;
				}
				number[index]=0;sizes[count++]=atoi(number);
			}
		
			if(fstype=="fat" || fstype=="iso") {
				// get the drive letter
				if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) || ((temp_line.size()>1) && (temp_line[1]!=':'))) {
					WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
				drive=toupper(temp_line[0]);
				if (!isalpha(drive)) {
					WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
					return;
				}
			} else if (fstype=="none") {
				cmd->FindCommand(1,temp_line);
				if ((temp_line.size() > 1) || (!isdigit(temp_line[0]))) {
					WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
				drive=temp_line[0];
				if ((drive<'0') || (drive>3+'0')) {
					WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
					return;
				}
			} else {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"),fstype.c_str());
				return;
			}
			
			// find all file parameters, assuming that all option parameters have been removed
			if (type != "ram")
			while(cmd->FindCommand((unsigned int)(paths.size() + 2), temp_line) && temp_line.size()) {
#if defined (WIN32) || defined(OS2)
                /* nothing */
#else
                // Linux: Convert backslash to forward slash
                if (temp_line.size() > 0) {
                    for (size_t i=0;i < temp_line.size();i++) {
                        if (temp_line[i] == '\\')
                            temp_line[i] = '/';
                    }
                }
#endif

				pref_struct_stat test;
				if (pref_stat(temp_line.c_str(),&test)) {
					//See if it works if the ~ are written out
					std::string homedir(temp_line);
					Cross::ResolveHomedir(homedir);
					if(!pref_stat(homedir.c_str(),&test)) {
						temp_line = homedir;
					} else {
						// convert dosbox filename to system filename
						char fullname[CROSS_LEN];
						char tmp[CROSS_LEN];
						safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);

						Bit8u dummy;
						if (!DOS_MakeName(tmp, fullname, &dummy) || strncmp(Drives[dummy]->GetInfo(),"local directory",15)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
							return;
						}

						localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
						if (ldp==NULL) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
							return;
						}
						ldp->GetSystemFilename(tmp, fullname);
						temp_line = tmp;

						if (pref_stat(temp_line.c_str(),&test)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
							return;
						}
					}
				}
				if ((test.st_mode & S_IFDIR)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT"));
					return;
				}
				paths.push_back(temp_line);
			}

			if (el_torito != "") {
				if (paths.size() != 0) {
					WriteOut("Do not specify files when mounting virtual floppy disk images from El Torito bootable CDs\n");
					return;
				}
			}
			else if (type != "ram") {
                if (paths.size() == 0) {
                    WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
                    return;	
                }
                if (paths.size() == 1)
                    temp_line = paths[0];
            }
			else {
				temp_line = "";
			}

            if(fstype=="fat") {
                if (el_torito != "") {
                    if (Drives[drive-'A']) {
                        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
                        return;
                    }

                    newImage = new imageDiskElToritoFloppy(el_torito_cd_drive,el_torito_floppy_base,el_torito_floppy_type);
                    newImage->Addref();

                    DOS_Drive* newDrive = new fatDrive(newImage,sizes[0],sizes[1],sizes[2],sizes[3],0);
                    if(!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
                        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
                        newImage->Release();
                        return;
                    }

                    DriveManager::AppendDisk(drive - 'A', newDrive);
                    DriveManager::InitializeDrive(drive - 'A');

                    // Set the correct media byte in the table 
                    mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 2, mediaid);

                    /* Command uses dta so set it to our internal dta */
                    RealPt save_dta = dos.dta();
                    dos.dta(dos.tables.tempdta);

                    {
                        DriveManager::CycleAllDisks();

                        char root[4] = {drive, ':', '\\', 0};
                        DOS_FindFirst(root, DOS_ATTR_VOLUME); // force obtaining the label and saving it in dirCache
                    }
                    dos.dta(save_dta);
                }
                else {
                    /* .HDI images contain the geometry explicitly in the header. */
                    if (str_size.size() == 0) {
                        const char *ext = strrchr(temp_line.c_str(),'.');
                        if (ext != NULL) {
                            if (!strcasecmp(ext,".hdi")) {
                                imgsizedetect = false;
                            }
                        }
                    }

                    if (imgsizedetect) {
                        bool yet_detected = false;
                        FILE * diskfile = fopen64(temp_line.c_str(), "rb+");
                        if(!diskfile) {
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
                            return;
                        }
                        fseeko64(diskfile, 0L, SEEK_END);
                        Bit32u fcsize = (Bit32u)(ftello64(diskfile) / 512L);
                        Bit8u buf[512];
                        // check for vhd signature
                        fseeko64(diskfile, -512, SEEK_CUR);
                        if (fread(buf,sizeof(Bit8u),512,diskfile)<512) {
                            fclose(diskfile);
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
                            return;
                        }
                        if(!strcmp((const char*)buf,"conectix")) {
                            fcsize--;	// skip footer (512 bytes)
                            sizes[0]=512;	// sector size
                            sizes[1]=buf[0x3b];	// sectors
                            sizes[2]=buf[0x3a];	// heads
                            sizes[3]=SDL_SwapBE16(*(Bit16s*)(buf + 0x38));	// cylinders

                            // Do translation (?)
                            while((sizes[2] < 128) && (sizes[3] > 1023)) {
                                sizes[2]<<=1;
                                sizes[3]>>=1;
                            }

                            if (sizes[3]>1023) {
                                // Set x/255/63
                                sizes[2] = 255;
                                sizes[3] = fcsize/sizes[2]/sizes[1];
                            }

                            LOG_MSG("VHD image detected: %u,%u,%u,%u",
                                    (unsigned int)sizes[0], (unsigned int)sizes[1], (unsigned int)sizes[2], (unsigned int)sizes[3]);
                            if(sizes[3]>1023) LOG_MSG("WARNING: cylinders>1023, INT13 will not work unless extensions are used");
                            yet_detected = true;
                        }

                        fseeko64(diskfile, 0L, SEEK_SET);
                        if (fread(buf,sizeof(Bit8u),512,diskfile)<512) {
                            fclose(diskfile);
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
                            return;
                        }
                        fclose(diskfile);
                        // check it is not dynamic VHD image
                        if(!strcmp((const char*)buf,"conectix")) {
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
                            LOG_MSG("Dynamic VHD images are not supported");
                            return;
                        }
                        // check MBR signature for unknown images
                        if (!yet_detected && ((buf[510]!=0x55) || (buf[511]!=0xaa))) {
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
                            return;
                        }
                        // check MBR partition entry 1
                        Bitu starthead = buf[0x1bf];
                        Bitu startsect = (buf[0x1c0]&0x3f)-1;
                        Bitu startcyl = buf[0x1c1]|((buf[0x1c0]&0xc0)<<2);
                        Bitu endcyl = buf[0x1c5]|((buf[0x1c4]&0xc0)<<2);

                        Bitu heads = buf[0x1c3]+1;
                        Bitu sectors = buf[0x1c4]&0x3f;

                        Bitu pe1_size = host_readd(&buf[0x1ca]);
                        if(pe1_size!=0) {
                            Bitu part_start = startsect + sectors*starthead +
                                startcyl*sectors*heads;
                            Bitu part_end = heads*sectors*endcyl;
                            Bits part_len = part_end - part_start;
                            // partition start/end sanity check
                            // partition length should not exceed file length
                            // real partition size can be a few cylinders less than pe1_size
                            // if more than 1023 cylinders see if first partition fits
                            // into 1023, else bail.
                            if((part_len<0)||((Bitu)part_len > pe1_size)||(pe1_size > fcsize)||
                                    ((pe1_size-part_len)/(sectors*heads)>2)||
                                    ((pe1_size/(heads*sectors))>1023)) {
                                //LOG_MSG("start(c,h,s) %u,%u,%u",startcyl,starthead,startsect);
                                //LOG_MSG("endcyl %u heads %u sectors %u",endcyl,heads,sectors);
                                //LOG_MSG("psize %u start %u end %u",pe1_size,part_start,part_end);
                            } else if (!yet_detected) {
                                sizes[0]=512; sizes[1]=sectors;
                                sizes[2]=heads; sizes[3]=(Bit16u)(fcsize/(heads*sectors));
                                if(sizes[3]>1023) sizes[3]=1023;
                                yet_detected = true;
                            }
                        }
                        if(!yet_detected) {
                            // Try bximage disk geometry
                            Bitu cylinders=(Bitu)(fcsize/(16*63));
                            // Int13 only supports up to 1023 cylinders
                            // For mounting unknown images we could go up with the heads to 255
                            if ((cylinders*16*63==fcsize)&&(cylinders<1024)) {
                                yet_detected=true;
                                sizes[0]=512; sizes[1]=63; sizes[2]=16; sizes[3]=cylinders;
                            }
                        }

                        if(yet_detected)
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_AUTODET_VALUES"),sizes[0],sizes[1],sizes[2],sizes[3]);


                        //"Image geometry auto detection: -size %u,%u,%u,%u\r\n",
                        //sizes[0],sizes[1],sizes[2],sizes[3]);
                        else {
                            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
                            return;
                        }
                    }

                    if (Drives[drive-'A']) {
                        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
                        return;
                    }

                    std::vector<DOS_Drive*> imgDisks;
                    std::vector<std::string>::size_type i;
                    std::vector<DOS_Drive*>::size_type ct;

					if (type == "ram") {
						//imageDiskMemory* dsk = new imageDiskMemory(sizes[3], sizes[2], sizes[1], sizes[0]);
						imageDiskMemory* dsk = new imageDiskMemory(sizes[0]);
						if (!dsk->active || (dsk->Format() != 0x00)) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
							delete dsk;
							return;
						}
						//dsk->Addref(); //fatDrive will manage reference count
						DOS_Drive* newDrive = new fatDrive(dsk, dsk->sector_size, dsk->sectors, dsk->heads, dsk->cylinders, 0);
						imgDisks.push_back(newDrive);
						if (!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
							WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
							delete newDrive; //this executes dsk.Release() which executes delete dsk
							return;
						}
					}
					else {
						for (i = 0; i < paths.size(); i++) {
							DOS_Drive* newDrive = new fatDrive(paths[i].c_str(), sizes[0], sizes[1], sizes[2], sizes[3], 0);
							imgDisks.push_back(newDrive);
							if (!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
								WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
								for (ct = 0; ct < imgDisks.size(); ct++) {
									delete imgDisks[ct];
								}
								return;
							}
						}
					}

                    // Update DriveManager
                    for(ct = 0; ct < imgDisks.size(); ct++) {
                        DriveManager::AppendDisk(drive - 'A', imgDisks[ct]);
                    }
                    DriveManager::InitializeDrive(drive - 'A');

                    // Set the correct media byte in the table 
                    mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 2, mediaid);

                    /* Command uses dta so set it to our internal dta */
                    RealPt save_dta = dos.dta();
                    dos.dta(dos.tables.tempdta);

                    for(ct = 0; ct < imgDisks.size(); ct++) {
                        DriveManager::CycleAllDisks();

                        char root[4] = {drive, ':', '\\', 0};
                        DOS_FindFirst(root, DOS_ATTR_VOLUME); // force obtaining the label and saving it in dirCache
                    }
                    dos.dta(save_dta);

					if (type == "ram") {
						WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, "ram drive");
					}
					else {
						std::string tmp(paths[0]);
						for (i = 1; i < paths.size(); i++) {
							tmp += "; " + paths[i];
						}
						WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());
					}

                    if (type != "ram" && paths.size() == 1) {
                        newdrive = imgDisks[0];
                        if(((fatDrive *)newdrive)->loadedDisk->hardDrive) {
                            if(imageDiskList[2] == NULL) {
                                imageDiskList[2] = ((fatDrive *)newdrive)->loadedDisk;
                                imageDiskList[2]->Addref();
                                // If instructed, attach to IDE controller as ATA hard disk
                                if (ide_index >= 0) IDE_Hard_Disk_Attach(ide_index,ide_slave,2);
                                updateDPT();
                                return;
                            }
                            if(imageDiskList[3] == NULL) {
                                imageDiskList[3] = ((fatDrive *)newdrive)->loadedDisk;
                                imageDiskList[3]->Addref();
                                // If instructed, attach to IDE controller as ATA hard disk
                                if (ide_index >= 0) IDE_Hard_Disk_Attach(ide_index,ide_slave,3);
                                updateDPT();
                                return;
                            }
                        }
                        if(!((fatDrive *)newdrive)->loadedDisk->hardDrive) {
                            imageDiskList[0] = ((fatDrive *)newdrive)->loadedDisk;
                            imageDiskList[0]->Addref();
                        }
                    }
                }
			} else if (fstype=="iso") {
				if (el_torito != "") {
					WriteOut("El Torito bootable CD: -fs iso mounting not supported\n"); /* <- NTS: Will never implement, either */
					return;
				}

				if (Drives[drive-'A']) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
					return;
				}
				MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
				// create new drives for all images
				std::vector<DOS_Drive*> isoDisks;
				std::vector<std::string>::size_type i;
				std::vector<DOS_Drive*>::size_type ct;
				for (i = 0; i < paths.size(); i++) {
					int error = -1;
					DOS_Drive* newDrive = new isoDrive(drive, paths[i].c_str(), mediaid, error);
					isoDisks.push_back(newDrive);
					switch (error) {
						case 0  :	break;
						case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
						case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
						case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_OPEN"));				break;
						case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
						case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
						case 6  :	WriteOut(MSG_Get("MSCDEX_INVALID_FILEFORMAT"));		break;
						default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
					}
					// error: clean up and leave
					if (error) {
						for(ct = 0; ct < isoDisks.size(); ct++) {
							delete isoDisks[ct];
						}
						return;
					}
				}
				// Update DriveManager
				for(ct = 0; ct < isoDisks.size(); ct++) {
					DriveManager::AppendDisk(drive - 'A', isoDisks[ct]);
				}
				DriveManager::InitializeDrive(drive - 'A');
				
				// Set the correct media byte in the table 
				mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 2, mediaid);
				
				// If instructed, attach to IDE controller as ATAPI CD-ROM device
				if (ide_index >= 0) IDE_CDROM_Attach(ide_index,ide_slave,drive - 'A');

				// Print status message (success)
				WriteOut(MSG_Get("MSCDEX_SUCCESS"));
				std::string tmp(paths[0]);
				for (i = 1; i < paths.size(); i++) {
					tmp += "; " + paths[i];
				}
				WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());
			} else if (el_torito != "") {
				newImage = new imageDiskElToritoFloppy(el_torito_cd_drive,el_torito_floppy_base,el_torito_floppy_type);
				newImage->Addref();
			} else {
				if (el_torito != "") {
					WriteOut("El Torito bootable CD: -fs none unexpected path (BUG)\n");
					return;
				}

				/* auto-fill: sector size */
				if (sizes[0] == 0) sizes[0] = 512;

				FILE *newDisk = fopen64(temp_line.c_str(), "rb+");

				QCow2Image::QCow2Header qcow2_header = QCow2Image::read_header(newDisk);
				
				Bit64u sectors;
				if (qcow2_header.magic == QCow2Image::magic && (qcow2_header.version == 2 || qcow2_header.version == 3)){
					Bit32u cluster_size = 1 << qcow2_header.cluster_bits;
					if ((sizes[0] < 512) || ((cluster_size % sizes[0]) != 0)){
						WriteOut("Sector size must be larger than 512 bytes and evenly divide the image cluster size of %lu bytes.\n", cluster_size);
						return;
					}
					sectors = (Bit64u)qcow2_header.size / (Bit64u)sizes[0];
					imagesize = (Bit32u)(qcow2_header.size / 1024L);
					setbuf(newDisk,NULL);
					newImage = new QCow2Disk(qcow2_header, newDisk, (Bit8u *)temp_line.c_str(), imagesize, sizes[0], (imagesize > 2880));
				}
				else{
					fseeko64(newDisk,0L, SEEK_END);
					sectors = (Bit64u)ftello64(newDisk) / (Bit64u)sizes[0];
					imagesize = (Bit32u)(sectors / 2); /* orig. code wants it in KBs */
					setbuf(newDisk,NULL);
					newImage = new imageDisk(newDisk, (Bit8u *)temp_line.c_str(), imagesize, (imagesize > 2880));
				}
				
				newImage->Addref();

				/* auto-fill: sector/track count */
				if (sizes[1] == 0) sizes[1] = 63;
				/* auto-fill: head/cylinder count */
				if (sizes[3]/*cylinders*/ == 0 && sizes[2]/*heads*/ == 0) {
					sizes[2] = 16; /* typical hard drive, unless a very old drive */
					sizes[3]/*cylinders*/ = (Bitu)((Bit64u)sectors / (Bit64u)sizes[2]/*heads*/ / (Bit64u)sizes[1]/*sectors/track*/);

					/* INT 13h mapping, deal with 1024-cyl limit */
					while (sizes[3] > 1024) {
						if (sizes[2] >= 255) break; /* nothing more we can do */

						/* try to generate head count 16, 32, 64, 128, 255 */
						sizes[2]/*heads*/ *= 2;
						if (sizes[2] >= 256) sizes[2] = 255;

						/* and recompute cylinders */
						sizes[3]/*cylinders*/ = (Bitu)((Bit64u)sectors / (Bit64u)sizes[2]/*heads*/ / (Bit64u)sizes[1]/*sectors/track*/);
					}
				}

				LOG(LOG_MISC, LOG_NORMAL)("Mounting image as C/H/S %u/%u/%u with %u bytes/sector",
					(unsigned int)sizes[3],(unsigned int)sizes[2],(unsigned int)sizes[1],(unsigned int)sizes[0]);

				if(imagesize>2880) newImage->Set_Geometry(sizes[2],sizes[3],sizes[1],sizes[0]);
				if (reserved_cylinders > 0) newImage->Set_Reserved_Cylinders(reserved_cylinders);
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED"),type.c_str());
			return;
		}

		if (fstype=="none") {
			/* TODO: Notify IDE ATA emulation if a drive is already there */
			if(imageDiskList[drive-'0'] != NULL) imageDiskList[drive-'0']->Release();
			imageDiskList[drive-'0'] = newImage;
			updateDPT();
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),drive-'0',temp_line.c_str());
			// If instructed, attach to IDE controller as ATA hard disk
			if (ide_index >= 0) IDE_Hard_Disk_Attach(ide_index,ide_slave,drive-'0');
		}
		else {
			if (newImage != NULL) newImage->Release();
		}

		// let FDC know if we mounted a floppy
		if (type == "floppy") {
			if (drive >= '0' && drive <= '1')
				FDC_AssignINT13Disk(drive-'0');
			else if (drive >= 'A' && drive <= 'B')
				FDC_AssignINT13Disk(drive-'A');
			else if (drive >= 'a' && drive <= 'b')
				FDC_AssignINT13Disk(drive-'a');
		}

		// check if volume label is given. becareful for cdrom
		//if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
		return;
	}
};

void IMGMOUNT_ProgramStart(Program * * make) {
	*make=new IMGMOUNT;
}

Bitu DOS_SwitchKeyboardLayout(const char* new_layout, Bit32s& tried_cp);
Bitu DOS_LoadKeyboardLayout(const char * layoutname, Bit32s codepage, const char * codepagefile);
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
			Bit32s tried_cp = -1;
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
					break;
				case KEYB_FILENOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
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

void MODE::Run(void) {
	Bit16u rate=0,delay=0,mode;
	if (!cmd->FindCommand(1,temp_line) || temp_line=="/?") {
		WriteOut(MSG_Get("PROGRAM_MODE_USAGE"));
		return;
	}
	else if (strcasecmp(temp_line.c_str(),"con")==0 || strcasecmp(temp_line.c_str(),"con:")==0) {
		if (cmd->GetCount()!=3) goto modeparam;
		if (cmd->FindStringBegin("rate=", temp_line,false)) rate= atoi(temp_line.c_str());
		if (cmd->FindStringBegin("delay=",temp_line,false)) delay=atoi(temp_line.c_str());
		if (rate<1 || rate>32 || delay<1 || delay>4) goto modeparam;
		IO_Write(0x60,0xf3); IO_Write(0x60,(Bit8u)(((delay-1)<<5)|(32-rate)));
		return;
	}
	else if (cmd->GetCount()>1) goto modeparam;
	else if (strcasecmp(temp_line.c_str(),"mono")==0) mode=7;
	else if (machine==MCH_HERC) goto modeparam;
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
	Bit16u ncols=mem_readw(BIOS_SCREEN_COLUMNS);
	Bit16u nrows=(Bit16u)mem_readb(BIOS_ROWS_ON_SCREEN_MINUS_1);
	Bit16u col=1,row=1;
	Bit8u c;Bit16u n=1;
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

class NMITEST : public Program {
public:
	void Run(void) {
		CPU_Raise_NMI();
	}
};

static void NMITEST_ProgramStart(Program * * make) {
	*make=new NMITEST;
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
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1","The currently mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_USAGE",
		"Usage \033[34;1mMOUNT Drive-Letter Local-Directory\033[0m\n"
		"For example: MOUNT c %s\n"
		"This makes the directory %s act as the C: drive inside DOSBox.\n"
		"The directory has to exist.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","\033[31;1mMounting c:\\ is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","\033[31;1mMounting / is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC","%d kb allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC","%d kb freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

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
		"\033[2J\033[32;1mWelcome to DOSBox\033[0m, an x86 emulator with sound and graphics.\n"
		"DOSBox creates a shell for you which looks like old plain DOS.\n"
		"\n"
		"\033[31;1mDOSBox will stop/exit without a warning if an error occurred!\033[0m\n"
		"\n"
		"\n" );
	if (machine == MCH_PC98) {
		MSG_Add("PROGRAM_INTRO_MENU_UP",
			"\033[44m\033[K\033[0m\n"
			"\033[44m\033[K\033[1m\033[1m\t\t\t\t\t\t\t  DOSBox Introduction \033[0m\n"
			"\033[44m\033[K\033[1m\033[1m \x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\x86\x43\033[0m\n"
			"\033[44m\033[K\033[0m\n"
			);
	} else {
		MSG_Add("PROGRAM_INTRO_MENU_UP",
			"\033[44m\033[K\033[0m\n"
			"\033[44m\033[K\033[1m\033[1m\t\t\t\t\t\t\t  DOSBox Introduction \033[0m\n"
			"\033[44m\033[K\033[1m\033[1m \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\033[0m\n"
			"\033[44m\033[K\033[0m\n"
			);
	}
	MSG_Add("PROGRAM_INTRO_MENU_BASIC","Basic mount");
	MSG_Add("PROGRAM_INTRO_MENU_CDROM","CD-ROM support");
	MSG_Add("PROGRAM_INTRO_MENU_SPECIAL","Special keys");
	MSG_Add("PROGRAM_INTRO_MENU_USAGE","Usage");
	MSG_Add("PROGRAM_INTRO_MENU_INFO","Information");
	MSG_Add("PROGRAM_INTRO_MENU_QUIT","Quit");
	MSG_Add("PROGRAM_INTRO_MENU_BASIC_HELP","\n\033[1m   \033[1m\033[KMOUNT allows you to connect real hardware to DOSBox's emulated PC.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MENU_CDROM_HELP","\n\033[1m   \033[1m\033[KTo mount your CD-ROM in DOSBox, you have to specify some additional options\n   when mounting the CD-ROM.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MENU_SPECIAL_HELP","\n\033[1m   \033[1m\033[KSpecial key combinations used in DOSBox.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MENU_USAGE_HELP","\n\033[1m   \033[1m\033[KAn overview of the command line options you can give to DOSBox.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MENU_INFO_HELP","\n\033[1m   \033[1m\033[KHow to get more information about DOSBox.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MENU_QUIT_HELP","\n\033[1m   \033[1m\033[KExit from Intro.\033[0m\n");
	MSG_Add("PROGRAM_INTRO_USAGE_TOP",
		"\033[2J\033[32;1mAn overview of the command line options you can give to DOSBox.\033[0m\n"
		"Windows Users must open cmd.exe or command.com or edit the shortcut to\n"
		"DOSBox.exe for this.\n\n"
		"dosbox [name] [-exit] [-c command] [-fullscreen] [-conf congfigfile]\n"
		"       [-lang languagefile] [-machine machinetype] [-noconsole]\n"
		"       [-startmapper] [-noautoexec] [-scaler scaler | -forcescaler scaler]\n       [-version]\n\n"
		);
	MSG_Add("PROGRAM_INTRO_USAGE_1",
		"\033[33;1m  name\033[0m\n"
		"\tIf name is a directory it will mount that as the C: drive.\n"
		"\tIf name is an executable it will mount the directory of name\n"
		"\tas the C: drive and execute name.\n\n"
		"\033[33;1m  -exit\033[0m\n"
		"\tDOSBox will close itself when the DOS application name ends.\n\n"
		"\033[33;1m  -c\033[0m command\n"
		"\tRuns the specified command before running name. Multiple commands\n"
		"\tcan be specified. Each command should start with -c, though.\n"
		"\tA command can be: an Internal Program, a DOS command or an executable\n"
		"\ton a mounted drive.\n"
		);
	MSG_Add("PROGRAM_INTRO_USAGE_2",
		"\033[33;1m  -fullscreen\033[0m\n"
		"\tStarts DOSBox in fullscreen mode.\n\n"
		"\033[33;1m  -conf\033[0m configfile\n"
		"\tStart DOSBox with the options specified in configfile.\n"
		"\tSee README for more details.\n\n"
		"\033[33;1m  -lang\033[0m languagefile\n"
		"\tStart DOSBox using the language specified in languagefile.\n\n"
		"\033[33;1m  -noconsole\033[0m (Windows Only)\n"
		"\tStart DOSBox without showing the console window. Output will\n"
		"\tbe redirected to stdout.txt and stderr.txt\n"
		);
	MSG_Add("PROGRAM_INTRO_USAGE_3",
		"\033[33;1m  -machine\033[0m machinetype\n"
		"\tSetup DOSBox to emulate a specific type of machine. Valid choices are:\n"
		"\thercules, cga, pcjr, tandy, vga (default). The machinetype affects\n"
		"\tboth the videocard and the available soundcards.\n\n"
		"\033[33;1m  -startmapper\033[0m\n"
		"\tEnter the keymapper directly on startup. Useful for people with\n"
		"\tkeyboard problems.\n\n"
		"\033[33;1m  -noautoexec\033[0m\n"
		"\tSkips the [autoexec] section of the loaded configuration file.\n\n"
		"\033[33;1m  -version\033[0m\n"
		"\toutput version information and exit. Useful for frontends.\n"
		);
	MSG_Add("PROGRAM_INTRO_INFO",
		"\033[32;1mInformation:\033[0m\n\n"
		"For information about basic mount, type \033[34;1mintro mount\033[0m\n"
		"For information about CD-ROM support, type \033[34;1mintro cdrom\033[0m\n"
		"For information about special keys, type \033[34;1mintro special\033[0m\n"
		"For information about usage, type \033[34;1mintro usage\033[0m\n\n"
		"For the latest version of DOSBox, go to \033[34;1mhttp://www.dosbox.com\033[0m\n"
		"\n"
		"For more information about DOSBox, read README first!\n"
		"\n"
		"\033[34;1mhttp://www.dosbox.com/wiki\033[0m\n"
		"\033[34;1mhttp://vogons.zetafleet.com\033[0m\n"
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
			"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
			" \033[34;1mcd\033[0m will allow you to\n"
			"enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
			"You can run programs/files which end with \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
			);
	}
	MSG_Add("PROGRAM_INTRO_CDROM",
		"\033[2J\033[32;1mHow to mount a Real/Virtual CD-ROM Drive in DOSBox:\033[0m\n"
		"DOSBox provides CD-ROM emulation on several levels.\n"
		"\n"
		"The \033[33mbasic\033[0m level works on all CD-ROM drives and normal directories.\n"
		"It installs MSCDEX and marks the files read-only.\n"
		"Usually this is enough for most games:\n"
		"\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom\033[0m   or   \033[34;1mmount d C:\\example -t cdrom\033[0m\n"
		"If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
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
	MSG_Add("PROGRAM_INTRO_SPECIAL",
		"\033[2J\033[32;1mSpecial keys:\033[0m\n"
		"These are the default keybindings.\n"
		"They can be changed in the \033[33mkeymapper\033[0m.\n"
		"\n"
		"\033[33;1mALT-ENTER\033[0m   : Go full screen and back.\n"
		"\033[33;1mALT-PAUSE\033[0m   : Pause DOSBox.\n"
		"\033[33;1mCTRL-1~4\033[0m    : Use normal/full/dynamic/simple core.\n"
		"\033[33;1mCTRL-=\033[0m      : Maximize CPU cycles.\n"
		"\033[33;1mALT-F11\033[0m     : Unlock/Lock speed.\n"
		"\033[33;1mCTRL-F1\033[0m     : Start the \033[33mkeymapper\033[0m.\n"
		"\033[33;1mCTRL-F4\033[0m     : Update directory cache for all drives! Swap mounted disk-image.\n"
		"\033[33;1mCTRL-ALT-F5\033[0m : Start/Stop creating a movie of the screen.\n"
		"\033[33;1mCTRL-F5\033[0m     : Save a screenshot.\n"
		"\033[33;1mCTRL-F6\033[0m     : Start/Stop recording sound output to a wave file.\n"
		"\033[33;1mCTRL-ALT-F7\033[0m : Start/Stop recording of OPL commands.\n"
		"\033[33;1mCTRL-ALT-F8\033[0m : Start/Stop the recording of raw MIDI commands.\n"
		"\033[33;1mCTRL-F7\033[0m     : Decrease frameskip.\n"
		"\033[33;1mCTRL-F8\033[0m     : Increase frameskip.\n"
		"\033[33;1mCTRL-F9\033[0m     : Kill DOSBox.\n"
		"\033[33;1mCTRL-F10\033[0m    : Capture/Release the mouse.\n"
		"\033[33;1mCTRL-F11\033[0m    : Slow down emulation (Decrease DOSBox Cycles).\n"
		"\033[33;1mCTRL-F12\033[0m    : Speed up emulation (Increase DOSBox Cycles).\n"
		"\033[33;1mALT-F12\033[0m     : Unlock speed (turbo button/fast forward).\n"
		);
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR","This command boots DOSBox from either a floppy or hard disk image.\n\n"
		"For this command, one can specify a succession of floppy disks swappable\n"
		"by pressing Ctrl-F4, and -l specifies the mounted drive to boot from.  If\n"
		"no drive letter is specified, this defaults to booting from the A drive.\n"
		"The only bootable drive letters are A, C, and D.  For booting from a hard\n"
		"drive (C or D), the image should have already been mounted using the\n"
		"\033[34;1mIMGMOUNT\033[0m command.\n\n"
		"The syntax of this command is:\n\n"
		"\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n"
		);
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS","Available PCjr cartridge commandos:%s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS","No PCjr cartridge commandos found");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE","Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2","Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
		"\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
		"Check that the path is correct and the image is accessible.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
		"Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
	MSG_Add("PROGRAM_IMGMOUNT_AUTODET_VALUES","Image geometry auto detection: -size %u,%u,%u,%u\n");
	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED","Type \"%s\" is unsupported. Specify \"hdd\" or \"floppy\" or\"iso\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host or local drive.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");

	MSG_Add("PROGRAM_IMGMOUNT_HELP",
		"Mounts hard drive and optical disc images.\n\n"
		"IMGMOUNT drive filename [-t floppy] [-fs fat] [-size ss,s,h,c]\n"
		"IMGMOUNT drive filename [-t hdd] [-fs fat] [-size ss,s,h,c] [-ide 1m|1s|2m|2s]\n"
		"IMGMOUNT driveLocation filename [-t hdd] -fs none [-size ss,s,h,c]\n"
		"IMGMOUNT drive filename [-t iso] [-fs iso]\n"
		"IMGMOUNT drive -t floppy -el-torito cdDrive\n"
		"IMGMOUNT drive -t ram -size driveSize\n"
		"IMGMOUNT -u drive|driveLocation\n"
		" drive               Drive letter to mount the image at\n"
		" driveLocation       Location to mount drive, where 2 = Master and 3 = Slave\n"
		" filename            Filename of the image to mount\n"
		" -t iso              Image type is optical disc iso or cue / bin image\n"
		" -t floppy           Image type is floppy\n"
		" -t hdd              Image type is hard disk; VHD and HDI files are supported\n"
		" -t ram              Image type is ramdrive\n"
		" -fs iso             File system is ISO 9660\n"
		" -fs fat             File system is FAT; FAT12 and FAT16 are supported\n"
		" -fs none            Do not detect file system\n"
		" -ide 1m|1s|2m|2s    Specifies the controller to mount drive\n"
		" -size ss,s,h,c      Specify the geometry: Sector size,Sectors,Heads,Cylinders\n"
		" -size driveSize     Specify the drive size in KB\n"
		" -el-torito cdDrive  Specify the CD drive to load the bootable floppy from\n"
		" -u                  Unmount the drive"
	);
	MSG_Add("PROGRAM_IMGMAKE_SYNTAX",
		"Creates floppy or harddisk images.\n"
		"Syntax: IMGMAKE file [-t type] [[-size size] | [-chs geometry]] [-nofs]\n"
		"  [-source source] [-r retries] [-bat]\n"
		"  file: The image file that is to be created - !path on the host!\n"
		"  -type: Type of image.\n"
		"    Floppy templates (names resolve to floppy sizes in kilobytes):\n"
		"     fd_160 fd_180 fd_200 fd_320 fd_360 fd_400 fd_720 fd_1200 fd_1440 fd_2880\n"
		"    Harddisk templates:\n"
		"     hd_250: 250MB image, hd_520: 520MB image, hd_2gig: 2GB image\n"
		"     hd_4gig:  4GB image, hd_8gig: 8GB image (maximum size)\n"
		"     hd_st251: 40MB image, hd_st225: 20MB image (geometry from old drives)\n"
		"    Custom harddisk images:\n"
		"     hd (requires -size or -chs)\n"
		"  -size: size of a custom harddisk image in MB.\n"
		"  -geometry: disk geometry in cylinders(1-1023),heads(1-255),sectors(1-63).\n"
		"  -nofs: add this parameter if a blank image should be created.\n"
		"  -bat: creates a .bat file with the IMGMOUNT command required for this image.\n"
#ifdef WIN32
		"  -source: drive letter - if specified the image is read from a floppy disk.\n"
		"  -retries: how often to retry when attempting to read a bad floppy disk(1-99).\n"
#endif
		" Examples:\n"
		"    imgmake c:\\image.img -t fd_1440          - create a 1.44MB floppy image\n"
		"    imgmake c:\\image.img -t hd -size 100     - create a 100MB hdd image\n"
		"    imgmake c:\\image.img -t hd -chs 130,2,17 - create a special hd image"
#ifdef WIN32
		"\n    imgmake c:\\image.img -source a           - read image from physical drive A"
#endif
		);
#ifdef WIN32
	MSG_Add("PROGRAM_IMGMAKE_FLREAD",
		"Disk geometry: %d Cylinders, %d Heads, %d Sectors, %d Kilobytes\n\n");
	MSG_Add("PROGRAM_IMGMAKE_FLREAD2",
		"\xdb =good, \xb1 =good after retries, ! =CRC error, x =sector not found, ? =unknown\n\n");
#endif
	MSG_Add("PROGRAM_IMGMAKE_FILE_EXISTS","The file \"%s\" already exists.\n");
	MSG_Add("PROGRAM_IMGMAKE_CANNOT_WRITE","The file \"%s\" cannot be opened for writing.\n");
	MSG_Add("PROGRAM_IMGMAKE_NOT_ENOUGH_SPACE","Not enough space availible for the image file. Need %u bytes.\n");
	MSG_Add("PROGRAM_IMGMAKE_PRINT_CHS","Creating an image file with %u cylinders, %u heads and %u sectors\n");
	MSG_Add("PROGRAM_IMGMAKE_CANT_READ_FLOPPY","\n\nUnable to read floppy.");

	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("PROGRAM_KEYB_SHOWHELP",
		"\033[32;1mKEYB\033[0m [keyboard layout ID[ codepage number[ codepage file]]]\n\n"
		"Some examples:\n"
		"  \033[32;1mKEYB\033[0m: Display currently loaded codepage.\n"
		"  \033[32;1mKEYB\033[0m sp: Load the spanish (SP) layout, use an appropriate codepage.\n"
		"  \033[32;1mKEYB\033[0m sp 850: Load the spanish (SP) layout, use codepage 850.\n"
		"  \033[32;1mKEYB\033[0m sp 850 mycp.cpi: Same as above, but use file mycp.cpi.\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");
	MSG_Add("PROGRAM_MODE_USAGE",
			"\033[34;1mMODE\033[0m display-type       :display-type codes are "
			"\033[1mCO80\033[0m, \033[1mBW80\033[0m, \033[1mCO40\033[0m, \033[1mBW40\033[0m, or \033[1mMONO\033[0m\n"
			"\033[34;1mMODE CON RATE=\033[0mr \033[34;1mDELAY=\033[0md :typematic rates, r=1-32 (32=fastest), d=1-4 (1=lowest)\n");
	MSG_Add("PROGRAM_MODE_INVALID_PARAMETERS","Invalid parameter(s).\n");
	//MSG_Add("PROGRAM_MORE_USAGE","Usage: \033[34;1mMORE <\033[0m text-file\n");
	//MSG_Add("PROGRAM_MORE_MORE","-- More --");

	/*regular setup*/
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM",LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM",RESCAN_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM",INTRO_ProgramStart);
	PROGRAMS_MakeFile("BOOT.COM",BOOT_ProgramStart);

    if (!IS_PC98_ARCH)
        PROGRAMS_MakeFile("LDGFXROM.COM", LDGFXROM_ProgramStart);

	PROGRAMS_MakeFile("IMGMAKE.COM", IMGMAKE_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);

    if (!IS_PC98_ARCH)
        PROGRAMS_MakeFile("MODE.COM", MODE_ProgramStart);

	//PROGRAMS_MakeFile("MORE.COM", MORE_ProgramStart);

    if (!IS_PC98_ARCH)
        PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart);

    if (!IS_PC98_ARCH)
        PROGRAMS_MakeFile("MOUSE.COM", MOUSE_ProgramStart);

	PROGRAMS_MakeFile("A20GATE.COM",A20GATE_ProgramStart);
#if !defined(C_SDL2)
	PROGRAMS_MakeFile("SHOWGUI.COM",SHOWGUI_ProgramStart);
#endif
	PROGRAMS_MakeFile("NMITEST.COM",NMITEST_ProgramStart);
    PROGRAMS_MakeFile("RE-DOS.COM",REDOS_ProgramStart);

    if (IS_PC98_ARCH)
        PROGRAMS_MakeFile("PC98UTIL.COM",PC98UTIL_ProgramStart);
}
