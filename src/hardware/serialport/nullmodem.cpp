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


#include "dosbox.h"


#if C_MODEM

#include "control.h"
#include "logging.h"
#include "serialport.h"
#include "nullmodem.h"

extern int socknum;

CNullModem::CNullModem(Bitu id, CommandLine* cmd):CSerial (id, cmd) {
	Bitu temptcpport=23;
	memset(&telClient, 0, sizeof(telClient));
	InstallationSuccessful = false;
	serversocket = nullptr;
	clientsocket = nullptr;
	serverport = 0;
	clientport = 0;

	rx_retry = 0;
	rx_retry_max = 20;
	rx_state=N_RX_DISC;

	tx_gather = 12;
	
	dtrrespect=false;
	tx_block=false;
	receiveblock=false;
	transparent=false;
    nonlocal=false;
	telnet=false;
	
	Bitu bool_temp=0;

    // enet: Setting to 1 enables enet on the port, otherwise TCP.
	if (getBituSubstring("sock:", &bool_temp, cmd)) {
		if (bool_temp == 1) {
			socketType = SOCKET_TYPE_ENET;
		}
	}
	// usedtr: The nullmodem will
	// 1) when it is client connect to the server not immediately but
	//    as soon as a modem-aware application is started (DTR is switched on).
	// 2) only receive data when DTR is on.
	if (getBituSubstring("usedtr:", &bool_temp, cmd)) {
		if (bool_temp==1) {
			dtrrespect=true;
			transparent=true;
			DTR_delta=false; // connect immediately when DTR is already 1
		}
	}
	// transparent: don't add additional handshake control.
	if (getBituSubstring("transparent:", &bool_temp, cmd)) {
		if (bool_temp==1) transparent=true;
		else transparent=false;
	}
    // nonlocal: enable connections not originating from localhost.
    //           otherwise, connections not coming from localhost are rejected for security reasons.
    if (getBituSubstring("nonlocal:", &bool_temp, cmd)) {
        if (bool_temp==1) {
            nonlocal=true;
        }
	}
	// telnet: interpret telnet commands.
	if (getBituSubstring("telnet:", &bool_temp, cmd)) {
		if (bool_temp==1) {
			transparent=true;
			telnet=true;
		}
	}
	// rxdelay: How many milliseconds to wait before causing an
	// overflow when the application is unresponsive.
	if (getBituSubstring("rxdelay:", &rx_retry_max, cmd)) {
		if (!(rx_retry_max<=10000)) {
			rx_retry_max=50;
		}
	}
	// txdelay: How many milliseconds to wait before sending data.
	// This reduces network overhead quite a lot.
	if (getBituSubstring("txdelay:", &tx_gather, cmd)) {
		if (!(tx_gather<=500)) {
			tx_gather=12;
		}
	}
	// port is for both server and client
	if (getBituSubstring("port:", &temptcpport, cmd)) {
		if (!(temptcpport>0&&temptcpport<65536)) {
			temptcpport=23;
		}
	}
	// socket inheritance (client-alike)
	if (getBituSubstring("inhsocket:", &bool_temp, cmd)) {
#ifdef NATIVESOCKETS
		if (Netwrapper_GetCapabilities()&NETWRAPPER_TCP_NATIVESOCKET) {
			if (bool_temp==1) {
				if (socknum>-1) {
					dtrrespect=false;
					transparent=true;
					LOG_MSG("Inheritance socket handle: %d",socknum);
					if (!ClientConnect(new TCPClientSocket(socknum)))
						return;
				} else {
					LOG_MSG("Serial%d: -socket parameter missing.",(int)COMNUMBER);
					return;
				}
			}
		} else {
			LOG_MSG("Serial%d: socket inheritance not supported on this platform.",(int)COMNUMBER);
			return;
		}
#else
		LOG_MSG("Serial%d: socket inheritance not available.",(int)COMNUMBER);
#endif
	} else {
		// normal server/client
		std::string tmpstring;
		if (cmd->FindStringBegin("server:",tmpstring,false)) {
			// we are a client
			const char* hostnamechar=tmpstring.c_str();
			size_t hostlen=strlen(hostnamechar)+1;
			if (hostlen>sizeof(hostnamebuffer)) {
				hostlen=sizeof(hostnamebuffer);
				hostnamebuffer[sizeof(hostnamebuffer)-1]=0;
			}
			memcpy(hostnamebuffer,hostnamechar,hostlen);
			clientport=(uint16_t)temptcpport;
			if (dtrrespect) {
				// we connect as soon as DTR is switched on
				setEvent(SERIAL_NULLMODEM_DTR_EVENT, 50);
				LOG_MSG("Serial%d: Waiting for DTR...",(int)COMNUMBER);
			} else if (!ClientConnect(NETClientSocket::NETClientFactory(socketType, (char *)hostnamebuffer, clientport)))
				return;
		} else {
			// we are a server
			serverport = (uint16_t)temptcpport;
			if (!ServerListen()) return;
		}
	}
	CSerial::Init_Registers();
	InstallationSuccessful = true;

	setCTS(dtrrespect||transparent);
	setDSR(dtrrespect||transparent);
	setRI(false);
	setCD(!!clientsocket); // CD on if connection established
}

CNullModem::~CNullModem() {
	if (serversocket) delete serversocket;
	if (clientsocket) delete clientsocket;
	// remove events
	for(uint16_t i = SERIAL_BASE_EVENT_COUNT+1;
			i <= SERIAL_NULLMODEM_EVENT_COUNT; i++) {
		removeEvent(i);
	}
}

void CNullModem::WriteChar(uint8_t data) {
	if (clientsocket)clientsocket->SendByteBuffered(data);
	if (!tx_block) {
		//LOG_MSG("setevreduct");
		setEvent(SERIAL_TX_REDUCTION, (float)tx_gather);
		tx_block=true;
	}
}

Bits CNullModem::readChar(uint8_t &val) {
	SocketState state = clientsocket->GetcharNonBlock(val);
	if (state == SocketState::Closed)
		return -2;
	if (state != SocketState::Good)
		return -1;
	Bits rxchar = val;
	if (telnet && rxchar>=0) return TelnetEmulation((uint8_t)rxchar);
	else if (rxchar==0xff && !transparent) {// escape char
		// get the next char
		state = clientsocket->GetcharNonBlock(val);
		if (state != SocketState::Good) // 0xff 0xff -> 0xff was meant
			return -1;
		Bits rxchar = val;
		if (rxchar==0xff) return rxchar; // 0xff 0xff -> 0xff was meant
		rxchar&0x1? setCTS(true) : setCTS(false);
		rxchar&0x2? setDSR(true) : setDSR(false);
		if (rxchar&0x4) receiveByteEx(0x0,0x10);
		return -1;	// no "payload" received
	} else return rxchar;
}

bool CNullModem::ClientConnect(NETClientSocket *newsocket) {
	char peernamebuf[INET_ADDRSTRLEN];
	clientsocket = newsocket;
 
	if (!clientsocket->isopen) {
		LOG_MSG("Serial%d: Connection failed.",(int)COMNUMBER);
		delete clientsocket;
		clientsocket = nullptr;
		setCD(false);
		return false;
	}
	clientsocket->SetSendBufferSize(256);
	clientsocket->GetRemoteAddressString(peernamebuf);
	// transmit the line status
	if (!transparent) setRTSDTR(getRTS(), getDTR());
	rx_state=N_RX_IDLE;
	LOG_MSG("Serial%d: Connected to %s",(int)COMNUMBER,peernamebuf);
	setEvent(SERIAL_POLLING_EVENT, 1);
	setCD(true);
	return true;
}

bool CNullModem::ServerListen() {
	// Start the server listen port.
	serversocket = NETServerSocket::NETServerFactory(socketType, serverport);
	if (!serversocket->isopen) return false;
	LOG_MSG("Serial%d: Nullmodem server waiting for connection on %s port %d...",
		(int)COMNUMBER,socketType ? "ENet" : "TCP",serverport);
	setEvent(SERIAL_SERVER_POLLING_EVENT, 50);
	setCD(false);
	return true;
}

bool CNullModem::ServerConnect() {
	// check if a connection is available.
	clientsocket=serversocket->Accept();
	if (!clientsocket) return false;
	
	char peeripbuf[INET_ADDRSTRLEN];
	clientsocket->GetRemoteAddressString(peeripbuf);
	LOG_MSG("Serial%d: A client (%s) has connected.",(int)COMNUMBER,peeripbuf);
#if SERIAL_DEBUG
	log_ser(dbg_aux,"Nullmodem: A client (%s) has connected.", peeripbuf);
#endif

    /* FIXME: It would be nice if the SDL net library had a bind() call to bind only to a specific interface.
     *        Or maybe it does... what am I missing? */
    if (!nonlocal && strcmp((char*)peeripbuf,"127.0.0.1") != 0) {
        LOG_MSG("Serial%d: Non-localhost client (%s) dropped by nonlocal:0 policy. To accept connections from network, set nonlocal:1",(int)COMNUMBER,peeripbuf);
        delete clientsocket;
        clientsocket = NULL;
        return false;
    }

	clientsocket->SetSendBufferSize(256);
	rx_state=N_RX_IDLE;
	setEvent(SERIAL_POLLING_EVENT, 1);
	
	// we don't accept further connections
	delete serversocket;
	serversocket = nullptr;

	// transmit the line status
	setRTSDTR(getRTS(), getDTR());
	if (transparent) setCD(true);
	return true;
}

void CNullModem::Disconnect() {
	removeEvent(SERIAL_POLLING_EVENT);
	removeEvent(SERIAL_RX_EVENT);
	// it was disconnected; free the socket and restart the server socket
	LOG_MSG("Serial%d: Disconnected.",(int)COMNUMBER);
	delete clientsocket;
	clientsocket = nullptr;
	setDSR(false);
	setCTS(false);
	setCD(false);
	
	if (serverport) {
		serversocket = NETServerSocket::NETServerFactory(socketType,serverport);
		if (serversocket->isopen)
			setEvent(SERIAL_SERVER_POLLING_EVENT, 50);
		else delete serversocket;
	} else if (dtrrespect) {
		setEvent(SERIAL_NULLMODEM_DTR_EVENT,50);
		DTR_delta = getDTR(); // try to reconnect the next time DTR is set
	}
}

void CNullModem::handleUpperEvent(uint16_t type) {
	
	switch(type) {
		case SERIAL_POLLING_EVENT: {
			// periodically check if new data arrived, disconnect
			// if required. Add it back.
			setEvent(SERIAL_POLLING_EVENT, 1.0f);
			// update Modem input line states
			updateMSR();
			switch(rx_state) {
				case N_RX_IDLE:
					if (CanReceiveByte()) {
						if (doReceive()) {
							// a byte was received
							rx_state=N_RX_WAIT;
							setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
						} // else still idle
					} else {
#if SERIAL_DEBUG
						log_ser(dbg_aux,"Nullmodem: block on polling.");
#endif
						rx_state=N_RX_BLOCKED;
						// have both delays (1ms + bytetime)
						setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
					}
					break;
				case N_RX_BLOCKED:
                    // one timeout tick
					if (!CanReceiveByte()) {
						rx_retry++;
						if (rx_retry>=rx_retry_max) {
							// it has timed out:
							rx_retry=0;
							removeEvent(SERIAL_RX_EVENT);
							if (doReceive()) {
								// read away everything
								while(doReceive());
								rx_state=N_RX_WAIT;
								setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
							} else {
								// much trouble about nothing
                                rx_state=N_RX_IDLE;
#if SERIAL_DEBUG
								log_ser(dbg_aux,"Nullmodem: unblock due to no more data",rx_retry);
#endif
							}
						} // else wait further
					} else {
						// good: we can receive again
						removeEvent(SERIAL_RX_EVENT);
						rx_retry=0;
						if (doReceive()) {
							rx_state=N_RX_FASTWAIT;
							setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
						} else {
							// much trouble about nothing
							rx_state=N_RX_IDLE;
						}
					}
					break;

				case N_RX_WAIT:
				case N_RX_FASTWAIT:
					break;
			}
			break;
		}
		case SERIAL_RX_EVENT: {
			switch(rx_state) {
				case N_RX_IDLE:
					LOG_MSG("internal error in nullmodem");
					break;

				case N_RX_BLOCKED: // try to receive
				case N_RX_WAIT:
				case N_RX_FASTWAIT:
					if (CanReceiveByte()) {
						// just works or unblocked
						if (doReceive()) {
							rx_retry=0; // not waiting anymore
							if (rx_state==N_RX_WAIT) setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
							else {
								// maybe unblocked
								rx_state=N_RX_FASTWAIT;
								setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
							}
						} else {
							// didn't receive anything
							rx_retry=0;
							rx_state=N_RX_IDLE;
						}
					} else {
						// blocking now or still blocked
#if SERIAL_DEBUG
						if (rx_state==N_RX_BLOCKED)
							log_ser(dbg_aux,"Nullmodem: rx still blocked (retry=%d)",rx_retry);
						else log_ser(dbg_aux,"Nullmodem: block on continued rx (retry=%d).",rx_retry);
#endif
						setEvent(SERIAL_RX_EVENT, bytetime*0.65f);
						rx_state=N_RX_BLOCKED;
					}

					break;
			}
			break;
		}
		case SERIAL_TX_EVENT: {
			// Maybe echo circuit works a bit better this way
			if (rx_state==N_RX_IDLE && CanReceiveByte() && clientsocket) {
				if (doReceive()) {
					// a byte was received
					rx_state=N_RX_WAIT;
					setEvent(SERIAL_RX_EVENT, bytetime*0.9f);
				}
			}
			ByteTransmitted();
			break;
		}
		case SERIAL_THR_EVENT: {
			ByteTransmitting();
			// actually send it
			setEvent(SERIAL_TX_EVENT,bytetime+0.01f);
			break;				   
		}
		case SERIAL_SERVER_POLLING_EVENT: {
			// As long as nothing is connected to our server poll the
			// connection.
			if (!ServerConnect()) {
				// continue looking
				setEvent(SERIAL_SERVER_POLLING_EVENT, 50);
			}
			break;
		}
		case SERIAL_TX_REDUCTION: {
			// Flush the data in the transmitting buffer.
			if (clientsocket) clientsocket->FlushBuffer();
			tx_block=false;
			break;						  
		}
		case SERIAL_NULLMODEM_DTR_EVENT: {
			if ((!DTR_delta) && getDTR()) {
				// DTR went positive. Try to connect.
				if (ClientConnect(NETClientSocket::NETClientFactory(socketType, (char *)hostnamebuffer, clientport)))
					break; // no more DTR wait event when connected
			}
			DTR_delta = getDTR();
			setEvent(SERIAL_NULLMODEM_DTR_EVENT,50);
			break;
		}
	}
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CNullModem::updatePortConfig (uint16_t /*divider*/, uint8_t /*lcr*/) {
	
}

void CNullModem::updateMSR () {
	
}

bool CNullModem::doReceive () {
		uint8_t val;
		Bits rxchar = readChar(val);
		if (rxchar>=0) {
			receiveByteEx((uint8_t)rxchar,0);
			return true;
		}
		else if (rxchar==-2) {
			Disconnect();
		}
		return false;
}
 
void CNullModem::transmitByte (uint8_t val, bool first) {
 	// transmit it later in THR_Event
	if (first) setEvent(SERIAL_THR_EVENT, bytetime/8);
	else setEvent(SERIAL_TX_EVENT, bytetime);

	// disable 0xff escaping when transparent mode is enabled
	if (!transparent && (val==0xff)) WriteChar(0xff);
	
	WriteChar(val);
}

Bits CNullModem::TelnetEmulation(uint8_t data) {
	uint8_t response[3];
	if (telClient.inIAC) {
		if (telClient.recCommand) {
			if ((data != 0) && (data != 1) && (data != 3)) {
				LOG_MSG("Serial%d: Unrecognized telnet option %d",(int)COMNUMBER, data);
				if (telClient.command>250) {
					/* Reject anything we don't recognize */
					response[0]=0xff;
					response[1]=252;
					response[2]=data; /* We won't do crap! */
					if (clientsocket) clientsocket->SendArray(response, 3);
				}
			}
			switch(telClient.command) {
				case 251: /* Will */
					if (data == 0) telClient.binary[TEL_SERVER] = true;
					if (data == 1) telClient.echo[TEL_SERVER] = true;
					if (data == 3) telClient.supressGA[TEL_SERVER] = true;
					break;
				case 252: /* Won't */
					if (data == 0) telClient.binary[TEL_SERVER] = false;
					if (data == 1) telClient.echo[TEL_SERVER] = false;
					if (data == 3) telClient.supressGA[TEL_SERVER] = false;
					break;
				case 253: /* Do */
					if (data == 0) {
						telClient.binary[TEL_CLIENT] = true;
							response[0]=0xff;
							response[1]=251;
							response[2]=0; /* Will do binary transfer */
							if (clientsocket) clientsocket->SendArray(response, 3);
					}
					if (data == 1) {
						telClient.echo[TEL_CLIENT] = false;
							response[0]=0xff;
							response[1]=252;
							response[2]=1; /* Won't echo (too lazy) */
							if (clientsocket) clientsocket->SendArray(response, 3);
					}
					if (data == 3) {
						telClient.supressGA[TEL_CLIENT] = true;
							response[0]=0xff;
							response[1]=251;
							response[2]=3; /* Will Suppress GA */
							if (clientsocket) clientsocket->SendArray(response, 3);
					}
					break;
				case 254: /* Don't */
					if (data == 0) {
						telClient.binary[TEL_CLIENT] = false;
						response[0]=0xff;
						response[1]=252;
						response[2]=0; /* Won't do binary transfer */
						if (clientsocket) clientsocket->SendArray(response, 3);
					}
					if (data == 1) {
						telClient.echo[TEL_CLIENT] = false;
						response[0]=0xff;
						response[1]=252;
						response[2]=1; /* Won't echo (fine by me) */
						if (clientsocket) clientsocket->SendArray(response, 3);
					}
					if (data == 3) {
						telClient.supressGA[TEL_CLIENT] = true;
						response[0]=0xff;
						response[1]=251;
						response[2]=3; /* Will Suppress GA (too lazy) */
						if (clientsocket) clientsocket->SendArray(response, 3);
					}
					break;
				default:
					LOG_MSG("MODEM: Telnet client sent IAC %d", telClient.command);
					break;
			}
			telClient.inIAC = false;
			telClient.recCommand = false;
			return -1; //continue;
		} else {
			if (data==249) {
				/* Go Ahead received */
				telClient.inIAC = false;
				return -1; //continue;
			}
			telClient.command = data;
			telClient.recCommand = true;
			
			if ((telClient.binary[TEL_SERVER]) && (data == 0xff)) {
				/* Binary data with value of 255 */
				telClient.inIAC = false;
				telClient.recCommand = false;
					return 0xff;
			}
		}
	} else {
		if (data == 0xff) {
			telClient.inIAC = true;
			return -1;
		}
		return data;
	}
	return -1; // ???
}
	

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CNullModem::setBreak (bool /*value*/) {
	CNullModem::setRTSDTR(getRTS(), getDTR());
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CNullModem::setRTSDTR(bool xrts, bool xdtr) {
	if (!transparent) {
		uint8_t control[2];
		control[0]=0xff;
		control[1]=0x0;
		if (xrts) control[1]|=1;
		if (xdtr) control[1]|=2;
		if (LCR&LCR_BREAK_MASK) control[1]|=4;
		if (clientsocket) clientsocket->SendArray(control, 2);
	}
}
void CNullModem::setRTS(bool val) {
	setRTSDTR(val, getDTR());
}
void CNullModem::setDTR(bool val) {
	setRTSDTR(getRTS(), val);
}
#endif
