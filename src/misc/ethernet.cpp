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

#include <assert.h>

#include "ethernet.h"
#include "ethernet_pcap.h"
#include "ethernet_slirp.h"
#include <cstring>
#include "dosbox.h"
#include "control.h"

EthernetConnection* OpenEthernetConnection(std::string backendstr)
{
    EthernetConnection* conn = nullptr;
    Section* settings = nullptr;
    std::string backend = "none";
    if (backendstr == "auto")
    {
#if defined(C_SLIRP)
        backend = "slirp";
#elif defined(C_PCAP)
        backend = "pcap";
#endif
    } else
        backend = backendstr;
#ifdef C_SLIRP
    if (backend == "slirp")
    {
        conn = ((EthernetConnection*)new SlirpEthernetConnection);
        settings = control->GetSection("ethernet, slirp");
    }
#endif
    if (backendstr == "auto" && !conn) backend = "pcap";
#ifdef C_PCAP
    if (backend == "pcap")
    {
        conn = ((EthernetConnection*)new PcapEthernetConnection);
        settings = control->GetSection("ethernet, pcap");
    }
#endif
    if (backendstr == "auto" && !conn) backend = "none";
    if (!conn)
    {
        if (backend == "pcap" || backend == "slirp")
            LOG_MSG("ETHERNET: Backend not supported in this build: %s", backend.c_str());
        else if (backend == "none")
            LOG_MSG("ETHERNET: No backend available for NE2000 Ethernet emulation.");
        else
            LOG_MSG("ETHERNET: Unknown ethernet backend: %s", backend.c_str());
        return nullptr;
    } else
        LOG_MSG("ETHERNET: NE2000 Ethernet emulation backend selected: %s", backend.c_str());
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
