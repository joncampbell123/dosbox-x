#include "agent/agent_bridge.h"
#include "agent/agent_protocol.h"
#include "agent/agent_server.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

dosbox_agent::AgentConfig MakeTestConfig()
{
    dosbox_agent::AgentConfig config;
    config.transport = dosbox_agent::AgentTransport::NamedPipe;
    config.endpoint = R"(\\.\pipe\dosbox-agent-unit-test)";
    config.dosbox_workdir = "tests/agent/runtime";
    config.request_timeout_ms = 5000;
    config.max_message_bytes = 1024;
    config.max_memory_read_bytes = 64;
    config.max_trace_events = 16;
    return config;
}

std::string StartFixtureSession(dosbox_agent::AgentServer* server)
{
    const std::string response = server->HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"start\",\"method\":\"session.start\","
            "\"params\":{\"target\":{\"command\":\"AGENTFIX.COM\",\"arguments\":[]},"
            "\"mounts\":[{\"drive\":\"C\",\"host_path\":\"tests/agent/runtime\"}],\"break_at\":\"entry\"}}");
    EXPECT_NE(std::string::npos, response.find("\"session_id\":\"ses-1\""));
    EXPECT_NE(std::string::npos, response.find("\"state\":\"stopped\""));
    EXPECT_NE(std::string::npos, response.find("\"kind\":\"startup\""));
    return response;
}

TEST(AgentConfig, LoadsOnlyTheExplicitConfigFile)
{
    dosbox_agent::AgentConfig config;
    std::string error;

    ASSERT_TRUE(dosbox_agent::AGENT_LoadConfigFile("tests/agent/agent-test.env", &config, &error)) << error;
    EXPECT_EQ(dosbox_agent::AgentTransport::NamedPipe, config.transport);
    EXPECT_EQ("\\\\.\\pipe\\dosbox-agent-test", config.endpoint);
    EXPECT_EQ(5000U, config.request_timeout_ms);
    EXPECT_EQ(static_cast<std::size_t>(1048576), config.max_message_bytes);
    EXPECT_EQ(static_cast<std::size_t>(65536), config.max_memory_read_bytes);
    EXPECT_EQ(static_cast<std::size_t>(10000), config.max_trace_events);
    EXPECT_TRUE(config.test_profile);
    EXPECT_NE(std::string::npos, config.dosbox_workdir.find("tests"));
    EXPECT_NE(std::string::npos, config.dosbox_workdir.find("runtime"));

    EXPECT_FALSE(dosbox_agent::AGENT_LoadConfigFile("tests/agent/missing.env", &config, &error));
    EXPECT_FALSE(error.empty());
}

TEST(AgentConfig, StartupLogContainsResolvedLimits)
{
    dosbox_agent::AgentConfig config;
    std::string error;
    ASSERT_TRUE(dosbox_agent::AGENT_LoadConfigFile("tests/agent/agent-test.env", &config, &error)) << error;

    const std::string log = dosbox_agent::AGENT_FormatStartupLog(config);
    EXPECT_NE(std::string::npos, log.find("\"endpoint\":\"\\\\\\\\.\\\\pipe\\\\dosbox-agent-test\""));
    EXPECT_NE(std::string::npos, log.find("\"request_timeout_ms\":5000"));
    EXPECT_NE(std::string::npos, log.find("\"max_memory_read_bytes\":65536"));
}

TEST(AgentBridge, RejectsPumpFromNonEmulationThread)
{
    dosbox_agent::EmulationThreadQueue queue;
    ASSERT_TRUE(queue.BindToCurrentThread());

    std::atomic<int> calls(0);
    ASSERT_NE(0U, queue.Submit([&calls](const std::uint64_t) { ++calls; }));

    std::size_t foreign_pump_count = 1;
    std::thread foreign_thread([&queue, &foreign_pump_count]() {
        foreign_pump_count = queue.Pump();
    });
    foreign_thread.join();

    EXPECT_EQ(0U, foreign_pump_count);
    EXPECT_EQ(0, calls.load());
    EXPECT_EQ(1U, queue.Pump());
    EXPECT_EQ(1, calls.load());
}

TEST(AgentBridge, PreservesSubmissionOrderUnderConcurrentLoad)
{
    const int command_count = 1000;
    dosbox_agent::EmulationThreadQueue queue;
    ASSERT_TRUE(queue.BindToCurrentThread());

    std::vector<std::uint64_t> completed;
    completed.reserve(command_count);
    std::atomic<int> submitted(0);
    std::atomic<int> rejected(0);
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back([&queue, &completed, &submitted, &rejected, command_count]() {
            for (;;) {
                const int index = submitted.fetch_add(1);
                if (index >= command_count)
                    return;
                if (queue.Submit([&completed](const std::uint64_t sequence) {
                        completed.push_back(sequence);
                    }) == 0) {
                    ++rejected;
                }
            }
        });
    }
    for (std::vector<std::thread>::iterator it = workers.begin(); it != workers.end(); ++it)
        it->join();

    EXPECT_EQ(0, rejected.load());
    EXPECT_EQ(static_cast<std::size_t>(command_count), queue.Pump());
    ASSERT_EQ(static_cast<std::size_t>(command_count), completed.size());
    for (std::size_t index = 0; index < completed.size(); ++index)
        EXPECT_EQ(static_cast<std::uint64_t>(index + 1), completed[index]);
}

TEST(AgentBridge, DelaysRetriesAndDropsPendingCommandsOnShutdown)
{
    dosbox_agent::EmulationThreadQueue queue;
    ASSERT_TRUE(queue.BindToCurrentThread());

    std::atomic<int> calls(0);
    ASSERT_NE(0U, queue.SubmitAfter(20, [&calls](const std::uint64_t) { ++calls; }));
    EXPECT_EQ(0U, queue.Pump());
    EXPECT_EQ(0, calls.load());

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(1U, queue.Pump());
    EXPECT_EQ(1, calls.load());

    ASSERT_NE(0U, queue.SubmitAfter(20, [&calls](const std::uint64_t) { ++calls; }));
    queue.Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(0U, queue.Pump());
    EXPECT_EQ(1, calls.load());
}

TEST(AgentProtocol, RejectsInvalidRequestsAndOversizedMessages)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;

    EXPECT_NE(std::string::npos, server.HandleJsonRpc("{\"method\":\"agent.capabilities\"}").find("\"code\":-32600"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":\"unknown\",\"method\":\"unknown.method\"}").find("\"code\":-32601"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc("{\"jsonrpc\":\"2.0\",\"id\":\"bad\",\"method\":\"agent.capabilities\",\"params\":[]}").find("\"code\":-32600"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(std::string(1025, 'x')).find("REQUEST_TOO_LARGE"));
}

TEST(AgentProtocol, ReportsBuildCapabilitiesAndLimits)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;

    const std::string response = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"capabilities\",\"method\":\"agent.capabilities\"}");
    EXPECT_NE(std::string::npos, response.find("\"debugger\":true"));
#ifdef C_HEAVY_DEBUG
    EXPECT_NE(std::string::npos, response.find("\"cpu\":true"));
#else
    EXPECT_NE(std::string::npos, response.find("\"cpu\":false"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"trace\",\"method\":\"trace.start\",\"params\":{}}").find("CAPABILITY_UNAVAILABLE"));
#endif
    EXPECT_NE(std::string::npos, response.find("\"segmented\""));
    EXPECT_NE(std::string::npos, response.find("\"linear\""));
    EXPECT_NE(std::string::npos, response.find("\"physical\""));
    EXPECT_NE(std::string::npos, response.find("\"max_memory_read_bytes\":64"));
}

TEST(AgentLifecycle, StopsAndRestartsTheSameServer)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;
    EXPECT_TRUE(server.IsStarted());

    server.Stop();
    EXPECT_FALSE(server.IsStarted());

    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;
    EXPECT_TRUE(server.IsStarted());
    server.Stop();
    EXPECT_FALSE(server.IsStarted());
}

TEST(AgentSession, EnforcesSingleSessionAndStartupStopState)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;
    StartFixtureSession(&server);

    const std::string status = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"status\",\"method\":\"session.status\",\"params\":{\"session_id\":\"ses-1\"}}");
    EXPECT_NE(std::string::npos, status.find("\"state\":\"stopped\""));
    EXPECT_NE(std::string::npos, status.find("\"state_revision\":1"));

    const std::string second_start = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"second\",\"method\":\"session.start\","
            "\"params\":{\"target\":{\"command\":\"SECOND.COM\"},\"mounts\":[{\"drive\":\"C\",\"host_path\":\"tests/agent/runtime\"}],\"break_at\":\"entry\"}}");
    EXPECT_NE(std::string::npos, second_start.find("SESSION_BUSY"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"status-after\",\"method\":\"session.status\",\"params\":{\"session_id\":\"ses-1\"}}").find("\"state_revision\":1"));
}

TEST(AgentSession, ReusesCompletedStartRequestAndRejectsConflicts)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;

    const std::string request =
            "{\"jsonrpc\":\"2.0\",\"id\":\"start\",\"method\":\"session.start\","
            "\"params\":{\"target\":{\"command\":\"AGENTFIX.COM\",\"arguments\":[]},"
            "\"mounts\":[{\"drive\":\"C\",\"host_path\":\"tests/agent/runtime\"}],\"break_at\":\"entry\"}}";
    const std::string first = server.HandleJsonRpc(request);
    const std::string retry = server.HandleJsonRpc(request);
    EXPECT_EQ(first, retry);
    EXPECT_NE(std::string::npos, retry.find("\"session_id\":\"ses-1\""));

    const std::string conflict = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"start\",\"method\":\"session.start\","
            "\"params\":{\"target\":{\"command\":\"OTHER.COM\",\"arguments\":[]},"
            "\"mounts\":[{\"drive\":\"C\",\"host_path\":\"tests/agent/runtime\"}],\"break_at\":\"entry\"}}");
    EXPECT_NE(std::string::npos, conflict.find("REQUEST_ID_CONFLICT"));
}

TEST(AgentSession, BlocksStateOperationsWhileRunningAndWaitsForPause)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;
    StartFixtureSession(&server);

    const std::string continued = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"continue\",\"method\":\"execution.continue\",\"params\":{\"session_id\":\"ses-1\"}}");
    EXPECT_NE(std::string::npos, continued.find("\"operation_id\":\"op-1\""));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"registers\",\"method\":\"state.get_registers\",\"params\":{\"session_id\":\"ses-1\"}}").find("TARGET_RUNNING"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"read\",\"method\":\"memory.read\",\"params\":{\"session_id\":\"ses-1\"}}").find("TARGET_RUNNING"));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"write\",\"method\":\"memory.write\",\"params\":{\"session_id\":\"ses-1\"}}").find("TARGET_RUNNING"));

    const std::string timed_out = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"wait\",\"method\":\"execution.wait\",\"params\":{\"session_id\":\"ses-1\",\"operation_id\":\"op-1\",\"timeout_ms\":1}}");
    EXPECT_NE(std::string::npos, timed_out.find("\"running\":true"));
    EXPECT_EQ(std::string::npos, timed_out.find("stop_reason"));

    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"pause\",\"method\":\"execution.pause\",\"params\":{\"session_id\":\"ses-1\"}}").find("\"operation_id\":\"op-2\""));
    EXPECT_NE(std::string::npos, server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"wait-pause\",\"method\":\"execution.wait\",\"params\":{\"session_id\":\"ses-1\",\"operation_id\":\"op-2\",\"timeout_ms\":1}}").find("\"kind\":\"pause\""));
}

TEST(AgentSession, ReusesCompletedRequestResultsAndRejectsConflicts)
{
    dosbox_agent::AgentServer server;
    std::string error;
    ASSERT_TRUE(server.StartForTest(MakeTestConfig(), &error)) << error;
    StartFixtureSession(&server);

    const std::string request =
            "{\"jsonrpc\":\"2.0\",\"id\":\"operation\",\"method\":\"execution.continue\",\"params\":{\"session_id\":\"ses-1\"}}";
    const std::string first = server.HandleJsonRpc(request);
    const std::string retry = server.HandleJsonRpc(request);
    EXPECT_EQ(first, retry);
    EXPECT_NE(std::string::npos, retry.find("\"operation_id\":\"op-1\""));

    const std::string conflict = server.HandleJsonRpc(
            "{\"jsonrpc\":\"2.0\",\"id\":\"operation\",\"method\":\"execution.pause\",\"params\":{\"session_id\":\"ses-1\"}}");
    EXPECT_NE(std::string::npos, conflict.find("REQUEST_ID_CONFLICT"));
}

} // namespace
