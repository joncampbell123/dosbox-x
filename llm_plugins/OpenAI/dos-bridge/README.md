# DOSBox-X Agent86 bridge for Codex

This Codex plugin packages the native C++ `agent86-mcp` server with the DOS
development and TSR-development skills. It does not require Python, `pip`, a
virtual environment, or a separately installed launcher.

## Build and install

Build the DOSBox-X solution or Automake project. The build stages this complete
plugin under `<output>/llm_plugins/OpenAI/dos-bridge/`. The same output contains
`dosbox-x`, `agent86-mcp`, and the Anthropic and Google packages. Install from
the staged OpenAI directory:

```shell
codex plugin marketplace add .
codex plugin add dos-bridge@dosbox-x-openai
```

Restart Codex after installation.

## Configure and activate

Choose one long random secret. Put it in the DOSBox-X `[dos]` section and in
the environment inherited by Codex:

```ini
bridge_port = 8090
bridge_token = replace-with-a-long-random-secret
automount_c = false
```

```powershell
$env:DOS_BRIDGE_TOKEN = "replace-with-a-long-random-secret"
```

`DOS_BRIDGE_PORT` optionally overrides the MCP client's default port of 8090.
At the DOS prompt, run `CTTY BRIDGE`. Use `release_dos_console` or `CTTY CON`
to return control to the local emulator window.

## Tools and safety

- `run_dos_command` executes one bounded DOS command.
- `write_dos_file` transactionally writes bounded CP437 text to an absolute
  DOS 8.3 path.
- `release_dos_console` closes the authenticated bridge session.
- `capture_graphics_screen`, `move_mouse`, and `click_mouse` inspect and
  interact with graphical DOS applications.
- `set_floppy_image`, `set_cd_image`, and `create_disk_image` manage image
  files directly without opening an interactive dialog.
- The remaining tools expose drive swap, mount/unmount, rescan, status,
  text-screen capture/export, and bounded keyboard input.

The listener is loopback-only, but mounted host directories are real host
directories, not a sandbox. Keep `automount_c = false`, mount only deliberate
project paths, serialize operations, and inspect state before retrying an
operation that timed out. That setting blocks automatic host-C mounting, not
an explicit `MOUNT` command authorized at bridge activation.

Tool outcomes are appended to `~/.dos-bridge/session.log`. Command text is
redacted unless `DOS_BRIDGE_LOG_COMMANDS=true` is deliberately set.
