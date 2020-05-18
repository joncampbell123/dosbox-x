/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA.
 */


#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "drives.h"
#include "cross.h"
#include "control.h"
#include "dos_network2.h"

#define DOS_FILESTART 4

#define FCB_SUCCESS     0
#define FCB_READ_NODATA	1
#define FCB_READ_PARTIAL 3
#define FCB_ERR_NODATA  1
#define FCB_ERR_EOF     3
#define FCB_ERR_WRITE   1

extern bool log_int21;
extern bool log_fileio;
extern int dos_clipboard_device_access;
extern char *dos_clipboard_device_name;

Bitu DOS_FILES = 127;
DOS_File ** Files = NULL;
DOS_Drive * Drives[DOS_DRIVES] = {NULL};
bool force = false;
int sdrive = 0;

/* This is the LFN filefind handle that is currently being used, with normal values between
 * 0 and 254 for LFN calls. The value LFN_FILEFIND_INTERNAL (255) is used internally by LFN
 * handling functions. For non-LFN calls the value is fixed to be LFN_FILEFIND_NONE (256). */
int lfn_filefind_handle = LFN_FILEFIND_NONE;

bool shiftjis_lead_byte(int c);

Bit8u DOS_GetDefaultDrive(void) {
//	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	Bit8u d = DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	if( d != dos.current_drive ) LOG(LOG_DOSMISC,LOG_ERROR)("SDA drive %d not the same as dos.current_drive %d",d,dos.current_drive);
	return dos.current_drive;
}

void DOS_SetDefaultDrive(Bit8u drive) {
//	if (drive<=DOS_DRIVES && ((drive<2) || Drives[drive])) DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);
	if (drive<DOS_DRIVES && ((drive<2) || Drives[drive])) {dos.current_drive = drive; DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);}
}

bool DOS_MakeName(char const * const name,char * const fullname,Bit8u * drive) {
	if(!name || *name == 0 || *name == ' ') {
		/* Both \0 and space are seperators and
		 * empty filenames report file not found */
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
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
		Bit8u c=(Bit8u)name_int[r++];
		if (c=='/') c='\\';
		else if (c=='"') {q++;continue;}
		else if (uselfn&&!force) {
			if (c==' ' && q/2*2 == q) continue;
		} else {
			if ((c>='a') && (c<='z')) c-=32;
			else if (c==' ') continue; /* should be separator */
		}
		upname[w++]=(char)c;
        if (IS_PC98_ARCH && shiftjis_lead_byte(c) && r<DOS_PATHLENGTH) {
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
	Bit32u lastdir=0;Bit32u t=0;
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

			Bit32s iDown;
			bool dots = true;
			Bit32s templen=(Bit32s)strlen(tempdir);
			for(iDown=0;(iDown < templen) && dots;iDown++)
				if(tempdir[iDown] != '.')
					dots = false;

			// only dots?
			if (dots && (templen > 1)) {
				Bit32s cDots = templen - 1;
				for(iDown=(Bit32s)strlen(fullname)-1;iDown>=0;iDown--) {
					if(fullname[iDown]=='\\' || iDown==0) {
						lastdir = (Bit32u)iDown;
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
			

			lastdir=(Bit32u)strlen(fullname);

			if (lastdir!=0) strcat(fullname,"\\");
			if (!uselfn||force) {
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
					// if((strlen(tempdir) - strlen(ext)) > 8) memmove(tempdir + 8, ext, 5);
                } else {
                    // tempdir[8] = 0;
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

bool DOS_GetSFNPath(char const * const path,char * SFNPath,bool LFN) {
    char pdir[LFN_NAMELENGTH], *p;
    Bit8u drive;char fulldir[DOS_PATHLENGTH],LFNPath[CROSS_LEN];
    char name[DOS_NAMELENGTH_ASCII], lname[LFN_NAMELENGTH];
    Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
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

bool DOS_GetCurrentDir(Bit8u drive,char * const buffer, bool LFN) {
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
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
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
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	size_t len = strlen(dir);
	if(!len || dir[len-1] == '\\') {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
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
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
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

bool DOS_Rename(char const * const oldname,char const * const newname) {
	Bit8u driveold;char fullold[DOS_PATHLENGTH];
	Bit8u drivenew;char fullnew[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullold,&driveold)) return false;
	if (!DOS_MakeName(newname,fullnew,&drivenew)) return false;
	/* No tricks with devices */
	bool clip=false;
	if ( (DOS_FindDevice(oldname) != DOS_DEVICES) ||
	     (DOS_FindDevice(newname) != DOS_DEVICES) ) {
#if defined (WIN32)
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
#endif
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
	/*Test if target exists => no access */
	Bit16u attr;
	if(Drives[drivenew]->GetFileAttr(fullnew,&attr)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	/* Source must exist, check for path ? */
	if (!Drives[driveold]->GetFileAttr( fullold, &attr ) ) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

#if defined (WIN32)
	if (clip) {
		Bit16u sourceHandle, targetHandle, toread = 0x8000;
		static Bit8u buffer[0x8000];
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
	/* If it still fails, which error should we give ? PATH NOT FOUND or EACCESS */
	if (dos.errorcode!=DOSERR_ACCESS_DENIED&&dos.errorcode!=DOSERR_WRITE_PROTECTED) {
		LOG(LOG_FILES,LOG_NORMAL)("Rename fails for %s to %s, no proper errorcode returned.",oldname,newname);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
	}
	return false;
}

bool DOS_FindFirst(char * search,Bit16u attr,bool fcb_findfirst) {
	LOG(LOG_FILES,LOG_NORMAL)("file search attributes %X name %s",attr,search);
	DOS_DTA dta(dos.dta());
	Bit8u drive;char fullsearch[DOS_PATHLENGTH];
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

	sdrive=drive;
	dta.SetupSearch(drive,(Bit8u)attr,pattern);

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
	Bit8u i = dta.GetSearchDrive();
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


bool DOS_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_ReadFile(entry,data,amount);
#endif
	Bit32u handle = fcb?entry:RealHandle(entry);
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
	Bit16u toread=*amount;
	bool ret=Files[handle]->Read(data,&toread);
	*amount=toread;
	return ret;
}

bool DOS_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_WriteFile(entry,data,amount);
#endif
	Bit32u handle = fcb?entry:RealHandle(entry);
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
	Bit16u towrite=*amount;
	bool ret=Files[handle]->Write(data,&towrite);
	*amount=towrite;
	return ret;
}

bool DOS_SeekFile(Bit16u entry,Bit32u * pos,Bit32u type,bool fcb) {
	Bit32u handle = fcb?entry:RealHandle(entry);
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
bool DOS_LockFile(Bit16u entry,Bit8u mode,Bit32u pos,Bit32u size) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
#ifdef WIN32
	return Files[handle]->LockFile(mode,pos,size);
#else
    (void)mode;//UNUSED
    (void)size;//UNUSED
    (void)pos;//UNUSED
	return true;
#endif
}

bool DOS_CloseFile(Bit16u entry, bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsActiveResource(entry))
		return Network_CloseFile(entry);
#endif
	Bit32u handle = fcb?entry:RealHandle(entry);
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

	if (Files[handle]->RemoveRef()<=0) {
		delete Files[handle];
		Files[handle]=0;
	}
	return true;
}

bool DOS_FlushFile(Bit16u entry) {
	Bit32u handle=RealHandle(entry);
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

static bool PathExists(char const * const name) {
	const char* leading = strrchr(name,'\\');
	if(!leading) return true;
	char temp[CROSS_LEN];
	strcpy(temp,name);
	char * lead = strrchr(temp,'\\');
	if (lead == temp) return true;
	*lead = 0;
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(temp,fulldir,&drive)) return false;
	if(!Drives[drive]->TestDir(fulldir)) return false;
	return true;
}


bool DOS_CreateFile(char const * name,Bit16u attributes,Bit16u * entry,bool fcb) {
	// Creation of a device is the same as opening it
	// Tc201 installer
	if (DOS_FindDevice(name) != DOS_DEVICES)
		return DOS_OpenFile(name, OPEN_READ, entry, fcb);

	LOG(LOG_FILES,LOG_NORMAL)("file create attributes %X file %s",attributes,name);
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	DOS_PSP psp(dos.psp());
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	/* Check for a free file handle */
	Bit8u handle=(Bit8u)DOS_FILES;Bit8u i;
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
		if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
		else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_OpenFile(char const * name,Bit8u flags,Bit16u * entry,bool fcb) {
#if defined(WIN32) && !defined(__MINGW32__)
	if(Network_IsNetworkResource(const_cast<char *>(name)))
		return Network_OpenFile(const_cast<char *>(name),flags,entry);
#endif
	/* First check for devices */
	if (flags>2) LOG(LOG_FILES,LOG_NORMAL)("Special file open command %X file %s",flags,name); // FIXME: Why? Is there something about special opens DOSBox doesn't handle properly?
	else LOG(LOG_FILES,LOG_NORMAL)("file open command %X file %s",flags,name);

	DOS_PSP psp(dos.psp());
	Bit16u attr = 0;
	Bit8u devnum = DOS_FindDevice(name);
	bool device = (devnum != DOS_DEVICES);
	if(!device && DOS_GetFileAttr(name,&attr)) {
	//DON'T ALLOW directories to be openened.(skip test if file is device).
		if((attr & DOS_ATTR_DIRECTORY) || (attr & DOS_ATTR_VOLUME)){
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	char fullname[DOS_PATHLENGTH];Bit8u drive;Bit8u i;
	/* First check if the name is correct */
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	Bit8u handle=255;		
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
		if(((flags&3) != OPEN_READ) && Drives[drive]->FileExists(fullname))
			DOS_SetError(DOSERR_ACCESS_DENIED);
		else {
			if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
			else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		}
		return false;
	}
}

bool DOS_OpenFileExtended(char const * name, Bit16u flags, Bit16u createAttr, Bit16u action, Bit16u *entry, Bit16u* status) {
// FIXME: Not yet supported : Bit 13 of flags (int 0x24 on critical error)
	Bit16u result = 0;
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
	if (DOS_OpenFile(name, (Bit8u)(flags&0xff), entry)) {
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
	char fullname[DOS_PATHLENGTH];Bit8u drive;
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
	} else {
		if (dos.errorcode!=DOSERR_ACCESS_DENIED&&dos.errorcode!=DOSERR_WRITE_PROTECTED) DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_GetFileAttr(char const * const name,Bit16u * attr) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
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

bool DOS_GetFileAttrEx(char const* const name, struct stat *status, Bit8u hdrive)
{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
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
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	return Drives[drive]->GetCompressedSize(fullname);
}

#if defined (WIN32)
HANDLE DOS_CreateOpenFile(char const* const name)
{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return INVALID_HANDLE_VALUE;
	return Drives[drive]->CreateOpenFile(fullname);
}
#endif

bool DOS_SetFileAttr(char const * const name,Bit16u attr) 
// returns false when using on cdrom (stonekeep)
{
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;	
	if (strncmp(Drives[drive]->GetInfo(),"CDRom ",6)==0 || strncmp(Drives[drive]->GetInfo(),"isoDrive ",9)==0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* This function must prevent changing a file into a directory, volume label into a file, etc.
	 * Also Windows 95 setup likes to create WINBOOT.INI as a file and then chattr it into a directory for some stupid reason. */
	Bit16u old_attr;
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
	const Bit16u attr_mask = (DOS_ATTR_VOLUME|DOS_ATTR_DIRECTORY);

	attr = (attr & ~attr_mask) | (old_attr & attr_mask);

	return Drives[drive]->SetFileAttr(fullname,attr);
}

bool DOS_Canonicalize(char const * const name,char * const big) {
//TODO Add Better support for devices and shit but will it be needed i doubt it :) 
	Bit8u drive;
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
		Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free,
		const Bit32u bytes32,const Bit32u sectors32,const Bit32u clusters32,const Bit32u free32) {
	Bit32u cdiv = 1;

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
	const Bit32u clust2gb = (Bit32u)0x7FFF8000ul / (Bit32u)bytes32 / (sectors32 * cdiv);

	*bytes = bytes32;
	*sectors = sectors32 * cdiv;
	*clusters = (Bit16u)MIN(MIN(clusters32 / cdiv,clust2gb),0xFFFFu);
	*free = (Bit16u)MIN(MIN(free32 / cdiv,clust2gb),0xFFFFu);
	return true;
}

bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}

	{
		Bit32u bytes32,sectors32,clusters32,free32;
		if ((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) &&
			Drives[drive]->AllocationInfo32(&bytes32,&sectors32,&clusters32,&free32) &&
			DOS_CommonFAT32FAT16DiskSpaceConv(bytes,sectors,clusters,free,bytes32,sectors32,clusters32,free32))
			return true;
	}

	if (Drives[drive]->AllocationInfo(bytes,sectors,clusters,free))
		return true;

	return false;
}

bool DOS_GetFreeDiskSpace32(Bit8u drive,Bit32u * bytes,Bit32u * sectors,Bit32u * clusters,Bit32u * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}

	if ((dos.version.major > 7 || (dos.version.major == 7 && dos.version.minor >= 10)) && Drives[drive]->AllocationInfo32(bytes,sectors,clusters,free))
		return true;

	{
		Bit8u sectors8;
		Bit16u bytes16,clusters16,free16;
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

bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry) {
	// Dont duplicate console handles
/*	if (entry<=STDPRN) {
		*newentry = entry;
		return true;
	};
*/	
	Bit8u handle=RealHandle(entry);
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

bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry) {
	if(entry == newentry) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	Bit8u orig = RealHandle(entry);
	if (orig >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[orig] || !Files[orig]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	Bit8u newone = RealHandle(newentry);
	if (newone < DOS_FILES && Files[newone]) {
		DOS_CloseFile(newentry);
	}
	DOS_PSP psp(dos.psp());
	Files[orig]->AddRef();
	psp.SetFileHandle(newentry,orig);
	return true;
}


bool DOS_CreateTempFile(char * const name,Bit16u * entry) {
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
	do {
		Bit32u i;
		for (i=0;i<8;i++) {
			tempname[i]=(rand()%26)+'A';
		}
		tempname[8]=0;
	} while ((!DOS_CreateFile(name,0,entry)) && (dos.errorcode==DOSERR_FILE_ALREADY_EXISTS));
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
	return (Bit8u(in)>0x1F) && (!strchr(ill,in));
}

#define PARSE_SEP_STOP          0x01
#define PARSE_DFLT_DRIVE        0x02
#define PARSE_BLNK_FNAME        0x04
#define PARSE_BLNK_FEXT         0x08

#define PARSE_RET_NOWILD        0
#define PARSE_RET_WILD          1
#define PARSE_RET_BADDRIVE      0xff

Bit8u FCB_Parsename(Bit16u seg,Bit16u offset,Bit8u parser ,char *string, Bit8u *change) {
    const char* string_begin = string;
	Bit8u ret=0;
	if (!(parser & PARSE_DFLT_DRIVE)) {
		// default drive forced, this intentionally invalidates an extended FCB
		mem_writeb(PhysMake(seg,offset),0);
	}
	DOS_FCB fcb(seg,offset,false);	// always a non-extended FCB
	bool hasdrive,hasname,hasext,finished;
	hasdrive=hasname=hasext=finished=false;
	Bitu index=0;
	Bit8u fill=' ';
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
		if (IS_PC98_ARCH && shiftjis_lead_byte(nc)) {
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
		if (IS_PC98_ARCH && shiftjis_lead_byte(nc)) {
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
	*change=(Bit8u)(string-string_begin);
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
    Bit32u size;Bit16u date;Bit16u time;Bit8u attr;Bit8u drive;
	char file_name[9];char ext[4];
	find_dta.GetResult(name,lname,size,date,time,attr);
	drive=find_fcb.GetDrive()+1;
	Bit8u find_attr = DOS_ATTR_ARCHIVE;
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

bool DOS_FCBCreate(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	Bit8u attr = DOS_ATTR_ARCHIVE;
	fcb.GetAttr(attr);
	if (!attr) attr = DOS_ATTR_ARCHIVE; //Better safe than sorry 

    if (attr & DOS_ATTR_VOLUME) {
	    fcb.GetVolumeName(shortname);
        return Drives[fcb.GetDrive()]->FileCreate(NULL,shortname,attr);
    }

	fcb.GetName(shortname);
	if (!DOS_CreateFile(shortname,attr,&handle,true)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBOpen(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	fcb.GetName(shortname);

	/* Search for file if name has wildcards */
	if (strpbrk(shortname,"*?")) {
		LOG(LOG_FCB,LOG_WARN)("Wildcards in filename");
		if (!DOS_FCBFindFirst(seg,offset)) return false;
		DOS_DTA find_dta(dos.tables.tempdta);
		DOS_FCB find_fcb(RealSeg(dos.tables.tempdta),RealOff(dos.tables.tempdta));
		char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH],file_name[9],ext[4];
		Bit32u size;Bit16u date,time;Bit8u attr;
		find_dta.GetResult(name,lname,size,date,time,attr);
		DTAExtendName(name,file_name,ext);
		find_fcb.SetName(fcb.GetDrive()+1,file_name,ext);
		find_fcb.GetName(shortname);
	}

	/* First check if the name is correct */
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(shortname,fullname,&drive)) return false;
	
	/* Check, if file is already opened */
	for (Bit8u i = 0;i < DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->IsName(fullname)) {
			Files[i]->AddRef();
			fcb.FileOpen(i);
			return true;
		}
	}
	
	if (!DOS_OpenFile(shortname,OPEN_READWRITE,&handle,true)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBClose(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	if(!fcb.Valid()) return false;
	Bit8u fhandle;
	fcb.FileClose(fhandle);
	DOS_CloseFile(fhandle,true);
	return true;
}

bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta);
	char name[DOS_FCBNAME];fcb.GetName(name);
	Bit8u attr = DOS_ATTR_ARCHIVE;
	fcb.GetAttr(attr); /* Gets search attributes if extended */
	bool ret=DOS_FindFirst(name,attr,true);
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

bool DOS_FCBFindNext(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta);
	bool ret=DOS_FindNext();
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset,Bit16u recno) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
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
	Bit32u pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_READ_NODATA; 
	Bit16u toread=rec_size;
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

Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u recno) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
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
	Bit32u pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	MEM_BlockRead(Real2Phys(dos.dta())+(PhysPt)(recno*rec_size),dos_copybuf,rec_size);
	Bit16u towrite=rec_size;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	Bit32u size;Bit16u date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	Bit32u ticks = mem_readd(BIOS_TIMER);
	Bit32u seconds = (ticks*10u)/182u;
	Bit16u hour = (Bit16u)(seconds/3600u);
	Bit16u min = (Bit16u)((seconds % 3600u)/60u);
	Bit16u sec = (Bit16u)(seconds % 60u);
	time = DOS_PackTime(hour,min,sec);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	if (++cur_rec>127u) { cur_block++;cur_rec=0; }	
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

Bit8u DOS_FCBIncreaseSize(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128u)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	Bit16u towrite=0;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	Bit32u size;Bit16u date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	Bit32u ticks = mem_readd(BIOS_TIMER);
	Bit32u seconds = (ticks*10u)/182u;
	Bit16u hour = (Bit16u)(seconds/3600u);
	Bit16u min = (Bit16u)((seconds % 3600u)/60u);
	Bit16u sec = (Bit16u)(seconds % 60u);
	time = DOS_PackTime(hour,min,sec);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore) {
/* if restore is true :random read else random blok read. 
 * random read updates old block and old record to reflect the random data
 * before the read!!!!!!!!! and the random data is not updated! (user must do this)
 * Random block read updates these fields to reflect the state after the read!
 */
	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block=0;
	Bit8u old_rec=0;
	Bit8u error=0;
	Bit16u count;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128u),(Bit8u)(random & 127u));
	if (restore) fcb.GetRecord(old_block,old_rec);//store this for after the read.
	// Read records
	for (count=0; count<*numRec; count++) {
		error = DOS_FCBRead(seg,offset,count);
		if (error!=FCB_SUCCESS) break;
	}
	if (error==FCB_READ_PARTIAL) count++;	//partial read counts
	*numRec=count;
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false*/
	if(!restore) fcb.SetRandom(new_block*128u+new_rec); 
	return error;
}

Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u * numRec,bool restore) {
/* see FCB_RandomRead */
	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block=0;
	Bit8u old_rec=0;
	Bit8u error=0;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128u),(Bit8u)(random & 127u));
	if (restore) fcb.GetRecord(old_block,old_rec);
	if (*numRec > 0) {
		Bit16u count;
		/* Write records */
		for (count=0; count<*numRec; count++) {
			error = DOS_FCBWrite(seg,offset,count);// dos_fcbwrite return 0 false when true...
			if (error!=FCB_SUCCESS) break;
		}
		*numRec=count;
	} else {
		DOS_FCBIncreaseSize(seg,offset);
	}
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false */
	if (!restore) fcb.SetRandom(new_block*128u+new_rec); 
	return error;
}

bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset) {
	char shortname[DOS_PATHLENGTH];Bit16u entry;
	DOS_FCB fcb(seg,offset);
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,OPEN_READ,&entry,true)) return false;
	Bit32u size = 0;
	Files[entry]->Seek(&size,DOS_SEEK_END);
	DOS_CloseFile(entry,true);

	Bit8u handle; Bit16u rec_size;
	fcb.GetSeqData(handle,rec_size);
	if (rec_size == 0) rec_size = 128; //Use default if missing.

	Bit32u random=(size/rec_size);
	if (size % rec_size) random++;
	fcb.SetRandom(random);
	return true;
}

bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset){
/* Special case: ????????.??? and DOS_ATTR_VOLUME */
    {
        DOS_FCB fcb(seg,offset);
        Bit8u attr = 0;
        fcb.GetAttr(attr);
        Bit8u drive = fcb.GetDrive();
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

bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset){
	DOS_FCB fcbold(seg,offset);
    DOS_FCB fcbnew(seg,offset);
    fcbnew.SetPtPhys(fcbnew.GetPtPhys()+0x10u);//HACK: FCB NEW memory offset is affected by whether FCB OLD is extended
    if(!fcbold.Valid()) return false;
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);fcbnew.GetName(newname);

    {
        Bit8u drive = fcbold.GetDrive();
        std::string label = Drives[drive]->GetLabel();
        Bit8u attr = 0;

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
	Bit8u drive; char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullname,&drive)) return false;
	
	DOS_PSP psp(dos.psp());
	for (Bit8u i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->IsName(fullname)) {
			Bit16u handle = psp.FindEntryByHandle(i);
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

void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit16u block;Bit8u rec;
	fcb.GetRecord(block,rec);
	fcb.SetRandom(block*128u+rec);
}


bool DOS_FileExists(char const * const name) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileExists(fullname);
}

bool DOS_GetAllocationInfo(Bit8u drive,Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters) {
	if (!drive) drive =  DOS_GetDefaultDrive();
	else drive--;
	if (drive >= DOS_DRIVES || !Drives[drive]) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	Bit16u _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector,_sectors_cluster,_total_clusters,&_free_clusters);
	SegSet16(ds,RealSeg(dos.tables.mediaid));
	reg_bx=RealOff(dos.tables.mediaid+drive*dos.tables.dpb_size);
	return true;
}

bool DOS_SetDrive(Bit8u drive) {
	if (Drives[drive]) {
		DOS_SetDefaultDrive(drive);
		return true;
	} else {
		return false;
	}
}

bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate) {
	Bit32u handle=RealHandle(entry);
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

bool DOS_SetFileDate(Bit16u entry, Bit16u ntime, Bit16u ndate)
{
	Bit32u handle=RealHandle(entry);
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

void DOS_SetupFiles (void) {
	/* Setup the File Handles */
	Files = new DOS_File * [DOS_FILES];
	Bit32u i;
	for (i=0;i<DOS_FILES;i++) {
		Files[i]=0;
	}
	/* Setup the Virtual Disk System */
	for (i=0;i<DOS_DRIVES;i++) {
		Drives[i]=0;
	}
	Drives[25]=new Virtual_Drive();
}

