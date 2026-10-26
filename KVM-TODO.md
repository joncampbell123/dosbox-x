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
- **Step 18: port I/O.** `KVM_CAP_COALESCED_PIO`: writes to ports with no effect the guest
  could see before its next exit are queued in the coalesced MMIO ring, in order with the
  video memory writes, and applied (`IO_Write*_Checked()`) as soon as `KVM_RUN` returns; reads
  still exit, after the queue is applied. Ports: 80h and EDh (I/O delay), the VGA DAC
  (3C6h-3C9h), the PIT command port 43h (a latch takes effect at the read that follows), the
  DMA flip-flop resets 0Ch and D8h, and the VGA index registers 3B4h, 3C4h, 3CEh and 3D4h (a
  word OUT to an index register also writes the data register, falls outside the one-port
  zone, and exits). Not the VGA data registers, which can change the memory map. A palette
  benchmark (768 single OUTs to 3C9h per frame, 200 frames, then 200000 OUTs to 80h) went
  from 4.2 s to 1.7 s (dynamic core 0.8 s); a palette test reads the palette back through
  3C7h/3C9h after every load and passes. The log line at exit lists the busiest ports.
  Ports polled most:
  - Quake (DJGPP, CWSDPMI or HDPMI32): PIT channel 0 (latch at 43h, two reads of 40h) for
    its clock, the SB16 DMA count (C6h, after a D8h flip-flop reset), 201h (joystick), the
    SB16 IRQ acknowledge (22Fh). 412000 exits in the timedemo, 237000 with the queued writes.
  - Spycraft (DOS/4GW): the MPU-401 status port 331h, the RTC (70h/71h), the PIC, the DMA count.
  - Linux (booting an install CD): PIT, the CRTC (cursor updates, 3D4h/3D5h), the PIC.
  - These are reads, or writes whose effect is immediate (PIC EOI, RTC data); they have to
    exit.
- **Step 19: callback round trips.** Profiled with CBBENCH (100000 INT 21h calls): per call
  about 5.5 us in `KVM_RUN` (VM exit and entry, KVM's FPU and vCPU switching, the same for
  every userspace exit), about 3 us in DOSBox-X (the callback itself and the main loop, as
  on an emulated core), and under 1 us of the KVM core's own work. Cut:
  - the slice timer is no longer disarmed when the core returns (two system calls per call):
    it stays armed, and if it fires outside `KVM_RUN` (its handler also sets a `fired` flag,
    since `KVM_CompletePending()` uses `immediate_exit` too) the next `KVM_RUN` returns at once;
  - the vCPU events are written back only when one can be in flight (after a `KVM_RUN` that a
    signal ended, or a full state fetch), not after every callback;
  - the vCPU events are no longer synced through `kvm_run` at every exit.
  CBBENCH stays at about 1.4-1.5 s: the exit itself is what is left. The 0.19 s that a
  start-up spends classifying all 4 GB six times is the "first slot build" open issue.
- **Step 20: more guests** (first pass).
  - **Quake 1.06** (`timedemo demo1`, 320x200): 969 frames under CWSDPMI (its own paging)
    and under HDPMI32, the same as the emulated cores, but at 45 fps against 173-194 fps on
    the dynamic core (27 fps on the normal core). 44 of 60 s are in `KVM_RUN`: every write
    to mode 13h video memory is an exit that KVM handles in the kernel (100 million queued
    writes). See the next steps.
  - **Spycraft** (DOS/4GW, SVGA, SB16, MPU-401): runs for a minute with the same video modes
    and BIOS calls as on the normal core.
  - **Linux** (an install CD, `boot -el-torito`): the kernel boots, prints to the text
    console and idles with HLT; the log is the same as on the dynamic core.
  - **OS/2 Warp 3 and 4** installers: boot from the installation diskette and stop at the
    same point as on the dynamic core (waiting with the next diskette prompt). The floppy
    controller emulation is incomplete, so OS/2 cannot be tested further yet; changing disks
    from a headless test would also need AUTOTYPE to trigger mapper handlers (`swapimg`).
  - **Windows 98** (the image stalls on the dynamic core too with the test configuration),
    **REAL/32** (installer floppies only), **Windows NT/2000**, a Glide game and **DOS/32A,
    PMODE/W**: not tested yet. RDTSC timing: Windows 95's performance counter and Quake agree
    with the emulated cores.
- **Step 21: mode 13h shadow.** While all of A0000-AFFFF is the plain chained 256-colour
  handler (`vgaph.cvga`: chain 4, no raster op, bit mask or the like, not Tseng), the KVM core
  maps a 64KB shadow there instead, in the guest's byte order, with dirty logging
  (`KVM_MEM_LOG_DIRTY_PAGES`). Each aligned dword of the shadow is one dword of VGA memory
  (`VGA_ChainedShadowable()`, `VGA_ChainedShadowLoad()`/`Store()` in vga_memory.cpp).
  - The pages the guest wrote (`KVM_GET_DIRTY_LOG`) go to VGA memory whenever the core
    returns, so drawing, callbacks, save states and the other cores always see them, and
    before the map changes at A0000 (a mode set, a bank switch, a raster op), with the bank
    and mask the shadow was loaded with.
  - `VGA_MemWriteCount` counts the writes through the VGA memory handlers (BIOS text and
    pixel output, the other cores): when it changed, the shadow is loaded again before the
    guest runs.
  - Switching to the normal core stores the shadow and drops it.
  - Quake's timedemo: 45 fps -> 203-225 fps (dynamic core 173). VGABENCH 0.66 s -> 0.34 s.
    The VGA test at `cycles=30000` 1.5 s -> 0.56 s. A test that mixes direct writes with BIOS
    pixel writes and reads (VGA13) gives a byte-identical dump to the normal core.
- **Step 22: the first slot build.** It took 162 ms: most of the handler table above RAM is
  empty at first, and looking a page up asks every device (`MEM_SlowPath()`). Pages above the
  first MB whose handler is not known yet (`MEM_PageHandlerKnown()`) are now left as MMIO and
  marked deferred; an MMIO exit on a deferred page whose handler the access made known marks
  it for the next build, so that a PCI LFB, for example, gets its slot at the first access.
  Also, a page whose handler is the last one seen without memory behind it (not readable) is
  not looked at further. The first build now takes a few ms, and every program starts about
  0.25 s sooner (CBBENCH 1.5 s -> 1.25 s); rebuilds of the whole range (A20) take about 2 ms.
- **Step 23: more guests.**
  - **DOS/32A, PMODE/W and CauseWay** (Open Watcom's DOS extenders, the LFB and Voodoo
    tests linked for each): the same results as the normal core. (Testing them found a bug in
    the emulated cores' new segment limit checks, `LSS ESP,[ESP]`, since fixed.)
  - **Windows 95** boots and runs its test on the step 21-22 build, from a fresh copy of the
    image. (A copy that had been booted and cut off several times stopped before the test
    program on every core: Windows may show a dialog about the daylight saving time change, for
    example. Use a fresh copy for each run.)
  - Not available here: Windows NT/2000 install media, a Glide program. Windows 98 and the
    OS/2 installers wait on the dynamic core too (see step 20).
- **Step 24: more guests.**
  - **Windows 98** (`~/emu/win98`, with its own `dosbox.conf`: Pentium II, 128 MB, ISA PnP
    BIOS, APM, IDE `pio32`/`int13fakev86io`; without the NE2000 and MT-32): the image has
    `BootGUI=0` in `MSDOS.SYS`, so it stops at the DOS prompt and Windows is started with
    `WIN`. That was the "stall" of steps 14, 20 and 23. With `WIN` in `AUTOEXEC.BAT` of the
    test copy, the Win32 test passes on the dynamic core and on KVM (16 MB checked, the FPU,
    `Sleep(2000)` measures 2001-2006 ms by both clocks, two threads), and Windows powers off
    (APM), which ends DOSBox-X: 51 s on the dynamic core, 81 s on KVM.
  - **Windows 2000 Professional** (`~/emu/dostest/win95/win2k.iso`, retail, no product key
    here): `WINNT` run from the DOSBox-X shell with an answer file copies the local source to
    a copy of the 1 GB DOS disk (31 s), then the disk boots through IDE: text-mode setup (the
    NT kernel, `atapi.sys`) copies the files, restarts, and GUI setup detects and installs the
    devices and stops where the answer file has no product key ("Unattended Setup is unable to
    continue because a Setup parameter ... is missing"), which is as far as it can get here.
    Text-mode setup first took more than 30 minutes: KVM's instruction emulator does `REP
    OUTSW` one word per exit (it batches `REP INSW`), 133 million exits to the IDE data port at
    8.5 us. The IDE data port and taskfile writes are queued now (see step 18; 1F0h-1F3h,
    170h-173h), and text-mode setup takes about 7 minutes.
  - **Glide**: TRIP (Kosmic, The Party 97; DOS/4GW, Glide 2.43 with its own `glide2x.ovl`)
    on the emulated Voodoo (`voodoo_card=software`), answers to its menu redirected from a
    file (AUTOTYPE does not reach it). The KVM core gave up at once: "KVM internal error,
    suberror 1" at `FSTP dword [edi+4]`. Glide writes floats to the Voodoo registers with x87
    stores, and KVM's instruction emulator cannot do x87 instructions on MMIO. Such an
    instruction now runs on the normal core (`KVM_EmulateOne()`), and the core goes on in
    KVM: TRIP renders its parts on KVM, about 27000 instructions emulated per second. Without
    `KVM_CAP_EXIT_ON_EMULATION_FAILURE` (kernel 5.17+), KVM would exit only at CPL 0 and give
    the guest a #UD at CPL 3, so the core turns it on; checked at CPL 3 with a real mode
    program in V86 mode (EMM386 active) that does x87 loads and stores on mode 12h memory:
    the same results as the normal core. TRIP shows only a black screen on the emulated
    cores unless `enable msr=true`: it reads MSRs, which KVM answers whatever DOSBox-X's
    setting is (see the findings). It does not end by itself, so its own statistics file is
    only written when it is left with Esc.
  - **OS/2**: still waits for the floppy controller emulation (no change since step 20).
  - **DOSBox-X did not quit** (SIGTERM, `-time-limit`, closing the window) while a protected
    mode program ran on the KVM core with its output redirected to a file: closing the file,
    `LocalFile::UpdateLocalDateTime()` asks the DOS date and time with `CALLBACK_RunRealInt()`,
    which DOSBox-X's own INT 21h answers only in real or V86 mode; in protected mode it went
    to DOS/4GW's handler and never came back. It now uses the host's time unless the CPU is
    in real or V86 mode (the emulated cores could hit this too, depending on where the guest
    was), and the `CALLBACK_RunReal*()` functions call `CPU_SyncState()` first.
- **Step 25: planar VGA.** Measured, and the answer is yes, 16-colour and unchained games are
  slow on KVM. FastDoom 0.9.1, `-timedemo demo1 -nosound`, AMD Ryzen 9 7940HS, `cycles=max`:

  | Renderer | normal | dynamic | KVM |
  |---|---|---|---|
  | mode 13h (`FDOOM13H`, the step 21 shadow) | 143 fps | 217 fps | 1725 fps |
  | EGA 16-colour planar (`FDOOMEGA`) | 54 fps | 187 fps | 19.4 fps |
  | mode Y, unchained 256 colours (`FDOOM`) | 103 fps | 339 fps | 4.5 fps |

  In planar and unchained modes all of A0000-AFFFF is MMIO: every read exits, and every write
  is queued but still an exit inside the kernel (about 4 us each in mode Y: 264 million of
  them in the timedemo). A planar benchmark (PLANBNCH, wall seconds including about 1.2 s of
  start-up; normal, dynamic, KVM before and after the change below): 200000 PutPixels (bit
  mask OUT, latch load, write) 1.43 / 1.48 / 8.81 / 5.27; 768000 plane reads 1.16 / 1.18 /
  3.27 / 3.25; mode X fills 1.20 / 1.23 / 1.98 / 2.11.
  - **Done:** the sequencer and graphics controller data ports (3C5h, 3CFh) are queued as
    well while no page of A0000-BFFFF is mapped directly (`KVM_QueueVGAData()`, switched at
    each slot rebuild): map mask, bit mask, set/reset, write mode and the like were an exit
    each. All video memory accesses are MMIO then, so they are applied in order with the port
    writes, also when a port write changes the memory map. PutPixel 7.6 s -> 4.0 s above
    start-up, FastDoom EGA 17.2 -> 19.4 fps. PLANTEST (map mask, set/reset, bit mask, write
    modes 0-3, XOR, latch copies, read mode 1, a memory map switch to B8000 written right
    after, chain 4 off and on in mode 13h/mode X with writes right after) dumps the same
    219600 bytes on the normal, dynamic and KVM cores.
  - **Not done here:** what is left is the exits themselves, and a shadow like mode 13h's
    cannot take reads (they load the latches) or the four planes behind one address. Step 26
    runs those stretches on an emulated core instead.
- **Step 26: planar and unchained VGA on an emulated core.** `[cpu] kvm vga core` (auto,
  dynamic, normal, off; auto is the dynamic core, or the normal core in builds without one).
  - **Switching:** the KVM core estimates how much of the guest's time in KVM goes to exits
    for A0000-BFFFF (MMIO exits and queued writes, at the measured exit costs); when that is
    40% or more over 20 ms of guest time, it fetches the full state and runs the guest on the
    emulated core, from `CPU_Core_KVM_Run()`, which stays the `cpudecoder`. Every 50 emulated
    ms it prices the period's video memory accesses through the memory handlers
    (`VGA_MemWriteCount`, the new `VGA_MemReadCount`: they do not count host memory mapped
    directly, such as the SVGA bank map or the LFB) at KVM's exit costs, and goes back to KVM
    when they would take less than 25% of the time, when the mode 13h shadow applies, or when
    the guest sets the trap flag (KVM single steps natively).
  - **Going back:** the dynamic core's code cache is reset. The guest changes code under KVM
    without DOSBox-X seeing it, and the dynamic core's code pages have page handlers of their
    own (`MEM_SetPageHandler()`), which would be MMIO under KVM; the reset puts the old ones
    back and the slots follow.
  - **TSC:** on the emulated core, `CPU_RDTSC()` gives the host's TSC plus KVM's offset for the
    guest (`CPU_RDTSCHook`), so it goes on at the same rate and never goes back.
  - **Cycles:** with auto-adjusted cycles, KVM's (nanoseconds) and the emulated core's
    (instructions) are kept apart and swapped at each switch; the cycles left in the current
    millisecond are scaled and the auto-adjustment starts over. The emulated core starts at
    200000. Fixed cycles stay as they are.
  - **Measured:** FastDoom EGA 19.4 -> 217 fps (dynamic core 187), mode Y 4.5 -> 330 fps
    (dynamic 339), mode 13h unchanged (about 1650 fps, no switches). HYBTEST (8 rounds of
    planar PutPixels and CPU-only work, which switches every round) gives the normal core's
    checksum, RDTSC never goes back and runs at the same rate in both phases. Windows 3.1 with
    the 16-colour VGA driver (`win /3` and `win /s`) passes its test, switching twice;
    Windows 98 passes and boots faster (72 s; four switches at its VGA boot screens); PLANTEST,
    VGA13, LFB, Voodoo and FSTPTEST dumps are the same as the normal core's; TRIP does not
    switch (the Voodoo is not in A0000-BFFFF).
- **Debugger tools for all of the above.**
  - `MEMMAP [start [end]]` lists the physical map as `MEM_ClassifyPage` sees it (coalesced
    runs with kind, host pointer relative to `MemBase`, and handler class), plus the map
    change notifications since the last `MEMMAP`.
  - `CPUSTATE` shows `CPU_FullState` and checks that `CPU_SetFullState(CPU_GetFullState())`
    leaves DOSBox-X's own state unchanged.

  Tested in real mode, V86 (DOSBox-X EMM386), DOS/4GW 16-bit kernel and 32-bit flat code,
  with paging (VCPI) and without; all round trips OK.

## Findings to keep in mind

- **Segment access rights** are kept in `Segs.ar[]` now (VMX format, `include/regs.h`): a
  protected mode load takes them from the descriptor (accessed bit set), a virtual 8086 mode
  load sets F3h, a real mode load leaves them, a null selector is `SEG_AR_UNUSABLE`, and
  SYSENTER/SYSEXIT set their fixed ones. `CPU_GetFullState()` uses them in protected mode instead
  of reading the descriptor tables, so a descriptor changed after the load, or the real mode
  segments still loaded right after setting CR0.PE, come out as the CPU has them. (Real and
  virtual 8086 mode keep the fixed access rights VM entry needs.) The emulated cores do not use
  them for access checks. The KVM core still keeps the vCPU's own cache for segments DOSBox-X
  did not load (step 14), which is no longer needed for this.
- **V86 segments must be normalised for KVM.** VM entry requires base = selector << 4, limit
  = FFFFh and access rights F3h for every segment in virtual 8086 mode. DOSBox-X now sets all
  of that at every virtual 8086 mode segment load (`CPU_SetSegV86Hidden()`), including CS
  (IRET, task switch, far JMP/CALL/RET) and the reloads when DOSBox-X's EMM386 enters virtual
  8086 mode again for Windows (`WinVM86Ctl()`, which loaded CS/DS/ES/SS before setting VM and
  left FS/GS alone); with `segment limits=false` the limit is ~0 like everywhere else. The KVM
  core still normalises on the way in, which covers `segment limits=false`.
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
- **MSRs**: KVM answers RDMSR and WRMSR itself, so `enable msr=false` does not take them away
  under KVM (TRIP needs `enable msr=true` on the emulated cores and runs on KVM without it).
- **KVM's instruction emulator** (used for MMIO) cannot do x87 instructions (Glide's FSTP to
  the Voodoo registers): the KVM core runs such an instruction on the normal core (step 24).
  It does `REP OUTSW`/`OUTSD` one element per exit (it batches `REP INSW`), which is why the
  IDE data port writes are queued.
- **Windows 98 image** (`~/emu/win98`): `BootGUI=0`, it boots to the DOS prompt. A test copy
  needs `WIN` in `AUTOEXEC.BAT` (step 24).
- **Changing the CPU type at run time** calls `KVM_CoreInit()` again, which sets the CPUID
  again. Newer kernels refuse `KVM_SET_CPUID2` with different values after the vCPU has run,
  so once the core runs, a CPU type change needs a new vCPU (keeping the state through
  `CPU_FullState`).
- **Prefetch CPU types** (`*_prefetch`) stop DOSBox-X with an error unless the core is normal
  or auto, which includes kvm.
- **Pages above 4GB** (`memory file` + `Mem4GBPageHandler`) are plain RAM in the memory file;
  they need a slot of their own when PSE-36/PAE guests use them.

## Open issues of the KVM core

- **Callback round trips** cost about 10 us (an emulated core: about 2.5 us including the
  callback), most of it KVM's exit and entry. `CALLBACK_Idle()` is a round trip per call in
  DOSBox-X's own wait loops.
- **Port I/O exits** cost 5-7 us each in the kernel, so a guest that polls a port (a PIT or
  status port in a tight loop) runs at a fraction of the emulated cores' speed (IOBENCH).
  Port reads always exit; writes that can be queued are (step 18).
- **Exit-heavy guests at low fixed cycle counts** can still run slower than on an emulated
  core: exit costs vary, and the estimate (`KVM_Overhead()`) errs towards charging the guest,
  so that it never gets free time. (Mode 13h no longer exits, step 21.)
- **Planar and unchained VGA modes** (16 colours, mode X/Y) exit for every video memory
  access under KVM; the guest runs on an emulated core while that dominates (step 26). The
  thresholds (40% to switch, 25% to stay, 20 ms and 50 ms) are first guesses, tuned on FastDoom
  only. A guest whose VGA use comes in short bursts may switch often, and each switch back
  resets the dynamic core's code cache.
- **Short slices** (exit-bound guests at `cycles=max`) still spend part of their time
  re-arming the slice timer.
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
- **Windows 95 with 16 MB** (`~/emu/win95/dosbox.conf`, `memsize=16`): under KVM the desktop
  came up but the Win32 test program started from `WIN.INI` did not run in 400 s; the dynamic
  core ran it, and so did KVM with 32 MB, and the build from before steps 24-26 behaved the
  same. Probably Windows waiting on something rather than the KVM core: a prompt about the
  daylight saving time change, the NE2000 card not working (the test configuration leaves it
  out; AUTOTYPE's Enter dismissed one such dialog), or reinstalling the IDE driver and asking
  to restart. Not investigated yet.

## Next steps

27. **OS/2** once the floppy controller emulation is complete.
28. **The other open issues above.**
