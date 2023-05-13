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

#ifndef DOSBOX_ETHERNET_NOTHING_H
#define DOSBOX_ETHERNET_NOTHING_H

#include "config.h"

#include "ethernet.h"

#ifdef WIN32
#define HAVE_REMOTE
#endif

#include "support.h" /* Prevents snprintf conflict with pcap on Windows */

/** This connection exists and it goes nowhere
 */
class NothingEthernetConnection : public EthernetConnection {
	public:
		NothingEthernetConnection();
		~NothingEthernetConnection();
		bool Initialize(Section* config);
		void SendPacket(const uint8_t* packet, int len);
		void GetPackets(std::function<void(const uint8_t*, int)> callback);
};

#endif
