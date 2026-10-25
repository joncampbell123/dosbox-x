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
  changes a descriptor after loading the segment gets slightly different type/DPL/AVL bits.
  The fully correct fix is to store the access rights in `Segs` at every load (about 25
  places in `cpu.cpp` set `Segs.expanddown[]`).
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
  `new[]` + 16-byte alignment, so it cannot be mapped into the guest as it is. Allocate it
  page-aligned (and its size a multiple of 4KB) before trying to map VRAM directly.
- **Mode 13h and planar VGA go through device handlers** (`VGA_ChainedVGA_Handler`,
  `VGA_UnchainedVGA_Handler`), so every write would be an MMIO exit. Only the SVGA bank map
  (`VGA_Map_Handler`), LFB, Tandy/PCjr and Hercules handlers map host memory directly.
  Performance work: coalesced MMIO, or direct mapping plus dirty logging where the handler
  has no side effects.
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

- **Callback round trips are expensive** (about 25 us). Use `KVM_CAP_SYNC_REGS` (registers and
  segment registers through `kvm_run`, no ioctls), and keep the FPU, debug registers and MSRs in
  the vCPU until something really needs them (save states, the debugger, switching cores). Also
  look at `CALLBACK_Idle()`: DOSBox-X's own wait loops call it, and each call is a round trip.
- **The first slot build** classifies every page up to 4 GB (165 ms), and an A20 change
  reclassifies all of them (4-6 ms). Limit it to where something is mapped, or reclassify only
  pages whose handler could change.
- **Paging changes inside the guest are not noticed** until the state is fetched: the first MB
  layout (`firstmb`, EMS) follows the paging state DOSBox-X last saw. With
  `KVM_CAP_SYNC_REGS` the core can check CR0 after every exit.
- **VGA memory is not page-aligned**, so all video memory accesses are MMIO exits. Allocate it
  page-aligned, then map direct-mapped modes (SVGA banks, LFB) as RAM slots.
- **Interrupt delivery by DOSBox-X overwrites the vCPU state**: if KVM had an event pending of
  its own (an exception being delivered when an exit happened), it is lost or delivered in
  the wrong order. Consider `KVM_GET_VCPU_EVENTS` before overwriting, or `KVM_INTERRUPT`.
- **NMI** is not passed on.

## After that

14. **Protected mode, V86 and DOS extenders.** DOS/4GW runs, with EMM386 on and off. Test more
    extenders and protected mode systems (DPMI hosts, Windows 3.x, Windows 9x), and exceptions
    that DOSBox-X code raises during a callback (`GuestPageFaultException`).
15. **Exit performance.** VGA planar/mode 13h writes, Sound Blaster and DMA port traffic, and
    PIT polling will cause most exits. Coalesced MMIO for VGA, direct VRAM mapping with dirty
    logging in linear modes (needs page-aligned VRAM), lazy register sync.
16. **Timing accuracy**, especially sound.
17. **Machine coverage**: PC-98 (different memory map; works through the handler table),
    VESA LFB, Voodoo LFB, Glide.
