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
#ifndef DOSBOX_NULLMODEM_WIN32_H
#define DOSBOX_NULLMODEM_WIN32_H

#include "dosbox.h"

#if C_MODEM

#include "misc_util.h"
#include "serialport.h"

#define SERIAL_SERVER_POLLING_EVENT	SERIAL_BASE_EVENT_COUNT+1
#define SERIAL_TX_REDUCTION		SERIAL_BASE_EVENT_COUNT+2
#define SERIAL_NULLMODEM_DTR_EVENT	SERIAL_BASE_EVENT_COUNT+3
#define SERIAL_NULLMODEM_EVENT_COUNT	SERIAL_BASE_EVENT_COUNT+3

class CNullModem : public CSerial {
public:
	CNullModem(Bitu id, CommandLine* cmd);
	~CNullModem();

	void updatePortConfig(uint16_t divider, uint8_t lcr);
	void updateMSR();
	void transmitByte(uint8_t val, bool first);
	void setBreak(bool value);
	
	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(uint16_t type);

private:
	TCPServerSocket* serversocket;
	TCPClientSocket* clientsocket;

	bool receiveblock;		// It's not a block of data it rather blocks
	uint16_t serverport;		// we are a server if this is nonzero
	uint16_t clientport;

	uint8_t hostnamebuffer[128]; // the name passed to us by the user

	Bitu rx_state;
#define N_RX_IDLE		0
#define N_RX_WAIT		1
#define N_RX_BLOCKED	2
#define N_RX_FASTWAIT	3
#define N_RX_DISC		4

	bool doReceive();
	bool ClientConnect(TCPClientSocket* newsocket);
	bool ServerListen();
	bool ServerConnect();
    void Disconnect();
	Bits readChar();
	void WriteChar(uint8_t data);

	bool DTR_delta;		// with dtrrespect, we try to establish a connection
						// whenever DTR switches to 1. This variable is
						// used to remember the old state.

	bool tx_block;		// true while the SERIAL_TX_REDUCTION event
						// is pending

	Bitu rx_retry;		// counter of retries

	Bitu rx_retry_max;	// how many POLL_EVENTS to wait before causing
						// a overrun error.

	Bitu tx_gather;		// how long to gather tx data before
						// sending all of them [milliseconds]

	
	bool dtrrespect;	// dtr behavior - only send data to the serial
						// port when DTR is on

	bool transparent;	// if true, don't send 0xff 0xXX to toggle
						// DSR/CTS.

	bool telnet;		// Do Telnet parsing.

    bool nonlocal;      // Enable connections NOT originating from localhost

	// Telnet's brain
#define TEL_CLIENT 0
#define TEL_SERVER 1

	Bits TelnetEmulation(uint8_t data);

	// Telnet's memory
	struct {
		bool binary[2];
		bool echo[2];
		bool supressGA[2];
		bool timingMark[2];
					
		bool inIAC;
		bool recCommand;
		uint8_t command;
	} telClient;
};

#endif	// C_MODEM
#endif	// include guard
