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

## First milestone: boot to the DOS prompt in real mode

8. **Memory slots** built from `MEM_ClassifyPage()` over the guest physical range, rebuilt
   from `MEM_AddMapChangeHandler` notifications:
   - runs of `MEM_PAGE_RAM` pages with contiguous host pointers become RAM slots;
   - `MEM_PAGE_READONLY` becomes `KVM_MEM_READONLY` slots (writes exit as MMIO, go to the
     handler);
   - `MEM_PAGE_DEVICE` gets no slot (MMIO exit to the handler's `readX`/`writeX`);
   - apply `firstmb` for the first MB+64KB while paging is off (see Findings).

   KVM has a limited number of slots (query `KVM_CAP_NR_MEMSLOTS`; older kernels allow 509),
   so coalesce, and batch rebuilds (apply pending changes before the next `KVM_RUN`, not on
   each notification).
9. **`CPU_Core_KVM_Run`** as the `cpudecoder`. It loads state, then for each `KVM_RUN`:
   `PIC_HostTimeMark()`, run bounded by a per-thread timer (`timer_create` with
   `SIGEV_THREAD_ID`, or `immediate_exit`) set to `PIC_HostTimeBudget()`, and on return
   `PIC_HostTimeAccount()` first, so device handlers see the current time (PIT, RTC). Mark
   again just before the next `KVM_RUN`, so exit handling is not charged to the guest. Return
   when `CPU_Cycles <= 0` or for a callback. It handles these exits:
   - `KVM_EXIT_IO`: `IO_Read*`/`IO_Write*`, including string I/O (`count`, `data_offset`).
   - `KVM_EXIT_MMIO`: the page handler of the physical address.
   - `KVM_EXIT_HLT`: idle until the next PIC event, like the emulated HLT: give up the rest of
     the slice (`CPU_Cycles = 0`); `increaseticks()` then sleeps until wall time catches up.
   - `KVM_EXIT_SHUTDOWN`: triple fault, which resets.
   - `KVM_EXIT_DEBUG` with #BP: if `FE 38` follows the INT3 at the guest RIP (translate
     through the guest page tables when paging is on), advance RIP by 5, sync the state out,
     return the callback number (so `Normal_Loop` runs the handler as usual), and sync back
     on the next entry. Otherwise re-inject #BP with `KVM_SET_VCPU_EVENTS`.
10. **Interrupts.** When the PIC has an IRQ pending: inject it with `KVM_INTERRUPT` if
    `ready_for_interrupt_injection` is set; otherwise set `request_interrupt_window` and
    inject on the next exit. NMI goes through `KVM_NMI`.
11. **State sync.** At first, sync the full state (`CPU_GetFullState`/`CPU_SetFullState`, plus
    the conversion to `kvm_regs`/`kvm_sregs`/`kvm_fpu`) on every exit that runs DOSBox-X code.
    Callbacks, `CALLBACK_RunReal*` nesting, `CPU_SetSegGeneral`, and DOS/BIOS code that reads
    guest page tables through DOSBox-X's own paging then work unchanged. Make it lazy later.
    Normalise V86 segments on the way in (see Findings).
12. **Debugger.** On debugger entry, sync the state and switch to the normal core for
    stepping; switch back on RUN. This works because every core runs the INT3 callback form.
    Heavy-debug tracing could use `KVM_GUESTDBG_SINGLESTEP` later.
13. **Save states.** Sync out before saving and back in after loading. The callback encoding
    check already exists.

Check this milestone against the normal core with the existing headless tests: INT3 handling,
mouse, `dir`, `mem`, memory dumps.

## After that

14. **Protected mode, V86 and DOS extenders** (DOS/4GW with EMM386 on and off). The guest gets
    real paging and exceptions. Exceptions that DOSBox-X code raises during a callback (for
    example `GuestPageFaultException`) must be injected into KVM instead.
15. **Exit performance.** VGA planar/mode 13h writes, Sound Blaster and DMA port traffic, and
    PIT polling will cause most exits. Coalesced MMIO for VGA, direct VRAM mapping with dirty
    logging in linear modes (needs page-aligned VRAM), lazy register sync.
16. **Timing accuracy**, especially sound.
17. **Machine coverage**: PC-98 (different memory map; works through the handler table),
    VESA LFB, Voodoo LFB, Glide.
