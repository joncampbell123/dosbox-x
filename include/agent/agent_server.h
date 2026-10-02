#ifndef DOSBOX_AGENT_SERVER_H
#define DOSBOX_AGENT_SERVER_H

#include "agent/agent_protocol.h"

#include <cstdint>
#include <memory>
#include <string>

namespace dosbox_agent {

class AgentServer {
public:
    class Impl;

    AgentServer();
    ~AgentServer();

    AgentServer(const AgentServer&) = delete;
    AgentServer& operator=(const AgentServer&) = delete;

    bool StartFromConfigFile(const std::string& path, std::string* error);
    bool Start(const AgentConfig& config, std::string* error);
    bool StartForTest(const AgentConfig& config, std::string* error);
    void Stop();

    bool IsStarted() const;
    const AgentConfig* GetConfig() const;
    std::string HandleJsonRpc(const std::string& request);

    // Covers protocol and lifecycle semantics without starting a guest target.
    bool RunProtocolSelfTest(std::string* error);

private:
    static std::string HandleJsonRpcImpl(const std::shared_ptr<Impl>& impl, const std::string& request);
    static void StartTargetOnEmulationThread(const std::shared_ptr<Impl>& impl,
                                             const std::string& session_id,
                                             const std::string& target_command,
                                             const std::vector<std::string>& target_arguments,
                                             const std::string& workdir);
    static void CompleteTargetTerminationOnEmulationThread(const std::shared_ptr<Impl>& impl,
                                                           const std::string& session_id,
                                                           const std::string& operation_id);
    static void OnDebuggerStopped(const std::shared_ptr<Impl>& impl,
                                  std::uint64_t generation,
                                  std::uint16_t segment,
                                  std::uint32_t instruction_pointer);

    std::shared_ptr<Impl> impl;
};

} // namespace dosbox_agent

#endif
