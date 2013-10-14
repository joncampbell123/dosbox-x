
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

inline Bit16u	Network_ToDosError(DWORD dwLastError)
{
	Bit16u	retCode;

	switch(dwLastError)
	{
	case	ERROR_BAD_PIPE:
	case	ERROR_PIPE_BUSY:
	case	ERROR_NO_DATA:
	case	ERROR_PIPE_NOT_CONNECTED:
	case	ERROR_MORE_DATA:
		retCode=(Bit16u)dwLastError;
		break;
	default:
		retCode=0x1;
		break;
	}

	return	retCode;
}//Bit16u	Network_ToDosError(DWORD dwLastError)

 bool	Network_SetNamedPipeState(Bit16u entry,
								  Bit16u mask,
								  Bit16u& errorcode)
{
	int		ifd=NetworkHandleList[entry];
	HANDLE	hNamedPipe=(HANDLE)_get_osfhandle(ifd);

	DWORD	dwMode=0;

	//nonblocking/blocking
	if(mask & 0x8000)
			dwMode|=PIPE_NOWAIT;
	else	dwMode|=PIPE_WAIT;
	
	//Message/byte
	if(mask & 0x0100)
			dwMode|=PIPE_READMODE_MESSAGE;
	else	dwMode|=PIPE_READMODE_BYTE;

	if(SetNamedPipeHandleState(hNamedPipe,&dwMode,NULL,NULL))
			errorcode=0;
	else	errorcode=Network_ToDosError(GetLastError());

	return	(errorcode==0);
}//bool	Network_SetNamedPipeState(Bit16u entry,Bit16u mask,Bit16u& errorcode)


 bool	Network_PeekNamedPipe(Bit16u entry,
							  unsigned char* lpBuffer,Bit16u nBufferSize,
							  Bit16u& cxRead,Bit16u& siLeft,Bit16u& dxLeftCurrent,
							  Bit16u& diStatus,
							  Bit16u& errorcode)
{
	int		ifd=NetworkHandleList[entry];
	HANDLE	hNamedPipe=(HANDLE)_get_osfhandle(ifd);

	DWORD	BytesRead,TotalBytesAvail,BytesLeftThisMessage;
	if(PeekNamedPipe(hNamedPipe,lpBuffer,nBufferSize,
						&BytesRead,&TotalBytesAvail,&BytesLeftThisMessage))
	{
		cxRead=(Bit16u)BytesRead;
		siLeft=(Bit16u)TotalBytesAvail;
		dxLeftCurrent=(Bit16u)BytesLeftThisMessage;
		diStatus=0x3;	//Connected
		errorcode=0;
	}
	else	errorcode=Network_ToDosError(GetLastError());

	return	(errorcode==0);
}//bool	Network_PeekNamedPipe


 bool	Network_TranscateNamedPipe(Bit16u entry,
								   unsigned char* lpInBuffer,Bit16u nInBufferSize,
								   unsigned char* lpOutBuffer,Bit16u nOutBufferSize,
								   Bit16u& cxRead,Bit16u& errorcode)
{
	int		ifd=NetworkHandleList[entry];
	HANDLE	hNamedPipe=(HANDLE)_get_osfhandle(ifd);

	DWORD	BytesRead;

	if(TransactNamedPipe(hNamedPipe,
					lpInBuffer,nInBufferSize,				
					lpOutBuffer,nOutBufferSize,
					&BytesRead,NULL))
	{
		cxRead=(Bit16u)BytesRead;
		errorcode=0;
	}
	else	errorcode=Network_ToDosError(GetLastError());

	return	(errorcode==0);
}//bool	Network_TranscateNamedPipe

#endif	//WIN32

