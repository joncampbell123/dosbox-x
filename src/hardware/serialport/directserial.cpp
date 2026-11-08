/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#include <algorithm>
#include <limits>

#include "dosbox.h"

#if C_DIRECTSERIAL

#include "logging.h"
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
	comport = nullptr;

	rx_retry = 0;
    rx_retry_max = 0;
    rx_state = 0;

	std::string tmpstring;
	if(!cmd->FindStringBegin("realport:",tmpstring,false)) return;

	LOG_MSG ("Serial%d: Opening %s", (int)(COMNUMBER), tmpstring.c_str());
	if(!SERIAL_open(tmpstring.c_str(), &comport)) {
		char errorbuffer[256];
		SERIAL_getErrorString(errorbuffer, sizeof(errorbuffer));
		LOG_MSG("Serial%d: Serial Port \"%s\" could not be opened.",
			(int)(COMNUMBER), tmpstring.c_str());
		LOG_MSG("%s",errorbuffer);
		return;
	}

	// rxdelay: How many milliseconds to wait before causing an
	// overflow when the application is unresponsive.
	bool rxdelay_set = getBituSubstring("rxdelay:", &rx_retry_max, cmd);
	if(rxdelay_set) {
		if(!(rx_retry_max<=10000)) {
			rx_retry_max=0;
		}
	}

	// rib:1 - a Motorola RIB cable, see directserial.h. ribholdoff: ms
	// (default 30), ribecho:0 to turn the local echo off, ribburst:0 to
	// send each byte at once.
	if(cmd->FindStringBegin("rib:",tmpstring,false) && tmpstring!="0") {
		rib = true;
		if(getBituSubstring("ribholdoff:", &ribHoldoff, cmd) && ribHoldoff > 1000)
			ribHoldoff = 1000;
		if(cmd->FindStringBegin("ribecho:",tmpstring,false))
			ribEcho = tmpstring!="0";
		if(cmd->FindStringBegin("ribburst:",tmpstring,false))
			ribBurst = tmpstring!="0";
		// RSS polls the UART between delay loops; an echo and a reply arriving
		// together overrun it unless received bytes are held for a while.
		if(!rxdelay_set) rx_retry_max = 50;
		// opening the port raised DTR; BUSY starts released
		SERIAL_setDTR(comport, false);
		ribPortDtr = false;
		LOG_MSG("Serial%d: RIB mode, BUSY hold-off %d ms, rxdelay %d ms%s%s",
			(int)(COMNUMBER), (int)ribHoldoff, (int)rx_retry_max,
			ribEcho ? ", local echo" : "", ribEcho && ribBurst ? ", burst" : "");
	}

	// lowlatency:1 (the default with rib:1) - ask the host driver to pass
	// received bytes and modem line changes on at once. A USB adapter
	// otherwise holds them for its latency timer (FTDI: 16 ms).
	bool lowlatency = rib;
	if(cmd->FindStringBegin("lowlatency:",tmpstring,false))
		lowlatency = tmpstring!="0";
	if(lowlatency) {
		if(SERIAL_setLowLatency(comport))
			LOG_MSG("Serial%d: low-latency mode on", (int)(COMNUMBER));
		else
			LOG_MSG("Serial%d: low-latency mode not supported by this port or host", (int)(COMNUMBER));
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

void CDirectSerial::handleUpperEvent(uint16_t type) {
/*
#if SERIAL_DEBUG
		const char* s;
		const char* s2;
		switch(type) {
		case SERIAL_POLLING_EVENT: s = "POLLING_EVENT"; break;
		case SERIAL_RX_EVENT: s = "RX_EVENT"; break;
		case SERIAL_TX_EVENT: s = "TX_EVENT"; break;
		case SERIAL_THR_EVENT: s = "THR_EVENT"; break;
		}
		switch(rx_state) {
		case D_RX_IDLE: s2 = "RX_IDLE"; break;
		case D_RX_WAIT: s2 = "RX_WAIT"; break;
		case D_RX_BLOCKED: s2 = "RX_BLOCKED"; break;
		case D_RX_FASTWAIT: s2 = "RX_FASTWAIT"; break;
		}
		log_ser(dbg_aux,"Directserial: Event enter %s, %s",s,s2);
#endif
		*/

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
			ribDeliverEcho();
			// anything shorter than a message goes out once the program pauses
			if(!ribTx.empty() && ribBusyOut() && (!ribBurst ||
				PIC_FullIndex() - ribTxLast > 2 * bytetime))
				ribFlush();
			if(rib) ribPortDtrDown();
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
			ribDeliverEcho();
			// Maybe echo circuit works a bit better this way
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
	/*
	#if SERIAL_DEBUG
		switch(type) {
		case SERIAL_POLLING_EVENT: s = "POLLING_EVENT"; break;
		case SERIAL_RX_EVENT: s = "RX_EVENT"; break;
		case SERIAL_TX_EVENT: s = "TX_EVENT"; break;
		case SERIAL_THR_EVENT: s = "THR_EVENT"; break;
		}
		switch(rx_state) {
			case D_RX_IDLE: s2 = "RX_IDLE"; break;
			case D_RX_WAIT: s2 = "RX_WAIT"; break;
			case D_RX_BLOCKED: s2 = "RX_BLOCKED"; break;
			case D_RX_FASTWAIT: s2 = "RX_FASTWAIT"; break;
		}
		log_ser(dbg_aux,"Directserial: Event exit %s, %s",s,s2);
#endif*/
}

bool CDirectSerial::doReceive() {
	int value;
	while((value = SERIAL_getextchar(comport))) {
		uint8_t data = (uint8_t)(value&0xff);
		uint8_t error = (uint8_t)((value&0xff00)>>8);
		while(!ribHwEcho.empty() && PIC_FullIndex() - ribHwEcho.front().second > RIB_HW_ECHO_TIMEOUT)
			ribHwEcho.pop_front();
		if(!ribHwEcho.empty()) {
			uint8_t want = ribHwEcho.front().first;
			ribHwEcho.pop_front();
			if(data == want && !error) {			// already echoed locally
#if SERIAL_DEBUG
				log_ser(dbg_aux,"Directserial: RIB hardware echo 0x%02x dropped", data);
#endif
				continue;
			}
#if SERIAL_DEBUG
			log_ser(dbg_aux,"Directserial: RIB received 0x%02x, expected echo 0x%02x", data, want);
#endif
			ribHwEcho.clear();						// collision: pass it on
		}
		if(rib && !ribDtr) {
			// another device talking while we don't hold the bus: the BUSY
			// the hardware shows is theirs
			ribHwSettled = true;
			ribForeignRx = PIC_FullIndex();
		}
		receiveByteEx(data, error);
		return true;
	}
	return false;
}

void CDirectSerial::ribDeliverEcho() {
	if(!ribLocalEcho.empty() && CanReceiveByte()) {
#if SERIAL_DEBUG
		log_ser(dbg_aux,"Directserial: RIB local echo 0x%02x", ribLocalEcho.front());
#endif
		receiveByte(ribLocalEcho.front());
		ribLocalEcho.pop_front();
	}
}

// updatePortConfig is called when emulated app changes the serial port
// parameters baudrate, stopbits, number of databits, parity.
void CDirectSerial::updatePortConfig(double baudrate, uint8_t lcr) {
	uint8_t parity = 0;

	switch ((lcr & 0x38)>>3) {
	case 0x1: parity='o'; break;
	case 0x3: parity='e'; break;
	case 0x5: parity='m'; break;
	case 0x7: parity='s'; break;
	default: parity='n'; break;
	}

	uint8_t bytelength = (lcr & 0x3)+5;

	if (baudrate > (std::numeric_limits<int>::max)()) {
		LOG_MSG("Serial%d: Desired serial baud rate is not supported (%g)",
			(int)COMNUMBER, baudrate);
		return;
	}
	const auto host_baudrate = static_cast<int>(baudrate);

	// stopbits
	uint8_t stopbits;
	if (lcr & 0x4) {
		if (bytelength == 5) stopbits = SERIAL_15STOP;
		else stopbits = SERIAL_2STOP;
	} else stopbits = SERIAL_1STOP;

	if(!SERIAL_setCommParameters(comport, host_baudrate, (char)parity, (char)stopbits, (char)bytelength)) {
#if SERIAL_DEBUG
		log_ser(dbg_aux,"Serial port settings not supported by host." );
#endif
		LOG_MSG ("Serial%d: Desired serial mode not supported (%d,%d,%c,%d)",
			(int)(COMNUMBER),host_baudrate,(int)bytelength,parity,(int)stopbits);
	} 
	CDirectSerial::setRTSDTR(getRTS(), getDTR());
}

void CDirectSerial::updateMSR () {
	int new_status = SERIAL_getmodemstatus(comport);

	if (rib) {
		// BUSY is wired-OR: our own DTR, or another device on the bus. Right
		// after a DTR change the hardware lines still show the old state, so
		// trust only DTR until they have settled: the line reads clear after
		// our DTR drop, another device's bytes arrive, or ribholdoff passes.
		bool hw = (new_status & (SERIAL_CTS | SERIAL_RI)) != 0;
		if (ribDtr && hw) ribHwBusy = true;
		if (!ribDtr && !ribHwSettled &&
			(!hw || PIC_FullIndex() - ribDtrChanged >= ribHoldoff))
			ribHwSettled = true;
		// bytes from another device mean the bus is busy, even before the
		// adapter reports the line
		bool busy = ribDtr || (ribHwSettled && hw) ||
			PIC_FullIndex() - ribForeignRx < 2 * bytetime;
#if SERIAL_DEBUG
		if (hw != dbgmsg_rib_hw || busy != dbgmsg_rib_busy) {
			log_ser(dbg_aux,"Directserial: RIB hardware BUSY %d, BUSY %d (DTR %d, settled %d)",
				(int)hw, (int)busy, (int)ribDtr, (int)ribHwSettled);
			dbgmsg_rib_hw = hw;
			dbgmsg_rib_busy = busy;
		}
#endif
		setCTS(busy);
		setRI(busy);
		if (!ribTx.empty() && !ribBurst && ribBusyOut()) ribFlush();
	} else {
		setCTS((new_status&SERIAL_CTS)? true:false);
		setRI((new_status&SERIAL_RI)? true:false);
	}
	setDSR((new_status&SERIAL_DSR)? true:false);
	setCD((new_status&SERIAL_CD)? true:false);
}

// The program's DTR. The port's DTR follows it, but goes down only once the
// bytes sent have left the port: with local echo the program is done with a
// message before its bytes are on the bus, and BUSY must stay up until the
// end of the message.
void CDirectSerial::ribSetDTR(bool val) {
	if (val && !ribPortDtr) {
		SERIAL_setDTR(comport, true);
		ribPortDtr = true;
	}
	if (val != ribDtr) {
		ribDtr = val;
		ribDtrChanged = PIC_FullIndex();
		ribHwSettled = false;
		ribHwBusy = false;
		if (val) {
			// bytes of a message the program gave up on never go out
			if (!ribTx.empty()) {
#if SERIAL_DEBUG
				log_ser(dbg_aux,"Directserial: RIB %d unsent bytes dropped", (int)ribTx.size());
#endif
				for (size_t i = 0; i < ribTx.size() && !ribHwEcho.empty(); i++)
					ribHwEcho.pop_back();
				ribTx.clear();
			}
			ribDrainRx();
		}
	}
	updateMSR();
	if (!ribTx.empty() && !ribBurst && ribBusyOut()) ribFlush();
	ribPortDtrDown();
}

void CDirectSerial::ribPortDtrDown() {
	if (ribPortDtr && !ribDtr && ribTx.empty() &&
		PIC_FullIndex() >= ribTxDone + bytetime) {
		SERIAL_setDTR(comport, false);
		ribPortDtr = false;
	}
}

// Our BUSY is on the bus: the hardware shows it, or it has had time to.
bool CDirectSerial::ribBusyOut() {
	return !ribDtr || ribHwBusy || PIC_FullIndex() - ribDtrChanged >= RIB_BUSY_OUT_MAX;
}

void CDirectSerial::ribDrainRx() {
	int value;
	while((value = SERIAL_getextchar(comport))) {
		uint8_t data = (uint8_t)(value & 0xff);
		// the end of our last message may still be echoing back
		if(!ribHwEcho.empty() && ribHwEcho.front().first == data && !(value & 0xff00)) {
			ribHwEcho.pop_front();
#if SERIAL_DEBUG
			log_ser(dbg_aux,"Directserial: RIB hardware echo 0x%02x dropped", data);
#endif
			continue;
		}
#if SERIAL_DEBUG
		log_ser(dbg_aux,"Directserial: RIB stale byte 0x%02x dropped", data);
#endif
	}
}

void CDirectSerial::ribFlush() {
	// when these bytes will have left the port
	ribTxDone = std::max<pic_tickindex_t>(ribTxDone, PIC_FullIndex()) +
		(pic_tickindex_t)ribTx.size() * bytetime;
	for(uint8_t b : ribTx)
		if(!SERIAL_sendchar(comport, (char)b))
			LOG_MSG("Serial%d: COM port error: write failed!", (int)COMNUMBER);
	ribTx.clear();
}

void CDirectSerial::transmitByte (uint8_t val, bool first) {
	if(rib && ribEcho) {
		ribTx.push_back(val);
		ribTxLast = PIC_FullIndex();
		if(ribBusyOut() && (!ribBurst || ribTx.size() >= RIB_MESSAGE_LEN)) ribFlush();
		ribLocalEcho.push_back(val);
		ribHwEcho.push_back(std::make_pair(val, PIC_FullIndex()));
	} else if(!SERIAL_sendchar(comport, (char)val))
		LOG_MSG("Serial%d: COM port error: write failed!", (int)COMNUMBER);
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
	if (rib) ribSetDTR(dtr);
	else SERIAL_setDTR(comport,dtr);
}

void CDirectSerial::setRTS(bool val) {
	SERIAL_setRTS(comport,val);
}

void CDirectSerial::setDTR(bool val) {
	if (rib) ribSetDTR(val);
	else SERIAL_setDTR(comport,val);
}

#endif
