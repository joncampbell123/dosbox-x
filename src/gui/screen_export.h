// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace ScreenExport {

enum class Format { Text, Markdown };

inline std::string NormalizeLineEndings(const std::string &text)
{
	std::string normalized;
	normalized.reserve(text.size());
	for (size_t index = 0; index < text.size(); ++index) {
		if (text[index] == '\r') {
			if (index + 1 < text.size() && text[index + 1] == '\n')
				continue;
			normalized.push_back('\n');
		} else {
			normalized.push_back(text[index]);
		}
	}
	return normalized;
}

inline bool FormatFromPath(const std::string &path, Format &format)
{
	const size_t separator = path.find_last_of("/\\");
	const size_t dot = path.find_last_of('.');
	if (dot == std::string::npos ||
	    (separator != std::string::npos && dot < separator))
		return false;

	std::string extension = path.substr(dot);
	std::transform(extension.begin(),
	               extension.end(),
	               extension.begin(),
	               [](const unsigned char value) {
		               return static_cast<char>(std::tolower(value));
	               });
	if (extension == ".txt") {
		format = Format::Text;
		return true;
	}
	if (extension == ".md") {
		format = Format::Markdown;
		return true;
	}
	return false;
}

inline size_t LongestBacktickRun(const std::string &text)
{
	size_t longest = 0;
	size_t current = 0;
	for (const char value : text) {
		current = value == '`' ? current + 1 : 0;
		longest = std::max(longest, current);
	}
	return longest;
}

inline std::string BuildDocument(const std::string &utf8_text,
	                             const Format format)
{
	std::string text = NormalizeLineEndings(utf8_text);
	if (format == Format::Text) {
		if (!text.empty() && text.back() != '\n')
			text.push_back('\n');
		return text;
	}

	const std::string fence(std::max<size_t>(3, LongestBacktickRun(text) + 1),
	                        '`');
	std::string markdown = "# DOS text screen\n\n" + fence + "text\n" + text;
	if (!text.empty() && text.back() != '\n')
		markdown.push_back('\n');
	markdown += fence + "\n";
	return markdown;
}

} // namespace ScreenExport
