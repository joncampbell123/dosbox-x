// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "mcp_server.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <thread>

namespace Agent86 {
namespace {

constexpr size_t MaxMcpMessageBytes = 1024 * 1024;
constexpr size_t MaxScreenshotBytes = 8 * 1024 * 1024;
constexpr const char *ServerVersion = "2.2.0";
constexpr const char *LatestProtocolVersion = "2025-11-25";
const std::array<const char *, 4> SupportedProtocolVersions = {{
        "2024-11-05", "2025-03-26", "2025-06-18", "2025-11-25"}};

Json::Object Object(std::initializer_list<std::pair<const std::string, Json>> values)
{
	return Json::Object(values);
}

Json::Array Array(std::initializer_list<Json> values)
{
	return Json::Array(values);
}

const Json::Object &RequireObject(const Json *value, const char *field)
{
	if (!value || !value->IsObject())
		throw std::invalid_argument(std::string(field) + " must be an object");
	return value->AsObject();
}

const std::string &RequireString(const Json::Object &object, const char *field)
{
	const auto found = object.find(field);
	if (found == object.end() || !found->second.IsString())
		throw std::invalid_argument(std::string(field) + " must be a string");
	return found->second.AsString();
}

std::string OptionalString(const Json::Object &object, const char *field)
{
	const auto found = object.find(field);
	if (found == object.end())
		return {};
	if (!found->second.IsString())
		throw std::invalid_argument(std::string(field) + " must be a string");
	return found->second.AsString();
}

std::string RequireDriveLetter(const Json::Object &object)
{
	std::string drive = RequireString(object, "drive");
	if (drive.size() == 2 && drive[1] == ':') drive.pop_back();
	if (drive.size() != 1 || drive[0] < 'A' || drive[0] > 'Y')
		throw std::invalid_argument("drive must be one letter from A through Y");
	return drive;
}

std::string RequireHostDirectory(const Json::Object &object)
{
	const std::string &raw = RequireString(object, "path");
	if (raw.empty() || raw.size() > 4096 || raw.find_first_of("\r\n") != std::string::npos ||
	    raw.find('\0') != std::string::npos)
		throw std::invalid_argument("path must be a single host directory path of at most 4096 bytes");
	std::error_code error;
	const auto path = std::filesystem::weakly_canonical(std::filesystem::u8path(raw), error);
	if (error || !path.is_absolute() || !std::filesystem::is_directory(path, error) || error)
		throw std::invalid_argument("path must identify an existing host directory");
	return path.u8string();
}

std::filesystem::path RequireExistingHostFile(const Json::Object &object,
                                              const std::set<std::string> &extensions)
{
	const std::string &raw = RequireString(object, "path");
	if (raw.empty() || raw.size() > 4096 || raw.find_first_of("\r\n\"") != std::string::npos ||
	    raw.find('\0') != std::string::npos)
		throw std::invalid_argument("path must be one host file path of at most 4096 bytes");
	std::error_code error;
	const auto path = std::filesystem::canonical(std::filesystem::u8path(raw), error);
	if (error || !path.is_absolute() || !std::filesystem::is_regular_file(path, error) || error)
		throw std::invalid_argument("path must identify an existing host file");
	std::string extension = path.extension().u8string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
	               [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	if (extensions.find(extension) == extensions.end())
		throw std::invalid_argument("path has an unsupported image extension");
	return path;
}

std::filesystem::path RequireNewDiskImagePath(const Json::Object &object)
{
	const std::string &raw = RequireString(object, "path");
	if (raw.empty() || raw.size() > 4096 || raw.find_first_of("\r\n\"") != std::string::npos ||
	    raw.find('\0') != std::string::npos)
		throw std::invalid_argument("path must be one host file path of at most 4096 bytes");
	const auto requested = std::filesystem::u8path(raw);
	std::string extension = requested.extension().u8string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
	               [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	if (!requested.is_absolute() || extension != ".img")
		throw std::invalid_argument("path must be an absolute new .img file path");
	std::error_code error;
	const auto parent = std::filesystem::canonical(requested.parent_path(), error);
	if (error || !std::filesystem::is_directory(parent, error) || error)
		throw std::invalid_argument("the disk image parent directory must exist");
	const auto path = parent / requested.filename();
	if (std::filesystem::exists(path, error) || error)
		throw std::invalid_argument("the disk image destination must not already exist");
	return path;
}

int RequireBoundedInteger(const Json::Object &object, const char *field, const int maximum)
{
	const auto found = object.find(field);
	if (found == object.end() || !found->second.IsNumber())
		throw std::invalid_argument(std::string(field) + " must be an integer");
	const double number = found->second.AsNumber();
	if (!std::isfinite(number) || std::floor(number) != number || number < 0 || number > maximum)
		throw std::invalid_argument(std::string(field) + " must be an integer from 0 through " +
		                            std::to_string(maximum));
	return static_cast<int>(number);
}

std::string Base64Encode(const std::string &bytes)
{
	static constexpr char Alphabet[] =
	        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string encoded;
	encoded.reserve(((bytes.size() + 2) / 3) * 4);
	for (size_t index = 0; index < bytes.size(); index += 3) {
		const uint32_t first = static_cast<unsigned char>(bytes[index]);
		const uint32_t second = index + 1 < bytes.size() ? static_cast<unsigned char>(bytes[index + 1]) : 0;
		const uint32_t third = index + 2 < bytes.size() ? static_cast<unsigned char>(bytes[index + 2]) : 0;
		const uint32_t value = (first << 16) | (second << 8) | third;
		encoded.push_back(Alphabet[(value >> 18) & 63]);
		encoded.push_back(Alphabet[(value >> 12) & 63]);
		encoded.push_back(index + 1 < bytes.size() ? Alphabet[(value >> 6) & 63] : '=');
		encoded.push_back(index + 2 < bytes.size() ? Alphabet[value & 63] : '=');
	}
	return encoded;
}

std::string ReadBoundedBinaryFile(const std::filesystem::path &path, const size_t maximum)
{
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if (error || size == 0 || size > maximum)
		throw BridgeError("DOSBox-X screenshot is empty or exceeds the 8 MiB limit");
	std::ifstream input(path, std::ios::binary);
	if (!input) throw BridgeError("cannot open the DOSBox-X screenshot");
	std::string bytes(static_cast<size_t>(size), '\0');
	input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()))
		throw BridgeError("cannot read the complete DOSBox-X screenshot");
	return bytes;
}

Json TextResult(const std::string &text, const bool is_error = false)
{
	Json::Object result = Object({
	        {"content", Array({Json(Object({{"type", "text"}, {"text", text}}))})},
	        {"isError", is_error},
	});
	if (!is_error)
		result.emplace("structuredContent", Json(Object({{"result", text}})));
	return Json(std::move(result));
}

Json ImageResult(const std::string &png_bytes)
{
	return Json(Object({
	        {"content", Array({Json(Object({{"type", "image"},
	                                         {"data", Base64Encode(png_bytes)},
	                                         {"mimeType", "image/png"}}))})},
	        {"isError", false},
	}));
}

Json JsonRpcError(const Json &id, const int code, const std::string &message)
{
	return Json(Object({
	        {"jsonrpc", "2.0"},
	        {"id", id},
	        {"error", Json(Object({{"code", code}, {"message", message}}))},
	}));
}

Json Tool(const char *name,
	      const char *description,
	      Json::Object properties,
	      Json::Array required,
	      const bool read_only,
	      const bool destructive,
	      const bool idempotent)
{
	return Json(Object({
	        {"name", name},
	        {"description", description},
	        {"inputSchema", Json(Object({
	                                {"type", "object"},
	                                {"properties", Json(std::move(properties))},
	                                {"required", Json(std::move(required))},
	                                {"additionalProperties", false},
	                        }))},
	        {"annotations", Json(Object({
	                                {"readOnlyHint", read_only},
	                                {"destructiveHint", destructive},
	                                {"idempotentHint", idempotent},
	                                {"openWorldHint", false},
	                        }))},
	}));
}

Json ToolList()
{
	Json::Array tools;
	tools.emplace_back(Tool(
	        "run_dos_command",
	        "Execute one command in DOS and return its bounded CP437 output.",
	        Object({{"command", Json(Object({
	                                      {"type", "string"},
	                                      {"description", "One DOS command line to execute."},
	                              }))}}),
	        Array({"command"}),
	        false,
	        true,
	        false));
	tools.emplace_back(Tool(
	        "write_dos_file",
	        "Write one CP437 text file to a mounted DOS drive using an 8.3 path.",
	        Object({
	                {"filename", Json(Object({
	                                     {"type", "string"},
	                                     {"description", "Absolute DOS 8.3 path, for example A:\\\\SRC\\\\GAME.C."},
	                             }))},
	                {"content", Json(Object({
	                                    {"type", "string"},
	                                    {"description", "UTF-8 text representable in DOS code page 437."},
	                            }))},
	        }),
	        Array({"filename", "content"}),
	        false,
	        true,
	        false));
	tools.emplace_back(Tool(
	        "release_dos_console",
	        "End the bridge session and restore the local DOSBox-X console.",
	        Object({}),
	        Array({}),
	        false,
	        false,
	        true));
	const auto add_control = [&tools](const char *name,
	                                  const char *description,
	                                  const bool read_only,
	                                  const bool idempotent) {
		tools.emplace_back(Tool(name,
		                        description,
		                        Object({}),
		                        Array({}),
		                        read_only,
		                        !read_only,
		                        idempotent));
	};
	add_control("swap_floppy_drive", "Swap to the next configured floppy disk.", false, false);
	add_control("swap_cd_drive", "Swap to the next configured CD image.", false, false);
	add_control("change_floppy_image", "Open DOSBox-X's floppy image picker.", false, false);
	add_control("change_cd_image", "Open DOSBox-X's CD image picker.", false, false);
	add_control("create_blank_disk_image", "Open DOSBox-X's blank disk image creator.", false, false);
	add_control("list_mounted_drives", "Return mounted BIOS disk and swap positions.", true, true);
	add_control("get_ide_status", "Return IDE disk and CD assignments.", true, true);
	add_control("rescan_all_drives", "Rescan every mounted DOS drive.", false, true);
	add_control("export_text_screen", "Open the TXT or Markdown DOS screen export dialog.", false, false);
	add_control("get_text_screen", "Return the visible DOS text-mode screen as UTF-8.", true, true);
	add_control("capture_graphics_screen", "Capture the current DOS framebuffer and return it as a PNG image.", false, false);
	tools.emplace_back(Tool(
	        "move_mouse",
	        "Move the DOS mouse to normalized guest-screen coordinates from 0 through 65535.",
	        Object({
	                {"x", Json(Object({{"type", "integer"}, {"minimum", 0}, {"maximum", 65535}}))},
	                {"y", Json(Object({{"type", "integer"}, {"minimum", 0}, {"maximum", 65535}}))},
	        }),
	        Array({"x", "y"}), false, false, true));
	tools.emplace_back(Tool(
	        "click_mouse",
	        "Click one DOS mouse button at the current guest position.",
	        Object({{"button", Json(Object({{"type", "string"}, {"enum", Array({"left", "right", "middle"})}}))}}),
	        Array({"button"}), false, false, false));
	tools.emplace_back(Tool(
	        "set_floppy_image",
	        "Load an existing host floppy image directly into DOSBox-X without opening a dialog.",
	        Object({
	                {"drive", Json(Object({{"type", "string"}, {"enum", Array({"A", "B"})}}))},
	                {"path", Json(Object({{"type", "string"}, {"description", "Absolute existing floppy image path."}}))},
	        }),
	        Array({"drive", "path"}), false, true, false));
	tools.emplace_back(Tool(
	        "set_cd_image",
	        "Load an existing host CD image directly into a mounted DOSBox-X CD drive without opening a dialog.",
	        Object({
	                {"drive", Json(Object({{"type", "string"}, {"description", "Mounted CD drive letter C through Y."}}))},
	                {"path", Json(Object({{"type", "string"}, {"description", "Absolute existing CD image path."}}))},
	        }),
	        Array({"drive", "path"}), false, true, false));
	tools.emplace_back(Tool(
	        "create_disk_image",
	        "Create a new blank DOSBox-X disk image at an absolute host .img path without opening a dialog.",
	        Object({
	                {"path", Json(Object({{"type", "string"}, {"description", "Absolute new .img file path whose parent exists."}}))},
	                {"format", Json(Object({{"type", "string"}, {"enum", Array({"fd_360", "fd_400", "fd_720", "fd_1200", "fd_1440", "fd_2880", "hd_250", "hd_520", "hd_1gig", "hd_2gig", "hd_4gig", "hd_8gig"})}}))},
	        }),
	        Array({"path", "format"}), false, true, false));
	tools.emplace_back(Tool(
	        "mount_host_folder",
	        "Mount an existing host folder on a DOS drive using DOSBox-X internals.",
	        Object({
	                {"drive", Json(Object({{"type", "string"}, {"description", "DOS drive letter A through Y."}}))},
	                {"path", Json(Object({{"type", "string"}, {"description", "Absolute existing host directory."}}))},
	                {"kind", Json(Object({{"type", "string"}, {"enum", Array({"local", "cdrom", "floppy"})}}))},
	        }),
	        Array({"drive", "path", "kind"}), false, true, false));
	tools.emplace_back(Tool(
	        "unmount_drive",
	        "Unmount a DOS drive using DOSBox-X internals.",
	        Object({{"drive", Json(Object({{"type", "string"}, {"description", "Mounted DOS drive letter A through Y."}}))}}),
	        Array({"drive"}), false, true, true));
	tools.emplace_back(Tool(
	        "type_text",
	        "Type bounded text into the running DOS application through DOSBox-X keyboard input.",
	        Object({{"text", Json(Object({{"type", "string"}, {"maxLength", 4096}}))}}),
	        Array({"text"}), false, true, false));
	return Json(Object({{"tools", Json(std::move(tools))}}));
}

bool LogCommandsEnabled()
{
	const char *value = std::getenv("DOS_BRIDGE_LOG_COMMANDS");
	return value && (std::string(value) == "1" || std::string(value) == "true");
}

enum class LineReadResult { Line, End, Oversized, Failed };

LineReadResult ReadBoundedLine(std::istream &input, std::string &line)
{
	line.clear();
	bool oversized = false;
	for (;;) {
		const int next = input.get();
		if (next == std::char_traits<char>::eof()) {
			if (input.eof())
				return line.empty() && !oversized ? LineReadResult::End
				                                  : (oversized ? LineReadResult::Oversized
				                                               : LineReadResult::Line);
			return LineReadResult::Failed;
		}
		if (next == '\n')
			return oversized ? LineReadResult::Oversized : LineReadResult::Line;
		if (line.size() < MaxMcpMessageBytes)
			line.push_back(static_cast<char>(next));
		else
			oversized = true;
	}
}

void RequireOnlyFields(const Json::Object &object,
	                   std::initializer_list<const char *> allowed)
{
	for (const auto &entry : object) {
		const bool recognized = std::any_of(
		        allowed.begin(), allowed.end(), [&entry](const char *candidate) {
			        return entry.first == candidate;
		        });
		if (!recognized)
			throw std::invalid_argument("unexpected argument: " + entry.first);
	}
}

} // namespace

McpServer::McpServer(DosBridgeClient &requested_bridge, SessionLogger &requested_logger)
        : bridge(requested_bridge), logger(requested_logger)
{}

int McpServer::Run(std::istream &input,
	               std::ostream &output,
	               std::ostream &errors)
{
	std::string line;
	for (;;) {
		const LineReadResult read_result = ReadBoundedLine(input, line);
		if (read_result == LineReadResult::End)
			break;
		if (read_result == LineReadResult::Failed) {
			errors << "agent86-mcp: failed while reading standard input\n";
			return 1;
		}
		if (read_result == LineReadResult::Oversized) {
			output << JsonRpcError(Json(), -32700, "MCP message exceeds 1048576 bytes").Dump()
			       << '\n';
			output.flush();
			continue;
		}
		if (line.empty())
			continue;
		try {
			const Json request = Json::Parse(line);
			const Json *id = request.Find("id");
			if (!id) {
				(void)HandleRequest(request);
				continue;
			}
			output << HandleRequest(request).Dump() << '\n';
		} catch (const std::exception &error) {
			output << JsonRpcError(Json(), -32700, error.what()).Dump() << '\n';
		}
		output.flush();
	}
	return 0;
}

Json McpServer::HandleRequest(const Json &request)
{
	if (!request.IsObject())
		return JsonRpcError(Json(), -32600, "request must be an object");
	const Json *id_value = request.Find("id");
	const Json id = id_value ? *id_value : Json();
	const Json *version = request.Find("jsonrpc");
	const Json *method_value = request.Find("method");
	if (!version || !version->IsString() || version->AsString() != "2.0" ||
	    !method_value || !method_value->IsString())
		return JsonRpcError(id, -32600, "invalid JSON-RPC request");

	try {
		const std::string &method = method_value->AsString();
		Json result;
		if (method == "initialize")
			result = Initialize(request.Find("params") ? *request.Find("params")
			                                           : Json(Object({})));
		else if (method == "ping")
			result = Json(Object({}));
		else if (method == "tools/list")
			result = ToolList();
		else if (method == "tools/call")
			result = CallTool(request.Find("params") ? *request.Find("params")
			                                         : Json(Object({})));
		else if (method.rfind("notifications/", 0) == 0)
			result = Json(Object({}));
		else
			return JsonRpcError(id, -32601, "method not found");
		return Json(Object({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}}));
	} catch (const std::invalid_argument &error) {
		return JsonRpcError(id, -32602, error.what());
	} catch (const std::exception &error) {
		return JsonRpcError(id, -32603, error.what());
	}
}

Json McpServer::Initialize(const Json &parameters) const
{
	const auto &object = RequireObject(&parameters, "params");
	std::string protocol = OptionalString(object, "protocolVersion");
	if (protocol.empty())
		protocol = LatestProtocolVersion;
	const bool supported = std::any_of(
	        SupportedProtocolVersions.begin(),
	        SupportedProtocolVersions.end(),
	        [&protocol](const char *candidate) { return protocol == candidate; });
	if (!supported)
		protocol = LatestProtocolVersion;
	return Json(Object({
	        {"protocolVersion", protocol},
	        {"capabilities", Json(Object({{"tools", Json(Object({{"listChanged", false}}))}}))},
	        {"serverInfo", Json(Object({{"name", "agent86-mcp"}, {"version", ServerVersion}}))},
	        {"instructions", "Connects an MCP client to the active DOSBox-X CTTY BRIDGE. Commands and file writes mutate the emulated DOS session."},
	}));
}

Json McpServer::CallTool(const Json &parameters)
{
	const auto &params = RequireObject(&parameters, "params");
	const std::string &name = RequireString(params, "name");
	const auto found_arguments = params.find("arguments");
	Json empty_arguments(Object({}));
	const Json &arguments = found_arguments == params.end() ? empty_arguments
	                                                       : found_arguments->second;
	const auto &args = RequireObject(&arguments, "arguments");

	std::string summary;
	try {
		std::string result;
		if (name == "run_dos_command") {
			RequireOnlyFields(args, {"command"});
			const std::string &command = RequireString(args, "command");
			summary = LogCommandsEnabled() ? command : "<command redacted>";
			result = bridge.RunCommand(command);
		} else if (name == "write_dos_file") {
			RequireOnlyFields(args, {"filename", "content"});
			const std::string &filename = RequireString(args, "filename");
			const std::string &content = RequireString(args, "content");
			summary = filename;
			result = bridge.WriteFile(filename, content);
		} else if (name == "release_dos_console") {
			if (!args.empty())
				throw std::invalid_argument("release_dos_console accepts no arguments");
			result = bridge.ReleaseConsole();
		} else if (name == "capture_graphics_screen") {
			if (!args.empty())
				throw std::invalid_argument("capture_graphics_screen accepts no arguments");
			(void)bridge.RunControl(name);
			std::string status;
			for (int attempt = 0; attempt < 50; attempt++) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				status = bridge.RunControl("get_graphics_screen_result");
				if (status != "PENDING") break;
			}
			if (status == "PENDING")
				throw BridgeError("DOSBox-X did not render a screenshot within 5 seconds");
			if (status.rfind("ERROR\n", 0) == 0)
				throw BridgeError(status.substr(6));
			if (status.rfind("READY\n", 0) != 0)
				throw BridgeError("DOSBox-X returned an invalid screenshot status");
			std::error_code error;
			const auto path = std::filesystem::canonical(
			        std::filesystem::u8path(status.substr(6)), error);
			if (error || path.extension() != ".png" ||
			    !std::filesystem::is_regular_file(path, error) || error)
				throw BridgeError("DOSBox-X returned an invalid screenshot path");
			const std::string png = ReadBoundedBinaryFile(path, MaxScreenshotBytes);
			if (png.size() < 8 || png.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) != 0)
				throw BridgeError("DOSBox-X screenshot is not a valid PNG file");
			RecordOutcome(name, path.u8string(), "ok");
			return ImageResult(png);
		} else if (name == "swap_floppy_drive" || name == "swap_cd_drive" ||
		           name == "change_floppy_image" || name == "change_cd_image" ||
		           name == "create_blank_disk_image" || name == "list_mounted_drives" ||
		           name == "get_ide_status" || name == "rescan_all_drives" ||
		           name == "export_text_screen" || name == "get_text_screen") {
			if (!args.empty())
				throw std::invalid_argument(name + " accepts no arguments");
			result = bridge.RunControl(name);
		} else if (name == "mount_host_folder") {
			RequireOnlyFields(args, {"drive", "path", "kind"});
			const std::string drive = RequireDriveLetter(args);
			const std::string path = RequireHostDirectory(args);
			const std::string &kind = RequireString(args, "kind");
			if (kind != "local" && kind != "cdrom" && kind != "floppy")
				throw std::invalid_argument("kind must be local, cdrom, or floppy");
			summary = drive + ": " + path;
			result = bridge.RunControl(name, drive + "\n" + kind + "\n" + path);
		} else if (name == "unmount_drive") {
			RequireOnlyFields(args, {"drive"});
			const std::string drive = RequireDriveLetter(args);
			summary = drive + ":";
			result = bridge.RunControl(name, drive);
		} else if (name == "move_mouse") {
			RequireOnlyFields(args, {"x", "y"});
			const int x = RequireBoundedInteger(args, "x", 65535);
			const int y = RequireBoundedInteger(args, "y", 65535);
			summary = std::to_string(x) + "," + std::to_string(y);
			result = bridge.RunControl(name, std::to_string(x) + "\n" + std::to_string(y));
		} else if (name == "click_mouse") {
			RequireOnlyFields(args, {"button"});
			const std::string &button = RequireString(args, "button");
			if (button != "left" && button != "right" && button != "middle")
				throw std::invalid_argument("button must be left, right, or middle");
			summary = button;
			result = bridge.RunControl(name, button);
		} else if (name == "set_floppy_image") {
			RequireOnlyFields(args, {"drive", "path"});
			const std::string drive = RequireDriveLetter(args);
			if (drive != "A" && drive != "B")
				throw std::invalid_argument("floppy drive must be A or B");
			const auto path = RequireExistingHostFile(
			        args, {".ima", ".img", ".xdf", ".fdi", ".hdm", ".nfd", ".d88", ".td0"});
			summary = drive + ": " + path.u8string();
			result = bridge.RunControl(name, drive + "\n" + path.u8string());
		} else if (name == "set_cd_image") {
			RequireOnlyFields(args, {"drive", "path"});
			const std::string drive = RequireDriveLetter(args);
			if (drive[0] < 'C')
				throw std::invalid_argument("CD drive must be C through Y");
			const auto path = RequireExistingHostFile(
			        args, {".iso", ".cue", ".bin", ".chd", ".mdf", ".gog", ".ins", ".inst"});
			summary = drive + ": " + path.u8string();
			result = bridge.RunControl(name, drive + "\n" + path.u8string());
		} else if (name == "create_disk_image") {
			RequireOnlyFields(args, {"path", "format"});
			const std::string &format = RequireString(args, "format");
			static const std::set<std::string> formats = {
			        "fd_360", "fd_400", "fd_720", "fd_1200", "fd_1440", "fd_2880",
			        "hd_250", "hd_520", "hd_1gig", "hd_2gig", "hd_4gig", "hd_8gig"};
			if (formats.find(format) == formats.end())
				throw std::invalid_argument("format is not a supported DOSBox-X disk image template");
			const auto path = RequireNewDiskImagePath(args);
			summary = format + ": " + path.u8string();
			result = bridge.RunControl(name, format + "\n" + path.u8string(),
			                           std::chrono::seconds(60));
		} else if (name == "type_text") {
			RequireOnlyFields(args, {"text"});
			const std::string &text = RequireString(args, "text");
			if (text.empty() || text.size() > 4096 || text.find('\0') != std::string::npos)
				throw std::invalid_argument("text must contain 1 to 4096 characters without NUL bytes");
			summary = "<typed text redacted>";
			result = bridge.RunControl(name, text);
		} else {
			throw std::invalid_argument("unknown tool: " + name);
		}
		RecordOutcome(name, summary, "ok");
		return TextResult(result);
	} catch (const std::exception &error) {
		RecordOutcome(name, summary, "FAILED");
		return TextResult(error.what(), true);
	}
}

void McpServer::RecordOutcome(const std::string &tool,
	                          const std::string &summary,
	                          const std::string &outcome) noexcept
{
	try {
		logger.Record(tool, summary, outcome);
	} catch (...) {
		// Logging must not hide the actual DOS operation result.
	}
}

} // namespace Agent86
