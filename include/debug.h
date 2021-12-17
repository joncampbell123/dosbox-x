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

#include "Breakpoint.h"
#include "DebugVariable.h"

void DEBUG_SetupConsole();
void DEBUG_DrawScreen();
bool DEBUG_Breakpoint();
bool DEBUG_IntBreakpoint(uint8_t intNum);
//void DEBUG_Enable(bool pressed); ? not defined
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop();
void DEBUG_RefreshPage(char scroll);
Bitu DEBUG_EnableDebugger();
void DEBUG_Enable_Handler(bool pressed);
void DEBUG_ShowMsg(char const* format, ...);

extern Bitu cycle_count;
extern Bitu debugCallback;

bool IsDebuggerActive();
bool IsDebuggerRunwatch();
bool IsDebuggerRunNormal();
bool ParseCommand(char* str);
char* F80ToString(int regIndex, char* dest);
uint64_t GetAddress(uint16_t seg, uint32_t offset);

#if WIN32
void WIN32_SetConsoleTitle();
#endif

#ifdef C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint();
void DEBUG_HeavyWriteLogInstruction();
#endif
#ifdef C_DEBUG_SERVER
#define DEBUG_SERVER_PORT "2999"

void DEBUG_EnableServer();
void DEBUG_ShutdownServer();
void DEBUG_Server(void* arguments);

// Those must be implemented platform-specific
bool DEBUG_ServerStartThread();
void DEBUG_ServerShutdownThread();
void* DEBUG_ServerAcceptConnection(char* addr, char* port);
int DEBUG_ServerReadRequest(char* buffer, int bufferLength);
int DEBUG_ServerWriteResponse(char* response);

struct DebugServer {
    bool shutdown;
    bool isConnected;
    char clientAddress[128];
};
extern struct DebugServer DEBUG_server;
#endif
