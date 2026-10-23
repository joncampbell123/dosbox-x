// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "plugin_stager.h"

#include <exception>
#include <iostream>

int main(const int argc, char **argv)
{
	if (argc != 4) {
		std::cerr << "usage: agent86-plugin-stage SOURCE_ROOT OUTPUT_ROOT MCP_BINARY\n";
		return 2;
	}
	try {
		Agent86::StagePlugins(argv[1], argv[2], argv[3]);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "agent86-plugin-stage: " << error.what() << '\n';
		return 1;
	}
}
