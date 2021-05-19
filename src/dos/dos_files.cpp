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


#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#if defined(WIN32) && defined(__MINGW32__)
# include <malloc.h>
#endif

#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "drives.h"
#include "cross.h"
#include "control.h"
#include "dos_network2.h"
#include "menu.h"
#include "cdrom.h"
#include "ide.h"
#include "bios_disk.h"

#define DOS_FILESTART 4

#define FCB_SUCCESS     0
#define FCB_READ_NODATA	1
#define FCB_READ_PARTIAL 3
#define FCB_ERR_NODATA  1
#define FCB_ERR_EOF     3
#define FCB_ERR_WRITE   1

extern bool log_int21;
extern bool log_fileio;
extern bool enable_share_exe, enable_dbcs_tables;
extern int dos_clipboard_device_access;
extern char *dos_clipboard_device_name;

Bitu DOS_FILES = 127;
DOS_File ** Files = NULL;
DOS_Drive * Drives[DOS_DRIVES] = {NULL};
bool force_sfn = false;
int sdrive = 0;

/* This is the LFN filefind handle that is currently being used, with normal values between
 * 0 and 254 for LFN calls. The value LFN_FILEFIND_INTERNAL and LFN_FILEFIND_IMG are used
 * internally by LFN and image handling functions. For non-LFN calls the value is fixed to
 * be LFN_FILEFIND_NONE as defined in drives.h. */
int lfn_filefind_handle = LFN_FILEFIND_NONE;
extern uint8_t lead[6];
bool shiftjis_lead_byte(int c), isDBCSCP(), isDBCSLB(uint8_t chr, uint8_t* lead);

uint8_t DOS_GetDefaultDrive(void) {
//	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	uint8_t d = DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	if( d != dos.current_drive ) LOG(LOG_DOSMISC,LOG_ERROR)("SDA drive %d not the same as dos.current_drive %d",d,dos.current_drive);
	return dos.current_drive;
}

void DOS_SetDefaultDrive(uint8_t drive) {
//	if (drive<=DOS_DRIVES && ((drive<2) || Drives[drive])) DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);
	if (drive<DOS_DRIVES && ((drive<2) || Drives[drive])) {dos.current_drive = drive; DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);}
}

bool DOS_MakeName(char const * const name,char * const fullname,uint8_t * drive) {
	if(!name || *name == 0 || *name == ' ') {
		/* Both \0 and space are seperators and
		 * empty filenames report file not found */
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
    for (int i=0; i<6; i++) lead[i] = 0;
    if (isDBCSCP())
        for (int i=0; i<6; i++) {
            lead[i] = mem_readb(Real2Phys(dos.tables.dbcs)+i);
            if (lead[i] == 0) break;
        }
	char names[LFN_NAMELENGTH];
	strcpy(names,name);
	char * name_int = names;
	if (strlen(names)==14 && name_int[1]==':' && name_int[2]!='\\' && name_int[9]==' ' && name_int[10]=='.') {
		for (unsigned int i=0;i<strlen(names);i++)
			if (i<10 && name_int[i]==32) {
				name_int[i]='.';
				name_int[i+1]=name_int[11]==32?0:toupper(name_int[11]);
				name_int[i+2]=name_int[12]==32?0:toupper(name_int[12]);
				name_int[i+3]=name_int[13]==32?0:toupper(name_int[13]);
				name_int[i+4]=0;
				break;
			} else if (i<10) name_int[i]=toupper(name_int[i]);
	}
	char tempdir[DOS_PATHLENGTH];
	char upname[DOS_PATHLENGTH];
    Bitu r,w, q=0;
	/* First get the drive */
	*drive = DOS_GetDefaultDrive();
	while (name_int[0]=='"') {
		q++;
		name_int++;
	}
	if (name_int[1]==':') {
		*drive=(name_int[0] | 0x20)-'a';
		name_int+=2;
	}
	if (*drive>=DOS_DRIVES || !Drives[*drive]) { 
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false; 
	}
	r=0;w=0;
	while (r<DOS_PATHLENGTH && name_int[r]!=0) {
		uint8_t c=(uint8_t)name_int[r++];
		if (c=='/') c='\\';
		else if (c=='"') {q++;continue;}
		else if (uselfn&&!force_sfn) {
			if (c==' ' && q/2*2 == q) continue;
		} else {
			if ((c>='a') && (c<='z')) c-=32;
			else if (c==' ') continue; /* should be separator */
		}
		upname[w++]=(char)c;
        if (((IS_PC98_ARCH && shiftjis_lead_byte(c)) || (isDBCSCP() && isDBCSLB(c, lead))) && r<DOS_PATHLENGTH) {
            /* The trailing byte is NOT ASCII and SHOULD NOT be converted to uppercase like ASCII */
            upname[w++]=name_int[r++];
        }
	}
	while (r>0 && name_int[r-1]==' ') r--;
	if (r>=DOS_PATHLENGTH) { DOS_SetError(DOSERR_PATH_NOT_FOUND);return false; }
	upname[w]=0;

	/* Now parse the new file name to make the final filename */
	if (upname[0]!='\\') strcpy(fullname,Drives[*drive]->curdir);
	else fullname[0]=0;
	uint32_t lastdir=0;uint32_t t=0;
	while (fullname[t]!=0) {
		if ((fullname[t]=='\\') && (fullname[t+1]!=0)) lastdir=t;
		t++;
	}
	r=0;w=0;
	tempdir[0]=0;
	bool stop=false;
	while (!stop) {
		if (upname[r]==0) stop=true;
		if ((upname[r]=='\\') || (upname[r]==0)){
			tempdir[w]=0;
			if (tempdir[0]==0) { w=0;r++;continue;}
			if (strcmp(tempdir,".")==0) {
				tempdir[0]=0;			
				w=0;r++;
				continue;
			}

			int32_t iDown;
			bool dots = true;
			int32_t templen=(int32_t)strlen(tempdir);
			for(iDown=0;(iDown < templen) && dots;iDown++)
				if(tempdir[iDown] != '.')
					dots = false;

			// only dots?
			if (dots && (templen > 1)) {
				int32_t cDots = templen - 1;
				for(iDown=(int32_t)strlen(fullname)-1;iDown>=0;iDown--) {
					if(fullname[iDown]=='\\' || iDown==0) {
						lastdir = (uint32_t)iDown;
						cDots--;
						if(cDots==0)
							break;
					}
				}
				fullname[lastdir]=0;
				t=0;lastdir=0;
				while (fullname[t]!=0) {
					if ((fullname[t]=='\\') && (fullname[t+1]!=0)) lastdir=t;
					t++;
				}
				tempdir[0]=0;
				w=0;r++;
				continue;
			}
			

			lastdir=(uint32_t)strlen(fullname);

			if (lastdir!=0) strcat(fullname,"\\");
			if (!uselfn||force_sfn) {
				char * ext=strchr(tempdir,'.');
				if (ext) {
					if(strchr(ext+1,'.')) { 
					//another dot in the extension =>file not found
					//Or path not found depending on wether 
					//we are still in dir check stage or file stage
						if(stop)
							DOS_SetError(DOSERR_FILE_NOT_FOUND);
						else
							DOS_SetError(DOSERR_PATH_NOT_FOUND);
						return false;
					}
					
					ext[4] = 0;
					if((strlen(tempdir) - strlen(ext)) > 8) memmove(tempdir + 8, ext, 5);
                } else {
                    tempdir[8] = 0;
                }
			}

			if (strlen(fullname)+strlen(tempdir)>=DOS_PATHLENGTH) {
				DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			}
		   
			strcat(fullname,tempdir);
			tempdir[0]=0;
			w=0;r++;
			continue;
		}
		tempdir[w++]=upname[r++];
	}
	return true;	
}

bool checkwat=false;
bool DOS_GetSFNPath(char const * const path,char * SFNPath,bool LFN) {
    char pdir[LFN_NAMELENGTH+4], *p;
    uint8_t drive;char fulldir[DOS_PATHLENGTH],LFNPath[CROSS_LEN];
    char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH];
    uint32_t size;uint16_t date;uint16_t time;uint8_t attr;
    if (!DOS_MakeName(path,fulldir,&drive)) return false;
    sprintf(SFNPath,"%c:\\",drive+'A');
    strcpy(LFNPath,SFNPath);
    p = fulldir;
    if (*p==0) return true;
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	int fbak=lfn_filefind_handle;
    for (char *s = strchr(p,'\\'); s != NULL; s = strchr(p,'\\')) {
		*s = 0;
		if (SFNPath[strlen(SFNPath)-1]=='\\')
			sprintf(pdir,"\"%s%s\"",SFNPath,p);
		else
			sprintf(pdir,"\"%s\\%s\"",SFNPath,p);
		if (!strrchr(p,'*') && !strrchr(p,'?')) {
			*s = '\\';
			p = s + 1;
			lfn_filefind_handle=LFN_FILEFIND_INTERNAL;
			if (DOS_FindFirst(pdir,0xffff & DOS_ATTR_DIRECTORY & ~DOS_ATTR_VOLUME,false)) {
				lfn_filefind_handle=fbak;
				dta.GetResult(name,lname,size,date,time,attr);
				strcat(SFNPath,name);
				strcat(LFNPath,lname);
				strcat(SFNPath,"\\");
				strcat(LFNPath,"\\");
			}
			else {
				lfn_filefind_handle=fbak;
				dos.dta(save_dta);
				return false;
			}
		} else {
			strcat(SFNPath,p);
			strcat(LFNPath,p);
			strcat(SFNPath,"\\");
			strcat(LFNPath,"\\");
			*s = '\\';
			p = s + 1;
			break;
		}
    }
    if (p != 0) {
		sprintf(pdir,"\"%s%s\"",SFNPath,p);
		lfn_filefind_handle=LFN_FILEFIND_INTERNAL;
		if (!strrchr(p,'*')&&!strrchr(p,'?')&&DOS_FindFirst(pdir,0xffff & ~DOS_ATTR_VOLUME,false)) {
			dta.GetResult(name,lname,size,date,time,attr);
			strcat(SFNPath,name);
			strcat(LFNPath,lname);
        } else if (checkwat) {
            lfn_filefind_handle=fbak;
            dos.dta(save_dta);
            return false;
		} else {
			strcat(SFNPath,p);
			strcat(LFNPath,p);
		}
		lfn_filefind_handle=fbak;
    }
	dos.dta(save_dta);
    if (LFN) strcpy(SFNPath,LFNPath);
    return true;
}

bool DOS_GetCurrentDir(uint8_t drive,char * const buffer, bool LFN) {
    if (drive==0) drive=DOS_GetDefaultDrive();
    else drive--;

    if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
        DOS_SetError(DOSERR_INVALID_DRIVE);
        return false;
    }

    if (LFN && uselfn) {
        char cdir[DOS_PATHLENGTH+8],ldir[DOS_PATHLENGTH];

        if (strchr(Drives[drive]->curdir,' '))
            sprintf(cdir,"\"%c:\\%s\"",drive+'A',Drives[drive]->curdir);
        else
            sprintf(cdir,"%c:\\%s",drive+'A',Drives[drive]->curdir);

        if (!DOS_GetSFNPath(cdir,ldir,true))
            return false;

        strcpy(buffer,ldir+3);
        if (DOS_GetSFNPath(cdir,ldir,false))
            strcpy(Drives[drive]->curdir,ldir+3);
    } else {
        strcpy(buffer,Drives[drive]->curdir);
    }

    return true;
}

bool DOS_ChangeDir(char const * const dir) {
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	const char * testdir=dir;
	if (strlen(testdir) && testdir[1]==':') testdir+=2;
	size_t len=strlen(testdir);
	if (!len) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	if (strlen(fulldir) && testdir[len-1]=='\\') {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	
	if (Drives[drive]->TestDir(fulldir)) {
		strcpy(Drives[drive]->curdir,fulldir);
		return true;
	} else {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	}
	return false;
}

bool DOS_MakeDir(char const * const dir) {
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	size_t len = strlen(dir);
	if(!len || dir[len-1] == '\\') {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	while (strlen(fulldir)&&(*(fulldir+strlen(fulldir)-1)=='.'||*(fulldir+strlen(fulldir)-1)==' ')) *(fulldir+strlen(fulldir)-1)=0;
	if(Drives[drive]->MakeDir(fulldir)) return true;

	/* Determine reason for failing */
	if(Drives[drive]->TestDir(fulldir)) 
		DOS_SetError(DOSERR_ACCESS_DENIED);
	else
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	return false;
}

bool DOS_RemoveDir(char const * const dir) {
/* We need to do the test before the removal as can not rely on
 * the host to forbid removal of the current directory.
 * We never change directory. Everything happens in the drives.
 */
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	/* Check if exists */
	if(!Drives[drive]->TestDir(fulldir)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	/* See if it's current directory */
    char currdir[DOS_PATHLENGTH]= { 0 }, lcurrdir[DOS_PATHLENGTH]= { 0 };
    DOS_GetCurrentDir(drive + 1 ,currdir, false);
    DOS_GetCurrentDir(drive + 1 ,lcurrdir, true);
    if(strcasecmp(currdir,fulldir) == 0 || (uselfn && strcasecmp(lcurrdir,fulldir) == 0)) {
		DOS_SetError(DOSERR_REMOVE_CURRENT_DIRECTORY);
		return false;
	}

	if(Drives[drive]->RemoveDir(fulldir)) return true;

	/* Failed. We know it exists and it's not the current dir */
	/* Assume non empty */
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

static bool PathExists(char const * const name) {
	const char* leading = strrchr(name,'\\');
	if(!leading) return true;
	char temp[CROSS_LEN];
	strcpy(temp,name);
	char * lead = strrchr(temp,'\\');
	if (lead == temp) return true;
	*lead = 0;
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(temp,fulldir,&drive)) return false;
	if(!Drives[drive]->TestDir(fulldir)) return false;
	return true;
}

bool DOS_Rename(char const * const oldname,char const * const newname) {
	uint8_t driveold;char fullold[DOS_PATHLENGTH];
	uint8_t drivenew;char fullnew[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullold,&driveold)) return false;
	if (!DOS_MakeName(newname,fullnew,&drivenew)) return false;
	while (strlen(fullnew)&&(*(fullnew+strlen(fullnew)-1)=='.'||*(fullnew+strlen(fullnew)-1)==' ')) *(fullnew+strlen(fullnew)-1)=0;

	/* No tricks with devices */
	bool clip=false;
	if ( (DOS_FindDevice(oldname) != DOS_DEVICES) ||
	     (DOS_FindDevice(newname) != DOS_DEVICES) ) {
        if (!control->SecureMode()&&(dos_clipboard_device_access==3||dos_clipboard_device_access==4)) {
            if (DOS_FindDevice(oldname) == DOS_DEVICES) {
                const char* find_last;
                find_last=strrchr(fullnew,'\\');
                if (find_last==NULL) find_last=fullnew;
                else find_last++;
                if (!strcasecmp(find_last, *dos_clipboard_device_name?dos_clipboard_device_name:"CLIP$"))
                    clip=true;
            }
        }
		if (!clip) {
			DOS_SetError(DOSERR_FILE_NOT_FOUND);
			return false;
		}
	}
	/* Must be on the same drive */
	if(driveold != drivenew) {
		DOS_SetError(DOSERR_NOT_SAME_DEVICE);
		return false;
	}
	uint16_t attr;
	/* Source must exist */
	if (!Drives[driveold]->GetFileAttr( fullold, &attr ) ) {
		if (!PathExists(oldname)) DOS_SetError(DOSERR_PATH_NOT_FOUND);
		else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	/*Test if target exists => no access */
	if(Drives[drivenew]->GetFileAttr(fullnew,&attr)&&!(uselfn&&!force_sfn&&strcmp(fullold, fullnew)&&!strcasecmp(fullold, fullnew))) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

#if defined (WIN32)
	if (clip) {
		uint16_t sourceHandle, targetHandle, toread = 0x8000;
		static uint8_t buffer[0x8000];
		bool failed = false;
		if (DOS_OpenFile(oldname,OPEN_READ,&sourceHandle) && DOS_OpenFile(newname,OPEN_WRITE,&targetHandle)) {
			do {
				if (!DOS_ReadFile(sourceHandle,buffer,&toread) || !DOS_WriteFile(targetHandle,buffer,&toread)) failed=true;
			} while (toread == 0x8000);
			if (!DOS_CloseFile(sourceHandle)||!DOS_CloseFile(targetHandle)) failed=true;
		} else failed=true;
		if (!failed&&Drives[drivenew]->FileUnlink(fullold)) return true;
	} else
#endif

	if (Drives[drivenew]->Rename(fullold,fullnew)) return true;
	/* Rename failed despite checks => no access */
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool DOS_FindFirst(const char * search,uint16_t attr,bool fcb_findfirst) {
	LOG(LOG_FILES,LOG_NORMAL)("file search attributes %X name %s",attr,search);
	DOS_DTA dta(dos.dta());
	uint8_t drive;char fullsearch[DOS_PATHLENGTH];
	char dir[DOS_PATHLENGTH];char pattern[DOS_PATHLENGTH];
	size_t len = strlen(search);
	if(len && search[len - 1] == '\\' && !( (len > 2) && (search[len - 2] == ':') && (attr == DOS_ATTR_VOLUME) )) { 
		//Dark Forces installer, but c:\ is allright for volume labels(exclusively set)
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if (!DOS_MakeName(search,fullsearch,&drive)) return false;
	//Check for devices. FindDevice checks for leading subdir as well
	bool device = (DOS_FindDevice(search) != DOS_DEVICES);

	/* Split the search in dir and pattern */
	char * find_last;
	find_last=strrchr(fullsearch,'\\');
	if (!find_last) {	/*No dir */
		strcpy(pattern,fullsearch);
		dir[0]=0;
	} else {
		*find_last=0;
		strcpy(pattern,find_last+1);
		strcpy(dir,fullsearch);
	}

	// Silence CHKDSK "Invalid sub-directory entry"
	if (fcb_findfirst && !strcmp(search+1, ":????????.???") && attr==30) {
		char psp_name[9];
		DOS_MCB psp_mcb(dos.psp()-1);
		psp_mcb.GetFileName(psp_name);
		if (!strcmp(psp_name, "CHKDSK")) attr&=~DOS_ATTR_DIRECTORY;
	}

	sdrive=drive;
	dta.SetupSearch(drive,(uint8_t)attr,pattern);

	if(device) {
		find_last = strrchr(pattern,'.');
		if(find_last) *find_last = 0;
		//TODO use current date and time
        dta.SetResult(pattern,pattern,0,0,0,DOS_ATTR_DEVICE);
		LOG(LOG_DOSMISC,LOG_WARN)("finding device %s",pattern);
		return true;
	}
   
	if (Drives[drive]->FindFirst(dir,dta,fcb_findfirst)) return true;
	
	return false;
}

bool DOS_FindNext(void) {
	DOS_DTA dta(dos.dta());
	uint8_t i = dta.GetSearchDrive();
	if(uselfn && (i >= DOS_DRIVES || !Drives[i])) i=sdrive;
	if(i >= DOS_DRIVES || !Drives[i]) {
		/* Corrupt search. */
		LOG(LOG_FILES,LOG_ERROR)("Corrupt search!!!!");
		DOS_SetError(DOSERR_NO_MORE_FILES); 
		return false;
	} 
	if (Drives[i]->FindNext(dta)) return true;
	return false;
}


bool DOS_ReadFile(uint16_t entry,uint8_t * data,uint16_t * amount,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_ReadFile(entry,data,amount);
#endif
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}

    if (log_fileio) {
        LOG(LOG_FILES, LOG_DEBUG)("Reading %d bytes from %s ", *amount, Files[handle]->name);
    }
/*
	if ((Files[handle]->flags & 0x0f) == OPEN_WRITE)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	uint16_t toread=*amount;
	bool ret=Files[handle]->Read(data,&toread);
	*amount=toread;
	return ret;
}

bool DOS_WriteFile(uint16_t entry,const uint8_t * data,uint16_t * amount,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_WriteFile(entry,data,amount);
#endif
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}

    if (log_fileio) {
        LOG(LOG_FILES, LOG_DEBUG)("Writing %d bytes to %s", *amount, Files[handle]->name);
    }
/*
	if ((Files[handle]->flags & 0x0f) == OPEN_READ)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	uint16_t towrite=*amount;
	bool ret=Files[handle]->Write(data,&towrite);
	*amount=towrite;
	return ret;
}

bool DOS_SeekFile(uint16_t entry,uint32_t * pos,uint32_t type,bool fcb) {
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	return Files[handle]->Seek(pos,type);
}

/* ert, 20100711: Locking extensions */
bool DOS_LockFile(uint16_t entry,uint8_t mode,uint32_t pos,uint32_t size) {
	uint32_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	return Files[handle]->LockFile(mode,pos,size);
}

bool DOS_CloseFile(uint16_t entry, bool fcb, uint8_t * refcnt) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_CloseFile(entry);
#endif
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
    if (Files[handle]->IsOpen()) {
        if (log_fileio) {
            LOG(LOG_FILES, LOG_NORMAL)("Closing file %s", Files[handle]->name);
        }
        Files[handle]->Close();
	}

	DOS_PSP psp(dos.psp());
	if (!fcb) psp.SetFileHandle(entry,0xff);

	Bits refs=Files[handle]->RemoveRef();
	if (refs<=0) {
		delete Files[handle];
		Files[handle]=0;
	}
	if (refcnt!=NULL) *refcnt=static_cast<uint8_t>(refs+1);
	return true;
}

bool DOS_FlushFile(uint16_t entry) {
	uint32_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}

	LOG(LOG_DOSMISC,LOG_DEBUG)("FFlush used.");

    Files[handle]->Flush();
	return true;
}


bool DOS_CreateFile(char const * name,uint16_t attributes,uint16_t * entry,bool fcb) {
	// Creation of a device is the same as opening it
	// Tc201 installer
	if (DOS_FindDevice(name) != DOS_DEVICES)
		return DOS_OpenFile(name, OPEN_READ, entry, fcb);

	LOG(LOG_FILES,LOG_NORMAL)("file create attributes %X file %s",attributes,name);
	char fullname[DOS_PATHLENGTH];uint8_t drive;
	DOS_PSP psp(dos.psp());
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	while (strlen(fullname)&&(*(fullname+strlen(fullname)-1)=='.'||*(fullname+strlen(fullname)-1)==' ')) *(fullname+strlen(fullname)-1)=0;

	/* Check for a free file handle */
	uint8_t handle=(uint8_t)DOS_FILES;uint8_t i;
	for (i=0;i<DOS_FILES;i++) {
		if (!Files[i]) {
			handle=i;
			break;
		}
	}
	if (handle==DOS_FILES) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	/* We have a position in the main table now find one in the psp table */
	*entry = fcb?handle:psp.FindFreeFileEntry();
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	/* Don't allow directories to be created */
	if (attributes&DOS_ATTR_DIRECTORY) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool foundit=Drives[drive]->FileCreate(&Files[handle],fullname,attributes);
	if (foundit) { 
		if (Files[handle]) {
			Files[handle]->SetDrive(drive);
			Files[handle]->AddRef();
			Files[handle]->drive = drive;
		}
		if (!fcb) psp.SetFileHandle(*entry,handle);
		if (Files[handle]) Drives[drive]->EmptyCache();
		return true;
	} else {
		if(dos.errorcode==DOSERR_ACCESS_DENIED||dos.errorcode==DOSERR_WRITE_PROTECTED) return false;
		if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
		else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_OpenFile(char const * name,uint8_t flags,uint16_t * entry,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsNetworkResource(name))
		return Network_OpenFile(name,flags,entry);
#endif
	/* First check for devices */
	if (flags>2) LOG(LOG_FILES,LOG_NORMAL)("Special file open command %X file %s",flags,name); // FIXME: Why? Is there something about special opens DOSBox doesn't handle properly?
	else LOG(LOG_FILES,LOG_NORMAL)("file open command %X file %s",flags,name);

	DOS_PSP psp(dos.psp());
	uint16_t attr = 0;
	uint8_t devnum = DOS_FindDevice(name);
	bool device = (devnum != DOS_DEVICES);
	if(!device && DOS_GetFileAttr(name,&attr)) {
	//DON'T ALLOW directories to be openened.(skip test if file is device).
		if((attr & DOS_ATTR_DIRECTORY) || (attr & DOS_ATTR_VOLUME)){
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	char fullname[DOS_PATHLENGTH];uint8_t drive;uint8_t i;
	/* First check if the name is correct */
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	uint8_t handle=255;		
	/* Check for a free file handle */
	for (i=0;i<DOS_FILES;i++) {
		if (!Files[i]) {
			handle=i;
			break;
		}
	}
	if (handle==255) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	/* We have a position in the main table now find one in the psp table */
	*entry = fcb?handle:psp.FindFreeFileEntry();

	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool exists=false;
	if (device) {
		Files[handle]=new DOS_Device(*Devices[devnum]);
	} else {
        exists=Drives[drive]->FileOpen(&Files[handle],fullname,flags) || Drives[drive]->FileOpen(&Files[handle],upcase(fullname),flags);
		if (exists) Files[handle]->SetDrive(drive);
	}
	if (exists || device ) { 
		Files[handle]->AddRef();
		psp.SetFileHandle(*entry,handle);
		Files[handle]->drive = drive;
		return true;
	} else {
		//Test if file exists, but opened in read-write mode (and writeprotected)
		if((((flags&3) != OPEN_READ) || (enable_share_exe && !strncmp(Drives[drive]->GetInfo(),"local directory ",16))) && Drives[drive]->FileExists(fullname))
			DOS_SetError(DOSERR_ACCESS_DENIED);
		else {
			if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
			else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		}
		return false;
	}
}

bool DOS_OpenFileExtended(char const * name, uint16_t flags, uint16_t createAttr, uint16_t action, uint16_t *entry, uint16_t* status) {
// FIXME: Not yet supported : Bit 13 of flags (int 0x24 on critical error)
	uint16_t result = 0;
	if (action==0) {
		// always fail setting
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	} else {
		if (((action & 0x0f)>2) || ((action & 0xf0)>0x10)) {
			// invalid action parameter
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
	}
	if (DOS_OpenFile(name, (uint8_t)(flags&0xff), entry)) {
		// File already exists
		switch (action & 0x0f) {
			case 0x00:		// failed
				DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
				return false;
			case 0x01:		// file open (already done)
				result = 1;
				break;
			case 0x02:		// replace
				DOS_CloseFile(*entry);
				if (!DOS_CreateFile(name, createAttr, entry)) return false;
				result = 3;
				break;
			default:
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				E_Exit("DOS: OpenFileExtended: Unknown action.");
				break;
		}
	} else {
		// File doesn't exist
		if ((action & 0xf0)==0) {
			// uses error code from failed open
			return false;
		}
		// Create File
		if (!DOS_CreateFile(name, createAttr, entry)) {
			// uses error code from failed create
			return false;
		}
		result = 2;
	}
	// success
	*status = result;
	return true;
}

bool DOS_UnlinkFile(char const * const name) {
	char fullname[DOS_PATHLENGTH];uint8_t drive;
    // An existing device returns an access denied error
    if (log_fileio) {
        LOG(LOG_FILES, LOG_NORMAL)("Deleting file %s", name);
    }
    if (DOS_FindDevice(name) != DOS_DEVICES) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	if(Drives[drive]->FileUnlink(fullname)){
		return true;
	} else if(uselfn&&!force_sfn&&(strchr(fullname, '*')||strchr(fullname, '?'))) { // Wildcard delete as used by MS-DOS 7+ "DEL *.*" in LFN mode
		char dir[DOS_PATHLENGTH], temp[DOS_PATHLENGTH], fname[DOS_PATHLENGTH], spath[DOS_PATHLENGTH], pattern[DOS_PATHLENGTH];
		if (!DOS_Canonicalize(name,fullname)||!strlen(fullname)) {
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
		if (!strchr(name,'\"')||!DOS_GetSFNPath(("\""+std::string(fullname)+"\"").c_str(), fname, false))
			strcpy(fname, fullname);
		char * find_last=strrchr(fname,'\\');
		if (!find_last) {
			dir[0]=0;
			strcpy(pattern, fname);
		} else {
			*find_last=0;
			strcpy(dir,fname);
			strcpy(pattern, find_last+1);
		}
		int k=0;
		for (int i=0;i<(int)strlen(pattern);i++)
			if (pattern[i]!='\"')
				pattern[k++]=pattern[i];
		pattern[k]=0;
		RealPt save_dta=dos.dta();
		dos.dta(dos.tables.tempdta);
		DOS_DTA dta(dos.dta());
		std::vector<std::string> cdirs;
		cdirs.clear();
		strcpy(spath, dir);
		if (!DOS_GetSFNPath(dir, spath, false)) strcpy(spath, dir);
		if (!strlen(spath)||spath[strlen(spath)-1]!='\\') strcat(spath, "\\");
		std::string pfull=std::string(spath)+std::string(pattern);
		int fbak=lfn_filefind_handle;
		lfn_filefind_handle=LFN_FILEFIND_INTERNAL;
		bool ret=DOS_FindFirst(((pfull.length()&&pfull[0]=='"'?"":"\"")+pfull+(pfull.length()&&pfull[pfull.length()-1]=='"'?"":"\"")).c_str(),0xffu & ~DOS_ATTR_VOLUME & ~DOS_ATTR_DIRECTORY);
		if (ret) do {
			char find_name[DOS_NAMELENGTH_ASCII],lfind_name[LFN_NAMELENGTH];
			uint16_t find_date,find_time;uint32_t find_size;uint8_t find_attr;
			dta.GetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
			if (!(find_attr & DOS_ATTR_DIRECTORY)&&strlen(find_name)&&!strchr(find_name, '*')&&!strchr(find_name, '?')) {
				strcpy(temp, dir);
				if (strlen(temp)&&temp[strlen(temp)-1]!='\\') strcat(temp, "\\");
				strcat(temp, find_name);
				cdirs.push_back(std::string(temp));
			}
		} while ((ret=DOS_FindNext())==true);
		lfn_filefind_handle=fbak;
		bool removed=false;
		while (!cdirs.empty()) {
			if (DOS_UnlinkFile(cdirs.begin()->c_str()))
				removed=true;
			cdirs.erase(cdirs.begin());
		}
		dos.dta(save_dta);
		if (removed)
			return true;
		else {
			if (dos.errorcode!=DOSERR_ACCESS_DENIED&&dos.errorcode!=DOSERR_WRITE_PROTECTED) DOS_SetError(DOSERR_FILE_NOT_FOUND);
			return false;
		}
	} else {
		if (dos.errorcode!=DOSERR_ACCESS_DENIED&&dos.errorcode!=DOSERR_WRITE_PROTECTED) DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_GetFileAttr(char const * const name,uint16_t * attr) {
	char fullname[DOS_PATHLENGTH];uint8_t drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
#if defined (WIN32)
	if (!control->SecureMode()&&dos_clipboard_device_access) {
        const char* find_last;
		find_last=strrchr(fullname,'\\');
		if (find_last==NULL) find_last=fullname;
		else find_last++;
		if (!strcasecmp(find_last, *dos_clipboard_device_name?dos_clipboard_device_name:"CLIP$"))
			return true;
	}
#endif
	if (Drives[drive]->GetFileAttr(fullname,attr)) {
		return true;
	} else {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_GetFileAttrEx(char const* const name, struct stat *status, uint8_t hdrive)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	bool usehdrive=/*hdrive>=0&&(always true)*/hdrive<DOS_FILES;
	if (usehdrive)
		strcpy(fullname,name);
	else if (!DOS_MakeName(name, fullname, &drive))
		return false;
	return Drives[usehdrive?hdrive:drive]->GetFileAttrEx(fullname, status);
}

unsigned long DOS_GetCompressedFileSize(char const* const name)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	return Drives[drive]->GetCompressedSize(fullname);
}

#if defined (WIN32)
HANDLE DOS_CreateOpenFile(char const* const name)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return INVALID_HANDLE_VALUE;
	return Drives[drive]->CreateOpenFile(fullname);
}
#endif

bool DOS_SetFileAttr(char const * const name,uint16_t attr) 
// returns false when using on cdrom (stonekeep)
{
	char fullname[DOS_PATHLENGTH];uint8_t drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;	
	if (strncmp(Drives[drive]->GetInfo(),"CDRom ",6)==0 || strncmp(Drives[drive]->GetInfo(),"isoDrive ",9)==0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* This function must prevent changing a file into a directory, volume label into a file, etc.
	 * Also Windows 95 setup likes to create WINBOOT.INI as a file and then chattr it into a directory for some stupid reason. */
	uint16_t old_attr;
	if (!Drives[drive]->GetFileAttr(fullname,&old_attr)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	if ((old_attr ^ attr) & DOS_ATTR_VOLUME) { /* change in volume label attribute */
		LOG(LOG_DOSMISC,LOG_WARN)("Attempted to change volume label attribute of '%s' with SetFileAttr",name);
		return false;
	}

	if ((old_attr ^ attr) & DOS_ATTR_DIRECTORY) /* change in directory attribute (ex: Windows 95 SETUP.EXE vs WINBOOT.INI) */
		LOG(LOG_DOSMISC,LOG_WARN)("Attempted to change directory attribute of '%s' with SetFileAttr",name);

	/* define what cannot be changed */
	const uint16_t attr_mask = (DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY);

	attr = (attr & ~attr_mask) | (old_attr & attr_mask);

	return Drives[drive]->SetFileAttr(fullname,attr);
}

bool DOS_Canonicalize(char const * const name,char * const big) {
//TODO Add Better support for devices and shit but will it be needed i doubt it :) 
	uint8_t drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	big[0]=drive+'A';
	big[1]=':';
	big[2]='\\';
	strcpy(&big[3],fullname);
	return true;
}

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

/* Common routine to take larger allocation information (such as FAT32) and convert it to values
 * that are suitable for use with older DOS programs that pre-date FAT32 and partitions 2GB or larger. 
 * This is what Windows 95 OSR2 and higher do with FAT32 partitions anyway, as documented by Microsoft. */
bool DOS_CommonFAT32FAT16DiskSpaceConv(
		uint16_t * bytes,uint8_t * sectors,uint16_t * clusters,uint16_t * free,
		const uint32_t bytes32,const uint32_t sectors32,const uint32_t clusters32,const uint32_t free32) {
	uint32_t cdiv = 1;

	if (sectors32 > 128 || bytes32 > 0x8000)
		return false;

	/* This function is for the old API. It is necessary to adjust the values so that they never overflow
	 * 16-bit unsigned integers and never multiply out to a number greater than just under 2GB. Because
	 * old DOS programs use 32-bit signed integers for disk total/free and FAT12/FAT16 filesystem limitations. */
	/* NTS: Make sure (bytes per sector * sectors per cluster) is less than 0x10000, or else FORMAT.COM will
	 * crash with divide by zero or produce incorrect results when run with "FORMAT /S" */
	while ((clusters32 > 0xFFFFu || free32 > 0xFFFFu) && (sectors32 * cdiv) <= 64u && (bytes32 * sectors32 * cdiv) < 0x8000/*Needed for FORMAT.COM*/)
		cdiv *= 2u;

	/* The old API must never report more than just under 2GB for total and free */
	const uint32_t clust2gb = (uint32_t)0x7FFF8000ul / (uint32_t)bytes32 / (sectors32 * cdiv);

	*bytes = bytes32;
	*sectors = sectors32 * cdiv;
	*clusters = (uint16_t)MIN(MIN(clusters32 / cdiv,clust2gb),0xFFFFu);
	*free = (uint16_t)MIN(MIN(free32 / cdiv,clust2gb),0xFFFFu);
	return true;
}

bool DOS_GetFreeDiskSpace(uint8_t drive,uint16_t * bytes,uint8_t * sectors,uint16_t * clusters,uint16_t * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}

	{
		uint32_t bytes32,sectors32,clusters32,free32;
		if ((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) &&
			Drives[drive]->AllocationInfo32(&bytes32,&sectors32,&clusters32,&free32) &&
			DOS_CommonFAT32FAT16DiskSpaceConv(bytes,sectors,clusters,free,bytes32,sectors32,clusters32,free32))
			return true;
	}

	if (Drives[drive]->AllocationInfo(bytes,sectors,clusters,free))
		return true;

	return false;
}

bool DOS_GetFreeDiskSpace32(uint8_t drive,uint32_t * bytes,uint32_t * sectors,uint32_t * clusters,uint32_t * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}

	if ((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) && Drives[drive]->AllocationInfo32(bytes,sectors,clusters,free))
		return true;

	{
		uint8_t sectors8;
		uint16_t bytes16,clusters16,free16;
		if (Drives[drive]->AllocationInfo(&bytes16,&sectors8,&clusters16,&free16)) {
			*free = free16;
			*bytes = bytes16;
			*sectors = sectors8;
			*clusters = clusters16;
			return true;
		}
	}

	return false;
}

bool DOS_DuplicateEntry(uint16_t entry,uint16_t * newentry) {
	// Dont duplicate console handles
/*	if (entry<=STDPRN) {
		*newentry = entry;
		return true;
	};
*/	
	uint8_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	DOS_PSP psp(dos.psp());
	*newentry = psp.FindFreeFileEntry();
	if (*newentry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	Files[handle]->AddRef();	
	psp.SetFileHandle(*newentry,handle);
	return true;
}

bool DOS_ForceDuplicateEntry(uint16_t entry,uint16_t newentry) {
	if(entry == newentry) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	uint8_t orig = RealHandle(entry);
	if (orig >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[orig] || !Files[orig]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	uint8_t newone = RealHandle(newentry);
	if (newone < DOS_FILES && Files[newone]) {
		DOS_CloseFile(newentry);
	}
	DOS_PSP psp(dos.psp());
	Files[orig]->AddRef();
	psp.SetFileHandle(newentry,orig);
	return true;
}

void initRand() {
#ifdef WIN32
    srand(GetTickCount());
#else
    struct timespec ts;
    unsigned theTick = 0U;
    clock_gettime( CLOCK_REALTIME, &ts );
    theTick  = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;
    srand(theTick);
#endif
}

bool DOS_CreateTempFile(char * const name,uint16_t * entry) {
	size_t namelen=strlen(name);
	char * tempname=name+namelen;
	if (namelen==0) {
		// temp file created in root directory
		tempname[0]='\\';
		tempname++;
	} else {
		if ((name[namelen-1]!='\\') && (name[namelen-1]!='/')) {
			tempname[0]='\\';
			tempname++;
		}
	}
	dos.errorcode=0;
	/* add random crap to the end of the name and try to open */
	initRand();
	bool cont;
	do {
		cont=false;
		uint32_t i;
		for (i=0;i<8;i++) {
			tempname[i]=(rand()%26)+'A';
		}
		tempname[8]=0;
		//if (DOS_FileExists(name)) {cont=true;continue;} // FIXME: Check name uniqueness
	} while (cont || DOS_FileExists(name));
	DOS_CreateFile(name,0,entry);
	if (dos.errorcode) return false;
	return true;
}

char DOS_ToUpper(char c) {
	unsigned char uc = *reinterpret_cast<unsigned char*>(&c);
	if (uc > 0x60 && uc < 0x7B) uc -= 0x20;
	else if (uc > 0x7F && uc < 0xA5) {
		const unsigned char t[0x25] = { 
			0x00, 0x9a, 0x45, 0x41, 0x8E, 0x41, 0x8F, 0x80, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x00, 0x00,
			0x00, 0x92, 0x00, 0x4F, 0x99, 0x4F, 0x55, 0x55, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x41, 0x49, 0x4F, 0x55, 0xA5};
			if (t[uc - 0x80]) uc = t[uc-0x80];
	}
	char sc = *reinterpret_cast<char*>(&uc);
	return sc;
}

#define FCB_SEP ":;,=+"
#define ILLEGAL ":.;,=+ \t/\"[]<>|"

static bool isvalid(const char in){
	const char ill[]=ILLEGAL;    
	return (uint8_t(in)>0x1F) && (!strchr(ill,in));
}

#define PARSE_SEP_STOP          0x01
#define PARSE_DFLT_DRIVE        0x02
#define PARSE_BLNK_FNAME        0x04
#define PARSE_BLNK_FEXT         0x08

#define PARSE_RET_NOWILD        0
#define PARSE_RET_WILD          1
#define PARSE_RET_BADDRIVE      0xff

uint8_t FCB_Parsename(uint16_t seg,uint16_t offset,uint8_t parser ,char *string, uint8_t *change) {
    for (int i=0; i<6; i++) lead[i] = 0;
    if (isDBCSCP())
        for (int i=0; i<6; i++) {
            lead[i] = mem_readb(Real2Phys(dos.tables.dbcs)+i);
            if (lead[i] == 0) break;
        }
    const char* string_begin = string;
	uint8_t ret=0;
	if (!(parser & PARSE_DFLT_DRIVE)) {
		// default drive forced, this intentionally invalidates an extended FCB
		mem_writeb(PhysMake(seg,offset),0);
	}
	DOS_FCB fcb(seg,offset,false);	// always a non-extended FCB
	bool hasdrive,hasname,hasext,finished;
	hasdrive=hasname=hasext=finished=false;
	Bitu index=0;
	uint8_t fill=' ';
/* First get the old data from the fcb */
#ifdef _MSC_VER
#pragma pack (1)
#endif
	union {
		struct {
			char drive[2];
			char name[9];
			char ext[4];
		} GCC_ATTRIBUTE (packed) part;
		char full[DOS_FCBNAME];
	} fcb_name;
#ifdef _MSC_VER
#pragma pack()
#endif
	/* Get the old information from the previous fcb */
	fcb.GetName(fcb_name.full);
	fcb_name.part.drive[0]-='A'-1;fcb_name.part.drive[1]=0;
	fcb_name.part.name[8]=0;fcb_name.part.ext[3]=0;
	/* strip leading spaces */
	while((*string==' ')||(*string=='\t')) string++;

	/* Strip of the leading separator */
	if((parser & PARSE_SEP_STOP) && *string) {
		char sep[] = FCB_SEP;char a[2];
		a[0] = *string;a[1] = '\0';
		if (strcspn(a,sep) == 0) string++;
	} 

	/* Skip following spaces as well */
	while((*string==' ')||(*string=='\t')) string++;

	/* Check for a drive */
	if (string[1]==':') {
		unsigned char d = *reinterpret_cast<unsigned char*>(&string[0]);
		if (!isvalid(ascii_toupper(d))) {string += 2; goto savefcb;} //TODO check (for ret value)
		fcb_name.part.drive[0]=0;
		hasdrive=true;
		if (isalpha(d) && Drives[ascii_toupper(d)-'A']) { //Floppies under dos always exist, but don't bother with that at this level
			; //THIS* was here
		} else ret=0xff;
		fcb_name.part.drive[0]=DOS_ToUpper(string[0])-'A'+1; //Always do THIS* and continue parsing, just return the right code
		string+=2;
	}

	/* Check for extension only file names */
	if (string[0] == '.') {string++;goto checkext;}

	/* do nothing if not a valid name */
	if(!isvalid(string[0])) goto savefcb;

	hasname=true;finished=false;fill=' ';index=0;
	/* Copy the name */	
	while (true) {
		unsigned char nc = *reinterpret_cast<unsigned char*>(&string[0]);
		if ((IS_PC98_ARCH && shiftjis_lead_byte(nc)) || (isDBCSCP() && isDBCSLB(nc, lead))) {
                /* Shift-JIS is NOT ASCII and SHOULD NOT be converted to uppercase like ASCII */
                fcb_name.part.name[index]=(char)nc;
                string++;
                index++;
                if (index >= 8) break;

                /* should be trailing byte of Shift-JIS */
                if (nc < 32 || nc >= 127) continue;

                fcb_name.part.name[index]=(char)nc;
            }
		else
		{
			char ncs = (char)ascii_toupper(nc); //Should use DOS_ToUpper, but then more calls need to be changed.
			if (ncs == '*') { //Handle *
				fill = '?';
				ncs = '?';
			}
			if (ncs == '?' && !ret && index < 8) ret = 1; //Don't override bad drive
			if (!isvalid(ncs)) { //Fill up the name.
				while(index < 8) 
					fcb_name.part.name[index++] = (char)fill;
				break;
			}
			if (index < 8) { 
				fcb_name.part.name[index++] = (char)((fill == '?')?fill:ncs);
			}
		}
		string++;
	}
	if (!(string[0]=='.')) goto savefcb;
	string++;
checkext:
	/* Copy the extension */
	hasext=true;finished=false;fill=' ';index=0;
	while (true) {
		unsigned char nc = *reinterpret_cast<unsigned char*>(&string[0]);
		if ((IS_PC98_ARCH && shiftjis_lead_byte(nc)) || (isDBCSCP() && isDBCSLB(nc, lead))) {
                /* Shift-JIS is NOT ASCII and SHOULD NOT be converted to uppercase like ASCII */
                fcb_name.part.ext[index]=(char)nc;
                string++;
                index++;
                if (index >= 3) break;

                /* should be trailing byte of Shift-JIS */
                if (nc < 32u || nc >= 127u) continue;

                fcb_name.part.ext[index]=(char)nc;
            }
		else
		{
			char ncs = (char)ascii_toupper(nc);
			if (ncs == '*') { //Handle *
				fill = '?';
				ncs = '?';
			}
			if (ncs == '?' && !ret && index < 3) ret = 1;
			if (!isvalid(ncs)) { //Fill up the name.
				while(index < 3) 
					fcb_name.part.ext[index++] = (char)fill;
				break;
			}
			if (index < 3) { 
				fcb_name.part.ext[index++] = (char)((fill=='?')?fill:ncs);
			}
		}
		string++;
	}
savefcb:
	if (!hasdrive && !(parser & PARSE_DFLT_DRIVE)) fcb_name.part.drive[0] = 0;
	if (!hasname && !(parser & PARSE_BLNK_FNAME)) strcpy(fcb_name.part.name,"        ");
	if (!hasext && !(parser & PARSE_BLNK_FEXT)) strcpy(fcb_name.part.ext,"   ");
	fcb.SetName((unsigned char)fcb_name.part.drive[0],fcb_name.part.name,fcb_name.part.ext);
	fcb.ClearBlockRecsize(); //Undocumented bonus work.
	*change=(uint8_t)(string-string_begin);
	return ret;
}

static void DTAExtendNameVolumeLabel(const char* const name, char* const filename, char* const ext) {
    size_t i,s;

    i=0;
    s=0;
    while (i < 8 && name[s] != 0) filename[i++] = name[s++];
    while (i < 8) filename[i++] = ' ';

    i=0;
    while (i < 3 && name[s] != 0) ext[i++] = name[s++];
    while (i < 3) ext[i++] = ' ';
}

static void DTAExtendName(char * const name,char * const filename,char * const ext) {
	char * find=strchr(name,'.');
	if (find && find!=name) {
		strcpy(ext,find+1);
		*find=0;
	} else ext[0]=0;
	strcpy(filename,name);
	size_t i;
	for (i=strlen(name);i<8;i++) filename[i]=' ';
	filename[8]=0;
	for (i=strlen(ext);i<3;i++) ext[i]=' ';
	ext[3]=0;
}

static void SaveFindResult(DOS_FCB & find_fcb) {
	DOS_DTA find_dta(dos.tables.tempdta);
    char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];
    uint32_t size;uint16_t date;uint16_t time;uint8_t attr;uint8_t drive;
	char file_name[9];char ext[4];
	find_dta.GetResult(name,lname,size,date,time,attr);
	drive=find_fcb.GetDrive()+1;
	uint8_t find_attr = DOS_ATTR_ARCHIVE;
	find_fcb.GetAttr(find_attr); /* Gets search attributes if extended */
	/* Create a correct file and extention */
    if (attr & DOS_ATTR_VOLUME)
        DTAExtendNameVolumeLabel(name,file_name,ext);
    else
        DTAExtendName(name,file_name,ext);	

    DOS_FCB fcb(RealSeg(dos.dta()),RealOff(dos.dta()));//TODO
	fcb.Create(find_fcb.Extended());
	fcb.SetName(drive,file_name,ext);
	fcb.SetAttr(find_attr);      /* Only adds attribute if fcb is extended */
	fcb.SetResult(size,date,time,attr);
}

bool DOS_FCBCreate(uint16_t seg,uint16_t offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];uint16_t handle;
	uint8_t attr = DOS_ATTR_ARCHIVE;
	fcb.GetAttr(attr);
	if (!attr) attr = DOS_ATTR_ARCHIVE; //Better safe than sorry 

    if (attr & DOS_ATTR_VOLUME) {
	    fcb.GetVolumeName(shortname);
        return Drives[fcb.GetDrive()]->FileCreate(NULL,shortname,attr);
    }

	fcb.GetName(shortname);
	if (!DOS_CreateFile(shortname,attr,&handle,true)) return false;
	fcb.FileOpen((uint8_t)handle);
	return true;
}

bool DOS_FCBOpen(uint16_t seg,uint16_t offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];uint16_t handle;
	fcb.GetName(shortname);

	/* Search for file if name has wildcards */
	if (strpbrk(shortname,"*?")) {
		LOG(LOG_FCB,LOG_WARN)("Wildcards in filename");
		if (!DOS_FCBFindFirst(seg,offset)) return false;
		DOS_DTA find_dta(dos.tables.tempdta);
		DOS_FCB find_fcb(RealSeg(dos.tables.tempdta),RealOff(dos.tables.tempdta));
		char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH],file_name[9],ext[4];
		uint32_t size;uint16_t date,time;uint8_t attr;
		find_dta.GetResult(name,lname,size,date,time,attr);
		DTAExtendName(name,file_name,ext);
		find_fcb.SetName(fcb.GetDrive()+1,file_name,ext);
		find_fcb.GetName(shortname);
	}

	/* First check if the name is correct */
	uint8_t drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(shortname,fullname,&drive)) return false;
	
	/* Check, if file is already opened */
	for (uint8_t i = 0;i < DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->IsName(fullname)) {
			Files[i]->AddRef();
			fcb.FileOpen(i);
			return true;
		}
	}
	
	if (!DOS_OpenFile(shortname,OPEN_READWRITE,&handle,true)) return false;
	fcb.FileOpen((uint8_t)handle);
	return true;
}

bool DOS_FCBClose(uint16_t seg,uint16_t offset) {
	DOS_FCB fcb(seg,offset);
	if(!fcb.Valid()) return false;
	uint8_t fhandle;
	fcb.FileClose(fhandle);
	DOS_CloseFile(fhandle,true);
	return true;
}

bool DOS_FCBFindFirst(uint16_t seg,uint16_t offset) {
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta);
	char name[DOS_FCBNAME];fcb.GetName(name);
	uint8_t attr = DOS_ATTR_ARCHIVE;
	fcb.GetAttr(attr); /* Gets search attributes if extended */
	bool ret=DOS_FindFirst(name,attr,true);
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

bool DOS_FCBFindNext(uint16_t seg,uint16_t offset) {
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta);
	bool ret=DOS_FindNext();
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

uint8_t DOS_FCBRead(uint16_t seg,uint16_t offset,uint16_t recno) {
	DOS_FCB fcb(seg,offset);
	uint8_t fhandle,cur_rec;uint16_t cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	if (fhandle==0xff && rec_size!=0) {
		if (!DOS_FCBOpen(seg,offset)) return FCB_READ_NODATA;
		LOG(LOG_FCB,LOG_WARN)("Reopened closed FCB");
		fcb.GetSeqData(fhandle,rec_size);
	}
	if (rec_size == 0) {
		rec_size = 128;
		fcb.SetSeqData(fhandle,rec_size);
	}
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_READ_NODATA; 
	uint16_t toread=rec_size;
	if (!DOS_ReadFile(fhandle,dos_copybuf,&toread,true)) return FCB_READ_NODATA;
	if (toread==0) return FCB_READ_NODATA;
	if (toread < rec_size) { //Zero pad copybuffer to rec_size
		Bitu i = toread;
		while(i < rec_size) dos_copybuf[i++] = 0;
	}
	MEM_BlockWrite(Real2Phys(dos.dta())+(PhysPt)(recno*rec_size),dos_copybuf,rec_size);
	if (++cur_rec>127u) { cur_block++;cur_rec=0; }
	fcb.SetRecord(cur_block,cur_rec);
	if (toread==rec_size) return FCB_SUCCESS;
	return FCB_READ_PARTIAL;
}

uint8_t DOS_FCBWrite(uint16_t seg,uint16_t offset,uint16_t recno) {
	DOS_FCB fcb(seg,offset);
	uint8_t fhandle,cur_rec;uint16_t cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	if (fhandle==0xffu && rec_size!=0u) {
		if (!DOS_FCBOpen(seg,offset)) return FCB_READ_NODATA;
		LOG(LOG_FCB,LOG_WARN)("Reopened closed FCB");
		fcb.GetSeqData(fhandle,rec_size);
	}
	if (rec_size == 0) {
		rec_size = 128;
		fcb.SetSeqData(fhandle,rec_size);
	}
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	MEM_BlockRead(Real2Phys(dos.dta())+(PhysPt)(recno*rec_size),dos_copybuf,rec_size);
	uint16_t towrite=rec_size;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	uint32_t size;uint16_t date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	uint32_t ticks = mem_readd(BIOS_TIMER);
	uint32_t seconds = (ticks*10u)/182u;
	uint16_t hour = (uint16_t)(seconds/3600u);
	uint16_t min = (uint16_t)((seconds % 3600u)/60u);
	uint16_t sec = (uint16_t)(seconds % 60u);
	time = DOS_PackTime(hour,min,sec);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	if (++cur_rec>127u) { cur_block++;cur_rec=0; }	
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

uint8_t DOS_FCBIncreaseSize(uint16_t seg,uint16_t offset) {
	DOS_FCB fcb(seg,offset);
	uint8_t fhandle,cur_rec;uint16_t cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	uint16_t towrite=0;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	uint32_t size;uint16_t date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	uint32_t ticks = mem_readd(BIOS_TIMER);
	uint32_t seconds = (ticks*10u)/182u;
	uint16_t hour = (uint16_t)(seconds/3600u);
	uint16_t min = (uint16_t)((seconds % 3600u)/60u);
	uint16_t sec = (uint16_t)(seconds % 60u);
	time = DOS_PackTime(hour,min,sec);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

uint8_t DOS_FCBRandomRead(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore) {
/* if restore is true :random read else random blok read. 
 * random read updates old block and old record to reflect the random data
 * before the read!!!!!!!!! and the random data is not updated! (user must do this)
 * Random block read updates these fields to reflect the state after the read!
 */
	DOS_FCB fcb(seg,offset);
	uint32_t random;
	uint16_t old_block=0;
	uint8_t old_rec=0;
	uint8_t error=0;
	uint16_t count;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((uint16_t)(random / 128u),(uint8_t)(random & 127u));
	if (restore) fcb.GetRecord(old_block,old_rec);//store this for after the read.
	// Read records
	for (count=0; count<*numRec; count++) {
		error = DOS_FCBRead(seg,offset,count);
		if (error!=FCB_SUCCESS) break;
	}
	if (error==FCB_READ_PARTIAL) count++;	//partial read counts
	*numRec=count;
	uint16_t new_block;uint8_t new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false*/
	if(!restore) fcb.SetRandom(new_block*128u+new_rec); 
	return error;
}

uint8_t DOS_FCBRandomWrite(uint16_t seg,uint16_t offset,uint16_t * numRec,bool restore) {
/* see FCB_RandomRead */
	DOS_FCB fcb(seg,offset);
	uint32_t random;
	uint16_t old_block=0;
	uint8_t old_rec=0;
	uint8_t error=0;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((uint16_t)(random / 128u),(uint8_t)(random & 127u));
	if (restore) fcb.GetRecord(old_block,old_rec);
	if (*numRec > 0) {
		uint16_t count;
		/* Write records */
		for (count=0; count<*numRec; count++) {
			error = DOS_FCBWrite(seg,offset,count);// dos_fcbwrite return 0 false when true...
			if (error!=FCB_SUCCESS) break;
		}
		*numRec=count;
	} else {
		DOS_FCBIncreaseSize(seg,offset);
	}
	uint16_t new_block;uint8_t new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false */
	if (!restore) fcb.SetRandom(new_block*128u+new_rec); 
	return error;
}

bool DOS_FCBGetFileSize(uint16_t seg,uint16_t offset) {
	char shortname[DOS_PATHLENGTH];uint16_t entry;
	DOS_FCB fcb(seg,offset);
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,OPEN_READ,&entry,true)) return false;
	uint32_t size = 0;
	Files[entry]->Seek(&size,DOS_SEEK_END);
	DOS_CloseFile(entry,true);

	uint8_t handle; uint16_t rec_size;
	fcb.GetSeqData(handle,rec_size);
	if (rec_size == 0) rec_size = 128; //Use default if missing.

	uint32_t random=(size/rec_size);
	if (size % rec_size) random++;
	fcb.SetRandom(random);
	return true;
}

bool DOS_FCBDeleteFile(uint16_t seg,uint16_t offset){
/* Special case: ????????.??? and DOS_ATTR_VOLUME */
    {
        DOS_FCB fcb(seg,offset);
        uint8_t attr = 0;
        fcb.GetAttr(attr);
        uint8_t drive = fcb.GetDrive();
        std::string label = Drives[drive]->GetLabel();

        if (attr & DOS_ATTR_VOLUME) {
        char shortname[DOS_FCBNAME];
            fcb.GetVolumeName(shortname);

            if (!strcmp(shortname,"???????????")) {
                if (!label.empty()) {
                    Drives[drive]->SetLabel("",false,true);
                    LOG(LOG_DOSMISC,LOG_NORMAL)("FCB delete volume label");
                    return true;
                }
            }
            else {
                /* MS-DOS 6.22 LABEL.EXE will explicitly request to delete the volume label by the volume label not ????????.???? */
                if (!label.empty()) {
                    while (label.length() < 11) label += ' ';
                    if (!memcmp(label.c_str(),shortname,11)) {
                        Drives[drive]->SetLabel("",false,true);
                        LOG(LOG_DOSMISC,LOG_NORMAL)("FCB delete volume label deleted");
                        return true;
                    }
                }
            }

            LOG(LOG_DOSMISC,LOG_NORMAL)("FCB delete volume label not found (current='%s' asking='%s')",label.c_str(),shortname);
            DOS_SetError(DOSERR_FILE_NOT_FOUND); // right?
            return false;
        }
    }

/* FCB DELETE honours wildcards. it will return true if one or more
 * files get deleted. 
 * To get this: the dta is set to temporary dta in which found files are
 * stored. This can not be the tempdta as that one is used by fcbfindfirst
 */
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta_fcbdelete);
	RealPt new_dta=dos.dta();
	bool nextfile = false;
	bool return_value = false;
	nextfile = DOS_FCBFindFirst(seg,offset);
	DOS_FCB fcb(RealSeg(new_dta),RealOff(new_dta));
	while(nextfile) {
		char shortname[DOS_FCBNAME] = { 0 };
		fcb.GetName(shortname);
		bool res=DOS_UnlinkFile(shortname);
		if(!return_value && res) return_value = true; //at least one file deleted
		nextfile = DOS_FCBFindNext(seg,offset);
	}
	dos.dta(old_dta);  /*Restore dta */
	return return_value;
}

char* trimString(char* str);

bool DOS_FCBRenameFile(uint16_t seg, uint16_t offset){
	DOS_FCB fcbold(seg,offset);
    DOS_FCB fcbnew(seg,offset);
    fcbnew.SetPtPhys(fcbnew.GetPtPhys()+0x10u);//HACK: FCB NEW memory offset is affected by whether FCB OLD is extended
    if(!fcbold.Valid()) return false;
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);fcbnew.GetName(newname);

    {
        uint8_t drive = fcbold.GetDrive();
        std::string label = Drives[drive]->GetLabel();
        uint8_t attr = 0;

        fcbold.GetAttr(attr);
        /* According to RBIL and confirmed with SETLABEL.ASM in DOSLIB2, you can rename a volume label dirent as well with this function */
        if (attr & DOS_ATTR_VOLUME) {
            fcbold.GetVolumeName(oldname);
            fcbnew.GetVolumeName(newname);

            for (unsigned int i=0;i < 11;i++)
                oldname[i] = toupper(oldname[i]);

            trimString(oldname);
            trimString(newname);

            if (!label.empty()) {
                if (!strcmp(oldname,"???????????") || label == oldname) {
                    Drives[drive]->SetLabel(newname,false,true);
                    LOG(LOG_DOSMISC,LOG_NORMAL)("FCB rename volume label to '%s' from '%s'",newname,oldname);
                    return true;
                }
                else {
                    LOG(LOG_DOSMISC,LOG_NORMAL)("FCB rename volume label rejected, does not match current label '%s' from '%s'",newname,oldname);
                    DOS_SetError(DOSERR_FILE_NOT_FOUND); // right?
                    return false;
                }
            }
            else {
                LOG(LOG_DOSMISC,LOG_NORMAL)("FCB rename volume label rejected, no label set");
                DOS_SetError(DOSERR_FILE_NOT_FOUND); // right?
                return false;
            }
        }
    }

	/* Check, if sourcefile is still open. This was possible in DOS, but modern oses don't like this */
	uint8_t drive; char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullname,&drive)) return false;
	
	DOS_PSP psp(dos.psp());
	for (uint8_t i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->IsName(fullname)) {
			uint16_t handle = psp.FindEntryByHandle(i);
			//(more than once maybe)
			if (handle == 0xFFu) {
				DOS_CloseFile(i,true);
			} else {
				DOS_CloseFile(handle);
			}
		}
	}

	/* Rename the file */
	return DOS_Rename(oldname,newname);
}

void DOS_FCBSetRandomRecord(uint16_t seg, uint16_t offset) {
	DOS_FCB fcb(seg,offset);
	uint16_t block;uint8_t rec;
	fcb.GetRecord(block,rec);
	fcb.SetRandom(block*128u+rec);
}


bool DOS_FileExists(char const * const name) {
	char fullname[DOS_PATHLENGTH];uint8_t drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileExists(fullname);
}

bool DOS_GetAllocationInfo(uint8_t drive,uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters) {
	if (!drive) drive =  DOS_GetDefaultDrive();
	else drive--;
	if (drive >= DOS_DRIVES || !Drives[drive]) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	uint16_t _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector,_sectors_cluster,_total_clusters,&_free_clusters);
	SegSet16(ds,RealSeg(dos.tables.mediaid));
	reg_bx=RealOff(dos.tables.mediaid+drive*dos.tables.dpb_size);
	return true;
}

bool DOS_SetDrive(uint8_t drive) {
	if (Drives[drive]) {
		DOS_SetDefaultDrive(drive);
		return true;
	} else {
		return false;
	}
}

bool DOS_GetFileDate(uint16_t entry, uint16_t* otime, uint16_t* odate) {
	uint32_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]->UpdateDateTimeFromHost()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false; 
	}
	*otime = Files[handle]->time;
	*odate = Files[handle]->date;
	return true;
}

bool DOS_SetFileDate(uint16_t entry, uint16_t ntime, uint16_t ndate)
{
	uint32_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	Files[handle]->time = ntime;
	Files[handle]->date = ndate;
	Files[handle]->newtime = true;

	return true;
}

extern int swapInDisksSpecificDrive;
void DOS_SetupFiles (void) {
	/* Setup the File Handles */
	Files = new DOS_File * [DOS_FILES];
	uint32_t i;
	for (i=0;i<DOS_FILES;i++) {
		Files[i]=0;
	}
	/* Setup the Virtual Disk System */
	for (i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]) DriveManager::UnmountDrive(i);
		Drives[i]=0;
	}
    for (int i=0; i<MAX_DISK_IMAGES; i++) {
        if (imageDiskList[i]) {
            delete imageDiskList[i];
            imageDiskList[i] = NULL;
        }
    }
    if (swapInDisksSpecificDrive != -1) {
        for (size_t si=0;si < MAX_SWAPPABLE_DISKS;si++) {
            if (diskSwap[si] != NULL) {
                diskSwap[si]->Release();
                diskSwap[si] = NULL;
            }
        }
        swapInDisksSpecificDrive = -1;
    }
	Drives[25]=new Virtual_Drive();
}

// save state support
void DOS_File::SaveState( std::ostream& stream )
{
	uint32_t file_namelen, seek_pos;


	file_namelen = (uint32_t)strlen( name );
	seek_pos = GetSeekPos();

	//******************************************
	//******************************************
	//******************************************

	// - pure data
	WRITE_POD( &file_namelen, file_namelen );
	WRITE_POD_SIZE( name, file_namelen );

	WRITE_POD( &flags, flags );
	WRITE_POD( &open, open );

	WRITE_POD( &attr, attr );
	WRITE_POD( &time, time );
	WRITE_POD( &date, date );
	WRITE_POD( &refCtr, refCtr );
	WRITE_POD( &hdrive, hdrive );

	//******************************************
	//******************************************
	//******************************************

	// - reloc ptr
	WRITE_POD( &seek_pos, seek_pos );
}


void DOS_File::LoadState( std::istream& stream, bool pop )
{
	uint32_t file_namelen, seek_pos;
	char *file_name;

	//******************************************
	//******************************************
	//******************************************

	// - pure data
	READ_POD( &file_namelen, file_namelen );
	file_name = (char*)alloca( file_namelen * sizeof(char) );
	READ_POD_SIZE( file_name, file_namelen );

	READ_POD( &flags, flags );
	READ_POD( &open, open );

	READ_POD( &attr, attr );
	READ_POD( &time, time );
	READ_POD( &date, date );
	READ_POD( &refCtr, refCtr );
	READ_POD( &hdrive, hdrive );

	//******************************************
	//******************************************
	//******************************************

	// - reloc ptr
	READ_POD( &seek_pos, seek_pos );

	if (pop)
		return;

	if( open ) Seek( &seek_pos, DOS_SEEK_SET );
	else Close();
}

extern bool dos_kernel_disabled;
extern uint8_t ZDRIVE_NUM;
struct Alloc {
    uint16_t bytes_sector;
    uint8_t sectors_cluster;
    uint16_t total_clusters;
    uint16_t free_clusters;
    uint8_t mediaid;
};
Alloc lalloc, oalloc;
struct Opts {
    uint32_t bytesector;
    uint32_t cylsector;
    uint32_t headscyl;
    uint32_t cylinders;
    int mounttype;
    uint8_t mediaid;
    unsigned char CDROM_drive;
    unsigned long cdrom_sector_offset;
    unsigned char floppy_emu_type;
};
Opts opts;
char overlaydir[CROSS_LEN];
void MSCDEX_SetCDInterface(int intNr, int forceCD);
void POD_Save_DOS_Files( std::ostream& stream )
{
    char dinfo[256];
    WRITE_POD( &ZDRIVE_NUM, ZDRIVE_NUM);
	if (!dos_kernel_disabled) {
		// 1. Do drives first (directories -> files)
		// 2. Then files next

		for( int i=2; i<DOS_DRIVES+2; i++ ) {
            int lcv=i<DOS_DRIVES?i:i-DOS_DRIVES;
			uint8_t drive_valid;

			drive_valid = 0;
			if( Drives[lcv] == 0 ) drive_valid = 0xff;

			//**********************************************
			//**********************************************
			//**********************************************

			// - reloc ptr
			WRITE_POD( &drive_valid, drive_valid );
			if( drive_valid == 0xff ) continue;

            strcpy(dinfo, Drives[lcv]->GetInfo());
            WRITE_POD( &dinfo, dinfo);
            *overlaydir=0;
            if (!strncmp(dinfo,"local directory ",16) || !strncmp(dinfo,"CDRom ",6) || !strncmp(dinfo,"PhysFS directory ",17) || !strncmp(dinfo,"PhysFS CDRom ",13) ) {
                localDrive *ldp = dynamic_cast<localDrive*>(Drives[lcv]);
                if (!ldp) ldp = dynamic_cast<cdromDrive*>(Drives[lcv]);
                if (!ldp) ldp = dynamic_cast<physfsDrive*>(Drives[lcv]);
                if (ldp) {
                    lalloc.bytes_sector=ldp->allocation.bytes_sector;
                    lalloc.sectors_cluster=ldp->allocation.sectors_cluster;
                    lalloc.total_clusters=ldp->allocation.total_clusters;
                    lalloc.free_clusters=ldp->allocation.free_clusters;
                    lalloc.mediaid=ldp->allocation.mediaid;
                }
                Overlay_Drive *odp = dynamic_cast<Overlay_Drive*>(Drives[lcv]);
                if (odp) {
                    strcpy(overlaydir,odp->getOverlaydir());
                    oalloc.bytes_sector=odp->allocation.bytes_sector;
                    oalloc.sectors_cluster=odp->allocation.sectors_cluster;
                    oalloc.total_clusters=odp->allocation.total_clusters;
                    oalloc.free_clusters=odp->allocation.free_clusters;
                    oalloc.mediaid=odp->allocation.mediaid;
                } else {
                    physfsDrive *pdp = dynamic_cast<physfsDrive*>(Drives[lcv]);
                    if (pdp && pdp->getOverlaydir())
                        strcpy(overlaydir,pdp->getOverlaydir());
                }
            } else if (!strncmp(dinfo,"fatDrive ",9)) {
                fatDrive *fdp = dynamic_cast<fatDrive*>(Drives[lcv]);
                if (fdp) {
                    opts.bytesector=fdp->loadedDisk?fdp->loadedDisk->sector_size:fdp->opts.bytesector;
                    opts.cylsector=fdp->loadedDisk?fdp->loadedDisk->sectors:fdp->opts.cylsector;
                    opts.headscyl=fdp->loadedDisk?fdp->loadedDisk->heads:fdp->opts.headscyl;
                    opts.cylinders=fdp->loadedDisk?fdp->loadedDisk->cylinders:fdp->opts.cylinders;
                    opts.mounttype=fdp->opts.mounttype;
                    opts.mediaid=fdp->GetMediaByte();
                    opts.CDROM_drive=fdp->el.CDROM_drive;
                    opts.cdrom_sector_offset=fdp->el.cdrom_sector_offset;
                    opts.floppy_emu_type=fdp->el.floppy_emu_type;
                }
            }

            WRITE_POD( &overlaydir, overlaydir);
            WRITE_POD( &lalloc, lalloc);
            WRITE_POD( &oalloc, oalloc);
            WRITE_POD( &opts, opts);
			Drives[lcv]->SaveState(stream);
		}

		for( unsigned int lcv=0; lcv<DOS_FILES; lcv++ ) {
			uint8_t file_valid;
			char *file_name;
			uint8_t file_namelen, file_drive, file_flags;

			file_valid = 0;
			if( !Files[lcv] || Files[lcv]->GetName() == NULL ) file_valid = 0xff;
			else {
				if( strcmp( Files[lcv]->GetName(), "NUL" ) == 0 ) file_valid = 0xfe;//earth 2140 needs this
				if( strcmp( Files[lcv]->GetName(), "CON" ) == 0 ) file_valid = 0xfe;
				if( strcmp( Files[lcv]->GetName(), "LPT1" ) == 0 ) file_valid = 0xfe;
				if( strcmp( Files[lcv]->GetName(), "PRN" ) == 0 ) file_valid = 0xfe;
				if( strcmp( Files[lcv]->GetName(), "AUX" ) == 0 ) file_valid = 0xfe;
				if( strcmp( Files[lcv]->GetName(), "EMMXXXX0" ) == 0 ) file_valid = 0xfe;//raiden needs this
			}

			// - reloc ptr
			WRITE_POD( &file_valid, file_valid );
			// system files
			if( file_valid == 0xff ) continue;
			if( file_valid == 0xfe ) {
				WRITE_POD( &Files[lcv]->refCtr, Files[lcv]->refCtr );
				continue;
			}

			//**********************************************
			//**********************************************
			//**********************************************

			file_namelen = (uint8_t)strlen( Files[lcv]->name );
			file_name = (char *) alloca( file_namelen );
			strcpy( file_name, Files[lcv]->name );

			file_drive = Files[lcv]->GetDrive();
			file_flags = Files[lcv]->flags;

			// - Drives->FileOpen vars (repeat copy)
			WRITE_POD( &file_namelen, file_namelen );
			WRITE_POD_SIZE( file_name, file_namelen );

			WRITE_POD( &file_drive, file_drive );
			WRITE_POD( &file_flags, file_flags );

			Files[lcv]->SaveState(stream);
		}
	} else {
        for( int i=2; i<DOS_DRIVES+2; i++ ) {
            int lcv=i<DOS_DRIVES?i:i-DOS_DRIVES;
			uint8_t drive_valid;
			drive_valid = 0;
			if( Drives[lcv] == 0 ) drive_valid = 0xff;
			WRITE_POD( &drive_valid, drive_valid );
			if( drive_valid == 0xff ) continue;
            strcpy(dinfo, Drives[lcv]->GetInfo());
            WRITE_POD( &dinfo, dinfo);
        }
    }

    for (int i=2; i<MAX_DISK_IMAGES+2; i++) {
        int d=i<MAX_DISK_IMAGES?i:i-MAX_DISK_IMAGES;
        bool valid=imageDiskList[d]!=NULL;
        WRITE_POD( &valid, valid );
        if (!valid) continue;
        char diskname[256];
        imageDiskElToritoFloppy *image = dynamic_cast<imageDiskElToritoFloppy *>(imageDiskList[d]);
        strcpy(diskname, image!=NULL?"El Torito floppy drive":imageDiskList[d]->diskname.c_str());
        WRITE_POD( &diskname, diskname );
        if (image!=NULL) {
            opts.CDROM_drive=image->CDROM_drive;
            opts.cdrom_sector_offset=image->cdrom_sector_offset;
            opts.floppy_emu_type=image->floppy_type;
            opts.mounttype=1;
        } else {
            imageDiskMemory* idmem = dynamic_cast<imageDiskMemory *>(imageDiskList[d]);
            imageDiskVHD* idvhd = dynamic_cast<imageDiskVHD *>(imageDiskList[d]);
            if (idmem!=NULL)
                opts.mounttype=2;
            else if (idvhd!=NULL)
                opts.mounttype=3;
            else {
                opts.bytesector=imageDiskList[d]->sector_size;
                opts.cylsector=imageDiskList[d]->sectors;
                opts.headscyl=imageDiskList[d]->heads;
                opts.cylinders=imageDiskList[d]->cylinders;
                opts.mounttype=0;
            }
        }
        WRITE_POD( &opts, opts);
    }
}

void DOS_EnableDriveMenu(char drv);
void IDE_Auto(signed char &index,bool &slave);
bool AttachToBiosByLetter(imageDisk* image, const char drive);
bool AttachToBiosAndIdeByLetter(imageDisk* image, const char drive, const unsigned char ide_index, const bool ide_slave);
imageDiskMemory* CreateRamDrive(Bitu sizes[], const int reserved_cylinders, const bool forceFloppy, Program* obj);

void unmount(int lcv) {
    if (!Drives[lcv] || lcv>=DOS_DRIVES-1) return;
    const isoDrive* cdrom = dynamic_cast<isoDrive*>(Drives[lcv]);
    if (DriveManager::UnmountDrive(lcv) == 0) {
        if (cdrom) IDE_CDROM_Detach(lcv);
        Drives[lcv]=0;
        DOS_EnableDriveMenu('A'+lcv);
        mem_writeb(Real2Phys(dos.tables.mediaid)+(unsigned int)'A'+lcv*dos.tables.dpb_size,0);
    }
}

void POD_Load_DOS_Files( std::istream& stream )
{
    char dinfo[256];
    std::vector<int> clist;
    clist.clear();
    uint8_t ZDRIVE_CUR = ZDRIVE_NUM;
    READ_POD( &ZDRIVE_NUM, ZDRIVE_NUM);
	if (!dos_kernel_disabled) {
        if (ZDRIVE_CUR != ZDRIVE_NUM) {
            Drives[ZDRIVE_NUM] = Drives[ZDRIVE_CUR];
            Drives[ZDRIVE_CUR] = 0;
        }
		// 1. Do drives first (directories -> files)
		// 2. Then files next

		for( int i=2; i<DOS_DRIVES+2; i++ ) {
            int lcv=i<DOS_DRIVES?i:i-DOS_DRIVES;
			uint8_t drive_valid;

			// - reloc ptr
			READ_POD( &drive_valid, drive_valid );
			if( drive_valid == 0xff ) {
                if (Drives[lcv] && lcv<DOS_DRIVES-1) unmount(lcv);
                continue;
            }

            READ_POD( &dinfo, dinfo);
            READ_POD( &overlaydir, overlaydir);
            READ_POD( &lalloc, lalloc);
            READ_POD( &oalloc, oalloc);
            READ_POD( &opts, opts);
            if( Drives[lcv] && strcasecmp(Drives[lcv]->info, dinfo) && (!strncmp(dinfo,"local directory ",16) || !strncmp(dinfo,"CDRom ",6) || !strncmp(dinfo,"PhysFS directory ",17) || !strncmp(dinfo,"PhysFS CDRom ",13) || (!strncmp(dinfo,"isoDrive ",9) || !strncmp(dinfo,"fatDrive ",9))))
                unmount(lcv);
            if( !Drives[lcv] ) {
                std::vector<std::string> options;
                if (!strncmp(dinfo,"local directory ",16)) {
                    Drives[lcv]=new localDrive(dinfo+16,lalloc.bytes_sector,lalloc.sectors_cluster,lalloc.total_clusters,lalloc.free_clusters,lalloc.mediaid,options);
                    if (Drives[lcv]) {
                        DOS_EnableDriveMenu('A'+lcv);
                        mem_writeb(Real2Phys(dos.tables.mediaid)+lcv*dos.tables.dpb_size,lalloc.mediaid);
                        if (strlen(overlaydir)) {
                            uint8_t error = 0;
                            Drives[lcv]=new Overlay_Drive(dynamic_cast<localDrive*>(Drives[lcv])->getBasedir(),overlaydir,oalloc.bytes_sector,oalloc.sectors_cluster,oalloc.total_clusters,oalloc.free_clusters,oalloc.mediaid,error,options);
                        }
                    } else
                        LOG_MSG("Error: Cannot restore drive from directory %s\n", dinfo+16);
                } else if (!strncmp(dinfo,"CDRom ",6) || !strncmp(dinfo,"PhysFS CDRom ",13)) {
                    int num = -1;
                    int error = 0;
                    int id, major, minor;
                    DOSBox_CheckOS(id, major, minor);
                    if ((id==VER_PLATFORM_WIN32_NT) && (major>5))
                        MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DX, num);
                    else
                        MSCDEX_SetCDInterface(CDROM_USE_IOCTL_DIO, num);
                    if (!strncmp(dinfo,"PhysFS CDRom ",13)) {
                        std::string str=std::string(dinfo+13);
                        std::size_t found=str.find(", ");
                        if (found!=std::string::npos)
                            str=str.substr(0,found);
                        Drives[lcv] = new physfscdromDrive('A'+lcv,(":"+str+"\\").c_str(),lalloc.bytes_sector,lalloc.sectors_cluster,lalloc.total_clusters,0,lalloc.mediaid,error,options);
                    } else
                        Drives[lcv] = new cdromDrive('A'+lcv,dinfo+6,lalloc.bytes_sector,lalloc.sectors_cluster,lalloc.total_clusters,lalloc.free_clusters,lalloc.mediaid,error,options);
                    if (Drives[lcv]) {
                        DOS_EnableDriveMenu('A'+lcv);
                        mem_writeb(Real2Phys(dos.tables.mediaid)+lcv*dos.tables.dpb_size,lalloc.mediaid);
                    } else
                        LOG_MSG("Error: Cannot restore drive from directory %s\n", dinfo+6);
                } else if (!strncmp(dinfo,"PhysFS directory ",17)) {
                    int error = 0;
                    std::string str=std::string(dinfo+17);
                    std::size_t found=str.find(", ");
                    if (found!=std::string::npos)
                        str=str.substr(0,found);
                    Drives[lcv]=new physfsDrive('A'+lcv,(":"+str+"\\").c_str(),lalloc.bytes_sector,lalloc.sectors_cluster,lalloc.total_clusters,lalloc.free_clusters,lalloc.mediaid,error,options);
                    if (Drives[lcv]) {
                        if (strlen(overlaydir)) dynamic_cast<physfsDrive*>(Drives[lcv])->setOverlaydir(overlaydir);
                        DOS_EnableDriveMenu('A'+lcv);
                        mem_writeb(Real2Phys(dos.tables.mediaid)+lcv*dos.tables.dpb_size,lalloc.mediaid);
                    } else
                        LOG_MSG("Error: Cannot restore drive from directory %s\n", dinfo+16);
                } else if (!strncmp(dinfo,"isoDrive ",9) && *(dinfo+9)) {
                    MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
                    uint8_t mediaid = 0xF8;
                    int error = -1;
                    isoDrive* newDrive = new isoDrive('A'+lcv, dinfo+9, mediaid, error);
                    if (!error) {
                        Drives[lcv] = newDrive;
                        DriveManager::AppendDisk(lcv, newDrive);
                        DriveManager::InitializeDrive(lcv);
                        DOS_EnableDriveMenu('A'+lcv);
                        mem_writeb(Real2Phys(dos.tables.mediaid) + lcv*dos.tables.dpb_size, mediaid);
                        clist.push_back(lcv);
                    }
                } else if (!strncmp(dinfo,"fatDrive ",9)) {
                    fatDrive* newDrive = NULL;
                    Bitu sizes[4] = { 0,0,0,0 };
                    if (opts.mounttype==1) {
                        imageDisk * newImage = new imageDiskElToritoFloppy((unsigned char)opts.CDROM_drive, opts.cdrom_sector_offset, opts.floppy_emu_type);
                        if (newImage != NULL) {
                            newImage->Addref();
                            newDrive = new fatDrive(newImage, options);
                            newImage->Release();
                        } else
                            LOG_MSG("Error: Cannot restore drive from El Torito floppy image.\n");

                    } else if (opts.mounttype==2) {
                        imageDiskMemory* image = CreateRamDrive(sizes, 0, lcv < 2 && sizes[0] == 0, NULL);
                        if (image != NULL && image->Format() == 0x00) {
                            image->Addref();
                            newDrive = new fatDrive(image, options);
                            image->Release();
                        }
                    } else if (opts.mounttype==3 && *(dinfo+9)) {
                        imageDisk* vhdImage = NULL;
                        if (imageDiskVHD::Open(dinfo+9, false, &vhdImage)==imageDiskVHD::OPEN_SUCCESS)
                            newDrive = new fatDrive(vhdImage, options);
                        vhdImage = NULL;
                    } else if (!opts.mounttype && *(dinfo+9))
                        newDrive = new fatDrive(dinfo+9, opts.bytesector, opts.cylsector, opts.headscyl, opts.cylinders, options);
                    if (newDrive && newDrive->created_successfully) {
                        Drives[lcv] = newDrive;
                        DriveManager::AppendDisk(lcv, newDrive);
                        DriveManager::InitializeDrive(lcv);
                        DOS_EnableDriveMenu('A'+lcv);
                        mem_writeb(Real2Phys(dos.tables.mediaid) + lcv*dos.tables.dpb_size, opts.mediaid);
                    } else if ((!opts.mounttype || opts.mounttype==3) && *(dinfo+9))
                        LOG_MSG("Error: Cannot restore drive from image file %s\n", dinfo+9);
                }
            }
			if( Drives[lcv] ) Drives[lcv]->LoadState(stream);
		}

		//Alien Carnage - game creates and unlinks temp files
		//Here are two situations
		//1. Game already unlinked temp file, but information about file is still in Files[] and we saved it. In this case we must only pop old data from stream by loading. This is fixed.
		//2. Game still did not unlink file, We saved this information. Then was game restarted and temp files were removed. Then we try load save state, but we don't have temp file. This is not fixed
		DOS_File *dummy = NULL;

		for( unsigned int lcv=0; lcv<DOS_FILES; lcv++ ) {
			uint8_t file_valid;
			char *file_name;
			uint8_t file_namelen, file_drive, file_flags;

			// - reloc ptr
			READ_POD( &file_valid, file_valid );

			// ignore system files
			if( file_valid == 0xfe ) {
				READ_POD( &Files[lcv]->refCtr, Files[lcv]->refCtr );
				continue;
			}

			// shutdown old file
			if( Files[lcv] && Files[lcv]->GetName() != NULL ) {
				// invalid file state - abort
				if( strcmp( Files[lcv]->GetName(), "NUL" ) == 0 ) break;
				if( strcmp( Files[lcv]->GetName(), "CON" ) == 0 ) break;
				if( strcmp( Files[lcv]->GetName(), "LPT1" ) == 0 ) break;
				if( strcmp( Files[lcv]->GetName(), "PRN" ) == 0 ) break;
				if( strcmp( Files[lcv]->GetName(), "AUX" ) == 0 ) break;
				if( strcmp( Files[lcv]->GetName(), "EMMXXXX0" ) == 0 ) break;//raiden needs this


				if( Files[lcv]->IsOpen() ) Files[lcv]->Close();
				if (Files[lcv]->RemoveRef()<=0) {
					delete Files[lcv];
				}
				Files[lcv]=0;
			}

			// ignore NULL file
			if( file_valid == 0xff ) continue;

			//**********************************************
			//**********************************************
			//**********************************************

			// - Drives->FileOpen vars (repeat copy)

			READ_POD( &file_namelen, file_namelen );
			file_name = (char *) alloca( file_namelen );
			READ_POD_SIZE( file_name, file_namelen );

			READ_POD( &file_drive, file_drive );
			READ_POD( &file_flags, file_flags );


			// NOTE: Must open regardless to get 'new' DOS_File class
			Drives[file_drive]->FileOpen( &Files[lcv], file_name, file_flags );

			if( Files[lcv] ) {
				Files[lcv]->LoadState(stream, false);
			} else {
				//Alien carnage ->pop data for invalid file from stream
				if (dummy == NULL) {
					dummy = new localFile();
				}
				dummy->LoadState(stream, true);
			};
		}

		if (dummy != NULL) {
			delete dummy;
		}
	} else {
        for( int i=2; i<DOS_DRIVES+2; i++ ) {
            int lcv=i<DOS_DRIVES?i:i-DOS_DRIVES;
			uint8_t drive_valid;

			READ_POD( &drive_valid, drive_valid );
			if( drive_valid == 0xff ) {
                if (Drives[lcv]) unmount(lcv);
                continue;
            }
            READ_POD( &dinfo, dinfo);
            if( Drives[lcv] && strcasecmp(Drives[lcv]->info, dinfo))
                unmount(lcv);
            if (!Drives[lcv] && !strncmp(dinfo,"isoDrive ",9)) {
                MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
                uint8_t mediaid = 0xF8;
                int error = -1;
                isoDrive* newDrive = new isoDrive('A'+lcv, dinfo+9, mediaid, error);
                if (!error) {
                    Drives[lcv] = newDrive;
                    DriveManager::AppendDisk(lcv, newDrive);
                    DriveManager::InitializeDrive(lcv);
                    mem_writeb(Real2Phys(dos.tables.mediaid) + lcv*dos.tables.dpb_size, mediaid);
                    clist.push_back(lcv);
                }
            }
        }
    }

    for (int i=2; i<MAX_DISK_IMAGES+2; i++) {
        int d=i<MAX_DISK_IMAGES?i:i-MAX_DISK_IMAGES;
        bool ide_slave = false;
        signed char ide_index = -1;
        bool valid=false;
        READ_POD( &valid, valid );
        if (!valid) {
            if (imageDiskList[d]) {
                if (d > 1) IDE_Hard_Disk_Detach(d);
                imageDiskList[d]->Release();
                imageDiskList[d] = NULL;
                imageDiskChange[d] = true;
            }
            if (i==MAX_DISK_IMAGES-1)
                for (auto &it : clist) {
                    ide_slave = false;
                    ide_index = -1;
                    IDE_Auto(ide_index,ide_slave);
                    if (ide_index >= 0) IDE_CDROM_Attach(ide_index, ide_slave, it);
                }
            continue;
        }
        char diskname[256];
        READ_POD( &diskname, diskname );
        READ_POD( &opts, opts);
        Bitu sizes[4] = { 0,0,0,0 };
        ide_slave = false;
        ide_index = -1;
        IDE_Auto(ide_index,ide_slave);
        if( imageDiskList[d] && ((opts.mounttype==1 && dynamic_cast<imageDiskElToritoFloppy *>(imageDiskList[d])==NULL) || (opts.mounttype!=1 && strcasecmp(imageDiskList[d]->diskname.c_str(), diskname )))) {
            if (d > 1) IDE_Hard_Disk_Detach(d);
            imageDiskList[d]->Release();
            imageDiskList[d] = NULL;
            imageDiskChange[d] = true;
        }
        if (imageDiskList[d])
            ;
        else if (opts.mounttype==1) {
            imageDisk * image = new imageDiskElToritoFloppy((unsigned char)opts.CDROM_drive, opts.cdrom_sector_offset, opts.floppy_emu_type);
            if (image) AttachToBiosByLetter(image, 'A'+d);
            else LOG_MSG("Warning: Cannot restore drive number from El Torito floppy image.\n");
        } else if (opts.mounttype==2) {
            imageDiskMemory* image = CreateRamDrive(sizes, 0, d < 2 && sizes[0] == 0, NULL);
            if (image != NULL && image->Format() == 0x00) AttachToBiosAndIdeByLetter(image, d, (unsigned char)ide_index, ide_slave);
        } else if (opts.mounttype==3 && *diskname) {
            imageDisk* image = NULL;
            if (imageDiskVHD::Open(diskname, false, &image)==imageDiskVHD::OPEN_SUCCESS)
                AttachToBiosAndIdeByLetter(image, 'A'+d, (unsigned char)ide_index, ide_slave);
            else
                LOG_MSG("Warning: Cannot restore drive number from image file %s\n", diskname);
            image = NULL;
        } else if (!opts.mounttype && *diskname) {
            std::vector<std::string> options;
            fatDrive* newDrive = new fatDrive(diskname, opts.bytesector, opts.cylsector, opts.headscyl, opts.cylinders, options);
            if (newDrive->created_successfully) {
                imageDisk* image = newDrive->loadedDisk;
                AttachToBiosAndIdeByLetter(image, 'A'+d, (unsigned char)ide_index, ide_slave);
            } else
                LOG_MSG("Warning: Cannot restore drive number from image file %s\n", diskname);
            if (newDrive) delete newDrive;
        }
        if (i==MAX_DISK_IMAGES-1)
            for (auto &it : clist) {
                ide_slave = false;
                ide_index = -1;
                IDE_Auto(ide_index,ide_slave);
                if (ide_index >= 0) IDE_CDROM_Attach(ide_index, ide_slave, it);
            }
    }
}
