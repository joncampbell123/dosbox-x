// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "session_logger.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Agent86 {
namespace {

constexpr size_t MaxLogFieldBytes = 4096;

std::string UtcTimestamp()
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t value = std::chrono::system_clock::to_time_t(now);
	std::tm utc = {};
#if defined(_WIN32)
	gmtime_s(&utc, &value);
#else
	gmtime_r(&value, &utc);
#endif
	std::ostringstream output;
	output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
	return output.str();
}

} // namespace

SessionLogger::SessionLogger() : path(DefaultPath()) {}

SessionLogger::SessionLogger(std::filesystem::path requested_path)
        : path(std::move(requested_path))
{}

void SessionLogger::Record(const std::string &tool,
                           const std::string &summary,
                           const std::string &outcome)
{
	const std::string line = UtcTimestamp() + " " + SanitizeField(tool) + " " +
	                         SanitizeField(outcome) + " " + SanitizeField(summary) +
	                         "\n";
	std::lock_guard<std::mutex> lock(mutex);
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	if (error)
		throw std::runtime_error("cannot create DOS bridge log directory: " +
		                         error.message());
	std::ofstream output(path, std::ios::app | std::ios::binary);
	if (!output)
		throw std::runtime_error("cannot open DOS bridge session log");
	output << line;
	if (!output)
		throw std::runtime_error("cannot append DOS bridge session log");
}

const std::filesystem::path &SessionLogger::Path() const
{
	return path;
}

std::filesystem::path SessionLogger::DefaultPath()
{
	if (const char *override_path = std::getenv("DOS_BRIDGE_SESSION_LOG")) {
		if (*override_path)
			return std::filesystem::u8path(override_path);
	}
#if defined(_WIN32)
	if (const char *profile = std::getenv("USERPROFILE")) {
		if (*profile)
			return std::filesystem::u8path(profile) / ".dos-bridge" /
		       "session.log";
	}
	const char *drive = std::getenv("HOMEDRIVE");
	const char *relative_path = std::getenv("HOMEPATH");
	if (drive && *drive && relative_path && *relative_path)
		return std::filesystem::u8path(std::string(drive) + relative_path) /
		       ".dos-bridge" / "session.log";
#else
	const char *home = std::getenv("HOME");
	if (home && *home)
		return std::filesystem::u8path(home) / ".dos-bridge" / "session.log";
#endif
	throw std::runtime_error("cannot determine home directory for session log");
}

std::string SessionLogger::SanitizeField(const std::string &field)
{
	std::string output;
	output.reserve(std::min(field.size(), MaxLogFieldBytes));
	for (const unsigned char ch : field) {
		if (output.size() >= MaxLogFieldBytes)
			break;
		if (ch == '\r') output += "\\r";
		else if (ch == '\n') output += "\\n";
		else if (ch == '\t') output += "\\t";
		else if (ch < 0x20 || ch == 0x7f) output += '?';
		else output.push_back(static_cast<char>(ch));
	}
	return output;
}

} // namespace Agent86
