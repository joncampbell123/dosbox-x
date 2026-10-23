// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "dos_bridge_client.h"

#include "secure_random.h"
#include "text_encoding.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using NativeSocket = SOCKET;
constexpr NativeSocket InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using NativeSocket = int;
constexpr NativeSocket InvalidSocket = -1;
#endif

namespace Agent86 {
namespace {

constexpr size_t MaxCommandBytes = 4096;
constexpr size_t MaxFileBytes = 64 * 1024;
constexpr size_t MaxDosPathBytes = 260;
constexpr size_t ReceiveChunkBytes = 4096;
constexpr size_t MarkerRandomBytes = 12;
constexpr auto ConnectTimeout = std::chrono::seconds(3);

std::string HexEncode(const std::string &value)
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

unsigned HexValue(const char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	throw BridgeError("DOSBox-X control service returned invalid hexadecimal data");
}

std::string HexDecode(const std::string &value)
{
	if (value.size() % 2 != 0)
		throw BridgeError("DOSBox-X control service returned truncated data");
	std::string decoded;
	decoded.reserve(value.size() / 2);
	for (size_t index = 0; index < value.size(); index += 2)
		decoded.push_back(static_cast<char>((HexValue(value[index]) << 4) |
		                                    HexValue(value[index + 1])));
	return decoded;
}

#if defined(_WIN32)
class SocketRuntime {
public:
	SocketRuntime()
	{
		WSADATA data = {};
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			throw BridgeError("cannot initialize Windows sockets");
	}
	~SocketRuntime() { WSACleanup(); }
};

SocketRuntime &Runtime()
{
	static SocketRuntime runtime;
	return runtime;
}
#endif

void CloseNativeSocket(NativeSocket &socket)
{
	if (socket == InvalidSocket)
		return;
#if defined(_WIN32)
	closesocket(socket);
#else
	close(socket);
#endif
	socket = InvalidSocket;
}

int LastSocketError()
{
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif
}

bool IsWouldBlock(const int error)
{
#if defined(_WIN32)
	return error == WSAEWOULDBLOCK;
#else
	return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

bool IsTimedOut(const int error)
{
#if defined(_WIN32)
	return error == WSAETIMEDOUT;
#else
	return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

std::string SocketErrorText(const int error)
{
	return "socket error " + std::to_string(error);
}

void SetBlocking(const NativeSocket socket, const bool blocking)
{
#if defined(_WIN32)
	u_long mode = blocking ? 0 : 1;
	if (ioctlsocket(socket, FIONBIO, &mode) != 0)
		throw BridgeError("cannot change bridge socket mode");
#else
	const int flags = fcntl(socket, F_GETFL, 0);
	if (flags < 0 ||
	    fcntl(socket,
	          F_SETFL,
	          blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK) < 0)
		throw BridgeError("cannot change bridge socket mode");
#endif
}

timeval ToTimeval(const std::chrono::milliseconds timeout)
{
	timeval value = {};
	value.tv_sec = static_cast<long>(timeout.count() / 1000);
	value.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
	return value;
}

void RequirePositiveTimeout(const std::chrono::milliseconds timeout)
{
	if (timeout.count() <= 0)
		throw std::invalid_argument("timeout must be positive");
}

bool IsAllowedDosNameCharacter(const char ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
	       (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
}

void ValidateDosComponent(const std::string &component, const bool filename)
{
	if (component.empty())
		throw std::invalid_argument("DOS path contains an empty component");
	const size_t dot = component.find('.');
	if (!filename && dot != std::string::npos)
		throw std::invalid_argument("DOS directory names must use 8.3 characters");
	if (dot != std::string::npos && component.find('.', dot + 1) != std::string::npos)
		throw std::invalid_argument("DOS filename contains multiple extensions");
	const size_t stem_length = dot == std::string::npos ? component.size() : dot;
	const size_t extension_length =
	        dot == std::string::npos ? 0 : component.size() - dot - 1;
	if (stem_length == 0 || stem_length > 8 ||
	    (dot != std::string::npos && (extension_length == 0 || extension_length > 3)))
		throw std::invalid_argument("DOS path must use 8.3 names");
	for (size_t index = 0; index < component.size(); index++) {
		if (index == dot)
			continue;
		if (!IsAllowedDosNameCharacter(component[index]))
			throw std::invalid_argument("DOS path contains an unsupported character");
	}
}

std::string NormalizeDosPath(const std::string &filename)
{
	if (filename.size() < 4 || filename.size() > MaxDosPathBytes ||
	    !((filename[0] >= 'A' && filename[0] <= 'Z') ||
	      (filename[0] >= 'a' && filename[0] <= 'z')) ||
	    filename[1] != ':' || (filename[2] != '\\' && filename[2] != '/'))
		throw std::invalid_argument(
		        "filename must be an absolute DOS 8.3 path such as A:\\SOURCE\\GAME.C");

	std::string path = filename;
	path[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(path[0])));
	std::replace(path.begin(), path.end(), '/', '\\');
	size_t start = 3;
	while (start < path.size()) {
		const size_t separator = path.find('\\', start);
		const bool is_filename = separator == std::string::npos;
		const size_t end = is_filename ? path.size() : separator;
		ValidateDosComponent(path.substr(start, end - start), is_filename);
		if (is_filename)
			return path;
		start = end + 1;
	}
	throw std::invalid_argument("filename must end with an 8.3 filename");
}

std::string SiblingPath(const std::string &path, const char *prefix)
{
	const size_t slash = path.rfind('\\');
	const std::string directory = slash == 2 ? path.substr(0, 2)
	                                        : path.substr(0, slash);
	return directory + "\\" + prefix + SecureRandomHex(3) + ".TMP";
}

std::string Basename(const std::string &path)
{
	return path.substr(path.rfind('\\') + 1);
}

std::string BytesToString(const std::vector<uint8_t> &bytes)
{
	return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

std::string EncodeCommand(const std::string &command)
{
	if (command.empty() ||
	    std::all_of(command.begin(), command.end(), [](const unsigned char ch) {
		    return ch == ' ' || ch == '\t';
	    }))
		throw std::invalid_argument("command must not be empty");
	const auto encoded = Utf8ToCp437(command);
	if (encoded.size() > MaxCommandBytes)
		throw std::invalid_argument("command exceeds 4096 encoded bytes");
	for (const uint8_t byte : encoded) {
		if ((byte < 0x20 && byte != '\t') || byte == 0x7f)
			throw std::invalid_argument("command must contain exactly one DOS command line");
	}
	return BytesToString(encoded);
}

std::string EncodeFileContent(const std::string &content)
{
	std::string normalized;
	normalized.reserve(content.size());
	for (size_t index = 0; index < content.size(); index++) {
		if (content[index] == '\r') {
			if (index + 1 < content.size() && content[index + 1] == '\n')
				index++;
			normalized.push_back('\n');
		} else {
			normalized.push_back(content[index]);
		}
	}
	const auto encoded = Utf8ToCp437(normalized);
	std::string output;
	output.reserve(encoded.size() + 2);
	for (const uint8_t byte : encoded) {
		if ((byte < 0x20 && byte != '\t' && byte != '\n') || byte == 0x7f)
			throw std::invalid_argument("content contains unsupported control characters");
		if (byte == '\n') output += "\r\n";
		else output.push_back(static_cast<char>(byte));
	}
	if (!output.empty() &&
	    (output.size() < 2 || output.compare(output.size() - 2, 2, "\r\n") != 0))
		output += "\r\n";
	if (output.size() > MaxFileBytes)
		throw std::invalid_argument("content exceeds 65536 encoded bytes");
	return output;
}

std::string NewMarker(const char *kind)
{
	return std::string("__DOSBRIDGE_") + kind + "_" +
	       SecureRandomHex(MarkerRandomBytes) + "__";
}

size_t FindLineMarker(const std::vector<uint8_t> &output,
	                    const std::string &marker)
{
	const auto begin = output.begin();
	auto current = begin;
	while (current != output.end()) {
		current = std::search(current, output.end(), marker.begin(), marker.end());
		if (current == output.end())
			return std::string::npos;
		const size_t position = static_cast<size_t>(current - begin);
		const bool starts_line = position == 0 || output[position - 1] == '\n';
		const size_t after = position + marker.size();
		const bool ends_line = after == output.size() || output[after] == '\r' ||
		                       output[after] == '\n';
		if (starts_line && ends_line) {
			if (position > 1 && output[position - 2] == '\r') return position - 2;
			if (position > 0 && output[position - 1] == '\n') return position - 1;
			return position;
		}
		current++;
	}
	return std::string::npos;
}

bool ContainsLineMarker(const std::string &output, const std::string &marker)
{
	const std::vector<uint8_t> bytes(output.begin(), output.end());
	return FindLineMarker(bytes, marker) != std::string::npos;
}

std::string StripExitReport(const std::string &output, const std::string &prefix)
{
	std::string result = output;
	size_t marker = result.find(prefix);
	if (marker == std::string::npos)
		return result;
	size_t end = result.find("__", marker + prefix.size());
	if (end == std::string::npos)
		return result;
	end += 2;
	if (end + 1 < result.size() && result[end] == '\r' && result[end + 1] == '\n')
		end += 2;
	else if (end < result.size() && result[end] == '\n')
		end++;
	result.erase(marker, end - marker);
	return result;
}

int ExtractExitCode(const std::string &output, const std::string &prefix)
{
	const size_t marker = output.find(prefix);
	if (marker == std::string::npos)
		return -1;
	const size_t digits = marker + prefix.size();
	const size_t end = output.find("__", digits);
	if (end == std::string::npos || end == digits)
		return -1;
	int value = 0;
	for (size_t index = digits; index < end; index++) {
		if (output[index] < '0' || output[index] > '9')
			return -1;
		if (value > (std::numeric_limits<int>::max() - 9) / 10)
			return -1;
		value = value * 10 + (output[index] - '0');
	}
	return value;
}

} // namespace

class DosBridgeClient::Socket {
public:
	Socket(const uint16_t port,
	       const std::string &authentication_token,
	       const std::string &required_session = {})
	{
#if defined(_WIN32)
		(void)Runtime();
#endif
		handle = ::socket(AF_INET, SOCK_STREAM, 0);
		if (handle == InvalidSocket)
			throw BridgeError("cannot create DOS bridge socket");
		try {
			Connect(port);
			if (required_session.empty())
				Authenticate(authentication_token);
			else
				AuthenticateControl(authentication_token, required_session);
		} catch (...) {
			CloseNativeSocket(handle);
			throw;
		}
	}

	~Socket() { CloseNativeSocket(handle); }

	const std::string &SessionId() const { return session_id; }

	void SendAll(const std::string &data,
	             const std::chrono::milliseconds timeout)
	{
		SetTimeout(SO_SNDTIMEO, timeout);
		size_t sent = 0;
		while (sent < data.size()) {
			const size_t remaining = data.size() - sent;
			const int chunk = static_cast<int>(std::min<size_t>(remaining, INT_MAX));
			const int count = ::send(handle,
			                         data.data() + sent,
			                         chunk,
#if defined(MSG_NOSIGNAL)
			                         MSG_NOSIGNAL
#else
			                         0
#endif
			);
			if (count <= 0)
				throw BridgeError("DOS bridge send failed: " +
				                  SocketErrorText(LastSocketError()));
			sent += static_cast<size_t>(count);
		}
	}

	std::vector<uint8_t> Receive(const std::chrono::milliseconds timeout)
	{
		SetTimeout(SO_RCVTIMEO, timeout);
		std::vector<uint8_t> bytes(ReceiveChunkBytes);
		const int count = ::recv(handle,
		                         reinterpret_cast<char *>(bytes.data()),
		                         static_cast<int>(bytes.size()),
		                         0);
		if (count == 0)
			return {};
		if (count < 0) {
			const int error = LastSocketError();
			if (IsTimedOut(error))
				throw BridgeError("DOS bridge receive timed out");
			throw BridgeError("DOS bridge receive failed: " +
			                  SocketErrorText(error));
		}
		bytes.resize(static_cast<size_t>(count));
		return bytes;
	}

	void DiscardPending()
	{
		SetBlocking(handle, false);
		std::array<char, ReceiveChunkBytes> buffer = {};
		try {
			for (;;) {
				const int count = ::recv(handle,
				                         buffer.data(),
				                         static_cast<int>(buffer.size()),
				                         0);
				if (count > 0) continue;
				if (count == 0)
					throw BridgeError("DOSBox-X closed the bridge connection");
				if (IsWouldBlock(LastSocketError())) break;
				throw BridgeError("cannot discard pending DOS prompt");
			}
			SetBlocking(handle, true);
		} catch (...) {
			try { SetBlocking(handle, true); } catch (...) {}
			throw;
		}
	}

	void ShutdownWrite()
	{
#if defined(_WIN32)
		shutdown(handle, SD_SEND);
#else
		shutdown(handle, SHUT_WR);
#endif
	}

private:
	void AuthenticateControl(const std::string &token,
	                        const std::string &required_session)
	{
		if (token.empty())
			throw BridgeError("DOS_BRIDGE_TOKEN is required for DOSBox-X control authentication");
		SendAll("AGENT86/1 CONTROL-AUTH " + required_session + " " + HexEncode(token) + "\n",
		        ConnectTimeout);
		ReadAuthenticationResponse();
		if (session_id != required_session)
			throw BridgeError("DOSBox-X rejected the active bridge session");
	}

	void Authenticate(const std::string &token)
	{
		if (token.empty())
			throw BridgeError("DOS_BRIDGE_TOKEN is required for CTTY BRIDGE authentication");
		if (token.size() > 256 || token.find_first_of("\r\n") != std::string::npos)
			throw BridgeError("DOS_BRIDGE_TOKEN must be 1 to 256 characters without line breaks");
		SendAll("AGENT86/1 AUTH " + token + "\n", ConnectTimeout);
		ReadAuthenticationResponse();
	}

	void ReadAuthenticationResponse()
	{
		std::string line;
		const auto deadline = std::chrono::steady_clock::now() + ConnectTimeout;
		while (line.find('\n') == std::string::npos) {
			const auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				throw BridgeError("CTTY BRIDGE authentication timed out");
			const auto response = Receive(
			        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
			if (response.empty())
				throw BridgeError("DOSBox-X closed the bridge during authentication");
			line.append(response.begin(), response.end());
			if (line.size() > 128)
				throw BridgeError("CTTY BRIDGE sent an oversized authentication response");
		}
		const std::string prefix = "AGENT86/1 OK ";
		if (line.size() != prefix.size() + 32 + 1 ||
		    line.compare(0, prefix.size(), prefix) != 0 || line.back() != '\n' ||
		    !std::all_of(line.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
		                 line.end() - 1,
		                 [](const unsigned char ch) { return std::isxdigit(ch) != 0; }))
			throw BridgeError("CTTY BRIDGE authentication was rejected");
		session_id = line.substr(prefix.size(), 32);
	}

	void Connect(const uint16_t port)
	{
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		SetBlocking(handle, false);
		const int result = ::connect(handle,
		                             reinterpret_cast<sockaddr *>(&address),
		                             sizeof(address));
		if (result != 0 && !ConnectInProgress(LastSocketError()))
			throw BridgeError("cannot connect to DOSBox-X: " +
			                  SocketErrorText(LastSocketError()));
		if (result != 0)
			WaitForConnection();
		SetBlocking(handle, true);
	}

	void WaitForConnection()
	{
		fd_set write_set;
		FD_ZERO(&write_set);
		FD_SET(handle, &write_set);
		timeval timeout = ToTimeval(ConnectTimeout);
		const int selected = select(
#if defined(_WIN32)
		                            0,
#else
		                            handle + 1,
#endif
		                            nullptr,
		                            &write_set,
		                            nullptr,
		                            &timeout);
		if (selected <= 0)
			throw BridgeError("timed out connecting to DOSBox-X");
		int error = 0;
#if defined(_WIN32)
		int length = sizeof(error);
#else
		socklen_t length = sizeof(error);
#endif
		if (getsockopt(handle,
		               SOL_SOCKET,
		               SO_ERROR,
		               reinterpret_cast<char *>(&error),
		               &length) != 0 || error != 0)
			throw BridgeError("cannot connect to DOSBox-X: " +
			                  SocketErrorText(error));
	}

	static bool ConnectInProgress(const int error)
	{
#if defined(_WIN32)
		return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
		return error == EINPROGRESS;
#endif
	}

	void SetTimeout(const int option, const std::chrono::milliseconds timeout)
	{
#if defined(_WIN32)
		const DWORD milliseconds = static_cast<DWORD>(std::min<int64_t>(
		        timeout.count(), std::numeric_limits<DWORD>::max()));
		if (setsockopt(handle,
		               SOL_SOCKET,
		               option,
		               reinterpret_cast<const char *>(&milliseconds),
		               sizeof(milliseconds)) != 0)
			throw BridgeError("cannot configure DOS bridge socket timeout");
#else
		timeval value = ToTimeval(timeout);
		if (setsockopt(handle, SOL_SOCKET, option, &value, sizeof(value)) != 0)
			throw BridgeError("cannot configure DOS bridge socket timeout");
#endif
	}

	NativeSocket handle = InvalidSocket;
	std::string session_id;
};

DosBridgeClient::DosBridgeClient(const uint16_t requested_port,
                                 const size_t requested_max_output_bytes)
        : port(requested_port), max_output_bytes(requested_max_output_bytes)
{
	if (port == 0)
		throw std::invalid_argument("port must be between 1 and 65535");
	if (max_output_bytes == 0)
		throw std::invalid_argument("max_output_bytes must be positive");
}

DosBridgeClient::~DosBridgeClient()
{
	Close();
}

std::string DosBridgeClient::RunCommand(
        const std::string &command,
        const std::chrono::milliseconds timeout)
{
	const std::string encoded_command = EncodeCommand(command);
	const std::string exit_hex = SecureRandomHex(MarkerRandomBytes);
	const std::string exit_prefix = "__DOSBRIDGE_EXIT_" + exit_hex + "_";
	const std::string end_marker = NewMarker("END");
	const std::string payload = encoded_command + "\r" + "echo " + exit_prefix +
	                            "%ERRORLEVEL%__\r" + "echo " + end_marker + "\r";
	const std::string output = Exchange(payload, end_marker, timeout);
	const int exit_code = ExtractExitCode(output, exit_prefix);
	if (exit_code < 0)
		throw BridgeError("DOS did not report an exit code; session state is unknown");
	const std::string clean_output = StripExitReport(output, exit_prefix);
	if (exit_code != 0)
		throw BridgeError("DOS command failed (exit code " +
		                  std::to_string(exit_code) + "):\n" + clean_output);
	if (clean_output.find("Bad command or filename") != std::string::npos)
		throw BridgeError("DOS command failed (unrecognized command):\n" +
		                  clean_output);
	return clean_output;
}

std::string DosBridgeClient::WriteFile(
        const std::string &filename,
        const std::string &content,
        const std::chrono::milliseconds timeout)
{
	const std::string target = NormalizeDosPath(filename);
	const std::string staging = SiblingPath(target, "BR");
	const std::string backup = SiblingPath(target, "BK");
	const std::string preflight_ok = NewMarker("PREFLIGHT_OK");
	const std::string preflight_fail = NewMarker("PREFLIGHT_FAIL");
	const std::string preflight_end = NewMarker("END");
	const std::string preflight =
	        "copy nul " + staging + " > nul\r" +
	        "if exist " + staging + " echo " + preflight_ok + "\r" +
	        "if not exist " + staging + " echo " + preflight_fail + "\r" +
	        "if exist " + staging + " del " + staging + " > nul\r" +
	        "echo " + preflight_end + "\r";
	const std::string preflight_output = Exchange(preflight, preflight_end, timeout);
	if (!ContainsLineMarker(preflight_output, preflight_ok) ||
	    ContainsLineMarker(preflight_output, preflight_fail))
		throw BridgeError("DOS cannot write to " + target + ":\n" +
		                  preflight_output);

	const std::string write_ok = NewMarker("WRITE_OK");
	const std::string write_fail = NewMarker("WRITE_FAIL");
	const std::string write_end = NewMarker("END");
	const std::string encoded_content = EncodeFileContent(content);
	const std::string payload =
	        "copy con " + staging + "\r" + encoded_content + "\x1a\r" +
	        "if exist " + backup + " del " + backup + " > nul\r" +
	        "if exist " + target + " ren " + target + " " + Basename(backup) + "\r" +
	        "if not exist " + target + " if exist " + staging + " ren " + staging +
	                " " + Basename(target) + "\r" +
	        "if not exist " + target + " if exist " + backup + " ren " + backup +
	                " " + Basename(target) + "\r" +
	        "if exist " + target + " if not exist " + staging + " echo " + write_ok + "\r" +
	        "if not exist " + target + " echo " + write_fail + "\r" +
	        "if exist " + backup + " if exist " + target + " del " + backup + " > nul\r" +
	        "echo " + write_end + "\r";
	const std::string output = Exchange(payload, write_end, timeout);
	if (!ContainsLineMarker(output, write_ok) ||
	    ContainsLineMarker(output, write_fail))
		throw BridgeError("DOS did not confirm writing " + target + ":\n" + output);
	return "File " + target + " written successfully.\n" + output;
}

std::string DosBridgeClient::ReleaseConsole(
        const std::chrono::milliseconds timeout)
{
	RequirePositiveTimeout(timeout);
	std::lock_guard<std::mutex> lock(mutex);
	try {
		Socket &connection = EnsureConnected();
		connection.DiscardPending();
		connection.SendAll("CTTY CON\r", timeout);
		connection.ShutdownWrite();
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for (;;) {
			const auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				throw BridgeError("DOSBox-X did not release the bridge console");
			if (connection.Receive(std::chrono::duration_cast<std::chrono::milliseconds>(
			                           deadline - now)).empty())
				break;
		}
	} catch (const BridgeError &) {
		CloseUnlocked();
		throw;
	}
	CloseUnlocked();
	return "DOS console released to the local DOSBox-X window.";
}

std::string DosBridgeClient::RunControl(
        const std::string &operation,
        const std::string &payload,
        const std::chrono::milliseconds timeout)
{
	RequirePositiveTimeout(timeout);
	if (operation.empty() || operation.size() > 64 ||
	    !std::all_of(operation.begin(), operation.end(), [](const unsigned char ch) {
		    return std::islower(ch) || ch == '_';
	    }))
		throw std::invalid_argument("control operation must use lowercase letters and underscores");
	if (payload.size() > MaxFileBytes)
		throw std::invalid_argument("control payload exceeds 65536 bytes");
	std::lock_guard<std::mutex> lock(mutex);
	Socket &console = EnsureConnected();
	if (port == std::numeric_limits<uint16_t>::max())
		throw BridgeError("DOS bridge control port is unavailable");
	const char *token = std::getenv("DOS_BRIDGE_TOKEN");
	Socket control(static_cast<uint16_t>(port + 1),
	               token ? token : "",
	               console.SessionId());
	if (control.SessionId() != console.SessionId())
		throw BridgeError("DOSBox-X control connection belongs to a different bridge session");
	control.SendAll("AGENT86/1 CONTROL " + operation + " " + HexEncode(payload) + "\n",
	                timeout);
	std::string line;
	while (line.find('\n') == std::string::npos) {
		const auto chunk = control.Receive(timeout);
		if (chunk.empty())
			throw BridgeError("DOSBox-X closed the control connection");
		line.append(chunk.begin(), chunk.end());
		if (line.size() > MaxFileBytes * 2 + 64)
			throw BridgeError("DOSBox-X control response exceeds the size limit");
	}
	if (line.back() == '\n') line.pop_back();
	const std::string ok_prefix = "AGENT86/1 RESULT OK ";
	const std::string error_prefix = "AGENT86/1 RESULT ERROR ";
	if (line.compare(0, ok_prefix.size(), ok_prefix) == 0)
		return HexDecode(line.substr(ok_prefix.size()));
	if (line.compare(0, error_prefix.size(), error_prefix) == 0)
		throw BridgeError(HexDecode(line.substr(error_prefix.size())));
	throw BridgeError("DOSBox-X control service returned an invalid response");
}

void DosBridgeClient::Close()
{
	std::lock_guard<std::mutex> lock(mutex);
	CloseUnlocked();
}

uint16_t DosBridgeClient::ConfiguredPort()
{
	const char *raw = std::getenv("DOS_BRIDGE_PORT");
	if (!raw || !*raw)
		return DefaultPort;
	char *end = nullptr;
	const long value = std::strtol(raw, &end, 10);
	if (*end != '\0' || value < 1 || value > 65535)
		throw BridgeError("DOS_BRIDGE_PORT must be an integer between 1 and 65535");
	return static_cast<uint16_t>(value);
}

std::string DosBridgeClient::Exchange(
        const std::string &payload,
        const std::string &end_marker,
        const std::chrono::milliseconds timeout)
{
	RequirePositiveTimeout(timeout);
	std::lock_guard<std::mutex> lock(mutex);
	try {
		Socket &connection = EnsureConnected();
		connection.DiscardPending();
		connection.SendAll(payload, timeout);
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::vector<uint8_t> output;
		for (;;) {
			const auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				throw BridgeError("DOS command timed out");
			auto chunk = connection.Receive(
			        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
			if (chunk.empty())
				throw BridgeError("DOSBox-X closed the bridge connection");
			if (output.size() > max_output_bytes ||
			    chunk.size() > max_output_bytes - output.size())
				throw BridgeError("DOS command output exceeded " +
				                  std::to_string(max_output_bytes) + " bytes");
			output.insert(output.end(), chunk.begin(), chunk.end());
			const size_t marker = FindLineMarker(output, end_marker);
			if (marker != std::string::npos)
				return Cp437ToUtf8(output.data(), marker);
		}
	} catch (...) {
		CloseUnlocked();
		throw;
	}
}

DosBridgeClient::Socket &DosBridgeClient::EnsureConnected()
{
	if (!socket) {
		const char *token = std::getenv("DOS_BRIDGE_TOKEN");
		socket = std::make_unique<Socket>(port, token ? token : "");
	}
	return *socket;
}

void DosBridgeClient::CloseUnlocked()
{
	socket.reset();
}

} // namespace Agent86
