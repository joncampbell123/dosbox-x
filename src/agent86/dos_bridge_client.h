// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace Agent86 {

class BridgeError final : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class DosBridgeClient {
public:
	static constexpr uint16_t DefaultPort = 8090;
	static constexpr size_t DefaultMaxOutputBytes = 1024 * 1024;

	explicit DosBridgeClient(uint16_t port = ConfiguredPort(),
	                         size_t max_output_bytes = DefaultMaxOutputBytes);
	~DosBridgeClient();

	DosBridgeClient(const DosBridgeClient &) = delete;
	DosBridgeClient &operator=(const DosBridgeClient &) = delete;

	std::string RunCommand(
	        const std::string &command,
	        std::chrono::milliseconds timeout = std::chrono::seconds(60));
	std::string WriteFile(
	        const std::string &filename,
	        const std::string &content,
	        std::chrono::milliseconds timeout = std::chrono::seconds(30));
	std::string ReleaseConsole(
	        std::chrono::milliseconds timeout = std::chrono::seconds(3));
	std::string RunControl(
	        const std::string &operation,
	        const std::string &payload = {},
	        std::chrono::milliseconds timeout = std::chrono::seconds(10));
	void Close();

	static uint16_t ConfiguredPort();

private:
	class Socket;

	std::string Exchange(const std::string &payload,
	                     const std::string &end_marker,
	                     std::chrono::milliseconds timeout);
	Socket &EnsureConnected();
	void CloseUnlocked();

	const uint16_t port;
	const size_t max_output_bytes;
	std::unique_ptr<Socket> socket;
	std::mutex mutex;
};

} // namespace Agent86
