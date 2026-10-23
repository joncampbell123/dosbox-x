# DOSBox-X Agent86 bridge for Gemini CLI

This Gemini CLI extension includes the native C++ `agent86-mcp` executable and
the DOS and TSR development guidance. It requires no Python runtime.

## Build and install

Build DOSBox-X and install the staged extension from
`<output>/llm_plugins/Google/dos-bridge/`:

```shell
gemini extensions install <output>/llm_plugins/Google/dos-bridge
```

The `gemini-extension.json` manifest uses Gemini's `${extensionPath}` and path
separator substitutions to find the bundled native executable after the
extension is copied.

## Configure and activate

Set one long random secret in both DOSBox-X and the environment inherited by
Gemini CLI:

```ini
bridge_port = 8090
bridge_token = replace-with-a-long-random-secret
automount_c = false
```

`DOS_BRIDGE_TOKEN` is required. `DOS_BRIDGE_PORT` optionally overrides port
8090. Run `CTTY BRIDGE` at the DOS prompt. The MCP tools include DOS command
and file access, graphical PNG capture and mouse input, direct disk-image and
drive management, text-screen access, and local-console release.

Mounted DOS drives are real host directories, not a sandbox. Keep automatic
host-system mounting disabled and inspect DOS state before retrying a timed-out
mutation. Tool outcomes are logged to `~/.dos-bridge/session.log`; command text
is redacted by default. `automount_c = false` does not block an explicit
authenticated `MOUNT` or media-tool request.
