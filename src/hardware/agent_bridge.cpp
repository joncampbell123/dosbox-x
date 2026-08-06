/*
 *  Agent Telemetry & Remote Execution Subsystem (A-TRES) for DOSBox-X
 *  Copyright (C) 2026 Michael P. Burgus (https://github.com/NeuralDrifter)
 *  Cross-platform C++ Subsystem implementation
 */

#include "agent_bridge.h"
#include "logging.h"
#include "control.h"

#include <iostream>
#include <sstream>
#include <cstring>

#if defined(_WIN32) || defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define CLOSESOCKET(s) close(s)
#endif

AgentBridge& AgentBridge::GetInstance() {
    static AgentBridge instance;
    return instance;
}

AgentBridge::AgentBridge()
    : enabled(false), port(8090), token("dosbox-agent-secret"),
      running(false), last_exit_code(0), command_in_progress(false) {}

AgentBridge::~AgentBridge() {
    Shutdown();
}

void AgentBridge::Initialize(uint16_t in_port, const std::string& in_token) {
    if (running) return;

    port = in_port;
    if (!in_token.empty()) {
        token = in_token;
    }

#if defined(_WIN32) || defined(WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    enabled = true;
    running = true;
    server_thread = std::thread(&AgentBridge::ServerLoop, this);
    
    LOG_MSG("A-TRES: Agent Telemetry Subsystem listening on 127.0.0.1:%d", port);
}

void AgentBridge::Shutdown() {
    if (!running) return;

    running = false;
    if (server_thread.joinable()) {
        server_thread.join();
    }

#if defined(_WIN32) || defined(WIN32)
    WSACleanup();
#endif

    enabled = false;
    LOG_MSG("A-TRES: Subsystem shut down.");
}

void AgentBridge::StreamOutputBytes(const char* data, size_t length) {
    std::lock_guard<std::mutex> lock(output_mutex);
    current_output_buffer.append(data, length);
}

void AgentBridge::OnCommandCompleted(int exit_code) {
    last_exit_code = exit_code;
    command_in_progress = false;
}

bool AgentBridge::QueueCommand(const std::string& command, std::string& out_response_json) {
    std::lock_guard<std::mutex> lock(command_mutex);
    pending_commands.push(command);
    command_in_progress = true;
    
    out_response_json = "{\"jsonrpc\":\"2.0\",\"result\":{\"status\":\"queued\",\"command\":\"" + command + "\"},\"id\":1}";
    return true;
}

void AgentBridge::ServerLoop() {
    socket_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == INVALID_SOCKET) {
        LOG_MSG("A-TRES Error: Could not create TCP socket.");
        return;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Strictly local loopback security boundary
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        LOG_MSG("A-TRES Error: Bind failed on port %d", port);
        CLOSESOCKET(listen_fd);
        return;
    }

    if (listen(listen_fd, 5) == SOCKET_ERROR) {
        LOG_MSG("A-TRES Error: Listen failed.");
        CLOSESOCKET(listen_fd);
        return;
    }

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select((int)listen_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (activity > 0 && FD_ISSET(listen_fd, &read_fds)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            socket_t client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);

            if (client_fd != INVALID_SOCKET) {
                ProcessClient((int)client_fd);
                CLOSESOCKET(client_fd);
            }
        }
    }

    CLOSESOCKET(listen_fd);
}

void AgentBridge::ProcessClient(int client_fd) {
    char buffer[2048];
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) return;

    buffer[bytes_read] = '\0';
    std::string request_str(buffer);

    // Simple Token check
    if (request_str.find(token) == std::string::npos && !token.empty()) {
        std::string err_resp = "HTTP/1.1 401 Unauthorized\r\n\r\n{\"error\":\"Invalid Bearer Token\"}";
        send(client_fd, err_resp.c_str(), (int)err_resp.length(), 0);
        return;
    }

    std::string response_payload = HandleJsonRpcRequest(request_str);
    std::string http_resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n" + response_payload;
    send(client_fd, http_resp.c_str(), (int)http_resp.length(), 0);
}

std::string AgentBridge::HandleJsonRpcRequest(const std::string& request_str) {
    // Health / Status ping check
    if (request_str.find("\"method\":\"get_status\"") != std::string::npos) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::ostringstream ss;
        ss << "{\"jsonrpc\":\"2.0\",\"result\":{\"status\":\"online\",\"command_in_progress\":"
           << (command_in_progress ? "true" : "false")
           << ",\"last_exit_code\":" << last_exit_code
           << ",\"output_length\":" << current_output_buffer.length() << "},\"id\":1}";
        return ss.str();
    }

    // Flush output buffer
    if (request_str.find("\"method\":\"read_output\"") != std::string::npos) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::string out = current_output_buffer;
        current_output_buffer.clear();
        
        // Escape newlines & quotes simple sanitize
        std::string escaped;
        for (char c : out) {
            if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }

        return "{\"jsonrpc\":\"2.0\",\"result\":{\"output\":\"" + escaped + "\"},\"id\":1}";
    }

    return "{\"jsonrpc\":\"2.0\",\"result\":{\"ready\":true,\"subsystem\":\"A-TRES\"},\"id\":1}";
}

void AGENT_BRIDGE_Init() {
    // Initialize Agent Subsystem with default port 8090
    AgentBridge::GetInstance().Initialize(8090, "dosbox-agent-secret");
}

void AGENT_BRIDGE_StreamOutput(const char* data, size_t len) {
    if (AgentBridge::GetInstance().IsEnabled()) {
        AgentBridge::GetInstance().StreamOutputBytes(data, len);
    }
}
