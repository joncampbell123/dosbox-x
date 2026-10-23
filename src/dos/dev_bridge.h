// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include "callback.h"
#include "dos_inc.h"
#include "logging.h"

#include <string>
#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

bool Agent86_ExecuteControl(const std::string &operation,
	                        const std::string &payload,
	                        std::string &response);

#if defined(_WIN32) || defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET bridge_socket_t;
static const bridge_socket_t InvalidBridgeSocket = INVALID_SOCKET;
static const int BridgeSocketError = SOCKET_ERROR;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int bridge_socket_t;
static const bridge_socket_t InvalidBridgeSocket = -1;
static const int BridgeSocketError = -1;
#endif

class device_BRIDGE : public DOS_Device {
public:
	explicit device_BRIDGE(const uint16_t port, std::string token)
	        : listen_port(port),
	          authentication_token(std::move(token)),
	          listen_socket(InvalidBridgeSocket),
	          connection_socket(InvalidBridgeSocket),
	          control_listen_socket(InvalidBridgeSocket),
	          control_socket(InvalidBridgeSocket),
	          session_active(false),
	          connection_authenticated(false)
#if defined(_WIN32) || defined(WIN32)
	          , winsock_started(false)
#endif
	{
		SetName("BRIDGE");
	}
	device_BRIDGE(const device_BRIDGE &) = delete;
	device_BRIDGE &operator=(const device_BRIDGE &) = delete;

	~device_BRIDGE() override
	{
		Deactivate();
#if defined(_WIN32) || defined(WIN32)
		if (winsock_started)
			WSACleanup();
#endif
	}

	bool Activate()
	{
		if (authentication_token.empty() || authentication_token.size() > 256 ||
		    authentication_token.find_first_of("\r\n") != std::string::npos) {
			LOG_MSG("DOS bridge activation refused: bridge_token must be 1 to 256 characters without line breaks");
			return false;
		}
		try {
			session_id = NewSessionId();
		} catch (...) {
			LOG_MSG("DOS bridge activation refused: unable to create a secure session identifier");
			return false;
		}
		session_active = true;
		if (EnsureListening() && EnsureControlListening())
			return true;
		Deactivate();
		return false;
	}

	bool EnsureListening()
	{
		if (IsValidSocket(listen_socket))
			return true;
		if (listen_port == 0 || !StartSocketRuntime())
			return false;

		listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
		if (!IsValidSocket(listen_socket))
			return false;

		if (!ConfigureListener() || !BindListener() ||
		    ::listen(listen_socket, 1) == BridgeSocketError ||
		    !SetNonBlocking(listen_socket)) {
			CloseListener();
			return false;
		}
		LOG_MSG("DOS bridge listening on 127.0.0.1:%u (CTTY BRIDGE)",
		        static_cast<unsigned>(listen_port));
		return true;
	}

	void Deactivate()
	{
		const bool was_active = session_active;
		session_active = false;
		if (was_active)
			LOG_MSG("DOS bridge session ended");
		FlushInputLine();
		Disconnect();
		CloseListener();
		DisconnectControl();
		CloseSocket(control_listen_socket);
	}

	void PollControl()
	{
		if (!session_active || !EnsureControlListening()) return;
		AcceptControlConnection();
		if (!IsValidSocket(control_socket)) return;
		std::array<char, 4096> buffer = {};
		const int count = ::recv(control_socket,
		                         buffer.data(),
		                         static_cast<int>(buffer.size()),
		                         0);
		if (count == 0 || (count < 0 && !WouldBlock())) {
			DisconnectControl();
			return;
		}
		if (count < 0) return;
		control_buffer.append(buffer.data(), static_cast<size_t>(count));
		if (control_buffer.size() > MaxControlLine) {
			SendControlResult(false, "control request exceeds 131072 bytes");
			DisconnectControl();
			return;
		}
		ProcessControlLines();
	}

	bool Read(uint8_t *data, uint16_t *size) override
	{
		if (!session_active || !EnsureListening()) {
			*size = 0;
			return false;
		}

		const uint16_t requested = *size;
		uint16_t received = 0;
		while (received < requested) {
			PollControl();
			if (!session_active || !EnsureListening()) {
				*size = received;
				return false;
			}
			AcceptConnection();
			if (IsValidSocket(connection_socket)) {
				if (!connection_authenticated) {
					if (!AuthenticateConnection())
						continue;
				}
				const int count = ::recv(connection_socket,
				                         reinterpret_cast<char *>(data) + received,
				                         requested - received,
				                         0);
				if (count > 0) {
					LogAgentInput(data + received, static_cast<uint16_t>(count));
					received += static_cast<uint16_t>(count);
					break;
				}
				if (count == 0 || !WouldBlock()) {
					Deactivate();
					*size = received;
					return false;
				}
			}
			if (received == 0)
				CALLBACK_Idle();
		}
		*size = received;
		return true;
	}

	bool Write(const uint8_t *data, uint16_t *size) override
	{
		if (!session_active || !EnsureListening()) {
			*size = 0;
			return false;
		}

		AcceptConnection();
		if (!IsValidSocket(connection_socket) || !connection_authenticated) {
			*size = 0;
			return true;
		}

		uint16_t sent = 0;
		uint16_t wait_count = 0;
		while (sent < *size) {
			const int count = SendBytes(data + sent, *size - sent);
			if (count > 0) {
				sent += static_cast<uint16_t>(count);
				wait_count = 0;
				continue;
			}
			if (count == 0 || !WouldBlock() || ++wait_count > MaxWriteWaits) {
				Deactivate();
				*size = sent;
				return false;
			}
			CALLBACK_Idle();
		}
		return true;
	}

	bool Seek(uint32_t *position, uint32_t) override
	{
		*position = 0;
		return true;
	}
	bool Close() override { return true; }
	uint16_t GetInformation() override { return 0x80D3; }
	bool ReadFromControlChannel(PhysPt, uint16_t, uint16_t *) override
	{
		return false;
	}
	bool WriteToControlChannel(PhysPt, uint16_t, uint16_t *) override
	{
		return false;
	}

private:
	static constexpr uint16_t MaxWriteWaits = 1024;
	static constexpr size_t MaxLoggedInputLine = 4096;
	static constexpr size_t MaxAuthenticationLine = 512;
	static constexpr size_t MaxControlLine = 128 * 1024;

	static std::string NewSessionId()
	{
		std::random_device random;
		std::array<unsigned char, 16> bytes = {};
		for (auto &byte : bytes)
			byte = static_cast<unsigned char>(random());
		std::ostringstream output;
		output << std::hex << std::setfill('0');
		for (const auto byte : bytes)
			output << std::setw(2) << static_cast<unsigned>(byte);
		return output.str();
	}

	static bool ConstantTimeEqual(const std::string &left,
	                              const std::string &right)
	{
		const size_t maximum = left.size() > right.size() ? left.size()
		                                                   : right.size();
		unsigned difference = static_cast<unsigned>(left.size() ^ right.size());
		for (size_t i = 0; i < maximum; i++) {
			const unsigned l = i < left.size() ?
			                           static_cast<unsigned char>(left[i]) : 0;
			const unsigned r = i < right.size() ?
			                           static_cast<unsigned char>(right[i]) : 0;
			difference |= l ^ r;
		}
		return difference == 0;
	}

	bool AuthenticateConnection()
	{
		std::array<char, 256> buffer = {};
		const int count = ::recv(connection_socket,
		                         buffer.data(),
		                         static_cast<int>(buffer.size()),
		                         0);
		if (count == 0 || (count < 0 && !WouldBlock())) {
			Disconnect();
			return false;
		}
		if (count < 0)
			return false;
		authentication_buffer.append(buffer.data(), static_cast<size_t>(count));
		if (authentication_buffer.size() > MaxAuthenticationLine) {
			LOG_MSG("DOS bridge rejected an oversized authentication message");
			Disconnect();
			return false;
		}
		const size_t newline = authentication_buffer.find('\n');
		if (newline == std::string::npos)
			return false;
		std::string line = authentication_buffer.substr(0, newline);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		const std::string prefix = "AGENT86/1 AUTH ";
		const std::string supplied = line.compare(0, prefix.size(), prefix) == 0
		                                   ? line.substr(prefix.size())
		                                   : std::string();
		if (!ConstantTimeEqual(supplied, authentication_token)) {
			const std::string response = "AGENT86/1 DENIED\n";
			SendHandshake(response);
			LOG_MSG("DOS bridge rejected an unauthenticated client");
			Disconnect();
			return false;
		}
		if (newline + 1 != authentication_buffer.size()) {
			LOG_MSG("DOS bridge rejected data sent before authentication completed");
			Disconnect();
			return false;
		}
		const std::string response = "AGENT86/1 OK " + session_id + "\n";
		if (!SendHandshake(response)) {
			Disconnect();
			return false;
		}
		authentication_buffer.clear();
		connection_authenticated = true;
		LOG_MSG("DOS bridge client authenticated for session %s",
		        session_id.c_str());
		return true;
	}

	bool SendHandshake(const std::string &response)
	{
		size_t sent = 0;
		uint16_t waits = 0;
		while (sent < response.size()) {
			const int count = SendBytes(
			        reinterpret_cast<const uint8_t *>(response.data() + sent),
			        static_cast<uint16_t>(response.size() - sent));
			if (count > 0) {
				sent += static_cast<size_t>(count);
				continue;
			}
			if (count == 0 || !WouldBlock() || ++waits > MaxWriteWaits)
				return false;
			CALLBACK_Idle();
		}
		return true;
	}

	void LogAgentInput(const uint8_t *data, const uint16_t length)
	{
		for (uint16_t i = 0; i < length; i++) {
			const uint8_t ch = data[i];
			if (ch == '\r' || ch == '\n' ||
			    input_line_buffer.size() >= MaxLoggedInputLine) {
				FlushInputLine();
				continue;
			}
			input_line_buffer.push_back(static_cast<char>(ch));
		}
	}

	void FlushInputLine()
	{
		if (input_line_buffer.empty())
			return;
		LOG_MSG("DOS bridge in: %s", input_line_buffer.c_str());
		input_line_buffer.clear();
	}

	void Disconnect()
	{
		CloseSocket(connection_socket);
		connection_authenticated = false;
		authentication_buffer.clear();
	}

	static unsigned HexValue(const char value)
	{
		if (value >= '0' && value <= '9') return value - '0';
		if (value >= 'a' && value <= 'f') return value - 'a' + 10;
		if (value >= 'A' && value <= 'F') return value - 'A' + 10;
		return 16;
	}

	static bool HexDecode(const std::string &encoded, std::string &decoded)
	{
		if (encoded.size() % 2 != 0) return false;
		decoded.clear();
		decoded.reserve(encoded.size() / 2);
		for (size_t index = 0; index < encoded.size(); index += 2) {
			const unsigned high = HexValue(encoded[index]);
			const unsigned low = HexValue(encoded[index + 1]);
			if (high > 15 || low > 15) return false;
			decoded.push_back(static_cast<char>((high << 4) | low));
		}
		return true;
	}

	static std::string HexEncode(const std::string &value)
	{
		static constexpr char Digits[] = "0123456789abcdef";
		std::string encoded;
		encoded.reserve(value.size() * 2);
		for (const unsigned char byte : value) {
			encoded.push_back(Digits[byte >> 4]);
			encoded.push_back(Digits[byte & 0x0f]);
		}
		return encoded;
	}

	void ProcessControlLines()
	{
		const size_t newline = control_buffer.find('\n');
		if (newline == std::string::npos) return;
		std::string line = control_buffer.substr(0, newline);
		control_buffer.erase(0, newline + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!control_authenticated) {
			AuthenticateControl(line);
			return;
		}
		const std::string prefix = "AGENT86/1 CONTROL ";
		if (line.compare(0, prefix.size(), prefix) != 0) {
			SendControlResult(false, "invalid control request");
			DisconnectControl();
			return;
		}
		const size_t separator = line.find(' ', prefix.size());
		if (separator == std::string::npos) {
			SendControlResult(false, "control payload is missing");
			DisconnectControl();
			return;
		}
		const std::string operation = line.substr(prefix.size(), separator - prefix.size());
		std::string payload;
		std::string response;
		if (!HexDecode(line.substr(separator + 1), payload))
			SendControlResult(false, "control payload is not valid hexadecimal data");
		else
			SendControlResult(Agent86_ExecuteControl(operation, payload, response), response);
		DisconnectControl();
	}

	void AuthenticateControl(const std::string &line)
	{
		const std::string prefix = "AGENT86/1 CONTROL-AUTH ";
		const size_t separator = line.find(' ', prefix.size());
		std::string supplied_token;
		const std::string supplied_session =
		        line.compare(0, prefix.size(), prefix) == 0 && separator != std::string::npos
		                ? line.substr(prefix.size(), separator - prefix.size())
		                : std::string();
		const bool token_valid = separator != std::string::npos &&
		                         HexDecode(line.substr(separator + 1), supplied_token) &&
		                         ConstantTimeEqual(supplied_token, authentication_token);
		if (!token_valid || !ConstantTimeEqual(supplied_session, session_id)) {
			SendControlText("AGENT86/1 DENIED\n");
			DisconnectControl();
			return;
		}
		control_authenticated = true;
		SendControlText("AGENT86/1 OK " + session_id + "\n");
	}

	void SendControlResult(const bool success, const std::string &response)
	{
		SendControlText(std::string("AGENT86/1 RESULT ") +
		                (success ? "OK " : "ERROR ") + HexEncode(response) + "\n");
	}

	void SendControlText(const std::string &text)
	{
		if (!IsValidSocket(control_socket)) return;
		::send(control_socket, text.data(), static_cast<int>(text.size()), 0);
	}

	bool EnsureControlListening()
	{
		if (IsValidSocket(control_listen_socket)) return true;
		if (listen_port == 0 || listen_port == 65535 || !StartSocketRuntime()) return false;
		control_listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
		if (!IsValidSocket(control_listen_socket)) return false;
		int enabled = 1;
#if defined(_WIN32) || defined(WIN32)
		if (setsockopt(control_listen_socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
		               reinterpret_cast<const char *>(&enabled), sizeof(enabled)) == BridgeSocketError)
#else
		if (setsockopt(control_listen_socket, SOL_SOCKET, SO_REUSEADDR,
		               &enabled, sizeof(enabled)) == BridgeSocketError)
#endif
			return false;
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<uint16_t>(listen_port + 1));
		if (bind(control_listen_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == BridgeSocketError ||
		    listen(control_listen_socket, 1) == BridgeSocketError || !SetNonBlocking(control_listen_socket)) {
			CloseSocket(control_listen_socket);
			return false;
		}
		return true;
	}

	void AcceptControlConnection()
	{
		if (IsValidSocket(control_socket)) return;
		bridge_socket_t candidate = ::accept(control_listen_socket, nullptr, nullptr);
		if (!IsValidSocket(candidate)) return;
		if (!SetNonBlocking(candidate)) { CloseSocket(candidate); return; }
		control_socket = candidate;
		control_authenticated = false;
		control_buffer.clear();
	}

	void DisconnectControl()
	{
		CloseSocket(control_socket);
		control_authenticated = false;
		control_buffer.clear();
	}

	static bool IsValidSocket(const bridge_socket_t socket)
	{
		return socket != InvalidBridgeSocket;
	}

	static void CloseSocket(bridge_socket_t &socket)
	{
		if (!IsValidSocket(socket))
			return;
#if defined(_WIN32) || defined(WIN32)
		closesocket(socket);
#else
		close(socket);
#endif
		socket = InvalidBridgeSocket;
	}

	bool StartSocketRuntime()
	{
#if defined(_WIN32) || defined(WIN32)
		if (winsock_started)
			return true;
		WSADATA data = {};
		winsock_started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
		return winsock_started;
#else
		return true;
#endif
	}

	bool ConfigureListener()
	{
		int enabled = 1;
#if defined(_WIN32) || defined(WIN32)
		return setsockopt(listen_socket,
		                  SOL_SOCKET,
		                  SO_EXCLUSIVEADDRUSE,
		                  reinterpret_cast<const char *>(&enabled),
		                  sizeof(enabled)) != BridgeSocketError;
#else
		return setsockopt(listen_socket,
		                  SOL_SOCKET,
		                  SO_REUSEADDR,
		                  &enabled,
		                  sizeof(enabled)) != BridgeSocketError;
#endif
	}

	bool BindListener()
	{
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(listen_port);
		return bind(listen_socket,
		            reinterpret_cast<sockaddr *>(&address),
		            sizeof(address)) != BridgeSocketError;
	}

	static bool SetNonBlocking(const bridge_socket_t socket)
	{
#if defined(_WIN32) || defined(WIN32)
		u_long enabled = 1;
		return ioctlsocket(socket, FIONBIO, &enabled) != BridgeSocketError;
#else
		const int flags = fcntl(socket, F_GETFL, 0);
		return flags >= 0 &&
		       fcntl(socket, F_SETFL, flags | O_NONBLOCK) != BridgeSocketError;
#endif
	}

	static bool WouldBlock()
	{
#if defined(_WIN32) || defined(WIN32)
		return WSAGetLastError() == WSAEWOULDBLOCK;
#else
		return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
	}

	void AcceptConnection()
	{
		if (IsValidSocket(connection_socket) ||
		    !IsValidSocket(listen_socket))
			return;

		bridge_socket_t candidate = ::accept(listen_socket, nullptr, nullptr);
		if (!IsValidSocket(candidate)) {
			if (!WouldBlock())
				CloseListener();
			return;
		}
		if (!SetNonBlocking(candidate)) {
			CloseSocket(candidate);
			return;
		}
		connection_socket = candidate;
		connection_authenticated = false;
		authentication_buffer.clear();
		LOG_MSG("DOS bridge client connected; awaiting authentication");
	}

	int SendBytes(const uint8_t *data, const uint16_t size)
	{
#if defined(MSG_NOSIGNAL)
		return ::send(connection_socket,
		              reinterpret_cast<const char *>(data),
		              size,
		              MSG_NOSIGNAL);
#else
		return ::send(connection_socket,
		              reinterpret_cast<const char *>(data),
		              size,
		              0);
#endif
	}

	void CloseListener()
	{
		CloseSocket(listen_socket);
	}

	const uint16_t listen_port;
	const std::string authentication_token;
	bridge_socket_t listen_socket;
	bridge_socket_t connection_socket;
	bridge_socket_t control_listen_socket;
	bridge_socket_t control_socket;
	bool session_active;
	bool connection_authenticated;
	bool control_authenticated = false;
	std::string authentication_buffer;
	std::string session_id;
	std::string input_line_buffer;
	std::string control_buffer;
#if defined(_WIN32) || defined(WIN32)
	bool winsock_started;
#endif
};
