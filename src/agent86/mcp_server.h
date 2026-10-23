// Copyright (c) 2026 Michael P. Burgus - https://github.com/NeuralDrifter
#pragma once

#include "dos_bridge_client.h"
#include "json.h"
#include "session_logger.h"

#include <istream>
#include <ostream>

namespace Agent86 {

class McpServer {
public:
	McpServer(DosBridgeClient &bridge, SessionLogger &logger);

	int Run(std::istream &input, std::ostream &output, std::ostream &errors);
	Json HandleRequest(const Json &request);

private:
	Json CallTool(const Json &parameters);
	Json Initialize(const Json &parameters) const;
	void RecordOutcome(const std::string &tool,
	                   const std::string &summary,
	                   const std::string &outcome) noexcept;

	DosBridgeClient &bridge;
	SessionLogger &logger;
};

} // namespace Agent86
