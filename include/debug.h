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

#include <cstdint>
#include <string>
#include <vector>

void DEBUG_SetupConsole(void);
void DEBUG_DrawScreen(void);
bool DEBUG_Breakpoint(void);
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(char scroll);
Bitu DEBUG_EnableDebugger(void);

#if C_DEBUG
bool DEBUG_AgentStep(bool over, bool* continued);
bool DEBUG_AgentResumeAfterTerminate(void);
bool DEBUG_AgentCanStartTarget(void);
uint64_t DEBUG_AgentEntryBreakpointSequence(void);
bool DEBUG_AgentCreateExecutionBreakpoint(uint16_t seg, uint32_t off, bool once, uintptr_t* handle);
bool DEBUG_AgentCreateMemoryBreakpoint(uint16_t seg, uint32_t off, bool protected_mode, bool linear, uintptr_t* handle);
bool DEBUG_AgentDeleteBreakpoint(uintptr_t handle);
uintptr_t DEBUG_AgentConsumeLastBreakpoint(void);
void DEBUG_AgentClearLastBreakpoint(void);
bool DEBUG_AgentBeginOutputCapture(void);
std::string DEBUG_AgentEndOutputCapture(void);
#if C_HEAVY_DEBUG
struct DEBUG_AgentTraceEvent {
    uint16_t cs = 0;
    uint32_t instruction_pointer = 0;
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    uint32_t esi = 0;
    uint32_t edi = 0;
    uint32_t ebp = 0;
    uint32_t esp = 0;
    uint16_t ds = 0;
    uint16_t es = 0;
    uint16_t fs = 0;
    uint16_t gs = 0;
    uint16_t ss = 0;
    uint32_t flags = 0;
    std::string instruction;
    std::string analysis;
};
bool DEBUG_AgentStartTrace(uint32_t instruction_count);
bool DEBUG_AgentStopTrace(uint32_t* event_count);
bool DEBUG_AgentTraceIsActive(void);
void DEBUG_AgentCopyTraceEvents(std::vector<DEBUG_AgentTraceEvent>* events);
#endif
#endif

extern Bitu cycle_count;
extern Bitu debugCallback;

#ifdef C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint(void);
void DEBUG_HeavyWriteLogInstruction(void);
#endif
