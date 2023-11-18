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

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"
#include "inout.h"
#include "timer.h"
#include "logging.h"

#include <vector>
#include <string>
#include <cassert>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define OVERLAY_DIR 1
bool logoverlay = false;
extern int lfn_filefind_handle;
extern uint16_t ldid[256];
extern std::string ldir[256];
std::string prefix_overlay = ".DBOVERLAY";

using namespace std;

/* 
 * Wengier: Long filenames are supported in all including overlay drives.
 * Shift-JIS characters (Kana, Kanji, etc) are also supported in PC-98 mode.
 *
 * New rename for base directories (not yet supported):
 * Alter shortname in the drive_cache: take care of order and long names. 
 * update stored deleted files list in overlay. 
 */

//TODO recheck directories under linux with the filename_cache (as one adds the dos name (and runs cross_filename on the other))


//TODO Check: Maybe handle file redirection in ccc (opening the new file), (call update datetime host there ?)


/* For rename/delete(unlink)/makedir/removedir we need to rebuild the cache. (shouldn't be needed, 
 * but cacheout/delete entry currently throw away the cached folder and rebuild it on read. 
 * so we have to ensure the rebuilding is controlled through the overlay.
 * In order to not reread the overlay directory contents, the information in there is cached and updated when
 * it changes (when deleting a file or adding one)
 */


//directories that exist only in overlay can not be added to the drive_cache currently. 
//Either upgrade addentry to support directories. (without actually caching stuff in! (code in testing))
//Or create an empty directory in local drive base.

host_cnv_char_t *CodePageGuestToHost(const char *s);
char* GetCrossedName(const char *basedir, const char *dir);
bool isDBCSCP(), isKanji1(uint8_t chr), shiftjis_lead_byte(int c);

bool Overlay_Drive::RemoveDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}
	
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	//DOS_RemoveDir checks if directory exists.
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay: trying to remove directory: %s",dir);
#else
	E_Exit("Overlay: trying to remove directory: %s",dir);
#endif
	/* Overlay: Check if folder is empty (findfirst/next, skipping . and .. and breaking on first file found ?), if so, then it is not too tricky. */
	if (is_dir_only_in_overlay(dir)) {
		//The simple case
		char sdir[CROSS_LEN],odir[CROSS_LEN];
		strcpy(sdir,dir);
		strcpy(odir,overlaydir);
		strcat(odir,sdir);
		CROSS_FILENAME(odir);
		const host_cnv_char_t* host_name = CodePageGuestToHost(odir);
		int temp=-1;
		if (host_name!=NULL) {
#if defined (WIN32)
			temp=_wrmdir(host_name);
#else
			temp=rmdir(host_name);
#endif
		}
		if (temp) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dir));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(sdir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				strcpy(odir,overlaydir);
				strcat(odir,sdir);
				CROSS_FILENAME(odir);
				host_name = CodePageGuestToHost(odir);
				if (host_name!=NULL) {
#if defined (WIN32)
					temp=_wrmdir(host_name);
#else
					temp=rmdir(host_name);
#endif
				}
			}
		}
		if (temp == 0) {
			remove_DOSdir_from_cache(dir);
			char newdir[CROSS_LEN];
			strcpy(newdir,basedir);
			strcat(newdir,sdir);
			CROSS_FILENAME(newdir);
			dirCache.DeleteEntry(newdir,true);
			dirCache.EmptyCache();
			update_cache(false);
		}
		return (temp == 0);
	} else {
		uint16_t olderror = dos.errorcode; //FindFirst/Next always set an errorcode, while RemoveDir itself shouldn't touch it if successful
		DOS_DTA dta(dos.tables.tempdta);
		char stardotstar[4] = {'*', '.', '*', 0};
		dta.SetupSearch(0,(0xff & ~DOS_ATTR_VOLUME),stardotstar); //Fake drive as we don't use it.
		bool ret = this->FindFirst(dir,dta,false);// DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			//Path not found. Should not be possible due to removedir doing a testdir, but lets be correct
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
		bool empty = true;
		do {
			char name[DOS_NAMELENGTH_ASCII],lname[LFN_NAMELENGTH];uint32_t size,hsize;uint16_t date;uint16_t time;uint8_t attr;
			dta.GetResult(name,lname,size,hsize,date,time,attr);
			if (logoverlay) LOG_MSG("RemoveDir found %s",name);
			if (empty && strcmp(".",name ) && strcmp("..",name)) 
				empty = false; //Neither . or .. so directory not empty.
		} while ( (ret=this->FindNext(dta)) );
		//Always exhaust list, so drive_cache entry gets invalidated/reused.
		//FindNext is done, restore error code to old value. DOS_RemoveDir will set the right one if needed.
		dos.errorcode = olderror;

		if (!empty) return false;
		if (logoverlay) LOG_MSG("directory empty! Hide it.");
		//Directory is empty, mark it as deleted and create .DBOVERLAY file.
		//Ensure that overlap folder can not be created.
		char odir[CROSS_LEN];
		strcpy(odir,overlaydir);
		strcat(odir,dir);
		CROSS_FILENAME(odir);
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dir));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(odir,overlaydir);
			strcat(odir,temp_name);
			CROSS_FILENAME(odir);
			rmdir(odir);
		}
		add_deleted_path(dir,true);
		return true;
	}
}

bool Overlay_Drive::MakeDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
	//DOS_MakeDir tries first, before checking if the directory already exists, so doing it here as well, so that case is handled.
	if (TestDir(dir)) return false;
	if (overlap_folder == dir) return false; //TODO Test
#if OVERLAY_DIR
	if (logoverlay) LOG_MSG("Overlay trying to make directory: %s",dir);
#else
	E_Exit("Overlay trying to make directory: %s",dir);
#endif
	/* Overlay: Create in Overlay only and add it to drive_cache + some entries else the drive_cache will try to access it. Needs an AddEntry for directories. */ 

	//Check if leading dir is marked as deleted.
	if (check_if_leading_is_deleted(dir)) return false;

	//Check if directory itself is marked as deleted
	if (is_deleted_path(dir) && localDrive::TestDir(dir)) {
		//Was deleted before and exists (last one is safety check)
		remove_deleted_path(dir,true);
		return true;
	}
	char newdir[CROSS_LEN],sdir[CROSS_LEN],pdir[CROSS_LEN];
	strcpy(sdir,dir);
	char *p=strrchr_dbcs(sdir,'\\');
	if (p!=NULL) {
		*p=0;
		char *temp_name=dirCache.GetExpandName(GetCrossedName(basedir,sdir));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			strcpy(newdir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
			strcat(newdir,"\\");
			strcat(newdir,p+1);
			strcpy(sdir,newdir);
		}
	}
	strcpy(newdir,overlaydir);
	strcat(newdir,sdir);
	CROSS_FILENAME(newdir);
	p=strrchr_dbcs(sdir,'\\');
	int temp=-1;
	bool madepdir=false;
	const host_cnv_char_t* host_name;
	if (p!=NULL) {
		*p=0;
		if (strlen(sdir)) {
			strcpy(pdir,overlaydir);
			strcat(pdir,sdir);
			CROSS_FILENAME(pdir);
			if (!is_deleted_path(sdir) && localDrive::TestDir(sdir)) {
				host_name = CodePageGuestToHost(pdir);
				if (host_name!=NULL) {
#if defined (WIN32)
					temp=_wmkdir(host_name);
					if (temp) temp=_wmkdir_p(host_name);
#else
					temp=mkdir(host_name,0775);
					if (temp) temp=mkdir_p(host_name,0775);
#endif
					if (temp==0) madepdir=true;
				}
			}
		}
	}
	temp=-1;
	host_name = CodePageGuestToHost(newdir);
	if (host_name!=NULL) {
#if defined (WIN32)
		temp=_wmkdir(host_name);
		if (temp) temp=_wmkdir_p(host_name);
#else
		temp=mkdir(host_name,0775);
		if (temp) temp=mkdir_p(host_name,0775);
#endif
	}
	if (temp==0) {
		char fakename[CROSS_LEN];
		strcpy(fakename,basedir);
		strcat(fakename,dir);
		CROSS_FILENAME(fakename);
		strcpy(sdir, dir);
		upcase(sdir);
		dirCache.AddEntryDirOverlay(fakename,sdir,true);
		add_DOSdir_to_cache(dir,sdir);
		dirCache.EmptyCache();
		update_cache(true);
	} else if (madepdir) {
		host_name = CodePageGuestToHost(pdir);
		if (host_name!=NULL) {
#if defined (WIN32)
			_wrmdir(host_name);
#else
			rmdir(host_name);
#endif
		}
	}

	return (temp == 0);// || ((temp!=0) && (errno==EEXIST));
}

bool Overlay_Drive::TestDir(const char * dir) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	//First check if directory exist exclusively in the overlay. 
	//Currently using the update_cache cache, alternatively access the directory itself.

	//Directories are stored without a trailing backslash
	char tempdir[CROSS_LEN];
	strcpy(tempdir,dir);
	size_t templen = strlen(dir);
	if (templen && tempdir[templen-1] == '\\') tempdir[templen-1] = 0;

#if OVERLAY_DIR
	if (is_dir_only_in_overlay(tempdir)) return true;
#endif

	//Next Check if the directory is marked as deleted or one of its leading directories is.
	//(it still might exists in the localDrive)

	if (is_deleted_path(tempdir)) return false; 

	// Not exclusive to overlay nor marked as deleted. Pass on to LocalDrive
	return localDrive::TestDir(dir);
}


class OverlayFile: public localFile {
public:
	OverlayFile(const char* name, FILE * handle):localFile(name,handle){
		overlay_active = false;
		if (logoverlay) LOG_MSG("constructing OverlayFile: %s",name);
	}
	bool Write(const uint8_t * data,uint16_t * size) {
		uint32_t f = flags&0xf;
		if (!overlay_active && (f == OPEN_READWRITE || f == OPEN_WRITE)) {
			if (logoverlay) LOG_MSG("write detected, switching file for %s",GetName());
			if (*data == 0) {
				if (logoverlay) LOG_MSG("OPTIMISE: truncate on switch!!!!");
			}
			uint32_t a = GetTicks();
			bool r = create_copy();
			if (GetTicks() - a > 2) {
				if (logoverlay) LOG_MSG("OPTIMISE: switching took %d",GetTicks() - a);
			}
			if (!r) return false;
			overlay_active = true;
			
		}
		return localFile::Write(data,size);
	}
	bool create_copy();
//private:
	bool overlay_active;
};

//Create leading directories of a file being overlaid if they exist in the original (localDrive).
//This function is used to create copies of existing files, so all leading directories exist in the original.

FILE* Overlay_Drive::create_file_in_overlay(const char* dos_filename, char const* mode) {
	char newname[CROSS_LEN];
	strcpy(newname,overlaydir); //TODO GOG make part of class and join in 
	strcat(newname,dos_filename); //HERE we need to convert it to Linux TODO
	CROSS_FILENAME(newname);

#ifdef host_cnv_use_wchar
    wchar_t wmode[8];
    unsigned int tis;
    for (tis=0;tis < 7 && mode[tis] != 0;tis++) wmode[tis] = (wchar_t)mode[tis];
    assert(tis < 7); // guard
    wmode[tis] = 0;
#endif
	FILE* f;
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
	if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
		f = _wfopen(host_name,wmode);
#else
		f = fopen_wrap(host_name,mode);
#endif
	}
	else {
		f = fopen_wrap(newname,mode);
	}
	//Check if a directory is part of the name:
	char* dir = strrchr_dbcs((char *)dos_filename,'\\');
	if (!f && dir && *dir) {
		if (logoverlay) LOG_MSG("Overlay: warning creating a file inside a directory %s",dos_filename);
		//ensure they exist, else make them in the overlay if they exist in the original....
		Sync_leading_dirs(dos_filename);
		//try again
		char temp_name[CROSS_LEN],tmp[CROSS_LEN];
		strcpy(tmp, dos_filename);
		char *p=strrchr_dbcs(tmp, '\\');
		assert(p!=NULL);
		*p=0;
		bool found=false;
		for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2)
			if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
				found=true;
				strcpy(tmp, (*it).c_str());
				break;
			}
		if (found) {
			strcpy(temp_name,overlaydir);
			strcat(temp_name,tmp);
			strcat(temp_name,dir);
			CROSS_FILENAME(temp_name);
			const host_cnv_char_t* host_name = CodePageGuestToHost(temp_name);
			if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
				f = _wfopen(host_name,wmode);
#else
				f = fopen_wrap(host_name,mode);
#endif
			}
			else {
				f = fopen_wrap(temp_name,mode);
			}
		}
		if (!f) {
			strcpy(temp_name,dirCache.GetExpandName(GetCrossedName(basedir,dos_filename)));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(newname,overlaydir);
				strcat(newname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				CROSS_FILENAME(newname);
			}
			const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
			if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
				f = _wfopen(host_name,wmode);
#else
				f = fopen_wrap(host_name,mode);
#endif
			}
			else {
				f = fopen_wrap(newname,mode);
			}
		}
	}

	return f;
}

#ifndef BUFSIZ
#define BUFSIZ 2048
#endif

bool OverlayFile::create_copy() {
	//test if open/valid/etc
	//ensure file position
	FILE* lhandle = this->fhandle;
	fseek(lhandle,ftell(lhandle),SEEK_SET);
	int location_in_old_file = ftell(lhandle);
	fseek(lhandle,0L,SEEK_SET);
	
	FILE* newhandle = NULL;
	uint8_t drive_set = GetDrive();
	if (drive_set != 0xff && drive_set < DOS_DRIVES && Drives[drive_set]){
		Overlay_Drive* od = dynamic_cast<Overlay_Drive*>(Drives[drive_set]);
		if (od) {
			newhandle = od->create_file_in_overlay(GetName(),"wb+"); //todo check wb+
		}
	}
 
	if (!newhandle) return false;
	char buffer[BUFSIZ];
	size_t s;
	while ( (s = fread(buffer,1,BUFSIZ,lhandle)) != 0 ) fwrite(buffer, 1, s, newhandle);
	fclose(lhandle);
	//Set copied file handle to position of the old one 
	fseek(newhandle,location_in_old_file,SEEK_SET);
	this->fhandle = newhandle;
	//Flags ?
	return true;
}



static OverlayFile* ccc(DOS_File* file) {
	localFile* l = dynamic_cast<localFile*>(file);
	if (!l) E_Exit("overlay input file is not a localFile");
	//Create an overlayFile
	OverlayFile* ret = new OverlayFile(l->GetName(),l->fhandle);
	ret->flags = l->flags;
	ret->refCtr = l->refCtr;
	delete l;
	return ret;
}

Overlay_Drive::Overlay_Drive(const char * startdir,const char* overlay, uint16_t _bytes_sector,uint8_t _sectors_cluster,uint16_t _total_clusters,uint16_t _free_clusters,uint8_t _mediaid,uint8_t &error,std::vector<std::string> &options)
:localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid,options),special_prefix(prefix_overlay.c_str()) {
	optimize_cache_v1 = true; //Try to not reread overlay files on deletes. Ideally drive_cache should be improved to handle deletes properly.
	//Currently this flag does nothing, as the current behavior is to not reread due to caching everything.
#if defined (WIN32)	
	if (strcasecmp(startdir,overlay) == 0) {
#else 
	if (strcmp(startdir,overlay) == 0) {
#endif
		//overlay directory can not be the base directory
		error = 2;
		return;
	}

	std::string s(startdir);
	std::string o(overlay);
	bool s_absolute = Cross::IsPathAbsolute(s);
	bool o_absolute = Cross::IsPathAbsolute(o);
	error = 0;
	if (s_absolute != o_absolute) { 
		error = 1;
		return;
	}
	strcpy(overlaydir,overlay);
	char dirname[CROSS_LEN] = { 0 };
	//Determine if overlaydir is part of the startdir.
	convert_overlay_to_DOSname_in_base(dirname);

	size_t dirlen = strlen(dirname);
	if(dirlen && dirname[dirlen - 1] == '\\') dirname[dirlen - 1] = 0;
			
	//add_deleted_path(dirname); //update_cache will add the overlap_folder
	overlap_folder = dirname;

	update_cache(true);
}

void Overlay_Drive::convert_overlay_to_DOSname_in_base(char* dirname ) 
{
	dirname[0] = 0;//ensure good return string
	if (strlen(overlaydir) >= strlen(basedir) ) {
		//Needs to be longer at least.
#if defined (WIN32)
//OS2 ?	
		if (strncasecmp(overlaydir,basedir,strlen(basedir)) == 0) {
#else
		if (strncmp(overlaydir,basedir,strlen(basedir)) == 0) {
#endif
			//Beginning is the same.
			char t[CROSS_LEN];
			strcpy(t,overlaydir+strlen(basedir));

			char* p = t;
			char* b = t;

			while ( (p =strchr_dbcs(p,CROSS_FILESPLIT)) ) {
				char directoryname[CROSS_LEN]={0};
				char dosboxdirname[CROSS_LEN]={0};
				strcpy(directoryname,dirname);
				strncat(directoryname,b,p-b);

				char d[CROSS_LEN];
				strcpy(d,basedir);
				strcat(d,directoryname);
				CROSS_FILENAME(d);
				//Try to find the corresponding directoryname in DOSBox.
				if(!dirCache.GetShortName(d,dosboxdirname) ) {
					//Not a long name, assume it is a short name instead
					strncpy(dosboxdirname,b,p-b);
					upcase(dosboxdirname);
				}


				strcat(dirname,dosboxdirname);
				strcat(dirname,"\\");

				if (logoverlay) LOG_MSG("HIDE directory: %s",dirname);

				b=++p;

			}
		}
	}
}

bool Overlay_Drive::FileOpen(DOS_File * * file,const char * name,uint32_t flags) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

    if (ovlreadonly) {
        if ((flags&0xf) == OPEN_WRITE || (flags&0xf) == OPEN_READWRITE) {
            DOS_SetError(DOSERR_WRITE_PROTECTED);
            return false;
        }
    }
	const host_cnv_char_t * type;
	switch (flags&0xf) {
	case OPEN_READ:        type = _HT("rb"); break;
	case OPEN_WRITE:       type = _HT("rb+"); break;
	case OPEN_READWRITE:   type = _HT("rb+"); break;
	case OPEN_READ_NO_MOD: type = _HT("rb"); break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}

	//Flush the buffer of handles for the same file. (Betrayal in Antara)
	uint8_t i,drive = DOS_DRIVES;
	localFile *lfp;
	for (i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]==this) {
			drive=i;
			break;
		}
	}
	if (!dos_kernel_disabled)
	for (i=0;i<DOS_FILES;i++) {
		if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive()==drive && Files[i]->IsName(name)) {
			lfp=dynamic_cast<localFile*>(Files[i]);
			if (lfp) lfp->Flush();
		}
	}


	//Todo check name first against local tree
	//if name exists, use that one instead!
	//overlay file.
	char newname[CROSS_LEN];
	strcpy(newname,overlaydir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	const host_cnv_char_t* host_name = CodePageGuestToHost(newname);
#ifdef host_cnv_use_wchar
    FILE * hand = _wfopen(host_name,type);
#else
	FILE * hand = fopen_wrap(newname,type);
#endif
	if (!hand) {
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(newname,overlaydir);
			strcat(newname,temp_name);
			CROSS_FILENAME(newname);
			host_name = CodePageGuestToHost(newname);
			if (host_name != NULL) {
#ifdef host_cnv_use_wchar
				hand = _wfopen(host_name,type);
#else
				hand = fopen_wrap(newname,type);
#endif
			}
		}
	}
	bool fileopened = false;
	if (hand) {
		if (logoverlay) LOG_MSG("overlay file opened %s",newname);
		*file=new localFile(name,hand);
		(*file)->flags=flags;
		fileopened = true;
	} else {
		; //TODO error handling!!!! (maybe check if it exists and read only (should not happen with overlays)
	}
	bool overlaid = fileopened;

	//File not present in overlay, try normal drive
	//TODO take care of file being marked deleted.

	if (!fileopened && !is_deleted_file(name)) fileopened = localDrive::FileOpen(file,name, OPEN_READ);


	if (fileopened) {
		if (logoverlay) LOG_MSG("file opened %s",name);
		//Convert file to OverlayFile
		OverlayFile* f = ccc(*file);
		f->flags = flags; //ccc copies the flags of the localfile, which were not correct in this case
		f->overlay_active = overlaid; //No need to switch if already in overlay.
		*file = f;
	}
	return fileopened;
}

bool Overlay_Drive::FileCreate(DOS_File * * file,const char * name,uint16_t /*attributes*/) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}
	
    if (ovlreadonly) {
		DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }
	//TODO Check if it exists in the dirCache ? // fix addentry ?  or just double check (ld and overlay)
	//AddEntry looks sound to me.. 
	
	//check if leading part of filename is a deleted directory
	if (check_if_leading_is_deleted(name)) return false;

	FILE* f = create_file_in_overlay(name,"wb+");
	if(!f) {
		if (logoverlay) LOG_MSG("File creation in overlay system failed %s",name);
		return false;
	}
	*file = new localFile(name,f);
	(*file)->flags = OPEN_READWRITE;
	OverlayFile* of = ccc(*file);
	of->overlay_active = true;
	of->flags = OPEN_READWRITE;
	*file = of;
	//create fake name for the drive cache
	char fakename[CROSS_LEN];
	strcpy(fakename,basedir);
	strcat(fakename,name);
	CROSS_FILENAME(fakename);
	dirCache.AddEntry(fakename,true); //add it.
	add_DOSname_to_cache(name);
	remove_deleted_file(name,true);
	return true;
}

void Overlay_Drive::add_DOSname_to_cache(const char* name) {
	for (std::vector<std::string>::const_iterator itc = DOSnames_cache.begin(); itc != DOSnames_cache.end(); ++itc){
		if (!strcasecmp((*itc).c_str(), name)) return;
	}
	DOSnames_cache.push_back(name);
}

void Overlay_Drive::remove_DOSname_from_cache(const char* name) {
	for (std::vector<std::string>::iterator it = DOSnames_cache.begin(); it != DOSnames_cache.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) { DOSnames_cache.erase(it); return;}
	}

}

bool Overlay_Drive::Sync_leading_dirs(const char* dos_filename){
	const char* lastdir = strrchr_dbcs((char *)dos_filename,'\\');
	//If there are no directories, return success.
	if (!lastdir) return true; 
	
	const char* leaddir = dos_filename;
	while ( (leaddir=strchr_dbcs((char *)leaddir,'\\')) != 0) {
		char dirname[CROSS_LEN] = {0};
		strncpy(dirname,dos_filename,leaddir-dos_filename);

		if (logoverlay) LOG_MSG("syncdir: %s",dirname);
		//Test if directory exist in base.
		char dirnamebase[CROSS_LEN] ={0};
		strcpy(dirnamebase,basedir);
		strcat(dirnamebase,dirname);
		CROSS_FILENAME(dirnamebase);
		struct stat basetest;
		if (stat(dirCache.GetExpandName(dirnamebase),&basetest) == 0 && basetest.st_mode & S_IFDIR) {
			if (logoverlay) LOG_MSG("base exists: %s",dirnamebase);
			//Directory exists in base folder.
			//Ensure it exists in overlay as well

			struct stat overlaytest;
			char dirnameoverlay[CROSS_LEN] ={0};
			strcpy(dirnameoverlay,overlaydir);
			strcat(dirnameoverlay,dirname);
			CROSS_FILENAME(dirnameoverlay);
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,dirname));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(dirnameoverlay,overlaydir);
				strcat(dirnameoverlay,temp_name);
				CROSS_FILENAME(dirnameoverlay);
			}
			if (stat(dirnameoverlay,&overlaytest) == 0 ) {
				//item exist. Check if it is a folder, if not a folder =>fail!
				if ((overlaytest.st_mode & S_IFDIR) ==0) return false;
			} else {
				//folder does not exist, make it
				if (logoverlay) LOG_MSG("creating %s",dirnameoverlay);
#if defined (WIN32)						/* MS Visual C++ */
				int temp = mkdir(dirnameoverlay);
#else
				int temp = mkdir(dirnameoverlay,0775);
#endif
				if (temp != 0) return false;
			}
		}
		leaddir = leaddir + 1; //Move to next
	} 

	return true;
}

void Overlay_Drive::update_cache(bool read_directory_contents) {
	uint32_t a = GetTicks();
	std::vector<std::string> specials;
	std::vector<std::string> dirnames;
	std::vector<std::string> filenames;
	if (read_directory_contents) {
		//Clear all lists
		DOSnames_cache.clear();
		DOSdirs_cache.clear();
		deleted_files_in_base.clear();
		deleted_paths_in_base.clear();
		//Ensure hiding of the folder that contains the overlay, if it is part of the base folder.
		add_deleted_path(overlap_folder.c_str(), false);
	}

	//Needs later to support stored renames and removals of files existing in the localDrive plane.
	//and by taking in account if the file names are actually already renamed. 
	//and taking in account that a file could have gotten an overlay version and then both need to be removed. 
	//
	//Also what about sequences were a base file gets copied to a working save game and then removed/renamed...
	//copy should be safe as then the link with the original doesn't exist.
	//however the working safe can be rather complicated after a rename and delete..

	//Currently directories existing only in the overlay can not be added to drive cache:
	//1. possible workaround create empty directory in base. Drawback would break the no-touching-of-base.
	//2. double up Addentry to support directories, (and adding . and .. to the newly directory so it counts as cachedin.. and won't be recached, as otherwise
	//   cache will realize we are faking it. 
	//Working on solution 2.

	//Random TODO: Does the root drive under DOS have . and .. ? 

	//This function needs to be called after any localDrive function calling cacheout/deleteentry, as those throw away directories.
	//either do this with a parameter stating the part that needs to be rebuild,(directory) or clear the cache by default and do it all.

	std::vector<std::string>::iterator i;
	std::string::size_type const prefix_lengh = special_prefix.length();
	if (read_directory_contents) {
		void* dirp = opendir(overlaydir);
		if (dirp == NULL) return;
		// Read complete directory
		char dir_name[CROSS_LEN], dir_sname[CROSS_LEN];
		bool is_directory;
		if (read_directory_first(dirp, dir_name, dir_sname, is_directory)) {
			if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.emplace_back(dir_name);
			else if (is_directory) {
				dirnames.emplace_back(dir_name);
				if (!strlen(dir_sname)) {
					strcpy(dir_sname, dir_name);
					upcase(dir_sname);
				}
				dirnames.emplace_back(dir_sname);
			} else filenames.emplace_back(dir_name);
			while (read_directory_next(dirp, dir_name, dir_sname, is_directory)) {
				if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.emplace_back(dir_name);
				else if (is_directory) {
					dirnames.emplace_back(dir_name);
					if (!strlen(dir_sname)) {
						strcpy(dir_sname, dir_name);
						upcase(dir_sname);
					}
					dirnames.emplace_back(dir_sname);
				} else filenames.emplace_back(dir_name);
			}
		}
		closedir(dirp);
		//parse directories to add them.

		for (i = dirnames.begin(); i != dirnames.end(); i+=2) {
			if ((*i) == ".") continue;
			if ((*i) == "..") continue;
			std::string testi(*i);
			std::string::size_type ll = testi.length();
			//TODO: Use the dirname\. and dirname\.. for creating fake directories in the driveCache.
			if( ll >2 && testi[ll-1] == '.' && testi[ll-2] == CROSS_FILESPLIT) continue; 
			if( ll >3 && testi[ll-1] == '.' && testi[ll-2] == '.' && testi[ll-3] == CROSS_FILESPLIT) continue;

#if OVERLAY_DIR
			char tdir[CROSS_LEN],sdir[CROSS_LEN];
			strcpy(tdir,(*i).c_str());
			strcpy(sdir,(*(i+1)).c_str());
			CROSS_DOSFILENAME(tdir);
			bool dir_exists_in_base = localDrive::TestDir(tdir);
#endif

			char dir[CROSS_LEN];
			strcpy(dir,overlaydir);
			strcat(dir,(*i).c_str());
			char dirpush[CROSS_LEN],dirspush[CROSS_LEN];
			strcpy(dirpush,(*i).c_str());
			strcpy(dirspush,(*(i+1)).c_str());
			if (!strlen(dirspush)) {
				strcpy(dirspush, dirpush);
				upcase(dirspush);
			}
			static char end[2] = {CROSS_FILESPLIT,0};
			strcat(dirpush,end); //Linux ?
			strcat(dirspush,end);
			void* dirp = opendir(dir);
			if (dirp == NULL) continue;

#if OVERLAY_DIR
			//Good directory, add to DOSdirs_cache if not existing in localDrive. tested earlier to prevent problems with opendir
			if (!dir_exists_in_base) {
				if (!strlen(sdir)) {
					strcpy(sdir, tdir);
					upcase(sdir);
				}
				add_DOSdir_to_cache(tdir,sdir);
			}
#endif

			std::string backupi(*i);
			// Read complete directory
			char dir_name[CROSS_LEN], dir_sname[CROSS_LEN];
			bool is_directory;
			if (read_directory_first(dirp, dir_name, dir_sname, is_directory)) {
				if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
				else if (is_directory) {
					dirnames.push_back(string(dirpush)+dir_name);
					if (!strlen(dir_sname)) {
						strcpy(dir_sname, dir_name);
						upcase(dir_sname);
					}
					dirnames.push_back(string(dirspush)+dir_sname);
				} else filenames.push_back(string(dirpush)+dir_name);
				while (read_directory_next(dirp, dir_name, dir_sname, is_directory)) {
					if ((strlen(dir_name) > prefix_lengh+5) && strncmp(dir_name,special_prefix.c_str(),prefix_lengh) == 0) specials.push_back(string(dirpush)+dir_name);
					else if (is_directory) {
						dirnames.push_back(string(dirpush)+dir_name);
						if (!strlen(dir_sname)) {
							strcpy(dir_sname, dir_name);
							upcase(dir_sname);
						}
						dirnames.push_back(string(dirspush)+dir_sname);
					} else filenames.push_back(string(dirspush)+dir_name);
				}
			}
			closedir(dirp);
			for(i = dirnames.begin(); i != dirnames.end(); i+=2) {
				if ( (*i) == backupi) break; //find current directory again, for the next round.
			}
		}
	}

	if (read_directory_contents) {
		for( i = filenames.begin(); i != filenames.end(); ++i) {
			char dosname[CROSS_LEN];
			strcpy(dosname,(*i).c_str());
			//upcase(dosname);  //Should not be really needed, as uppercase in the overlay is a requirement...
			CROSS_DOSFILENAME(dosname);
			if (logoverlay) LOG_MSG("update cache add dosname %s",dosname);
			DOSnames_cache.emplace_back(dosname);
		}
	}

#if OVERLAY_DIR
	for (i = DOSdirs_cache.begin(); i !=DOSdirs_cache.end(); i+=2) {
		char fakename[CROSS_LEN],sdir[CROSS_LEN],tmp[CROSS_LEN],*p;
		strcpy(fakename,basedir);
		strcat(fakename,(*i).c_str());
		CROSS_FILENAME(fakename);
		strcpy(sdir,(*(i+1)).c_str());
		dirCache.AddEntryDirOverlay(fakename,sdir,true);
		if (strlen(sdir)) {
			strcpy(tmp,(*(i+1)).c_str());
			p=strrchr_dbcs(tmp, '\\');
			if (p==NULL) *(i+1)=std::string(sdir);
			else {
				*p=0;
				for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it<i && it != DOSdirs_cache.end(); it+=2) {
					if (!strcasecmp((*it).c_str(), tmp)) {
						strcpy(tmp, (*(it+1)).c_str());
						break;
					}
				}
				strcat(tmp,"\\");
				strcat(tmp,sdir);
				*(i+1)=std::string(tmp);
			}
		}
	}
#endif

	for (i = DOSnames_cache.begin(); i != DOSnames_cache.end(); ++i) {
		char fakename[CROSS_LEN];
		strcpy(fakename,basedir);
		strcat(fakename,(*i).c_str());
		CROSS_FILENAME(fakename);
		dirCache.AddEntry(fakename,true);
	}

	if (read_directory_contents) {
		for (i = specials.begin(); i != specials.end(); ++i) {
			//Specials look like this .DBOVERLAY_YYY_FILENAME.EXT or DIRNAME[\/].DBOVERLAY_YYY_FILENAME.EXT where
			//YYY is the operation involved. Currently only DEL is supported.
			//DEL = file marked as deleted, (but exists in localDrive!)
			std::string name(*i);
			std::string special_dir("");
			std::string special_file("");
			std::string special_operation("");
			std::string::size_type s = name.find(special_prefix);
			if (s == std::string::npos) continue;
			if (s) {
				special_dir = name.substr(0,s);
				name.erase(0,s);
			}
			name.erase(0,special_prefix.length()+1); //Erase .DBOVERLAY_
			s = name.find('_');
			if (s == std::string::npos || s == 0) continue;
			special_operation = name.substr(0,s);
			name.erase(0,s + 1);
			special_file = name;
			if (special_file.length() == 0) continue;
			if (special_operation == "DEL") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				
				add_deleted_file(name.c_str(),false);
			} else if (special_operation == "RMD") {
				name = special_dir + special_file;
				//CROSS_DOSFILENAME for strings:
				while ( (s = name.find('/')) != std::string::npos) name.replace(s,1,"\\");
				add_deleted_path(name.c_str(),false);

			} else {
				if (logoverlay) LOG_MSG("unsupported operation %s on %s",special_operation.c_str(),(*i).c_str());
			}

		}
	}
	if (logoverlay) LOG_MSG("OPTIMISE: update cache took %d",GetTicks()-a);
}

bool Overlay_Drive::FindNext(DOS_DTA & dta) {

	char * dir_ent, *ldir_ent;
	ht_stat_t stat_block;
	char full_name[CROSS_LEN], lfull_name[LFN_NAMELENGTH+1];
	char dir_entcopy[CROSS_LEN], ldir_entcopy[CROSS_LEN];

	uint8_t srch_attr;char srch_pattern[LFN_NAMELENGTH+1];
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

	strcpy(full_name,lfn_filefind_handle>=LFN_FILEFIND_MAX?srchInfo[id].srch_dir:(ldir[lfn_filefind_handle]!=""?ldir[lfn_filefind_handle].c_str():"\\"));
	strcpy(lfull_name,full_name);

	strcat(full_name,dir_ent);
    strcat(lfull_name,ldir_ent);
	
	//GetExpandName might indirectly destroy dir_ent (by caching in a new directory 
	//and due to its design dir_ent might be lost.)
	//Copying dir_ent first
	strcpy(dir_entcopy,dir_ent);
    strcpy(ldir_entcopy,ldir_ent);
	
	//First try overlay:
	char ovname[CROSS_LEN];
	char relativename[CROSS_LEN];
	strcpy(relativename,srchInfo[id].srch_dir);
	//strip off basedir: //TODO cleanup
	strcpy(ovname,overlaydir);
	char* prel = lfull_name + strlen(basedir);

	char preldos[CROSS_LEN];
	strcpy(preldos,prel);
	CROSS_DOSFILENAME(preldos);
	strcat(ovname,prel);
	bool statok = false;
	const host_cnv_char_t* host_name=NULL;
	if (!is_deleted_file(preldos)) {
		host_name = CodePageGuestToHost(ovname);
		if (host_name != NULL) statok = ht_stat(host_name,&stat_block)==0;
		if (!statok) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,prel));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(ovname,GetCrossedName(overlaydir,temp_name));
				host_name = CodePageGuestToHost(ovname);
				if (host_name != NULL) statok = ht_stat(host_name,&stat_block)==0;
			}
		}
	}

	if (statok) {
		if (logoverlay) LOG_MSG("using overlay data for %s : %s",uselfn?lfull_name:full_name, ovname);
	} else {
		strcpy(preldos,prel);
		CROSS_DOSFILENAME(preldos);
		if (is_deleted_file(preldos)) { //dir.. maybe lower or keep it as is TODO
			if (logoverlay) LOG_MSG("skipping deleted file %s %s %s",preldos,uselfn?lfull_name:full_name,ovname);
			goto again;
		}
		const host_cnv_char_t* host_name = CodePageGuestToHost(dirCache.GetExpandName(lfull_name));
		if (ht_stat(host_name,&stat_block)!=0) {
			if (logoverlay) LOG_MSG("stat failed for %s . This should not happen.",dirCache.GetExpandName(uselfn?lfull_name:full_name));
			goto again;//No symlinks and such
		}
	}

	if(stat_block.st_mode & S_IFDIR) find_attr=DOS_ATTR_DIRECTORY;
	else find_attr=0;
#if defined (WIN32)
	Bitu attribs = host_name==NULL?INVALID_FILE_ATTRIBUTES:GetFileAttributesW(host_name);
	if (attribs != INVALID_FILE_ATTRIBUTES)
		find_attr|=attribs&0x3f;
#else
	bool isdir = find_attr & DOS_ATTR_DIRECTORY;
	if (!isdir) find_attr|=DOS_ATTR_ARCHIVE;
	if(!(stat_block.st_mode & S_IWUSR)) find_attr|=DOS_ATTR_READ_ONLY;
    std::string fname = create_filename_of_special_operation(host_name, "ATR");
    if (ht_stat(fname.c_str(),&stat_block)==0) {
        unsigned int len = stat_block.st_size;
        if (len & 1) {
            if (isdir)
                find_attr|=DOS_ATTR_ARCHIVE;
            else
                find_attr&=~DOS_ATTR_ARCHIVE;
        }
        if (len & 2) find_attr|=DOS_ATTR_HIDDEN;
        if (len & 4) find_attr|=DOS_ATTR_SYSTEM;
    }
#endif
 	if (~srch_attr & find_attr & DOS_ATTR_DIRECTORY) goto again;
	
	/* file is okay, setup everything to be copied in DTA Block */
	char find_name[DOS_NAMELENGTH_ASCII], lfind_name[LFN_NAMELENGTH+1];
	uint16_t find_date,find_time;uint32_t find_size, find_hsize;

	if(strlen(dir_entcopy)<DOS_NAMELENGTH_ASCII){
		strcpy(find_name,dir_entcopy);
		upcase(find_name);
	}
	strcpy(lfind_name,ldir_entcopy);
    lfind_name[LFN_NAMELENGTH]=0;

	find_hsize=(uint32_t) (stat_block.st_size / 0x100000000);
	find_size=(uint32_t) (stat_block.st_size % 0x100000000);
	struct tm *time;
	if((time=
#if defined(__MINGW32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    _localtime64
#else
    localtime
#endif
    (&stat_block.st_mtime))!=0){
		find_date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
		find_time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
	} else {
		find_time=6; 
		find_date=4;
	}
	dta.SetResult(find_name,lfind_name,find_size,find_hsize,find_date,find_time,find_attr);
	return true;
}

bool Overlay_Drive::FileUnlink(const char * name) {
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

//TODO check the basedir for file existence in order if we need to add the file to deleted file list.
	uint32_t a = GetTicks();
	if (logoverlay) LOG_MSG("calling unlink on %s",name);
	char basename[CROSS_LEN];
	strcpy(basename,basedir);
	strcat(basename,name);
	CROSS_FILENAME(basename);

	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	
	bool removed=false;
	const host_cnv_char_t* host_name;
	struct stat temp_stat;
	if (stat(overlayname,&temp_stat)) {
		char* temp_name = dirCache.GetExpandName(basename);
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			char overtmpname[CROSS_LEN];
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overtmpname,overlaydir);
			strcat(overtmpname,temp_name);
			CROSS_FILENAME(overtmpname);
			host_name = CodePageGuestToHost(overtmpname);
			if (host_name != NULL && ht_unlink(host_name)==0) removed=true;
		}
	}
//	char *fullname = dirCache.GetExpandName(newname);

	if (!removed&&unlink(overlayname)) {
		//Unlink failed for some reason try finding it.
		ht_stat_t status;
		host_name = CodePageGuestToHost(overlayname);
		if(host_name==NULL || ht_stat(host_name,&status)) {
			//file not found in overlay, check the basedrive
			//Check if file not already deleted 
			if (is_deleted_file(name)) return false;

			char *fullname = dirCache.GetExpandName(basename);
			host_name = CodePageGuestToHost(fullname);
			if (host_name==NULL || ht_stat(host_name,&status)) return false; // File not found in either, return file false.
			//File does exist in normal drive.
			//Maybe do something with the drive_cache.
			add_deleted_file(name,true);
			return true;
//			E_Exit("trying to remove existing non-overlay file %s",name);
		}
		FILE* file_writable = fopen_wrap(overlayname,"rb+");
		if(!file_writable) return false; //No access ? ERROR MESSAGE NOT SET. FIXME ?
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
		if (unlink(overlayname) == 0) { //Overlay file removed
			//Mark basefile as deleted if it exists:
			if (localDrive::FileExists(name)) add_deleted_file(name,true);
#if !defined (WIN32)
			remove_special_file_from_disk(name, "ATR");
#endif
			remove_DOSname_from_cache(name); //Should be an else ? although better safe than sorry.
			//Handle this better
			dirCache.DeleteEntry(basename);
			dirCache.EmptyCache();
			update_cache(false);
			//Check if it exists in the base dir as well
			
			return true;
		}
		return false;
	} else { //Removed from overlay.
		//TODO IF it exists in the basedir: and more locations above.
		if (localDrive::FileExists(name)) add_deleted_file(name,true);
#if !defined (WIN32)
		remove_special_file_from_disk(name, "ATR");
#endif
		remove_DOSname_from_cache(name);
		//TODODO remove from the update_cache cache as well
		//Handle this better
		//Check if it exists in the base dir as well
		dirCache.DeleteEntry(basename);
		dirCache.EmptyCache();
		update_cache(false);
		if (logoverlay) LOG_MSG("OPTIMISE: unlink took %d",GetTicks()-a);
		return true;
	}
}

bool Overlay_Drive::SetFileAttr(const char * name,uint16_t attr) {
	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(tmp, name);
	char *q=strrchr_dbcs(tmp, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tmp=0;
	char *p=strrchr_dbcs(temp_name, '\\');
	if (p!=NULL)
		strcat(tmp,p+1);
	else
		strcat(tmp,temp_name);
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir)))
		strcpy(tmp,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
	strcpy(overtmpname,overlaydir);
	strcat(overtmpname,tmp);
	CROSS_FILENAME(overtmpname);

	ht_stat_t status;
	const host_cnv_char_t* host_name;
	bool success=false;
#if defined (WIN32)
    host_name = CodePageGuestToHost(overtmpname);
    if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
	if (!success) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
	}
#else
	if (ht_stat(overtmpname,&status)==0 || ht_stat(overlayname,&status)==0) {
		if (attr & DOS_ATTR_READ_ONLY)
			status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
		else
			status.st_mode |=  S_IWUSR;
		if (chmod(overtmpname,status.st_mode) >= 0 || chmod(overlayname,status.st_mode) >= 0)
			success=true;
		if (!(attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN)) && (status.st_mode & S_IFDIR) == !(attr & DOS_ATTR_ARCHIVE))
			remove_special_file_from_disk(name, "ATR");
		else if (!add_special_file_to_disk(name, "ATR", attr, status.st_mode & S_IFDIR))
			LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,name);
	}
#endif
	if (success) {
		dirCache.EmptyCache();
		update_cache(false);
		return true;
	}

	char newname[CROSS_LEN];
	strcpy(newname,basedir);
	strcat(newname,name);
	CROSS_FILENAME(newname);
	temp_name = dirCache.GetExpandName(newname);
	host_name = CodePageGuestToHost(temp_name);

	bool created=false;
	if (host_name != NULL && ht_stat(host_name,&status)==0 && (status.st_mode & S_IFDIR)) {
		host_name = CodePageGuestToHost(overtmpname);
		int temp=-1;
		if (host_name!=NULL) {
#if defined (WIN32)
			temp=_wmkdir(host_name);
#else
			temp=mkdir(host_name,0775);
#endif
		}
		if (temp==0) created=true;
	} else {
		FILE * hand;
		host_name = CodePageGuestToHost(temp_name);
#ifdef host_cnv_use_wchar
		if (host_name!=NULL)
			hand = _wfopen(host_name,_HT("rb"));
		else
#endif
			hand = fopen_wrap(temp_name,"rb");
		if (hand) {
			if (logoverlay) LOG_MSG("overlay file opened %s",temp_name);
			FILE * layfile=NULL;
			host_name = CodePageGuestToHost(overtmpname);
#ifdef host_cnv_use_wchar
			if (host_name!=NULL) layfile=_wfopen(host_name,_HT("wb"));
#endif
			if (layfile==NULL) layfile=fopen_wrap(overlayname,"wb");
			int numr,numw;
			char buffer[1000];
			while(feof(hand)==0) {
				if((numr=(int)fread(buffer,1,1000,hand))!=1000){
					if(ferror(hand)!=0){
						fclose(hand);
						fclose(layfile);
						return false;
					} else if(feof(hand)!=0) { }
				}
				if((numw=(int)fwrite(buffer,1,numr,layfile))!=numr){
						fclose(hand);
						fclose(layfile);
						return false;
				}
			}
			fclose(hand);
			fclose(layfile);
			created=true;
		}
	}
	if (created) {
#if defined (WIN32)
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
		if (!success) {
			host_name = CodePageGuestToHost(overlayname);
			if (host_name != NULL&&SetFileAttributesW(host_name, attr)) success=true;
		}
#else
		if (ht_stat(overtmpname,&status)==0 || ht_stat(overlayname,&status)==0) {
			if (attr & DOS_ATTR_READ_ONLY)
				status.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
			else
				status.st_mode |=  S_IWUSR;
			if (chmod(overtmpname,status.st_mode) >= 0 || chmod(overlayname,status.st_mode) >= 0)
				success=true;
			if (!(attr & (DOS_ATTR_SYSTEM|DOS_ATTR_HIDDEN)) && (status.st_mode & S_IFDIR) == !(attr & DOS_ATTR_ARCHIVE))
				remove_special_file_from_disk(name, "ATR");
			else if (!add_special_file_to_disk(name, "ATR", attr, status.st_mode & S_IFDIR))
				LOG(LOG_DOSMISC,LOG_WARN)("%s: Application attempted to set system or hidden attributes for '%s' which is ignored for local drives",__FUNCTION__,name);
		}
#endif
		if (success) {
			dirCache.EmptyCache();
			update_cache(false);
			return true;
		}
	}
	return false;
}

bool Overlay_Drive::GetFileAttr(const char * name,uint16_t * attr) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(tmp, name);
	char *q=strrchr_dbcs(tmp, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tmp=0;
	char *p=strrchr_dbcs(temp_name, '\\');
	if (p!=NULL)
		strcat(tmp,p+1);
	else
		strcat(tmp,temp_name);
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir)))
		strcpy(tmp,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
	strcpy(overtmpname,overlaydir);
	strcat(overtmpname,tmp);
	CROSS_FILENAME(overtmpname);

#if defined (WIN32)
	Bitu attribs = INVALID_FILE_ATTRIBUTES;
    const host_cnv_char_t* host_name = CodePageGuestToHost(overtmpname);
    if (host_name != NULL) attribs = GetFileAttributesW(host_name);
	if (attribs == INVALID_FILE_ATTRIBUTES) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL) attribs = GetFileAttributesW(host_name);
	}
	if (attribs != INVALID_FILE_ATTRIBUTES) {
		*attr = attribs&0x3f;
		return true;
	}
#else
	ht_stat_t status;
	bool res = ht_stat(overtmpname,&status)==0;
	if (res || ht_stat(overlayname,&status)==0) {
        bool isdir = status.st_mode & S_IFDIR;
		*attr=isdir?0:DOS_ATTR_ARCHIVE;
		if(isdir) *attr|=DOS_ATTR_DIRECTORY;
		if(!(status.st_mode & S_IWUSR)) *attr|=DOS_ATTR_READ_ONLY;
		std::string fname = create_filename_of_special_operation(res?overtmpname:overlayname, "ATR");
        if (ht_stat(fname.c_str(),&status)==0) {
            unsigned int len = status.st_size;
            if (len & 1) {
                if (isdir)
                    *attr|=DOS_ATTR_ARCHIVE;
                else
                    *attr&=~DOS_ATTR_ARCHIVE;
            }
            if (len & 2) *attr|=DOS_ATTR_HIDDEN;
            if (len & 4) *attr|=DOS_ATTR_SYSTEM;
        }
		return true;
	}
#endif
	//Maybe check for deleted path as well
	if (is_deleted_file(name)) {
		*attr = 0;
		return false;
	}
	return localDrive::GetFileAttr(name,attr);
}

static std::string hostname = "";
std::string Overlay_Drive::GetHostName(const char * name) {
	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	ht_stat_t temp_stat;
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	if (host_name != NULL && ht_stat(host_name ,&temp_stat)==0) {
        hostname = overlayname;
        return hostname;
    }
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(overlayname,overlaydir);
		strcat(overlayname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_FILENAME(overlayname);
		host_name = CodePageGuestToHost(overlayname);
		if(host_name != NULL && ht_stat(host_name,&temp_stat)==0) {
            hostname = overlayname;
            return hostname;
        }
	}
	hostname = is_deleted_file(name)? "" : localDrive::GetHostName(name);
	return hostname;
}

void Overlay_Drive::add_deleted_file(const char* name,bool create_on_disk) {
	char tname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr_dbcs(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr_dbcs(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	if (!is_deleted_file(tname)) {
		deleted_files_in_base.emplace_back(tname);
		if (create_on_disk) add_special_file_to_disk(tname, "DEL");
	}
}

bool Overlay_Drive::add_special_file_to_disk(const char* dosname, const char* operation, uint16_t value, bool isdir) {
	std::string name = create_filename_of_special_operation(dosname, operation);
	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name.c_str());
	CROSS_FILENAME(overlayname);
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	FILE* f;
	if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
		f = _wfopen(host_name,_HT("wb+"));
#else
		f = fopen_wrap(host_name,"wb+");
#endif
	}
	else {
		f = fopen_wrap(overlayname,"wb+");
	}

	if (!f) {
		Sync_leading_dirs(dosname);
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name.c_str()));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overlayname,overlaydir);
			strcat(overlayname,temp_name);
			CROSS_FILENAME(overlayname);
		}
		host_name = CodePageGuestToHost(overlayname);
		if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
			f = _wfopen(host_name,_HT("wb+"));
#else
			f = fopen_wrap(host_name,"wb+");
#endif
		}
		else {
			f = fopen_wrap(overlayname,"wb+");
		}
	}
	if (!f) E_Exit("Failed creation of %s",overlayname);
    if (!strcmp(operation, "ATR")) {
        size_t len = 0;
        if (isdir != !(value & DOS_ATTR_ARCHIVE)) len |= 1;
        if (value & DOS_ATTR_HIDDEN) len |= 2;
        if (value & DOS_ATTR_SYSTEM) len |= 4;
        char *buf = new char[len + 1];
        fwrite(buf,len,1,f);
        fclose(f);
        delete[] buf;
    } else {
        char buf[5] = {'e','m','p','t','y'};
        fwrite(buf,5,1,f);
        fclose(f);
    }
    return true;
}

void Overlay_Drive::remove_special_file_from_disk(const char* dosname, const char* operation) {
	std::string name = create_filename_of_special_operation(dosname,operation);
	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name.c_str());
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name.c_str()));
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
		strcpy(overlayname,overlaydir);
		strcat(overlayname,temp_name);
		CROSS_FILENAME(overlayname);
	}
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	if ((host_name == NULL || ht_unlink(host_name) != 0) && unlink(overlayname) != 0 && strcmp(operation, "ATR"))
		E_Exit("Failed removal of %s",overlayname);
}

std::string Overlay_Drive::create_filename_of_special_operation(const char* dosname, const char* operation, bool expand) {
	(void)expand;//unused

	std::string res(dosname);
	std::string::size_type s = std::string::npos; //CHECK DOS or host endings.... on update_cache
	bool lead = false;
	for (unsigned int i=0; i<res.size(); i++) {
		if (lead) lead = false;
		else if ((IS_PC98_ARCH && shiftjis_lead_byte(res[i])) || (isDBCSCP() && isKanji1(res[i]))) lead = true;
		else if (res[i]=='\\'||res[i]==CROSS_FILESPLIT) s = i;
	}
	if (s == std::string::npos) s = 0; else s++;
	std::string oper = special_prefix + "_" + operation + "_";
	res.insert(s,oper);
	return res;
}

bool Overlay_Drive::is_dir_only_in_overlay(const char* name) {
	if (!name || !*name) return false;
	if (DOSdirs_cache.empty()) return false;
	char fname[CROSS_LEN];
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
		if (!strcasecmp((*it).c_str(), name)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))||((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), name))) return true;
	}
	return false;
}

bool Overlay_Drive::is_deleted_file(const char* name) {
	if (!name || !*name) return false;
	if (deleted_files_in_base.empty()) return false;
	char tname[CROSS_LEN],fname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr_dbcs(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr_dbcs(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); it++) {
		if (!strcasecmp((*it).c_str(), name)||!strcasecmp((*it).c_str(), tname)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))) return true;
	}
	return false;
}

void Overlay_Drive::add_DOSdir_to_cache(const char* name, const char *sname) {
	if (!name || !*name ) return; //Skip empty file.
	if (logoverlay) LOG_MSG("Adding name to overlay_only_dir_cache %s",name);
	if (!is_dir_only_in_overlay(name)) {
		DOSdirs_cache.emplace_back(name);
		DOSdirs_cache.emplace_back(sname);
	}
}

void Overlay_Drive::remove_DOSdir_from_cache(const char* name) {
	char fname[CROSS_LEN];
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
		if (!strcasecmp((*it).c_str(), name)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))||((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), name))) {
			DOSdirs_cache.erase(it+1);
			DOSdirs_cache.erase(it);
			return;
		}
	}
}

void Overlay_Drive::remove_deleted_file(const char* name,bool create_on_disk) {
	char tname[CROSS_LEN],fname[CROSS_LEN];
	strcpy(tname,basedir);
	strcat(tname,name);
	CROSS_FILENAME(tname);
	char* temp_name = dirCache.GetExpandName(tname);
	strcpy(tname, name);
	char *q=strrchr_dbcs(tname, '\\');
	if (q!=NULL) *(q+1)=0;
	else *tname=0;
	char *p=strrchr_dbcs(temp_name, '\\');
	if (p!=NULL)
		strcat(tname,p+1);
	else
		strcat(tname,temp_name);
	strcpy(fname, "");
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(fname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_DOSFILENAME(fname);
	}
	for(std::vector<std::string>::iterator it = deleted_files_in_base.begin(); it != deleted_files_in_base.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)||!strcasecmp((*it).c_str(), tname)||(strlen(fname)&&!strcasecmp((*it).c_str(), fname))) {
			deleted_files_in_base.erase(it);
			if (create_on_disk) remove_special_file_from_disk(name, "DEL");
			return;
		}
	}
}
void Overlay_Drive::add_deleted_path(const char* name, bool create_on_disk) {
	if (!name || !*name ) return; //Skip empty file.
	if (!is_deleted_path(name)) {
		deleted_paths_in_base.emplace_back(name);
		//Add it to deleted files as well, so it gets skipped in FindNext. 
		//Maybe revise that.
		if (create_on_disk) add_special_file_to_disk(name,"RMD");
		add_deleted_file(name,false);
	}
}
bool Overlay_Drive::is_deleted_path(const char* name) {
	if (!name || !*name) return false;
	if (deleted_paths_in_base.empty()) return false;
	std::string sname(name);
	std::string::size_type namelen = sname.length();;
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		std::string::size_type blockedlen = (*it).length();
		if (namelen < blockedlen) continue;
		//See if input starts with name. 
		std::string::size_type n = sname.find(*it);
		if (n == 0 && ((namelen == blockedlen) || *(name + blockedlen) == '\\' )) return true;
	}
	return false;
}

void Overlay_Drive::remove_deleted_path(const char* name, bool create_on_disk) {
	for(std::vector<std::string>::iterator it = deleted_paths_in_base.begin(); it != deleted_paths_in_base.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) {
			deleted_paths_in_base.erase(it);
			remove_deleted_file(name,false); //Rethink maybe.
			if (create_on_disk) remove_special_file_from_disk(name,"RMD");
			break;
		}
	}
}
bool Overlay_Drive::check_if_leading_is_deleted(const char* name){
	const char* dname = strrchr_dbcs((char *)name,'\\');
	if (dname != NULL) {
		char dirname[CROSS_LEN];
		strncpy(dirname,name,dname - name);
		dirname[dname - name] = 0;
		if (is_deleted_path(dirname)) return true;
	}
	return false;
}

bool Overlay_Drive::FileExists(const char* name) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	ht_stat_t temp_stat;
	const host_cnv_char_t* host_name = CodePageGuestToHost(overlayname);
	if (host_name != NULL && ht_stat(host_name ,&temp_stat)==0 && (temp_stat.st_mode & S_IFDIR)==0) return true;
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(overlayname,overlaydir);
		strcat(overlayname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_FILENAME(overlayname);
		host_name = CodePageGuestToHost(overlayname);
		if(host_name != NULL && ht_stat(host_name,&temp_stat)==0 && (temp_stat.st_mode & S_IFDIR)==0) return true;
	}
	
	if (is_deleted_file(name)) return false;

	return localDrive::FileExists(name);
}

bool Overlay_Drive::Rename(const char * oldname,const char * newname) {
    if (ovlreadonly) {
        DOS_SetError(DOSERR_WRITE_PROTECTED);
        return false;
    }

	//TODO with cache function!
	//Tricky function.
	//Renaming directories is currently not fully supported, due the drive_cache not handling that smoothly.
	//So oldname is directory => exit unless it is only in overlay.
	//If oldname is on overlay => simple rename.
	//if oldname is on base => copy file to overlay with new name and mark old file as deleted. 
	//More advanced version. keep track of the file being renamed in order to detect that the file is being renamed back. 
	
	char tmp[CROSS_LEN];
	host_cnv_char_t host_nameold[CROSS_LEN], host_namenew[CROSS_LEN];
	uint16_t attr = 0;
	if (!GetFileAttr(oldname,&attr)) E_Exit("rename, but source doesn't exist, should not happen %s",oldname);
	ht_stat_t temp_stat;
	if (attr&DOS_ATTR_DIRECTORY) {
		//See if the directory exists only in the overlay, then it should be possible.
#if OVERLAY_DIR
		if (localDrive::TestDir(oldname)) {
			LOG_MSG("Overlay: renaming base directory %s to %s not yet supported", oldname,newname);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
#endif
		char overlaynameold[CROSS_LEN];
		strcpy(overlaynameold,overlaydir);
		strcat(overlaynameold,oldname);
		CROSS_FILENAME(overlaynameold);
#ifdef host_cnv_use_wchar
		wcscpy
#else
		strcpy
#endif
		(host_nameold, CodePageGuestToHost(overlaynameold));

		char overlaynamenew[CROSS_LEN];
		strcpy(overlaynamenew,overlaydir);
		strcat(overlaynamenew,newname);
		CROSS_FILENAME(overlaynamenew);
#ifdef host_cnv_use_wchar
		wcscpy
#else
		strcpy
#endif
		(host_namenew, CodePageGuestToHost(overlaynamenew));

		if (ht_stat(host_nameold,&temp_stat)) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,oldname));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				strcpy(overlaynameold,GetCrossedName(overlaydir,temp_name));
#ifdef host_cnv_use_wchar
				wcscpy
#else
				strcpy
#endif
				(host_nameold, CodePageGuestToHost(overlaynameold));
			}
			strcpy(tmp,newname);
			char *p=strrchr_dbcs(tmp,'\\'), ndir[CROSS_LEN];
			if (p!=NULL) {
				*p=0;
				temp_name=dirCache.GetExpandName(GetCrossedName(basedir,tmp));
				if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
					strcpy(ndir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
					strcat(ndir,"\\");
					strcat(ndir,p+1);
					strcpy(overlaynamenew,GetCrossedName(overlaydir,ndir));
#ifdef host_cnv_use_wchar
					wcscpy
#else
					strcpy
#endif
					(host_namenew, CodePageGuestToHost(overlaynamenew));
				}
			}
		}
		int temp=-1;
#ifdef host_cnv_use_wchar
		temp = _wrename(host_nameold,host_namenew);
#else
		temp = rename(host_nameold,host_namenew);
#endif
		if (temp==0) {
			dirCache.EmptyCache();
			update_cache(true);
			return true;
		}
		LOG_MSG("Overlay: renaming overlay directory %s to %s not yet supported",oldname,newname); //TODO
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	uint32_t a = GetTicks();
	//First generate overlay names.
	char overlaynameold[CROSS_LEN];
	strcpy(overlaynameold,overlaydir);
	strcat(overlaynameold,oldname);
	CROSS_FILENAME(overlaynameold);
#ifdef host_cnv_use_wchar
	wcscpy
#else
	strcpy
#endif
	(host_nameold, CodePageGuestToHost(overlaynameold));

	char overlaynamenew[CROSS_LEN];
	strcpy(overlaynamenew,overlaydir);
	strcat(overlaynamenew,newname);
	CROSS_FILENAME(overlaynamenew);
#ifdef host_cnv_use_wchar
	wcscpy
#else
	strcpy
#endif
	(host_namenew, CodePageGuestToHost(overlaynamenew));
	if (ht_stat(host_nameold,&temp_stat)) {
		char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,oldname));
		if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
			temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
			strcpy(overlaynameold,GetCrossedName(overlaydir,temp_name));
#ifdef host_cnv_use_wchar
			wcscpy
#else
			strcpy
#endif
			(host_nameold, CodePageGuestToHost(overlaynameold));
		}
		strcpy(tmp,newname);
		char *p=strrchr_dbcs(tmp,'\\'), ndir[CROSS_LEN];
		if (p!=NULL) {
			*p=0;
			temp_name=dirCache.GetExpandName(GetCrossedName(basedir,tmp));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				strcpy(ndir,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
				strcat(ndir,"\\");
				strcat(ndir,p+1);
				strcpy(overlaynamenew,GetCrossedName(overlaydir,ndir));
#ifdef host_cnv_use_wchar
				wcscpy
#else
				strcpy
#endif
				(host_namenew, CodePageGuestToHost(overlaynamenew));
			}
		}
	}

	//No need to check if the original is marked as deleted, as GetFileAttr would fail if it did.

	//Check if overlay source file exists
	int temp = -1; 
	if (ht_stat(host_nameold,&temp_stat) == 0) {
		//Simple rename
#ifdef host_cnv_use_wchar
		temp = _wrename(host_nameold,host_namenew);
#else
		temp = rename(host_nameold,host_namenew);
#endif
		//TODO CHECK if base has a file with same oldname!!!!! if it does mark it as deleted!!
		if (localDrive::FileExists(oldname)) add_deleted_file(oldname,true);
	} else {
		uint32_t aa = GetTicks();
		//File exists in the basedrive. Make a copy and mark old one as deleted.
		char newold[CROSS_LEN];
		strcpy(newold,basedir);
		strcat(newold,oldname);
		CROSS_FILENAME(newold);
		dirCache.ExpandName(newold);
		const host_cnv_char_t* host_name = CodePageGuestToHost(newold);
		FILE* o;
		if (host_name!=NULL) {
#ifdef host_cnv_use_wchar
			o = _wfopen(host_name,_HT("rb"));
#else
			o = fopen_wrap(host_name,"rb");
#endif
		}
		else {
			o = fopen_wrap(newold,"rb");
		}
		if (!o) return false;
		FILE* n = create_file_in_overlay(newname,"wb+");
		if (!n) {fclose(o); return false;}
		char buffer[BUFSIZ];
		size_t s;
		while ( (s = fread(buffer,1,BUFSIZ,o)) ) fwrite(buffer, 1, s, n);
		fclose(o); fclose(n);

		//File copied.
		//Mark old file as deleted
		add_deleted_file(oldname,true);
		temp =0; //success
		if (logoverlay) LOG_MSG("OPTIMISE: update rename with copy took %d",GetTicks()-aa);

	}
	if (temp ==0) {
		//handle the drive_cache (a bit better)
		//Ensure that the file is not marked as deleted anymore.
		if (is_deleted_file(newname)) remove_deleted_file(newname,true);
		dirCache.EmptyCache();
		update_cache(true);
		if (logoverlay) LOG_MSG("OPTIMISE: rename took %d",GetTicks()-a);

	}
	return (temp==0);

}

bool Overlay_Drive::FindFirst(const char * _dir,DOS_DTA & dta,bool fcb_findfirst) {
	if (logoverlay) LOG_MSG("FindFirst in %s",_dir);
	
	if (is_deleted_path(_dir)) {
		//No accidental listing of files in there.
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	if (*_dir) {
		char newname[CROSS_LEN], tmp[CROSS_LEN];
		strcpy(newname,overlaydir);
		strcat(newname,_dir);
		CROSS_FILENAME(newname);
		struct stat temp_stat;
		if (stat(newname,&temp_stat)) {
			char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,_dir));
			if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
				temp_name+=strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0);
				return localDrive::FindFirst(temp_name,dta,fcb_findfirst);
			}
			strcpy(tmp, _dir);
			bool found=false;
			for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2) {
				if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
					found=true;
					strcpy(tmp, (*it).c_str());
					break;
				}
			}
			if (found) {
				return localDrive::FindFirst(tmp,dta,fcb_findfirst);
			}
		}
	}
	return localDrive::FindFirst(_dir,dta,fcb_findfirst);
}

bool Overlay_Drive::FileStat(const char* name, FileStat_Block * const stat_block) {
	if (ovlnocachedir) {
		dirCache.EmptyCache();
		update_cache(true);
	}

	char overlayname[CROSS_LEN], tmp[CROSS_LEN], overtmpname[CROSS_LEN];
	strcpy(overlayname,overlaydir);
	strcat(overlayname,name);
	CROSS_FILENAME(overlayname);
	char* temp_name = dirCache.GetExpandName(GetCrossedName(basedir,name));
	const host_cnv_char_t* host_name;
	ht_stat_t temp_stat;
	bool success=false;
	if (strlen(temp_name)>strlen(basedir)&&!strncasecmp(temp_name, basedir, strlen(basedir))) {
		strcpy(overtmpname,overlaydir);
		strcat(overtmpname,temp_name+strlen(basedir)+(*(temp_name+strlen(basedir))=='\\'?1:0));
		CROSS_FILENAME(overtmpname);
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat)==0) success=true;
	}
	if (!success) {
		host_name = CodePageGuestToHost(overlayname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat) == 0) success=true;
	}
	if (!success) {
		strcpy(tmp,name);
		char *p=strrchr_dbcs(tmp, '\\'), *q=strrchr_dbcs(temp_name, '\\');
		if (p!=NULL&&q!=NULL) {
			*p=0;
			for(std::vector<std::string>::iterator it = DOSdirs_cache.begin(); it != DOSdirs_cache.end(); it+=2)
				if ((*(it+1)).length()&&!strcasecmp((*(it+1)).c_str(), tmp)) {
					strcpy(tmp, (*it).c_str());
					break;
				}
			strcat(tmp, "\\");
			strcat(tmp, q+1);
		}
		strcpy(overtmpname,overlaydir);
		strcat(overtmpname,tmp);
		CROSS_FILENAME(overtmpname);
		host_name = CodePageGuestToHost(overtmpname);
		if (host_name != NULL && ht_stat(host_name,&temp_stat)==0) success=true;
	}
	if(!success) {
		if (is_deleted_file(name)) return false;
		return localDrive::FileStat(name,stat_block);
	}
	
	/* Convert the stat to a FileStat */
	struct tm *time;
	if((time=
#if defined(__MINGW32__) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
    _localtime64
#else
    localtime
#endif
    (&temp_stat.st_mtime))!=0) {
		stat_block->time=DOS_PackTime((uint16_t)time->tm_hour,(uint16_t)time->tm_min,(uint16_t)time->tm_sec);
		stat_block->date=DOS_PackDate((uint16_t)(time->tm_year+1900),(uint16_t)(time->tm_mon+1),(uint16_t)time->tm_mday);
	} else {
			// ... But this function is not used at the moment.
	}
	stat_block->size=(uint32_t)temp_stat.st_size;
	return true;
}

Bits Overlay_Drive::UnMount(void) { 
	delete this;
	return 0; 
}

void Overlay_Drive::EmptyCache(void){
	localDrive::EmptyCache();
	update_cache(true);//lets rebuild it.
}
