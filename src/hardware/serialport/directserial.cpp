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

/* $Id: directserial.cpp,v 1.2 2009-09-26 09:15:19 h-a-l-9000 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL

#include "serialport.h"
#include "directserial.h"
#include "misc_util.h"
#include "pic.h"

#include "libserial.h"

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */

CDirectSerial::CDirectSerial (Bitu id, CommandLine* cmd)
					:CSerial (id, cmd) {
	InstallationSuccessful = false;
	comport = 0;

	rx_retry = 0;
    rx_retry_max = 0;

	std::string tmpstring;
	if(!cmd->FindStringBegin("realport:",tmpstring,false)) return;

	LOG_MSG ("Serial%d: Opening %s", COMNUMBER, tmpstring.c_str());
	if(!SERIAL_open(tmpstring.c_str(), &comport)) {
		char errorbuffer[256];
		SERIAL_getErrorString(errorbuffer, sizeof(errorbuffer));
		LOG_MSG("Serial%d: Serial Port \"%s\" could not be opened.",
			COMNUMBER, tmpstring.c_str());
		LOG_MSG("%s",errorbuffer);
		return;
	}

#if SERIAL_DEBUG
	dbgmsg_poll_block=false;
	dbgmsg_rx_block=false;
#endif

	// rxdelay: How many milliseconds to wait before causing an
	// overflow when the application is unresponsive.
	if(getBituSubstring("rxdelay:", &rx_retry_max, cmd)) {
		if(!(rx_retry_max<=10000)) {
			rx_retry_max=0;
		}
	}

	CSerial::Init_Registers();
	InstallationSuccessful = true;
	rx_state = D_RX_IDLE;
	setEvent(SERIAL_POLLING_EVENT, 1); // millisecond receive tick
}

CDirectSerial::~CDirectSerial () {
	if(comport) SERIAL_close(comport);
	// We do not use own events so we don't have to clear them.
}

// CanReceive: true:UART part has room left
// doReceive:  true:there was really a byte to receive
// rx_retry is incremented in polling events

// in POLLING_EVENT: always add new polling event
// D_RX_IDLE + CanReceive + doReceive		-> D_RX_WAIT   , add RX_EVENT
// D_RX_IDLE + CanReceive + not doReceive	-> D_RX_IDLE
// D_RX_IDLE + not CanReceive				-> D_RX_BLOCKED, add RX_EVENT

// D_RX_BLOCKED + CanReceive + doReceive	-> D_RX_FASTWAIT, rem RX_EVENT
//											   rx_retry=0   , add RX_EVENT
// D_RX_BLOCKED + CanReceive + !doReceive	-> D_RX_IDLE,     rem RX_EVENT
//											   rx_retry=0
// D_RX_BLOCKED + !CanReceive + doReceive + retry < max	-> D_RX_BLOCKED, rx_retry++ 
// D_RX_BLOCKED + !CanReceive + doReceive + retry >=max	-> rx_retry=0 	

// to be continued...

void CDirectSerial::handleUpperEvent(Bit16u type) {
	switch(type) {
		case SERIAL_POLLING_EVENT: {
			setEvent(SERIAL_POLLING_EVENT, 1.0f);
			// update Modem input line states
			switch(rx_state) {
				case D_RX_IDLE:
					if(CanReceiveByte()) {
						if(doReceive()) {
							// a byte was received
							rx_state=D_RX_WAIT;
							setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
						} // else still idle
					} else {
#if SERIAL_DEBUG
						if(!dbgmsg_poll_block) {
							log_ser(dbg_aux,"Directserial: block on polling.");
							dbgmsg_poll_block=true;
						}
#endif
						rx_state=D_RX_BLOCKED;
						// have both delays (1ms + bytetime)
						setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
					}
					break;
				case D_RX_BLOCKED:
                    // one timeout tick
					if(!CanReceiveByte()) {
						rx_retry++;
						if(rx_retry>=rx_retry_max) {
							// it has timed out:
							rx_retry=0;
							removeEvent(SERIAL_RX_EVENT);
							if(doReceive()) {
								// read away everything
								// this will set overrun errors
								while(doReceive());
								rx_state=D_RX_WAIT;
								setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
							} else {
								// much trouble about nothing
                                rx_state=D_RX_IDLE;
							}
						} // else wait further
					} else {
						// good: we can receive again
#if SERIAL_DEBUG
						dbgmsg_poll_block=false;
						dbgmsg_rx_block=false;
#endif
						removeEvent(SERIAL_RX_EVENT);
						rx_retry=0;
						if(doReceive()) {
							rx_state=D_RX_FASTWAIT;
							setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
						} else {
							// much trouble about nothing
							rx_state=D_RX_IDLE;
						}
					}
					break;

				case D_RX_WAIT:
				case D_RX_FASTWAIT:
					break;
			}
			updateMSR();
			break;
		}
		case SERIAL_RX_EVENT: {
			switch(rx_state) {
				case D_RX_IDLE:
					LOG_MSG("internal error in directserial");
					break;

				case D_RX_BLOCKED: // try to receive
				case D_RX_WAIT:
				case D_RX_FASTWAIT:
					if(CanReceiveByte()) {
						// just works or unblocked
						rx_retry=0; // not waiting anymore
						if(doReceive()) {
							if(rx_state==D_RX_WAIT) setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
							else {
								// maybe unblocked
								rx_state=D_RX_FASTWAIT;
								setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
							}
						} else {
							// didn't receive anything
							rx_state=D_RX_IDLE;
						}
					} else {
						// blocking now or still blocked
#if SERIAL_DEBUG
						if(rx_state==D_RX_BLOCKED) {
							if(!dbgmsg_rx_block) {
                                log_ser(dbg_aux,"Directserial: rx still blocked (retry=%d)",rx_retry);
								dbgmsg_rx_block=true;
							}
						}






						else log_ser(dbg_aux,"Directserial: block on continued rx (retry=%d).",rx_retry);
#endif
						setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
						rx_state=D_RX_BLOCKED;
					}

					break;
			}
			updateMSR();
			break;
		}
		case SERIAL_TX_EVENT: {
			// Maybe echo cirquit works a bit better this way
			if(rx_state==D_RX_IDLE && CanReceiveByte()) {
				if(doReceive()) {
					// a byte was received
					rx_state=D_RX_WAIT;
					setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
				}
			}
			ByteTransmitted();
			updateMSR();
			break;
		}
		case SERIAL_THR_EVENT: {
			ByteTransmitting();
			setEvent(SERIAL_TX_EVENT,bytetime*1.1f);
			break;				   
		}
	}
}

bool CDirectSerial::doReceive() {
	int value = SERIAL_getextchar(comport);
	if(value) {
		receiveByteEx((Bit8u)(value&0xff),(Bit8u)((value&0xff00)>>8));
		return true;
	}
	return false;
}

// updatePortConfig is called when emulated app changes the serial port
// parameters baudrate, stopbits, number of databits, parity.
void CDirectSerial::updatePortConfig (Bit16u divider, Bit8u lcr) {
	Bit8u parity = 0;

	switch ((lcr & 0x38)>>3) {
	case 0x1: parity='o'; break;
	case 0x3: parity='e'; break;
	case 0x5: parity='m'; break;
	case 0x7: parity='s'; break;
	default: parity='n'; break;
	}

	Bit8u bytelength = (lcr & 0x3)+5;

	// baudrate
	Bitu baudrate;
	if(divider==0) baudrate=115200;
	else baudrate = 115200 / divider;

	// stopbits
	Bit8u stopbits;
	if (lcr & 0x4) {
		if (bytelength == 5) stopbits = SERIAL_15STOP;
		else stopbits = SERIAL_2STOP;
	} else stopbits = SERIAL_1STOP;

	if(!SERIAL_setCommParameters(comport, baudrate, parity, stopbits, bytelength)) {
#if SERIAL_DEBUG
		log_ser(dbg_aux,"Serial port settings not supported by host." );
#endif
		LOG_MSG ("Serial%d: Desired serial mode not supported (%d,%d,%c,%d)",
			COMNUMBER, baudrate,bytelength,parity,stopbits);
	} 
	CDirectSerial::setRTSDTR(getRTS(), getDTR());
}

void CDirectSerial::updateMSR () {
	int new_status = SERIAL_getmodemstatus(comport);

	setCTS(new_status&SERIAL_CTS? true:false);
	setDSR(new_status&SERIAL_DSR? true:false);
	setRI(new_status&SERIAL_RI? true:false);
	setCD(new_status&SERIAL_CD? true:false);
}

void CDirectSerial::transmitByte (Bit8u val, bool first) {
	if(!SERIAL_sendchar(comport, val))
		LOG_MSG("Serial%d: COM port error: write failed!", COMNUMBER);
	if(first) setEvent(SERIAL_THR_EVENT, bytetime/8);
	else setEvent(SERIAL_TX_EVENT, bytetime);
}


// setBreak(val) switches break on or off
void CDirectSerial::setBreak (bool value) {
	SERIAL_setBREAK(comport,value);
}

// updateModemControlLines(mcr) sets DTR and RTS. 
void CDirectSerial::setRTSDTR(bool rts, bool dtr) {
	SERIAL_setRTS(comport,rts);
	SERIAL_setDTR(comport,dtr);
}

void CDirectSerial::setRTS(bool val) {
	SERIAL_setRTS(comport,val);
}

void CDirectSerial::setDTR(bool val) {
	SERIAL_setDTR(comport,val);
}

#endif
