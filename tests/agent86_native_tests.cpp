// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "dos_bridge_client.h"
#include "json.h"
#include "mcp_server.h"
#include "session_logger.h"
#include "text_encoding.h"
#include "../src/gui/screen_export.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
constexpr TestSocket InvalidTestSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSocket = int;
constexpr TestSocket InvalidTestSocket = -1;
#endif

namespace {

void Require(const bool condition, const std::string &message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void CloseSocket(TestSocket socket)
{
#if defined(_WIN32)
	closesocket(socket);
#else
	close(socket);
#endif
}

void SendAll(const TestSocket socket, const std::string &data)
{
	size_t sent = 0;
	while (sent < data.size()) {
		const int count = send(socket,
		                       data.data() + sent,
		                       static_cast<int>(data.size() - sent),
		                       0);
		if (count <= 0)
			throw std::runtime_error("fake bridge send failed");
		sent += static_cast<size_t>(count);
	}
}

std::string ReceiveUntil(const TestSocket socket,
	                     const std::string &marker,
	                     const size_t maximum)
{
	std::string data;
	std::array<char, 1024> buffer = {};
	while (data.find(marker) == std::string::npos) {
		const int count = recv(socket, buffer.data(), buffer.size(), 0);
		if (count <= 0)
			throw std::runtime_error("fake bridge receive failed");
		data.append(buffer.data(), static_cast<size_t>(count));
		if (data.size() > maximum)
			throw std::runtime_error("fake bridge request exceeded its test bound");
	}
	return data;
}

class FakeBridge {
public:
	FakeBridge()
	{
#if defined(_WIN32)
		WSADATA data = {};
		Require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup failed");
#endif
		listener = socket(AF_INET, SOCK_STREAM, 0);
		Require(listener != InvalidTestSocket, "fake bridge socket failed");
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		Require(bind(listener,
		             reinterpret_cast<sockaddr *>(&address),
		             sizeof(address)) == 0,
		        "fake bridge bind failed");
		Require(listen(listener, 1) == 0, "fake bridge listen failed");
#if defined(_WIN32)
		int length = sizeof(address);
#else
		socklen_t length = sizeof(address);
#endif
		Require(getsockname(listener,
		                    reinterpret_cast<sockaddr *>(&address),
		                    &length) == 0,
		        "fake bridge getsockname failed");
		port = ntohs(address.sin_port);
		worker = std::thread([this]() { Serve(); });
	}

	~FakeBridge()
	{
		if (worker.joinable())
			worker.join();
		CloseSocket(listener);
#if defined(_WIN32)
		WSACleanup();
#endif
	}

	uint16_t Port() const { return port; }

	void RethrowFailure()
	{
		if (worker.joinable())
			worker.join();
		if (failure)
			std::rethrow_exception(failure);
	}

private:
	void Serve()
	{
		TestSocket client = InvalidTestSocket;
		try {
			client = accept(listener, nullptr, nullptr);
			Require(client != InvalidTestSocket, "fake bridge accept failed");
			const std::string auth = ReceiveUntil(client, "\n", 512);
			Require(auth == "AGENT86/1 AUTH test-token\n",
			        "native client did not authenticate first");
			SendAll(client, "AGENT86/1 OK 00112233445566778899aabbccddeeff\n");
			const std::string request = ReceiveUntil(client, "\r", 16384);
			std::string all = request;
			while (std::count(all.begin(), all.end(), '\r') < 3)
				all += ReceiveUntil(client, "\r", 16384);

			const std::string exit_label = "__DOSBRIDGE_EXIT_";
			const size_t exit_start = all.find(exit_label);
			Require(exit_start != std::string::npos, "missing exit marker");
			const size_t percent = all.find("%ERRORLEVEL%__", exit_start);
			Require(percent != std::string::npos, "malformed exit marker");
			const std::string exit_prefix = all.substr(exit_start, percent - exit_start);
			const std::string end_label = "__DOSBRIDGE_END_";
			const size_t end_start = all.find(end_label);
			const size_t end_finish = all.find("__", end_start + end_label.size());
			Require(end_start != std::string::npos && end_finish != std::string::npos,
			        "missing end marker");
			const std::string end_marker =
			        all.substr(end_start, end_finish + 2 - end_start);
			SendAll(client,
			        "FAKE DOS OUTPUT\r\n" + exit_prefix + "0__\r\n" + end_marker +
			                "\r\n");
			CloseSocket(client);
		} catch (...) {
			if (client != InvalidTestSocket)
				CloseSocket(client);
			failure = std::current_exception();
		}
	}

	TestSocket listener = InvalidTestSocket;
	uint16_t port = 0;
	std::thread worker;
	std::exception_ptr failure;
};

void SetTestToken()
{
#if defined(_WIN32)
	_putenv_s("DOS_BRIDGE_TOKEN", "test-token");
#else
	setenv("DOS_BRIDGE_TOKEN", "test-token", 1);
#endif
}

void TestJsonAndEncoding()
{
	const Agent86::Json parsed = Agent86::Json::Parse(
	        "{\"message\":\"hello \\u263a\",\"values\":[true,null,3]}");
	Require(parsed.IsObject(), "JSON object did not parse");
	Require(Agent86::Json::Parse(parsed.Dump()).IsObject(),
	        "JSON did not round-trip");
	const auto cp437 = Agent86::Utf8ToCp437("DOS £");
	Require(Agent86::Cp437ToUtf8(cp437) == "DOS £", "CP437 did not round-trip");
	bool rejected = false;
	try {
		(void)Agent86::Json::Parse("{\"duplicate\":1,\"duplicate\":2}");
	} catch (const std::exception &) {
		rejected = true;
	}
	Require(rejected, "duplicate JSON keys were accepted");
}

void TestScreenExportFormatting()
{
	ScreenExport::Format format = ScreenExport::Format::Text;
	Require(ScreenExport::FormatFromPath("screen.TXT", format) &&
	                format == ScreenExport::Format::Text,
	        "TXT export extension was not recognized");
	Require(ScreenExport::FormatFromPath("screen.md", format) &&
	                format == ScreenExport::Format::Markdown,
	        "Markdown export extension was not recognized");
	Require(!ScreenExport::FormatFromPath("screen.exe", format),
	        "unsupported export extension was accepted");
	Require(ScreenExport::BuildDocument("A\r\nB", ScreenExport::Format::Text) ==
	                "A\nB\n",
	        "text export did not normalize line endings");
	const std::string markdown = ScreenExport::BuildDocument(
	        "DOS ``` screen", ScreenExport::Format::Markdown);
	Require(markdown.find("````text\nDOS ``` screen\n````\n") != std::string::npos,
	        "Markdown export did not protect its code fence");
}

void TestAuthenticatedMcpEndToEnd()
{
	SetTestToken();
	FakeBridge fake;
	Agent86::DosBridgeClient bridge(fake.Port());
	const auto log_path = std::filesystem::temp_directory_path() /
	                      "agent86-native-test-session.log";
	std::error_code ignored;
	std::filesystem::remove(log_path, ignored);
	Agent86::SessionLogger logger(log_path);
	Agent86::McpServer server(bridge, logger);
	std::istringstream input(
	        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\"}}\n"
	        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"run_dos_command\",\"arguments\":{\"command\":\"VER\"}}}\n");
	std::ostringstream output;
	std::ostringstream errors;
	Require(server.Run(input, output, errors) == 0, "MCP stdio loop failed");
	Require(output.str().find("FAKE DOS OUTPUT") != std::string::npos,
	        "MCP result did not contain DOS output");
	Require(output.str().find("\"isError\":false") != std::string::npos,
	        "MCP tool unexpectedly failed");
	fake.RethrowFailure();
	Require(std::filesystem::is_regular_file(log_path), "session log was not written");
	std::filesystem::remove(log_path, ignored);
}

void TestMcpInputBoundAndRecovery()
{
	Agent86::DosBridgeClient bridge(1);
	const auto log_path = std::filesystem::temp_directory_path() /
	                      "agent86-native-bound-test.log";
	Agent86::SessionLogger logger(log_path);
	Agent86::McpServer server(bridge, logger);
	std::string requests(1024 * 1024 + 1, 'x');
	requests += "\n{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"ping\",\"params\":{}}\n";
	std::istringstream input(requests);
	std::ostringstream output;
	std::ostringstream errors;
	Require(server.Run(input, output, errors) == 0, "bounded MCP loop failed");
	Require(output.str().find("MCP message exceeds 1048576 bytes") !=
	                std::string::npos,
	        "oversized MCP message was not rejected");
	Require(output.str().find("\"id\":7") != std::string::npos,
	        "MCP loop did not recover after an oversized message");
}

void TestDosBoxControlToolsArePublished()
{
	Agent86::DosBridgeClient bridge(1);
	const auto log_path = std::filesystem::temp_directory_path() /
	                      "agent86-native-tools-test.log";
	Agent86::SessionLogger logger(log_path);
	Agent86::McpServer server(bridge, logger);
	const Agent86::Json response = server.HandleRequest(Agent86::Json::Parse(
	        R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})"));
	const std::string tools = response.Dump();
	for (const char *name : {"swap_floppy_drive", "swap_cd_drive",
	                         "change_floppy_image", "change_cd_image",
	                         "create_blank_disk_image", "list_mounted_drives",
	                         "get_ide_status", "rescan_all_drives",
	                         "export_text_screen", "get_text_screen",
	                         "capture_graphics_screen", "move_mouse", "click_mouse",
	                         "set_floppy_image", "set_cd_image", "create_disk_image",
	                         "mount_host_folder", "unmount_drive", "type_text"})
		Require(tools.find(std::string("\"") + name + "\"") != std::string::npos,
		        std::string("MCP tool is missing: ") + name);
}

} // namespace

int main()
{
	try {
		TestJsonAndEncoding();
		TestScreenExportFormatting();
		TestMcpInputBoundAndRecovery();
		TestDosBoxControlToolsArePublished();
		TestAuthenticatedMcpEndToEnd();
		std::cout << "agent86 native tests passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "agent86 native test failed: " << error.what() << '\n';
		return 1;
	}
}
