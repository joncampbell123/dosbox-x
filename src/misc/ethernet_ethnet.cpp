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

#if defined(C_SDL_NET) || defined(C_SDL2_NET)
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

static uint16_t l2tp_ns = 0;
static uint16_t l2tp_nr = 0;
static uint32_t l2tp_cli_control_connection_id = 0;/*NTS: L2TPv2 defines this as 16:16 tunnel_id:session_id*/
static uint32_t l2tp_svr_control_connection_id = 0;
static uint32_t l2tp_router_id = 0;

static void l2tp_ctrlmsg_hdr(unsigned char* &w,unsigned char *wf,uint32_t ctrl_conn_id) {
	if ((w+12) > wf) return;

	/* [https://www.rfc-editor.org/info/rfc3931/#section-3.2.1] */
	*w++ = 0xC8;/* TLxxSxxx T=1 L=1 S=1 */
	*w++ = 2; /* xxxxVVVV L2TP version -- FIXME: xl2tpd rejects version 3 becuase it thinks ver==3 means PPTP(??) */
	*((uint16_t*)w) = 0; w+=2; /* length (set to 0 for now) */
	*((uint32_t*)w) = htobe32(ctrl_conn_id); w+=4; /* Control Connection ID */
	*((uint16_t*)w) = htobe16(l2tp_ns); w+=2;
	*((uint16_t*)w) = htobe16(l2tp_nr); w+=2;
}

static void l2tp_ctrlmsg_hdr_update(unsigned char *base,unsigned char* &w,unsigned char *wf) {
	(void)wf;

	size_t sz = (w-base);
	*((uint16_t*)(base+2)) = htobe16((uint16_t)sz);
}

static void l2tp_avp_begin(unsigned char* &w,unsigned char *wf,bool mandatory,uint16_t vendor_id,uint16_t attribute_type) {
	if ((w+6) > wf) return;

	/* [https://www.rfc-editor.org/info/rfc3931/#section-5.1] */
	*((uint16_t*)w) = htobe16((mandatory?0x8000u:0u)); w+=2; /* MHrrrrLLLLLLLLLL M=mandatory H=0 */
	*((uint16_t*)w) = htobe16(vendor_id); w+=2;
	*((uint16_t*)w) = htobe16(attribute_type); w+=2;
	/* attribute value follows, so start writing! */
}

static void l2tp_avp_end(unsigned char *base,unsigned char* &w,unsigned char *wf) {
	size_t sz = (w-base);
	uint16_t t = be16toh(*((uint16_t*)base));
	*((uint16_t*)base) = htobe16((t & ~0x3FFu) | (sz & 0x3FFu)); /* length is low 10 bits */
}

#define AVP_CTRL_MSG_TYPE                           0
# define AVP_CTRL_MSG_TYPE_SCCRQ                    1
# define AVP_CTRL_HOST_NAME                         7
# define AVP_CTRL_ROUTER_ID                         60
# define AVP_CTRL_ASSN_CTRL_CONN_ID                 61
# define AVP_CTRL_PSW_CAP_LIST                      62

static bool ConnectToServer(char const *strAddr) {
	int numsent;
	UDPpacket regPacket={0};
	if(!SDLNet_ResolveHost(&ethnetServConnIp, strAddr, (uint16_t)udpPort)) {
		// Select an anonymous UDP port
		ethnetClientSocket = SDLNet_UDP_Open(0);
		if(ethnetClientSocket) {
			unsigned char tmp[2048];
			unsigned char *w,*wf=tmp+sizeof(tmp);

			// Bind UDP port to address to channel
			UDPChannel = SDLNet_UDP_Bind(ethnetClientSocket,-1,&ethnetServConnIp);

			regPacket.maxlen = sizeof(tmp);
			regPacket.channel = UDPChannel;

			l2tp_ns = 0;
			l2tp_nr = 0;
			l2tp_cli_control_connection_id = 0;
			l2tp_svr_control_connection_id = 0;
			l2tp_router_id = (uint32_t)(rand()*rand());

			/* SCCRQ [https://www.rfc-editor.org/info/rfc3931/#section-6.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-5.4.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-3.1] */
			w=tmp; l2tp_ctrlmsg_hdr(w,wf,l2tp_cli_control_connection_id);
			{/*Message Type*/
				unsigned char *ab = w;
				l2tp_avp_begin(w,wf,/*mandatory*/true,/*vendor*/0,/*attribute type*/AVP_CTRL_MSG_TYPE);
				*((uint16_t*)w) = htobe16(AVP_CTRL_MSG_TYPE_SCCRQ);w+=2;
				l2tp_avp_end(ab,w,wf);
			}
			{/*Host Name*/
				uint32_t r1=(uint32_t)(rand()*rand());
				uint32_t r2=(uint32_t)(rand()*rand());
				unsigned char *ab = w;
				l2tp_avp_begin(w,wf,/*mandatory*/true,/*vendor*/0,/*attribute type*/AVP_CTRL_HOST_NAME);
				// FIXME: What would be more appropriate?
				w += snprintf((char*)w,size_t(wf-w),"DOSBox-X.%u,%u",(unsigned int)r1,(unsigned int)r2);
				l2tp_avp_end(ab,w,wf);
			}
			{/*Router ID*/
				unsigned char *ab = w;
				l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_ROUTER_ID);//FIXME: xl2tpd doesn't like this send as mandatory??
				*((uint32_t*)w) = htobe32(l2tp_router_id);w+=4;
				l2tp_avp_end(ab,w,wf);
			}
			{/*Assigned Control Connection ID*/
				unsigned char *ab = w;
				l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_ASSN_CTRL_CONN_ID);//FIXME: xl2tpd doesn't like this send as mandatory??
				*((uint32_t*)w) = htobe32(l2tp_cli_control_connection_id);w+=4;
				l2tp_avp_end(ab,w,wf);
			}
			{/*Pseudowire Capabilities List*/
				unsigned char *ab = w;
				l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_PSW_CAP_LIST);//FIXME: xl2tpd doesn't like this send as mandatory??
				*((uint16_t*)w) = htobe16(0x0005/*ethernet*/);w+=2;
				l2tp_avp_end(ab,w,wf);
			}
			l2tp_ctrlmsg_hdr_update(tmp,w,wf);
			regPacket.len = size_t(w-tmp);
			regPacket.data = tmp;
			LOG_MSG("ETHNET: Using Ns=%u Nr=%u router_id=%u our_control_conn_id=%u (%u,%u)",
				l2tp_ns,l2tp_nr,l2tp_router_id,l2tp_cli_control_connection_id,
				l2tp_cli_control_connection_id>>16,l2tp_cli_control_connection_id&0xFFFFu);
			l2tp_ns++;
			numsent = SDLNet_UDP_Send(ethnetClientSocket, regPacket.channel, &regPacket);
			if(!numsent) {
				LOG_MSG("ETHNET: Unable to connect to server: %s", SDLNet_GetError());
				SDLNet_UDP_Close(ethnetClientSocket);
				return false;
			}

			{
				Bits result;
				uint32_t ticks;
				ticks = GetTicks();

				while(true) {
					uint32_t elapsed = GetTicks() - ticks;
					if(elapsed > 5000) {
						LOG_MSG("Timeout connecting to server at %s", strAddr);
						SDLNet_UDP_Close(ethnetClientSocket);
						return false;
					}
					CALLBACK_Idle();
					result = SDLNet_UDP_Recv(ethnetClientSocket, &regPacket);
					if (result != 0) {
						LOG_MSG("YAY, RESPONSE");
						break;
					}
				}
			}

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

#endif
