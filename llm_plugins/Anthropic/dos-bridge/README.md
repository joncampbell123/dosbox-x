# DOSBox-X Agent86 bridge for Claude Code

This Claude Code plugin includes the native C++ `agent86-mcp` executable, a
`/dos` command, and DOS and TSR development skills. No Python installation is
required.

## Build and install

Build DOSBox-X, then use the staged package under
`<output>/llm_plugins/Anthropic/dos-bridge/`. From that directory:

```shell
claude plugin validate . --strict
claude plugin validate .. --strict
claude plugin marketplace add ..
claude plugin install dos-bridge@dosbox-x-plugins
```

## Configure and activate

Set the same long random secret in DOSBox-X and the environment inherited by
Claude Code:

```ini
bridge_port = 8090
bridge_token = replace-with-a-long-random-secret
automount_c = false
```

`DOS_BRIDGE_TOKEN` is required. `DOS_BRIDGE_PORT` optionally overrides port
8090. Run `CTTY BRIDGE` at the DOS prompt to start a session. Use
`release_dos_console` or `CTTY CON` to restore the local console.

The server exposes command/file tools, graphical PNG capture and mouse input,
direct floppy/CD image selection and blank-image creation, drive management,
text-screen access, and `release_dos_console`. Claude namespaces these tools
when it loads them from a plugin.

Mounted DOS drives can modify real host files. Keep automatic host-system
mounting disabled, mount only intended project directories, and do not retry a
timed-out mutation until its DOS state is inspected. Tool outcomes are logged
to `~/.dos-bridge/session.log`; command text is redacted by default.
`automount_c = false` does not block an explicit authenticated `MOUNT` or
media-tool request.
