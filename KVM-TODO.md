# KVM CPU core: plan and status

A CPU core (`core=kvm`) that runs guest code with hardware virtualization through the Linux
KVM interface instead of emulating the CPU. DOSBox-X keeps emulating all devices (PIC, PIT,
VGA, sound, DOS and BIOS callbacks); KVM only executes instructions.

The main design constraint: DOSBox-X keeps time by counting CPU cycles, and KVM cannot count
guest instructions. The kvm core counts a cycle per nanosecond of host time the guest runs
(step 4), so `CPU_CycleMax` becomes the guest execution time per emulated millisecond:
`cycles=max` leaves room for device emulation automatically, and a fixed `cycles=` value
throttles the guest by duty cycle.

## Done

- **Callback instructions with an INT3 prefix.** `[cpu] callback int3 prefix` (auto/true/false;
  auto = on when `core=kvm`). Callbacks are written as `CC FE 38 imm16` so the kvm core sees
  them as #BP exits (`KVM_GUESTDBG_USE_SW_BP`), since #UD never reaches userspace. All emulated
  cores, the debugger disassembler and debugger breakpoints understand the prefixed form. Save
  states with the other encoding are refused. See `include/callback.h`
  (`CALLBACK_WriteInstr*`, `CALLBACK_Int3PrefixFollows`).
- **Step 1: memory map change notification.** `MEM_AddMapChangeHandler()` in `include/paging.h`.
  - `MEM_MAPCHANGE_PHYS`: what guest physical pages decode to may have changed (page
    handler, host memory behind the page, or the A20 gate alias; `pages == 0` means all).
    Reported from `MEM_RegisterHandler`, `MEM_InvalidateCachedHandler` (so memory callout
    install/uninstall and LFB moves), `MEM_SetPageHandler`, `MEM_ResetPageHandler_*`, the
    physmem map/unmap functions, A20 changes, RAM and address mask setup, memory save-state
    load, the end of `VGA_SetupHandlers` (SVGA/Tandy/PCjr bank switching changes the host
    pointer without changing the handler), and Glide enable/disable.
  - `MEM_MAPCHANGE_FIRSTMB`: `paging.firstmb[]` changed. This is a linear-to-physical remap
    of the first 1MB+64KB that DOSBox-X applies **only while paging is off**, and DOSBox-X's
    EMS emulation maps its page frame with it (`PAGING_MapPage`), not with page handlers.
    Reported from `PAGING_MapPage`, paging init and paging save-state load.
  - Rule for new code: changing the memory map must call `MEM_MapChanged()`.
    `PAGING_ClearTLB()` alone is not enough (and too frequent to use as the signal, since
    every CR3 load clears the TLB).
- **Step 2: page classification.** `MEM_ClassifyPage(page)` returns `MEM_PAGE_RAM` (reads and
  writes direct to `host`), `MEM_PAGE_READONLY` (reads direct, writes to the handler: ROM, or
  a device that maps only reads; becomes a `KVM_MEM_READONLY` slot) or `MEM_PAGE_DEVICE` (no
  slot, every access is an MMIO exit to the handler). It applies the A20 alias like
  `PAGING_LinkPage`.
- **Step 3: neutral CPU state.** `include/cpu_state.h`: `CPU_FullState`,
  `CPU_GetFullState()` and `CPU_SetFullState()`. The fields follow `kvm_regs`, `kvm_sregs`,
  `kvm_fpu` (FXSAVE layout), the debug registers and the SYSENTER MSRs. `CPU_SetFullState()`
  sets CR0 without `CPU_SET_CRX(0)`'s core and cycles auto-switching.
- **Step 4: time accounting from host time.** `include/pic.h`: `PIC_HostTimeNow()`,
  `PIC_HostTimeMark()`, `PIC_HostTimeBudget()` and `PIC_HostTimeAccount()`. Emulated time
  stays in cycles; the core counts one cycle per nanosecond of host time the guest runs
  (above 1000000 cycles per millisecond a cycle is shorter, so an emulated millisecond never
  needs more than a host millisecond of guest execution). A charge can overrun the slice by
  at most one emulated millisecond, so a long stop is not paid back.

  Tested with a stand-in core in a scratch build that spins on host time like a vCPU, with a
  simulated exit every 20 us (and every 1 ms) over 6 s of wall time:
  - `cycles=max`: emulated time 100.02% of wall time, `CPU_CycleMax` settles near 915000
    (about 91% of each millisecond for the guest; it starts at 200 when the system was idle
    and ramps up, so the guest got 72% over the whole 6 s), thread CPU only ~3 points above
    guest time even with 320000 exits.
  - `cycles=fixed 300000`: guest 29.9% of wall time, emulated time 100.00%.
  - `cycles=fixed 1000000` and above: the guest gets 97% and emulated time runs 2.6% slow (the
    loop's own overhead has no room), as with an emulated core whose fixed cycles are too
    high. Use `cycles=max` (or fixed below about 950000) with the kvm core.
  - Emulated time seen at exits never went backwards with fixed cycles, and once per run with
    `cycles=max`, presumably where auto-adjust changed `CPU_CycleMax` in the middle of a
    millisecond (`PIC_TickIndex()` divides by it; existing behaviour, not verified).
  - Worst overrun past a slice: 0.05-0.9 ms, from host scheduling and the timer granularity.

  **Convention:** every nanosecond calculation made for the KVM core gets a comment saying so
  (as in `PIC_HostTimeBudget()`), so that they are not mistaken for emulated time, which is in
  cycles and milliseconds.
- **Steps 5-7: scaffolding.**
  - `./configure --enable-kvm` (Linux on x86-64 only, needs `linux/kvm.h`) defines `C_KVM`.
    The core is `src/cpu/core_kvm.cpp` and `include/core_kvm.h` (in the Visual Studio project
    too; the file compiles to nothing without `C_KVM`).
  - `[cpu] core=kvm` is offered only in such builds. Core selection (end of
    `CPU::Change_Config`, once the CPU type and features are known) falls back, logging
    `CPU: core=kvm is not used: <reason>`, when the INT3 callback prefix is off, when
    `cputype` is below 386, or when `KVM_CoreInit()` fails (no `/dev/kvm`, no permission,
    wrong API version, a missing capability, `kvm_intel` without unrestricted guest, too few
    memory slots, or an ioctl failing). The fallback is the core the CPU type chose (normally
    the normal core). The automatic core switching only applies to `core=auto`, so it leaves
    kvm alone.
  - The CPU is set up before `CALLBACK_Init()`, so the selection calls
    `CALLBACK_ChooseInt3Prefix()` (now public, done once) to know the prefix setting.
  - `KVM_CoreInit()` checks the host (API 12; `KVM_CAP_USER_MEMORY`, `READONLY_MEM`,
    `SET_GUEST_DEBUG`, `EXT_CPUID`, `SET_TSS_ADDR`; at least 32 memory slots), creates the VM,
    sets the TSS address (needed on Intel), creates vCPU 0, maps its `kvm_run` area and sets
    the CPUID. No `KVM_CREATE_IRQCHIP`. Everything is closed at exit.
  - **CPUID** is DOSBox-X's own (`CPU_CPUID()`, leaves 0 to its maximum, including a custom
    `cpuid string`), with the leaf 1 feature bits the host KVM cannot provide removed (logged).
  - **For now the selection always falls back** ("the KVM virtual machine is ready, but the
    core cannot run guest code yet"), since there is no run loop. Step 9 replaces that with
    `cpudecoder = CPU_Core_KVM_Run`.
  - Tested on an AMD Ryzen 9 7940HS host: VM and vCPU created (32764 memory slots, immediate
    exit supported); each fallback reason; CPU types 486old (no CPUID), Pentium III, Pentium 4
    with SSE2; and the INT3/mouse/V86/DOS/4GW suite with `core=kvm` (prefixed callbacks and a
    live VM), EMM386 on and off.
- **Steps 8-13: the first milestone, the KVM core runs DOS.** `CPU_Core_KVM_Run()` in
  `src/cpu/core_kvm.cpp` is the `cpudecoder` for `core=kvm`. It boots to the DOS prompt and runs
  real mode, V86 mode (DOSBox-X's EMM386) and DOS/4GW protected mode programs (raw and VCPI).
  - **Memory slots (8):** built from `MEM_ClassifyPage()` before each `KVM_RUN` for what the
    map change notifications marked dirty; slots that did not change are kept. Read-only pages
    become `KVM_MEM_READONLY` slots. Device pages, and host memory that is not page-aligned,
    get no slot. The first MB+64KB follows `paging.firstmb[]` while paging is off (EMS).
  - **Run loop (9):** `PIC_HostTimeMark()`, `KVM_RUN` bounded by a per-thread `CLOCK_MONOTONIC`
    timer whose signal (SIGRTMIN+4) sets `immediate_exit`, `PIC_HostTimeAccount()` first on
    return. A run gets at least 2 us, so the guest always gets somewhere. Exits:
    - port I/O: the new `IO_ReadB_Checked()` and so on, without `CPU_IO_Exception()` (the CPU
      has checked IOPL and the TSS I/O bitmap already; a denied V86 port raises #GP in the
      guest, which goes to DOSBox-X's V86 monitor through the guest's IDT);
    - MMIO: `physdev_read*()`/`physdev_write*()`;
    - HLT: the state comes back and `CPU_HLT()` makes `HLT_Decode` wait for the interrupt, which
      then returns to this core;
    - #BP (`KVM_GUESTDBG_USE_SW_BP`; `debug.arch.pc` is the INT3's linear address): a callback
      (INT3 + FE 38 imm16) returns the callback number with EIP past it; otherwise a debugger
      breakpoint (`DEBUG_Breakpoint()`/`DEBUG_IntBreakpoint(3)`), otherwise the guest's own INT3,
      delivered with `CPU_SW_Interrupt_NoIOPLCheck(3)`;
    - triple fault: `On_Software_CPU_Reset()`; anything unexpected (internal error, failed entry,
      unknown exit): log the reason, CS:EIP and code bytes, and switch to the normal core.
  - **Interrupts (10):** DOSBox-X delivers them, with `CPU_HW_Interrupt()` in `PIC_runIRQs()`,
    to the state in DOSBox-X (real mode IVT, protected mode IDT and gates, V86). When an IRQ is
    pending and the guest's IF is clear, `request_interrupt_window` ends the slice as soon as it
    can take it. `KVM_INTERRUPT` is not used. NMI is not handled.
  - **State sync (11), lazy:** the state stays in the vCPU from slice to slice. Code outside a
    CPU core that reads or changes the CPU state calls `CPU_SyncState()` (`cpu.h`) first, which
    fetches it into DOSBox-X; the core puts it back before the next `KVM_RUN`, also when an exit
    handler fetched it. Called from `CPU_Interrupt()`, `PIC_runIRQs()`, `CPU_OnReset()`,
    `On_Software_CPU_Reset()`, `CPU::Change_Config()`, `SaveState::save()`/`load()`,
    `DEBUG_EnableDebugger()` and `DEBUG_Enable_Handler()`. **Rule for new code:** anything
    outside a CPU core and outside callbacks that uses `reg_*`, `Segs`, `cpu.*` or the FPU must
    call `CPU_SyncState()` first. Callbacks already have the state.
    V86 segments are normalised on the way in (base = selector << 4, limit FFFFh, AR F3h); a
    missing TR becomes a usable busy TSS; null TR/LDT selectors come back as unusable.
  - **Debugger (12):** while the debugger is stopped, or `DEBUG_NeedsEmulatedCore()` says it
    needs every instruction (heavy debugging breakpoints and logging, interrupt breakpoints), the
    core fetches the state and runs `CPU_Core_Normal_Run()` instead. So breakpoints, BPINT, F11,
    STEPIN and the log commands work as with the emulated cores, but the guest runs emulated
    while there are breakpoints in a heavy debugging build. Without heavy debugging, physical
    breakpoints are INT3s in memory and stop the KVM core through #BP (checked before running a
    callback, as the emulated cores do). The one instruction that RUN, F5 and F10 step past a
    breakpoint (with `inhibit_int_breakpoint` set) also runs on the normal core, since KVM cannot
    run exactly one.
  - **Save states (13):** `SaveState::save()`/`load()` call `CPU_SyncState()`; the loaded state is
    put into the vCPU at the next run, and memory map changes from the load rebuild the slots.
  - **Cycles:** with `cycles=auto` the KVM core adjusts the cycles from the start as `cycles=max`
    does, and with auto-adjusting cycles it starts at 500000 (half of the host time) instead of a
    few thousand.
  - **Measured** (AMD Ryzen 9 7940HS): a `dec ecx`/`jnz` loop of 3.2 billion iterations takes
    1.9 s (dynamic core 8.5 s). Startup to the DOS
    prompt takes 0.5 s (dynamic core 0.16 s): the first slot build classifies 4 GB of pages
    (165 ms), and a callback round trip (exit, fetching and putting the state, entry) costs about
    25 us, against about 1 us on an emulated core.
  - **DOSBox-X bugs found and fixed on the way** (they affect every core):
    - `VGA_SetupDrawing()` left `vga.draw.resizing` set when it ran in `M_ERROR` mode, so a mode
      set after it never started drawing (no display, no vertical retrace events). It happened
      under KVM because the POST reached the VGA BIOS init later in emulated time.
    - The RTC: the first update after a reset rounded the time of the last update up instead of
      down (`fmod()` of a negative time), so status register A reported an update in progress
      for a whole second. Any program that read the RTC status in its first emulated second
      waited up to a second (the BIOS POST did, under KVM).
  - Tested: the INT3/mouse/V86/DOS/4GW suite with EMM386 off and on; the debugger breakpoint
    test with F5, RUN and RUNWATCH; STEPIN into a callback; VRT, SAVESTATE and LOADSTATE while
    the KVM core runs a loop.
- **Step 15: exit performance.** Measured with four small DOS programs (wall time including
  about 0.3 s of startup; dynamic core in brackets):

  | Program | What it does | Before | Now | (dynamic) |
  |---|---|---|---|---|
  | CBBENCH | 100000 INT 21h AH=30h (callback round trips) | 2.8 s | 1.4 s | (0.55 s) |
  | IOBENCH | 1000000 `IN AL,61h` (port I/O exits) | 9.8 s | 11.0 s | (1.65 s) |
  | VGABENCH | mode 13h, 20 full-screen fills | 6.6 s | 0.8 s | (0.51 s) |
  | VESABNCH | VESA mode 101h, 20 frames of 5 banks | - | 0.6 s | (0.70 s) |

  - **Registers through `kvm_run`** (`KVM_CAP_SYNC_REGS`): the general and segment registers
    (and the vCPU events) come with every exit and go back with `kvm_dirty_regs`, without
    ioctls; the segment registers only when they changed. The FPU, debug registers and MSRs
    (the "rest") stay in the vCPU until something needs them: `CPU_SyncState()` fetches
    everything, the new `CPU_SyncRegisters()` only the registers (callbacks, HLT, interrupt
    delivery). A callback round trip went from about 25 us to about 12 us.
  - **The slice timer** is armed once per slice at an absolute deadline
    (`timer_settime(TIMER_ABSTIME)`), not around every `KVM_RUN`.
  - **Coalesced MMIO** (`KVM_REGISTER_COALESCED_MMIO`) for A0000-BFFFF, and on PC-98 also for
    the fourth graphics plane at E0000-E7FFF: writes are queued in a ring in `kvm_run` instead of
    exiting, and `KVM_FlushWrites()` applies them through `physdev_write*()` as soon as
    `KVM_RUN` returns, before any exit is handled, so the order against reads and port I/O is
    kept.
  - **VGA memory is page-aligned** (`VGA_SetupMemory()`), so the pages that the SVGA bank map
    and the LFB give as host memory are mapped as RAM slots: banked VESA modes and the LFB run
    without exits.
  - **An unfinished instruction**: KVM completes an I/O or MMIO exit's instruction (with the
    data the handler left in `kvm_run`) only at the next `KVM_RUN`, and a REP string
    instruction exits again for each element. The vCPU state is not consistent before that, so
    `KVM_CompletePending()` finishes it with `immediate_exit` set before the state leaves the
    vCPU (in the sync hook). Without it, DOSBox-X delivered an interrupt into the middle of a
    `REP SCASB` on video memory.
  - **Events in flight**: when a timer signal ends `KVM_RUN` while KVM is in the middle of
    delivering a software interrupt or exception (after a page fault on the stack, typically
    right after a memory slot change), KVM keeps it and delivers it again at the next
    `KVM_RUN`, on top of whatever state DOSBox-X put. With an IRQ delivered in between, the
    return address pushed was wrong (seen: a VESA bank switch `INT 10h` ended up in the BIOS's
    boot failure stub, about one run in 20). `KVM_DropEvent()` writes the events back with
    nothing being delivered whenever DOSBox-X changed the state; the instruction, which has
    not completed, raises the event again. `KVM_GET_VCPU_EVENTS` does not even report software
    interrupts, so this has to be done blind.
- **Step 14: protected mode, V86, DOS extenders and Windows.**
  - **Windows 3.1** runs in standard mode (`win /s`) and 386 enhanced mode (`win /3`): a Win16
    test program (`GetWinFlags()`, a 1 MB huge block written and checked, the FPU, 2 seconds
    of `WM_TIMER`) gives the same results as the normal core, and Windows exits normally.
    Enhanced mode first failed with "Error loading USER.EXE": WIN386 changes its page tables
    (and reloads CR3 with the same value) inside the vCPU, so DOSBox-X's own TLB, which the DOS
    callbacks use to reach the guest's buffers, was out of date. `KVM_GetState()` now clears
    DOSBox-X's TLB whenever it fetches a state with paging on.
  - **Windows 3.0** runs the same test in standard and enhanced mode. (The 1 MB allocation fails
    in standard mode, on the normal core too.)
  - **Windows 95** (build 950) boots from a copy of the disk image and runs a Win32 test
    program started from `WIN.INI` (16 MB of memory written and checked, the FPU, `Sleep(2000)`
    against `GetTickCount()` and the performance counter, two threads with interlocked
    increments), with the same results as the dynamic core (`Sleep(2000)` measures 2003-2007 ms
    by both clocks), and shuts down. AUTOTYPE presses Enter every 10 seconds to get past the
    logon dialog. **Windows 98** is not tested yet: the image does not get to the desktop with
    the test configuration on the dynamic core either (an INT 15h AX=D800 loop, then back to
    text mode).
  - **DOS/4GW** (raw and VCPI, EMM386 off and on), and as the extender of the LFB and Voodoo
    tests below (DPMI 0800h physical mappings). Raw mode triple faulted now and then: a state
    fetched while DOS/4GW switches modes (between setting or clearing CR0.PE and the far jump
    after it) came back with real mode segments rebuilt from the protected mode descriptor
    tables, not present. `KVM_PutState()` now keeps the vCPU's own descriptor cache for the
    segment registers that DOSBox-X has not loaded since the fetch (same selector, base and
    limit, same mode; not in V86 mode, whose segments are normalised anyway). Also,
    `PIC_runIRQs()` now asks for the interrupt flag through `CPU_InterruptFlag()` first, which
    the KVM core answers from the last exit, and fetches the state only to deliver an
    interrupt: while the guest ran with interrupts disabled and an IRQ was pending, every slice
    used to be a state round trip. 48 runs without EMM386 and 18 with it all passed afterwards.
  - **Page faults raised by DOSBox-X code**: in a callback, a guest page fault goes through
    `PAGING_PageFault()`'s recursive path, which swaps `cpudecoder` for `PageFaultCore` and runs
    the guest's page fault handler on the full core with the state DOSBox-X already has; the
    KVM core is not involved. While the KVM core itself runs, `dosbox_allow_nonrecursive_page_fault`
    is set, and the only linear reads it does are those of the bytes after an INT3: the
    `FE 38` check stays within the INT3's page, and the callback number (pc+3) can only fault
    for a callback instruction that straddles a page boundary, which becomes a #PF in the
    guest at the INT3, as an emulated core's instruction fetch would.
- **Step 16: timing.** KVM's own exit costs (going into and out of the guest, a few us; KVM's
  instruction emulation for MMIO; queued writes, which are exits that KVM handles itself) are
  no longer charged as guest time: `KVM_Overhead()` estimates them per kind of exit (port I/O,
  MMIO, callback, HLT, queued write by one instruction, queued writes by a string instruction)
  from the shortest runs that ended with them, and `PIC_HostTimeAccount()` takes the estimate
  off. The log line at exit shows the estimates.
  - Since the exits during a slice are no longer charged, the slice takes longer than its
    budget, and the slice timer would fire early, an exit of its own: `KVM_TimerArm()` also
    moves the deadline later when it would fire more than a quarter early, and a slice ends at
    an exit when less of it is left than the exit cost.
  - A fixed `cycles=30000` VGA test (84000 MMIO exits, 84000 queued writes) went from 32.5 s to
    1.5 s (normal core 0.3 s). IOBENCH (port I/O exits only) got slower, 9.9 s to 11.0 s:
    `cycles=max` settles at about 21000 cycles there, so the slices are a few exits long and
    arming the timer is a noticeable part of each.
  - A Sound Blaster test (8-bit single-cycle DMA, 32000 bytes at 22050 Hz, three times, timed
    with the BIOS tick count to the IRQ) measures 26 ticks per block on the normal core and on
    KVM, at `cycles=max` and `cycles=30000`, and `DX-CAPTURE /A` records the same audio: three
    1.44 s blocks with 40 ms gaps.
  - Windows 95: `Sleep(2000)` measures 2003-2007 ms by `GetTickCount()` and the performance
    counter.
- **Step 17: machine coverage.**
  - **PC-98**: DOS runs; a test that writes text VRAM (A000) and the graphics planes (A800,
    B000, B800, E000) and dumps the readback gives byte-identical dumps on the normal core and
    KVM, with no MMIO exits (writes queued, reads direct).
  - **VESA LFB**: a DOS/4GW program sets mode 4101h, maps the LFB with DPMI 0800h, writes bytes,
    words and dwords and reads them back: identical to the normal core, no exits (the LFB is
    direct-mapped VGA memory).
  - **Voodoo**: a DOS/4GW program finds the card through the PCI BIOS, maps BAR0, reads the
    status register and writes and reads 64 KB of the LFB: identical to the normal core. Every
    access is an MMIO exit.
  - **Glide**: the Glide port handler takes guest linear addresses, so it now calls
    `CPU_SyncRegisters()` first (the paging state). Not tested: no Glide program headless.
- **Debugger tools for all of the above.**
  - `MEMMAP [start [end]]` lists the physical map as `MEM_ClassifyPage` sees it (coalesced
    runs with kind, host pointer relative to `MemBase`, and handler class), plus the map
    change notifications since the last `MEMMAP`.
  - `CPUSTATE` shows `CPU_FullState` and checks that `CPU_SetFullState(CPU_GetFullState())`
    leaves DOSBox-X's own state unchanged.

  Tested in real mode, V86 (DOSBox-X EMM386), DOS/4GW 16-bit kernel and 32-bit flat code,
  with paging (VCPI) and without; all round trips OK.

## Findings to keep in mind

- **Segment access rights are not kept by DOSBox-X.** Only base, limit, expand-down, and the
  code/stack size are cached when a segment is loaded. `CPU_GetFullState()` rebuilds the access
  rights from those, the CPL, and (in protected mode) the descriptor tables. A guest that
  changes a descriptor after loading the segment gets slightly different type/DPL/AVL bits,
  and around a mode switch the result can be a not present segment. The KVM core works around
  it by keeping the vCPU's cache for segments DOSBox-X did not load (step 14). The fully
  correct fix is to store the access rights in `Segs` at every load (about 25 places in
  `cpu.cpp` set `Segs.expanddown[]`).
- **V86 segments must be normalised for KVM.** VM entry requires base = selector << 4, limit
  = FFFFh and access rights F3h for every segment in virtual 8086 mode. DOSBox-X does not
  reset the limit on real-mode or V86 segment loads (`SegSet16`), so a V86 segment can keep a
  protected-mode limit (seen: `CS limit=FFFFFFFF` under EMM386).
- **`segment limits=false`** stores `~0` limits (unreal everywhere). KVM always enforces
  limits, so under KVM that option has no effect. It is on by default.
- **EMM386 V86 runs without paging** (`CR0=00000011`), so the `firstmb` remap is in effect in V86
  mode too. The kvm core's slot layout for the first MB+64KB must apply `firstmb` while
  `CR0.PG=0` and must not while `CR0.PG=1`. That means a slot rebuild on every PG transition
  (rare). Pages whose `firstmb` entry is not the identity need their own slots (the EMS
  frame is 16KB granular: 4 pages per mapping).
- **Host pointers must be page-aligned for KVM slots.** `MemBase` is page-aligned when it
  comes from mmap (normal Linux build, memory file). It is not page-aligned from `malloc`
  (GameLink) or from `new[]` (the non-mmap fallback). VGA memory (`vga.mem.linear`) is
  allocated page-aligned since step 15.
- **Mode 13h and planar VGA go through device handlers** (`VGA_ChainedVGA_Handler`,
  `VGA_UnchainedVGA_Handler`), so every access is MMIO; writes are queued (coalesced MMIO),
  reads exit. Only the SVGA bank map (`VGA_Map_Handler`), LFB, Tandy/PCjr and Hercules
  handlers map host memory directly.
- **A queued write still costs an exit inside the kernel**: a single write instruction is a
  full exit that KVM handles itself (about 3-4 us on the test machine); the writes of a
  string instruction (`REP STOSB`) are queued by KVM's instruction emulator in one go (well
  under 1 us each). Only the userspace round trip is saved.
- **KVM delivers software interrupts again after an interrupted delivery**, and
  `KVM_GET_VCPU_EVENTS` does not report them (the API has no field for the instruction
  length). Any change of the state by DOSBox-X has to write the events back (see
  `KVM_DropEvent()`).
- **DOSBox-X's TLB** is filled from guest page tables that the guest changes inside the vCPU
  without DOSBox-X seeing it (INVLPG, CR3 reloads with the same value), so it is cleared at
  every state fetch with paging on. Code that reads guest linear memory outside callbacks
  (device handlers, events) also needs `CPU_SyncRegisters()` for the current CR3.
- **RDTSC** reads the host's TSC (with KVM's offset), not a count derived from emulated
  cycles. Its rate is the host CPU's.
- **`MEM_SlowPath` conflicts**: when two devices claim a page, the handler table entry is left
  empty and resolved on every access. `MEM_ClassifyPage` returns whichever handler won.
- **Guest physical 0xfffbc000-0xfffbffff is KVM's** (identity map page and the TSS set with
  `KVM_SET_TSS_ADDR`, see `kvm_tss_addr` in `core_kvm.cpp`). No memory slot may cover it.
- **CPU types without CPUID** (386, 486old): under KVM the CPUID instruction still runs (it
  returns zeros) and the EFLAGS ID bit can still be toggled, so CPUID detection finds a CPUID.
  Likewise `fpu=false` cannot remove the FPU. These are limits of running on real hardware.
- **Changing the CPU type at run time** calls `KVM_CoreInit()` again, which sets the CPUID
  again. Newer kernels refuse `KVM_SET_CPUID2` with different values after the vCPU has run,
  so once the core runs, a CPU type change needs a new vCPU (keeping the state through
  `CPU_FullState`).
- **Prefetch CPU types** (`*_prefetch`) stop DOSBox-X with an error unless the core is normal
  or auto, which includes kvm.
- **Pages above 4GB** (`memory file` + `Mem4GBPageHandler`) are plain RAM in the memory file;
  they need a slot of their own when PSE-36/PAE guests use them.

## Open issues of the KVM core

- **Callback round trips** cost about 12 us (an emulated core: about 1 us), most of it the
  exit and entry themselves. `CALLBACK_Idle()` is a round trip per call in DOSBox-X's own wait
  loops. Ideas: handle the hottest callbacks (INT 21h, INT 16h polling) with fewer state
  transfers, e.g. without putting the segment registers back when they did not change (done)
  and without fetching the state for callbacks that only read a few registers.
- **Port I/O exits** cost 5-7 us each in the kernel, so a guest that polls a port (a PIT or
  status port in a tight loop) runs at a fraction of the emulated cores' speed (IOBENCH).
  Ideas: coalesced PIO (`KVM_CAP_COALESCED_PIO`) for write-only ports whose writes have no
  immediate effect. Port reads always exit to userspace.
- **Exit-heavy guests at low fixed cycle counts** still run slower than on an emulated core
  (the VGA test at `cycles=30000`: 1.5 s against 0.3 s): exit costs vary, and the estimate
  (`KVM_Overhead()`) errs towards charging the guest, so that it never gets free time.
- **Short slices** (exit-bound guests at `cycles=max`) spend a noticeable part of their time
  arming and disarming the slice timer (two syscalls per slice).
- **The first slot build** classifies every page up to 4 GB (165 ms), and an A20 change
  reclassifies all of them (4-6 ms). Limit it to where something is mapped, or reclassify only
  pages whose handler could change.
- **Paging changes inside the guest are not noticed** until the state is fetched: the first MB
  layout (`firstmb`, EMS) follows the paging state DOSBox-X last saw. With
  `KVM_CAP_SYNC_REGS` the core can check CR0 after every exit.
- **Voodoo register and LFB writes are not queued**: every access is an MMIO exit. The PCI
  BAR can move, so a coalesced zone would have to follow `VOODOO_PCI_SetLFB()`.
- **A port handler that reads guest linear memory and faults** (`GuestPageFaultException`)
  while its I/O exit is still pending would raise the #PF after the I/O instruction instead
  of at it. No such handler is known to fault in practice (Glide's parameters are in DOS
  memory).
- **A KVM internal error at Windows 95 shutdown**, seen once in four runs: "suberror 3"
  (an exit while delivering an event) with a nested page fault at C000ABF0, in real mode at
  0661:0602 just before Windows reloads the IDT. The IDTR still had Windows' protected mode
  base, so delivering any event read the "IVT" from unmapped memory, which KVM cannot
  emulate during delivery. The core switches to the normal core when it happens (the shutdown
  then finishes). The event being delivered is not known yet; the log line now includes
  the vCPU events and the IDT.
- **NMI** is not passed on.

## Next steps

18. **Port I/O**: coalesced PIO for write-only ports, and see which ports games poll.
19. **Callbacks**: profile a round trip and cut what is not the exit itself.
20. **More guests**: OS/2, Windows NT/2000 (needs `cputype` with the right CPUID), Linux,
    DOS extenders with their own paging (CWSDPMI, DOS/32A, PMODE/W), a Glide game, games that
    use RDTSC for timing.
21. **The open issues above.**
