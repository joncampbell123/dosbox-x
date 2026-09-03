#ifndef DOSBOX_AGENT_RPC_TRANSPORT_H
#define DOSBOX_AGENT_RPC_TRANSPORT_H

#include "agent/agent_protocol.h"

#include <functional>
#include <memory>
#include <string>

namespace dosbox_agent {

class IRpcTransport {
public:
    typedef std::function<std::string(const std::string&)> RequestHandler;

    virtual ~IRpcTransport() {}

    virtual const char* Name() const = 0;
    virtual bool ValidateConfiguration(const AgentConfig& config,
                                       std::string* error) const = 0;
    virtual bool Start(const AgentConfig& config,
                       RequestHandler handler,
                       std::string* error) = 0;
    virtual void Stop() = 0;
};

std::unique_ptr<IRpcTransport> AGENT_CreateLocalTransport(AgentTransport transport);

} // namespace dosbox_agent

#endif
