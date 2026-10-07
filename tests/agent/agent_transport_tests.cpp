#if defined(C_DEBUG) && defined(C_DOSBOX_AGENT) && !defined(WIN32)
#include "../../src/agent/transport/rpc_transport.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

class UnixSocketTransportTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Keep the path short: sun_path is only 104 bytes on macOS.
        char directory_template[] = "/tmp/dxaXXXXXX";
        ASSERT_NE(nullptr, mkdtemp(directory_template));
        directory = directory_template;
        endpoint = directory + "/agent.sock";
        config.transport = dosbox_agent::AgentTransport::UnixSocket;
        config.endpoint = endpoint;
        config.max_message_bytes = 64;
    }

    void TearDown() override
    {
        unlink(endpoint.c_str());
        rmdir(directory.c_str());
    }

    std::unique_ptr<dosbox_agent::IRpcTransport> StartTransport(std::string* error)
    {
        std::unique_ptr<dosbox_agent::IRpcTransport> transport =
                dosbox_agent::AGENT_CreateLocalTransport(dosbox_agent::AgentTransport::UnixSocket);
        if (!transport->Start(config, [](const std::string& request) { return "echo:" + request; }, error))
            transport.reset();
        return transport;
    }

    int Connect() const
    {
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un address;
        std::memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, endpoint.c_str(), sizeof(address.sun_path) - 1);
        if (fd >= 0 && connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            close(fd);
            return -1;
        }
#ifdef SO_NOSIGPIPE
        const int no_sigpipe = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
        return fd;
    }

    static bool Send(const int fd, const std::string& data)
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        return send(fd, data.data(), data.size(), flags) == static_cast<ssize_t>(data.size());
    }

    // Reads one line, or returns "<closed>" / "<timeout>" so a broken
    // transport fails the test instead of hanging the test run.
    static std::string ReadLine(const int fd)
    {
        std::string line;
        for (;;) {
            pollfd ready;
            ready.fd = fd;
            ready.events = POLLIN;
            ready.revents = 0;
            if (poll(&ready, 1, 5000) <= 0)
                return "<timeout>";
            char character = 0;
            const ssize_t received = recv(fd, &character, 1, 0);
            if (received <= 0)
                return "<closed>";
            if (character == '\n')
                return line;
            line += character;
        }
    }

    std::string directory;
    std::string endpoint;
    dosbox_agent::AgentConfig config;
};

TEST_F(UnixSocketTransportTest, AnswersNewlineDelimitedRequestsAndCleansUp)
{
    std::string error;
    std::unique_ptr<dosbox_agent::IRpcTransport> transport = StartTransport(&error);
    ASSERT_TRUE(transport != nullptr) << error;

    struct stat info;
    ASSERT_EQ(0, lstat(endpoint.c_str(), &info));
    EXPECT_TRUE(S_ISSOCK(info.st_mode));
    EXPECT_EQ(static_cast<mode_t>(S_IRUSR | S_IWUSR), info.st_mode & 0777);

    const int client = Connect();
    ASSERT_GE(client, 0);
    ASSERT_TRUE(Send(client, "one\ntwo\r\n"));
    EXPECT_EQ("echo:one", ReadLine(client));
    EXPECT_EQ("echo:two", ReadLine(client));
    close(client);

    transport->Stop();
    EXPECT_NE(0, lstat(endpoint.c_str(), &info));
}

TEST_F(UnixSocketTransportTest, AnswersOversizedRequestThenCloses)
{
    std::string error;
    std::unique_ptr<dosbox_agent::IRpcTransport> transport = StartTransport(&error);
    ASSERT_TRUE(transport != nullptr) << error;

    const int client = Connect();
    ASSERT_GE(client, 0);
    ASSERT_TRUE(Send(client, std::string(100, 'a')));
    EXPECT_EQ("echo:" + std::string(config.max_message_bytes + 1, 'x'), ReadLine(client));
    EXPECT_EQ("<closed>", ReadLine(client));
    close(client);
}

TEST_F(UnixSocketTransportTest, ReplacesStaleSocketButRefusesLiveEndpoint)
{
    // A bound socket that is closed without unlinking is what a crash leaves.
    const int stale = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address;
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, endpoint.c_str(), sizeof(address.sun_path) - 1);
    ASSERT_EQ(0, bind(stale, reinterpret_cast<const sockaddr*>(&address), sizeof(address)));
    close(stale);

    std::string error;
    std::unique_ptr<dosbox_agent::IRpcTransport> first = StartTransport(&error);
    ASSERT_TRUE(first != nullptr) << error;

    std::unique_ptr<dosbox_agent::IRpcTransport> second = StartTransport(&error);
    EXPECT_TRUE(second == nullptr);
    EXPECT_NE(std::string::npos, error.find("in use"));

    const int client = Connect();
    ASSERT_GE(client, 0);
    ASSERT_TRUE(Send(client, "still-first\n"));
    EXPECT_EQ("echo:still-first", ReadLine(client));
    close(client);
}

TEST_F(UnixSocketTransportTest, LeavesNonSocketFilesAlone)
{
    FILE* file = std::fopen(endpoint.c_str(), "w");
    ASSERT_TRUE(file != NULL);
    std::fclose(file);

    std::string error;
    EXPECT_TRUE(StartTransport(&error) == nullptr);
    EXPECT_NE(std::string::npos, error.find("not a socket"));

    struct stat info;
    ASSERT_EQ(0, lstat(endpoint.c_str(), &info));
    EXPECT_TRUE(S_ISREG(info.st_mode));
}

TEST_F(UnixSocketTransportTest, StopsWhileClientIsIdle)
{
    std::string error;
    std::unique_ptr<dosbox_agent::IRpcTransport> transport = StartTransport(&error);
    ASSERT_TRUE(transport != nullptr) << error;

    const int client = Connect();
    ASSERT_GE(client, 0);
    ASSERT_TRUE(Send(client, "ping\n"));
    EXPECT_EQ("echo:ping", ReadLine(client));

    // The worker is now blocked waiting for the next request.
    transport->Stop();
    EXPECT_EQ("<closed>", ReadLine(client));
    close(client);
}

TEST_F(UnixSocketTransportTest, RejectsEndpointsThatDoNotFitSunPath)
{
    config.endpoint = "/" + std::string(sizeof(sockaddr_un::sun_path), 'a');
    std::string error;
    EXPECT_TRUE(StartTransport(&error) == nullptr);
    EXPECT_NE(std::string::npos, error.find("too long"));
}

} // namespace
#endif // C_DEBUG && C_DOSBOX_AGENT && !WIN32
