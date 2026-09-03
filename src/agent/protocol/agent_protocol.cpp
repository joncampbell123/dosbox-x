#include "agent/agent_protocol.h"

#include <cerrno>
#include <climits>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace dosbox_agent {
namespace {

static std::string Trim(const std::string& value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static bool IsAbsolutePath(const std::string& path)
{
    if (path.empty())
        return false;

#ifdef WIN32
    return (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
            path[1] == ':' && (path[2] == '\\' || path[2] == '/')) ||
           (path.size() >= 2 && path[0] == '\\' && path[1] == '\\');
#else
    return path[0] == '/';
#endif
}

static std::string DirectoryName(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("\\/");
    if (separator == std::string::npos)
        return std::string(".");
    if (separator == 0)
        return path.substr(0, 1);
    return path.substr(0, separator);
}

static std::string JoinPath(const std::string& directory, const std::string& path)
{
    if (directory.empty() || directory == ".")
        return path;
    if (directory[directory.size() - 1] == '\\' || directory[directory.size() - 1] == '/')
        return directory + path;
#ifdef WIN32
    return directory + "\\" + path;
#else
    return directory + "/" + path;
#endif
}

static bool AbsolutePath(const std::string& path, std::string* absolute, std::string* error)
{
#ifdef WIN32
    const DWORD required = GetFullPathNameA(path.c_str(), 0, NULL, NULL);
    if (required == 0) {
        *error = "Unable to resolve path: " + path;
        return false;
    }

    std::vector<char> buffer(required, '\0');
    if (GetFullPathNameA(path.c_str(), required, &buffer[0], NULL) == 0) {
        *error = "Unable to resolve path: " + path;
        return false;
    }
    absolute->assign(&buffer[0]);
    return true;
#else
    char buffer[PATH_MAX];
    if (realpath(path.c_str(), buffer) == NULL) {
        *error = "Unable to resolve path: " + path;
        return false;
    }
    absolute->assign(buffer);
    return true;
#endif
}

static bool ParseUnsigned(const std::string& value,
                          const std::uint64_t maximum,
                          std::uint64_t* parsed)
{
    if (value.empty())
        return false;

    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        if (!std::isdigit(static_cast<unsigned char>(*it)))
            return false;
    }

    errno = 0;
    char* end = NULL;
    const unsigned long long result = std::strtoull(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' || result == 0 || result > maximum)
        return false;

    *parsed = result;
    return true;
}

static std::string JsonEscape(const std::string& value)
{
    std::ostringstream escaped;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        switch (*it) {
        case '\\': escaped << "\\\\"; break;
        case '"': escaped << "\\\""; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default: escaped << *it; break;
        }
    }
    return escaped.str();
}

static void AppendUtf8(const unsigned int codepoint, std::string* output)
{
    if (codepoint <= 0x7f) {
        output->push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input(input) {}

    bool Parse(JsonValue* value, std::string* parse_error)
    {
        SkipWhitespace();
        if (!ParseValue(value, 0)) {
            *parse_error = error;
            return false;
        }
        SkipWhitespace();
        if (position != input.size()) {
            *parse_error = "Unexpected trailing data at byte " + std::to_string(position);
            return false;
        }
        return true;
    }

private:
    const std::string& input;
    std::size_t position = 0;
    std::string error;

    void SkipWhitespace()
    {
        while (position < input.size()) {
            const char c = input[position];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++position;
        }
    }

    bool Fail(const std::string& message)
    {
        error = message + " at byte " + std::to_string(position);
        return false;
    }

    bool ParseValue(JsonValue* value, const unsigned int depth)
    {
        if (depth > 64)
            return Fail("JSON nesting exceeds 64 levels");
        if (position == input.size())
            return Fail("Unexpected end of JSON input");

        switch (input[position]) {
        case 'n': return ParseLiteral("null", JsonValue::Null(), value);
        case 't': return ParseLiteral("true", JsonValue::Bool(true), value);
        case 'f': return ParseLiteral("false", JsonValue::Bool(false), value);
        case '"': return ParseStringValue(value);
        case '[': return ParseArray(value, depth + 1);
        case '{': return ParseObject(value, depth + 1);
        default:
            if (input[position] == '-' || std::isdigit(static_cast<unsigned char>(input[position])))
                return ParseNumber(value);
            return Fail("Expected JSON value");
        }
    }

    bool ParseLiteral(const char* literal, const JsonValue& literal_value, JsonValue* value)
    {
        const std::size_t length = std::strlen(literal);
        if (input.compare(position, length, literal) != 0)
            return Fail("Invalid JSON literal");
        position += length;
        *value = literal_value;
        return true;
    }

    bool ParseHex4(unsigned int* value)
    {
        if (position + 4 > input.size())
            return Fail("Incomplete unicode escape");

        unsigned int result = 0;
        for (unsigned int index = 0; index < 4; ++index) {
            const char c = input[position++];
            result <<= 4;
            if (c >= '0' && c <= '9') result |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f') result |= static_cast<unsigned int>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') result |= static_cast<unsigned int>(c - 'A' + 10);
            else return Fail("Invalid unicode escape");
        }
        *value = result;
        return true;
    }

    bool ParseString(std::string* value)
    {
        if (position == input.size() || input[position] != '"')
            return Fail("Expected string");
        ++position;
        value->clear();

        while (position < input.size()) {
            const unsigned char c = static_cast<unsigned char>(input[position++]);
            if (c == '"')
                return true;
            if (c < 0x20)
                return Fail("Unescaped control character in string");
            if (c != '\\') {
                value->push_back(static_cast<char>(c));
                continue;
            }

            if (position == input.size())
                return Fail("Incomplete string escape");
            const char escape = input[position++];
            switch (escape) {
            case '"': value->push_back('"'); break;
            case '\\': value->push_back('\\'); break;
            case '/': value->push_back('/'); break;
            case 'b': value->push_back('\b'); break;
            case 'f': value->push_back('\f'); break;
            case 'n': value->push_back('\n'); break;
            case 'r': value->push_back('\r'); break;
            case 't': value->push_back('\t'); break;
            case 'u': {
                unsigned int codepoint = 0;
                if (!ParseHex4(&codepoint))
                    return false;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (position + 2 > input.size() || input[position] != '\\' || input[position + 1] != 'u')
                        return Fail("Missing low surrogate");
                    position += 2;
                    unsigned int low = 0;
                    if (!ParseHex4(&low))
                        return false;
                    if (low < 0xdc00 || low > 0xdfff)
                        return Fail("Invalid low surrogate");
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return Fail("Unexpected low surrogate");
                }
                AppendUtf8(codepoint, value);
                break;
            }
            default: return Fail("Invalid string escape");
            }
        }
        return Fail("Unterminated string");
    }

    bool ParseStringValue(JsonValue* value)
    {
        std::string parsed;
        if (!ParseString(&parsed))
            return false;
        *value = JsonValue::String(parsed);
        return true;
    }

    bool ParseNumber(JsonValue* value)
    {
        const std::size_t start = position;
        if (input[position] == '-')
            ++position;
        if (position == input.size())
            return Fail("Incomplete number");

        if (input[position] == '0') {
            ++position;
        } else if (input[position] >= '1' && input[position] <= '9') {
            do { ++position; } while (position < input.size() && std::isdigit(static_cast<unsigned char>(input[position])));
        } else {
            return Fail("Invalid number");
        }

        if (position < input.size() && input[position] == '.') {
            ++position;
            const std::size_t fraction_start = position;
            while (position < input.size() && std::isdigit(static_cast<unsigned char>(input[position])))
                ++position;
            if (fraction_start == position)
                return Fail("Invalid number fraction");
        }
        if (position < input.size() && (input[position] == 'e' || input[position] == 'E')) {
            ++position;
            if (position < input.size() && (input[position] == '+' || input[position] == '-'))
                ++position;
            const std::size_t exponent_start = position;
            while (position < input.size() && std::isdigit(static_cast<unsigned char>(input[position])))
                ++position;
            if (exponent_start == position)
                return Fail("Invalid number exponent");
        }

        *value = JsonValue::Number(input.substr(start, position - start));
        return true;
    }

    bool ParseArray(JsonValue* value, const unsigned int depth)
    {
        ++position;
        JsonValue result = JsonValue::Array();
        SkipWhitespace();
        if (position < input.size() && input[position] == ']') {
            ++position;
            *value = result;
            return true;
        }

        for (;;) {
            JsonValue item;
            if (!ParseValue(&item, depth))
                return false;
            result.array.push_back(item);
            SkipWhitespace();
            if (position == input.size())
                return Fail("Unterminated array");
            const char separator = input[position++];
            if (separator == ']') {
                *value = result;
                return true;
            }
            if (separator != ',')
                return Fail("Expected comma or array end");
            SkipWhitespace();
        }
    }

    bool ParseObject(JsonValue* value, const unsigned int depth)
    {
        ++position;
        JsonValue result = JsonValue::Object();
        SkipWhitespace();
        if (position < input.size() && input[position] == '}') {
            ++position;
            *value = result;
            return true;
        }

        for (;;) {
            std::string name;
            if (!ParseString(&name))
                return false;
            SkipWhitespace();
            if (position == input.size() || input[position++] != ':')
                return Fail("Expected object member separator");
            SkipWhitespace();
            JsonValue member;
            if (!ParseValue(&member, depth))
                return false;
            if (result.object.find(name) != result.object.end())
                return Fail("Duplicate object member: " + name);
            result.object[name] = member;
            SkipWhitespace();
            if (position == input.size())
                return Fail("Unterminated object");
            const char separator = input[position++];
            if (separator == '}') {
                *value = result;
                return true;
            }
            if (separator != ',')
                return Fail("Expected comma or object end");
            SkipWhitespace();
        }
    }
};

static void SerializeString(const std::string& value, std::ostringstream* output)
{
    *output << '"';
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        const unsigned char c = static_cast<unsigned char>(*it);
        switch (c) {
        case '"': *output << "\\\""; break;
        case '\\': *output << "\\\\"; break;
        case '\b': *output << "\\b"; break;
        case '\f': *output << "\\f"; break;
        case '\n': *output << "\\n"; break;
        case '\r': *output << "\\r"; break;
        case '\t': *output << "\\t"; break;
        default:
            if (c < 0x20) {
                *output << "\\u" << std::hex << std::uppercase << std::setw(4)
                        << std::setfill('0') << static_cast<unsigned int>(c)
                        << std::dec << std::nouppercase << std::setfill(' ');
            } else {
                *output << static_cast<char>(c);
            }
            break;
        }
    }
    *output << '"';
}

static void SerializeValue(const JsonValue& value, std::ostringstream* output)
{
    switch (value.type) {
    case JsonType::Null: *output << "null"; return;
    case JsonType::Bool: *output << (value.boolean ? "true" : "false"); return;
    case JsonType::Number: *output << value.text; return;
    case JsonType::String: SerializeString(value.text, output); return;
    case JsonType::Array:
        *output << '[';
        for (std::size_t index = 0; index < value.array.size(); ++index) {
            if (index != 0) *output << ',';
            SerializeValue(value.array[index], output);
        }
        *output << ']';
        return;
    case JsonType::Object:
        *output << '{';
        for (std::map<std::string, JsonValue>::const_iterator it = value.object.begin(); it != value.object.end(); ++it) {
            if (it != value.object.begin()) *output << ',';
            SerializeString(it->first, output);
            *output << ':';
            SerializeValue(it->second, output);
        }
        *output << '}';
        return;
    }
}

} // namespace

bool AGENT_LoadConfigFile(const std::string& path,
                          AgentConfig* config,
                          std::string* error)
{
    if (config == NULL || error == NULL)
        return false;

    config->config_path.clear();
    *error = std::string();

    std::string absolute_path;
    if (!AbsolutePath(path, &absolute_path, error))
        return false;

    std::ifstream file(absolute_path.c_str());
    if (!file.is_open()) {
        *error = "Unable to open agent config: " + absolute_path;
        return false;
    }

    const char* const required_keys[] = {
        "transport",
        "endpoint",
        "dosbox_executable",
        "dosbox_workdir",
        "request_timeout_ms",
        "max_message_bytes",
        "max_memory_read_bytes",
        "max_trace_events"
    };
    const char* const optional_keys[] = {
        "profile"
    };

    std::map<std::string, std::string> values;
    std::string line;
    unsigned int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
            continue;

        const std::string::size_type equals = trimmed.find('=');
        if (equals == std::string::npos) {
            *error = "Invalid agent config line " + std::to_string(line_number);
            return false;
        }

        const std::string key = Trim(trimmed.substr(0, equals));
        const std::string value = Trim(trimmed.substr(equals + 1));
        bool known = false;
        for (std::size_t index = 0; index < sizeof(required_keys) / sizeof(required_keys[0]); ++index) {
            if (key == required_keys[index]) {
                known = true;
                break;
            }
        }
        if (!known) {
            for (std::size_t index = 0; index < sizeof(optional_keys) / sizeof(optional_keys[0]); ++index) {
                if (key == optional_keys[index]) {
                    known = true;
                    break;
                }
            }
        }

        if (!known) {
            *error = "Unknown agent config key on line " + std::to_string(line_number) + ": " + key;
            return false;
        }
        if (key.empty() || value.empty()) {
            *error = "Empty agent config key or value on line " + std::to_string(line_number);
            return false;
        }
        if (values.find(key) != values.end()) {
            *error = "Duplicate agent config key: " + key;
            return false;
        }
        values[key] = value;
    }

    for (std::size_t index = 0; index < sizeof(required_keys) / sizeof(required_keys[0]); ++index) {
        if (values.find(required_keys[index]) == values.end()) {
            *error = "Missing required agent config key: " + std::string(required_keys[index]);
            return false;
        }
    }

    AgentConfig parsed;
    parsed.config_path = absolute_path;
    if (values["transport"] == "named_pipe")
        parsed.transport = AgentTransport::NamedPipe;
    else if (values["transport"] == "unix_socket")
        parsed.transport = AgentTransport::UnixSocket;
    else {
        *error = "Unsupported agent transport: " + values["transport"];
        return false;
    }

    parsed.endpoint = values["endpoint"];
    const std::string profile = values.find("profile") == values.end() ?
            "production" : values["profile"];
    if (profile == "production")
        parsed.test_profile = false;
    else if (profile == "test")
        parsed.test_profile = true;
    else {
        *error = "profile must be production or test";
        return false;
    }
    const bool executable_is_absolute = IsAbsolutePath(values["dosbox_executable"]);
    const bool workdir_is_absolute = IsAbsolutePath(values["dosbox_workdir"]);
    if (!parsed.test_profile && (!executable_is_absolute || !workdir_is_absolute)) {
        *error = "Production agent config requires absolute dosbox_executable and dosbox_workdir paths";
        return false;
    }

    const std::string config_directory = DirectoryName(absolute_path);
    parsed.dosbox_executable = executable_is_absolute
                                       ? values["dosbox_executable"]
                                       : JoinPath(config_directory, values["dosbox_executable"]);
    parsed.dosbox_workdir = workdir_is_absolute
                                ? values["dosbox_workdir"]
                                : JoinPath(config_directory, values["dosbox_workdir"]);

    std::uint64_t number = 0;
    if (!ParseUnsigned(values["request_timeout_ms"], UINT_MAX, &number)) {
        *error = "Invalid request_timeout_ms";
        return false;
    }
    parsed.request_timeout_ms = static_cast<std::uint32_t>(number);

    const std::uint64_t max_size =
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)());
    if (!ParseUnsigned(values["max_message_bytes"], max_size, &number)) {
        *error = "Invalid max_message_bytes";
        return false;
    }
    parsed.max_message_bytes = static_cast<std::size_t>(number);

    if (!ParseUnsigned(values["max_memory_read_bytes"], max_size, &number)) {
        *error = "Invalid max_memory_read_bytes";
        return false;
    }
    parsed.max_memory_read_bytes = static_cast<std::size_t>(number);

    if (!ParseUnsigned(values["max_trace_events"], max_size, &number)) {
        *error = "Invalid max_trace_events";
        return false;
    }
    parsed.max_trace_events = static_cast<std::size_t>(number);

    *config = parsed;
    return true;
}

const char* AGENT_TransportName(const AgentTransport transport)
{
    switch (transport) {
    case AgentTransport::NamedPipe: return "named_pipe";
    case AgentTransport::UnixSocket: return "unix_socket";
    }
    return "unknown";
}

std::string AGENT_FormatStartupLog(const AgentConfig& config)
{
    std::ostringstream log;
    log << "{\"component\":\"dosbox-agent\",\"event\":\"config_loaded\""
        << ",\"transport\":\"" << AGENT_TransportName(config.transport) << "\""
        << ",\"endpoint\":\"" << JsonEscape(config.endpoint) << "\""
        << ",\"profile\":\"" << (config.test_profile ? "test" : "production") << "\""
        << ",\"dosbox_executable\":\"" << JsonEscape(config.dosbox_executable) << "\""
        << ",\"dosbox_workdir\":\"" << JsonEscape(config.dosbox_workdir) << "\""
        << ",\"request_timeout_ms\":" << config.request_timeout_ms
        << ",\"max_message_bytes\":" << config.max_message_bytes
        << ",\"max_memory_read_bytes\":" << config.max_memory_read_bytes
        << ",\"max_trace_events\":" << config.max_trace_events
        << "}";
    return log.str();
}

JsonValue JsonValue::Null()
{
    return JsonValue();
}

JsonValue JsonValue::Bool(const bool value)
{
    JsonValue result;
    result.type = JsonType::Bool;
    result.boolean = value;
    return result;
}

JsonValue JsonValue::Number(const std::string& value)
{
    JsonValue result;
    result.type = JsonType::Number;
    result.text = value;
    return result;
}

JsonValue JsonValue::String(const std::string& value)
{
    JsonValue result;
    result.type = JsonType::String;
    result.text = value;
    return result;
}

JsonValue JsonValue::Array()
{
    JsonValue result;
    result.type = JsonType::Array;
    return result;
}

JsonValue JsonValue::Object()
{
    JsonValue result;
    result.type = JsonType::Object;
    return result;
}

const JsonValue* JsonValue::Find(const std::string& name) const
{
    if (type != JsonType::Object)
        return NULL;
    const std::map<std::string, JsonValue>::const_iterator it = object.find(name);
    return it == object.end() ? NULL : &it->second;
}

bool AGENT_ParseJson(const std::string& input, JsonValue* value, std::string* error)
{
    if (value == NULL || error == NULL)
        return false;
    error->clear();
    return JsonParser(input).Parse(value, error);
}

std::string AGENT_SerializeJson(const JsonValue& value)
{
    std::ostringstream output;
    SerializeValue(value, &output);
    return output.str();
}

std::string AGENT_CanonicalJson(const JsonValue& value)
{
    return AGENT_SerializeJson(value);
}

bool AGENT_ParseJsonRpcRequest(const std::string& input,
                               JsonRpcRequest* request,
                               JsonRpcParseError* kind,
                               std::string* error)
{
    if (request == NULL || kind == NULL || error == NULL)
        return false;

    JsonValue root;
    if (!AGENT_ParseJson(input, &root, error)) {
        *kind = JsonRpcParseError::ParseError;
        return false;
    }

    *kind = JsonRpcParseError::InvalidRequest;
    if (root.type != JsonType::Object) {
        *error = "JSON-RPC request must be an object";
        return false;
    }
    const JsonValue* jsonrpc = root.Find("jsonrpc");
    const JsonValue* method = root.Find("method");
    const JsonValue* id = root.Find("id");
    const JsonValue* params = root.Find("params");
    if (jsonrpc == NULL || jsonrpc->type != JsonType::String || jsonrpc->text != "2.0") {
        *error = "jsonrpc must be the string 2.0";
        return false;
    }
    if (method == NULL || method->type != JsonType::String || method->text.empty()) {
        *error = "method must be a non-empty string";
        return false;
    }
    if (id != NULL && id->type != JsonType::String && id->type != JsonType::Number && id->type != JsonType::Null) {
        *error = "id must be a string, number, or null";
        return false;
    }
    if (params != NULL && params->type != JsonType::Object) {
        *error = "params must be an object";
        return false;
    }

    request->id = id == NULL ? JsonValue::Null() : *id;
    request->has_id = id != NULL;
    request->id_key = id == NULL ? std::string() : AGENT_CanonicalJson(*id);
    request->method = method->text;
    request->params = params == NULL ? JsonValue::Object() : *params;
    JsonValue fingerprint = JsonValue::Object();
    fingerprint.object["method"] = JsonValue::String(request->method);
    fingerprint.object["params"] = request->params;
    request->fingerprint = AGENT_CanonicalJson(fingerprint);
    return true;
}

std::string AGENT_MakeJsonRpcResult(const JsonValue& id, const JsonValue& result)
{
    JsonValue response = JsonValue::Object();
    response.object["jsonrpc"] = JsonValue::String("2.0");
    response.object["id"] = id;
    response.object["result"] = result;
    return AGENT_SerializeJson(response);
}

std::string AGENT_MakeJsonRpcError(const JsonValue& id,
                                   const int code,
                                   const std::string& message,
                                   const JsonValue* data)
{
    JsonValue error = JsonValue::Object();
    error.object["code"] = JsonValue::Number(std::to_string(code));
    error.object["message"] = JsonValue::String(message);
    if (data != NULL)
        error.object["data"] = *data;

    JsonValue response = JsonValue::Object();
    response.object["jsonrpc"] = JsonValue::String("2.0");
    response.object["id"] = id;
    response.object["error"] = error;
    return AGENT_SerializeJson(response);
}

} // namespace dosbox_agent
