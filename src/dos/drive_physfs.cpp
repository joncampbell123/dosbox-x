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

/* $Id$ */

#include "dosbox.h"

#if C_HAVE_PHYSFS

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <physfs.h>
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"

/* yuck. Hopefully, later physfs versions improve things */
/* The hackishness level is quite low, but to get perfect, here is my personal wishlist for PHYSFS:
 - mounting zip files at arbitrary locations (already in CVS, I think)
 - rename support
 - a better API for stat() infos
 - more stdio-like API for seek, open and truncate
 - perhaps a ramdisk as write dir?
*/
PHYSFS_sint64 PHYSFS_fileLength(const char *name) {
	PHYSFS_file *f = PHYSFS_openRead(name);
	if (f == NULL) return 0;
	PHYSFS_sint64 size = PHYSFS_fileLength(f);
	PHYSFS_close(f);
	return size;
}

class physfsFile : public DOS_File {
public:
	physfsFile(const char* name, PHYSFS_file * handle,Bit16u devinfo, const char* physname, bool write);
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool prepareRead();
	bool prepareWrite();
	bool Close();
	Bit16u GetInformation(void);
	bool UpdateDateTimeFromHost(void);   
private:
	PHYSFS_file * fhandle;
	enum { READ,WRITE } last_action;
	Bit16u info;
	char pname[CROSS_LEN];
};

/* Need to strip "/.." components and transform '\\' to '/' for physfs */
static char *normalize(char * name, const char *basedir) {
	int last = strlen(name)-1;
	strreplace(name,'\\','/');
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (last > 0 && name[last] == '.' && name[last-1] == '/') name[last-1] = 0;
	if (last > 1 && name[last] == '.' && name[last-1] == '.' && name[last-2] == '/') {
		name[last-2] = 0;
		char *slash = strrchr(name,'/');
		if (slash) *slash = 0;
	}
	if (strlen(basedir) > strlen(name)) { strcpy(name,basedir); strreplace(name,'\\','/'); }
	last = strlen(name)-1;
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (name[0] == 0) name[0] = '/';
	//LOG_MSG("File access: %s",name);
	return name;
}

bool physfsDrive::FileCreate(DOS_File * * file,const char * name,Bit16u attributes) {
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
		char file[CROSS_LEN];
		*slash = 0;
		if (!PHYSFS_isDirectory(newname)) return false;
		PHYSFS_mkdir(newname);
		*slash = '/';
	}

	PHYSFS_file * hand=PHYSFS_openWrite(newname);
	if (!hand){
		LOG_MSG("Warning: file creation failed: %s (%s)",newname,PHYSFS_getLastError());
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
	}
	return true;
}

bool physfsDrive::FileOpen(DOS_File * * file,const char * name,Bit32u flags) {
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
				LOG_MSG("Warning: file %s exists and failed to open in write mode.\nPlease mount a write directory (see docs).",newname);
			}
		}
		return false;
	}
   
	*file=new physfsFile(name,hand,0x202,newname,false);
	(*file)->flags=flags;  //for the inheritance flag and maybe check for others.
	return true;
}

bool physfsDrive::FileUnlink(const char * name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	if (PHYSFS_delete(newname)) {
		CROSS_FILENAME(newname);
		dirCache.DeleteEntry(newname);
		return true;
	};
	return false;
}


bool physfsDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {

	char tempDir[CROSS_LEN];
	strcpy(tempDir,basedir);
	strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir)-1]!=CROSS_FILESPLIT) strcat(tempDir,end);
	
	Bit16u id;
	if (!dirCache.FindFirst(tempDir,id))
	{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	strcpy(srchInfo[id].srch_dir,tempDir);
	dta.SetDirID(id);
	
	Bit8u sAttr;
	dta.GetSearchParams(sAttr,tempDir);

	if (sAttr == DOS_ATTR_VOLUME) {
		if ( strcmp(dirCache.GetLabel(), "") == 0 ) {
			LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
			dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
		dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
		return true;
	} else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst) { 
	//should check for a valid leading directory instead of 0
	//exists==true if the volume label matches the searchmask and the path is valid
		if ( strcmp(dirCache.GetLabel(), "") == 0 ) {
			LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
			dta.SetResult("NO_LABEL",0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
		if (WildFileCmp(dirCache.GetLabel(),tempDir)) {
			dta.SetResult(dirCache.GetLabel(),0,0,0,DOS_ATTR_VOLUME);
			return true;
		}
	}
	return FindNext(dta);
}

bool physfsDrive::FindNext(DOS_DTA & dta) {

	char * dir_ent;
	char full_name[CROSS_LEN];

	Bit8u srch_attr;char srch_pattern[DOS_NAMELENGTH_ASCII];
	Bit8u find_attr;

	dta.GetSearchParams(srch_attr,srch_pattern);
	
	Bitu id = dta.GetDirID();

again:
	if (!dirCache.FindNext(id,dir_ent)) {
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if(!WildFileCmp(dir_ent,srch_pattern)) goto again;

	char find_name[DOS_NAMELENGTH_ASCII];Bit16u find_date,find_time;Bit32u find_size;
	if(strlen(dir_ent)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_ent);
		upcase(find_name);
	} 

	strcpy(full_name,srchInfo[id].srch_dir);
	strcat(full_name,dir_ent);
	dirCache.ExpandName(full_name);
	normalize(full_name,basedir);
	
	if (PHYSFS_isDirectory(full_name)) find_attr=DOS_ATTR_DIRECTORY|DOS_ATTR_ARCHIVE;
	else find_attr=DOS_ATTR_ARCHIVE;
 	if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM)) goto again;
	
	/*file is okay, setup everything to be copied in DTA Block */
	find_size=(Bit32u)PHYSFS_fileLength(full_name);
	time_t mytime = PHYSFS_getLastModTime(full_name);
	struct tm *time;
	if((time=localtime(&mytime))!=0){
		find_date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
		find_time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,find_size,find_date,find_time,find_attr);
	return true;
}

bool physfsDrive::GetFileAttr(const char * name,Bit16u * attr) {
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
	if (PHYSFS_isDirectory(newname)) *attr|=DOS_ATTR_DIRECTORY;
	return true;
}

bool physfsDrive::MakeDir(const char * dir) {
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

bool physfsDrive::RemoveDir(const char * dir) {
	char newdir[CROSS_LEN];
	strcpy(newdir,basedir);
	strcat(newdir,dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandName(newdir);
	normalize(newdir,basedir);
	if (PHYSFS_isDirectory(newdir) && PHYSFS_delete(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.DeleteEntry(newdir,true);
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
	return (PHYSFS_isDirectory(newdir));
}

bool physfsDrive::Rename(const char * oldname,const char * newname) {
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
	/* yuck. physfs doesn't have "rename". */
	LOG_MSG("PHYSFS TODO: rename not yet implemented (%s -> %s)",newold,newnew);
	return false;

}

bool physfsDrive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	/* Always report 100 mb free should be enough */
	/* Total size is always 1 gb */
	*_bytes_sector=allocation.bytes_sector;
	*_sectors_cluster=allocation.sectors_cluster;
	*_total_clusters=allocation.total_clusters;
	*_free_clusters=allocation.free_clusters;
	return true;
}

bool physfsDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	return PHYSFS_exists(newname) && !PHYSFS_isDirectory(newname);
}

bool physfsDrive::FileStat(const char* name, FileStat_Block * const stat_block) {
	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	dirCache.ExpandName(newname);
	normalize(newname,basedir);
	time_t mytime = PHYSFS_getLastModTime(newname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		stat_block->time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
		stat_block->date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
	} else {
		stat_block->time=DOS_PackTime(0,0,0);
		stat_block->date=DOS_PackDate(1980,1,1);
	}
	stat_block->size=(Bit32u)PHYSFS_fileLength(newname);
	return true;
}


Bit8u physfsDrive::GetMediaByte(void) {
	return allocation.mediaid;
}

bool physfsDrive::isRemote(void) {
	return false;
}

bool physfsDrive::isRemovable(void) {
	return false;
}

struct opendirinfo {
	char **files;
	int pos;
};
/* helper functions for drive cache */
bool physfsDrive::isdir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
	return PHYSFS_isDirectory(myname);
}

void *physfsDrive::opendir(const char *name) {
	char myname[CROSS_LEN];
	strcpy(myname,name);
	normalize(myname,basedir);
	if (!PHYSFS_isDirectory(myname)) return false;

	struct opendirinfo *oinfo = (struct opendirinfo *)malloc(sizeof(struct opendirinfo));
	oinfo->files = PHYSFS_enumerateFiles(myname);
	if (oinfo->files == NULL) {
		LOG_MSG("PHYSFS: nothing found for %s (%s)",myname,PHYSFS_getLastError());
		free(oinfo);
		return NULL;
	}

	oinfo->pos = (myname[1] == 0?0:-2);
	return (void *)oinfo;
}

void physfsDrive::closedir(void *handle) {
	struct opendirinfo *oinfo = (struct opendirinfo *)handle;
	if (handle == NULL) return;
	if (oinfo->files != NULL) PHYSFS_freeList(oinfo->files);
	free(oinfo);
}

bool physfsDrive::read_directory_first(void* dirp, char* entry_name, bool& is_directory) {
	return read_directory_next(dirp, entry_name, is_directory);
}

bool physfsDrive::read_directory_next(void* dirp, char* entry_name, bool& is_directory) {
	struct opendirinfo *oinfo = (struct opendirinfo *)dirp;
	if (!oinfo) return false;
	if (oinfo->pos == -2) {
		oinfo->pos++;
		safe_strncpy(entry_name,".",CROSS_LEN);
		is_directory = true;
		return true;
	}
	if (oinfo->pos == -1) {
		oinfo->pos++;
		safe_strncpy(entry_name,"..",CROSS_LEN);
		is_directory = true;
		return true;
	}
	if (!oinfo->files || !oinfo->files[oinfo->pos]) return false;
	safe_strncpy(entry_name,oinfo->files[oinfo->pos++],CROSS_LEN);
	is_directory = isdir(entry_name);
	return true;
}

extern std::string capturedir;
static Bit8u physfs_used = 0;
physfsDrive::physfsDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid) {

	char newname[CROSS_LEN+1];

	/* No writedir given, use capture directory */
	if(startdir[0] == ':') {
		strcpy(newname,capturedir.c_str());
		strcat(newname,startdir);
	} else {
		strcpy(newname,startdir);
	}

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
			int tmp = strlen(dir)-1;
			dir[tmp++] = ':';
			dir[tmp++] = CROSS_FILESPLIT;
			dir[tmp] = '\0';
		}
		if (*lastdir && PHYSFS_addToSearchPath(lastdir,true) == 0) {
			LOG_MSG("PHYSFS couldn't add '%s': %s",lastdir,PHYSFS_getLastError());
		}
		lastdir = dir;
		dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	}
	const char *oldwrite = PHYSFS_getWriteDir();
	if (oldwrite) oldwrite = strdup(oldwrite);
	if (!PHYSFS_setWriteDir(newname)) {
		if (!oldwrite)
			LOG_MSG("PHYSFS can't use '%s' for writing, you might encounter problems",newname);
		else
			PHYSFS_setWriteDir(oldwrite);
	}
	if (oldwrite) free((char *)oldwrite);
	
	strcpy(basedir,lastdir);

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

const char *physfsDrive::GetInfo() {
	char **files = PHYSFS_getSearchPath(), **list = files;
	sprintf(info,"PHYSFS directory %s in ",basedir);
	while (*files != NULL) {
		strcat(info,*files++);
		strcat(info,", ");
	}
	if (PHYSFS_getWriteDir() != NULL) {
		strcat(info,"writing to ");
		strcat(info,PHYSFS_getWriteDir());
	} else {
		strcat(info,"read-only");
	}
	PHYSFS_freeList(list);
	return info;
}


bool physfsFile::Read(Bit8u * data,Bit16u * size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {        // check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==WRITE) prepareRead();
	last_action=READ;
	PHYSFS_sint64 mysize = PHYSFS_read(fhandle,data,1,(PHYSFS_uint64)*size);
	//LOG_MSG("Read %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
	*size = (Bit16u)mysize;
	return true;
}

bool physfsFile::Write(Bit8u * data,Bit16u * size) {
	if ((this->flags & 0xf) == OPEN_READ) { // check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action==READ) prepareWrite();
	last_action=WRITE;
	if (*size==0) {
		if (PHYSFS_tell(fhandle) == 0) {
			PHYSFS_close(PHYSFS_openWrite(pname));
			//LOG_MSG("Truncate %s (%s)",name,PHYSFS_getLastError());
		} else {
			LOG_MSG("PHYSFS TODO: truncate not yet implemented (%s at %i)",pname,PHYSFS_tell(fhandle));
			return false;
		}
	} else {
		PHYSFS_sint64 mysize = PHYSFS_write(fhandle,data,1,(PHYSFS_uint64)*size);
		//LOG_MSG("Wrote %i bytes (wanted %i) at %i of %s (%s)",(int)mysize,(int)*size,(int)PHYSFS_tell(fhandle),name,PHYSFS_getLastError());
		*size = (Bit16u)mysize;
		return true;
	}
}
bool physfsFile::Seek(Bit32u * pos,Bit32u type) {
	PHYSFS_sint64 mypos = (Bit32s)*pos;
	switch (type) {
	case DOS_SEEK_SET:break;
	case DOS_SEEK_CUR:mypos += PHYSFS_tell(fhandle); break;
	case DOS_SEEK_END:mypos += PHYSFS_fileLength(fhandle);-mypos; break;
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

	*pos=(Bit32u)PHYSFS_tell(fhandle);
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
			LOG_MSG("PHYSFS copy-on-write failed: %s.",PHYSFS_getLastError());
			return false;
		}
		char buffer[65536];
		PHYSFS_sint64 size;
		PHYSFS_seek(fhandle, 0);
		while ((size = PHYSFS_read(fhandle,buffer,1,65536)) > 0) {
			if (PHYSFS_write(whandle,buffer,1,size) != size) {
				LOG_MSG("PHYSFS copy-on-write failed: %s.",PHYSFS_getLastError());
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
		int rc = fcntl(**(int**)fhandle->opaque,F_SETFL,0);
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

Bit16u physfsFile::GetInformation(void) {
	return info;
}
	

physfsFile::physfsFile(const char* _name, PHYSFS_file * handle,Bit16u devinfo, const char* physname, bool write) {
	fhandle=handle;
	info=devinfo;
	strcpy(pname,physname);
	time_t mytime = PHYSFS_getLastModTime(pname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
		this->date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
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
	time_t mytime = PHYSFS_getLastModTime(pname);
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=localtime(&mytime))!=0) {
		this->time=DOS_PackTime((Bit16u)time->tm_hour,(Bit16u)time->tm_min,(Bit16u)time->tm_sec);
		this->date=DOS_PackDate((Bit16u)(time->tm_year+1900),(Bit16u)(time->tm_mon+1),(Bit16u)time->tm_mday);
	} else {
		this->time=DOS_PackTime(0,0,0);
		this->date=DOS_PackDate(1980,1,1);
	}
	return true;
}


// ********************************************
// CDROM DRIVE
// ********************************************

int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, Bit8u& subUnit);
bool MSCDEX_HasMediaChanged(Bit8u subUnit);
bool MSCDEX_GetVolumeName(Bit8u subUnit, char* name);


physfscdromDrive::physfscdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error)
		   :physfsDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
};

const char *physfscdromDrive::GetInfo() {
	char **files = PHYSFS_getSearchPath(), **list = files;
	sprintf(info,"PHYSFS directory %s in ",basedir);
	while (*files != NULL) {
		strcat(info,*files++);
		strcat(info,", ");
	}
	strcat(info,"CD-ROM mode (read-only)");
	PHYSFS_freeList(list);
	return info;
}

bool physfscdromDrive::FileOpen(DOS_File * * file,const char * name,Bit32u flags)
{
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return physfsDrive::FileOpen(file,name,flags);
};

bool physfscdromDrive::FileCreate(DOS_File * * file,const char * name,Bit16u attributes)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::FileUnlink(const char * name)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::RemoveDir(const char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::MakeDir(const char * dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::Rename(const char * oldname,const char * newname)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::GetFileAttr(const char * name,Bit16u * attr)
{
	bool result = physfsDrive::GetFileAttr(name,attr);
	if (result) *attr |= DOS_ATTR_READ_ONLY;
	return result;
};

bool physfscdromDrive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return physfsDrive::FindFirst(_dir,dta);
};

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
};

bool physfscdromDrive::isRemote(void) {
	return true;
}

bool physfscdromDrive::isRemovable(void) {
	return true;
}

Bits physfscdromDrive::UnMount(void) {
	return true;
}

#endif // C_HAVE_PHYSFS
