#include "config.h"

#if defined(C_DEBUG) && defined(C_DOSBOX_AGENT)
#include "rpc_transport.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef WIN32
#include <windows.h>
#include <sddl.h>
#pragma comment(lib, "Advapi32.lib")
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace dosbox_agent {
namespace {

class NamedPipeTransport final : public IRpcTransport {
public:
    NamedPipeTransport() : impl(new Impl()) {}
    ~NamedPipeTransport() override { Stop(); }

    const char* Name() const override { return "named_pipe"; }

    bool ValidateConfiguration(const AgentConfig& config,
                               std::string* error) const override
    {
        if (config.endpoint.compare(0, 9, "\\\\.\\pipe\\") != 0 || config.endpoint.size() == 9) {
            if (error != NULL)
                *error = "Named pipe endpoint must use \\\\.\\pipe\\<name>";
            return false;
        }
        return true;
    }

    bool Start(const AgentConfig& config,
               RequestHandler handler,
               std::string* error) override
    {
        if (!ValidateConfiguration(config, error) || !handler)
            return false;

#ifndef WIN32
        if (error != NULL)
            *error = "Windows named pipes are unavailable on this platform";
        return false;
#else
        std::unique_lock<std::mutex> lock(impl->mutex);
        if (impl->listener.joinable()) {
            if (error != NULL)
                *error = "Named pipe transport is already running";
            return false;
        }

        impl->config = config;
        impl->handler = handler;
        impl->stopping = false;
        impl->ready = false;
        impl->start_succeeded = false;
        impl->start_error.clear();
        impl->listener = std::thread(&NamedPipeTransport::Run, this);
        impl->ready_condition.wait(lock, [this]() { return impl->ready; });
        if (impl->start_succeeded)
            return true;

        const std::string start_error = impl->start_error;
        lock.unlock();
        impl->listener.join();
        if (error != NULL)
            *error = start_error;
        return false;
#endif
    }

    void Stop() override
    {
#ifdef WIN32
        std::thread listener;
        std::vector<std::thread> client_workers;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (!impl->listener.joinable())
                return;
            impl->stopping = true;
            for (std::vector<HANDLE>::const_iterator it = impl->active_pipes.begin();
                 it != impl->active_pipes.end(); ++it)
                CancelIoEx(*it, NULL);
            listener.swap(impl->listener);
        }
        listener.join();
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            client_workers.swap(impl->client_workers);
        }
        for (std::vector<std::thread>::iterator it = client_workers.begin();
             it != client_workers.end(); ++it)
            if (it->joinable())
                it->join();
#endif
    }

private:
    class Impl {
    public:
        std::mutex mutex;
        std::condition_variable ready_condition;
        AgentConfig config;
        RequestHandler handler;
        std::thread listener;
        bool stopping = false;
        bool ready = false;
        bool start_succeeded = false;
        std::string start_error;
#ifdef WIN32
        HANDLE active_pipe = INVALID_HANDLE_VALUE;
        std::vector<HANDLE> active_pipes;
        std::vector<std::thread> client_workers;
#endif
    };

    std::unique_ptr<Impl> impl;

#ifdef WIN32
    static bool WriteAll(const HANDLE pipe, const std::string& message)
    {
        std::size_t offset = 0;
        while (offset < message.size()) {
            const DWORD remaining = static_cast<DWORD>((std::min)(message.size() - offset,
                                                                    static_cast<std::size_t>(0xffffffffu)));
            DWORD written = 0;
            if (!WriteFile(pipe, message.data() + offset, remaining, &written, NULL) || written == 0)
                return false;
            offset += written;
        }
        return true;
    }

    HANDLE CreatePipe(std::string* error)
    {
        PSECURITY_DESCRIPTOR descriptor = NULL;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:P(A;;GA;;;OW)",
                                                                    SDDL_REVISION_1,
                                                                    &descriptor,
                                                                    NULL)) {
            *error = "Unable to create named pipe security descriptor";
            return INVALID_HANDLE_VALUE;
        }

        SECURITY_ATTRIBUTES attributes;
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = descriptor;
        attributes.bInheritHandle = FALSE;
        const DWORD buffer_size = static_cast<DWORD>((std::min)(impl->config.max_message_bytes,
                                                                 static_cast<std::size_t>(0xffffffffu)));
        HANDLE pipe = CreateNamedPipeA(impl->config.endpoint.c_str(),
                                       PIPE_ACCESS_DUPLEX,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES,
                                       buffer_size,
                                       buffer_size,
                                       0,
                                       &attributes);
        LocalFree(descriptor);
        if (pipe == INVALID_HANDLE_VALUE)
            *error = "Unable to create named pipe endpoint";
        return pipe;
    }

    void RemoveActivePipe(const HANDLE pipe)
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        for (std::vector<HANDLE>::iterator it = impl->active_pipes.begin();
             it != impl->active_pipes.end(); ++it) {
            if (*it == pipe) {
                impl->active_pipes.erase(it);
                break;
            }
        }
    }

    bool ServeClient(const HANDLE pipe)
    {
        std::string pending;
        char buffer[4096];
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(pipe, buffer, sizeof(buffer), &read, NULL) || read == 0)
                return false;
            pending.append(buffer, read);

            for (;;) {
                const std::string::size_type line_end = pending.find('\n');
                if (line_end == std::string::npos) {
                    if (pending.size() > impl->config.max_message_bytes) {
                        const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                        WriteAll(pipe, response);
                        return false;
                    }
                    break;
                }

                std::string request = pending.substr(0, line_end);
                pending.erase(0, line_end + 1);
                if (!request.empty() && request[request.size() - 1] == '\r')
                    request.erase(request.size() - 1);
                if (request.size() > impl->config.max_message_bytes) {
                    const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                    WriteAll(pipe, response);
                    return false;
                }
                if (!WriteAll(pipe, impl->handler(request) + "\n"))
                    return false;
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->stopping)
                return false;
        }
    }

    void ServeClientThread(const HANDLE pipe)
    {
        ServeClient(pipe);
        CloseHandle(pipe);
        RemoveActivePipe(pipe);
    }

    void Run()
    {
        bool first_pipe = true;
        for (;;) {
            std::string error;
            const HANDLE pipe = CreatePipe(&error);
            if (pipe == INVALID_HANDLE_VALUE) {
                std::lock_guard<std::mutex> lock(impl->mutex);
                if (first_pipe) {
                    impl->start_error = error;
                    impl->ready = true;
                    impl->ready_condition.notify_all();
                }
                return;
            }

            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->active_pipe = pipe;
                impl->active_pipes.push_back(pipe);
                if (first_pipe) {
                    impl->start_succeeded = true;
                    impl->ready = true;
                    impl->ready_condition.notify_all();
                    first_pipe = false;
                }
                if (impl->stopping) {
                    CloseHandle(pipe);
                    impl->active_pipe = INVALID_HANDLE_VALUE;
                    impl->active_pipes.pop_back();
                    return;
                }
            }

            const BOOL connected = ConnectNamedPipe(pipe, NULL);
            const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
            if (connected || connect_error == ERROR_PIPE_CONNECTED) {
                bool stopping = false;
                {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    stopping = impl->stopping;
                    if (!stopping)
                        impl->client_workers.push_back(std::thread(&NamedPipeTransport::ServeClientThread, this, pipe));
                }
                if (stopping) {
                    CloseHandle(pipe);
                    RemoveActivePipe(pipe);
                    return;
                }
            } else {
                CloseHandle(pipe);
                RemoveActivePipe(pipe);
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->active_pipe = INVALID_HANDLE_VALUE;
            if (impl->stopping)
                return;
        }
    }
#endif
};

// The POSIX counterpart of NamedPipeTransport: the same newline-delimited
// JSON-RPC framing, one worker thread per client, and access limited to the
// user running DOSBox-X. Apart from the peer credential check, only portable
// POSIX calls are used so that the same code serves Linux, macOS and the BSDs.
class UnixSocketTransport final : public IRpcTransport {
public:
    UnixSocketTransport() : impl(new Impl()) {}
    ~UnixSocketTransport() override { Stop(); }

    const char* Name() const override { return "unix_socket"; }

    bool ValidateConfiguration(const AgentConfig& config,
                               std::string* error) const override
    {
        if (config.endpoint.empty() || config.endpoint[0] != '/') {
            if (error != NULL)
                *error = "Unix socket endpoint must be an absolute path";
            return false;
        }
#ifndef WIN32
        // sun_path is 108 bytes on Linux but only 104 on macOS and the BSDs.
        if (config.endpoint.size() >= sizeof(sockaddr_un::sun_path)) {
            if (error != NULL)
                *error = "Unix socket endpoint path is too long for this platform";
            return false;
        }
#endif
        return true;
    }

    bool Start(const AgentConfig& config,
               RequestHandler handler,
               std::string* error) override
    {
        if (!ValidateConfiguration(config, error) || !handler)
            return false;

#ifdef WIN32
        if (error != NULL)
            *error = "Unix socket transport is unavailable on this platform";
        return false;
#else
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->listener.joinable()) {
            if (error != NULL)
                *error = "Unix socket transport is already running";
            return false;
        }

        std::string start_error;
        if (!OpenWakePipe(&start_error) || !Listen(config.endpoint, &start_error)) {
            CloseDescriptors();
            if (error != NULL)
                *error = start_error;
            return false;
        }

        impl->config = config;
        impl->handler = handler;
        impl->stopping = false;
        impl->listener = std::thread(&UnixSocketTransport::Run, this);
        return true;
#endif
    }

    void Stop() override
    {
#ifndef WIN32
        std::thread listener;
        std::vector<std::thread> client_workers;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (!impl->listener.joinable())
                return;
            impl->stopping = true;
            listener.swap(impl->listener);
        }

        // The wake pipe is never drained, so every thread polling it sees it
        // become readable: the listener and all client workers stop waiting.
        const char wake = 0;
        const ssize_t ignored = write(impl->wake_write_fd, &wake, 1);
        (void)ignored;

        listener.join();
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            client_workers.swap(impl->client_workers);
        }
        for (std::vector<std::thread>::iterator it = client_workers.begin();
             it != client_workers.end(); ++it)
            if (it->joinable())
                it->join();
        CloseDescriptors();
#endif
    }

private:
    class Impl {
    public:
        std::mutex mutex;
        AgentConfig config;
        RequestHandler handler;
        std::thread listener;
        bool stopping = false;
#ifndef WIN32
        std::string socket_path;
        dev_t socket_device = 0;
        ino_t socket_inode = 0;
        int listen_fd = -1;
        int wake_read_fd = -1;
        int wake_write_fd = -1;
        std::vector<std::thread> client_workers;
#endif
    };

    std::unique_ptr<Impl> impl;

#ifndef WIN32
    static std::string SystemError(const std::string& message)
    {
        return message + ": " + std::strerror(errno);
    }

    static void SetCloseOnExec(const int fd)
    {
        const int flags = fcntl(fd, F_GETFD);
        if (flags >= 0)
            fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    static void SetNonBlocking(const int fd)
    {
        const int flags = fcntl(fd, F_GETFL);
        if (flags >= 0)
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static sockaddr_un SocketAddress(const std::string& path)
    {
        sockaddr_un address;
        std::memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
        return address;
    }

    // Linux reports the peer's credentials through SO_PEERCRED. macOS and the
    // BSDs provide getpeereid() instead.
    static bool PeerIsCurrentUser(const int client)
    {
#if defined(__linux__)
        struct ucred credentials;
        socklen_t length = sizeof(credentials);
        if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0)
            return false;
        return credentials.uid == geteuid();
#else
        uid_t uid = 0;
        gid_t gid = 0;
        if (getpeereid(client, &uid, &gid) != 0)
            return false;
        return uid == geteuid();
#endif
    }

    bool OpenWakePipe(std::string* error)
    {
        int fds[2];
        if (pipe(fds) != 0) {
            *error = SystemError("Unable to create Unix socket wake pipe");
            return false;
        }
        SetCloseOnExec(fds[0]);
        SetCloseOnExec(fds[1]);
        SetNonBlocking(fds[1]);
        impl->wake_read_fd = fds[0];
        impl->wake_write_fd = fds[1];
        return true;
    }

    // A socket file left behind by a DOSBox-X that crashed refuses connections
    // and can be replaced. Anything else at the path is left alone.
    static bool RemoveStaleSocket(const std::string& path, std::string* error)
    {
        struct stat info;
        if (lstat(path.c_str(), &info) != 0) {
            if (errno == ENOENT)
                return true;
            *error = SystemError("Unable to inspect Unix socket endpoint " + path);
            return false;
        }
        if (!S_ISSOCK(info.st_mode)) {
            *error = "Unix socket endpoint " + path + " exists and is not a socket";
            return false;
        }

        const int probe = socket(AF_UNIX, SOCK_STREAM, 0);
        if (probe < 0) {
            *error = SystemError("Unable to create Unix socket");
            return false;
        }
        const sockaddr_un address = SocketAddress(path);
        const int connected = connect(probe, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        const int connect_error = errno;
        close(probe);
        if (connected == 0) {
            *error = "Unix socket endpoint " + path + " is in use by another process";
            return false;
        }
        if (connect_error == ENOENT)
            return true;
        if (connect_error != ECONNREFUSED) {
            errno = connect_error;
            *error = SystemError("Unable to probe Unix socket endpoint " + path);
            return false;
        }
        if (unlink(path.c_str()) != 0 && errno != ENOENT) {
            *error = SystemError("Unable to remove stale Unix socket endpoint " + path);
            return false;
        }
        return true;
    }

    bool Listen(const std::string& path, std::string* error)
    {
        if (!RemoveStaleSocket(path, error))
            return false;

        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            *error = SystemError("Unable to create Unix socket");
            return false;
        }
        SetCloseOnExec(fd);
        SetNonBlocking(fd);

        const sockaddr_un address = SocketAddress(path);
        if (bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            *error = SystemError("Unable to bind Unix socket endpoint " + path);
            close(fd);
            return false;
        }

        // Connections are refused until listen(), so restricting the mode
        // here leaves no window in which another user could connect.
        struct stat info;
        if (chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0 ||
            lstat(path.c_str(), &info) != 0 ||
            listen(fd, SOMAXCONN) != 0) {
            *error = SystemError("Unable to listen on Unix socket endpoint " + path);
            unlink(path.c_str());
            close(fd);
            return false;
        }

        impl->socket_path = path;
        impl->socket_device = info.st_dev;
        impl->socket_inode = info.st_ino;
        impl->listen_fd = fd;
        return true;
    }

    void CloseDescriptors()
    {
        if (impl->listen_fd >= 0) {
            // Only remove the socket file if it is still the one this
            // transport created, not one a newer process has since bound.
            struct stat info;
            if (lstat(impl->socket_path.c_str(), &info) == 0 && S_ISSOCK(info.st_mode) &&
                info.st_dev == impl->socket_device && info.st_ino == impl->socket_inode)
                unlink(impl->socket_path.c_str());
            close(impl->listen_fd);
            impl->listen_fd = -1;
        }
        if (impl->wake_read_fd >= 0) {
            close(impl->wake_read_fd);
            impl->wake_read_fd = -1;
        }
        if (impl->wake_write_fd >= 0) {
            close(impl->wake_write_fd);
            impl->wake_write_fd = -1;
        }
    }

    // Waits until fd is ready for events. Returns false once Stop() has been
    // called or the descriptor is no longer valid.
    bool WaitFor(const int fd, const short events) const
    {
        for (;;) {
            pollfd fds[2];
            fds[0].fd = fd;
            fds[0].events = events;
            fds[0].revents = 0;
            fds[1].fd = impl->wake_read_fd;
            fds[1].events = POLLIN;
            fds[1].revents = 0;
            if (poll(fds, 2, -1) < 0) {
                if (errno == EINTR)
                    continue;
                return false;
            }
            if (fds[1].revents != 0 || (fds[0].revents & POLLNVAL) != 0)
                return false;
            // Errors and hang-ups are reported by the following recv() or send().
            if (fds[0].revents != 0)
                return true;
        }
    }

    bool WriteAll(const int client, const std::string& message) const
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;    // SO_NOSIGPIPE was set on the socket instead
#endif
        std::size_t offset = 0;
        while (offset < message.size()) {
            if (!WaitFor(client, POLLOUT))
                return false;
            const ssize_t sent = send(client, message.data() + offset, message.size() - offset, flags);
            if (sent < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                return false;
            }
            offset += static_cast<std::size_t>(sent);
        }
        return true;
    }

    bool ServeClient(const int client)
    {
        std::string pending;
        char buffer[4096];
        for (;;) {
            if (!WaitFor(client, POLLIN))
                return false;
            const ssize_t received = recv(client, buffer, sizeof(buffer), 0);
            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                return false;
            }
            if (received == 0)
                return false;
            pending.append(buffer, static_cast<std::size_t>(received));

            for (;;) {
                const std::string::size_type line_end = pending.find('\n');
                if (line_end == std::string::npos) {
                    if (pending.size() > impl->config.max_message_bytes) {
                        const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                        WriteAll(client, response);
                        return false;
                    }
                    break;
                }

                std::string request = pending.substr(0, line_end);
                pending.erase(0, line_end + 1);
                if (!request.empty() && request[request.size() - 1] == '\r')
                    request.erase(request.size() - 1);
                if (request.size() > impl->config.max_message_bytes) {
                    const std::string response = impl->handler(std::string(impl->config.max_message_bytes + 1, 'x')) + "\n";
                    WriteAll(client, response);
                    return false;
                }
                if (!WriteAll(client, impl->handler(request) + "\n"))
                    return false;
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->stopping)
                return false;
        }
    }

    void ServeClientThread(const int client)
    {
        ServeClient(client);
        close(client);
    }

    void Run()
    {
        for (;;) {
            if (!WaitFor(impl->listen_fd, POLLIN))
                return;
            const int client = accept(impl->listen_fd, NULL, NULL);
            if (client < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
                    continue;
                return;
            }

            // macOS and the BSDs let accepted sockets inherit O_NONBLOCK and
            // Linux does not, so set it explicitly. WaitFor() does the blocking.
            SetCloseOnExec(client);
            SetNonBlocking(client);
#ifdef SO_NOSIGPIPE
            const int no_sigpipe = 1;
            setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
            if (!PeerIsCurrentUser(client)) {
                close(client);
                continue;
            }

            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->stopping) {
                close(client);
                return;
            }
            impl->client_workers.push_back(std::thread(&UnixSocketTransport::ServeClientThread, this, client));
        }
    }
#endif
};

} // namespace

std::unique_ptr<IRpcTransport> AGENT_CreateLocalTransport(const AgentTransport transport)
{
    switch (transport) {
    case AgentTransport::NamedPipe:
        return std::unique_ptr<IRpcTransport>(new NamedPipeTransport());
    case AgentTransport::UnixSocket:
        return std::unique_ptr<IRpcTransport>(new UnixSocketTransport());
    }
    return std::unique_ptr<IRpcTransport>();
}

} // namespace dosbox_agent
#endif // defined(C_DEBUG) && defined(C_DOSBOX_AGENT)
