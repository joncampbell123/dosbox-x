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

// include guard
#pragma once

#include "config.h"
#include "parport.h"
#include <cstddef>
#include <cstdint>
#include <string>

// A transport for the extlpt 4-byte request / 4-byte response frame (protocol
// in extlpt.cpp). Hides its own connection setup and its own way of bounding a
// blocked read/write; exposes all-or-nothing framed I/O plus a liveness flag.
class LPTTransport {
public:
	virtual ~LPTTransport() {}

	// False once a read/write has failed, or if setup never completed.
	virtual bool IsConnected() const = 0;

	// Move the whole buffer or fail. Bounded internally so a stalled peer
	// cannot hang the core; on failure, logs and marks itself disconnected.
	virtual bool WriteFull(const uint8_t *buf, size_t len) = 0;
	virtual bool ReadFull(uint8_t *buf, size_t len) = 0;
};

// Factories. portnr is 0-based, for log messages. serverMode makes DOSBox-X
// create the endpoint and wait for the peer. Return nullptr (after LOG_MSG) on
// failure. An empty path selects a platform default.
LPTTransport *CreatePipeTransport(int portnr, const std::string &path, bool serverMode);
#if C_MODEM
LPTTransport *CreateTcpTransport(int portnr, const std::string &host, uint16_t port, bool serverMode);
#endif

// Raw bit-bang LPT passthrough to an external process over an LPTTransport.
// No printer handshake, no knowledge of any device protocol - the external
// process makes sense of the bytes. See dosbox.cpp for the parallel<n>=extlpt
// options; the register routing and the "registers:"/"mask*:" filters are
// described above CExtLPT::ShouldForwardWrite() in extlpt.cpp.
class CExtLPT : public CParallel {
public:
	CExtLPT(Bitu nr, uint8_t initIrq, CommandLine* cmd);
	virtual ~CExtLPT();

	bool InstallationSuccessful;        // check after constructing. If
										 // something was wrong, delete it right away.
private:
	bool Putchar(uint8_t) override;

	Bitu Read_PR() override;
	Bitu Read_COM() override;
	Bitu Read_SR() override;

	void Write_PR(Bitu) override;
	void Write_CON(Bitu) override;
	void Write_IOSEL(Bitu) override;

	void handleUpperEvent(uint16_t type) override;

	bool Transact(uint8_t opcode, uint8_t port_offset, uint8_t value, uint8_t &outValue);
	bool ShouldForwardWrite(uint8_t &lastVal, bool &haveLast, uint8_t newVal,
	                         bool useMask, uint8_t mask);

	// OP_HELLO/version exchange, once, before any register access.
	bool DoHandshake();

	LPTTransport *transport = nullptr;

	uint8_t nextSeq;
	bool externalResponsePending = false;

	// Which registers are wired to the external device (see "registers:");
	// one not in this set never touches the transport, on read or write.
	bool forwardData = true;
	bool forwardStatus = true;
	bool forwardControl = true;

	// Optional per-register "mask*:" bit-change filters; off by default.
	bool useDataMask = false, useControlMask = false;
	uint8_t dataMask = 0xFF, controlMask = 0xFF;
	uint8_t lastDataVal = 0, lastControlVal = 0;
	bool haveLastData = false, haveLastControl = false;
};
