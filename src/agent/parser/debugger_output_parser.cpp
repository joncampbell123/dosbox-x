#include "agent/debugger_output_parser.h"

#include <cctype>
#include <set>
#include <sstream>

namespace dosbox_agent {
namespace {

bool IsHexValue(const std::string& value, const std::size_t width)
{
    if (value.size() != width + 2 || value[0] != '0' || value[1] != 'x')
        return false;
    for (std::size_t index = 2; index < value.size(); ++index) {
        if (!std::isxdigit(static_cast<unsigned char>(value[index])))
            return false;
    }
    return true;
}

} // namespace

DebuggerOutputParseResult AGENT_ParseDebuggerRegisterOutput(const std::string& output)
{
    DebuggerOutputParseResult result;
    static const char* const required_names[] = {
        "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP",
        "CS", "DS", "ES", "FS", "GS", "SS", "EIP", "FLAGS"
    };

    std::istringstream tokens(output);
    std::string token;
    std::set<std::string> required(required_names, required_names + sizeof(required_names) / sizeof(required_names[0]));
    while (tokens >> token) {
        const std::string::size_type separator = token.find('=');
        if (separator == std::string::npos)
            continue;
        const std::string name = token.substr(0, separator);
        const std::string value = token.substr(separator + 1);
        if (required.find(name) == required.end() || result.registers.find(name) != result.registers.end()) {
            result.registers.clear();
            result.unparsed_output = output;
            return result;
        }
        const std::size_t width = name == "CS" || name == "DS" || name == "ES" ||
                                  name == "FS" || name == "GS" || name == "SS" ? 4u : 8u;
        if (!IsHexValue(value, width)) {
            result.registers.clear();
            result.unparsed_output = output;
            return result;
        }
        result.registers[name] = value;
    }

    if (result.registers.size() != required.size()) {
        result.registers.clear();
        result.unparsed_output = output;
        return result;
    }
    result.complete = true;
    return result;
}

} // namespace dosbox_agent
