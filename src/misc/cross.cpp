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


#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include <string>
#include <stdlib.h>

#ifdef WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif
#include <shlobj.h>
#endif

#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
#include <sys/types.h>
#include <pwd.h>
#endif

extern bool uselfn;

#ifdef WIN32
static void W32_ConfDir(std::string& in,bool create) {
	int c = create?1:0;
	char result[MAX_PATH] = { 0 };
	BOOL r = SHGetSpecialFolderPath(NULL,result,CSIDL_LOCAL_APPDATA,c);
	if(!r || result[0] == 0) r = SHGetSpecialFolderPath(NULL,result,CSIDL_APPDATA,c);
	if(!r || result[0] == 0) {
		char const * windir = getenv("windir");
		if(!windir) windir = "c:\\windows";
		safe_strncpy(result,windir,MAX_PATH);
		char const* appdata = "\\Application Data";
		size_t len = strlen(result);
		if(len + strlen(appdata) < MAX_PATH) strcat(result,appdata);
		if(create) _mkdir(result);
	}
	in = result;
}
#endif

void Cross::GetPlatformConfigDir(std::string& in) {
#ifdef WIN32
	W32_ConfDir(in,false);
	in += "\\DOSBox";
#elif defined(MACOSX)
	in = "~/Library/Preferences";
	ResolveHomedir(in);
#else
	in = "~/.dosbox";
	ResolveHomedir(in);
#endif
	in += CROSS_FILESPLIT;
}

void Cross::GetPlatformConfigName(std::string& in) {
#ifdef WIN32
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "DOSBox " VERSION " Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#endif
	in = DEFAULT_CONFIG_FILE;
}

void Cross::CreatePlatformConfigDir(std::string& in) {
#ifdef WIN32
	W32_ConfDir(in,true);
	in += "\\DOSBox";
	_mkdir(in.c_str());
#elif defined(MACOSX)
	in = "~/Library/Preferences/";
	ResolveHomedir(in);
	//Don't create it. Assume it exists
#else
	in = "~/.dosbox";
	ResolveHomedir(in);
	mkdir(in.c_str(),0700);
#endif
	in += CROSS_FILESPLIT;
}

void Cross::ResolveHomedir(std::string & temp_line) {
	if(!temp_line.size() || temp_line[0] != '~') return; //No ~

	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
}

void Cross::CreateDir(std::string const& in) {
#ifdef WIN32
	_mkdir(in.c_str());
#else
	mkdir(in.c_str(),0700);
#endif
}

bool Cross::IsPathAbsolute(std::string const& in) {
	// Absolute paths
#if defined (WIN32) || defined(OS2)
	// drive letter
	if (in.size() > 2 && in[1] == ':' ) return true;
	// UNC path
	else if (in.size() > 2 && in[0]=='\\' && in[1]=='\\') return true;
#else
	if (in.size() > 1 && in[0] == '/' ) return true;
#endif
	return false;
}

#if defined (WIN32)

dir_information* open_directory(const char* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = strlen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len-1] == '\\') strcat(dir.base_path,"*.*");
	else                        strcat(dir.base_path,"\\*.*");

	dir.handle = INVALID_HANDLE_VALUE;

	return (_access(dirname,0) ? NULL : &dir);
}

bool read_directory_first2(dir_information* dirp, char* entry_name, bool& is_directory) {
	dirp->handle = FindFirstFile(dirp->base_path, &dirp->search_data);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next2(dir_information* dirp, char* entry_name, bool& is_directory) {
	int result = FindNextFile(dirp->handle, &dirp->search_data);
	if (result==0) return false;

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	dirp->handle = FindFirstFile(dirp->base_path, &dirp->search_data);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
	safe_strncpy(entry_sname,dirp->search_data.cAlternateFileName,13);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	int result = FindNextFile(dirp->handle, &dirp->search_data);
	if (result==0) return false;

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
	safe_strncpy(entry_sname,dirp->search_data.cAlternateFileName,13);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp->handle != INVALID_HANDLE_VALUE) {
		FindClose(dirp->handle);
		dirp->handle = INVALID_HANDLE_VALUE;
	}
}

#else

dir_information* open_directory(const char* dirname) {
	static dir_information dir;
	dir.dir=opendir(dirname);
	safe_strncpy(dir.base_path,dirname,CROSS_LEN);
	return dir.dir?&dir:NULL;
}

bool read_directory_first2(dir_information* dirp, char* entry_name, bool& is_directory) {
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

#ifdef DIRENT_HAS_D_TYPE
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	// probably use d_type here instead of a full stat()
	static char buffer[2*CROSS_LEN] = { 0 };
	buffer[0] = 0;
	strcpy(buffer,dirp->base_path);
	strcat(buffer,entry_name);
	struct stat status;
	if (stat(buffer,&status)==0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	return true;
}

bool read_directory_next2(dir_information* dirp, char* entry_name, bool& is_directory) {
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

#ifdef DIRENT_HAS_D_TYPE
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	// probably use d_type here instead of a full stat()
	static char buffer[2*CROSS_LEN] = { 0 };
	buffer[0] = 0;
	strcpy(buffer,dirp->base_path);
	strcat(buffer,entry_name);
	struct stat status;

	if (stat(buffer,&status)==0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	return true;
}

bool should_fake_LFN(const char *name) {
	int namelen=0,extlen=0;
	char need_lfn=false;

	/* need LFN if name prior to dot is longer than 8 or has certain chars, like spaces.
	 * we're trying to mimic Windows 95 LFN behavior. */
	while (*name != 0) {
		if (*name == '.') break;
		if (*name == ' ' || *name < 32/*NTS: as signed char, also covers >= (unsigned char) 128*/) need_lfn = true;
		namelen++;
		name++;
	}

	if (*name == '.') {
		name++;

		/* and now file extension */
		while (*name != 0) {
			if (*name == ' ' || *name == '.'/*clearly the first dot was not the file extension*/ ||
				*name < 32/*NTS: as signed char, also covers (unsigned char) >= 128*/) need_lfn = true;

			extlen++;
			name++;
		}
	}

	return need_lfn || (namelen > 8) || (extlen > 3);
}

unsigned int lfn_fake_ext_from_inode(ino_t inode) {
	while (inode > 0xFFF) // FIXME: This probably sucks. Got any other hashing functions?
		inode = ((inode & 0xFFF) ^ (inode >> 12)) + 3;

	return inode;
}

void do_fake_lfn(const char *entry_name,char *entry_sname,struct stat &status) {
	/* fake an LFN using the file's inode number, cram it into 3 hex digits */
	const char *s = entry_name;
	const char *ext = strrchr(s,'.');
	int i=0,j=0;

	if (ext == NULL) ext = s + strlen(s);

	while (s < ext && i < 4) {
		if (*s == ' ' || *s < 32) {
			s++;
			continue;
		}

		entry_sname[i++] = *s++;
	}

	entry_sname[i++] = '~';
	sprintf(entry_sname+i,"%03x",lfn_fake_ext_from_inode(status.st_ino));
	i += 3;

	/* then the remainder of the string, for file extension */
	if (ext != NULL) {
		s = ext;
		if (*s == '.') s++;
		entry_sname[i++] = '.';

		while (*s && j < 3) {
			if (*s == ' ' || *s < 32) {
				s++;
				continue;
			}

			entry_sname[i++] = *s++;
			j++;
		}
	}

	entry_sname[i] = 0;
	assert(i <= (8+1+3)); /* fits 8.3 format RIGHT?? */

	LOG_MSG("Faking Windows 95 LFN: '%s' => '%s'\n",
		entry_name,entry_sname);
}

bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	struct dirent* dentry = readdir(dirp->dir);
	bool fake_lfn=false;

	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);
	entry_sname[0]=0;

	/* if LFN emulation is enabled, then use other characteristics provided by
	 * Linux or Mac OS X to at least try to fake LFN support */
	if (uselfn) fake_lfn=should_fake_LFN(dentry->d_name);

#ifdef DIRENT_HAS_D_TYPE
	if (!fake_lfn) { /* LFN fakery needs the full stat(), sorry */
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
	}
#endif

	// probably use d_type here instead of a full stat()
	static char buffer[2*CROSS_LEN] = { 0 };
	buffer[0] = 0;
	strcpy(buffer,dirp->base_path);
	strcat(buffer,entry_name); // FIXME: This is clearly an opportunity for a buffer overflow and crash
	struct stat status;
	if (stat(buffer,&status)==0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	if (fake_lfn) do_fake_lfn(entry_name,entry_sname,status);

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	struct dirent* dentry = readdir(dirp->dir);
	bool fake_lfn=false;

	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);
	entry_sname[0]=0;

	/* if LFN emulation is enabled, then use other characteristics provided by
	 * Linux or Mac OS X to at least try to fake LFN support */
	if (uselfn) fake_lfn=should_fake_LFN(dentry->d_name);

#ifdef DIRENT_HAS_D_TYPE
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	// probably use d_type here instead of a full stat()
	static char buffer[2*CROSS_LEN] = { 0 };
	buffer[0] = 0;
	strcpy(buffer,dirp->base_path);
	strcat(buffer,entry_name);
	struct stat status;

	if (stat(buffer,&status)==0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	if (fake_lfn) do_fake_lfn(entry_name,entry_sname,status);

	return true;
}

void close_directory(dir_information* dirp) {
	closedir(dirp->dir);
}

#endif
