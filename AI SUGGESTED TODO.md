# AI SUGGESTED TODO

This document describes the changes on the `main-ai` branch that `master` does not have: what
each one does, why it was made, and how to implement it. DOSBox-X accepts AI assistance but not
AI-generated code, so the code on `main-ai` is not meant to be merged. Instead, developers can
re-implement what they want from these descriptions, by hand, in master's own style. The
descriptions deliberately contain no code to copy: they name files, functions, settings,
constants and protocol details, and explain the steps, the order and the pitfalls.

The descriptions were written with AI assistance from the `main-ai` commits, their commit
messages, and `KVM-TODO.md` (the plan and status document of the KVM core on `main-ai`).
Statements that could not be confirmed from the commits are marked as such in the text.
The "Testing" parts say how a change was verified on `main-ai` where the commits record it,
and otherwise suggest how to test it. The test programs mentioned (CBBENCH, PLANTEST, HYBTEST,
EMSTEST and so on) were small ad-hoc programs and are not in the repository.

## What this covers

- `main-ai` at commit `a0d427709`, compared with `origin/master` at `305dbb9c3`.
- `origin/master` is merged into `main-ai` from time to time, so `main-ai` has all of master's
  changes. What `main-ai` adds is exactly the non-merge commits listed by
  `git log --no-merges origin/master..main-ai` (108 commits at the time of writing). None of
  them is in master in any form (`git cherry` finds no equivalents).
- To bring this document up to date after more work on `main-ai`, list the commits after
  `a0d427709` with `git log --no-merges a0d427709..main-ai` and describe those.

## Provenance of the commits

Most commits on `main-ai` were written with Claude and carry a `Co-Authored-By: Claude` line.
The following carry none:

| Commit | Author | Subject | Section |
|---|---|---|---|
| 6acb82287 | Matthew Hiltner | Tested helpers for Windows 3.1 framebuffer snapshots | Agent and debugger, 9 |
| bd8cc4967 | Matthew Hiltner | Publish Windows 3.1 framebuffer snapshots at VGA frame boundaries | Agent and debugger, 9 |
| a2b3f71a6 | Marcos Hernandez | Debugger SAVESTATE and LOADSTATE commands | Agent and debugger, 7 |
| dc7554049 | Marcos Hernandez | Keep host tick handlers out of save states | Agent and debugger, 6 |
| 423b9c556 | Wleeaf | drive_overlay: SetFileAttr crash and FindNext size | Callbacks and fixes, 2 |
| 098f88723 | Wleeaf | shell: output redirection reading the uninitialized pipe file name | Callbacks and fixes, 3 |

By their metadata these are not AI-generated, so they may not need re-implementing; that is for
the maintainers to judge. They are described here anyway so that the document is complete.

The remaining commits without an AI line are the maintainer's own and are not features to port:
`667b6d62e` marks `main-ai` builds as the "AI vibe coded edition" of DOSBox-X (only for that
branch), and `2747c1638`, `cfaed7050`, `4e0f53b8f`, `c6572d101`, `fe71c3154`, `5b42181ee` are
CHANGELOG and reference configuration updates. The reference configuration files should be
regenerated from the code once the settings described below exist in master, rather than copied.

## Sections and a suggested order

The sections are ordered roughly as they could be implemented. Each section orders its own
subsections with prerequisites first.

1. **Callback instructions and independent fixes.** Subsections 1-8 are small, stand-alone bug
   fixes (save state CRC, overlay drive, shell redirection, VGA M_ERROR setup, RTC first second,
   debugger toggle, debugger stepping over callbacks, quit hang). They are the easiest to port
   first. Subsections 9-14 describe the optional INT3 prefix on callback instructions, which only
   the KVM core needs.
2. **CPU emulation and memory infrastructure.** Changes that are useful on their own:
   - 286 TSS task switching, instruction fixes, segment limit checks, DR0-DR3 breakpoints;
   - segment access rights and virtual 8086 mode segment limits;
   - EMM386 running virtual 8086 mode with paging;
   - the memory map notification and `CPU_FullState` that the KVM core builds on.
3. **RPC debugger agent and debugger tooling.** The agent on Linux (build, Unix socket, headless
   debugger, session handling), debugger `SAVESTATE`/`LOADSTATE`, `-nopromptquit`, and console
   monitor fixes.
4. **Wayland video driver for the in-tree SDL 1.x.**
5. **3dfx Voodoo rendering: OpenGL render-to-texture, threaded software rendering, Direct3D.**
6. **Vulkan output and Vulkan 3dfx rendering.**
7. **Metal output and Metal 3dfx rendering.**
8. **KVM CPU core.**

Dependencies between sections:
- **3dfx work:**
  - Start with the OpenGL render-to-texture work (3dfx section, 1-5).
  - Then the back-end framework: the `voodoo_hw.h` dispatch, `voodoo_card=auto` chosen per
    activation, and the render resolution helpers. It arrived on `main-ai` with the Vulkan 3dfx
    commit (Vulkan section, 5) and is also summarised in the 3dfx section, 7.
  - Only then the Vulkan, Direct3D 9/11 and Metal back ends.
- **Vulkan in SDL 1.x:** Vulkan windows on the SDL 1.x Wayland driver need both the Wayland
  driver and the Vulkan support in the in-tree SDL 1.x.
- **KVM core:** it needs the INT3 callback prefix (callbacks section, 9-13), the memory map
  notification, page classification and `CPU_FullState` (CPU section, 1 and 4), and preferably
  the segment access rights, V86 segment limits and EMM386 paging (CPU section, 2, 3 and 10). It
  also needs the VGA, RTC and quit fixes (callbacks section, 4, 5 and 8), which it exposed.

## Problems found in `main-ai` while writing this

Reading the commits for this document turned up some problems in `main-ai` itself. A hand
implementation should avoid them. Two have since been fixed on `main-ai`:
- **APM suspend loop (INT3 prefix), fixed in `fe7fd992a`:** the APM suspended-mode loop in
  `ISAPNP_Cfg_Reset()` (`src/ints/bios.cpp`) wrote HLT, `JMP $-5` and RET at fixed offsets
  after a `CB_RETF` callback. With the INT3 prefix the callback instruction is one byte longer,
  so the HLT overwrote the callback number and a suspended guest never woke up (callbacks
  section, 14).
- **DR0-DR3 after a state load, fixed in `a0d427709`:** these breakpoints stayed inactive after
  a save state load or `CPU_SetFullState()` until the guest wrote a debug register again,
  because neither called `CPU_DebugRegsChanged()` (CPU section, 9).

Still open on `main-ai`:
- **Vulkan:**
  - The Vulkan 3dfx back end does not apply the clip registers to triangles.
  - `src/hardware/Makefile.am` does not add `VULKAN_CFLAGS`.
  - `vs/config.h` does not define `C_VULKAN`, so Visual Studio builds get only stubs.
  - The SDL 1.x windib Vulkan surface code was never compiled (Vulkan section).
- **Unchecked on `main-ai`:**
  - The dynamic core decides some segment limit checks when it translates a block.
  - x87/SSE memory operands are not limit checked (CPU section, 8).
- **EMM386 paging:** the page tables map RAM only, not device memory above it (CPU section, 10).

## Callback instructions with an optional INT3 prefix, debugger support for callbacks, and independent fixes

DOSBox-X runs its emulated BIOS, DOS and device services through "callback instructions": the bytes FE 38 followed by a 16-bit callback number (GRP 4 /7, which is not a valid instruction on real hardware). Every emulated CPU core decodes FE 38 imm16, returns the callback number to the emulation loop, and the loop calls `CallBack_Handlers[n]`. This group changes how those instructions are written so that they can optionally carry an INT3 (CC) in front: CC FE 38 lo hi, five bytes instead of four. All code that writes a callback instruction now goes through one set of helpers and lays out the code around it from the length the helper returns.

Why the prefix exists: the experimental KVM CPU core (`core=kvm`, described in `KVM-TODO.md` on main-ai) runs guest code with hardware virtualization. FE 38 raises #UD in the guest, and KVM never hands #UD to userspace, so DOSBox-X could not see the callback. INT3 raises #BP, which KVM does report to userspace when guest debugging is enabled with `KVM_GUESTDBG_USE_SW_BP`, with the linear address of the INT3 in `debug.arch.pc`. The KVM core then checks whether the INT3 is followed by FE 38. If it is, the core returns the callback number with EIP past the five bytes. If not, it is a debugger breakpoint or the guest's own INT3. The prefix is only needed for a KVM-style core, but the refactoring that makes the callback length variable is worth doing on its own. It removes about two dozen hand-copied "write FE, write 38, write word, advance 4" sequences and a number of hard-coded sizes, jump displacements and IP adjustments that silently assume 4 bytes.

The group also contains debugger fixes found while testing callbacks: stepping commands that dropped the callback they stepped over, and a menu toggle that had stopped working. It also contains several unrelated bug fixes found during the KVM work. Those affect every core and are the best candidates to port first.

Implementation order below: the independent fixes (sections 1 to 8), then the callback encoding (9 to 13), then one known gap in the main-ai implementation (14). Sections 6 and 7 are debugger fixes that work with the plain FE 38 encoding too and need nothing from the INT3 work.

Authorship note: 423b9c556 (overlay drive) and 098f88723 (shell redirection) are by Wleeaf and carry no AI co-author trailer; they reference GitHub issues #6676, #6677 and #6723 and come with unit tests. Check their provenance before deciding whether they need re-implementation. All other commits in this group are AI-written.

CHANGELOG wording on main-ai (1f18cdbc0, d321e1e92) is summarised at the end of each section where it exists.

### 1. Save states: the Vga component was written with a bad CRC

(24deffc39)

**What.** `zip_ostreambuf::xsputn()` in `include/zipcppstdbuf.h` returns 0 at once, without calling `zipWriteInFileInZip()`, when the write count is zero or negative.

**Why.** Loading any save state failed: the `Vga` file inside the save state zip had a CRC that did not match its data, so minizip refused it. The cause is an empty write with a NULL data pointer, from a `WRITE_POD_SIZE()` of a buffer that has not been allocated. The commit does not name the field. The VGA save code has several `WRITE_POD_SIZE` calls (`vga.mem.linear` in `vga_memory.cpp`, `vga.changes.map` in `vga.cpp`, `TempLine`/`TempLineSize` in `vga_draw.cpp`), and an empty one among them is the likely source. `zipWriteInFileInZip()` updates the running CRC with zlib's `crc32(crc, buf, len)`, and zlib defines `crc32(x, NULL, 0)` as "return the initial CRC value". The NULL buffer therefore reset the running CRC of the file being written to 0, and everything written before it was lost from the CRC.

**How.** In `xsputn()`, after the existing `assert(zf != NULL)`, return 0 when `count <= 0`. Keep a comment explaining the zlib NULL-buffer behaviour, because nothing about the fix makes the reason obvious. Fixing it in the stream buffer, rather than at the `WRITE_POD_SIZE` call, protects every component, not just Vga. It does not affect the save state format: data written by a working build is the same.

**Testing.** Save a state (any machine and program, for example at the DOS prompt) and load it again. Before the fix the load reports a CRC error on `Vga`; after it, the load succeeds. `unzip -t` on the save file also shows the bad CRC before and a clean result after.

CHANGELOG: "Fixed save states: the Vga part of every save state was written with a bad CRC, so loading any save state failed. An empty write with a NULL pointer reset the CRC of the file being written in the zip."

### 2. Overlay drive: SetFileAttr crash, and FindNext reporting the ATR file's size

(423b9c556, by Wleeaf; fixes #6676 and #6677)

**What.** Two fixes in `src/dos/drive_overlay.cpp`.

**Why.**
- `Overlay_Drive::SetFileAttr()` copies a file that exists only in the base directory into the overlay directory before it changes the attributes. It did not create the file's directory in the overlay first, and it did not check that the copy could be opened. For a file in a subdirectory that the overlay did not have yet, `fopen` returned NULL and the copy loop wrote through a NULL `FILE*`, crashing DOSBox-X (for example `ATTRIB -A SUB\F.TXT` on an overlay mount).
- `Overlay_Drive::FindNext()` calls `ht_stat()` on the special `.DBOVERLAY_ATR_` file that holds the attributes, and it used the same `stat_block` from which it later takes the entry's size and date. After any attribute change, DIR showed the file with the ATR file's size (a few bytes) and date.

**How.**
1. In `SetFileAttr()`, after building `newname` and before expanding it through the dir cache, call `Sync_leading_dirs(name)`, the same call `create_file_in_overlay()` makes. It creates the leading directories in the overlay.
2. In the same function, after both attempts to open the overlay copy for writing (`_wfopen` on Windows, then `fopen_wrap`), if `layfile` is still NULL, close the source handle `hand` and return false.
3. In `FindNext()`, declare a second `ht_stat_t` (`special_stat_block`) and use it for the `ht_stat()` of the ATR file name and for reading its `st_size`. `localDrive::FindNext()` already does it this way.

**Testing.** The commit adds two gtest cases to `tests/dos_files_tests.cpp`. Each creates a temporary base and overlay directory with a 1000-byte `F.TXT` (one at the top level, one in `SUB\`), mounts an `Overlay_Drive`, calls `DOS_SetFileAttr(name, 0)`, then checks that `DOS_FindFirst` reports attribute 0 and size 1000. Run `dosbox-x -tests`. By hand: `MOUNT C base`, `MOUNT -t overlay C overlay`, then `ATTRIB -A C:\SUB\F.TXT` and `DIR C:\SUB`.

### 3. Shell: output redirection read an uninitialized pipe file name

(098f88723, by Wleeaf; fixes #6723)

**What.** In `DOS_Shell::ParseLine()` (`src/shell/shell.cpp`), look at `pipetmp` only when the command line has a pipe.

**Why.** `pipetmp` is a stack buffer that `ParseLine` fills only for a pipe (`toc` true). The output-redirection branch still passed it to `DOS_FindDevice()` and `strncasecmp(pipetmp, "Z:\\", 3)` for a plain `cmd >file`. Valgrind reports an uninitialised read from `DOS_FindDevice`/`DOS_MakeName`, and #6723 reports the MSVC debug runtime stopping there.

**How.** Two conditions in the redirection branch: `device` becomes "`toc` and `DOS_FindDevice(pipetmp)` is not `DOS_DEVICES`", and `pipetmp_is_zdrive` becomes "`toc` and the prefix compare matches". The rest of the branch already tests `toc` before using `pipetmp`.

**Testing.** The commit adds `ParseLine_Output_Redirection_Without_Pipe` to `tests/shell_redirection_tests.cpp`. It mounts a temporary local drive, runs `echo hello>X:\OUT.TXT` through `ParseLine`, and checks that the file contains `hello` followed by CR LF. Also run `echo hi>OUT.TXT` under valgrind and confirm the uninitialised-read report is gone.

### 4. VGA: a setup in M_ERROR mode no longer blocks the next mode set

(3e461902e)

**What.** In `VGA_SetupDrawing()` (`src/hardware/vga_draw.cpp`), the early-return branch for `vga.mode == M_ERROR` clears `vga.draw.resizing` before it returns.

**Why.** `VGA_StartResize()` (`vga.cpp`) sets `vga.draw.resizing` and schedules `VGA_SetupDrawing`, and it does nothing while `resizing` is already set. The normal path of `VGA_SetupDrawing` clears the flag, but the `M_ERROR` branch only removed the vertical timer, panning latch and display start latch events and returned. If the first setup ran while the mode was still `M_ERROR`, before the VGA BIOS had set a mode, then every later mode set called `VGA_StartResize()`, which saw `resizing` still set and did nothing. The result was no display and no vertical retrace events for the rest of the session. It was seen with the KVM core: its POST reached the VGA BIOS initialisation later in emulated time, so the first scheduled setup ran first. Any timing change that makes the first setup run early can trigger it, so the fix applies to every core.

**How.** Add one assignment, `vga.draw.resizing = false`, in the `M_ERROR` branch after the three `PIC_RemoveEvents` calls. The comment should say that otherwise the next mode set can never start drawing.

**Testing.** It is hard to reproduce with the emulated cores because the timing does not line up. To reproduce, force a `VGA_StartResize()` before INT 10h sets the first mode, for example by delaying the VGA BIOS init in a test build, and check that the display comes up and that a program waiting on port 3DAh bit 3 (vertical retrace) does not hang. Regression check: boot normally on `machine=svga_s3`, `ega` and `cga`, switch through modes 3, 13h and a VESA mode, and confirm nothing changes.

CHANGELOG: "Fixed the display never starting when the VGA was first set up before the VGA BIOS set a video mode."

### 5. RTC: update in progress for the whole first second after a reset

(d2978e5d4)

**What.** In `cmos_timerevent()` (`src/hardware/cmos.cpp`), round `cmos.last.ended` down to a whole second with floor division instead of subtracting `fmod(ended, 1000)`.

**Why.** On reset, `cmos.last.ended` is set to `PIC_FullIndex() - 1000` because PIC time restarts at 0, so in the first second it is negative (for example -999.5 ms). `fmod()` keeps the sign of the dividend, so subtracting it rounds a negative value up toward zero instead of down. In the example, -999.5 becomes 0, then +1000 gives 1000. The end of the "last update" was then a whole second in the future. Status register A (port 71h, index 0Ah) reports UIP (bit 7) whenever `PIC_FullIndex() - cmos.last.ended` is below 0.244 ms, which includes any negative value, so UIP stayed set for up to one second. Anything that waits for UIP to clear before reading the clock waited that long. The BIOS POST did so under the KVM core. A guest that polls the RTC in its first emulated second after a reset is affected on any core.

**How.** Replace the `fmod` line with "ended = floor(ended / 1000.0) * 1000.0". The `+= 1000` that follows stays. Comment why `fmod` is wrong for negative values. Nothing else changes: the clamp to `index - 1000` above it, and the reset value, stay as they are.

**Testing.** Write a small test program, or use a debugger script, that reads port 71h index 0Ah in a loop from immediately after a reset (for example from an option ROM, a boot sector, or a program started at once from AUTOEXEC after a `-set` reboot) and counts how long bit 7 stays set. Before the fix it stays set for up to 1 s; after it, only for the normal 244 us window around each update. Also check that the time of day keeps advancing normally afterwards.

CHANGELOG: "Fixed the RTC reporting an update in progress for the whole first second after a reset, which made anything that waits for the update to end wait up to a second."

### 6. Debugger: the "Start DOSBox-X Debugger" menu item and hotkey are a toggle again

(02a102a3e)

**What.** In `DEBUG_Enable_Handler()` (`src/debug/debug.cpp`): when execution is stopped in the debugger, the menu item and hotkey leave the debugger and resume normal execution. While the guest runs (normally, after RUN or F5, or under RUNWATCH), they stop in the debugger as before. The `mapper_debugger` menu check mark now means "stopped in the debugger".

**Why.** On master, while stopped in the debugger, the menu item or hotkey resumed execution and then dropped straight back into the debugger. The `if (tohide) return;` at the end of the "leave the debugger" block had been commented out (when RUN and RUNWATCH were fixed earlier), so control fell through to the code that enters the debugger. The hide condition was also too broad: `tohide && (runnormal || debug_running || debugging)` treated RUNWATCH (`debug_running`) as "stopped" and hid the debugger instead of breaking in. The check mark also stayed on after RUN, F5, RUNWATCH and the INT command, although step-over already cleared it.

**How.**
1. In `DEBUG_Enable_Handler`, add a local "leave" flag, initially false.
2. Change the hide condition to `tohide && (runnormal || (debugging && !debug_running))`, and set "leave" in that branch. The rest of the branch (hide the console on Windows, set `debugrunmode`, update the menu items, return if `runnormal`) is unchanged.
3. The RUNWATCH branch (`if (debug_running)`) stays as it is. Only its comment changes: it now just breaks back into the debugger.
4. At the end of the `if (debugging)` block, where `//if (tohide) return;` was, return if "leave" is set.
5. Wherever the debugger leaves the stopped state, uncheck `mapper_debugger` next to the existing `debugger_rundebug`/`runnormal`/`runwatch` updates. That means the RUN command, RUNWATCH, the INT command (which raises a hardware interrupt and resumes) and the F5 key handler in `DEBUG_CheckKeys`. On main-ai it is also done in `DEBUG_AgentResumeAfterTerminate`, which section 7 later replaces with a call to RUN.

**Testing.** Start the debugger from the menu, then choose the menu item again: the guest must resume and stay running, and the check mark must clear. Choose it again: the debugger must stop. After RUNWATCH, the hotkey must break into the debugger. After RUN or F5, the check mark must be off.

CHANGELOG: "Debugger: the 'Start DOSBox-X Debugger' menu item and the debugger hotkey are a toggle again: stopped in the debugger, they leave it and resume."

### 7. Debugger: RUN, RUNWATCH, STEPIN and the agent's step run the callback of the instruction they step

(b641f7a9a, 9b85fceb0)

**What.** Several debugger commands execute one instruction with `DEBUG_Run(1, ...)` and then let the guest run or stop again. They now handle a callback return from that single step the same way the normal loop does. A non-zero return from the callback handler (such as `CBRET_STOP`) is passed back to the emulation loop that is running, so a nested `CALLBACK_RunReal*()` call ends correctly.

**Why.** `DEBUG_Run()` returns what the CPU core returned. For a callback instruction, that is the callback number, and the caller must run `CallBack_Handlers[n]`. The F5, F10 and F11 key handlers already do this, at the end of `DEBUG_CheckKeys`. The RUN and RUNWATCH commands in `ParseCommand`, the STEPIN command, and `DEBUG_AgentStep()` threw the return value away, so stepping a callback instruction behaved like a NOP. Concrete failures:
- RUN at a breakpoint on the DOS INT 21h callback lost that DOS call.
- Stepping the `call_stop` callback at the end of the INT xx stub used by `CALLBACK_RunRealInt()` did not end that nested loop. Execution ran on into the next INT stub in the callback area.
- `DEBUG_AgentResumeAfterTerminate()` kept a copy of RUN that deliberately did not step, as a workaround for exactly this (it had to land on the stop callback after the target was terminated).

This affects plain FE 38 callbacks as well, and it does not depend on the INT3 work. Under KVM it matters more, because a breakpoint on a callback stops on the callback instruction itself (section 13).

**How.**
1. Add a file-static "pending loop return" value in `debug.cpp`, and a public `DEBUG_TakeLoopReturn()` (declared in `include/debug.h`) that returns the value and clears it.
2. Add a static `DEBUG_RunSteppedCallback(ret)`. If `ret` is between 1 and `CB_MAX - 1`, it runs the handler as `Normal_Loop` in `dosbox.cpp` does: save `last_callback` and set it to `ret`, save `dosbox_allow_nonrecursive_page_fault` and set it to false, call `CallBack_Handlers[ret]`, then restore both. If the handler returns non-zero, store it as the pending loop return, set `exitLoop`, and zero `CPU_Cycles` and `CPU_CycleLeft`. Note that it sets `last_callback` and the page-fault flag the way `Normal_Loop` does. The existing F5 path in `DEBUG_CheckKeys` does not; keep the new helper consistent with `Normal_Loop`.
3. RUN: keep the result of `DEBUG_Run(1,false)` and call the helper with it as the last step, after `DOSBOX_SetNormalLoop()` and the title update. Calling it last means the callback runs with the debugger already in the "running" state.
4. RUNWATCH: likewise, after the menu updates and `DEBUG_DrawScreen()`.
5. STEPIN: call the helper with the result of `DEBUG_Run(1,true)` right after it, before the `STEPIN CS:IP=` message, so that the message shows the state after the callback.
6. `DEBUG_AgentStep()`: call the helper after both the step-over path and the trace path.
7. `DEBUG_AgentResumeAfterTerminate()`: replace its hand-written copy of RUN (and the comment explaining why it did not step) with a call to `ParseCommand("RUN")`. RUN now steps the stop callback and runs it, which ends the enclosing `CALLBACK_RunRealInt`.
8. `Normal_Loop` (`dosbox.cpp`): under `C_DEBUG`, at the top of the `while(1)` loop (after the agent bridge pump), take the pending return. If it is non-zero, return it.
9. `DEBUG_Loop`: near the top (after `ControlServer_Poll()`, before the "loop changed" check), take the pending return and return it if non-zero. At the end, where it returns `DEBUG_CheckKeys()`, return the key result if it is non-zero and otherwise the pending return. STEPIN arrives through `DEBUG_CheckKeys` → `ParseCommand`.

Pitfall: the reason for passing the value through the loop, instead of returning it from `ParseCommand`, is that `ParseCommand` returns a bool and is called from several places (keys, agent, MCP). The value must reach whichever loop `DOSBOX_RunMachine()` is running in the nested `CALLBACK_RunReal*()` call.

**Testing.**
- Set a breakpoint on the INT 21h callback (`BPINT 21` or a breakpoint at the INT 21h vector), run `DIR`, and type RUN at the stop. The listing must appear. Before the fix the DOS call was lost.
- Step with STEPIN onto the `call_stop` instruction of an INT stub (for example inside a `CALLBACK_RunRealInt(0x10)` started by a DOS-side service) and confirm execution returns to the caller instead of running into the next stub.
- Do the same with RUNWATCH.
- With the agent: run a target, terminate it, resume, and check that the DEBUGBOX command returns.

CHANGELOG: "Debugger: RUN, RUNWATCH and STEPIN, and the RPC agent's step, now run the callback of the instruction they step past. They skipped it before, so for example RUN at a breakpoint on a DOS call lost the call."

### 8. Don't hang at quit when a file is closed with the guest in protected mode

(80f9d0157)

**What.** `LocalFile::UpdateLocalDateTime()` (`src/dos/drive_local.cpp`) asks DOS for the date and time through `CALLBACK_RunRealInt(0x21)` only when the CPU is in real mode or V86 mode. Otherwise it keeps the host's local time, which it has already computed with `localtime()`. On main-ai the `CALLBACK_RunReal*()` functions also call `CPU_SyncState()` first. That part only exists with the KVM core.

**Why.** `UpdateLocalDateTime()` sets a closed file's timestamp from the DOS clock (INT 21h AH=2Ah, then AH=2Ch). DOSBox-X's own INT 21h answers that only in real or V86 mode: under a DOS extender in protected mode the guest's own IDT handles the interrupt. DOSBox-X also closes files when it shuts down, for example a redirected `OUT.TXT`, and the guest can be in any mode at that point. With DOS/4GW in protected mode, the INT 21h went to the extender's handler and never came back, so SIGTERM or `-time-limit` left DOSBox-X running. It was seen with `core=kvm`. The code path is core-independent, but the commit does not say whether the emulated cores hang the same way, and I have not checked.

**How.**
1. Include `cpu.h` in `drive_local.cpp`.
2. In `UpdateLocalDateTime()`, keep the `localtime()` result as the default. Wrap the save of AX/CX/DX, the two `CALLBACK_RunRealInt(0x21)` calls, the copy into `tm`, and the restore in a test for "`!cpu.pmode` or `reg_flags & FLAG_VM`". Comment why: a DOS extender reflects guest INT 21h to real/V86 mode, but a DOSBox-X-initiated INT 21h in protected mode goes to the guest's own handler.
3. Only if a KVM-style core is ported: `CPU_SyncState()` at the top of `UpdateLocalDateTime()` and of `CALLBACK_RunRealFarInt`, `CALLBACK_RunRealFar`, `CALLBACK_RunRealInt_retcsip` and `CALLBACK_RunRealInt`. These can be called from outside a CPU core while the register state still lives in the vCPU. Master has no `CPU_SyncState()`, so skip this part otherwise.

**Testing.** Run a DOS/4GW program with output redirected to a file on a mounted local drive, for example `PROG > OUT.TXT`, and send SIGTERM (or use `-time-limit`) while it runs in protected mode. DOSBox-X must exit, and `OUT.TXT` must get a sensible timestamp (host time). Normal case: write a file from a real mode program and check that its timestamp still follows the DOS clock (set the date with `DATE` to something odd).

### 9. Write every callback instruction through `CALLBACK_WriteInstr` helpers (no behaviour change)

(57dc691af)

**What.** A global `CALLBACK_Int3Prefix` (false for now), three writer functions, and two inline length helpers in `include/callback.h`. Every place that wrote FE 38 imm16 by hand uses them, and lays out what follows by the returned length instead of assuming 4. Guest memory is byte-identical as long as the prefix is off. Save states record the encoding.

**Why.** Section 11 needs the encoding to be selectable, and the old code assumed a 4-byte callback instruction in many places: the size returned by every `CALLBACK_SetupExtra` case, the 6-byte INT xx stub stride, forward and backward jump displacements, ROM space reservations at fixed addresses, and code that steps IP back over the callback. Doing this as a separate step with the prefix forced off makes it easy to check: nothing in guest memory may change.

**How.**

*Header (`include/callback.h`).*
- `extern bool CALLBACK_Int3Prefix`, defined in `callback.cpp`, initially false.
- `CALLBACK_InstrLength()`: 5 if the prefix is on, else 4.
- `CALLBACK_RealIntStubSize()`: 2 + `CALLBACK_InstrLength()`. This is the size of one entry in the block of 256 "INT nn; callback call_stop" stubs after the callback slots.
- `CALLBACK_WriteInstrToBuffer(buf, cb)`: writes the optional CC, then FE, 38, the callback number low byte, the high byte; returns the length. `CALLBACK_WriteInstr(PhysPt, cb)` writes the same bytes with `phys_writeb`, and `CALLBACK_WriteInstrLinear(LinearPt, cb)` with `mem_writeb`. Both return the length.
- A comment saying that the encoding is chosen when the callbacks are first written and cannot change while they exist, and that all writers must use these functions.

*`src/cpu/callback.cpp`.*
- `CALLBACK_SetupExtra()`: compute `cbl` = `CALLBACK_InstrLength()` if `use_cb`, else 0. In every case, replace the hand-written sequence with "advance `physAddress` by `CALLBACK_WriteInstr(...)`". Every return value becomes the size without the callback plus `cbl`; for example CB_RETN 1+cbl, CB_RETF8 3+cbl, CB_IRQ0 0x0f+cbl, CB_INT21 11+cbl. Cases that need more than a mechanical change:
  - CB_IRQ1 (non-PCjr): the `jc skip` that jumps over the callback was the fixed word 73 04. Its displacement byte must be `cbl`. The callback follows it at +0x0a, and `physAddress` advances by 2 + length. The return value is 0x1a, plus 2 + `cbl` with a callback.
  - CB_INT16: the backward `jmp` at the end (EB ED) goes back to the callback instruction right after the STI. Its displacement is 0x100 - (0x0f + `CALLBACK_InstrLength()`). With 4 bytes that is ED; with 5 it is EC.
  - CB_IRQ12: always has a callback (it `E_Exit`s otherwise). It returned a fixed 0x13; now 0x0f + `cbl`.
  - CB_MOUSE: unchanged in this step (still jumps over 7 bytes); see section 10.
  - CB_INT13: the trailing `INT 13h; callback call_idle` used by image disk support is written with `CALLBACK_WriteInstr` at +7 after the callback-adjusted `physAddress`. The return value (5 + `cbl`) does not include that tail, which matches the old behaviour (the old code returned 9 but wrote 13 bytes).
  - CB_VESA_PM: the callback is at the start and the trailing RETN only exists with a callback; return 27 + (`cbl` + 1 with a callback).
  - CB_INT6F_ATOK: the callback is at the fixed offset +8 (target of the EB 06 at the start). The IRET goes at +8 + length, and the return value is 9 + length.
  - CB_INT28 still returns 4, as before. That was already too small with a callback, but the callback is written in its own 32-byte slot, so nothing depends on it.
- `CALLBACK_RunRealInt()` and `CALLBACK_RunRealInt_retcsip()`: the stub offset is `CB_SOFFSET + CB_MAX*CB_SIZE + intnum * CALLBACK_RealIntStubSize()` (was `intnum*6`).
- `CALLBACK_Init()`: the callback area allocation and the "spans 64KB segment" check use `256 * CALLBACK_RealIntStubSize()`. The `call_stop` and `call_idle` instructions (idle: 12 NOPs, then the callback at +12) and the 256 INT stubs are written with `CALLBACK_WriteInstr`, and the stub pointer advances by the stub size.

*Other call sites (each was a hand-written FE 38 sequence or a hard-coded length).*
- `src/dos/dos_mscdex.cpp`, `CMscdex::AddDrive`: the device driver after the header is "callback strategy; RETF; callback interrupt; RETF". Allocate the header plus 2 × (length + 1); the old comment said "3 callbacks" but the code wrote two. Set the strategy entry to the offset before the first callback and the interrupt entry to the offset after the first RETF. The branch that re-registers the header when there are no drives sets the interrupt entry to offset + length + 1 (was +5). It uses the linear writer because it writes by segment:offset.
- `src/hardware/ipx.cpp`, ESR stub: the push sequence takes 8 bytes, then the callback; the pop sequence and IRET follow at the returned end, not at fixed +12...+19.
- `src/ints/bios.cpp`:
  - The APM protected-mode entry in `ISAPNP_Cfg_Reset` (PUSHF, PUSH BP, PUSH BP, callback, POP BP, POP BP, POPF, RETF): the pops follow the callback's returned end.
  - The video BIOS scan and adapter ROM scan callbacks "step back into the callback instruction" before far-calling the ROM, so that the ROM returns to the callback and it runs again. They subtract `CALLBACK_InstrLength()` from EIP (was 4).
  - `BIOS_DEFAULT_IRQ1_LOCATION` ROM reservation: 0x1c + length (was 0x20; this is the CB_IRQ1 size with a callback).
  - The POST/reset code chain at `BIOS_DEFAULT_RESET_CODE_LOCATION` (POST, video BIOS scan, adapter ROM scan, startup screen, optional user boot hook, boot, boot fail, then `JMP $`) is written with `CALLBACK_WriteInstr`, advancing `wo` by each length. `BIOS_boot_code_offset` and `BIOS_bootfail_code_offset` are taken from `wo`, and the existing 64-byte fence check still applies.
  - The PC-98 N88 BASIC entry at E800:0002 and the two IBM ROM BASIC entries at F600:2DB0 and F600:4C79: reserve length + 2 bytes (callback plus `JMP $`, EB FE), and write the JMP after the callback.
- `src/ints/bios_keyboard.cpp`, PCjr NMI and INT 48h handlers: these are a CB_IRET callback (callback, IRET) followed by hand-written code at fixed offsets +5 onward, ending with a `jmp` back to +0. Every offset becomes length + 1 + n. The backward jump displacement is 256 - (length + 17); with 4 that is the old 256 - 21.
- `src/ints/ems.cpp`, VCPI V86 monitor at `vcpi.private_area + 0x2e00`: the callback, then IRETD (66 CF) at the returned end. Uses the linear writer.
- `src/ints/imagedisk_int13.cpp`, `imageDiskCallINT13`: it jumps to the `INT 13h` inside the CB_INT13 code at callback start + length + 5 (STI, callback, IRET, INT 0Eh, IRET). Was +4+5.
- `src/ints/int10_memory.cpp`, VGA ROM entry at C000:0003: callback, then RETF at 3 + length. The copyright at +0x1e is unaffected.
- `src/ints/int_dosv.cpp`, DOS/V font handlers: the callback in the slot, then RETF 4 (CA 04 00) for the MS-KANJI API or RETF (CB), at the returned end.
- `src/misc/programs.cpp`, the .COM image of the built-in programs (MOUNT.COM and the rest): the old static `exe_block` with the callback number patched in at byte 14 becomes a fixed head (MOV SP,400h; MOV BX,40h; MOV AH,4Ah; INT 21h; XOR AL,AL), the callback for `call_program` written into a buffer with the buffer writer, and a fixed tail (MOV AH,4Ch; INT 21h), followed by the program index byte. Add a small builder function that assembles these into a buffer and returns the length. `PROGRAMS_MakeFile`, `PROGRAMS_IsBuiltinProgram` (which compares guest memory at PSP:0100 against the image) and `PROGRAMS_Handler` (which reads the index byte at 0100h + image length) all use the builder instead of `sizeof(exe_block)` and the old `CB_POS`. The image is 21 or 22 bytes, which fits the existing 32-byte allocation.
- `src/shell/shell_cmds.cpp`, `Int2fhook()` (the INT 2Fh debug hook: far call to the old handler, then the callback, then IRET): advance by the returned length.

Also search for anything else in your tree that writes 0xFE then 0x38, or adds or subtracts 4 or 6 around a callback. The list above is what main-ai changed. One place was missed; see section 14.

*Save state (`src/misc/savestates.cpp`).*
- Include `callback.h`. Add a helper that names the encoding: "FE38" without the prefix, "INT3+FE38" with it.
- `SaveState::save()`: after the `Machine_Type` entry, write a zip entry `Callback_Encoding` containing that name, in the same way as the other text entries.
- `SaveState::load()`: after the `Machine_Type` check, read `Callback_Encoding` if it is present. If it is missing, the state is from before this change and is "FE38". If the name differs from the current encoding, call `notifyError` and fail the load. Do this even when `force_load_state` is set. The message must explain the mismatch; section 11 makes it name the setting to change.
- Why the refusal cannot be overridden: guest memory in the state holds callback instructions of the saved length, while this session computes stub offsets (`CALLBACK_RunRealInt`), step-back amounts and so on from the current length. A plain FE 38 in a session with the prefix on would still run on an emulated core but not on KVM. A CC FE 38 in a session without the prefix becomes a guest INT3. Either way the machine crashes later, far from the cause.

**Testing.** Build with the prefix forced off and compare guest memory with master. Dump F000:0000-FFFF and the callback area segment (it is in the ROM BIOS area; see the "DOSBox callback area" log line) with the debugger's MEMDUMPBIN, or compare a save state's `Memory` component. They must be byte-identical. Also check that a save state from master still loads (no `Callback_Encoding` entry → FE38). Run the usual smoke tests: boot to the prompt, `MOUNT`, `DIR`, a CD-ROM mount (MSCDEX), the mouse in a DOS program, EMM386/VCPI with a DOS extender, IPX if configured, `machine=pcjr` keyboard input, `machine=pc98` booting a disk image, IBM ROM BASIC (`BOOT` with no disk on a machine with BASIC), and an image-booted DOS using INT 13h.

### 10. Emulated CPU cores run INT3-prefixed callback instructions; the mouse backdoor gets room for the longer callback

(901409c9e)

**What.** When `CALLBACK_Int3Prefix` is set, the emulated cores treat an INT3 followed by FE 38 as the start of a callback instruction. They consume CC FE 38 imm16 and return the callback number, exactly as for a plain FE 38 imm16. Any other INT3 is still delivered to the guest. Also, the CB_MOUSE layout reserves room by callback length.

**How.**
1. Add `CALLBACK_Int3PrefixFollows(LinearPt next)` to `callback.h`/`callback.cpp` (include `paging.h`). `next` is the linear address just past the INT3. It returns true if the bytes at `next` and `next+1` are FE and 38. With paging enabled, it first returns false if `next - 1` (the INT3) and `next + 1` (the 38) are not in the same 4 KB page. The point is that the check must never cause a page fault where executing the INT3 would not, since the INT3's own page was just fetched successfully. Reading with `mem_readb` within that page is then safe.
2. Normal core, `src/cpu/core_normal/prefix_none.h`, `CASE_B(0xcc)`: before the existing debug breakpoint check and `CPU_SW_Interrupt_NoIOPLCheck(3, ...)`, if the prefix is on and `CALLBACK_Int3PrefixFollows(SegBase(cs) + GETIP)` (IP is already past the CC), fetch two bytes (FE 38), fetch the word, `FillFlags()`, `SAVEIP`, and return the word. This mirrors the existing FE /7 callback case. `prefix_none.h` is shared by the normal, simple, prefetch and 286/8086 cores, so they all get it.
3. Full core, `src/cpu/core_full/op.h`, `case O_INT`: the same, conditioned on `(inst.entry & 0xFF) == 0xcc` (O_INT also covers CD nn), using `inst.cseip` as the address after the CC, `Fetchb`/`Fetchw`, `FillFlags()`, `SaveIP()`, and returning the number.
4. Dynamic cores (`core_dyn_x86`, `core_dynrec`): no change needed. Their decoders do not translate 0xCC, so they end the block and the instruction runs through the normal core, which handles it as above. Confirm this in your tree, because it is an assumption about the decoders, not something the commit changes.
5. CB_MOUSE in `CALLBACK_SetupExtra`: the INT 33h entry starts with a short jump over room for the "mouse backdoor" entry point, which `mouse.cpp` places at INT 33h vector + 2 as a separate CB_RETF8 callback (`call_mouse_bd`, `MOUSE_BD_Handler`): callback, then RETF 8 (CA 08 00). The jump was the fixed EB 07, skipping 7 bytes. With the prefix, the backdoor is 5 + 3 = 8 bytes, so it overran the start of the INT 33h callback. The room becomes `CALLBACK_InstrLength() + 3`: the jump displacement is that value, `physAddress` advances by 2 + room, then comes the INT 33h callback and IRET. The return value is 2 + room + 1 + `cbl`. With the prefix off this is still EB 07 and 9 bytes of offset, so memory does not change.

Pitfalls:
- Because of the page check, a prefixed callback whose CC, FE and 38 straddle a 4 KB page boundary would be taken as a guest INT3 while paging is on. DOSBox-X's callbacks are mostly in the ROM BIOS area or in 32-byte callback slots, but some are written into DOS memory (MSCDEX driver, IPX ESR page, the `Int2fhook` code). The commits do not guard against a straddle, and I have not checked whether any of those placements can land on one. The KVM core uses the same function and has the same constraint.
- A guest INT3 followed by FE 38 is taken as a callback when the prefix is on. FE /7 is undefined on real hardware, so ordinary code does not contain that byte sequence.
- A consequence not stated in the commits: with the prefix on, a guest-side debugger that sets a breakpoint by writing CC over the first byte of a DOSBox-X callback (for example on a BIOS or DOS entry point) writes CC over CC, and the core still runs the callback instead of trapping to the guest debugger. The setting's help text warns more generally that programs relying heavily on INT3 may not work with the prefix.

**Testing.** Temporarily force the prefix on (section 11 adds the setting) and run with `core=normal`, `simple`, `full`, `dynamic` and a `cputype` with prefetch (for example `386_prefetch`). Boot to the prompt and run the smoke tests from section 9. Specifically test the INT 33h mouse driver (a mouse-driven DOS program) and the mouse backdoor entry. Run a small test program that hooks INT 3 and executes CC, and check that its handler is called. Run the same program under EMM386 (V86 mode, paging on) and a DOS/4GW program, to exercise the paged path of the prefix check.

### 11. `[cpu] callback int3 prefix` setting

(3213ec691)

**What.** A new `[cpu]` setting, `callback int3 prefix`, with values `auto` (default), `true` and `false`, `Changeable::OnlyAtStart`. `auto` turns the prefix on only when `core` is `kvm`. On a build without a KVM core, `auto` therefore means off.

**How.**
1. `src/dosbox.cpp`, `DOSBOX_SetupConfigSections`: add the string property right after `core`, with the three values and help text covering: what it does (callback instructions in guest memory written with an INT3 in front, so a hardware virtualization core can run them); that the emulated cores run either form; that programs using INT3 heavily (anti-debugging) may not work with it; that a save state loads only with the same setting; what each value means; and that it takes effect at startup, not at a machine reset. It is an advanced option (not `SetBasic`).
2. `src/cpu/callback.cpp`: a function that chooses the prefix once per process (guarded by a static flag). It reads the setting from the `cpu` section: `true` and `false` are literal, and `auto` is on if `core` is "kvm". It logs a line when the prefix is on. Call it at the very start of `CALLBACK_Init()`, before the callback area is sized or written, because the area size depends on the stub size.
3. On main-ai the KVM core work later made this function public (`CALLBACK_ChooseInt3Prefix()` in `callback.h`), because the CPU is configured before `CALLBACK_Init()` and core selection must know the prefix (the KVM core falls back to an emulated core when the prefix is off). That is only needed with a KVM core.
4. Why "once per process" and OnlyAtStart: callback instructions written at startup stay in the ROM BIOS and elsewhere for the life of the process, and a machine reset does not rewrite all of them. Changing the encoding at reset would leave a mix of the two lengths, and code that computes offsets from the current length would then be wrong.
5. Save state refusal message (section 9): it now names the setting and the value to use. Set `callback int3 prefix` in `[cpu]` to `true` if the saved encoding was "INT3+FE38", otherwise `false`, then restart DOSBox-X and load again.
6. Regenerate the reference configs: `dosbox-x.reference.full.conf` (help text, and the line `callback int3 prefix = auto` after `core`), `dosbox-x.reference.conf` (the advanced-options list line starts with `callback int3 prefix;`), and `contrib/windows/installer/dosbox-x.reference.setup.conf` (the `#DOSBOX-X-ADV:` help lines, the advanced list, and the commented `#DOSBOX-X-ADV:callback int3 prefix = auto` line). On main-ai these were edited by hand to match the generated text. Use the usual config-generation path instead if there is one.

**Testing.** Start with `callback int3 prefix=true` and check the log line. Check in the debugger that the INT xx stubs are now 7 bytes apart, and that the bytes at the INT 21h vector start with CC FE 38. Run the smoke tests from sections 9 and 10. Save a state with `true`, restart with `false`, and try to load it: it must be refused with the message naming `true`, also with forced loading. Do the reverse as well. Change the setting at runtime with `CONFIG -set` and check that it is refused or ignored (OnlyAtStart).

CHANGELOG: "Added [cpu] 'callback int3 prefix' (auto, true, false) to write the DOSBox-X callback instructions with an INT3 in front of them, as a CPU core using hardware virtualization (KVM) will need. auto writes it if the CPU core is set to kvm. It takes effect when DOSBox-X starts. All the emulated CPU cores run either form, and other INT3 instructions still go to the guest." And: "Save states record the callback instruction encoding, and a save state with the other encoding is refused with an explanation, because its memory holds callback instructions the session cannot run. Save states from before this are the plain encoding."

### 12. Debugger disassembler shows a prefixed callback as one instruction

(2b6089e2f)

**What.** In `DasmI386()` (`src/debug/debug_disasm.cpp`), when the prefix is on and the first byte is CC followed by FE 38, the disassembler treats the CC as part of the instruction and decodes the FE 38 imm16 as the existing "callback %Iw" entry. The instruction length is 5, and the opcode bytes column shows CC FE 38 lo hi. A guest INT3 that is not followed by FE 38 is still shown as `int 03`.

**How.** Include `callback.h`. Right after reading the first opcode byte, if it is 0xCC and `CALLBACK_Int3Prefix` is set, peek at the next two bytes (at `getbyte_mac` and `getbyte_mac + 1`). If they are FE and 38, read the next byte as the opcode, which skips the CC. Apply the same page rule as `CALLBACK_Int3PrefixFollows` (no match if the CC and the 38 are in different pages while paging is on), so the display matches what the core will execute. Use `mem_readb_checked` for the peek, so that disassembling near an unmapped page does not fault. The core's helper uses `mem_readb`, which is not suitable here.

**Testing.** With the prefix on, open the debugger and view the INT 21h handler (`C` to the vector, or step into an INT 21h). It must show `callback xxxx` with five opcode bytes, and the next line must start after it. A guest `int 3` must still disassemble as `int 03`. Step over a prefixed callback with F10/F11 and confirm the code view stays aligned.

### 13. Debugger breakpoints on INT3-prefixed callbacks

(950bb3ac3, 94f8fd467)

**What.** A breakpoint placed on the CC of a prefixed callback stops there before the callback runs. In builds without heavy debugging, it also works when the callback is in ROM.

**Why.** Without heavy debugging, a physical breakpoint is implemented by writing 0xCC at the address and remembering the old byte. On a prefixed callback, `CBreakpoint::Activate()` found 0xCC already there. It reported "Internal error while activating breakpoint" and recorded 0xCC as the old byte. The core then saw CC FE 38 and ran the callback without stopping, so the breakpoint never fired. In ROM, the debugger cannot write 0xCC at all, so breakpoints on ROM callbacks never worked in non-heavy builds. The first fix added a breakpoint check to the cores at the prefix. In heavy debugging builds, however, breakpoints are checked before every instruction and a stop leaves the breakpoint active, so the extra check stopped a second time at the same place. F11 (trace into) at such a breakpoint then never advanced. Hence the second commit.

**How.**
1. `src/debug/debug.cpp`, only when `!C_HEAVY_DEBUG`: add a static helper that says whether the 0xCC at a breakpoint address is a callback prefix, that is, the prefix is on and `CALLBACK_Int3PrefixFollows(address + 1)`.
2. `CBreakpoint::Activate(true)`: if the byte is 0xCC and the helper says it is a prefix, set `oldData` to 0xCC and write nothing. Test this before the existing "another breakpoint is already here" logic, so it does not report an internal error.
3. `CBreakpoint::Activate(false)`: in the "byte is 0xCC" path, report the internal error only if `oldData` is 0xCC and the address is not a callback prefix. Writing `oldData` (0xCC) back when the last breakpoint there goes away is harmless.
4. Normal core (`prefix_none.h`, the prefixed-callback branch from section 10) and full core (`op.h`, same branch): inside the branch, before consuming FE 38 imm16, and only under `C_DEBUG && !C_HEAVY_DEBUG`, call `FillFlags()`. If `DEBUG_Breakpoint()` is true, return `debugCallback`, the same as the existing guest-INT3 breakpoint path. IP has not been saved past the CC at that point, so `DEBUG_Breakpoint()` looks up the breakpoint at the CC's address, and the debugger stops with CS:IP on the callback instruction.
5. Do not add the check in heavy debugging builds; comment why (breakpoints are checked before every instruction there).

Interaction with section 7: once the debugger stops on a prefixed callback, the next RUN, F5, STEPIN or step runs exactly that callback instruction. Without section 7, RUN and STEPIN would skip the callback. Port section 7 first.

**Testing.** In a normal debug build (no heavy debugging) and in a heavy debugging build, with the prefix on: set a breakpoint on the INT 21h callback's CC address. Run `DIR`. The debugger must stop at the callback with no "internal error" message. F11 must execute the callback and move on (in the heavy build, it must not stop again at the same address). RUN must run the DOS call (section 7). Set a breakpoint on a callback in the F000 ROM (for example the INT 16h handler) and check that it fires in the non-heavy build. Delete the breakpoints and confirm the CC prefixes are still in memory.

CHANGELOG: "Debugger: callback instructions with the INT3 prefix are shown as one callback instruction, and breakpoints on them work."

### 14. Known gap: the APM suspended-mode loop assumes a 4-byte callback

(found while writing this section; fixed on main-ai in `fe7fd992a` as suggested here)

**What.** In `ISAPNP_Cfg_Reset()` (`src/ints/bios.cpp`), the APM "suspend/standby loop" callback is set up as CB_RETF at `APM_SuspendedLoopRptr`, and the code then patches fixed offsets: HLT at +4, `JMP $-5` (EB F9) at +5, and RET at +7. `APM_SuspendedLoopFunc` adds 3 to EIP to skip the HLT and JMP. With the prefix on, the callback occupies +0..+4, so the HLT overwrites the high byte of the callback number. The core then returns a callback number of F4xxh, for which `Normal_Loop` runs no handler, because it is at least `CB_MAX`. The jump goes back to +0, and the loop spins without ever calling `APM_SuspendedLoopFunc`, so it never wakes up and never halts.

**How (suggested).** Write the HLT at `CALLBACK_InstrLength()` instead of 4, the JMP right after it with displacement -(length + 3) so that it lands back on +0 (including the CC), and the RET after that. The `+3` in `APM_SuspendedLoopFunc` stays correct, because it is relative to the end of the callback. With the prefix off this produces the same bytes as now. Grep for other code that patches bytes at fixed offsets inside a callback after `CALLBACK_Setup()`. This was the only one I found, but the search was by pattern, not exhaustive.

**Testing.** With the prefix on, use APM suspend from a guest (for example Windows 9x "Suspend", or an APM test tool calling INT 15h AX=5307h, BX=0001h, CX=0002h), then press a key or the power button. The machine must halt while suspended (low host CPU use) and resume. On main-ai, a program that suspends with INT 15h AX=5307h and is woken with Space (the wake-up key, on release) now resumes with the prefix on and off, on the normal core and KVM; with the prefix on it hung before the fix.

## CPU emulation and memory infrastructure

This group covers changes to the emulated CPU and the memory/paging code. Some came out of the KVM core work: the KVM core needs to map guest memory itself and to move the full CPU state in and out of the vCPU. Others are CPU correctness fixes found with the DOSLIB CPU test programs in `testing/cpu/ins` (8086.EXE, 286.EXE, 286P.EXE, 386.EXE, in the doslib-ai tree). The KVM core itself is described in its own section. Here it appears only where a change exists for its sake.

All of it is useful without KVM:
- The memory map notifications, `CPU_FullState` and the debugger commands are general infrastructure.
- Segment access rights in the hidden part, correct V86 segment loads, 286 task switching, segment limit checks, the DR0-DR3 breakpoints and EMM386 paging are emulation accuracy fixes in their own right.

Two constraints from the maintainer apply throughout:
- The simple core skips segment limit checks on purpose, for speed. Nothing here adds them there, and nobody should.
- Everything about limits follows the existing `[cpu] segment limits` setting (`do_seg_limits`).

Implementation order below: infrastructure first (map notifications; access rights, then V86 loads, which `CPU_FullState` builds on), then the debugger commands that exercise it, then CPU fixes from small to large (286 TSS, instruction fixes, segment limit checks, debug registers), and EMM386 paging last because it builds on the V86 segment load change.

### 1. Memory map change notification and page classification

(faed1c403)

**What.** Adds a registry of callbacks that fire when what a guest physical page decodes to may have changed. Also adds a query, `MEM_ClassifyPage()`, that reports how a physical page decodes: RAM, read-only host memory, or device. Emulated behaviour does not change.

**Why.** A CPU core that maps guest memory itself (KVM memory slots) needs to know when the physical map changes, and which pages it can back with host memory directly. DOSBox-X had no such signal:
- `PAGING_ClearTLB()` cannot serve. It is called on every CR3 load, which is far too often, and many map changes do not call it at all.
- DOSBox-X's EMS emulation maps its page frame through `paging.firstmb[]`. That is a linear-to-physical remap of the first 1MB+64KB, applied only while paging is off, and nothing outside DOSBox-X's own paging code can see it.

**How.**
- In `include/paging.h`, add `enum MEM_MapChangeKind` with two values:
  - `MEM_MAPCHANGE_PHYS`: physical pages [page, page+pages) may decode differently now. Their page handler, the host memory behind them or the A20 alias changed. pages == 0 means "all of memory".
  - `MEM_MAPCHANGE_FIRSTMB`: `paging.firstmb[]` changed for linear pages [page, page+pages). pages == 0 means all of them.
- Add the handler type `MEM_MapChangeHandler(kind, page, pages)`, `MEM_AddMapChangeHandler()`, `MEM_RemoveMapChangeHandler()`, `MEM_NotifyMapChange()`, and an inline convenience `MEM_MapChanged(page, pages)` for the PHYS kind. Put the registry in `src/hardware/memory.cpp` as a vector of function pointers. Adding the same handler twice is a no-op.
- Report `MEM_MAPCHANGE_PHYS` from every place that changes `memory.phandlers[]` or the host memory behind a handler. In `memory.cpp`:
  - `MEM_RegisterHandler`, `MEM_InvalidateCachedHandler` (this covers memory callout install/uninstall and LFB moves), `MEM_SetPageHandler`, `MEM_ResetPageHandler_RAM`, `MEM_ResetPageHandler_Unmapped`.
  - `MEM_unmap_physmem`, `MEM_map_RAM_physmem`, `MEM_map_ROM_physmem`, `MEM_map_ROM_alias_physmem`.
  - `RemoveEMSPageFrame` (D0-DF on PC-98/J-3100, E0-EF otherwise), `PreparePCJRCartRom` (D0-DF), `Init_PCJR_CartridgeROM` (E0-EF).
  - `MEM_A20_Enable` when the alias mask actually changes (all pages), and `Init_AddressLimitAndGateMask` (all).
  - `Init_RAM` and `Init_MemoryAccessArray` (all), `MEM_cut_RAM_up_to` (the cut range), and the memory save-state load (all, before the handler table is rebuilt).
- Report from outside `memory.cpp` as well:
  - At the end of `VGA_SetupHandlers()` in `src/hardware/vga_memory.cpp`, for 32 pages from `VGA_PAGE_A0`. SVGA, Tandy and PCjr bank switching change the host pointer behind the same handler object, so a handler-pointer comparison would miss them.
  - In `src/hardware/glide.cpp`, for `GLIDE_LFB>>12` / `GLIDE_PAGES`, whenever `glide.enabled` changes (shutdown, `GLIDE_DisableScreen`, enable in `process_msg`).
- Report `MEM_MAPCHANGE_FIRSTMB` from three places:
  - `PAGING_MapPage()` for lin_page < `LINK_START`, only when the value actually changes.
  - `PAGING_Init()` (all pages).
  - The paging save-state load right after `paging.firstmb` is read (all pages).
- Write `MEM_ClassifyPage(phys_page)`, returning `MEM_PageInfo {kind, host, handler}`. Get the handler with `MEM_GetPageHandler()`, which applies the A20 alias as `PAGING_LinkPage()` does, and pass it the page as addressed. Then:
  - Without `PFLAG_READABLE`, or with a NULL `GetHostReadPt()`, the page is `MEM_PAGE_DEVICE`.
  - Otherwise `host` is the read pointer. The page is `MEM_PAGE_RAM` if it is also `PFLAG_WRITEABLE` and `GetHostWritePt()` returns the same pointer, else `MEM_PAGE_READONLY` (ROM, or a device that maps only reads).
  - `host` stays valid until the next PHYS notification for that page.
- Pitfall: some call sites notify before they modify the table and some after. Handlers must therefore only record the dirty range and act on it later (the KVM core rebuilds its slots before the next guest entry). They must not read `memory.phandlers[]` from inside the callback.
- Rule to document for new code: whatever changes the memory map must call `MEM_MapChanged()`. Clearing the TLB is not enough.

**Save states.** No format change. The loaders just notify.

**Testing.** Exercised through the debugger `MEMMAP` command (section 5) and the KVM core. Per KVM-TODO.md it was checked in real mode, V86 under DOSBox-X's EMM386, and DOS/4GW (16-bit kernel and 32-bit flat), with and without paging.

### 2. Keep segment access rights in the hidden part of the segment registers

(3f1f51f86)

**What.** `Segments` (`include/regs.h`) gets a new array `uint32_t ar[8]`. It holds each segment register's access rights the way the CPU caches them, in the VMX/KVM layout:

| Bits | Field |
|---|---|
| 0-3 | type |
| 4 | S |
| 5-6 | DPL |
| 7 | P |
| 12 | AVL |
| 13 | L |
| 14 | D/B |
| 15 | G |
| 16 | `SEG_AR_UNUSABLE` (null selector in protected mode) |

New constants: `SEG_AR_UNUSABLE` (1<<16), `SEG_AR_REAL_CODE` 9Bh, `SEG_AR_REAL_DATA` 93h, `SEG_AR_V86` F3h.

The emulated cores do not use these values for access checks yet. They exist so the state can be exported exactly (section 4). They are also the groundwork for the type checks and the B=0 expand-down bound listed as open in section 8.

**Why.** DOSBox-X kept base, limit, expand-down and the code/stack big bits, but not the rest of the descriptor cache. The first `CPU_FullState` implementation rebuilt access rights by re-reading the GDT/LDT, which is wrong in two cases that real programs hit:
- The guest changes a descriptor after loading the segment.
- Between setting CR0.PE and the far jump after it, the segment registers still hold real mode segments, which the descriptor tables do not describe.

KVM-TODO.md records the visible failure. DOS/4GW in raw mode triple-faulted now and then under the KVM core: a state fetched mid mode switch came back with "real mode segments rebuilt from the protected mode descriptor tables, not present". The KVM core worked around it at first by keeping the vCPU's own segment cache. Keeping `Segs.ar` makes that workaround unnecessary.

**How.**
- In `include/cpu.h`, add `Descriptor::GetAccessRights()`. It returns:
  - the type and S bits (the descriptor's 5-bit type field), DPL in bits 5-6 and P in bit 7;
  - for `CPU_ArchitectureType >= CPU_ARCHTYPE_386` also AVL (12), the reserved/L bit `saved.seg.r` (13), `big` as D/B (14) and G (15);
  - with the accessed bit (bit 0) forced on for code/data descriptors (S=1), because the CPU sets it when it loads one.
- Set `Segs.ar[]` at every place that loads a segment register's hidden part in `src/cpu/cpu.cpp`. All CPU cores load segments through these functions, so this is the complete list:
  - `CPU_SetSegGeneral`:
    - Protected mode SS load and DS/ES/FS/GS load: `desc.GetAccessRights()`.
    - Null selector for DS/ES/FS/GS: `SEG_AR_UNUSABLE`.
    - Real mode: leave `ar` unchanged, as the CPU does. This keeps unreal-style tricks consistent.
    - V86 mode: `SEG_AR_V86`. Section 3 replaces this with a helper.
  - `CPU_SwitchTask`: CS when the new task is V86 (`SEG_AR_V86`) and in the protected mode CS path (`cs_desc.GetAccessRights()`).
  - `CPU_Interrupt`: the new inner SS on a privilege change. When interrupting V86 mode through a 32-bit gate, GS/FS/DS/ES get zeroed, and must be marked `SEG_AR_UNUSABLE`. Also the new CS.
  - `CPU_IRET`: the two V86 IRET paths and the "back to V86" path for CS (`SEG_AR_V86`). The protected mode same-level and outer-level paths, for CS and SS.
  - `CPU_JMP` and `CPU_CALL`:
    - The real/V86 path for CS (V86 only).
    - The direct code segment path.
    - Through call gates: the new CS, and for CALL the new SS on a privilege change.
    - The task gate/TSS paths go through `CPU_SwitchTask`.
  - `CPU_RET`: the real/V86 path (V86 only), the same-level path, and the outer-level path (CS and SS).
  - Reset and initialisation: in the CPU section init and in `CPU_OnReset`, CS = 9Bh and DS/ES/FS/GS/SS = 93h.
  - `CPU_SYSENTER`: CS C09Bh, SS C093h. `CPU_SYSEXIT`: CS C0FBh, SS C0F3h. These are the fixed flat segments those instructions load.
- `CPU_CheckSegments` (nulling data segments on a return to an outer level) goes through `CPU_SetSegGeneral(seg, 0)`, so it is covered.
- `src/ints/bios.cpp` writes `Segs.limit` directly at two reset-like places without touching `ar`. The commit leaves them alone. Real mode keeps `ar` anyway, but check whether they should reset it too.
- When `CPU_FullState` exists (section 4), `GetSegment` uses `Segs.ar` in protected mode (not V86) instead of reading descriptor tables. Real and V86 mode keep the fixed values VM entry needs. `SetSegment` writes `Segs.ar` back:
  - In protected mode, only when the state's type/S/DPL/P/AVL/D/B or usability differ from what `GetSegment` would produce now. The state has no L bit, and G is derived from the limit, so an unconditional overwrite would lose information on every round trip.
  - In V86 mode it sets `SEG_AR_V86`.

**Save states.** `Segs` is saved as one POD (`WRITE_POD(&Segs, Segs)` in `cpu.cpp`). Adding the array changes the size of that record, and the commit simply extends it. Save states are not expected to stay compatible between DOSBox-X versions, so nothing more is needed.

**Testing.** Through `CPUSTATE` round trips and the KVM core. The DOS/4GW raw-mode triple faults and Windows 3.1/95 under KVM are the cases the change was made for (KVM-TODO.md).

### 3. Virtual 8086 mode segment loads set limit, expand-down and access rights

(1ebe7d125). Depends on 2 for the `ar` part. The limit fix stands alone.

**What.** Any segment register load in V86 mode sets the hidden part as the CPU does:
- limit FFFFh, or ~0 with `segment limits=false`, as for every other segment;
- not expand-down;
- access rights F3h.

This now includes CS.

**Why.**
- `CPU_SetSegGeneral()` set the limit to FFFFh in V86 mode even with segment limits off. Everywhere else, segment limits off means ~0.
- CS loads in V86 mode kept whatever limit CS had before: IRET within V86 and back to V86, a task switch into V86, and far JMP/CALL/RET inside V86. Under DOSBox-X's EMM386 with segment limits off, that was FFFFFFFFh.
- VMX requires base = selector<<4, limit = FFFFh and AR = F3h for every segment in V86 mode, so the KVM core needs this to hold at all times.
- DOSBox-X's own EMM386, in `WinVM86Ctl()` (re-entering V86 mode for Windows), reloaded CS/DS/ES/SS before setting VM. Those loads therefore took the protected mode path, and FS/GS were left as they were.

**How.**
- Add `CPU_SetSegV86Hidden(SegNames)` in `cpu.cpp`, declared in `include/cpu.h`. It sets `Segs.limit` (FFFFh or ~0 depending on `do_seg_limits`), `Segs.expanddown = false` and `Segs.ar = SEG_AR_V86`. It does not touch the selector or base: callers have already used `SegSet16`.
- Call it in:
  - the V86 branch of `CPU_SetSegGeneral`;
  - `CPU_SwitchTask` when the new task has VM set (CS);
  - the three V86 CS paths in `CPU_IRET`;
  - the real/V86 paths of `CPU_JMP`, `CPU_CALL` and `CPU_RET` when `FLAG_VM` is set (in real mode these must keep the old limit, for unreal mode).
- In `src/ints/ems.cpp` `WinVM86Ctl()`, set `FLAG_VM` before reloading the segments and reload all six (CS, DS, ES, FS, GS, SS) through `CPU_SetSegGeneral`.

**Save states.** None.

**Testing.** Covered by the EMM386/V86 runs listed in section 10, and by the KVM core's V86 tests.

### 4. Complete CPU state independent of DOSBox-X internals: `CPU_FullState`

(91556c7f1, revised by 3f1f51f86, and `ReadDescriptor()` removed in 8b10363cd). Implement after 2 and 3 so the descriptor-table fallback is never needed.

**What.** A new header `include/cpu_state.h` and `src/cpu/cpu_state.cpp` (add it to `src/cpu/Makefile.am` and the Visual Studio project and filters). `CPU_GetFullState()` and `CPU_SetFullState()` copy the whole architectural state to and from a plain struct. The struct mirrors what KVM takes (`kvm_regs`, `kvm_sregs`, `kvm_fpu`/FXSAVE, the debug registers, the SYSENTER MSRs) without including Linux headers. Debugging aids: `CPU_FullStateToString()` and `CPU_FullStateRoundTripCheck()`.

**Why.** A hardware virtualisation core, or any future external consumer such as a GDB stub or a state dump, needs the CPU state in a neutral form. It also has to be able to put that state back without perturbing DOSBox-X's own representation (lazy flags, `cpu.stack.mask`, the TSS cache, FPU tag handling and so on).

**How.**
- Data structures:
  - `CPU_FullSegment`: selector, base, limit (in bytes, after granularity), type (4 bits), dpl, s, present, db, g (set when limit > FFFFFh), avl, unusable.
  - `CPU_FullTable`: base, 16-bit limit.
  - `CPU_FullState` holds:
    - eax..edi in REGI order, eip, eflags;
    - `seg[6]` indexed by `SegNames` (es, cs, ss, ds, fs, gs), plus tr, ldt, gdt, idt;
    - cr0, cr2, cr3, cr4, cpl, `dr[8]`;
    - fcw, fsw, ftw (FXSAVE abridged tag), `st[8][10]` (ST(0)..ST(7), 80-bit, in stack order as FXSAVE stores them), `xmm[8][16]`, mxcsr;
    - sysenter_cs/esp/eip.
- Accessors that `cpu_state.cpp` needs:
  - On the GDT descriptor table class in `include/cpu.h`: `GetLDTBase()`, `GetLDTLimit()`, `SetLDTCache(value, base, limit)`.
  - In `cpu.cpp`, since `cpu_tss` is local to that file: `CPU_GetTaskRegister()` and `CPU_SetTaskRegister()` for selector, descriptor, base, limit, is386, valid.
  - In `src/fpu/fpu.cpp`, next to the FXSAVE code: `FPU_GetFullState()` and `FPU_SetFullState()`. They use the existing static `FPU_GetAbridgedTag`/`FPU_SetAbridgedTag` and `fpu_GetRegister80()`, and map ST(i) through `STV(i)`. On builds without `HAS_LONG_DOUBLE`, the setter must also refresh the double copy (`fpu.regs[].v` via `float80::convertToDouble`) and set `use80`.
  - `cpu_sep_cs`/`cpu_sep_esp`/`cpu_sep_eip` are used as externs.
- `CPU_GetFullState()`:
  1. Call `FillFlags()` and copy the registers.
  2. For each segment:
     - Real mode: type Bh for CS (execute/read, accessed) and 3h for data, DPL 0.
     - V86 mode: type 3h for all, CS included, with DPL 3, which gives F3h.
     - Protected mode: the fields come from `Segs.ar`. `SEG_AR_UNUSABLE` makes the segment unusable and not present. D/B for CS/SS comes from `cpu.code.big`/`cpu.stack.big`; other segments use the D/B bit in `ar`.
     - CS and SS get DPL = CPL, except for a conforming code segment.
     - For data segments, the expand-down bit of the type follows `Segs.expanddown`.
     - G always follows the limit, not the descriptor. With segment limits off, the limit is ~0, so that is the only consistent choice.
  3. TR comes from the TSS cache. When no TR is loaded, report a present busy TSS (type Bh for a 386 TSS, 3h for a 286 TSS), as the CPU has after reset, because VM entry needs a usable TR.
  4. LDT is type 2, unusable if the selector is null.
  5. CR2/CR3 come from `paging`.
- `CPU_SetFullState()`, in this order:
  1. CR3 (only if different, because loading it clears the TLB), then CR2 and CR4.
  2. CR0 without `CPU_SET_CRX(0)`. That function also switches the CPU core and the cycles mode on entry to protected mode, which is wrong here. Instead set `cpu.cr0` and `cpu.pmode`, call `PAGING_SetWP` when WP changes on 486 and later, and call `PAGING_Enable(pmode && PG)`.
  3. Then the CPL.
  4. The GPRs and EIP. Call `FillFlags()` before writing `reg_flags`, to discard pending lazy flags, then recompute `cpu.direction` from DF.
  5. Segments: base, limit, expanddown (S=1 and type & Ch == 4) and `ar` as in section 2. Then `cpu.code.big`, `cpu.stack.big` and `stack.mask`/`notmask`.
  6. TR: rebuild a descriptor from the state's fields (`MakeDescriptor`), or keep DOSBox-X's descriptor when the state says unusable.
  7. The LDT cache, GDT, IDT, DRs, FPU and SYSENTER MSRs.
- Not handled: `CPU_SetFullState` writes `cpu.drx[]` directly. Once section 9 exists, it should also call `CPU_DebugRegsChanged()` when DR0-DR3 or DR7 change (only then: the function also ends the decoder's time slice). Otherwise breakpoints a guest set while running under KVM are not active when it falls back to an emulated core. main-ai does this since `a0d427709`.
- Round-trip check: snapshot everything `SetFullState` writes, in DOSBox-X's own representation:
  - `cpu_regs`, `Segs`, CPL, CR0/CR4, pmode;
  - the GDT/IDT/LDT caches, stack mask and big flags, code big, direction, `drx`;
  - CR2/CR3, paging enabled and WP;
  - the TR cache (descriptor only when valid), the SYSENTER values;
  - FPU cw/sw, regvalid, the 80-bit registers, XMM and MXCSR.

  Then do get followed by set, snapshot again, and report the names of the fields that differ.

**Save states.** None.

**Testing.** `CPUSTATE` round trips in real mode, V86 (DOSBox-X EMM386), DOS/4GW 16-bit kernel and 32-bit flat code, with paging (VCPI) and without: all OK (KVM-TODO.md).

### 5. Debugger commands `MEMMAP` and `CPUSTATE`

(51e30db8b). Depends on 1 and 4.

**What.** Two new debugger commands, both listed in the debugger help:
- `MEMMAP [start [end]]` shows the physical memory map as `MEM_ClassifyPage()` sees it, and the map change notifications since the last `MEMMAP`.
- `CPUSTATE` shows `CPU_FullState` and runs the round-trip check.

**Why.** These make the infrastructure in sections 1 and 4 inspectable. They let a developer verify that a device's map change is reported, and that state conversion is lossless, without a KVM build.

**How (`src/debug/debug.cpp`).**
- In `DEBUG_Init()`, register a map-change counter with `MEM_AddMapChangeHandler`. It keeps two buckets, PHYS and FIRSTMB, each with a count, the lowest and highest page touched, and an "all pages" flag (set when pages == 0).
- `MEMMAP`:
  - Arguments are hex byte addresses, converted to pages. The default range is 0 to `MEM_PageMask()`, capped at FFFFFh pages (4GB).
  - Walk the pages and merge runs that have the same kind and the same handler object, and whose host pointers are contiguous (4096 bytes per page).
  - Print each run as: start-end, kind (RAM/READONLY/DEVICE), the host pointer as `MemBase+offset` when inside `GetMemBase()`..+`MEM_TotalPages()`*4096 (else the raw pointer), and the handler's class name (`typeid(...).name()`, so RTTI must be available).
  - Then print each bucket ("none", "N, including all pages", or "N, pages lo-hi") and reset the counters.
- `CPUSTATE` prints `CPU_FullStateToString()` line by line, then either "round trip OK" or the list of changed fields.

**Testing.** As in section 4.

### 6. 286 TSS task switching

(1ce49858f)

**What.** Task switches to and from 16-bit (286) TSSes work. Previously `CPU_SwitchTask()` called `E_Exit("286 task switch")` for any 16-bit TSS. CALL to a 286 TSS exited with "CALL:Descriptor type 1 unsupported". JMP or CALL to a busy TSS now raises #GP(selector) instead of E_Exit.

**Why.** Real 286 protected mode software uses 286 TSSes. DOSLIB 286P.EXE exercises calling a TSS and returning via IRET with NT, the back link, busy bits, the saved IP, and JMP to a busy TSS.

**How (`src/cpu/cpu.cpp`).**
- Reading the new TSS (16-bit branch), using `offsetof(TSS_16, ...)`:
  - IP, FLAGS, AX, CX, DX, BX, SP, BP, SI, DI, ES, CS, SS, DS and LDT.
  - Each 32-bit GPR keeps its upper 16 bits.
  - FS and GS keep their current selectors, and CR3 keeps its current value (`paging.cr3`), since a 286 TSS has neither.
- Saving the old TSS (16-bit branch): FLAGS (low 16 bits), IP (low 16 bits of the old EIP), AX..DI, ES, CS, SS, DS. No FS/GS.
- Loading the registers (16-bit branch):
  - EIP = the 16-bit IP.
  - Flags via `CPU_SetFlags(new, (FMASK_ALL & FFFFh) | FLAG_VM)`, so only the low 16 bits are loaded and VM is cleared (the value has bit 17 clear). A 286 task never runs in V86 mode.
  - The GPRs as merged above.
  - The selector loading after that is shared with the 386 path.
- `CPU_JMP`:
  - A direct TSS descriptor accepts `DESC_286_TSS_A` as well as `DESC_386_TSS_A`.
  - `DESC_286_TSS_B`/`DESC_386_TSS_B` raise `EXCEPTION_GP` with selector & FFFCh.
  - Through a task gate, the target may be a 286 available TSS.
- `CPU_CALL`: the same three changes.
- The commit does not change what happens when a task gate points at a busy TSS.

**Save states.** None.

**Testing.** DOSLIB 286P.EXE on cputype=286, cputype=386 and machine=pc98.

### 7. Instruction fixes: 16-bit SHLD OF, V86 INT3/INTO, zero shift count in the full core, expand-down upper bound

(927da6344). All four were found with DOSLIB 386.EXE.

**16-bit SHLD overflow flag.**
- What/why: OF compared the result with the wrong word. For `t_DSHLw`, the lazy flags pack op1:op2 into `lf_var1d`, so the original destination is the high word, not `lf_var1w`. (`t_DSHRw` packs op2:op1, and there `lf_var1w` is right.)
- How: in `src/cpu/flags.cpp`, `get_OF()` gives `t_DSHLw` its own case, which XORs the result with `lf_var1d >> 16`. The `t_DSHLw` case of `FillFlags()` gets the same correction.

**INT3 and INTO in V86 mode are not IOPL sensitive.**
- What/why: only INT n (CDh) traps to the monitor when IOPL < 3. INT3 (CCh) and INTO (CEh) go through the IDT gate directly.
- How:
  - The normal core already used `CPU_SW_Interrupt_NoIOPLCheck` for INT3. In `core_normal/prefix_none.h`, INTO now uses it too.
  - The full core (`core_full/op.h`, `O_INT`) handles CC, CD and CE in one place. Use `CPU_SW_Interrupt` only when the opcode byte (`inst.entry & FFh`) is CDh, and `CPU_SW_Interrupt_NoIOPLCheck` otherwise.

**Full core: a shift/rotate count that masks to 0.**
- What/why: in `core_full/load.h`, for `M_SHIFT_Ib` and `M_SHIFT_CL`, a count of 0 after `& 1Fh` did a `break` out of the ModR/M decode before the group operation was resolved. An unrelated operation then executed and changed the flags.
- How: a zero count jumps straight to `nextopcode`, a no-op with flags unchanged.

**Expand-down segments: upper bound of 4GB on direct (moffs) operands.**
- What/why: an access through an expand-down segment that crosses FFFFFFFFh must raise #GP (the upper bound of a B=1 segment).
- How:
  - In the normal core's `GetEADirect` macro (`core_normal/table_ea.h`), add the check `eaa + size - 1 > FFFFFFFFh`, computed in 64 bits, after the existing expand-down lower-bound check.
  - The B=0 bound (FFFFh) is not checked, because the B bit of data segments is not kept (`Segs.ar` from section 2 now has it, as D/B bit 14). This is listed as open in section 8.

**Testing.** DOSLIB 386.EXE.

### 8. Segment limit checks for ModR/M operands and the stack (normal, 286, full and dynamic cores); LDS/LES/LSS/LFS/LGS operand order

(331b571a4 for the normal and 286 cores, de4ef06ef for the full and dynamic cores and the LxS order). Not for the simple core, by design.

**What.**
- Memory operands addressed through ModR/M are checked against the limit of the segment they use, at the actual access and with the actual size:
  - LEA never faults.
  - LES/LDS-type instructions check both the offset and the selector word.
- Stack PUSH/POP done by the core's own instructions are checked against SS.
- A violation raises #GP(0), or #SS(0) when the segment is SS, with EIP at the start of the instruction.
- Real and V86 mode use the cached limits, so unreal mode keeps working.
- A limit equal to `EANoSegmentLimitMagic` disables the check: FFFFFFFFh, or FFFFh on the 286 core. This matches a 386 with a 4GB segment, which does not fault on wrap.
- Everything follows `segment limits`.

**Why.** Before this, the normal core checked limits only for direct (moffs) operands in `GetEADirect`, and string instructions had their own checks. ModR/M operands and the stack were not checked at all. DOSLIB found these failures:
- 386.EXE: real mode offsets above FFFFh did not fault; a V86 word access at FFFFh did not fault; resetting unreal mode had no effect on later accesses.
- 286.EXE and 286P.EXE: various protected mode limit tests.

The LxS reordering came from a regression: with the checks in place, `LSS ESP,[ESP]` checked the offset against the *new* SS base, and CauseWay failed with "error 05: Not enough memory".

**How, shared part (`src/cpu/core_normal/seglimit.h`, new).**
- `CORE_EA_NONE` (FFh) means "no ModR/M operand, do not check".
- `CoreSegLimitFault` is a small struct carrying the exception vector. It is thrown as a C++ exception from the check and caught around the core loop. This keeps the fast path to a compare and a branch, and avoids threading an error return through every `LoadM*`.
- `Core_SegLimitCheck(seg, lin, size)` does the check:
  - It does nothing if seg is `CORE_EA_NONE` or `do_seg_limits` is off.
  - offset = lin - `SegPhys(seg)` (32-bit), last = offset + size - 1 (64-bit).
  - Expand-down segment: bad if offset <= limit and limit != 0, or if last > FFFFFFFFh. The limit 0 exception mirrors the existing note in `table_ea.h`.
  - Normal segment: bad if last > limit and limit != `EANoSegmentLimitMagic`.
  - Vector `EXCEPTION_SS` for SS, `EXCEPTION_GP` otherwise.
- `EANoSegmentLimitMagic` is defined there under `#ifndef`, with the same `CPU_CORE`-based value as in `core_normal/helpers.h`, because `seglimit.h` is included first. The full core defines `CPU_CORE` as `CPU_ARCHTYPE_386` before including it.
- Stack check `Core_StackLimitCheck(off, size)`:
  - Always `EXCEPTION_SS`.
  - The upper bound of an expand-down stack is FFFFFFFFh if `cpu.stack.big`, else FFFFh.
  - `Core_Push16/32` check (ESP - size) & `stack.mask`, and `Core_Pop16/32` check ESP & `stack.mask`, before calling `CPU_Push*`/`CPU_Pop*`.
- Normal-core-only part, excluded when `SEGLIMIT_NO_CORE_STRUCT` is defined: the macros `EA_SEG_DS`/`EA_SEG_SS`, which store `core.base_val_ds`/`core.base_val_ss` into `core.ea_seg`, and `Core_EALimitCheck(lin, size)`, which passes `core.ea_seg`.

**How, normal and 286 cores (`core_normal.cpp`, `core_normal_286.cpp`, `core_normal/table_ea.h`).**
- Add `SegNames base_val_ss` and `uint8_t ea_seg` to the core struct:
  - At the start of each instruction: `base_val_ss = ss` and `ea_seg = CORE_EA_NONE`.
  - `DO_PREFIX_SEG` sets `base_val_ss` to the override segment, just as it already sets `base_val_ds`. That is needed because BP-based modes use `BaseSS`, which the override also replaces.
- In `table_ea.h`, define `EA_SEG_DS`/`EA_SEG_SS` as empty if not defined. Other cores that include the file (the simple core, the prefetch cores and so on) then get no checks and no cost.
- Put the matching macro in every EA function:
  - 16-bit modes: BX/SI/DI/disp16 record DS, BP-based ones record SS.
  - 32-bit modes: record DS, and SS for the EBP base with mod != 0.
  - `Sib()`: each base case records its own segment (ESP base = SS, EBP base with mod != 0 = SS, no-base disp32 = DS).
- Redefine `LoadMb/w/d/q` and `SaveMb/w/d/q` to call `Core_EALimitCheck(off, size)` before the access. The Q forms check 8 bytes once.
- Instruction fetch (`Fetchb/w/d`, `FetchPeekb`, and the prefix peek in the 286 core's `DO_PREFIX_SEG`) must stop using `LoadM*` and call `mem_read*_inline` directly. Otherwise code fetches would be checked against the last data segment.
- Map `Push_16`/`Push_32`/`Pop_16`/`Pop_32` to `Core_Push*`/`Core_Pop*`.
- Rename the core function to a static `..._Run_Loop` and wrap it in the public `CPU_Core_Normal_Run`/`CPU_Core286_Normal_Run`. The wrapper loops on try/catch of `CoreSegLimitFault`: `FillFlags()`, `CPU_Exception(vector, 0)`, return if `CPU_Cycles` <= 0, otherwise continue. EIP is still at the start of the instruction because the normal core only commits `reg_eip` at the end of the instruction. The trap-run variants call the wrapper, so they are covered.
- Not covered:
  - String instructions have no ModR/M, so `ea_seg` stays NONE. They keep their existing checks in `string.h`.
  - x87/SSE memory operands use `mem_read*` directly in `src/fpu`, not `LoadM*`. As far as the diff shows they are not checked.

**How, LxS operand order (normal core).** In `prefix_none.h` (LES, LDS), `prefix_66.h` (32-bit LES, LDS), `prefix_0f.h` (LSS, LFS, LGS) and `prefix_66_0f.h` (32-bit LSS, LFS, LGS):
1. Read the offset (word or dword at eaa) and the selector (word at eaa+2 or eaa+4).
2. Then call `CPU_SetSegGeneral()`.
3. Then write the register.

The check computes the offset from the segment's base *at the time of the access*. With the old order, loading SS first made the second read's offset relative to the new SS base. This is also the architectural order: the CPU reads the full pointer before loading the segment.

**How, full core (`core_full.cpp`, `core_full/ea_lookup.h`, `load.h`, `loadwrite.h`, `support.h`).**
- In `FullData`, add `rm_seg` and `seg.idx` (the override segment, set at `L_PRESEG`).
- `ea_lookup.h` tracks a local `seg_idx` next to `seg_base` in every 16-bit and 32-bit case, including the `SIB` macro. It stores `inst.rm_seg` = override idx or `seg_idx`.
- The direct-address path in `load.h` sets `rm_seg` to the override or DS.
- Reset `rm_seg` to `CORE_EA_NONE` per instruction.
- Include `seglimit.h` with `SEGLIMIT_NO_CORE_STRUCT` defined.
- `LoadM*`/`SaveM*` call `Core_SegLimitCheck(inst.rm_seg, off, size)`.
- `the_Fetchb/w/d` use `mem_read*_inline` directly.
- Push/Pop map to the `Core_*` versions.
- Catch `CoreSegLimitFault` in the main loop next to the existing exception catch: restore `reg_esp = old_esp`, because the full core may already have popped, then `FillFlags()` and `CPU_Exception(vector, 0)`, and continue without `SaveIP`.

**How, dynamic core (`core_dyn_x86/decoder.h`, x86-64 code generator only).**
- In the decode struct, add `ea_check`, `ea_seg` and `ea_reg`. Reset `ea_check` per instruction in `CreateCacheBlock`.
- Split `dyn_fill_ea` into an inner function, which records the default segment's DynReg (`dyn_ea_segbase`) in every addressing case, and a wrapper. When `addseg` is set, the wrapper stores the segment index (via a DynReg-to-`SegNames` map) and the EA register.
- For MOV A0h-A3h (moffs), `dyn_ea_note_direct()` records DS or the override with `DREG(EA)`.
- `dyn_read_byte/word` and `dyn_write_byte/word` first call `dyn_seglimit_check(addr, size)`. It emits code only when `ea_check` is set, addr is the recorded EA register (so only the ModR/M operand is checked) and `do_seg_limits` is on.
- The emitted fast path:
  1. Compare `Segs.expanddown[s]` with 0, and branch to the slow path if set.
  2. `lea tmp, [addr+size-1]`.
  3. `sub tmp, Segs.phys[s]`.
  4. `cmp tmp, Segs.limit[s]`.
  5. `jbe ok`.
- The slow path calls `dyn_seglimit_slow(lin, seg | size<<8)`. That function repeats the full check (expand-down, the FFFFFFFFh magic, the setting) and returns `CPU_PrepareException(...)`. `dyn_check_bool_exception_al` follows the call.
- Register allocation needs care: mark the source register and the two argument registers not usable while picking the temp, and save/sync the `DynState` around the call.
- Byte accesses with 16-bit addressing in real/V86 mode get no check, because they cannot exceed a 64KB limit.
- Pitfalls:
  - `do_seg_limits` and the CPU mode are evaluated at translation time and baked into the cached block. A block translated in real mode and later run in protected mode would lack its byte checks. Consider including the mode in the decision or the cache key.
  - The inline fast path computes in 32 bits, so an access that wraps past 4GB through a segment with a limit below 4GB passes the fast path.
  - The 32-bit x86 generator and the dynrec core have no checks at all.

**Known remaining problems (TODO.md in main-ai, from the same tests).**
- Access through a null selector in protected mode should raise #GP(0) (286P "access through null DS").
- The B=0 expand-down upper bound (FFFFh) needs the B bit; `Segs.ar` now provides it (286P "expand-down mov ax,[FFFF]").
- No type checks yet: writes through read-only data or code segments, and reads through execute-only code (286P).
- The accessed bit is not set in the GDT/LDT descriptor on a segment load (286P).
- Pushes during exception/interrupt delivery and during far CALL/RET/IRET in `cpu.cpp` are not limit-checked. 286P "Ring 3: pop past SS limit" gives #DF instead of #SS on the normal and full cores.
- The dynamic core does not check the stack. It gives #GP instead of #SS for 286P "push, expand-down SS" and "pop past SS limit".
- The 286 core's string instructions still treat FFFFh as "no check".
- Unrelated to limits, from the same runs:
  - cputype=8086 SHR r8/r16 by CL=33 gives wrong flags (30 failures).
  - Full core with cputype=386: `MOV CS,AX` does not raise #UD, and MOVSW from [FFFF] does not #GP in real mode.
  - cputype=286: SMSW should return 1s in MSW bits 4-15.
  - cputype=286: MOVSW from [FFFF] reads the second byte from 10000h instead of wrapping to 0000h.

**Save states.** None.

**Testing.**
- DOSLIB 386.EXE, 286.EXE and 286P.EXE on the normal, 286 and full cores, and on the dynamic core.
- CauseWay after the LxS fix.
- The EMM386 runs in section 10 (DOS/4GW, DOS/32A, PMODE/W, CauseWay, Quake, Windows 3.1) were done later, on builds that include these checks. `segment limits` is on by default, but the commit messages do not say which setting was used.

### 9. DR0-DR3 hardware breakpoints; CS limit on far JMP/CALL

(2cdd91796)

**What.**
- The debug registers' breakpoints take effect on the emulated cores:
  - Instruction breakpoints fault before the instruction, and EFLAGS.RF suppresses one check.
  - Data write and read/write breakpoints trap after the instruction.
  - If a single-step #DB happens on the same instruction, both are reported in one DR6.
  - I/O breakpoints (RW=10b) are not emulated.
- With DR7 enabling nothing, the only cost is one flag test per decoder call.
- Separately, a far JMP or CALL to a code segment raises #GP(0) when the new EIP is beyond the segment limit (with segment limits on).

**Why.** DOSBox-X stored DR0-DR7 but never acted on them, so guest debuggers and software that uses debug registers did not work. Both problems were found with DOSLIB 386.EXE and 286P.EXE.

**How.**
- State in `src/cpu/cpu.cpp`:
  - `cpu_dbg_active` (exported).
  - A 4-entry breakpoint table: on, rw, addr aligned down to len, and len from the LEN bits (00 = 1, 01 = 2, 10 = 8, 11 = 4).
  - `cpu_dbg_pending`, a bitmask.
  - `cpu_dbg_in_step`.
- `CPU_DebugRegsChanged()`:
  - Rebuilds the table from DR7: the L/G enable bits for breakpoint i are bits 2i/2i+1, RW at bit 16+4i and LEN at 18+4i. Breakpoints with RW=2 are disabled.
  - Sets `cpu_dbg_active` if any breakpoint is enabled.
  - Passes the pages of the data breakpoints to `PAGING_DebugSetPages()`.
  - Ends the current time slice (moves `CPU_Cycles` into `CPU_CycleLeft` and sets `CPU_Cycles` to 0), so the main loop picks the right runner before the next instruction.
- `CPU_WRITE_DRX()` calls `CPU_DebugRegsChanged()` after writing DR0-3, DR5 or DR7. DR5 is included because it aliases DR7 when CR4.DE=0.
- `src/dosbox.cpp` `Normal_Loop`: call `CPU_Debug_Run()` instead of `(*cpudecoder)()` when `cpu_dbg_active` is set. Mark the branch unlikely.
- `CPU_Debug_Run()` loops while cycles remain and `cpu_dbg_active` is set:
  1. If RF is clear, compare CS base + EIP with each execute breakpoint. On a hit, call `FillFlags()` and `CPU_DebugException(hits, reg_eip)`, then charge a cycle and continue.
  2. Run exactly one instruction:
     - Detect whether the current decoder is a trap-flag decoder (`CPU_Core_*_Trap_Run` for normal, simple, dyn_x86, dynrec and prefetch). If it is, call it, since it runs one instruction and raises the single-step #DB. Otherwise call `CPU_Core_Normal_Run()` with `CPU_Cycles = 1`.
     - Set `cpu.trap_skip = true`, clear `pending` and set `in_step` around the call. Reset `in_step` if the call throws.
  3. Clear RF if it was set.
  4. If any data breakpoints are pending:
     - If a single-step #DB was raised (trap decoder and `trap_skip` now false), OR the pending bits into DR6.
     - Otherwise call `CPU_DebugException(pending, reg_eip)`, a trap with EIP after the instruction.
  5. Return any non-`CBRET_NONE` result.

  Under the KVM core it simply calls the KVM decoder: there the guest's MOV DRx runs in the vCPU, and the breakpoints are real hardware ones.
- `CPU_DebugException()` already existed and does not set RF in the pushed EFLAGS image. A guest handler that wants to resume past an instruction breakpoint has to set RF itself. Verify that this matches what the target software expects.
- Data breakpoints in `src/cpu/paging.cpp`:
  - `PAGING_DebugSetPages(pages, count)` (at most 4, linear pages below `TLB_SIZE`, deduplicated) unlinks the previously watched pages and records the new ones, then unlinks those too, so they are wrapped when next linked.
  - `PAGING_DebugReapply()` runs at the end of `PAGING_LinkPage`, `PAGING_LinkPageNew`, `PAGING_SwitchCPL` and the `PageFoilHandler` write path, only when `dbg_npages` != 0. For each watched page whose TLB read or write handler is neither the debug handler nor `init_page_handler`, it:
    1. saves the TLB host pointer and handler;
    2. sets the TLB host pointer to NULL;
    3. installs `DebugBPPageHandler`.
  - `DebugBPPageHandler` (flags `PFLAG_NOCODE`) implements all the plain and `_checked` byte/word/dword reads and writes. Each one calls `CPU_DebugDataAccess(addr, size, write)` and then performs the access through the saved host pointer (TLB host pointers are biased by the linear page, so host + linear address works) or the saved handler.
  - `CPU_DebugDataAccess` sets pending bits only while `cpu_dbg_in_step` is set, so DOSBox-X's own accesses outside instruction execution do not trigger. A bit is set for each enabled breakpoint whose range overlaps the access: RW=3 for any access, RW=1 only for writes.
  - Only the watched pages are slowed down, and all other pages keep their direct TLB pointers.
- Far JMP/CALL CS limit (`cpu.cpp`):
  - In the protected mode direct code segment path of `CPU_JMP`, after the present check, add a `CPU_CHECK_COND` that raises `EXCEPTION_GP` 0 when `do_seg_limits` is on and offset > `desc.GetLimit()`.
  - `CPU_CALL` gets the same check, comparing the 16-bit-masked offset when the operand size is 16.
  - Call gate paths are not changed.
- Pitfalls and open points:
  - Instruction breakpoints compare linear addresses only at instruction boundaries reached by `CPU_Debug_Run`. While breakpoints are active everything runs one instruction at a time on the normal core, so it is slow, by design.
  - A data access that straddles into a watched page from an unwatched one is only seen if the access goes through the watched page's handler.
  - The save-state load and `CPU_SetFullState` must call `CPU_DebugRegsChanged()` too. Without it (main-ai before `a0d427709`), after loading a state with DR7 set the breakpoints stay inactive until the next MOV DRx. main-ai calls it at the end of the CPU component's save-state load (after paging is loaded, which the watched pages need), and from `CPU_SetFullState` when DR0-DR3 or DR7 change. Tested: a DR0 write breakpoint saved in one session and loaded into a session that never set it traps all three writes (none before).

**Save states.** No format change, but see the last pitfall.

**Testing.** DOSLIB 386.EXE and 286P.EXE.

### 10. DOSBox-X's EMM386: run V86 mode with paging, as EMM386.EXE does

(8b10363cd). Depends on 3 (`WinVM86Ctl` segment reload); `PAGING_GuestPageTablesChanged` is new here.

**What.** When DOSBox-X's built-in EMM386 puts DOS into V86 mode, paging is now on (CR0=80000011h instead of 00000011h), with page tables built by DOSBox-X. The EMS page frame is mapped through those page tables. The `paging.firstmb[]` remap remains for EMS without V86 mode.

**Why.**
- `paging.firstmb[]` only applies while paging is off, and it is DOSBox-X-internal: a KVM vCPU (or any other CPU implementation) cannot see it. Real EMM386.EXE uses page tables.
- VCPI DE0C (switch from protected mode to V86) used to turn paging off first. That worked only when the client's stack was identity-mapped. It also wrote the client's TSS descriptor (via `CPU_TSS_ForceBusy(false)`) to the client's GDT *linear* address as if it were physical.

**How (`src/ints/ems.cpp`).**
- Add `uint32_t page_dir` to the `vcpi` struct (0 means no paging) and register it for save states.
- `SetupVCPI()` builds the tables:
  1. tables = ceil(`MEM_TotalPages()` / 1024).
  2. Allocate 1 + tables pages with `MEM_AllocatePages(n, true)` (contiguous).
  3. Fill the page directory with the first `tables` entries pointing to the following pages, flags 7 (present, writable, user), and the rest 0.
  4. Fill the page tables with all RAM pages mapped to themselves (flags 7), and 0 beyond RAM.

  If the allocation fails, log it and run V86 mode without paging, as before. Identity-mapping all of RAM is required for two reasons: the CPU reaches EMM386's GDT, IDT and TSS (in the private area above 1MB) through the tables, and DOSBox-X's own code copies extended memory by linear address (XMS moves, for example). The cost is 4KB per 4MB of RAM plus one page, taken from extended memory.
- `EMM_SetPTE(lin_page, phys_page)`: if paging tables exist and lin_page < 400h, write (phys << 12) | 7 into the first page table, whose address comes from PDE 0.
- `EMM_EnterPaging()`:
  - With tables: CR3 = `page_dir` first, then CR0 = (CR0 & ~TS) | PG | PE. CR3 must be loaded before PG is set.
  - Without tables: CR0 = (CR0 & 7FFFFFF7h) | PE, CR3 = 0.
- Use `EMM_EnterPaging()` wherever V86 mode is entered:
  - the startup V86 task setup (replacing `CPU_SET_CRX(0, 1)`; the old TODO about page tables goes away);
  - `WinVM86Ctl()` when Windows re-enables V86 mode;
  - VCPI DE0C.
- When `WinVM86Ctl()` disables V86 mode for Windows, also clear CR3.
- `EMM_MapPage()` and `EMM_MapSegment()`, for mapping and unmapping alike: next to every `PAGING_MapPage()` call, also call `EMM_SetPTE()` with the same pages. Replace `PAGING_ClearTLB()` with `PAGING_GuestPageTablesChanged()`.
- VCPI DE01 (get protected mode interface):
  - When tables exist, copy the 256 PTEs of EMM386's first page table into the client's buffer, keeping the frame address and forcing the flags to 67h. EMS mappings are then already included, so skip the old "adjust page frame entries" loop.
  - Without tables, keep the old synthetic buffer.
  - Note a behaviour difference: the old fixed buffer mapped page FFh to 110000h, while the copy maps it as EMM386's table does, to itself.
- VCPI DE0C (from protected mode to V86), new order:
  1. Clear IF.
  2. Read the 9-dword V86 return frame (EIP, CS, EFLAGS, ESP, SS, ES, DS, FS, GS), which sits after the 8-byte far return address at SS:ESP+8, through the client's paging, with `mem_readd` while the client's CR3 is still loaded.
  3. Force the frame's EFLAGS to 23002h (VM, IOPL 3).
  4. Call `CPU_TSS_ForceBusy(false)` while the client's GDT and paging are still active, so the client's TSS descriptor is written where the client has it.
  5. `EMM_EnterPaging()`.
  6. Clear the busy bit of EMM386's TSS descriptor in the private area GDT (selector 10h).
  7. LGDT/LIDT, LLDT 08h, LTR 10h.
  8. Load SS = 14h (EMM386's ring 0 stack in its LDT, the same one its TSS names), set ESP = 2000h - 36, and write the saved frame there.
  9. Clear NT and `CPU_IRET(true, 0)` into V86 mode.

  The old code instead patched the flags in place on the client's stack, turned paging off, and did ESP += 8.
- `src/cpu/paging.cpp` and `include/paging.h`: add `PAGING_GuestPageTablesChanged()`, which calls `PAGING_ClearTLB()` and then the optional function pointer `PAGING_GuestPageTablesHook`. The point is that DOSBox-X changed the guest's page tables itself, and a core with its own TLB must flush. The KVM core sets the hook. Its handler flags a flush, and before the next KVM_RUN it sets the special registers once with CR3 ^ 1000h and once as they are, which forces KVM to reset the vCPU's MMU context. Details are in the KVM section.
- `cpu_state.cpp`: `ReadDescriptor()` is removed, since section 2 made it unused.
- Pitfall: only RAM is mapped. Device memory above RAM (LFB, PCI BARs) has no PTEs. A V86 program cannot reach it anyway, but any DOSBox-X code that touches such memory by *linear* address while EMM386's V86 mode is active would page fault. This was not observed in testing; it is mentioned so a reviewer can check.
- The firstmb remap is still applied when paging is off (`ems=true` without V86). Keep both paths in sync in `EMM_MapPage`/`EMM_MapSegment`.

**Save states.** `vcpi.page_dir` is added to the EMS module's registered PODs, which changes that save-state layout. The tables themselves live in guest RAM and are saved with memory.

**Testing.** With EMM386 active on the normal, dynamic and KVM cores:
- EMS mapping and VCPI DE06;
- DOS/4GW, DOS/32A, PMODE/W and CauseWay through VCPI;
- Quake (CWSDPMI) timedemo;
- Windows 3.1 in enhanced and standard mode.

## RPC debugger agent, headless debugger, and debugger/save-state tooling

Master already has the RPC debugger agent (`src/agent/`, `include/agent/`, the Python client in `client/python/dosbox_agent`, `docs/rpc-agent-usage.md`, `docs/rpc-development.md`, the `--enable-dosbox_agent` configure switch, and unit tests in `tests/agent/`). On master it effectively works only on Windows with Visual Studio. With autotools it does not build at all, the `unix_socket` transport is a stub that always fails, and on Linux/macOS the debugger refuses to start without a terminal. Session lifecycle also has gaps: targets that exit by themselves are never noticed, `session.stop` leaves breakpoints and traces behind, and child programs survive a stop.

This group closes those gaps. It also covers four smaller pieces of tooling that came in alongside: debugger `SAVESTATE`/`LOADSTATE` commands, a PIC save-state fix that keeps host tick handlers (such as the `mcp_server` poll) out of save states, a `-nopromptquit` command-line option for automated runs, and fixes to the console monitor (`monitor socket`). A separate Windows 3.1 framebuffer-publication service on the DOSBox integration device is also in this group. It is described last because nothing else depends on it.

Authorship note: commits 6acb82287 and bd8cc4967 (Windows 3.1 publication) are by Matthew Hiltner, and a2b3f71a6 and dc7554049 (save-state commands, tick handler fix) are by Marcos Hernandez. None of the four carries an AI co-author trailer. Check their provenance before deciding whether they need re-implementation or can be taken as contributed. All the other commits in this group are AI-written.

Implementation order below: build fix, then transport, then headless mode, then session lifecycle (four steps that build on each other), then the save-state pair, `-nopromptquit`, the monitor, the Windows 3.1 service, and finally the reference config files.

### 1. Make the agent build with autotools; fix the `memory.read` error order

(a6103b579)

**What.** Every agent source file must include `config.h` before it tests `C_DEBUG`/`C_DOSBOX_AGENT`. Move the agent header includes in `sdlmain.cpp` below `dosbox.h`. In the JSON-RPC handler, check session state in `memory.read` before validating parameters. Make the unit test create its mount directory.

**Why.**
- Every file under `src/agent/` (`bridge/agent_bridge.cpp`, `debugger/debugger_adapter.cpp`, `parser/debugger_output_parser.cpp`, `protocol/agent_protocol.cpp`, `server/agent_server.cpp`, `trace/trace_store.cpp`, `transport/local_transport.cpp`) starts with `#if defined(C_DEBUG) && defined(C_DOSBOX_AGENT)` before including anything. With autotools those macros come only from `config.h`, so every agent file compiled to an empty object and linking failed. Visual Studio defines the macros on the compiler command line, which hid the problem there.
- `src/gui/sdlmain.cpp` includes `agent/agent_bridge.h` and `agent/agent_server.h` near the top under `#if defined(C_DOSBOX_AGENT)`, which also runs before `config.h` has been pulled in.
- Once the agent built, the existing unit tests showed that `memory.read` on a running target returned `INVALID_PARAMS` instead of `TARGET_RUNNING`. It validated the address and length before checking the session state, unlike `memory.write` and the protocol spec.
- The architecture tests mount `tests/agent/runtime`. On POSIX, `session.start` resolves the mount path with `realpath()`, which fails if the directory does not exist. Windows only normalizes the path text, so this never showed up there.

**How.**
1. Add `#include "config.h"` as the first line of each of the seven agent `.cpp` files listed above, before the feature `#if`.
2. In `sdlmain.cpp`, delete the early agent include block and put the same two includes after `debug.h` (that is, after `dosbox.h` and the other core headers).
3. In `AgentServer::HandleJsonRpcImpl`, branch `memory.read`, reorder the checks: first `Running` returns `TARGET_RUNNING` (`kErrorTargetRunning`), then `Exited` returns `TARGET_EXITED` (`kErrorCapabilityUnavailable`), then parameter validation (`ParseMemoryAddress`, positive `length`), then the `max_memory_read_bytes` limit (`REQUEST_TOO_LARGE`).
4. In `tests/agent/agent_architecture_tests.cpp`, `MakeTestConfig()` creates `tests/agent/runtime` (`_mkdir` on Windows, `mkdir` with mode 0755 elsewhere) and ignores "already exists".

**Testing.** Build on Linux with `./configure --enable-debug --enable-dosbox_agent` and confirm it links. Run `dosbox-x -tests` and confirm the agent architecture and parser tests pass, including the `memory.read`-while-running case.

### 2. Unix domain socket transport for the agent

(495659337)

**What.** Implement `UnixSocketTransport` in `src/agent/transport/local_transport.cpp` to the same contract as the existing `NamedPipeTransport`. Add a matching transport to the Python client. Skip the working-folder prompt when started with `-agent-config`. Add transport unit tests and Linux/macOS documentation.

**Why.** On master, `UnixSocketTransport::Start` always fails with "Unix socket transport is outside the Windows v1 scope", so the agent cannot be used anywhere but Windows. Separately, with default settings and no `dosbox-x.conf`, DOSBox-X showed the "choose working folder" prompt even when launched by automation with `-agent-config`. Nobody can answer it, so automated launches hung or exited.

**How.**
1. Protocol and framing are identical to the named pipe transport:
   - Newline-delimited UTF-8 JSON-RPC. One request per line, a trailing `\r` is stripped, and each response is the handler's string plus `\n`.
   - One worker thread per connected client.
   - A request (or a pending partial line) longer than `max_message_bytes` is answered with whatever the request handler returns for an oversized message (the code passes it a string of `max_message_bytes + 1` filler characters so the handler produces its standard `REQUEST_TOO_LARGE` reply), and then the connection is closed.
2. `ValidateConfiguration`: the endpoint must be an absolute path, and it must be shorter than `sizeof(sockaddr_un::sun_path)`. That is 108 bytes on Linux but only 104 on macOS/BSD, so use the platform's size, not a constant. On `WIN32`, `Start` still returns an error ("unavailable on this platform").
3. `Start`: create a wake pipe first (both ends close-on-exec, write end non-blocking). Then listen:
   - **Stale endpoint handling.** `lstat` the path. If it does not exist, continue. If it exists and is not a socket, refuse ("exists and is not a socket"). If it is a socket, try to `connect()` to it. A successful connect means a live server, so refuse ("in use by another process"). `ECONNREFUSED` means a socket left by a crashed process, so `unlink` it and continue. Any other error is reported.
   - Create the socket (close-on-exec, non-blocking) and `bind` it. Then `chmod` the path to 0600 *before* `listen()`. Connections are refused until `listen()`, so there is no window in which another user could connect. Record the socket file's `st_dev`/`st_ino`, then `listen(SOMAXCONN)`.
   - Start the listener thread.
4. Listener thread:
   - Wait with `poll()` on the listen fd plus the wake pipe's read end.
   - `accept()`, retrying on `EINTR`/`EAGAIN`/`ECONNABORTED`.
   - On each accepted socket, set close-on-exec and set `O_NONBLOCK` explicitly. macOS/BSD inherit it from the listener and Linux does not; all blocking is done by `poll()`.
   - Where `SO_NOSIGPIPE` exists, set it on the socket.
   - Check the peer is the same user: `getsockopt(SO_PEERCRED)` on Linux, `getpeereid()` elsewhere, compared with `geteuid()`. Close the connection on mismatch.
   - Under the mutex, unless stopping, start a client worker thread and keep it in a list.
5. Client worker: loop `poll()` on client fd + wake pipe, then `recv` into a pending buffer, split on `\n`, call the handler and write the full response. The write loop polls for `POLLOUT` and also watches the wake pipe. Send with `MSG_NOSIGNAL` where defined. DOSBox-X does not ignore `SIGPIPE`, so a client that disconnects mid-response would otherwise kill the emulator. After each batch, check the stopping flag under the mutex. Close the fd when the worker ends.
6. `Stop`: under the mutex set stopping and take the listener thread out. Write one byte to the wake pipe and never drain it, so every thread polling it wakes and stays woken. Join the listener, then swap out and join all client workers, then close descriptors. When closing, unlink the socket path only if `lstat` still shows a socket with the recorded device/inode. A newer process may have bound the same path, and its socket must not be deleted. Use a pipe rather than `shutdown()` on the listening socket because `shutdown()` does not wake a blocked `accept()`/`poll()` on macOS. The destructor calls `Stop()`.
7. Use only portable POSIX calls apart from the peer-credential check, so the same code serves Linux, macOS and the BSDs.
8. `sdlmain.cpp` `main()`: after the existing block that sets `opt_promptfolder = 0` for config/`-defaultdir`/etc., add a `C_DOSBOX_AGENT` case: if `opt_promptfolder` is still undecided (< 0) and `opt_agent_config` is non-empty, set it to 0.
9. Python client (`client/python/dosbox_agent/client.py`):
   - Add a `UnixSocketTransport` class with the same interface as `NamedPipeTransport` (`request(payload, timeout_ms, max_message_bytes)`, `close()`).
   - Connect lazily. Like the pipe client, retry on `FileNotFoundError`/`ConnectionRefusedError` every 10 ms until the request timeout, so a server that is still starting is tolerated.
   - Keep a receive buffer, return one line per request, and enforce `max_message_bytes` on both request and response.
   - On timeout, close the connection. A late response would otherwise be read as the answer to the next request. Reconnecting is safe because sessions live in the server.
   - Select the transport class from a `named_pipe`/`unix_socket` map in `AgentClient` and `load_config`. `load_config` requires an absolute endpoint for `unix_socket`.
   - Fix the client test that assumed Windows path separators.
10. Documentation (`docs/rpc-agent-usage.md`): supported platforms, a Linux/macOS config example (`transport=unix_socket`, absolute `endpoint`), the path length limits, stale socket behavior, and launch examples.

**Testing.** New `tests/agent/agent_transport_tests.cpp` (non-Windows only), included from `tests/tests.h`, runs under `dosbox-x -tests`. Use a short `mkdtemp` directory under `/tmp` because of the 104-byte macOS limit. Cases:
- newline-delimited requests are answered and the socket file is removed on stop;
- an oversized request gets one response and then the connection closes;
- a stale socket (bound and closed without unlinking) is replaced, but a live endpoint is refused;
- a regular file at the path is left alone and refused;
- `Stop()` returns while a client is connected but idle;
- endpoints longer than `sun_path` are rejected.

The test's line reader should return a sentinel on close or timeout so a broken transport fails rather than hangs. Python: `client/python/tests/test_client.py` covers the new transport against a socket pair or server.

**Depends on.** Section 1.

### 3. Headless debugger mode (`debugger headless` setting)

(1407b88d6; documented in the reference configs by 4e0f53b8f)

**What.** Add a `[dosbox]` setting, `debugger headless` = `auto` | `true` | `false` (also accepts `1`/`0`; default `auto`, `OnlyAtStart`). In headless mode the debugger works as usual (breakpoints, stepping, commands) but never creates its curses screen or reads the keyboard. Commands come only through the RPC agent or the `mcp_server` control channel, and messages go to the log.

**Why.** On Linux and macOS, `DEBUG_Enable_Handler` refuses to enter the debugger unless stdin, stdout and stderr are all terminals, because the curses UI needs one. An agent-controlled DOSBox-X started by automation has no terminal. The debugger never engaged, so `session.start` timed out waiting for the entry-point stop. The only workaround was wrapping the process in `script` to fake a pseudo-terminal.

**How.**
1. `src/dosbox.cpp` `DOSBOX_SetupConfigSections`: add the string property right after `mcp_server`, with `truefalseautoopt` values and help text for each value. In short: `true` is always headless and ignores the hotkey; `false` is never headless, so Linux/macOS need a terminal; `auto` is headless only when started with `-agent-config` and there is no terminal.
2. `src/debug/debug.cpp`:
   - Add a file-static `debugger_headless` flag. Factor the existing `isatty(0) && isatty(1) && isatty(2)` test into a `DEBUG_HasTerminal()` helper. It returns true on non-Linux/macOS platforms, where the console can be allocated.
   - In `DEBUG_Init`, read the setting. `true`/`1` sets the flag. `auto` sets it only when `C_DOSBOX_AGENT` is compiled in, `control->opt_agent_config` is non-empty, and `DEBUG_HasTerminal()` is false. Without the agent compiled in, `auto` means off. Log one line when headless is on.
   - In `DEBUG_Enable_Handler`, change the Linux/macOS refusal to "not headless and no terminal".
   - `DEBUG_SetupConsole` returns at once when headless, so `dbg.win_main`/`dbg.win_out` stay NULL.
   - `DEBUG_DrawScreen` returns at once when `dbg.win_main` is NULL. This covers both "not set up yet" and headless.
   - In `DEBUG_Loop`, after the agent pump and MCP handling, return 0 instead of calling `DEBUG_CheckKeys()` when headless.
   - Add `DEBUG_Hotkey_Handler(bool)`, which ignores the event when headless and otherwise forwards to `DEBUG_Enable_Handler`. Without a screen, the hotkey would freeze emulation with nothing to show. DEBUGBOX, the menu and the agent keep calling `DEBUG_Enable_Handler` directly, so they still work headless.
3. `src/debug/debug_gui.cpp` `DEBUG_BeginPagedContent`: return early when `dbg.win_out` is NULL. Otherwise paged output (long command results) would wait for a "next page" key that can never be pressed. `DEBUG_ShowMsg` already falls back to stderr/log output when `win_out` is NULL, so no change is needed there, but check that `DEBUG_RefreshPage` is safe with no window.
4. `src/gui/sdlmain.cpp`: declare `DEBUG_Hotkey_Handler` next to `DEBUG_Enable_Handler`, and register the debugger mapper key with `DEBUG_Hotkey_Handler` instead.
5. Update `docs/rpc-agent-usage.md`: automation can launch with stdin from `/dev/null`; use a terminal or `script` only to see the curses UI as well.

**Testing.**
- On Linux, start `dosbox-x --agent-config <cfg> < /dev/null` from a non-tty context (e.g. via `setsid` or a CI job). Confirm the log line appears and that `session.start` stops at the entry point and accepts step/continue/memory requests.
- With `debugger headless=false` the same launch should time out as before.
- From a real terminal with `auto`, the curses debugger must appear as before, and the hotkey must still work.
- With `true`, the hotkey does nothing.

**Depends on.** Section 1 (and section 2 for testing on Linux/macOS).

### 4. Session lifecycle fixes

These four changes build on each other and should be implemented in order. Together they make `session.stop` and natural program exit leave the DOS shell clean for the next `session.start`. Background: the adapter (`DebuggerAdapter::StartTargetAtEntry` in `src/agent/debugger/debugger_adapter.cpp`) launches the target by running `DEBUGBOX <command>` through `first_shell->DoCommand()`. That call returns only when the target has ended. `session.stop` calls `DebuggerAdapter::TerminateTarget` on the emulation thread, then `AgentServer::CompleteTargetTerminationOnEmulationThread` polls (every 10 ms, via `SubmitEmulationCommandAfter`, until `termination_deadline` = now + `request_timeout_ms`) for `IsReadyForTargetStart()`, i.e. the shell being back.

#### 4a. End the CPU trace when `session.stop` terminates the target

(f794f1c00)

**Why.** If a CPU trace (`trace.start`) was still active when the session was stopped, it kept counting instructions in the DOS shell after the target died. When its event count ran out, the debugger broke in and stranded the shell. Either the stop timed out and the session failed, or the stop finished first and the next `session.start` found no ready shell.

**How.** In `TerminateTarget`, under `C_HEAVY_DEBUG`, call `DEBUG_AgentStopTrace()` before calling `DOS_Terminate` if `DEBUG_AgentTraceIsActive()`. Events already collected must stay readable through the trace read methods.

**Testing.** In `client/python/tests/test_e2e.py`, start a short trace (mode `normal`, count 4) just before `session.stop`. Then start and stop a second session on `AGENTFIX.COM` and expect it to stop at `startup`. Without the fix the second start fails.

#### 4b. Report targets that exit by themselves

(041485444)

**What.** Add a bridge notification for "target ended". The server moves the session to `exited` with stop kind `program_exit`, completes pending operations, and removes the session's breakpoints and trace from the debugger.

**Why.** Only `session.stop` ever moved a session to `exited`. A target ending with INT 20h or INT 21h/4Ch left a client waiting on `execution.continue` seeing `running` forever. Also, breakpoints were never removed from the debugger when a session ended, so a second run of the same program stopped on a breakpoint the new session did not own.

**How.**
1. `include/agent/agent_bridge.h` / `src/agent/bridge/agent_bridge.cpp`:
   - Add a `TargetExitListener` (void callback) with `AGENT_SetTargetExitListener` and `AGENT_NotifyTargetExited`, following the existing `DebuggerStopListener` pattern: copy the callback under a mutex, then invoke it outside the lock.
   - Rename the mutex so one mutex serves both listeners.
   - `AGENT_BridgeShutdown` clears both listeners.
2. In `StartTargetAtEntry`, call `AGENT_NotifyTargetExited()` right after the DEBUGBOX `DoCommand()` returns. This happens whether the target exited itself or was terminated. If the target never started, the session is still in `starting` and the server ignores the notification.
3. `AgentServer` (`include/agent/agent_server.h`, `src/agent/server/agent_server.cpp`):
   - Register the listener in `Start` and `StartForTest`, capturing the shared state and lifecycle generation exactly like the debugger stop listener. Clear it on start failure and in `Stop`.
   - The new static `OnTargetExited(impl, generation)` does the following:
     - It takes an `EmulationLease` and returns if it is not active.
     - Under the state mutex, it returns unless the session is `Running` or `Stopped`. Otherwise it notes whether the session trace is active and copies the session's native breakpoints.
     - Outside the lock (it is on the emulation thread), it calls `DeleteBreakpoint` for each one. If the trace was active, it reads the native trace and stops it if it is still running.
     - It relocks, re-checks stopping/session/state, and merges the trace samples. It stops the session trace, clears `breakpoints`, and sets state `Exited` and `last_stop_kind` = `program_exit`. It clears the last-stop breakpoint fields and `pending_stop_kind`, and bumps `state_revision`.
     - It marks every incomplete operation complete with terminal state `Exited` and stop kind `program_exit`, then calls `notify_all` on `state_changed`.
   - In `CompleteTargetTerminationOnEmulationThread`, return early if the session is already `Exited`. When `session.stop` ends the target, DEBUGBOX returns and `OnTargetExited` normally runs first.
4. Document in `docs/rpc-agent-usage.md` that natural exit gives `exited`/`program_exit`, the same as `session.stop`.

**Testing.**
- Unit test `AgentSession.ReportsProgramThatExitsByItself`: start the fixture session, send `execution.continue`, call `AGENT_NotifyTargetExited()` directly, then `execution.wait` must show not running with state `exited` and kind `program_exit`, and `session.status` must show `exited`.
- End-to-end: the second session continues `AGENTFIX.COM` to its own INT 20h and expects `program_exit`. This test caught both bugs.

#### 4c. `RESTART_REQUIRED`: fail `session.start` immediately when a previous target could not be ended

(68777563d)

**What.** New error `RESTART_REQUIRED`, code -32018 (`kErrorRestartRequired`). If `session.stop` could not end its target, every later `session.start` fails immediately with this error until the stuck target ends by itself.

**Why.** When `session.stop` failed, the target could still be holding the shell. The next `session.start` then waited the full `request_timeout_ms` for a shell that would never be ready, and failed with a timeout that did not say why.

**How.**
1. Add a `restart_required` string to `AgentServer::Impl`. It is set under the state mutex by a helper (`RequireRestart`) to a message naming the session id and its `failure_message`.
2. Set it in two places in the `session.stop` path:
   - when `TerminateTarget` returns false;
   - when `CompleteTargetTerminationOnEmulationThread` hits `termination_deadline` ("Timed out waiting for the debugger shell after target termination").

   Do not set it when the failure is only "operation record missing" or "bridge unavailable".
3. In `session.start`, after the `SESSION_BUSY` check and before parameter validation, return `RESTART_REQUIRED` if the string is non-empty.
4. In `OnTargetExited`, clear the string first, under the state mutex, before the session-state early return. If DEBUGBOX has returned, even a target that could not be ended is gone, and the shell can start another program.
5. Python: add `RestartRequiredError` in `errors.py`, map `RESTART_REQUIRED` to it, and export it from `__init__.py`. Add a row to the error tables in both docs.

**Testing.** At the time of this commit the realistic trigger was a parent whose child was running. Section 4d fixes that case, so the final test uses a broken PSP chain instead (see 4d). In both cases the test asserts that the stop ends `failed` and that the next `session.start` raises `RestartRequiredError` in under one second. This phase leaves DOSBox-X stuck, so it must be the last phase of the end-to-end test.

#### 4d. End the target's child programs too

(a20b2283c)

**What.** `TerminateTarget` walks the PSP chain from the current PSP back to the DOS shell's PSP and ends every guest program on it, innermost first. Built-in programs (COMMAND.COM, MOUNT.COM, ...) need special handling. A chain that does not lead back to the shell is refused.

**Why.** `session.stop` called `DOS_Terminate(dos.psp())` only, which ends the innermost program. If the target had started a child (launchers, installers, menus, C runtime `system()` calls), only the child ended. The parent kept running, DEBUGBOX never returned, and the session failed.

**How.**
1. Add `bool PROGRAMS_IsBuiltinProgram(uint16_t psp)` to `include/programs.h` / `src/misc/programs.cpp`. It returns true if the bytes at `psp:0100` match `exe_block` exactly, with the two bytes at `CB_POS` replaced by the `call_program` callback number. This is the stub `PROGRAMS_MakeFile` writes for every built-in program, so it identifies them exactly.
2. In `TerminateTarget` (under `C_DEBUG`), build the chain:
   - Start at `dos.psp()` and follow `DOS_PSP(psp).GetParent()` until reaching `DOS_ShellGetPSP()`.
   - Refuse with the error "The running program's PSP chain does not lead back to the DOS shell" if the chain exceeds 64 entries, if a PSP does not start with `CD 20` (INT 20h), or if a PSP repeats (a loop).
3. Find the first built-in program in the chain, counting from the innermost. Only the programs above it are ended now, innermost first, with one `DOS_Terminate` per PSP.
   - Each EXEC saved its caller's stack in the caller's PSP, so terminating one level at a time unwinds correctly.
   - A built-in program runs as native code inside its own nested emulation loop and cannot be unwound by `DOS_Terminate` from outside. Once its child is gone, it finishes by itself (e.g. `COMMAND /C` returns).
4. If at least one program was ended, do the existing post-termination fixups once, after the loop:
   - clear `dos_program_running`, `appname` and `appargs`;
   - set AX to 0x3E01;
   - pop the IRET frame `DOS_Terminate` set up (IP, CS, FLAGS from SS:SP, SP += 6).

   If nothing was ended (a built-in program is innermost), skip the fixups. In both cases end the trace (4a) and call `DEBUG_AgentResumeAfterTerminate()` as before.
5. In `CompleteTargetTerminationOnEmulationThread`, each retry while the shell is not ready now also calls `TerminateTarget` again. This ends the outer programs once the built-in one has returned. If that call fails, finish the operation as failed and require a restart.
6. Factor the "fail the stop" sequence into a helper (`FailTermination`): set state `Failed`, `failure_message`, `RequireRestart`, `last_stop_kind` `fault`, and complete the operation with `Failed`/`fault`. Use it for both the deadline and the refusal.
7. Documentation: describe nested stop in the usage guide, and update the `RESTART_REQUIRED` description (broken PSP chain, or not ended before the timeout).

**Testing.** New fixtures in `tests/agent/fixtures/`. The `.asm` files are NASM, and `build_fixture.ps1` writes the `.com` files and copies them into the runtime directory.
- `agent_nested` / `AGNEST.COM`: shrinks its memory block, runs `AGENTRUN.COM` (an endless loop) with INT 21h/4B00h, then loops itself.
- `agent_nested_command` / `AGNESTC.COM`: the same, but runs `Z:\COMMAND.COM /C AGENTRUN.COM`.
- `agent_bad_psp` / `AGBADPSP.COM`: builds a fake PSP (starts with `CD 20`, parent field at offset 0x16 pointing to itself), makes it current with INT 21h/50h, and spins.

In `test_e2e.py`, a helper starts a target, continues, sleeps 0.5 s and pauses. For `AGNEST.COM` and `AGNESTC.COM`, `session.stop` must end in `exited`/`program_exit`. For `AGBADPSP.COM`, the stop must end `failed`, and the next `session.start` must raise `RestartRequiredError` within one second.

**Depends on.** 4a–4c.

### 5. `-nopromptquit` command-line option

(8f847bb75)

**What.** With `-nopromptquit`, DOSBox-X quits immediately when its window is closed or it is told to quit (SIGTERM etc.), whatever is running. It ignores `quit warning` and `allow quit after warning`.

**Why.** For automated/headless testing (agent sessions, scripted DOS and Windows runs) the quit prompt kept DOSBox-X from exiting when the harness closed it.

**How.**
1. Add `opt_nopromptquit` (default false) to the `Config` options in `include/control.h`.
2. Parse `nopromptquit` in `DOSBOX_parse_argv()` in `src/gui/sdlmain.cpp`.
3. Add a line to the `-help` text there.
4. At the top of `CheckQuit()`, return true when it is set, before the settings are read.
5. Document it in `contrib/linux/dosbox-x.1`, `contrib/linux/dosbox-x.asciidoc`, and the bash completion list in `contrib/linux/dosbox-x`.

**Testing.** Run a DOS program with `quit warning=true`, send SIGTERM or close the window, and confirm it exits at once with the option and prompts without it.

### 6. Keep host tick handlers out of save states

(dc7554049; implement before section 7)

**What.** The PIC save state no longer saves tick handlers that are not in `pic_state_timer_table`, and keeps them across a load instead of replacing them.

**Why.** The PIC save state (`src/hardware/pic.cpp`) stores the tick handler list as indexes into `pic_state_timer_table`. A handler not in the table, such as the debugger's `ControlServer_Poll` registered when `mcp_server` is set, was saved as 0xFFFF. `PIC_State_IndexTimer(0xFFFF)` returned 0, so the restored list contained a NULL handler and the first tick after loading crashed with SIGSEGV in `TIMER_AddTick`. Section 7 makes loading from that very channel possible, so this fix must come first.

**How.**
1. `PIC_State_FindTimer` still returns 0xFFFF for unknown handlers. Comment that these are host pollers (network pollers, `mcp_server` poll).
2. `PIC_State_IndexTimer` returns 0 for any index at or beyond the table size, not just 0xFFFF.
3. Save (`getBytes`): count and write only handlers whose `PIC_State_FindTimer` result is not 0xFFFF.
4. Load (`setBytes`):
   - Initialize the count to 0.
   - While freeing the current list, move host handlers (index 0xFFFF) to a separate list, preserving their order.
   - Read the saved indexes, stopping if the stream fails. Skip any that map to NULL; this handles old states that contain 0xFFFF entries.
   - Build the machine handlers in saved order and append the host list after them.

   The format does not change.

**Testing.** Start with `mcp_server` set to a port. Save a state, load it, and confirm emulation continues with no crash and the control channel still answers `PING`. Also load a state saved by an unfixed build with `mcp_server` set; its 0xFFFF entry must be skipped.

### 7. Debugger `SAVESTATE` and `LOADSTATE` commands

(a2b3f71a6)

**What.** New debugger commands `SAVESTATE [slot|file]` and `LOADSTATE [slot|file]`. They work when typed and through the `mcp_server` control channel (`REQ <id> EXEC SAVESTATE 3`). They save or load without any dialog and report the outcome as the command's response.

**Why.** A front end driving the debugger remotely could save and load states only through the menu or a mapper shortcut. Both need someone at the window and report problems through message boxes.

**How.**
1. Argument parsing in `ParseCommand` (`src/debug/debug.cpp`):
   - `ParseCommand` upper-cases its copy of the line, which breaks file names on case-sensitive file systems. Add a helper (`RawCommandArgument`) that returns everything after the command word from the original string, trimmed. Spaces inside are allowed and quotes are not stripped.
   - An all-digits argument is a slot 1..`SLOT_COUNT*MAX_PAGE` (100). Out of range is an error.
   - Anything else is a file name.
   - No argument means the current slot (`GetGameState_Run()`), even when the menu is set to save to a file.
2. File name rules:
   - The name must end in lowercase `.sav` and contain `.sav` nowhere else. FLAGSAVE's flagged files are stored beside the state under the same name with the first `.sav` replaced by `.dat`, so a name without `.sav` would overwrite the state itself, and an earlier `.sav` would put them somewhere else.
   - The name must be shorter than `CROSS_LEN`, because it is copied into such a buffer.
3. Refuse with a message and an error return in two cases:
   - when the debugger is not stopped (`!debugging || debug_running`);
   - when called from a timer tick.

   For the second check, add `bool TIMER_InTickHandlers` (declared in `include/timer.h`, defined in `pic.cpp`) and set it with a scope guard around the tick handler loop in `TIMER_AddTick`. The `mcp_server` channel is served from a tick handler while the guest runs (e.g. a batch with `BREAK` followed by `LOADSTATE`). A load there would replace the tick handler list that `TIMER_AddTick` is walking, since it holds the next pointer. The client can resend once the stop is reached.
4. `src/misc/savestates.cpp`:
   - Add a file-static `unattended_save_state` flag. While it is set:
     - `notifyError` logs instead of showing a message box;
     - `loadstateconfirm` answers "no", so a version/program/memory/machine mismatch is refused unless `forceloadstate` is set (the existing force path bypasses the confirm);
     - `SaveState::save` does not ask for a remark (`tinyfd_inputBox`).
   - Change `SaveState::save` and `SaveState::load` (declared in `include/dosbox.h`) to return `bool`: false on every early exit and `!save_err`/`!load_err` at the end. Existing callers can ignore the result.
   - Add `SaveState_Unattended(bool load, size_t slot, const std::string& file)`:
     - Save `unattended_save_state`, `use_save_file` and `savefilename`, and restore them on every exit path, exceptions included (an RAII guard).
     - Set `use_save_file`/`savefilename` from `file`, set the flag, and call save or load.
     - Catch `SaveState::Error` and log it.
     - After a successful slot save, call `refresh_slots()` so the menu updates. After a successful load with TTF output active, call `resetFontSize()`.
5. Command response: on success `DEBUG: Saved slot N` / `DEBUG: Loaded <file>`. On failure `DEBUG: SAVESTATE failed: <target> (the log says why)` with a false return, which makes the control channel answer ERR. After a load, refresh the debugger view the way F10/F11 do: `DrawRegistersUpdateOld`, `SetCodeWinStart`, `DEBUG_DrawScreen`.
6. Add both commands to the `HELP` output and to `README.debugger`, both the command table and a "Save state commands" section. That section should say that a load which fails part-way (a state missing a part) can leave the machine partly restored, as the menu's load can.

**Testing.**
- With `mcp_server` set, connect to the control channel and `BREAK`, then `EXEC SAVESTATE 3`, modify memory, `EXEC LOADSTATE 3`, and verify the memory is restored and the response text matches.
- Try a mixed-case absolute `.sav` path on Linux.
- Error cases: running target, slot 0 and 101, `foo.bin`, `a.sav.sav`, and loading a state from a different machine type without `forceloadstate`.
- No dialog should appear in any of these.

**Depends on.** Section 6 (loading with `mcp_server` active otherwise crashes).

### 8. Console monitor: honour `monitor socket`, local connections only, crash fixes

(e5012aa7d; the reference config entries were regenerated by cfaed7050 and 4e0f53b8f)

**What.** Fixes to the `C_MONITOR` console monitor (`src/monitor/monitor.h`, `sdlmain.cpp`, setting `monitor socket` in `src/dosbox.cpp`):
- The setting is now honoured.
- Only loopback TCP clients are accepted.
- Two crashes are fixed.

**Why.** Four bugs on master:
- **The setting is ignored.** In `sdlmain.cpp`, the condition is `!mon_sock.empty() || mon_sock.compare("off")`, which is always true, so the monitor always starts. In `Monitor::OpenMonitor`, `tLocation.compare("on")` is non-zero for anything except "on", so every value was replaced with "4444". The net effect: the monitor always listened on TCP port 4444 on all interfaces, whatever the setting said. On Windows the TCP branch was also guarded by `false == isPort`.
- **Remote access.** SDL_net cannot bind a server to 127.0.0.1, so the monitor was reachable from other machines.
- **Crash when the port is busy.** If the TCP port could not be opened (for example, a second instance), the listen thread still started and called `SDLNet_TCP_Accept()` on a NULL server.
- **Unix socket server unusable.** It called `accept()` and `send()` on file descriptor 1, because `socket_t` converts to `int` through its `operator bool`.

**How.**
1. `sdlmain.cpp`: open the monitor only if the value is non-empty and not `off`.
2. `OpenMonitor` setting rules:
   - `on` becomes `4444`.
   - A non-empty all-digit value is a port. Parse it with `strtoul` and reject 0 or anything above 65535 with a log message.
   - On Linux, any other value is a Unix socket path, handled by the existing UDS server (`CB_ListenUDP`).
   - Elsewhere, a non-numeric value logs "a socket path is only supported on Linux".
   - Cast to `unsigned char` before `isdigit`.
3. Create the listener thread and set `_isRunning` only if a server was actually started (the `run` function pointer is non-NULL). In the TCP server start path, log the `SDLNet_GetError()` text and return failure when the server socket is NULL.
4. Add an `isLoopbackPeer` check:
   - Read `SDLNet_TCP_GetPeerAddress` and accept only addresses whose first octet is 127.
   - Otherwise log the refused address, close the socket, and keep accepting.
   - Apply it right after `SDLNet_TCP_Accept` in the TCP accept loop.
5. In the Linux UDS paths, use the descriptor member (`_server.sock.uds`, `tClient.sock.uds`) for `accept()` and `send()`, not the `socket_t` object.
6. Update the help text for `monitor socket`: empty or `off` = no monitor; otherwise a port number, `on` (4444), or a Linux socket path; TCP accepts only local connections. Fix the "can be use" typo.

**Testing.**
- With `monitor socket` empty, nothing should listen on 4444 (check with `ss -ltn`).
- With `on`, port 4444 should listen; connecting from 127.0.0.1 works, and a connection from another host is closed and logged.
- A second instance with the same port logs the error and does not crash.
- On Linux, `/tmp/dosbox-x.sock` should accept `socat - UNIX-CONNECT:` and show the banner.

### 9. Windows 3.1 framebuffer publication service (integration device)

(6acb82287, bd8cc4967; external author, see the authorship note at the top)

**What.** An optional, negotiated service on the DOSBox integration device. A cooperating Windows 3.1 display driver hands over a complete 8 bpp frame, plus its software cursor. DOSBox-X copies it synchronously and writes it into VRAM between VGA frames, so scanout never shows the driver's intermediate redraws. It is inactive until a guest submits a frame, and it does not change the emulated VGA programming model. `docs/windows31-frame-publication.md` in the commit is the protocol specification and should be reproduced in substance.

**Why.** A driver drawing directly into the linear framebuffer shows partial redraws (e.g. large text redraws) during scanout. Publishing whole frames at frame boundaries avoids that tearing.

**How.**
1. Registers. Add to `include/iglib.h`:

   | Register | Name | Write | Read |
   | --- | --- | --- | --- |
   | 0x00684582 | `DOSBOX_ID_REG_WIN31_PUBLISH_CAPS` | – | Capability value 0x57333101 |
   | 0x00684583 | `..._SUBMIT` | Guest linear address of a descriptor | Status of the previous submit |
   | 0x00684584 | `..._CANCEL` | Zero, to cancel | Status of the previous cancel (0) |

   In `src/ints/bios.cpp`:
   - The read path returns the capability value for CAPS, and leaves the data register untouched for SUBMIT/CANCEL. A result read must not submit or cancel again.
   - The write path calls `VGA_Win31PublishSubmit(value)` and stores the returned status in the data register, or calls `VGA_Win31PublishCancel()` and stores 0.

   Clients are expected to use the existing PUSH_STATE/POP_STATE device commands (0x20/0x21).
2. Pure helpers go in a new header `include/win31_publish.h` (namespace `Win31Publish`, added to `include/Makefile.am`) so they can be unit-tested without the emulator:
   - **Range check.** A size-nonzero 32-bit range check (`range`) that rejects ranges which do not fit below 4 GiB.
   - **Shape.** A `Shape` with width/height/stride/bytes. It is valid when width and height are non-zero, width ≤ stride, stride × height = bytes, and bytes ≤ 16 MiB.
   - **Descriptor check.** Ten little-endian DWORDs: size 40, version 1, back buffer linear address, reserved 0, cursor linear address, byte size, stride, width, height, bpp 8. The shape must be valid, and the buffer and cursor ranges must fit.
   - **Cursor format.** The cursor block is 16 + 16384 + 2 bytes: the existing Windows VBE driver's shared cursor structure. A flags WORD after the mask workspace has bit 2 = visible. The WORDs at offsets 4/6/8/10 are clipped width, height, source x and source y. The DWORD at offset 12 is the framebuffer byte offset. The mask is 32×32 entries of interleaved AND/XOR byte pairs.
   - **Cursor composition** (`compose`). Hidden or zero-size cursors are a no-op. Width, height and source rectangle must stay within 32×32, and the destination must lie within the frame width and height. Each pixel becomes (pixel AND mask) XOR xor-mask.
   - **Pending snapshot.** A `Pending` struct holds the pixels, shape and a generation number, with `cancel()` and `matches(shape, generation)`.
3. Service in `src/hardware/vga_draw.cpp`. Keep file-static state: a pending snapshot, a generation counter, and a log-once flag.
   - **`VGA_Win31PublishCancel`** clears the pending pixels and increments the generation.
   - **Current mode check.** Supported only for `M_LIN8` with `vga.mem.linear` present, not resizing, and zero `display_start`, panning and `bytes_skip`. Width and height come from `vga.draw`, and stride is `vga.draw.address_add`. Bytes must not exceed VRAM size or 16 MiB.
   - **Safe guest RAM read.** Translate page by page with `PAGING_MakePhysPage` and check that the page is below `MEM_TotalPages()`.
     - The page handler flags must be readable and not ROM, and must be either writeable or has-code. The dynamic core's code-cache wrappers clear WRITEABLE, but the backing is still RAM.
     - The host read pointer must equal `MemBase + page*4096`. This rejects MMIO and device-backed pages even if they advertise host storage.
     - Copy with `memcpy`. Never call a handler's read methods.
   - **Submit.** Steps, with the status returned on failure:
     1. The caller must be in protected mode, at CPL 0, and not in V86 mode, and the current mode must be supported. Otherwise status 1.
     2. Read the 40-byte descriptor. Status 3 on a bad read.
     3. Validate the descriptor, and require its shape to equal the current shape. Status 2 on mismatch.
     4. Copy the back buffer and the cursor block into a new `Pending` stamped with the current generation. Status 3 on a bad read; also return 3 if `std::bad_alloc` is caught.
     5. Compose the cursor. Status 4 on a bounds error.
     6. Replace the pending snapshot and log once. Return 0.

     Any non-zero result also cancels, so an older pending frame cannot later be drawn over the driver's fallback path. Never keep guest pointers and never write guest memory.
   - **`VGA_Win31PublishCommit`.** Called in `VGA_VerticalTimer` right after `RENDER_EndUpdate`, when the previous scanout has ended. If something is pending and the current mode still matches its shape and generation, copy it into `vga.mem.linear` and set `vga.draw.must_complete_frame`. Always clear the pending snapshot afterwards.
   - **Cancel hooks.** Call `VGA_Win31PublishCancel()` in `VGA_SetModeNow`, `VGA_SetMode` (only when the mode actually changes), `VGA_StartResize`, `VGA_Reset`, and at the start of `POD_Load_VGA_Draw`. Pending host pixels belong to the guest state before the load.
4. Declare `VGA_Win31PublishSubmit`/`Cancel`/`Commit` in `include/vga.h`.
5. Client obligations, to document:
   - Cancelling keeps the last committed frame on screen.
   - The service does not update the driver's front-buffer shadow, so a driver returning to its normal copy path must invalidate it.
   - Drivers must not submit during an incomplete drawing operation, must keep their fallback when the capability is absent or a submit fails, and must track which submissions can be revoked so that starting new work does not keep cancelling frames before they are shown.

**Testing.**
- Helper tests (`tests/win31_publish_tests.cpp`): invalid ranges and shapes; descriptor version, geometry and address validation; cursor composition (hidden, invert, replace, each out-of-bounds direction, fully clipped); snapshot ownership and rejection after a generation or shape change.
- Service tests (`tests/win31_publish_service_tests.cpp`), which set up `M_LIN8` and RAM at 4 MiB:
  - a descriptor that crosses a page boundary is accepted, VRAM is unchanged until commit, and a snapshot commits only once;
  - a newer submit replaces the pending one;
  - a failed submit and an explicit cancel both discard pending pixels while keeping the committed frame;
  - a geometry change or resize discards pending pixels, and returning to the old geometry does not revive them;
  - a save-state load discards them;
  - unsupported context or mode returns 1;
  - bad descriptor, memory or cursor returns 2/3/4, and the BIOS ROM at 0xF0000 is rejected as not RAM.

  Both test files are included from `tests/tests.h` and run under `dosbox-x -tests`.
- A full guest test needs a cooperating driver, which is not part of these commits. With one: compare large text redraws at the same resolution and CPU budget, check cursor clipping, run continuous DCI drawing, exit and restart Windows, and change modes or load a state while frames are pending.

### 10. Reference configuration files

The reference configs are generated from the settings' help text, so regenerate them after implementing sections 3 and 8; do not edit them by hand. The relevant parts of 1407b88d6, cfaed7050 and 4e0f53b8f are:
- `debugger headless` appears in `dosbox-x.reference.full.conf` and in `contrib/windows/installer/dosbox-x.reference.setup.conf` (as an advanced `#DOSBOX-X-ADV:` entry), with its description and per-value help. It is added to the "Advanced options" list in `dosbox-x.reference.conf`.
- The stale `enable monitor` / `monitor port` / `monitor sock` entries are replaced by `monitor socket` with the new help text from section 8, and `monitor socket` is added to the advanced options list.

The other changes in those regeneration commits, and all of 2747c1638 (`utf8 file names`), belong to other feature groups.

## Wayland video driver for the in-tree SDL 1.x (vs/sdl)

Commits on main-ai: c5beb2338 (the driver), 0778d87cd (fixes from testing on Weston, including DOSBox-X side changes), 57f21c20d (no black flash on resize).

SDL1 builds of DOSBox-X can only run through XWayland on a Wayland desktop, and on a desktop without XWayland SDL 1.x has no usable driver at all. This work adds a native Wayland video driver to the in-tree SDL 1.2 under `vs/sdl/src/video/wayland/`. It supports the three ways DOSBox-X presents output: software surfaces (output=surface), OpenGL through EGL, and SDL_VULKAN windows (the Vulkan surface is created by the application). It also handles keyboard input through xkbcommon, pointer input, cursors, pointer lock for mouse capture, decorations, fullscreen and compositor-driven resizing.

X11 stays the default. Wayland is chosen first only when `XDG_SESSION_TYPE` is `wayland`, and X11 is still used whenever Wayland is not available. SDL2 builds are not affected.

The first commit was written without a compositor to test on. The second commit contains what testing on Weston 13 (X11 backend nested in Xephyr) found: surface, OpenGL and Vulkan output, keyboard, menus, mapper, mouse capture, fullscreen, switching output at runtime, with and without libdecor. The third commit was tested on headless Weston with mode 13h / text mode switches.

New files: `SDL_waylandvideo.c/.h` (device, registry, window, shm presentation, window manager functions), `SDL_waylandevents.c` / `SDL_waylandevents_c.h` (dispatch, seat, keyboard, pointer, relative pointer), `SDL_waylandmouse.c` / `SDL_waylandmouse_c.h` (cursors, grab mode), `SDL_waylandgl.c` / `SDL_waylandgl_c.h` (EGL), `SDL_waylandvulkan.c` / `SDL_waylandvulkan_c.h` (VK_KHR_wayland_surface), `SDL_waylanddecor.c/.h` (libdecor loaded at runtime), and `protocols/` (protocol XML and generated code).

**Depends on:**
- The Vulkan section (in-tree SDL 1.x Vulkan support, commit afbaa7038). It provides the `SDL_VULKAN` flag, `SDL_vulkan_c.h`, `SDL_Vulkan_CopyExtensions`, `SDL_Vulkan_GetInstanceProc`, `SDL_Vulkan_NotifyWindowDestroy`, and the `Vulkan_GetInstanceExtensions` / `Vulkan_CreateSurface` members of `SDL_VideoDevice`. The driver as written sets those members unconditionally, so either do the Vulkan section first or leave the Vulkan pieces out and add them later.
- The 3dfx OpenGL render-to-texture section (commit 00fd06f7a) later adds `Wayland_GL_CreateSharedContext`, `Wayland_GL_DeleteContext`, `Wayland_GL_MakeContextCurrent` and `Wayland_GL_GetVideoModeContext` to this driver's EGL code. That belongs to that section. Note only that it builds on the EGL code described here.

### Build and configure

**What:** A `CheckWayland` function in `vs/sdl/configure.ac`, three new config defines, and the matching hand edits to the generated `vs/sdl/configure`.

**Why:** The driver has to be optional. It is built only when its libraries are present, and EGL and libdecor are further optional layers on top.

**How:**
1. Add `CheckWayland()` to `configure.ac` next to `CheckX11`, and call it in the Linux host case right after `CheckX11`.
2. Options: `--enable-video-wayland` (default yes) and `--enable-video-wayland-libdecor` (default yes). Only check when `enable_video` and `enable_video_wayland` are both yes.
3. Use pkg-config. The requirements are `wayland-client` at least version 1.20, plus `wayland-cursor` and `xkbcommon`. If they are found, take cflags and libs from all three, define `SDL_VIDEO_DRIVER_WAYLAND`, and add `src/video/wayland/*.c` and `src/video/wayland/protocols/*.c` to `SOURCES`.
4. EGL: if `enable_video_opengl` is yes and pkg-config has `wayland-egl` and `egl`, add their cflags and libs, and define `SDL_VIDEO_DRIVER_WAYLAND_EGL` and `SDL_VIDEO_OPENGL`.
5. libdecor: if enabled and `libdecor-0` exists, add only its cflags (not its libs) and define `SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR`. Only the header is used at build time. The library is opened at runtime, so it never becomes a link dependency.
6. Append the collected flags to `EXTRA_CFLAGS` / `EXTRA_LDFLAGS` and set `have_video=yes`.
7. Add `#undef SDL_VIDEO_DRIVER_WAYLAND`, `SDL_VIDEO_DRIVER_WAYLAND_EGL` and `SDL_VIDEO_DRIVER_WAYLAND_LIBDECOR` to `vs/sdl/include/SDL_config.h.in`.
8. The generated `vs/sdl/configure` was edited by hand to match: the help text for both options, and the expanded shell function. This follows the existing practice for this tree. Regenerating it with autoconf is the alternative, if the maintainers prefer that.

**Testing:** Configure on a system with and without the Wayland development packages, with `--disable-video-wayland`, and without `wayland-egl` (the driver should build without OpenGL). Check `SDL_config.h` for the defines.

### Generated protocol code (protocols/)

**What:** `protocols/` holds four protocol XML files copied from wayland-protocols: `xdg-shell.xml`, `xdg-decoration-unstable-v1.xml`, `relative-pointer-unstable-v1.xml` and `pointer-constraints-unstable-v1.xml`. For each one it also holds the `*-client-protocol.h` and `*-protocol.c` that wayland-scanner generates from it, plus `regen.sh`.

**Why:** Keeping the generated files in the tree means a build does not need wayland-scanner or the wayland-protocols package.

**How:** These files are generated, not written. `regen.sh` changes to its own directory and, for each `*.xml`, runs `wayland-scanner client-header` to produce `<name>-client-protocol.h` and `wayland-scanner private-code` to produce `<name>-protocol.c`. To re-implement this, copy the XML files from a wayland-protocols release, write the equivalent script, and run it. Do not edit the output by hand. The driver includes the four client headers from `SDL_waylandvideo.h`. configure's `SOURCES` glob picks up the `.c` files. `private-code` (rather than `public-code`) keeps the interface symbols hidden inside the SDL library.

### Driver selection (SDL_video.c, SDL_sysvideo.h, sdlmain.cpp)

**What:** The driver is registered in the bootstrap list, and selection prefers it in a Wayland session.

**Why:** If the driver were placed first in the list, it would take over everywhere a Wayland socket happens to exist. Placing it after X11 keeps existing behaviour unless the desktop session really is Wayland.

**How:**
1. `SDL_sysvideo.h`: declare `extern VideoBootStrap WAYLAND_bootstrap` under `SDL_VIDEO_DRIVER_WAYLAND`.
2. `SDL_video.c`: add `&WAYLAND_bootstrap` to `bootstrap[]` immediately after the X11 entry.
3. In `SDL_VideoInit`, when no driver name was given: if `XDG_SESSION_TYPE` equals `wayland`, first look for the bootstrap entry named `wayland` and try only that one (`available()` then `create()`). If that produces no device, fall through to the normal loop over all entries. The existing `sdl1_force_x11` logic only disables fbcon and svga, so it does not need to change. `SDL_VIDEODRIVER=wayland` still works through the existing named-driver path.
4. The bootstrap `Wayland_Available` returns false unless `WAYLAND_DISPLAY` or `WAYLAND_SOCKET` is set, or `SDL_VIDEODRIVER` is exactly `wayland`. Without either variable, `wl_display_connect(NULL)` would silently try `wayland-0`. Otherwise it connects and disconnects once to prove the compositor is there.
5. `src/gui/sdlmain.cpp` `main()`: on Linux, DOSBox-X puts `SDL_VIDEODRIVER=x11` into the environment when the variable is unset. For SDL1 builds (`!C_SDL2`), skip that when `XDG_SESSION_TYPE` is `wayland`, so that the SDL-side preference can take effect. SDL2 builds keep the old behaviour.

**Testing:** In an X11 session, the X11 driver is used as before. In a Wayland session with XWayland, the Wayland driver is used. With `SDL_VIDEODRIVER=x11`, X11 is forced. On a Wayland session with no compositor reachable, selection falls back to X11.

### Device and private data

**What:** `Wayland_CreateDevice` allocates the `SDL_VideoDevice`, its `hidden` (`struct SDL_PrivateVideoData`) and its `gl_data` (`struct SDL_PrivateGLData`), and fills in the function pointers.

**How / notes:**
- The private struct (in `SDL_waylandvideo.h`) holds:
  - the display, registry and bound globals, with the compositor and seat versions;
  - the first output's current mode (`output_w/h`);
  - the seat, pointer, keyboard, relative/locked pointer state, and xkb context/keymap/state;
  - cursor surface, theme and current cursor;
  - window objects (`wl_surface`, `xdg_surface`, `xdg_toplevel`, decoration, libdecor context and frame);
  - `configured`, `content_mode` (`WAYLAND_MODE_SHM`, `_OPENGL`, `_VULKAN`), the window size, the compositor-suggested `configure_w/h`, `fullscreen`, `resizable` and `app_id`;
  - the shm buffer array (`WAYLAND_MAX_BUFFERS` = 4) with the framebuffer and `present_pending`;
  - the `wl_egl_window`.
- Hooks that are set: VideoInit, ListModes, SetVideoMode, ToggleFullScreen, UpdateMouse, SetColors, UpdateRects, VideoQuit, the HW surface stubs, the GL hooks (only with EGL), the Vulkan hooks, SetCaption, IconifyWindow, GrabInput, GetWMInfo (only when the X11 driver is compiled in, see below), the cursor hooks, CheckMouseMode, InitOSKeymap and PumpEvents.
- Hooks that stay NULL: `SetIcon`, `CreateYUVOverlay`, the HW blit/fill/colorkey/alpha hooks and `FlipHWSurface`. `WarpWMCursor` is also NULL on purpose: Wayland clients cannot warp the pointer, and SDL then just posts the motion itself.
- HW surfaces are refused (`AllocHWSurface` returns -1). Lock and unlock do nothing.

### Display connection, registry and globals

**What:** `Wayland_VideoInit` connects to the compositor, binds the globals, and sets the reported pixel format.

**How:**
1. `wl_display_connect(NULL)`, `wl_display_get_registry`, then add the registry listener.
2. Call `wl_display_roundtrip` twice. The first roundtrip delivers the `global` events. The second delivers the initial events of the objects bound during the first: `wl_output.mode` and the `wl_seat` capabilities.
3. In `registry_global`, bind by interface name:
   - `wl_compositor` at min(version, 4). Version 4 is needed for `wl_surface.damage_buffer`.
   - `wl_shm` at version 1.
   - `xdg_wm_base` at version 1, with a listener that answers `ping` with `xdg_wm_base_pong`. Without the pong, compositors mark the client as unresponsive.
   - `zxdg_decoration_manager_v1`, `zwp_relative_pointer_manager_v1` and `zwp_pointer_constraints_v1`, all at version 1 and all optional.
   - The first `wl_seat`, at min(version, 5), followed by `Wayland_AddSeat`.
   - The first `wl_output` with version 2 or higher, bound at version 2. Its listener records `mode` events that carry `WL_OUTPUT_MODE_CURRENT`. Only this first output is used, as the "desktop" size.

   `global_remove` is ignored.
4. Fail with "does not support xdg-shell" if the compositor, shm or `xdg_wm_base` global is missing.
5. Create the `xkb_context`.
6. If there is no decoration manager and libdecor loads (see below), create a libdecor context with `libdecor_new`. Its `error` callback just sets the SDL error.
7. The app_id (the Wayland counterpart of WM_CLASS) is chosen in this order: `SDL_VIDEO_WAYLAND_WMCLASS`, then `SDL_VIDEO_X11_WMCLASS`, then glibc's `program_invocation_short_name`, then `SDL_App`.
8. Report the video format as 32 bpp with masks 0x00FF0000 / 0x0000FF00 / 0x000000FF, which matches `WL_SHM_FORMAT_XRGB8888`. SDL emulates other depths with a shadow surface, so `SetColors` is a no-op that returns success.
9. Set `info.wm_available = 1` and `info.current_w/h` from the output mode. With EGL, set `gl_config.driver_loaded = 1`, because EGL is linked in and there is nothing to load.

### Window: xdg-shell toplevel and decorations

**What:** `Wayland_CreateWindow(content_mode)` and `Wayland_DestroyWindow` in `SDL_waylandvideo.c`, plus `SDL_waylanddecor.c/.h`.

**Why:** xdg-shell is the standard desktop shell protocol. Many compositors (GNOME, Weston) draw no server-side decorations, so without client-side decorations the window would have no title bar. libdecor provides those decorations. It is loaded at runtime so it is never a hard dependency.

**How (create):**
1. `wl_compositor_create_surface`. Reset `configured` and `configure_w/h`.
2. If a libdecor context exists, call `libdecor_decorate(libdecor, surface, &frame_iface, this)`. If that returns NULL, unref the libdecor context and continue without it. On success, call `libdecor_frame_set_title`, `libdecor_frame_set_app_id` and `libdecor_frame_map`. libdecor then creates the xdg_surface and toplevel itself.
3. Otherwise:
   1. `xdg_wm_base_get_xdg_surface`, with a listener whose `configure` calls `xdg_surface_ack_configure`, sets `configured`, and calls `Wayland_HandleConfigureSize`.
   2. `xdg_surface_get_toplevel`, with a listener for `configure` (stores the width and height) and `close` (calls `SDL_PrivateQuit`).
   3. `xdg_toplevel_set_title` and `xdg_toplevel_set_app_id`.
   4. If the decoration manager exists, `zxdg_decoration_manager_v1_get_toplevel_decoration` followed by `set_mode(SERVER_SIDE)`.
   5. Commit the surface with no buffer attached. This initial commit is what makes the compositor send the first configure.
4. Wait until `configured` is set by calling `Wayland_Dispatch(this, 1)` in a loop. xdg-shell forbids attaching a buffer before the first configure is acked. If the connection breaks, destroy the window and fail.
5. Call `Wayland_UpdateRelativeMode`, because a pointer lock belongs to a surface and must be re-created for a new one.

**libdecor frame interface:**
- `configure`: read the suggested content size with `libdecor_configuration_get_content_size`, storing 0 if there is none. Then commit a `libdecor_state` with the application's current window size (640x480 if there is none yet). The content stays at the size the application chose until the application itself resizes. Then set `configured` and call `Wayland_HandleConfigureSize`.
- `close`: `SDL_PrivateQuit`.
- `commit`: `wl_surface_commit`.
- `dismiss_popup`: empty.

**libdecor loading (SDL_waylanddecor.c/.h):** One X-macro list, `SDL_LIBDECOR_FUNCS`, names the functions used. From it the code generates the function pointer typedefs, `extern` declarations, definitions, a load loop and an unload loop. The functions are: `libdecor_new`, `_unref`, `_dispatch`, `_decorate`; `libdecor_frame_unref`, `_set_title`, `_set_app_id`, `_set_capabilities`, `_unset_capabilities`, `_set_min_content_size`, `_set_max_content_size`, `_set_fullscreen`, `_unset_fullscreen`, `_set_minimized`, `_set_visibility`, `_map`, `_commit`, `_get_xdg_toplevel`; `libdecor_state_new` and `_free`; `libdecor_configuration_get_content_size` and `_get_window_state`.

`Wayland_LoadLibdecor` opens `libdecor-0.so.0` with `SDL_LoadObject`, or the name in `SDL_VIDEO_WAYLAND_LIBDECOR` if that is set. It resolves every function, and if any one is missing it unloads the library and reports failure. `Wayland_UnloadLibdecor` closes the library and clears the pointers.

**How (destroy):** Order matters:
1. If the content mode is Vulkan, call `SDL_Vulkan_NotifyWindowDestroy()` first. The application must destroy its VkSurfaceKHR before the `wl_surface` goes away.
2. Tear down the EGL window.
3. Release the relative pointer and the lock.
4. Destroy the shm buffers and free the framebuffer (clearing `screen->pixels`).
5. Destroy the toplevel decoration, then the xdg_toplevel, then the xdg_surface, or unref the libdecor frame instead.
6. `wl_surface_destroy`, clear `configured` and `pointer_focus`, then flush.

**Testing:** Run on a compositor that has xdg-decoration (KDE, wlroots-based) and on one without it (GNOME, Weston), with libdecor installed and with libdecor made unavailable. Setting `SDL_VIDEO_WAYLAND_LIBDECOR` to a name that does not exist is an easy way to disable it. Check that the window gets a title bar and that the close button quits.

### Window state: title, minimize, fullscreen, resizable, no frame

**What:** `Wayland_ApplyWindowState`, `Wayland_ToggleFullScreen`, `Wayland_SetCaption`, `Wayland_IconifyWindow`.

**How:**
- **With libdecor:**
  - fullscreen: `libdecor_frame_set_fullscreen(frame, NULL)` or `_unset_fullscreen`.
  - resizable: set the `LIBDECOR_ACTION_RESIZE` capability with min/max content size 0,0 (no limit).
  - not resizable: unset that capability and set min = max = the window size.
  - `SDL_NOFRAME`: `libdecor_frame_set_visibility(frame, false)`.
  - Then commit a `libdecor_state` with the window size.
- **With plain xdg-shell:**
  - fullscreen: `xdg_toplevel_set_fullscreen(NULL)` or `unset_fullscreen`. A NULL output lets the compositor choose.
  - min/max size: 0,0 for resizable, the window size otherwise.
  - With a decoration object, `SDL_NOFRAME` switches the mode to `CLIENT_SIDE`. The driver draws nothing, so the window ends up with no frame. Otherwise the mode is `SERVER_SIDE`.
- `ToggleFullScreen` sets `fullscreen`, applies the state with the screen's flags, commits the surface, flushes and returns 1. SDL then treats the toggle as supported.
- Title: `libdecor_frame_set_title` or `xdg_toplevel_set_title`. Minimize: `libdecor_frame_set_minimized` or `xdg_toplevel_set_minimized`.
- There is no window positioning, because Wayland has no global coordinates.

### Software surfaces: wl_shm buffer presentation

**What:** `Wayland_SetVideoMode` (the SHM case), `Wayland_CreateShmFile`, `Wayland_CreateBuffer` / `Wayland_DestroyBuffer(s)`, `Wayland_PresentShm` and `Wayland_UpdateRects`.

**Why:** SDL 1.2 hands the application a CPU framebuffer and gets told about changed rectangles. Wayland needs buffers that the client may not touch while the compositor holds them. Rendering into a private framebuffer and copying it into a free shm buffer on each update keeps SDL's model intact.

**How (SetVideoMode):**
1. Choose the content mode from the flags: `SDL_OPENGL` selects OpenGL, or fails if EGL was not compiled in. `SDL_VULKAN` selects Vulkan. Anything else is SHM.
2. Store `window_w/h`, `fullscreen` and `resizable`.
3. If there is no surface yet, or the content mode changed, destroy and re-create the window. EGL, Vulkan and wl_shm content cannot share one `wl_surface`. A mode change within the same content mode keeps the window.
4. Call `Wayland_ApplyWindowState`.
5. `SDL_ReallocFormat(current, 32, ...)` with the XRGB masks. Set `current->flags` from the request (`SDL_FULLSCREEN`, `SDL_RESIZABLE`, `SDL_NOFRAME`, plus DOSBox-X's `SDL_HAX_NOREFRESH`), set w and h, set pitch = width*4, and set pixels to NULL.
6. SHM: calloc a framebuffer of pitch*height bytes, point `current->pixels` at it, and set `this->UpdateRects = Wayland_UpdateRects`. The first commit then presented it at once. The resize fix later changed this (see that subsection).
7. Flush the display at the end.

**How (shm files):** `Wayland_CreateShmFile(size)` calls `mkstemp` on `$XDG_RUNTIME_DIR/sdl-shared-XXXXXX` (falling back to `/tmp`), unlinks the file immediately, sets `FD_CLOEXEC`, and calls `ftruncate` to the size, retrying on `EINTR`. `memfd_create` was not used, presumably for portability. The cursor code shares this helper.

**How (buffers):** `Wayland_CreateBuffer`:
1. stride = w*4; mmap the file shared read/write;
2. `wl_shm_create_pool`, then `wl_shm_pool_create_buffer` at offset 0 with `WL_SHM_FORMAT_XRGB8888`;
3. destroy the pool and close the fd right away (the buffer keeps the memory alive);
4. add a `wl_buffer` listener whose `release` clears the buffer's `busy` flag.

**How (present):** `Wayland_PresentShm(numrects, rects)`:
1. Return early if there is no surface, the window is not configured, the content is not SHM, or there is no framebuffer or no size.
2. Pick a buffer that is not busy and already has the right size. Otherwise destroy a buffer that is not busy and create one at the current size in its place.
3. If all four are busy, set `present_pending` and return. `Wayland_PumpEvents` retries when a buffer comes back.
4. Copy the whole framebuffer into the buffer, not only the rects. A reused buffer holds an older frame, so a partial copy would show stale content.
5. `wl_surface_attach`. Damage the whole surface when `numrects <= 0`, otherwise each rect. Use `wl_surface_damage_buffer` when the compositor version is 4 or higher, else `wl_surface_damage`.
6. Mark the buffer busy, clear `present_pending`, `wl_surface_commit`, `wl_display_flush`.

**Testing:** output=surface in DOSBox-X: text mode, graphics modes, and mode switches. Watch for tearing or stale regions when only parts of the screen change.

### OpenGL through EGL

**What:** `SDL_waylandgl.c`, built only with `SDL_VIDEO_DRIVER_WAYLAND_EGL`.

**How:**
1. `Wayland_GL_Init` runs once per device:
   1. `eglGetDisplay` on the `wl_display`, then `eglInitialize`.
   2. `eglBindAPI(EGL_OPENGL_API)`. This gives desktop OpenGL with the compatibility (fixed-function) profile that DOSBox-X's OpenGL output uses, not GLES.
   3. `eglChooseConfig`. Request a window surface type and `EGL_OPENGL_BIT`, R/G/B sizes from `gl_config`, and alpha, depth, stencil and multisample values only when they are non-zero.
   4. `eglCreateContext` with no share context and no attributes.

   The context outlives window surfaces, so it survives video mode changes.
2. `Wayland_GL_SetupWindow(w, h)`:
   - If a `wl_egl_window` already exists, call `wl_egl_window_resize` and stop there.
   - Otherwise call `wl_egl_window_create` on the `wl_surface`, then `eglCreateWindowSurface`, then make the context current.
   - Then call `eglSwapInterval` with `gl_config.swap_control` if that is positive, else 0. Pitfall: Mesa's EGL Wayland platform waits for frame callbacks in `eglSwapBuffers` by default. Those callbacks never arrive while the window is hidden or minimized, so swaps would block forever. Wait for vsync only when the application asks for it.
3. `Wayland_GL_DestroyWindow`: make nothing current, destroy the EGL surface, destroy the `wl_egl_window`. `Wayland_GL_Shutdown` additionally destroys the context, terminates the display, and unloads the GL library.
4. `Wayland_GL_GetProcAddress`: try `eglGetProcAddress` first. If that returns NULL (older EGL does not return core functions), load `libOpenGL.so.0`, falling back to `libGL.so.1`, and look the symbol up there. `Wayland_GL_LoadLibrary` only sets `driver_loaded`.
5. `Wayland_GL_GetAttribute` maps the SDL attributes to `eglGetConfigAttrib`:
   - red, green, blue, alpha, buffer, depth and stencil sizes, sample buffers and samples map directly;
   - doublebuffer is always 1;
   - accelerated visual is true unless the config caveat is `EGL_SLOW_CONFIG`;
   - swap control comes from `gl_config`;
   - anything else (accumulation buffers, stereo) is 0.
6. `MakeCurrent` and `SwapBuffers` are thin wrappers that check that a surface exists.
7. In SetVideoMode's OpenGL case: call `Wayland_GL_SetupWindow`, add `SDL_OPENGL` to the flags, and set `UpdateRects` to NULL.

**Testing:** output=opengl and openglnb, window resize, fullscreen toggle, minimize while running (no hang), and the vsync setting.

### Vulkan windows (Wayland side)

**Depends on:** the Vulkan section must be implemented first.

**What:** `SDL_waylandvulkan.c` and the `WAYLAND_MODE_VULKAN` content mode.

**How:**
- `Wayland_Vulkan_GetInstanceExtensions` reports `VK_KHR_surface` and `VK_KHR_wayland_surface` through `SDL_Vulkan_CopyExtensions`.
- `Wayland_Vulkan_CreateSurface`:
  1. Look up `vkCreateWaylandSurfaceKHR` with `SDL_Vulkan_GetInstanceProc`. If it is missing, report that the instance probably lacks the extension.
  2. Fill a locally declared copy of `VkWaylandSurfaceCreateInfoKHR` with structure type value 1000006000 and the `wl_display` and `wl_surface`. The Vulkan headers are not required to build SDL.
  3. Call `vkCreateWaylandSurfaceKHR`.
- In SetVideoMode's Vulkan case: add `SDL_VULKAN` to the flags, set `UpdateRects` to NULL, and commit the surface. The window appears once the application presents to it.
- On window destroy, `SDL_Vulkan_NotifyWindowDestroy()` runs before anything else (see above).

### Event dispatch

**What:** `Wayland_Dispatch(blocking)` and `Wayland_PumpEvents` in `SDL_waylandevents.c`.

**How:**
- With libdecor active, all dispatch goes through `libdecor_dispatch(ctx, blocking ? -1 : 0)`. libdecor has its own events (plugin, frame surfaces) and must drive the display queue.
- Blocking without libdecor: `wl_display_dispatch`.
- Non-blocking without libdecor uses the standard prepare/read sequence:
  1. Loop on `wl_display_prepare_read`, calling `wl_display_dispatch_pending` until the prepare succeeds.
  2. `wl_display_flush`. EAGAIN is not an error; on any other error, cancel the read and fail.
  3. `poll` the display fd with timeout 0.
  4. If the fd is readable, `wl_display_read_events`; otherwise `wl_display_cancel_read`.
  5. Finally `wl_display_dispatch_pending`.
- `Wayland_PumpEvents` does a non-blocking dispatch. On a lost connection it sets an error and posts `SDL_PrivateQuit` once (a static flag stops repeats). After dispatching, it retries a pending shm present.

### Keyboard (xkbcommon, keymaps)

**What:** The seat's keyboard listener and `Wayland_InitOSKeymap` / `Wayland_TranslateKey`.

**Why:** Wayland sends raw evdev key codes and an XKB keymap. DOSBox-X's mapper and keyboard code expect what the X11 driver produces: SDLKey symbols from X keysyms, and X11 key codes as scancodes. xkbcommon keysyms have the same values as X11 keysyms, so the X11 driver's translation tables can be reused unchanged.

**How:**
1. `seat_capabilities`: create a `wl_keyboard` with the listener when the keyboard capability appears, and destroy it when it goes away. Do the same for the pointer. A new pointer also calls `Wayland_UpdateRelativeMode`. Losing the pointer releases the relative mode and clears `pointer_focus`. `seat_name` is ignored.
2. `keymap`: accept only `WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1` and close the fd otherwise. mmap the fd with `MAP_PRIVATE` (the protocol requires a private mapping from wl_seat version 7 on; the driver binds at most version 5, but private is correct for all versions) and close it. Drop the old keymap and state, call `xkb_keymap_new_from_string` (text v1), munmap, then `xkb_state_new`.
3. `Wayland_InitOSKeymap` fills `ODD_keymap` and `MISC_keymap` (256 entries each) exactly as the X11 driver does, using the `XKB_KEY_*` names for dead keys, ISO_Level3_Shift, keypad, cursor keys, F1 to F15, the lock keys, modifiers, Super, Mode_switch, Multi_key, Help/Print/SysReq/Break/Menu and Hyper_R.
4. `Wayland_TranslateKey(evdev key)`:
   1. The xkb key code is key + 8.
   2. Take the level-0 (unshifted) symbol for the key's current layout: `xkb_keymap_key_get_syms_by_level` with `xkb_state_key_get_layout`. This matches the X11 driver, which also uses the unshifted keysym.
   3. If the symbol is backslash and the evdev code is `KEY_RO` (89) or `KEY_YEN` (124), return `SDLK_JP_RO` / `SDLK_JP_YEN`. These are DOSBox-X additions to SDL 1.x.
   4. Otherwise translate by the keysym's high byte the same way the X11 driver does: Latin-1 directly, other legacy 8-bit sets by their low byte, 0xFE through ODD_keymap, 0xFF through MISC_keymap.
5. `key`:
   - The scancode is the evdev code + 8 truncated to 8 bits, the same as an X11 key code under the evdev keycodes.
   - The sym comes from TranslateKey.
   - Unicode is filled only on press, only when `SDL_TranslateUNICODE` is on, and only for BMP characters, from `xkb_state_key_get_utf32`.
   - Post the result with `SDL_PrivateKeyboard`.
6. `modifiers`: `xkb_state_update_mask(depressed, latched, locked, 0, 0, group)`, then sync the lock modifiers.
7. Lock sync (`Wayland_SyncLockModifiers`): SDL tracks modifiers from key presses, but Num Lock and Caps Lock state comes from the compositor. Clear `KMOD_NUM|KMOD_CAPS` in `SDL_GetModState()` and set them from `xkb_state_mod_name_is_active` (effective mods). Do this on `enter` and on every `modifiers` event.
8. `enter`: sync the locks and set `SDL_APPINPUTFOCUS`. The keys array is not used. `leave`: `SDL_ResetKeyboard()`, because releases for held keys will not arrive, then clear input focus.
9. `repeat_info` is ignored, because SDL 1.2 does its own key repeat (`SDL_EnableKeyRepeat`).

**Pitfall:** The DOSBox-X mapper must know that these scancodes are evdev-based. See the sdl_mapper.cpp fix in the Weston subsection.

**Testing:** Typing in DOS and the mapper, arrow and navigation keys, keypad with Num Lock on and off, Caps Lock state when focusing the window, a JP106 layout for Ro/Yen, AltGr layouts, and keys held across a focus change (they must not stick).

### Pointer: motion, buttons, wheel

**What:** The `wl_pointer` listener.

**How:**
- The listener must provide every event of seat version 5: `enter`, `leave`, `motion`, `button`, `axis`, `frame`, `axis_source`, `axis_stop` and `axis_discrete`. libwayland aborts if an event arrives for a NULL listener entry, so the ones that are not used are empty functions.
- `enter`:
  - Ignore surfaces other than the window surface. libdecor's decoration surfaces also produce pointer events.
  - Store the enter serial (needed for `wl_pointer_set_cursor`), set `pointer_focus`, apply the cursor, and set `SDL_APPMOUSEFOCUS`.
  - When not in relative mode, post an absolute motion to the entry position.
- `leave`: if it is for the window surface and the pointer had focus, clear focus and `SDL_APPMOUSEFOCUS`.
- `motion`: absolute motion through `SDL_PrivateMouseMotion(0, 0, x, y)`, only with focus and not in relative mode. Positions are `wl_fixed_to_int` in surface coordinates. Output scale is not handled; the surface is always buffer scale 1.
- `button`: map evdev BTN_LEFT, BTN_MIDDLE and BTN_RIGHT to the matching SDL buttons, BTN_SIDE to X1 and BTN_EXTRA to X2. The BTN_* values are defined locally because `linux/input-event-codes.h` is not available on every system that has Wayland. Other buttons are ignored.
- `axis`: vertical scroll only. SDL 1.2 represents the wheel as buttons. Accumulate the axis value, and for each 10 units (one notch) post a press and release of `SDL_BUTTON_WHEELDOWN` (positive values) or `SDL_BUTTON_WHEELUP` (negative values). `axis_stop` resets the accumulator. `axis_discrete` is ignored, because the accompanying `axis` event is used instead.

### Relative pointer and pointer constraints (mouse capture)

**What:** `Wayland_UpdateRelativeMode` / `Wayland_ReleaseRelativeMode` in `SDL_waylandevents.c`, plus `Wayland_CheckMouseMode` and `Wayland_GrabInput` in `SDL_waylandmouse.c`.

**Why:** Wayland clients cannot grab the pointer or warp it. On X11, SDL 1.2's relative mode (grab + hidden cursor) works by warping the pointer to the window center. On Wayland the equivalent is to lock the pointer to the surface with pointer-constraints and to read relative motion from relative-pointer. DOSBox-X's mouse capture is exactly "grab on + cursor hidden".

**How:**
1. `Wayland_GrabInput` only maps `SDL_GRAB_FULLSCREEN` to `SDL_GRAB_ON` and returns the mode. There is no real grab.
2. `Wayland_CheckMouseMode` (SDL calls it when the grab mode or the cursor visibility changes) sets `relative_wanted = (input_grab != SDL_GRAB_OFF) && !(SDL_cursorstate & CURSOR_VISIBLE)`, then calls `Wayland_UpdateRelativeMode`.
3. `Wayland_UpdateRelativeMode`:
   - If relative mode is not wanted, release it.
   - If relative mode is already active, do nothing.
   - If the pointer, surface, relative-pointer manager or pointer-constraints global is missing, leave motion absolute.
   - Otherwise:
     1. `zwp_relative_pointer_manager_v1_get_relative_pointer(pointer)` with a listener;
     2. `zwp_pointer_constraints_v1_lock_pointer(surface, pointer, NULL region, LIFETIME_PERSISTENT)`, with empty `locked` / `unlocked` handlers. Persistent means the lock comes back by itself when focus returns;
     3. reset the fractional accumulators, set `relative_mode`, flush.
4. `relative_motion` (only in relative mode): add `dx`/`dy` (the accelerated values) to double accumulators, post the integer part with `SDL_PrivateMouseMotion(0, 1, ix, iy)`, and keep the fraction. Slow movements would otherwise be lost.
5. While in relative mode, absolute motion events are ignored.
6. `Wayland_ReleaseRelativeMode` destroys the locked pointer and the relative pointer. It runs on window destroy, on seat removal and when the pointer capability goes away. A new window calls `UpdateRelativeMode` again, because the lock is tied to the old surface.

**Testing:** Capture the mouse in DOSBox-X (click or the hotkey) and move slowly and quickly in a DOS mouse program. Release the capture, then repeat with a window resize and a fullscreen toggle while captured. Also run on a compositor without pointer-constraints: capture should still work, with absolute motion.

### Cursors

**What:** `SDL_waylandmouse.c`: `Wayland_CreateWMCursor`, `Wayland_FreeWMCursor`, `Wayland_ShowWMCursor`, `Wayland_ApplyCursor`, `Wayland_QuitMouse`.

**How:**
1. `CreateWMCursor` turns SDL's 1-bit data/mask cursor into an ARGB8888 shm buffer, using the shared shm-file helper. Data bit set gives opaque black. Mask set without data gives opaque white. Neither gives transparent. SDL's "inverted" pixel (data 1, mask 0) falls under the first rule and shows as black. The cursor records its size and hotspot.
2. It also checks whether the cursor is SDL's built-in default arrow by comparing size, hotspot and the bytes against `default_cdata` / `default_cmask` from `default_cursor.h`. If so, it is marked `is_default`, and the desktop's themed cursor is shown in its place.
3. The theme is loaded lazily with `wl_cursor_theme_load(XCURSOR_THEME, XCURSOR_SIZE or 24, shm)`. Look up the `default` cursor and fall back to `left_ptr`. Only the first image is used, so there is no animation.
4. `Wayland_ApplyCursor` runs only with a pointer that has focus on the window:
   - If the cursor is NULL (hidden), call `wl_pointer_set_cursor(serial, NULL, 0, 0)`.
   - Otherwise attach the theme image buffer or the cursor's own buffer to a single reusable cursor `wl_surface` (created on first use), damage it, commit it, then call `wl_pointer_set_cursor` with the last enter serial and the hotspot.

   It is called from `ShowWMCursor` and from `pointer_enter`, because the cursor must be set again on every enter with the new serial.
5. `FreeWMCursor` clears the current cursor if it is the one being freed, destroys its buffer and unmaps its memory. `QuitMouse` destroys the cursor surface and the theme.

### Configure handling: compositor sizes, resize events

**What:** `Wayland_HandleConfigureSize`, plus the part of `Wayland_UpdateMouse` that repeats the suggestion.

**Why:** On Wayland the compositor decides window size changes: interactive resizes, maximize and fullscreen. SDL 1.2 has no way to accept a size directly. The application receives an `SDL_VIDEORESIZE` event and calls `SDL_SetVideoMode` at the new size. Until then the content stays at the old size, which xdg-shell allows.

**How:**
1. The toplevel configure (or the libdecor configure) records `configure_w/h`, which are 0 when the compositor leaves the size to the client.
2. On the xdg_surface configure (after `ack_configure`), or after the libdecor commit, `Wayland_HandleConfigureSize` calls `SDL_PrivateResize(w, h)` if the suggested size is non-zero and differs from the window size.
3. `Wayland_UpdateMouse` is called by SDL after every `SetVideoMode`. It re-posts mouse focus and position (absolute mode only). It also repeats the resize request if the compositor's suggestion still differs from the window size, because `SDL_SetVideoMode` throws away pending resize events. A suggestion that arrived during the mode change would otherwise be lost.
4. Fullscreen modes, the ListModes behaviour and the fullscreen-state check were changed by the Weston fixes below.

### SDL_GetWMInfo and SDL_SYSWM_WAYLAND

**What:** `SDL_syswm.h` gains `SDL_SYSWM_WAYLAND` in `SDL_SYSWM_TYPE`, forward declarations of `struct wl_display` and `struct wl_surface`, and an `info.wl` member in the union with `display`, `surface` and `shell_surface` (a `void *` to the xdg_toplevel).

**How:**
- `Wayland_GetWMInfo` fills `subsystem = SDL_SYSWM_WAYLAND` and the three pointers. With libdecor, `shell_surface` comes from `libdecor_frame_get_xdg_toplevel`. It returns 1, or -1 with an error if the caller's major version is newer than the library's.
- The UNIX `SDL_SysWMinfo` with its union only exists in the X11 part of `SDL_syswm.h`. For that reason the hook is compiled only when `SDL_VIDEO_DRIVER_X11` is also defined. A build with Wayland but no X11 driver has no GetWMInfo. That is acceptable for now, but worth knowing.
- Pitfall: existing callers that assume `info.x11` is valid whenever `SDL_GetWMInfo` succeeds will break. They must check `subsystem`. See the Weston fixes for the DOSBox-X call sites that needed it.

### Shutdown

`Wayland_VideoQuit` releases everything in this order: destroy the window, `Wayland_GL_Shutdown`, `Wayland_QuitMouse`, `Wayland_RemoveSeat` (relative mode, pointer, keyboard, xkb state/keymap, seat), unref libdecor, unref the xkb context, then destroy the output, pointer-constraints, relative-pointer manager, decoration manager, wm_base, shm, compositor and registry. Then flush, disconnect, free the app_id and clear `screen->pixels`. The SDL comment about being called in the middle of another routine (from the X11 driver) applies here too. `Wayland_DeleteDevice` frees `gl_data`, `hidden` and the device.

### Fixes from testing on Weston (0778d87cd)

**SDL side:**

1. **ListModes.** The first version returned NULL (no modes) for every depth other than 32 bpp, and "any size" (`(SDL_Rect **)-1`) for 32 bpp. `SDL_GetVideoMode` then fell into the -1 "list" of another depth and walked it as an array, which crashed. Fix:
   - For `SDL_FULLSCREEN`, when the output size is known, return a one-entry list holding the output's size. The list is stored in the private data as an `SDL_Rect fullscreen_mode` plus a NULL-terminated `SDL_Rect *fullscreen_modes[2]`, so the pointers stay valid.
   - In every other case return -1.
   - Behave the same for every depth, since the screen is always 32 bpp and SDL emulates the rest.

   This matches the X11 driver without XRandR: SDL centers smaller fullscreen modes in that one mode.
2. **Configure for the wrong fullscreen state.** Right after the application switches fullscreen on or off, the compositor can still send a configure sized for the previous state. That turned into a bogus `SDL_VIDEORESIZE`. Fix:
   - Add `configure_fullscreen` to the private data.
   - In the toplevel configure, walk the `states` array (`wl_array_for_each`) and set it when `XDG_TOPLEVEL_STATE_FULLSCREEN` is present.
   - In the libdecor configure, read it with `libdecor_configuration_get_window_state` and `LIBDECOR_WINDOW_STATE_FULLSCREEN`.
   - Both `Wayland_HandleConfigureSize` and the repeat in `Wayland_UpdateMouse` now also require `configure_fullscreen == fullscreen`.
3. **libdecor that cannot decorate.** The commit message lists "if libdecor can't decorate the window, do without it". In the tree, the fallback (unref the context and use plain xdg-shell when `libdecor_decorate` returns NULL) is already in `Wayland_CreateWindow` in the first commit. The 0778d87cd diff contains no further libdecor change apart from the window-state read above. Implement the fallback as described in the window subsection.

**DOSBox-X side:**

4. **`src/gui/sdl_mapper.cpp`, `loadScanCode()`** (Linux/BSD, `C_X11_XKB` path): the code called `XkbGetMap` on `info.info.x11.display` whenever `SDL_GetWMInfo` succeeded. On the Wayland driver that pointer is the Wayland display, and the Xlib call crashed at startup. Fix: if `SDL_GetWMInfo` returns > 0 and `subsystem` is not `SDL_SYSWM_X11`, set `evdev_input = true` directly. The Wayland driver's key codes are evdev + 8, exactly what the evdev mapping table expects. Only take the Xkb path for X11. The diff calls `SDL_GetWMInfo` twice in the if/else-if; calling it once and testing the result is cleaner.
5. **`src/hardware/glide.cpp`, `statWMInfo()`:** only take `wmi.info.x11.window` as the window handle when `wmi.subsystem == SDL_SYSWM_X11`.
6. **`src/gui/sdlmain_linux.cpp`, `Linux_GetDesktopResolution()`** (SDL1 path): when the subsystem is not X11, the code used to guess 1024x768. Now it calls `SDL_ListModes(NULL, SDL_FULLSCREEN)` and, if that returns a real list with an entry, uses the first entry's size. With the ListModes fix above, that is the output size. The 1024x768 guess remains the fallback.
7. **`src/gui/sdlmain.cpp`, `GFX_SwitchFullScreen()` and `GFX_HandleVideoResize()`:** the screen size report (`screen_size_info.screen_dimensions_pixels`) is -1 x -1 when it cannot be read without X11. The checks were `!= 0`, so -1 passed and the fullscreen size became 65535x65535 after the cast to 16 bits. Both checks are now `> 0`.
8. **`src/output/output_vulkan.cpp`, `VK_CreateDevice()`:** this file belongs to the Vulkan section, but the fix came from this testing. When choosing a device and queue, also call `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` and skip the device if it fails, even when `vkGetPhysicalDeviceSurfaceSupportKHR` reported presentation support. RADV on a Wayland compositor without dmabuf says yes and then fails. With the extra check, another device (lavapipe) is chosen instead.

**Testing:**
- Start DOSBox-X on Weston with the mapper's default bindings and open the mapper (no crash, and keys map correctly, including the arrow, navigation and keypad keys).
- Toggle fullscreen in surface and OpenGL output. It should use the output's size, with no 65535 sizes and no bogus resize after leaving fullscreen.
- Check `Linux_GetDesktopResolution`'s log output.
- Vulkan output on a machine with RADV plus lavapipe.

### Resize without a black flash (57f21c20d)

**What:** Changes to the SHM path in `Wayland_SetVideoMode`, `Wayland_PresentShm`, `Wayland_PumpEvents`, and a new `Wayland_HoldingUpdates`. Three fields are added to the private data: `shown`, `hold` and `hold_ticks`.

**Why:** Calling `SDL_SetVideoMode` on a window that was already visible committed the new, zeroed framebuffer at once. DOSBox-X then draws its menu bar first, as a partial update. Because the present copies the whole framebuffer, that update pushed the still-black frame to the compositor. Every resize, including every DOS video mode change, therefore showed a black frame, then a black window with only the menu bar, and only then the real image.

**How:**
1. `Wayland_CreateWindow` clears `shown` and `hold`. `Wayland_PresentShm` sets `shown = 1` after each successful commit.
2. In SetVideoMode's SHM case:
   - If the window has never been shown, present right away as before, so a new window appears.
   - Otherwise do not present. Set `hold = 1` and `hold_ticks = SDL_GetTicks()`. The compositor keeps the last frame on screen.
3. SetVideoMode no longer calls `Wayland_DestroyBuffers`. A buffer the compositor still holds must not be destroyed under it. `Wayland_PresentShm` already replaces buffers of the wrong size as they are released, because it recreates any buffer that is not busy and has the wrong size. SetVideoMode instead clears `present_pending`, because an update dropped at the old size is no longer wanted.
4. `Wayland_HoldingUpdates` returns `hold`. It first clears `hold` when 250 ms (`WAYLAND_HOLD_MS`) have passed since `hold_ticks`, using an unsigned tick difference. The timeout covers an application that never draws a full frame.
5. At the start of `Wayland_PresentShm`, while `hold` is set:
   - Treat the update as whole if `numrects <= 0` or any rect starts at 0,0 and covers at least the window size. A whole update clears `hold` and is presented. That is DOSBox-X's `SDL_Flip` / full update after a resize.
   - A partial update while still holding sets `present_pending` and returns.
   - After the 250 ms timeout, partial updates go through.
6. Damage: compute `damage_all = (numrects <= 0) || present_pending` before choosing a buffer. If an earlier update was dropped (all buffers busy, or held back), its changed area is in the framebuffer but not in this call's rects, so the whole surface must be damaged. Otherwise damage only the rects.
7. `Wayland_PumpEvents` retries a pending present only when `!Wayland_HoldingUpdates(this)`. A held partial update is then flushed by the timeout rather than right away.

**Pitfalls:**
- This relies on the application sending a full update after a resize, which DOSBox-X does. The 250 ms limit is the fallback for applications that don't.
- The hold applies only to SHM content. OpenGL and Vulkan present on their own.
- The window keeps showing the old frame at the old size until the full update arrives. That is intended.

**Testing:** On a compositor (headless Weston worked), switch repeatedly between mode 13h and text mode, and resize the window by dragging. Capture or watch the committed frames: there should be no all-black or menu-bar-only frame on size changes. Also check that a newly created window still appears immediately, and that a program which only ever does partial updates still shows them after at most 250 ms.

### Known limitations and open points

- Only the first `wl_output` is used for the desktop/fullscreen size. Output hotplug (`global_remove`) and `wl_output.scale` / `wl_surface.set_buffer_scale` are not handled, so HiDPI outputs show the window at buffer scale 1, upscaled by the compositor.
- No window icon (`SetIcon` is NULL), no window positioning, no pointer warping, no YUV overlays.
- Horizontal scrolling is ignored. Key repeat comes from SDL, not from the compositor's repeat info.
- Text input is limited to `xkb_state_key_get_utf32` per key: no compose sequences beyond what xkb state gives, and no input method.
- `SDL_GetWMInfo` is only available when the X11 driver is also compiled in (see above).
- The first commit was written untested. Everything described here reflects the tree after all three commits, which was tested on Weston only. Testing on GNOME/Mutter, KDE/KWin and wlroots compositors (sway) is still to do, in particular the xdg-decoration server-side path, which Weston (as far as is known) does not offer.

## 3dfx Voodoo rendering: OpenGL render-to-texture, threaded software rendering, Direct3D 9 and 11 back ends

On master, `[voodoo] voodoo_card` accepts `false`, `software`, `opengl` and `auto`, and `auto` means `opengl` whenever the build has OpenGL. The OpenGL emulation (`src/hardware/voodoo_opengl.cpp`) takes over the SDL window when the 3dfx output turns on. `voodoo_ogl_reset_videomode()` tears down the output (`GFX_TearDown`) and sets an `SDL_OPENGL` video mode of its own at the 3dfx resolution. It then renders straight into the window's default framebuffer and calls `SDL_GL_SwapBuffers()` on every 3dfx buffer swap. While it runs, `sdl.desktop.prevent_fullscreen` locks out fullscreen, window resizing, the menu bar toggle, the mapper, the aspect shortcut and the configuration tool. Scalers, aspect correction and GLSL shaders do not apply. The software renderer runs on the emulation thread only.

On main-ai, every hardware back end works the same way. It renders on the device or context that belongs to the current output, into off-screen buffers of its own (front color, back color, depth/stencil) at a render resolution it chooses. At each 3dfx vertical retrace it copies the front buffer into a "present" texture and asks the output to draw that texture where the emulated video card's frame would normally go. The output stays in charge of the window, so menus, resizing, fullscreen, scaling and aspect keep working. The back end must match the output:

- `opengl`: output=opengl, openglnb or openglpp (any `SCREEN_OPENGL`).
- `vulkan`: output=vulkan. This back end is in a separate group, but it introduces the dispatch layer the Direct3D back ends use; see 7.
- `direct3d`: output=direct3d (Direct3D 9).
- `direct3d11`: output=direct3d11.
- `metal` also exists on main-ai, but belongs to another group.
- `auto` picks whichever of these matches the output that is active when the 3dfx output turns on.

If the requested back end does not match the active output, or fails to initialize, the card is emulated in software ("VOODOO: acceleration disabled"). The choice is made again each time the 3dfx output turns on. Software rendering can now use several worker threads.

New settings in this group: `voodoo_resolution` (`native`/`double`/`display`), `voodoo_multithreading`, `voodoo_threads`, and the `voodoo_card` values `direct3d` and `direct3d11`.

Implementation order: OpenGL context plumbing in the output and in-tree SDL 1.x (1–2), the OpenGL render-to-texture back end (3), render resolution and correctness fixes (4–5), threaded software rendering (6, independent of everything else), back-end dispatch (7), Direct3D 9 (8–9), Direct3D 11 (10–11).

### 1. OpenGL output: shared contexts and the "make current" discipline

(part of 00fd06f7a)

**What.** The OpenGL output gets an abstraction for several GL contexts: a "main" context for the window and other contexts that share objects with it. Every place where the output (or the SDL-drawn menu) makes GL calls first makes the main context current. Two output bugs are fixed along the way.

**Why.** For the 3dfx emulation to stop owning the window, it has to render with a context of its own whose textures the output can sample. With two contexts, any GL call made while the wrong context is current corrupts state or draws into the wrong place. Two existing bugs also showed up:
- With SDL2, after a window resize, `SDL_GetWindowSurface()` can make SDL's own renderer context current (with `SDL_HINT_FRAMEBUFFER_ACCELERATION`), so the output's later GL calls went to the wrong context.
- When a new main context is created that shares objects with the old one, the GLSL program object survives (`glIsProgram` is true), but vertex attribute arrays and uniforms are per-context state. The program was reused without setting them up again.

**How.**
1. In `src/output/output_opengl.h`, declare an opaque `GFX_GLContext` (a `void*`) and these functions:
   - `GFX_GL_CreateMainContext`
   - `GFX_GL_CreateSharedContext`
   - `GFX_GL_DeleteContext`
   - `GFX_GL_MakeContextCurrent`
   - `GFX_GL_GetCurrentContext`
   - `GFX_OpenGL_MakeCurrent` (make the main context current)
   - `OUTPUT_OPENGL_Active` (the desktop type is `SCREEN_OPENGL` and a window/context exists)
2. Implement them three times in `output_opengl.cpp`:
   - **SDL2.** Wrap `SDL_GL_GetCurrentContext`, `SDL_GL_MakeCurrent(sdl.window, ctx)` and `SDL_GL_DeleteContext`. `GFX_GL_CreateSharedContext` makes the main context current, sets `SDL_GL_SHARE_WITH_CURRENT_CONTEXT` to 1, calls `SDL_GL_CreateContext`, and sets the attribute back to 0. `GFX_GL_CreateMainContext` (used wherever `SDL_GL_CreateContext(sdl.window)` was called for the output: `OUTPUT_OPENGL_Select` and both places in `GFX_SetSDLWindowMode`) checks whether a 3dfx context exists. If one does, it makes that context current, lets it release (see 3), creates the new context with share-with-current, and then waits on the 3dfx fence in the new context. This keeps the 3dfx textures visible across a window re-creation.
   - **SDL1 with the in-tree SDL** (guarded by `SDL_GL_SHARED_CONTEXTS`, see 2). The main context is the one `SDL_SetVideoMode()` made (`SDL_GL_GetVideoModeContext`), and shared contexts come from `SDL_GL_CreateSharedContext`.
   - **Stock SDL1.** Stubs that return NULL/false, so the 3dfx OpenGL emulation simply can't start. Commit 6eac4d9fb makes the stub `GFX_GL_CreateSharedContext` log why: the SDL 1.x has no `SDL_GL_CreateSharedContext`, so build with the in-tree `vs/sdl`. Before that, only "acceleration disabled" was logged.
3. `GFX_OpenGL_MakeCurrent`: if the current context is already the main one, return. If the current context is the 3dfx one, call `Voodoo_OGL_ReleaseContext()` first. Then make the main context current and call `Voodoo_OGL_WaitRelease()`. Both functions are defined in `voodoo_opengl.cpp` (see 3).
4. Call `GFX_OpenGL_MakeCurrent()` at the top of every output GL entry point:
   - `OUTPUT_OPENGL_SetSize`, `OUTPUT_OPENGL_StartUpdate`, `OUTPUT_OPENGL_EndUpdate`, `OUTPUT_OPENGL_Shutdown`
   - `GFX_SetShader` (before deleting the program) and `GFX_OpenGLRedrawScreen`
   - `UpdateSDLDrawTexture` and `UpdateSDLDrawDBCSTexture`
   - all the `Menu*` drawing functions in `src/gui/menu.cpp` (`MenuShadeRect`, `MenuDrawRect`, `MenuDrawTextChar`, `MenuDrawTextChar2x`, `MenuDrawText`)
   - the glClear calls in `sdlmain.cpp`'s SDL-drawn menu mouse handling
   On SDL2, also call it right after `SetupSurfaceScaledOpenGL` in `OUTPUT_OPENGL_SetSize` and after `SDL_GetWindowSurface` in `OUTPUT_OPENGL_Select`. That is the resize bug fix.
5. Move the shader program setup out of `OUTPUT_OPENGL_SetSize` into a helper, `OpenGL_SetupShaderProgram`. It does `glUseProgram`, the `a_position` attribute pointer and enable, the `rubyTexture` sampler uniform, the ruby uniform locations, and `RENDER_SetForceUpdate`. Call it both when a program is newly linked and when an existing program is still valid in a new context.
6. With SDL2, `sdl.surface` is not updated on window resize. Add a helper (`OpenGL_GetWindowSize`) that asks `SDL_GetWindowSize`, and use it in place of `sdl.surface->w/h` for the viewport, `glOrtho`, and the SDL-drawn menu's `screenWidth`/`screenHeight`.

**Testing.** With output=opengl, without any 3dfx use, check window resize, fullscreen toggle, switching GLSL shaders at runtime, and the SDL-drawn menu, on both SDL1 and SDL2 builds. Nothing should change visually. The SDL2 resize case should no longer leave a garbage or black window.

### 2. In-tree SDL 1.x: shared OpenGL contexts

(part of 00fd06f7a, plus 6eac4d9fb)

**What.** The in-tree SDL 1.x (`vs/sdl`) gets five functions that SDL 1.2 lacks: `SDL_GL_CreateSharedContext`, `SDL_GL_DeleteContext`, `SDL_GL_MakeCurrentContext`, `SDL_GL_GetCurrentContext` and `SDL_GL_GetVideoModeContext`, with the `SDL_GLContext` typedef and a feature macro, `SDL_GL_SHARED_CONTEXTS`. They are implemented for the X11 (GLX), windib (WGL) and Wayland (EGL) drivers.

**Why.** SDL1 builds can only get the one context `SDL_SetVideoMode` creates. The 3dfx emulation needs a second context that shares textures with it. That share must also survive later `SDL_SetVideoMode` calls, which destroy and re-create the video mode context (each output resize).

**How.**
1. `include/SDL_video.h`: declare the functions and define `SDL_GL_SHARED_CONTEXTS` to 1. A NULL argument to `SDL_GL_MakeCurrentContext` means the video mode's own context.
2. `SDL_sysvideo.h`: add four device hooks: `GL_CreateSharedContext`, `GL_DeleteContext`, `GL_MakeContextCurrent`, `GL_GetVideoModeContext`.
3. `SDL_video.c`:
   - Keep a static "current non-video-mode context" and reset it to NULL in `SDL_SetVideoMode`, where the new video mode context is made current.
   - `SDL_GL_GetCurrentContext` returns the tracked context, or the video mode context if none is tracked.
   - `SDL_GL_MakeCurrentContext` calls the hook and updates the tracked context.
   - Fail with `SDL_SetError` when there is no `SDL_OPENGL` mode or the driver lacks the hooks.
   The tracking matters: after a mode set, the next "enter" by the 3dfx emulation sees that its context is not current and re-binds it, now to the new window.
4. GLX (`SDL_x11gl.c`, hooked up in `SDL_x11video.c`):
   - `X11_GL_CreateSharedContext` calls `glXCreateContext(display, visual, glx_context, True)` and remembers the first shared context in a new `gl_data->glx_share_context`.
   - Change `X11_GL_CreateContext` to pass `glx_share_context` as the share list. Later video mode contexts then join the same share group.
   - `X11_GL_MakeContextCurrent` binds to the current `SDL_Window`.
   - `X11_GL_DeleteContext` refuses to delete the video mode context and clears `glx_share_context` if that context is the one being deleted.
5. WGL (`SDL_wingl.c`, hooked up in `SDL_dibvideo.c`):
   - Load `wglShareLists` alongside the other WGL entry points.
   - `WIN_GL_CreateSharedContext` creates a context on `GL_hdc`, calls `wglShareLists(GL_hrc, new)`, and remembers the first one as `GL_share_hrc`.
   - When a new video mode context is created, call `wglShareLists(GL_share_hrc, GL_hrc)`. WGL has no share argument at creation time.
6. Wayland EGL (`SDL_waylandgl.c`, hooked up in `SDL_waylandvideo.c`):
   - Use `eglCreateContext` with the video mode context as the share context.
   - The EGL context outlives window surfaces, so no re-sharing is needed.
   - Make current against `gl->surface`, and unbind before destroying a context that is current.
7. On the DOSBox-X side, the stub for stock SDL1 logs the explanatory message from 1.

**Testing.** SDL1 builds on Linux X11, Linux Wayland (SDL1 Wayland support comes from another group) and Windows: start a 3dfx title with output=opengl, then resize, toggle fullscreen and switch outputs. Each of those re-runs `SDL_SetVideoMode`. The 3dfx picture must keep showing. Building DOSBox-X against a system SDL 1.2 should log the new message instead of crashing.

### 3. 3dfx OpenGL emulation renders to a framebuffer object drawn by the output

(00fd06f7a)

**What.** `voodoo_opengl.cpp` stops creating its own video mode. It creates a shared context (1) and renders into a framebuffer object at the 3dfx resolution. At each vertical retrace, the front buffer is blitted, flipped to top-row-first, into a present texture, and the OpenGL output draws that texture in place of the VGA frame. Remove the `prevent_fullscreen` lockout everywhere.

**Why.** With the master design, while a 3dfx game runs:
- fullscreen, resizing, the menu bar (SDL-drawn and Windows HMENU), the mapper (it shows `MAPPEREDITOR_NOT_AVAILABLE`), the configuration tool and the aspect toggle are all disabled or broken;
- scalers, aspect correction and shaders don't apply;
- leaving 3dfx mode has to restore the old video mode, including a Direct3D special case that calls `change_output(6)`;
- the SDL1 drawn menu bar is known to grow every frame.

Rendering off-screen avoids all of this and keeps everything on the GPU (nothing is read back for display).

**How.**
1. **Function loading.** Load the framebuffer-object functions with `SDL_GL_GetProcAddress`, trying the core name first and then the `EXT` name: Gen/Delete/Bind framebuffers, `FramebufferTexture2D`, Gen/Delete/Bind renderbuffers, `RenderbufferStorage`, `FramebufferRenderbuffer`, `CheckFramebufferStatus`, `BlitFramebuffer`. That covers OpenGL 3.0 or `GL_EXT_framebuffer_object` plus `GL_EXT_framebuffer_blit`. If any is missing, log it and fail init (fall back to software).
   - Optionally load `glFenceSync`, `glWaitSync` and `glDeleteSync` (OpenGL 3.2 or `ARB_sync`), all or none. `GLsync` is handled as `void*` because older headers lack it. Define the missing GL enums locally (`GL_FRAMEBUFFER`, `GL_READ/DRAW_FRAMEBUFFER`, `GL_DEPTH24_STENCIL8`, `GL_DEPTH_COMPONENT24`, `GL_SYNC_GPU_COMMANDS_COMPLETE`, ...).
   - Pitfall: the Windows OpenGL 1.1 headers have no `GLuint64`. Commit e7052de2b adds a typedef in `voodoo_vogl.h` when neither `GL_VERSION_3_2` nor `GL_ARB_sync` is defined.
2. **Per-context state.** Keep one static struct with:
   - the context handle;
   - the main FBO and its two color textures (RGBA8, `GL_NEAREST`, `GL_CLAMP`) on `GL_COLOR_ATTACHMENT0` and `GL_COLOR_ATTACHMENT0+1`;
   - a depth/stencil renderbuffer;
   - the index of the front color attachment;
   - a present FBO with its present texture;
   - width and height;
   - the pending fence.
3. **Buffer creation.**
   - Try `GL_DEPTH24_STENCIL8`, attached as both depth and stencil. If the framebuffer is incomplete, fall back to `GL_DEPTH_COMPONENT24` without stencil and log "no stencil buffer". In that case do not flag `VOGL_HAS_STENCIL_BUFFER`, so depth-source-compare loses its stencil pass. Always flag `VOGL_HAS_ALPHA_PLANE`.
   - Clear both color buffers to black and depth to 1.0.
   - Coordinates stay as they were when rendering to a window: row 0 at the bottom, same projection, same `glViewport(0,0,w,h)`. So triangle, fastfill and LFB code is unchanged apart from the points below.
   - Drop the old fullscreen `adjust_x`/`adjust_y` viewport offsets.
4. **Front/back mapping.**
   - In `voodoo_vogl.cpp`, replace the hard-coded `GL_FRONT`/`GL_BACK` in `VOGL_SetDrawMode` and `VOGL_SetReadMode` with two variables.
   - Add `VOGL_SetBuffers(front, back)`, which updates them and re-issues `glDrawBuffer`/`glReadBuffer` for the current mode.
   - `voodoo_ogl_swap_buffer` no longer calls `SDL_GL_SwapBuffers`. It flips the front index and calls `VOGL_SetBuffers` with the two attachment enums. The new front buffer becomes visible at the next retrace.
5. **Entering the context.**
   - Every `voodoo_ogl_*` entry point starts with an "enter" helper: draw triangle, swap, draw pixel/z/pixel pipeline, fastfill, clear, read pixel, vblank flush, set window, update dimensions, leave, plus init and present.
   - The helper returns false when there is no context, or when the OpenGL output is not active. In that case it logs once, "not drawing, the 3dfx emulation needs the OpenGL output", and the caller does nothing. Otherwise it makes the 3dfx context current if it isn't already, and drops any stale fence.
   - `VOGL_ClearBeginMode()` must run before switching contexts, because the emulation batches `glBegin`.
6. **Synchronization between contexts.**
   - `Voodoo_OGL_ReleaseContext` (called with the 3dfx context current, just before another context becomes current) closes any open `glBegin`. It then either inserts a `glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE)` and calls `glFlush`, or calls `glFinish` if sync objects are unavailable.
   - `Voodoo_OGL_WaitRelease` (called in the newly current context) does `glWaitSync` with `GL_TIMEOUT_IGNORED` and deletes the fence. This is a GPU-side wait, so the CPU does not stall.
7. **Present** (`voodoo_ogl_present`, called from a new `voodoo_present()` in `voodoo_emu.cpp`, itself called from `Voodoo_VerticalTimer` in `voodoo_interface.cpp` in the hardware branch after `voodoo_set_window()`):
   - Disable the scissor test temporarily (blits are scissored).
   - Blit the front color attachment to the present FBO with the destination Y range reversed (height to 0), `GL_NEAREST`, so the present texture is top row first like any frame the output uploads.
   - Re-bind the main FBO and re-apply the buffers.
   - Call `OUTPUT_OPENGL_PresentExternal()`.
   The separate present texture is what the output samples. The 3dfx emulation can keep drawing into its own front buffer (front-buffer rendering, LFB writes) while the output redraws the window at any time, for menus or resizes.
8. **Output side** (`output_opengl.cpp`): add `OUTPUT_OPENGL_SetExternalTexture(tex, w, h)`, `OUTPUT_OPENGL_HasExternalTexture`, `OUTPUT_OPENGL_DrawExternal` and `OUTPUT_OPENGL_PresentExternal`.
   - **SetExternalTexture** stores the texture and sets the menu-draw and clear countdowns to 3.
   - **DrawExternal** binds the texture with `GL_NEAREST` for the `GLNearest`/`GLPerfect` kinds and `GL_LINEAR` otherwise, with `GL_CLAMP` wrapping.
     - With a GLSL program, it sets `rubyTextureSize` and `rubyInputSize` both to the texture size (the texture is fully used, unlike the output's power-of-two texture), increments `rubyFrameCount`, and draws the existing 4-vertex triangle strip.
     - Without a program, it draws a textured quad over `sdl.clip`. It pushes a texture matrix that scales texel coordinates by 1/width and 1/height, then restores the output's own texture binding.
   - **PresentExternal** makes the main context current (which waits on the fence), clears if the countdown says so, draws the external texture, draws the menu (`CheckMenuDrawing`), swaps, and counts the frame.
   - `GFX_OpenGLRedrawScreen` draws the external texture instead of the PBO/VGA texture when one is set.
9. **Video mode** (`voodoo_ogl_reset_videomode`): create the buffers at `v->fbi.width` × `v->fbi.height`, call `OUTPUT_OPENGL_SetExternalTexture`, then `RENDER_SetSize(width, height, 32, 60.0, 4/3)`. The output sizes the window for the 3dfx mode like for any video mode, with 4:3 aspect. Keep the `glShadeModel(GL_SMOOTH)` note and call. `RENDER_SetSize` may run the output's resize, which makes the main context current, so re-enter afterwards.
10. **Init** (`voodoo_ogl_init`):
    - Require `OUTPUT_OPENGL_Active()`, otherwise log "needs output=opengl" and fail.
    - Create the shared context once, enter it, `VOGL_Reset`, then `VOGL_Initialize`, load the functions, and reset the video mode. On any failure, destroy the context.
    - **Destroy context:** delete the buffers in the 3dfx context, delete the fence, unset the external texture, make the main context current, and delete the 3dfx context.
    - `voodoo_ogl_leave(true)` and `voodoo_ogl_shutdown` destroy the context. Leaving without `leavemode` still frees textures and shaders, but only if the context can be entered. The output then simply goes back to VGA frames.
11. **Remove the lockout:**
    - `sdl.desktop.prevent_fullscreen` and its initialization;
    - `GFX_GetPreventFullscreen` and `GFX_PreventFullscreen` (with the `SDL1_hax_RemoveMinimize` toggle);
    - `Voodoo_OGL_GetWidth/GetHeight` and the special cases using them in `SetupSurfaceScaledOpenGL` (fixed window size, no menu scaling or menu height);
    - every caller of the getter: `ToggleMenu`, `DOSBox_SetSysMenu` (menu item disabled state), `DOSBox_RefreshMenu`, the sys-menu `RESTOREMENU`/`TOGGLEMENU` handlers (both copies in `GFX_Events`), `GFX_SwitchFullScreen`, `GFX_EndUpdate` (early return), both resize handlers (`GFX_HandleVideoResize`, SDL1 `HandleVideoResize`), the SDL-drawn menu hover/click suppression, `GUI_ResetResize`, `MAPPER_RunInternal`, `RunCfgTool` (`voodoo_ogl_update_dimensions` call), `AspectRatio_mapper_shortcut`, the menu rebuild guards in `dos.cpp`, `programs.cpp` (`Load_Language`, `ApplySetting`) and `shell_cmds.cpp` (`toSetCodePage`), and the GLSL path in `OUTPUT_OPENGL_SetSize` that hid the menu only when not locked;
    - the old `voodoo_ogl_leave` code that restored the mode (`GFX_RestoreMode`, the Direct3D `change_output(6)` hack).
12. `src/output/output_vulkan.cpp` only loses its unused `Voodoo_OGL_*` declarations.

**Testing.**
- Use a DOS Glide title or a Win9x guest with Voodoo drivers, voodoo_card=opengl or auto, output=opengl, on SDL1 and SDL2.
- Check that the 3dfx picture scales with the window, honours aspect and the GLSL shader, and that the menu bar stays usable.
- While the 3dfx output is on, toggle fullscreen, resize, open the mapper and the configuration tool, and switch to another output and back. With a non-OpenGL output it should log "not drawing" and not crash.
- Exit the game and check that the VGA picture comes back.
- Try a driver without `ARB_sync` if available (glFinish path).

### 4. Render resolution option and correctness fixes for the OpenGL emulation

(1aa82a12d)

**What.**
- A new setting, `[voodoo] voodoo_resolution` = `native` | `double` | `display` (default `native`, `Changeable::Always`), also in Video > 3dfx emulation as three menu items: `3dfx_res_native`, `3dfx_res_double`, `3dfx_res_display`.
- Fixes for LFB coordinates, a black first frame, fastfill clipping, the front-buffer status bits, mode changes while active, and an SDL1 X11 resize bug.

**Why.**
- Rendering only at the 3dfx mode resolution looks blocky when scaled up. Some users expect the old DOSBox behaviour of rendering at window size.
- LFB writes in OpenGL mode landed one row too low. `lfb_w` passed `scry+1`, and the writes were drawn as `GL_POINTS` on pixel edges. LFB reads used a different and inconsistent mapping.
- The first frame after the 3dfx output turned on was black. What the game had drawn in software, into card memory, before activation was not carried over. The OpenGL fastfill also assumed `yorigin == height-1`, which produced a negative scissor on a fresh context.
- `voodoo_swap_buffers` returned early in OpenGL mode, so `frontbuf`/`backbuf` and the swap history never changed. The front-buffer bits in the status register stayed constant, which can confuse software that polls them.
- A 3dfx video mode change while the output was on did not resize the OpenGL buffers until later. In software mode it did not resize the DOSBox-X render at all.
- SDL1/X11: after switching to output=opengl, the window snapped back to the size of the window SDL had just replaced. A stale `ConfigureNotify` was taken as a user resize.

**How.**
1. **Setting and menu.**
   - Define `VOODOO_OGL_RES_NATIVE`, `VOODOO_OGL_RES_DOUBLE` and `VOODOO_OGL_RES_DISPLAY` in `include/voodoo.h`, plus `Voodoo_OGL_SetResolutionMode`, `Voodoo_OGL_GetResolutionMode`, `Voodoo_OGL_ParseResolutionMode` (case-insensitive, unknown means native) and `Voodoo_OGL_SetDisplaySize(w, h)`.
   - Add the setting in `dosbox.cpp` with help text.
   - Allocate the three menu items in `menu_callback.cpp`. Their callback does `SetVal("voodoo","voodoo_resolution",...)`, sets the mode, and refreshes the checkmarks (`Voodoo_UpdateResolutionMenu`).
   - Initialize the checkmarks in `main()`.
   - Changes from the menu, the configuration tool or `CONFIG -set` take effect at the next frame, because present re-reads the setting each time.
   - Also in this commit, the Vulkan output entry moves after the OpenGL entries in `def_menu_video_output` (cosmetic).
2. **Display size.** `SetupSurfaceScaledOpenGL` calls `Voodoo_OGL_SetDisplaySize(sdl.clip.w, sdl.clip.h)` after computing the clip rectangle.
3. **Render size:**
   - start from the mode size;
   - double it for `double`;
   - for `display`, take the per-axis maximum of the mode size and the display size. Never go smaller than the mode, or LFB reads would miss pixels;
   - clamp to `min(GL_MAX_TEXTURE_SIZE, GL_MAX_RENDERBUFFER_SIZE)` (queried at function load, floor 1024) and to at least 1.
   The window is still sized for the 3dfx mode (`RENDER_SetSize` with `v->fbi.width/height`). Only the off-screen buffers change. Call `RENDER_SetSize` before computing the render size, because it updates the display size.
4. **Resizing between frames.**
   - At each present (before the blit), poll the setting and recompute the size. If it changed, create a new set of buffers.
   - Blit both color attachments from the old FBO with `GL_NEAREST`, so pixels keep exact colors for LFB reads. Blit depth+stencil with `GL_NEAREST`, the only filter allowed for depth. Blit the present texture with `GL_LINEAR`.
   - Delete the old objects, keep the front index, reset the projection/viewport (`voodoo_ogl_set_window` path), and hand the new present texture to the output.
   - If creation fails, restore the old handles and continue.
   - Pitfalls: `glClear` and `glBlitFramebuffer` are clipped by the scissor test, so disable it around them. A new context's scissor box defaults to the size of the window it was first bound to, so set `glScissor(0,0,w,h)` after creating buffers.
5. **One pixel mapping for all LFB access** (`voodoo_ogl_lfb_span`). Mode pixel `i` covers the render pixels whose centers fall inside it. With `d = render/mode`, the range is from `ceil(i·d − 0.5)` to `ceil((i+1)·d − 0.5)`, at least one pixel, clamped to the buffer. This is exactly what a `GL_NEAREST` blit from mode size to render size produces, so LFB writes, LFB reads, loading from RAM and resizing all agree.
   - **Writes** (`draw_pixel`, `draw_z`, `draw_pixel_pipeline`) become `GL_QUADS` over the covered span instead of loops of `GL_POINTS`. The Y span is mirrored when the projection is bottom-origin (`last_orientation == 1`).
   - In `voodoo_emu.cpp`, the three `lfb_w` call sites and the `lfb_r` call pass `scry` instead of `scry+1`.
   - **Reads** take the middle render pixel of each span.
   - Color reads use a per-buffer line cache. Re-read the row when the row or the width changes. Note that the old code compared `x+1` against the cached width and is replaced.
   - Depth reads use `glReadPixels(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT)` per pixel, keeping the top 16 bits.
6. **First frame** (`voodoo_ogl_load_framebuffers_from_ram`, called from `voodoo_activate` right after a successful init and clear):
   - For the front and back buffers (`v->fbi.rgboffs[frontbuf/backbuf]`, skipped if `~0` or out of range), convert RGB565 from `v->fbi.ram` (stride `rowpixels`, bounded by the memory mask) to RGBA8 with bit replication and alpha 0, flipping rows to bottom-first.
   - Upload into a temporary texture attached to a temporary read FBO, and blit `GL_NEAREST` into the matching attachment at render size.
7. **Fastfill rows.** The clip rows are screen rows with 0 at the top. With Y origin at the bottom (`FBZMODE_Y_ORIGIN`), shift both by `height − 1 − yorigin`. Otherwise flip them to `height − ey .. height − sy`. Clamp to the mode size and skip empty rectangles. The scissor is needed unless the rectangle covers the whole mode.
8. **Status bits.** Remove the early return in `voodoo_swap_buffers`. Call `voodoo_ogl_swap_buffer()` inside the existing "rotate the buffers" block, so the frontbuf/backbuf rotation and swap history always run.
   - Side effect: on Voodoo 2 with `vblank_dont_swap` set, the OpenGL back end now also doesn't swap, matching software.
9. **Mode changes.**
   - In `register_w`, where `videoDimensions` updates `fbi.width/height` and sets `ogl_dimchange`, also call a new `voodoo_ogl_follow_dimensions()` when OpenGL is active. It re-checks the render size immediately, so later drawing uses the new size.
   - `voodoo_ogl_update_dimensions` (from `Voodoo_UpdateScreen`) creates buffers if none exist. Otherwise it calls `RENDER_SetSize` for the new mode, re-checks the render size, re-sets the external texture and logs.
   - In `Voodoo_UpdateScreen`, the "dimensions changed while shown" branch now handles software mode too: clear `ogl_dimchange`, set `vdraw.height`, `RENDER_SetSize` with 16 bpp and the 3dfx refresh.
10. **SDL1 X11** (`vs/sdl/src/video/x11`):
    - Add `X11_ConfigureNotifyMinSerial`. `X11_SetVideoMode` sets it to `NextRequest(SDL_Display)` before touching the window.
    - In the event loop, drop `ConfigureNotify` events whose serial is older (signed difference below zero), since the server generated them before it processed the mode set.

**Testing.**
- Switch `voodoo_resolution` from the menu and with `CONFIG -set voodoo voodoo_resolution=display` during a 3dfx scene. The picture should get sharper without flicker or losing content, and `display` should follow window resizes and fullscreen.
- Use titles that draw HUDs, menus or movies through the LFB, and ones that read it back (screenshots, some menus). Check there is no one-row offset at any resolution.
- A game that draws its splash in software before the 3dfx output turns on should show that splash instead of a black frame.
- Change resolution in-game (e.g. 640x480 to 800x600 on Voodoo 2).
- On SDL1/X11, switch output to opengl from the menu and check the window keeps its size.

### 5. OpenGL output on SDL1: forced redraw after a resize

(dbf503b6a)

**What.** In `SetupSurfaceScaledOpenGL`, re-enable `sdl.gfx_force_redraw_count = 2` (it was commented out), for SDL1 builds only.

**Why.** Right after `SDL_SetVideoMode()` resizes the window, Mesa can still draw into a buffer of the old size. A frame that never changes afterwards, typically the BIOS screen at startup, was shown shifted up by the height difference, or not at all. SDL2 is not affected. This showed up while testing 4.

**How.** Under `#if !defined(C_SDL2)`, set the force-redraw count to 2 before `UpdateWindowDimensions()`. Update the NTS comment to say the problem still exists with SDL 1.x.

**Testing.** SDL1 build on Linux/Mesa with output=opengl: the BIOS screen right after startup must be placed correctly.

### 6. Software 3dfx rendering with several threads

(b9b43b9f7)

**What.** When the card is rendered in software, triangles and fastfills are queued for worker threads (SDL threads), each owning a set of scanlines. The output is bit-identical to single-threaded rendering. New settings: `voodoo_multithreading` (bool, default true, `WhenIdle`) and `voodoo_threads` (string, default `auto`, `WhenIdle`).

**Why.** The software renderer is the only option with outputs that have no back end (surface, TTF) and the fallback whenever acceleration fails. On one thread it is slow at higher resolutions.

**How.** All of this is in `voodoo_emu.cpp`.
1. **Work queue.** Keep a ring of 256 jobs. Each job holds the scanline callback, the destination buffer pointer, the `poly_extra_data` (owned by the job from then on and deleted when the slot is reused or at stop), the first scanline, and a copy of the per-scanline extents.
   - Keep a counter of jobs queued, one "drawn up to" counter per worker, a "waited" counter, a mutex, and two condition variables: `work` (new jobs or quit) and `done` (a worker finished).
   - Use at most 64 workers.
2. **Workers.** Worker `k` of `n` draws, for every job in queue order, the scanlines whose absolute Y satisfies `y mod n == k`. The first index into a job is `((k − starty) mod n + n) mod n`, then steps of `n`.
   - Ownership is by absolute Y, the same in every job, so each pixel is still written by one thread in submission order. This is why the result matches single-threaded rendering exactly, including blending and depth.
   - A worker takes the range of queued jobs under the lock, draws it unlocked (queued jobs are not modified until every worker is past them), then updates its counter. It broadcasts `done` only if the emulation thread is waiting (a flag), to avoid needless wakeups.
3. **Queueing.**
   - `poly_render_triangle` now returns bool. When the threads are in use, it fills a static extents vector instead of calling the callback per scanline, queues the job, and returns true. `triangle_create_work_item` then must not delete `extra`.
   - `fastfill` queues one job with `raster_fastfill` and one extent per row (the dither matrix is copied into `extra`) instead of looping over blocks of 64 extents.
   - The producer blocks while the ring is full (queued minus the slowest worker's counter ≥ 256).
4. **When to wait.** `voodoo_sw_wait()` returns immediately if nothing was queued since the last wait. Otherwise it blocks until every worker has drawn everything. The emulation thread must wait before anything the rasterizers read or write changes, or is accessed another way:
   - In `register_w`, for every register except the triangle setup registers (`vertexAx`..`dWdY`, `fvertexAx`..`fdWdY`, whose values are copied into `poly_extra_data` at setup) and the commands `triangleCMD`, `ftriangleCMD`, `nopCMD`, `fastfillCMD` and `swapbufferCMD`.
   - At the top of `lfb_w`, `lfb_r` and `texture_w`.
   - In `Voodoo_VerticalTimer` before reading the frame for display (software branch).
   - In `update_statistics`.
   - In `voodoo_activate`, because a hardware back end starts from the frame buffer contents (see 4).
   - Stop the threads in `voodoo_shutdown`.
5. **Rotating stipple.** Triangles with stipple rotation (`FBZMODE_ENABLE_STIPPLE` with `FBZMODE_STIPPLE_PATTERN == 0`) change the `stipple` register per pixel, so they must be drawn inline on the emulation thread. The decision is per triangle or fill. It is safe because enabling that mode means writing `fbzMode`, which already waited for the queue to drain.
6. **Statistics.** Make `v->thread_stats` an array of threads+1 blocks (index 0 is the emulation thread). A `thread_local` index tells `raster_generic` and `raster_fastfill` which block to count in. `update_statistics` waits, then sums and resets all blocks.
7. **Setup.**
   - `voodoo_sw_set_threads(n)` stops any existing threads and reallocates the stats.
   - With `n < 2`, render inline as before.
   - Otherwise create the mutex, the condition variables and `n` threads (`SDL_CreateThread`, whose signature differs between SDL1 and SDL2). If a thread can't be created, stop the ones already made and fall back to one thread. Otherwise workers would wait forever for the missing one.
   - `Voodoo_Initialize` (in `voodoo_interface.cpp`) calls it after `voodoo_init`. The thread count is 1 if `voodoo_multithreading` is false. Otherwise it is `atoi(voodoo_threads)`; if that is ≤ 0 (`auto`), use half the logical CPUs (`GetSystemInfo` on Windows, `sysconf(_SC_NPROCESSORS_ONLN)` elsewhere), capped at 8. The hard cap is 64.

**Testing.**
- With voodoo_card=software, compare captured frames from a deterministic demo with `voodoo_threads=1` and with several threads. They must be identical.
- Check speed with `auto`.
- Exercise LFB-heavy titles, texture uploads mid-frame, a game using stipple, and switching from software to a hardware back end mid-session (output change).
- Run with thread sanitizer if possible.

### 7. Back-end dispatch: `v->hw_api`

(part of e7052de2b, extended in 00cef5312)

**What.** Replace the `v->vulkan` flag in `voodoo_state` (`voodoo_data.h`) with `int hw_api`, one of `VOODOO_HW_API_OPENGL` (0), `VOODOO_HW_API_VULKAN` (1), `VOODOO_HW_API_DIRECT3D` (2) or `VOODOO_HW_API_DIRECT3D11` (3), defined in `include/voodoo.h`. Add `VOODOO_EMU_TYPE_DIRECT3D` (5) and `VOODOO_EMU_TYPE_DIRECT3D11` (6) next to SOFTWARE (1), ACCELERATED/OpenGL (2), VULKAN (3) and AUTO (4).

**How.**
1. `voodoo_hw.h` dispatches every `voodoo_hw_*` call (set_window, present, swap_buffer, vblank_flush, update_dimensions, follow_dimensions, clear, load_framebuffers_from_ram, fastfill, clip_window, texture_clear, invalidate_paltex, draw_triangle, draw_pixel, draw_z, draw_pixel_pipeline, read_pixel, init, leave, shutdown) to `voodoo_ogl_`, `voodoo_vk_`, `voodoo_d3d_` or `voodoo_d3d11_` according to `hw_api`.
2. `voodoo_activate` picks the API:
   - the requested type maps directly;
   - for AUTO, use Vulkan if `OUTPUT_VULKAN_Active()`, Direct3D if `OUTPUT_DIRECT3D_Active()`, Direct3D 11 if `OUTPUT_DIRECT3D11_Active()`, and otherwise OpenGL, whose init then fails without the OpenGL output.
   On init failure, reset to software (`ogl = false`, `hw_api` back to OpenGL).
3. Bump the upper bound check in `Voodoo_Initialize`, and add the new types to every `switch` in the `VOODOO` module class in `voodoo.cpp`: init, destructor, PCI init/enable, page handler.
4. Add the `voodoo_card` strings `direct3d` (under `C_DIRECT3D`) and `direct3d11`, and update the help text.

**Depends on.** The Vulkan group (d14cdb453) introduces `voodoo_hw.h`, `VOODOO_EMU_TYPE_AUTO`, `v->hw_request` with per-activation selection, and the shared helpers `Voodoo_HW_PollResolutionSetting` and `Voodoo_HW_RenderSize(max, w, h)` (the render-size logic from 4, moved out of the `C_OPENGL` block). It also moves `ogl_get_vertex_data`/`ogl_vertex_data` (vertex setup from the iterators: x, y, depth `d` from `ogl_get_depth`, clamped colors, per-TMU `sw`, `tw`, `w` and LOD blend, fog blend) out of `#if C_OPENGL`. The Direct3D back ends use all of these. If Direct3D is implemented without Vulkan, build those pieces as part of this step.

### 8. Direct3D 9 back end (voodoo_card=direct3d)

(e7052de2b)

**What.** New `src/hardware/voodoo_d3d.cpp` and `voodoo_d3d.h`, implementing the full `voodoo_d3d_*` set on the Direct3D 9 output's device. Hooks go into `CDirect3D` (`src/output/direct3d/direct3d.cpp`/`.h`) and `output_direct3d.cpp`. Add the files to `src/hardware/Makefile.am` (sources and `EXTRA_DIST`) and to `vs/dosbox-x.vcxproj` and `.filters`.

**Why.** On master, a user with output=direct3d gets software 3dfx rendering, or the OpenGL takeover, which then has to switch back to Direct3D on exit.

**How.**
1. **Sharing the device with the output.**
   - `CDirect3D` gets `GetDevice()`, `GetD3D()`, `IsDeviceLost()`, `BeginExternal()`/`EndExternal()`, `SetExternalTexture(tex, w, h)` and `PresentExternal()`, plus private `lpExternal`, its size, and an `externalDirty` flag.
   - The output has a worker thread (`D3D_THREAD`). `BeginExternal` calls `Wait(false)`, which returns with the critical section held and the worker idle. `EndExternal` sets `externalDirty` and leaves the critical section.
   - In the back end, wrap every device use in a scope object that does Begin/End and reports whether drawing is possible: buffers ready, the same device as the output's, and not lost. If the device changed, log "not drawing, the 3dfx emulation needs the Direct3D output" once.
   - `OUTPUT_DIRECT3D_Active()`: `d3d` exists, the desktop type is `SCREEN_DIRECT3D`, and there is a device.
   - `OUTPUT_DIRECT3D_SetSize` reports the clip size with `Voodoo_OGL_SetDisplaySize`, for `voodoo_resolution=display`.
2. **Output drawing.**
   - In `D3DSwapBuffers`, if `externalDirty`, call a new `RestoreOutputState()` first. It resets everything the 3dfx emulation changes: render target back to the back buffer (which resets the viewport), depth surface NULL, vertex and pixel shader NULL, the output's FVF, stream source and texture, stage 1 disabled, and the cull, lighting, Z, Z-write, blend, separate blend, alpha test, stencil, scissor, fog and color write render states. It then calls `SetupSceneScaled()` (sampler 0 linear) and, without pixel shaders, the projection, view and world transforms.
   - Then, if `lpExternal` is set, draw it instead of the normal path. Use a pre-transformed (`XYZRHW|TEX1`) triangle strip over `dwX, dwY, dwScaledWidth, dwScaledHeight` (minus 0.5 for D3D9 pixel centers), pixel shader NULL, via `DrawPrimitiveUP`. Then restore the FVF, stream source and texture, because `DrawPrimitiveUP` unsets the stream source.
   - `PresentExternal` waits for the worker, calls `Resize3DEnvironment` if the device is lost, and runs `D3DSwapBuffers`.
3. **Device reset and loss.**
   - `Resize3DEnvironment` calls `voodoo_d3d_device_reset(true)` before invalidating and `voodoo_d3d_device_reset(false)` after `RestoreDeviceObjects`.
   - Before the reset, the back end copies both color buffers into `D3DPOOL_SYSTEMMEM` offscreen surfaces (`GetRenderTargetData`) and releases all `D3DPOOL_DEFAULT` objects, which a Reset requires.
   - After the reset, it recreates them at the same size and `UpdateSurface`s the colors back. Depth is not kept.
   - `DestroyD3D` calls `voodoo_d3d_device_lost()`, which destroys everything and sets a "lost" flag. The next present re-initializes on the output's new device once it is active again, losing what was drawn.
4. **Buffers.**
   - Two A8R8G8B8 render-target textures (color), one more for present, each with its level-0 surface.
   - Depth: an `INTZ` FOURCC depth texture if `CheckDeviceFormat` allows it (so depth can be read back), otherwise a `D3DFMT_D24S8` depth-stencil surface.
   - Rows are top first. Clear to black, depth 1.0, stencil 0 (present to opaque black).
   - Caps check at init: `PixelShaderVersion` ≥ 2.0, max texture size from caps, separate alpha blend from `D3DPMISCCAPS_SEPARATEALPHABLEND`.
5. **Pixel shaders.** No HLSL compiler is used: ps_2_0 bytecode is assembled at runtime by a small token writer (version token `0xFFFF0200`, dcl/def/tex/texkill/mov/add/mul/dp3/max/cmp, end token).
   - The shader is a step-by-step translation of the OpenGL emulation's GLSL generator (`ogl_shaders`: tex combine per TMU, color path, fog). It is cached in a map keyed by both texture modes (0 when the TMU is unused), `fbzColorPath`, the alpha-test bits of `alphaMode` (low 4 bits), the fog mode (low 6 bits if fog is enabled, else 0), and flags for TMU0/TMU1/chroma key.
   - Inputs: t0 is the iterated color; t1 and t2 are the TMU texture coordinates as (s·w, t·w, 0, w), sampled with projected `texldp` for perspective; t3 holds the fog blend and the two LOD blends.
   - Constants c0–c4 are `color0`, `color1`, `chromaKey`, `fogColor`, and the alpha reference (`alphaMode` bits 24–31 / 255) with `zaColor` / 65535. c10 and c11 are defined inside the shader (0, 0.5, 1, 1e-4; 1e-8, −1, 1e-6).
   - ps_2_0 has no compare-and-discard, so every discard is `texkill` on a value that is negative exactly when the pixel must go:
     - **Chroma key:** squared distance minus 1e-8.
     - **Alpha test** for each of the eight functions: a reference/alpha difference with a small epsilon, an absolute value via `max(x, −x)`, a constant −1 for "never", and nothing for "always".
     - **The local-select override** (`texel.a < 0.5 ? iterated : color0`) uses `cmp`.
   - If `CreatePixelShader` fails (likely the 64-instruction ps_2_0 limit), log the ALU count and skip the triangle.
6. **Draw state and batching.**
   - Collect all state of a draw in one struct: target buffer, kind (shader triangles, flat triangles, flat points), shader, two textures, two sampler settings, blend enable and factors, depth test, function and write, color write mask, stencil mode, and the constants. `memset` it so padding compares equal.
   - Append vertices to a pending batch while the state compares equal (`memcmp`). Otherwise flush with `DrawPrimitiveUP` inside Begin/EndScene, after setting all state, since the output changes it too.
   - Flush before any read, swap, present, texture destruction or clear, and at 30000 vertices.
   - Every draw invalidates the read-back cache of its target and of depth.
7. **Mapping the Voodoo state:**
   - **Vertices:** take `ogl_get_vertex_data` for the three vertices and scale x and y from mode to render resolution. Flip Y when `FBZMODE_Y_ORIGIN` is set, subtract 0.5, set rhw = 1, and use z = the depth from the vertex data. Clamp colors to [0,1], as OpenGL's fixed-function path does.
   - **Blending:** use `ALPHAMODE_ALPHABLEND`. Source and destination RGB factors come from 16-entry tables: zero, src alpha, dest color/src color, dest alpha, one, inverse src alpha, inverse dest/src color, inverse dest alpha, reserved entries as zero, and entry 15 as `SRCALPHASAT` (source) or `SRCCOLOR` (destination, standing in for "color before fog"). The alpha factors are ONE if the code is 4, else ZERO. Use separate alpha blend only if supported.
   - **Depth:** the function is `D3DCMP_NEVER + FBZMODE_DEPTH_FUNCTION`; the orders match.
     - With alpha planes enabled, the aux buffer is alpha, so the depth test is ALWAYS and depth is never written.
     - Otherwise, use the depth function if the depth buffer is enabled, or ALWAYS if only the aux mask is set.
     - Depth is written when the aux mask is set and alpha planes are off.
     - The color write mask is RGB from `RGB_BUFFER_MASK` and alpha from `AUX_BUFFER_MASK`.
   - **Depth source compare** (`FBZMODE_DEPTH_SOURCE_COMPARE`), the same two passes as the OpenGL emulation:
     1. Flush and clear stencil. Draw the triangle flat at depth `zaColor` with the depth function (if depth is enabled and alpha planes are off), color writes off, and stencil ALWAYS/REPLACE 1.
     2. Draw the real triangle with stencil EQUAL 1 and depth ALWAYS.
   - **Samplers:** min and mag linear from the texture mode filter bits. The mip filter is NONE when `lodmin == lodmax`, LINEAR for trilinear, POINT otherwise. Clamp or wrap per `TEXMODE_CLAMP_S/T`.
   - **Draw buffer:** front or back from `FBZMODE_DRAW_BUFFER`. Drawing to the front sets `v->fbi.vblank_flush_pending`, as OpenGL does. `vblank_flush`, `set_window` and `clip_window` are no-ops, because the projection is computed per triangle and there is no scissor use.
8. **Textures.** Use a per-TMU map keyed by the base address (`lodoffset` of the first used LOD) holding format, width, height, `ilod`, `lodmask` and NCC table. Drop the entry when any of these change.
   - Palette formats (1, 5, 9, 14) keep up to about 8 variants keyed by an FNV-1a hash of the palette or the selected NCC table. `invalidate_paltex` marks them for re-hashing.
   - `texture_clear` (texture memory writes) only records a dirty address range; a wrapped dword dirties everything. Before the next lookup, entries overlapping the range are dropped, with flush first and wrap-around handled.
   - Conversion uses the TMU's `lookup` table: 8-bit formats by byte, formats 10–12 by 16-bit value, and the 8+8 alpha formats by low-byte lookup with alpha from the high byte.
   - Mipmaps are built on the CPU by 2x2 rounded averaging from the base level. The OpenGL emulation uses `glGenerateMipmapEXT`, so these are generated levels too, not the card's LOD levels.
   - Upload into A8R8G8B8 `D3DPOOL_MANAGED` textures via `LockRect` per level. Managed textures survive device resets.
9. **Present, swap, clear and resize.**
   - **Present:** if the device is gone and lost, re-init. Otherwise check the render size (resize as in 4), flush, `StretchRect` front to present with POINT, end the scope, then call `PresentExternal`.
   - **Swap:** flush and flip the front index.
   - **Clear:** clear both buffers.
   - **Resize:** recreate the targets and `StretchRect` the colors with POINT. Depth is cleared, because D3D9 can't stretch depth.
   - **Video mode reset:** release targets, `RENDER_SetSize` (outside the device lock, since it may resize the output), then create targets at `Voodoo_HW_RenderSize(max)` and set the external texture.
10. **Fastfill.** Map the clip rows like the software fill; with Y origin, rows become `yorigin − ey + 1 .. yorigin − sy + 1`. Clamp, scale to render pixels, and use `IDirect3DDevice9::Clear` with a rect: target if the RGB mask is set (front or back per the draw buffer), Z if the aux mask is set and `auxoffs` is valid, color `color1`, depth `zaColor`/65535.
11. **LFB.**
    - **Writes:** draw the span from 4 as a single point (when it is one pixel) or two flat triangles (a rectangle). Use fixed function with a diffuse-color FVF, point size 1, and the color mask from `has_rgb`/`has_alpha`.
      - `draw_z` writes depth with color off and depth ALWAYS.
      - `draw_pixel_pipeline` uses the `alphaMode` blending and the aux mask for alpha.
      - The target buffer comes from `LFBMODE_WRITE_BUFFER_SELECT`.
    - **Reads:** read back a whole buffer and cache it until the next draw to that buffer.
      - Colors come through `GetRenderTargetData` into a system-memory surface.
      - Depth (INTZ only) is drawn with a tiny "copy texture" pixel shader into an R32F render target, read back, and converted to 16 bits. Without INTZ, depth reads return 0xffff.
      - Return two pixels as RGB565 (or two 16-bit depths), sampled at the middle of each span.
    - **Load from RAM:** build column and row maps from the span function, convert RGB565 to ARGB in a system-memory surface at render size, and `UpdateSurface` into each buffer.

**Testing.**
- On Windows, and under Wine if possible: output=direct3d with voodoo_card=direct3d and with auto. Run Glide titles with fog, chroma key (sprites), alpha test, trilinear mipmapping, two-TMU multitexture (Voodoo 2), depth-source-compare, LFB writes and reads.
- Resize the window and toggle fullscreen during a 3dfx scene. The device reset must keep the picture.
- Switch outputs away and back. The emulation restarts on the new device.
- Check `voodoo_resolution` modes, and compare frames against voodoo_card=software.

### 9. Direct3D 9 output: log why d3d9 can't be used

(48d2f8977)

**What.** In `CDirect3D::InitializeDX`, log "D3D9: Failed to load d3d9.dll" when `LoadLibrary` fails. In `InitD3D`, log "D3D9: Direct3DCreate9 not found in d3d9.dll" when `GetProcAddress` fails. Behaviour does not change; it still falls back to the surface output.

**Why.** The fallback to surface was silent, so users could not tell why direct3d (and its 3dfx back end) wasn't used. The Direct3D 11 output already logs this. The CHANGELOG entry mentions this ("cannot find DLL").

**Testing.** Run with a renamed or missing d3d9.dll (e.g. a Wine DLL override) and check the log.

### 10. Direct3D 11 back end (voodoo_card=direct3d11)

(00cef5312)

**What.** New `src/hardware/voodoo_d3d11.cpp` and `voodoo_d3d11.h`, the same function set as 8 with a `voodoo_d3d11_` prefix, on the Direct3D 11 output's device. Hooks go into `CDirect3D11` (`output_direct3d11.cpp`/`.h`). Add the files to `Makefile.am` and the VS project. At this commit it is SDL2-only (`C_DIRECT3D && C_SDL2`); 11 lifts that.

**Why.** Same as 8, for output=direct3d11.

**How.** The structure is the same as 8. The differences:
1. **Output side.**
   - `CDirect3D11` gets `GetDevice()`, `GetContext()`, `SetExternalTexture(srv)` and `PresentExternal()`, plus `externalSRV` and a saved `viewport`, captured in `Initialize` and `Resize`.
   - Factor the draw-and-present part of `EndUpdate` into a private `DrawFrame(srv)`. `EndUpdate` draws `externalSRV` if set, otherwise the frame SRV. `PresentExternal` draws `externalSRV`.
   - `DrawFrame` resets what the 3dfx emulation changes on the shared immediate context: the viewport, rasterizer state (NULL), blend and depth-stencil state (NULL), and vertex buffer slot 0.
   - `Shutdown` calls `voodoo_d3d11_device_lost()`.
   - `Resize` reports the viewport size through `Voodoo_OGL_SetDisplaySize`.
   - Add `OUTPUT_DIRECT3D11_Active()`, `OUTPUT_DIRECT3D11_Get()`, and `OUTPUT_DIRECT3D11_Compile(source, entry, target, blob)`. The latter uses the output's dynamically loaded `D3DCompile` (`my_pD3DCompile`) and logs compile errors.
   - There is no worker thread, so no locking is needed.
2. **Output fixes in the same commit.**
   - The output's fullscreen-quad vertex shader computes position and UV from `SV_VertexID` with conditionals instead of indexing local arrays. Wine's HLSL compiler fails on the arrays.
   - The `int10.h` include uses forward slashes.
3. **Requirements.** Feature level ≥ 10_0, for shader model 4 (`vs_4_0`/`ps_4_0`). The maximum texture size is 16384 at feature level 11 and above, else 8192.
4. **Device objects, created at init:**
   - **Vertex shader:** takes positions in render pixels (row 0 at top) and outputs NDC using a constant buffer with (2/w, 2/h), Y inverted. It passes through the color, two TMU coordinates and an extra float4 (fog blend, LOD blends).
   - **Input layout:** five float4 elements (`POSITION`, `COLOR`, `TEXCOORD0..2`).
   - **Flat pixel shader:** returns the vertex color.
   - **Copy pixel shader:** samples one texture.
   - **Rasterizer state:** no culling, `DepthClipEnable` false.
   - **Dynamic vertex buffer** of 65536 vertices, used as a ring with `MAP_WRITE_NO_OVERWRITE`, and `MAP_WRITE_DISCARD` on wrap.
   - **Dynamic constant buffers:** one for the VS scale, one for the five PS float4 constants.
5. **Triangle pixel shaders** are HLSL text generated line by line from the same key as in 8, mirroring the GLSL generator (`tex1` combine, then `tex0` combine, color path, fog). Use real `discard`, `abs` and the ternary operator. Perspective is done as `Sample(..., t.xy / t.w)`. Compile and cache in a map.
6. **State objects** instead of render states: blend states keyed by (enable, factors, write mask), depth-stencil states keyed by (test, function, write, stencil mode), and samplers keyed by the sampler bits. All are created on demand and cached.
   - Sampler filter bits: min linear 0x10, mag linear 0x04, mip linear 0x01 (trilinear).
   - `MaxLOD` is 0 when not mipmapping.
   - The blend factor tables and depth function mapping (`D3D11_COMPARISON_NEVER + function`) are as in 8.
7. **Buffers.**
   - **Color and present:** `R8G8B8A8_UNORM` textures with both RTV and SRV.
   - **Depth:** `R24G8_TYPELESS` with a `D24_UNORM_S8_UINT` DSV.
   - **Present:** `CopyResource` from the front buffer, same size.
   - **Resize:** draw the old SRV into the new RTV with the copy shader and a point/clamp sampler. Depth is cleared.
   - There is no device-reset path. Loss of the output's device works as in 8: destroy everything, then re-init on the next present.
8. **Pixel centers** are at +0.5 in D3D11. So single-pixel LFB points go at x0+0.5, y0+0.5, and triangle vertices get no −0.5 offset (unlike D3D9).
9. **Fastfill** draws a flat rectangle at depth `zaColor` with blending off. The write mask is all channels or none, by the RGB mask. Depth is written (func ALWAYS) if the aux mask applies. The draw is used because D3D11.0 has no partial clear.
10. **Textures** are `B8G8R8A8_UNORM`, `D3D11_USAGE_IMMUTABLE`, created with all mip levels as initial data. Mips are built on the CPU as in 8.
11. **LFB reads** use staging textures (`R8G8B8A8_UNORM` for color, `R24G8_TYPELESS` for depth), `CopyResource`, and `Map` READ. Swizzle RGBA bytes to ARGB. Depth is the low 24 bits scaled to 16. So depth readback always works here, with no INTZ question.
12. **Loading from RAM** uses `UpdateSubresource` with RGBA byte order.
13. **Teardown.** The back end holds a reference to the immediate context (`GetImmediateContext`). On destroy, it calls `ClearState()` and releases that reference.
14. **Wiring:** add `VOODOO_EMU_TYPE_DIRECT3D11` and `VOODOO_HW_API_DIRECT3D11` (7) and the `direct3d11` `voodoo_card` value.

**Testing.** Same as 8, with output=direct3d11, including under Wine, since that is where the vertex shader problem was found. Additionally:
- check LFB depth reads (aux buffer select 2);
- check that the normal VGA picture is right after leaving 3dfx mode, since the output must restore its own state after the emulation's draws.

**Depends on.** 7. It shares its design with 8, so do 8 first.

### 11. Direct3D 11 output (and its 3dfx back end) in SDL1 builds

(f7b3a303d)

**What.** Remove the `C_SDL2` guards around output=direct3d11 and voodoo_card=direct3d11. The output works under SDL1 on Windows, with no changes to the in-tree SDL1.

**Why.** On master, SDL1 Windows builds have output=direct3d but not direct3d11. The in-tree SDL1 already provides everything the Direct3D 9 output uses: the windib GDI window, its HWND as `child_window`, and `SDL1_hax_inhibit_WM_PAINT`.

**How.**
1. **Lift the `C_SDL2` guards** (keep `C_DIRECT3D`) in:
   - `SCREEN_DIRECT3D11` in `include/sdlmain.h`
   - the `output` and `voodoo_card` value lists
   - the output menu entry and its callback
   - `render.cpp` (hardware scaler warning, doublescan check)
   - `sdl_gui.cpp`, `sdl_mapper.cpp`, `vga_draw.cpp` (`CheckSourceResolution`)
   - `voodoo.cpp`, `voodoo_emu.cpp`, `voodoo_d3d11.cpp`
   - `output_tools.cpp` (`change_output(13)`, menu checkmark)
   - every `SCREEN_DIRECT3D11` case in `sdlmain.cpp` (SetSize, StartUpdate, EndUpdate, GetRGB, shutdown, startup)
   - `output_direct3d11.cpp`/`.h`
2. **`GFX_GetBestMode`:** add the missing `SCREEN_DIRECT3D11` case (`OUTPUT_DIRECT3D11_GetBestMode`). Without it, SDL1 fell back to surface.
3. **SDL1 path in `output_direct3d11.cpp`:**
   - `d3d11_init` switches SDL to the windib driver when needed, exactly as the Direct3D 9 output does: unless `SDL_VIDEODRIVER` is `dummy` or `directx`, quit the video subsystem, set `SDL_VIDEODRIVER=windib`, re-init, and restore the icon, title and menu. It creates the window with `SDL_SetVideoMode(640, 400, 0, SDL_RESIZABLE)` and draws into `wmi.child_window`.
   - The window size comes from the HWND's client rect (`GetClientRect`).
   - Resizing and fullscreen go through `SDL_SetVideoMode`: `SDL_RESIZABLE` when windowed, `SDL_FULLSCREEN` at the monitor size (`GetMonitorInfo`/`MonitorFromWindow`) when fullscreen, skipped if nothing changed. Set `must_redraw_all` and clear `deferred_resize`.
   - `OUTPUT_DIRECT3D11_Select` sets `SDL1_hax_inhibit_WM_PAINT = 1`, so SDL1 doesn't paint over the Direct3D image.
   - Wrap the `SDL_GetWindowSize`, `SDL_SetWindowSize` and `SDL_SetWindowFullscreen` calls in small helpers with SDL1 equivalents.
4. **Sampler choice:** the output used `ASPECT_NEAREST`/`ASPECT_BILINEAR`, which exist only in SDL2 builds. Define an own enum (`D3D11_SAMPLE_NEAREST`, `D3D11_SAMPLE_BILINEAR`) in `output_direct3d11.h`.

**Testing.** An SDL1 Windows build: output=direct3d11 at startup and switched from the menu, window resize, fullscreen in and out, the menu bar, and then a 3dfx title with voodoo_card=direct3d11 and with auto.

**Depends on.** 10.

## Vulkan: output driver, SDL 1.x support, 3dfx rendering

main-ai adds Vulkan in four steps. Each step builds on the one before it.

1. configure detects Vulkan (4512940a9).
2. A new output, `output=vulkan`, for SDL2 builds (08b97c801).
3. Vulkan support in the in-tree SDL 1.x (`vs/sdl`), so the same output also works in SDL1 builds (afbaa7038).
4. The 3dfx Voodoo emulation can render with Vulkan on the output's device (`voodoo_card=vulkan`, d14cdb453).

The SDL1 Wayland driver (c5beb2338) has a small Vulkan part as well. It is mentioned below and described in the Wayland section.

CHANGELOG wording on main-ai: "Add Vulkan support. Add Vulkan output driver. Add Vulkan support to SDL1. Add Wayland support to SDL1." and "3Dfx hardware rendering now supports OpenGL and Vulkan APIs."

Design points that apply to all of it:

- **No link-time dependency on Vulkan.** The executable never links the Vulkan loader. The loader is opened at runtime through SDL (`SDL_Vulkan_LoadLibrary`). Every Vulkan entry point is fetched with `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`, and `VK_NO_PROTOTYPES` is defined before `<vulkan/vulkan.h>` is included. If the loader or a usable device is missing, the output falls back to `surface`. Only the Vulkan headers are needed at build time.
- **No shader compiler at build time or runtime.**
  - The output's two shaders are SPIR-V 1.0 word arrays embedded in the source. The source comment says they were "hand assembled" and gives the equivalent GLSL.
  - The 3dfx emulation writes its SPIR-V at runtime with a small builder class.
  - The blobs are opaque, so a human re-implementation can produce them differently. One option is to compile the GLSL given below with `glslangValidator -V` or `glslc` and paste the words in. Another is to reuse the runtime builder approach. Either is fine as long as the result is SPIR-V 1.0 with the interface described here.
- **One device, one queue, strict submission order.** The 3dfx emulation renders with the output's `VkDevice` and `VkQueue`. Its synchronisation with the output relies on queue submission order plus pipeline barriers; no semaphores are used between the two.

---

### 1. configure: detect Vulkan (commit 4512940a9)

**What:** configure gets an `--enable-vulkan`/`--disable-vulkan` option (default: enabled). It also gets a `C_VULKAN` define in config.h, substituted variables `VULKAN_CFLAGS` and `VULKAN_LIBS`, and an automake conditional `C_VULKAN`.

**Why:** The output and the 3dfx code need the Vulkan headers and must be compiled only when they exist.

The flags are deliberately not added to the global `CFLAGS`/`LIBS`. The binary does not link the loader, and only the Makefiles that need the headers use `VULKAN_CFLAGS`.

**How:**
- In `configure.ac`, after the Metal block (`AM_CONDITIONAL([C_METAL] ...)`):
  - add `AH_TEMPLATE(C_VULKAN, ...)` and an `AC_ARG_ENABLE(vulkan, ...)`.
- If enabled, first try `pkg-config --exists vulkan`. If that succeeds, take `VULKAN_CFLAGS` from `pkg-config vulkan --cflags` and `VULKAN_LIBS` from `--libs`.
- Otherwise, loop over the library names `vulkan`, `vulkan-1` (Windows) and `MoltenVK` (macOS). For each, run an `AC_LINK_IFELSE` with that library appended to `LIBS`, using a test program that includes `<vulkan/vulkan.h>` and calls `vkEnumerateInstanceExtensionProperties`. On success, set `VULKAN_LIBS` to that `-l` flag and stop. Restore `LIBS` afterwards.
- If found, `AC_DEFINE(C_VULKAN,1)`; otherwise clear both variables.
- `AC_SUBST` both variables and add `AM_CONDITIONAL([C_VULKAN], ...)`.
- `VULKAN_LIBS` is substituted but no Makefile on main-ai uses it, since the loader comes in through SDL at runtime. Keep it for completeness or drop it.

**Testing:** Run configure with and without the Vulkan development headers installed, and with `--disable-vulkan`. Check `C_VULKAN` in config.h and the configure output line "whether Vulkan support will be enabled".

---

### 2. Vulkan output for SDL2 builds (commit 08b97c801)

**What:** A new output driver, `src/output/output_vulkan.cpp` and `output_vulkan.h`, selected with `output=vulkan`, from the Video > Output menu, or as the TTF `outputswitch` target.

- The emulator's 32-bit frame is uploaded to a GPU texture. Only the changed lines are uploaded.
- The texture is drawn scaled into `sdl.clip`.
- SDL drawn menus keep working: they are drawn into a window-sized ARGB overlay surface that the output composites on top of the frame with alpha blending.

**Why:** It is a modern GPU path, an alternative to OpenGL that is the native API on some platforms (e.g. Linux with Mesa, MoltenVK on macOS). It also serves as the base for 3dfx rendering with Vulkan.

**Depends on:** Step 1.

#### How: build and feature macros

- `include/dosbox.h`: define `C_VULKAN_OUTPUT` when `C_VULKAN` is set and the build is SDL2 but not SDL3. Step 3 widens this to SDL1 builds. All output code is guarded by `C_VULKAN_OUTPUT`, not `C_VULKAN`.
- `src/output/Makefile.am`: inside `if C_VULKAN`, add `$(VULKAN_CFLAGS)` to `AM_CPPFLAGS` and add `output_vulkan.cpp` to `liboutput_a_SOURCES`.
- `include/sdlmain.h`: add `SCREEN_VULKAN` to `enum SCREEN_TYPES` (guarded). Declare two new helpers, `GFX_UpdateWindowSurface()` and `GFX_UpdateWindowSurfaceRects(rects, n)`. They are SDL2-only in this commit and become unconditional in step 3.

#### How: the public API (`output_vulkan.h`)

- `OUTPUT_VULKAN_Select()`, `OUTPUT_VULKAN_GetBestMode(flags)`, `OUTPUT_VULKAN_SetSize()`, `OUTPUT_VULKAN_StartUpdate(pixels, pitch)`, `OUTPUT_VULKAN_EndUpdate(changedLines)`, `OUTPUT_VULKAN_Shutdown()`. These follow the same pattern as the other outputs.
- `OUTPUT_VULKAN_ReleaseWindow()`: destroys the swapchain and `VkSurfaceKHR`. It must be called before the SDL window they belong to is destroyed.
- `OUTPUT_VULKAN_UpdateOverlay()`: called wherever code would otherwise call `SDL_UpdateWindowSurface(Rects)` after drawing to `sdl.surface`. It returns false if `sdl.surface` is not the Vulkan overlay; the caller then does the normal SDL update.

#### How: integration into sdlmain/menus

- **`src/gui/sdlmain.cpp`:**
  - Include `output_vulkan.h`.
  - Implement `GFX_UpdateWindowSurface()` and `GFX_UpdateWindowSurfaceRects()`. Each first tries `OUTPUT_VULKAN_UpdateOverlay()` and otherwise calls `SDL_UpdateWindowSurface(Rects)` on `sdl.window`.
  - Replace every direct `SDL_UpdateWindowSurface(Rects)` call in the menu and blank-display code with these helpers: `BlankDisplay`, `GFX_DrawSDLMenu`, `MenuFullScreenRedraw`, the menu-attention drop-shadow update in `HandleMouseButton`, and in `menu.cpp` `updateScreenFromItem` / `updateScreenFromPopup`.
- **`GFX_SetSDLWindowMode()`:**
  - Before `SDL_DestroyWindow`, call `OUTPUT_VULKAN_ReleaseWindow()`. This matters because the window is recreated whenever the screen type changes (mapper, configuration GUI, output switches). A `VkSurfaceKHR` must not outlive its window.
  - Add `SDL_WINDOW_VULKAN` to the creation flags when the type is `SCREEN_VULKAN`.
  - For `SCREEN_VULKAN`, do not call `SDL_GetWindowSurface()`. Set `sdl.surface` to NULL and let the output install its overlay. Asking SDL2 for a window surface on a Vulkan window would make SDL draw to it. This applies in both places the function sets `sdl.surface`.
- **Dispatch:** add `SCREEN_VULKAN` cases to `GFX_GetBestMode`, `GFX_SetSize`, `GFX_StartUpdate`, `GFX_EndUpdate` and `GUI_ShutDown`, calling the functions above.
- **`GFX_GetRGB`:** `SCREEN_VULKAN` returns BGRA with alpha forced to 255, i.e. (blue << 0) | (green << 8) | (red << 16) | (255 << 24). The alpha is needed because the overlay uses alpha to tell drawn pixels from transparent ones, so whatever the menu code draws must come out opaque.
- **`GUI_StartUp`:**
  - Add `"vulkan"` to the xBRZ allowed-output list.
  - `output == "vulkan"` calls `OUTPUT_VULKAN_Select()`.
  - The initial 640x400 `GFX_SetSDLSurfaceWindow` is skipped when `want_type == SCREEN_VULKAN`, because Select already made the window.
- **Settings and menus:**
  - Add `"vulkan"` to the `output` values and help text.
  - Add `"vulkan"` to the TTF `switchoutputs[]` list in `src/dosbox.cpp`. That list is not ifdef'd so that the reference config lists every value.
  - Add a menu item `output_vulkan` ("Vulkan") in `menu.cpp` (`def_menu_video_output`) and `menu_callback.cpp` (`AllocCallback1`, using `output_menu_callback`).
- **`src/output/output_tools.cpp`:**
  - `change_output()` gets case 15, which calls `OUTPUT_VULKAN_Select()`.
  - `OutputSettingMenuUpdate()` checks the `output_vulkan` item.
  - `toOutput("vulkan")` calls `change_output(15)`, re-maximising the window on Windows if it was maximised, like the other outputs.
- **`src/output/output_ttf.cpp`:** `SetOutputSwitch` maps "vulkan" to 15, and `ttf_switch_off` maps 15 back to "vulkan".
- **`MenuShadeRect` (`menu.cpp`, 32 bpp path):** with Vulkan active, the overlay has an alpha channel.
  - Where a pixel is fully transparent (alpha 0), write translucent black: alpha 0xC0, colour 0, using the surface's `Ashift`/`Amask`. That darkens the emulator image underneath.
  - Otherwise darken the colour as before (shift right by 2) and keep its alpha.
  - Without this, shaded areas over the transparent overlay would have no effect.

#### How: Vulkan objects and their lifetimes (`output_vulkan.cpp`)

All state is in one static struct.

**Function loading.** Three X-macro lists of `PFN_vk*` statics, loaded at three levels:

1. Global: `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties`, via `vkGetInstanceProcAddr(NULL, ...)`.
2. Instance: physical device queries, surface queries, `vkDestroySurfaceKHR`, `vkCreateDevice`, `vkGetDeviceProcAddr`.
3. Device: everything else, via `vkGetDeviceProcAddr`.

A missing function logs "VULKAN: missing name" and fails the step.

**Library (process lifetime).** `VK_LoadLibrary()` calls `SDL_Vulkan_LoadLibrary(NULL)`, then `SDL_Vulkan_GetVkGetInstanceProcAddr()`, then loads the global functions. It is done once, in `OUTPUT_VULKAN_Select()` and again in `SetSize()` as a no-op. This must happen before creating a window with `SDL_WINDOW_VULKAN`.

**Instance (created once, on the first window).** `VK_CreateInstance()`:

- Extensions come from `SDL_Vulkan_GetInstanceExtensions(window, ...)`.
- If the loader offers `VK_KHR_portability_enumeration`, also enable it and set `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`. Without this, MoltenVK devices are not enumerated. The check is compiled only if the header defines `VK_KHR_portability_enumeration`.
- API version 1.0; application and engine name "DOSBox-X".
- No validation layers are enabled in code. Use the loader environment variable for testing.

**Surface (per SDL window).** `VK_AttachWindow()`:

- Checks that the window has the Vulkan flag.
- If the existing surface was made for another window, releases it.
- Creates the instance if needed, then calls `SDL_Vulkan_CreateSurface()`.
- Remembers the window pointer the surface belongs to (`surface_window`). `VK_Present` refuses to draw if the current window is not that window.

**Physical device and logical device (created once, on the first surface).** `VK_CreateDevice()`:

- Enumerates physical devices and skips any without `VK_KHR_swapchain`.
- For each device, takes the first queue family that has `VK_QUEUE_GRAPHICS_BIT`, supports presenting to the surface, and whose `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` succeeds.
  - The last check was added later, by the Wayland test fixes (0778d87cd). Some drivers claim present support and then fail, e.g. RADV on a compositor without dmabuf; skipping them lets lavapipe be picked.
- Scores devices: discrete 4, integrated 3, virtual 2, other 1. The best one wins, and its name is logged.
- Creates one queue in that family, enabling `VK_KHR_swapchain` plus `VK_KHR_portability_subset` if the device lists it (required on MoltenVK).
- Loads the device functions and gets the queue.
- When a later window gets a new surface, the existing device is kept. The code only checks `vkGetPhysicalDeviceSurfaceSupportKHR` for the chosen family and fails if presentation is no longer possible.

**Device objects (once, with the device).** `VK_CreateDeviceObjects()` creates:

- A command pool with `RESET_COMMAND_BUFFER_BIT`, and one primary command buffer.
- One semaphore, `image_acquired`.
- One fence, `frame_fence`, created signalled.
- A descriptor set layout with one combined image sampler at binding 0, fragment stage.
- A descriptor pool for 3 sets, and the 3 sets allocated from it: frame, overlay, and "external". The external set is added in step 4; step 2 has 2 sets.
- A pipeline layout with that set layout plus a vertex-stage push constant range of 8 floats.
- Two samplers, nearest and linear, both clamp-to-edge with `maxLod` 0.

**Textures (`VulkanTexture`).** Each texture has:

- a `VK_FORMAT_B8G8R8A8_UNORM` optimal-tiling image (usage `TRANSFER_DST | SAMPLED`), preferring `DEVICE_LOCAL` memory, falling back to any type;
- an image view;
- a persistently mapped staging buffer of width × height × 4 bytes, `TRANSFER_SRC`, in `HOST_VISIBLE | HOST_COHERENT` memory;
- its descriptor set and the sampler last written to it. The descriptor is only rewritten when the sampler changes.

An `initialized` flag records whether the image holds valid contents in `SHADER_READ_ONLY_OPTIMAL`.

`VK_CreateTexture` reuses the texture if the size is unchanged; the caller must make sure the GPU is idle. There are two textures:

- `frame`: the emulator image, at `sdl.draw.width` × `height`, or that times the xBRZ scale factor when xBRZ is active.
- `overlay`: the menu overlay, at window size.

**Render pass and pipelines (recreated only when the swapchain format changes).** `VK_CreateRenderPass(format)`:

- One colour attachment: load op CLEAR, store op STORE, initial layout UNDEFINED, final layout `PRESENT_SRC_KHR`.
- One subpass.
- One external→0 dependency on `COLOR_ATTACHMENT_OUTPUT` (dst access `COLOR_ATTACHMENT_WRITE`), so writes wait for the acquired image.
- Two graphics pipelines with the same shaders:
  - `pipeline_opaque`: no blending, for the frame.
  - `pipeline_blend`: colour `SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA`, alpha `ONE` / `ZERO`, for the overlay.
- Both use triangle-strip topology, no vertex inputs, no culling, and dynamic viewport and scissor.

**Swapchain (per window and size).** `VK_CreateSwapchain()` is called from `VK_AttachWindow` and whenever `VK_Present` finds it missing, dirty, or made for a different drawable size. It:

- Waits for the device to go idle and destroys framebuffers, views and per-image semaphores.
- Takes `currentExtent`, or the SDL drawable size if that is 0xFFFFFFFF (e.g. Wayland), clamped to min/max. If the extent is 0 (minimised), it returns without a swapchain and simply does not draw.
- Picks a surface format: B8G8R8A8 or R8G8B8A8 *UNORM* with `SRGB_NONLINEAR` colour space, so the emulator's colours are not gamma-corrected again. Otherwise it takes the first format listed, or B8G8R8A8_UNORM if that is UNDEFINED.
- Image count is `minImageCount + 1`, capped at `maxImageCount`.
- Composite alpha is `OPAQUE` if supported, else INHERIT, else PRE_MULTIPLIED, else POST_MULTIPLIED.
- Pre-transform is IDENTITY if supported, else the current transform.
- `clipped` is true. `oldSwapchain` is set to the previous one, which is destroyed right after the create call.
- Present mode: if `[vsync] vsyncmode=host`, FIFO. Otherwise MAILBOX if available, else IMMEDIATE, else FIFO.
- Then it ensures the render pass for the format exists, gets the swapchain images, and creates one view, one framebuffer and **one `render_done` semaphore per swapchain image**.
  - A semaphore per image (rather than per frame) avoids reusing a semaphore that the presentation engine may still be waiting on.
- It records the SDL drawable size it was made for.

**Release and shutdown.**
- `OUTPUT_VULKAN_ReleaseWindow()`: wait idle, destroy the swapchain resources, the swapchain and the surface. The device and instance stay.
- `OUTPUT_VULKAN_Shutdown()`: release the window; free the overlay `SDL_Surface` (clearing `sdl.surface` if it pointed there); destroy the textures, render pass and pipelines, samplers, layouts, pool, fence, semaphore, command pool, device and instance.
  - Step 4 adds: first tell the 3dfx emulation the device is going away (`voodoo_vk_device_lost()`), and clear the external image.
  - The loader is not unloaded.

#### How: the shaders

- **Vertex shader:** draws one rectangle as a 4-vertex triangle strip with no vertex buffer. The corner (cx, cy) is (index & 1, index >> 1), taken from `gl_VertexIndex`. Push constants are two vec4s:
  - `pos` (x, y, w, h in normalised device coordinates): output position = pos.xy + pos.zw × corner, z 0, w 1.
  - `uv` (u, v, du, dv): output texcoord = uv.xy + uv.zw × corner.
- **Fragment shader:** samples a combined image sampler at set 0, binding 0 with the texcoord and outputs the sample.

#### How: from the render buffer to the screen

1. **`OUTPUT_VULKAN_Select()`:**
   - Set `want_type = SCREEN_VULKAN` and `render.aspectOffload = true`, so the output does aspect correction (like OpenGL).
   - Make the window resizable.
   - Load the library, create a 640x400 window with `GFX_SetSDLWindowMode(..., SCREEN_VULKAN)`, attach it, set up the overlay, and call `ApplyPreventCap()`.
   - Any failure logs "Could not initialize Vulkan, switching back to surface" and sets `want_type = SCREEN_SURFACE`.
2. **`OUTPUT_VULKAN_GetBestMode()`:** requires `GFX_CAN_32`. Returns the flags with `GFX_SCALING` added and the 8/15/16-bit flags removed. The output only takes 32-bit BGRA.
3. **`OUTPUT_VULKAN_SetSize()`:**
   - `SetupWindowScaledVulkan()` is a copy of the OpenGL output's window sizing rules:
     - fixed fullscreen or window size, else the user-resized size or the maximised window;
     - menu bar scaling, with the menu height added to the window and to `sdl.clip.y`;
     - `aspectCorrectFitClip` / `aspectCorrectExtend` when `render.aspect` is on;
     - then `GFX_SetSDLWindowMode(w, h, SCREEN_VULKAN)`. If that fails in fullscreen, log, drop to windowed, `GFX_CaptureMouse()` and retry.
   - Step 4 adds `Voodoo_OGL_SetDisplaySize(clip.w, clip.h)` here.
   - Then `VK_AttachWindow()`, wait idle, set up the overlay at the window size, and (re)create the frame and overlay textures.
   - Allocate the CPU frame buffer (a `vector<uint32_t>` of the texture size, pitch = width × 4).
   - Choose linear or nearest sampling for the frame: nearest if `[render] aspect` is `nearest`, or if `clip` is an integer multiple of the texture size in both directions; otherwise linear.
   - Clear `deferred_resize`, set `must_redraw_all`, call `UpdateWindowDimensions()`, `GFX_LogSDLState()` and `ApplyPreventCap()`.
   - With SDL drawn menus, update `mainMenu.screenWidth/Height` from `sdl.surface`, `updateRect()`, `setRedraw()` and `GFX_DrawSDLMenu()`.
   - Present once without upload, so the menu and black background appear immediately.
   - Return `GFX_CAN_32 | GFX_SCALING`, or 0 on any failure, which makes the caller fall back.
4. **`OUTPUT_VULKAN_StartUpdate()`:** hands the renderer the CPU frame buffer, or `sdl_xbrz.renderbuf` (sized `draw.width × draw.height`) when xBRZ is scaling, and sets `sdl.updating`.
5. **`OUTPUT_VULKAN_EndUpdate(changedLines)`:**
   - If `must_redraw_all` is set and `changedLines` is NULL, do nothing.
   - With xBRZ: run `xBRZ_Render` from the render buffer into the CPU frame buffer, then present with a full upload.
   - Otherwise, if `changedLines[0] == draw.height` (nothing changed), present only if the overlay is dirty.
   - Else present with upload of the changed lines.
   - Count `frames++` unless `menu.hidecycles` is set or the window is fullscreen, matching the other outputs.
6. **`VK_Present(upload, changedLines)`** is the single place that draws:
   1. Return if there is no device or surface, or the surface is for another window.
   2. If the drawable size differs from the swapchain's, or the swapchain is dirty or missing, recreate it.
   3. `vkWaitForFences(frame_fence)`. The previous frame must be finished before the staging buffers and the command buffer are reused.
   4. `vkAcquireNextImageKHR` with `image_acquired`. On `OUT_OF_DATE`, recreate and acquire again. On other errors, mark the swapchain dirty and return. `SUBOPTIMAL` is accepted.
   5. Reset and begin the command buffer (one-time submit).
   6. Upload the frame (if `upload` is set):
      - `changedLines` uses the usual DOSBox run-length format: alternating counts of unchanged and changed lines, starting with unchanged.
      - Copy only the changed runs from the CPU buffer into the staging buffer at the same offsets, and build one `VkBufferImageCopy` per run.
      - Do a full copy if `changedLines` is NULL, the texture is not yet initialised, or its height differs from `draw.height` (the xBRZ case).
      - Record: barrier (SHADER_READ_ONLY or UNDEFINED → TRANSFER_DST, from the fragment-shader stage or top-of-pipe), `vkCmdCopyBufferToImage`, barrier (TRANSFER_DST → SHADER_READ_ONLY, transfer → fragment shader).
   7. Upload the overlay if it is dirty: copy the overlay surface row by row into staging (pitches may differ), upload the whole image the same way, and clear the dirty flag.
   8. Begin the render pass, clearing to opaque black. Set viewport and scissor to the swapchain extent.
   9. Draw, with all coordinates in the overlay surface's size (window coordinates, which is what `sdl.clip` is computed in):
      - if the external (3dfx) image is set: draw it into `sdl.clip` with the opaque pipeline;
      - else if the frame is initialised: draw the frame into `sdl.clip` with the opaque pipeline;
      - then, if the overlay is initialised: draw it over the whole window with the blend pipeline and the nearest sampler.
      - `VK_DrawRect` converts the rectangle to NDC by dividing by the window size, so on high-DPI displays (drawable larger than the window) it still covers the right area.
   10. End, then submit: wait on `image_acquired` at `COLOR_ATTACHMENT_OUTPUT`, signal `render_done[index]`, fence `frame_fence`.
       - The fence is reset just before submit. If the submit fails, wait idle, recreate the fence signalled (otherwise the next wait hangs forever), and mark the swapchain dirty.
   11. `vkQueuePresentKHR` waiting on `render_done[index]`. `OUT_OF_DATE` or `SUBOPTIMAL` marks the swapchain dirty for the next frame.

#### How: the menu overlay

`VK_SetupOverlay(w, h)` (re)creates a window-sized `SDL_Surface`: SDL2 uses `SDL_PIXELFORMAT_ARGB8888`; SDL1 uses an RGB surface with masks R 0x00FF0000, G 0x0000FF00, B 0x000000FF, A 0xFF000000. It then:

- fills the surface with 0, which is fully transparent;
- points `sdl.surface` at it;
- marks it dirty.

The rest of DOSBox-X draws menus into `sdl.surface` exactly as with `output=surface`. `OUTPUT_VULKAN_UpdateOverlay()` marks the overlay dirty and presents immediately, unless the emulator is in the middle of a frame (`sdl.updating`). In that case the overlay is shown with the frame at `EndUpdate`.

**Resize and fullscreen.**
- The window size comes from `SetupWindowScaledVulkan` through `GFX_SetSDLWindowMode`, which on SDL2 resizes or recreates the window and handles the fullscreen flags.
- The swapchain follows lazily: every present compares the SDL drawable size with the size the swapchain was made for, and recreates on mismatch or on `OUT_OF_DATE`/`SUBOPTIMAL`.
- If the window is recreated (screen-type change, mapper, configuration GUI, output switch), `GFX_SetSDLWindowMode` releases the surface first, and the next `SetSize` → `VK_AttachWindow` creates a new surface for the new window. The device and instance survive.

**Pitfalls and design notes:**
- Never let SDL2 create a window surface (`SDL_GetWindowSurface`) on the Vulkan window.
- Release the `VkSurfaceKHR` before the window is destroyed. Some platforms crash or leak otherwise.
- The staging buffers are not double-buffered. Correctness depends on the fence wait at the top of `VK_Present`, before any `memcpy` into staging.
- `GFX_GetRGB` must set alpha 255 for this output, or menu text and boxes will be invisible over the frame.

**Testing:**
- Start with `output=vulkan`, in windowed and fullscreen modes.
- Resize the window, maximise it, minimise and restore it (zero extent).
- Toggle the menu bar and open menus and popups. Shaded areas must darken the image.
- Use the mapper and the configuration GUI (window recreation), and switch outputs at runtime both ways via the menu and `toOutput`.
- Try TTF output with `outputswitch=vulkan`, xBRZ, `aspect=true/nearest`, `vsyncmode=host`, and high-DPI.
- Without a Vulkan loader, or with no usable device, the output must fall back to surface with a log message.
- Useful environment variables:
  - `VK_LOADER_LAYERS_ENABLE=*validation*` (or `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`) enables validation.
  - `VK_ICD_FILENAMES` / `VK_DRIVER_FILES` forces a driver; lavapipe gives a software Vulkan device.

---

### 3. Vulkan in the in-tree SDL 1.x, and Vulkan output for SDL1 builds (commit afbaa7038)

**What:** `vs/sdl` gets an SDL2-style Vulkan API, minus the window parameter since SDL 1.2 has only one window, plus a new `SDL_SetVideoMode()` flag `SDL_VULKAN`. It is implemented for the x11 and windib drivers. DOSBox-X SDL1 builds then enable the Vulkan output too.

**Why:** Stock SDL 1.2 has no Vulkan. DOSBox-X ships its own patched SDL 1.x, so Vulkan output in SDL1 builds needs it there.

**Depends on:** Steps 1 and 2.

#### How: SDL side (`vs/sdl`)

**`include/SDL_video.h`:** new flag `SDL_VULKAN` = 0x02000000. The value 0x40 was avoided because the SDL1 Direct3D output already passes that bit. The flag gives a window SDL never draws to: like `SDL_OPENGL` without a context, there are no pixels and no shadow surface, and `SDL_UpdateRects`/`SDL_Flip` do nothing.

**New public header `include/SDL_vulkan.h`** (also add it to `HDRS` in `vs/sdl/Makefile.in` so it is installed):
- It does not include `vulkan.h`. Unless `VULKAN_H_` is already defined, it declares `VkInstance` as a dispatchable handle and `VkSurfaceKHR` as a non-dispatchable one: a pointer on 64-bit targets, `Uint64` otherwise, following the usual Vulkan header logic for which platforms count as 64-bit.
- Typedefs `SDL_vulkanInstance` and `SDL_vulkanSurface`.
- Functions:
  - `SDL_Vulkan_LoadLibrary(path)`
  - `SDL_Vulkan_GetVkGetInstanceProcAddr()`
  - `SDL_Vulkan_UnloadLibrary()`
  - `SDL_Vulkan_GetInstanceExtensions(pCount, pNames)`
  - `SDL_Vulkan_CreateSurface(instance, surface)`
  - `SDL_Vulkan_GetDrawableSize(w, h)`
  - DOSBox-X-specific: `SDL_Vulkan_SetWindowDestroyCallback(callback, userdata)`, a function called right before SDL destroys the window.

**Internal header `src/video/SDL_vulkan_c.h`:**
- Declares the minimal Vulkan types needed: a VkResult-compatible int where 0 is success, a flags type, and a `__stdcall` calling convention on Windows.
- Helper functions: `SDL_Vulkan_GetInstanceProc(instance, name)`, `SDL_Vulkan_CopyExtensions(...)` (implements the count/array semantics) and `SDL_Vulkan_NotifyWindowDestroy()`.

**`src/video/SDL_vulkan.c`:**
- `LoadLibrary`:
  - Library path: the argument, else `SDL_VULKAN_LIBRARY` from the environment, else `vulkan-1.dll`, `libvulkan.1.dylib` or `libvulkan.so.1` by platform.
  - Loads it with `SDL_LoadObject` and looks up `vkGetInstanceProcAddr`.
  - Calls are reference-counted.
  - The library handle is kept in file statics, not in the video device, so it survives `SDL_QuitSubSystem(SDL_INIT_VIDEO)`.
- `GetInstanceExtensions` and `CreateSurface` forward to new video-device hooks.
  - `CreateSurface` checks that the loader is loaded and that `SDL_VideoSurface` has `SDL_VULKAN`.
- `GetDrawableSize` returns `SDL_VideoSurface`'s w/h. There is no high-DPI handling in SDL1.
- The destroy-callback setter stores a function pointer and userdata; `NotifyWindowDestroy` calls it.

**`src/video/SDL_sysvideo.h`:** `SDL_VideoDevice` gets two hooks, `Vulkan_GetInstanceExtensions(this, count, names)` and `Vulkan_CreateSurface(this, instance, surface)`.

**`src/video/SDL_video.c` (`SDL_SetVideoMode`):**
- With `SDL_VULKAN`: fail with an error if the driver has no `Vulkan_CreateSurface`, and strip `SDL_OPENGL`, `SDL_OPENGLBLIT`, `SDL_HWSURFACE`, `SDL_DOUBLEBUF` and `SDL_HWPALETTE`.
- After the driver's SetVideoMode: fail if the returned mode lacks `SDL_VULKAN`.
- Skip the "mode smaller than requested" check (as for OpenGL) and the shadow-surface creation (test `SDL_OPENGL|SDL_VULKAN`).
- `SDL_UpdateRects` and `SDL_Flip` return early with an error on a Vulkan screen.
- `SDL_VideoQuit` calls `SDL_Vulkan_NotifyWindowDestroy()` before `VideoQuit`.

**x11 driver:**
- New files `SDL_x11vulkan.c` / `_c.h`:
  - extensions `VK_KHR_surface` and `VK_KHR_xlib_surface`;
  - `CreateSurface` fetches `vkCreateXlibSurfaceKHR` through the instance, does `XSync` so the server knows the window, fills a locally declared `VkXlibSurfaceCreateInfoKHR` (sType constant 1000004000; display and window) and calls it.
- `SDL_x11video.c`:
  - registers the hooks;
  - `X11_DestroyWindow` calls `NotifyWindowDestroy` when it owns the window (no `SDL_windowid`) and clears `SDL_VULKAN` from the screen flags;
  - `X11_SetVideoMode` treats a change of the `SDL_VULKAN` bit like a change of `SDL_OPENGL`, and carries `SDL_VULKAN` into `current->flags`.
- `SDL_x11image.c` (`X11_ResizeImage`): no XImage for `SDL_OPENGL|SDL_VULKAN`.
- `SDL_x11events.c`: on ConfigureNotify, OpenGL and Vulkan windows get `SDL_PrivateExpose()` so the application redraws.

**windib driver** (the commit says this was neither compiled nor tested):
- New files `wincommon/SDL_winvulkan.c` / `_c.h`, active only if not HX-DOS and not WinCE (`SDL_WIN32_VULKAN`): extensions `VK_KHR_surface` and `VK_KHR_win32_surface`; `vkCreateWin32SurfaceKHR` with a local create-info struct (sType 1000009000; `SDL_Instance`, `SDL_Window`).
- `SDL_dibvideo.c`:
  - registers the hooks;
  - the "same window, just resize" fast path also applies to `SDL_VULKAN`;
  - no DIB section for Vulkan, and `screen_bmp` is reset to NULL after `DeleteObject`;
  - Vulkan modes set `SDL_VULKAN` on the video surface with `pixels = NULL`;
  - `DIB_WinPAINT` returns if there is no bitmap.
- `SDL_sysevents.c`: WM_PAINT does not blit for Vulkan screens and calls `SDL_PrivateExpose()` instead.
- `SDL_dibevents.c` (`DIB_DestroyWindow`) and `SDL_wingl.c` (`WIN_GL_ResetWindow`) call `NotifyWindowDestroy` before `DestroyWindow`.
- Add the new .c/.h files to `vs/sdl/VisualC/SDL/SDL.vcxproj`.
- The autotools build picks them up through the existing `src/video/*.c`, `x11/*.c` and `wincommon/*.c` globs in `vs/sdl/configure.ac`.

#### How: DOSBox-X side

- `configure.ac`: if Vulkan was found and the build is SDL1 (`SDL_STRING = SDL1`), `AC_CHECK_HEADER(SDL_vulkan.h)` (with `SDL.h` included first) defines `C_SDL1_VULKAN`. If the header is missing, it warns that the in-tree SDL 1.x is needed.
- `include/dosbox.h`: `C_VULKAN_OUTPUT` is also defined for non-SDL2 builds that have `C_VULKAN && C_SDL1_VULKAN`.
- `include/sdlmain.h`: `GFX_UpdateWindowSurface*()` are declared for all builds.
- `sdlmain.cpp`: SDL1 implementations try the Vulkan overlay first, then call `SDL_Flip` or `SDL_UpdateRects`. All remaining SDL1 `#else` branches that called `SDL_Flip`/`SDL_UpdateRects` directly in the menu code now call the helpers.
- `GUI_StartUp` (SDL1):
  - skip the initial `SDL_SetVideoMode(640,400,...)` when `want_type == SCREEN_VULKAN`;
  - take `sdl.desktop.bpp` from `SDL_GetVideoSurface()` rather than `sdl.surface`, which with Vulkan is the overlay, not the video surface.
- `output_vulkan.cpp` hides SDL1/SDL2 differences in small helpers:
  - `VK_CurrentWindow()`: SDL2 returns `sdl.window`. SDL1 returns the address of a static sentinel while a video surface exists. This is valid because SDL1 announces window destruction through the callback, so a surface is good for as long as the sentinel is non-NULL.
  - `VK_WindowIsVulkan()`: `SDL_WINDOW_VULKAN` on SDL2, `SDL_VULKAN` in the video surface flags on SDL1.
  - `VK_GetWindowSize()` and `VK_GetDrawableSize()`.
  - `VK_SetVideoMode(w, h)` (SDL1 only) calls `SDL_SetVideoMode(w, h, 0, ...)` with `SDL_VULKAN` plus `SDL_FULLSCREEN` or `SDL_RESIZABLE`, and adds `SDL_HAX_NORESIZEWINDOW` when `setSizeButNotResize()` says so (`SDL_DOSBOX_X_SPECIAL`). It clears `deferred_resize`.
  - `VK_LoadLibrary` registers the SDL1 window-destroy callback, which calls `OUTPUT_VULKAN_ReleaseWindow()`. Shutdown unregisters it.
  - Instance extensions and surface creation use the window-less SDL1 calls.
  - Select and `SetupWindowScaledVulkan` use `VK_SetVideoMode` on SDL1 (with the same fullscreen → windowed fallback).
  - `VK_SetupOverlay` on SDL1 copies `SDL_RESIZABLE`/`SDL_FULLSCREEN` from the video surface into the overlay surface's flags, because SDL1 code elsewhere checks `sdl.surface->flags` to learn the window state.
- Remove the "(SDL2 builds)" note from the `output` help text.
- Behaviour change versus step 2: `ReleaseWindow` no longer frees the overlay surface. It is kept and re-set-up at the next SetSize.

**Pitfalls:**
- `SDL_SetVideoMode` on X11 may destroy and recreate the window when flags change. Without the destroy callback, the `VkSurfaceKHR` would dangle.
- In SDL1, `sdl.surface` with Vulkan is not the video surface; check every place that reads its flags or format.
- Other SDL1 drivers fail `SDL_SetVideoMode` with `SDL_VULKAN`, which makes the output fall back to surface.

**Testing:** Build an SDL1 build against the in-tree SDL and repeat the step 2 tests on X11. Pay particular attention to window recreation (fullscreen toggles, mapper, config GUI) and exit, where `SDL_VideoQuit` triggers the callback. Windows (windib) needs a first compile and test.

---

### 4. Vulkan windows on the SDL1 Wayland driver (part of c5beb2338)

The Wayland driver for the in-tree SDL 1.x (see the Wayland section) also implements the hooks from step 3:

- `SDL_waylandvulkan.c`: extensions `VK_KHR_surface` and `VK_KHR_wayland_surface`. It creates the surface with `vkCreateWaylandSurfaceKHR` from the driver's `wl_display` and `wl_surface`, using a locally declared create-info struct, as for X11.
- An `SDL_VULKAN` mode is a separate content mode of the driver window. EGL, Vulkan and wl_shm cannot share a `wl_surface`, so it is recreated when the mode changes.
- `SDL_Vulkan_NotifyWindowDestroy()` is called before a Vulkan-mode surface goes away.

Implement this after step 3 and as part of the Wayland driver. A related DOSBox-X fix from the Wayland testing, the device-selection check on surface capabilities, is described in step 2.

---

### 5. 3dfx Voodoo rendered with Vulkan (commit d14cdb453)

**What:** A third 3dfx back end, `src/hardware/voodoo_vulkan.cpp` and `.h`, equivalent to the OpenGL one. It needs `output=vulkan` and renders on the output's device.

- Selected with `[voodoo] voodoo_card=vulkan`.
- `voodoo_card=auto` now picks OpenGL or Vulkan to match the output in use at the moment the 3dfx output turns on, and software otherwise.
- `voodoo_resolution` (native/double/display) works as with OpenGL.

**Why:** With `output=vulkan`, hardware-accelerated 3dfx would otherwise not be available.

**Depends on:** Step 2, and the 3dfx OpenGL render-to-texture change (commit 00fd06f7a, described in its own section). That change introduced the render-to-texture model, `voodoo_present()` at vertical retrace, `voodoo_activate()`, the `voodoo_resolution` setting and menu, and `Voodoo_OGL_SetDisplaySize()`. The Vulkan back end is a port of that design.

#### How: refactoring and dispatch (prerequisites inside this commit)

- **`include/voodoo.h`:** keep `VOODOO_EMU_TYPE_OFF` (0), `SOFTWARE` (1) and `ACCELERATED` (2, which now means OpenGL). Add `VOODOO_EMU_TYPE_VULKAN` (3) and `VOODOO_EMU_TYPE_AUTO` (4).
- **`src/hardware/voodoo.cpp`:**
  - Parse "software", "opengl" (only `#if C_OPENGL`), "vulkan" (only `#if C_VULKAN_OUTPUT`) and "auto". Auto maps to `AUTO` if either API is compiled in, else to `SOFTWARE`.
  - Replace the bare 1/2 in all the `switch (emulation_type)` statements with the four named types.
- **`src/dosbox.cpp`:** add "vulkan" to the `voodoo_card` values (guarded), and expand the help text to explain software/opengl/vulkan/auto/false.
- **`voodoo_data.h`:** in `voodoo_state`, add `int hw_request` (what `voodoo_card` asked for) and `bool vulkan`. `ogl` now means "a hardware back end is in use (OpenGL or Vulkan)".
- **`voodoo_interface.cpp`:**
  - `Voodoo_Initialize` accepts types 1..4 and stores `hw_request`.
  - It no longer sets `ogl` up front; both flags start false.
- **`voodoo_emu.cpp`:**
  - Include the new `voodoo_hw.h` instead of `voodoo_opengl.h`, and change every `voodoo_ogl_*` call to `voodoo_hw_*`. Some `#if C_OPENGL` guards around update_dimensions and leave are removed, because the dispatch handles it.
  - **`voodoo_activate()`**, called when the 3dfx output turns on:
    1. Set `active`; clear `ogl` and `vulkan`.
    2. Decide from `hw_request`: ACCELERATED means OpenGL; VULKAN means Vulkan; AUTO means Vulkan if `OUTPUT_VULKAN_Active()`, else OpenGL; anything else means software (return).
    3. Set `ogl = true`, and `vulkan` accordingly, then call `voodoo_hw_init`.
    4. On success, `voodoo_hw_clear()` and `voodoo_hw_load_framebuffers_from_ram()`, because anything drawn before was drawn in software into card memory.
    5. On failure, clear both flags and log "acceleration disabled", which leaves the software path.
    - With AUTO and a non-OpenGL output, the OpenGL init fails, so the result is software.
  - `voodoo_leave()` also clears `ogl` and `vulkan`, so the choice is made again every time.
- **New `src/hardware/voodoo_hw.h`:**
  - One small static inline wrapper per back-end function: `set_window`, `present`, `swap_buffer`, `vblank_flush`, `update_dimensions`, `follow_dimensions`, `clear`, `load_framebuffers_from_ram`, `fastfill`, `clip_window`, `texture_clear`, `invalidate_paltex`, `draw_triangle`, `draw_pixel`, `draw_z`, `draw_pixel_pipeline`, `read_pixel`, `init`, `leave`, `shutdown`.
  - Each calls `voodoo_vk_*` if `v->vulkan`, else `voodoo_ogl_*`.
  - `voodoo_vulkan.h` declares the matching `voodoo_vk_*` set.
- **`voodoo_opengl.cpp` / `.h`:** move what both back ends need out of the `#if C_OPENGL` block:
  - the resolution mode state and `Voodoo_OGL_Set/Get/ParseResolutionMode`;
  - `Voodoo_OGL_SetDisplaySize`;
  - new `Voodoo_HW_PollResolutionSetting()`, which reads `voodoo_resolution` and refreshes the menu when it changed;
  - new `Voodoo_HW_RenderSize(max_size, w, h)`: the 3dfx mode size, doubled, or at least the display size in each direction; never smaller than the mode; clamped to `max_size` and at least 1;
  - the vertex setup `ogl_get_depth`, `ogl_get_fog_blend` and `ogl_get_vertex_data`, plus the `ogl_vertex_data`/`ogl_vertex_map_data` structs in the header.

  The OpenGL back end now calls these, so its behaviour must stay the same. The no-OpenGL stub section must provide every `voodoo_ogl_*` function the dispatch references (`load_framebuffers_from_ram`, `follow_dimensions`, `present`, etc.).
- **Menus:** the resolution menu items and `Voodoo_UpdateResolutionMenu` (`menu.cpp`, `menu_callback.cpp`, and the startup code in `sdlmain.cpp`) are now built when `C_OPENGL || C_VULKAN_OUTPUT`.
- **Build:**
  - Add `voodoo_vulkan.cpp` unconditionally to `src/hardware/Makefile.am`. The file compiles to stubs without `C_VULKAN_OUTPUT`.
  - Add `voodoo_vulkan.cpp`, `voodoo_vulkan.h` and `voodoo_hw.h` to `vs/dosbox-x.vcxproj` and `.filters`.
  - Note: `src/hardware/Makefile.am` does not add `$(VULKAN_CFLAGS)`, so this file relies on the Vulkan headers being on the default include path. Add the flags there if they are not.
  - The Visual Studio config (`vs/config.h`) does not define `C_VULKAN` on main-ai, so MSVC builds get the stubs.

#### How: output hooks (`output_vulkan.cpp` and new `src/output/output_vulkan_device.h`)

- `struct OUTPUT_VULKAN_Device`: instance, physical device, device, queue, queue family, and the `vkGetInstanceProcAddr` pointer. The 3dfx code loads its own function pointers from these.
- `bool OUTPUT_VULKAN_GetDevice(dev)` / `bool OUTPUT_VULKAN_Active()`: true when `sdl.desktop.type == SCREEN_VULKAN` and the device exists.
- `OUTPUT_VULKAN_SetExternalImage(view, w, h)` makes the output draw this image view instead of the emulator frame (VK_NULL_HANDLE stops). It uses a third descriptor set (`vk.external`; the pool grows from 2 to 3 sets) whose descriptor is rewritten when the view changes.
  - Contract: the image must be in `SHADER_READ_ONLY_OPTIMAL` whenever the output draws.
  - In `VK_Present`, the external image takes precedence over the frame. It is sampled with nearest filtering when `sdl.clip` is an integer multiple of its size, linear otherwise.
- `OUTPUT_VULKAN_PresentExternal()`: if an external image is set, call `VK_Present` without upload and count `frames++`.
- `voodoo_vk_device_lost()`: implemented by the 3dfx code; the output calls it first in Shutdown.
- `SetupWindowScaledVulkan` calls `Voodoo_OGL_SetDisplaySize(clip.w, clip.h)`, so `voodoo_resolution=display` can follow the window size.

#### How: Vulkan objects in the 3dfx back end

State lives in one static struct (`vvk`) plus per-TMU texture maps. Device functions are loaded from the shared device: instance-level `vkGetDeviceProcAddr`, properties, memory properties and format properties, then the device-level list.

**`voodoo_vk_init`** gets the output's device or fails with "needs output=vulkan". It then:

- loads the functions;
- chooses the depth format (below);
- reads memory properties, and sets `max_size` to the minimum of `maxImageDimension2D`, `maxFramebufferWidth` and `maxFramebufferHeight`;
- sets `ready`;
- polls the resolution setting;
- creates the device objects and the targets (`vvk_reset_videomode`);
- logs "rendering 3dfx graphics with Vulkan on device name".

**Depth format:**
- First try formats with stencil: `D24_UNORM_S8_UINT`, then `D32_SFLOAT_S8_UINT`. Without stencil, try `D32_SFLOAT`, then `D16_UNORM`, and log "no stencil buffer".
- The format must support `DEPTH_STENCIL_ATTACHMENT` in optimal tiling.
- Record whether the format supports `BLIT_SRC` and `BLIT_DST` (`depth_blit`). This decides whether depth survives a render-resolution change.

**Device objects (`vvk_create_device_objects`), kept until the 3dfx output turns off or the device is lost:**
- **One render pass for everything.**
  - Attachment 0: colour, `R8G8B8A8_UNORM`.
  - Attachment 1: depth/stencil.
  - Load op LOAD and store op STORE for colour, depth and stencil.
  - Initial and final layouts are the attachment-optimal layouts. Design rule: the colour and depth images stay in their attachment layouts permanently, and are only moved to TRANSFER_SRC/DST around copies and immediately back (`vvk_color_to`, `vvk_depth_to`). This avoids layout tracking.
  - Two subpass dependencies:
    - external→0: from colour/depth writes and transfer writes, to colour/depth access and fragment-shader reads;
    - 0→external: from colour/depth writes, to colour/depth access and transfer read/write.
    - Together they order back-to-back render passes and copies/uploads recorded between them.
- **Descriptor set layout:** two combined image samplers, bindings 0 and 1 (TMU0 and TMU1), fragment stage.
- **Pipeline layout:** that set plus a fragment-stage push constant block, `VvkPushConstants`, 72 bytes:
  - offset 0, vec4: color0 (RGBA / 255)
  - offset 16, vec4: color1
  - offset 32, vec4: chromaKey (rgb)
  - offset 48, vec4: fogColor (rgb, a = 1)
  - offset 64, float: alpha reference (alphaMode alpha byte / 255)
  - offset 68, float: zaColor (low 16 bits / 65535)
- **Shader modules:** the triangle vertex shader, the rectangle vertex shader and the flat fragment shader are built once. Triangle fragment shaders are built on demand and cached.
- **Command pool** (reset-command-buffer flag), and a **ring of 3 slots** (`VVK_SLOTS`). Each slot has:
  - a primary command buffer and a fence (created unsignalled);
  - a list of "arenas": host-visible mapped buffers used for vertices and upload data, usage `VERTEX_BUFFER | TRANSFER_SRC`, at least 4 MiB or twice the request, preferring `HOST_CACHED | HOST_COHERENT`;
  - a list of descriptor pools (512 sets / 1024 descriptors each);
  - a descriptor-set cache;
  - a "trash" list of images and framebuffers to destroy once the slot's fence has signalled.
- **A 1x1 white texture** (BGRA8, cleared to 1.0), bound to any TMU that is not used.

**Command buffer lifecycle:**
- `vvk_begin()` starts recording into the current slot, if it is not already recording:
  1. If the slot was submitted, wait for its fence.
  2. Empty its trash, reset its descriptor pools and set cache, and rewind its arenas.
  3. Begin the command buffer (one-time submit) and reset the bound-state tracking.
- `vvk_submit(wait)`: end any render pass, end the command buffer, reset the fence and submit to the shared queue. Optionally wait on the fence. Advance to the next slot.
  - On submit failure, wait idle.
- The CPU can thus record the next command buffer while the GPU draws the previous ones.
- Objects that in-flight command buffers may still reference (texture images, old framebuffers) are never destroyed directly. `vvk_trash_image` defers them to the current slot's trash.

**Render passes and batching:**
- `vvk_begin_pass(target)` (0 or 1 = colour buffer index) begins the render pass on that buffer's framebuffer (colour + shared depth), with viewport and scissor covering the whole render target. It keeps the pass open across draws to the same target.
- Anything that needs to be outside a render pass (uploads, copies, clears via `vkCmdClearColorImage`) calls `vvk_end_pass()` first.
- `vvk_draw` copies the vertex data into the current arena and appends it to a pending batch. The batch must have the same pipeline, descriptor set, push constants and buffer, and the data must be contiguous.
- `vvk_flush_batch` binds only what changed (pipeline, set, push constants, vertex buffer), then issues one `vkCmdDraw`: triangles as a vertex count, rectangles as 4 vertices × N instances.
- Buffer swaps, clears, and ends of passes flush the batch.

**Render targets (`vvk_create_targets`), sized from `Voodoo_HW_RenderSize(max_size)`:**
- two colour images, `R8G8B8A8_UNORM`, usage `COLOR_ATTACHMENT | TRANSFER_SRC | TRANSFER_DST`;
- one depth/stencil image;
- a separate "present" image, `R8G8B8A8_UNORM`, usage `SAMPLED | TRANSFER_DST`;
- two framebuffers.

All images use device-local memory and are stored top row first. `vvk_init_targets` transitions them from UNDEFINED via TRANSFER_DST to their resting layouts:

- colour images are cleared to black and moved to COLOR_ATTACHMENT;
- depth is cleared to 1.0 with stencil 0 and moved to DEPTH_STENCIL_ATTACHMENT;
- the present image is cleared to opaque black and moved to SHADER_READ_ONLY.

**Video mode (`vvk_reset_videomode`), on init and on the first `update_dimensions`:**
1. Clear the external image.
2. Submit, wait, wait idle, and destroy the targets.
3. Call `RENDER_SetSize(fbi.width, fbi.height, 32, 60 Hz, 4:3)`. This comes first so that the output sizes its window and clip for the 3dfx mode, and the display size is known when the render size is computed.
4. Create and initialise the targets, submit and wait.
5. Set `front = 0`.
6. `OUTPUT_VULKAN_SetExternalImage(present view, w, h)`.

**Presenting (`voodoo_vk_present`, called by `voodoo_present()` at every 3dfx vertical retrace):**
1. Poll the render size and resize the targets if needed.
2. End the pass.
3. Front colour buffer → TRANSFER_SRC; present image SHADER_READ → TRANSFER_DST (from the fragment-shader stage, so it waits for the output's previous sampling).
4. `vkCmdCopyImage` of the whole buffer.
5. Move both images back to their resting layouts, with the present image's barrier making transfer writes visible to fragment-shader reads.
6. Submit without waiting.
7. `OUTPUT_VULKAN_PresentExternal()`.
- Correctness relies on the output's submit coming after this one on the same queue: the barriers order across submissions in submission order.
- The separate present image means the output always shows the last complete retrace, while the emulation keeps drawing into its buffers, including front-buffer drawing and LFB writes.

**Buffer swap (`voodoo_vk_swap_buffer`):** flush the batch and toggle `front`. "Back" is always `front ^ 1` (`vvk_target`).

**Render-resolution change (`vvk_resize_targets`)**, triggered when the setting, the display size or the 3dfx mode changes (`follow_dimensions`, `update_dimensions`, or `present`):
1. Submit, wait and wait idle; the output may still be drawing the old present image.
2. Create new targets.
3. Blit the old colour buffers into the new ones with `VK_FILTER_NEAREST`, so pixel values stay exact for LFB reads.
4. Blit depth/stencil (also nearest) if `depth_blit` is supported; otherwise clear depth.
5. Destroy the old images and point the output at the new present image.
6. On failure, restore the old targets.

#### How: shaders built at runtime (`SpvBuilder`)

A small class emits SPIR-V 1.0 words. Each instruction is encoded as word count << 16 | opcode, followed by its operands.

- **Sections:** annotations, types/constants/globals, and code, kept as separate vectors. `Finish(model)` assembles them in the order the SPIR-V spec requires: header (magic 0x07230203, version 1.0, generator 0, id bound, 0); `OpCapability Shader`; `OpMemoryModel Logical GLSL450`; `OpEntryPoint` named "main", listing all Input/Output variables as the interface; `OpExecutionMode OriginUpperLeft` for fragment shaders; then annotations, types and code.
- **Types** are deduplicated by (opcode, operands). Float and int constants are cached by value.
- **Helpers:** variables, Decorate/MemberDecorate, main function begin, Load/Store/AccessChain, and a generic `Op(opcode, type, operands)` returning a new id.
- **`KillIf(cond)`** emits a selection merge, a conditional branch to a block containing `OpKill`, and the merge label.
- **Opcodes used:**
  - `OpCompositeConstruct`, `OpCompositeExtract`, `OpVectorShuffle`
  - `OpFAdd`, `OpFSub`, `OpFMul`, `OpDot`
  - `OpFOrdLessThan`, `OpFOrdGreaterThan`, `OpFOrdLessThanEqual`, `OpFOrdGreaterThanEqual`
  - `OpSelect`
  - `OpImageSampleProjImplicitLod`: projective texture sampling, i.e. perspective-correct texturing from (s·w, t·w, w)
  - for the rect shader, `OpBitwiseAnd`, `OpShiftRightArithmetic` and `OpConvertSToF`

Shaders:

- **Triangle vertex shader.** Inputs:
  - location 0: vec3 position (NDC x, NDC y, depth);
  - location 1: vec4 colour;
  - locations 2 and 3: vec3 texture coordinates for TMU0 and TMU1, as (s·w, t·w, w);
  - location 4: vec3 extra (fog blend, LOD blend for TMU0, LOD blend for TMU1).

  It writes `gl_Position` = (pos, 1) and passes the four attributes to fragment locations 0..3.
- **Rectangle vertex shader** (LFB pixels), per instance: vec4 rect (x0, y0, x1, y1 in NDC), vec4 colour, float depth. From `gl_VertexIndex` it builds a 4-vertex strip with corner (index & 1, index >> 1), interpolating between the rect corners, at the given depth. It outputs the colour.
- **Flat fragment shader:** outputs the input colour.
- **Triangle fragment shader**, generated per `VvkFsKey` and cached in a map. The key holds:
  - texture mode 0 and 1 (0 if that TMU is unused);
  - fbzColorPath;
  - the low 4 bits of alphaMode (alpha test enable and function);
  - the low 6 bits of fogMode when fog is enabled, otherwise 0;
  - flags TMU0 used, TMU1 used, chroma key enabled.

  It is a step-by-step port of the OpenGL emulation's GLSL generator (`ogl_shaders`, `ogl_sh_tex_combine`, `ogl_sh_color_path`, `ogl_sh_fog`). Colours are carried as an rgb vec3 plus a separate alpha float:
  - Start with texel = 1 and cother = 0.
  - If TMU1 is used: sample it projectively, apply the texture combine for TMU1 with cother = 0, and the result becomes both cother and texel. Then, if TMU0 is used: sample, apply the TMU0 combine with cother = the TMU1 result, and the result becomes texel.
  - **Texture combine (per TEXMODE):**
    - start from cother, or zero if `TC_ZERO_OTHER` / `TCA_ZERO_OTHER`;
    - optionally subtract clocal;
    - multiply by a blend factor chosen by `TC_MSELECT` / `TCA_MSELECT`: 0, clocal, cother.a, clocal.a, or the per-vertex LOD blend for "detail" and "LOD";
    - the factor is inverted unless `REVERSE_BLEND`;
    - optionally add clocal or clocal.a;
    - optionally invert the output.
  - **Colour path (fbzColorPath):**
    - cother rgb is the iterated colour, the texel, or color1 (by `CC_RGBSELECT`).
    - Chroma key: discard if the squared distance between cother.rgb and the key is below 1e-8 (a distance below 1e-4 as in GLSL).
    - cother alpha comes from the iterated alpha, texel alpha or color1 alpha.
    - Alpha test against the reference with the 8 functions. Each function kills when its condition fails: never → always kill; less → kill if a ≥ ref; equal → kill if (a − ref)² > 1e-8; and so on through always → no test.
    - clocal is the iterated colour or color0. With `LOCALSELECT_OVERRIDE`, an `OpSelect` on texel.a < 0.5 chooses between them.
    - clocal alpha is chosen by `CCA_LOCALSELECT`. The clamped-Z choice is a TODO, as in OpenGL.
    - Then the same zero/sub/multiply/reverse/add/invert sequence as the texture combine, including the voodoo2 `MSELECT` value 5 (texel rgb).
  - **Fog**, if enabled:
    - Constant fog returns the pixel unchanged. This matches the OpenGL emulation, which does not apply it either.
    - Otherwise, start with the fog colour (or zero if `FOG_ADD`); subtract the pixel colour unless `FOG_MULT`; multiply by the fog blend (iterated alpha if `FOG_ZALPHA` == 1, else the per-vertex fog blend from the vertex setup); then either add the result to the pixel or replace the pixel with it.
  - The output is vec4(rgb, a).

#### How: pipelines and the 3dfx state mapping

**`VvkPipelineState`:**
- rect or triangle (selects the vertex shader, vertex layout and topology);
- blend enable plus src/dst colour and alpha factors;
- depth test, depth compare op, depth write;
- colour write mask;
- stencil mode: none, write 1, or test == 1.

Pipelines are cached in a map keyed by (vertex shader handle, fragment shader handle, packed blend state, packed depth/mask/stencil state). They are never pruned until teardown. Every pipeline uses the single render pass, no culling, viewport and scissor dynamic, and 1 sample.

**Vertex layouts:**
- Triangles: one binding, stride 64 bytes, per vertex. Attributes at offsets 0 (vec3), 12 (vec4), 28 (vec3), 40 (vec3), 52 (vec3). Triangle list.
- Rectangles: stride 36 bytes, per instance. vec4 at 0, vec4 at 16, float at 32. Triangle strip.

**Blending (`vvk_blend_state`, from alphaMode, as `VOGL_SetAlphaMode` does):**
- Blending is on if `ALPHABLEND` is set.
- Source factor from `SRCRGBBLEND` via a 16-entry table: ZERO, SRC_ALPHA, DST_COLOR, DST_ALPHA, ONE, ONE_MINUS_SRC_ALPHA, ONE_MINUS_DST_COLOR, ONE_MINUS_DST_ALPHA, ZERO for 8–14, and 15 = SRC_ALPHA_SATURATE.
- Destination factor from `DSTRGBBLEND`: the same table, except index 2 is SRC_COLOR, index 6 is ONE_MINUS_SRC_COLOR, and 15 ("colour before fog") is approximated by SRC_COLOR.
- Alpha factors: ONE if the src/dst alpha blend field equals 4, else ZERO.
- With blending off: ONE / ZERO.

**Depth compare:** the Voodoo `DEPTH_FUNCTION` field (0 never, 1 less, 2 equal, 3 lequal, 4 greater, 5 notequal, 6 gequal, 7 always) is cast directly to `VkCompareOp`. The enum values coincide.

**Triangles (`voodoo_vk_draw_triangle`):**
1. Get the three vertices from the shared `ogl_get_vertex_data` (fbi.ax/ay, bx/by, cx/cy).
2. For each TMU used (`texcount`, TMU RAM present, `lodmin` < 8·256), get the cached texture.
3. Choose a sampler per TMU and build the descriptor set; unused TMUs get the white texture.
4. Get the fragment shader and the push constants from the registers.
5. Build the vertices:
   - x NDC = 2·x/fbi.width − 1;
   - y NDC = 2·y/fbi.height − 1, or flipped (1 − 2·y/h) when `FBZMODE_Y_ORIGIN` is set, because the targets are top row first;
   - z = the vertex setup's depth;
   - colour clamped to [0,1], since OpenGL's fixed-function vertex stage clamps;
   - texture coordinates (s·w, t·w, w), or (0, 0, 1) if the TMU is unused;
   - fog blend and LOD blends.
6. Target: the front buffer if `FBZMODE_DRAW_BUFFER` == 0 (which also sets `vblank_flush_pending`), else the back buffer.
7. **Depth source compare** (`FBZMODE_DEPTH_SOURCE_COMPARE`, only when the depth format has stencil):
   - Clear the stencil (`vkCmdClearAttachments`, stencil aspect, whole target).
   - Draw the triangle with the flat shader at the constant depth zaColor/65535, colour writes off, the configured depth test (if depth buffering is on and alpha planes are off), and stencil ALWAYS/REPLACE with reference 1.
   - Then draw the real triangle with stencil EQUAL 1, and the depth test set to ALWAYS when depth buffering is enabled.
8. Otherwise, the depth state:
   - alpha planes off and depth buffer on: test with the register's function;
   - alpha planes off, depth buffer off but aux mask on: test ALWAYS (so writes happen);
   - alpha planes on: test ALWAYS.
   - Depth write = aux mask && !alpha planes.
   - Colour write mask: RGB from `RGB_BUFFER_MASK`, A from the aux mask.
   - Blending as above.
9. Draw, and invalidate the read-back caches for that buffer and for depth.

**Clipping registers:** `voodoo_vk_set_window`, `voodoo_vk_clip_window` and `voodoo_vk_vblank_flush` are no-ops. The projection is computed per triangle, and every retrace submits. The scissor always covers the whole target, so the clip rectangle is not applied to triangles in this back end; it is only used by fastfill. Check whether that matches the OpenGL path before reproducing it.

**`voodoo_vk_clear`:** `vkCmdClearAttachments` with colour black and depth 1.0 in both colour buffers.

**`voodoo_vk_fastfill`:**
- Takes the rectangle from `clipLeftRight` / `clipLowYHighY`.
- If `Y_ORIGIN` is set, the rows are flipped as yorigin − y, as the software fastfill does.
- The rectangle is clamped to the mode and scaled to the render resolution.
- Colour is cleared to color1 if `RGB_BUFFER_MASK`; depth to zaColor/65535 if the aux mask is set and an aux buffer exists. This is done with `vkCmdClearAttachments` inside a render pass on the back buffer, or on the front buffer when `DRAW_BUFFER` == 0, which also sets `vblank_flush_pending`.

**LFB writes:**
- `draw_pixel`, `draw_z` and `draw_pixel_pipeline` draw one instanced rectangle covering exactly the render-resolution pixels of the 3dfx pixel. The span is the same as `voodoo_ogl_lfb_span`: pixel i covers [ceil(i·d − 0.5), ceil((i+1)·d − 0.5)) with d = render size / mode size, at least one pixel.
- `draw_pixel`: colour mask from has_rgb/has_alpha, no blending.
- `draw_z`: colour writes off, depth test ALWAYS with write, depth z/65535.
- `draw_pixel_pipeline`: RGB plus alpha only if the aux mask is set, with alphaMode blending.
- The target is chosen by `LFBMODE_WRITE_BUFFER_SELECT`; the front buffer sets `vblank_flush_pending`.

**LFB reads (`voodoo_vk_read_pixel`):**
1. Map the two requested 3dfx pixels to the centre render pixel of their spans.
2. Select the buffer with `LFBMODE_READ_BUFFER_SELECT`: front, back, or aux (depth). Any other value is an `E_Exit`, as in OpenGL.
3. If that buffer's cached CPU copy is invalid:
   - (re)allocate a host-visible read-back buffer (`TRANSFER_DST`) of w·h·4 bytes;
   - copy the whole colour image, or the depth aspect only, into it;
   - add a buffer barrier transfer-write → host-read;
   - submit and wait;
   - copy into a `vector<UINT32>`. D16 data is widened from 2 bytes per texel; the other depth formats and colour are 4 bytes.
4. The copy is kept until something draws to that buffer.
5. Colour is returned as two RGB565 pixels from the RGBA8 bytes, matching the OpenGL emulation's bit order.
6. Depth is converted to 16 bits by format: D24 scales the low 24 bits to 0..65535, D16 is used as is, D32F is clamped to [0,1] and multiplied by 65535.

**`load_framebuffers_from_ram`:** after a software→hardware switch, convert the card's RGB565 front and back buffers (`fbi.rgboffs` for frontbuf/backbuf, stride `rowpixels`, bounds-checked against `fbi.mask`) to RGBA8 at the render resolution. It uses the same span mapping (render column → mode column) so reads and writes agree, uploads through an arena, and copies into the corresponding colour image.

#### How: textures

- **Cache:** per TMU, a `map` from texture base address (`lodoffset[ilod]`) to a `VvkTexMap`. The `VvkTexMap` holds format, width, height, ilod, lodmask, the NCC table for the 8-bit YIQ formats, a current texture, and for palette formats a map from palette hash to texture.
  - ilod comes from `lodmin` >> 8, moved up one if that LOD is masked out. The width and height come from `wmask`/`hmask` shifted by ilod.
  - An entry whose layout fields differ from the current registers is destroyed and rebuilt. Addresses get reused with other layouts.
- **Palette formats (1, 5, 9, 14):**
  - The texture is keyed additionally by an FNV-1a hash over the 256 entries of the palette, or of the selected NCC table for the YIQ formats. The OpenGL emulation uses a CRC.
  - `voodoo_vk_invalidate_paltex` marks all palette textures as needing a re-hash. On the next use the hash selects an existing variant or uploads a new one.
  - More than 8 variants per address flushes them.
- **Invalidation on texture memory writes:**
  - `voodoo_vk_texture_clear(address, TMU)` grows one dirty address range per TMU. A dword that wraps the TMU mask dirties everything.
  - Before the next lookup on that TMU, every cached texture overlapping the range is destroyed, with wraparound-aware overlap. Sizes are width × height × 1 byte for formats < 8, else 2 bytes.
  - This batches many small writes into one scan.
- **Conversion** (identical to the OpenGL emulation), read directly into an arena:
  - 8-bit formats: through `tmu.lookup`;
  - formats 10–12: 16-bit through `lookup`;
  - other 16-bit formats: rgb from `lookup` of the low byte, with alpha from the high byte.
- **Upload:**
  - The image is `B8G8R8A8_UNORM` with a full mip chain down to 1×1, usage `SAMPLED | TRANSFER_DST | TRANSFER_SRC`.
  - Copy level 0 from the arena, then generate each further level with `vkCmdBlitImage` (`VK_FILTER_LINEAR`) from the level above, with per-level TRANSFER_DST → TRANSFER_SRC barriers. Finally transition all levels to SHADER_READ_ONLY.
  - The mip levels are generated, not taken from the card's own LOD data.
  - Uploads end any open render pass, because transfers are not allowed inside one.
- **Samplers**, cached by a 6-bit key:
  - minification filter linear/nearest (`MINIFICATION_FILTER`);
  - magnification filter (`MAGNIFICATION_FILTER`);
  - mipmaps on if `lodmin` != `lodmax` (`maxLod` 1000, else 0);
  - mip mode linear if `TRILINEAR`, else nearest;
  - S and T: clamp-to-edge if `CLAMP_S`/`CLAMP_T`, else repeat.
- **Descriptor sets** are allocated from the current slot's pools. They are cached per slot by (view0, sampler0, view1, sampler1), so the same combination is written once per command buffer. A full pool moves allocation to the next pool.

#### How: teardown

- `voodoo_vk_leave(true)` (3dfx output off), `voodoo_vk_shutdown`, and `voodoo_vk_device_lost` (output shutting down) all lead to `vvk_destroy_all`:
  1. Submit, wait idle, and clear the output's external image.
  2. Destroy the textures; each slot's trash, arenas, pools and fence; the targets; the white texture; the read-back buffer; all pipelines, cached fragment shaders and samplers; the fixed shaders; the command pool; the layouts and render pass.
  3. Clear the device copy.
- `voodoo_vk_leave(false)` only submits, waits and drops the textures.
- Every entry point except present first checks `vvk_enter()`. If the back end is not ready, the call does nothing and "needs the Vulkan output" is logged once.

**Pitfalls and design notes:**
- **Output switches.** If the user switches away from `output=vulkan` while 3dfx is active, the output's Shutdown calls `voodoo_vk_device_lost()` before destroying the device. The next activation of the 3dfx output re-chooses the back end.
- **Synchronisation.**
  - Nothing is shared across threads.
  - The 3dfx back end and the output submit to the same queue from the same thread, and all cross-submission ordering is done with barriers whose scopes cover earlier and later submissions.
  - The present image's transitions (shader-read ↔ transfer-dst, at the fragment-shader stage) are what keep the output from sampling a half-copied image.
- **Render-resolution limits.** `max_size` from the device limits caps "double" and "display"; the targets must never exceed it.
- **Stencil.** Without a stencil-capable depth format, depth-source-compare triangles are drawn without the stencil pass (normal depth state).
- **LFB reads are slow.** A read waits for the GPU and copies a whole buffer, hence the per-buffer cache. Every drawing path must invalidate it: triangle, rect, clear, fastfill, load-from-RAM, resize.
- **`v` pointer.** `voodoo_vk_draw_triangle` sets the global `v` from `extra->state` first, as the OpenGL path does.

**Testing:**
- Run Win9x or DOS 3dfx titles and demos with `output=vulkan`:
  - `voodoo_card=vulkan`;
  - `voodoo_card=auto` (must pick Vulkan; with `output=opengl` it must pick OpenGL, with surface it must pick software);
  - `voodoo_card=software` and `opengl` for visual comparison.
- Try `voodoo_resolution=native`, `double` and `display`; change it at runtime from the menu and with `CONFIG -set`; resize the window with `display` (the render resolution follows).
- Exercise: texture-heavy scenes with palettised textures, fog, alpha test/blend, chroma key, games doing LFB reads and writes (e.g. HUD overlays and screenshots), fastfill, and switching the 3dfx output on and off (passthrough on/off).
- Switch the output away from Vulkan while 3dfx is running; this must not crash and must fall back to software.
- Check with the Khronos validation layer for barrier and layout errors.
- Also verify that the OpenGL 3dfx back end is unchanged after the code move, and that builds without OpenGL and/or without Vulkan still link (stubs).

## Metal output on macOS and the Metal 3dfx back end

Master already has a basic Metal output (`output=metal`, `src/output/output_metal.mm` / `.h`, class `CMetal`, menu item "Metal(Experimental)", SDL2 builds only). It uploads the scaled VGA frame into a shared `MTLPixelFormatBGRA8Unorm` texture and draws it as a full-window quad into a `CAMetalLayer` that sits on a subview of SDL's Cocoa content view.

On main-ai this was extended in several steps:

- small fixes to the output (sampler binding, the frame buffer size following `sdl.draw`)
- a hardware 3dfx Voodoo back end that renders with Metal (`voodoo_card=metal`, later also chosen by `voodoo_card=auto`)
- colour-space fixes so colours are not washed out on wide-gamut displays (Metal layer and the in-tree SDL2 Cocoa windows)
- window-size and "fit to aspect ratio" behaviour matching the surface and OpenGL outputs
- Metal output in SDL1 builds through a new `SDL_METAL` video mode flag in the in-tree SDL 1.x, plus 32bpp host-order pixels on SDL1 macOS
- the menu item no longer marked experimental

main-ai's CHANGELOG has three entries for this group: "3Dfx Metal hardware rendering support on macOS, with output=metal and voodoo_card=metal or auto"; "Metal output (and its 3Dfx hardware rendering) now works in SDL1 builds, with Metal support added to the in-tree SDL 1.x. SDL1 macOS builds now use the usual RGBA order instead of the backwards PowerPC order where macOS supports it"; and "Fix crash with output=metal when the 3Dfx is emulated in software: the Metal output did not resize its frame buffer for the 3Dfx video mode". The colour, window-size, aspect and menu changes have no CHANGELOG entry.

**Overall dependency.** The Metal 3dfx back end plugs into the hardware-emulation framework that main-ai added for the Vulkan, Direct3D 9 and Direct3D 11 back ends (described in other sections). That framework is not on master and consists of:

- the `voodoo_hw.h` dispatch macros and `v->hw_api`
- `VOODOO_EMU_TYPE_AUTO` and choosing the API in `voodoo_activate()` when the 3dfx output turns on
- the `voodoo_resolution` setting with `Voodoo_HW_PollResolutionSetting()`, `Voodoo_HW_RenderSize()` and `Voodoo_OGL_SetDisplaySize()`
- the LFB span mapping (`voodoo_ogl_lfb_span()`)
- `ogl_get_vertex_data()` / `ogl_vertex_data` exported from `voodoo_opengl.h`
- the hooks `voodoo_hw_present`, `voodoo_hw_swap_buffer`, `voodoo_hw_load_framebuffers_from_ram`, `voodoo_hw_update_dimensions` and `voodoo_hw_follow_dimensions` called from `voodoo_emu.cpp`

Implement that first. The Metal back end is modelled on the Direct3D 11 one (`voodoo_d3d11.cpp`) and the OpenGL one, and reuses their structure.

The output-only changes (sections 1, 5, 6, 7, 9) do not depend on the 3dfx work.

---

### 1. Metal output: bind the sampler in every render encoder

**What.** `CMetal::SetSamplerMode()` on master caches the last mode in a function-static `last_mode` and returns early when it has not changed. As a result, `setFragmentSamplerState` is called only in the first frame's render encoder.

**Why.** Sampler bindings belong to a render command encoder. Every new encoder starts with no sampler bound. From the second frame on, the fragment shader samples with an unbound sampler: undefined behaviour, and Metal API validation flags it.

**How.** In `CMetal::SetSamplerMode(id<MTLRenderCommandEncoder>)`, remove the static cache. Always pick `samplerNearest` or `samplerLinear` from `current_render_mode` and set it at fragment sampler index 0 on the encoder passed in.

**Testing.** Run with Metal API validation enabled (Xcode scheme, or `MTL_DEBUG_LAYER=1`). No warnings about a missing sampler after the first frame. Switching `render.aspect` between nearest and bilinear still changes the filtering.

---

### 2. Metal output: follow the frame size in SetSize (software 3dfx crash)

**What.** `OUTPUT_Metal_SetSize()` now calls `metal->CheckSourceResolution()` before it reads `frame_width` / `frame_height`.

**Why.** `OUTPUT_Metal_SetSize()` took the texture size from `metal->frame_width/frame_height`. Those values only follow `sdl.draw` when the VGA code calls `OUTPUT_Metal_CheckSourceResolution()` from `VGA_VerticalTimer`, which happens only while the VGA draws.

With `voodoo_card=software`, the 3dfx output calls `RENDER_SetSize()` for its own mode (640x480) while the VGA is not drawing. The CPU buffer (`cpu_buffer`) and frame texture stayed at the previous size (720x400 text mode). The scaler then wrote a 640x480 frame past the end of the vector. The heap was corrupted, and the program crashed later at an unrelated place.

**How.** At the top of the "Framebuffer (texture) size" part of `OUTPUT_Metal_SetSize()`, call `CheckSourceResolution()`. It compares `sdl.draw.width/height` with its last values and, on change, grows the CPU buffer and calls `Resize()`, which recreates the frame texture. After that call, `frame_width/frame_height` already match `sdl.draw`.

**Testing.** Run `output=metal`, `voodoo_card=software`, and a Glide/3dfx demo that switches from text mode to 640x480. Before the fix it crashes, sometimes only after a while. After the fix the 3dfx picture appears. A run under AddressSanitizer is a good check.

---

### 3. Metal output: hooks for a 3dfx back end on the same device

**What.** `CMetal` gets what a 3dfx renderer needs to share it:

- access to its device and command queue
- an "external texture" that is drawn instead of the VGA frame while set
- a way to draw that texture at the 3dfx's vertical retrace
- notification when the device goes away
- the size of the area the frame is shown in

**Why.** The 3dfx back end renders on the GPU into its own textures. The output has to show those textures without a CPU round trip, on the same device. Ordering is guaranteed by using the same command queue: Metal runs command buffers of one queue in commit order.

**How.**

1. **`output_metal.h`.** Add public `GetDevice()` and `GetQueue()` (return the existing `device` / `queue` members) and `SetExternalTexture(id<MTLTexture>)` (stores into a new member `externalTexture`, nil by default). Add `PresentExternal()` and a private `DrawFrame(id<MTLTexture>)`. Add free functions `bool OUTPUT_Metal_Active(void)` and `CMetal* OUTPUT_Metal_Get(void)`, with a forward declaration of `class CMetal`.
2. **Split `CMetal::EndUpdate()`.** It keeps the `replaceRegion` upload of `cpu_buffer` into `frameTexture`, then calls `DrawFrame(externalTexture ? externalTexture : frameTexture)` and clears `textureMapped`. `DrawFrame(tex)` holds the rest of the old code: get `[layer nextDrawable]`, build a render pass that clears to opaque black, encode viewport + pipeline + `setFragmentTexture:tex` + sampler, draw the 4-vertex strip, `presentDrawable`, `commit`. Wrap the body of `DrawFrame` in an `@autoreleasepool`. Drawables, pass descriptors, command buffers and encoders are autoreleased objects. The emulator loop does not drain a Cocoa pool per frame, and `DrawFrame` is now also called from the 3dfx emulation, outside any SDL event processing.
3. **`PresentExternal()`.** Returns if there is no `externalTexture` or no `layer`, otherwise calls `DrawFrame(externalTexture)`. The 3dfx back end calls it at every vertical retrace, because while the 3dfx output is on, the VGA does not call `EndUpdate`.
4. **`CMetal::Shutdown()`.** If `device` is set, first call `voodoo_metal_device_lost()` (declared at the top of `output_metal.mm`), then set `externalTexture = nil`, then release the rest as before. The 3dfx back end holds references to the device and to textures it created there. It must drop everything when the output destroys or replaces its device, for example on a switch to another output and back.
5. **`CMetal::Resize()`.** After `currentViewport` is computed, call `Voodoo_OGL_SetDisplaySize(round(viewport.width), round(viewport.height))`. The `voodoo_resolution=display` mode then renders at the size the frame is shown at, in drawable pixels (Retina pixels, not points).
6. **`OUTPUT_Metal_Active()`.** Returns true when the `metal` object exists, `sdl.desktop.type == SCREEN_METAL`, and `GetDevice()` is non-nil. `OUTPUT_Metal_Get()` returns the static `metal` pointer.

Section 1's sampler fix matters more once there are two callers of `DrawFrame`.

**Testing.** No visible change without the 3dfx back end. Switching outputs back and forth must not crash.

**Depends on.** Section 1 (recommended).

---

### 4. 3dfx emulation rendered with Metal (`voodoo_card=metal`)

**What.** A new back end, `src/hardware/voodoo_metal.mm` plus `voodoo_metal.h`, that implements the same function set as the OpenGL/Vulkan/D3D back ends with the `voodoo_metal_` prefix:

- `init`, `leave`, `shutdown`
- `set_window`, `present`, `load_framebuffers_from_ram`, `follow_dimensions`, `swap_buffer`, `vblank_flush`, `update_dimensions`
- `clear`, `fastfill`, `clip_window`
- `texture_clear`, `invalidate_paltex`
- `draw_pixel`, `draw_z`, `draw_pixel_pipeline`, `read_pixel`
- `draw_triangle`

It renders only with `output=metal`, on the output's device and queue. With any other output the card stays in software, as for the other back ends.

**Why.** On macOS, OpenGL is deprecated, and the OpenGL 3dfx emulation does not work with the Metal output. Without this back end, `output=metal` users only get the slow software 3dfx.

**How: wiring.**

- **`include/voodoo.h`.** Add `VOODOO_EMU_TYPE_METAL` (7) and `VOODOO_HW_API_METAL` (4).
- **`src/hardware/voodoo.cpp`.** Parse `voodoo_card=metal` into `VOODOO_EMU_TYPE_METAL`. Add that type to every `switch` that lists the hardware types: init, shutdown, PCI init enable, PCI enable, page handler.
- **`voodoo_interface.cpp`.** `Voodoo_Initialize()` range-checks the type against the highest value; raise the upper bound to `VOODOO_EMU_TYPE_METAL`.
- **`voodoo_emu.cpp`, `voodoo_activate()`.** Map `VOODOO_EMU_TYPE_METAL` to `VOODOO_HW_API_METAL`.
- **`voodoo_hw.h`.** Include `voodoo_metal.h` and add the Metal branch to `VOODOO_HW_CALL` and to the hand-written dispatchers (`voodoo_hw_read_pixel`, `voodoo_hw_init`).
- **`src/dosbox.cpp`.** Add `"metal"` to the `voodoo_card` value list and a help line: rendered with Metal, with output=metal (macOS; otherwise in software).
- **`voodoo_metal.h`.** Declare the functions when Metal output is built. Otherwise provide inline no-op stubs, with `init` returning false and `read_pixel` returning 0, so the dispatch compiles on every platform.
- **Build.** Add `voodoo_metal.h` to `EXTRA_DIST` in `src/hardware/Makefile.am`, and add `voodoo_metal.mm` to `libhardware_a_SOURCES` under `if C_METAL`.
- **Guard.** Originally the guard was `defined(MACOSX) && defined(C_SDL2) && C_METAL`. Section 8 replaces it everywhere with `C_METAL_OUTPUT`. If you do section 8 first, use that directly.
- **No ARC.** The file is built without ARC, like the output, and manages Objective-C references with explicit `retain`/`release`. It has an `#error` if `__has_feature(objc_arc)` is on. Objects from `new…`/`alloc` are owned. Objects returned by `commandBuffer` and `renderCommandEncoderWithDescriptor:` are autoreleased, so the back end retains the ones it keeps across calls. Every public entry point that creates Metal objects wraps its work in `@autoreleasepool`.
- **Include order.** `voodoo_def.h` defines a register named `status`, which clashes with `-[MTLCommandBuffer status]`. The small helper that checks whether a command buffer is completed (status `Completed` or `Error`) is defined before including `voodoo_def.h`.

**How: Metal objects and their lifetimes.** All state is in one file-static struct.

- **Device and queue.** Retained copies of the output's `GetDevice()` / `GetQueue()`. The back end never creates its own queue, so its command buffers and the output's are ordered by commit order on one queue. Everything runs on the emulation thread, which is also the thread the output draws on, so no locking is needed.
- **Render targets**, each at the render resolution (`Voodoo_HW_RenderSize()`, capped at 16384, which every Metal-capable Mac GPU supports):
  - two colour buffers `color[0..1]`, `MTLPixelFormatRGBA8Unorm`, usage RenderTarget|ShaderRead, `MTLStorageModePrivate`, with an index `front`
  - one `present` texture, same format, which the output draws
  - one `depth` texture, `MTLPixelFormatDepth32Float_Stencil8`, usage RenderTarget, private

  New targets are cleared through render-pass load actions: colour 0 with alpha 0, depth 1.0, stencil 0. `present` is cleared to opaque black. Row 0 is the top row in all of them.
- **Shaders.** Metal shading language source is compiled at run time with `newLibraryWithSource`. Every library contains the same vertex function. It takes a vertex buffer of structs (position in render-resolution pixels, iterated colour, TMU0 and TMU1 texture coordinates as (s·w, t·w, 0, w), and an "extra" vector of fog blend, TMU0 LOD blend and TMU1 LOD blend) plus a constant scale vector (2/width, 2/height). It maps pixels to clip space with Y flipped (top row at +1), passes z through, sets w to 1, and sets point size 1.

  A built-in library holds two fragment functions:
  - `flat`: output the vertex colour; used for LFB writes, fastfill and stencil work
  - `copy`: sample texture 0 at the first texture coordinate; used to rescale buffers

  Per-state triangle fragment libraries are generated as described below.
- **Pipeline states.** One per (shader, blend key). The blend key holds: blending on/off, the RGB source and destination factors, the alpha source and destination factors, and the colour write mask. Pipelines are cached in a map inside each shader object. Colour attachment format is RGBA8Unorm; depth and stencil attachment formats are Depth32Float_Stencil8.
- **Depth/stencil states.** Cached in a map keyed by depth test on/off, compare function, depth write, and stencil mode (none / write / test / clear).
- **Sampler states.** Cached in a map keyed by a bit set: min linear, mag linear, mipmaps, trilinear, clamp S, clamp T. Repeat when not clamped. The r address is ClampToEdge.
- **Teardown.** All caches are released in `vmtl_destroy_all()`. That function also commits the pending command buffer without waiting. Releasing a texture or buffer that encoded commands still use is safe, because default command buffers (`commandBuffer`, not the unretained variant) retain what they reference until they complete. The code relies on this in several places.

**How: command flow.**

- **Command buffers and passes.** At most one command buffer is being built at a time. It is created lazily from the shared queue and retained. A render pass (encoder) is reused as long as the next draw targets the same colour texture and nothing needs clearing. Otherwise the current encoder is ended and a new pass begins: load action Load, or Clear for the requested attachments; store action Store for colour, depth and stencil.

  Every new encoder gets:
  - a viewport of the full target
  - cull mode None
  - `MTLDepthClipModeClamp`, because 3dfx depth values can fall slightly outside 0..1 and must not clip triangles away
  - stencil reference value 1
- **Vertex memory.** Vertex data goes into 4 MiB shared, write-combined `MTLBuffer` chunks, with each draw's data aligned to 256 bytes. The chunks used by a command buffer are recorded with it at commit. A chunk is recycled when that command buffer's status is Completed or Error, which is polled. No completion handlers are used, so no threading is involved.
- **Batching.** Draws are batched: the complete draw state (target, kind, shader pointer, textures, sampler keys, blend key, depth key, fragment constants) is compared with `memcmp` against the pending one. Vertices are appended while it is equal. The batch is flushed when the state changes, at about 32768 vertices, and before anything that reads or changes resources. Because of the `memcmp`, the state structs must be fully zeroed (`memset`) before they are filled, including padding.
- **Flushing a batch.** Upload the vertices, then set:
  - the pipeline (the triangle shader, or `flat`) and the depth/stencil state
  - the vertex buffer at index 0 and the scale at vertex index 1 (`setVertexBytes`)
  - for triangles: the fragment constants at fragment buffer 0 (`setFragmentBytes`), plus each used TMU's texture and sampler at indices 0/1

  Then draw triangles, or points for single-pixel LFB writes.

**How: mapping Voodoo state onto Metal.**

- **Triangle fragment shader.** This is a straight transcription of the OpenGL emulation's generated GLSL (`ogl_shaders()`, `ogl_sh_tex_combine()`, `ogl_sh_color_path()`, `ogl_sh_fog()`) into MSL text. It is the same as the D3D11 back end's HLSL, step for step.

  The cache key holds:
  - `textureMode` of TMU0 and TMU1, 0 if that TMU is not used
  - the full `fbzColorPath`
  - `alphaMode & 0x0f` (alpha test enable and function)
  - `fogMode & 0x3f` when fog is enabled, else 0
  - flags for TMU0 used, TMU1 used, and chroma key enabled

  The function signature only declares the textures and samplers that are used.

  Shader steps:
  1. TMU1 is sampled first, with perspective divide (xy / w), then its texture combine runs. Its output becomes "other" for TMU0, then TMU0 runs. The LOD/detail blend factor comes from the per-vertex extra values.
  2. The colour path follows: cother select, chroma-key discard (colour distance below 1e-4), alpha select, alpha test against the reference in the constants (all eight functions; "never" discards always), local select (including the override by texel alpha), zero/sub/mselect/reverse/add/invert.
  3. Fog: constant fog takes the fog colour. Otherwise start from fog colour or 0, subtract the pixel unless fog_mult, multiply by iterated alpha (zalpha mode) or the per-vertex fog blend, then add or replace.

  The `CLAMPED_Z` alpha local select is not implemented (same TODO as OpenGL).

  The fragment constants are color0, color1, chromaKey, fogColor (alpha 1), and a misc vector (alpha reference, zaColor/65535), all as floats in 0..1.

  A shader that fails to compile is cached as NULL and not tried again. Its triangles are skipped. The MSL source is logged with the compiler error.
- **Vertices.** The values come from `ogl_get_vertex_data()` for the three vertices.
  - x and y are scaled from 3dfx mode pixels to render pixels.
  - If the `fbzMode` Y origin is at the bottom, y becomes height − y. The projection is done here per triangle, so `set_window` and `clip_window` are no-ops, as in the Vulkan and D3D11 back ends.
  - z is the vertex depth `d`.
  - Colours are clamped to 0..1, because OpenGL clamps fixed-function vertex colours and the result must match.
  - Texture coordinates are `sw`, `tw`, `w`, or (0, 0, w = 1) for an unused TMU.
- **Depth.** The Voodoo's 3-bit depth function is cast directly to `MTLCompareFunction`. The orders are identical: never, less, equal, less-equal, greater, not-equal, greater-equal, always. Cases:
  - Depth buffer enabled without alpha planes: test with that function.
  - Depth disabled but the aux mask set: test Always.
  - Alpha planes enabled (aux buffer holds alpha, not depth): test Always and never write depth. The alpha channel is written through the colour write mask instead.
  - Depth write = aux mask and not alpha planes.
- **Depth source compare** (`FBZMODE_DEPTH_SOURCE_COMPARE`, compare against `zaColor`), done with the stencil:
  1. Clear stencil to 0 with a full-target flat rectangle: no colour write, stencil op Zero, compare Always. This is cheaper than a new render pass.
  2. Draw the triangle flat at depth zaColor/65535 with the depth test and function: no colour write, stencil Replace with reference 1, read/write mask 1.
  3. Draw the real triangle with the stencil test Equal 1 and depth compare Always.
- **Blending.** From `alphaMode`, as the OpenGL emulation's `VOGL_SetAlphaMode()`. The RGB source and destination factors come from 16-entry tables:
  - Source: 0 Zero, 1 SrcAlpha, 2 DstColor, 3 DstAlpha, 4 One, 5 1−SrcAlpha, 6 1−DstColor, 7 1−DstAlpha, 15 SourceAlphaSaturated, the rest Zero.
  - Destination: like source, except 2 SrcColor and 6 1−SrcColor, and 15 (`A_COLORBEFOREFOG`) is approximated as SourceColor.
  - Alpha factors are One if the 3dfx alpha factor is 4, else Zero.
  - Blend operation Add.
  - The colour write mask comes from the RGB mask and the aux mask (alpha).
- **Draw target.** `FBZMODE_DRAW_BUFFER` 0 means the front buffer, else the back buffer. Drawing to the front sets `v->fbi.vblank_flush_pending`.
- **Textures.** Converted on the CPU as in the OpenGL emulation (`ogl_cache_texture()`).
  - The base level is chosen from `lodmin`, skipping to the next level if it is not in `lodmask`. The texture starts at `lodoffset[ilod]`, with width (`wmask` >> ilod) + 1 and height likewise.
  - Texels: 8-bit formats go through the TMU `lookup` table. Formats 10..12 go through `lookup` on the 16-bit value. The remaining 16-bit formats take colour from `lookup` of the low byte and alpha from the high byte.
  - The mip chain is built on the CPU from 2x2 rounded box averages; it is not read from the Voodoo's own LOD levels in texture memory, same as OpenGL.
  - Upload as `MTLPixelFormatBGRA8Unorm`. The 0xAARRGGBB words are B,G,R,A in memory on little-endian. Use `MTLStorageModeManaged` and `replaceRegion` per mip level.
  - Cache per TMU in a map keyed by the texture's start address. The entry records format, size, ilod, lodmask and NCC table. A mismatch on lookup drops the entry, because addresses get reused with other layouts.
  - Palette and NCC formats (1, 5, 9, 14) keep one texture per palette, keyed by an FNV-1a hash of the 256 palette or NCC entries. Above 8 variants the set is cleared. `invalidate_paltex` marks palette textures stale, and the next use rehashes.
  - Texture memory writes (`texture_clear(address, TMU)`) only grow a dirty address range: a dword that wraps the mask dirties everything. The range is applied before the next texture lookup. Overlapping textures are destroyed, with a batch flush first because queued draws may reference them. Overlap tests must handle textures that wrap past the TMU memory mask.
  - Sampler key: min/mag linear from the texture mode, mipmaps when `lodmin != lodmax`, trilinear bit, clamp S/T.
- **LFB writes.**
  - A 3dfx pixel maps to the render-pixel span `vmtl_lfb_span()` (= `voodoo_ogl_lfb_span()`: p0 = ceil(i·d − 0.5), p1 = ceil((i+1)·d − 0.5), at least one pixel).
  - A 1x1 span is drawn as a point at the pixel centre; larger spans as a rectangle (two triangles). Both use the flat pipeline.
  - `draw_pixel` writes RGB and/or alpha according to `has_rgb` / `has_alpha`. `draw_z` writes depth only. `draw_pixel_pipeline` applies `alphaMode` blending and writes alpha when the aux mask is set.
  - The target buffer comes from `LFBMODE_WRITE_BUFFER_SELECT`. Front-buffer writes set `vblank_flush_pending`.
- **Fastfill.** Clip rectangle from `clipLeftRight` / `clipLowYHighY`, flipped against `yorigin` when the Y origin is at the bottom (like the software fastfill), clamped to the mode, then scaled to render pixels. Draw a flat rectangle with colour `color1` and z zaColor/65535, without depth test or blending. The colour mask is all channels if the RGB mask is set, else none. Depth is written if the aux mask is set and an aux buffer exists. The target is the front buffer when the RGB mask and draw buffer 0 are set, else the back buffer.
- **LFB reads.**
  - The whole buffer is read back and cached per buffer: colour 0, colour 1, depth. A buffer's cache stays valid until something is drawn into it. Any draw also invalidates the depth cache.
  - To read: flush, end the pass, blit-copy the texture into a shared `MTLBuffer` (for depth use `MTLBlitOptionDepthFromDepthStencil`, which gives 4-byte floats), commit and `waitUntilCompleted`. This is a deliberate synchronous stall, and the cache keeps it to once per buffer per frame.
  - Convert colours from RGBA bytes to 0xAARRGGBB, and depth to 16-bit (clamped, ×65535, rounded).
  - `read_pixel` returns two horizontally adjacent pixels, as RGB565 or 16-bit depth packed into 32 bits, each from the render pixel in the middle of its span. Coordinates outside the mode return 0xffffffff. Read buffer select 3 is fatal (`E_Exit`), as in the other back ends.
- **Start from what is in 3dfx RAM** (`load_framebuffers_from_ram`). Called when the hardware emulation takes over from software.
  1. For the front and back RGB buffers (`rgboffs[frontbuf]`, `rgboffs[backbuf]`), build a render-resolution image by mapping each render pixel to its 3dfx pixel. Use the same span function, so reads and writes agree.
  2. Expand RGB565 to 8 bits per channel by bit replication, alpha 0, using `rowpixels` as the pitch.
  3. Copy it into the private colour texture through a temporary shared buffer and a blit (`copyFromBuffer:…toTexture:`). Private textures cannot be written by the CPU, and the blit keeps it in order with the drawing. Depth is not loaded.
- **Buffer swap and present.**
  - `swap_buffer` flushes and toggles `front`.
  - `present` is called at every vertical retrace. It re-checks the render size, flushes, ends the pass, blit-copies `color[front]` into `present`, and commits without waiting. Then it recycles chunks, calls `SetExternalTexture(present)` and `PresentExternal()` on the output.
  - A separate `present` texture is used so the output always draws a stable snapshot taken at retrace. Games that render to the front buffer would otherwise show half-drawn frames. The same applies if the output draws later from `EndUpdate`.
  - `vblank_flush` is a no-op, because everything is committed at present anyway.
- **Video mode and resolution changes.**
  - `vmtl_reset_videomode()` (at init, or when there are no targets yet) first calls `RENDER_SetSize(fbi.width, fbi.height, 32 bpp, 60 Hz, 4:3)` so the output sizes its window for the 3dfx mode. That call also reports the display area through `Voodoo_OGL_SetDisplaySize`, which "display" resolution needs. Then it creates the targets and hands `present` to the output.
  - `update_dimensions` does the same when targets already exist. `follow_dimensions` only re-checks the size.
  - A render-size change (setting changed, 3dfx mode changed, or window resized with `voodoo_resolution=display`) creates new targets. Both old colour buffers are drawn into them with the `copy` shader and a nearest/clamp sampler. Nearest keeps pixel colours exact for later LFB reads. Depth is cleared, and the old textures are released after encoding.
  - `clear` clears colour 0 + depth + stencil, and colour 1, through pass load actions.
- **Init and device loss.**
  - `voodoo_metal_init()` refuses (logging "needs output=metal") unless `OUTPUT_Metal_Active()`. It retains the output's device and queue, compiles the built-in library, polls the resolution setting, and resets the video mode.
  - `vmtl_usable()` (ready, has targets, and the output's current device is the one held) guards every entry point, so nothing touches a stale device.
  - `voodoo_metal_device_lost()`, called from `CMetal::Shutdown()`, destroys everything and sets `lost`. The next `present` while the 3dfx is active re-initializes once Metal output is active again. Picture contents are lost; the next frames redraw.
  - `leave(true)` and `shutdown` destroy everything and clear `lost`. `leave(false)` only drops the texture cache.

**Testing.** Use `output=metal`, `voodoo_card=metal`, and `voodoo_resolution` set to native, double and display. Run several Glide games or demos, and compare with `voodoo_card=software` and with OpenGL or D3D11 on other hosts. Check:

- textured, fogged and alpha-blended scenes
- chroma-keyed sprites
- games that use LFB writes (overlays, movie playback) and LFB reads (screenshots, software cursor compositing)
- games that render to the front buffer
- palette (P8) texture animation
- mode switches between 3dfx and VGA
- switching output to OpenGL/surface and back while the 3dfx is active (must recover, not crash)
- resizing the window with "display" resolution

Run once with Metal validation on. The log shows "VOODOO: Metal: rendering 3dfx graphics with Metal" and the mode/resolution lines.

**Depends on.** Section 3, and the hardware 3dfx framework from the Vulkan/D3D/D3D11 work (see the overview).

---

### 5. `voodoo_card=auto` picks Metal with `output=metal`

**What.** With `voodoo_card=auto`, `voodoo_activate()` also selects `VOODOO_HW_API_METAL` when `OUTPUT_Metal_Active()` is true at the moment the 3dfx output turns on.

**Why.** At first Metal was opt-in only (help text: "Not picked by auto"). Once it was tested, auto should use it like the other APIs.

**How.**
- In `voodoo.cpp`, `voodoo_card=auto` maps to `VOODOO_EMU_TYPE_AUTO` when any hardware API is built in; add the Metal-output condition to that `#if`.
- In `voodoo_emu.cpp`, declare `OUTPUT_Metal_Active()` and add the check after the D3D11 one in the AUTO case.
- Update the comments in `voodoo.h` and the help text in `dosbox.cpp`: metal is "Rendered with Metal, with output=metal (macOS; otherwise in software)", and auto is "OpenGL, Vulkan, Direct3D, Direct3D 11 or Metal for the output in use, otherwise software".

**Testing.** With `output=metal` and `voodoo_card=auto`, the log shows the Metal back end. With `output=surface` the card is emulated in software.

**Depends on.** Section 4.

---

### 6. Metal output: layer in the display's colour space (no colour conversion)

**What.** The output's `CAMetalLayer` gets the colour space of the screen the window is on. It is set when the layer is configured and again on every `Resize()`.

**Why.** On master the layer has no colour space. macOS then treats its pixels as sRGB and converts them to the display's colour space. On wide-gamut (Display P3) Macs this made the Metal output visibly less saturated than the surface and OpenGL outputs (and SDL1 builds), which show the emulated colours as they are. DOS colours have no defined colour space, and the expected look is "the values go to the display unchanged".

**How.**
- Factor the layer setup (device, `MTLPixelFormatBGRA8Unorm`, `framebufferOnly = NO`) into `CMetal::ConfigureLayer()`. Section 8 needs it as well.
- Add `CMetal::SetLayerColorspace()`. It takes the screen of `view.window`, falling back to `[NSScreen mainScreen]`, gets that screen's `colorSpace` as a `CGColorSpace`, and assigns it to `layer.colorspace` only if it differs (compare with `CFEqual`). This avoids needless layer updates.
- Call it from `ConfigureLayer()` and from `Resize()` next to the `contentsScale` update. The commit relies on `Resize()` to pick up a window that moved to another display. There is no screen-change or profile-change notification hook for the Metal output. I have not verified that moving a window between displays always leads to a `Resize()`; if it does not, the colour space is only updated at the next resize. Check this, and consider hooking `NSWindowDidChangeScreenNotification` as section 7 does for SDL2.

**Testing.** On a P3 display, compare a colour test (for example the 256-colour palette or a bright game) between `output=metal` and `output=opengl` in an SDL1 build, or in SDL2 after section 7. They should match. Move the window between a P3 and an sRGB display.

---

### 7. In-tree SDL2: Cocoa windows in their display's colour space, not sRGB

**What.** In `vs/sdl2/src/video/cocoa/SDL_cocoawindow.m`, a new static helper `Cocoa_MatchWindowColorSpace(NSWindow*)` sets the window's colour space to that of its screen (falling back to the main screen) if it is not already equal. It replaces the `[nswindow setColorSpace:[NSColorSpace sRGBColorSpace]]` in `Cocoa_CreateWindow`, and is also called in the window listener's `windowDidChangeScreen:` and `windowDidChangeScreenProfile:` handlers before their existing code.

**Why.** Upstream SDL2 tags every Cocoa window as sRGB, so macOS converts what is drawn into it to the display's colour space. In SDL2 builds that desaturated the surface output (drawn through SDL's OpenGL renderer) and the OpenGL output on wide-gamut displays. SDL1 builds and the Metal output (after section 6) did not do this. All outputs should look the same.

**How.** As above, three call sites. Mark each with a "DOSBox-X" comment, since this diverges from upstream SDL2 and has to survive SDL updates.

**Testing.** On a P3 display, an SDL2 build's surface and OpenGL outputs should now match the Metal output and an SDL1 build. Move the window between displays, and change the display's colour profile in System Settings while running.

---

### 8. Metal output (and its 3dfx back end) in SDL1 builds; SDL1 macOS uses the usual RGBA order

This is the largest change. It has three parts that can be reviewed separately but land together.

#### 8a. In-tree SDL 1.x: `SDL_METAL` video mode flag and `SDL_metal.h`

**What.**
- A new `SDL_SetVideoMode()` flag `SDL_METAL` (value `0x00800000`, DOSBox-X specific, declared in `SDL_video.h` next to `SDL_VULKAN`). It requests a window that SDL does not draw to: no pixels, no shadow surface, and `SDL_UpdateRects()` / `SDL_Flip()` return with an error ("Metal active, SDL does not draw to the window"). It is modelled on the existing `SDL_VULKAN` support.
- A new public header `SDL_metal.h`, added to `HDRS` in `vs/sdl/Makefile.in`, with `SDL_Metal_GetLayer()` (the `CAMetalLayer*` as `void*`, or NULL) and `SDL_Metal_GetDrawableSize(int*, int*)`.

**How.**
- **`SDL_sysvideo.h`.** Add two driver hooks to `SDL_VideoDevice`: `Metal_GetLayer` and `Metal_GetDrawableSize`.
- **`SDL_video.c`, `SDL_SetVideoMode()`.** If `SDL_METAL` is requested and the driver has no `Metal_GetLayer`, fail with "Metal not available with the %s video driver". Otherwise strip OPENGL/OPENGLBLIT/HWSURFACE/DOUBLEBUF/HWPALETTE/VULKAN from the flags. Treat a mode returned without `SDL_METAL` as failure. Skip the "video mode smaller than requested" sanity check and the shadow-surface logic for Metal, as for OpenGL and Vulkan.
- **Public functions.** `SDL_Metal_GetLayer()` errors out unless the current mode was set with `SDL_METAL`. `SDL_Metal_GetDrawableSize()` returns 0x0 in that case.
- **Quartz driver (`SDL_QuartzVideo.m` / `.h`).**
  - Add a `metal_layer` member to the private data, and set it to nil wherever `window_view` is cleared in `QZ_UnsetVideoMode`.
  - Add `QZ_CreateMetalView(frame)`. It looks up `CAMetalLayer` at run time with `NSClassFromString`, so SDL builds and links without the Metal SDK or QuartzCore's Metal parts; if the class is missing, it fails with an error. It creates the layer and an `NSView` with width/height autoresizing. The view is layer-hosting, so set the layer before `setWantsLayer:YES`. It records the layer and returns the view.
  - Windowed mode: changing `SDL_METAL` forces the window to be recreated (add it to the flags compared in `QZ_SetVideoWindowed`). When `SDL_METAL` is set, create the view if there is none, add it to the content view, and `makeKeyAndOrderFront`.
  - Fullscreen mode (`QZ_SetVideoFullScreen`): supported on 10.7 and later only, with an error before that. Create the view, add it, and raise the window to `CGShieldingWindowLevel()`, as the OpenGL fullscreen path does.
  - `QZ_Metal_GetDrawableSize` returns the view bounds converted to backing pixels when `convertRectToBacking:` exists.
  - The draw-rect update callback `QZ_UpdateRectsOnDrawRect` returns early for `SDL_METAL` modes and when there is no `cg_context`.

#### 8b. Quartz 32bpp in host byte order (`SDL_DOSBOX_X_QUARTZ_HOST_ORDER`)

**What.** In SDL 1.x on Intel and ARM Macs, 32bpp modes were XRGB in memory, which is PowerPC's big-endian ARGB. As a 32-bit value that is BGRA, with alpha in the low byte. DOSBox-X carried many special cases for this "weird RGBA order". The in-tree SDL now creates its CoreGraphics bitmap with `kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host` where the SDK has it (`MAC_OS_X_VERSION_MAX_ALLOWED >= 1040`). 32bpp surfaces are then normal ARGB in host order: masks R 0x00FF0000, G 0x0000FF00, B 0x000000FF, which is BGRA in memory.

**Why.** It matches what the Metal layer expects (`MTLPixelFormatBGRA8Unorm`), so the Metal output can share the scalers' output with no swizzle. It also removes the need for the special-case code paths in SDL1 builds.

**How.**
- **`SDL_video.h`.** On Apple, when the SDK is 10.4 or newer, define `SDL_DOSBOX_X_QUARTZ_HOST_ORDER` to 1. Applications use it to know which order they get.
- **`SDL_QuartzVideo.m`.** A `QZ_BITMAP_INFO` macro selects the bitmap info for both `CGBitmapContextCreate` calls (windowed and 10.7+ fullscreen). In `QZ_SetVideoModeInternal`'s 32bpp case, use the host-order masks when that macro is on. Otherwise keep the old logic, but give `SDL_METAL` modes the host-order masks too. Also fix the misplaced `break` so it ends the 32bpp case properly.
- **DOSBox-X, `include/render.h`.** Define `MACOSX_SDL1_WEIRD_RGBA` only for macOS, SDL1, little-endian, and without `SDL_DOSBOX_X_QUARTZ_HOST_ORDER`; it includes `SDL.h` for this. Every existing "macOS SDL1 weird order" condition is switched to this macro:
  - the mask definitions and `PMAKE` for 15/16 to 32 bpp in `render_templates.h`, plus the TV3x/TV3xDw scaler variants there
  - the overflow-safe add path in `render_scalers.cpp`
  - in `vga_draw.cpp`: `guest_bgr_to_macosx_rgba` with the 24-to-32 linear line drawer, the LIN32 hardware-mouse line drawer, and its selection in `VGA_ActivateHardwareCursor`
  - the 32bpp capture byte shuffle in `CAPTURE_AddImage` (`hardware.cpp`)
  - the OpenGL `GFX_GetRGB` BGRA case in `sdlmain.cpp`
- **OpenGL output upload type.** In `output_opengl.cpp`, the four `glTexSubImage2D` calls are unified behind a small helper, `OPENGL_PixelType()`. It returns `GL_UNSIGNED_INT_8_8_8_8` for the weird order and `GL_UNSIGNED_INT_8_8_8_8_REV` otherwise.

  Note: on master, the pixel-buffer-object upload used `8_8_8_8` for every macOS build, including SDL2, while the other three used it only for SDL1. After this change the SDL2 macOS PBO path uses `_REV` like the rest. That looks like a latent bug fix, but verify it on an SDL2 macOS build with PBOs in use.
- **SDL-drawn OpenGL menus** (`MenuDrawRect`, `MenuDrawText` in `menu.cpp`). The colour swap was conditioned on little-endian macOS. It is now little-endian macOS and (SDL2 or `MACOSX_SDL1_WEIRD_RGBA`), so SDL2 behaviour is unchanged.
- **Stock SDL 1.2.** Builds against stock SDL 1.2 keep the old behaviour because `SDL_DOSBOX_X_QUARTZ_HOST_ORDER` is not defined there.

#### 8c. DOSBox-X: `C_METAL_OUTPUT` and the SDL1 Metal output

**What and why.** Make `output=metal` (and with it `voodoo_card=metal`) available in SDL1 macOS builds that use the in-tree SDL 1.x.

**How.**
- **`configure.ac`.** After the existing Metal check, for SDL1 builds with Metal, run `AC_CHECK_HEADER(SDL_metal.h)` (with `#include "SDL.h"` as prologue). On success define `C_SDL1_METAL`. On failure, warn that the in-tree SDL 1.x is needed for Metal output.
- **`include/dosbox.h`.** Define `C_METAL_OUTPUT` to 1 for macOS + `C_METAL` + SDL2 (not SDL3), or macOS + `C_METAL` + SDL1 + `C_SDL1_METAL`.
- **Guards.** Replace every `defined(MACOSX) && defined(C_SDL2) && C_METAL` guard (and variants such as `MACOSX && defined(C_SDL2) && C_METAL`, `defined(C_SDL2) && C_METAL`) with `C_METAL_OUTPUT`. Places:
  - `SCREEN_METAL` in `sdlmain.h`
  - the output list and `voodoo_card` list in `dosbox.cpp`; the output list in `SDL_SetupConfigSection`
  - the menu item and its callback (`menu.cpp`, `menu_callback.cpp`)
  - `render.cpp` (the doublescan and scaler checks)
  - `sdl_gui.cpp` and `sdl_mapper.cpp` (the output shutdown around the config tool and mapper)
  - in `sdlmain.cpp`: `GFX_SetSize`, `GFX_StartUpdate`, `GFX_EndUpdate`, `GFX_GetRGB`, `GUI_ShutDown`, `GUI_StartUp`
  - `vga_draw.cpp` (`CheckSourceResolution` call)
  - `voodoo.cpp`, `voodoo_emu.cpp`, `voodoo_metal.h/.mm` (which now include `dosbox.h` instead of `config.h`)
  - `output_tools.cpp` (`change_output` case 14, menu check, `toOutput`)
  - `shell.cpp` (switch to Metal on startup)
  - `output_metal.h/.mm`
- **Include fix.** `sdlmain.h` now includes `dosbox.h` first. Otherwise some translation units would see `C_METAL_OUTPUT` undefined and get a different `SCREEN_TYPES` enum (`SCREEN_METAL` shifts the later values) than others: a silent mismatch between files.
- **`GFX_GetBestMode`.** `sdlmain.cpp` gains a `SCREEN_METAL` case calling `OUTPUT_Metal_GetBestMode()`. The SDL1 path did not have one.
- **`ASPECT_*` constants.** `ASPECT_NEAREST` / `ASPECT_BILINEAR` exist only in SDL2 builds. `output_metal.h` gets its own enum `METAL_SAMPLE_NEAREST` / `METAL_SAMPLE_BILINEAR` for `current_render_mode`, used in `GetRenderMode()` and `SetSamplerMode()`.
- **`CMetal::Initialize(nsview, sdl_layer, w, h)`.** With `sdl_layer` NULL (SDL2), create the layer and subview as before. With a layer (SDL1), call the new `AttachLayer(nsview, sdl_layer)`, which:
  - stores the view
  - retains the new layer and releases the previous one if it was retained (`layer_retained` flag). The window and its layer can be destroyed by `SDL_SetVideoMode` while the output still points at them.
  - calls `ConfigureLayer()`
  - zeroes `last_window_w/h` so the next `Resize()` sizes the layer

  `Shutdown()` releases a retained layer.
- **SDL1 window size.** A file-static `metal_GetWindowSize()` uses `SDL_GetWindowSize` on SDL2. On SDL1 it reads the bounds of `[sdl1_hax_get_window() contentView]`, converted to backing pixels when `sdl1_hax_highdpi_enable` is set. It reads the window rather than the surface so it follows a user resize before the video mode is set again.
- **`metal_SetWindowSize(w, h)`.** On SDL2 this is `SDL_SetWindowSize`. On SDL1 it calls `metal_SetVideoMode`, which:
  - uses `SDL_METAL | SDL_RESIZABLE` with w x h in a window
  - uses `SDL_METAL | SDL_FULLSCREEN` at `sdl.desktop.full.width/height` in fullscreen, the equivalent of SDL2's fullscreen-desktop
  - skips the call if the current surface already has `SDL_METAL`, the same size and the same fullscreen state
  - if fullscreen fails: logs, clears `sdl.desktop.fullscreen`, and retries as a window at the saved window size (or 640x400)
  - on success: stores `sdl.surface`, clears `deferred_resize`, sets `must_redraw_all`, and re-attaches the output to the (possibly new) window's content view and `SDL_Metal_GetLayer()`
- **SDL2 calls replaced.** All `SDL_GetWindowSize` / `SDL_SetWindowSize` calls in `OUTPUT_Metal_SetSize()` and `CMetal::Resize()` go through these helpers. The fullscreen enter/leave in `SetSize` uses `metal_SetVideoMode` on SDL1. After entering, `was_fullscreen` follows `sdl.desktop.fullscreen`, because fullscreen can fail and leave a window.
- **Layer geometry in `Resize()` on SDL1.** The layer belongs to SDL's view, which fills the window. Its frame is set to the view's bounds, which are in points, and the drawable size to bounds × scale. This is different from SDL2, where the frame is set from the window size. SDL1 window sizes are in pixels when high-DPI is on, so using them for the frame would double-scale.
- **`metal_init()` on SDL1.**
  - Delete the old `CMetal` after `Shutdown()`, so it is not drawn into while the mode is being set.
  - If the current surface is not an `SDL_METAL` mode, set one at the current surface size (windowed) or 640x400.
  - Get the window from `sdl1_hax_get_window()`, the view from its content view, and the layer from `SDL_Metal_GetLayer()`. If any is missing, log and fall back to the surface output (`OUTPUT_SURFACE_Select()`).
  - Pass the layer to `Initialize()`.

**Testing.**
- Build SDL1 macOS (`--enable-sdl` / in-tree vs/sdl) on Intel and ARM.
- Check surface, OpenGL and Metal output colours, including 15/16/24/32bpp SVGA modes, the TV3x scaler, the hardware mouse cursor in LIN32 modes, screenshots and video captures, and the SDL-drawn menus.
- Toggle fullscreen in Metal; resize the window; use high-DPI on and off; switch output Metal → surface → Metal.
- Run `voodoo_card=metal` under SDL1.
- Build against stock SDL 1.2: Metal must be absent and the old colour order must still work.
- SDL2 builds must be unchanged apart from the PBO note above.

**Depends on.** Sections 3 and 6 (`ConfigureLayer`). Section 4 for the 3dfx part.

---

### 9. Metal output: keep the window size the user chooses; fit the frame with aspect ratio

**What.** In `CMetal::Resize()`:
- A window the user has resized (`userResizeWindowWidth` and `userResizeWindowHeight` both non-zero) or maximized (`menu.maxwindow`) keeps its size, including across video mode and scaler-size changes.
- "Fit to aspect ratio" (`render.aspect`) no longer forces the window height from its width.
- With aspect on, the frame is drawn in the largest centred region of the drawable with the target ratio, black around it, in a window as well as in fullscreen. With aspect off it fills the window.

**Why.** On master, a mode change resized a window the user had sized, and with aspect on every resize forced the height from the width (the window "snapped"). The aspect-correct viewport was only used in fullscreen, so in a window the image was stretched. The surface and OpenGL outputs already behave the new way.

**How.**
- Compute `user_sized` as above.
- `reset_window_size` becomes false when `user_sized`.
- The scaler-size change (`render.scale.size != last_scalesize`) only resizes the window when not `user_sized`.
- Delete the block that, with `render.aspect`, read the real window width and set the height from the ratio.
- In the viewport computation, change the condition from `sdl.desktop.fullscreen && render.aspect` to just `render.aspect`. The letterbox/pillarbox math (compare drawable ratio with target ratio, centre) is unchanged.

**Testing.** Resize the window by hand, then change video modes (text → 320x200 → 640x480): the size stays. Maximize: the size stays. Toggle aspect: a window keeps its size, and the image is letterboxed or pillarboxed in a window and in fullscreen.

**Depends on.** Section 8 if you use the `metal_GetWindowSize` / `metal_SetWindowSize` helpers; otherwise independent.

---

### 10. Metal output: "fit to aspect ratio" takes effect right away

**What.** `Resize()` no longer returns early ("No change") when only `render.aspect` or the target aspect ratio changed.

**Why.** Toggling aspect, or picking another aspect ratio, resets the screen with the same window and frame size. The Metal output saw no change and kept the old viewport until the window was resized, so the menu toggle seemed to do nothing.

**How.**
- Add `last_aspect` (bool) and `last_target_ratio` (double) to `CMetal`.
- Extend the early-return test: it also requires `(render.aspect != 0) == last_aspect`, and, when aspect is on, `target_ratio == last_target_ratio`.
- Store both at the end of `Resize()` next to `last_window_w/h` and `last_tex_w/h`.

**Testing.** Toggle "Fit to aspect ratio" and change the aspect ratio from the menu with `output=metal`, in a window and in fullscreen. The picture updates immediately.

**Depends on.** Section 9.

---

### 11. Output menu: Metal no longer experimental

**What.** The menu item `output_metal` is labelled "Metal" instead of "Metal(Experimental)". It is listed directly after the OpenGL items ("OpenGL perfect"), before TrueType font, Game Link and Vulkan, in both `def_menu_video_output[]` (`menu.cpp`) and the `alloc_item` order in `AllocCallback1()` (`menu_callback.cpp`).

**Why.** With the fixes above, the Metal output is on par with the other outputs on macOS and is the only one with a hardware 3dfx back end that is not deprecated there.

**How.** Move the guarded entries and change the label. The guard is `C_METAL_OUTPUT` after section 8.

**Testing.** The Video → Output menu on macOS shows "Metal" after the OpenGL entries, and the check mark follows the active output.

**Depends on.** Sections 1–10 (policy, not code).

## KVM CPU core (`core=kvm`)

A CPU core that runs guest code with hardware virtualization through the Linux KVM interface
instead of emulating the CPU. DOSBox-X keeps emulating every device (PIC, PIT, VGA, sound, DMA,
IDE), the BIOS and DOS callbacks, and delivers interrupts itself; KVM only executes instructions.
It is a build option (`./configure --enable-kvm`, Linux on x86-64 only) and a value of `[cpu]
core`. The detailed plan, status, measurements and findings are in `KVM-TODO.md` at the root of
the main-ai branch; this section condenses it into an order of implementation.

The design constraint behind most of what follows: DOSBox-X keeps time by counting CPU cycles,
and KVM cannot count guest instructions. The KVM core therefore counts one cycle per nanosecond
of host time the guest runs. `CPU_CycleMax` becomes "nanoseconds of guest execution per emulated
millisecond": `cycles=max` then automatically leaves host time over for device emulation, and a
fixed `cycles=` value throttles the guest by duty cycle.

The second constraint is cost per exit. Every return from the guest to DOSBox-X (port I/O,
access to memory that is not mapped into the guest, a callback, HLT) costs several microseconds
on the test machine, against well under a microsecond for the same thing on an emulated core.
Most of the later steps exist to avoid exits (queued writes, direct mapping, a mode 13h shadow)
or to run the guest on an emulated core while exits would dominate (planar VGA).

All measurements below were made on an AMD Ryzen 9 7940HS host. `KVM-TODO.md` does not report
any testing on an Intel host, so the Intel-specific parts (TSS address, unrestricted guest check)
are untested. The test programs named below (CBBENCH, IOBENCH, VGABENCH, PLANTEST, HYBTEST and so
on) were small ad-hoc DOS, DOS/4GW and Win16/Win32 programs; they are not in the repository.

**Prerequisites from other sections** (implement those first):

- The INT3 callback prefix (`[cpu] callback int3 prefix`, `CALLBACK_WriteInstr*`,
  `CALLBACK_Int3PrefixFollows()`). Callback instructions are written as `CC FE 38 imm16` so that
  the KVM core sees them as #BP exits; KVM never passes #UD on an unknown opcode to userspace, so
  the plain `FE 38` form cannot work.
- Memory map change notification and page classification (`MEM_AddMapChangeHandler()`,
  `MEM_MapChanged()`, `MEM_MAPCHANGE_PHYS`/`MEM_MAPCHANGE_FIRSTMB`, `MEM_ClassifyPage()` with
  `MEM_PAGE_RAM`/`MEM_PAGE_READONLY`/`MEM_PAGE_DEVICE`).
- The neutral CPU state (`CPU_FullState`, `CPU_GetFullState()`, `CPU_SetFullState()`; fields laid
  out after `kvm_regs`, `kvm_sregs`, `kvm_fpu`, debug registers, SYSENTER MSRs).
- Segment access rights kept in the hidden part of the segment registers, and virtual 8086 mode
  segment loads setting base/limit/access rights as VM entry requires (see the sections on
  segment access rights and on V86 segment limits). The KVM core still normalises V86 segments
  on the way in, so it works without them, but less exactly.
- EMM386 running virtual 8086 mode with paging (see the EMM386 section), which is what calls the
  `PAGING_GuestPageTablesChanged()` hook described here.
- Two DOSBox-X bugs found while bringing the core up that affect every core, each in its own
  section: `VGA_SetupDrawing()` leaving `vga.draw.resizing` set after a setup in `M_ERROR` mode,
  and the RTC reporting "update in progress" for the whole first second after a reset. Both
  showed up under KVM first because the POST reaches the affected code at a different emulated
  time. Also "Don't hang at quit when a file is closed with the guest in protected mode", which
  makes the `CALLBACK_RunReal*()` functions call `CPU_SyncState()`.
- The DR0-DR3 hardware breakpoint section introduces `CPU_Debug_Run()`; the KVM core needs one
  line there (see "Build option, setting and VM setup").

### 1. Host time accounting: `PIC_HostTime*()`

**What.** Four functions in `include/pic.h` / `src/hardware/pic.cpp` that let a CPU core run the
guest for an amount of host time instead of a number of instructions, and charge the host time
used back to `CPU_Cycles`.

- `PIC_HostTimeNow()`: monotonic host time in nanoseconds (`std::chrono::steady_clock`, which is
  `CLOCK_MONOTONIC` on Linux; the slice timer later relies on that equivalence).
- `PIC_HostTimeMark()`: remember "now" as the start of guest execution (returns it).
- `PIC_HostTimeBudget()`: how many host nanoseconds the remaining `CPU_Cycles` stand for.
- `PIC_HostTimeAccount(overhead_ns = 0)`: charge the nanoseconds since the mark, less
  `overhead_ns` that were not guest execution, to `CPU_Cycles`; move the mark to now; return the
  cycles charged. (The `overhead_ns` parameter comes with the exit cost accounting, subsection
  11; implement it with a default of 0 first.)

**Why.** Emulated time in DOSBox-X is `CPU_CycleMax` cycles per emulated millisecond and
`CPU_Cycles` until the next PIC event. Keeping that unit means the PIC event queue, the cycle
auto-adjustment, `cycles=max` and fixed cycles all work unchanged with a core that cannot count
instructions.

**How.**

1. Conversion: one cycle per nanosecond while `CPU_CycleMax` is at most 1000000. Above 1000000
   (nanoseconds per millisecond) a cycle is shorter than a nanosecond: cycles = ns ×
   `CPU_CycleMax` / 1000000, so that an emulated millisecond never needs more than one host
   millisecond of guest execution. The budget is the inverse; round it up, because any cycles
   left must give the guest some time or emulated time would stop. A non-positive `CPU_Cycles`
   gives a budget of 0.
2. Clamp the charge to at most `max(CPU_Cycles, 0) + CPU_CycleMax`, i.e. a charge may overrun
   the slice by at most one emulated millisecond. Otherwise a long stop (debugger, host suspend,
   scheduling) would be paid back by the emulation afterwards.
3. Subtract the result from `CPU_Cycles`.
4. Convention, worth keeping: every nanosecond calculation made for the KVM core carries a
   comment saying it is host time in nanoseconds, so that nobody mistakes it for emulated time
   (cycles, milliseconds). The main-ai code marks them "KVM host time".

**Testing.** Done before the KVM core existed, with a stand-in core in a scratch build that spun
on host time like a vCPU with a simulated exit every 20 us (and every 1 ms) for 6 s of wall time:

- `cycles=max`: emulated time 100.02% of wall time; `CPU_CycleMax` settled near 915000 (about
  91% of each millisecond for the guest); thread CPU only about 3 points above guest time even
  with 320000 exits.
- `cycles=fixed 300000`: guest 29.9% of wall time, emulated time 100.00%.
- `cycles=fixed 1000000` and above: guest 97%, emulated time 2.6% slow, as with an emulated core
  whose fixed cycles are too high. Use `cycles=max` (or fixed below about 950000) with KVM.
- Emulated time at exits never went backwards with fixed cycles, and once per run with
  `cycles=max`, presumably where auto-adjust changed `CPU_CycleMax` in the middle of a
  millisecond (`PIC_TickIndex()` divides by it; existing behaviour, not verified).
- Worst overrun past a slice: 0.05-0.9 ms (host scheduling, timer granularity).

### 2. Port I/O without the I/O permission check: `IO_*_Checked()`

**What.** `IO_WriteB_Checked()`, `IO_WriteW_Checked()`, `IO_WriteD_Checked()`,
`IO_ReadB_Checked()`, `IO_ReadW_Checked()`, `IO_ReadD_Checked()` in `include/inout.h` and
`src/hardware/iohandler.cpp`. They are `IO_WriteB()` and so on without the
`CPU_IO_Exception()` call: the same `log_io()`, `IO_USEC_write_delay()`/`IO_USEC_read_delay()`
and `io_writehandlers[]`/`io_readhandlers[]` dispatch.

**Why.** Under KVM the hardware has already checked IOPL and the TSS I/O permission bitmap before
an I/O instruction exits to userspace. Checking again would be redundant, and wrong:
`CPU_IO_Exception()` reads DOSBox-X's copy of the flags and CPL, which is out of date while the
state is in the vCPU. A port that is denied in V86 mode raises #GP inside the guest, which goes
through the guest's IDT to DOSBox-X's (or another) V86 monitor as usual.

**How.** Add the six functions next to the existing ones; keep the I/O delay calls, since they
are how port access costs emulated time.

### 3. Keeping the CPU state outside DOSBox-X: `CPU_SyncState()` and friends

**What.** Hooks in `include/cpu.h` / `src/cpu/cpu.cpp` that let a core keep the CPU state in a
place of its own between calls (the vCPU), and make code outside the core fetch it before using
it.

- `CPU_StateSyncHook` (function pointer taking a `full` flag, NULL when no core holds the state).
- `CPU_SyncState()`: inline; calls the hook with `full = true` if set. Brings everything into
  DOSBox-X (registers, segments, control registers, flags, FPU/SSE, debug registers, MSRs), and
  makes DOSBox-X's copy the one that counts until the core runs again.
- `CPU_SyncRegisters()`: the same with `full = false`: only the general, segment and control
  registers and flags. That is all that interrupt delivery and callbacks use, and it is much
  cheaper than also fetching the FPU, debug registers and MSRs (three extra ioctls).
- `CPU_StateIFHook` and `CPU_InterruptFlag()`: the guest's interrupt flag without fetching the
  state. Without a hook it is `GETFLAG(IF)`.

With no such core active each is a NULL pointer test.

**Why.** Fetching the state after every exit, as a first version would, costs ioctls on every
exit. Keeping it in the vCPU and fetching it lazily makes the common exits (port I/O, MMIO,
queued writes) cheap. `CPU_InterruptFlag()` was added after DOS/4GW measurements: while the guest
ran with interrupts disabled and an IRQ was pending, `PIC_runIRQs()` fetched the state on every
slice only to find IF clear, a full state round trip per slice.

**How.** Call sites (each was needed by an observed failure or by inspection):

- `CPU_Interrupt()`: `CPU_SyncRegisters()` at the top (interrupts and exceptions raised by
  DOSBox-X code).
- `PIC_runIRQs()`: after the NMI checks, test `CPU_InterruptFlag()` instead of `GETFLAG(IF)`; only
  if it is set, call `CPU_SyncRegisters()` and go on to deliver.
- `CPU_OnReset()` and `On_Software_CPU_Reset()` (memory.cpp): `CPU_SyncState()` first.
- `CPU::Change_Config()`: `CPU_SyncState()` first, since the core may change.
- `SaveState::save()` and `SaveState::load()`: `CPU_SyncState()` first. On load, the loaded state
  then replaces DOSBox-X's copy and goes into the vCPU at the next run.
- `DEBUG_Enable_Handler()` and `DEBUG_EnableDebugger()`: `CPU_SyncState()`.
- Glide's port handler `write_gl()` (`src/hardware/glide.cpp`): `CPU_SyncRegisters()` before
  `MEM_BlockRead32()` of the parameters, because they are guest linear addresses and need the
  current paging state.

**Rule for new code** (worth a comment in `cpu.h`): anything outside a CPU core and outside
callbacks that reads or writes `reg_*`, `Segs`, `cpu.*` or the FPU must call `CPU_SyncState()`
(or `CPU_SyncRegisters()` if it only needs registers) first. Callbacks run with the registers
already fetched. Code that reads guest linear memory outside callbacks (device handlers, PIC
events) needs at least `CPU_SyncRegisters()` for the current CR3.

### 4. `DEBUG_NeedsEmulatedCore()`

**What.** A new function in `src/debug/debug.cpp` (declared in `include/debug.h`) that says
whether something in the debugger needs every instruction to go through an emulated core:

- with `C_HEAVY_DEBUG`: any breakpoint at all (`CBreakpoint::BPoints` not empty), `cpuLog`,
  `logHeavy`, `zeroProtect`, and an active RPC agent trace (`agent_trace_active` under
  `C_DOSBOX_AGENT`);
- without heavy debugging: only interrupt breakpoints (new `CBreakpoint::HasIntBreakpoints()`,
  which walks `BPoints` for `BKPNT_INTERRUPT`). Physical breakpoints are INT3 bytes written into
  memory in such builds, and the KVM core sees those as #BP.

The function needs to be a friend of `CBreakpoint`.

**Why.** KVM does not show DOSBox-X each instruction, so heavy-debug breakpoint checks, CPU
logging and BPINT cannot work inside it. The KVM core runs `CPU_Core_Normal_Run()` while this
returns true, so these features work exactly as with an emulated core (at emulated speed).

### 5. Build option, `core=kvm`, VM and vCPU setup, core selection

**What.** `./configure --enable-kvm` builds `src/cpu/core_kvm.cpp` (with `include/core_kvm.h`) and
defines `C_KVM`; `[cpu] core=kvm` selects the core where possible and falls back with a logged
reason otherwise.

**How.**

1. `configure.ac`: an `--enable-kvm` option (default no) and `AH_TEMPLATE(C_KVM, ...)`. Only for
   `linux*-x86_64` hosts; requires `linux/kvm.h`; error out otherwise. Add `core_kvm.cpp` to
   `src/cpu/Makefile.am` unconditionally and wrap the whole file in `#if C_KVM`, so it compiles
   to nothing elsewhere; add the `.cpp` and `.h` to `vs/dosbox-x.vcxproj` and its `.filters`.
2. `include/core_kvm.h`: under `C_KVM`, `KVM_CoreInit(std::string &reason)` (create the VM and
   vCPU, or bring an existing one up to date with the CPU settings; fills `reason` on failure),
   `KVM_CoreShutdown()`, and `CPU_Core_KVM_Run()` (the `cpudecoder`).
3. `master` already has stale declarations of `CPU_Core_KVM_Run` and `CPU_Core_KVM_Trap_Run` in
   `include/cpu.h` under `C_HAVE_LINUX_KVM_X86` (which `include/dosbox.h` defines whenever
   `linux/kvm.h` exists, from an earlier attempt). Remove them; `core_kvm.h` declares the core.
   Any `#if` that refers to the KVM core must test `C_KVM`, not `C_HAVE_LINUX_KVM_X86`, or a
   Linux build without `--enable-kvm` fails to link. In particular, `CPU_Debug_Run()` (from the
   DR0-DR3 breakpoint section) must hand straight over to the KVM core when `cpudecoder` is
   `CPU_Core_KVM_Run`: the debug registers are put into the vCPU and KVM handles them natively.
4. `src/dosbox.cpp`: add "kvm" to the `core` values only under `C_KVM`; extend the help text
   ("kvm runs guest code with hardware virtualization (Linux KVM). It is experimental and only
   in builds made with --enable-kvm."), and regenerate the reference config files.
5. `KVM_CheckHost()`:
   - open `/dev/kvm` (`O_RDWR|O_CLOEXEC`), with distinct messages for `ENOENT` (no KVM / no
     hardware virtualization) and `EACCES` (user not in the `kvm` group);
   - `KVM_GET_API_VERSION` must equal `KVM_API_VERSION` (12);
   - `KVM_CHECK_EXTENSION` for `KVM_CAP_USER_MEMORY`, `KVM_CAP_READONLY_MEM`,
     `KVM_CAP_SET_GUEST_DEBUG` (callbacks), `KVM_CAP_EXT_CPUID`, `KVM_CAP_SET_TSS_ADDR`;
   - if `/sys/module/kvm_intel/parameters/unrestricted_guest` exists and reads N/n/0, refuse:
     Intel CPUs run real mode and big real mode guests only with unrestricted guest; without it
     KVM emulates such code slowly and incompletely. AMD always can.
   - `KVM_CAP_NR_MEMSLOTS` at least 32.
6. `KVM_CoreInit()` on first use: `KVM_CREATE_VM`; `KVM_SET_TSS_ADDR` at `0xfffbd000` (needed on
   Intel; KVM's identity map page is at its default `0xfffbc000` just below, so guest physical
   `0xfffbc000-0xfffbffff` belongs to KVM and no memory slot may cover it; DOSBox-X maps nothing
   there, the BIOS alias is at `0xffff0000`). Do **not** call `KVM_CREATE_IRQCHIP`: DOSBox-X
   emulates the PIC and PIT. `KVM_CREATE_VCPU` 0; `KVM_GET_VCPU_MMAP_SIZE` and `mmap` the
   `kvm_run` area (`MAP_SHARED`); set the CPUID; register `KVM_CoreShutdown()` with
   `AddExitFunction()` once. Later steps add: the register sync capability, the coalesced MMIO
   ring and zones, `KVM_CAP_EXIT_ON_EMULATION_FAILURE`, `KVM_SET_GUEST_DEBUG` with
   `KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP` (so INT3 exits as `KVM_EXIT_DEBUG`), the
   `PAGING_GuestPageTablesHook`, and the memory map change handler (`MEM_AddMapChangeHandler`),
   with all of guest memory marked dirty so that the slots are built at the first `KVM_RUN` (the
   memory system is set up after the CPU). Log one line saying what the host offered (memory
   slots, immediate exit, registers through `kvm_run` or ioctls, video memory writes queued or
   exiting). Every failure closes everything again and fills `reason`.
7. CPUID (`KVM_SetCPUID()`): DOSBox-X's own, so the guest sees the emulated CPU type including a
   custom `cpuid string`. Get `KVM_GET_SUPPORTED_CPUID` (grow the buffer on `E2BIG`). Run
   `CPU_CPUID()` for leaf 0 to get the maximum leaf, then for each leaf (cap at 32), saving and
   restoring EAX/EBX/ECX/EDX around it. For leaf 1, mask EDX and ECX with what the host supports
   and log the bits removed. Set it with `KVM_SET_CPUID2`. If the CPU type has no CPUID
   (`CPU_CPUID()` returns false), set an empty table and log that CPUID still runs and returns
   zeros under KVM.
8. If `KVM_CoreInit()` is called again with the vCPU existing (CPU type change at run time), only
   set the CPUID again. Pitfall: newer kernels refuse `KVM_SET_CPUID2` with different values once
   the vCPU has run, so a run-time CPU type change really needs a new vCPU (keeping the state
   through `CPU_FullState`). Not done.
9. Core selection in `CPU::Change_Config()`: the `core == "kvm"` branch only sets the normal core
   and a `want_kvm` flag. The decision is made at the end, once the CPU type and features are
   known: call `CALLBACK_ChooseInt3Prefix()` (made public in `callback.cpp`/`callback.h`; it
   decides once, and the CPU is set up before `CALLBACK_Init()`), then fall back with
   `CPU: core=kvm is not used: <reason>. Using the <normal|emulated> core.` when
   `CALLBACK_Int3Prefix` is off (tell the user to set `callback int3 prefix=true` or start with
   `core=kvm`), when `CPU_ArchitectureType < CPU_ARCHTYPE_386`, or when `KVM_CoreInit()` fails.
   Otherwise set `cpudecoder = CPU_Core_KVM_Run`, `core_mode` "kvm", and log that the
   experimental KVM core is used. The automatic core switching only applies to `core=auto`, so it
   leaves the KVM core alone.
10. Cycles when the KVM core is chosen: with `cycles=auto` (the `CPU_AUTODETERMINE_CYCLES` bit)
    clear that bit and switch to auto-adjust as `cycles=max` does (`CPU_CycleAutoAdjust = true`,
    `CPU_CycleLimit = -1`), logging it; the small fixed real mode count of `cycles=auto` would
    starve the guest when a cycle is a nanosecond. With auto-adjust, start at 500000
    (`CPU_CycleMax` and `CPU_OldCycleMax`) instead of a few thousand, or every return to DOSBox-X
    (each callback) costs emulated milliseconds until the adjustment catches up.
11. Call `menu_update_core()` after the decision (see subsection 18).

Prefetch CPU types (`*_prefetch`) already stop DOSBox-X with an error unless the core is normal
or auto; `kvm` counts as "not normal" there, which is fine.

**Testing.** VM and vCPU created on the AMD host (32764 memory slots, immediate exit supported);
each fallback reason provoked; CPU types 486old (no CPUID), Pentium III, Pentium 4 with SSE2; the
INT3/mouse/V86/DOS/4GW test suite with `core=kvm` (prefixed callbacks and a live VM, before the
run loop existed), EMM386 on and off. Also a Linux build without `--enable-kvm` must still link.

**Depends on.** The INT3 callback prefix section.

### 6. Guest memory as KVM memory slots

**What.** Guest physical memory is mapped into KVM memory slots built from `MEM_ClassifyPage()`,
and rebuilt incrementally from the memory map change notifications before each `KVM_RUN`.

**How.**

1. Data: a sorted vector of slots (id, first page, page count, host pointer, read-only flag,
   dirty-log flag), a free list of slot ids and the next unused id, the number of guest pages
   covered, a dirty page range `[dirty_lo, dirty_hi]`, and the paging state the first MB layout
   was built for.
2. The map change handler (registered in `KVM_CoreInit()`) only widens the dirty range:
   `MEM_MAPCHANGE_FIRSTMB` with no page count marks pages below `LINK_START` (1MB+64KB); any other
   change with no page count marks everything; otherwise the given pages.
3. `KVM_PageAt(page)`: while guest paging is off, translate pages below `LINK_START` through
   `paging.firstmb[]` first (DOSBox-X's EMS without V86 mode remaps the first MB that way, and
   KVM's physical address space must show it), then `MEM_ClassifyPage()`. (The mode 13h shadow,
   subsection 15, hooks in here too.)
4. `KVM_ApplyMapChanges()`, before every `KVM_RUN`:
   - the page count is `MEM_PageMask() + 1`, capped at 4GB of pages; if it changed, mark
     everything dirty;
   - if `paging.enabled` differs from the state the first MB was built for, mark the first MB
     dirty (the `firstmb` remap only applies while paging is off);
   - nothing dirty: return;
   - widen the range so that every existing slot overlapping it is rebuilt whole;
   - walk the pages and collect runs of mappable pages: not `MEM_PAGE_DEVICE`, a non-NULL host
     pointer, a **page-aligned** host pointer, not one of KVM's reserved pages. A run continues
     while the read-only and dirty-log flags match and the host pointer is contiguous;
   - compare with the existing slots in the range: keep identical ones, delete the others
     (`KVM_SET_USER_MEMORY_REGION` with size 0) and free their ids, add new runs with free or new
     ids (fail if out of slots). `MEM_PAGE_READONLY` becomes `KVM_MEM_READONLY` (ROM: reads
     direct, writes exit to the handler);
   - keep the slot list sorted by page.
5. Pages with no slot are MMIO: every access exits as `KVM_EXIT_MMIO` and goes to
   `physdev_read*()`/`physdev_write*()`.
6. **Quick first build** (step 22). The first build used to take 162 ms: above RAM the handler
   table starts out empty, and looking a page up asks every device whether it claims it
   (`MEM_SlowPath()`). Add `MEM_PageHandlerKnown(page)` to `memory.cpp`/`paging.h`: apply
   `mem_alias_pagemask_active`; inside the handler table (and outside the Glide LFB range when
   Glide is enabled), true if `memory.phandlers[page]` is non-NULL; outside the table, true. In
   the walk, for pages at or above `LINK_START`:
   - if the handler is not known, leave the page as MMIO and set its bit in a 1-bit-per-page
     "deferred" bitmap (128 KB for 4GB) instead of looking it up;
   - remember the last handler seen that is not readable (`PFLAG_READABLE` clear, i.e. no memory
     behind it) and skip further pages with that same handler without classifying them; most of
     the 4GB is one such handler.
   In the MMIO exit handler, if the page was deferred and the access made its handler known,
   clear the bit and mark the page dirty so that it gets a slot at the next build (a PCI LFB, for
   example, gets its slot at its first access).
7. **Page-aligned video memory** (step 15): in `VGA_SetupMemory()`, allocate `vga.mem.linear`
   with 4096 bytes of slack and align it to 4096 instead of 16. Then the pages that the SVGA bank
   map (`VGA_Map_Handler`) and the LFB give as host memory become RAM slots, and banked VESA
   modes and the LFB run without exits.

**Pitfalls.**

- Host pointers must be page-aligned for slots. `MemBase` is when it comes from mmap (normal Linux
  build, memory file); it is not from `malloc` (GameLink) or the `new[]` fallback, and then RAM
  gets no slots (all of RAM would be MMIO). The core does not check or log this yet.
- Pages above 4GB (`memory file` + `Mem4GBPageHandler`) are not covered; PSE-36/PAE guests that
  use them would need slots of their own.
- When two devices claim a page (`MEM_SlowPath` conflict), the handler table entry stays empty;
  `MEM_ClassifyPage` returns whichever handler won.

**Testing.** Startup to the DOS prompt and the test suite (step 8-13 milestone). After step 22
the first build takes a few ms instead of 162 ms, every program starts about 0.25 s sooner
(CBBENCH 1.5 s to 1.25 s), and a rebuild of the whole range (A20 change) takes about 2 ms. Banked
VESA (VESABNCH, mode 101h, 20 frames of 5 banks): 0.6 s against 0.70 s on the dynamic core. A
DOS/4GW program mapping the LFB in mode 4101h with DPMI 0800h and writing/reading bytes, words and
dwords: identical to the normal core with no exits.

**Depends on.** Memory map change notification and page classification.

### 7. Moving the CPU state between DOSBox-X and the vCPU

**What.** The state stays in the vCPU from slice to slice and is fetched lazily through the hooks
of subsection 3. Two flags track where it is: `in_kvm` (registers) and `rest_in_kvm` (FPU, debug
registers, MSRs).

**How.**

1. **Registers through `kvm_run`** (step 15). If `KVM_CAP_SYNC_REGS` reports both
   `KVM_SYNC_X86_REGS` and `KVM_SYNC_X86_SREGS`, initialise `kvm_run->s.regs` with
   `KVM_GET_REGS`/`KVM_GET_SREGS` once and set `kvm_valid_regs` to those two bits. KVM then stores
   the registers there at every exit and loads what `kvm_dirty_regs` marks before the next run,
   without ioctls. When writing, always mark `REGS` dirty but mark `SREGS` dirty only if the
   segment state differs from what is there (loading segment registers is the expensive part
   and they mostly stay the same). Do **not** sync the vCPU events through `kvm_run`
   (`KVM_SYNC_X86_EVENTS`): KVM would fetch them at every exit, and they are only needed after a
   run that a signal ended. Fall back to the ioctls without the capability.
2. **Putting the state** (`KVM_PutState(regs, rest)`), starting from `CPU_GetFullState()`:
   - segments ES, CS, SS, DS, FS, GS from `CPU_FullState` into `kvm_segment` (selector, base,
     limit, type, present, DPL, D/B, S, G, AVL, unusable; L always 0). Set the accessed bit in
     the type of any usable code/data segment, as the CPU does on a load;
   - in virtual 8086 mode, overwrite every segment with what VM entry demands: base = selector
     << 4, limit FFFFh, type 3, S=1, DPL 3, present, no D/B/G/AVL, usable;
   - TR: if unusable (DOSBox-X never loaded one), give the vCPU a usable busy 32-bit TSS (type
     0Bh, base 0, limit FFFFh), since VM entry requires one; LDT as is;
   - **keep the vCPU's own descriptor cache** for every segment register (and TR, LDT) whose
     selector, base and limit are unchanged since the fetch, provided CR0.PE and EFLAGS.VM are
     the same as in the vCPU and the guest is not in V86 mode. Reason: when the state was fetched
     between DOS/4GW setting or clearing CR0.PE and the far jump after it, DOSBox-X rebuilt the
     real mode segments' access rights from the protected mode descriptor tables, made CS not
     present, and the guest triple faulted. With the segment access rights section implemented
     this is no longer strictly needed, but it was kept;
   - GDTR, IDTR, CR0, CR2, CR3, CR4; general registers, EIP, EFLAGS with bit 1 forced on;
   - if an event may be in flight (item 6) and anything differs from what the vCPU has, drop it
     first;
   - the rest: `KVM_SET_FPU` (control, status, abridged tag word, ST0-7 as 10 bytes, XMM0-7,
     MXCSR), `KVM_SET_DEBUGREGS` (DR0-3, DR6, DR7), `KVM_SET_MSRS` for the SYSENTER MSRs 174h,
     175h, 176h.
3. **Fetching the state** (`KVM_GetState(regs, rest)`): start from `CPU_GetFullState()` (keeps
   what is not fetched, and DR4/DR5), fill in from `kvm_regs`/`kvm_sregs` and/or the FPU, debug
   register and MSR ioctls, and `CPU_SetFullState()`. Derive CPL: 0 in real mode, 3 in V86 mode,
   otherwise SS.DPL. A null TR or LDT selector comes back as unusable. **If paging is on, call
   `PAGING_ClearTLB()`**: the guest changes its page tables inside the vCPU (INVLPG, CR3 reloads
   with the same value) without DOSBox-X seeing it, and the callbacks reach guest buffers
   through DOSBox-X's TLB. Without this, Windows 3.1 enhanced mode failed with "Error loading
   USER.EXE" (WIN386 reloads CR3 with the same value after changing its tables).
4. **The sync hook** (`KVM_SyncHook(full)`): fetch the registers if they are in the vCPU, and the
   rest too if `full` and it is there; finish any pending instruction first (item 5); clear the
   flags; drop `CPU_StateIFHook` when the registers are out of the vCPU and `CPU_StateSyncHook`
   when nothing is left in it; after fetching the rest, set "event in flight" (the caller may
   replace the state entirely: save states, the debugger). On ioctl failure, log and mark the
   core failed.
5. **Unfinished instructions** (step 15). KVM completes an I/O or MMIO exit's instruction (with
   the data the handler left in `kvm_run`) only at the next `KVM_RUN`, and a REP string
   instruction exits again for each element; until then the vCPU state is not consistent. Set a
   `pending` flag on every I/O and MMIO exit. `KVM_CompletePending()` (called by the sync hook
   before fetching) runs `KVM_RUN` with `immediate_exit = 1` (the method the KVM API
   documentation gives) in a loop: apply queued writes, and if it exits again for MMIO or I/O
   (the next REP element), handle that and repeat. Without it, DOSBox-X delivered an IRQ into the
   middle of a `REP SCASB` on video memory.
6. **Events in flight** (step 15). When the slice timer signal ends `KVM_RUN` while KVM is
   delivering a software interrupt or exception (typically after a page fault on the stack,
   right after a memory slot change), KVM keeps the event and delivers it again at the next
   `KVM_RUN`, on top of whatever state DOSBox-X put there. If DOSBox-X delivered an IRQ in
   between, the return address pushed was wrong: a VESA bank switch `INT 10h` ended in the BIOS
   boot failure stub about one run in 20. `KVM_GET_VCPU_EVENTS` does not even report software
   interrupts (the API has no field for the instruction length), so this has to be done blind:
   `KVM_DropEvent()` reads the events, clears `exception.injected` and `interrupt.injected`,
   turns an injected NMI into a pending one (`KVM_VCPUEVENT_VALID_NMI_PENDING`), and writes them
   back. The instruction has not completed (RIP still points at it), so the guest raises the
   event again when it gets back there. Do this only when an event can be in flight (after a
   `KVM_RUN` that returned with an error/EINTR or `KVM_EXIT_INTR`, and after a full fetch; start
   with it set), and only when DOSBox-X actually changed the state; I/O, MMIO, callback and HLT
   exits happen between instructions, and an interrupt window only opens when nothing is being
   delivered. (Step 19 measured the unconditional version as a noticeable part of a callback
   round trip.)
7. **Entering** (`KVM_EnterState()`, at the top of every iteration of the run loop): put whatever
   is not in the vCPU; when the registers were put, take the guest IF from `reg_flags`; set both
   flags and install `CPU_StateSyncHook = KVM_SyncHook` and `CPU_StateIFHook` (returns the IF
   from the last exit, `kvm_run->if_flag`).

**Testing.** VRT, SAVESTATE and LOADSTATE (debugger commands from another section) while the KVM
core runs a loop; DOS/4GW raw mode, 48 runs without EMM386 and 18 with it, all passing after the
descriptor cache change (they triple faulted now and then before); Windows 3.1 enhanced mode
after the TLB clear. Callback round trip about 25 us before `KVM_CAP_SYNC_REGS` and lazy rest,
about 12 us after.

**Depends on.** `CPU_FullState`; subsection 3.

### 8. The run loop

**What.** `CPU_Core_KVM_Run()` is the `cpudecoder` for `core=kvm`. It runs the guest in time
slices bounded by a timer and handles the exits.

**How.** `KVM_Run()`, in this order:

1. Debugger (`C_DEBUG` builds): if the debugger is stopped (`IsDebuggerActive()` and not
   `IsDebuggerRunwatch()`), or `inhibit_int_breakpoint` is set (RUN, F5 and F10 stepping one
   instruction past a breakpoint: KVM cannot run exactly one instruction), or
   `DEBUG_NeedsEmulatedCore()`: `CPU_SyncState()` and return `CPU_Core_Normal_Run()`. This is
   evaluated on every call, so it can change at any time.
2. If the core has failed: `CPU_SyncState()`, set `cpudecoder` to the normal core and run it.
3. `CPU_Cycles <= 0`: return.
4. Create the slice timer on first use (from the thread that runs the vCPU): `sigaction` for
   `SIGRTMIN+4` with `SA_RESTART` (so other system calls it interrupts carry on), handler sets a
   `fired` flag and `kvm_run->immediate_exit = 1` (which also covers the signal arriving just
   before `KVM_RUN`); `timer_create(CLOCK_MONOTONIC)` with `SIGEV_THREAD_ID` aimed at
   `gettid()` (define `sigev_notify_thread_id` as `_sigev_un._tid` where libc lacks it).
5. Loop:
   - `KVM_EnterState()`; then the guest TLB flush if requested (subsection 17); then
     `KVM_ApplyMapChanges()`; then reload the mode 13h shadow if DOSBox-X wrote video memory
     (subsection 15). Any failure: `KVM_Fail()`.
   - `request_interrupt_window = 1` if `PIC_IRQCheck` and the guest's IF is clear, so the run
     ends as soon as the guest can take the IRQ.
   - Budget = `PIC_HostTimeBudget()`; 0 ends the slice; at least 2 us, so the guest gets
     somewhere even when the slice is shorter than an entry and exit.
   - If the timer fired outside `KVM_RUN` (between core calls, or during
     `KVM_CompletePending()`, which uses `immediate_exit` itself), treat it as disarmed: clear
     `fired`, the deadline and `immediate_exit`.
   - Arm the timer (`KVM_TimerArm(budget)`, below), `PIC_HostTimeMark()`, `KVM_RUN`, save errno.
   - Immediately after: count the run; clear `pending`; set "event in flight" if `KVM_RUN`
     failed or the exit was `KVM_EXIT_INTR`; compute the elapsed time; `PIC_HostTimeAccount()`
     with the exit cost estimate (subsection 11); **apply the queued writes** (subsection 12)
     before handling anything; record `kvm_run->if_flag` as the guest IF.
   - `KVM_RUN` returned -1 with `EINTR`/`EAGAIN`: the timer (or another signal). Clear
     `immediate_exit`, `fired` and the deadline; end the slice if `CPU_Cycles <= 0`, otherwise
     continue. Any other error: disarm and `KVM_Fail()`.
   - Dispatch on `exit_reason` (subsection 9).
   - After an exit handled in the loop: stop if the core failed or `CPU_Cycles <= 0`; also end
     the slice if less of it is left than the cost of the exit just taken (another `KVM_RUN`
     would most likely be cut short by the timer, an exit of its own; the cycles left stay for
     the next slice).
6. `CPU_Core_KVM_Run()` wraps this: if the planar VGA emulated core is active, run that instead
   (subsection 16); otherwise `KVM_Run()`, then store the mode 13h shadow's dirty pages to VGA
   memory, then run a single instruction on the normal core if KVM could not emulate one
   (subsection 14), then the planar VGA check.

**The slice timer** (steps 15 and 19). Arm it once per slice at an **absolute** deadline
(`timer_settime` with `TIMER_ABSTIME`; `PIC_HostTimeNow()` is `CLOCK_MONOTONIC`, so the deadline
in its units can be used directly), not around every `KVM_RUN`. `KVM_TimerArm(ns)` keeps the
current deadline if one is armed, it is not later than the new one, and it is early by at most
max(ns/4, 20 us); otherwise it re-arms (earlier when the slice got shorter, e.g. an earlier PIC
event; later when the old deadline would fire more than a quarter early, because exits are not
charged to the guest and the slice takes longer than its budget). Do **not** disarm it when the
core returns: the core is usually called again within microseconds with about the same deadline,
and disarming plus re-arming are two system calls per callback. This is why the handler needs
the separate `fired` flag.

**Why these details.** Each was measured. Step 19 profiled CBBENCH (100000 `INT 21h` AH=30h): per
call about 5.5 us inside `KVM_RUN` (VM exit and entry, KVM's FPU and vCPU switching), about 3 us
in DOSBox-X (the callback and the main loop, as on an emulated core), under 1 us in the KVM core.
Not disarming the timer, writing the vCPU events back only when needed and not syncing events
through `kvm_run` are what was left to cut.

**Testing.** A `dec ecx`/`jnz` loop of 3.2 billion iterations: 1.9 s (dynamic core 8.5 s).
CBBENCH 2.8 s before step 15, 1.4-1.5 s after step 19 (dynamic core 0.55 s); the remaining cost
is the exit itself. IOBENCH (1000000 `IN AL,61h`): 11.0 s (dynamic 1.65 s): port reads always
exit, and `cycles=max` settles at about 21000 there, so slices are a few exits long and arming
the timer is a visible part of each.

### 9. Exit handlers, interrupts and the debugger

**How.**

- **`KVM_EXIT_IO`**: set `pending`; for each of `io.count` elements at `io.data_offset` in
  `kvm_run` (REP string I/O may batch), call `IO_Write*_Checked()` or put the result of
  `IO_Read*_Checked()` there, by `io.size`. Count exits per port and direction for the
  statistics (subsection 19).
- **`KVM_EXIT_MMIO`**: set `pending`; `physdev_write*()`/`physdev_read*()` by length (1, 2, 4;
  8 as two dwords; anything else bytewise), little-endian through `host_readw/d`,
  `host_writew/d`; the deferred page check of subsection 6; for A0000-BFFFF, add the MMIO exit
  cost to the planar VGA window (subsection 16).
- **`KVM_EXIT_INTR`**: nothing (the timer).
- **`KVM_EXIT_IRQ_WINDOW_OPEN`**: return `CBRET_NONE`, so that the main loop's `PIC_RunQueue()`
  → `PIC_runIRQs()` delivers the IRQ.
- **`KVM_EXIT_HLT`**: `CPU_SyncRegisters()`, then `CPU_HLT(reg_eip)` (KVM's RIP is already past
  the HLT, as with the emulated cores) and return. `HLT_Decode` waits for the interrupt in
  emulated time and then restores `cpu.hlt.old_decoder`, which is the KVM core.
- **`KVM_EXIT_DEBUG` with exception 3** (#BP; `debug.arch.pc` is the INT3's linear address):
  `CPU_SyncRegisters()`; set `reg_eip` to pc minus the CS base, masked to 16 bits unless
  `cpu.code.big`. Then, in this order:
  1. builds with `C_DEBUG` and without `C_HEAVY_DEBUG`: `FillFlags()` and `DEBUG_Breakpoint()`;
     if it fires, return `debugCallback`. This is checked first, also on a callback's INT3
     prefix, as the emulated cores do;
  2. `CALLBACK_Int3PrefixFollows(pc + 1)`: read the callback number with `mem_readw(pc + 3)`,
     advance EIP by 5 (INT3 + FE 38 + imm16) and return the callback number, which the main loop
     runs as for any core;
  3. `C_DEBUG`: `DEBUG_IntBreakpoint(3)` → `debugCallback`;
  4. otherwise the guest's own INT3: `CPU_SW_Interrupt_NoIOPLCheck(3, eip + 1)`.
  Any other debug exception: fail.
  Page faults: while the KVM core runs, `dosbox_allow_nonrecursive_page_fault` is set. The only
  linear reads it does itself are the bytes after an INT3. The `FE 38` check stays within the
  INT3's page; the callback number (pc+3) can only fault for a callback instruction straddling a
  page boundary, which then becomes a #PF in the guest at the INT3, as an emulated core's
  instruction fetch would. Inside callbacks, guest page faults go through `PAGING_PageFault()`'s
  existing recursive path (`PageFaultCore` on the full core), which does not involve KVM.
- **`KVM_EXIT_SHUTDOWN`** (triple fault): `CPU_SyncState()`, log CS:EIP, then
  `On_Software_CPU_Reset()` if `cpu_triple_fault_reset`, else `E_Exit()`.
- **`KVM_EXIT_FAIL_ENTRY`**: fail with the hardware reason.
- **`KVM_EXIT_INTERNAL_ERROR`**: suberror `KVM_INTERNAL_ERROR_EMULATION` → subsection 14.
  Anything else: fail with the suberror, the `internal.data[]` words, the vCPU events and the IDT
  base/limit (added to chase the Windows 95 shutdown error in the open issues).
- **Anything else**: fail with the exit reason.

**`KVM_Fail(why)`**: log the reason; `CPU_SyncState()`; log CS:EIP, linear address, CR0, EFLAGS
and 16 code bytes (read with `mem_readb_checked`, stopping at a fault); store and drop the mode
13h shadow; mark failed, clear the flags and both hooks, set `cpudecoder` to the normal core and
call `menu_update_core()`. The guest continues on the normal core.

**Interrupts.** DOSBox-X delivers them itself with `CPU_HW_Interrupt()` from `PIC_runIRQs()`, to
the state in DOSBox-X (real mode IVT, protected mode IDT and gates, V86), which then goes back
into the vCPU. `KVM_INTERRUPT` is not used, there is no in-kernel irqchip, NMI is not passed on.

**Debugger.** Subsection 8 item 1 covers the stopped debugger, stepping and heavy debugging.
Without heavy debugging, physical breakpoints are INT3s in memory and come back as #BP (handled
above).

**Testing.** The INT3/mouse/V86/DOS/4GW suite with EMM386 off and on; the debugger breakpoint test
with F5, RUN and RUNWATCH; STEPIN into a callback. Boots to the DOS prompt in about 0.5 s
(dynamic core 0.16 s) before step 22. The Linux install CD boots and idles with HLT, same log as
the dynamic core.

### 10. Protected mode, V86, DOS extenders and Windows (step 14 findings)

No separate code beyond what is described in subsections 3 and 7 (the TLB clear on fetch, the
descriptor cache retention, `CPU_InterruptFlag()`), but this is the guest coverage those changes
were made for, and it is the regression set:

- Windows 3.1 standard (`win /s`) and enhanced (`win /3`) mode: a Win16 test (`GetWinFlags()`,
  a 1 MB huge block written and checked, the FPU, 2 s of `WM_TIMER`) gives the same results as
  the normal core and Windows exits normally. Windows 3.0 the same (its 1 MB allocation fails in
  standard mode on the normal core too).
- Windows 95 (build 950) from a fresh copy of the image runs a Win32 test from `WIN.INI` (16 MB
  written and checked, the FPU, `Sleep(2000)` against `GetTickCount()` and the performance
  counter, two threads with interlocked increments): same as the dynamic core, `Sleep(2000)`
  measures 2003-2007 ms by both clocks, and it shuts down. Use a fresh image copy per run
  (Windows may otherwise show a daylight saving time dialog, for example).
- Windows 98 (Pentium II, 128 MB, ISA PnP BIOS, APM, IDE `pio32`/`int13fakev86io`, with `WIN`
  in `AUTOEXEC.BAT` because the image has `BootGUI=0`): the same Win32 test passes and Windows
  powers off through APM: 51 s on the dynamic core, 81 s on KVM before subsection 16, 72 s after.
- DOS/4GW raw and VCPI, EMM386 off and on; DOS/32A, PMODE/W and CauseWay with the LFB and Voodoo
  tests; Quake 1.06 under CWSDPMI (its own paging) and HDPMI32; Spycraft (DOS/4GW, SVGA, SB16,
  MPU-401) for a minute with the same video modes and BIOS calls as the normal core; OS/2 Warp
  3/4 installers stop at the same place as on the dynamic core (floppy emulation).
- Windows 2000 text-mode and GUI setup through IDE, up to where the answer file has no product
  key (see subsection 13 for why it first took over 30 minutes).

### 11. Not charging KVM's exit costs as guest time (step 16)

**What.** `KVM_Overhead(r, elapsed)` estimates how much of each `KVM_RUN`'s elapsed host time was
KVM's own work rather than guest execution, and `PIC_HostTimeAccount()` takes it off.

**Why.** Charged as guest time, every exit costs thousands of cycles where an emulated core
counts one instruction, so an exit-heavy guest crawls at low fixed cycle counts. A fixed
`cycles=30000` VGA test (84000 MMIO exits, 84000 queued writes) took 32.5 s (normal core 0.3 s).

**How.**

1. Keep estimates (average and minimum, in ns) for four exit kinds (port I/O, MMIO, debug i.e.
   callback, HLT), for a queued write by a single instruction, and for a queued write by a
   string instruction (a "burst").
2. Update rule (`KVM_Estimate()`): track the minimum; if filtering, only take samples at most
   four times the minimum (so samples with real guest execution in them hardly count); average
   over about the last 64 samples (subtract 1/64 of the average, add 1/64 of the sample); cap
   the returned value at 20 us.
3. Per run: for an I/O, MMIO, debug or HLT exit (and an internal error with suberror
   `KVM_INTERNAL_ERROR_EMULATION`, which costs about what an MMIO exit does), if no writes were
   queued during the run, update that kind's estimate with the whole elapsed time (filtered) and
   use it; otherwise just use the current estimate. A run ended by the timer signal has no exit
   overhead. Remember the result as the "last exit cost" (used to end slices early, subsection
   8).
4. If writes were queued (count from the ring's `first`/`last`, before applying them): 64 or more
   use the burst estimate, otherwise the single-write one; update it with (elapsed − exit
   overhead) / writes (filtered only for single writes) and add writes × the estimate (capped at
   20 us each).
5. The slice then takes longer than its budget, which is why `KVM_TimerArm()` moves a deadline
   that would fire more than a quarter early, and why a slice ends at an exit when less of it is
   left than the exit cost.
6. The estimates are errors in favour of charging the guest, so that it never gets free time.

**Testing.** The `cycles=30000` VGA test: 32.5 s to 1.5 s (normal core 0.3 s; later 0.56 s with
the mode 13h shadow). A Sound Blaster test (8-bit single-cycle DMA, 32000 bytes at 22050 Hz,
three times, timed with the BIOS tick count to the IRQ) measures 26 ticks per block on the normal
core and on KVM, at `cycles=max` and `cycles=30000`; `DX-CAPTURE /A` recorded the same audio
(three 1.44 s blocks with 40 ms gaps). Windows 95 `Sleep(2000)` as above. IOBENCH got slower
(9.9 s to 11.0 s) because of short slices and timer arming (open issue). The exit-time log line
shows the estimates. (Separately measured on the test host: a single queued write costs about
3-4 us, a write of a string instruction well under 1 us.)

**Depends on.** Subsection 1 (the `overhead_ns` parameter).

### 12. Queued video memory writes (coalesced MMIO)

**What.** Writes to A0000-BFFFF (and on PC-98 also E0000-E7FFF, the fourth graphics plane) are
queued by KVM in the coalesced MMIO ring instead of each exiting to userspace, and applied in
order as soon as `KVM_RUN` returns.

**How.**

1. In `KVM_CoreInit()`: `KVM_CHECK_EXTENSION(KVM_CAP_COALESCED_MMIO)` returns the page offset of
   the ring inside the `kvm_run` mapping. `KVM_REGISTER_COALESCED_MMIO` a zone at A0000 size
   20000h; if that works, the ring is at `kvm_run` + offset × page size, and its capacity is
   (page size − ring header) / entry size. On PC-98, also a zone at E0000 size 8000h.
2. `KVM_FlushWrites()`: while `first != last`, take the entry at `first`; a port write (`pio`,
   subsection 13) goes to `IO_Write*_Checked()` by length; a memory write to `physdev_write*()`
   (1, 2, 4; 8 as two dwords; otherwise bytewise); memory barrier; advance `first` modulo
   capacity. Count writes to A0000-BFFFF for the planar VGA estimate.
3. Call it right after every `KVM_RUN` (also the ones in `KVM_CompletePending()`), **before** any
   exit is handled, so the order against reads (which still exit) and port I/O is kept, and
   anything DOSBox-X does with the device afterwards sees the writes.

**Pitfall.** A queued write still costs an exit inside the kernel: a single write instruction is
a full exit that KVM handles itself (about 3-4 us); only the writes of a string instruction are
queued by KVM's instruction emulator in one go. Only the userspace round trip is saved.

**Testing.** VGABENCH (mode 13h, 20 full-screen fills): 6.6 s to 0.8 s (dynamic 0.51 s). PC-98: a
test writing text VRAM (A000) and the graphics planes (A800, B000, B800, E000) and dumping the
readback gives byte-identical dumps on the normal core and KVM, with no MMIO exits.

### 13. Queued port writes (coalesced PIO), VGA data ports, IDE

**What.** With `KVM_CAP_COALESCED_PIO` (not on PC-98, whose port map differs), writes to ports
whose effect the guest cannot see before its next exit are queued in the same ring as the video
memory writes (zones registered with `pio = 1`), so they stay in order with them. Reads still
exit, after the queue is applied.

**Ports and the reason for each.**

- 80h, EDh: I/O delay ports.
- 3C6h-3C9h: the VGA DAC (palette loads).
- 43h: PIT command port; a counter latch takes effect at the read that follows, which exits.
- 0Ch, D8h: DMA flip-flop resets, which come before reading a DMA address or count.
- 3B4h, 3C4h, 3CEh, 3D4h: VGA index registers, which only select what the following data port
  access reads or writes. A word OUT to an index port also writes the data port, falls outside
  the one-port zone and exits, which is correct.
- 1F0h-1F3h and 170h-173h: the IDE data port and the taskfile registers after it. KVM's
  instruction emulator does `REP OUTSW`/`OUTSD` one element per exit (it batches `REP INSW`), so
  writing a sector was 256 exits. A write takes effect at the command register (1F7h/177h) or
  when the guest reads status, both of which exit. Side effect: the IRQ at the end of a sector
  arrives at the next exit, at most a time slice later.
- Not the VGA data registers in general: they can change the memory map (an SVGA bank, the
  memory window), which must take effect before the guest's next access to directly mapped video
  memory.
- The sequencer and graphics controller **data** ports 3C5h and 3CFh are queued only while no
  page of A0000-BFFFF is mapped directly (`KVM_QueueVGAData(on)`). Then every video memory access
  is MMIO (reads exit, writes queued), so map mask, bit mask, set/reset, write mode and so on are
  applied in order with the memory accesses, even when a port write changes the memory map (the
  slots follow at the next `KVM_RUN`). Implementation: for 3C4h and 3CEh, unregister the
  two-port zone (which also removes a one-port zone inside it) and register a zone of size 2
  (index and data) or 1 (index only). Decide at the end of every `KVM_ApplyMapChanges()` by
  whether any slot overlaps pages A0h-BFh (the mode 13h shadow or the SVGA bank map count as
  directly mapped).

Ports that are polled most and cannot be queued (reads, or writes with immediate effect such as
PIC EOI and RTC data): Quake polls PIT channel 0 (latch at 43h, two reads of 40h), the SB16 DMA
count (C6h after a D8h reset), 201h and 22Fh; Spycraft polls 331h (MPU-401 status), 70h/71h, the
PIC and DMA count; Linux the PIT, CRTC (3D4h/3D5h) and PIC.

**Testing.** A palette benchmark (768 single OUTs to 3C9h per frame, 200 frames, then 200000 OUTs
to 80h): 4.2 s to 1.7 s (dynamic 0.8 s). A palette test reading back through 3C7h/3C9h after
every load passes. Quake timedemo: 412000 port exits to 237000. Windows 2000 text-mode setup:
133 million exits to the IDE data port at 8.5 us each, over 30 minutes, down to about 7 minutes.
Queuing 3C5h/3CFh: planar PutPixel 7.6 s to 4.0 s above start-up, FastDoom EGA 17.2 to 19.4 fps;
PLANTEST (map mask, set/reset, bit mask, write modes 0-3, XOR, latch copies, read mode 1, a
memory map switch to B8000 written right after, chain 4 off and on with writes right after) dumps
the same 219600 bytes on the normal, dynamic and KVM cores.

### 14. Instructions KVM cannot emulate on MMIO (`KVM_EmulateOne()`)

**What.** When KVM's instruction emulator cannot handle an MMIO access (x87 loads/stores, as
Glide's `FSTP dword [edi+4]` to the Voodoo registers), the core runs that one instruction on the
normal core and continues in KVM, instead of giving up.

**Why.** TRIP (Kosmic, The Party 97; DOS/4GW, Glide 2.43) on the emulated Voodoo stopped at once
with "KVM internal error, suberror 1".

**How.**

1. In `KVM_CoreInit()`, if `KVM_CAP_EXIT_ON_EMULATION_FAILURE` exists (kernel 5.17+; guard the
   use with `#ifdef`), enable it with `KVM_ENABLE_CAP` on the VM. Without it KVM exits to
   userspace only at CPL 0 and gives the guest a #UD at CPL 3; log a warning saying so.
2. On `KVM_EXIT_INTERNAL_ERROR` with `KVM_INTERNAL_ERROR_EMULATION`: set a flag and return
   `CBRET_NONE` from the loop. `CPU_Core_KVM_Run()` then calls `KVM_EmulateOne()` if the loop
   returned `CBRET_NONE`.
3. `KVM_EmulateOne()`: without the capability, read the vCPU events and if a #UD is injected or
   pending, clear it and write them back. `CPU_SyncState()`. Then run exactly one instruction:
   with TF set, `CPU_Core_Normal_Trap_Run()` (one instruction plus the single-step trap);
   otherwise set `CPU_Cycles = 1`, call `CPU_Core_Normal_Run()`, and restore `CPU_Cycles` to the
   old value minus one. The normal core may change `cpudecoder` (e.g. to its trap decoder after a
   POPF that sets TF); if it is the normal or normal-trap decoder afterwards, put back the
   previous one (KVM single-steps natively). Do the same restore when the normal core throws (a
   guest page fault, which the main loop handles) and rethrow.

**Testing.** TRIP renders its parts on KVM, about 27000 instructions emulated per second (TRIP
needs `enable msr=true` on the emulated cores, not on KVM; see findings). A real mode program in
V86 mode (EMM386 active, so CPL 3) doing x87 loads and stores on mode 12h memory (FSTPTEST): same
results as the normal core.

### 15. Mode 13h video memory as a dirty-logged shadow (step 21)

**What.** While all of A0000-AFFFF is the plain chained 256-colour handler, the KVM core maps a
64 KB shadow buffer there as RAM with dirty logging, so the guest writes it without exits.

**Why.** Mode 13h goes through `VGA_ChainedVGA_Handler` because chain 4 does not map linearly
onto VGA memory, so every write was an exit. Quake's timedemo ran at 45 fps against 173 on the
dynamic core, with 44 of 60 s in `KVM_RUN` and 100 million queued writes.

**How.**

1. `vga_memory.cpp` / `vga.h`:
   - `VGA_MemWriteCount`: incremented in `vga_vram_write_trigger_update()` and
     `vga_vram_write_trigger_update_planar_mem()`, i.e. on every write through the VGA memory
     handlers (BIOS text and pixel output, the emulated cores);
   - `struct VGA_ChainedShadowMap { bank, mask, memmask }`;
   - `VGA_ChainedShadowable(map)`: true when `vgapages.base` is `VGA_PAGE_A0` and all 16 pages
     from there have `&vgaph.cvga` as handler (`MEM_GetPageHandler()`); fills bank from
     `vga.svga.bank_read_full`, mask from `vgapages.mask`, memmask from `vga.mem.memmask`;
   - the mapping: guest byte A of the window is VGA memory byte
     `chain4remap((A & mask) + bank) & memmask`, so each aligned dword of the shadow is one
     aligned dword of VGA memory. `VGA_ChainedShadowLoad(map, shadow, page)` and
     `VGA_ChainedShadowStore(map, shadow, page)` copy one 4 KB page dword by dword in either
     direction (`VGA_ChainedVGA_Handler::chain4remap` needs to be reachable); the store sets
     `vga.draw.must_complete_frame`.
2. In the KVM core: a 64 KB page-aligned buffer (anonymous mmap, allocated on first use), an
   "active" flag, the map it was loaded with, and the `VGA_MemWriteCount` at the last load.
   - `KVM_PageAt()`: while active, pages A0h-AFh are `MEM_PAGE_RAM` with the shadow as host
     memory. `KVM_ApplyMapChanges()` sets the dirty-log flag on runs that point into the shadow
     (`KVM_MEM_LOG_DIRTY_PAGES`), which keeps them in slots of their own.
   - When the dirty range touches A0h-AFh, widen it to all 16 pages and call
     `KVM_ShadowUpdate()`: store what the guest wrote with the old mapping, deactivate, ask
     `VGA_ChainedShadowable()` again, and if yes activate with the new map and load all 16
     pages. So a mode set, bank switch or raster op change at A0000 stores first, with the bank
     and mask the shadow was loaded with.
   - `KVM_ShadowStore()`: for each dirty-logged slot, `KVM_GET_DIRTY_LOG` (one 64-bit bitmap is
     enough for 16 pages) and store each page whose bit is set. Called every time
     `CPU_Core_KVM_Run()` returns, so drawing, callbacks, save states and the other cores always
     see the guest's writes.
   - Before each `KVM_RUN`: if `VGA_MemWriteCount` changed since the last load, store then load
     the whole shadow (DOSBox-X wrote video memory through the handlers).
   - `KVM_Fail()` (switching to the normal core) stores the shadow and deactivates it.

**Testing.** Quake timedemo 45 fps to 203-225 fps (dynamic core 173). VGABENCH 0.66 s to 0.34 s;
the `cycles=30000` VGA test 1.5 s to 0.56 s. VGA13 (direct writes mixed with BIOS pixel writes
and reads) dumps byte-identical to the normal core. FastDoom mode 13h renderer: 1725 fps (normal
143, dynamic 217).

### 16. Planar and unchained VGA on an emulated core (step 26)

**What.** In planar (16-colour) and unchained (mode X/Y) modes, all of A0000-AFFFF stays MMIO:
every read exits (it loads the latches) and every write is an in-kernel exit, and a shadow cannot
represent four planes behind one address. When such exits take a large share of the guest's time,
`CPU_Core_KVM_Run()` runs the guest on an emulated core instead, and goes back to KVM when that
stops paying. New setting `[cpu] kvm vga core` = auto | dynamic | normal | off
(`Property::Changeable::WhenIdle`, default auto: the dynamic core, or the normal core in builds
without one; off: always KVM).

**Why.** FastDoom 0.9.1 `-timedemo demo1 -nosound`, `cycles=max`: EGA renderer 54 fps normal, 187
dynamic, 19.4 KVM; mode Y 103 normal, 339 dynamic, 4.5 KVM (264 million writes at about 4 us
each). PLANBNCH: 200000 PutPixels 1.43 s normal / 8.81 s KVM.

**How.**

1. `vga_memory.cpp`: add `VGA_MemReadCount`, incremented in `VGAMEM_USEC_read_delay()` (called by
   the VGA memory read handlers). Together with `VGA_MemWriteCount` it counts accesses through
   the memory handlers, i.e. those that would be MMIO under KVM; host memory mapped directly
   (bank map, LFB) is not counted.
2. `cpu.h`/`cpu.cpp`: `CPU_RDTSCHook` (function pointer returning bool and the TSC); `CPU_RDTSC()`
   returns the hook's value when it is set and returns true.
3. `KVM_VGASetup()` (called from `KVM_CoreInit()`, after `KVM_VGAEnd()`): read the setting, pick
   `CPU_Core_Dyn_X86_Run`/`_Trap_Run` (when `GetDynamicType() == 1`), or
   `CPU_Core_Dynrec_Run`/`_Trap_Run` (type 2), or the normal core and its trap decoder; remember
   which dynamic core it is; "off" leaves no core.
4. Measuring under KVM: add each `KVM_RUN`'s elapsed time to a window counter; add the estimated
   cost of each exit for A0000-BFFFF: an MMIO exit at max(MMIO exit estimate, 1000 ns), each
   queued write at max(burst or single write estimate, 300 ns) depending on whether 64 or more
   were queued in that run. `KVM_VGACheck()` after each `KVM_Run()` that returned `CBRET_NONE`:
   once the window reaches 20 ms, switch if the VGA cost is 40% or more of it; reset both
   counters either way.
5. `KVM_VGAStart()`: `CPU_SyncState()` (full); give up if TF is set (KVM single-steps natively).
   Read the guest TSC (MSR 10h) with `KVM_GET_MSRS` and store its offset from the host's
   `__rdtsc()`. Initialise the dynamic core's code cache (`CPU_Core_Dyn_X86_Cache_Init(true)` or
   the dynrec one). Swap the cycles (item 8) to the emulated core's last value, 200000 the first
   time. Set `CPU_RDTSCHook`. Record `PIC_FullIndex()` and both access counters.
6. `KVM_VGARun()` (what `CPU_Core_KVM_Run()` does while active): call the emulated core directly;
   `cpudecoder` stays `CPU_Core_KVM_Run`. Afterwards fix the decoder: if the emulated core set it
   to its trap decoder (POPF or IRET setting TF), set it back to the KVM core and end the stretch
   (KVM handles TF); if it set it to itself, set it back to the KVM core; leave anything else
   (`HLT_Decode`, which returns to the KVM core since `cpu.hlt.old_decoder` is the KVM core). Do
   the same fix before rethrowing a guest page fault exception. Then: if
   `VGA_ChainedShadowable()` (mode 13h), end the stretch. Every 50 emulated ms: price the
   period's handler writes at max(single write estimate, 300 ns) and reads at max(MMIO estimate,
   1000 ns); if that is under 25% of the period's emulated time (in ns), end the stretch;
   otherwise start a new period.
7. `KVM_VGAEnd()`: clear the hook; reset the dynamic core's code cache
   (`CPU_Core_Dyn_X86_Cache_Reset()` / `CPU_Core_Dynrec_Cache_Reset()`), for two reasons: the
   guest changes code under KVM without DOSBox-X seeing it, and the dynamic core's code pages have
   page handlers of their own (`MEM_SetPageHandler()`) that would be MMIO under KVM; the reset
   puts the old handlers back and the map change notifications rebuild the slots. Swap the cycles
   back; clear the KVM window counters. Also called from `KVM_CoreInit()` (CPU settings change)
   and accounted in the shutdown statistics.
8. Cycles (`KVM_VGASwapCycles()`), only with `CPU_CycleAutoAdjust`: KVM's `CPU_CycleMax` is
   nanoseconds, the emulated core's is instructions, so each keeps its own. Save the current one,
   scale `CPU_Cycles` and `CPU_CycleLeft` by new/old, set the new `CPU_CycleMax`, and reset the
   auto-adjust state in the main loop (`ticksDone`, `ticksScheduled`, `CPU_IODelayRemoved`).
   Fixed cycles stay as they are.
9. TSC: the hook returns host `__rdtsc()` plus the stored offset while the stretch is active and
   `cpudecoder` is the KVM core, so the guest's TSC goes on at the same (host) rate and never
   goes back across a switch.

**Testing.** FastDoom EGA 19.4 to 217 fps (dynamic 187), mode Y 4.5 to 330 fps (dynamic 339), mode
13h unchanged at about 1650 fps with no switches. HYBTEST (8 rounds alternating planar PutPixels
and CPU-only work, switching every round): the normal core's checksum, RDTSC never goes back and
runs at the same rate in both phases. Windows 3.1 with the 16-colour VGA driver (`/3` and `/s`)
passes, switching twice; Windows 98 passes and boots faster (72 s, four switches at its boot
screens); PLANTEST, VGA13, LFB, Voodoo and FSTPTEST dumps as the normal core; TRIP does not switch
(the Voodoo is not in A0000-BFFFF).

**Open.** The thresholds (40%, 25%, 20 ms, 50 ms) are first guesses tuned on FastDoom only; a
guest with short VGA bursts may switch often, and each switch back resets the dynamic core's code
cache.

### 17. Guest page tables changed by DOSBox-X: `PAGING_GuestPageTablesChanged()`

**What.** `PAGING_GuestPageTablesChanged()` in `src/cpu/paging.cpp` (declared in `paging.h` with
`PAGING_GuestPageTablesHook`): calls `PAGING_ClearTLB()` and then the hook if set. DOSBox-X code
that edits the guest's page tables itself calls it (the EMM386 section's `EMM_SetPTE()` paths
when mapping EMS pages). The KVM core installs a hook in `KVM_CoreInit()` that only sets a
`flush_tlb` flag.

**Why.** Once DOSBox-X's EMM386 runs V86 mode with paging, EMS mapping changes page table
entries behind the guest's back; the vCPU's TLB may still hold the old translations and nothing
the guest does will flush it.

**How.** In the run loop, after `KVM_EnterState()` and before the slot update, if the flag is set:
take the special registers (from `kvm_run->s.regs.sregs` with register sync, which is either the
vCPU's or what goes into it next; else `KVM_GET_SREGS`), `KVM_SET_SREGS` once with CR3 XOR 1000h
and once as they are (a CR3 change makes KVM reset the vCPU's MMU context and flush the guest
TLB), then clear `KVM_SYNC_X86_SREGS` from `kvm_dirty_regs`, since the ioctl already set them.

**Testing.** With EMM386 active on the normal, dynamic and KVM cores: EMS mapping and VCPI DE06,
DOS/4GW, DOS/32A, PMODE/W and CauseWay through VCPI, Quake (CWSDPMI), Windows 3.1 enhanced and
standard mode.

**Depends on.** The EMM386 paging section.

### 18. Menu: CPU → Core → KVM core

**What.** A "KVM core (experimental)" item (mapper event "kvm", `mapper_kvm` in
`def_menu_cpu_core[]` in `src/gui/menu.cpp`, under `C_KVM`) that sets `core=kvm` through
`HandleInputline`, like the other core items, with a description of the requirements.

**How.** In `menu_update_core()`: call `CALLBACK_ChooseInt3Prefix()`, check the item while
`cpudecoder == CPU_Core_KVM_Run`, and enable it only with `CALLBACK_Int3Prefix`, a 386 or later
and a CPU type without `_prefetch`. The prefix is written once at startup, so the item explains
that it needs `callback int3 prefix=true`, or auto with `core=kvm` when DOSBox-X starts. The menu
was updated before `core=kvm` was decided at the end of `Change_Config()`, so it showed the normal
core while KVM ran: call `menu_update_core()` again after the decision, and from `KVM_Fail()`.

### 19. Statistics at exit

`KVM_CoreShutdown()` logs, if the core ran: `KVM_RUN` calls; port I/O, MMIO and queued write
counts; callbacks, guest INT3s, HLTs, interrupt windows, slices; instructions emulated
(subsection 14); stretches and emulated ms on the planar VGA core; state puts and fetches; the six
exit cost estimates. Then the eight busiest ports by direction from a per-port, per-direction
exit counter (`in 040h 12345, out 3c9h ...`). Then unmap `kvm_run` and close the vCPU, VM and
`/dev/kvm`. These lines were the main tool for every performance step above.

### 20. Findings and limitations to document

- `segment limits=false` stores ~0 limits; KVM always enforces real limits, so the option has no
  effect under KVM.
- CPU types without CPUID (386, 486old): CPUID still runs (zeros) and the EFLAGS ID bit toggles;
  `fpu=false` cannot remove the FPU. Limits of running on real hardware.
- RDTSC is the host TSC with KVM's offset, at the host CPU's rate, not derived from emulated
  cycles. Windows 95's performance counter and Quake agree with the emulated cores.
- KVM answers RDMSR/WRMSR itself, so `enable msr=false` does not take them away.
- The first MB layout (`firstmb`, EMS without V86) follows the paging state DOSBox-X last saw;
  with `KVM_CAP_SYNC_REGS` the core could check CR0 after every exit (open).

**Open issues** (from `KVM-TODO.md`):

- Callback round trips cost about 10 us (emulated core about 2.5 us including the callback),
  mostly KVM's exit and entry; `CALLBACK_Idle()` is a round trip per call in DOSBox-X's own wait
  loops.
- Port I/O exits cost 5-7 us in the kernel, so a guest polling a port runs at a fraction of the
  emulated cores' speed. Short slices at `cycles=max` spend part of their time re-arming the
  timer.
- Exit-heavy guests at low fixed cycles can still be slower than an emulated core.
- Voodoo register and LFB writes are not queued (the PCI BAR can move, so a coalesced zone would
  have to follow `VOODOO_PCI_SetLFB()`).
- A port handler that reads guest linear memory and faults while its I/O exit is pending would
  raise the #PF after the I/O instruction instead of at it; none is known to.
- A KVM internal error at Windows 95 shutdown, seen once in four runs: suberror 3 (exit during
  event delivery) with a nested page fault at C000ABF0, in real mode at 0661:0602 just before
  Windows reloads the IDT; the IDTR still had the protected mode base. The core falls back to the
  normal core and the shutdown finishes. Not understood yet.
- NMI is not passed on.
- A run-time CPU type change needs a new vCPU on newer kernels (subsection 5).
- Windows 95 with `memsize=16` did not run its test program in 400 s under KVM (it did with 32 MB
  and on the dynamic core); probably a Windows dialog rather than the core, not investigated.
