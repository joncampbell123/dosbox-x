#include "debug_mcp.h"

#include "config.h"

#if defined(C_DEBUG) && C_DEBUG && \
        ((defined(C_SDL2_NET) && C_SDL2_NET) || (defined(C_SDL_NET) && C_SDL_NET))

#include "debug.h"

#if defined(C_SDL2_NET) && C_SDL2_NET
#include <SDL2/SDL_net.h>
#else
#include <SDL_net.h>
#endif

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* CONTROL_HOST = "127.0.0.1";

constexpr int SOCKET_POLL_TIMEOUT_MS = 50;
constexpr int RECONNECT_DELAY_MS = 1000;

constexpr size_t MAX_INCOMING_MESSAGES = 1024;
constexpr size_t MAX_OUTGOING_MESSAGES = 1024;
constexpr size_t MAX_CAPTURE_LINES = 16384;

// Protect against a peer continuously sending data without '\n'.
constexpr size_t MAX_RECEIVE_BUFFER = 1024 * 1024;

std::atomic<bool> g_running{false};
std::atomic<bool> g_connected{false};

uint16_t g_port = 0;

std::thread g_thread;

std::mutex g_incoming_mutex;
std::deque<std::string> g_incoming;

std::mutex g_outgoing_mutex;
std::deque<std::string> g_outgoing;

std::mutex g_capture_mutex;
bool g_capture_active = false;
size_t g_capture_discarded = 0;
std::vector<std::string> g_capture_lines;

//
// All socket operations happen ONLY in the I/O thread.
//
// This is important because SDLNet_TCP_Send/Recv/Close don't
// have to be synchronized with the DOSBox-X emulation thread.
//

TCPsocket g_socket = nullptr;
SDLNet_SocketSet g_socket_set = nullptr;

std::string g_receive_buffer;

struct ControlRequest {
    std::string id;
    std::string command;
    std::string payload;
};

bool IsSpace(const char c)
{
    return c == ' ' || c == '\t';
}

std::string TrimLeft(std::string value)
{
    while (!value.empty() && IsSpace(value.front()))
        value.erase(value.begin());

    return value;
}

std::string UpperAscii(std::string value)
{
    for (auto& c : value) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - ('a' - 'A'));
    }

    return value;
}

std::string SanitizeProtocolLine(std::string line)
{
    for (auto& c : line) {
        if (c == '\r' || c == '\n')
            c = ' ';
    }

    return line;
}

std::string FirstWordUpper(const std::string& value)
{
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos)
        return {};

    const auto end = value.find_first_of(" \t", begin);
    if (end == std::string::npos)
        return UpperAscii(value.substr(begin));

    return UpperAscii(value.substr(begin, end - begin));
}

bool IsResumeDebuggerCommand(const std::string& command)
{
    const auto word = FirstWordUpper(command);
    return word == "RUN" || word == "RUNWATCH" || word == "VRT";
}

void BeginCapture()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    g_capture_lines.clear();
    g_capture_discarded = 0;
    g_capture_active = true;
}

std::vector<std::string> EndCapture()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    g_capture_active = false;

    std::vector<std::string> lines;
    lines.swap(g_capture_lines);

    if (g_capture_discarded > 0) {
        char message[128];
        std::snprintf(message,
                      sizeof(message),
                      "[%lu debug output lines discarded]",
                      static_cast<unsigned long>(g_capture_discarded));
        lines.emplace_back(message);
        g_capture_discarded = 0;
    }

    return lines;
}

void CloseConnection()
{
    g_connected.store(false);

    if (g_socket) {
        if (g_socket_set)
            SDLNet_TCP_DelSocket(g_socket_set, g_socket);

        SDLNet_TCP_Close(g_socket);
        g_socket = nullptr;
    }

    if (g_socket_set) {
        SDLNet_FreeSocketSet(g_socket_set);
        g_socket_set = nullptr;
    }

    g_receive_buffer.clear();
}

bool Connect()
{
    IPaddress address = {};

    if (SDLNet_ResolveHost(&address, CONTROL_HOST, g_port) < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_ResolveHost failed: %s\n",
                     SDLNet_GetError());

        return false;
    }

    std::fprintf(stderr,
                 "[ControlClient] connecting to %s:%u...\n",
                 CONTROL_HOST,
                 static_cast<unsigned>(g_port));

    TCPsocket socket = SDLNet_TCP_Open(&address);

    if (!socket)
        return false;

    SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(1);

    if (!socket_set) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_AllocSocketSet failed: %s\n",
                     SDLNet_GetError());

        SDLNet_TCP_Close(socket);
        return false;
    }

    if (SDLNet_TCP_AddSocket(socket_set, socket) < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_TCP_AddSocket failed: %s\n",
                     SDLNet_GetError());

        SDLNet_FreeSocketSet(socket_set);
        SDLNet_TCP_Close(socket);

        return false;
    }

    g_socket = socket;
    g_socket_set = socket_set;

    g_receive_buffer.clear();

    g_connected.store(true);

    std::fprintf(stderr,
                 "[ControlClient] connected to %s:%u\n",
                 CONTROL_HOST,
                 static_cast<unsigned>(g_port));

    return true;
}

bool SendAll(const std::string& data)
{
    if (!g_socket)
        return false;

    const char* ptr = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        const int chunk =
                remaining > static_cast<size_t>(INT_MAX)
                        ? INT_MAX
                        : static_cast<int>(remaining);

        const int sent = SDLNet_TCP_Send(
                g_socket,
                ptr,
                chunk);

        if (sent <= 0)
            return false;

        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

void QueueIncoming(std::string message)
{
    std::lock_guard<std::mutex> lock(g_incoming_mutex);

    if (g_incoming.size() >= MAX_INCOMING_MESSAGES)
        g_incoming.pop_front();

    g_incoming.emplace_back(std::move(message));
}

bool PopIncoming(std::string& message)
{
    std::lock_guard<std::mutex> lock(g_incoming_mutex);

    if (g_incoming.empty())
        return false;

    message = std::move(g_incoming.front());
    g_incoming.pop_front();
    return true;
}

void ProcessReceiveBuffer()
{
    for (;;) {
        const auto newline = g_receive_buffer.find('\n');

        if (newline == std::string::npos)
            break;

        std::string line =
                g_receive_buffer.substr(0, newline);

        g_receive_buffer.erase(0, newline + 1);

        // Support both "\n" and "\r\n".
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        QueueIncoming(std::move(line));
    }
}

bool Receive()
{
    char buffer[8192];

    const int received =
            SDLNet_TCP_Recv(
                    g_socket,
                    buffer,
                    sizeof(buffer));

    if (received <= 0)
        return false;

    g_receive_buffer.append(
            buffer,
            static_cast<size_t>(received));

    if (g_receive_buffer.size() > MAX_RECEIVE_BUFFER) {
        std::fprintf(stderr,
                     "[ControlClient] receive buffer overflow\n");

        return false;
    }

    ProcessReceiveBuffer();

    return true;
}

bool FlushOutgoing()
{
    for (;;) {
        std::string message;

        {
            std::lock_guard<std::mutex> lock(g_outgoing_mutex);

            if (g_outgoing.empty())
                return true;

            message = g_outgoing.front();
        }

        if (!SendAll(message))
            return false;

        {
            std::lock_guard<std::mutex> lock(g_outgoing_mutex);

            if (!g_outgoing.empty())
                g_outgoing.pop_front();
        }
    }
}

void SleepReconnectDelay()
{
    //
    // Do it in short steps so ControlServer_Stop()
    // doesn't have to wait a full second.
    //

    constexpr int step_ms = 50;

    for (int elapsed = 0;
         elapsed < RECONNECT_DELAY_MS && g_running.load();
         elapsed += step_ms) {

        std::this_thread::sleep_for(
                std::chrono::milliseconds(step_ms));
    }
}

void ThreadMain()
{
    while (g_running.load()) {

        //
        // Connect / reconnect
        //

        if (!g_socket) {
            if (!Connect()) {
                SleepReconnectDelay();
                continue;
            }
        }

        //
        // Send everything currently queued.
        //

        if (!FlushOutgoing()) {
            std::fprintf(stderr,
                         "[ControlClient] connection lost while sending\n");

            CloseConnection();
            continue;
        }

        //
        // Wait for incoming data.
        //

        const int ready =
                SDLNet_CheckSockets(
                        g_socket_set,
                        SOCKET_POLL_TIMEOUT_MS);

        if (ready < 0) {
            std::fprintf(stderr,
                         "[ControlClient] SDLNet_CheckSockets failed: %s\n",
                         SDLNet_GetError());

            CloseConnection();
            continue;
        }

        if (ready == 0)
            continue;

        if (g_socket && SDLNet_SocketReady(g_socket)) {
            if (!Receive()) {
                std::fprintf(stderr,
                             "[ControlClient] connection closed\n");

                CloseConnection();
                continue;
            }
        }
    }

    CloseConnection();
}

bool ParseControlRequest(const std::string& line,
                         ControlRequest& request,
                         std::string& error)
{
    if (line.compare(0, 4, "REQ ") != 0) {
        error = "expected REQ <id> <PING|BREAK|EXEC>";
        return false;
    }

    const auto id_begin = line.find_first_not_of(" \t", 4);
    if (id_begin == std::string::npos) {
        error = "missing request id";
        return false;
    }

    const auto id_end = line.find_first_of(" \t", id_begin);
    if (id_end == std::string::npos) {
        error = "missing request command";
        return false;
    }

    request.id = line.substr(id_begin, id_end - id_begin);

    const auto command_begin = line.find_first_not_of(" \t", id_end);
    if (command_begin == std::string::npos) {
        error = "missing request command";
        return false;
    }

    const auto command_end = line.find_first_of(" \t", command_begin);
    if (command_end == std::string::npos) {
        request.command = UpperAscii(line.substr(command_begin));
        request.payload.clear();
    } else {
        request.command = UpperAscii(
                line.substr(command_begin, command_end - command_begin));
        request.payload = TrimLeft(line.substr(command_end));
    }

    if (request.command != "PING" &&
            request.command != "BREAK" &&
            request.command != "EXEC") {
        error = "unknown request command";
        return false;
    }

    if (request.command == "EXEC" && request.payload.empty()) {
        error = "missing debugger command";
        return false;
    }

    return true;
}

void SendResponse(const std::string& id,
                  const bool ok,
                  const std::vector<std::string>& lines)
{
    std::string response = std::string("BEGIN ") + id + (ok ? " OK" : " ERR");

    for (const auto& line : lines) {
        response.push_back('\n');
        response += SanitizeProtocolLine(line);
    }

    response += "\nEND ";
    response += id;

    ControlServer_Send(std::move(response));
}

void SendErrorResponse(const std::string& id, const std::string& error)
{
    SendResponse(id.empty() ? "0" : id, false, {error});
}

bool OutgoingQueueEmpty()
{
    std::lock_guard<std::mutex> lock(g_outgoing_mutex);
    return g_outgoing.empty();
}

void WaitForOutgoingDrain()
{
    constexpr int max_wait_ms = 250;
    constexpr int sleep_step_ms = 5;

    for (int elapsed_ms = 0;
         elapsed_ms < max_wait_ms && g_running.load();
         elapsed_ms += sleep_step_ms) {
        if (OutgoingQueueEmpty() || !g_connected.load())
            return;

        std::this_thread::sleep_for(
                std::chrono::milliseconds(sleep_step_ms));
    }
}

void ProcessControlCommand(const std::string& line)
{
    ControlRequest request;
    std::string error;

    if (!ParseControlRequest(line, request, error)) {
        SendErrorResponse(request.id, error);
        return;
    }

    if (request.command == "PING") {
        SendResponse(request.id, true, {"PONG"});
        return;
    }

    if (request.command == "EXEC" &&
            IsResumeDebuggerCommand(request.payload)) {
        SendResponse(request.id, true, {"OK"});
        WaitForOutgoingDrain();
        DEBUG_ExecuteCommand(request.payload.c_str());
        return;
    }

    BeginCapture();

    bool ok = true;
    if (request.command == "BREAK") {
        DEBUG_EnableDebugger();
    } else {
        ok = DEBUG_ExecuteCommand(request.payload.c_str());
    }

    auto output = EndCapture();
    SendResponse(request.id, ok, output);
}

} // namespace

void ControlServer_Start(const uint16_t port)
{
    if (port == 0)
        return;

    if (g_running.load())
        return;

    if (SDLNet_Init() < 0) {
        std::fprintf(stderr,
                     "[ControlClient] SDLNet_Init failed: %s\n",
                     SDLNet_GetError());

        return;
    }

    g_port = port;

    {
        std::lock_guard<std::mutex> lock(g_incoming_mutex);
        g_incoming.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_outgoing_mutex);
        g_outgoing.clear();
    }

    g_running.store(true);

    g_thread = std::thread(ThreadMain);
}

void ControlServer_Stop()
{
    if (!g_running.exchange(false))
        return;

    if (g_thread.joinable())
        g_thread.join();

    g_connected.store(false);
    g_port = 0;

    {
        std::lock_guard<std::mutex> lock(g_incoming_mutex);
        g_incoming.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_outgoing_mutex);
        g_outgoing.clear();
    }

    EndCapture();
}

bool ControlServer_IsConnected()
{
    return g_connected.load();
}

void ControlServer_Send(std::string message)
{
    if (!g_running.load())
        return;

    message.push_back('\n');

    std::lock_guard<std::mutex> lock(g_outgoing_mutex);

    if (g_outgoing.size() >= MAX_OUTGOING_MESSAGES)
        g_outgoing.pop_front();

    g_outgoing.emplace_back(std::move(message));
}

void ControlServer_SendEvent(
        const std::string& event,
        const std::string& data)
{
    ControlServer_Send(std::string("BEGIN event OK\n") +
                       SanitizeProtocolLine(event) + " " +
                       SanitizeProtocolLine(data) +
                       "\nEND event");
}

void ControlServer_Poll()
{
    std::string command;
    while (PopIncoming(command)) {
        std::fprintf(stderr,
                     "[ControlClient] mcp command %s\n",
                     command.c_str());

        ProcessControlCommand(command);
    }
}

bool DEBUG_MCP_IsCapturingOutput()
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);
    return g_capture_active;
}

void DEBUG_MCP_CaptureMessage(const char* message)
{
    std::lock_guard<std::mutex> lock(g_capture_mutex);

    if (!g_capture_active)
        return;

    if (g_capture_lines.size() >= MAX_CAPTURE_LINES) {
        ++g_capture_discarded;
        return;
    }

    g_capture_lines.emplace_back(message ? message : "");
}

#else

void ControlServer_Start(uint16_t) {}
void ControlServer_Stop() {}
bool ControlServer_IsConnected() { return false; }
void ControlServer_Send(std::string) {}
void ControlServer_SendEvent(const std::string&, const std::string&) {}
void ControlServer_Poll() {}
bool DEBUG_MCP_IsCapturingOutput() { return false; }
void DEBUG_MCP_CaptureMessage(const char*) {}

#endif
