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

#ifndef DOSBOX_LOCALPIPE_H
#define DOSBOX_LOCALPIPE_H

#include <cstdint>
#include <string>

// A local, same-host, point-to-point stream channel: a named pipe on
// Windows, an AF_UNIX SOCK_STREAM socket everywhere else. This helper only
// does the connection dance (create/connect, retry, one-client accept); the
// caller gets back the raw OS handle and applies whatever blocking or
// non-blocking I/O policy it wants.
//
// Used by the parport "extlpt" backend (transport:pipe) and by the
// serialport namedpipe socket type - two callers with opposite I/O needs
// (lockstep-blocking vs. non-blocking poll), hence "connect only".
//
// The handle is:
//   Windows - a HANDLE, carried as intptr_t
//   POSIX   - a file descriptor
typedef intptr_t localpipe_t;
static const localpipe_t LOCALPIPE_INVALID = (localpipe_t)-1;

struct LocalPipeOpenResult {
	localpipe_t handle = LOCALPIPE_INVALID;
	std::string error;   // human-readable; empty iff handle is valid
};

// serverMode = false: connect to `path`, retrying for up to timeoutMs while
//                     the peer endpoint is not there yet.
// serverMode = true:  create `path` and block until exactly one client
//                     connects (timeoutMs is ignored). Built on the
//                     non-blocking primitives below.
// Never logs - inspect .error and log it with your own context/prefix.
LocalPipeOpenResult LocalPipe_Open(const std::string &path, bool serverMode,
                                   unsigned timeoutMs = 5000);

// Close a handle returned by this module. Safe on LOCALPIPE_INVALID.
void LocalPipe_Close(localpipe_t handle);

// Remove a server endpoint's filesystem name (the POSIX AF_UNIX socket file).
// No-op on Windows and on an empty path; safe if the file is already gone.
// LocalPipe_Open(serverMode) unlinks on its own; a LocalPipe_Listen() caller
// must call this once it is done listening.
void LocalPipe_Unlink(const std::string &path);

// --- non-blocking building blocks (for a caller that polls, e.g. a serial
//     port that must never stall the emulator core) -----------------------

// Create a server endpoint at `path` without waiting for a client. Returns
// the listener handle, or LOCALPIPE_INVALID with *error set.
localpipe_t LocalPipe_Listen(const std::string &path, std::string *error);

// Poll a listener from LocalPipe_Listen() for one incoming client. Returns a
// connected handle, or LOCALPIPE_INVALID if none is pending yet. When it
// returns a handle it also sets *listenerConsumed: true means the listener
// handle itself became the connection (Windows named pipes) and must not be
// used or closed as a listener any more; false means the listener stays open
// (POSIX) and the caller owns both handles.
localpipe_t LocalPipe_TryAccept(localpipe_t listener, bool *listenerConsumed);

// Put a handle from this module into non-blocking mode.
void LocalPipe_SetNonBlocking(localpipe_t handle);

// Best-effort non-blocking I/O on a handle from this module.
//   > 0  bytes moved
//   = 0  nothing could be moved right now (would block) - not an error
//   < 0  hard error / peer closed
long LocalPipe_Recv(localpipe_t handle, void *buf, size_t len);
long LocalPipe_Send(localpipe_t handle, const void *buf, size_t len);

#endif // DOSBOX_LOCALPIPE_H
