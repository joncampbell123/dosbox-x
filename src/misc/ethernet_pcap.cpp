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

#if C_PCAP

#include "ethernet_pcap.h"
#include "dosbox.h"
#include "support.h" /* strcasecmp */
#include <cstring>

extern std::string niclist;

#ifdef WIN32

#include <windows.h>

// DLL loading
#define pcap_sendpacket(A,B,C)			PacketSendPacket(A,B,C)
#define pcap_close(A)					PacketClose(A)
#define pcap_freealldevs(A)				PacketFreealldevs(A)
#define pcap_open(A,B,C,D,E,F)			PacketOpen(A,B,C,D,E,F)
#define pcap_next_ex(A,B,C)				PacketNextEx(A,B,C)
#define pcap_findalldevs_ex(A,B,C,D)	PacketFindALlDevsEx(A,B,C,D)

int (*PacketSendPacket)(pcap_t *, const u_char *, int) = 0;
void (*PacketClose)(pcap_t *) = 0;
void (*PacketFreealldevs)(pcap_if_t *) = 0;
pcap_t* (*PacketOpen)(char const *,int,int,int,struct pcap_rmtauth *,char *) = 0;
int (*PacketNextEx)(pcap_t *, struct pcap_pkthdr **, const u_char **) = 0;
int (*PacketFindALlDevsEx)(char *, struct pcap_rmtauth *, pcap_if_t **, char *) = 0;

char pcap_src_if_string[] = PCAP_SRC_IF_STRING;

#endif

PcapEthernetConnection::PcapEthernetConnection()
      : EthernetConnection()
{
}

PcapEthernetConnection::~PcapEthernetConnection()
{
	if(adhandle) pcap_close(adhandle);
}

bool PcapEthernetConnection::Initialize()
{
	/* TODO: grab from config */
	const char* realnicstring = "list";

#ifdef WIN32
	// init the library
	HINSTANCE pcapinst;
	pcapinst = LoadLibrary("WPCAP.DLL");
	if(pcapinst==NULL) {
            niclist = "WinPcap has to be installed for the NE2000 to work.";
			LOG_MSG(niclist.c_str());
		return false;
	}
	FARPROC psp;
	
#ifdef __MINGW32__
	// C++ defines function and data pointers as separate types to reflect
	// Harvard architecture machines (like the Arduino). As such, casting
	// between them isn't portable and GCC will helpfully warn us about it.
	// We're only running this code on Windows which explicitly allows this
	// behaviour, so silence the warning to avoid confusion.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

	psp = GetProcAddress(pcapinst,"pcap_sendpacket");
	if(!PacketSendPacket) PacketSendPacket =
		(int (__cdecl *)(pcap_t *,const u_char *,int))psp;
	
	psp = GetProcAddress(pcapinst,"pcap_close");
	if(!PacketClose) PacketClose =
		(void (__cdecl *)(pcap_t *)) psp;
	
	psp = GetProcAddress(pcapinst,"pcap_freealldevs");
	if(!PacketFreealldevs) PacketFreealldevs =
		(void (__cdecl *)(pcap_if_t *)) psp;

	psp = GetProcAddress(pcapinst,"pcap_open");
	if(!PacketOpen) PacketOpen =
		(pcap_t* (__cdecl *)(char const *,int,int,int,struct pcap_rmtauth *,char *)) psp;

	psp = GetProcAddress(pcapinst,"pcap_next_ex");
	if(!PacketNextEx) PacketNextEx = 
		(int (__cdecl *)(pcap_t *, struct pcap_pkthdr **, const u_char **)) psp;

	psp = GetProcAddress(pcapinst,"pcap_findalldevs_ex");
	if(!PacketFindALlDevsEx) PacketFindALlDevsEx =
		(int (__cdecl *)(char *, struct pcap_rmtauth *, pcap_if_t **, char *)) psp;

#ifdef __MINGW32__
#pragma GCC diagnostic pop
#endif

	if(PacketFindALlDevsEx==0 || PacketNextEx==0 || PacketOpen==0 || 
		PacketFreealldevs==0 || PacketClose==0 || PacketSendPacket==0) {
            niclist = "Incorrect or non-functional WinPcap version.";
			LOG_MSG(niclist.c_str());
		pcapinst = NULL;
		return false;
	}
#endif

	// find out which pcap device to use
	pcap_if_t *alldevs;
	pcap_if_t *currentdev = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];
	unsigned int userdev;
#ifdef WIN32
	if (pcap_findalldevs_ex(pcap_src_if_string, NULL, &alldevs, errbuf) == -1)
#else
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
#endif
	{
            niclist = "Cannot enumerate network interfaces: "+std::string(errbuf);
			LOG_MSG("%s", niclist.c_str());
		return false;
	}
        {
            Bitu i = 0;
            niclist = "Network Interface List\n-------------------------------------------------------------\n";
            for(currentdev=alldevs; currentdev!=NULL; currentdev=currentdev->next) {
                const char* desc = "no description";
                if(currentdev->description) desc=currentdev->description;
                i++;
                niclist+=(i<10?"0":"")+std::to_string(i)+" "+currentdev->name+"\n    ("+desc+")\n";
            }
        }
	if (!strcasecmp(realnicstring,"list")) {
			// print list and quit
            std::istringstream in(("\n"+niclist+"\n").c_str());
            if (in)	for (std::string line; std::getline(in, line); )
                LOG_MSG("%s", line.c_str());
			pcap_freealldevs(alldevs);
			return false;
	} else if(1==sscanf(realnicstring,"%u",&userdev)) {
		// user passed us a number
		Bitu i = 0;
		currentdev=alldevs;
		while(currentdev!=NULL) {
			i++;
			if(i==userdev) break;
			else currentdev=currentdev->next;
		}
	} else {
		// user might have passed a piece of name
		for(currentdev=alldevs; currentdev!=NULL; currentdev=currentdev->next) {
			if(strstr(currentdev->name,realnicstring)) {
				break;
			}else if(currentdev->description!=NULL &&
				strstr(currentdev->description,realnicstring)) {
				break;
			}
		}
	}

	if(currentdev==NULL) {
		LOG_MSG("Unable to find network interface - check realnic parameter\n");
		pcap_freealldevs(alldevs);
		return false;
	}
	// print out which interface we are going to use
        const char* desc = "no description"; 
	if(currentdev->description) desc=currentdev->description;
	LOG_MSG("Using Network interface:\n%s\n(%s)\n",currentdev->name,desc);
	
	const char *timeoutstr = section->Get_string("pcaptimeout");
        char *end;
        int timeout = -1;
        if (!strlen(timeoutstr)||timeoutstr[0]!='-'&&!isdigit(timeoutstr[0])) { // Default timeout values
#ifdef WIN32
            timeout = -1; // For Windows, use -1 which appears to be specific to WinPCap and means "non-blocking mode"
#else
            timeout = 3000; // For other platforms, use 3000ms as the timeout which should work for platforms like macOS
#endif
        } else
            timeout = strtoul(timeoutstr,&end,10);
	// attempt to open it
#ifdef WIN32
	if ( (adhandle= pcap_open(
			currentdev->name, // name of the device
            65536,            // portion of the packet to capture
                              // 65536 = whole packet 
            PCAP_OPENFLAG_PROMISCUOUS,    // promiscuous mode
            timeout,          // read timeout
            NULL,             // authentication on the remote machine
            errbuf            // error buffer
            ) ) == NULL)
#else
	/*pcap_t *pcap_open_live(const char *device, int snaplen,
        int promisc, int to_ms, char *errbuf)*/
	if ( (adhandle= pcap_open_live(
			currentdev->name, // name of the device
            65536,            // portion of the packet to capture
                              // 65536 = whole packet 
            true,    // promiscuous mode
            timeout,          // read timeout
            errbuf            // error buffer
            ) ) == NULL)

#endif        
        {
		LOG_MSG("\nUnable to open the interface: %s.", errbuf);
        	pcap_freealldevs(alldevs);
		return false;
	}
	pcap_freealldevs(alldevs);
#ifndef WIN32
	pcap_setnonblock(adhandle,1,errbuf);
#endif
	return true;
}

void PcapEthernetConnection::SendPacket(const uint8_t* packet, int len)
{
	pcap_sendpacket(adhandle, packet, len);
}

void PcapEthernetConnection::GetPackets(std::function<void(const uint8_t*, int)> callback)
{
	int res;
	struct pcap_pkthdr *header;
	u_char *pkt_data;
//#if 0
	while((res = pcap_next_ex( adhandle, &header, (const u_char **)&pkt_data)) > 0) {
		callback(pkt_data, header->len);
	}
//#endif
}

#endif
