#ifndef DOSBOX_X_DEBUG_MCP_H
#define DOSBOX_X_DEBUG_MCP_H
#pragma once

#include <cstdint>
#include <string>

//
// Start connection to the external control server.
//
// port == 0 means disabled.
//
// DOSBox-X acts as a TCP client and connects to:
//
//     127.0.0.1:<port>
//
// The connection is maintained in a background thread.
// If the server is unavailable or the connection is lost,
// reconnection is attempted automatically.
//
void ControlServer_Start(uint16_t port);

//
// Stop background thread and close the connection.
//
void ControlServer_Stop();

//
// Returns true if currently connected.
//
bool ControlServer_IsConnected();

// Queue a line-oriented response for the control connection.
void ControlServer_Send(std::string message);

// Convenience function for asynchronous events.
void ControlServer_SendEvent(
        const std::string& event,
        const std::string& data);

//
// Retrieve and process all MCP commands
//
// IMPORTANT:
// This function should be called from the DOSBox-X
// main/emulation thread.
//
//
void ControlServer_Poll();

// Execute one existing debugger command through ParseCommand().
bool DEBUG_ExecuteCommand(const char* command);

// Capture hook used by DEBUG_ShowMsg while a control request is running.
bool DEBUG_MCP_IsCapturingOutput();
void DEBUG_MCP_CaptureMessage(const char* message);

#endif //DOSBOX_X_DEBUG_MCP_H
