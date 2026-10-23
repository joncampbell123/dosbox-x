// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Agent86 {

std::vector<uint8_t> Utf8ToCp437(const std::string &text);
std::string Cp437ToUtf8(const std::vector<uint8_t> &bytes);
std::string Cp437ToUtf8(const uint8_t *bytes, size_t length);

} // namespace Agent86
