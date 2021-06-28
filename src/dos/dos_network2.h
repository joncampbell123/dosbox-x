
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

 bool Network_GetFileAttr(const char * filename,uint16_t * attr)
{
    std::string name = filename;
    if (filename[0]=='"') {
        std::string name=filename+1;
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
        std::string name=filename+1;
        if (name.back()=='"') name.pop_back();
    }
    DWORD dwAttrib = GetFileAttributes(name.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

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
        std::string name=filename+1;
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

			return	true;
		}
		else
		{
			dos.errorcode=(uint16_t)_doserrno;
			return	false;
		}
}//bool	Network_CloseFile(uint16_t entry)

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
