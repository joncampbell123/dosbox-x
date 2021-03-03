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

#include "ethernet.h"
#include "ethernet_pcap.h"
#include "ethernet_slirp.h"
#include <cstring>
#include "dosbox.h"
#include "control.h"

EthernetConnection* OpenEthernetConnection(const std::string& backend)
{
    EthernetConnection* conn = nullptr;
    Section* settings = nullptr;
#ifdef C_PCAP
    if (backend == "pcap")
    {
        conn = ((EthernetConnection*)new PcapEthernetConnection);
        settings = control->GetSection("ethernet, pcap");
    }
#endif
#ifdef C_SLIRP
    if (backend == "slirp")
    {
        conn = ((EthernetConnection*)new SlirpEthernetConnection);
        settings = control->GetSection("ethernet, pcap"); /* Dummy section for now */
    }
#endif
    if (!conn)
    {
        LOG_MSG("ETHERNET: Unknown ethernet backend: %s", backend.c_str());
        return nullptr;
    }
    assert(settings);
    if (conn->Initialize(settings))
    {
        return conn;
    }
    else
    {
        delete conn;
        return nullptr;
    }
}
