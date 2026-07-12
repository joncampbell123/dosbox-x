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
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
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
static uint32_t ethnetServerTimeoutCheck = 0;
//static uint8_t recvBuffer[ETHNETBUFFERSIZE];	// Incoming packet buffer

static packetBuffer incomingPacket;

struct l2tp_client_t {
	uint32_t		their_control_connection_id = 0;
	uint32_t		my_control_connection_id = 0;
	uint32_t		router_id = 0;
	uint16_t		Ns = 0,Nr = 0;
	uint32_t		timeout = 0;
	uint32_t		hello = 0;
	uint32_t		hello_accept = 0;
	IPaddress		clientIP;
	bool			active = false;
};

#define MAX_CLIENTS		(64)
#define SOCKETTABLESIZE		MAX_CLIENTS

static l2tp_client_t l2tp_client[MAX_CLIENTS];

struct l2tp_client_t *lookup_client_by_ip(const IPaddress &ip) {
	size_t i=0;

	while (i < MAX_CLIENTS) {
		l2tp_client_t *c = &l2tp_client[i++];

		if (c->active && c->clientIP.host == ip.host && c->clientIP.port == ip.port)
			return c;
	}

	return NULL;
}

struct l2tp_client_t *new_client_by_ip(const IPaddress &ip) {
	size_t i=0;

	while (i < MAX_CLIENTS) {
		l2tp_client_t *c = &l2tp_client[i++];

		if (c->active) {
			if (c->clientIP.host == ip.host && c->clientIP.port == ip.port)
				return c;
		}
		else {
			*c = l2tp_client_t();
			c->timeout = GetTicks() + 15000;
			c->clientIP = ip;
			c->active = true;
			return c;
		}
	}

	return NULL;
}

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
	if(tableNum >= SOCKETTABLESIZE) return false;
	*ptrAddr = &l2tp_client[tableNum].clientIP;
	return l2tp_client[tableNum].active;
}

// WARNING: This is a non-standard protocol loosely based on L2TP (Layer 2 Tunneling Protocol).
//          If there is any interest in making this work with other standard L2TP ethernet tunneling implementations,
//          patches are welcome.

static uint16_t l2tp_ns = 0;
static uint16_t l2tp_nr = 0;
static uint32_t l2tp_cli_control_connection_id = 0;/*NTS: L2TPv2 defines this as 16:16 tunnel_id:session_id*/
static uint32_t l2tp_svr_control_connection_id = 0;
static uint32_t l2tp_cli_router_id = 0;/*NTS: L2TPv2 defines this as 16:16 tunnel_id:session_id*/
static uint32_t l2tp_svr_router_id = 0;
static uint32_t l2tp_cli_hello = 0;
static uint32_t l2tp_cli_timeout = 0;
static uint32_t l2tp_cli_hello_accept = 0;

static void l2tp_ctrlmsg_hdr(unsigned char* &w,unsigned char *wf,uint32_t ctrl_conn_id,uint16_t ns,uint16_t nr) {
	if ((w+12) > wf) return;

	/* [https://www.rfc-editor.org/info/rfc3931/#section-3.2.1] */
	*w++ = 0xC8;/* TLxxSxxx T=1 L=1 S=1 */
	*w++ = 3;
	*((uint16_t*)w) = 0; w+=2; /* length (set to 0 for now) */
	*((uint32_t*)w) = htobe32(ctrl_conn_id); w+=4; /* Control Connection ID */
	*((uint16_t*)w) = htobe16(ns); w+=2;
	*((uint16_t*)w) = htobe16(nr); w+=2;
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
# define AVP_CTRL_MSG_TYPE_SCCRP                    2
# define AVP_CTRL_MSG_TYPE_SCCCN                    3
# define AVP_CTRL_MSG_TYPE_StopCCN                  4
# define AVP_CTRL_MSG_TYPE_HELLO                    6
#define AVP_CTRL_FRAMING_CAPS                       3
#define AVP_CTRL_HOST_NAME                          7
#define AVP_CTRL_VENDOR_NAME                        8
#define AVP_CTRL_FRAMING_TYPE                       19
#define AVP_CTRL_ROUTER_ID                          60
#define AVP_CTRL_ASSN_CTRL_CONN_ID                  61
#define AVP_CTRL_PSW_CAP_LIST                       62
#define AVP_CTRL_ASSN_LOCAL_SESSION_ID              63
#define AVP_CTRL_ASSN_REMOTE_SESSION_ID             64

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

	struct avp_t {
		bool				M=false;
		bool				H=false;
		uint16_t			vendor_id=0;
		uint16_t			attribute_type=0;
		uint16_t			length=0;
		unsigned char*			data=NULL;
	};

	std::vector<struct avp_t>		recv_avp;//WARNING: points at buffer on recv, invalidated on resize
	std::map<uint16_t,uint16_t>		recv_avp_map;//NTS: There is no way any packet would have more than 0xFFFF AVPs!

	L2TPpacket &clear(void) {
		raw.clear();
		recv_avp.clear();
		recv_avp_map.clear();
		iscontrol=false;
		write=read=0;
		length_field=0;
		seq_field=0;
		use_connection_id=0;
		return *this;
	}
	L2TPpacket &setseq(const uint16_t ns,const uint16_t nr) {
		Ns=ns;
		Nr=nr;
		return *this;
	}
	L2TPpacket &needs(const size_t sz) {//invalidates pointers!
		if (raw.size() < sz) {
			raw.resize(sz);
			recv_avp.clear();//pointers in struct, this invalidates them!
			recv_avp_map.clear();
		}

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
			l2tp_ctrlmsg_hdr(w,writefence(),use_connection_id,Ns,Nr);
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
	L2TPpacket &fillUDPpacket(UDPpacket &udp,int channel) {
		memset(&udp,0,sizeof(udp));
		if (write) udp.len = write;
		else udp.len = raw.size();
		udp.maxlen = raw.size();
		udp.channel = channel;
		udp.data = raw.data();
		return *this;
	}

	L2TPpacket &fillUDPpacketForRecv(UDPpacket &udp,const uint32_t expect) {
		needs(expect);
		memset(&udp,0,sizeof(udp));
		udp.len = 0;
		udp.maxlen = expect;
		udp.channel = -1;
		udp.data = raw.data();
		return *this;
	}

	struct avp_t *recv_lookup_avp(const uint16_t a) {
		auto i = recv_avp_map.find(a);
		if (i != recv_avp_map.end() && i->second < recv_avp.size()) return &recv_avp[i->second];
		return NULL;
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
	uint16_t avp_message_type(void) {
		struct avp_t *avp = recv_lookup_avp(AVP_CTRL_MSG_TYPE);
		if (avp && avp->length >= 2) {
			unsigned char *r = avp->data,*rf = avp->data+avp->length;
			uint16_t mt = be16toh(*((uint16_t*)r)); r+=2;
			assert(r <= rf);
			return mt;
		}
		return 0;
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

	L2TPpacket &avp_vendor_name(const char *n) {
		const size_t len = strlen(n);
		needsmore(8+len);
		unsigned char *w = writeptr(),*ab = w,*wf = writefence();
		l2tp_avp_begin(w,wf,/*mandatory*/true,/*vendor*/0,/*attribute type*/AVP_CTRL_VENDOR_NAME);
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
	uint32_t avp_router_id(void) {
		struct avp_t *avp = recv_lookup_avp(AVP_CTRL_ROUTER_ID);
		if (avp && avp->length >= 4) {
			unsigned char *r = avp->data,*rf = avp->data+avp->length;
			uint32_t rid = be32toh(*((uint16_t*)r)); r+=4;
			assert(r <= rf);
			return rid;
		}
		return 0;
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
	uint32_t avp_assigned_control_connection_id(void) {
		struct avp_t *avp = recv_lookup_avp(AVP_CTRL_ASSN_CTRL_CONN_ID);
		if (avp && avp->length >= 4) {
			unsigned char *r = avp->data,*rf = avp->data+avp->length;
			uint32_t ccid = be32toh(*((uint16_t*)r)); r+=4;
			assert(r <= rf);
			return ccid;
		}
		return 0;
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
	uint32_t avp_framing_caps(void) {
		struct avp_t *avp = recv_lookup_avp(AVP_CTRL_FRAMING_CAPS);
		if (avp && avp->length >= 4) {
			unsigned char *r = avp->data,*rf = avp->data+avp->length;
			uint32_t fc = be32toh(*((uint32_t*)r)); r+=4;
			assert(r <= rf);
			return fc;
		}
		return 0;
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
	uint16_t avp_framing_type(void) {
		struct avp_t *avp = recv_lookup_avp(AVP_CTRL_FRAMING_CAPS);
		if (avp && avp->length >= 2) {
			unsigned char *r = avp->data,*rf = avp->data+avp->length;
			uint16_t ft = be16toh(*((uint16_t*)r)); r+=2;
			assert(r <= rf);
			return ft;
		}
		return 0;
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

					/* followed by AVPs */
					while ((r+6) <= rf) {
						/* [https://www.rfc-editor.org/info/rfc3931/#section-5.1] */
						struct avp_t a;

						unsigned char *ab = r;

						uint16_t hml = be16toh(*((uint16_t*)r)); r+=2;
						a.M = !!(hml & 0x8000u);
						a.H = !!(hml & 0x4000u);
						a.length = hml & 0x3FFu;
						if (a.length < 6) break;

						a.vendor_id = be16toh(*((uint16_t*)r)); r+=2;
						a.attribute_type = be16toh(*((uint16_t*)r)); r+=2;

						LOG_MSG("M=%u H=%u length=%u vendor_id=%u attr=%u",
							a.M,a.H,a.length,a.vendor_id,a.attribute_type);

						if ((ab+a.length) > rf) break;

						a.length -= 6;
						a.data = r;

						if (a.vendor_id == 0) {
							auto i=recv_avp_map.find(a.attribute_type);
							if (i==recv_avp_map.end()) recv_avp_map[a.attribute_type] = recv_avp.size();
						}

						recv_avp.push_back(a);
						r += a.length;
					}

					//if (r < rf) LOG_MSG("%u bytes left to parse",(unsigned int)(rf-r));
				}

				readptrupdate(r);
			}
		}

		return *this;
	}
};

static void DisconnectFromServer(bool unexpected);

static void ETHNET_ClientLoop(void) {
	uint32_t now = GetTicks();
	UDPpacket inPacket,outPacket;
	L2TPpacket pkt;
	Bits result;

	if (now >= l2tp_cli_timeout) {
		LOG_MSG("ETHNET: Client timeout");
		DisconnectFromServer(true);
		return;
	}
	else if (now >= l2tp_cli_hello) {
		l2tp_cli_hello = now + 2000 + ((int)rand() % 250);

		pkt.clear().setseq(l2tp_ns,l2tp_nr).connection_id(l2tp_svr_control_connection_id).begin_control().avp_message_type(AVP_CTRL_MSG_TYPE_HELLO);
		pkt.finishwrite().fillUDPpacket(/*&*/outPacket,UDPChannel);
		result = SDLNet_UDP_Send(ethnetClientSocket, outPacket.channel, &outPacket);

		LOG_MSG("ETHNET: Hello client to server");
	}

	pkt.clear().fillUDPpacketForRecv(/*&*/inPacket,4096);
	result = SDLNet_UDP_Recv(ethnetClientSocket, &inPacket);
	if (result) {
		pkt.didRecv(/*&*/inPacket);

		LOG_MSG("Client in: ctrl=%u ver=%u length=%u Ns=%u Nr=%u conn=%u",
			pkt.iscontrol,pkt.ver,pkt.length,pkt.Ns,pkt.Nr,pkt.connection_id());

		bool ignore = false;

		if (pkt.iscontrol) {
			L2TPpacket resp;

			if (pkt.avp_message_type() == AVP_CTRL_MSG_TYPE_HELLO) {
				if (now < l2tp_cli_hello_accept) ignore = true;/*avoid HELLO storms*/
				l2tp_cli_hello_accept = GetTicks() + 500;
			}
			else if (pkt.avp_message_type() == 0/*ACK*/) {
				ignore = true;
			}

			if (resp.write == 0 && !ignore) { /* ACK, no AVPs */
				pkt.clear().setseq(/*Ns=*/pkt.Nr,/*Nr=*/pkt.Ns+1u).connection_id(l2tp_svr_control_connection_id).begin_control().finishwrite().fillUDPpacket(/*&*/outPacket,UDPChannel);
				result = SDLNet_UDP_Send(ethnetClientSocket, outPacket.channel, &outPacket);
			}
		}

		l2tp_cli_timeout = GetTicks() + 15000;
		l2tp_cli_hello = GetTicks() + 3000;
	}
}

static void ETHNET_ServerLoop() {
	uint32_t now = GetTicks();
	UDPpacket inPacket,outPacket;
	L2TPpacket pkt;
	Bits result;

	if (now >= ethnetServerTimeoutCheck) {
		for (unsigned int ci=0;ci < SOCKETTABLESIZE;ci++) {
			struct l2tp_client_t *c = &l2tp_client[ci];
			if (c->active) {
				if (now >= c->timeout) {
					LOG_MSG("ETHNET: Server client timeout");
					c->active = false;
				}
				else if (now >= c->hello) {
					c->hello = now + 3000.0 + ((int)rand() % 250);

					// FIXME: Needs to use Ns = Last Nr from client
					pkt.clear().setseq(/*Ns=*/0,/*Nr=*/0).connection_id(c->their_control_connection_id).begin_control().avp_message_type(AVP_CTRL_MSG_TYPE_HELLO);
					pkt.finishwrite().fillUDPpacket(/*&*/outPacket,UDPChannel);
					outPacket.address = c->clientIP;
					result = SDLNet_UDP_Send(ethnetServerSocket, -1, &outPacket);

					LOG_MSG("ETHNET: Hello server to client");
				}
			}
		}
		ethnetServerTimeoutCheck = now + 100.0;
	}

	pkt.clear().fillUDPpacketForRecv(/*&*/inPacket,4096);
	result = SDLNet_UDP_Recv(ethnetServerSocket, &inPacket);
	if (result) {
		pkt.didRecv(/*&*/inPacket);

		struct l2tp_client_t *c = lookup_client_by_ip(inPacket.address);
		bool disconnect = false;
		bool ignore = false;

		LOG_MSG("Server in: ctrl=%u ver=%u length=%u Ns=%u Nr=%u conn=%u knownConnection=%u",
			pkt.iscontrol,pkt.ver,pkt.length,pkt.Ns,pkt.Nr,pkt.connection_id(),c?1:0);

		if (pkt.iscontrol) {
			L2TPpacket resp;

			if (pkt.avp_message_type() == AVP_CTRL_MSG_TYPE_SCCRQ) {
				uint32_t sfc = pkt.avp_framing_caps();
				uint32_t accid = pkt.avp_assigned_control_connection_id();

				/* create new connection only for SCCRQ */
				if (c == NULL) c = new_client_by_ip(inPacket.address);

				if ((sfc&3) && pkt.connection_id() == 0 && accid != 0 && c) {
					std::vector<uint16_t> pscl = {0x0005/*ethernet*/};

					c->their_control_connection_id = accid;
					if (c->my_control_connection_id == 0) c->my_control_connection_id = (uint32_t)rand()*rand();
					pkt.clear().setseq(/*Ns=*/pkt.Nr,/*Nr=*/pkt.Ns+1u).connection_id(c->their_control_connection_id).begin_control().avp_message_type(AVP_CTRL_MSG_TYPE_SCCRP);
					pkt.avp_assigned_control_connection_id(c->my_control_connection_id).avp_vendor_name("DOSBox-X");
					pkt.avp_pseudowire_capabilities_list(pscl).avp_framing_caps(3);/*A=1 S=1*/
					pkt.finishwrite().fillUDPpacket(/*&*/outPacket,UDPChannel);
					outPacket.address = inPacket.address;
					result = SDLNet_UDP_Send(ethnetServerSocket, -1, &outPacket);
				}
			}
			else if (pkt.avp_message_type() == AVP_CTRL_MSG_TYPE_HELLO) {
				if (now < c->hello_accept) ignore = true;/*avoid HELLO storms*/
				c->hello_accept = GetTicks() + 500;
			}
			else if (pkt.avp_message_type() == AVP_CTRL_MSG_TYPE_StopCCN) {
				/* client wishes to tear down control connection and sessions */
				LOG_MSG("Server: Client wants to disconnect");
				disconnect = true;
			}
			else if (pkt.avp_message_type() == 0/*ACK*/) {
				ignore = true;
			}

			if (resp.write == 0 && !ignore) { /* ACK, no AVPs */
				pkt.clear().setseq(/*Ns=*/pkt.Nr,/*Nr=*/pkt.Ns+1u).connection_id(c?c->their_control_connection_id:0).begin_control().finishwrite().fillUDPpacket(/*&*/outPacket,UDPChannel);
				outPacket.address = inPacket.address;
				result = SDLNet_UDP_Send(ethnetServerSocket, -1, &outPacket);
			}

			if (disconnect) {
				c->active = false;
			}
		}

		if (c) {
			c->timeout = GetTicks() + 15000;
			c->hello = GetTicks() + 4000;
		}
	}
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
	UDPpacket regPacket={0};
	if(!SDLNet_ResolveHost(&ethnetServConnIp, strAddr, (uint16_t)udpPort)) {
		// Select an anonymous UDP port
		ethnetClientSocket = SDLNet_UDP_Open(0);
		if(ethnetClientSocket) {
			std::vector<uint16_t> pscl = {0x0005/*ethernet*/};
			uint32_t ticks = GetTicks(); // start timeout from now
			L2TPpacket pkt;

			// Bind UDP port to address to channel
			UDPChannel = SDLNet_UDP_Bind(ethnetClientSocket,-1,&ethnetServConnIp);

			l2tp_ns = 0;
			l2tp_nr = 0;
			l2tp_cli_control_connection_id = (uint32_t)(rand()*rand());
			l2tp_svr_control_connection_id = 0;
			l2tp_cli_router_id = (uint32_t)(rand()*rand());
			l2tp_svr_router_id = 0;
			l2tp_cli_hello = 0;
			l2tp_cli_timeout = 0;

			/* SCCRQ [https://www.rfc-editor.org/info/rfc3931/#section-6.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-5.4.1] */
			/* [https://www.rfc-editor.org/info/rfc3931/#section-3.1] */
			/* [https://www.rfc-editor.org/info/rfc2661/] Assigned Tunnel ID because xl2tpd doesn't know the control connection ID tag */
			pkt.clear().connection_id(0).setseq(l2tp_ns,l2tp_nr).begin_control()
				.avp_message_type(AVP_CTRL_MSG_TYPE_SCCRQ)
				.avp_assigned_control_connection_id(l2tp_cli_control_connection_id)
				.avp_framing_caps(3)/*A=1 S=1*/
				.avp_router_id(l2tp_cli_router_id)
				.avp_pseudowire_capabilities_list(pscl)
				.avp_vendor_name("DOSBox-X");
			{/*Host Name*/
				uint32_t r1=(uint32_t)(rand()*rand());
				uint32_t r2=(uint32_t)(rand()*rand());
				char tmp[128];
				// FIXME: What would be more appropriate?
				snprintf(tmp,sizeof(tmp),"DOSBox-X.%u,%u",(unsigned int)r1,(unsigned int)r2);
				pkt.avp_host_name(tmp);
			}
			pkt.finishwrite().fillUDPpacket(/*&*/regPacket,UDPChannel);
			LOG_MSG("ETHNET: Starting L2TP with assigned ctrlconnid=%u",
				l2tp_cli_control_connection_id);
			l2tp_ns++;
			numsent = SDLNet_UDP_Send(ethnetClientSocket, regPacket.channel, &regPacket);
			if(!numsent) {
				LOG_MSG("ETHNET: Unable to connect to server: %s", SDLNet_GetError());
				SDLNet_UDP_Close(ethnetClientSocket);
				return false;
			}

			/* SCCRP [https://www.rfc-editor.org/info/rfc3931/#section-6.2] */
			{
				Bits result;

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
						bool ok = false;

						/* Ns should match the Nr we sent */

						pkt.didRecv(/*&*/regPacket);
						LOG_MSG("In: ctrl=%u ver=%u length=%u Ns=%u Nr=%u conn=%u NsExpect=%u",
							pkt.iscontrol,pkt.ver,pkt.length,pkt.Ns,pkt.Nr,pkt.connection_id(),l2tp_nr);

						if (pkt.iscontrol && pkt.avp_message_type() == AVP_CTRL_MSG_TYPE_SCCRP && pkt.Ns == l2tp_nr) {
							uint32_t sfc = pkt.avp_framing_caps();
							uint32_t accid = pkt.avp_assigned_control_connection_id();
							LOG_MSG("framing_caps: svr(just recv)=%u",sfc);
							LOG_MSG("assigned_control_connection_id: svr(just recv)=%u",accid);
							if (sfc && accid) {
								l2tp_svr_control_connection_id = accid;
								ok = true;
							}
						}

						if (ok) {
							l2tp_nr++;
							break;
						}
						else {
							LOG_MSG("Failed to connect to server %s", strAddr);
							SDLNet_UDP_Close(ethnetClientSocket);
							return false;
						}
					}
				}
			}

			/* SCCCN [https://www.rfc-editor.org/info/rfc3931/#section-6.3] */
			pkt.clear().setseq(l2tp_ns,l2tp_nr).connection_id(l2tp_svr_control_connection_id).setseq(l2tp_ns,l2tp_nr).begin_control()
				.avp_message_type(AVP_CTRL_MSG_TYPE_SCCCN);
			pkt.finishwrite().fillUDPpacket(/*&*/regPacket,UDPChannel);
			LOG_MSG("ETHNET: Completing L2TP with assigned ctrlconnid=%u",
				l2tp_cli_control_connection_id);
			l2tp_ns++;
			numsent = SDLNet_UDP_Send(ethnetClientSocket, regPacket.channel, &regPacket);
			if(!numsent) {
				LOG_MSG("ETHNET: Unable to connect to server: %s", SDLNet_GetError());
				SDLNet_UDP_Close(ethnetClientSocket);
				return false;
			}

			LOG_MSG("ETHNET: Connected to server.");

			l2tp_cli_hello = GetTicks() + 3000;
			l2tp_cli_timeout = GetTicks() + 15000;

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

static void ClientStopCCN(void) {
	if (incomingPacket.connected) {
		UDPpacket regPacket={0};
		L2TPpacket pkt;
		pkt.clear().setseq(l2tp_ns,l2tp_nr).connection_id(l2tp_svr_control_connection_id).setseq(l2tp_ns,l2tp_nr).begin_control()
			.avp_message_type(AVP_CTRL_MSG_TYPE_StopCCN);
		pkt.finishwrite().fillUDPpacket(/*&*/regPacket,UDPChannel);
		LOG_MSG("ETHNET: Client sending StopCCN to server");
		l2tp_ns++;
		SDLNet_UDP_Send(ethnetClientSocket, regPacket.channel, &regPacket);
	}
}

static void DisconnectFromServer(bool unexpected) {
	if(unexpected) LOG_MSG("ETHNET: Server disconnected unexpectedly");
	if(incomingPacket.connected) {
		if (!unexpected) ClientStopCCN(); // Let the server know!
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
						udpPort = 11701;
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
					udpPort = 11701;
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
