#include "agent/debugger_output_parser.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string ReadFixture(const char* const name)
{
    std::ifstream input(std::string("tests/agent/fixtures/") + name, std::ios::in | std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

TEST(DebuggerOutputParser, RejectsMissingRegisterFieldsWithoutPartialResult)
{
    const std::string fixture = ReadFixture("debugger_output_missing_field.txt");
    const dosbox_agent::DebuggerOutputParseResult result =
            dosbox_agent::AGENT_ParseDebuggerRegisterOutput(fixture);

    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(result.registers.empty());
    EXPECT_EQ(fixture, result.unparsed_output);
}

TEST(DebuggerOutputParser, RejectsAbnormalSpacingWithoutPartialResult)
{
    const std::string fixture = ReadFixture("debugger_output_abnormal_spacing.txt");
    const dosbox_agent::DebuggerOutputParseResult result =
            dosbox_agent::AGENT_ParseDebuggerRegisterOutput(fixture);

    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(result.registers.empty());
    EXPECT_EQ(fixture, result.unparsed_output);
}

} // namespace
