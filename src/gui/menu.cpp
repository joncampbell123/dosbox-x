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

#include "../dos/drives.h"
#include "control.h"
#include "cpu.h"
#include "render.h"
#include "menu.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include "bios_disk.h"
#include "ide.h" // for ide support
#include "mapper.h"
#include "keyboard.h"
#include "inout.h"

extern bool dos_kernel_disabled;

std::string MSCDEX_Output(int num) {
	std::string MSCDEX_MSG = "GUI: MSCDEX ";
	std::string MSCDEX_MSG_Failure = "Failure: ";
	switch (num) {
	case 0: return MSCDEX_MSG + "installed";
	case 1: return MSCDEX_MSG + MSCDEX_MSG_Failure + "Drive-letters of multiple CDRom-drives have to be continuous.";
	case 2: return MSCDEX_MSG + MSCDEX_MSG_Failure + "Not yet supported.";
	case 3: return MSCDEX_MSG + MSCDEX_MSG_Failure + "Path not valid.";
	case 4: return MSCDEX_MSG + MSCDEX_MSG_Failure + "Too many CDRom-drives (max: 5). MSCDEX Installation failed";
	case 5: return MSCDEX_MSG + "Mounted subdirectory: limited support.";
	case 6: return MSCDEX_MSG + MSCDEX_MSG_Failure + "Unknown error";
	default: return 0;
	}
}

static std::string not_recommended = "Mounting C:\\ is NOT recommended.\nDo you want to continue?";

void SetVal(const std::string secname, std::string preval, const std::string val) {
	if (dos_kernel_disabled)
		return;

	if(preval=="keyboardlayout") {
		DOS_MCB mcb(dos.psp()-1);
		static char name[9];
		mcb.GetFileName(name);
		if (strlen(name)) {
			LOG_MSG("GUI: Exit %s running in DOSBox, and then try again.",name);
			return;
		}
	}
	Section* sec = control->GetSection(secname);
	sec->ExecuteDestroy(false);
	if(sec) {
        std::string real_val=preval+"="+val;
		sec->HandleInputline(real_val);
		sec->ExecuteInit(false);
	}
}

MENU_Block menu;

#ifdef WIN32
#include <shlobj.h>

extern void RENDER_CallBack( GFX_CallBackFunctions_t function );

unsigned int hdd_defsize=16000;
char hdd_size[20]="";

HWND GetHWND(void) {
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);

	if(!SDL_GetWMInfo(&wmi)) {
		return NULL;
	}
	return wmi.window;
}

void GetDefaultSize(void) {
	char sizetemp[20]="512,32,32765,";
	char sizetemp2[20]="";
	sprintf(sizetemp2,"%d",hdd_defsize);
	strcat(sizetemp,sizetemp2);
	sprintf(hdd_size,sizetemp);
}

void SearchFolder( char path[MAX_PATH], char drive, std::string drive_type ) {
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	hFind = FindFirstFile ( "*.*", &FindFileData );

	if ( hFind != INVALID_HANDLE_VALUE ) MountDrive_2(drive,path,drive_type);
	FindClose ( hFind );
}

void BrowseFolder( char drive , std::string drive_type ) {
	if (Drives[drive-'A']) {
		LOG_MSG("Unmount drive %c first, and then try again.",drive);
		return;
	}
	std::string title = "Select a drive/directory to mount";
	char path[MAX_PATH];
	BROWSEINFO bi = { 0 };
	if(drive_type=="CDROM")
		bi.lpszTitle = ( title + " CD-ROM\nMounting a directory as CD-ROM gives an limited support" ).c_str();
	else if(drive_type=="FLOPPY")
		bi.lpszTitle = ( title + " as Floppy" ).c_str();
	else if(drive_type=="LOCAL")
		bi.lpszTitle = ( title + " as Local").c_str();
	else
		bi.lpszTitle = (title.c_str());
	LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );

	if ( pidl != 0 ) {
		SHGetPathFromIDList ( pidl, path );
//		SetCurrentDirectory ( path );
		SearchFolder( path , drive, drive_type );
		IMalloc * imalloc = 0;
		if ( SUCCEEDED( SHGetMalloc ( &imalloc )) ) {
			imalloc->Free ( pidl );
			imalloc->Release ( );
		}
	}
}

void mem_conf(std::string memtype, int option) {
	std::string tmp;
	Section* sec = control->GetSection("dos");
	Section_prop * section=static_cast<Section_prop *>(sec); 
	if (!option) {
		tmp = section->Get_bool(memtype) ? "false" : "true";
	} else {
		switch (option) {
			case 1: tmp = "true"; break;
			case 2: tmp = "false"; break;
			case 3: tmp = "emsboard"; break;
			case 4: tmp = "emm386"; break;
			default: return;
		}
	}
	if(sec) {
		sec->ExecuteDestroy(false);
		memtype += "=" + tmp;
		sec->HandleInputline(memtype);
		sec->ExecuteInit(false);
	}
}

void UnMount(int i_drive) {
	if (dos_kernel_disabled)
		return;

	i_drive = toupper(i_drive);
	if(i_drive-'A' == DOS_GetDefaultDrive()) {
		DOS_MCB mcb(dos.psp()-1);
		static char name[9];
		mcb.GetFileName(name);
		if (!strlen(name)) goto umount;
		LOG_MSG("GUI:Drive %c is being used. Aborted.",i_drive);
		return;
	};
umount:
	if (i_drive-'A' < DOS_DRIVES && i_drive-'A' >= 0 && Drives[i_drive-'A']) {
		switch (DriveManager::UnmountDrive(i_drive-'A')) {
		case 0:
			Drives[i_drive-'A'] = 0;
			if(i_drive-'A' == DOS_GetDefaultDrive()) 
				DOS_SetDrive(toupper('Z') - 'A');
			LOG_MSG("GUI:Drive %c has succesfully been removed.",i_drive); break;
		case 1:
			LOG_MSG("GUI:Virtual Drives can not be unMOUNTed."); break;
		case 2:
			LOG_MSG(MSCDEX_Output(1).c_str()); break;
		}
	}
}

#include "../dos/cdrom.h"
extern void MSCDEX_SetCDInterface(int intNr, int numCD);
void MountDrive_2(char drive, const char drive2[DOS_PATHLENGTH], std::string drive_type) {
	DOS_Drive * newdrive;
	std::string temp_line;
	std::string str_size;
	Bit16u sizes[4];
	Bit8u mediaid;
	int num = SDL_CDNumDrives();

	if((drive_type=="LOCAL") && (drive2=="C:\\")) {
		if (MessageBox(GetHWND(),not_recommended.c_str(), "Warning", MB_YESNO) == IDNO) return;
	}

	if(drive_type=="CDROM") {
		mediaid=0xF8;		/* Hard Disk */
		str_size="650,127,16513,1700";
	} else {
		if(drive_type=="FLOPPY") {
			str_size="512,1,2847,2847";	/* All space free */
			mediaid=0xF0;			/* Floppy 1.44 media */
		} else if(drive_type=="LOCAL") {
			mediaid=0xF8;
			GetDefaultSize();
			str_size=hdd_size;		/* Hard Disk */
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
	Bit8u bit8size=(Bit8u) sizes[1];

	if(drive_type=="CDROM") {
		num = -1;
		int error;

		int id, major, minor;
		DOSBox_CheckOS(id, major, minor);
		if ((id==VER_PLATFORM_WIN32_NT) && (major>5)) {
			// Vista/above
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
		} else {
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
		}
		newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
		LOG_MSG(MSCDEX_Output(error).c_str());
	} else newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);

	if (!newdrive) E_Exit("DOS:Can't create drive");
	Drives[drive-'A']=newdrive;
	mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,mediaid);
	if(drive_type=="CDROM")
		LOG_MSG("GUI: Drive %c is mounted as CD-ROM",drive);
	else
		LOG_MSG("GUI: Drive %c is mounted as local directory",drive);
    if(drive == drive2[0] && sizeof(drive2) == 4) {
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

void MountDrive(char drive, const char drive2[DOS_PATHLENGTH]) {
	DOS_Drive * newdrive;
	std::string drive_warn="Do you really want to give DOSBox access to";
	std::string temp_line;
	std::string str_size;
	Bit16u sizes[4];
	Bit8u mediaid;
	int num = SDL_CDNumDrives();
	std::string str(1, drive);
	if(GetDriveType(drive2)==DRIVE_CDROM) {
		drive_warn += " your real CD-ROM drive ";
	}
	else if(GetDriveType(drive2)==DRIVE_REMOVABLE) {
		drive_warn += " your real floppy drive ";
	}
	else {
		drive_warn += " everything\non your real drive ";
	}

	if (MessageBox(GetHWND(),(drive_warn+str+"?").c_str(),"Warning",MB_YESNO)==IDNO) return;

	if((GetDriveType(drive2)==DRIVE_FIXED) && (strcasecmp(drive2,"C:\\")==0)) {
		if (MessageBox(GetHWND(), not_recommended.c_str(), "Warning", MB_YESNO) == IDNO) return;
	}

	if(GetDriveType(drive2)==DRIVE_CDROM) {
		mediaid=0xF8;		/* Hard Disk */
		str_size="650,127,16513,1700";
	} else {
		if(GetDriveType(drive2)==DRIVE_REMOVABLE) {
			str_size="512,1,2847,2847";	/* All space free */
			mediaid=0xF0;			/* Floppy 1.44 media */
		} else {
			mediaid=0xF8;
			GetDefaultSize();
			str_size=hdd_size;	/* Hard Disk */
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
	Bit8u bit8size=(Bit8u) sizes[1];

	temp_line = drive2;
	int error; num = -1;
	if(GetDriveType(drive2)==DRIVE_CDROM) {
		int id, major, minor;
		DOSBox_CheckOS(id, major, minor);

		if ((id==VER_PLATFORM_WIN32_NT) && (major>5)) {
			// Vista/above
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
		} else {
			MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
		}
		newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
		LOG_MSG(MSCDEX_Output(error).c_str());
	} else newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);

	if (!newdrive) E_Exit("DOS:Can't create drive");
	if(error && (GetDriveType(drive2)==DRIVE_CDROM)) return;
	Drives[drive-'A']=newdrive;
	mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,mediaid);
	if(GetDriveType(drive2)==DRIVE_CDROM) LOG_MSG("GUI: Drive %c is mounted as CD-ROM %c:\\",drive,drive);
	else LOG_MSG("GUI: Drive %c is mounted as local directory %c:\\",drive,drive);
    if(drive == drive2[0] && sizeof(drive2) == 4) {
        // automatic mount
    } else {
        if(GetDriveType(drive2) == DRIVE_CDROM) return;
        std::string label;
        label = drive;
        if(GetDriveType(drive2) == DRIVE_FIXED)
            label += "_DRIVE";
        else
            label += "_FLOPPY";
        newdrive->SetLabel(label.c_str(),false,true);
    }
}

void Mount_Img_Floppy(char drive, std::string realpath) {
	DOS_Drive * newdrive = NULL;
	imageDisk * newImage = NULL;
	std::string label;
	std::string temp_line = realpath;
	std::vector<std::string> paths;
	std::string umount;
	//std::string type="hdd";
	std::string fstype="fat";
	Bit8u mediaid;
	Bit16u sizes[4];
			
	std::string str_size;
	mediaid=0xF0;
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
	struct stat test;
	if (stat(temp_line.c_str(),&test)) {
		// convert dosbox filename to system filename
		char fullname[CROSS_LEN];
		char tmp[CROSS_LEN];
		safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);
		Bit8u dummy;
		localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
		ldp->GetSystemFilename(tmp, fullname);
		temp_line = tmp;
	}
	paths.push_back(temp_line);
	if (paths.size() == 1)
		temp_line = paths[0];

				std::vector<DOS_Drive*> imgDisks;
				std::vector<std::string>::size_type i;
				std::vector<DOS_Drive*>::size_type ct;
				
				for (i = 0; i < paths.size(); i++) {
					DOS_Drive* newDrive = new fatDrive(paths[i].c_str(),sizes[0],sizes[1],sizes[2],sizes[3],0);
					imgDisks.push_back(newDrive);
					if(!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
						LOG_MSG("Can't create drive from file.");
						for(ct = 0; ct < imgDisks.size(); ct++) {
							delete imgDisks[ct];
						}
						return;
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

				std::string tmp(paths[0]);
				for (i = 1; i < paths.size(); i++) {
					tmp += "; " + paths[i];
				}
				LOG_MSG("Drive %c is mounted as %s", drive, tmp.c_str());

				if (paths.size() == 1) {
					newdrive = imgDisks[0];
					if(((fatDrive *)newdrive)->loadedDisk->hardDrive) {
						if(imageDiskList[2] == NULL) {
							imageDiskList[2] = ((fatDrive *)newdrive)->loadedDisk;
							updateDPT();
							return;
						}
						if(imageDiskList[3] == NULL) {
							imageDiskList[3] = ((fatDrive *)newdrive)->loadedDisk;
							updateDPT();
							return;
						}
					}
					if(!((fatDrive *)newdrive)->loadedDisk->hardDrive) {
						imageDiskList[0] = ((fatDrive *)newdrive)->loadedDisk;
					}
				}
}

void Mount_Img_HDD(char drive, std::string realpath) {
	/* ide support start */
	bool ide_slave = false;
	signed char ide_index = -1;
	std::string ideattach="auto";
	/* ide support end */

	DOS_Drive * newdrive = NULL;
	imageDisk * newImage = NULL;
	std::string label;
	std::string temp_line = realpath;
	std::vector<std::string> paths;
	std::string umount;
	std::string fstype="fat";
	Bit8u mediaid;
	Bit16u sizes[4];
	std::string str_size;
	mediaid=0xF8;

	/* ide support start */
	IDE_Auto(ide_index,ide_slave);
	/* ide support end */

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
	struct stat test;
	if (stat(temp_line.c_str(),&test)) {
		// convert dosbox filename to system filename
		char fullname[CROSS_LEN];
		char tmp[CROSS_LEN];
		safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);
		Bit8u dummy;
		localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
		ldp->GetSystemFilename(tmp, fullname);
		temp_line = tmp;
	}
	paths.push_back(temp_line);
	if (paths.size() == 1)
		temp_line = paths[0];
		FILE * diskfile = fopen64(temp_line.c_str(), "rb+");
		if(!diskfile) {
			LOG_MSG("Could not load image file.");
			return;
		}
		fseeko64(diskfile, 0L, SEEK_END);
		Bit32u fcsize = (Bit32u)(ftello64(diskfile) / 512L);
		Bit8u buf[512];
		fseeko64(diskfile, 0L, SEEK_SET);
		if (fread(buf,sizeof(Bit8u),512,diskfile)<512) {
			fclose(diskfile);
			LOG_MSG("Could not load image file.");
			return;
		}
		fclose(diskfile);
		if ((buf[510]!=0x55) || (buf[511]!=0xaa)) {
			LOG_MSG("Could not extract drive geometry from image. Use IMGMOUNT with parameter -size bps,spc,hpc,cyl to specify the geometry.");
			return;
		}
		bool yet_detected = false;
		// check MBR partition entry 1
		Bitu starthead = buf[0x1bf];
		Bitu startsect = buf[0x1c0]&0x3f-1;
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
			} else {	
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
			LOG_MSG("autosized image file: %d:%d:%d:%d",sizes[0],sizes[1],sizes[2],sizes[3]);
		else {
			LOG_MSG("Could not extract drive geometry from image. Use IMGMOUNT with parameter -size bps,spc,hpc,cyl to specify the geometry.");
			return;
		}
	std::vector<DOS_Drive*> imgDisks;
	std::vector<std::string>::size_type i;
	std::vector<DOS_Drive*>::size_type ct;
				
	for (i = 0; i < paths.size(); i++) {
		DOS_Drive* newDrive = new fatDrive(paths[i].c_str(),sizes[0],sizes[1],sizes[2],sizes[3],0);
		imgDisks.push_back(newDrive);
		if(!(dynamic_cast<fatDrive*>(newDrive))->created_successfully) {
			LOG_MSG("Can't create drive from file.");
			for(ct = 0; ct < imgDisks.size(); ct++) {
				delete imgDisks[ct];
			}
			return;
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

	std::string tmp(paths[0]);
	for (i = 1; i < paths.size(); i++) {
		tmp += "; " + paths[i];
	}
	LOG_MSG("Drive %c is mounted as %s", drive, tmp.c_str());

	if (paths.size() == 1) {
		newdrive = imgDisks[0];
		if(((fatDrive *)newdrive)->loadedDisk->hardDrive) {
			if(imageDiskList[2] == NULL) {
				imageDiskList[2] = ((fatDrive *)newdrive)->loadedDisk;
				/* ide support start */
				// If instructed, attach to IDE controller as ATA hard disk
				if (ide_index >= 0) IDE_Hard_Disk_Attach(ide_index,ide_slave,2);
				/* ide support end */
				updateDPT();
				return;
			}
			if(imageDiskList[3] == NULL) {
				imageDiskList[3] = ((fatDrive *)newdrive)->loadedDisk;
				/* ide support start */
				// If instructed, attach to IDE controller as ATA hard disk
				if (ide_index >= 0) IDE_Hard_Disk_Attach(ide_index,ide_slave,3);
				/* ide support end */
				updateDPT();
				return;
			}
		}
		if(!((fatDrive *)newdrive)->loadedDisk->hardDrive) {
			imageDiskList[0] = ((fatDrive *)newdrive)->loadedDisk;
		}
	}
}

void Mount_Img(char drive, std::string realpath) {
	/* ide support start */
	bool ide_slave = false;
	signed char ide_index = -1;
	std::string ideattach="auto";
	/* ide support end */
	DOS_Drive * newdrive = NULL;
	imageDisk * newImage = NULL;
	std::string label;
	std::string temp_line = realpath;
	std::vector<std::string> paths;

	Bit8u mediaid;
	Bit16u sizes[4];
	std::string str_size;
	mediaid=0xF8;
	/* ide support start */
	IDE_Auto(ide_index,ide_slave);
	/* ide support end */
	str_size="650,127,16513,1700";
	mediaid=0xF8;
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
	struct stat test;
	if (stat(temp_line.c_str(),&test)) {
		// convert dosbox filename to system filename
		char fullname[CROSS_LEN];
		char tmp[CROSS_LEN];
		safe_strncpy(tmp, temp_line.c_str(), CROSS_LEN);
		Bit8u dummy;
		localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
		ldp->GetSystemFilename(tmp, fullname);
		temp_line = tmp;
	}
	paths.push_back(temp_line);
	if (paths.size() == 1)
		temp_line = paths[0];
	MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
	// create new drives for all images
	std::vector<DOS_Drive*> isoDisks;
	std::vector<std::string>::size_type i;
	for (i = 0; i < paths.size(); i++) {
		int error = -1;
		DOS_Drive* newDrive = new isoDrive(drive, paths[i].c_str(), mediaid, error);
		isoDisks.push_back(newDrive);
		LOG_MSG(MSCDEX_Output(error).c_str());
		// error: clean up and leave
		if (error) {
			for(i = 0; i < isoDisks.size(); i++) {
				delete isoDisks[i];
			}
			return;
		}
		// Update DriveManager
		for(i = 0; i < isoDisks.size(); i++) {
			DriveManager::AppendDisk(drive - 'A', isoDisks[i]);
		}
		DriveManager::InitializeDrive(drive - 'A');

		// Set the correct media byte in the table 
		mem_writeb(Real2Phys(dos.tables.mediaid) + (drive - 'A') * 2, mediaid);

		/* ide support start */
		// If instructed, attach to IDE controller as ATAPI CD-ROM device
		if (ide_index >= 0) IDE_CDROM_Attach(ide_index,ide_slave,drive - 'A');
		/* ide support end */

		// Print status message (success)
		LOG_MSG(MSCDEX_Output(0).c_str());
		std::string tmp(paths[0]);
		for (i = 1; i < paths.size(); i++) {
			tmp += "; " + paths[i];
		}
		LOG_MSG("GUI: Drive %c is mounted as %s", drive, tmp.c_str());

		// check if volume label is given
		//if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
		return;
	}
}

void DOSBox_SetMenu(void) {
	if(!menu.gui) return;

	LOG(LOG_MISC,LOG_DEBUG)("Win32: loading and attaching menu resource to DOSBox's window");

	menu.toggle=true;
	SetMenu(GetHWND(), LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
	DrawMenuBar (GetHWND());

	if(menu.startup) {
		RENDER_CallBack( GFX_CallBackReset );
	}
}

void DOSBox_NoMenu(void) {
	if(!menu.gui) return;
	menu.toggle=false;
	SetMenu(GetHWND(), NULL);
	DrawMenuBar(GetHWND());
	RENDER_CallBack( GFX_CallBackReset );
}

void DOSBox_CheckOS(int &id, int &major, int &minor) {
	OSVERSIONINFO osi;
	ZeroMemory(&osi, sizeof(OSVERSIONINFO));
	osi.dwOSVersionInfoSize = sizeof(osi);
	GetVersionEx(&osi);
	id=osi.dwPlatformId;
	if(id==1) { major=0; minor=0; return; }
	major=osi.dwMajorVersion;
	minor=osi.dwMinorVersion;
}

bool DOSBox_Kor(void) {
    if(menu.compatible) return false;
    char Buffer[30];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SABBREVLANGNAME, Buffer, sizeof(Buffer));
    return (!strcmp(Buffer,"KOR") ? true : false);
}

void DOSBox_RefreshMenu(void) {
    int width, height; bool fullscreen;
    void GFX_GetSize(int &width, int &height, bool &fullscreen);
    GFX_GetSize(width,height,fullscreen);
    void SDL_Prepare(void);
    SDL_Prepare();
    if(!menu.gui) return;

    if(fullscreen) {
    	SetMenu(GetHWND(), NULL);
    	DrawMenuBar(GetHWND());
        return;
    }
	if(menu.toggle)
		DOSBox_SetMenu();
	else
		DOSBox_NoMenu();
}

void DOSBox_RefreshMenu2(void) {
	if(!menu.gui) return;
   int width, height; bool fullscreen;
   void GFX_GetSize(int &width, int &height, bool &fullscreen);
   GFX_GetSize(width,height,fullscreen);
    void SDL_Prepare(void);
    SDL_Prepare();
    if(!menu.gui) return;

    if(fullscreen) {
    	SetMenu(GetHWND(), NULL);
    	DrawMenuBar(GetHWND());
        return;
    }
	if(menu.toggle) {
		menu.toggle=true;
		SetMenu(GetHWND(), LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
		DrawMenuBar (GetHWND());
	} else {
		menu.toggle=false;
		SetMenu(GetHWND(), NULL);
		DrawMenuBar(GetHWND());
	}
}

void ToggleMenu(bool pressed) {
    menu.resizeusing=true;
	int width, height; bool fullscreen;
	void GFX_GetSize(int &width, int &height, bool &fullscreen);
	GFX_GetSize(width, height, fullscreen);
    if(!menu.gui || !pressed || fullscreen) return;
	if(!menu.toggle) {
		menu.toggle=true;
		DOSBox_SetMenu();
	} else {
		menu.toggle=false;
		DOSBox_NoMenu();
	}
}

void MENU_Check_Drive(HMENU handle, int cdrom, int floppy, int local, int image, int automount, int umount, char drive) {
	std::string full_drive(1, drive);
	Section_prop * sec = static_cast<Section_prop *>(control->GetSection("dos"));
	full_drive += ":\\";
	EnableMenuItem(handle, cdrom, (Drives[drive - 'A'] || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(handle, floppy, (Drives[drive - 'A'] || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(handle, local, (Drives[drive - 'A'] || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(handle, image, (Drives[drive - 'A'] || menu.boot) ? MF_GRAYED : MF_ENABLED);
	if(sec) EnableMenuItem(handle, automount, AUTOMOUNT(full_drive.c_str(), drive) && !menu.boot && sec->Get_bool("automount") ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(handle, umount, (!Drives[drive - 'A']) || menu.boot ? MF_GRAYED : MF_ENABLED);
}

bool MENU_SetBool(std::string secname, std::string value) {
	Section_prop * sec = static_cast<Section_prop *>(control->GetSection(secname));
	if(sec) SetVal(secname, value, sec->Get_bool(value) ? "false" : "true");
	return sec->Get_bool(value);
}

void MENU_KeyDelayRate(int delay, int rate) {
	IO_Write(0x60,0xf3); IO_Write(0x60,(Bit8u)(((delay-1)<<5)|(32-rate)));
	LOG_MSG("GUI: Keyboard rate %d, delay %d", rate, delay);
}

enum SCREEN_TYPES	{
	SCREEN_OPENGLHQ,
	SCREEN_SURFACE,
	SCREEN_SURFACE_DDRAW,
	SCREEN_OVERLAY,
	SCREEN_OPENGL,
	SCREEN_DIRECT3D
};
extern bool load_videodrv;

int Reflect_Menu(void) {
	extern bool Mouse_Drv;
	static char name[9];

	if (!menu.gui) return 0;
	HMENU m_handle = GetMenu(GetHWND());
	if (!m_handle) return 0;

	if (!dos_kernel_disabled) {
		DOS_MCB mcb(dos.psp() - 1);
		mcb.GetFileName(name);
	}
	else {
		name[0] = 0;
	}

	CheckMenuItem(m_handle, ID_WAITONERR, GetSetSDLValue(1, "wait_on_error", 0) ? MF_CHECKED : MF_STRING);
	EnableMenuItem(m_handle, ID_OPENFILE, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_GLIDE_TRUE, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_GLIDE_EMU, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_NONE, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_BG, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_CZ, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_FR, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_GK, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_GR, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_HR, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_HU, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_IT, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_NL, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_NO, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_PL, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_RU, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SK, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SP, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SU, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SV, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_BE, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_BR, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_CF, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_DK, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_LA, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_PO, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SF, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SG, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_UK, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_US, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_YU, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_FO, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_MK, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_MT, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_PH, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_RO, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_SQ, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_TM, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_TR, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_UX, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_YC, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_DV, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_RH, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_KEY_LH, (strlen(name)) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_A, (strlen(name) || menu.boot || (Drives['A' - 'A'])) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_C, (strlen(name) || menu.boot || (Drives['C' - 'A'])) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_D, (strlen(name) || menu.boot || (Drives['D' - 'A'])) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_A_MOUNTED, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_C_MOUNTED, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	EnableMenuItem(m_handle, ID_BOOT_D_MOUNTED, (strlen(name) || menu.boot) ? MF_GRAYED : MF_ENABLED);
	CheckMenuItem(m_handle, ID_MOUSE, Mouse_Drv ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_AUTOCYCLE, (CPU_CycleAutoAdjust) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_AUTODETER, (CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_NORMAL, (!strcasecmp(core_mode, "Normal")) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_DYNAMIC, (!strcasecmp(core_mode, "Dynamic")) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_FULL, (!strcasecmp(core_mode, "Full")) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_SIMPLE, (!strcasecmp(core_mode, "Simple")) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_AUTO, (!strcasecmp(core_mode, "Auto")) ? MF_CHECKED : MF_STRING);

	Section_prop * sec = 0;
	sec = static_cast<Section_prop *>(control->GetSection("cpu"));
	const std::string cputype = sec->Get_string("cputype");
	CheckMenuItem(m_handle, ID_CPUTYPE_AUTO, cputype == "auto" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_CPUTYPE_386, cputype == "386" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_CPUTYPE_386_PREFETCH, cputype == "386_prefetch" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_CPUTYPE_486, cputype == "486" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_CPUTYPE_PENTIUM, cputype == "pentium" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_CPUTYPE_PENTIUM_MMX, cputype == "pentium_mmx" ? MF_CHECKED : MF_STRING);

	extern bool ticksLocked;
	CheckMenuItem(m_handle, ID_CPU_TURBO, ticksLocked ? MF_CHECKED : MF_STRING);

	sec = static_cast<Section_prop *>(control->GetSection("joystick"));
	const std::string joysticktype = sec->Get_string("joysticktype");
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_AUTO, joysticktype == "auto" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_2AXIS, joysticktype == "2axis" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_4AXIS, joysticktype == "4axis" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_4AXIS_2, joysticktype == "4axis_2" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_FCS, joysticktype == "fcs" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_CH, joysticktype == "ch" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICKTYPE_NONE, joysticktype == "none" ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICK_TIMED, sec->Get_bool("timed") ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICK_AUTOFIRE, sec->Get_bool("autofire") ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICK_SWAP34, sec->Get_bool("swap34") ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_JOYSTICK_BUTTONWRAP, sec->Get_bool("buttonwrap") ? MF_CHECKED : MF_STRING);

	CheckMenuItem(m_handle, ID_ASPECT, (render.aspect) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_SURFACE, ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_SURFACE) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_DDRAW, ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_SURFACE_DDRAW) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_DIRECT3D, ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_DIRECT3D) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_OVERLAY, ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_OVERLAY) ? MF_CHECKED : MF_STRING);
	if ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_OPENGL) {
		if (GetSetSDLValue(1, "opengl.bilinear", 0)) {
			CheckMenuItem(m_handle, ID_OPENGL, MF_CHECKED);
			CheckMenuItem(m_handle, ID_OPENGLNB, MF_STRING);
		}
		else {
			CheckMenuItem(m_handle, ID_OPENGL, MF_STRING);
			CheckMenuItem(m_handle, ID_OPENGLNB, MF_CHECKED);
		}
	}
	else {
		CheckMenuItem(m_handle, ID_OPENGLNB, MF_STRING);
		CheckMenuItem(m_handle, ID_OPENGL, MF_STRING);
	}
	
	CheckMenuItem(m_handle, ID_OPENGLHQ, ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_OPENGLHQ) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_FULLDOUBLE, (GetSetSDLValue(1, "desktop.doublebuf", 0)) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_AUTOLOCK, (GetSetSDLValue(1, "mouse.autoenable", 0)) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_HIDECYCL, !menu.hidecycles ? MF_CHECKED : MF_STRING);

	sec = static_cast<Section_prop *>(control->GetSection("serial"));
	if (sec) {
		bool serial = false;
		const std::string serial1 = sec->Get_string("serial1");
		const std::string serial2 = sec->Get_string("serial2");
		const std::string serial3 = sec->Get_string("serial3");
		const std::string serial4 = sec->Get_string("serial4");
		if (serial1 != "disabled" || serial2 != "disabled" || serial3 != "disabled"
			|| serial4 != "disabled") serial = true;
		CheckMenuItem(m_handle, ID_SERIAL_SECTION, serial ? MF_CHECKED : MF_STRING);
	}

	sec = static_cast<Section_prop *>(control->GetSection("parallel"));
	if (sec) {
		//CheckMenuItem(m_handle,ID_DONGLE,sec->Get_bool("dongle")?MF_CHECKED:MF_STRING);
		bool parallel = false;
		const std::string parallel1 = sec->Get_string("parallel1");
		const std::string parallel2 = sec->Get_string("parallel2");
		const std::string parallel3 = sec->Get_string("parallel3");
		if (parallel1 != "disabled" || parallel2 != "disabled" || parallel3 != "disabled")
			parallel = true;
		CheckMenuItem(m_handle, ID_PARALLEL_SECTION, (parallel || sec->Get_bool("dongle")) ? MF_CHECKED : MF_STRING);
	}

	std::string path;
	std::string real_path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if (Get_Custom_SaveDir(path)) {
		path += CROSS_FILESPLIT;
	}
	else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		}
		else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path += "save";
		path += CROSS_FILESPLIT;
	}

	sec = static_cast<Section_prop *>(control->GetSection("printer"));
	if (sec) CheckMenuItem(m_handle, ID_PRINTER_SECTION, sec->Get_bool("printer") ? MF_CHECKED : MF_STRING);

	sec = static_cast<Section_prop *>(control->GetSection("sdl"));
	if (sec) {
		const char* windowresolution = sec->Get_string("windowresolution");
		const int sdl_overscan = sec->Get_int("overscan");
		CheckMenuItem(m_handle, ID_OVERSCAN_0, sdl_overscan == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_1, sdl_overscan == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_2, sdl_overscan == 2 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_3, sdl_overscan == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_4, sdl_overscan == 4 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_5, sdl_overscan == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_6, sdl_overscan == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_7, sdl_overscan == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_8, sdl_overscan == 8 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_9, sdl_overscan == 9 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_OVERSCAN_10, sdl_overscan == 10 ? MF_CHECKED : MF_STRING);

		if (windowresolution && *windowresolution) {
			char res[30];
			safe_strncpy(res, windowresolution, sizeof(res));
			windowresolution = lowcase(res);//so x and X are allowed
			CheckMenuItem(m_handle, ID_USESCANCODES, (sec->Get_bool("usescancodes")) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_NONE, SCALER_SW_2(scalerOpNormal, 1) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_NORMAL2X, SCALER_SW_2(scalerOpNormal, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_NORMAL3X, SCALER_SW_2(scalerOpNormal, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_NORMAL4X, SCALER_SW_2(scalerOpNormal, 4) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_NORMAL5X, SCALER_SW_2(scalerOpNormal, 5) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HARDWARE_NONE, (SCALER_HW_2(scalerOpNormal, 1)) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HARDWARE2X, (SCALER_HW_2(scalerOpNormal, 4)) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HARDWARE3X, (SCALER_HW_2(scalerOpNormal, 6)) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HARDWARE4X, (SCALER_HW_2(scalerOpNormal, 8)) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HARDWARE5X, (SCALER_HW_2(scalerOpNormal, 10)) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_ADVMAME2X, SCALER_2(scalerOpAdvMame, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_ADVMAME3X, SCALER_2(scalerOpAdvMame, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_ADVINTERP2X, SCALER_2(scalerOpAdvInterp, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_ADVINTERP3X, SCALER_2(scalerOpAdvInterp, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HQ2X, SCALER_2(scalerOpHQ, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_HQ3X, SCALER_2(scalerOpHQ, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_TV2X, SCALER_2(scalerOpTV, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_TV3X, SCALER_2(scalerOpTV, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SCAN2X, SCALER_2(scalerOpScan, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SCAN3X, SCALER_2(scalerOpScan, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_RGB2X, SCALER_2(scalerOpRGB, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_RGB3X, SCALER_2(scalerOpRGB, 3) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_2XSAI, SCALER_2(scalerOpSaI, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SUPER2XSAI, SCALER_2(scalerOpSuperSaI, 2) ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SUPEREAGLE, SCALER_2(scalerOpSuperEagle, 2) ? MF_CHECKED : MF_STRING);
			//EnableMenuItem(m_handle,ID_FORCESCALER,SCALER_2(scalerOpNormal,4) || SCALER_2(scalerOpNormal,6)?MF_GRAYED:MF_ENABLED);
			CheckMenuItem(m_handle, ID_FORCESCALER, render.scale.forced ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_0, render.frameskip.max==0 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_1, render.frameskip.max==1 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_2, render.frameskip.max==2 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_3, render.frameskip.max==3 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_4, render.frameskip.max==4 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_5, render.frameskip.max==5 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_6, render.frameskip.max==6 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_7, render.frameskip.max==7 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_8, render.frameskip.max==8 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_9, render.frameskip.max==9 ? MF_CHECKED : MF_STRING);
			CheckMenuItem(m_handle, ID_SKIP_10, render.frameskip.max==10 ? MF_CHECKED : MF_STRING);
		}
		const std::string winres = sec->Get_string("windowresolution");
		const std::string fullres = sec->Get_string("fullresolution");

		if (!(winres == "original" || winres == "desktop" || winres == "0x0"))
			CheckMenuItem(m_handle, ID_WINRES_USER, MF_CHECKED);
		else
			CheckMenuItem(m_handle, ID_WINRES_USER, MF_STRING);

		if (!(fullres == "original" || fullres == "desktop" || fullres == "0x0"))
			CheckMenuItem(m_handle, ID_WINFULL_USER, MF_CHECKED);
		else
			CheckMenuItem(m_handle, ID_WINFULL_USER, MF_STRING);
		CheckMenuItem(m_handle, ID_WINFULL_DESKTOP, (fullres == "desktop") || (fullres == "0x0") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_WINFULL_ORIGINAL, (fullres == "original") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_WINRES_DESKTOP, (winres == "desktop" || winres == "0x0") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_WINRES_ORIGINAL, (winres == "original") ? MF_CHECKED : MF_STRING);
	}

	sec = static_cast<Section_prop *>(control->GetSection("midi"));
	if (sec) {
		const std::string mpu401 = sec->Get_string("mpu401");
		const std::string device = sec->Get_string("mididevice");
		const std::string mt32reverbmode = sec->Get_string("mt32.reverb.mode");
		const std::string mt32dac = sec->Get_string("mt32.dac");
		const std::string mt32reversestereo = sec->Get_string("mt32.reverse.stereo");
		const int mt32reverbtime = sec->Get_int("mt32.reverb.time");
		const int mt32reverblevel = sec->Get_int("mt32.reverb.level");
		CheckMenuItem(m_handle, ID_MIDI_NONE, (mpu401 == "none") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_UART, (mpu401 == "uart") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_INTELLI, (mpu401 == "intelligent") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_DEV_NONE, (device == "none") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_DEFAULT, (device == "default") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_ALSA, (device == "alsa") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_OSS, (device == "oss") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_WIN32, (device == "win32") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_COREAUDIO, (device == "coreaudio") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_COREMIDI, (device == "coremidi") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32, (device == "mt32") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_SYNTH, (device == "synth") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_TIMIDITY, (device == "timidity") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBMODE_AUTO, (mt32reverbmode == "auto") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBMODE_0, (mt32reverbmode == "0") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBMODE_1, (mt32reverbmode == "1") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBMODE_2, (mt32reverbmode == "2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBMODE_3, (mt32reverbmode == "3") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_DAC_AUTO, (mt32dac == "auto") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_DAC_0, (mt32dac == "0") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_DAC_1, (mt32dac == "1") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_DAC_2, (mt32dac == "2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_DAC_3, (mt32dac == "3") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_0, mt32reverbtime == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_1, mt32reverbtime == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_2, mt32reverbtime == 2 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_3, mt32reverbtime == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_4, mt32reverbtime == 4 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_5, mt32reverbtime == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_6, mt32reverbtime == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBTIME_7, mt32reverbtime == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_0, mt32reverblevel == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_1, mt32reverblevel == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_2, mt32reverblevel == 2 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_3, mt32reverblevel == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_4, mt32reverblevel == 4 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_5, mt32reverblevel == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_6, mt32reverblevel == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERBLEV_7, mt32reverblevel == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERSESTEREO_TRUE, (mt32reversestereo == "on") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MIDI_MT32_REVERSESTEREO_FALSE, (mt32reversestereo == "off") ? MF_CHECKED : MF_STRING);
	}

	CheckMenuItem(m_handle, ID_MUTE, SDL_GetAudioStatus() == SDL_AUDIO_PAUSED ? MF_CHECKED : MF_STRING);

	sec = static_cast<Section_prop *>(control->GetSection("mixer"));
	if (sec) {
		CheckMenuItem(m_handle, ID_SWAPSTEREO, sec->Get_bool("swapstereo") ? MF_CHECKED : MF_STRING);
	}

	sec = static_cast<Section_prop *>(control->GetSection("sblaster"));
	if (sec) {
		const std::string sbtype = sec->Get_string("sbtype");
		const int sbbase = sec->Get_hex("sbbase");
		const int hwbase = sec->Get_hex("hardwarebase");
		const int irq = sec->Get_int("irq");
		const int dma = sec->Get_int("dma");
		const int hdma = sec->Get_int("hdma");
		const std::string oplmode = sec->Get_string("oplmode");
		const std::string oplemu = sec->Get_string("oplemu");
		const int oplrate = sec->Get_int("oplrate");
		CheckMenuItem(m_handle, ID_SB_NONE, (sbtype == "none") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SB1, (sbtype == "sb1") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SB2, (sbtype == "sb2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SBPRO1, (sbtype == "sbpro1") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SBPRO2, (sbtype == "sbpro2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SB16, (sbtype == "sb16") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_SB16VIBRA, (sbtype == "sb16vibra") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_GB, (sbtype == "gb") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_220, sbbase == 544 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_240, sbbase == 576 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_260, sbbase == 608 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_280, sbbase == 640 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_300, sbbase == 768 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_2a0, sbbase == 672 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_2c0, sbbase == 704 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_2e0, sbbase == 736 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW210, hwbase == 528 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW220, hwbase == 544 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW230, hwbase == 560 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW240, hwbase == 576 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW250, hwbase == 592 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW260, hwbase == 608 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HW280, hwbase == 640 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_3, irq == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_5, irq == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_7, irq == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_9, irq == 9 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_10, irq == 10 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_11, irq == 11 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_IRQ_12, irq == 12 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_0, dma == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_1, dma == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_3, dma == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_5, dma == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_6, dma == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_DMA_7, dma == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_0, hdma == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_1, hdma == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_3, hdma == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_5, hdma == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_6, hdma == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_HDMA_7, hdma == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_AUTO, (oplmode == "auto") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_NONE, (oplmode == "none") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_CMS, (oplmode == "cms") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_OPL2, (oplmode == "opl2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_DUALOPL2, (oplmode == "dualopl2") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_OPL3, (oplmode == "opl3") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_HARDWARE, (oplmode == "hardware") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_HARDWAREGB, (oplmode == "hardwaregb") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_EMU_DEFAULT, (oplemu == "default") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_EMU_COMPAT, (oplemu == "compat") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_EMU_FAST, (oplemu == "fast") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_49716, oplrate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_48000, oplrate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_44100, oplrate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_32000, oplrate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_22050, oplrate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_16000, oplrate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_11025, oplrate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_SB_OPL_8000, oplrate == 8000 ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("glide"));
	if (sec) {
		const std::string glide = sec->Get_string("glide");
		const std::string lfb = sec->Get_string("lfb");
		CheckMenuItem(m_handle, ID_GLIDE_TRUE, (glide == "true") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_EMU, (glide == "emu") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_FULL, (lfb == "full") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_FULL_NOAUX, (lfb == "full_noaux") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_READ, (lfb == "read") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_READ_NOAUX, (lfb == "read_noaux") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_WRITE, (lfb == "write") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_WRITE_NOAUX, (lfb == "write_noaux") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_LFB_NONE, (lfb == "none") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_SPLASH, sec->Get_bool("splash") ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("pci"));
	if(sec) {
		const std::string emu = sec->Get_string("voodoo");
		CheckMenuItem(m_handle, ID_GLIDE_EMU_FALSE, (emu == "false") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_EMU_SOFTWARE, (emu == "software") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_EMU_OPENGL, (emu == "opengl") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GLIDE_EMU_AUTO, (emu == "auto") ? MF_CHECKED : MF_STRING);
	}

	sec = static_cast<Section_prop *>(control->GetSection("ne2000"));
	if (sec) {
		CheckMenuItem(m_handle, ID_NE2000_SECTION, sec->Get_bool("ne2000") ? MF_CHECKED : MF_STRING);
	}

	sec = static_cast<Section_prop *>(control->GetSection("gus"));
	if (sec) {
		const bool gus = sec->Get_bool("gus");
		const int gusrate = sec->Get_int("gusrate");
		const int gusbase = sec->Get_hex("gusbase");
		const int gusirq = sec->Get_int("gusirq");
		const int gusdma = sec->Get_int("gusdma");
		CheckMenuItem(m_handle, ID_GUS_TRUE, gus ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_49716, gusrate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_48000, gusrate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_44100, gusrate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_32000, gusrate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_22050, gusrate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_16000, gusrate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_11025, gusrate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_8000, gusrate == 8000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_300, gusbase == 768 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_280, gusbase == 640 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_260, gusbase == 608 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_240, gusbase == 576 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_220, gusbase == 544 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_2a0, gusbase == 672 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_2c0, gusbase == 704 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_2e0, gusbase == 736 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_3, gusirq == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_5, gusirq == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_7, gusirq == 7 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_9, gusirq == 9 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_10, gusirq == 10 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_11, gusirq == 11 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_IRQ_12, gusirq == 12 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_0, gusdma == 0 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_1, gusdma == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_3, gusdma == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_5, gusdma == 5 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_6, gusdma == 6 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_GUS_DMA_7, gusdma == 7 ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("innova"));
	if (sec) {
		const bool innova = sec->Get_bool("innova");
		const int samplerate = sec->Get_int("samplerate");
		const int sidbase = sec->Get_hex("sidbase");
		const int quality = sec->Get_int("quality");
		CheckMenuItem(m_handle, ID_INNOVA_TRUE, innova ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_49716, samplerate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_48000, samplerate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_44100, samplerate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_32000, samplerate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_22050, samplerate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_16000, samplerate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_11025, samplerate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_8000, samplerate == 8000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_280, sidbase == 640 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_2A0, sidbase == 672 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_2C0, sidbase == 704 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_2E0, sidbase == 736 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_220, sidbase == 544 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_240, sidbase == 576 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_260, sidbase == 608 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_300, sidbase == 768 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_3, quality == 3 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_2, quality == 2 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_1, quality == 1 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_INNOVA_0, quality == 0 ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("speaker"));
	if (sec) {
		const bool pcspeaker = sec->Get_bool("pcspeaker");
		const int pcrate = sec->Get_int("pcrate");
		const std::string tandy = sec->Get_string("tandy");
		const int tandyrate = sec->Get_int("tandyrate");
		const bool disney = sec->Get_bool("disney");
		const std::string ps1audio = sec->Get_string("ps1audio");
		const int ps1audiorate = sec->Get_int("ps1audiorate");
		CheckMenuItem(m_handle, ID_PS1_ON, (ps1audio == "on") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_OFF, (ps1audio == "off") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_49716, ps1audiorate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_48000, ps1audiorate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_44100, ps1audiorate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_32000, ps1audiorate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_22050, ps1audiorate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_16000, ps1audiorate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_11025, ps1audiorate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PS1_8000, ps1audiorate == 8000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_TRUE, pcspeaker ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_49716, pcrate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_48000, pcrate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_44100, pcrate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_32000, pcrate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_22050, pcrate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_16000, pcrate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_11025, pcrate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_PCSPEAKER_8000, pcrate == 8000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_ON, (tandy == "on") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_OFF, (tandy == "off") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_AUTO, (tandy == "auto") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_49716, tandyrate == 49716 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_48000, tandyrate == 48000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_44100, tandyrate == 44100 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_32000, tandyrate == 32000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_22050, tandyrate == 22050 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_16000, tandyrate == 16000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_11025, tandyrate == 11025 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_TANDY_8000, tandyrate == 8000 ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_DISNEY_TRUE, disney ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_DISNEY_FALSE, !disney ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("render"));
	if (sec) {
		CheckMenuItem(m_handle, ID_CHAR9, sec->Get_bool("char9") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_MULTISCAN, sec->Get_bool("multiscan") ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("vsync"));
	if (sec) {
		const std::string vsyncmode = sec->Get_string("vsyncmode");
		EnableMenuItem(m_handle, ID_VSYNC, ((vsyncmode == "off") || (vsyncmode == "host")) ? MF_GRAYED : MF_ENABLED);
		CheckMenuItem(m_handle, ID_VSYNC_ON, (vsyncmode == "on") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_VSYNC_OFF, (vsyncmode == "off") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_VSYNC_HOST, (vsyncmode == "host") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_VSYNC_FORCE, (vsyncmode == "force") ? MF_CHECKED : MF_STRING);
	}
	char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
	CheckMenuItem(m_handle, ID_DRVFORCE_DIRECTX, ((!strcmp(sdl_videodrv, "directx")) && (load_videodrv)) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_DRVFORCE_WINDIB, ((!strcmp(sdl_videodrv, "windib")) && (load_videodrv)) ? MF_CHECKED : MF_STRING);
	CheckMenuItem(m_handle, ID_DRVFORCE_AUTO, !load_videodrv ? MF_CHECKED : MF_STRING);
	extern bool Mouse_Vertical;
	CheckMenuItem(m_handle, ID_MOUSE_VERTICAL, Mouse_Vertical ? MF_CHECKED : MF_STRING);
	sec = static_cast<Section_prop *>(control->GetSection("dos"));
	if (sec) {
		const std::string ems = sec->Get_string("ems");
		CheckMenuItem(m_handle, ID_XMS, sec->Get_bool("xms") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_EMS_TRUE, (ems == "true") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_EMS_FALSE, (ems == "false") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_EMS_EMSBOARD, (ems == "emsboard") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_EMS_EMM386, (ems == "emm386") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_UMB, sec->Get_bool("umb") ? MF_CHECKED : MF_STRING);

		const std::string key = sec->Get_string("keyboardlayout");
		CheckMenuItem(m_handle, ID_KEY_NONE, (key == "auto") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_BG, (key == "bg") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_CZ, (key == "CZ") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_FR, (key == "fr") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_GK, (key == "gk") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_GR, (key == "gr") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_HR, (key == "hr") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_HU, (key == "hu") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_IT, (key == "it") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_NL, (key == "nl") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_NO, (key == "no") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_PL, (key == "pl") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_RU, (key == "ru") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SK, (key == "sk") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SP, (key == "sp") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SU, (key == "su") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SV, (key == "sv") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_BE, (key == "be") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_BR, (key == "br") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_CF, (key == "cf") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_DK, (key == "dk") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_LA, (key == "la") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_PO, (key == "po") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SF, (key == "sf") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SG, (key == "sg") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_UK, (key == "uk") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_US, (key == "us") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_YU, (key == "yu") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_FO, (key == "fo") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_MK, (key == "mk") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_MT, (key == "mt") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_PH, (key == "ph") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_RO, (key == "ro") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_SQ, (key == "sq") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_TM, (key == "tm") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_TR, (key == "tr") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_UX, (key == "ux") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_YC, (key == "yc") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_DV, (key == "dv") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_RH, (key == "rh") ? MF_CHECKED : MF_STRING);
		CheckMenuItem(m_handle, ID_KEY_LH, (key == "lh") ? MF_CHECKED : MF_STRING);
	}
	sec = static_cast<Section_prop *>(control->GetSection("ipx"));
	if (sec)	CheckMenuItem(m_handle, ID_IPXNET, sec->Get_bool("ipx") ? MF_CHECKED : MF_STRING);
	sec = 0;

	sec = static_cast<Section_prop *>(control->GetSection("dos"));
	if (sec) CheckMenuItem(m_handle, ID_AUTOMOUNT, sec->Get_bool("automount") ? MF_CHECKED : MF_STRING);
	sec=0;

	DWORD dwExStyle = ::GetWindowLong(GetHWND(), GWL_EXSTYLE);
	CheckMenuItem(m_handle, ID_ALWAYS_ON_TOP, (dwExStyle & WS_EX_TOPMOST) ? MF_CHECKED : MF_STRING);

	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_A, ID_MOUNT_FLOPPY_A, ID_MOUNT_LOCAL_A, ID_MOUNT_IMAGE_A, ID_AUTOMOUNT_A, ID_UMOUNT_A, 'A');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_B, ID_MOUNT_FLOPPY_B, ID_MOUNT_LOCAL_B, ID_MOUNT_IMAGE_B, ID_AUTOMOUNT_B, ID_UMOUNT_B, 'B');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_C, ID_MOUNT_FLOPPY_C, ID_MOUNT_LOCAL_C, ID_MOUNT_IMAGE_C, ID_AUTOMOUNT_C, ID_UMOUNT_C, 'C');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_D, ID_MOUNT_FLOPPY_D, ID_MOUNT_LOCAL_D, ID_MOUNT_IMAGE_D, ID_AUTOMOUNT_D, ID_UMOUNT_D, 'D');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_E, ID_MOUNT_FLOPPY_E, ID_MOUNT_LOCAL_E, ID_MOUNT_IMAGE_E, ID_AUTOMOUNT_E, ID_UMOUNT_E, 'E');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_F, ID_MOUNT_FLOPPY_F, ID_MOUNT_LOCAL_F, ID_MOUNT_IMAGE_F, ID_AUTOMOUNT_F, ID_UMOUNT_F, 'F');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_G, ID_MOUNT_FLOPPY_G, ID_MOUNT_LOCAL_G, ID_MOUNT_IMAGE_G, ID_AUTOMOUNT_G, ID_UMOUNT_G, 'G');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_H, ID_MOUNT_FLOPPY_H, ID_MOUNT_LOCAL_H, ID_MOUNT_IMAGE_H, ID_AUTOMOUNT_H, ID_UMOUNT_H, 'H');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_I, ID_MOUNT_FLOPPY_I, ID_MOUNT_LOCAL_I, ID_MOUNT_IMAGE_I, ID_AUTOMOUNT_I, ID_UMOUNT_I, 'I');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_J, ID_MOUNT_FLOPPY_J, ID_MOUNT_LOCAL_J, ID_MOUNT_IMAGE_J, ID_AUTOMOUNT_J, ID_UMOUNT_J, 'J');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_K, ID_MOUNT_FLOPPY_K, ID_MOUNT_LOCAL_K, ID_MOUNT_IMAGE_K, ID_AUTOMOUNT_K, ID_UMOUNT_K, 'K');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_L, ID_MOUNT_FLOPPY_L, ID_MOUNT_LOCAL_L, ID_MOUNT_IMAGE_L, ID_AUTOMOUNT_L, ID_UMOUNT_L, 'L');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_M, ID_MOUNT_FLOPPY_M, ID_MOUNT_LOCAL_M, ID_MOUNT_IMAGE_M, ID_AUTOMOUNT_M, ID_UMOUNT_M, 'M');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_N, ID_MOUNT_FLOPPY_N, ID_MOUNT_LOCAL_N, ID_MOUNT_IMAGE_N, ID_AUTOMOUNT_N, ID_UMOUNT_N, 'N');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_O, ID_MOUNT_FLOPPY_O, ID_MOUNT_LOCAL_O, ID_MOUNT_IMAGE_O, ID_AUTOMOUNT_O, ID_UMOUNT_O, 'O');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_P, ID_MOUNT_FLOPPY_P, ID_MOUNT_LOCAL_P, ID_MOUNT_IMAGE_P, ID_AUTOMOUNT_P, ID_UMOUNT_P, 'P');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_Q, ID_MOUNT_FLOPPY_Q, ID_MOUNT_LOCAL_Q, ID_MOUNT_IMAGE_Q, ID_AUTOMOUNT_Q, ID_UMOUNT_Q, 'Q');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_R, ID_MOUNT_FLOPPY_R, ID_MOUNT_LOCAL_R, ID_MOUNT_IMAGE_R, ID_AUTOMOUNT_R, ID_UMOUNT_R, 'R');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_S, ID_MOUNT_FLOPPY_S, ID_MOUNT_LOCAL_S, ID_MOUNT_IMAGE_S, ID_AUTOMOUNT_S, ID_UMOUNT_S, 'S');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_T, ID_MOUNT_FLOPPY_T, ID_MOUNT_LOCAL_T, ID_MOUNT_IMAGE_T, ID_AUTOMOUNT_T, ID_UMOUNT_T, 'T');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_U, ID_MOUNT_FLOPPY_U, ID_MOUNT_LOCAL_U, ID_MOUNT_IMAGE_U, ID_AUTOMOUNT_U, ID_UMOUNT_U, 'U');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_V, ID_MOUNT_FLOPPY_V, ID_MOUNT_LOCAL_V, ID_MOUNT_IMAGE_V, ID_AUTOMOUNT_V, ID_UMOUNT_V, 'V');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_W, ID_MOUNT_FLOPPY_W, ID_MOUNT_LOCAL_W, ID_MOUNT_IMAGE_W, ID_AUTOMOUNT_W, ID_UMOUNT_W, 'W');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_X, ID_MOUNT_FLOPPY_X, ID_MOUNT_LOCAL_X, ID_MOUNT_IMAGE_X, ID_AUTOMOUNT_X, ID_UMOUNT_X, 'X');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_Y, ID_MOUNT_FLOPPY_Y, ID_MOUNT_LOCAL_Y, ID_MOUNT_IMAGE_Y, ID_AUTOMOUNT_Y, ID_UMOUNT_Y, 'Y');
	MENU_Check_Drive(m_handle, ID_MOUNT_CDROM_Z, ID_MOUNT_FLOPPY_Z, ID_MOUNT_LOCAL_Z, ID_MOUNT_IMAGE_Z, ID_AUTOMOUNT_Z, ID_UMOUNT_Z, 'Z');
}

void MSG_Loop(void) {
	if (!menu.gui || GetSetSDLValue(1, "desktop.fullscreen", 0)) return;
	if (!GetMenu(GetHWND())) return;
	MSG Message;
	while (PeekMessage(&Message, GetHWND(), 0, 0, PM_REMOVE)) {
		switch (Message.message) {
		case WM_SYSCHAR:
			break;
		case WM_COMMAND:
			switch (LOWORD(Message.wParam)) {
			case ID_USESCANCODES: {
				Section* sec = control->GetSection("sdl");
				Section_prop * section = static_cast<Section_prop *>(sec);
				SetVal("sdl", "usescancodes", section->Get_bool("usescancodes") ? "false" : "true");
				}
				break;
			case ID_WAITONERR:
				if (GetSetSDLValue(1, "wait_on_error", 0)) {
					SetVal("sdl", "waitonerror", "false");
					GetSetSDLValue(0, "wait_on_error", (void*)false);
				}
				else {
					SetVal("sdl", "waitonerror", "true");
					GetSetSDLValue(0, "wait_on_error", (void*)true);
				}
				break;
			case ID_HDD_SIZE: GUI_Shortcut(18); break;
			case ID_BOOT_A: Go_Boot("A"); break;
			case ID_BOOT_C: Go_Boot("C"); break;
			case ID_BOOT_D: Go_Boot("D"); break;
			case ID_BOOT_A_MOUNTED: Go_Boot2("A"); break;
			case ID_BOOT_C_MOUNTED: Go_Boot2("C"); break;
			case ID_BOOT_D_MOUNTED: Go_Boot2("D"); break;
			case ID_RESTART: void restart_program(std::vector<std::string> & parameters); restart_program(control->startup_params); break;
			case ID_QUIT: throw(0); break;
			case ID_OPENFILE: OpenFileDialog(0); break;
			case ID_PAUSE: void PauseDOSBox(bool pressed); PauseDOSBox(1); break;
			case ID_NORMAL:
				if (strcasecmp(core_mode, "normal") == 0) break;
				SetVal("cpu", "core", "normal");
				break;
#if (C_DYNAMIC_X86)
			case ID_DYNAMIC: if (strcmp(core_mode, "dynamic") != 0) SetVal("cpu", "core", "dynamic"); break;
#endif
			case ID_FULL: if (strcmp(core_mode, "full") != 0) SetVal("cpu", "core", "full"); break;
			case ID_SIMPLE: if (strcmp(core_mode, "simple") != 0) SetVal("cpu", "core", "simple"); break;
			case ID_AUTO: if (strcmp(core_mode, "auto") != 0) SetVal("cpu", "core", "auto"); break;
			case ID_KEYMAP: MAPPER_RunInternal(); break;
			case ID_AUTOCYCLE: SetVal("cpu", "cycles", (!CPU_CycleAutoAdjust) ? "max" : "auto"); break;
			case ID_AUTODETER:
			{
			if (!(CPU_AutoDetermineMode&CPU_AUTODETERMINE_CYCLES)) {
				SetVal("cpu", "cycles", "auto");
				break;
			}
			else {
				std::ostringstream str;
				str << "fixed " << CPU_CycleMax;
				std::string cycles = str.str();
				SetVal("cpu", "cycles", cycles);
				break;
			}
			}
			case ID_CAPMOUSE: GFX_CaptureMouse(); break;
			case ID_REFRESH: GUI_Shortcut(1); break;
			case ID_FULLSCREEN: GFX_SwitchFullScreen(); break;
			case ID_ASPECT: SetVal("render", "aspect", render.aspect ? "false" : "true"); break;
			case ID_HIDECYCL:
				menu.hidecycles = !menu.hidecycles;
				GFX_SetTitle(CPU_CycleMax, -1, -1, false);
				break;
			case ID_TOGGLE: ToggleMenu(true); break;
			case ID_NONE: SCALER_SW(scalerOpNormal, 1) break; SetVal("render", "scaler", !render.scale.forced ? "none" : "none forced"); break;
			case ID_NORMAL2X: SCALER_SW(scalerOpNormal, 2) break; SetVal("render", "scaler", !render.scale.forced ? "normal2x" : "normal2x forced"); break;
			case ID_NORMAL3X: SCALER_SW(scalerOpNormal, 3) break; SetVal("render", "scaler", !render.scale.forced ? "normal3x" : "normal3x forced"); break;
			case ID_NORMAL4X: SCALER_SW(scalerOpNormal, 4) break; SetVal("render", "scaler", !render.scale.forced ? "normal4x" : "normal4x forced"); break;
			case ID_NORMAL5X: SCALER_SW(scalerOpNormal, 5) break; SetVal("render", "scaler", !render.scale.forced ? "normal5x" : "normal5x forced"); break;
			case ID_HARDWARE_NONE: SCALER_HW(scalerOpNormal, 1) break; SetVal("render", "scaler", !render.scale.forced ? "hardware_none" : "hardware_none forced"); break;
			case ID_HARDWARE2X: SCALER_HW(scalerOpNormal, 4) break; SetVal("render", "scaler", !render.scale.forced ? "hardware2x" : "hardware2x forced"); break;
			case ID_HARDWARE3X: SCALER_HW(scalerOpNormal, 6) break; SetVal("render", "scaler", !render.scale.forced ? "hardware3x" : "hardware3x forced"); break;
			case ID_HARDWARE4X: SCALER_HW(scalerOpNormal, 8) break; SetVal("render", "scaler", !render.scale.forced ? "hardware4x" : "hardware4x forced"); break;
			case ID_HARDWARE5X: SCALER_HW(scalerOpNormal, 10) break; SetVal("render", "scaler", !render.scale.forced ? "hardware5x" : "hardware5x forced"); break;
			case ID_ADVMAME2X: SCALER(scalerOpAdvMame, 2) break; SetVal("render", "scaler", !render.scale.forced ? "advmame2x" : "advmame2x forced"); break;
			case ID_ADVMAME3X: SCALER(scalerOpAdvMame, 3) break; SetVal("render", "scaler", !render.scale.forced ? "advmame3x" : "advmame3x forced"); break;
			case ID_ADVINTERP2X: SCALER(scalerOpAdvInterp, 2) break; SetVal("render", "scaler", !render.scale.forced ? "advinterp2x" : "advinterp2x forced"); break;
			case ID_ADVINTERP3X: SCALER(scalerOpAdvInterp, 3) break; SetVal("render", "scaler", !render.scale.forced ? "advinterp3x" : "advinterp3x forced"); break;
			case ID_HQ2X: SCALER(scalerOpHQ, 2) break; SetVal("render", "scaler", !render.scale.forced ? "hq2x" : "hq2x forced"); break;
			case ID_HQ3X: SCALER(scalerOpHQ, 3) break; SetVal("render", "scaler", !render.scale.forced ? "hq3x" : "hq3x forced"); break;
			case ID_2XSAI: SCALER(scalerOpSaI, 2) break; SetVal("render", "scaler", !render.scale.forced ? "2xsai" : "2xsai forced"); break;
			case ID_SUPER2XSAI: SCALER(scalerOpSuperSaI, 2) break; SetVal("render", "scaler", !render.scale.forced ? "super2xsai" : "super2xsai forced"); break;
			case ID_SUPEREAGLE: SCALER(scalerOpSuperEagle, 2) break; SetVal("render", "scaler", !render.scale.forced ? "supereagle" : "supereagle forced"); break;
			case ID_TV2X: SCALER(scalerOpTV, 2) break; SetVal("render", "scaler", !render.scale.forced ? "tv2x" : "tv2x forced"); break;
			case ID_TV3X: SCALER(scalerOpTV, 3) break; SetVal("render", "scaler", !render.scale.forced ? "tv3x" : "tv3x forced"); break;
			case ID_RGB2X: SCALER(scalerOpRGB, 2) break; SetVal("render", "scaler", !render.scale.forced ? "rgb2x" : "rgb2x forced"); break;
			case ID_RGB3X: SCALER(scalerOpRGB, 3) break; SetVal("render", "scaler", !render.scale.forced ? "rgb3x" : "rgb3x forced"); break;
			case ID_SCAN2X: SCALER(scalerOpScan, 2) break; SetVal("render", "scaler", !render.scale.forced ? "scan2x" : "scan2x forced"); break;
			case ID_SCAN3X: SCALER(scalerOpScan, 3) break; SetVal("render", "scaler", !render.scale.forced ? "scan3x" : "scan3x forced"); break;
			case ID_FORCESCALER: {
				SCALER_SW(scalerOpNormal, 1) SetVal("render", "scaler", render.scale.forced ? "none" : "none forced"); else
					SCALER_HW(scalerOpNormal, 1) SetVal("render", "scaler", render.scale.forced ? "hardware_none" : "hardware_none forced"); else
					SCALER_HW(scalerOpNormal, 4) SetVal("render", "scaler", render.scale.forced ? "hardware2x" : "hardware2x forced"); else
					SCALER_HW(scalerOpNormal, 6) SetVal("render", "scaler", render.scale.forced ? "hardware3x" : "hardware3x forced"); else
					SCALER_HW(scalerOpNormal, 8) SetVal("render", "scaler", render.scale.forced ? "hardware4x" : "hardware4x forced"); else
					SCALER_HW(scalerOpNormal, 10) SetVal("render", "scaler", render.scale.forced ? "hardware5x" : "hardware5x forced"); else
					SCALER(scalerOpNormal, 2) SetVal("render", "scaler", render.scale.forced ? "normal2x" : "normal2x forced"); else
					SCALER(scalerOpNormal, 3) SetVal("render", "scaler", render.scale.forced ? "normal3x" : "normal3x forced"); else
					SCALER_SW(scalerOpNormal, 4) SetVal("render", "scaler", render.scale.forced ? "normal4x" : "normal4x forced"); else
					SCALER_SW(scalerOpNormal, 5) SetVal("render", "scaler", render.scale.forced ? "normal5x" : "normal5x forced"); else
					SCALER(scalerOpAdvMame, 2) SetVal("render", "scaler", render.scale.forced ? "advmame2x" : "advmame2x forced"); else
					SCALER(scalerOpAdvMame, 3) SetVal("render", "scaler", render.scale.forced ? "advmame3x" : "advmame3x forced"); else
					SCALER(scalerOpAdvInterp, 2) SetVal("render", "scaler", render.scale.forced ? "advinterp2x" : "advinterp2x forced"); else
					SCALER(scalerOpAdvInterp, 3) SetVal("render", "scaler", render.scale.forced ? "advinterp3x" : "advinterp3x forced"); else
					SCALER(scalerOpHQ, 2) SetVal("render", "scaler", render.scale.forced ? "hq2x" : "hq2x forced"); else
					SCALER(scalerOpHQ, 3) SetVal("render", "scaler", render.scale.forced ? "hq3x" : "hq3x forced"); else
					SCALER(scalerOpSaI, 2) SetVal("render", "scaler", render.scale.forced ? "2xsai" : "2xsai forced"); else
					SCALER(scalerOpSuperSaI, 2) SetVal("render", "scaler", render.scale.forced ? "super2xsai" : "super2xsai forced"); else
					SCALER(scalerOpSuperEagle, 2) SetVal("render", "scaler", render.scale.forced ? "supereagle" : "supereagle forced"); else
					SCALER(scalerOpTV, 2) SetVal("render", "scaler", render.scale.forced ? "tv2x" : "tv2x forced"); else
					SCALER(scalerOpTV, 3) SetVal("render", "scaler", render.scale.forced ? "tv3x" : "tv3x forced"); else
					SCALER(scalerOpRGB, 2) SetVal("render", "scaler", render.scale.forced ? "rgb2x" : "rgb2x forced"); else
					SCALER(scalerOpRGB, 3) SetVal("render", "scaler", render.scale.forced ? "rgb3x" : "rgb3x forced"); else
					SCALER(scalerOpScan, 2) SetVal("render", "scaler", render.scale.forced ? "scan2x" : "scan2x forced"); else
					SCALER(scalerOpScan, 3) SetVal("render", "scaler", render.scale.forced ? "scan3x" : "scan3x forced");
				break;
				}
				break;

			case ID_CYCLE: GUI_Shortcut(16); break;
			case ID_CPU_TURBO: extern void DOSBOX_UnlockSpeed2(bool pressed); DOSBOX_UnlockSpeed2(1); break;
			case ID_SKIP_0: SetVal("render", "frameskip", "0"); break;
			case ID_SKIP_1: SetVal("render", "frameskip", "1"); break;
			case ID_SKIP_2: SetVal("render", "frameskip", "2"); break;
			case ID_SKIP_3: SetVal("render", "frameskip", "3"); break;
			case ID_SKIP_4: SetVal("render", "frameskip", "4"); break;
			case ID_SKIP_5: SetVal("render", "frameskip", "5"); break;
			case ID_SKIP_6: SetVal("render", "frameskip", "6"); break;
			case ID_SKIP_7: SetVal("render", "frameskip", "7"); break;
			case ID_SKIP_8: SetVal("render", "frameskip", "8"); break;
			case ID_SKIP_9: SetVal("render", "frameskip", "9"); break;
			case ID_SKIP_10: SetVal("render", "frameskip", "10"); break;
			case ID_UMOUNT_A: UnMount('A'); break;
			case ID_UMOUNT_B: UnMount('B'); break;
			case ID_UMOUNT_C: UnMount('C'); break;
			case ID_UMOUNT_D: UnMount('D'); break;
			case ID_UMOUNT_E: UnMount('E'); break;
			case ID_UMOUNT_F: UnMount('F'); break;
			case ID_UMOUNT_G: UnMount('G'); break;
			case ID_UMOUNT_H: UnMount('H'); break;
			case ID_UMOUNT_I: UnMount('I'); break;
			case ID_UMOUNT_J: UnMount('J'); break;
			case ID_UMOUNT_K: UnMount('K'); break;
			case ID_UMOUNT_L: UnMount('L'); break;
			case ID_UMOUNT_M: UnMount('M'); break;
			case ID_UMOUNT_N: UnMount('N'); break;
			case ID_UMOUNT_O: UnMount('O'); break;
			case ID_UMOUNT_P: UnMount('P'); break;
			case ID_UMOUNT_Q: UnMount('Q'); break;
			case ID_UMOUNT_R: UnMount('R'); break;
			case ID_UMOUNT_S: UnMount('S'); break;
			case ID_UMOUNT_T: UnMount('T'); break;
			case ID_UMOUNT_U: UnMount('U'); break;
			case ID_UMOUNT_V: UnMount('V'); break;
			case ID_UMOUNT_W: UnMount('W'); break;
			case ID_UMOUNT_X: UnMount('X'); break;
			case ID_UMOUNT_Y: UnMount('Y'); break;
			case ID_UMOUNT_Z: UnMount('Z'); break;
			case ID_AUTOMOUNT:
			{
			Section_prop * sec = static_cast<Section_prop *>(control->GetSection("dos"));
			if(sec) SetVal("dos", "automount", sec->Get_bool("automount") ? "false" : "true");
			}
			break;
			case ID_AUTOMOUNT_A: MountDrive('A', "A:\\"); break;
			case ID_AUTOMOUNT_B: MountDrive('B', "B:\\"); break;
			case ID_AUTOMOUNT_C: MountDrive('C', "C:\\"); break;
			case ID_AUTOMOUNT_D: MountDrive('D', "D:\\"); break;
			case ID_AUTOMOUNT_E: MountDrive('E', "E:\\"); break;
			case ID_AUTOMOUNT_F: MountDrive('F', "F:\\"); break;
			case ID_AUTOMOUNT_G: MountDrive('G', "G:\\"); break;
			case ID_AUTOMOUNT_H: MountDrive('H', "H:\\"); break;
			case ID_AUTOMOUNT_I: MountDrive('I', "I:\\"); break;
			case ID_AUTOMOUNT_J: MountDrive('J', "J:\\"); break;
			case ID_AUTOMOUNT_K: MountDrive('K', "K:\\"); break;
			case ID_AUTOMOUNT_L: MountDrive('L', "L:\\"); break;
			case ID_AUTOMOUNT_M: MountDrive('M', "M:\\"); break;
			case ID_AUTOMOUNT_N: MountDrive('N', "N:\\"); break;
			case ID_AUTOMOUNT_O: MountDrive('O', "O:\\"); break;
			case ID_AUTOMOUNT_P: MountDrive('P', "P:\\"); break;
			case ID_AUTOMOUNT_Q: MountDrive('Q', "Q:\\"); break;
			case ID_AUTOMOUNT_R: MountDrive('R', "R:\\"); break;
			case ID_AUTOMOUNT_S: MountDrive('S', "S:\\"); break;
			case ID_AUTOMOUNT_T: MountDrive('T', "T:\\"); break;
			case ID_AUTOMOUNT_U: MountDrive('U', "U:\\"); break;
			case ID_AUTOMOUNT_V: MountDrive('V', "V:\\"); break;
			case ID_AUTOMOUNT_W: MountDrive('W', "W:\\"); break;
			case ID_AUTOMOUNT_X: MountDrive('X', "X:\\"); break;
			case ID_AUTOMOUNT_Y: MountDrive('Y', "Y:\\"); break;
			case ID_AUTOMOUNT_Z: MountDrive('Z', "Z:\\"); break;
			case ID_MOUNT_CDROM_A: BrowseFolder('A', "CDROM"); break;
			case ID_MOUNT_CDROM_B: BrowseFolder('B', "CDROM"); break;
			case ID_MOUNT_CDROM_C: BrowseFolder('C', "CDROM"); break;
			case ID_MOUNT_CDROM_D: BrowseFolder('D', "CDROM"); break;
			case ID_MOUNT_CDROM_E: BrowseFolder('E', "CDROM"); break;
			case ID_MOUNT_CDROM_F: BrowseFolder('F', "CDROM"); break;
			case ID_MOUNT_CDROM_G: BrowseFolder('G', "CDROM"); break;
			case ID_MOUNT_CDROM_H: BrowseFolder('H', "CDROM"); break;
			case ID_MOUNT_CDROM_I: BrowseFolder('I', "CDROM"); break;
			case ID_MOUNT_CDROM_J: BrowseFolder('J', "CDROM"); break;
			case ID_MOUNT_CDROM_K: BrowseFolder('K', "CDROM"); break;
			case ID_MOUNT_CDROM_L: BrowseFolder('L', "CDROM"); break;
			case ID_MOUNT_CDROM_M: BrowseFolder('M', "CDROM"); break;
			case ID_MOUNT_CDROM_N: BrowseFolder('N', "CDROM"); break;
			case ID_MOUNT_CDROM_O: BrowseFolder('O', "CDROM"); break;
			case ID_MOUNT_CDROM_P: BrowseFolder('P', "CDROM"); break;
			case ID_MOUNT_CDROM_Q: BrowseFolder('Q', "CDROM"); break;
			case ID_MOUNT_CDROM_R: BrowseFolder('R', "CDROM"); break;
			case ID_MOUNT_CDROM_S: BrowseFolder('S', "CDROM"); break;
			case ID_MOUNT_CDROM_T: BrowseFolder('T', "CDROM"); break;
			case ID_MOUNT_CDROM_U: BrowseFolder('U', "CDROM"); break;
			case ID_MOUNT_CDROM_V: BrowseFolder('V', "CDROM"); break;
			case ID_MOUNT_CDROM_W: BrowseFolder('W', "CDROM"); break;
			case ID_MOUNT_CDROM_X: BrowseFolder('X', "CDROM"); break;
			case ID_MOUNT_CDROM_Y: BrowseFolder('Y', "CDROM"); break;
			case ID_MOUNT_CDROM_Z: BrowseFolder('Z', "CDROM"); break;
			case ID_MOUNT_FLOPPY_A: BrowseFolder('A', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_B: BrowseFolder('B', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_C: BrowseFolder('C', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_D: BrowseFolder('D', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_E: BrowseFolder('E', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_F: BrowseFolder('F', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_G: BrowseFolder('G', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_H: BrowseFolder('H', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_I: BrowseFolder('I', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_J: BrowseFolder('J', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_K: BrowseFolder('K', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_L: BrowseFolder('L', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_M: BrowseFolder('M', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_N: BrowseFolder('N', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_O: BrowseFolder('O', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_P: BrowseFolder('P', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_Q: BrowseFolder('Q', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_R: BrowseFolder('R', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_S: BrowseFolder('S', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_T: BrowseFolder('T', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_U: BrowseFolder('U', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_V: BrowseFolder('V', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_W: BrowseFolder('W', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_X: BrowseFolder('X', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_Y: BrowseFolder('Y', "FLOPPY"); break;
			case ID_MOUNT_FLOPPY_Z: BrowseFolder('Z', "FLOPPY"); break;
			case ID_MOUNT_LOCAL_A: BrowseFolder('A', "LOCAL"); break;
			case ID_MOUNT_LOCAL_B: BrowseFolder('B', "LOCAL"); break;
			case ID_MOUNT_LOCAL_C: BrowseFolder('C', "LOCAL"); break;
			case ID_MOUNT_LOCAL_D: BrowseFolder('D', "LOCAL"); break;
			case ID_MOUNT_LOCAL_E: BrowseFolder('E', "LOCAL"); break;
			case ID_MOUNT_LOCAL_F: BrowseFolder('F', "LOCAL"); break;
			case ID_MOUNT_LOCAL_G: BrowseFolder('G', "LOCAL"); break;
			case ID_MOUNT_LOCAL_H: BrowseFolder('H', "LOCAL"); break;
			case ID_MOUNT_LOCAL_I: BrowseFolder('I', "LOCAL"); break;
			case ID_MOUNT_LOCAL_J: BrowseFolder('J', "LOCAL"); break;
			case ID_MOUNT_LOCAL_K: BrowseFolder('K', "LOCAL"); break;
			case ID_MOUNT_LOCAL_L: BrowseFolder('L', "LOCAL"); break;
			case ID_MOUNT_LOCAL_M: BrowseFolder('M', "LOCAL"); break;
			case ID_MOUNT_LOCAL_N: BrowseFolder('N', "LOCAL"); break;
			case ID_MOUNT_LOCAL_O: BrowseFolder('O', "LOCAL"); break;
			case ID_MOUNT_LOCAL_P: BrowseFolder('P', "LOCAL"); break;
			case ID_MOUNT_LOCAL_Q: BrowseFolder('Q', "LOCAL"); break;
			case ID_MOUNT_LOCAL_R: BrowseFolder('R', "LOCAL"); break;
			case ID_MOUNT_LOCAL_S: BrowseFolder('S', "LOCAL"); break;
			case ID_MOUNT_LOCAL_T: BrowseFolder('T', "LOCAL"); break;
			case ID_MOUNT_LOCAL_U: BrowseFolder('U', "LOCAL"); break;
			case ID_MOUNT_LOCAL_V: BrowseFolder('V', "LOCAL"); break;
			case ID_MOUNT_LOCAL_W: BrowseFolder('W', "LOCAL"); break;
			case ID_MOUNT_LOCAL_X: BrowseFolder('X', "LOCAL"); break;
			case ID_MOUNT_LOCAL_Y: BrowseFolder('Y', "LOCAL"); break;
			case ID_MOUNT_LOCAL_Z: BrowseFolder('Z', "LOCAL"); break;
			case ID_MOUNT_IMAGE_A: OpenFileDialog_Img('A'); break;
			case ID_MOUNT_IMAGE_B: OpenFileDialog_Img('B'); break;
			case ID_MOUNT_IMAGE_C: OpenFileDialog_Img('C'); break;
			case ID_MOUNT_IMAGE_D: OpenFileDialog_Img('D'); break;
			case ID_MOUNT_IMAGE_E: OpenFileDialog_Img('E'); break;
			case ID_MOUNT_IMAGE_F: OpenFileDialog_Img('F'); break;
			case ID_MOUNT_IMAGE_G: OpenFileDialog_Img('G'); break;
			case ID_MOUNT_IMAGE_H: OpenFileDialog_Img('H'); break;
			case ID_MOUNT_IMAGE_I: OpenFileDialog_Img('I'); break;
			case ID_MOUNT_IMAGE_J: OpenFileDialog_Img('J'); break;
			case ID_MOUNT_IMAGE_K: OpenFileDialog_Img('K'); break;
			case ID_MOUNT_IMAGE_L: OpenFileDialog_Img('L'); break;
			case ID_MOUNT_IMAGE_M: OpenFileDialog_Img('M'); break;
			case ID_MOUNT_IMAGE_N: OpenFileDialog_Img('N'); break;
			case ID_MOUNT_IMAGE_O: OpenFileDialog_Img('O'); break;
			case ID_MOUNT_IMAGE_P: OpenFileDialog_Img('P'); break;
			case ID_MOUNT_IMAGE_Q: OpenFileDialog_Img('Q'); break;
			case ID_MOUNT_IMAGE_R: OpenFileDialog_Img('R'); break;
			case ID_MOUNT_IMAGE_S: OpenFileDialog_Img('S'); break;
			case ID_MOUNT_IMAGE_T: OpenFileDialog_Img('T'); break;
			case ID_MOUNT_IMAGE_U: OpenFileDialog_Img('U'); break;
			case ID_MOUNT_IMAGE_V: OpenFileDialog_Img('V'); break;
			case ID_MOUNT_IMAGE_W: OpenFileDialog_Img('W'); break;
			case ID_MOUNT_IMAGE_X: OpenFileDialog_Img('X'); break;
			case ID_MOUNT_IMAGE_Y: OpenFileDialog_Img('Y'); break;
			case ID_MOUNT_IMAGE_Z: OpenFileDialog_Img('Z'); break;
//			case ID_SSHOT: void CAPTURE_ScreenShotEvent(bool pressed); CAPTURE_ScreenShotEvent(true); break;
//			case ID_MOVIE: void CAPTURE_VideoEvent(bool pressed); CAPTURE_VideoEvent(true); break;
			case ID_WAVE: void CAPTURE_WaveEvent(bool pressed); CAPTURE_WaveEvent(true); break;
			case ID_OPL: void OPL_SaveRawEvent(bool pressed); OPL_SaveRawEvent(true); break;
			case ID_MIDI: void CAPTURE_MidiEvent(bool pressed); CAPTURE_MidiEvent(true); break;
			case ID_XMS: mem_conf("xms", 0); break;
			case ID_EMS_TRUE: mem_conf("ems", 1); break;
			case ID_EMS_FALSE: mem_conf("ems", 2); break;
			case ID_EMS_EMSBOARD: mem_conf("ems", 3); break;
			case ID_EMS_EMM386: mem_conf("ems", 4); break;
			case ID_UMB: mem_conf("umb", 0); break;
			case ID_SEND_CTRL_ESC: {
				KEYBOARD_AddKey(KBD_leftctrl, true);
				KEYBOARD_AddKey(KBD_esc, true);
				KEYBOARD_AddKey(KBD_leftctrl, false);
				KEYBOARD_AddKey(KBD_esc, false);
				break;
				}
			case ID_SEND_ALT_TAB: {
				KEYBOARD_AddKey(KBD_leftalt, true);
				KEYBOARD_AddKey(KBD_tab, true);
				KEYBOARD_AddKey(KBD_leftalt, false);
				KEYBOARD_AddKey(KBD_tab, false);
				break;
				}
			case ID_SEND_CTRL_ALT_DEL: {
				KEYBOARD_AddKey(KBD_leftctrl, true);
				KEYBOARD_AddKey(KBD_leftalt, true);
				KEYBOARD_AddKey(KBD_delete, true);
				KEYBOARD_AddKey(KBD_leftctrl, false);
				KEYBOARD_AddKey(KBD_leftalt, false);
				KEYBOARD_AddKey(KBD_delete, false);
				break;
				}
			case ID_CHAR9: MENU_SetBool("render", "char9"); break;
			case ID_MULTISCAN: MENU_SetBool("render", "multiscan"); break;
			case ID_DRVFORCE_DIRECTX: {
				load_videodrv = true;
				putenv("SDL_VIDEODRIVER=directx");
				GetSetSDLValue(0, "using_windib", (void*) false);
				void restart_program(std::vector<std::string> & parameters);
				restart_program(control->startup_params);
				break;
				}
			case ID_DRVFORCE_WINDIB: {
				load_videodrv = true;
				putenv("SDL_VIDEODRIVER=windib");
				GetSetSDLValue(0, "using_windib", (void*) true);
				void restart_program(std::vector<std::string> & parameters);
				restart_program(control->startup_params);
				break;
				}
			case ID_DRVFORCE_AUTO: {
				load_videodrv = false;
				void restart_program(std::vector<std::string> & parameters);
				restart_program(control->startup_params);
				break;
				}
			case ID_VSYNC_ON: SetVal("vsync", "vsyncmode", "on"); break;
			case ID_VSYNC_HOST: SetVal("vsync", "vsyncmode", "host"); break;
			case ID_VSYNC_FORCE: SetVal("vsync", "vsyncmode", "force"); break;
			case ID_VSYNC_OFF: SetVal("vsync", "vsyncmode", "off"); break;
			case ID_SURFACE: if ((int) GetSetSDLValue(1, "desktop.want_type", 0) != SCREEN_SURFACE) { change_output(0); SetVal("sdl", "output", "surface"); } break;
			case ID_DDRAW: if ((int) GetSetSDLValue(1, "desktop.want_type", 0) != SCREEN_SURFACE_DDRAW) { change_output(1); SetVal("sdl", "output", "ddraw"); } break;
			case ID_OVERLAY: if ((int) GetSetSDLValue(1, "desktop.want_type", 0) != SCREEN_OVERLAY) { change_output(2); SetVal("sdl", "output", "overlay"); } break;
			case ID_OPENGL: change_output(3); SetVal("sdl", "output", "opengl"); break;
			case ID_OPENGLNB: change_output(4); SetVal("sdl", "output", "openglnb"); break;
			case ID_DIRECT3D: if ((int) GetSetSDLValue(1, "desktop.want_type", 0) != SCREEN_DIRECT3D) { change_output(5); SetVal("sdl", "output", "direct3d"); } break;
			case ID_OPENGLHQ: if ((int) GetSetSDLValue(1, "desktop.want_type", 0) != SCREEN_OPENGLHQ) { change_output(6); SetVal("sdl", "output", "openglhq"); } break;
			case ID_WINFULL_USER: case ID_WINRES_USER: GUI_Shortcut(2); break;
			case ID_WINRES_ORIGINAL: res_input(true, "original"); break;
			case ID_WINFULL_ORIGINAL: res_input(false, "original"); break;
			case ID_WINRES_DESKTOP: res_input(true, "desktop"); break;
			case ID_WINFULL_DESKTOP: res_input(false, "desktop"); break;
			case ID_FULLDOUBLE: SetVal("sdl", "fulldouble", (GetSetSDLValue(1, "desktop.doublebuf", 0)) ? "false" : "true"); res_init(); break;
			case ID_AUTOLOCK: (GetSetSDLValue(0, "mouse.autoenable", (void*)MENU_SetBool("sdl", "autolock"))); break;
			case ID_MOUSE: extern bool Mouse_Drv; Mouse_Drv = !Mouse_Drv; break;
			case ID_KEY_NONE: SetVal("dos", "keyboardlayout", "auto"); break;
			case ID_KEY_BG: SetVal("dos", "keyboardlayout", "bg"); break;
			case ID_KEY_CZ: SetVal("dos", "keyboardlayout", "cz"); break;
			case ID_KEY_FR: SetVal("dos", "keyboardlayout", "fr"); break;
			case ID_KEY_GK: SetVal("dos", "keyboardlayout", "gk"); break;
			case ID_KEY_GR: SetVal("dos", "keyboardlayout", "gr"); break;
			case ID_KEY_HR: SetVal("dos", "keyboardlayout", "hr"); break;
			case ID_KEY_HU: SetVal("dos", "keyboardlayout", "hu"); break;
			case ID_KEY_IT: SetVal("dos", "keyboardlayout", "it"); break;
			case ID_KEY_NL: SetVal("dos", "keyboardlayout", "nl"); break;
			case ID_KEY_NO: SetVal("dos", "keyboardlayout", "no"); break;
			case ID_KEY_PL: SetVal("dos", "keyboardlayout", "pl"); break;
			case ID_KEY_RU: SetVal("dos", "keyboardlayout", "ru"); break;
			case ID_KEY_SK: SetVal("dos", "keyboardlayout", "sk"); break;
			case ID_KEY_SP: SetVal("dos", "keyboardlayout", "sp"); break;
			case ID_KEY_SU: SetVal("dos", "keyboardlayout", "su"); break;
			case ID_KEY_SV: SetVal("dos", "keyboardlayout", "sv"); break;
			case ID_KEY_BE: SetVal("dos", "keyboardlayout", "be"); break;
			case ID_KEY_BR: SetVal("dos", "keyboardlayout", "br"); break;
			case ID_KEY_CF: SetVal("dos", "keyboardlayout", "cf"); break;
			case ID_KEY_DK: SetVal("dos", "keyboardlayout", "dk"); break;
			case ID_KEY_LA: SetVal("dos", "keyboardlayout", "la"); break;
			case ID_KEY_PO: SetVal("dos", "keyboardlayout", "po"); break;
			case ID_KEY_SF: SetVal("dos", "keyboardlayout", "sf"); break;
			case ID_KEY_SG: SetVal("dos", "keyboardlayout", "sg"); break;
			case ID_KEY_UK: SetVal("dos", "keyboardlayout", "uk"); break;
			case ID_KEY_US: SetVal("dos", "keyboardlayout", "us"); break;
			case ID_KEY_YU: SetVal("dos", "keyboardlayout", "yu"); break;
			case ID_KEY_FO: SetVal("dos", "keyboardlayout", "fo"); break;
			case ID_KEY_MK: SetVal("dos", "keyboardlayout", "mk"); break;
			case ID_KEY_MT: SetVal("dos", "keyboardlayout", "mt"); break;
			case ID_KEY_PH: SetVal("dos", "keyboardlayout", "ph"); break;
			case ID_KEY_RO: SetVal("dos", "keyboardlayout", "ro"); break;
			case ID_KEY_SQ: SetVal("dos", "keyboardlayout", "sq"); break;
			case ID_KEY_TM: SetVal("dos", "keyboardlayout", "tm"); break;
			case ID_KEY_TR: SetVal("dos", "keyboardlayout", "tr"); break;
			case ID_KEY_UX: SetVal("dos", "keyboardlayout", "ux"); break;
			case ID_KEY_YC: SetVal("dos", "keyboardlayout", "yc"); break;
			case ID_KEY_DV: SetVal("dos", "keyboardlayout", "dv"); break;
			case ID_KEY_RH: SetVal("dos", "keyboardlayout", "rh"); break;
			case ID_KEY_LH: SetVal("dos", "keyboardlayout", "lh"); break;
			case ID_MIDI_NONE: SetVal("midi", "mpu401", "none"); break;
			case ID_MIDI_UART: SetVal("midi", "mpu401", "uart"); break;
			case ID_MIDI_INTELLI: SetVal("midi", "mpu401", "intelligent"); break;
			case ID_MIDI_DEFAULT: SetVal("midi", "mididevice", "default"); break;
			case ID_MIDI_ALSA: SetVal("midi", "mididevice", "alsa"); break;
			case ID_MIDI_OSS: SetVal("midi", "mididevice", "oss"); break;
			case ID_MIDI_WIN32: SetVal("midi", "mididevice", "win32"); break;
			case ID_MIDI_COREAUDIO: SetVal("midi", "mididevice", "coreaudio"); break;
			case ID_MIDI_COREMIDI: SetVal("midi", "mididevice", "coremidi"); break;
			case ID_MIDI_MT32: SetVal("midi", "mididevice", "mt32"); break;
			case ID_MIDI_SYNTH: SetVal("midi", "mididevice", "synth"); break;
			case ID_MIDI_TIMIDITY: SetVal("midi", "mididevice", "timidity"); break;
			case ID_MIDI_MT32_REVERBMODE_AUTO: SetVal("midi", "mt32.reverb.mode", "auto"); break;
			case ID_MIDI_MT32_REVERBMODE_0: SetVal("midi", "mt32.reverb.mode", "0"); break;
			case ID_MIDI_MT32_REVERBMODE_1: SetVal("midi", "mt32.reverb.mode", "1"); break;
			case ID_MIDI_MT32_REVERBMODE_2: SetVal("midi", "mt32.reverb.mode", "2"); break;
			case ID_MIDI_MT32_REVERBMODE_3: SetVal("midi", "mt32.reverb.mode", "3"); break;
			case ID_MIDI_MT32_DAC_AUTO: SetVal("midi", "mt32.dac", "auto"); break;
			case ID_MIDI_MT32_DAC_0: SetVal("midi", "mt32.dac", "0"); break;
			case ID_MIDI_MT32_DAC_1: SetVal("midi", "mt32.dac", "1"); break;
			case ID_MIDI_MT32_DAC_2: SetVal("midi", "mt32.dac", "2"); break;
			case ID_MIDI_MT32_DAC_3: SetVal("midi", "mt32.dac", "3"); break;
			case ID_MIDI_MT32_REVERBTIME_0: SetVal("midi", "mt32.reverb.time", "0"); break;
			case ID_MIDI_MT32_REVERBTIME_1: SetVal("midi", "mt32.reverb.time", "1"); break;
			case ID_MIDI_MT32_REVERBTIME_2: SetVal("midi", "mt32.reverb.time", "2"); break;
			case ID_MIDI_MT32_REVERBTIME_3: SetVal("midi", "mt32.reverb.time", "3"); break;
			case ID_MIDI_MT32_REVERBTIME_4: SetVal("midi", "mt32.reverb.time", "4"); break;
			case ID_MIDI_MT32_REVERBTIME_5: SetVal("midi", "mt32.reverb.time", "5"); break;
			case ID_MIDI_MT32_REVERBTIME_6: SetVal("midi", "mt32.reverb.time", "6"); break;
			case ID_MIDI_MT32_REVERBTIME_7: SetVal("midi", "mt32.reverb.time", "7"); break;
			case ID_MIDI_MT32_REVERBLEV_0: SetVal("midi", "mt32.reverb.level", "0"); break;
			case ID_MIDI_MT32_REVERBLEV_1: SetVal("midi", "mt32.reverb.level", "1"); break;
			case ID_MIDI_MT32_REVERBLEV_2: SetVal("midi", "mt32.reverb.level", "2"); break;
			case ID_MIDI_MT32_REVERBLEV_3: SetVal("midi", "mt32.reverb.level", "3"); break;
			case ID_MIDI_MT32_REVERBLEV_4: SetVal("midi", "mt32.reverb.level", "4"); break;
			case ID_MIDI_MT32_REVERBLEV_5: SetVal("midi", "mt32.reverb.level", "5"); break;
			case ID_MIDI_MT32_REVERBLEV_6: SetVal("midi", "mt32.reverb.level", "6"); break;
			case ID_MIDI_MT32_REVERBLEV_7: SetVal("midi", "mt32.reverb.level", "7"); break;
			case ID_MIDI_MT32_REVERSESTEREO_TRUE: SetVal("midi", "mt32ReverseStereo", "on"); break;
			case ID_MIDI_MT32_REVERSESTEREO_FALSE: SetVal("midi", "mt32ReverseStereo", "off"); break;
			case ID_MIDI_DEV_NONE: SetVal("midi", "mididevice", "none"); break;
			case ID_GUS_TRUE:
			{
				Section_prop * sec = static_cast<Section_prop *>(control->GetSection("gus"));
				if(sec) SetVal("gus", "gus", sec->Get_bool("gus")?"false":"true"); break;
			}
			case ID_GUS_49716: SetVal("gus", "gusrate", "49716"); break;
			case ID_GUS_48000: SetVal("gus", "gusrate", "48000"); break;
			case ID_GUS_44100: SetVal("gus", "gusrate", "44100"); break;
			case ID_GUS_32000:  SetVal("gus", "gusrate", "32000"); break;
			case ID_GUS_22050: SetVal("gus", "gusrate", "22050"); break;
			case ID_GUS_16000: SetVal("gus", "gusrate", "16000"); break;
			case ID_GUS_11025: SetVal("gus", "gusrate", "11025"); break;
			case ID_GUS_8000: SetVal("gus", "gusrate", "8000"); break;
			case ID_GUS_300: SetVal("gus", "gusbase", "300"); break;
			case ID_GUS_280: SetVal("gus", "gusbase", "280"); break;
			case ID_GUS_260: SetVal("gus", "gusbase", "260"); break;
			case ID_GUS_240: SetVal("gus", "gusbase", "240"); break;
			case ID_GUS_220: SetVal("gus", "gusbase", "220"); break;
			case ID_GUS_2a0: SetVal("gus", "gusbase", "2a0"); break;
			case ID_GUS_2c0: SetVal("gus", "gusbase", "2c0"); break;
			case ID_GUS_2e0: SetVal("gus", "gusbase", "2e0"); break;
			case ID_GUS_IRQ_3: SetVal("gus", "gusirq", "3"); break;
			case ID_GUS_IRQ_5: SetVal("gus", "gusirq", "5"); break;
			case ID_GUS_IRQ_7: SetVal("gus", "gusirq", "7"); break;
			case ID_GUS_IRQ_9: SetVal("gus", "gusirq", "9"); break;
			case ID_GUS_IRQ_10: SetVal("gus", "gusirq", "10"); break;
			case ID_GUS_IRQ_11: SetVal("gus", "gusirq", "11"); break;
			case ID_GUS_IRQ_12: SetVal("gus", "gusirq", "12"); break;
			case ID_GUS_DMA_0: SetVal("gus", "gusdma", "0"); break;
			case ID_GUS_DMA_1: SetVal("gus", "gusdma", "1"); break;
			case ID_GUS_DMA_3: SetVal("gus", "gusdma", "3"); break;
			case ID_GUS_DMA_5: SetVal("gus", "gusdma", "5"); break;
			case ID_GUS_DMA_6: SetVal("gus", "gusdma", "6"); break;
			case ID_GUS_DMA_7: SetVal("gus", "gusdma", "7"); break;
			case ID_INNOVA_TRUE:
			{
			Section_prop * sec =  static_cast<Section_prop *>(control->GetSection("innova"));
			if(sec) SetVal("innova", "innova", sec->Get_bool("innova")?"false":"true"); break;
			}
			case ID_INNOVA_49716: SetVal("innova", "samplerate", "49716"); break;
			case ID_INNOVA_48000: SetVal("innova", "samplerate", "48000"); break;
			case ID_INNOVA_44100: SetVal("innova", "samplerate", "44100"); break;
			case ID_INNOVA_32000: SetVal("innova", "samplerate", "32000"); break;
			case ID_INNOVA_22050: SetVal("innova", "samplerate", "22050"); break;
			case ID_INNOVA_11025: SetVal("innova", "samplerate", "11025"); break;
			case ID_INNOVA_8000: SetVal("innova", "samplerate", "8000"); break;
			case ID_INNOVA_280: SetVal("innova", "sidbase", "280"); break;
			case ID_INNOVA_2A0: SetVal("innova", "sidbase", "2a0"); break;
			case ID_INNOVA_2C0: SetVal("innova", "sidbase", "2c0"); break;
			case ID_INNOVA_2E0: SetVal("innova", "sidbase", "2e0"); break;
			case ID_INNOVA_220: SetVal("innova", "sidbase", "220"); break;
			case ID_INNOVA_240: SetVal("innova", "sidbase", "240"); break;
			case ID_INNOVA_260: SetVal("innova", "sidbase", "260"); break;
			case ID_INNOVA_300: SetVal("innova", "sidbase", "300"); break;
			case ID_INNOVA_3: SetVal("innova", "quality", "3"); break;
			case ID_INNOVA_2: SetVal("innova", "quality", "2"); break;
			case ID_INNOVA_1: SetVal("innova", "quality", "1"); break;
			case ID_INNOVA_0: SetVal("innova", "quality", "0"); break;
			case ID_PCSPEAKER_TRUE:
			{
			Section_prop * sec = static_cast<Section_prop *>(control->GetSection("speaker"));
			if(sec) SetVal("speaker", "pcspeaker", sec->Get_bool("pcspeaker")?"false":"true"); break;
			}
			case ID_PCSPEAKER_49716: SetVal("speaker", "pcrate", "49716"); break;
			case ID_PCSPEAKER_48000: SetVal("speaker", "pcrate", "48000"); break;
			case ID_PCSPEAKER_44100: SetVal("speaker", "pcrate", "44100"); break;
			case ID_PCSPEAKER_32000: SetVal("speaker", "pcrate", "32000"); break;
			case ID_PCSPEAKER_22050: SetVal("speaker", "pcrate", "22050"); break;
			case ID_PCSPEAKER_16000: SetVal("speaker", "pcrate", "16000"); break;
			case ID_PCSPEAKER_11025: SetVal("speaker", "pcrate", "11025"); break;
			case ID_PCSPEAKER_8000: SetVal("speaker", "pcrate", "8000"); break;
			case ID_PS1_ON: SetVal("speaker", "ps1audio", "on"); break;
			case ID_PS1_OFF: SetVal("speaker", "ps1audio", "off"); break;
			case ID_PS1_49716: SetVal("speaker", "ps1audiorate", "49716"); break;
			case ID_PS1_48000: SetVal("speaker", "ps1audiorate", "48000"); break;
			case ID_PS1_44100: SetVal("speaker", "ps1audiorate", "44100"); break;
			case ID_PS1_32000: SetVal("speaker", "ps1audiorate", "32000"); break;
			case ID_PS1_22050: SetVal("speaker", "ps1audiorate", "22050"); break;
			case ID_PS1_16000: SetVal("speaker", "ps1audiorate", "16000"); break;
			case ID_PS1_11025: SetVal("speaker", "ps1audiorate", "11025"); break;
			case ID_PS1_8000: SetVal("speaker", "ps1audiorate", "8000"); break;
			case ID_TANDY_ON: SetVal("speaker", "tandy", "on"); break;
			case ID_TANDY_OFF: SetVal("speaker", "tandy", "off"); break;
			case ID_TANDY_AUTO: SetVal("speaker", "tandy", "auto"); break;
			case ID_TANDY_49716: SetVal("speaker", "tandyrate", "49716"); break;
			case ID_TANDY_48000: SetVal("speaker", "tandyrate", "48000"); break;
			case ID_TANDY_44100: SetVal("speaker", "tandyrate", "44100"); break;
			case ID_TANDY_32000: SetVal("speaker", "tandyrate", "32000"); break;
			case ID_TANDY_22050: SetVal("speaker", "tandyrate", "22050"); break;
			case ID_TANDY_16000: SetVal("speaker", "tandyrate", "16000"); break;
			case ID_TANDY_11025: SetVal("speaker", "tandyrate", "11025"); break;
			case ID_TANDY_8000: SetVal("speaker", "tandyrate", "8000"); break;
			case ID_DISNEY_TRUE: SetVal("speaker", "disney", "true"); break;
			case ID_DISNEY_FALSE: SetVal("speaker", "disney", "false"); break;
			case ID_SB_NONE: SetVal("sblaster", "sbtype", "none"); break;
			case ID_SB_SB1: SetVal("sblaster", "sbtype", "sb1"); break;
			case ID_SB_SB2: SetVal("sblaster", "sbtype", "sb2"); break;
			case ID_SB_SBPRO1: SetVal("sblaster", "sbtype", "sbpro1"); break;
			case ID_SB_SBPRO2: SetVal("sblaster", "sbtype", "sbpro2"); break;
			case ID_SB_SB16: SetVal("sblaster", "sbtype", "sb16"); break;
			case ID_SB_SB16VIBRA: SetVal("sblaster", "sbtype", "sb16vibra"); break;
			case ID_SB_GB: SetVal("sblaster", "sbtype", "gb"); break;
			case ID_SB_300: SetVal("sblaster", "sbbase", "300"); break;
			case ID_SB_220: SetVal("sblaster", "sbbase", "220"); break;
			case ID_SB_240: SetVal("sblaster", "sbbase", "240"); break;
			case ID_SB_260: SetVal("sblaster", "sbbase", "260"); break;
			case ID_SB_280: SetVal("sblaster", "sbbase", "280"); break;
			case ID_SB_2a0: SetVal("sblaster", "sbbase", "2a0"); break;
			case ID_SB_2c0: SetVal("sblaster", "sbbase", "2c0"); break;
			case ID_SB_2e0: SetVal("sblaster", "sbbase", "2e0"); break;
			case ID_SB_HW210: SetVal("sblaster", "hardwarebase", "210"); break;
			case ID_SB_HW220: SetVal("sblaster", "hardwarebase", "220"); break;
			case ID_SB_HW230: SetVal("sblaster", "hardwarebase", "230"); break;
			case ID_SB_HW240: SetVal("sblaster", "hardwarebase", "240"); break;
			case ID_SB_HW250: SetVal("sblaster", "hardwarebase", "250"); break;
			case ID_SB_HW260: SetVal("sblaster", "hardwarebase", "260"); break;
			case ID_SB_HW280: SetVal("sblaster", "hardwarebase", "280"); break;
			case ID_SB_IRQ_3: SetVal("sblaster", "irq", "3"); break;
			case ID_SB_IRQ_5: SetVal("sblaster", "irq", "5"); break;
			case ID_SB_IRQ_7: SetVal("sblaster", "irq", "7"); break;
			case ID_SB_IRQ_9: SetVal("sblaster", "irq", "9"); break;
			case ID_SB_IRQ_10: SetVal("sblaster", "irq", "10"); break;
			case ID_SB_IRQ_11: SetVal("sblaster", "irq", "11"); break;
			case ID_SB_IRQ_12: SetVal("sblaster", "irq", "12"); break;
			case ID_SB_DMA_0: SetVal("sblaster", "dma", "0"); break;
			case ID_SB_DMA_1: SetVal("sblaster", "dma", "1"); break;
			case ID_SB_DMA_3: SetVal("sblaster", "dma", "3"); break;
			case ID_SB_DMA_5: SetVal("sblaster", "dma", "5"); break;
			case ID_SB_DMA_6: SetVal("sblaster", "dma", "6"); break;
			case ID_SB_DMA_7: SetVal("sblaster", "dma", "7"); break;
			case ID_SB_HDMA_0: SetVal("sblaster", "hdma", "0"); break;
			case ID_SB_HDMA_1: SetVal("sblaster", "hdma", "1"); break;
			case ID_SB_HDMA_3: SetVal("sblaster", "hdma", "3"); break;
			case ID_SB_HDMA_5: SetVal("sblaster", "hdma", "5"); break;
			case ID_SB_HDMA_6: SetVal("sblaster", "hdma", "6"); break;
			case ID_SB_HDMA_7: SetVal("sblaster", "hdma", "7"); break;
			case ID_SB_OPL_AUTO: SetVal("sblaster", "oplmode", "auto"); break;
			case ID_SB_OPL_NONE: SetVal("sblaster", "oplmode", "none"); break;
			case ID_SB_OPL_CMS: SetVal("sblaster", "oplmode", "cms"); break;
			case ID_SB_OPL_OPL2: SetVal("sblaster", "oplmode", "opl2"); break;
			case ID_SB_OPL_DUALOPL2: SetVal("sblaster", "oplmode", "dualopl2"); break;
			case ID_SB_OPL_OPL3: SetVal("sblaster", "oplmode", "opl3"); break;
			case ID_SB_OPL_HARDWARE: SetVal("sblaster", "oplmode", "hardware"); break;
			case ID_SB_OPL_HARDWAREGB: SetVal("sblaster", "oplmode", "hardwaregb"); break;
			case ID_SB_OPL_EMU_DEFAULT: SetVal("sblaster", "oplemu", "default"); break;
			case ID_SB_OPL_EMU_COMPAT: SetVal("sblaster", "oplemu", "compat"); break;
			case ID_SB_OPL_EMU_FAST: SetVal("sblaster", "oplemu", "fast"); break;
			case ID_SB_OPL_49716: SetVal("sblaster", "oplrate", "49716"); break;
			case ID_SB_OPL_48000: SetVal("sblaster", "oplrate", "48000"); break;
			case ID_SB_OPL_44100: SetVal("sblaster", "oplrate", "44100"); break;
			case ID_SB_OPL_32000: SetVal("sblaster", "oplrate", "32000"); break;
			case ID_SB_OPL_22050: SetVal("sblaster", "oplrate", "22050"); break;
			case ID_SB_OPL_16000: SetVal("sblaster", "oplrate", "16000"); break;
			case ID_SB_OPL_11025: SetVal("sblaster", "oplrate", "11025"); break;
			case ID_SB_OPL_8000: SetVal("sblaster", "oplrate", "8000"); break;
			case ID_OVERSCAN_0: LOG_MSG("GUI: Overscan 0 (surface)"); SetVal("sdl", "overscan", "0"); change_output(7); break;
			case ID_OVERSCAN_1: LOG_MSG("GUI: Overscan 1 (surface)"); SetVal("sdl", "overscan", "1"); change_output(7); break;
			case ID_OVERSCAN_2: LOG_MSG("GUI: Overscan 2 (surface)"); SetVal("sdl", "overscan", "2"); change_output(7); break;
			case ID_OVERSCAN_3: LOG_MSG("GUI: Overscan 3 (surface)"); SetVal("sdl", "overscan", "3"); change_output(7); break;
			case ID_OVERSCAN_4: LOG_MSG("GUI: Overscan 4 (surface)"); SetVal("sdl", "overscan", "4"); change_output(7); break;
			case ID_OVERSCAN_5: LOG_MSG("GUI: Overscan 5 (surface)"); SetVal("sdl", "overscan", "5"); change_output(7); break;
			case ID_OVERSCAN_6: LOG_MSG("GUI: Overscan 6 (surface)"); SetVal("sdl", "overscan", "6"); change_output(7); break;
			case ID_OVERSCAN_7: LOG_MSG("GUI: Overscan 7 (surface)"); SetVal("sdl", "overscan", "7"); change_output(7); break;
			case ID_OVERSCAN_8: LOG_MSG("GUI: Overscan 8 (surface)"); SetVal("sdl", "overscan", "8"); change_output(7); break;
			case ID_OVERSCAN_9: LOG_MSG("GUI: Overscan 9 (surface)"); SetVal("sdl", "overscan", "9"); change_output(7); break;
			case ID_OVERSCAN_10: LOG_MSG("GUI: Overscan 10 (surface)"); SetVal("sdl", "overscan", "10"); change_output(7); break;
			case ID_VSYNC: GUI_Shortcut(17); break;
			case ID_IPXNET: MENU_SetBool("ipx", "ipx"); break;
			case ID_D3D_PS: D3D_PS(); if ((int) GetSetSDLValue(1, "desktop.want_type", 0) == SCREEN_DIRECT3D) change_output(7); break;
			case ID_JOYSTICKTYPE_AUTO: SetVal("joystick", "joysticktype", "auto"); break;
			case ID_JOYSTICKTYPE_2AXIS: SetVal("joystick", "joysticktype", "2axis"); break;
			case ID_JOYSTICKTYPE_4AXIS: SetVal("joystick", "joysticktype", "4axis"); break;
			case ID_JOYSTICKTYPE_4AXIS_2: SetVal("joystick", "joysticktype", "4axis_2"); break;
			case ID_JOYSTICKTYPE_FCS: SetVal("joystick", "joysticktype", "fcs"); break;
			case ID_JOYSTICKTYPE_CH: SetVal("joystick", "joysticktype", "ch"); break;
			case ID_JOYSTICKTYPE_NONE: SetVal("joystick", "joysticktype", "none"); break;
			case ID_JOYSTICK_TIMED: MENU_SetBool("joystick", "timed"); break;
			case ID_JOYSTICK_AUTOFIRE: MENU_SetBool("joystick", "autofire"); break;
			case ID_JOYSTICK_SWAP34: MENU_SetBool("joystick", "swap34"); break;
			case ID_JOYSTICK_BUTTONWRAP: MENU_SetBool("joystick", "buttonwrap"); break;
			case ID_SWAPSTEREO: MENU_swapstereo(MENU_SetBool("mixer", "swapstereo")); break;
			case ID_MUTE: SDL_PauseAudio((SDL_GetAudioStatus() != SDL_AUDIO_PAUSED)); break;
			case ID_DOSBOX_SECTION:  GUI_Shortcut(3); break;
			case ID_MIXER_SECTION:  GUI_Shortcut(4); break;
			case ID_SERIAL_SECTION:  GUI_Shortcut(5); break;
			case ID_PARALLEL_SECTION:  GUI_Shortcut(11); break;
			case ID_PRINTER_SECTION:  GUI_Shortcut(12); break;
			case ID_NE2000_SECTION:  GUI_Shortcut(6); break;
			case ID_AUTOEXEC:  GUI_Shortcut(7); break;
			case ID_MOUSE_VERTICAL: extern bool Mouse_Vertical; Mouse_Vertical = !Mouse_Vertical; break;
			case ID_GLIDE_TRUE:
			{	
			Section_prop * sec = static_cast<Section_prop *>(control->GetSection("glide"));
			if(sec) SetVal("glide", "glide", sec->Get_string("glide")=="true"?"false":"true");
			break;
			}
			case ID_GLIDE_EMU:
			{	
			Section_prop * sec = static_cast<Section_prop *>(control->GetSection("glide"));
			if(sec) SetVal("glide", "glide", sec->Get_string("glide")=="emu"?"false":"emu");
			break;
			}
			case ID_SAVELANG:  GUI_Shortcut(9); break;
			case ID_CPUTYPE_AUTO: SetVal("cpu", "cputype", "auto"); break;
			case ID_CPUTYPE_386: SetVal("cpu", "cputype", "386"); break;
			//case ID_CPUTYPE_386_SLOW: SetVal("cpu","cputype","386_slow"); break;
			case ID_CPUTYPE_386_PREFETCH: SetVal("cpu", "cputype", "386_prefetch"); break;
			case ID_CPUTYPE_486: SetVal("cpu", "cputype", "486"); break;
			case ID_CPUTYPE_PENTIUM: SetVal("cpu", "cputype", "pentium"); break;
			case ID_CPUTYPE_PENTIUM_MMX: SetVal("cpu", "cputype", "pentium_mmx"); break;
			case ID_CPU_ADVANCED:  GUI_Shortcut(13); break;
			case ID_DOS_ADVANCED:  GUI_Shortcut(14); break;
			case ID_MIDI_ADVANCED:  GUI_Shortcut(15); break;
			case ID_RATE_1_DELAY_1: MENU_KeyDelayRate(1, 1); break;
			case ID_RATE_2_DELAY_1: MENU_KeyDelayRate(1, 2); break;
			case ID_RATE_3_DELAY_1: MENU_KeyDelayRate(1, 3); break;
			case ID_RATE_4_DELAY_1: MENU_KeyDelayRate(1, 4); break;
			case ID_RATE_5_DELAY_1: MENU_KeyDelayRate(1, 5); break;
			case ID_RATE_6_DELAY_1: MENU_KeyDelayRate(1, 6); break;
			case ID_RATE_7_DELAY_1: MENU_KeyDelayRate(1, 7); break;
			case ID_RATE_8_DELAY_1: MENU_KeyDelayRate(1, 8); break;
			case ID_RATE_9_DELAY_1: MENU_KeyDelayRate(1, 9); break;
			case ID_RATE_10_DELAY_1: MENU_KeyDelayRate(1, 10); break;
			case ID_RATE_11_DELAY_1: MENU_KeyDelayRate(1, 11); break;
			case ID_RATE_12_DELAY_1: MENU_KeyDelayRate(1, 12); break;
			case ID_RATE_13_DELAY_1: MENU_KeyDelayRate(1, 13); break;
			case ID_RATE_14_DELAY_1: MENU_KeyDelayRate(1, 14); break;
			case ID_RATE_15_DELAY_1: MENU_KeyDelayRate(1, 15); break;
			case ID_RATE_16_DELAY_1: MENU_KeyDelayRate(1, 16); break;
			case ID_RATE_17_DELAY_1: MENU_KeyDelayRate(1, 17); break;
			case ID_RATE_18_DELAY_1: MENU_KeyDelayRate(1, 18); break;
			case ID_RATE_19_DELAY_1: MENU_KeyDelayRate(1, 19); break;
			case ID_RATE_20_DELAY_1: MENU_KeyDelayRate(1, 20); break;
			case ID_RATE_21_DELAY_1: MENU_KeyDelayRate(1, 21); break;
			case ID_RATE_22_DELAY_1: MENU_KeyDelayRate(1, 22); break;
			case ID_RATE_23_DELAY_1: MENU_KeyDelayRate(1, 23); break;
			case ID_RATE_24_DELAY_1: MENU_KeyDelayRate(1, 24); break;
			case ID_RATE_25_DELAY_1: MENU_KeyDelayRate(1, 25); break;
			case ID_RATE_26_DELAY_1: MENU_KeyDelayRate(1, 26); break;
			case ID_RATE_27_DELAY_1: MENU_KeyDelayRate(1, 27); break;
			case ID_RATE_28_DELAY_1: MENU_KeyDelayRate(1, 28); break;
			case ID_RATE_29_DELAY_1: MENU_KeyDelayRate(1, 29); break;
			case ID_RATE_30_DELAY_1: MENU_KeyDelayRate(1, 30); break;
			case ID_RATE_31_DELAY_1: MENU_KeyDelayRate(1, 31); break;
			case ID_RATE_32_DELAY_1: MENU_KeyDelayRate(1, 32); break;
			case ID_RATE_1_DELAY_2: MENU_KeyDelayRate(2, 1); break;
			case ID_RATE_2_DELAY_2: MENU_KeyDelayRate(2, 2); break;
			case ID_RATE_3_DELAY_2: MENU_KeyDelayRate(2, 3); break;
			case ID_RATE_4_DELAY_2: MENU_KeyDelayRate(2, 4); break;
			case ID_RATE_5_DELAY_2: MENU_KeyDelayRate(2, 5); break;
			case ID_RATE_6_DELAY_2: MENU_KeyDelayRate(2, 6); break;
			case ID_RATE_7_DELAY_2: MENU_KeyDelayRate(2, 7); break;
			case ID_RATE_8_DELAY_2: MENU_KeyDelayRate(2, 8); break;
			case ID_RATE_9_DELAY_2: MENU_KeyDelayRate(2, 9); break;
			case ID_RATE_10_DELAY_2: MENU_KeyDelayRate(2, 10); break;
			case ID_RATE_11_DELAY_2: MENU_KeyDelayRate(2, 11); break;
			case ID_RATE_12_DELAY_2: MENU_KeyDelayRate(2, 12); break;
			case ID_RATE_13_DELAY_2: MENU_KeyDelayRate(2, 13); break;
			case ID_RATE_14_DELAY_2: MENU_KeyDelayRate(2, 14); break;
			case ID_RATE_15_DELAY_2: MENU_KeyDelayRate(2, 15); break;
			case ID_RATE_16_DELAY_2: MENU_KeyDelayRate(2, 16); break;
			case ID_RATE_17_DELAY_2: MENU_KeyDelayRate(2, 17); break;
			case ID_RATE_18_DELAY_2: MENU_KeyDelayRate(2, 18); break;
			case ID_RATE_19_DELAY_2: MENU_KeyDelayRate(2, 19); break;
			case ID_RATE_20_DELAY_2: MENU_KeyDelayRate(2, 20); break;
			case ID_RATE_21_DELAY_2: MENU_KeyDelayRate(2, 21); break;
			case ID_RATE_22_DELAY_2: MENU_KeyDelayRate(2, 22); break;
			case ID_RATE_23_DELAY_2: MENU_KeyDelayRate(2, 23); break;
			case ID_RATE_24_DELAY_2: MENU_KeyDelayRate(2, 24); break;
			case ID_RATE_25_DELAY_2: MENU_KeyDelayRate(2, 25); break;
			case ID_RATE_26_DELAY_2: MENU_KeyDelayRate(2, 26); break;
			case ID_RATE_27_DELAY_2: MENU_KeyDelayRate(2, 27); break;
			case ID_RATE_28_DELAY_2: MENU_KeyDelayRate(2, 28); break;
			case ID_RATE_29_DELAY_2: MENU_KeyDelayRate(2, 29); break;
			case ID_RATE_30_DELAY_2: MENU_KeyDelayRate(2, 30); break;
			case ID_RATE_31_DELAY_2: MENU_KeyDelayRate(2, 31); break;
			case ID_RATE_32_DELAY_2: MENU_KeyDelayRate(2, 32); break;
			case ID_RATE_1_DELAY_3: MENU_KeyDelayRate(3, 1); break;
			case ID_RATE_2_DELAY_3: MENU_KeyDelayRate(3, 2); break;
			case ID_RATE_3_DELAY_3: MENU_KeyDelayRate(3, 3); break;
			case ID_RATE_4_DELAY_3: MENU_KeyDelayRate(3, 4); break;
			case ID_RATE_5_DELAY_3: MENU_KeyDelayRate(3, 5); break;
			case ID_RATE_6_DELAY_3: MENU_KeyDelayRate(3, 6); break;
			case ID_RATE_7_DELAY_3: MENU_KeyDelayRate(3, 7); break;
			case ID_RATE_8_DELAY_3: MENU_KeyDelayRate(3, 8); break;
			case ID_RATE_9_DELAY_3: MENU_KeyDelayRate(3, 9); break;
			case ID_RATE_10_DELAY_3: MENU_KeyDelayRate(3, 10); break;
			case ID_RATE_11_DELAY_3: MENU_KeyDelayRate(3, 11); break;
			case ID_RATE_12_DELAY_3: MENU_KeyDelayRate(3, 12); break;
			case ID_RATE_13_DELAY_3: MENU_KeyDelayRate(3, 13); break;
			case ID_RATE_14_DELAY_3: MENU_KeyDelayRate(3, 14); break;
			case ID_RATE_15_DELAY_3: MENU_KeyDelayRate(3, 15); break;
			case ID_RATE_16_DELAY_3: MENU_KeyDelayRate(3, 16); break;
			case ID_RATE_17_DELAY_3: MENU_KeyDelayRate(3, 17); break;
			case ID_RATE_18_DELAY_3: MENU_KeyDelayRate(3, 18); break;
			case ID_RATE_19_DELAY_3: MENU_KeyDelayRate(3, 19); break;
			case ID_RATE_20_DELAY_3: MENU_KeyDelayRate(3, 20); break;
			case ID_RATE_21_DELAY_3: MENU_KeyDelayRate(3, 21); break;
			case ID_RATE_22_DELAY_3: MENU_KeyDelayRate(3, 22); break;
			case ID_RATE_23_DELAY_3: MENU_KeyDelayRate(3, 23); break;
			case ID_RATE_24_DELAY_3: MENU_KeyDelayRate(3, 24); break;
			case ID_RATE_25_DELAY_3: MENU_KeyDelayRate(3, 25); break;
			case ID_RATE_26_DELAY_3: MENU_KeyDelayRate(3, 26); break;
			case ID_RATE_27_DELAY_3: MENU_KeyDelayRate(3, 27); break;
			case ID_RATE_28_DELAY_3: MENU_KeyDelayRate(3, 28); break;
			case ID_RATE_29_DELAY_3: MENU_KeyDelayRate(3, 29); break;
			case ID_RATE_30_DELAY_3: MENU_KeyDelayRate(3, 30); break;
			case ID_RATE_31_DELAY_3: MENU_KeyDelayRate(3, 31); break;
			case ID_RATE_32_DELAY_3: MENU_KeyDelayRate(3, 32); break;
			case ID_RATE_1_DELAY_4: MENU_KeyDelayRate(4, 1); break;
			case ID_RATE_2_DELAY_4: MENU_KeyDelayRate(4, 2); break;
			case ID_RATE_3_DELAY_4: MENU_KeyDelayRate(4, 3); break;
			case ID_RATE_4_DELAY_4: MENU_KeyDelayRate(4, 4); break;
			case ID_RATE_5_DELAY_4: MENU_KeyDelayRate(4, 5); break;
			case ID_RATE_6_DELAY_4: MENU_KeyDelayRate(4, 6); break;
			case ID_RATE_7_DELAY_4: MENU_KeyDelayRate(4, 7); break;
			case ID_RATE_8_DELAY_4: MENU_KeyDelayRate(4, 8); break;
			case ID_RATE_9_DELAY_4: MENU_KeyDelayRate(4, 9); break;
			case ID_RATE_10_DELAY_4: MENU_KeyDelayRate(4, 10); break;
			case ID_RATE_11_DELAY_4: MENU_KeyDelayRate(4, 11); break;
			case ID_RATE_12_DELAY_4: MENU_KeyDelayRate(4, 12); break;
			case ID_RATE_13_DELAY_4: MENU_KeyDelayRate(4, 13); break;
			case ID_RATE_14_DELAY_4: MENU_KeyDelayRate(4, 14); break;
			case ID_RATE_15_DELAY_4: MENU_KeyDelayRate(4, 15); break;
			case ID_RATE_16_DELAY_4: MENU_KeyDelayRate(4, 16); break;
			case ID_RATE_17_DELAY_4: MENU_KeyDelayRate(4, 17); break;
			case ID_RATE_18_DELAY_4: MENU_KeyDelayRate(4, 18); break;
			case ID_RATE_19_DELAY_4: MENU_KeyDelayRate(4, 19); break;
			case ID_RATE_20_DELAY_4: MENU_KeyDelayRate(4, 20); break;
			case ID_RATE_21_DELAY_4: MENU_KeyDelayRate(4, 21); break;
			case ID_RATE_22_DELAY_4: MENU_KeyDelayRate(4, 22); break;
			case ID_RATE_23_DELAY_4: MENU_KeyDelayRate(4, 23); break;
			case ID_RATE_24_DELAY_4: MENU_KeyDelayRate(4, 24); break;
			case ID_RATE_25_DELAY_4: MENU_KeyDelayRate(4, 25); break;
			case ID_RATE_26_DELAY_4: MENU_KeyDelayRate(4, 26); break;
			case ID_RATE_27_DELAY_4: MENU_KeyDelayRate(4, 27); break;
			case ID_RATE_28_DELAY_4: MENU_KeyDelayRate(4, 28); break;
			case ID_RATE_29_DELAY_4: MENU_KeyDelayRate(4, 29); break;
			case ID_RATE_30_DELAY_4: MENU_KeyDelayRate(4, 30); break;
			case ID_RATE_31_DELAY_4: MENU_KeyDelayRate(4, 31); break;
			case ID_RATE_32_DELAY_4: MENU_KeyDelayRate(4, 32); break;
			case ID_MOUSE_SENSITIVITY: GUI_Shortcut(2); break;
			case ID_GLIDE_LFB_FULL: SetVal("glide", "lfb", "full"); break;
			case ID_GLIDE_LFB_FULL_NOAUX: SetVal("glide", "lfb", "full_noaux"); break;
			case ID_GLIDE_LFB_READ: SetVal("glide", "lfb", "read"); break;
			case ID_GLIDE_LFB_READ_NOAUX: SetVal("glide", "lfb", "read_noaux"); break;
			case ID_GLIDE_LFB_WRITE: SetVal("glide", "lfb", "write"); break;
			case ID_GLIDE_LFB_WRITE_NOAUX: SetVal("glide", "lfb", "write_noaux"); break;
			case ID_GLIDE_LFB_NONE: SetVal("glide", "lfb", "none"); break;
			case ID_GLIDE_SPLASH:
			{
			Section_prop * sec = static_cast<Section_prop *>(control->GetSection("glide"));
			if(sec) SetVal("glide", "splash", sec->Get_bool("splash") ? "false" : "true");
			}
			break;
			case ID_GLIDE_EMU_FALSE: SetVal("pci", "voodoo", "false"); break;
			case ID_GLIDE_EMU_SOFTWARE: SetVal("pci", "voodoo", "software"); break;
			case ID_GLIDE_EMU_OPENGL: SetVal("pci", "voodoo", "opengl"); break;
			case ID_GLIDE_EMU_AUTO: SetVal("pci", "voodoo", "auto"); break;
			case ID_ALWAYS_ON_TOP:
			{
			SetFocus(GetHWND());
			DWORD dwExStyle = ::GetWindowLong(GetHWND(), GWL_EXSTYLE);
			HWND top = ((dwExStyle & WS_EX_TOPMOST) == 0)?HWND_TOPMOST:HWND_NOTOPMOST;
			SetWindowPos(GetHWND(), top,  0, 0, 0, 0, SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE);
			break;
			}

			default:
				break;
			}
		default: {
			if (Message.message == 0x00A1) Reflect_Menu();

			if (!TranslateAccelerator(GetHWND(), 0, &Message)) {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
			}
		}
	}
}
#endif
