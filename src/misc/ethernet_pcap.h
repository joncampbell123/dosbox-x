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

#ifndef DOSBOX_ETHERNET_PCAP_H
#define DOSBOX_ETHERNET_PCAP_H

#include "config.h"

#if C_PCAP

#include "ethernet.h"

#ifdef WIN32
#define HAVE_REMOTE
#endif

#include "support.h" /* Prevents snprintf conflict with pcap on Windows */
#include "pcap.h"

/** A PCAP-based Ethernet connection
 * This backend uses a physical Ethernet device. All types of traffic
 * such as IPX, TCP, NetBIOS work over this interface.
 * This requires libpcap or WinPcap to be installed and your selected
 * network interface to support promiscious operation.
 */
class PcapEthernetConnection : public EthernetConnection {
	public:
		PcapEthernetConnection();
		~PcapEthernetConnection();
		bool Initialize();
		void SendPacket(const uint8_t* packet, int len);
		void GetPackets(std::function<void(const uint8_t*, int)> callback);

	private:
		pcap_t* adhandle = 0; /*!< The pcap handle used for this device */
};

#endif

#endif
