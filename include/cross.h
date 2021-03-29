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


#ifndef DOSBOX_CROSS_H
#define DOSBOX_CROSS_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>

#if defined (_MSC_VER)						/* MS Visual C++ */
#include <direct.h>
#include <io.h>
#define ULONGTYPE(a) a##ULL
#define LONGTYPE(a) a##LL
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define alloca _alloca
#define putenv _putenv
#else										/* LINUX / GCC */
#include <dirent.h>
#include <unistd.h>
#define ULONGTYPE(a) a##ULL
#define LONGTYPE(a) a##LL
#endif

#define CROSS_LEN 512						/* Maximum filename size */


#if defined (WIN32) || defined (OS2)				/* Win 32 & OS/2*/
#define CROSS_FILENAME(blah) 
#define CROSS_FILESPLIT '\\'
#define F_OK 0
#else
#define	CROSS_FILENAME(blah) strreplace(blah,'\\','/')
#define CROSS_FILESPLIT '/'
#endif

#define CROSS_NONE	0
#define CROSS_FILE	1
#define CROSS_DIR	2
#if defined (WIN32) && !defined (__MINGW32__)
#define ftruncate(blah,blah2) chsize(blah,blah2)
#endif

//Solaris maybe others
#if defined (DB_HAVE_NO_POWF)
#include <math.h>
static inline float powf (float x, float y) { return (float) pow (x,y); }
#endif

class Cross {
public:
	static void GetPlatformResDir(std::string& in);
	static void GetPlatformConfigDir(std::string& in);
	static void GetPlatformConfigName(std::string& in);
	static void CreatePlatformConfigDir(std::string& in);
	static void ResolveHomedir(std::string & temp_line);
	static void CreateDir(std::string const& in);
	static bool IsPathAbsolute(std::string const& in);
};


#if defined (WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN        // Exclude rarely-used stuff from
#endif

#include <windows.h>

typedef struct dir_struct {
	HANDLE          handle;
	char            base_path[(MAX_PATH+4)*sizeof(wchar_t)];
    wchar_t *wbase_path(void) {
        return (wchar_t*)base_path;
    }
    // TODO: offer a config.h option to opt out of Windows widechar functions
    union {
        WIN32_FIND_DATAA a;
        WIN32_FIND_DATAW w;
    } search_data;
    bool            wide;
} dir_information;

// TODO: offer a config.h option to opt out of Windows widechar functions
dir_information* open_directory(const char* dirname);
bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory);
bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory);
dir_information* open_directoryw(const wchar_t* dirname);
bool read_directory_firstw(dir_information* dirp, wchar_t* entry_name, wchar_t* entry_sname, bool& is_directory);
bool read_directory_nextw(dir_information* dirp, wchar_t* entry_name, wchar_t* entry_sname, bool& is_directory);

#else

//#include <sys/types.h> //Included above
#include <dirent.h>

typedef struct dir_struct { 
	DIR*  dir;
	char base_path[CROSS_LEN];
} dir_information;

dir_information* open_directory(const char* dirname);
bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory);
bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory);

#define open_directoryw open_directory
#define read_directory_firstw read_directory_first
#define read_directory_nextw read_directory_next

#endif

void close_directory(dir_information* dirp);

FILE* fopen_wrap(const char* path, const char* mode);
#endif
