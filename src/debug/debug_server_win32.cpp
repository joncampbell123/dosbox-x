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

#include "dosbox.h"

#if C_DEBUG && C_DEBUG_SERVER && WIN32

#include "debug.h"
#include "logging.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define LOGPREFIX "DebugServer: "

static struct {
    WSADATA wsaData;
    // Listen for incoming connections
    SOCKET listen = INVALID_SOCKET;
    // Single active client connection (i.e. remote debugger)
    SOCKET client = INVALID_SOCKET;
    // Server thread
    HANDLE thread = NULL;
    // Remote address of client
    SOCKADDR clientAddress;
} winServer;

static void LOG_WinError(char *message, int error) {
    char* errorMessage;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorMessage, 0, NULL);
    LOG_MSG(LOGPREFIX ": %s - %s\n", message, errorMessage);
    LocalFree(errorMessage);
    return;
}

static DWORD WINAPI serverThreadEntry(void* data) {
    DEBUG_Server(data);
    return 0;
}

bool DEBUG_ServerStartThread() {
    winServer.thread = CreateThread(NULL, 0, serverThreadEntry, NULL, 0, NULL);
    if (winServer.thread == NULL) {
        LOG_WinError("Could not start debug server thread", GetLastError());
        return false;
    }
    // Just go on with normal dosbox execution after creating the thread
    return true;
}

/**
 * Shutdown the debug server thread
 */
void DEBUG_ServerShutdownThread() {
    // Indicate that server thread should shut down
    DEBUG_server.shutdown = true;
    // Close potentially blocking sockets (will cancel accept()/recv() calls)
    closesocket(winServer.client);
    closesocket(winServer.listen);
    // Wait for thread to exit
    WaitForSingleObject(winServer.thread, 1000);
    WSACleanup();
}

static char* GetFormattedAddress(sockaddr* addr) {
    static char formatted[INET_ADDRSTRLEN];
    static int length = INET_ADDRSTRLEN;
    if (WSAAddressToString(addr, sizeof(sockaddr_in), 0, formatted, (LPDWORD)&length) != 0) {
        LOG_WinError("WSAAddressToString", WSAGetLastError());
    }
    return formatted;
}

void* DEBUG_ServerAcceptConnection(char* addr, char* port) {
    int result = 0, addressLength = 0;
    struct addrinfo* localAddress = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &winServer.wsaData);
    if (result != 0) {
        LOG_WinError("WSAStartup", result);
        return false;
    }

    // Resolve the local address and port
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    result = getaddrinfo(addr, port, &hints, &localAddress);
    if (result != 0) {
        LOG_MSG("getaddrinfo", result);
        WSACleanup();
        return false;
    }

    // Setup the TCP listening socket
    winServer.listen = socket(localAddress->ai_family, localAddress->ai_socktype, localAddress->ai_protocol);
    if (winServer.listen == INVALID_SOCKET) {
        LOG_WinError("socket", WSAGetLastError());
        freeaddrinfo(localAddress);
        WSACleanup();
        return false;
    }
    result = ::bind(winServer.listen, localAddress->ai_addr, (int)localAddress->ai_addrlen);
    if (result == SOCKET_ERROR) {
        LOG_WinError("bind", WSAGetLastError());
        freeaddrinfo(localAddress);
        closesocket(winServer.listen);
        WSACleanup();
        return false;
    }
    freeaddrinfo(localAddress);
    result = listen(winServer.listen, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        LOG_WinError("listen", WSAGetLastError());
        closesocket(winServer.listen);
        WSACleanup();
        return false;
    }

    WIN32_SetConsoleTitle();

    // Wait for an incoming connection (this is blocking)
    addressLength = sizeof(winServer.clientAddress);
    winServer.client = accept(winServer.listen, &winServer.clientAddress, &addressLength);
    if (winServer.client == INVALID_SOCKET) {
        LOG_WinError("accept", WSAGetLastError());
        closesocket(winServer.listen);
        WSACleanup();
        return NULL;
    }

    // Got a client connection -> update server state
    char* address = GetFormattedAddress(&winServer.clientAddress);
    LOG_MSG(LOGPREFIX ": New connection from %s\n", address);
    DEBUG_server.isConnected = true;
    safe_strcpy(DEBUG_server.clientAddress, address);
    WIN32_SetConsoleTitle();


    DEBUG_ServerWriteResponse("^connected\r\n");
    DEBUG_ServerWriteResponse("(gdb)\r\n");
    DEBUG_ServerWriteResponse("=thread-group-added,id=\"i1\"\r\n");
    //DEBUG_ServerWriteResponse("=thread-group-started,id=\"i1\",pid=\"1\"\n(gdb)\r\n");
    //DEBUG_ServerWriteResponse("=thread-created,id=\"1\",group-id=\"i1\"\r\n");
    if (IsDebuggerActive()) {
        //DEBUG_ServerWriteResponse("*stopped,reason=\"exec\"\r\n");
    }
    else {
        //DEBUG_ServerWriteResponse("*running\r\n");
    }
    
    // Stop listening for new connections
    closesocket(winServer.listen);

    return &winServer.client;
}

/**
 * Read a single request from the remote debugger
 *
 * Blocks until there is a request available
 */
int DEBUG_ServerReadRequest(char* requestBuffer, int bufferLength) {
    int bytesRead = 0, result = 0;
    if (bufferLength < 1) {
        return 0;
    }
    // Read from connection (blocking)
    bytesRead = recv(winServer.client, requestBuffer, bufferLength - 1, 0);
    if (bytesRead == SOCKET_ERROR) {
        DEBUG_server.isConnected = false;
        LOG_MSG(LOGPREFIX "[%s]: Connection closed\n", GetFormattedAddress(&winServer.clientAddress));
        //LOG_WinError("recv", WSAGetLastError());
    }
    else if (bytesRead == 0) {
        // Connection closed
        DEBUG_server.isConnected = false;
        requestBuffer[0] = 0;
        LOG_MSG(LOGPREFIX "[%s]: Connection closed\n", GetFormattedAddress(&winServer.clientAddress));
    }
    else {
        requestBuffer[bytesRead] = 0;
        //LOG_MSG(LOGPREFIX "[%s]: %s\n", GetFormattedAddress(&winServer.clientAddress), requestBuffer);
    }
    if (!DEBUG_server.isConnected) {
        // Shutdown the connection since we're done
        result = shutdown(winServer.client, SD_SEND);
        if (result == SOCKET_ERROR) {
            LOG_WinError("shutdown", WSAGetLastError());
        }
        closesocket(winServer.client);
        strcpy(DEBUG_server.clientAddress, "");
    }
    OutputDebugString(requestBuffer);
    return bytesRead;
}

/**
 * Sends a message string as response to the remote debugger
 */
int DEBUG_ServerWriteResponse(char* message) {
    OutputDebugString(message);
    int bytesWritten = 0;
    if (DEBUG_server.isConnected) {
        bytesWritten = send(winServer.client, message, strlen(message), 0);
        if (bytesWritten == SOCKET_ERROR) {
            LOG_WinError("send data", WSAGetLastError());
            closesocket(winServer.client);
            return 0;
        }
    }
    return bytesWritten;
}

#endif // C_DEBUG && C_DEBUG_SERVER && WIN32
