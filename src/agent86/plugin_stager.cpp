// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#include "plugin_stager.h"

#include <array>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

namespace Agent86 {
namespace {

const std::array<const char *, 3> Vendors = {{"Anthropic", "Google", "OpenAI"}};
const std::set<std::string> ExcludedDirectories = {
        ".pytest_cache", "__pycache__", "build", "scripts", "tests"};
const std::set<std::string> ExcludedFiles = {"pyproject.toml"};

bool HasEggInfoSuffix(const std::filesystem::path &path)
{
	const std::string name = path.filename().string();
	return name.size() >= 9 && name.compare(name.size() - 9, 9, ".egg-info") == 0;
}

bool IsExcluded(const std::filesystem::directory_entry &entry)
{
	const std::string name = entry.path().filename().string();
	if (entry.is_directory())
		return ExcludedDirectories.count(name) != 0 || HasEggInfoSuffix(entry.path());
	return ExcludedFiles.count(name) != 0 || entry.path().extension() == ".pyc";
}

void CopyTree(const std::filesystem::path &source,
	          const std::filesystem::path &destination)
{
	std::error_code error;
	std::filesystem::create_directories(destination, error);
	if (error)
		throw std::runtime_error("cannot create plugin output directory: " +
		                         error.message());
	for (const auto &entry : std::filesystem::directory_iterator(source)) {
		if (IsExcluded(entry))
			continue;
		const auto target = destination / entry.path().filename();
		if (entry.is_directory())
			CopyTree(entry.path(), target);
		else if (entry.is_regular_file()) {
			std::filesystem::copy_file(
			        entry.path(), target, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
				throw std::runtime_error("cannot copy plugin file " +
				                         entry.path().string() + ": " + error.message());
		}
	}
}

void RequireDirectory(const std::filesystem::path &path, const char *description)
{
	if (!std::filesystem::is_directory(path))
		throw std::invalid_argument(std::string(description) + " does not exist: " +
		                            path.string());
}

void ReplaceInJsonManifests(const std::filesystem::path &plugin_root,
	                        const std::string &needle,
	                        const std::string &replacement)
{
	if (needle == replacement)
		return;
	for (const auto &entry : std::filesystem::recursive_directory_iterator(plugin_root)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".json")
			continue;

		std::string content;
		{
			std::ifstream input(entry.path(), std::ios::binary);
			if (!input)
				throw std::runtime_error("cannot read staged plugin manifest: " +
				                         entry.path().string());
			content.assign((std::istreambuf_iterator<char>(input)),
			               std::istreambuf_iterator<char>());
		}
		if (content.find(needle) == std::string::npos)
			continue;

		size_t position = 0;
		while ((position = content.find(needle, position)) != std::string::npos) {
			content.replace(position, needle.size(), replacement);
			position += replacement.size();
		}

		std::ofstream output(entry.path(), std::ios::binary | std::ios::trunc);
		if (!output || !(output << content))
			throw std::runtime_error("cannot configure staged plugin manifest: " +
			                         entry.path().string());
	}
}

void ConfigureMcpBinaryNames(const std::filesystem::path &plugin_root,
	                         const std::string &binary_name)
{
	ReplaceInJsonManifests(plugin_root, "agent86-mcp", binary_name);
}

std::string JsonEscape(const std::string &value)
{
	std::string escaped;
	escaped.reserve(value.size());
	for (const char character : value) {
		if (character == '\\' || character == '"')
			escaped.push_back('\\');
		escaped.push_back(character);
	}
	return escaped;
}

// Codex never expands ${CLAUDE_PLUGIN_ROOT} in an MCP server command, so the
// OpenAI manifest is staged with the absolute path of the staged plugin.
void ConfigurePluginRootToken(const std::filesystem::path &plugin_root)
{
	ReplaceInJsonManifests(plugin_root,
	                       "${CLAUDE_PLUGIN_ROOT}",
	                       JsonEscape(plugin_root.string()));
}

} // namespace

void StagePlugins(const std::filesystem::path &requested_source_root,
	              const std::filesystem::path &requested_output_root,
	              const std::filesystem::path &requested_mcp_binary)
{
	const auto source_root = std::filesystem::weakly_canonical(requested_source_root);
	const auto output_root = std::filesystem::absolute(requested_output_root).lexically_normal();
	const auto mcp_binary = std::filesystem::weakly_canonical(requested_mcp_binary);
	RequireDirectory(source_root / "llm_plugins", "plugin source directory");
	if (!std::filesystem::is_regular_file(mcp_binary))
		throw std::invalid_argument("MCP binary does not exist: " + mcp_binary.string());
	if (output_root == source_root || output_root == source_root / "llm_plugins")
		throw std::invalid_argument("plugin output must not overwrite the source tree");

	const auto staged_root = output_root / "llm_plugins";
	for (const char *vendor : Vendors) {
		const auto source = source_root / "llm_plugins" / vendor;
		RequireDirectory(source, "vendor plugin directory");
		const auto destination = staged_root / vendor;
		std::error_code error;
		std::filesystem::remove_all(destination, error);
		if (error)
			throw std::runtime_error("cannot clean staged plugin directory: " +
			                         error.message());
		CopyTree(source, destination);

		const auto plugin_bin = destination / "dos-bridge" / "bin";
		std::filesystem::create_directories(plugin_bin, error);
		if (error)
			throw std::runtime_error("cannot create plugin binary directory: " +
			                         error.message());
#if defined(_WIN32)
		const auto binary_name = "agent86-mcp.exe";
#else
		const auto binary_name = "agent86-mcp";
#endif
		ConfigureMcpBinaryNames(destination / "dos-bridge", binary_name);
		if (std::string(vendor) == "OpenAI")
			ConfigurePluginRootToken(destination / "dos-bridge");
		std::filesystem::copy_file(mcp_binary,
		                           plugin_bin / binary_name,
		                           std::filesystem::copy_options::overwrite_existing,
		                           error);
		if (error)
			throw std::runtime_error("cannot stage native MCP binary: " + error.message());
	}
}

} // namespace Agent86
