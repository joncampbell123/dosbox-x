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


#ifndef SDLNETWRAPPER_H
#define SDLNETWRAPPER_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

#if C_MODEM

# ifndef DOSBOX_SUPPORT_H
#include "support.h"
#endif

// Netwrapper Capabilities
#define NETWRAPPER_TCP 1
#define NETWRAPPER_TCP_NATIVESOCKET 2

#if defined WIN32
 #define NATIVESOCKETS
 #include <winsock2.h>
 #include <ws2tcpip.h> //for socklen_t
 //typedef int  socklen_t;

//Tests for BSD/OS2/LINUX
#elif defined HAVE_STDLIB_H && defined HAVE_SYS_TYPES_H && defined HAVE_SYS_SOCKET_H && defined HAVE_NETINET_IN_H
 #define NATIVESOCKETS
 #define SOCKET int
 #include <stdio.h> //darwin
 #include <stdlib.h> //darwin
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 //socklen_t should be handled by configure
#endif

#ifdef NATIVESOCKETS
 #define CAPWORD (NETWRAPPER_TCP|NETWRAPPER_TCP_NATIVESOCKET)
#else
 #define CAPWORD NETWRAPPER_TCP
#endif

#include "SDL_net.h"



uint32_t Netwrapper_GetCapabilities();


class TCPClientSocket {
	public:
	TCPClientSocket(TCPsocket source);
	TCPClientSocket(const char* destination, uint16_t port);
#ifdef NATIVESOCKETS
	uint8_t* nativetcpstruct;
	TCPClientSocket(int platformsocket);
#endif
	~TCPClientSocket();
	
	// return:
	// -1: no data
	// -2: socket closed
	// >0: data char
	Bits GetcharNonBlock();
	
	
	bool Putchar(uint8_t data);
	bool SendArray(uint8_t* data, Bitu bufsize);
	bool ReceiveArray(uint8_t* data, Bitu* size);
	bool isopen;

	bool GetRemoteAddressString(uint8_t* buffer);

	void FlushBuffer();
	void SetSendBufferSize(Bitu bufsize);
	
	// buffered send functions
	bool SendByteBuffered(uint8_t data);
	bool SendArrayBuffered(uint8_t* data, Bitu bufsize);

	private:
	TCPsocket mysock = NULL;
	SDLNet_SocketSet listensocketset = NULL;

	// Items for send buffering
	Bitu sendbuffersize = 0;
	Bitu sendbufferindex = 0;
	
	uint8_t* sendbuffer;
};

class TCPServerSocket {
	public:
	bool isopen;
	TCPsocket mysock;
	TCPServerSocket(uint16_t port);
	~TCPServerSocket();
	TCPClientSocket* Accept();
};


#endif //C_MODEM

#endif //# SDLNETWRAPPER_H
