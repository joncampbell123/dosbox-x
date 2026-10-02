#ifndef DOSBOX_AGENT_PROTOCOL_H
#define DOSBOX_AGENT_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dosbox_agent {

enum class AgentTransport {
    NamedPipe,
    UnixSocket
};

struct AgentConfig {
    AgentTransport transport = AgentTransport::NamedPipe;
    std::string config_path;
    std::string endpoint;
    std::string dosbox_executable;
    std::string dosbox_workdir;
    std::uint32_t request_timeout_ms = 0;
    std::size_t max_message_bytes = 0;
    std::size_t max_memory_read_bytes = 0;
    std::size_t max_trace_events = 0;
    bool test_profile = false;
};

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    static JsonValue Null();
    static JsonValue Bool(bool value);
    static JsonValue Number(const std::string& value);
    static JsonValue String(const std::string& value);
    static JsonValue Array();
    static JsonValue Object();

    const JsonValue* Find(const std::string& name) const;
};

struct JsonRpcRequest {
    JsonValue id;
    bool has_id = false;
    std::string id_key;
    std::string method;
    JsonValue params;
    std::string fingerprint;
};

enum class JsonRpcParseError {
    ParseError,
    InvalidRequest
};

bool AGENT_ParseJson(const std::string& input, JsonValue* value, std::string* error);
std::string AGENT_SerializeJson(const JsonValue& value);
std::string AGENT_CanonicalJson(const JsonValue& value);
bool AGENT_ParseJsonRpcRequest(const std::string& input,
                               JsonRpcRequest* request,
                               JsonRpcParseError* kind,
                               std::string* error);
std::string AGENT_MakeJsonRpcResult(const JsonValue& id, const JsonValue& result);
std::string AGENT_MakeJsonRpcError(const JsonValue& id,
                                   int code,
                                   const std::string& message,
                                   const JsonValue* data = NULL);

bool AGENT_LoadConfigFile(const std::string& path,
                          AgentConfig* config,
                          std::string* error);

const char* AGENT_TransportName(AgentTransport transport);
std::string AGENT_FormatStartupLog(const AgentConfig& config);

} // namespace dosbox_agent

#endif
