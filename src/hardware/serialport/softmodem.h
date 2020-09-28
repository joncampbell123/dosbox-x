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


#ifndef DOSBOX_SERIALMODEM_H
#define DOSBOX_SERIALMODEM_H

#include "dosbox.h"
#if C_MODEM
#include "serialport.h"

#include "misc_util.h"

#define MODEMSPD 57600
#define SREGS 100

//If it's too high you overflow terminal clients buffers i think
#define MODEM_BUFFER_QUEUE_SIZE 1024

#define MODEM_DEFAULT_PORT 23

#define MODEM_TX_EVENT SERIAL_BASE_EVENT_COUNT + 1
#define MODEM_RX_POLLING SERIAL_BASE_EVENT_COUNT + 2
#define MODEM_RING_EVENT SERIAL_BASE_EVENT_COUNT + 3
#define SERIAL_MODEM_EVENT_COUNT SERIAL_BASE_EVENT_COUNT+3


enum ResTypes {
	ResNONE,
	ResOK,
	ResERROR,
	ResCONNECT,
	ResRING,
	ResBUSY,
	ResNODIALTONE,
	ResNOCARRIER,
	ResNOANSWER
};

#define TEL_CLIENT 0
#define TEL_SERVER 1

bool MODEM_ReadPhonebook(const std::string &filename);
void MODEM_ClearPhonebook();

class CFifo {
public:
	CFifo(Bitu _size) {
		size = _size;
		pos = used = 0;
		data = new uint8_t[size];
	}
	~CFifo() {
		delete[] data;
	}
	INLINE Bitu left(void) {
		return size-used;
	}
	INLINE Bitu inuse(void) {
		return used;
	}
	void clear(void) {
		used = pos = 0;
	}

	void addb(uint8_t _val) {
		if(used>=size) {
			static Bits lcount=0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (addb)");
			}
			return;
		}
		//assert(used < size);
		Bitu where = pos + used;
		if (where >= size) where -= size;
		data[where] = _val;
		//LOG_MSG("+%x", _val);
		used++;
	}
	void adds(uint8_t * _str, Bitu _len) {
		if((used+_len) > size) {
			static Bits lcount=0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (adds len %u)",
					static_cast<uint32_t>(_len));
			}
			return;
		}
		
		//assert((used+_len)<=size);
		Bitu where=pos+used;
		used+=_len;
		while (_len--) {
			if (where >= size) where -= size;
			//LOG_MSG("+'%x'", *_str);
			data[where++] = *_str++;
		}
	}
	uint8_t getb(void) {
		if (!used) {
			static Bits lcount=0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (getb)");
			}
			return data[pos];
		}
			Bitu where=pos;
		if (++pos >= size) pos -= size;
		used--;
		//LOG_MSG("-%x", data[where]);
		return data[where];
	}
	void gets(uint8_t * _str,Bitu _len) {
		if (!used) {
			static Bits lcount=0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (gets len %u)",
					static_cast<uint32_t>(_len));
			}
			return;
		}
			//assert(used>=_len);
		used-=_len;
		while (_len--) {
			//LOG_MSG("-%x", data[pos]);
			*_str++ = data[pos];
			if (++pos>=size) pos-=size;
		}
	}
private:
	uint8_t *data;
	Bitu size, pos, used;
};
#define MREG_AUTOANSWER_COUNT 0
#define MREG_RING_COUNT       1
#define MREG_ESCAPE_CHAR      2
#define MREG_CR_CHAR          3
#define MREG_LF_CHAR          4
#define MREG_BACKSPACE_CHAR   5
#define MREG_GUARD_TIME       12
#define MREG_DTR_DELAY        25

class CSerialModem : public CSerial {
public:

	CFifo *rqueue;
	CFifo *tqueue;

	CSerialModem(Bitu id, CommandLine* cmd);
	~CSerialModem();

	void Reset();

	void SendLine(const char *line);
	void SendRes(ResTypes response);
	void SendNumber(Bitu val);

	void EnterIdleState();
	void EnterConnectedState();

	void openConnection(void);
	bool Dial(const char *host);
	void AcceptIncomingCall(void);
	Bitu ScanNumber(char *&scan);
	char GetChar(char *&scan);

	void DoCommand();
	
	void MC_Changed(Bitu new_mc);

	void TelnetEmulation(uint8_t * data, Bitu size);

	//TODO
	void Timer2(void);
	void handleUpperEvent(Bit16u type);

	void RXBufferEmpty();

	void transmitByte(uint8_t val, bool first);
	void updatePortConfig(Bit16u divider, uint8_t lcr);
	void updateMSR();

	void setBreak(bool);

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

protected:
	char cmdbuf[255];
	bool commandmode;		// true: interpret input as commands
	bool echo;				// local echo on or off

	bool oldDTRstate;
	bool ringing;
	//bool response;
	bool numericresponse;	// true: send control response as number.
							// false: send text (i.e. NO DIALTONE)
	bool telnetmode;		// true: process IAC commands.
	
	bool connected;
	Bitu doresponse;

	uint8_t waiting_tx_character;

	Bitu cmdpause;
	Bits ringtimer;
	Bits ringcount;
	Bitu plusinc;
	Bitu cmdpos;
	Bitu flowcontrol;
	Bitu dtrmode;
	Bits dtrofftimer;
	uint8_t tmpbuf[MODEM_BUFFER_QUEUE_SIZE];

	Bitu listenport;
	uint8_t reg[SREGS];
	
	
	TCPServerSocket* serversocket;
	TCPClientSocket* clientsocket;
	TCPClientSocket* waitingclientsocket;

	struct {
		bool binary[2];
		bool echo[2];
		bool supressGA[2];
		bool timingMark[2];
					
		bool inIAC;
		bool recCommand;
		uint8_t command;
	} telClient;
	struct {
		bool active;
		double f1, f2;
		Bitu len,pos;
		char str[256];
	} dial;
};
#endif
#endif 
