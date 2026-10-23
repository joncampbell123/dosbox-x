// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Agent86 {

void FillSecureRandom(uint8_t *output, size_t length);
std::string SecureRandomHex(size_t byte_count);

} // namespace Agent86
