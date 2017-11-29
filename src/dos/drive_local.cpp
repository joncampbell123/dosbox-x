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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"
#include "inout.h"
#ifndef WIN32
#include <utime.h>
#else
#include <sys/utime.h>
#include <sys/locking.h>
#endif

class localFile : public DOS_File {
public:
	localFile(const char* name, FILE * handle);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
#ifdef WIN32
	bool LockFile(Bit8u mode, Bit32u pos, Bit16u size);
#endif
	Bit16u GetInformation(void);
	bool UpdateDateTimeFromHost(void);   
	void FlagReadOnlyMedium(void);
	void Flush(void);
	Bit32u GetSeekPos(void);
private:
	FILE * fhandle;
	bool read_only_medium;
	enum { NONE,READ,WRITE } last_action;
};

#include "cp437_uni.h"
#include "cp932_uni.h"

static char cpcnv_temp[4096];

template <class MT> bool String_SBCS_TO_HOST(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const char *sf = s + CROSS_LEN - 1;
    char *df = d + CROSS_LEN - 1;
    unsigned char ic;
    MT wc;

    while (*s != 0 && s < sf) {
        ic = (unsigned char)(*s++);
        if (ic >= map_max) return false; // non-representable
        wc = map[ic]; // output: unicode character

        if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_DBCS_TO_HOST_SHIFTJIS(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const char *sf = s + CROSS_LEN - 1;
    char *df = d + CROSS_LEN - 1;
    uint16_t ic;
    MT rawofs;
    MT wc;

    while (*s != 0 && s < sf) {
        ic = (unsigned char)(*s++);
        if ((ic & 0xE0) == 0x80 || (ic & 0xE0) == 0xE0) {
            if (*s == 0) return false;
            ic <<= 8U;
            ic += (unsigned char)(*s++);
        }

        rawofs = hitbl[ic >> 6];
        if (rawofs == 0xFFFF)
            return false;

        assert((size_t)(rawofs+0x40) <= rawtbl_max);
        wc = rawtbl[rawofs + (ic & 0x3F)];
        if (wc == 0x0000)
            return false;

        if (utf8_encode(&d,df,(uint32_t)wc) < 0) // will advance d by however many UTF-8 bytes are needed
            return false; // non-representable, or probably just out of room
    }

    assert(d <= df);
    *d = 0;

    return true;
}

// TODO: This is SLOW. Optimize.
template <class MT> int SBCS_From_Host_Find(int c,const MT *map,const size_t map_max) {
    for (size_t i=0;i < map_max;i++) {
        if ((MT)c == map[i])
            return i;
    }

    return -1;
}

// TODO: This is SLOW. Optimize.
template <class MT> int DBCS_SHIFTJIS_From_Host_Find(int c,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    for (size_t h=0;h < 1024;h++) {
        MT ofs = hitbl[h];

        if (ofs == 0xFFFF) continue;
        assert((size_t)(ofs+0x40) <= rawtbl_max);

        for (size_t l=0;l < 0x40;l++) {
            if ((MT)c == rawtbl[ofs+l])
                return (h << 6) + l;
        }
    }

    return -1;
}

template <class MT> bool String_HOST_TO_DBCS_SHIFTJIS(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *hitbl,const MT *rawtbl,const size_t rawtbl_max) {
    const char *sf = s + CROSS_LEN - 1;
    char *df = d + CROSS_LEN - 1;
    int ic;
    int oc;

    while (*s != 0 && s < sf) {
        const char *is = s;

        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable

        oc = DBCS_SHIFTJIS_From_Host_Find<MT>(ic,hitbl,rawtbl,rawtbl_max);
        if (oc < 0)
            return false; // non-representable

        {
            char tmp[16];
            memcpy(tmp,is,(size_t)(s-is));
            tmp[(size_t)(s-is)] = 0;
        }

        if (oc >= 0x100) {
            if ((d+1) >= df) return false;
            *d++ = (unsigned char)(oc >> 8U);
            *d++ = (unsigned char)oc;
        }
        else {
            if (d >= df) return false;
            *d++ = (unsigned char)oc;
        }
    }

    assert(d <= df);
    *d = 0;

    return true;
}

template <class MT> bool String_HOST_TO_SBCS(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/,const MT *map,const size_t map_max) {
    const char *sf = s + CROSS_LEN - 1;
    char *df = d + CROSS_LEN - 1;
    int ic;
    int oc;

    while (*s != 0 && s < sf) {
        if ((ic=utf8_decode(&s,sf)) < 0)
            return false; // non-representable

        oc = SBCS_From_Host_Find<MT>(ic,map,map_max);
        if (oc < 0)
            return false; // non-representable

        if (d >= df) return false;
        *d++ = (unsigned char)oc;
    }

    assert(d <= df);
    *d = 0;

    return true;
}

bool cpwarn_once = false;

bool CodePageHostToGuest(char *d/*CROSS_LEN*/,char *s/*CROSS_LEN*/) {
    switch (dos.loaded_codepage) {
        case 437:
            return String_HOST_TO_SBCS<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 932:
            return String_HOST_TO_DBCS_SHIFTJIS<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        default:
            /* at this time, it would be cruel and unusual to not allow any file I/O just because
             * our code page support is so limited. */
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to guest) for code page %u",dos.loaded_codepage);
            }
            strcpy(d,s);
            return true;
    }

    return false;
}

bool CodePageGuestToHost(char *d/*CROSS_LEN*/,char *s/*CROSS_LEN*/) {
    switch (dos.loaded_codepage) {
        case 437:
            return String_SBCS_TO_HOST<uint16_t>(d,s,cp437_to_unicode,sizeof(cp437_to_unicode)/sizeof(cp437_to_unicode[0]));
        case 932:
            return String_DBCS_TO_HOST_SHIFTJIS<uint16_t>(d,s,cp932_to_unicode_hitbl,cp932_to_unicode_raw,sizeof(cp932_to_unicode_raw)/sizeof(cp932_to_unicode_raw[0]));
        default:
            /* at this time, it would be cruel and unusual to not allow any file I/O just because
             * our code page support is so limited. */
            if (!cpwarn_once) {
                cpwarn_once = true;
                LOG_MSG("WARNING: No translation support (to host) for code page %u",dos.loaded_codepage);
            }
            strcpy(d,s);
            return true;
    }

    return false;
}

char *CodePageGuestToHost(char *s) {
    if (!CodePageGuestToHost(cpcnv_temp,s))
        return NULL;

    return cpcnv_temp;
}

char *CodePageHostToGuest(char *s) {
    if (!CodePageHostToGuest(cpcnv_temp,s))
        return NULL;

    return cpcnv_temp;
}

bool localDrive::FileCreate(DOS_File * * file,const char * name,Bit16u /*attributes*/) {
//TODO Maybe care for attributes but not likely
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	char* temp_name = dirCache.GetExpandName(newname); //Can only be used in till a new drive_cache action is preformed */
	/* Test if file exists (so we need to truncate it). don't add to dirCache then */
	bool existing_file=false;

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(temp_name);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    temp_name = n_temp_name;

    FILE * test=fopen(temp_name,"rb+");
	if(test) {
		fclose(test);
		existing_file=true;

	}
	
	FILE * hand=fopen(temp_name,"wb+");
	if (!hand){
		LOG_MSG("Warning: file creation failed: %s",newname);
		return false;
	}
   
	if(!existing_file) {
		strcpy(newname,basedir);
		strcat(newname,name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
	}

	/* Make the 16 bit device information */
	*file=new localFile(name,hand);
	(*file)->flags=OPEN_READWRITE;

	return true;
}

bool localDrive::FileOpen(DOS_File * * file,const char * name,Bit32u flags) {
	const char* type;
	switch (flags&0xf) {
	case OPEN_READ:        type = "rb" ; break;
	case OPEN_WRITE:       type = "rb+"; break;
	case OPEN_READWRITE:   type = "rb+"; break;
	case OPEN_READ_NO_MOD: type = "rb" ; break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	Bit8u i,drive=DOS_DRIVES;
	localFile *lfp;
	for (i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]==this) {
			drive=i;
			break;
		}
	}
	for (i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			lfp=dynamic_cast<localFile*>(Files[i]);
			if (lfp) lfp->Flush();
		}
	}

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    strcpy(newname,n_temp_name);

	FILE * hand=fopen(newname,type);
//	Bit32u err=errno;
	if (!hand) { 
		if((flags&0xf) != OPEN_READ) {
			FILE * hmm=fopen(newname,"rb");
			if (hmm) {
				fclose(hmm);
				LOG_MSG("Warning: file %s exists and failed to open in write mode.\nPlease Remove write-protection",newname);
			}
		}
		return false;
	}

	*file=new localFile(name,hand);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
//	(*file)->SetFileName(newname);
	return true;
}

FILE * localDrive::GetSystemFilePtr(char const * const name, char const * const type) {

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    strcpy(newname,n_temp_name);

	return fopen(newname,type);
}

bool localDrive::GetSystemFilename(char *sysName, char const * const dosName) {

	strcpy(sysName, basedir);
	strcat(sysName, dosName);
	CROSS_FILENAME(sysName);
	dirCache.ExpandName(sysName);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(sysName);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,sysName);
        return false;
    }
    strcpy(sysName,n_temp_name);

	return true;
}

bool localDrive::FileUnlink(const char * name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	char *fullname = dirCache.GetExpandName(newname);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(fullname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,fullname);
        return false;
    }
    strcpy(fullname,n_temp_name);

	if (unlink(fullname)) {
		//Unlink failed for some reason try finding it.
		struct stat buffer;
		if(stat(fullname,&buffer)) return false; // File not found.

		FILE* file_writable = fopen(fullname,"rb+");
		if(!file_writable) return false; //No acces ? ERROR MESSAGE NOT SET. FIXME ?
		fclose(file_writable);

		//File exists and can technically be deleted, nevertheless it failed.
		//This means that the file is probably open by some process.
		//See if We have it open.
		bool found_file = false;
		for(Bitu i = 0;i < DOS_FILES;i++){
			if(Files[i] && Files[i]->IsName(name)) {
				Bitu max = DOS_FILES;
				while(Files[i]->IsOpen() && max--) {
					Files[i]->Close();
					if (Files[i]->RemoveRef()<=0) break;
				}
				found_file=true;
			}
		}
		if(!found_file) return false;
		if (!unlink(fullname)) {
			dirCache.DeleteEntry(newname);
			return true;
		}
		return false;
	} else {
		dirCache.DeleteEntry(newname);
		return true;
	}
}

bool localDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	char tempDir[CROSS_LEN];
	strcpy(tempDir,basedir);
	strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid==0xF0 ) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}
    
	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir)-1]!=CROSS_FILESPLIT) strcat(tempDir,end);
	
	Bit16u id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	strcpy(srchInfo[id].srch_dir,tempDir);
	dta.SetDirID(id);
	
	Bit8u sAttr;
	dta.GetSearchParams(sAttr,tempDir);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (sAttr == DOS_ATTR_VOLUME) {
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	} else {
		if (sAttr == DOS_ATTR_VOLUME) {
			if ( strcmp(dirCache.GetLabel(), "") == 0 ) {
//				LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
//				dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
//				return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
		//should check for a valid leading directory instead of 0
		//exists==true if the volume label matches the searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
				dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
				return true;
			}
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	Bit16u id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if(!WildFileCmp(dir_ent,srch_pattern)) goto again;

	strcpy(full_name,srchInfo[id].srch_dir);
	strcat(full_name,dir_ent);
	
	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	strcpy(dir_entcopy,dir_ent);

    char *temp_name = dirCache.GetExpandName(full_name);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(temp_name);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,temp_name);
		goto again;//No symlinks and such
    }

	if (stat(n_temp_name,&stat_block)!=0) { 
		goto again;//No symlinks and such
	}	

	if(stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;

	if(strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_entcopy);
		upcase(find_name);
	} 

	find_size=(Bit32u) stat_block.st_size;
	struct tm *time;
	if((time=localtime(&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
		find_time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool localDrive::GetFileAttr(const char * name,Bit16u * attr) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    strcpy(newname,n_temp_name);

	struct stat status;
	if (stat(newname,&status)==0) {
		*attr=DOS_ATTR_ARCHIVE;
		if(status.st_mode & S_IFDIR) *attr|=DOS_ATTR_DIRECTORY;
		return true;
	}
	*attr=0;
	return false; 
}

bool localDrive::MakeDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newdir);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
        return false;
    }
    strcpy(newdir,n_temp_name);

#if defined (WIN32)						/* MS Visual C++ */
	int temp=mkdir(dirCache.GetExpandName(newdir));
#else
	int temp=mkdir(dirCache.GetExpandName(newdir),0700);
#endif
	if (temp==0) dirCache.CacheOut(newdir,true);

	return (temp==0);// || ((temp!=0) && (errno==EEXIST));
}

bool localDrive::RemoveDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newdir);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
        return false;
    }
    strcpy(newdir,n_temp_name);

	int temp=rmdir(dirCache.GetExpandName(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newdir);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newdir);
        return false;
    }
    strcpy(newdir,n_temp_name);

	// Skip directory test, if "\"
	size_t len = strlen(newdir);
	if (len && (newdir[len-1]!='\\')) {
		// It has to be a directory !
		struct stat test;
		if (stat(newdir,&test))			return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	};
	int temp=access(newdir,F_OK);
	return (temp==0);
}

bool localDrive::Rename(const char * oldname,const char * newname) {
	char newold[CROSS_LEN];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	
	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);

    // guest to host code page translation
    char *o_temp_name = CodePageGuestToHost(newold);
    if (o_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newold);
        return false;
    }
    strcpy(newold,o_temp_name);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newnew);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newnew);
        return false;
    }
    strcpy(newnew,n_temp_name);

	int temp=rename(newold,dirCache.GetExpandName(newnew));
	if (temp==0) dirCache.CacheOut(newnew);
	return (temp==0);

}

bool localDrive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	return true;
}

bool localDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    strcpy(newname,n_temp_name);

	struct stat temp_stat;
	if(stat(newname,&temp_stat)!=0) return false;
	if(temp_stat.st_mode & S_IFDIR) return false;
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);

    // guest to host code page translation
    char *n_temp_name = CodePageGuestToHost(newname);
    if (n_temp_name == NULL) {
        LOG_MSG("%s: Filename '%s' from guest is non-representable on the host filesystem through code page conversion",__FUNCTION__,newname);
        return false;
    }
    strcpy(newname,n_temp_name);

	struct stat temp_stat;
	if(stat(newname,&temp_stat)!=0) return false;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
		stat_block->date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
	} else {

	}
	stat_block->size=(Bit32u)temp_stat.st_size;
	return true;
}


Bit8u localDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

bool localDrive::isRemote(void) {
	return false;
}

bool localDrive::isRemovable(void) {
	return false;
}

Bits localDrive::UnMount(void) { 
	delete this;
	return 0; 
}

/* helper functions for drive cache */
void *localDrive::opendir(const char *name) {
	return open_directory(name);
}

void localDrive::closedir(void *handle) {
	close_directory((dir_information*)handle);
}

bool localDrive::read_directory_first(void *handle, char* entry_name, bool& is_directory) {
    if (::read_directory_first((dir_information*)handle, entry_name, is_directory)) {
        // guest to host code page translation
        char *n_temp_name = CodePageHostToGuest(entry_name);
        if (n_temp_name == NULL) {
            LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,entry_name);
            return false;
        }
        strcpy(entry_name,n_temp_name);
        return true;
    }

    return false;
}

bool localDrive::read_directory_next(void *handle, char* entry_name, bool& is_directory) {
    if (::read_directory_next((dir_information*)handle, entry_name, is_directory)) {
        // guest to host code page translation
        char *n_temp_name = CodePageHostToGuest(entry_name);
        if (n_temp_name == NULL) {
            LOG_MSG("%s: Filename '%s' from host is non-representable on the guest filesystem through code page conversion",__FUNCTION__,entry_name);
            return false;
        }
        strcpy(entry_name,n_temp_name);
        return true;
    }

    return false;
}

localDrive::localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid) {
	strcpy(basedir,startdir);
	sprintf(info,"local directory %s",startdir);
	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;

	dirCache.SetBaseDir(basedir,this);
}


//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(Bit8u * data,Bit16u * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {	// check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==WRITE) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=READ;
	*size=(Bit16u)fread(data,1,*size,fhandle);
	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as unmasking is realitively heavy to emulate */
    if (!IS_PC98_ARCH) {
        Bit8u mask = IO_Read(0x21);
        if(mask & 0x4 ) IO_Write(0x21,mask&0xfb);
    }

	return true;
}

bool localFile::Write(Bit8u * data,Bit16u * size) {
	if ((this->flags & 0xf) == OPEN_READ) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==READ) fseek(fhandle,ftell(fhandle),SEEK_SET);
	last_action=WRITE;
	if(*size==0){  
        return (!ftruncate(fileno(fhandle),ftell(fhandle)));
    }
    else 
    {
		*size=(Bit16u)fwrite(data,1,*size,fhandle);
		return true;
    }
}

/* ert, 20100711: Locking extensions */
#ifdef WIN32
#include <sys/locking.h>
bool localFile::LockFile(Bit8u mode, Bit32u pos, Bit16u size) {
	HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fhandle));
	BOOL bRet;

	switch (mode)
	{
	case 0: bRet = ::LockFile (hFile, pos, 0, size, 0); break;
	case 1: bRet = ::UnlockFile(hFile, pos, 0, size, 0); break;
	default: 
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	//LOG_MSG("::LockFile %s", name);

	if (!bRet)
	{
		switch (GetLastError())
		{
		case ERROR_ACCESS_DENIED:
		case ERROR_LOCK_VIOLATION:
		case ERROR_NETWORK_ACCESS_DENIED:
		case ERROR_DRIVE_LOCKED:
		case ERROR_SEEK_ON_DEVICE:
		case ERROR_NOT_LOCKED:
		case ERROR_LOCK_FAILED:
			DOS_SetError(0x21);
			break;
		case ERROR_INVALID_HANDLE:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case ERROR_INVALID_FUNCTION:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
		}
	}
	return bRet;
}
#endif

bool localFile::Seek(Bit32u * pos,Bit32u type) {
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}
	int ret=fseek(fhandle,*reinterpret_cast<Bit32s*>(pos),seektype);
	if (ret!=0) {
		// Out of file range, pretend everythings ok 
		// and move file pointer top end of file... ?! (Black Thorne)
		fseek(fhandle,0,SEEK_END);
	};
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	Bit32u * fake_pos=(Bit32u*)&temppos;
	*pos=*fake_pos;
#endif
	*pos=(Bit32u)ftell(fhandle);
	last_action=NONE;
	return true;
}

bool localFile::Close() {
	// only close if one reference left
	if (refCtr==1) {
		if(fhandle) fclose(fhandle); 
		fhandle = 0;
		open = false;
	};

	if (newtime) {
 		// backport from DOS_PackDate() and DOS_PackTime()
		struct tm tim = { 0 };
		tim.tm_sec  = (time&0x1f)*2;
		tim.tm_min  = (time>>5)&0x3f;
		tim.tm_hour = (time>>11)&0x1f;
		tim.tm_mday = date&0x1f;
		tim.tm_mon  = ((date>>5)&0x0f)-1;
		tim.tm_year = (date>>9)+1980-1900;
		//  have the C run-time library code compute whether standard time or daylight saving time is in effect.
		tim.tm_isdst = -1;
		// serialize time
		mktime(&tim);

		struct utimbuf ftim;
		ftim.actime = ftim.modtime = mktime(&tim);
	
		char fullname[DOS_PATHLENGTH];
		strcpy(fullname, Drives[drive]->GetBaseDir());
		strcat(fullname, name);
//		Dos_SpecoalChar(fullname, true);
		CROSS_FILENAME(fullname);
		if (utime(fullname, &ftim)) {
//			extern int errno; 
//			LOG_MSG("Set time failed for %s (%s)", fullname, strerror(errno));
			return false;
		}
	}

	return true;
}

Bit16u localFile::GetInformation(void) {
	return read_only_medium?0x40:0;
}
	

Bit32u localFile::GetSeekPos() {
	return ftell( fhandle );
}


localFile::localFile(const char* _name, FILE * handle) {
	fhandle=handle;
	open=true;
	UpdateDateTimeFromHost();

	attr=DOS_ATTR_ARCHIVE;
	last_action=NONE;
	read_only_medium=false;

	name=0;
	SetName(_name);
}

void localFile::FlagReadOnlyMedium(void) {
	read_only_medium = true;
}

bool localFile::UpdateDateTimeFromHost(void) {
	if(!open) return false;
	struct stat temp_stat;
	fstat(fileno(fhandle),&temp_stat);
	struct tm * ltime;
	if((ltime=localtime(&temp_stat.st_mtime))!=0) {
		time=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
		date=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
	} else {
		time=1;date=1;
	}
	return true;
}

void localFile::Flush(void) {
	if (last_action==WRITE) {
		fseek(fhandle,ftell(fhandle),SEEK_SET);
		last_action=NONE;
	}
}


// ********************************************
// CDROM DRIVE
// ********************************************

int  MSCDEX_RemoveDrive(char driveLetter);
int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit);
bool MSCDEX_HasMediaChanged(Bit8u subUnit);
bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name);


cdromDrive::cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid) {
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	strcpy(info, "CDRom ");
	strcat(info, startdir);
	this->driveLetter = driveLetter;
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File * * file,const char * name,Bit32u flags) {
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool retcode = localDrive::FileOpen(file,name,flags);
	if(retcode) (dynamic_cast<localFile*>(*file))->FlagReadOnlyMedium();
	return retcode;
}

bool cdromDrive::FileCreate(DOS_File * * /*file*/,const char * /*name*/,Bit16u /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(const char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(const char * /*oldname*/,const char * /*newname*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(const char * name,Bit16u * attr) {
	bool result = localDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
}

bool cdromDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path) {
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::isRemote(void) {
	return true;
}

bool cdromDrive::isRemovable(void) {
	return true;
}

Bits cdromDrive::UnMount(void) {
	if(MSCDEX_RemoveDrive(driveLetter)) {
		delete this;
		return 0;
	}
	return 2;
}
