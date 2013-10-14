
/*
DOS	Network Routines.


Log:
July 11 2005		cyberwalker		Created.
July 23 2005		cyberwalker
        Removed code from dos.cpp to this file.
August 8 2005		cyberwalker
		Make all functions  to avoid modifying makefile.

*/

#ifdef WIN32

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include "config.h"
#include "dosbox.h"
#include "dos_inc.h"

#include <io.h>
#include <fcntl.h>
#include <share.h>

extern Bit16u	NetworkHandleList[127];	/*in dos_files.cpp*/

 bool	Network_IsNetworkResource(char * filename)
{
	if(strlen(filename)>1)
			return (filename[0]=='\\' && filename[1]=='\\');
	else	return false;
}//bool	Network_IsNetworkFile(Bit16u entry)


 bool	Network_IsActiveResource(Bit16u entry)
{
	Bit32u		handle=RealHandle(entry);
	return	(NetworkHandleList[entry]==handle);
}//bool	Network_IsNetworkFile(Bit16u entry)


 bool	Network_OpenFile(char * filename,Bit8u flags,Bit16u * entry)
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

	int	handle=_sopen(filename,oflag,shflag,_S_IREAD|_S_IWRITE);

	if(handle!=-1)
	{
		DOS_PSP		psp(dos.psp());
		*entry = psp.FindFreeFileEntry();
		psp.SetFileHandle(*entry,handle);

		NetworkHandleList[*entry]=handle;

		return	true;
	}
	else	dos.errorcode=(Bit16u)_doserrno;

	return false;
}//bool	Network_OpenFile(char * filename,Bit8u flags,Bit16u * entry)

 extern "C" int _nhandle;
 bool	Network_CloseFile(Bit16u entry)
{
		Bit32u handle=RealHandle(entry);
		int _Expr_val=!!((handle >= 0 && (unsigned)handle < (unsigned)_nhandle));
		//_ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#(handle >= 0 && (unsigned)handle < (unsigned)_nhandle)) );
		if (!(handle > 0) || ( !( _Expr_val ))) {
			_doserrno = 0L;
			errno = EBADF;
			dos.errorcode=(Bit16u)_doserrno;
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
			dos.errorcode=(Bit16u)_doserrno;
			return	false;
		}
}//bool	Network_CloseFile(Bit16u entry)

 bool Network_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount)
{
	Bit32u handle=RealHandle(entry);
	Bit16u toread=*amount;

	toread=_read(handle,data,toread);
	if (toread>=0) 
			*amount=toread;
	else	
	{
		*amount=0;
		dos.errorcode=(Bit16u)_doserrno;
	}

	return	(toread>=0);
}//bool Network_ReadFile(Bit16u entry,Bit8u * data,Bit16u * amount)


 bool	Network_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount)
{
		Bit16u towrite=*amount;
		Bit32u handle=RealHandle(entry);

		towrite=_write(handle,data,towrite);
		if(towrite!=-1)
			*amount=towrite;
		else 
		{
			dos.errorcode=(Bit16u)_doserrno;
			*amount=0;
		}
		return (towrite!=-1);
}//bool	Network_WriteFile(Bit16u entry,Bit8u * data,Bit16u * amount)



#endif	//WIN32

