/*
 *  Agent Telemetry & Remote Execution Subsystem (A-TRES) for DOSBox-X
 *  Copyright (C) 2026 Michael P. Burgus (https://github.com/NeuralDrifter)
 *  Cross-platform C++ Subsystem exposing secure JSON-RPC / MCP Agent Control
 */

#ifndef DOSBOX_AGENT_BRIDGE_H
#define DOSBOX_AGENT_BRIDGE_H

#include "dosbox.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>

class AgentBridge {
public:
    static AgentBridge& GetInstance();

    // Lifecycle
    void Initialize(uint16_t port, const std::string& auth_token);
    void Shutdown();
    bool IsEnabled() const { return enabled; }

    // Execution interface
    bool QueueCommand(const std::string& command, std::string& out_response_json);
    void StreamOutputBytes(const char* data, size_t length);

    // Shell state callback
    void OnCommandCompleted(int exit_code);

    // Configuration / Info
    uint16_t GetPort() const { return port; }
    std::string GetToken() const { return token; }

private:
    AgentBridge();
    ~AgentBridge();

    void ServerLoop();
    void ProcessClient(int client_fd);
    std::string HandleJsonRpcRequest(const std::string& json_payload);

    bool enabled;
    uint16_t port;
    std::string token;
    
    std::atomic<bool> running;
    std::thread server_thread;

    std::mutex output_mutex;
    std::string current_output_buffer;

    std::mutex command_mutex;
    std::queue<std::string> pending_commands;
    
    int last_exit_code;
    bool command_in_progress;
};

// Section initialization function for DOSBox-X setup
void AGENT_BRIDGE_Init();

#endif // DOSBOX_AGENT_BRIDGE_H
