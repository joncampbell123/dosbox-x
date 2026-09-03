from __future__ import annotations

import base64
import binascii
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import time
from collections.abc import Mapping
from typing import Any, Protocol

from .errors import AgentConnectionError, AgentProtocolError, map_rpc_error
from .models import (
    AgentConfig,
    Breakpoint,
    DiagnosticCommandResult,
    MemoryAddress,
    MemoryRead,
    MemoryWrite,
    Operation,
    OutputPage,
    OutputRecord,
    RegisterSnapshot,
    Session,
    StopReason,
    TraceEvent,
    TracePage,
    WaitResult,
)


class RpcTransport(Protocol):
    def request(self, payload: str, timeout_ms: int, max_message_bytes: int) -> str:
        ...

    def close(self) -> None:
        ...


class NamedPipeTransport:
    """UTF-8 JSON-lines transport over the Windows local named-pipe API."""

    _GENERIC_READ = 0x80000000
    _GENERIC_WRITE = 0x40000000
    _OPEN_EXISTING = 3
    _ERROR_FILE_NOT_FOUND = 2
    _ERROR_PIPE_BUSY = 231
    _ERROR_BROKEN_PIPE = 109
    _INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

    def __init__(self, endpoint: str) -> None:
        self._endpoint = endpoint
        self._handle: int | None = None
        self._fd: int | None = None

    def _connect(self, timeout_ms: int) -> None:
        if self._fd is not None:
            return
        if os.name != "nt":
            raise AgentConnectionError("named_pipe transport requires Windows")

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        wait_named_pipe = kernel32.WaitNamedPipeW
        wait_named_pipe.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
        wait_named_pipe.restype = wintypes.BOOL
        create_file = kernel32.CreateFileW
        create_file.argtypes = [
            wintypes.LPCWSTR,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.DWORD,
            wintypes.HANDLE,
        ]
        create_file.restype = wintypes.HANDLE

        deadline = time.monotonic() + timeout_ms / 1000
        while True:
            handle = create_file(
                self._endpoint,
                self._GENERIC_READ | self._GENERIC_WRITE,
                0,
                None,
                self._OPEN_EXISTING,
                0,
                None,
            )
            if handle != self._INVALID_HANDLE_VALUE:
                import msvcrt

                self._handle = int(handle)
                self._fd = msvcrt.open_osfhandle(self._handle, os.O_RDWR | os.O_BINARY)
                return

            error = ctypes.get_last_error()
            remaining = deadline - time.monotonic()
            if remaining <= 0 or error not in (self._ERROR_FILE_NOT_FOUND, self._ERROR_PIPE_BUSY):
                raise AgentConnectionError(
                    f"unable to connect to named pipe {self._endpoint}: Win32 error {error}"
                )
            wait_named_pipe(self._endpoint, max(1, min(int(remaining * 1000), 100)))
            time.sleep(0.01)

    def _available_bytes(self) -> int:
        if self._handle is None:
            raise AgentConnectionError("named pipe is not connected")
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        peek_named_pipe = kernel32.PeekNamedPipe
        peek_named_pipe.argtypes = [
            wintypes.HANDLE,
            wintypes.LPVOID,
            wintypes.DWORD,
            wintypes.LPVOID,
            ctypes.POINTER(wintypes.DWORD),
            wintypes.LPVOID,
        ]
        peek_named_pipe.restype = wintypes.BOOL
        available = wintypes.DWORD(0)
        if not peek_named_pipe(self._handle, None, 0, None, ctypes.byref(available), None):
            error = ctypes.get_last_error()
            if error == self._ERROR_BROKEN_PIPE:
                raise AgentConnectionError("named pipe was closed by the server")
            raise AgentConnectionError(f"unable to inspect named pipe: Win32 error {error}")
        return int(available.value)

    def request(self, payload: str, timeout_ms: int, max_message_bytes: int) -> str:
        self._connect(timeout_ms)
        if self._fd is None:
            raise AgentConnectionError("named pipe did not produce a file descriptor")
        encoded = (payload + "\n").encode("utf-8")
        if len(encoded) > max_message_bytes:
            raise AgentProtocolError("request exceeds configured max_message_bytes")

        sent = 0
        while sent < len(encoded):
            written = os.write(self._fd, encoded[sent:])
            if written <= 0:
                self.close()
                raise AgentConnectionError("named pipe write did not make progress")
            sent += written

        deadline = time.monotonic() + timeout_ms / 1000
        response = bytearray()
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AgentConnectionError("timed out waiting for named-pipe response")
            available = self._available_bytes()
            if available == 0:
                time.sleep(min(0.01, remaining))
                continue
            chunk = os.read(self._fd, min(available, max_message_bytes + 1 - len(response)))
            if not chunk:
                self.close()
                raise AgentConnectionError("named pipe closed before a response was received")
            response.extend(chunk)
            newline = response.find(b"\n")
            if newline >= 0:
                return bytes(response[:newline]).decode("utf-8")
            if len(response) > max_message_bytes:
                raise AgentProtocolError("response exceeds configured max_message_bytes")

    def close(self) -> None:
        if self._fd is not None:
            os.close(self._fd)
        self._fd = None
        self._handle = None


class AgentClient:
    def __init__(self, config: AgentConfig, transport: RpcTransport | None = None) -> None:
        if config.transport != "named_pipe":
            raise ValueError(f"unsupported configured transport: {config.transport}")
        self.config = config
        self._transport: RpcTransport = transport or NamedPipeTransport(config.endpoint)
        self._next_request_id = 1

    @classmethod
    def from_config(cls, path: str | Path) -> "AgentClient":
        return cls(load_config(path))

    def __enter__(self) -> "AgentClient":
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> None:
        self.close()

    def close(self) -> None:
        self._transport.close()

    def call(self, method: str, params: Mapping[str, Any] | None = None, request_id: str | None = None) -> dict[str, Any]:
        if not method:
            raise ValueError("method must not be empty")
        identifier = request_id or self._new_request_id()
        request = {"jsonrpc": "2.0", "id": identifier, "method": method}
        if params is not None:
            request["params"] = dict(params)
        payload = json.dumps(request, separators=(",", ":"), ensure_ascii=False)
        # The server may consume its complete configured request timeout before
        # it writes the JSON-RPC response. Reserve local IPC time so a valid
        # terminal response is not mistaken for a transport failure at that edge.
        request_transport = self._transport
        transient_transport = isinstance(self._transport, NamedPipeTransport)
        if transient_transport:
            request_transport = NamedPipeTransport(self.config.endpoint)
        try:
            response_text = request_transport.request(
                payload,
                self.config.request_timeout_ms + 1000,
                self.config.max_message_bytes,
            )
        finally:
            if transient_transport:
                request_transport.close()
        try:
            response = json.loads(response_text)
        except json.JSONDecodeError as error:
            raise AgentProtocolError("server returned invalid JSON") from error
        if not isinstance(response, Mapping) or response.get("jsonrpc") != "2.0":
            raise AgentProtocolError("server returned an invalid JSON-RPC envelope")
        if response.get("id") != identifier:
            raise AgentProtocolError("server response id does not match the request")
        if "error" in response:
            error = response["error"]
            if not isinstance(error, Mapping):
                raise AgentProtocolError("server error payload is not an object")
            raise map_rpc_error(error)
        result = response.get("result")
        if not isinstance(result, Mapping):
            raise AgentProtocolError("server success payload is not an object")
        return dict(result)

    def capabilities(self, request_id: str | None = None) -> Mapping[str, Any]:
        return self.call("agent.capabilities", request_id=request_id)

    def start(self, command: str, arguments: tuple[str, ...] | list[str] = (), *, mount_path: str | Path | None = None,
              request_id: str | None = None) -> Session:
        if not command:
            raise ValueError("command must not be empty")
        target_arguments = list(arguments)
        if not all(isinstance(argument, str) for argument in target_arguments):
            raise ValueError("arguments must contain only strings")
        workdir = Path(mount_path) if mount_path is not None else self.config.dosbox_workdir
        result = self.call("session.start", {
            "target": {"command": command, "arguments": target_arguments},
            "mounts": [{"drive": "C", "host_path": str(workdir)}],
            "break_at": "entry",
        }, request_id)
        return _session(result, stop_key="stop_reason")

    def status(self, session_id: str, request_id: str | None = None) -> Session:
        return _session(self.call("session.status", {"session_id": session_id}, request_id), stop_key="last_stop")

    def stop(self, session_id: str, graceful_timeout_ms: int = 0, request_id: str | None = None) -> Operation:
        result = self.call("session.stop", {"session_id": session_id, "graceful_timeout_ms": graceful_timeout_ms}, request_id)
        return _operation(result)

    def continue_(self, session_id: str, request_id: str | None = None) -> Operation:
        return _operation(self.call("execution.continue", {"session_id": session_id}, request_id))

    def pause(self, session_id: str, request_id: str | None = None) -> Operation:
        return _operation(self.call("execution.pause", {"session_id": session_id}, request_id))

    def wait(self, session_id: str, operation_id: str, timeout_ms: int, request_id: str | None = None) -> WaitResult:
        result = self.call("execution.wait", {
            "session_id": session_id,
            "operation_id": operation_id,
            "timeout_ms": timeout_ms,
        }, request_id)
        running = result.get("running", False)
        if not isinstance(running, bool):
            raise AgentProtocolError("execution.wait response has an invalid running field")
        if running:
            return WaitResult(Session(_string(result, "session_id"), "running", _integer(result, "state_revision")), True)
        return WaitResult(_session(result, stop_key="stop_reason"), False)

    def step(self, session_id: str, mode: str = "into", request_id: str | None = None) -> tuple[Session, RegisterSnapshot]:
        if mode not in ("into", "over"):
            raise ValueError("mode must be into or over")
        result = self.call("execution.step", {"session_id": session_id, "mode": mode}, request_id)
        return _session(result, stop_key="stop_reason", default_state="stopped"), _registers(_object(result, "registers"))

    def get_registers(self, session_id: str, request_id: str | None = None) -> RegisterSnapshot:
        return _registers(self.call("state.get_registers", {"session_id": session_id}, request_id))

    def read_memory(self, session_id: str, address: MemoryAddress, length: int, request_id: str | None = None) -> MemoryRead:
        if length <= 0:
            raise ValueError("length must be positive")
        result = self.call("memory.read", {"session_id": session_id, "address": address.to_rpc(), "length": length}, request_id)
        try:
            data = base64.b64decode(_string(result, "data_base64"), validate=True)
        except (ValueError, binascii.Error) as error:
            raise AgentProtocolError("memory.read returned invalid base64") from error
        byte_count = _integer(result, "byte_count")
        if len(data) != byte_count:
            raise AgentProtocolError("memory.read byte_count does not match data_base64")
        return MemoryRead(MemoryAddress.from_rpc(_object(result, "address")), data, _string(result, "sha256"), _integer(result, "state_revision"))

    def write_memory(self, session_id: str, address: MemoryAddress, data: bytes, *, expected_sha256: str | None = None,
                     request_id: str | None = None) -> MemoryWrite:
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("data must be bytes-like")
        data = bytes(data)
        if not data:
            raise ValueError("data must not be empty")
        params: dict[str, Any] = {
            "session_id": session_id,
            "address": address.to_rpc(),
            "data_base64": base64.b64encode(data).decode("ascii"),
        }
        if expected_sha256 is not None:
            _validate_sha256(expected_sha256)
            params["expected_sha256"] = expected_sha256.lower()
        result = self.call("memory.write", params, request_id)
        return MemoryWrite(
            MemoryAddress.from_rpc(_object(result, "address")),
            _integer(result, "byte_count"),
            _string(result, "before_sha256"),
            _string(result, "after_sha256"),
            _integer(result, "state_revision"),
        )

    def create_execution_breakpoint(self, session_id: str, segment: str | int, offset: str | int, *, once: bool = False,
                                    request_id: str | None = None) -> Breakpoint:
        return self.create_breakpoint(session_id, "execution", MemoryAddress.segmented(segment, offset), once=once,
                                      request_id=request_id)

    def create_breakpoint(self, session_id: str, kind: str, address: MemoryAddress, *, once: bool = False,
                          request_id: str | None = None) -> Breakpoint:
        if kind not in ("execution", "memory_change"):
            raise ValueError("kind must be execution or memory_change")
        result = self.call("breakpoints.create", {
            "session_id": session_id,
            "kind": kind,
            "address": address.to_rpc(),
            "once": once,
        }, request_id)
        return Breakpoint(_string(result, "breakpoint_id"), _string(result, "kind"),
                          MemoryAddress.from_rpc(_object(result, "address")), bool(result.get("once")), True)

    def list_breakpoints(self, session_id: str, request_id: str | None = None) -> tuple[Breakpoint, ...]:
        result = self.call("breakpoints.list", {"session_id": session_id}, request_id)
        values = result.get("breakpoints")
        if not isinstance(values, list):
            raise AgentProtocolError("breakpoints.list response is missing breakpoints")
        parsed: list[Breakpoint] = []
        for value in values:
            entry = _object_value(value, "breakpoint")
            parsed.append(Breakpoint(
                _string(entry, "breakpoint_id"), _string(entry, "kind"), MemoryAddress.from_rpc(_object(entry, "address")),
                bool(entry.get("once")), bool(entry.get("enabled")),
            ))
        return tuple(parsed)

    def delete_breakpoint(self, session_id: str, breakpoint_id: str, request_id: str | None = None) -> int:
        result = self.call("breakpoints.delete", {"session_id": session_id, "breakpoint_id": breakpoint_id}, request_id)
        if result.get("breakpoint_id") != breakpoint_id or result.get("deleted") is not True:
            raise AgentProtocolError("breakpoints.delete response did not confirm the requested breakpoint")
        return _integer(result, "state_revision")

    def read_output(self, session_id: str, cursor: str | None, limit: int, request_id: str | None = None) -> OutputPage:
        if limit <= 0:
            raise ValueError("limit must be positive")
        result = self.call("debug.output.read", {"session_id": session_id, "cursor": cursor, "limit": limit}, request_id)
        values = result.get("output")
        if not isinstance(values, list):
            raise AgentProtocolError("debug.output.read response is missing output")
        records = tuple(OutputRecord(
            _integer(_object_value(value, "output record"), "sequence"),
            _integer(_object_value(value, "output record"), "timestamp"),
            _string(_object_value(value, "output record"), "level"),
            _string(_object_value(value, "output record"), "source"),
            _string(_object_value(value, "output record"), "message"),
        ) for value in values)
        return OutputPage(records, _optional_string(result.get("next_cursor"), "next_cursor"))

    def execute_command(self, session_id: str, command: str, request_id: str | None = None) -> DiagnosticCommandResult:
        result = self.call("debugger.execute_command", {"session_id": session_id, "command": command}, request_id)
        registers = result.get("parsed_registers")
        if registers is not None and not isinstance(registers, Mapping):
            raise AgentProtocolError("debugger.execute_command returned invalid parsed_registers")
        return DiagnosticCommandResult(
            result.get("accepted") is True,
            _string(result, "raw_output"),
            _string(result, "parse_status"),
            dict(registers) if isinstance(registers, Mapping) else None,
            _optional_string(result.get("unparsed_output"), "unparsed_output"),
        )

    def start_trace(self, session_id: str, detail: str, instruction_count: int, request_id: str | None = None) -> bool:
        result = self.call("trace.start", {"session_id": session_id, "detail": detail, "instruction_count": instruction_count}, request_id)
        return result.get("active") is True

    def read_trace(self, session_id: str, cursor: str | None, limit: int, request_id: str | None = None) -> TracePage:
        result = self.call("trace.read", {"session_id": session_id, "cursor": cursor, "limit": limit}, request_id)
        values = result.get("events")
        if not isinstance(values, list) or not isinstance(result.get("active"), bool):
            raise AgentProtocolError("trace.read returned an invalid page")
        events: list[TraceEvent] = []
        for value in values:
            entry = _object_value(value, "trace event")
            changes = _object(entry, "register_changes")
            events.append(TraceEvent(
                _integer(entry, "sequence"), MemoryAddress.from_rpc(_object(entry, "address")),
                _string(entry, "instruction"), {str(name): _string(changes, str(name)) for name in changes},
            ))
        return TracePage(result["active"], tuple(events), _optional_string(result.get("next_cursor"), "next_cursor"))

    def stop_trace(self, session_id: str, request_id: str | None = None) -> int:
        return _integer(self.call("trace.stop", {"session_id": session_id}, request_id), "event_count")

    def _new_request_id(self) -> str:
        request_id = f"client-{self._next_request_id}"
        self._next_request_id += 1
        return request_id


def load_config(path: str | Path) -> AgentConfig:
    config_path = Path(path).expanduser().resolve()
    if not config_path.is_file():
        raise ValueError(f"agent config file does not exist: {config_path}")
    values: dict[str, str] = {}
    for line_number, line in enumerate(config_path.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            raise ValueError(f"invalid dotenv line {line_number} in {config_path}")
        key, value = stripped.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key or key in values:
            raise ValueError(f"duplicate or empty dotenv key on line {line_number} in {config_path}")
        values[key] = value
    required = {
        "transport", "endpoint", "dosbox_executable", "dosbox_workdir", "request_timeout_ms",
        "max_message_bytes", "max_memory_read_bytes", "max_trace_events",
    }
    optional = {"profile"}
    missing = required.difference(values)
    unexpected = set(values).difference(required | optional)
    if missing or unexpected:
        details = []
        if missing:
            details.append("missing=" + ",".join(sorted(missing)))
        if unexpected:
            details.append("unexpected=" + ",".join(sorted(unexpected)))
        raise ValueError("invalid agent config: " + " ".join(details))
    transport = values["transport"]
    if transport != "named_pipe":
        raise ValueError("only transport=named_pipe is supported by the Python v1 client")
    profile = values.get("profile", "production")
    if profile not in {"production", "test"}:
        raise ValueError("profile must be production or test")
    if profile == "production":
        if not Path(values["dosbox_executable"]).is_absolute() or not Path(values["dosbox_workdir"]).is_absolute():
            raise ValueError("production agent config requires absolute dosbox_executable and dosbox_workdir paths")
    def positive(name: str) -> int:
        try:
            value = int(values[name], 10)
        except ValueError as error:
            raise ValueError(f"{name} must be a decimal integer") from error
        if value <= 0:
            raise ValueError(f"{name} must be positive")
        return value
    return AgentConfig(
        path=config_path,
        transport=transport,
        endpoint=values["endpoint"],
        dosbox_executable=_resolve_config_path(config_path.parent, values["dosbox_executable"]),
        dosbox_workdir=_resolve_config_path(config_path.parent, values["dosbox_workdir"]),
        request_timeout_ms=positive("request_timeout_ms"),
        max_message_bytes=positive("max_message_bytes"),
        max_memory_read_bytes=positive("max_memory_read_bytes"),
        max_trace_events=positive("max_trace_events"),
        test_profile=profile == "test",
    )


def _resolve_config_path(base: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else (base / path).resolve()


def _session(result: Mapping[str, Any], *, stop_key: str, default_state: str | None = None) -> Session:
    stop = result.get(stop_key)
    state = result.get("state", default_state)
    if not isinstance(state, str):
        raise AgentProtocolError("response field state must be a string")
    return Session(
        _string(result, "session_id"), state, _integer(result, "state_revision"),
        StopReason.from_rpc(_object_value(stop, stop_key)) if stop is not None else None,
    )


def _operation(result: Mapping[str, Any]) -> Operation:
    return Operation(_string(result, "operation_id"), _string(result, "session_id"), _integer(result, "state_revision"))


def _registers(result: Mapping[str, Any]) -> RegisterSnapshot:
    general = _object(result, "general")
    segments = _object(result, "segments")
    return RegisterSnapshot(
        {str(name): _string(general, str(name)) for name in general},
        {str(name): _string(segments, str(name)) for name in segments},
        _string(result, "instruction_pointer"), _string(result, "flags"), _string(result, "cpu_mode"),
        _integer(result, "state_revision"),
    )


def _validate_sha256(value: str) -> None:
    if len(value) != 64:
        raise ValueError("expected_sha256 must contain exactly 64 hexadecimal characters")
    try:
        int(value, 16)
    except ValueError as error:
        raise ValueError("expected_sha256 must contain only hexadecimal characters") from error


def _object(result: Mapping[str, Any], name: str) -> Mapping[str, Any]:
    return _object_value(result.get(name), name)


def _object_value(value: Any, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise AgentProtocolError(f"response field {name} must be an object")
    return value


def _string(result: Mapping[str, Any], name: str) -> str:
    value = result.get(name)
    if not isinstance(value, str):
        raise AgentProtocolError(f"response field {name} must be a string")
    return value


def _integer(result: Mapping[str, Any], name: str) -> int:
    value = result.get(name)
    if not isinstance(value, int) or isinstance(value, bool):
        raise AgentProtocolError(f"response field {name} must be an integer")
    return value


def _optional_string(value: Any, name: str) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise AgentProtocolError(f"response field {name} must be a string or null")
    return value
