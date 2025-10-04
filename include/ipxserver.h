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

#ifndef DOSBOX_IPXSERVER_H_
#define DOSBOX_IPXSERVER_H_

#if C_IPX

#include "SDL_net.h"

struct packetBuffer {
	uint8_t buffer[1024];
	int16_t packetSize;  // Packet size remaining in read
	int16_t packetRead;  // Bytes read of total packet
	bool inPacket;      // In packet reception flag
	bool connected;		// Connected flag
	bool waitsize;
};

#define SOCKETTABLESIZE 16
#define CONVIP(hostvar) hostvar & 0xff, (hostvar >> 8) & 0xff, (hostvar >> 16) & 0xff, (hostvar >> 24) & 0xff
#define CONVIPX(hostvar) hostvar[0], hostvar[1], hostvar[2], hostvar[3], hostvar[4], hostvar[5]


void IPX_StopServer();
bool IPX_StartServer(uint16_t portnum);
bool IPX_isConnectedToServer(Bits tableNum, IPaddress ** ptrAddr);

uint8_t packetCRC(uint8_t *buffer, uint16_t bufSize);

#endif

#endif
