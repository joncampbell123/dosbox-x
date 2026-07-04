/*
 *  Copyright (C) 2021  The DOSBox Team
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

#include "config.h"

#include "ethernet_ethnet.h"
#include "dosbox.h"
#include "logging.h"
#include "callback.h"
#include "timer.h"
#include "pic.h"
#include "support.h" /* strcasecmp */
#if defined(C_SDL2_NET) && C_SDL2_NET
#include <SDL2/SDL_net.h>
#else
#include "SDL_net.h"
#endif

#define ETHNETBUFFERSIZE 4096

struct packetBuffer {
	uint8_t buffer[1024];
	int16_t packetSize;  // Packet size remaining in read
	int16_t packetRead;  // Bytes read of total packet
	bool inPacket;      // In packet reception flag
	bool connected;		// Connected flag
	bool waitsize;
};

struct PackedIP {
	Uint32 host;
	Uint16 port;
} GCC_ATTRIBUTE(packed);

#define SOCKETTABLESIZE 16
#define CONVIP(hostvar) hostvar & 0xff, (hostvar >> 8) & 0xff, (hostvar >> 16) & 0xff, (hostvar >> 24) & 0xff
#define CONVIPX(hostvar) hostvar[0], hostvar[1], hostvar[2], hostvar[3], hostvar[4], hostvar[5]

struct ethnetnetaddr {
	Uint8 netnum[4];   // Both are big endian
	Uint8 netnode[6];
} localEthnetAddr;

static uint32_t udpPort = 0;
static int UDPChannel = 0;						// Channel used by UDP connection
static bool isEthnetServer = false;
//static bool isEthnetConnected = false;
static IPaddress ethnetServerIp;  // IPAddress for server's listening port
static IPaddress ethnetServConnIp;			// IPAddress for client connection to server
static UDPsocket ethnetServerSocket;  // Listening server socket
static UDPsocket ethnetClientSocket;
//static uint8_t recvBuffer[ETHNETBUFFERSIZE];	// Incoming packet buffer

static packetBuffer incomingPacket;

EthnetEthernetConnection::EthnetEthernetConnection()
      : EthernetConnection()
{
}

EthnetEthernetConnection::~EthnetEthernetConnection()
{
}

bool EthnetEthernetConnection::Initialize(Section* config)
{
	(void)config;
	return true;
}

void EthnetEthernetConnection::SendPacket(const uint8_t* packet, int len)
{
	(void)packet;
	(void)len;
}

void EthnetEthernetConnection::GetPackets(std::function<void(const uint8_t*, int)> callback)
{
	(void)callback;
}

bool ETHNET_isConnectedToServer(Bits tableNum, IPaddress ** ptrAddr) {
	(void)tableNum;
	(void)ptrAddr;
	return false;
}

static void ETHNET_ClientLoop(void) {
	//TODO
}

static void ETHNET_ServerLoop() {
	//TODO
}

void ETHNET_StopServer() {
	TIMER_DelTickHandler(&ETHNET_ServerLoop);
	SDLNet_UDP_Close(ethnetServerSocket);
}

bool ETHNET_StartServer(uint16_t portnum) {
	if(!SDLNet_ResolveHost(&ethnetServerIp, NULL, portnum)) {
		//serverSocketSet = SDLNet_AllocSocketSet(SOCKETTABLESIZE);
		ethnetServerSocket = SDLNet_UDP_Open(portnum);
		if(!ethnetServerSocket) return false;

		TIMER_AddTickHandler(&ETHNET_ServerLoop);
		return true;
	}
	return false;
}

static bool ConnectToServer(char const *strAddr) {
	int numsent;
	UDPpacket regPacket;
	if(!SDLNet_ResolveHost(&ethnetServConnIp, strAddr, (uint16_t)udpPort)) {
		// Select an anonymous UDP port
		ethnetClientSocket = SDLNet_UDP_Open(0);
		if(ethnetClientSocket) {
			// Bind UDP port to address to channel
			UDPChannel = SDLNet_UDP_Bind(ethnetClientSocket,-1,&ethnetServConnIp);

			LOG_MSG("ETHNET: Connected to server.");

			incomingPacket.connected = true;
			TIMER_AddTickHandler(&ETHNET_ClientLoop);
			return true;
		} else {
			LOG_MSG("ETHNET: Unable to open socket");
		}
	} else {
		LOG_MSG("ETHNET: Unable resolve connection to server");
	}
	return false;
}

static void DisconnectFromServer(bool unexpected) {
	if(unexpected) LOG_MSG("ETHNET: Server disconnected unexpectedly");
	if(incomingPacket.connected) {
		incomingPacket.connected = false;
		TIMER_DelTickHandler(&ETHNET_ClientLoop);
		SDLNet_UDP_Close(ethnetClientSocket);
	}
}

class ETHNET : public Program {
public:
	void HelpCommand(const char *helpStr) {
		// Help on connect command
		if(strcasecmp("connect", helpStr) == 0) {
			// TODO
			return;
		}
		// Help on the disconnect command
		if(strcasecmp("disconnect", helpStr) == 0) {
			// TODO
			return;
		}
		// Help on the startserver command
		if(strcasecmp("startserver", helpStr) == 0) {
			// TODO
			return;
		}
		// Help on the stop server command
		if(strcasecmp("stopserver", helpStr) == 0) {
			// TODO
			return;
		}
		// Help on the ping command
		if(strcasecmp("ping", helpStr) == 0) {
			// TODO
			return;
		}
		// Help on the status command
		if(strcasecmp("status", helpStr) == 0) {
			// TODO
			return;
		}
	}

	void Run(void) override
	{
		WriteOut("Ethernet tunneling utility for DOSBox-X\n\n");
		if(!cmd->GetCount()) {
			WriteOut("The syntax of this command is:\n\n");
			WriteOut("ETHNET [ CONNECT | DISCONNECT | STARTSERVER | STOPSERVER | PING | HELP |\n         STATUS ]\n\n");
			return;
		}

		if(cmd->FindCommand(1, temp_line, /*remove*/true)) {
			if(strcasecmp("help", temp_line.c_str()) == 0) {
				if(!cmd->FindCommand(1, temp_line, /*remove*/true)) {
					WriteOut("The following are valid ETHNET commands:\n\n");
					WriteOut("ETHNET CONNECT        ETHNET DISCONNECT       ETHNET STARTSERVER\n");
					WriteOut("ETHNET STOPSERVER     ETHNET PING             ETHNET STATUS\n\n");
					WriteOut("To get help on a specific command, type:\n\n");
					WriteOut("ETHNET HELP command\n\n");

				} else {
					HelpCommand(temp_line.c_str());
					return;
				}
				return;
			} 
			if(strcasecmp("startserver", temp_line.c_str()) == 0) {
				if(!isEthnetServer) {
					if(incomingPacket.connected) {
						WriteOut("ETHNET Tunneling Client already connected to another server.  Disconnect first.\n");
						return;
					}
					bool startsuccess;
					if(!cmd->FindCommand(1, temp_line, /*remove*/true)) {
						udpPort = 1701;/* We're going for our own implementation of L2TP so use the same port number */
					} else {
						udpPort = (unsigned int)strtol(temp_line.c_str(), NULL, 10);
					}
					startsuccess = ETHNET_StartServer((uint16_t)udpPort);
					if(startsuccess) {
						WriteOut("ETHNET Tunneling Server started\n");
						isEthnetServer = true;
						ConnectToServer("localhost");
					} else {
						WriteOut("ETHNET Tunneling Server failed to start.\n");
						if(udpPort < 1024) WriteOut("Try a port number above 1024. See ETHNET HELP CONNECT on how to specify a port.\n");
					}
				} else {
					WriteOut("ETHNET Tunneling Server already started\n");
				}
				return;
			}
			if(strcasecmp("stopserver", temp_line.c_str()) == 0) {
				if(!isEthnetServer) {
					WriteOut("ETHNET Tunneling Server not running in this DOSBox-X session.\n");
				} else {
					isEthnetServer = false;
					DisconnectFromServer(false);
					ETHNET_StopServer();
					WriteOut("ETHNET Tunneling Server stopped.");
				}
				return;
			}
			if(strcasecmp("connect", temp_line.c_str()) == 0) {
				char strHost[1024];
				if(incomingPacket.connected) {
					WriteOut("ETHNET Tunneling Client already connected.\n");
					return;
				}
				if(!cmd->FindCommand(1, temp_line, /*remove*/true)) {
					WriteOut("ETHNET Server address not specified.\n");
					return;
				}
				strcpy(strHost, temp_line.c_str());

				if(!cmd->FindCommand(1, temp_line, /*remove*/true)) {
					udpPort = 1701;/* We're going for our own implementation of L2TP so use the same port number */
				} else {
					udpPort = (unsigned int)strtol(temp_line.c_str(), NULL, 10);
				}

				if(ConnectToServer(strHost)) {
					WriteOut("ETHNET Tunneling Client connected to server at %s.\n", strHost);
				} else {
					WriteOut("ETHNET Tunneling Client failed to connect to server at %s.\n", strHost);
				}
				return;
			}

			if(strcasecmp("disconnect", temp_line.c_str()) == 0) {
				if(!incomingPacket.connected) {
					WriteOut("ETHNET Tunneling Client not connected.\n");
					return;
				}
				// TODO: Send a packet to the server notifying of disconnect
				WriteOut("ETHNET Tunneling Client disconnected from server.\n");
				DisconnectFromServer(false);
				return;
			}

			if(strcasecmp("status", temp_line.c_str()) == 0) {
				WriteOut("ETHNET Tunneling Status:\n\n");
				WriteOut("Server status: ");
				if(isEthnetServer) WriteOut("ACTIVE\n"); else WriteOut("INACTIVE\n");
				WriteOut("Client status: ");
				if(incomingPacket.connected) {
					WriteOut("CONNECTED -- Server at %d.%d.%d.%d port %d\n", CONVIP(ethnetServConnIp.host), udpPort);
				} else {
					WriteOut("DISCONNECTED\n");
				}
				if(isEthnetServer) {
					WriteOut("List of active connections:\n\n");
					int i;
					IPaddress *ptrAddr;
					for(i=0;i<SOCKETTABLESIZE;i++) {
						if(ETHNET_isConnectedToServer(i,&ptrAddr)) {
							WriteOut("     %d.%d.%d.%d from port %d\n", CONVIP(ptrAddr->host), SDLNet_Read16(&ptrAddr->port));
						}
					}
					WriteOut("\n");
				}
				return;
			}

			if(strcasecmp("ping", temp_line.c_str()) == 0) {
				return;
			}
		}
	}
};

void ETHNET_ProgramStart(Program * * make) {
	*make=new ETHNET;
}

