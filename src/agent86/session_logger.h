// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace Agent86 {

class SessionLogger {
public:
	SessionLogger();
	explicit SessionLogger(std::filesystem::path path);

	void Record(const std::string &tool,
	            const std::string &summary,
	            const std::string &outcome);
	const std::filesystem::path &Path() const;

private:
	static std::filesystem::path DefaultPath();
	static std::string SanitizeField(const std::string &field);

	std::filesystem::path path;
	std::mutex mutex;
};

} // namespace Agent86
