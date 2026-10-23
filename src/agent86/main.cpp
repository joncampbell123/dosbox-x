// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "dos_bridge_client.h"
#include "mcp_server.h"
#include "session_logger.h"

#include <exception>
#include <iostream>
#include <string>

int main(const int argc, char **argv)
{
	if (argc == 2 && std::string(argv[1]) == "--version") {
		std::cout << "agent86-mcp 2.0.0\n";
		return 0;
	}
	if (argc != 1) {
		std::cerr << "usage: agent86-mcp [--version]\n";
		return 2;
	}
	try {
		Agent86::DosBridgeClient bridge;
		Agent86::SessionLogger logger;
		Agent86::McpServer server(bridge, logger);
		return server.Run(std::cin, std::cout, std::cerr);
	} catch (const std::exception &error) {
		std::cerr << "agent86-mcp: " << error.what() << '\n';
		return 1;
	}
}
