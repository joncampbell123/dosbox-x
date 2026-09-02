#include "dosbox.h"
#include "agent/debugger_adapter.h"
#include "agent/agent_bridge.h"

#if C_DEBUG
#include "cpu.h"
#include "debug.h"
#include "dos_inc.h"
#include "mem.h"
#include "paging.h"
#include "shell.h"

extern bool ParseCommand(char* str);
extern char appname[];
extern char appargs[];
extern bool dos_program_running;
#endif

#include <cctype>
#include <limits>

namespace dosbox_agent {

namespace {

bool RequireEmulationThread(std::string* error)
{
    if (AGENT_EmulationQueue().IsBoundToCurrentThread())
        return true;
    if (error != NULL)
        *error = "Debugger access was not dispatched on the emulation thread";
    return false;
}

void SetAccessError(MemoryAccessError* access_error,
                    const char* reason,
                    const std::uint32_t failing_offset)
{
    if (access_error == NULL)
        return;
    access_error->reason = reason;
    access_error->failing_offset = failing_offset;
}

std::string Trim(const std::string& value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool IsReadOnlyDiagnosticCommand(const std::string& command)
{
    std::string normalized = Trim(command);
    for (std::string::iterator it = normalized.begin(); it != normalized.end(); ++it)
        *it = static_cast<char>(std::toupper(static_cast<unsigned char>(*it)));
    return normalized == "HELP" || normalized == "CPU" || normalized == "PIC";
}

#if C_DEBUG
bool ResolveSegmentedByte(const MemoryAddress& address,
                          const std::size_t index,
                          std::uint32_t* linear,
                          MemoryAccessError* access_error)
{
    const std::uint64_t raw_offset = static_cast<std::uint64_t>(address.offset) + index;
    if (raw_offset > (std::numeric_limits<std::uint32_t>::max)()) {
        SetAccessError(access_error, "address_overflow", address.offset);
        return false;
    }

    std::uint32_t offset = static_cast<std::uint32_t>(raw_offset);
    if (!cpu.pmode || (reg_flags & FLAG_VM)) {
        offset &= 0xffffu;
        *linear = (static_cast<std::uint32_t>(address.segment) << 4u) + offset;
        return true;
    }

    Descriptor descriptor;
    if (!cpu.gdt.GetDescriptor(address.segment, descriptor) || descriptor.Type() == 0 || !descriptor.saved.seg.p) {
        SetAccessError(access_error, "invalid_selector", static_cast<std::uint32_t>(raw_offset));
        return false;
    }

    if (!descriptor.Big())
        offset &= 0xffffu;

    const Bitu limit = descriptor.GetLimit();
    if ((!descriptor.GetExpandDown() && offset > limit) ||
        (descriptor.GetExpandDown() && offset <= limit)) {
        SetAccessError(access_error, "segment_limit", offset);
        return false;
    }

    const std::uint64_t resolved = static_cast<std::uint64_t>(descriptor.GetBase()) + offset;
    if (resolved > (std::numeric_limits<std::uint32_t>::max)()) {
        SetAccessError(access_error, "address_overflow", offset);
        return false;
    }
    *linear = static_cast<std::uint32_t>(resolved);
    return true;
}

bool ResolveLinearByte(const MemoryAddress& address,
                       const std::size_t index,
                       std::uint32_t* linear,
                       MemoryAccessError* access_error)
{
    const std::uint64_t resolved = static_cast<std::uint64_t>(address.offset) + index;
    if (resolved > (std::numeric_limits<std::uint32_t>::max)()) {
        SetAccessError(access_error, "address_overflow", address.offset);
        return false;
    }
    *linear = static_cast<std::uint32_t>(resolved);
    return true;
}

bool ResolveMemoryByte(const MemoryAddress& address,
                       const std::size_t index,
                       std::uint32_t* resolved,
                       MemoryAccessError* access_error)
{
    switch (address.space) {
    case MemorySpace::Segmented:
        return ResolveSegmentedByte(address, index, resolved, access_error);
    case MemorySpace::Linear:
    case MemorySpace::Physical:
        return ResolveLinearByte(address, index, resolved, access_error);
    }
    SetAccessError(access_error, "unsupported_address_space", address.offset);
    return false;
}

bool ReadResolvedByte(const MemoryAddress& address,
                      const std::uint32_t resolved,
                      std::uint8_t* value,
                      MemoryAccessError* access_error)
{
    if (address.space == MemorySpace::Physical) {
        if (resolved >= MemSize) {
            SetAccessError(access_error, "physical_out_of_range", resolved);
            return false;
        }
        *value = phys_readb(resolved);
        return true;
    }

    try {
        if (!mem_readb_checked(resolved, value))
            return true;
    } catch (const GuestPageFaultException&) {
    } catch (const GuestGenFaultException&) {
    }
    SetAccessError(access_error, "page_not_present", resolved);
    return false;
}

bool IsResolvedByteWritable(const MemoryAddress& address,
                            const std::uint32_t resolved,
                            MemoryAccessError* access_error)
{
    if (address.space == MemorySpace::Physical) {
        if (resolved < MemSize)
            return true;
        SetAccessError(access_error, "physical_out_of_range", resolved);
        return false;
    }

    if (get_tlb_write(resolved) != NULL)
        return true;
    SetAccessError(access_error, "write_not_mapped", resolved);
    return false;
}

void WriteResolvedByte(const MemoryAddress& address,
                       const std::uint32_t resolved,
                       const std::uint8_t value)
{
    if (address.space == MemorySpace::Physical)
        phys_writeb(resolved, value);
    else
        mem_writeb_inline(resolved, value);
}
#endif

} // namespace

bool DebuggerAdapter::IsAvailable() const
{
#if C_DEBUG
    return true;
#else
    return false;
#endif
}

bool DebuggerAdapter::RequireAvailable(std::string* error) const
{
    if (IsAvailable())
        return true;

    if (error != NULL)
        *error = "DEBUGGER_UNAVAILABLE: DOSBox-X was built without C_DEBUG";
    return false;
}

bool DebuggerAdapter::IsShellReady() const
{
#if C_DEBUG
    return first_shell != NULL && DOS_ShellGetPSP() != 0;
#else
    return false;
#endif
}

bool DebuggerAdapter::IsReadyForTargetStart() const
{
#if C_DEBUG
    return IsShellReady() && DEBUG_AgentCanStartTarget();
#else
    return false;
#endif
}

std::uint64_t DebuggerAdapter::EntryBreakpointSequence() const
{
#if C_DEBUG
    return DEBUG_AgentEntryBreakpointSequence();
#else
    return 0;
#endif
}

bool DebuggerAdapter::Continue(std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_DEBUG
    DEBUG_AgentClearLastBreakpoint();
    char command[] = "RUN";
    if (ParseCommand(command))
        return true;

    if (error != NULL)
        *error = "Debugger rejected RUN";
    return false;
#else
    return false;
#endif
}

bool DebuggerAdapter::Pause(std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_DEBUG
    DEBUG_EnableDebugger();
    return true;
#else
    return false;
#endif
}

bool DebuggerAdapter::StartTargetAtEntry(const std::string& command,
                                         const std::vector<std::string>& arguments,
                                         const std::string& workdir,
                                         std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_DEBUG
    if (!IsShellReady()) {
        if (error != NULL)
            *error = "DOS shell is not ready";
        return false;
    }
    dos.psp(DOS_ShellGetPSP());
    if (workdir.empty() || workdir.find('"') != std::string::npos) {
        if (error != NULL)
            *error = "Configured workdir cannot be mounted safely";
        return false;
    }

    const auto is_dos_token = [](const std::string& value) {
        if (value.empty())
            return false;
        for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
            const unsigned char character = static_cast<unsigned char>(*it);
            if (!std::isalnum(character) && *it != '.' && *it != '_' && *it != '-')
                return false;
        }
        return true;
    };
    if (!is_dos_token(command)) {
        if (error != NULL)
            *error = "target.command must be a DOS filename without shell metacharacters";
        return false;
    }
    for (std::vector<std::string>::const_iterator it = arguments.begin(); it != arguments.end(); ++it) {
        if (!is_dos_token(*it)) {
            if (error != NULL)
                *error = "target.arguments must contain only DOS-safe tokens";
            return false;
        }
    }

    // The agent accepts one immutable, process-level C: mount. Re-running MOUNT
    // for every session is not idempotent: the DOS command reports an existing
    // drive through its normal output path and can leave a stale DOS error code.
    if (Drives[2] == NULL) {
        std::string mount_command = "MOUNT C \"" + workdir + "\"";
        first_shell->DoCommand(&mount_command[0]);
        if (Drives[2] == NULL) {
            if (error != NULL)
                *error = "Configured C drive mount did not become active";
            return false;
        }
    }
    if (!DOS_SetDrive(2)) {
        if (error != NULL)
            *error = "Configured C drive is unavailable";
        return false;
    }

    std::string debugbox_command = "DEBUGBOX " + command;
    for (std::vector<std::string>::const_iterator it = arguments.begin(); it != arguments.end(); ++it)
        debugbox_command += " " + *it;
    dos.errorcode = 0;
    first_shell->DoCommand(&debugbox_command[0]);
    if (dos.errorcode != 0) {
        if (error != NULL)
            *error = "DOS target launch failed with error " + std::to_string(dos.errorcode);
        return false;
    }
    return true;
#else
    (void)command;
    (void)arguments;
    (void)workdir;
    return false;
#endif
}

bool DebuggerAdapter::GetRegisters(RegisterSnapshot* registers, std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || registers == NULL)
        return false;

#if C_DEBUG
    registers->eax = reg_eax;
    registers->ebx = reg_ebx;
    registers->ecx = reg_ecx;
    registers->edx = reg_edx;
    registers->esi = reg_esi;
    registers->edi = reg_edi;
    registers->ebp = reg_ebp;
    registers->esp = reg_esp;
    registers->cs = SegValue(cs);
    registers->ds = SegValue(ds);
    registers->es = SegValue(es);
    registers->fs = SegValue(fs);
    registers->gs = SegValue(gs);
    registers->ss = SegValue(ss);
    registers->instruction_pointer = reg_eip;
    registers->flags = static_cast<std::uint32_t>(reg_flags);
    registers->cpu_mode = !cpu.pmode ? "real" : ((reg_flags & FLAG_VM) ? "v86" : "protected");
    return true;
#else
    return false;
#endif
}

bool DebuggerAdapter::Step(const StepMode mode, bool* continued, std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || continued == NULL)
        return false;

#if C_DEBUG
    if (!DEBUG_AgentStep(mode == StepMode::Over, continued)) {
        if (error != NULL)
            *error = "Debugger is not stopped and ready to single-step";
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool DebuggerAdapter::ReadMemory(const MemoryAddress& address,
                                 const std::size_t length,
                                 std::vector<std::uint8_t>* data,
                                 MemoryAccessError* access_error,
                                 std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || data == NULL || length == 0)
        return false;

#if C_DEBUG
    if (access_error != NULL)
        *access_error = MemoryAccessError();
    data->clear();
    data->reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
        std::uint32_t resolved = 0;
        std::uint8_t value = 0;
        if (!ResolveMemoryByte(address, index, &resolved, access_error) ||
            !ReadResolvedByte(address, resolved, &value, access_error))
            return false;
        data->push_back(value);
    }
    return true;
#else
    (void)address;
    (void)length;
    (void)access_error;
    return false;
#endif
}

bool DebuggerAdapter::WriteMemory(const MemoryAddress& address,
                                  const std::vector<std::uint8_t>& data,
                                  std::vector<std::uint8_t>* after,
                                  MemoryAccessError* access_error,
                                  std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || after == NULL || data.empty())
        return false;

#if C_DEBUG
    if (access_error != NULL)
        *access_error = MemoryAccessError();
    std::vector<std::uint32_t> resolved_addresses;
    resolved_addresses.reserve(data.size());
    for (std::size_t index = 0; index < data.size(); ++index) {
        std::uint32_t resolved = 0;
        std::uint8_t unused = 0;
        if (!ResolveMemoryByte(address, index, &resolved, access_error) ||
            !ReadResolvedByte(address, resolved, &unused, access_error) ||
            !IsResolvedByteWritable(address, resolved, access_error))
            return false;
        resolved_addresses.push_back(resolved);
    }

    for (std::size_t index = 0; index < data.size(); ++index)
        WriteResolvedByte(address, resolved_addresses[index], data[index]);

    return ReadMemory(address, data.size(), after, access_error, error);
#else
    (void)address;
    (void)data;
    (void)access_error;
    return false;
#endif
}

bool DebuggerAdapter::CreateBreakpoint(const BreakpointKind kind,
                                       const MemoryAddress& address,
                                       const bool once,
                                       NativeBreakpoint* breakpoint,
                                       MemoryAccessError* access_error,
                                       std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || breakpoint == NULL)
        return false;

#if C_DEBUG
    if (access_error != NULL)
        *access_error = MemoryAccessError();
    std::uint8_t current_value = 0;
    std::vector<std::uint8_t> probe;
    if (!ReadMemory(address, 1, &probe, access_error, error))
        return false;
    current_value = probe[0];

    std::uintptr_t handle = 0;
    if (kind == BreakpointKind::Execution) {
        if (address.space != MemorySpace::Segmented) {
            if (error != NULL)
                *error = "Execution breakpoints currently require segmented addresses";
            return false;
        }
        if (!DEBUG_AgentCreateExecutionBreakpoint(address.segment, address.offset, once, &handle)) {
            if (error != NULL)
                *error = "Debugger rejected the execution breakpoint";
            return false;
        }
    } else {
#if C_HEAVY_DEBUG
        if (once) {
            if (error != NULL)
                *error = "memory_change breakpoints do not support once";
            return false;
        }
        if (address.space == MemorySpace::Physical) {
            if (error != NULL)
                *error = "Physical memory-change breakpoints are not supported by the debugger";
            return false;
        }
        const bool protected_mode = address.space == MemorySpace::Segmented && cpu.pmode && !(reg_flags & FLAG_VM);
        const bool linear = address.space == MemorySpace::Linear;
        if (!DEBUG_AgentCreateMemoryBreakpoint(address.segment, address.offset, protected_mode, linear, &handle)) {
            if (error != NULL)
                *error = "Debugger rejected the memory-change breakpoint";
            return false;
        }
        (void)current_value;
#else
        if (error != NULL)
            *error = "Memory-change breakpoints require C_HEAVY_DEBUG";
        return false;
#endif
    }

    breakpoint->handle = handle;
    breakpoint->kind = kind;
    breakpoint->address = address;
    breakpoint->once = once;
    return true;
#else
    (void)kind;
    (void)address;
    (void)once;
    (void)breakpoint;
    (void)access_error;
    return false;
#endif
}

bool DebuggerAdapter::DeleteBreakpoint(const NativeBreakpoint& breakpoint, std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_DEBUG
    if (breakpoint.handle != 0 && DEBUG_AgentDeleteBreakpoint(breakpoint.handle))
        return true;
    if (error != NULL)
        *error = "Breakpoint no longer exists in the debugger";
    return false;
#else
    (void)breakpoint;
    return false;
#endif
}

std::uintptr_t DebuggerAdapter::ConsumeLastBreakpointHandle() const
{
#if C_DEBUG
    return DEBUG_AgentConsumeLastBreakpoint();
#else
    return 0;
#endif
}

bool DebuggerAdapter::ExecuteDiagnosticCommand(const std::string& command,
                                               std::string* raw_output,
                                               std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || raw_output == NULL)
        return false;

#if C_DEBUG
    if (!IsReadOnlyDiagnosticCommand(command)) {
        if (error != NULL)
            *error = "Only HELP, CPU, and PIC are permitted debugger diagnostics";
        return false;
    }

    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');
    if (!DEBUG_AgentBeginOutputCapture()) {
        if (error != NULL)
            *error = "Debugger output capture is already active";
        return false;
    }
    const bool accepted = ParseCommand(&mutable_command[0]);
    *raw_output = DEBUG_AgentEndOutputCapture();
    if (!accepted && error != NULL)
        *error = "Debugger rejected diagnostic command";
    return accepted;
#else
    (void)command;
    return false;
#endif
}

bool DebuggerAdapter::StartTrace(const std::string& detail,
                                 const std::uint32_t instruction_count,
                                 std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_HEAVY_DEBUG
    if (detail != "short" && detail != "normal" && detail != "long" && detail != "csip") {
        if (error != NULL)
            *error = "Trace detail must be short, normal, long, or csip";
        return false;
    }
    if (!DEBUG_AgentStartTrace(instruction_count)) {
        if (error != NULL)
            *error = "A CPU trace is already active or instruction_count is invalid";
        return false;
    }
    return true;
#else
    (void)detail;
    (void)instruction_count;
    if (error != NULL)
        *error = "CPU trace requires C_HEAVY_DEBUG";
    return false;
#endif
}

bool DebuggerAdapter::ReadTrace(std::vector<TraceSample>* samples,
                                bool* active,
                                std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || samples == NULL || active == NULL)
        return false;

#if C_HEAVY_DEBUG
    std::vector<DEBUG_AgentTraceEvent> native_events;
    DEBUG_AgentCopyTraceEvents(&native_events);
    samples->clear();
    samples->reserve(native_events.size());
    for (std::vector<DEBUG_AgentTraceEvent>::const_iterator it = native_events.begin(); it != native_events.end(); ++it) {
        TraceSample sample;
        sample.address.space = MemorySpace::Segmented;
        sample.address.segment = it->cs;
        sample.address.offset = it->instruction_pointer;
        sample.registers.eax = it->eax;
        sample.registers.ebx = it->ebx;
        sample.registers.ecx = it->ecx;
        sample.registers.edx = it->edx;
        sample.registers.esi = it->esi;
        sample.registers.edi = it->edi;
        sample.registers.ebp = it->ebp;
        sample.registers.esp = it->esp;
        sample.registers.cs = it->cs;
        sample.registers.ds = it->ds;
        sample.registers.es = it->es;
        sample.registers.fs = it->fs;
        sample.registers.gs = it->gs;
        sample.registers.ss = it->ss;
        sample.registers.instruction_pointer = it->instruction_pointer;
        sample.registers.flags = it->flags;
        sample.registers.cpu_mode = !cpu.pmode ? "real" : ((reg_flags & FLAG_VM) ? "v86" : "protected");
        sample.instruction = Trim(it->instruction);
        sample.analysis = Trim(it->analysis);
        samples->push_back(sample);
    }
    *active = DEBUG_AgentTraceIsActive();
    return true;
#else
    (void)samples;
    (void)active;
    if (error != NULL)
        *error = "CPU trace requires C_HEAVY_DEBUG";
    return false;
#endif
}

bool DebuggerAdapter::StopTrace(std::size_t* event_count, std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error) || event_count == NULL)
        return false;

#if C_HEAVY_DEBUG
    if (!DEBUG_AgentTraceIsActive()) {
        if (error != NULL)
            *error = "No CPU trace is active";
        return false;
    }
    std::uint32_t native_event_count = 0;
    if (!DEBUG_AgentStopTrace(&native_event_count)) {
        if (error != NULL)
            *error = "Debugger failed to stop the CPU trace";
        return false;
    }
    *event_count = native_event_count;
    return true;
#else
    if (error != NULL)
        *error = "CPU trace requires C_HEAVY_DEBUG";
    return false;
#endif
}

bool DebuggerAdapter::IsTraceComplete() const
{
#if C_HEAVY_DEBUG
    return !DEBUG_AgentTraceIsActive();
#else
    return false;
#endif
}

bool DebuggerAdapter::TerminateTarget(std::string* error) const
{
    if (!RequireAvailable(error) || !RequireEmulationThread(error))
        return false;

#if C_DEBUG
    DOS_Terminate(dos.psp(), false, 0);

    // Keep the externally initiated exit equivalent to DOS INT 21h/AH=4Ch.
    // DOS_Terminate owns PSP/vector restoration; the interrupt handler owns
    // these process-lifecycle fields after it returns.
    dos_program_running = false;
    appname[0] = 0;
    appargs[0] = 0;
    reg_ax = 0x3e01;

    // DOS_Terminate prepares an IRET frame for the INT 20h/21h termination
    // handler. Agent termination bypasses that handler, so complete the same
    // frame transition before resuming the shell.
    const std::uint16_t return_stack_segment = SegValue(ss);
    const std::uint16_t return_instruction_pointer = real_readw(return_stack_segment, reg_sp);
    const std::uint16_t return_code_segment = real_readw(return_stack_segment, reg_sp + 2u);
    const std::uint16_t return_flags = real_readw(return_stack_segment, reg_sp + 4u);
    reg_sp += 6u;
    SegSet16(cs, return_code_segment);
    reg_ip = return_instruction_pointer;
    reg_flags = (reg_flags & 0xffff0000u) | return_flags;

    if (DEBUG_AgentResumeAfterTerminate())
        return true;
    if (error != NULL)
        *error = "Debugger did not resume the DOS shell after target termination";
    return false;
#else
    return false;
#endif
}

} // namespace dosbox_agent
