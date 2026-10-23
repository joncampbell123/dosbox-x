// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <filesystem>

namespace Agent86 {

void StagePlugins(const std::filesystem::path &source_root,
	              const std::filesystem::path &output_root,
	              const std::filesystem::path &mcp_binary);

} // namespace Agent86
