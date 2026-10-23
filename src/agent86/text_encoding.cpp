// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "text_encoding.h"

#include <array>
#include <stdexcept>

namespace Agent86 {
namespace {

constexpr std::array<uint16_t, 128> ExtendedCp437 = {{
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
	0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
}};

void AppendUtf8(std::string &output, const uint32_t codepoint)
{
	if (codepoint <= 0x7f) {
		output.push_back(static_cast<char>(codepoint));
	} else if (codepoint <= 0x7ff) {
		output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
		output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	} else {
		output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
		output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
}

uint32_t DecodeUtf8(const std::string &text, size_t &position)
{
	const auto first = static_cast<uint8_t>(text[position++]);
	if (first < 0x80)
		return first;

	uint32_t codepoint = 0;
	size_t continuation_count = 0;
	uint32_t minimum = 0;
	if ((first & 0xe0) == 0xc0) {
		codepoint = first & 0x1f;
		continuation_count = 1;
		minimum = 0x80;
	} else if ((first & 0xf0) == 0xe0) {
		codepoint = first & 0x0f;
		continuation_count = 2;
		minimum = 0x800;
	} else if ((first & 0xf8) == 0xf0) {
		codepoint = first & 0x07;
		continuation_count = 3;
		minimum = 0x10000;
	} else {
		throw std::runtime_error("text is not valid UTF-8");
	}

	if (position + continuation_count > text.size())
		throw std::runtime_error("text ends inside a UTF-8 sequence");
	for (size_t i = 0; i < continuation_count; i++) {
		const auto next = static_cast<uint8_t>(text[position++]);
		if ((next & 0xc0) != 0x80)
			throw std::runtime_error("text is not valid UTF-8");
		codepoint = (codepoint << 6) | (next & 0x3f);
	}
	if (codepoint < minimum || codepoint > 0x10ffff ||
	    (codepoint >= 0xd800 && codepoint <= 0xdfff))
		throw std::runtime_error("text contains an invalid UTF-8 code point");
	return codepoint;
}

uint8_t EncodeCodepoint(const uint32_t codepoint)
{
	if (codepoint < 0x80)
		return static_cast<uint8_t>(codepoint);
	for (size_t index = 0; index < ExtendedCp437.size(); index++) {
		if (ExtendedCp437[index] == codepoint)
			return static_cast<uint8_t>(index + 0x80);
	}
	throw std::runtime_error("text contains characters unavailable in DOS CP437");
}

} // namespace

std::vector<uint8_t> Utf8ToCp437(const std::string &text)
{
	std::vector<uint8_t> output;
	output.reserve(text.size());
	size_t position = 0;
	while (position < text.size())
		output.push_back(EncodeCodepoint(DecodeUtf8(text, position)));
	return output;
}

std::string Cp437ToUtf8(const std::vector<uint8_t> &bytes)
{
	return Cp437ToUtf8(bytes.data(), bytes.size());
}

std::string Cp437ToUtf8(const uint8_t *bytes, const size_t length)
{
	std::string output;
	output.reserve(length);
	for (size_t i = 0; i < length; i++) {
		const uint8_t byte = bytes[i];
		if (byte < 0x80)
			output.push_back(static_cast<char>(byte));
		else
			AppendUtf8(output, ExtendedCp437[byte - 0x80]);
	}
	return output;
}

} // namespace Agent86
