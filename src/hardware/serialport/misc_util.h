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

#include <vector>
#ifndef DOSBOX_SUPPORT_H
#include "support.h"
#endif

// Netwrapper Capabilities
#define NETWRAPPER_TCP 1
#define NETWRAPPER_TCP_NATIVESOCKET 2

#if defined WIN32
 #define NATIVESOCKETS
 #include <winsock2.h>
 #include <ws2tcpip.h> //for socklen_t

//Tests for BSD/OS2/LINUX
#elif defined HAVE_STDLIB_H && defined HAVE_SYS_TYPES_H && defined HAVE_SYS_SOCKET_H && defined HAVE_NETINET_IN_H
 #define NATIVESOCKETS
 #define SOCKET int
 #include <stdio.h> //darwin
 #include <stdlib.h> //darwin
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
#endif

// Using a non-blocking connection routine really should
// require changes to softmodem to prevent bogus CONNECT
// messages.  By default, we use the old blocking one.
// This is basically how TCP behaves anyway.
//#define ENET_BLOCKING_CONNECT

#include <queue>
#ifndef ENET_BLOCKING_CONNECT
#include <ctime>
#endif

#include <SDL_net.h>

#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include "enet.h"
#endif

uint32_t Netwrapper_GetCapabilities();

enum SocketTypesE { SOCKET_TYPE_TCP = 0, SOCKET_TYPE_ENET, SOCKET_TYPE_COUNT };

// helper functions
bool NetWrapper_InitializeSDLNet();
bool NetWrapper_InitializeENET();

enum class SocketState {
	Good,  // had data and socket is open
	Empty, // didn't have data but socket is open
	Closed // didn't have data and socket is closed
};

// --- GENERIC NET INTERFACE -------------------------------------------------

class NETClientSocket {
public:
	NETClientSocket();
	virtual ~NETClientSocket();

	NETClientSocket(const NETClientSocket &) = delete; // prevent copying
	NETClientSocket &operator=(const NETClientSocket &) = delete; // prevent assignment

	static NETClientSocket *NETClientFactory(SocketTypesE socketType,
	                                         const char *destination,
	                                         uint16_t port);

	virtual SocketState GetcharNonBlock(uint8_t &val) = 0;
	virtual bool Putchar(uint8_t val) = 0;
	virtual bool SendArray(const uint8_t *data, size_t n) = 0;
	virtual bool ReceiveArray(uint8_t *data, size_t &n) = 0;
	virtual bool GetRemoteAddressString(char *buffer) = 0;

	void FlushBuffer();
	void SetSendBufferSize(size_t n);
	bool SendByteBuffered(uint8_t val);

	bool isopen = false;

private:
	size_t sendbufferindex = 0;
	std::vector<uint8_t> sendbuffer = {};
};

class NETServerSocket {
public:
	NETServerSocket();
	virtual ~NETServerSocket();

	NETServerSocket(const NETServerSocket &) = delete; // prevent copying
	NETServerSocket &operator=(const NETServerSocket &) = delete; // prevent assignment

	static NETServerSocket *NETServerFactory(SocketTypesE socketType,
	                                         uint16_t port);

	virtual NETClientSocket *Accept() = 0;

	bool isopen = false;
};

// --- ENET UDP NET INTERFACE ------------------------------------------------

#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
class ENETServerSocket : public NETServerSocket {
public:
	ENETServerSocket(uint16_t port);
	ENETServerSocket(const ENETServerSocket &) = delete; // prevent copying
	ENETServerSocket &operator=(const ENETServerSocket &) = delete; // prevent assignment

	~ENETServerSocket();

	NETClientSocket *Accept();

private:
	ENetHost    *host      = nullptr;
	ENetAddress  address   = {};
	bool         nowClient = false;
};

class ENETClientSocket : public NETClientSocket {
public:
	ENETClientSocket(ENetHost *host);
	ENETClientSocket(const char *destination, uint16_t port);
	ENETClientSocket(const ENETClientSocket &) = delete; // prevent copying
	ENETClientSocket &operator=(const ENETClientSocket &) = delete; // prevent assignment

	~ENETClientSocket();

	SocketState GetcharNonBlock(uint8_t &val);
	bool Putchar(uint8_t val);
	bool SendArray(const uint8_t *data, size_t n);
	bool ReceiveArray(uint8_t *data, size_t &n);
	bool GetRemoteAddressString(char *buffer);

private:
	void updateState();

#ifndef ENET_BLOCKING_CONNECT
	int64_t              connectStart  = 0;
	bool                 connecting    = false;
#endif
	ENetHost            *client        = nullptr;
	ENetPeer            *peer          = nullptr;
	ENetAddress          address       = {};
	std::queue<uint8_t>  receiveBuffer = {};
};
#endif

// --- TCP NET INTERFACE -----------------------------------------------------

struct _TCPsocketX {
	int ready = 0;
#ifdef NATIVESOCKETS
	SOCKET channel = 0;
#endif
	IPaddress remoteAddress = {0, 0};
	IPaddress localAddress = {0, 0};
	int sflag = 0;
};

class TCPClientSocket : public NETClientSocket {
public:
	TCPClientSocket(TCPsocket source);
	TCPClientSocket(const char *destination, uint16_t port);
#ifdef NATIVESOCKETS
	TCPClientSocket(int platformsocket);
#endif
	TCPClientSocket(const TCPClientSocket&) = delete; // prevent copying
	TCPClientSocket& operator=(const TCPClientSocket&) = delete; // prevent assignment

	~TCPClientSocket();

	SocketState GetcharNonBlock(uint8_t &val);
	bool Putchar(uint8_t val);
	bool SendArray(const uint8_t *data, size_t n);
	bool ReceiveArray(uint8_t *data, size_t &n);
	bool GetRemoteAddressString(char *buffer);

private:

#ifdef NATIVESOCKETS
	_TCPsocketX *nativetcpstruct = nullptr;
#endif

	TCPsocket mysock = nullptr;
	SDLNet_SocketSet listensocketset = nullptr;
};

class TCPServerSocket : public NETServerSocket {
public:
	TCPsocket mysock = nullptr;

	TCPServerSocket(uint16_t port);
	TCPServerSocket(const TCPServerSocket&) = delete; // prevent copying
	TCPServerSocket& operator=(const TCPServerSocket&) = delete; // prevent assignment

	~TCPServerSocket();

	NETClientSocket *Accept();
};

#endif // C_MODEM

#endif //# SDLNETWRAPPER_H
