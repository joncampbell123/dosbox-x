#ifndef DOSBOX_AGENT_BRIDGE_H
#define DOSBOX_AGENT_BRIDGE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace dosbox_agent {

class EmulationThreadQueue {
public:
    typedef std::function<void(std::uint64_t)> Command;

    EmulationThreadQueue();
    ~EmulationThreadQueue();

    EmulationThreadQueue(const EmulationThreadQueue&) = delete;
    EmulationThreadQueue& operator=(const EmulationThreadQueue&) = delete;

    bool BindToCurrentThread();
    bool IsBoundToCurrentThread() const;
    std::uint64_t Submit(Command command);
    std::uint64_t SubmitAfter(std::uint32_t delay_ms, Command command);
    std::size_t Pump();
    void Shutdown();

private:
    class Impl;
    Impl* impl;
};

typedef std::function<void(std::uint16_t segment, std::uint32_t instruction_pointer)> DebuggerStopListener;

EmulationThreadQueue& AGENT_EmulationQueue();
void AGENT_BridgeAttachToCurrentThread();
std::size_t AGENT_BridgePump();
void AGENT_BridgeShutdown();
void AGENT_SetDebuggerStopListener(DebuggerStopListener listener);
void AGENT_NotifyDebuggerStopped(std::uint16_t segment, std::uint32_t instruction_pointer);
bool AGENT_RunQueueSelfTest(std::string* error);

} // namespace dosbox_agent

#endif
