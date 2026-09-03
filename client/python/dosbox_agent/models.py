from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping


def _string(value: Any, name: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{name} must be a string")
    return value


def _integer(value: Any, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{name} must be an integer")
    return value


def _mapping(value: Any, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError(f"{name} must be an object")
    return value


def format_hex(value: int, width: int) -> str:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0 or value >= 1 << (width * 4):
        raise ValueError(f"value does not fit in an unsigned {width * 4}-bit hexadecimal field")
    return f"0x{value:0{width}X}"


@dataclass(frozen=True)
class AgentConfig:
    path: Path
    transport: str
    endpoint: str
    dosbox_executable: Path
    dosbox_workdir: Path
    request_timeout_ms: int
    max_message_bytes: int
    max_memory_read_bytes: int
    max_trace_events: int
    test_profile: bool = False


@dataclass(frozen=True)
class MemoryAddress:
    space: str
    offset: str
    segment: str | None = None

    @classmethod
    def segmented(cls, segment: str | int, offset: str | int) -> "MemoryAddress":
        return cls("segmented", _hex_argument(offset, 8), _hex_argument(segment, 4))

    @classmethod
    def linear(cls, offset: str | int) -> "MemoryAddress":
        return cls("linear", _hex_argument(offset, 8))

    @classmethod
    def physical(cls, offset: str | int) -> "MemoryAddress":
        return cls("physical", _hex_argument(offset, 8))

    def to_rpc(self) -> dict[str, str]:
        result = {"space": self.space, "offset": self.offset}
        if self.segment is not None:
            result["segment"] = self.segment
        return result

    @classmethod
    def from_rpc(cls, value: Mapping[str, Any]) -> "MemoryAddress":
        space = _string(value.get("space"), "address.space")
        offset = _string(value.get("offset"), "address.offset")
        segment = value.get("segment")
        if segment is not None:
            segment = _string(segment, "address.segment")
        return cls(space=space, offset=offset, segment=segment)


def _hex_argument(value: str | int, width: int) -> str:
    if isinstance(value, int) and not isinstance(value, bool):
        return format_hex(value, width)
    if isinstance(value, str) and value.startswith("0x") and len(value) == width + 2:
        int(value[2:], 16)
        return "0x" + value[2:].upper()
    raise ValueError(f"hexadecimal argument must be an integer or 0x-prefixed {width}-digit string")


@dataclass(frozen=True)
class StopReason:
    kind: str
    address: MemoryAddress | None = None
    breakpoint_id: str | None = None

    @classmethod
    def from_rpc(cls, value: Mapping[str, Any]) -> "StopReason":
        address = value.get("address")
        return cls(
            kind=_string(value.get("kind"), "stop_reason.kind"),
            address=MemoryAddress.from_rpc(_mapping(address, "stop_reason.address")) if address is not None else None,
            breakpoint_id=value.get("breakpoint_id") if isinstance(value.get("breakpoint_id"), str) else None,
        )


@dataclass(frozen=True)
class Session:
    id: str
    state: str
    state_revision: int
    stop_reason: StopReason | None = None


@dataclass(frozen=True)
class Operation:
    id: str
    session_id: str
    state_revision: int


@dataclass(frozen=True)
class WaitResult:
    session: Session
    running: bool


@dataclass(frozen=True)
class RegisterSnapshot:
    general: Mapping[str, str]
    segments: Mapping[str, str]
    instruction_pointer: str
    flags: str
    cpu_mode: str
    state_revision: int


@dataclass(frozen=True)
class MemoryRead:
    address: MemoryAddress
    data: bytes
    sha256: str
    state_revision: int


@dataclass(frozen=True)
class MemoryWrite:
    address: MemoryAddress
    byte_count: int
    before_sha256: str
    after_sha256: str
    state_revision: int


@dataclass(frozen=True)
class Breakpoint:
    id: str
    kind: str
    address: MemoryAddress
    once: bool
    enabled: bool = True


@dataclass(frozen=True)
class OutputRecord:
    sequence: int
    timestamp: int
    level: str
    source: str
    message: str


@dataclass(frozen=True)
class OutputPage:
    records: tuple[OutputRecord, ...]
    next_cursor: str | None


@dataclass(frozen=True)
class TraceEvent:
    sequence: int
    address: MemoryAddress
    instruction: str
    register_changes: Mapping[str, str]


@dataclass(frozen=True)
class TracePage:
    active: bool
    events: tuple[TraceEvent, ...]
    next_cursor: str | None


@dataclass(frozen=True)
class DiagnosticCommandResult:
    accepted: bool
    raw_output: str
    parse_status: str
    parsed_registers: Mapping[str, str] | None
    unparsed_output: str | None
