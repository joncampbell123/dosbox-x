/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


// include guard
#ifndef DOSBOX_DIRECTSERIAL_WIN32_H
#define DOSBOX_DIRECTSERIAL_WIN32_H

#include "dosbox.h"

#if C_DIRECTSERIAL

#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"

#include "libserial.h"

class CDirectSerial : public CSerial {
public:
	CDirectSerial(Bitu id, CommandLine* cmd);
	~CDirectSerial();

	void updatePortConfig(Bit16u divider, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val, bool first);
	void setBreak(bool value);
	
	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(Bit16u type);

private:
	COMPORT comport;

	Bitu rx_state = 0;
#define D_RX_IDLE		0
#define D_RX_WAIT		1
#define D_RX_BLOCKED	2
#define D_RX_FASTWAIT	3

	Bitu rx_retry;		// counter of retries (every millisecond)
	Bitu rx_retry_max;	// how many POLL_EVENTS to wait before causing
						// an overrun error.
	bool doReceive();

#if SERIAL_DEBUG
	bool dbgmsg_poll_block = false;
	bool dbgmsg_rx_block = false;
#endif

};

#endif	// C_DIRECTSERIAL
#endif	// include guard
