/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "localpipe.h"

#include <cstdio>

#if defined(WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static std::string win_err(DWORD e) {
	char buf[256];
	DWORD n = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	while (n && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == '.' || buf[n - 1] == ' '))
		buf[--n] = 0;
	char out[320];
	snprintf(out, sizeof(out), "%s (error %lu)", n ? buf : "unknown", (unsigned long)e);
	return out;
}

localpipe_t LocalPipe_Listen(const std::string &path, std::string *error) {
	// PIPE_NOWAIT makes ConnectNamedPipe() return immediately so the caller
	// can poll for a client without blocking.
	HANDLE h = CreateNamedPipeA(path.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
		1, 4096, 4096, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		if (error) *error = "could not create pipe as server: " + win_err(GetLastError());
		return LOCALPIPE_INVALID;
	}
	return (localpipe_t)h;
}

localpipe_t LocalPipe_TryAccept(localpipe_t listener, bool *listenerConsumed) {
	if (listenerConsumed) *listenerConsumed = false;
	if (listener == LOCALPIPE_INVALID) return LOCALPIPE_INVALID;

	HANDLE h = (HANDLE)listener;
	if (ConnectNamedPipe(h, NULL)) {
		// unusual for a NOWAIT pipe, but treat as connected
		if (listenerConsumed) *listenerConsumed = true;
		return listener;
	}
	DWORD err = GetLastError();
	if (err == ERROR_PIPE_CONNECTED) {
		// a client is already attached: the server handle IS the connection
		if (listenerConsumed) *listenerConsumed = true;
		return listener;
	}
	// ERROR_PIPE_LISTENING (no client yet) / ERROR_NO_DATA (previous client
	// gone) - nothing to hand back this time.
	return LOCALPIPE_INVALID;
}

void LocalPipe_SetNonBlocking(localpipe_t handle) {
	if (handle == LOCALPIPE_INVALID) return;
	DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	SetNamedPipeHandleState((HANDLE)handle, &mode, NULL, NULL);
}

long LocalPipe_Recv(localpipe_t handle, void *buf, size_t len) {
	if (handle == LOCALPIPE_INVALID) return -1;
	DWORD got = 0;
	if (ReadFile((HANDLE)handle, buf, (DWORD)len, &got, NULL))
		return (long)got; // 0 here just means "nothing ready" on a NOWAIT pipe
	return (GetLastError() == ERROR_NO_DATA) ? 0 : -1;
}

long LocalPipe_Send(localpipe_t handle, const void *buf, size_t len) {
	if (handle == LOCALPIPE_INVALID) return -1;
	DWORD put = 0;
	if (WriteFile((HANDLE)handle, buf, (DWORD)len, &put, NULL))
		return (long)put;
	return (GetLastError() == ERROR_NO_DATA) ? 0 : -1;
}

LocalPipeOpenResult LocalPipe_Open(const std::string &path, bool serverMode, unsigned timeoutMs) {
	LocalPipeOpenResult r;

	if (serverMode) {
		localpipe_t listener = LocalPipe_Listen(path, &r.error);
		if (listener == LOCALPIPE_INVALID) return r;

		for (;;) {
			bool consumed = false;
			localpipe_t c = LocalPipe_TryAccept(listener, &consumed);
			if (c != LOCALPIPE_INVALID) {
				if (!consumed) LocalPipe_Close(listener);
				r.handle = c;
				return r;
			}
			Sleep(50);
		}
	}

	// client: retry while the server may not have created the pipe yet
	// (ERROR_FILE_NOT_FOUND) or has it but busy (ERROR_PIPE_BUSY).
	DWORD start = GetTickCount();
	for (;;) {
		HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
			NULL, OPEN_EXISTING, 0, NULL);
		if (h != INVALID_HANDLE_VALUE) {
			r.handle = (localpipe_t)h;
			return r;
		}

		DWORD err = GetLastError();
		if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
			r.error = "could not connect: " + win_err(err);
			return r;
		}
		if (GetTickCount() - start >= timeoutMs) {
			r.error = "could not connect: timed out";
			return r;
		}
		if (err == ERROR_PIPE_BUSY)
			WaitNamedPipeA(path.c_str(), 500);
		else
			Sleep(200);
	}
}

void LocalPipe_Close(localpipe_t handle) {
	if (handle != LOCALPIPE_INVALID)
		CloseHandle((HANDLE)handle);
}

void LocalPipe_Unlink(const std::string &) {
	// Windows named pipes have no filesystem entry to remove.
}

#else // !defined(WIN32) - POSIX AF_UNIX stream socket

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

static std::string posix_err(const char *what, int e) {
	char out[320];
	snprintf(out, sizeof(out), "%s (errno %d: %s)", what, e, strerror(e));
	return out;
}

// EAGAIN and EWOULDBLOCK are the same value on Linux, distinct on some others.
static bool would_block(int e) {
	if (e == EAGAIN) return true;
#if EWOULDBLOCK != EAGAIN
	if (e == EWOULDBLOCK) return true;
#endif
	return false;
}

static void fill_addr(struct sockaddr_un &addr, const std::string &path) {
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
}

localpipe_t LocalPipe_Listen(const std::string &path, std::string *error) {
	int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listenFd < 0) {
		if (error) *error = posix_err("could not create UNIX domain socket", errno);
		return LOCALPIPE_INVALID;
	}

	unlink(path.c_str()); // drop a stale socket file from a previous run

	struct sockaddr_un addr;
	fill_addr(addr, path);

	if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		if (error) *error = posix_err("could not bind socket as server", errno);
		close(listenFd);
		return LOCALPIPE_INVALID;
	}
	if (listen(listenFd, 1) < 0) {
		if (error) *error = posix_err("could not listen on socket", errno);
		close(listenFd);
		unlink(path.c_str());
		return LOCALPIPE_INVALID;
	}

	int fl = fcntl(listenFd, F_GETFL, 0);
	if (fl >= 0) fcntl(listenFd, F_SETFL, fl | O_NONBLOCK);
	return (localpipe_t)listenFd;
}

localpipe_t LocalPipe_TryAccept(localpipe_t listener, bool *listenerConsumed) {
	if (listenerConsumed) *listenerConsumed = false; // POSIX: listener always stays
	if (listener == LOCALPIPE_INVALID) return LOCALPIPE_INVALID;

	int fd = accept((int)listener, NULL, NULL);
	if (fd < 0) return LOCALPIPE_INVALID; // EAGAIN/EWOULDBLOCK: nobody yet
	return (localpipe_t)fd;
}

void LocalPipe_SetNonBlocking(localpipe_t handle) {
	if (handle == LOCALPIPE_INVALID) return;
	int fl = fcntl((int)handle, F_GETFL, 0);
	if (fl >= 0) fcntl((int)handle, F_SETFL, fl | O_NONBLOCK);
}

long LocalPipe_Recv(localpipe_t handle, void *buf, size_t len) {
	if (handle == LOCALPIPE_INVALID) return -1;
	for (;;) {
		ssize_t n = recv((int)handle, buf, len, 0);
		if (n >= 0) return (long)(n == 0 ? -1 : n); // 0 = peer closed
		if (errno == EINTR) continue;
		if (would_block(errno)) return 0;
		return -1;
	}
}

long LocalPipe_Send(localpipe_t handle, const void *buf, size_t len) {
	if (handle == LOCALPIPE_INVALID) return -1;
	for (;;) {
		ssize_t n = send((int)handle, buf, len, 0);
		if (n >= 0) return (long)n;
		if (errno == EINTR) continue;
		if (would_block(errno)) return 0;
		return -1;
	}
}

LocalPipeOpenResult LocalPipe_Open(const std::string &path, bool serverMode, unsigned timeoutMs) {
	LocalPipeOpenResult r;

	if (serverMode) {
		localpipe_t listener = LocalPipe_Listen(path, &r.error);
		if (listener == LOCALPIPE_INVALID) return r;

		for (;;) {
			bool consumed = false;
			localpipe_t c = LocalPipe_TryAccept(listener, &consumed);
			if (c != LOCALPIPE_INVALID) {
				if (!consumed) LocalPipe_Close(listener);
				unlink(path.c_str()); // connection established; drop the name
				r.handle = c;
				return r;
			}
			usleep(50 * 1000);
		}
	}

	// client: retry while the server may not have created/bound the socket yet
	struct sockaddr_un addr;
	fill_addr(addr, path);

	const unsigned retryDelayMs = 200;
	unsigned waited = 0;
	for (;;) {
		int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) {
			r.error = posix_err("could not create UNIX domain socket", errno);
			return r;
		}
		if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
			r.handle = (localpipe_t)fd;
			return r;
		}

		int err = errno;
		close(fd);
		if (err != ENOENT && err != ECONNREFUSED) {
			r.error = posix_err("could not connect to socket", err);
			return r;
		}
		if (waited >= timeoutMs) {
			r.error = "could not connect to socket: timed out";
			return r;
		}
		usleep(retryDelayMs * 1000);
		waited += retryDelayMs;
	}
}

void LocalPipe_Close(localpipe_t handle) {
	if (handle != LOCALPIPE_INVALID)
		close((int)handle);
}

void LocalPipe_Unlink(const std::string &path) {
	if (!path.empty())
		unlink(path.c_str());
}

#endif // WIN32 / POSIX
