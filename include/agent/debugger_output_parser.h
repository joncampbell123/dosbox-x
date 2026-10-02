#ifndef DOSBOX_AGENT_DEBUGGER_OUTPUT_PARSER_H
#define DOSBOX_AGENT_DEBUGGER_OUTPUT_PARSER_H

#include <map>
#include <string>

namespace dosbox_agent {

struct DebuggerOutputParseResult {
    bool complete = false;
    std::map<std::string, std::string> registers;
    std::string unparsed_output;
};

// Parses the strict register form emitted by debugger diagnostics. A failed
// parse never returns a partial register object.
DebuggerOutputParseResult AGENT_ParseDebuggerRegisterOutput(const std::string& output);

} // namespace dosbox_agent

#endif
