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


#include "libserial.h"

#include "config.h"

#ifdef WIN32

#include <windows.h>
#include <stdio.h>

struct _COMPORT {
	HANDLE porthandle;
	bool breakstatus;
	DCB orig_dcb;
};

bool SERIAL_open(const char* portname, COMPORT* port) {
	// allocate COMPORT structure
	COMPORT cp = (_COMPORT*)malloc(sizeof(_COMPORT));
	if(cp == NULL) return false;
	
	cp->breakstatus=false;

	// open the port in NT object space (recommended by Microsoft)
	// allows the user to open COM10+ and custom port names.
    size_t len = strlen(portname);
	if(len > 240) {
		SetLastError(ERROR_BUFFER_OVERFLOW);
		free(cp);
		return false;
	}
	char extended_portname[256] = "\\\\.\\";
	memcpy(extended_portname+4,portname,len+1);
	
	cp->porthandle = CreateFile (extended_portname,
					   GENERIC_READ | GENERIC_WRITE, 0,
									  // must be opened with exclusive-access
	                   NULL,          // no security attributes
	                   OPEN_EXISTING, // must use OPEN_EXISTING
	                   0,             // non overlapped I/O
	                   NULL           // hTemplate must be NULL for comm devices
	                  );

	if (cp->porthandle == INVALID_HANDLE_VALUE) goto cleanup_error;
	
	cp->orig_dcb.DCBlength=sizeof(DCB);

	if(!GetCommState(cp->porthandle, &cp->orig_dcb)) {
		goto cleanup_error;
	}

	// configure the port for polling
	DCB newdcb;
	memcpy(&newdcb,&cp->orig_dcb,sizeof(DCB));

	newdcb.fBinary=true;
	newdcb.fParity=true;
	newdcb.fOutxCtsFlow=false;
	newdcb.fOutxDsrFlow=false;
	newdcb.fDtrControl=DTR_CONTROL_DISABLE;
	newdcb.fDsrSensitivity=false;
	
	newdcb.fOutX=false;
	newdcb.fInX=false;
	newdcb.fErrorChar=0;
	newdcb.fNull=false;
	newdcb.fRtsControl=RTS_CONTROL_DISABLE;
	newdcb.fAbortOnError=false;

	if(!SetCommState(cp->porthandle, &newdcb)) {
		goto cleanup_error;
	}

	// Configure timeouts to effectively use polling
	COMMTIMEOUTS ct;
	ct.ReadIntervalTimeout = MAXDWORD;
	ct.ReadTotalTimeoutConstant = 0;
	ct.ReadTotalTimeoutMultiplier = 0;
	ct.WriteTotalTimeoutConstant = 0;
	ct.WriteTotalTimeoutMultiplier = 0;
	if(!SetCommTimeouts(cp->porthandle, &ct)) {
		goto cleanup_error;
	}
	if(!ClearCommBreak(cp->porthandle)) {
		// Bluetooth Bluesoleil seems to not implement it
		//goto cleanup_error;
	}
	DWORD errors;
	if(!ClearCommError(cp->porthandle, &errors, NULL)) {
		goto cleanup_error;
	}
	*port = cp;
	return true;

cleanup_error:
	if (cp->porthandle != INVALID_HANDLE_VALUE) CloseHandle(cp->porthandle);
	free(cp);
	return false;
}

void SERIAL_close(COMPORT port) {
	// restore original DCB, close handle, free the COMPORT struct
	if (port->porthandle != INVALID_HANDLE_VALUE) {
		SetCommState(port->porthandle, &port->orig_dcb);
		CloseHandle(port->porthandle);
	}
	free(port);
}

void SERIAL_getErrorString(char* buffer, size_t length) {
	int error = GetLastError();
	if(length < 50) return;
	memset(buffer,0,length);
	// get the error message text from the operating system
	LPVOID sysmessagebuffer;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &sysmessagebuffer,
		0,NULL);

    size_t sysmsg_offset = 0;

	if(error == 5) {
		const char* err5text = "The specified port is already in use.\n";
		sysmsg_offset = (int)strlen(err5text);
		memcpy(buffer,err5text,sysmsg_offset);

	} else if(error == 2) {
		const char* err2text = "The specified port does not exist.\n";
		sysmsg_offset = (int)strlen(err2text);
		memcpy(buffer,err2text,sysmsg_offset);
	}

	// Go for length > so there will be bytes left afterwards.
	// (which are 0 due to memset, thus the buffer is 0 terminated
	if ( length > (sysmsg_offset + (int)strlen((const char*)sysmessagebuffer)) ) {
		memcpy(buffer + sysmsg_offset, sysmessagebuffer,
		strlen((const char*)sysmessagebuffer));
	}
		
	LocalFree(sysmessagebuffer);
}


void SERIAL_setDTR(COMPORT port, bool value) {
	EscapeCommFunction(port->porthandle, value ? SETDTR:CLRDTR);
}

void SERIAL_setRTS(COMPORT port, bool value) {
	EscapeCommFunction(port->porthandle, value ? SETRTS:CLRRTS);
}

void SERIAL_setBREAK(COMPORT port, bool value) {
	EscapeCommFunction(port->porthandle, value ? SETBREAK:CLRBREAK);
	port->breakstatus = value;
}

int SERIAL_getmodemstatus(COMPORT port) {
	DWORD retval = 0;
	GetCommModemStatus (port->porthandle, &retval);
	return (int)retval;
}

bool SERIAL_sendchar(COMPORT port, char data) {
	DWORD bytesWritten;

	// mean bug: with break = 1, WriteFile will never return.
	if(port->breakstatus) return true; // true or false?!

	WriteFile (port->porthandle, &data, 1, &bytesWritten, NULL);
	if(bytesWritten==1) return true;
	else return false;
}

// 0-7 char data, higher=flags
int SERIAL_getextchar(COMPORT port) {
	DWORD errors = 0;	// errors from API
	DWORD dwRead = 0;	// Number of chars read
	char chRead;

	int retval = 0;
	// receive a byte; TODO communicate failure
	if (ReadFile (port->porthandle, &chRead, 1, &dwRead, NULL)) {
		if (dwRead) {
			// check for errors
			ClearCommError(port->porthandle, &errors, NULL);
			// mask bits are identical
			errors &= CE_BREAK|CE_FRAME|CE_RXPARITY|CE_OVERRUN;
			retval |= (errors<<8);
			retval |= (chRead & 0xff);
			retval |= 0x10000; 
		}
	}
	return retval;
}

bool SERIAL_setCommParameters(COMPORT port,
			int baudrate, char parity, int stopbits, int length) {
	
	DCB dcb;
	dcb.DCBlength=sizeof(dcb);
	GetCommState(port->porthandle,&dcb);

	// parity
	switch (parity) {
	case 'n': dcb.Parity = NOPARITY; break;
	case 'o': dcb.Parity = ODDPARITY; break;
	case 'e': dcb.Parity = EVENPARITY; break;
	case 'm': dcb.Parity = MARKPARITY; break;
	case 's': dcb.Parity = SPACEPARITY;	break;
	default:
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	// stopbits
	switch(stopbits) {
	case SERIAL_1STOP: dcb.StopBits = ONESTOPBIT; break;
	case SERIAL_2STOP: dcb.StopBits = TWOSTOPBITS; break;
	case SERIAL_15STOP: dcb.StopBits = ONE5STOPBITS; break;
	default:
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	// byte length
	if(length > 8 || length < 5) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}
	dcb.ByteSize = length;
	dcb.BaudRate = baudrate;

	if (!SetCommState (port->porthandle, &dcb)) return false;
	return true;
}
#endif

#if defined (LINUX) || defined (MACOSX) || defined (BSD) || defined (HAIKU)

#include <string.h> // strlen
#include <stdlib.h>

#include <termios.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h> // sprinf

struct _COMPORT {
	int porthandle;
	bool breakstatus;
	termios backup;
};

bool SERIAL_open(const char* portname, COMPORT* port) {
	int result;
	// allocate COMPORT structure
	COMPORT cp = (_COMPORT*)malloc(sizeof(_COMPORT));
	if(cp == NULL) return false;

	cp->breakstatus=false;

    size_t len = strlen(portname);
	if(len > 240) {
		free(cp);
		///////////////////////////////////SetLastError(ERROR_BUFFER_OVERFLOW);
		return false;
	}
	char extended_portname[256] = "/dev/";
	memcpy(extended_portname+5,portname,(size_t)len);

	cp->porthandle = open (extended_portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (cp->porthandle < 0) goto cleanup_error;

	result = tcgetattr(cp->porthandle,&cp->backup);
	if (result==-1) goto cleanup_error;

	// get port settings
	termios termInfo;
	memcpy(&termInfo,&cp->backup,sizeof(termios));

	// initialize the port
	termInfo.c_cflag = CS8 | CREAD | CLOCAL; // noparity, 1 stopbit
	termInfo.c_iflag = PARMRK | INPCK;
	termInfo.c_oflag = 0;
	termInfo.c_lflag = 0;
	termInfo.c_cc[VMIN] = 0;
	termInfo.c_cc[VTIME] = 0;

	tcflush (cp->porthandle, TCIFLUSH);
	tcsetattr (cp->porthandle, TCSANOW, &termInfo);

	*port = cp;
	return true;

cleanup_error:
	if (cp->porthandle != 0) close(cp->porthandle);
	free(cp);
	return false;
}

void SERIAL_close(COMPORT port) {
	// restore original termios, close handle, free the COMPORT struct
	if (port->porthandle >= 0) {
		tcsetattr(port->porthandle, TCSANOW, &port->backup);
		close(port->porthandle);
	}
	free(port);
}

void SERIAL_getErrorString(char* buffer, size_t length) {
	int error = errno;
	if(length < 50) return;
	memset(buffer,0,(size_t)length);
	// get the error message text from the operating system
	// TODO (or not)
	
	const char* err5text = "The specified port is already in use.\n";
	const char* err2text = "The specified port does not exist.\n";
	
    size_t sysmsg_offset = 0;

	if(error == EBUSY) {
		sysmsg_offset = strlen(err5text);
		memcpy(buffer,err5text,(size_t)sysmsg_offset);

	} else if(error == 2) {
		sysmsg_offset = strlen(err2text);
		memcpy(buffer,err2text,(size_t)sysmsg_offset);
	}
	
	sprintf(buffer + sysmsg_offset, "System error %d.",error);
	
}

int SERIAL_getmodemstatus(COMPORT port) {
	long flags = 0;
	ioctl (port->porthandle, TIOCMGET, &flags);
	int retval = 0;
	if (flags & TIOCM_CTS) retval |= SERIAL_CTS;
	if (flags & TIOCM_DSR) retval |= SERIAL_DSR;
	if (flags & TIOCM_RI) retval |= SERIAL_RI;
	if (flags & TIOCM_CD) retval |= SERIAL_CD;
	return retval;
}

bool SERIAL_sendchar(COMPORT port, char data) {
	if(port->breakstatus) return true; // true or false?!; Does POSIX need this check?
	int bytesWritten = write(port->porthandle, &data, 1);
	if(bytesWritten==1) return true;
	else return false;
}

int SERIAL_getextchar(COMPORT port) {
	unsigned char chRead = 0;
	int dwRead = 0;
	unsigned char error = 0;
	int retval = 0;

	dwRead=read(port->porthandle,&chRead,1);
	if (dwRead==1) {
		if(chRead==0xff) // error escape
		{
			dwRead=read(port->porthandle,&chRead,1);
			if(chRead==0x00) // an error 
			{
				dwRead=read(port->porthandle,&chRead,1);
				if(chRead==0x0) error=SERIAL_BREAK_ERR;
				else error=SERIAL_FRAMING_ERR;
			}
		}
		retval |= (error<<8);
		retval |= chRead;
		retval |= 0x10000; 
	}
	return retval;
}

bool SERIAL_setCommParameters(COMPORT port,
			int baudrate, char parity, int stopbits, int length) {
	
	termios termInfo;
	int result = tcgetattr(port->porthandle, &termInfo);
	if (result==-1) return false;
	termInfo.c_cflag = CREAD | CLOCAL;

	// parity
	// "works on many systems"
	#define CMSPAR 010000000000
	switch (parity) {
	case 'n': break;
	case 'o': termInfo.c_cflag |= (PARODD | PARENB); break;
	case 'e': termInfo.c_cflag |= PARENB; break;
	case 'm': termInfo.c_cflag |= (PARENB | CMSPAR | PARODD); break;
	case 's': termInfo.c_cflag |= (PARENB | CMSPAR); break;
	default:
		return false;
	}
	// stopbits
	switch(stopbits) {
	case SERIAL_1STOP: break;
	case SERIAL_2STOP: 
	case SERIAL_15STOP: termInfo.c_cflag |= CSTOPB; break;
	default:
		return false;
	}
	// byte length
	if(length > 8 || length < 5) return false;
	switch (length) {
	case 5: termInfo.c_cflag |= CS5; break;
	case 6: termInfo.c_cflag |= CS6; break;
	case 7: termInfo.c_cflag |= CS7; break;
	case 8: termInfo.c_cflag |= CS8; break;
	}
	// baudrate
	int posix_baudrate=0;
	switch(baudrate) {
		case 115200: posix_baudrate = B115200; break;
		case  57600: posix_baudrate = B57600; break;
		case  38400: posix_baudrate = B38400; break;
		case  19200: posix_baudrate = B19200; break;
		case   9600: posix_baudrate = B9600; break;
		case   4800: posix_baudrate = B4800; break;
		case   2400: posix_baudrate = B2400; break;
		case   1200: posix_baudrate = B1200; break;
		case    600: posix_baudrate = B600; break;
		case    300: posix_baudrate = B300; break;
		case    110: posix_baudrate = B110; break;
		default: return false;
	}
	cfsetospeed (&termInfo, (unsigned int)posix_baudrate);
	cfsetispeed (&termInfo, (unsigned int)posix_baudrate);

	int retval = tcsetattr(port->porthandle, TCSANOW, &termInfo);
	if(retval==-1) return false;
	return true;
}

void SERIAL_setBREAK(COMPORT port, bool value) {
	ioctl(port->porthandle, value?TIOCSBRK:TIOCCBRK);
}

void SERIAL_setDTR(COMPORT port, bool value) {
	long flag = TIOCM_DTR;
	ioctl(port->porthandle, value?TIOCMBIS:TIOCMBIC, &flag);
}

void SERIAL_setRTS(COMPORT port, bool value) {
	long flag = TIOCM_RTS;
	ioctl(port->porthandle, value?TIOCMBIS:TIOCMBIC, &flag);
}
#endif

#ifdef OS2
// OS/2 related headers
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#include <os2.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

struct _COMPORT {
	HFILE porthandle;
	DCBINFO orig_dcb;
};
// TODO: THIS IS INCOMPLETE and UNTESTED.

bool SERIAL_open(const char* portname, COMPORT* port) {
	// allocate COMPORT structure
	COMPORT cp = (_COMPORT*)malloc(sizeof(_COMPORT));
	if(cp == NULL) return false;
	cp->porthandle=0;

	USHORT errors = 0;
	ULONG ulAction = 0;
	ULONG ulParmLen = sizeof(DCBINFO);
	APIRET rc = DosOpen((PSZ)portname, &cp->porthandle,
		&ulAction, 0L, FILE_NORMAL, FILE_OPEN,
		OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE | OPEN_FLAGS_SEQUENTIAL, 0L);
	if (rc != NO_ERROR) {
		goto cleanup_error;
	}

	rc = DosDevIOCtl(cp->porthandle, IOCTL_ASYNC, ASYNC_GETDCBINFO,
		0, 0, 0, &cp->orig_dcb, ulParmLen, &ulParmLen);
	if ( rc != NO_ERROR) {
		goto cleanup_error;
	}
	// configure the port for polling
	DCBINFO newdcb;
	memcpy(&newdcb,&cp->orig_dcb,sizeof(DCBINFO));

	newdcb.usWriteTimeout = 0;
	newdcb.usReadTimeout = 0; //65535;
	newdcb.fbCtlHndShake = cp->orig_dcb.fbFlowReplace = 0;
	newdcb.fbTimeout = 6;

	rc = DosDevIOCtl(cp->porthandle, IOCTL_ASYNC, ASYNC_SETDCBINFO,
		&newdcb, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR) {
		goto cleanup_error;
	}

	ulParmLen = sizeof(errors);
	rc = DosDevIOCtl(cp->porthandle, IOCTL_ASYNC, ASYNC_GETCOMMERROR,
		0, 0, 0, &errors, ulParmLen, &ulParmLen);
	if ( rc != NO_ERROR) {
		goto cleanup_error;
	}

	*port = cp;
	return true;

cleanup_error:
	// TODO error string - rc value
	if (cp->porthandle != 0) DosClose(cp->porthandle);
	free(cp);
	return false;
}

void SERIAL_getErrorString(char* buffer, size_t length) {
	sprintf(buffer, "TODO: error handling is not fun");
}
void SERIAL_close(COMPORT port) {
	ULONG ulParmLen = sizeof(DCBINFO);
	// restore original DCB, close handle, free the COMPORT struct
	if (port->porthandle != 0) {
		DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_SETDCBINFO,
			&port->orig_dcb, ulParmLen, &ulParmLen,	0, 0, 0);
		DosClose (port->porthandle);
	}
	free(port);
}
bool SERIAL_sendchar(COMPORT port, char data) {
	ULONG bytesWritten = 0;

	APIRET rc = DosWrite(port->porthandle, &data, 1, &bytesWritten);
	if (rc == NO_ERROR && bytesWritten > 0) return true;
	else return false;
}

void SERIAL_setBREAK(COMPORT port, bool value) {
	USHORT error;
	ULONG ulParmLen = sizeof(error);
	DosDevIOCtl(port->porthandle, IOCTL_ASYNC,
		value ? ASYNC_SETBREAKON : ASYNC_SETBREAKOFF,
		0,0,0, &error, ulParmLen, &ulParmLen);
}

int SERIAL_getextchar(COMPORT port) {
	ULONG dwRead = 0;	// Number of chars read
	char chRead;

	int retval = 0;
	// receive a byte; TODO communicate failure
	if (DosRead(port->porthandle, &chRead, 1, &dwRead) == NO_ERROR) {
		if (dwRead) {
			// check for errors; will OS/2 clear the error on reading its data?
			// if yes then this is in wrong order
			USHORT errors = 0, event = 0;
			ULONG ulParmLen = sizeof(errors);
			DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_GETCOMMEVENT,
				0, 0, 0, &event, ulParmLen, &ulParmLen);
			if (event & (64 + 128) ) { // Break (Bit 6) or Frame or Parity (Bit 7) error
				uint8_t errreg = 0;
				if (event & 64) retval |= SERIAL_BREAK_ERR;
				if (event & 128) {
					DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_GETCOMMERROR,
						0, 0, 0, &errors, ulParmLen, &ulParmLen);
					if (errors & 8) retval |= SERIAL_FRAMING_ERR;
					if (errors & 4) retval |= SERIAL_PARITY_ERR;
				}
			}
			retval |= (chRead & 0xff);
			retval |= 0x10000; 
		}
	}
	return retval;
}


int SERIAL_getmodemstatus(COMPORT port) {
	UCHAR dptr = 0;
	ULONG ulParmLen = sizeof(dptr);
	DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_GETMODEMINPUT,
		0, 0, 0, &dptr, ulParmLen, &ulParmLen);
	// bits are the same as return value
	return (int)dptr;
}
void SERIAL_setDTR(COMPORT port, bool value) {
	UCHAR masks[2];
	ULONG ulParmLen = sizeof(masks);
	if(value) {
		masks[0]=0x01;
		masks[1]=0xFF;
	} else {
		masks[0]=0x00;
		masks[1]=0xFE;
	}
	DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_SETMODEMCTRL,
		0,0,0, &masks, ulParmLen, &ulParmLen);
}

void SERIAL_setRTS(COMPORT port, bool value) {
	UCHAR masks[2];
	ULONG ulParmLen = sizeof(masks);
	if(value) {
		masks[0]=0x02;
		masks[1]=0xFF;
	} else {
		masks[0]=0x00;
		masks[1]=0xFD;
	}
	DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_SETMODEMCTRL,
		0,0,0, &masks, ulParmLen, &ulParmLen);
}



bool SERIAL_setCommParameters(COMPORT port,
			int baudrate, char parity, int stopbits, int length) {
	// baud
	struct {
		ULONG baud;
		BYTE fraction;
	} setbaud;

	setbaud.baud = baudrate;
	setbaud.fraction = 0;
	ULONG ulParmLen = sizeof(setbaud);
	APIRET rc = DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE,
		&setbaud, ulParmLen, &ulParmLen, 0, 0, 0);
	if (rc != NO_ERROR) {
		return false;
	}

	struct {
		UCHAR data;
		UCHAR parity;
		UCHAR stop;
	} paramline;

	// byte length
	if(length > 8 || length < 5) {
		// TODO SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}
	paramline.data = length;

	// parity
	switch (parity) {
	case 'n': paramline.parity = 0; break;
	case 'o': paramline.parity = 1; break;
	case 'e': paramline.parity = 2; break;
	case 'm': paramline.parity = 3; break;
	case 's': paramline.parity = 4; break;
	default:
		// TODO SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}
	// stopbits
	switch(stopbits) {
	case SERIAL_1STOP: paramline.stop = 0; break;
	case SERIAL_2STOP: paramline.stop = 2; break;
	case SERIAL_15STOP: paramline.stop = 1; break;
	default:
		// TODO SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}
	// set it
	ulParmLen = sizeof(paramline);
	rc = DosDevIOCtl(port->porthandle, IOCTL_ASYNC, ASYNC_SETLINECTRL,
		&paramline, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
		return false;
	return true;
}
#endif
