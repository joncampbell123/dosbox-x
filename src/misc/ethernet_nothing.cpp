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

#include "ethernet_nothing.h"
#include "dosbox.h"
#include "logging.h"
#include "support.h" /* strcasecmp */

NothingEthernetConnection::NothingEthernetConnection()
      : EthernetConnection()
{
}

NothingEthernetConnection::~NothingEthernetConnection()
{
}

bool NothingEthernetConnection::Initialize(Section* config)
{
	(void)config;
	return true;
}

void NothingEthernetConnection::SendPacket(const uint8_t* packet, int len)
{
	(void)packet;
	(void)len;
}

void NothingEthernetConnection::GetPackets(std::function<void(const uint8_t*, int)> callback)
{
	(void)callback;
}

