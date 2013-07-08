/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: softmodem.h,v 1.12 2009-10-04 20:57:40 h-a-l-9000 Exp $ */

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
	ResOK,ResERROR,
	ResCONNECT,ResRING,
	ResBUSY,ResNODIALTONE,ResNOCARRIER
};

#define TEL_CLIENT 0
#define TEL_SERVER 1

class CFifo {
public:
	CFifo(Bitu _size) {
		size=_size;
		pos=used=0;
		data=new Bit8u[size];
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
		used=pos=0;
	}

	void addb(Bit8u _val) {
		if(used>=size) {
			static Bits lcount=0;
			if (lcount<1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (addb)");
			}
			return;
		}
		//assert(used<size);
		Bitu where=pos+used;
		if (where>=size) where-=size;
		data[where]=_val;
		//LOG_MSG("+%x",_val);
		used++;
	}
	void adds(Bit8u * _str,Bitu _len) {
		if((used+_len)>size) {
			static Bits lcount=0;
			if (lcount<1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (adds len %u)",_len);
			}
			return;
		}
		
		//assert((used+_len)<=size);
		Bitu where=pos+used;
		used+=_len;
		while (_len--) {
			if (where>=size) where-=size;
			//LOG_MSG("+'%x'",*_str);
			data[where++]=*_str++;
		}
	}
	Bit8u getb(void) {
		if (!used) {
			static Bits lcount=0;
			if (lcount<1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (getb)");
			}
			return data[pos];
		}
			Bitu where=pos;
		if (++pos>=size) pos-=size;
		used--;
		//LOG_MSG("-%x",data[where]);
		return data[where];
	}
	void gets(Bit8u * _str,Bitu _len) {
		if (!used) {
			static Bits lcount=0;
			if (lcount<1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (gets len %d)",_len);
			}
			return;
		}
			//assert(used>=_len);
		used-=_len;
		while (_len--) {
			//LOG_MSG("-%x",data[pos]);
			*_str++=data[pos];
			if (++pos>=size) pos-=size;
		}
	}
private:
	Bit8u * data;
	Bitu size,pos,used;
};
#define MREG_AUTOANSWER_COUNT 0
#define MREG_RING_COUNT 1
#define MREG_ESCAPE_CHAR 2
#define MREG_CR_CHAR 3
#define MREG_LF_CHAR 4
#define MREG_BACKSPACE_CHAR 5


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
	bool Dial(char * host);
	void AcceptIncomingCall(void);
	Bitu ScanNumber(char * & scan);
	char GetChar(char * & scan);

	void DoCommand();
	
	void MC_Changed(Bitu new_mc);

	void TelnetEmulation(Bit8u * data, Bitu size);

	//TODO
	void Timer2(void);
	void handleUpperEvent(Bit16u type);

	void RXBufferEmpty();

	void transmitByte(Bit8u val, bool first);
	void updatePortConfig(Bit16u divider, Bit8u lcr);
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

	Bit8u waiting_tx_character;

	Bitu cmdpause;
	Bits ringtimer;
	Bits ringcount;
	Bitu plusinc;
	Bitu cmdpos;
	Bitu flowcontrol;

	Bit8u tmpbuf[MODEM_BUFFER_QUEUE_SIZE];

	Bitu listenport;
	Bit8u reg[SREGS];
	
	
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
		Bit8u command;
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
