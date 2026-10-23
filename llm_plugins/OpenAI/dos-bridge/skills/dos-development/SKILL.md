---
name: dos-development
description: Instructs the agent on how to write code, discover compilers, and execute commands natively inside MS-DOS via the DOSBox-X TCP bridge. Use this skill when asked to interact with DOS, compile C/C++ or Basic code in DOS, or explore the DOS environment.
---

# DOSBox-X Development Bridge

You are equipped with an MCP integration to a DOSBox-X emulator running DOS. It exposes three tools through the `dos-bridge` MCP server:

- `run_dos_command(command: str)`
- `write_dos_file(filename: str, content: str)`
- `release_dos_console()`
- `swap_floppy_drive()` and `swap_cd_drive()`
- `change_floppy_image()` and `change_cd_image()` (interactive DOSBox-X picker)
- `create_blank_disk_image()` (interactive DOSBox-X dialog)
- `list_mounted_drives()` and `get_ide_status()`
- `rescan_all_drives()`
- `export_text_screen()` (interactive TXT/Markdown save dialog)
- `get_text_screen()`
- `capture_graphics_screen()` returns the current DOS framebuffer as PNG.
- `move_mouse(x, y)` and `click_mouse(button)` interact with graphical DOS applications.
- `set_floppy_image(drive, path)` and `set_cd_image(drive, path)` select existing host images without dialogs.
- `create_disk_image(path, format)` creates a new blank host `.img` without a dialog.
- `mount_host_folder(drive, path, kind)` and `unmount_drive(drive)`
- `type_text(text)` queues keyboard input for the running DOS application.

A command that fails in DOS (nonzero exit code, or COMMAND.COM reporting an
unrecognized command) raises a tool error carrying the full console output
and the exit code. Read that output before retrying, and never retry a
destructive command automatically after a timeout.

The host may qualify these names. Codex exposes tools from the bundled
`dos-bridge` MCP server in its available tool list; use the discovered tool
name instead of inventing a prefix.

## Activation and configuration

- The user must run `CTTY BRIDGE` at the DOS prompt before an MCP client can connect. Do not attempt to activate it through TCP.
- The listener binds to loopback only. Its DOSBox-X `[dos]` `bridge_port` setting defaults to `8090`; `bridge_port = 0` disables it.
- The MCP process uses `DOS_BRIDGE_PORT`, also defaulting to `8090`. Both port settings must match.
- `release_dos_console()` or `CTTY CON` restores the normal DOS console and closes the bridge listener.
- If the MCP client disconnects, DOSBox-X automatically restores `CON` and lifts bridge-only restrictions. A DOSBox-X reset also clears the bridge state.

## Workflow for DOS Tasks

Treat the DOS guest as single-tasking. Only one foreground command or program
executes at a time, and the MCP server serializes requests. Do not attempt
parallel or background DOS commands. TSR utilities may interrupt a foreground
program, but they are not general-purpose multitasking. Use the separate
`dos-tsr-development` skill only when resident interrupt-driven behavior is
actually requested.

1. **Environment Discovery:**
   Do NOT assume where the compiler (Borland C++, Turbo C, QBasic, MASM) is located, or even what drives are mounted.
   When asked to compile or explore, start by exploring the environment:
   - Run `mount` once to discover the drives already exposed to DOS. Do not probe absent host drives.
   - Check the `PATH` variable using `run_dos_command("set")`.
   - Look for standard installation directories (e.g., `TC\BIN`, `BORLANDC`, `BC5`).

2. **Writing Code:**
   You MUST write files *inside* the DOS environment to remain authentic unless requested otherwise.
   - Use `write_dos_file(filename, content)` to stream code via `copy con` directly to DOS.
   - Use an absolute path on any drive already mounted by the user and keep every component in 8.3 format (for example, `D:\\SRC\\PROGRAM.C`).

3. **Compiling:**
   - Compiling inside an emulator takes time. MCP command calls have a fixed 60-second timeout.
   - Use the compiler discovered from `PATH` or the mounted development drive; do not assume it is on C:.
   - Look for a `BUILD.BAT` in the directory first before constructing your own compile commands.

4. **Testing and Execution:**
   - Execute the compiled `.EXE` or `.COM` file using `run_dos_command("PROGRAM.EXE")`.
   - The tool returns bounded CP437 output over the loopback TCP bridge.
   - For graphical applications, use `capture_graphics_screen`, then normalized
     `move_mouse` coordinates (0 is left/top; 65535 is right/bottom) and
     `click_mouse`. Capture again after an interaction to verify its effect.

5. **Connection Discipline:**
   - Use only the MCP tools; do not open parallel or ad-hoc TCP connections to the configured bridge port.
   - The MCP server serializes calls over one persistent connection. Wait for each tool call to finish before issuing another.
   - A timeout or transport error is an unknown completion state. Inspect DOS state before retrying a state-changing command.
   - Call `release_dos_console()` when the DOS session is finished so the user regains the local console immediately.

Remember: You are operating natively inside an MS-DOS prompt. Standard DOS commands (`dir`, `del`, `type`, `copy`, `ren`) apply. Avoid Unix tools (`ls`, `rm`, `cat`).

## Critical security warning
**DO NOT ASSUME THIS IS A FULLY ISOLATED SANDBOX.**
DOSBox-X allows users to explicitly mount folders from the host operating system into the DOS environment.
Any files you modify, delete, or overwrite while working in these mounted DOS drives will permanently alter the files on the user's actual host PC!
**ALWAYS** double-check what drive you are in (e.g., using `dir` and paying attention to folder contents) and proceed with extreme caution before executing destructive commands.

Both `write_dos_file` and `run_dos_command` can modify any drive already mounted in DOSBox-X. Authenticated tools can explicitly mount host paths and manage disk images. Keep `automount_c = false` to block automatic host-C mounting, never probe unconfirmed drives, and never automatically retry a destructive command after a timeout.
