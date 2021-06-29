
/*
DOS	Network Routines.


Log:
July 11 2005		cyberwalker		Created.
July 23 2005		cyberwalker
        Removed code from dos.cpp to this file.
August 8 2005		cyberwalker
		Make all functions  to avoid modifying makefile.

*/

#if defined(WIN32) && !(defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
//#include <dirent.h>

#include "config.h"
#include "dosbox.h"
#include "dos_inc.h"

#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>

extern unsigned int lfn_id[256];
extern bool enable_network_redirector;
extern uint16_t	NetworkHandleList[127];	/*in dos_files.cpp*/

 bool Network_IsNetworkResource(const char * filename)
{
	if(strlen(filename)>1 && enable_network_redirector && !control->SecureMode())
		return (filename[0]=='\\' && filename[1]=='\\' || strlen(filename)>2 && filename[0]=='"' && filename[1]=='\\' && filename[2]=='\\');
	else
		return false;
}//bool	Network_IsNetworkFile(uint16_t entry)


 bool Network_IsActiveResource(uint16_t entry)
{
    if (!enable_network_redirector || control->SecureMode()) return false;
	uint32_t		handle=RealHandle(entry);
	return	(NetworkHandleList[entry]==handle);
}//bool	Network_IsNetworkFile(uint16_t entry)

WIN32_FIND_DATA fdw;
HANDLE hFind = INVALID_HANDLE_VALUE;
 bool Network_FindNext(DOS_DTA & dta)
 {
	uint8_t fattr;char pattern[CROSS_LEN];
	uint16_t date, time, attr;
	std::string name;
	SYSTEMTIME lt;
	dta.GetSearchParams(fattr,pattern,true);
    if (hFind != INVALID_HANDLE_VALUE) {
        while (FindNextFile(hFind, &fdw)) {
            if ((fattr & DOS_ATTR_DIRECTORY)||!(fdw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                FileTimeToSystemTime(&fdw.ftLastWriteTime, &lt);
                if (*fdw.cAlternateFileName)
                    name = fdw.cAlternateFileName;
                else {
                    name = fdw.cFileName;
                    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
                }
                date = DOS_PackDate(lt.wYear,lt.wMonth,lt.wDay);
                time = DOS_PackTime(lt.wHour,lt.wMinute,lt.wSecond);
                attr = (fdw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DOS_ATTR_DIRECTORY : 0) | (fdw.dwFileAttributes & 0x3f);
                dta.SetResult(name.c_str(),fdw.cFileName,fdw.nFileSizeLow,date,time,attr);
                return true;
            }
        }
        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }
	if (lfn_filefind_handle<LFN_FILEFIND_MAX)
		lfn_id[lfn_filefind_handle]=0;
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

 bool Network_FindFirst(const char * _dir,DOS_DTA & dta)
{
	uint8_t fattr;char pattern[CROSS_LEN];
	uint16_t date, time, attr;
	std::string name;
	SYSTEMTIME lt;
	dta.GetSearchParams(fattr,pattern,true);
	if (lfn_filefind_handle>=LFN_FILEFIND_MAX)
		dta.SetDirID(65534);
	else
		lfn_id[lfn_filefind_handle]=65534;
	if (fattr == DOS_ATTR_VOLUME) return false;
    std::string search = std::string(_dir)+"\\"+std::string(pattern);
    if (search.size() > 4 && search[0]=='\\' && search[1]=='\\' && search[2]!='\\' && std::count(search.begin()+3, search.end(), '\\')==1) search += "\\";
    hFind = FindFirstFile(search.c_str(), &fdw);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fattr & DOS_ATTR_DIRECTORY)||!(fdw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                FileTimeToSystemTime(&fdw.ftLastWriteTime, &lt);
                if (*fdw.cAlternateFileName)
                    name = fdw.cAlternateFileName;
                else {
                    name = fdw.cFileName;
                    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
                }
                date = DOS_PackDate(lt.wYear,lt.wMonth,lt.wDay);
                time = DOS_PackTime(lt.wHour,lt.wMinute,lt.wSecond);
                attr = (fdw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DOS_ATTR_DIRECTORY : 0) | (fdw.dwFileAttributes & 0x3f);
                dta.SetResult(name.c_str(),fdw.cFileName,fdw.nFileSizeLow,date,time,attr);
                return true;
            }
        } while (FindNextFile(hFind, &fdw));
        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }
	if (lfn_filefind_handle<LFN_FILEFIND_MAX)
		lfn_id[lfn_filefind_handle]=0;
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

 bool Network_MakeDir(char const * const dir)
{
    std::string name = dir;
    if (dir[0]=='"') {
        name=dir+1;
        if (name.back()=='"') name.pop_back();
    }
	if (CreateDirectory(name.c_str(), NULL))
		return true;
	uint16_t error=(uint16_t)GetLastError();
	DOS_SetError(error==ERROR_ALREADY_EXISTS?DOSERR_ACCESS_DENIED:error);
	return false;
}

 bool Network_RemoveDir(char const * const dir)
{
    std::string name = dir;
    if (dir[0]=='"') {
        name=dir+1;
        if (name.back()=='"') name.pop_back();
    }
	if (RemoveDirectory(name.c_str()))
		return true;
	uint16_t error=(uint16_t)GetLastError();
	DOS_SetError((error==ERROR_DIRECTORY||error==ERROR_DIR_NOT_EMPTY)?DOSERR_ACCESS_DENIED:error);
	return false;
}

 bool Network_Rename(char const * const oldname,char const * const newname)
{
    std::string name1 = oldname, name2 = newname;
    if (oldname[0]=='"') {
        name1=oldname+1;
        if (name1.back()=='"') name1.pop_back();
    }
    if (newname[0]=='"') {
        name2=newname+1;
        if (name2.back()=='"') name2.pop_back();
    }
	if (MoveFile(name1.c_str(), name2.c_str()))
		return true;
	uint16_t error=(uint16_t)GetLastError();
	if (error == ERROR_ALREADY_EXISTS)												// Not kwnown by DOS
		error = DOSERR_ACCESS_DENIED;
	DOS_SetError(error);
	return false;
}

bool Network_UnlinkFile(char const * const filename) {
    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
	if (DeleteFile(name.c_str()))
		return true;
	DOS_SetError((uint16_t)GetLastError());
	if (dos.errorcode == 0x20)														// Sharing violation
		dos.errorcode = 5;															// Should be reported as Access denied
	return false;
}

bool Network_SetFileAttr(char const * const filename, uint16_t attr) {
    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
	if (!SetFileAttributes(name.c_str(), attr)) {
		DOS_SetError((uint16_t)GetLastError());
		return false;
	}
	return true;
}

 bool Network_GetFileAttr(const char * filename,uint16_t * attr)
{
    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
	Bitu attribs = GetFileAttributes(name.c_str());
	if (attribs == INVALID_FILE_ATTRIBUTES) {
		DOS_SetError((uint16_t)GetLastError());
		return false;
	}
	*attr = attribs&0x3f;
	return true;
}

 bool Network_FileExists(const char* filename)
{
    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
    DWORD dwAttrib = GetFileAttributes(name.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

 bool Network_CreateFile(char const * filename,uint16_t attributes,uint16_t * entry)
{
	int attribs = FILE_ATTRIBUTE_NORMAL;
	if (attributes&3)																		// Read-only (1), Hidden (2), System (4) are the same in DOS and Windows
		attribs = attributes&3;															// We dont want (Windows) system files

    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
	int	handle=_sopen(name.c_str(),_O_CREAT|_O_TRUNC|O_RDWR|O_BINARY,_SH_DENYNO,_S_IREAD|_S_IWRITE);

	if(handle!=-1)
	{
		DOS_PSP		psp(dos.psp());
		*entry = psp.FindFreeFileEntry();
		psp.SetFileHandle(*entry,handle);

		NetworkHandleList[*entry]=handle;

		return	true;
	}
	else	dos.errorcode=(uint16_t)_doserrno;

	return false;
}//bool	Network_CreateFile(char const * filename,uint16_t attributes,uint16_t * entry)

 bool Network_OpenFile(const char * filename,uint8_t flags,uint16_t * entry)
{
	int oflag=_O_BINARY;
	int shflag=0;

	//bit0-bit2:access mode
	//000:read only 001:write only 010:read/write
	switch(flags & 0x3)
	{
	case 0:
		oflag|=_O_RDONLY;
		break;
	case 1:
		oflag|=_O_WRONLY;
		break;
	case 2:
		oflag|=_O_RDWR;
		break;
	}

	//bit4-bit6:share mode
	//000:compatibility mode 001:deny all 010:deny write 011:deny read 100:deny none
	switch((flags>>4) & 0x4)
	{
	case 0:
		shflag|=_SH_DENYNO;
		break;
	case 1:
		shflag|=_SH_DENYRW;
		break;
	case 2:
		shflag|=_SH_DENYWR;
		break;
	case 3:
		shflag|=_SH_DENYRD;
		break;
	case 4:
		shflag|=_SH_DENYNO;
		break;
	}

    std::string name = filename;
    if (filename[0]=='"') {
        name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
	int	handle=_sopen(name.c_str(),oflag,shflag,_S_IREAD|_S_IWRITE);

	if(handle!=-1)
	{
		DOS_PSP		psp(dos.psp());
		*entry = psp.FindFreeFileEntry();
		psp.SetFileHandle(*entry,handle);

		NetworkHandleList[*entry]=handle;

		return	true;
	}
	else	dos.errorcode=(uint16_t)_doserrno;

	return false;
}//bool	Network_OpenFile(char * filename,uint8_t flags,uint16_t * entry)

#if defined(__MINGW64_VERSION_MAJOR)
#define _nhandle 32
#else
extern "C" int _nhandle;
#endif

 bool Network_CloseFile(uint16_t entry)
{
	uint32_t handle=RealHandle(entry);
	int _Expr_val=!!((handle >= 0 && (unsigned)handle < (unsigned)_nhandle));
	//_ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#(handle >= 0 && (unsigned)handle < (unsigned)_nhandle)) );
	if (!(handle > 0) || ( !( _Expr_val ))) {
		_doserrno = 0L;
		errno = EBADF;
		dos.errorcode=(uint16_t)_doserrno;
		return false;
	}

	if(close(handle)==0)
	{
		NetworkHandleList[entry]=0;

		DOS_PSP		psp(dos.psp());
		psp.SetFileHandle(entry,0xff);

		return true;
	}
	else
	{
		dos.errorcode=(uint16_t)_doserrno;
		return false;
	}
}//bool	Network_CloseFile(uint16_t entry)

 bool Network_FlushFile(uint16_t entry)
{
	uint32_t handle=RealHandle(entry);
	HANDLE hand = (HANDLE)_get_osfhandle(handle);
	if (hand == INVALID_HANDLE_VALUE) {
		dos.errorcode = DOSERR_INVALID_HANDLE;
		return false;
	}
	if (FlushFileBuffers(hand))
		return true;
	else {
		dos.errorcode=(uint16_t)GetLastError();
		return false;
	}
}//bool	Network_FlushFile(uint16_t entry)

 bool Network_ReadFile(uint16_t entry,uint8_t * data,uint16_t * amount)
{
	uint32_t handle=RealHandle(entry);
	uint16_t toread=*amount;

	toread=_read(handle,data,toread);
	if (toread>=0) 
		*amount=toread;
	else
	{
		*amount=0;
		dos.errorcode=(uint16_t)_doserrno;
	}

	return	(toread>=0);
}//bool Network_ReadFile(uint16_t entry,uint8_t * data,uint16_t * amount)

 bool Network_SeekFile(uint16_t entry,uint32_t * pos,uint32_t type)
{
	uint32_t handle=RealHandle(entry);
	long topos=*pos;

	topos=_lseek(handle,*pos,type);
	if (topos!=-1)
		*pos=topos;
	else
		dos.errorcode=(uint16_t)_doserrno;
	return (topos!=-1);
}//bool Network_SeekFile(uint16_t entry,uint32_t * pos,uint32_t type)

 bool	Network_WriteFile(uint16_t entry,const uint8_t * data,uint16_t * amount)
{
		uint16_t towrite=*amount;
		uint32_t handle=RealHandle(entry);

		towrite=_write(handle,data,towrite);
		if(towrite!=-1)
			*amount=towrite;
		else 
		{
			dos.errorcode=(uint16_t)_doserrno;
			*amount=0;
		}
		return (towrite!=-1);
}//bool	Network_WriteFile(uint16_t entry,uint8_t * data,uint16_t * amount)

#endif // defined(WIN32) && !defined(__MINGW32__)
