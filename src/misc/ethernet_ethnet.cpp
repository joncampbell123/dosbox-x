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

#include <assert.h>

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
	(void)wf;

	size_t sz = (w-base);
	uint16_t t = be16toh(*((uint16_t*)base));
	*((uint16_t*)base) = htobe16((t & ~0x3FFu) | (sz & 0x3FFu)); /* length is low 10 bits */
}

#define AVP_CTRL_MSG_TYPE                           0
# define AVP_CTRL_MSG_TYPE_SCCRQ                    1
# define AVP_CTRL_FRAMING_CAPS                      3
# define AVP_CTRL_HOST_NAME                         7
# define AVP_CTRL_ASSN_TUNNEL_ID                    9
# define AVP_CTRL_FRAMING_TYPE                      19
# define AVP_CTRL_ROUTER_ID                         60
# define AVP_CTRL_ASSN_CTRL_CONN_ID                 61
# define AVP_CTRL_PSW_CAP_LIST                      62

struct L2TPpacket {
	std::vector<unsigned char>		raw; /* this is a way for the data to persist if desired */
	bool					iscontrol=false; /*T*/
	bool					didrecv=false;
	size_t					read=0;
	size_t					write=0;
	size_t					length_field=0; /*length field offset or 0 if none */
	size_t					seq_field=0; /* Nr/Ns field offset or 0 if none */
	uint8_t					ver=0;
	uint16_t				Ns=0,Nr=0;
	uint16_t				length=0;
	uint32_t				use_connection_id=0;

	L2TPpacket &clear(void) {
		raw.clear();
		iscontrol=false;
		write=read=0;
		length_field=0;
		seq_field=0;
		use_connection_id=0;
		return *this;
	}
	L2TPpacket &needs(const size_t sz) {//invalidates pointers!
		if (raw.size() < sz)
			raw.resize(sz);

		return *this;
	}
	L2TPpacket &needsmore(const size_t sz) {//invalidates pointers!
		return needs(sz+write);
	}
	unsigned char *writeptr(void) {
		return raw.data()+write;
	}
	unsigned char *writefence(void) {
		return raw.data()+raw.size();
	}
	void writeptrupdate(unsigned char *w) {
		assert(w <= writefence());
		write = size_t(w - raw.data());
	}
	size_t canwrite(void) {
		return raw.size()-write;
	}

	unsigned char *readptr(void) {
		return raw.data()+read;
	}
	unsigned char *readfence(void) {
		return raw.data()+raw.size();
	}
	void readptrupdate(unsigned char *w) {
		assert(w <= readfence());
		read = size_t(w - raw.data());
	}
	size_t canread(void) {
		return raw.size()-read;
	}

	L2TPpacket &connection_id(const uint32_t cid) {
		use_connection_id = cid;
		return *this;
	}
	uint32_t connection_id(void) {
		return use_connection_id;
	}
	L2TPpacket &begin_control(void) {
		if (write == 0) {
			/* [https://www.rfc-editor.org/info/rfc3931/#section-5.4.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-3.1] */
			iscontrol=true;
			needsmore(12);
			unsigned char *w = writeptr();
			l2tp_ctrlmsg_hdr(w,writefence(),use_connection_id);
			writeptrupdate(w);
		}

		return *this;
	}
	L2TPpacket &finishwrite(void) {
		if (write) {
			if (iscontrol) {
				unsigned char *w = writeptr();
				l2tp_ctrlmsg_hdr_update(raw.data(),w,writefence());
			}
		}

		return *this;
	}
	L2TPpacket &fillUDPpacket(UDPpacket &udp) {
		memset(&udp,0,sizeof(udp));
		if (write) udp.len = write;
		else udp.len = raw.size();
		udp.maxlen = raw.size();
		udp.channel = UDPChannel;
		udp.data = raw.data();
		return *this;
	}

	L2TPpacket &fillUDPpacketForRecv(UDPpacket &udp,const uint32_t expect) {
		needs(expect);
		memset(&udp,0,sizeof(udp));
		udp.len = 0;
		udp.maxlen = expect;
		udp.channel = UDPChannel;
		udp.data = raw.data();
		return *this;
	}

	L2TPpacket &avp_message_type(const uint16_t mt) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/true,/*vendor*/0,/*attribute type*/AVP_CTRL_MSG_TYPE);
		*((uint16_t*)w) = htobe16(mt);w+=2;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_host_name(const char *n) {
		const size_t len = strlen(n);
		needsmore(8+len);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/true,/*vendor*/0,/*attribute type*/AVP_CTRL_HOST_NAME);
		if (len) { memcpy(w,n,len); w += len; }
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_router_id(const uint32_t rid) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_ROUTER_ID);//FIXME: xl2tpd doesn't like this send as mandatory??
		*((uint32_t*)w) = htobe32(rid);w+=4;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_assigned_control_connection_id(const uint32_t ccid) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_ASSN_CTRL_CONN_ID);//FIXME: xl2tpd doesn't like this send as mandatory??
		*((uint32_t*)w) = htobe32(ccid);w+=4;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_assigned_tunnel_id(const uint16_t tid) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_ASSN_TUNNEL_ID);//FIXME: xl2tpd doesn't like this send as mandatory??
		*((uint16_t*)w) = htobe16(tid);w+=2;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_framing_caps(const uint32_t fc) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_FRAMING_CAPS);
		*((uint32_t*)w) = htobe32(fc);w+=4;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_framing_type(const uint16_t ft) {
		needsmore(32);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_FRAMING_TYPE);//FIXME: xl2tpd doesn't like this send as mandatory??
		*((uint16_t*)w) = htobe16(ft);w+=2;
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &avp_pseudowire_capabilities_list(const std::vector<uint16_t> &v) {
		needsmore(8+(v.size()*2u));
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/false,/*vendor*/0,/*attribute type*/AVP_CTRL_PSW_CAP_LIST);//FIXME: xl2tpd doesn't like this send as mandatory??
		for (auto i=v.begin();i!=v.end();i++) { *((uint16_t*)w) = htobe16(*i);w+=2; }
		l2tp_avp_end(ab,w,wf);
		writeptrupdate(w);
		return *this;
	}
	L2TPpacket &didRecv(UDPpacket &udp) {
		if (udp.len >= 0 && (size_t)udp.len <= raw.size() && udp.data == raw.data()) {
			raw.resize((size_t)udp.len);
			didrecv = true;
			read = 0;

			if (udp.len >= 2) {
				unsigned char *r = readptr(),*rf = readfence();
				uint16_t h = be16toh(*((uint16_t*)r)); r+=2;
				ver = (h & 0xFu);
				if ((h&0xC800u) == 0xC800u && udp.len >= 12) { /*If T=1, L=1, S=1*/
					iscontrol = true; 
					length_field = size_t(r-raw.data());
					length = be16toh(*((uint16_t*)r)); r+=2;
					use_connection_id = be32toh(*((uint32_t*)r)); r+=4;
					seq_field = size_t(r-raw.data());
					Ns = be16toh(*((uint16_t*)r)); r+=2;
					Nr = be16toh(*((uint16_t*)r)); r+=2;
				}
				
				readptrupdate(r);
			}
		}

		return *this;
	}
};

static bool ConnectToServer(char const *strAddr) {
	int numsent;
	UDPpacket regPacket={0};
	if(!SDLNet_ResolveHost(&ethnetServConnIp, strAddr, (uint16_t)udpPort)) {
		// Select an anonymous UDP port
		ethnetClientSocket = SDLNet_UDP_Open(0);
		if(ethnetClientSocket) {
			std::vector<uint16_t> pscl = {0x0005/*ethernet*/};
			L2TPpacket pkt;

			// Bind UDP port to address to channel
			UDPChannel = SDLNet_UDP_Bind(ethnetClientSocket,-1,&ethnetServConnIp);

			l2tp_ns = 0;
			l2tp_nr = 0;
			l2tp_cli_control_connection_id = (uint32_t)(rand()*rand());
			l2tp_svr_control_connection_id = 0;
			l2tp_router_id = (uint32_t)(rand()*rand());

			/* SCCRQ [https://www.rfc-editor.org/info/rfc3931/#section-6.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-5.4.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-3.1] */
			/* [https://www.rfc-editor.org/info/rfc2661/] Assigned Tunnel ID because xl2tpd doesn't know the control connection ID tag */
			pkt.clear().connection_id(0).begin_control()
				.avp_message_type(AVP_CTRL_MSG_TYPE_SCCRQ)
				.avp_router_id(l2tp_router_id)
				.avp_assigned_control_connection_id(l2tp_cli_control_connection_id)
				.avp_assigned_tunnel_id(l2tp_cli_control_connection_id>>16)/*required by x2ltpd*/
				.avp_framing_caps(0)/*A=1 S=1 required by x2ltpd*/
				.avp_pseudowire_capabilities_list(pscl);
			{/*Host Name*/
				uint32_t r1=(uint32_t)(rand()*rand());
				uint32_t r2=(uint32_t)(rand()*rand());
				char tmp[128];
				// FIXME: What would be more appropriate?
				snprintf(tmp,sizeof(tmp),"DOSBox-X.%u,%u",(unsigned int)r1,(unsigned int)r2);
				pkt.avp_host_name(tmp);
			}
			pkt.finishwrite().fillUDPpacket(/*&*/regPacket);
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
					pkt.clear().fillUDPpacketForRecv(/*&*/regPacket,4096);
					result = SDLNet_UDP_Recv(ethnetClientSocket, &regPacket);
					if (result != 0) {
						pkt.didRecv(/*&*/regPacket);
						LOG_MSG("In: ctrl=%u ver=%u length=%u Ns=%u Nr=%u conn=%u(%u,%u)",
							pkt.iscontrol,pkt.ver,pkt.length,pkt.Ns,pkt.Nr,pkt.connection_id(),
							pkt.connection_id()>>16,pkt.connection_id()&0xFFFFu);
						if (pkt.iscontrol) {
							LOG_MSG("YAY, RESPONSE (result %u) len %u",(unsigned int)result,(unsigned int)regPacket.len);
							break;
						}
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
