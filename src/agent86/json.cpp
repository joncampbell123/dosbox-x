// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "json.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Agent86 {
namespace {

constexpr size_t MaxJsonDepth = 64;

void AppendUtf8(std::string &output, const uint32_t codepoint)
{
	if (codepoint <= 0x7f) {
		output.push_back(static_cast<char>(codepoint));
	} else if (codepoint <= 0x7ff) {
		output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
		output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	} else if (codepoint <= 0xffff) {
		output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
		output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	} else {
		output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
		output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
		output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
}

class Parser {
public:
	explicit Parser(const std::string &input) : text(input) {}

	Json ParseDocument()
	{
		SkipWhitespace();
		Json result = ParseValue(0);
		SkipWhitespace();
		if (position != text.size())
			Fail("unexpected trailing data");
		return result;
	}

private:
	Json ParseValue(const size_t depth)
	{
		if (depth > MaxJsonDepth)
			Fail("maximum nesting depth exceeded");
		if (position == text.size())
			Fail("unexpected end of input");

		switch (text[position]) {
		case 'n': return ParseLiteral("null", Json());
		case 't': return ParseLiteral("true", Json(true));
		case 'f': return ParseLiteral("false", Json(false));
		case '"': return Json(ParseString());
		case '[': return ParseArray(depth + 1);
		case '{': return ParseObject(depth + 1);
		default:
			if (text[position] == '-' || IsDigit(text[position]))
				return ParseNumber();
			Fail("unexpected character");
		}
	}

	Json ParseLiteral(const char *literal, Json value)
	{
		const std::string expected(literal);
		if (text.compare(position, expected.size(), expected) != 0)
			Fail("invalid literal");
		position += expected.size();
		return value;
	}

	Json ParseArray(const size_t depth)
	{
		Json::Array values;
		position++;
		SkipWhitespace();
		if (Consume(']'))
			return Json(std::move(values));

		for (;;) {
			SkipWhitespace();
			values.push_back(ParseValue(depth));
			SkipWhitespace();
			if (Consume(']'))
				return Json(std::move(values));
			Require(',');
		}
	}

	Json ParseObject(const size_t depth)
	{
		Json::Object values;
		position++;
		SkipWhitespace();
		if (Consume('}'))
			return Json(std::move(values));

		for (;;) {
			SkipWhitespace();
			if (position == text.size() || text[position] != '"')
				Fail("object key must be a string");
			std::string key = ParseString();
			SkipWhitespace();
			Require(':');
			SkipWhitespace();
			const auto inserted = values.emplace(std::move(key), ParseValue(depth));
			if (!inserted.second)
				Fail("duplicate object key");
			SkipWhitespace();
			if (Consume('}'))
				return Json(std::move(values));
			Require(',');
		}
	}

	Json ParseNumber()
	{
		const size_t start = position;
		Consume('-');
		if (Consume('0')) {
			if (position < text.size() && IsDigit(text[position]))
				Fail("leading zero in number");
		} else {
			RequireDigits();
		}
		if (Consume('.'))
			RequireDigits();
		if (Consume('e') || Consume('E')) {
			if (!Consume('+'))
				Consume('-');
			RequireDigits();
		}

		const std::string token = text.substr(start, position - start);
		char *end = nullptr;
		const double value = std::strtod(token.c_str(), &end);
		if (end != token.c_str() + token.size() || !std::isfinite(value))
			Fail("invalid number");
		return Json(value);
	}

	std::string ParseString()
	{
		Require('"');
		std::string output;
		while (position < text.size()) {
			const unsigned char ch = static_cast<unsigned char>(text[position++]);
			if (ch == '"')
				return output;
			if (ch == '\\') {
				ParseEscape(output);
				continue;
			}
			if (ch < 0x20)
				Fail("control character in string");
			if (ch < 0x80)
				output.push_back(static_cast<char>(ch));
			else
				ParseUtf8Sequence(output, ch);
		}
		Fail("unterminated string");
	}

	void ParseUtf8Sequence(std::string &output, const unsigned char first)
	{
		size_t continuation_count = 0;
		uint32_t codepoint = 0;
		uint32_t minimum = 0;
		if ((first & 0xe0) == 0xc0) {
			continuation_count = 1;
			codepoint = first & 0x1f;
			minimum = 0x80;
		} else if ((first & 0xf0) == 0xe0) {
			continuation_count = 2;
			codepoint = first & 0x0f;
			minimum = 0x800;
		} else if ((first & 0xf8) == 0xf0) {
			continuation_count = 3;
			codepoint = first & 0x07;
			minimum = 0x10000;
		} else {
			Fail("invalid UTF-8 in string");
		}
		if (position + continuation_count > text.size())
			Fail("incomplete UTF-8 in string");
		std::string bytes(1, static_cast<char>(first));
		for (size_t i = 0; i < continuation_count; i++) {
			const unsigned char next =
			        static_cast<unsigned char>(text[position++]);
			if ((next & 0xc0) != 0x80)
				Fail("invalid UTF-8 continuation byte");
			bytes.push_back(static_cast<char>(next));
			codepoint = (codepoint << 6) | (next & 0x3f);
		}
		if (codepoint < minimum || codepoint > 0x10ffff ||
		    (codepoint >= 0xd800 && codepoint <= 0xdfff))
			Fail("invalid UTF-8 code point");
		output += bytes;
	}

	void ParseEscape(std::string &output)
	{
		if (position == text.size())
			Fail("unterminated escape");
		switch (text[position++]) {
		case '"': output.push_back('"'); break;
		case '\\': output.push_back('\\'); break;
		case '/': output.push_back('/'); break;
		case 'b': output.push_back('\b'); break;
		case 'f': output.push_back('\f'); break;
		case 'n': output.push_back('\n'); break;
		case 'r': output.push_back('\r'); break;
		case 't': output.push_back('\t'); break;
		case 'u': ParseUnicodeEscape(output); break;
		default: Fail("invalid escape sequence");
		}
	}

	void ParseUnicodeEscape(std::string &output)
	{
		uint32_t codepoint = ParseHexCodeUnit();
		if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
			if (position + 2 > text.size() || text[position] != '\\' ||
			    text[position + 1] != 'u')
				Fail("missing low surrogate");
			position += 2;
			const uint32_t low = ParseHexCodeUnit();
			if (low < 0xdc00 || low > 0xdfff)
				Fail("invalid low surrogate");
			codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
			            (low - 0xdc00);
		} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
			Fail("unexpected low surrogate");
		}
		AppendUtf8(output, codepoint);
	}

	uint32_t ParseHexCodeUnit()
	{
		if (position + 4 > text.size())
			Fail("incomplete unicode escape");
		uint32_t value = 0;
		for (size_t i = 0; i < 4; i++) {
			value = (value << 4) | HexValue(text[position++]);
		}
		return value;
	}

	static uint32_t HexValue(const char ch)
	{
		if (ch >= '0' && ch <= '9') return static_cast<uint32_t>(ch - '0');
		if (ch >= 'a' && ch <= 'f') return static_cast<uint32_t>(ch - 'a' + 10);
		if (ch >= 'A' && ch <= 'F') return static_cast<uint32_t>(ch - 'A' + 10);
		throw std::runtime_error("invalid hexadecimal digit in unicode escape");
	}

	void RequireDigits()
	{
		if (position == text.size() || !IsDigit(text[position]))
			Fail("number requires a digit");
		while (position < text.size() && IsDigit(text[position]))
			position++;
	}

	static bool IsDigit(const char ch)
	{
		return ch >= '0' && ch <= '9';
	}

	bool Consume(const char expected)
	{
		if (position == text.size() || text[position] != expected)
			return false;
		position++;
		return true;
	}

	void Require(const char expected)
	{
		if (!Consume(expected))
			Fail(std::string("expected '") + expected + "'");
	}

	void SkipWhitespace()
	{
		while (position < text.size()) {
			const char ch = text[position];
			if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
				break;
			position++;
		}
	}

	[[noreturn]] void Fail(const std::string &message) const
	{
		throw std::runtime_error("JSON parse error at byte " +
		                         std::to_string(position) + ": " + message);
	}

	const std::string &text;
	size_t position = 0;
};

void DumpString(std::ostringstream &output, const std::string &value)
{
	output << '"';
	for (const unsigned char ch : value) {
		switch (ch) {
		case '"': output << "\\\""; break;
		case '\\': output << "\\\\"; break;
		case '\b': output << "\\b"; break;
		case '\f': output << "\\f"; break;
		case '\n': output << "\\n"; break;
		case '\r': output << "\\r"; break;
		case '\t': output << "\\t"; break;
		default:
			if (ch < 0x20) {
				output << "\\u" << std::hex << std::setw(4)
				       << std::setfill('0') << static_cast<unsigned>(ch)
				       << std::dec << std::setfill(' ');
			} else {
				output << static_cast<char>(ch);
			}
		}
	}
	output << '"';
}

void DumpValue(std::ostringstream &output, const Json &value)
{
	if (value.IsNull()) {
		output << "null";
	} else if (value.IsBool()) {
		output << (value.AsBool() ? "true" : "false");
	} else if (value.IsNumber()) {
		output << std::setprecision(std::numeric_limits<double>::max_digits10)
		       << value.AsNumber();
	} else if (value.IsString()) {
		DumpString(output, value.AsString());
	} else if (value.IsArray()) {
		output << '[';
		bool first = true;
		for (const auto &entry : value.AsArray()) {
			if (!first) output << ',';
			first = false;
			DumpValue(output, entry);
		}
		output << ']';
	} else {
		output << '{';
		bool first = true;
		for (const auto &entry : value.AsObject()) {
			if (!first) output << ',';
			first = false;
			DumpString(output, entry.first);
			output << ':';
			DumpValue(output, entry.second);
		}
		output << '}';
	}
}

} // namespace

Json::Json() : value(nullptr) {}
Json::Json(std::nullptr_t) : value(nullptr) {}
Json::Json(const bool input) : value(input) {}
Json::Json(const double input) : value(input) {}
Json::Json(const int input) : value(static_cast<double>(input)) {}
Json::Json(const char *input) : value(std::string(input)) {}
Json::Json(std::string input) : value(std::move(input)) {}
Json::Json(Array input) : value(std::move(input)) {}
Json::Json(Object input) : value(std::move(input)) {}

Json Json::Parse(const std::string &text)
{
	return Parser(text).ParseDocument();
}

std::string Json::Dump() const
{
	std::ostringstream output;
	DumpValue(output, *this);
	return output.str();
}

bool Json::IsNull() const { return std::holds_alternative<std::nullptr_t>(value); }
bool Json::IsBool() const { return std::holds_alternative<bool>(value); }
bool Json::IsNumber() const { return std::holds_alternative<double>(value); }
bool Json::IsString() const { return std::holds_alternative<std::string>(value); }
bool Json::IsArray() const { return std::holds_alternative<Array>(value); }
bool Json::IsObject() const { return std::holds_alternative<Object>(value); }

bool Json::AsBool() const { return std::get<bool>(value); }
double Json::AsNumber() const { return std::get<double>(value); }
const std::string &Json::AsString() const { return std::get<std::string>(value); }
const Json::Array &Json::AsArray() const { return std::get<Array>(value); }
const Json::Object &Json::AsObject() const { return std::get<Object>(value); }
Json::Array &Json::AsArray() { return std::get<Array>(value); }
Json::Object &Json::AsObject() { return std::get<Object>(value); }

const Json *Json::Find(const std::string &key) const
{
	if (!IsObject()) return nullptr;
	const auto found = AsObject().find(key);
	return found == AsObject().end() ? nullptr : &found->second;
}

Json *Json::Find(const std::string &key)
{
	if (!IsObject()) return nullptr;
	const auto found = AsObject().find(key);
	return found == AsObject().end() ? nullptr : &found->second;
}

} // namespace Agent86
