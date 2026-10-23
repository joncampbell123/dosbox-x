// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "secure_random.h"

#include <array>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <cerrno>
#include <sys/random.h>
#else
#include <cstdlib>
#endif

namespace Agent86 {

void FillSecureRandom(uint8_t *output, const size_t length)
{
	if (length == 0)
		return;
#if defined(_WIN32)
	if (length > static_cast<size_t>(ULONG_MAX) ||
	    BCryptGenRandom(nullptr,
	                    output,
	                    static_cast<ULONG>(length),
	                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
		throw std::runtime_error("Windows secure random generator failed");
#elif defined(__linux__)
	size_t completed = 0;
	while (completed < length) {
		const ssize_t count = getrandom(output + completed, length - completed, 0);
		if (count > 0) {
			completed += static_cast<size_t>(count);
			continue;
		}
		if (count < 0 && errno == EINTR)
			continue;
		throw std::runtime_error("Linux secure random generator failed");
	}
#else
	arc4random_buf(output, length);
#endif
}

std::string SecureRandomHex(const size_t byte_count)
{
	static constexpr char HexDigits[] = "0123456789ABCDEF";
	std::vector<uint8_t> bytes(byte_count);
	FillSecureRandom(bytes.data(), bytes.size());
	std::string output;
	output.reserve(byte_count * 2);
	for (const uint8_t byte : bytes) {
		output.push_back(HexDigits[byte >> 4]);
		output.push_back(HexDigits[byte & 0x0f]);
	}
	return output;
}

} // namespace Agent86
