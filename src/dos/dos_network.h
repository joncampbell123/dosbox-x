
/*
DOS	Network Routines.


Log:
July 11 2005		cyberwalker		Created.
July 23 2005		cyberwalker
        Removed code from dos.cpp to this file.
August 8 2005		cyberwalker
		Make all functions  to avoid modifying makefile.

*/

#if defined(WIN32) && !defined(HX_DOS)

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

extern uint16_t	NetworkHandleList[127];	/*in dos_files.cpp*/

inline uint16_t	Network_ToDosError(DWORD dwLastError)
{
	uint16_t	retCode;

	switch(dwLastError)
	{
	case	ERROR_BAD_PIPE:
	case	ERROR_PIPE_BUSY:
	case	ERROR_NO_DATA:
	case	ERROR_PIPE_NOT_CONNECTED:
	case	ERROR_MORE_DATA:
		retCode=(uint16_t)dwLastError;
		break;
	default:
		retCode=0x1;
		break;
	}

	return	retCode;
}//uint16_t	Network_ToDosError(DWORD dwLastError)

 bool	Network_SetNamedPipeState(uint16_t entry,
								  uint16_t mask,
								  uint16_t& errorcode)
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
}//bool	Network_SetNamedPipeState(uint16_t entry,uint16_t mask,uint16_t& errorcode)


 bool	Network_PeekNamedPipe(uint16_t entry,
							  unsigned char* lpBuffer,uint16_t nBufferSize,
							  uint16_t& cxRead,uint16_t& siLeft,uint16_t& dxLeftCurrent,
							  uint16_t& diStatus,
							  uint16_t& errorcode)
{
	int		ifd=NetworkHandleList[entry];
	HANDLE	hNamedPipe=(HANDLE)_get_osfhandle(ifd);

	DWORD	BytesRead,TotalBytesAvail,BytesLeftThisMessage;
	if(PeekNamedPipe(hNamedPipe,lpBuffer,nBufferSize,
						&BytesRead,&TotalBytesAvail,&BytesLeftThisMessage))
	{
		cxRead=(uint16_t)BytesRead;
		siLeft=(uint16_t)TotalBytesAvail;
		dxLeftCurrent=(uint16_t)BytesLeftThisMessage;
		diStatus=0x3;	//Connected
		errorcode=0;
	}
	else	errorcode=Network_ToDosError(GetLastError());

	return	(errorcode==0);
}//bool	Network_PeekNamedPipe


 bool	Network_TranscateNamedPipe(uint16_t entry,
								   unsigned char* lpInBuffer,uint16_t nInBufferSize,
								   unsigned char* lpOutBuffer,uint16_t nOutBufferSize,
								   uint16_t& cxRead,uint16_t& errorcode)
{
	int		ifd=NetworkHandleList[entry];
	HANDLE	hNamedPipe=(HANDLE)_get_osfhandle(ifd);

	DWORD	BytesRead;

	if(TransactNamedPipe(hNamedPipe,
					lpInBuffer,nInBufferSize,				
					lpOutBuffer,nOutBufferSize,
					&BytesRead,NULL))
	{
		cxRead=(uint16_t)BytesRead;
		errorcode=0;
	}
	else	errorcode=Network_ToDosError(GetLastError());

	return	(errorcode==0);
}//bool	Network_TranscateNamedPipe

#endif	//WIN32

