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
#include "cross.h"
#include "support.h"
#include <string>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(MACOSX)
std::string MacOSXEXEPath;
std::string MacOSXResPath;
#endif

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

#if defined __MINGW32__
#define _mkdir(x) mkdir(x)
#endif

#if !defined(WIN32) && !defined(OS2)
#include <regex>
void autoExpandEnvironmentVariables(std::string & text) {
    static std::regex env("\\$\\{([^}]+)\\}");
    std::smatch match;
    while (std::regex_search(text, match, env)) {
        const char * s = getenv(match[1].str().c_str());
        const std::string var(s == NULL ? "" : s);
        text.replace(match[0].first, match[0].second, var);
    }
}
#endif

// Resolve environment variables (%VAR% [DOS/Windows] or ${VAR} [Linux/macOS], and tildes (~) in Linux/macOS)
void ResolvePath(std::string& in) {
#if defined(WIN32) || defined(OS2)
    char path[300],temp[300],*tempd=temp;
    strcpy(tempd, in.c_str());
    if (strchr(tempd, '%')&&ExpandEnvironmentStrings(tempd,path,300))
        tempd=path;
    in=std::string(tempd);
#else
    struct stat test;
    if (stat(in.c_str(),&test))
        Cross::ResolveHomedir(in);
    autoExpandEnvironmentVariables(in);
#endif
}

#if defined(WIN32) && !defined(HX_DOS)
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

void Cross::GetPlatformResDir(std::string& in) {
#if defined(MACOSX)
	in = MacOSXResPath;
#elif defined(RISCOS)
	in = "/<DosBox-X$Dir>/resources";
#elif defined(LINUX)
	const char *xdg_data_home = getenv("XDG_DATA_HOME");
	const std::string data_home = xdg_data_home && xdg_data_home[0] == '/' ? xdg_data_home: "~/.local/share";
	in = data_home + "/dosbox-x";
	ResolveHomedir(in);

	// Let's check if the above exists, otherwise use RESDIR
	struct stat info;
	if ((stat(in.c_str(), &info) != 0) || (!(info.st_mode & S_IFDIR))) {
		//LOG_MSG("XDG_DATA_HOME (%s) does not exist. Using %s", in.c_str(), RESDIR);
	        in = RESDIR;
	}
#elif defined(RESDIR)
	in = RESDIR;
#endif
	if (!in.empty())
		in += CROSS_FILESPLIT;
}

void Cross::GetPlatformConfigDir(std::string& in) {
#if defined(WIN32) && !defined(HX_DOS)
	W32_ConfDir(in,false);
	in += "\\DOSBox-X";
#elif defined(MACOSX)
	in = "~/Library/Preferences";
	ResolveHomedir(in);
#elif defined(HAIKU)
	in = "~/config/settings/dosbox-x";
	ResolveHomedir(in);
#elif defined(RISCOS)
	in = "/<Choices$Write>/DosBox-X";
#elif !defined(HX_DOS)
	const char *xdg_conf_home = getenv("XDG_CONFIG_HOME");
	const std::string conf_home = xdg_conf_home && xdg_conf_home[0] == '/' ? xdg_conf_home: "~/.config";
	in = conf_home + "/dosbox-x";
	ResolveHomedir(in);
#endif
	//LOG_MSG("Config dir: %s", in.c_str());
	in += CROSS_FILESPLIT;
}

void Cross::GetPlatformConfigName(std::string& in) {
#ifdef WIN32
#define DEFAULT_CONFIG_FILE "dosbox-x-" VERSION ".conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "DOSBox-X " VERSION " Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "dosbox-x-" VERSION ".conf"
#endif
	in = DEFAULT_CONFIG_FILE;
}

void Cross::CreatePlatformConfigDir(std::string& in) {
#if defined(WIN32) && !defined(HX_DOS)
	W32_ConfDir(in,true);
	in += "\\DOSBox-X";
	_mkdir(in.c_str());
#elif defined(MACOSX)
	in = "~/Library/Preferences";
	ResolveHomedir(in);
	//Don't create it. Assume it exists
#elif defined(HAIKU)
	in = "~/config/settings/dosbox-x";
	ResolveHomedir(in);
	mkdir(in.c_str(),0700);
#elif defined(RISCOS)
	in = "/<Choices$Write>/DosBox-X";
	mkdir(in.c_str(),0700);
#elif !defined(HX_DOS)
	const char *xdg_conf_home = getenv("XDG_CONFIG_HOME");
	const std::string conf_home = xdg_conf_home && xdg_conf_home[0] == '/' ? xdg_conf_home: "~/.config";
	in = conf_home + "/dosbox-x";
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
typedef wchar_t host_cnv_char_t;
extern char *CodePageHostToGuest(const host_cnv_char_t *s);

/* does the filename fit the 8.3 format? */
static bool is_filename_8by3w(const wchar_t* fname) {
	if (CodePageHostToGuest(fname)==NULL) return false;
    int i;

    /* Is the first part 8 chars or less? */
    i=0;
    while (*fname != 0 && *fname != L'.') {
		if (*fname<=32||*fname==127||*fname==L'"'||*fname==L'+'||*fname==L'='||*fname==L','||*fname==L';'||*fname==L':'||*fname==L'<'||*fname==L'>'||*fname==L'|'||*fname==L'?'||*fname==L'*') return false;
		if (IS_PC98_ARCH && (*fname & 0xFF00u) != 0u && (*fname & 0xFCu) != 0x08u) i++;
		fname++; i++; 
	}
    if (i > 8) return false;

    if (*fname == L'.') fname++;

    /* Is the second part 3 chars or less? A second '.' also makes it a LFN */
    i=0;
    while (*fname != 0 && *fname != L'.') {
		if (*fname<=32||*fname==127||*fname==L'"'||*fname==L'+'||*fname==L'='||*fname==L','||*fname==L';'||*fname==L':'||*fname==L'<'||*fname==L'>'||*fname==L'|'||*fname==L'?'||*fname==L'*') return false;
		if (IS_PC98_ARCH && (*fname & 0xFF00u) != 0u && (*fname & 0xFCu) != 0x08u) i++;
		fname++; i++;
	}
    if (i > 3) return false;

    /* if there is anything beyond this point, it's an LFN */
    if (*fname != 0) return false;

    return true;
}

dir_information* open_directoryw(const wchar_t* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = wcslen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	wcsncpy(dir.wbase_path(),dirname,MAX_PATH);

	if (dirname[len-1] == '\\') wcscat(dir.wbase_path(),L"*.*");
	else                        wcscat(dir.wbase_path(),L"\\*.*");

    dir.wide = true;
	dir.handle = INVALID_HANDLE_VALUE;

	return (_waccess(dirname,0) ? NULL : &dir);
}

dir_information* open_directory(const char* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = strlen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len-1] == '\\') strcat(dir.base_path,"*.*");
	else                        strcat(dir.base_path,"\\*.*");

    dir.wide = false;
	dir.handle = INVALID_HANDLE_VALUE;

	return (_access(dirname,0) ? NULL : &dir);
}

bool read_directory_firstw(dir_information* dirp, wchar_t* entry_name, wchar_t* entry_sname, bool& is_directory) {
    if (!dirp->wide) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	dirp->handle = FindFirstFileW(dirp->wbase_path(), &dirp->search_data.w);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	// TODO: offer a config.h option to opt out of Windows widechar functions
	wcsncpy(entry_name,dirp->search_data.w.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
	if (dirp->search_data.w.cAlternateFileName[0] != 0 && is_filename_8by3w(dirp->search_data.w.cFileName))
		wcsncpy(entry_sname,dirp->search_data.w.cFileName,13);
	else
		wcsncpy(entry_sname,dirp->search_data.w.cAlternateFileName,13);

	if (dirp->search_data.w.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_nextw(dir_information* dirp, wchar_t* entry_name, wchar_t* entry_sname, bool& is_directory) {
    if (!dirp->wide) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	int result = FindNextFileW(dirp->handle, &dirp->search_data.w);
	if (result==0) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	wcsncpy(entry_name,dirp->search_data.w.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
	if (dirp->search_data.w.cAlternateFileName[0] != 0 && is_filename_8by3w(dirp->search_data.w.cFileName))
		wcsncpy(entry_sname,dirp->search_data.w.cFileName,13);
	else
		wcsncpy(entry_sname,dirp->search_data.w.cAlternateFileName,13);

	if (dirp->search_data.w.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
    if (!dirp) return false;
    if (dirp->wide) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	dirp->handle = FindFirstFileA(dirp->base_path, &dirp->search_data.a);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

    // TODO: offer a config.h option to opt out of Windows widechar functions
	safe_strncpy(entry_name,dirp->search_data.a.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
    safe_strncpy(entry_sname,dirp->search_data.a.cAlternateFileName,13);
 
	if (dirp->search_data.a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
    if (!dirp) return false;
    if (dirp->wide) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	int result = FindNextFileA(dirp->handle, &dirp->search_data.a);
	if (result==0) return false;

    // TODO: offer a config.h option to opt out of Windows widechar functions
	safe_strncpy(entry_name,dirp->search_data.a.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);
	safe_strncpy(entry_sname,dirp->search_data.a.cAlternateFileName,13);

	if (dirp->search_data.a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp && dirp->handle != INVALID_HANDLE_VALUE) {
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

bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {	
	if (!dirp) return false;
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);
	entry_sname[0]=0;

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

bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory) {
	if (!dirp) return false;
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);
	entry_sname[0]=0;


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

void close_directory(dir_information* dirp) {
	if (dirp) closedir(dirp->dir);
}

#endif

FILE *fopen_wrap(const char *path, const char *mode) {
#if !defined(WIN32) && !defined(OS2) && !defined(MACOSX) && defined(HAVE_REALPATH)
	char work[CROSS_LEN] = {0};
	strncpy(work,path,CROSS_LEN-1);
	char* last = strrchr(work,'/');
	
	if (last) {
		if (last != work) {
			*last = 0;
			//If this compare fails, then we are dealing with files in / 
			//Which is outside the scope, but test anyway. 
			//However as realpath only works for exising files. The testing is 
			//in that case not done against new files.
		}
		char* check = realpath(work,NULL);
		if (check) {
			if ( ( strlen(check) == 5 && strcmp(check,"/proc") == 0) || strncmp(check,"/proc/",6) == 0) {
//				LOG_MSG("lst hit %s blocking!",path);
				free(check);
				return NULL;
			}
			free(check);
		}
	}

#if 0
//Lightweight version, but then existing files can still be read, which is not ideal	
	if (strpbrk(mode,"aw+") != NULL) {
		LOG_MSG("pbrk ok");
		char* check = realpath(path,NULL);
		//Will be null if file doesn't exist.... ENOENT
		//TODO What about unlink /proc/self/mem and then create it ?
		//Should be safe for what we want..
		if (check) {
			if (strncmp(check,"/proc/",6) == 0) {
				free(check);
				return NULL;
			}
			free(check);
		}
	}
*/
#endif //0 
#endif

	return fopen(path,mode);
}
