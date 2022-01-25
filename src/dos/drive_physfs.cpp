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
 *  Heavy improvements by the DOSBox-X Team
 *  With major works from joncampbell123 and Wengier
 */

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "logging.h"
#if !defined(WIN32)
#include <sys/statvfs.h>
#endif

#include "../libs/physfs/physfs.h"
#include "../libs/physfs/physfs.c"
#include "../libs/physfs/physfs_archiver_7z.c"
#include "../libs/physfs/physfs_archiver_dir.c"
#include "../libs/physfs/physfs_archiver_grp.c"
#include "../libs/physfs/physfs_archiver_hog.c"
#include "../libs/physfs/physfs_archiver_iso9660.c"
#include "../libs/physfs/physfs_archiver_mvl.c"
#include "../libs/physfs/physfs_archiver_qpak.c"
#include "../libs/physfs/physfs_archiver_slb.c"
#include "../libs/physfs/physfs_archiver_unpacked.c"
#include "../libs/physfs/physfs_archiver_vdf.c"
#include "../libs/physfs/physfs_archiver_wad.c"
#include "../libs/physfs/physfs_archiver_zip.c"
#include "../libs/physfs/physfs_byteorder.c"
#include "../libs/physfs/physfs_platform_posix.c"
#include "../libs/physfs/physfs_platform_unix.c"
#include "../libs/physfs/physfs_platform_windows.c"
#include "../libs/physfs/physfs_platform_winrt.cpp"
#include "../libs/physfs/physfs_unicode.c"

extern int lfn_filefind_handle;
extern uint16_t ldid[256];
extern std::string ldir[256];
extern bool rsize;
extern unsigned long totalc, freec;

int  MSCDEX_RemoveDrive(char driveLetter);
int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit);
bool MSCDEX_HasMediaChanged(uint8_t subUnit);
bool MSCDEX_GetVolumeName(uint8_t subUnit, char* name);

PHYSFS_sint64 PHYSFS_fileLength(const char *name) {
	PHYSFS_file *f = PHYSFS_openRead(name);
	if (f == NULL) return 0;
	PHYSFS_sint64 size = PHYSFS_fileLength(f);
	PHYSFS_close(f);
	return size;
}

/* Need to strip "/.." components and transform '\\' to '/' for physfs */
static char *normalize(char * name, const char *basedir) {
	int last = (int)(strlen(name)-1);
	strreplace_dbcs(name,'\\','/');
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (last > 0 && name[last] == '.' && name[last-1] == '/') name[last-1] = 0;
	if (last > 1 && name[last] == '.' && name[last-1] == '.' && name[last-2] == '/') {
		name[last-2] = 0;
		char *slash = strrchr(name,'/');
		if (slash) *slash = 0;
	}
	if (strlen(basedir) > strlen(name)) { strcpy(name,basedir); strreplace_dbcs(name,'\\','/'); }
	last = (int)(strlen(name)-1);
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (name[0] == 0) name[0] = '/';
	//LOG_MSG("File access: %s",name);
	return name;
}

class physfsFile : public DOS_File {
public:
	physfsFile(const char* name, PHYSFS_file * handle,uint16_t devinfo, const char* physname, bool write);
	bool Read(uint8_t * data,uint16_t * size);
	bool Write(const uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool prepareRead();
	bool prepareWrite();
	bool Close();
	uint16_t GetInformation(void);
	bool UpdateDateTimeFromHost(void);
private:
	PHYSFS_file * fhandle;
	enum { READ,WRITE } last_action;
	uint16_t info;
	char pname[CROSS_LEN];
};

bool physfsDrive::AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) {
	const char *wdir = PHYSFS_getWriteDir();
	if (wdir) {
		bool res=false;
#if defined(WIN32)
		long unsigned int dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
		uint8_t drive=strlen(wdir)>1&&wdir[1]==':'?toupper(wdir[0])-'A'+1:0;
		if (drive>26) drive=0;
		char root[4]="A:\\";
		root[0]='A'+drive-1;
		res = GetDiskFreeSpace(drive?root:NULL, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
		if (res) {
			unsigned long total = dwTotalClusters * dwSectPerClust;
			int ratio = total > 2097120 ? 64 : (total > 1048560 ? 32 : (total > 524280 ? 16 : (total > 262140 ? 8 : (total > 131070 ? 4 : (total > 65535 ? 2 : 1)))));
			*_bytes_sector = (uint16_t)dwBytesPerSect;
			*_sectors_cluster = ratio;
			*_total_clusters = total > 4194240? 65535 : (uint16_t)(dwTotalClusters * dwSectPerClust / ratio);
			*_free_clusters = dwFreeClusters ? (total > 4194240? 61440 : (uint16_t)(dwFreeClusters * dwSectPerClust / ratio)) : 0;
			if (rsize) {
				totalc=dwTotalClusters * dwSectPerClust / ratio;
				freec=dwFreeClusters * dwSectPerClust / ratio;
			}
#else
		struct statvfs stat;
		res = statvfs(wdir, &stat) == 0;
		if (res) {
			int ratio = stat.f_blocks / 65536, tmp=ratio;
			*_bytes_sector = 512;
			*_sectors_cluster = stat.f_frsize/512 > 64? 64 : stat.f_frsize/512;
			if (ratio>1) {
				if (ratio * (*_sectors_cluster) > 64) tmp = (*_sectors_cluster+63)/(*_sectors_cluster);
				*_sectors_cluster = ratio * (*_sectors_cluster) > 64? 64 : ratio * (*_sectors_cluster);
				ratio = tmp;
			}
			*_total_clusters = stat.f_blocks > 65535? 65535 : stat.f_blocks;
			*_free_clusters = stat.f_bavail > 61440? 61440 : stat.f_bavail;
			if (rsize) {
				totalc=stat.f_blocks;
				freec=stat.f_bavail;
				if (ratio>1) {
					totalc/=ratio;
					freec/=ratio;
				}
			}
#endif
		} else {
            // 512*32*32765==~500MB total size
            // 512*32*16000==~250MB total free size
            *_bytes_sector = 512;
            *_sectors_cluster = 32;
            *_total_clusters = 32765;
            *_free_clusters = 16000;
		}
	} else {
        *_bytes_sector=allocation.bytes_sector;
        *_sectors_cluster=allocation.sectors_cluster;
        *_total_clusters=allocation.total_clusters;
        *_free_clusters=0;
    }
	return true;
}

bool physfsDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
    bool result = PHYSFS_exists(newname);
    if(result) {
        PHYSFS_Stat statbuf;
        BAIL_IF_ERRPASS(!PHYSFS_stat(newname, &statbuf), false);
        result = (statbuf.filetype != PHYSFS_FILETYPE_DIRECTORY);
    }

	return result;
}

bool physfsDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(newname, &statbuf), false);
    time_t mytime = statbuf.modtime;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		stat_block->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		stat_block->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		stat_block->time=DOS_PackTime(0,0,0);
		stat_block->date=DOS_PackDate(1980,1,1);
	}
	stat_block->size=(uint32_t)PHYSFS_fileLength(newname);
	return true;
}

struct opendirinfo {
	char dir[CROSS_LEN];
	char **files;
	int pos;
};

/* helper functions for drive cache */
bool physfsDrive::isdir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(myname, &statbuf), false);
    return (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY);
}

void *physfsDrive::opendir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(myname, &statbuf), NULL);
    if(statbuf.filetype != PHYSFS_FILETYPE_DIRECTORY) return NULL;
	struct opendirinfo *oinfo = (struct opendirinfo *)malloc(sizeof(struct opendirinfo));
	strcpy(oinfo->dir, myname);
	oinfo->files = PHYSFS_enumerateFiles(myname);
	if (oinfo->files == NULL) {
        const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        const char* errorMessage = err ? PHYSFS_getErrorByCode(err) : "Unknown error";
		LOG_MSG("PHYSFS: nothing found for %s (%s)",myname,errorMessage);
		free(oinfo);
		return NULL;
	}

	oinfo->pos = (myname[4] == 0?0:-2);
	return (void *)oinfo;
}

void physfsDrive::closedir(void *handle) {
	struct opendirinfo *oinfo = (struct opendirinfo *)handle;
	if (handle == NULL) return;
	if (oinfo->files != NULL) PHYSFS_freeList(oinfo->files);
	free(oinfo);
}

bool physfsDrive::read_directory_first(void* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	return read_directory_next(dirp, entry_name, entry_sname, is_directory);
}

bool physfsDrive::read_directory_next(void* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	struct opendirinfo *oinfo = (struct opendirinfo *)dirp;
	if (!oinfo) return false;
	if (oinfo->pos == -2) {
		oinfo->pos++;
		safe_strncpy(entry_name,".",CROSS_LEN);
		safe_strncpy(entry_sname,".",DOS_NAMELENGTH+1);
		is_directory = true;
		return true;
	}
	if (oinfo->pos == -1) {
		oinfo->pos++;
		safe_strncpy(entry_name,"..",CROSS_LEN);
		safe_strncpy(entry_sname,"..",DOS_NAMELENGTH+1);
		is_directory = true;
		return true;
	}
	if (!oinfo->files || !oinfo->files[oinfo->pos]) return false;
	safe_strncpy(entry_name,oinfo->files[oinfo->pos++],CROSS_LEN);
	*entry_sname=0;
	is_directory = isdir(strlen(oinfo->dir)?(std::string(oinfo->dir)+"/"+std::string(entry_name)).c_str():entry_name);
	return true;
}

static uint8_t physfs_used = 0;
physfsDrive::physfsDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,int &error,std::vector<std::string> &options)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options)
{
	this->driveLetter = driveLetter;
	this->mountarc = "";
	char mp[] = "A_DRIVE";
	mp[0] = driveLetter;
	char newname[CROSS_LEN+1];
	strcpy(newname,startdir);
	CROSS_FILENAME(newname);
	if (!physfs_used) {
		PHYSFS_init("");
		PHYSFS_permitSymbolicLinks(1);
	}

	physfs_used++;
	char *lastdir = newname;
	char *dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	while (dir) {
		*dir++ = 0;
		if((lastdir == newname) && !strchr(dir+(((dir[0]|0x20) >= 'a' && (dir[0]|0x20) <= 'z')?2:0),':')) {
			// If the first parameter is a directory, the next one has to be the archive file,
			// do not confuse it with basedir if trailing : is not there!
			int tmp = (int)(strlen(dir)-1);
			dir[tmp++] = ':';
			dir[tmp++] = CROSS_FILESPLIT;
			dir[tmp] = '\0';
		}
		if (*lastdir) {
            if(PHYSFS_mount(lastdir, mp, true) == 0) {
                const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
                const char* errorMessage = err ? PHYSFS_getErrorByCode(err) : "Unknown error";
                LOG_MSG("PHYSFS couldn't mount '%s': %s", lastdir, errorMessage);
            }
            else {
                if (mountarc.size()) mountarc+=", ";
                mountarc+= lastdir;
            }
        }
		lastdir = dir;
		dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	}
	error = 0;
	if (!mountarc.size()) {error = 10;return;}
	strcpy(basedir,"\\");
	strcat(basedir,mp);
	strcat(basedir,"\\");

	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;

	dirCache.SetBaseDir(basedir, this);
}

physfsDrive::~physfsDrive(void) {
	if(!physfs_used) {
		LOG_MSG("PHYSFS invalid reference count!");
		return;
	}
	physfs_used--;
	if(!physfs_used) {
		LOG_MSG("PHYSFS calling PHYSFS_deinit()");
		PHYSFS_deinit();
	}
}

const char *physfsDrive::getOverlaydir(void) {
	static const char *overlaydir = PHYSFS_getWriteDir();
	return overlaydir;
}

bool physfsDrive::setOverlaydir(const char * name) {
	char newname[CROSS_LEN+1];
	strcpy(newname,name);
	CROSS_FILENAME(newname);
	const char *oldwrite = PHYSFS_getWriteDir();
	if (oldwrite) oldwrite = strdup(oldwrite);
	if (!PHYSFS_setWriteDir(newname)) {
		if (oldwrite)
			PHYSFS_setWriteDir(oldwrite);
        return false;
	} else {
        if (oldwrite) PHYSFS_unmount(oldwrite);
        PHYSFS_mount(newname, NULL, 1);
        dirCache.EmptyCache();
    }
	if (oldwrite) free((char *)oldwrite);
    return true;
}

const char *physfsDrive::GetInfo() {
	sprintf(info,"PhysFS directory %s", mountarc.c_str());
	return info;
}

bool physfsDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);

	PHYSFS_file * hand;
	if (!PHYSFS_exists(newname)) return false;
	if ((flags&0xf) == OPEN_READ) {
		hand = PHYSFS_openRead(newname);
	} else {

		/* open for reading, deal with writing later */
		hand = PHYSFS_openRead(newname);
	}

	if (!hand) {
		if((flags&0xf) != OPEN_READ) {
			PHYSFS_file *hmm = PHYSFS_openRead(newname);
			if (hmm) {
				PHYSFS_close(hmm);
				LOG_MSG("Warning: file %s exists and failed to open in write mode.",newname);
			}
		}
		return false;
	}

	*file=new physfsFile(name,hand,0x202,newname,false);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
	return true;
}

bool physfsDrive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes) {
    (void)attributes;//UNUSED
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);

	/* Test if file exists, don't add to dirCache then */
	bool existing_file=PHYSFS_exists(newname);

	char *slash = strrchr(newname,'/');
	if (slash && slash != newname) {
		*slash = 0;
        PHYSFS_Stat statbuf;
        BAIL_IF_ERRPASS(!PHYSFS_stat(newname, &statbuf), false);
		if (statbuf.filetype != PHYSFS_FILETYPE_DIRECTORY) return false;
		PHYSFS_mkdir(newname);
		*slash = '/';
	}

	PHYSFS_file * hand=PHYSFS_openWrite(newname);
	if (!hand){
        const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        const char* errorMessage = err ? PHYSFS_getErrorByCode(err) : "Unknown error";
		LOG_MSG("Warning: file creation failed: %s (%s)",newname,errorMessage);
		return false;
	}

	/* Make the 16 bit device information */
	*file=new physfsFile(name,hand,0x202,newname,true);
	(*file)->flags=OPEN_READWRITE;
	if(!existing_file) {
		strcpy(newname,basedir);
		strcat(newname,name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
		dirCache.EmptyCache();
	}
	return true;
}

bool physfsDrive::FileUnlink(const char * name) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	if (PHYSFS_delete(newname)) {
		CROSS_FILENAME(newname);
		dirCache.DeleteEntry(newname);
		dirCache.EmptyCache();
		return true;
	};
	return false;
}

bool physfsDrive::RemoveDir(const char * dir) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(newdir, &statbuf), false);
	if ((statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY) && PHYSFS_delete(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.DeleteEntry(newdir,true);
		dirCache.EmptyCache();
		return true;
	}
	return false;
}

bool physfsDrive::MakeDir(const char * dir) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
	if (PHYSFS_mkdir(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.CacheOut(newdir,true);
		return true;
	}
	return false;
}

bool physfsDrive::TestDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(newdir, &statbuf), false);
	return (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY);
}

bool physfsDrive::Rename(const char * oldname,const char * newname) {
    if (!getOverlaydir()) {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
	char newold[CROSS_LEN];
	strcpy(newold,basedir);
	strcat(newold,oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandName(newold);
	normalize(newold,basedir);

	char newnew[CROSS_LEN];
	strcpy(newnew,basedir);
	strcat(newnew,newname);
	CROSS_FILENAME(newnew);
	dirCache.ExpandName(newnew);
	normalize(newnew,basedir);
    const char *dir=PHYSFS_getRealDir(newold);
    if (dir && !strcmp(getOverlaydir(), dir)) {
        char fullold[CROSS_LEN], fullnew[CROSS_LEN];
        strcpy(fullold, dir);
        strcat(fullold, newold);
        strcpy(fullnew, dir);
        strcat(fullnew, newnew);
        if (rename(fullold, fullnew)==0) {
            dirCache.EmptyCache();
            return true;
        }
    } else LOG_MSG("PHYSFS: rename not supported (%s -> %s)",newold,newnew); // yuck. physfs doesn't have "rename".
	return false;
}

bool physfsDrive::SetFileAttr(const char * name,uint16_t attr) {
    (void)name;//UNUSED
    (void)attr;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfsDrive::GetFileAttr(const char * name,uint16_t * attr) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	char *last = strrchr(newname,'/');
	if (last == NULL) last = newname-1;

	*attr = 0;
	if (!PHYSFS_exists(newname)) return false;
	*attr=DOS_ATTR_ARCHIVE;
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(newname, &statbuf), false);
    if(statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY)
        *attr |= DOS_ATTR_DIRECTORY;
    return true;
}

bool physfsDrive::GetFileAttrEx(char* name, struct stat *status) {
	return localDrive::GetFileAttrEx(name,status);
}

unsigned long physfsDrive::GetCompressedSize(char* name) {
	return localDrive::GetCompressedSize(name);
}

#if defined (WIN32)
HANDLE physfsDrive::CreateOpenFile(const char* name) {
		return localDrive::CreateOpenFile(name);
}

unsigned long physfsDrive::GetSerial() {
		return localDrive::GetSerial();
}
#endif

bool physfsDrive::FindNext(DOS_DTA & dta) {
	char * dir_ent, *ldir_ent;
	char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];

	uint8_t srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	uint8_t find_attr;

    dta.GetSearchParams(srch_attr,srch_pattern,false);
	uint16_t id = lfn_filefind_handle>=LFN_FILEFIND_MAX?dta.GetDirID():ldid[lfn_filefind_handle];

again:
    if (!dirCache.FindNext(id,dir_ent,ldir_ent)) {
		if (lfn_filefind_handle<LFN_FILEFIND_MAX) {
			ldid[lfn_filefind_handle]=0;
			ldir[lfn_filefind_handle]="";
		}
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
    if(!WildFileCmp(dir_ent,srch_pattern)&&!LWildFileCmp(ldir_ent,srch_pattern)) goto again;

	strcpy(full_name,lfn_filefind_handle>=LFN_FILEFIND_MAX?srchInfo[id].srch_dir:ldir[lfn_filefind_handle].c_str());
	strcpy(lfull_name,full_name);

	strcat(full_name,dir_ent);
    strcat(lfull_name,ldir_ent);

	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	dirCache.ExpandName(lfull_name);
	normalize(lfull_name,basedir);

    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(lfull_name, &statbuf), false);
	if (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY) find_attr=DOS_ATTR_DIRECTORY|DOS_ATTR_ARCHIVE;
	else find_attr=DOS_ATTR_ARCHIVE;
	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;

	/*file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
	uint16_t find_date,find_time;uint32_t find_size;
	find_size=(uint32_t)PHYSFS_fileLength(lfull_name);
    time_t mytime = statbuf.modtime;
	struct tm *time;
	if((time=localtime(&mytime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6;
		find_date=4;
	}
	if(strlen(dir_ent)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_ent);
		upcase(find_name);
	}
	strcpy(lfind_name,ldir_ent);
	lfind_name[LFN_NAMELENGTH]=0;

	dta.SetResult(find_name,lfind_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool physfsDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool /*fcb_findfirst*/) {
	return localDrive::FindFirst(_dir,dta);
}

bool physfsDrive::isRemote(void) {
	return localDrive::isRemote();
}

bool physfsDrive::isRemovable(void) {
	return true;
}

Bits physfsDrive::UnMount(void) {
	char mp[] = "A_DRIVE";
	mp[0] = driveLetter;
	PHYSFS_unmount(mp);
	delete this;
	return 0;
}

bool physfsFile::Read(uint8_t * data,uint16_t * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {        // check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==WRITE) prepareRead();
	last_action=READ;
    const PHYSFS_sint64 mysize = PHYSFS_readBytes(fhandle, data, *size);
	//LOG_MSG("Read %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
	*size = (uint16_t)mysize;
	return true;
}

bool physfsFile::Write(const uint8_t * data,uint16_t * size) {
	if (!PHYSFS_getWriteDir() || (this->flags & 0xf) == OPEN_READ) { // check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==READ) prepareWrite();
	last_action=WRITE;
	if (*size==0) {
		if (PHYSFS_tell(fhandle) == 0) {
            if (PHYSFS_close(PHYSFS_openWrite(pname))) {
                //LOG_MSG("Truncate %s (%s)",name,PHYSFS_getLastError());
                return true;
            }
            else
                return false;
		} else {
			LOG_MSG("PHYSFS TODO: truncate not yet implemented (%s at %i)",pname,(int)PHYSFS_tell(fhandle));
			return false;
		}
	} else {
		PHYSFS_sint64 mysize = PHYSFS_writeBytes(fhandle, data, *size);
		//LOG_MSG("Wrote %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
		*size = (uint16_t)mysize;
		return true;
	}
}

bool physfsFile::Seek(uint32_t * pos,uint32_t type) {
	PHYSFS_sint64 mypos = (int32_t)*pos;
	switch (type) {
	case DOS_SEEK_SET:break;
	case DOS_SEEK_CUR:mypos += PHYSFS_tell(fhandle); break;
	case DOS_SEEK_END:mypos += PHYSFS_fileLength(fhandle)-mypos; break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}

	if (!PHYSFS_seek(fhandle,mypos)) {
		// Out of file range, pretend everythings ok
		// and move file pointer top end of file... ?! (Black Thorne)
		PHYSFS_seek(fhandle,PHYSFS_fileLength(fhandle));
	};
	//LOG_MSG("Seek to %i (%i at %x) of %s (%s)",(int)mypos,(int)*pos,type,name,PHYSFS_getLastError());

	*pos=(uint32_t)PHYSFS_tell(fhandle);
	return true;
}

bool physfsFile::prepareRead() {
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	PHYSFS_close(fhandle);
	fhandle = PHYSFS_openRead(pname);
	PHYSFS_seek(fhandle, pos);
	return true; //LOG_MSG("Goto read (%s at %i)",pname,PHYSFS_tell(fhandle));
}

#ifndef WIN32
#include <fcntl.h>
#include <errno.h>
#endif

bool physfsFile::prepareWrite() {
	const char *wdir = PHYSFS_getWriteDir();
	if (wdir == NULL) {
		LOG_MSG("PHYSFS could not fulfill write request: no write directory set.");
		return false;
	}
	//LOG_MSG("Goto write (%s at %i)",pname,PHYSFS_tell(fhandle));
	const char *fdir = PHYSFS_getRealDir(pname);
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	char *slash = strrchr(pname,'/');
	if (slash && slash != pname) {
		*slash = 0;
		PHYSFS_mkdir(pname);
		*slash = '/';
	}
	if (strcmp(fdir,wdir)) { /* we need COW */
		//LOG_MSG("COW",pname,PHYSFS_tell(fhandle));
		PHYSFS_file *whandle = PHYSFS_openWrite(pname);
		if (whandle == NULL) {
            const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
            const char* errorMessage = err ? PHYSFS_getErrorByCode(err) : "Unknown error";
			LOG_MSG("PHYSFS copy-on-write failed: %s.",errorMessage);
			return false;
		}
		char buffer[65536];
		PHYSFS_sint64 size;
		PHYSFS_seek(fhandle, 0);
		while ((size = PHYSFS_readBytes(fhandle,buffer,65536)) > 0) {
			if (PHYSFS_writeBytes(whandle, buffer, size) != size) {
                const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
                const char* errorMessage = err ? PHYSFS_getErrorByCode(err) : "Unknown error";
				LOG_MSG("PHYSFS copy-on-write failed: %s.",errorMessage);
				PHYSFS_close(whandle);
				return false;
			}
		}
		PHYSFS_seek(whandle, pos);
		PHYSFS_close(fhandle);
		fhandle = whandle;
	} else { // megayuck - physfs on posix platforms uses O_APPEND. We illegally access the fd directly and clear that flag.
		//LOG_MSG("noCOW",pname,PHYSFS_tell(fhandle));
		PHYSFS_close(fhandle);
		fhandle = PHYSFS_openAppend(pname);
#ifndef WIN32
		fcntl(**(int**)fhandle->opaque,F_SETFL,0);
#endif
		PHYSFS_seek(fhandle, pos);
	}
	return true;
}

bool physfsFile::Close() {
	// only close if one reference left
	if (refCtr==1) {
		PHYSFS_close(fhandle);
		fhandle = 0;
		open = false;
	};
	return true;
}

uint16_t physfsFile::GetInformation(void) {
	return info;
}

physfsFile::physfsFile(const char* _name, PHYSFS_file * handle,uint16_t devinfo, const char* physname, bool write) {
	fhandle=handle;
	info=devinfo;
	strcpy(pname,physname);
    PHYSFS_Stat statbuf;
    if(!PHYSFS_stat(pname, &statbuf)) return;
    time_t mytime = statbuf.modtime;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		this->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		this->time=DOS_PackTime(0,0,0);
		this->date=DOS_PackDate(1980,1,1);
	}

	attr=DOS_ATTR_ARCHIVE;
	last_action=(write?WRITE:READ);

	open=true;
	name=0;
	SetName(_name);
}

bool physfsFile::UpdateDateTimeFromHost(void) {
	if(!open) return false;
    PHYSFS_Stat statbuf;
    BAIL_IF_ERRPASS(!PHYSFS_stat(pname, &statbuf), false);
    time_t mytime = statbuf.modtime;
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		this->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
		this->time=DOS_PackTime(0,0,0);
		this->date=DOS_PackDate(1980,1,1);
	}
	return true;
}

physfscdromDrive::physfscdromDrive(const char driveLetter, const char * startdir,uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid, int& error, std::vector<std::string> &options)
		   :physfsDrive(driveLetter,startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,error,options)
{
	// Init mscdex
	if (!mountarc.size()) {error = 10;return;}
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	this->driveLetter = driveLetter;
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

const char *physfscdromDrive::GetInfo() {
	sprintf(info,"PhysFS CDRom %s", mountarc.c_str());
	return info;
}

bool physfscdromDrive::FileOpen(DOS_File * * file,const char * name,uint32_t flags)
{
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return physfsDrive::FileOpen(file,name,flags);
}

bool physfscdromDrive::FileCreate(DOS_File * * file,const char * name,uint16_t attributes)
{
    (void)file;//UNUSED
    (void)name;//UNUSED
    (void)attributes;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfscdromDrive::FileUnlink(const char * name)
{
    (void)name;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfscdromDrive::RemoveDir(const char * dir)
{
    (void)dir;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfscdromDrive::MakeDir(const char * dir)
{
    (void)dir;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfscdromDrive::Rename(const char * oldname,const char * newname)
{
    (void)oldname;//UNUSED
    (void)newname;//UNUSED
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool physfscdromDrive::GetFileAttr(const char * name,uint16_t * attr)
{
	bool result = physfsDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
}

bool physfscdromDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst)
{
    (void)fcb_findfirst;//UNUSED
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return physfsDrive::FindFirst(_dir,dta);
}

void physfscdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	physfsDrive::SetDir(path);
}

bool physfscdromDrive::isRemote(void) {
	return true;
}

bool physfscdromDrive::isRemovable(void) {
	return true;
}

Bits physfscdromDrive::UnMount(void) {
	if(MSCDEX_RemoveDrive(driveLetter))
		return physfsDrive::UnMount();
	return 2;
}
