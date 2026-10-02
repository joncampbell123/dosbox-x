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

#include "config.h"
#include "extlpt.h"
#include "localpipe.h"
#include "logging.h"
#include "printer_if.h"
#include <cstdlib>

#if defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#if C_MODEM
// transport:tcp reuses the serialport networking layer, hence the C_MODEM gate.
#include "timer.h"                         // GetTicks() / SDL_Delay()
#include "hardware/serialport/misc_util.h" // NETClientSocket, NET*Factory
#endif

// extlpt forwards raw LPT register accesses to a separate process that speaks
// whatever protocol the guest's device driver expects. Every access - reads
// and writes alike - is a synchronous blocking round-trip: the guest's next
// instruction does not run until the peer has answered. Writes block too
// because the guest protocol assumes electrical immediacy (write, strobe, read
// the result on the next IN); a write that returned early would let the guest
// CPU outrun a non-real-time peer and read stale state. A local pipe / AF_UNIX
// socket is the default because the guest polls these registers tightly and
// per-access latency is multiplied by the poll rate; transport:tcp is for a
// genuinely remote peer. A stalled peer is bounded by FRAME_IO_TIMEOUT_MS,
// after which the backend degrades to the printer engine / 0xFF.

// Wire protocol (see also the OP_* names below): 4-byte request, 4-byte
// response, no length prefix. byte0 opcode, byte1 port_offset (0=DATA/
// 1=STATUS/2=CONTROL), byte2 value, byte3 sequence_id (echoed; a framing
// tripwire only - the transport is a reliable ordered stream).
//   OP_WRITE -> OP_ACK       byte2 = 1 if the peer now has a response queued
//   OP_READ  -> OP_RESPONSE  byte2 = value to return to the guest
//   OP_HELLO -> OP_HELLO     first frame each way; version mismatch = refuse
//   (any)    -> OP_ERROR     byte2 = ERR_* (diagnostic only; fails the txn)
enum {
	OP_WRITE    = 0x01,
	OP_READ     = 0x02,
	OP_RESPONSE = 0x03,
	OP_ACK      = 0x04,
	OP_HELLO    = 0x05,
	OP_ERROR    = 0xFF
};

// Exchanged in OP_HELLO. Hard gate: a mismatch (or no OP_HELLO reply) refuses
// the connection. Bump in lockstep with the peer.
static const uint8_t PROTOCOL_VERSION = 1;

// OP_HELLO byte2: bitmask of optional extensions each side understands. Runs
// once per connection, so this is where the protocol can grow. None defined yet.
enum {
	CAP_NONE = 0,
};
static const uint8_t SUPPORTED_CAPS = CAP_NONE;

// port_offset is a register index, not an absolute I/O port - the base
// (378h/3BCh/278h/...) comes from the parallel<n> config.
enum {
	PORT_DATA    = 0x00, // base + 0
	PORT_STATUS  = 0x01, // base + 1
	PORT_CONTROL = 0x02  // base + 2
};

// OP_ERROR byte2. Decoded for diagnostics only - any error fails the txn.
enum {
	ERR_BAD_LENGTH     = 0, // peer got a frame that wasn't 4 bytes
	ERR_BAD_WRITE_REG  = 1, // WRITE to a read-only/invalid register
	ERR_BAD_READ_REG   = 2, // READ from an invalid register
	ERR_UNKNOWN_OPCODE = 3  // byte0 wasn't OP_WRITE or OP_READ
};

static const char *ErrorReasonName(uint8_t code) {
	switch (code) {
	case ERR_BAD_LENGTH:     return "malformed frame";
	case ERR_BAD_WRITE_REG:  return "write to an invalid/read-only register";
	case ERR_BAD_READ_REG:   return "read from an invalid register";
	case ERR_UNKNOWN_OPCODE: return "unknown opcode";
	default:                 return "unrecognized error code";
	}
}

// Max time one 4-byte exchange may block the core. On expiry the transaction
// fails and the transport disconnects (backend falls back to printer / 0xFF).
static const unsigned FRAME_IO_TIMEOUT_MS = 2000;

// Pipe / AF_UNIX transport. Connection setup is the shared LocalPipe_Open();
// only the bounded per-frame I/O policy differs per platform.

#if defined(WIN32)

namespace {
class PipeTransportWin : public LPTTransport {
public:
	PipeTransportWin(HANDLE h, int pnr) : hPipe(h), portnr(pnr) {}
	~PipeTransportWin() override {
		if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
	}

	bool IsConnected() const override { return connected; }

	// PIPE_NOWAIT: ReadFile/WriteFile never block - ERROR_NO_DATA means the
	// buffer can't progress right now. Spin with backoff until the frame
	// moves or the deadline hits.
	bool WriteFull(const uint8_t *buf, size_t len) override {
		DWORD start = GetTickCount();
		unsigned spins = 0;
		size_t done = 0;
		while (done < len) {
			DWORD n = 0;
			if (WriteFile(hPipe, buf + done, (DWORD)(len - done), &n, NULL)) {
				done += n;
				if (n != 0) { spins = 0; continue; }
			} else if (GetLastError() != ERROR_NO_DATA) {
				return fail("write");
			}
			if (!backoff(start, spins)) return fail("write timed out");
		}
		return true;
	}

	bool ReadFull(uint8_t *buf, size_t len) override {
		DWORD start = GetTickCount();
		unsigned spins = 0;
		size_t done = 0;
		while (done < len) {
			DWORD n = 0;
			if (ReadFile(hPipe, buf + done, (DWORD)(len - done), &n, NULL)) {
				done += n;
				if (n != 0) { spins = 0; continue; }
			} else if (GetLastError() != ERROR_NO_DATA) {
				return fail("read");
			}
			if (!backoff(start, spins)) return fail("read timed out");
		}
		return true;
	}

private:
	// Sleep(1) is 1-15 ms on Windows and would dominate a lockstep round-trip,
	// so spin/yield first and only fall back to a real sleep once a reply is
	// clearly not imminent. False = deadline passed.
	bool backoff(DWORD start, unsigned &spins) {
		if (GetTickCount() - start >= FRAME_IO_TIMEOUT_MS) return false;
		if (spins < 4096)        SwitchToThread();
		else                     Sleep(1);
		++spins;
		return true;
	}

	bool fail(const char *what) {
		LOG_MSG("parallel%d: Pipe %s failed (error %lu). External passthrough disconnected.",
			portnr + 1, what, (unsigned long)GetLastError());
		connected = false;
		return false;
	}

	HANDLE hPipe;
	int portnr;
	bool connected = true;
};
} // namespace

LPTTransport *CreatePipeTransport(int portnr, const std::string &path, bool serverMode) {
	std::string pipeName = path.empty() ? std::string("\\\\.\\pipe\\dosbox_lpt") : path;

	if (serverMode)
		LOG_MSG("parallel%d: Waiting for a client to connect to pipe %s ...",
			portnr + 1, pipeName.c_str());

	LocalPipeOpenResult r = LocalPipe_Open(pipeName, serverMode);
	if (r.handle == LOCALPIPE_INVALID) {
		LOG_MSG("parallel%d: extlpt pipe %s: %s.", portnr + 1, pipeName.c_str(), r.error.c_str());
		return nullptr;
	}
	HANDLE hPipe = (HANDLE)r.handle;

	// Non-blocking from here: per-frame I/O is bounded by PipeTransportWin.
	DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

	LOG_MSG("parallel%d: Connected to pipe %s for external LPT passthrough (%s mode).",
		portnr + 1, pipeName.c_str(), serverMode ? "server" : "client");
	return new PipeTransportWin(hPipe, portnr);
}

#else // !defined(WIN32) - POSIX AF_UNIX stream socket.

namespace {
class PipeTransportPosix : public LPTTransport {
public:
	PipeTransportPosix(int fd, int pnr) : sockFd(fd), portnr(pnr) {}
	~PipeTransportPosix() override {
		if (sockFd >= 0) close(sockFd);
	}

	bool IsConnected() const override { return connected; }

	// SO_SNDTIMEO/SO_RCVTIMEO bound each send()/recv(). Retry on EINTR; a
	// timeout or peer error is a hard failure.
	bool WriteFull(const uint8_t *buf, size_t len) override {
		size_t done = 0;
		while (done < len) {
			ssize_t n = send(sockFd, buf + done, len - done, 0);
			if (n > 0) { done += (size_t)n; continue; }
			if (n < 0 && errno == EINTR) continue;
			return fail("write");
		}
		return true;
	}

	bool ReadFull(uint8_t *buf, size_t len) override {
		size_t done = 0;
		while (done < len) {
			ssize_t n = recv(sockFd, buf + done, len - done, 0);
			if (n > 0) { done += (size_t)n; continue; }
			if (n < 0 && errno == EINTR) continue;
			return fail("read"); // 0 = peer closed; <0 = EAGAIN (timeout) or error
		}
		return true;
	}

private:
	bool fail(const char *what) {
		LOG_MSG("parallel%d: Socket %s failed (errno %d: %s). External passthrough disconnected.",
			portnr + 1, what, errno, strerror(errno));
		connected = false;
		return false;
	}

	int sockFd;
	int portnr;
	bool connected = true;
};
} // namespace

LPTTransport *CreatePipeTransport(int portnr, const std::string &path, bool serverMode) {
	std::string sockPath = path.empty() ? std::string("/tmp/dosbox_lpt") : path;

	if (serverMode)
		LOG_MSG("parallel%d: Waiting for a client to connect to %s ...",
			portnr + 1, sockPath.c_str());

	LocalPipeOpenResult r = LocalPipe_Open(sockPath, serverMode);
	if (r.handle == LOCALPIPE_INVALID) {
		LOG_MSG("parallel%d: extlpt socket %s: %s.", portnr + 1, sockPath.c_str(), r.error.c_str());
		return nullptr;
	}
	int sockFd = (int)r.handle;

	// Bound every blocking send()/recv() so a stalled peer can't hang the core.
	{
		struct timeval tv;
		tv.tv_sec = FRAME_IO_TIMEOUT_MS / 1000;
		tv.tv_usec = (FRAME_IO_TIMEOUT_MS % 1000) * 1000;
		setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}

	LOG_MSG("parallel%d: Connected to %s for external LPT passthrough (%s mode).",
		portnr + 1, sockPath.c_str(), serverMode ? "server" : "client");
	return new PipeTransportPosix(sockFd, portnr);
}

#endif // WIN32 / POSIX

// TCP transport - thin wrapper over the serialport NETClientSocket layer.

#if C_MODEM

namespace {
class TcpTransport : public LPTTransport {
public:
	TcpTransport(NETClientSocket *s, int pnr) : sock(s), portnr(pnr) {}
	~TcpTransport() override { delete sock; }

	bool IsConnected() const override { return sock && sock->isopen; }

	// SendArray() blocks until the frame is gone or the socket errors. The
	// underlying socket carries SO_SNDTIMEO (set in TCPClientSocket), so a
	// wedged peer fails here instead of hanging.
	bool WriteFull(const uint8_t *buf, size_t len) override {
		if (!sock || !sock->SendArray(buf, len)) return fail("send");
		return true;
	}

	// ReceiveArray() is non-blocking; poll with backoff (same reasoning as
	// PipeTransportWin::backoff) bounded by FRAME_IO_TIMEOUT_MS.
	bool ReadFull(uint8_t *buf, size_t len) override {
		if (!sock) return fail("recv");
		uint32_t start = GetTicks();
		unsigned spins = 0;
		size_t done = 0;
		while (done < len) {
			size_t got = len - done;
			if (!sock->ReceiveArray(buf + done, got)) return fail("recv");
			done += got;
			if (got != 0) { spins = 0; continue; }
			if (GetTicks() - start >= FRAME_IO_TIMEOUT_MS) return fail("recv timed out");
			if (spins < 4096) { SDL_Delay(0); ++spins; }
			else SDL_Delay(1);
		}
		return true;
	}

private:
	bool fail(const char *what) {
		LOG_MSG("parallel%d: TCP %s failed. External passthrough disconnected.",
			portnr + 1, what);
		if (sock) sock->isopen = false;
		return false;
	}

	NETClientSocket *sock;
	int portnr;
};
} // namespace

LPTTransport *CreateTcpTransport(int portnr, const std::string &host, uint16_t port, bool serverMode) {
	NETClientSocket *client = nullptr;

	if (serverMode) {
		NETServerSocket *server = NETServerSocket::NETServerFactory(SOCKET_TYPE_TCP, port);
		if (!server || !server->isopen) {
			LOG_MSG("parallel%d: Could not listen on TCP port %u.", portnr + 1, (unsigned)port);
			delete server;
			return nullptr;
		}

		LOG_MSG("parallel%d: Waiting for a client to connect on TCP port %u ...",
			portnr + 1, (unsigned)port);

		// Accept() is non-blocking; poll it for a while so the external side
		// can come up in any order, then give up rather than hang startup.
		const uint32_t acceptTimeoutMs = 30000;
		uint32_t start = GetTicks();
		while (!(client = server->Accept())) {
			if (GetTicks() - start >= acceptTimeoutMs) break;
			SDL_Delay(50);
		}
		delete server; // single client only; stop listening

		if (!client) {
			LOG_MSG("parallel%d: No client connected on TCP port %u.", portnr + 1, (unsigned)port);
			return nullptr;
		}
	} else {
		// Retry for a few seconds regardless of launch order.
		const uint32_t connectTimeoutMs = 5000;
		uint32_t start = GetTicks();
		for (;;) {
			client = NETClientSocket::NETClientFactory(SOCKET_TYPE_TCP, host.c_str(), port);
			if (client && client->isopen) break;
			delete client;
			client = nullptr;
			if (GetTicks() - start >= connectTimeoutMs) break;
			SDL_Delay(200);
		}

		if (!client) {
			LOG_MSG("parallel%d: Could not connect to %s:%u.",
				portnr + 1, host.c_str(), (unsigned)port);
			return nullptr;
		}
	}

	LOG_MSG("parallel%d: Connected to %s:%u for external LPT passthrough (%s mode).",
		portnr + 1, host.c_str(), (unsigned)port, serverMode ? "server" : "client");
	return new TcpTransport(client, portnr);
}

#endif // C_MODEM

// "registers:data,status,control" - which registers are wired to the external
// device at all. Absent = all three.
static void ParseRegisterFilter(CommandLine* cmd, bool &fwdData, bool &fwdStatus, bool &fwdControl) {
	std::string str;
	if (!cmd->FindStringBegin("registers:", str, false)) return; // default: all three stay true

	fwdData = fwdStatus = fwdControl = false;
	size_t pos = 0;
	while (pos < str.size()) {
		size_t comma = str.find(',', pos);
		std::string tok = str.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
		if (tok == "data") fwdData = true;
		else if (tok == "status") fwdStatus = true;
		else if (tok == "control") fwdControl = true;
		if (comma == std::string::npos) break;
		pos = comma + 1;
	}
}

CExtLPT::CExtLPT(Bitu nr, uint8_t initIrq, CommandLine* cmd)
	: CParallel(cmd, nr, initIrq),
	InstallationSuccessful(false), nextSeq(0)
{
	std::string transportStr = "pipe";
	cmd->FindStringBegin("transport:", transportStr, false);

	std::string path;
	cmd->FindStringBegin("pipe:", path, false); // empty -> transport picks a default

	ParseRegisterFilter(cmd, forwardData, forwardStatus, forwardControl);
	{
		std::string maskStr;
		if (cmd->FindStringBegin("maskdata:", maskStr, false)) {
			dataMask = (uint8_t)strtoul(maskStr.c_str(), NULL, 16);
			useDataMask = true;
		}
		if (cmd->FindStringBegin("maskcontrol:", maskStr, false)) {
			controlMask = (uint8_t)strtoul(maskStr.c_str(), NULL, 16);
			useControlMask = true;
		}
	}

	// Default: connect as client to an endpoint the peer created. "server"
	// flips it.
	bool serverMode = cmd->FindExist("server", false);

	if (transportStr == "pipe") {
		transport = CreatePipeTransport((int)nr, path, serverMode);
	} else if (transportStr == "tcp") {
#if C_MODEM
		std::string host = "127.0.0.1";
		cmd->FindStringBegin("host:", host, false);

		std::string portStr;
		unsigned long tcpPort = 0;
		if (cmd->FindStringBegin("port:", portStr, false))
			tcpPort = strtoul(portStr.c_str(), NULL, 10);
		if (tcpPort == 0 || tcpPort > 65535) {
			LOG_MSG("parallel%d: extlpt transport:tcp needs port:<1-65535>.", (int)nr + 1);
			return;
		}
		transport = CreateTcpTransport((int)nr, host, (uint16_t)tcpPort, serverMode);
#else
		LOG_MSG("parallel%d: extlpt transport:tcp needs a build with modem support.",
			(int)nr + 1);
		return;
#endif
	} else {
		LOG_MSG("parallel%d: Unknown extlpt transport '%s' (known: pipe, tcp).",
			(int)nr + 1, transportStr.c_str());
		return;
	}

	if (!transport) return; // the factory already logged the reason

	if (!DoHandshake()) return; // DoHandshake() logged the specific reason

	InstallationSuccessful = true;
}

// OP_HELLO exchange, once, before any register access. Kept out of Transact()
// because a version mismatch deserves its own diagnostic.
bool CExtLPT::DoHandshake() {
	uint8_t request[4] = { OP_HELLO, PROTOCOL_VERSION, SUPPORTED_CAPS, 0 };
	if (!transport->WriteFull(request, sizeof(request))) return false; // transport logged
	uint8_t response[4];
	if (!transport->ReadFull(response, sizeof(response))) return false; // transport logged

	if (response[0] != OP_HELLO) {
		LOG_MSG("parallel%d: extlpt handshake failed: peer replied with opcode=%02x instead of "
			"OP_HELLO - does it speak this protocol at all?", (int)port_nr + 1, response[0]);
		return false;
	}
	if (response[1] != PROTOCOL_VERSION) {
		LOG_MSG("parallel%d: extlpt protocol version mismatch: DOSBox-X speaks v%u, peer speaks v%u.",
			(int)port_nr + 1, (unsigned)PROTOCOL_VERSION, (unsigned)response[1]);
		return false;
	}
	// No capability bit implemented yet - just log what the peer claims.
	if (response[2] != 0)
		LOG_MSG("parallel%d: extlpt peer advertises capabilities=%02x (none in use yet).",
			(int)port_nr + 1, response[2]);
	return true;
}

CExtLPT::~CExtLPT() {
	delete transport;
	transport = nullptr;
}

bool CExtLPT::Transact(uint8_t opcode, uint8_t port_offset, uint8_t value, uint8_t &outValue) {
	outValue = 0xFF;

	if (!transport || !transport->IsConnected()) return false;

	uint8_t seq = nextSeq++;
	uint8_t request[4] = { opcode, port_offset, value, seq };

	if (!transport->WriteFull(request, sizeof(request))) return false; // transport logged
	uint8_t response[4];
	if (!transport->ReadFull(response, sizeof(response))) return false; // transport logged

	if (response[3] != seq) {
		LOG_MSG("parallel%d: extlpt sequence mismatch (got seq=%02x, expected %02x).",
			(int)port_nr + 1, response[3], seq);
		return false;
	}
	if (response[0] == OP_ERROR) {
		LOG_MSG("parallel%d: extlpt peer rejected opcode=%02x seq=%02x: %s (code %u).",
			(int)port_nr + 1, opcode, seq, ErrorReasonName(response[2]), response[2]);
		return false;
	}
	if (response[0] != OP_ACK && response[0] != OP_RESPONSE) {
		LOG_MSG("parallel%d: extlpt protocol error (unexpected opcode=%02x, seq=%02x).",
			(int)port_nr + 1, response[0], seq);
		return false;
	}

	outValue = response[2];
	return true;
}

// Register routing. No protocol knowledge lives here - the external process
// makes sense of the bytes. Default: every write goes to both the transport
// and the printer engine. Reads come from the transport once the peer has
// signalled a queued response and from the printer engine until then - so a
// status poll before any command still sees the real printer, not the
// device's idle 0x00 (which reads as offline+busy+error to a DOS app).
//
// externalResponsePending is that signal: it is device-level, not per-
// register (one flag for DATA/STATUS/CONTROL), set from the last forwarded
// write's ACK byte, and only consulted while the printer engine is active
// (a no-printer extlpt setup always reads straight from the transport).
//
// "registers:" and "mask*:" trim only the transport side, by wiring facts:
// forwardData/Status/Control = which lines exist (an excluded register goes
// straight to the printer), and ShouldForwardWrite()'s per-register mask
// skips the round-trip when no masked bit changed.
bool CExtLPT::ShouldForwardWrite(uint8_t &lastVal, bool &haveLast, uint8_t newVal,
                                  bool useMask, uint8_t mask) {
	if (!useMask) return true;
	bool changed = !haveLast || (((newVal ^ lastVal) & mask) != 0);
	lastVal = newVal;
	haveLast = true;
	return changed;
}

// Printer-engine calls below are all gated on PRINTER_isInited() ([printer]
// printer=true), same as every other caller, so extlpt works with no printer.
Bitu CExtLPT::Read_PR() {
#if C_PRINTER
	bool printerActive = PRINTER_isInited();
	if (printerActive && (!forwardData || !externalResponsePending)) return PRINTER_readdata(0, 1);
#endif
	if (!forwardData) return 0xFF; // no printer, no transport for this register: nothing connected
	uint8_t value = 0xFF;
	Transact(OP_READ, PORT_DATA, 0, value);
	return value;
}

Bitu CExtLPT::Read_COM() {
#if C_PRINTER
	bool printerActive = PRINTER_isInited();
	if (printerActive && (!forwardControl || !externalResponsePending)) return PRINTER_readcontrol(0, 1);
#endif
	if (!forwardControl) return 0xFF;
	uint8_t value = 0xFF;
	Transact(OP_READ, PORT_CONTROL, 0, value);
	return value;
}

Bitu CExtLPT::Read_SR() {
#if C_PRINTER
	bool printerActive = PRINTER_isInited();
	if (printerActive && (!forwardStatus || !externalResponsePending)) return PRINTER_readstatus(0, 1);
#endif
	if (!forwardStatus) return 0xFF;
	uint8_t value = 0xFF;
	Transact(OP_READ, PORT_STATUS, 0, value);
	return value;
}

void CExtLPT::Write_PR(Bitu val) {
	if (forwardData && ShouldForwardWrite(lastDataVal, haveLastData, (uint8_t)val,
	                                       useDataMask, dataMask)) {
		uint8_t ack;
		externalResponsePending = Transact(OP_WRITE, PORT_DATA, (uint8_t)val, ack) && ack != 0;
	}
#if C_PRINTER
	// Also latch into the printer engine so a following STROBE can still
	// print. Independent of forwardData/mask (transport side only).
	if (PRINTER_isInited()) PRINTER_writedata(0, (Bitu)val, 1);
#endif
}

void CExtLPT::Write_CON(Bitu val) {
	// CONTROL carries the STROBE bit. If a real print job shares this port,
	// the round-trip latency on the STROBE edge can corrupt output - use
	// "registers:"/"maskcontrol:" to keep CONTROL off the transport.
	if (forwardControl && ShouldForwardWrite(lastControlVal, haveLastControl, (uint8_t)val,
	                                          useControlMask, controlMask)) {
		uint8_t ack;
		externalResponsePending = Transact(OP_WRITE, PORT_CONTROL, (uint8_t)val, ack) && ack != 0;
	}
#if C_PRINTER
	if (PRINTER_isInited()) PRINTER_writecontrol(0, (Bitu)val, 1);
#endif
}

void CExtLPT::Write_IOSEL(Bitu /*val*/) {
	// Unreachable for this backend (no I/O write handler on the status port,
	// and it doesn't call CParallel::initialize()). Nothing to forward.
}

bool CExtLPT::Putchar(uint8_t val) {
	// LPTn:/PRN device redirection (COPY, PRINT, LPRINT) lands here, separate
	// from the raw register path the external device uses. Hand these to the
	// printer engine - the external process wouldn't know what to do with them.
#if C_PRINTER
	if (!PRINTER_isInited()) return true;
	PRINTER_StrobeByte(val);
#else
	(void)val;
#endif
	return true;
}

void CExtLPT::handleUpperEvent(uint16_t /*type*/) {}
