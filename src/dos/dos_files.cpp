/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

/* $Id: dos_files.cpp,v 1.113 2009-08-31 18:03:08 qbix79 Exp $ */

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

#define DOS_FILESTART 4

#define FCB_SUCCESS     0
#define FCB_READ_NODATA	1
#define FCB_READ_PARTIAL 3
#define FCB_ERR_NODATA  1
#define FCB_ERR_EOF     3
#define FCB_ERR_WRITE   1


DOS_File * Files[DOS_FILES];
DOS_Drive * Drives[DOS_DRIVES];

Bit8u DOS_GetDefaultDrive(void) {
//	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	Bit8u d = DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	if( d != dos.current_drive ) LOG(LOG_DOSMISC,LOG_ERROR)("SDA drive %d not the same as dos.current_drive %d",d,dos.current_drive);
	return dos.current_drive;
}

void DOS_SetDefaultDrive(Bit8u drive) {
//	if (drive<=DOS_DRIVES && ((drive<2) || Drives[drive])) DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);
	if (drive<=DOS_DRIVES && ((drive<2) || Drives[drive])) {dos.current_drive = drive; DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);}
}

bool DOS_MakeName(char const * const name,char * const fullname,Bit8u * drive) {
	if(!name || *name == 0 || *name == ' ') {
		/* Both \0 and space are seperators and
		 * empty filenames report file not found */
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	const char * name_int = name;
	char tempdir[DOS_PATHLENGTH];
	char upname[DOS_PATHLENGTH];
	Bitu r,w;
	*drive = DOS_GetDefaultDrive();
	/* First get the drive */
	if (name_int[1]==':') {
		*drive=(name_int[0] | 0x20)-'a';
		name_int+=2;
	}
	if (*drive>=DOS_DRIVES || !Drives[*drive]) { 
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false; 
	}
	r=0;w=0;
	while (name_int[r]!=0 && (r<DOS_PATHLENGTH)) {
		Bit8u c=name_int[r++];
		if ((c>='a') && (c<='z')) {upname[w++]=c-32;continue;}
		if ((c>='A') && (c<='Z')) {upname[w++]=c;continue;}
		if ((c>='0') && (c<='9')) {upname[w++]=c;continue;}
		switch (c) {
		case '/':
			upname[w++]='\\';
			break;
		case ' ': /* should be seperator */
			break;
		case '\\':	case '$':	case '#':	case '@':	case '(':	case ')':
		case '!':	case '%':	case '{':	case '}':	case '`':	case '~':
		case '_':	case '-':	case '.':	case '*':	case '?':	case '&':
		case '\'':	case '+':	case '^':	case 246:	case 255:	case 0xa0:
		case 0xe5:	case 0xbd:
			upname[w++]=c;
			break;
		default:
			LOG(LOG_FILES,LOG_NORMAL)("Makename encountered an illegal char %c hex:%X in %s!",c,c,name);
			DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			break;
		}
	}
	if (r>=DOS_PATHLENGTH) { DOS_SetError(DOSERR_PATH_NOT_FOUND);return false; }
	upname[w]=0;
	/* Now parse the new file name to make the final filename */
	if (upname[0]!='\\') strcpy(fullname,Drives[*drive]->curdir);
	else fullname[0]=0;
	Bit32u lastdir=0;Bit32u t=0;
	while (fullname[t]!=0) {
		if ((fullname[t]=='\\') && (fullname[t+1]!=0)) lastdir=t;
		t++;
	};
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
						lastdir = iDown;
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
			} else tempdir[8]=0;

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

bool DOS_GetCurrentDir(Bit8u drive,char * const buffer) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	strcpy(buffer,Drives[drive]->curdir);
	return true;
}

bool DOS_ChangeDir(char const * const dir) {
	Bit8u drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;
	
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
	char currdir[DOS_PATHLENGTH]= { 0 };
	DOS_GetCurrentDir(drive + 1 ,currdir);
	if(strcmp(currdir,fulldir) == 0) {
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
	if ( (DOS_FindDevice(oldname) != DOS_DEVICES) ||
	     (DOS_FindDevice(newname) != DOS_DEVICES) ) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
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

	if (Drives[drivenew]->Rename(fullold,fullnew)) return true;
	/* If it still fails. which error should we give ? PATH NOT FOUND or EACCESS */
	LOG(LOG_FILES,LOG_NORMAL)("Rename fails for %s to %s, no proper errorcode returned.",oldname,newname);
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

bool DOS_FindFirst(char * search,Bit16u attr,bool fcb_findfirst) {
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

	dta.SetupSearch(drive,(Bit8u)attr,pattern);

	if(device) {
		find_last = strrchr(pattern,'.');
		if(find_last) *find_last = 0;
		//TODO use current date and time
		dta.SetResult(pattern,0,0,0,DOS_ATTR_DEVICE);
		LOG(LOG_DOSMISC,LOG_WARN)("finding device %s",pattern);
		return true;
	}
   
	if (Drives[drive]->FindFirst(dir,dta,fcb_findfirst)) return true;
	
	return false;
}

bool DOS_FindNext(void) {
	DOS_DTA dta(dos.dta());
	Bit8u i = dta.GetSearchDrive();
	if(i >= DOS_DRIVES || !Drives[i]) {
		/* Corrupt search. */
		LOG(LOG_FILES,LOG_ERROR)("Corrupt search!!!!");
		DOS_SetError(DOSERR_NO_MORE_FILES); 
		return false;
	} 
	if (Drives[i]->FindNext(dta)) return true;
	return false;
}


bool DOS_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
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

bool DOS_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
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

bool DOS_SeekFile(Bit16u entry,Bit32u * pos,Bit32u type) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	return Files[handle]->Seek(pos,type);
}

bool DOS_CloseFile(Bit16u entry) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (Files[handle]->IsOpen()) {
		Files[handle]->Close();
	}
	DOS_PSP psp(dos.psp());
	psp.SetFileHandle(entry,0xff);
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
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	LOG(LOG_DOSMISC,LOG_NORMAL)("FFlush used.");
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


bool DOS_CreateFile(char const * name,Bit16u attributes,Bit16u * entry) {
	// Creation of a device is the same as opening it
	// Tc201 installer
	if (DOS_FindDevice(name) != DOS_DEVICES)
		return DOS_OpenFile(name, OPEN_READ, entry);

	LOG(LOG_FILES,LOG_NORMAL)("file create attributes %X file %s",attributes,name);
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	DOS_PSP psp(dos.psp());
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	/* Check for a free file handle */
	Bit8u handle=DOS_FILES;Bit8u i;
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
	*entry = psp.FindFreeFileEntry();
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
		Files[handle]->SetDrive(drive);
		Files[handle]->AddRef();
		psp.SetFileHandle(*entry,handle);
		return true;
	} else {
		if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
		else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_OpenFile(char const * name,Bit8u flags,Bit16u * entry) {
	/* First check for devices */
	if (flags>2) LOG(LOG_FILES,LOG_ERROR)("Special file open command %X file %s",flags,name);
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
	*entry = psp.FindFreeFileEntry();

	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	bool exists=false;
	if (device) {
		Files[handle]=new DOS_Device(*Devices[devnum]);
	} else {
		exists=Drives[drive]->FileOpen(&Files[handle],fullname,flags);
		if (exists) Files[handle]->SetDrive(drive);
	}
	if (exists || device ) { 
		Files[handle]->AddRef();
		psp.SetFileHandle(*entry,handle);
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
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	if(Drives[drive]->FileUnlink(fullname)){
		return true;
	} else {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_GetFileAttr(char const * const name,Bit16u * attr) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	if (Drives[drive]->GetFileAttr(fullname,attr)) {
		return true;
	} else {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_SetFileAttr(char const * const name,Bit16u /*attr*/) 
// this function does not change the file attributs
// it just does some tests if file is available 
// returns false when using on cdrom (stonekeep)
{
	Bit16u attrTemp;
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;	
	if (strncmp(Drives[drive]->GetInfo(),"CDRom ",6)==0 || strncmp(Drives[drive]->GetInfo(),"isoDrive ",9)==0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return Drives[drive]->GetFileAttr(fullname,&attrTemp);
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

bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit8u * sectors,Bit16u * clusters,Bit16u * free) {
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	return Drives[drive]->AllocationInfo(bytes,sectors,clusters,free);
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
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
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
	};
	if (!Files[orig] || !Files[orig]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
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

#define FCB_SEP ":.;,=+"
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
	char * string_begin=string;
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
	/* Strip of the leading sepetaror */
	if((parser & PARSE_SEP_STOP) && *string)  {       //ignore leading seperator
		char sep[] = FCB_SEP;char a[2];
		a[0]= *string;a[1]='\0';
		if (strcspn(a,sep)==0) string++;
	} 
	/* strip leading spaces */
	while((*string==' ')||(*string=='\t')) string++;
	/* Check for a drive */
	if (string[1]==':') {
		fcb_name.part.drive[0]=0;
		hasdrive=true;
		if (isalpha(string[0]) && Drives[toupper(string[0])-'A']) {
			fcb_name.part.drive[0]=(char)(toupper(string[0])-'A'+1);
		} else ret=0xff;
		string+=2;
	}
	/* Special checks for . and .. */
	if (string[0]=='.') {
		string++;
		if (!string[0])	{
			hasname=true;
			ret=PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name,".       ");
			goto savefcb;
		}
		if (string[1]=='.' && !string[1])	{
			string++;
			hasname=true;
			ret=PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name,"..      ");
			goto savefcb;
		}
		goto checkext;
	}
	/* Copy the name */	
	hasname=true;finished=false;fill=' ';index=0;
	while (index<8) {
		if (!finished) {
			if (string[0]=='*') {fill='?';fcb_name.part.name[index]='?';if (!ret) ret=1;finished=true;}
			else if (string[0]=='?') {fcb_name.part.name[index]='?';if (!ret) ret=1;}
			else if (isvalid(string[0])) {fcb_name.part.name[index]=(char)(toupper(string[0]));}
			else { finished=true;continue; }
			string++;
		} else {
			fcb_name.part.name[index]=fill;
		}
		index++;
	}
	if (!(string[0]=='.')) goto savefcb;
	string++;
checkext:
	/* Copy the extension */
	hasext=true;finished=false;fill=' ';index=0;
	while (index<3) {
		if (!finished) {
			if (string[0]=='*') {fill='?';fcb_name.part.ext[index]='?';finished=true;}
			else if (string[0]=='?') {fcb_name.part.ext[index]='?';if (!ret) ret=1;}
			else if (isvalid(string[0])) {fcb_name.part.ext[index]=(char)(toupper(string[0]));}
			else { finished=true;continue; }
			string++;
		} else {
			fcb_name.part.ext[index]=fill;
		}
		index++;
	}
savefcb:
	if (!hasdrive & !(parser & PARSE_DFLT_DRIVE)) fcb_name.part.drive[0] = 0;
	if (!hasname & !(parser & PARSE_BLNK_FNAME)) strcpy(fcb_name.part.name,"        ");
	if (!hasext & !(parser & PARSE_BLNK_FEXT)) strcpy(fcb_name.part.ext,"   ");
	fcb.SetName(fcb_name.part.drive[0],fcb_name.part.name,fcb_name.part.ext);
	*change=(Bit8u)(string-string_begin);
	return ret;
}

static void DTAExtendName(char * const name,char * const filename,char * const ext) {
	char * find=strchr(name,'.');
	if (find) {
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
	char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;Bit8u drive;
	char file_name[9];char ext[4];
	find_dta.GetResult(name,size,date,time,attr);
	drive=find_fcb.GetDrive()+1;
	/* Create a correct file and extention */
	DTAExtendName(name,file_name,ext);	
	DOS_FCB fcb(RealSeg(dos.dta()),RealOff(dos.dta()));//TODO
	fcb.Create(find_fcb.Extended());
	fcb.SetName(drive,file_name,ext);
	fcb.SetAttr(attr);      /* Only adds attribute if fcb is extended */
	fcb.SetSizeDateTime(size,date,time);
}

bool DOS_FCBCreate(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	fcb.GetName(shortname);
	if (!DOS_CreateFile(shortname,DOS_ATTR_ARCHIVE,&handle)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBOpen(Bit16u seg,Bit16u offset) { 
	DOS_FCB fcb(seg,offset);
	char shortname[DOS_FCBNAME];Bit16u handle;
	fcb.GetName(shortname);

	/* First check if the name is correct */
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(shortname,fullname,&drive)) return false;
	
	/* Check, if file is already opened */
	for (Bit8u i=0;i<DOS_FILES;i++) {
		DOS_PSP psp(dos.psp());
		if (Files[i] && Files[i]->IsOpen() && Files[i]->IsName(fullname)) {
			handle = psp.FindEntryByHandle(i);
			if (handle==0xFF) {
				// This shouldnt happen
				LOG(LOG_FILES,LOG_ERROR)("DOS: File %s is opened but has no psp entry.",shortname);
				return false;
			}
			fcb.FileOpen((Bit8u)handle);
			return true;
		}
	}
	
	if (!DOS_OpenFile(shortname,OPEN_READWRITE,&handle)) return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
}

bool DOS_FCBClose(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	if(!fcb.Valid()) return false;
	Bit8u fhandle;
	fcb.FileClose(fhandle);
	DOS_CloseFile(fhandle);
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
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET)) return FCB_READ_NODATA; 
	Bit16u toread=rec_size;
	if (!DOS_ReadFile(fhandle,dos_copybuf,&toread)) return FCB_READ_NODATA;
	if (toread==0) return FCB_READ_NODATA;
	if (toread < rec_size) { //Zero pad copybuffer to rec_size
		Bitu i = toread;
		while(i < rec_size) dos_copybuf[i++] = 0;
	}
	MEM_BlockWrite(Real2Phys(dos.dta())+recno*rec_size,dos_copybuf,rec_size);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }
	fcb.SetRecord(cur_block,cur_rec);
	if (toread==rec_size) return FCB_SUCCESS;
	if (toread==0) return FCB_READ_NODATA;
	return FCB_READ_PARTIAL;
}

Bit8u DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u recno) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET)) return FCB_ERR_WRITE; 
	MEM_BlockRead(Real2Phys(dos.dta())+recno*rec_size,dos_copybuf,rec_size);
	Bit16u towrite=rec_size;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite)) return FCB_ERR_WRITE;
	Bit32u size;Bit16u date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	Bit32u ticks = mem_readd(BIOS_TIMER);
	Bit32u seconds = (ticks*10)/182;
	Bit16u hour = (Bit16u)(seconds/3600);
	Bit16u min = (Bit16u)((seconds % 3600)/60);
	Bit16u sec = (Bit16u)(seconds % 60);
	time = DOS_PackTime(hour,min,sec);
	Bit8u temp=RealHandle(fhandle);
	Files[temp]->time=time;
	Files[temp]->date=date;
	fcb.SetSizeDateTime(size,date,time);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }	
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

Bit8u DOS_FCBIncreaseSize(Bit16u seg,Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle,cur_rec;Bit16u cur_block,rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET)) return FCB_ERR_WRITE; 
	Bit16u towrite=0;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite)) return FCB_ERR_WRITE;
	Bit32u size;Bit16u date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	Bit32u ticks = mem_readd(BIOS_TIMER);
	Bit32u seconds = (ticks*10)/182;
	Bit16u hour = (Bit16u)(seconds/3600);
	Bit16u min = (Bit16u)((seconds % 3600)/60);
	Bit16u sec = (Bit16u)(seconds % 60);
	time = DOS_PackTime(hour,min,sec);
	Bit8u temp=RealHandle(fhandle);
	Files[temp]->time=time;
	Files[temp]->date=date;
	fcb.SetSizeDateTime(size,date,time);
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore) {
/* if restore is true :random read else random blok read. 
 * random read updates old block and old record to reflect the random data
 * before the read!!!!!!!!! and the random data is not updated! (user must do this)
 * Random block read updates these fields to reflect the state after the read!
 */

/* BUG: numRec should return the amount of records read! 
 * Not implemented yet as I'm unsure how to count on error states (partial/failed) 
 */

	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block=0;
	Bit8u old_rec=0;
	Bit8u error=0;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128),(Bit8u)(random & 127));
	if (restore) fcb.GetRecord(old_block,old_rec);//store this for after the read.
	// Read records
	for (int i=0; i<numRec; i++) {
		error = DOS_FCBRead(seg,offset,(Bit16u)i);
		if (error!=0x00) break;
	}
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false*/
	if(!restore) fcb.SetRandom(new_block*128+new_rec); 
	return error;
}

Bit8u DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u numRec,bool restore) {
/* see FCB_RandomRead */
	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block=0;
	Bit8u old_rec=0;
	Bit8u error=0;

	/* Set the correct record from the random data */
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128),(Bit8u)(random & 127));
	if (restore) fcb.GetRecord(old_block,old_rec);
	if (numRec>0) {
		/* Write records */
		for (int i=0; i<numRec; i++) {
			error = DOS_FCBWrite(seg,offset,(Bit16u)i);// dos_fcbwrite return 0 false when true...
			if (error!=0x00) break;
		}
	} else {
		DOS_FCBIncreaseSize(seg,offset);
	}
	Bit16u new_block;Bit8u new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false */
	if(!restore) fcb.SetRandom(new_block*128+new_rec); 
	return error;
}

bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset) {
	char shortname[DOS_PATHLENGTH];Bit16u entry;Bit8u handle;Bit16u rec_size;
	DOS_FCB fcb(seg,offset);
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,OPEN_READ,&entry)) return false;
	handle = RealHandle(entry);
	Bit32u size = 0;
	Files[handle]->Seek(&size,DOS_SEEK_END);
	DOS_CloseFile(entry);fcb.GetSeqData(handle,rec_size);
	Bit32u random=(size/rec_size);
	if (size % rec_size) random++;
	fcb.SetRandom(random);
	return true;
}

bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset){
/* FCB DELETE honours wildcards. it will return true if one or more
 * files get deleted. 
 * To get this: the dta is set to temporary dta in which found files are
 * stored. This can not be the tempdta as that one is used by fcbfindfirst
 */
	RealPt old_dta=dos.dta();dos.dta(dos.tables.tempdta_fcbdelete);
	DOS_FCB fcb(RealSeg(dos.dta()),RealOff(dos.dta()));
	bool nextfile = false;
	bool return_value = false;
	nextfile = DOS_FCBFindFirst(seg,offset);
	while(nextfile) {
		char shortname[DOS_FCBNAME] = { 0 };
		fcb.GetName(shortname);
		bool res=DOS_UnlinkFile(shortname);
		if(!return_value && res) return_value = true; //at least one file deleted
		nextfile = DOS_FCBFindNext(seg,offset);
	}
	dos.dta(old_dta);  /*Restore dta */
	return  return_value;
}

bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset){
	DOS_FCB fcbold(seg,offset);
	DOS_FCB fcbnew(seg,offset+16);
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);fcbnew.GetName(newname);
	return DOS_Rename(oldname,newname);
}

void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset) {
	DOS_FCB fcb(seg,offset);
	Bit16u block;Bit8u rec;
	fcb.GetRecord(block,rec);
	fcb.SetRandom(block*128+rec);
}


bool DOS_FileExists(char const * const name) {
	char fullname[DOS_PATHLENGTH];Bit8u drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives[drive]->FileExists(fullname);
}

bool DOS_GetAllocationInfo(Bit8u drive,Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters) {
	if (!drive) drive =  DOS_GetDefaultDrive();
	else drive--;
	if (drive >= DOS_DRIVES || !Drives[drive]) return false;
	Bit16u _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector,_sectors_cluster,_total_clusters,&_free_clusters);
	SegSet16(ds,RealSeg(dos.tables.mediaid));
	reg_bx=RealOff(dos.tables.mediaid+drive*2);
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
	};
	if (!Files[handle] || !Files[handle]->IsOpen()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]->UpdateDateTimeFromHost()) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false; 
	}
	*otime = Files[handle]->time;
	*odate = Files[handle]->date;
	return true;
}

void DOS_SetupFiles (void) {
	/* Setup the File Handles */
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
