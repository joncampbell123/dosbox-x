from __future__ import annotations

from collections.abc import Mapping
from typing import Any


class AgentError(Exception):
    """Base class for client and JSON-RPC failures."""


class AgentConnectionError(AgentError):
    """The client could not establish or maintain the local IPC connection."""


class AgentProtocolError(AgentError):
    """The server returned a response that does not satisfy the RPC contract."""


class AgentRpcError(AgentError):
    def __init__(self, code: int, message: str, data: Mapping[str, Any] | None = None) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = dict(data or {})
        self.reason = self.data.get("reason")


class SessionBusyError(AgentRpcError):
    pass


class SessionNotFoundError(AgentRpcError):
    pass


class TargetRunningError(AgentRpcError):
    pass


class TargetNotStoppedError(AgentRpcError):
    pass


class TargetExitedError(AgentRpcError):
    pass


class CapabilityUnavailableError(AgentRpcError):
    pass


class RequestTooLargeError(AgentRpcError):
    pass


class InvalidBinaryLengthError(AgentRpcError):
    pass


class MemoryPreconditionFailedError(AgentRpcError):
    pass


class AddressNotMappedError(AgentRpcError):
    pass


class RequestIdConflictError(AgentRpcError):
    pass


class CommandRejectedError(AgentRpcError):
    pass


class BreakpointNotFoundError(AgentRpcError):
    pass


class OperationTimeoutError(AgentRpcError):
    pass


class CursorExpiredError(AgentRpcError):
    pass


_ERROR_TYPES: dict[str, type[AgentRpcError]] = {
    "SESSION_BUSY": SessionBusyError,
    "SESSION_NOT_FOUND": SessionNotFoundError,
    "TARGET_RUNNING": TargetRunningError,
    "TARGET_NOT_STOPPED": TargetNotStoppedError,
    "TARGET_NOT_RUNNING": TargetNotStoppedError,
    "TARGET_EXITED": TargetExitedError,
    "CAPABILITY_UNAVAILABLE": CapabilityUnavailableError,
    "REQUEST_TOO_LARGE": RequestTooLargeError,
    "INVALID_BINARY_LENGTH": InvalidBinaryLengthError,
    "MEMORY_PRECONDITION_FAILED": MemoryPreconditionFailedError,
    "ADDRESS_NOT_MAPPED": AddressNotMappedError,
    "REQUEST_ID_CONFLICT": RequestIdConflictError,
    "COMMAND_REJECTED": CommandRejectedError,
    "BREAKPOINT_NOT_FOUND": BreakpointNotFoundError,
    "OPERATION_TIMEOUT": OperationTimeoutError,
    "CURSOR_EXPIRED": CursorExpiredError,
}


def map_rpc_error(error: Mapping[str, Any]) -> AgentRpcError:
    data = error.get("data")
    if not isinstance(data, Mapping):
        data = {}
    reason = data.get("reason")
    error_type = _ERROR_TYPES.get(reason, AgentRpcError)
    code = error.get("code")
    message = error.get("message")
    if not isinstance(code, int) or not isinstance(message, str):
        raise AgentProtocolError("JSON-RPC error response has an invalid code or message")
    return error_type(code, message, data)
