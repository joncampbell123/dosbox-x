from __future__ import annotations

import base64
import json
import os
from pathlib import Path
import shutil
import socket
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from dosbox_agent import (
    AddressNotMappedError,
    AgentClient,
    AgentConfig,
    AgentConnectionError,
    BreakpointNotFoundError,
    InvalidBinaryLengthError,
    MemoryAddress,
    MemoryPreconditionFailedError,
    RequestTooLargeError,
    SessionNotFoundError,
    TargetExitedError,
    TargetNotStoppedError,
    TargetRunningError,
)
from dosbox_agent.client import UnixSocketTransport
from dosbox_agent.errors import map_rpc_error


class FakeTransport:
    def __init__(self, handler):
        self.handler = handler
        self.requests: list[dict] = []
        self.closed = False

    def request(self, payload: str, timeout_ms: int, max_message_bytes: int) -> str:
        request = json.loads(payload)
        self.requests.append(request)
        response = self.handler(request)
        return json.dumps({"jsonrpc": "2.0", "id": request["id"], **response})

    def close(self) -> None:
        self.closed = True


def make_config() -> AgentConfig:
    root = Path(__file__).resolve().parents[3]
    return AgentConfig(
        path=root / "tests" / "agent" / "agent-test.env",
        transport="named_pipe",
        endpoint=r"\\.\pipe\dosbox-agent-unit-test",
        dosbox_executable=root / "tests" / "agent" / "runtime" / "dosbox-x.exe",
        dosbox_workdir=root / "tests" / "agent" / "runtime",
        request_timeout_ms=5000,
        max_message_bytes=1024 * 1024,
        max_memory_read_bytes=65536,
        max_trace_events=10000,
        test_profile=True,
    )


def registers_result(revision: int = 3) -> dict:
    return {
        "session_id": "ses-1",
        "state_revision": revision,
        "general": {name: "0x00000000" for name in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")},
        "segments": {name: "0x0812" for name in ("cs", "ds", "es", "fs", "gs", "ss")},
        "instruction_pointer": "0x00000106",
        "flags": "0x00000202",
        "cpu_mode": "real",
    }


class AgentClientTests(unittest.TestCase):
    def test_all_v1_business_errors_have_typed_mappings(self) -> None:
        cases = {
            "SESSION_NOT_FOUND": SessionNotFoundError,
            "TARGET_NOT_STOPPED": TargetNotStoppedError,
            "TARGET_EXITED": TargetExitedError,
            "REQUEST_TOO_LARGE": RequestTooLargeError,
            "INVALID_BINARY_LENGTH": InvalidBinaryLengthError,
            "MEMORY_PRECONDITION_FAILED": MemoryPreconditionFailedError,
            "ADDRESS_NOT_MAPPED": AddressNotMappedError,
            "BREAKPOINT_NOT_FOUND": BreakpointNotFoundError,
        }
        for reason, error_type in cases.items():
            with self.subTest(reason=reason):
                error = map_rpc_error({"code": -32000, "message": reason, "data": {"reason": reason}})
                self.assertIsInstance(error, error_type)

    def test_config_is_explicit_and_relative_paths_resolve_from_config(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / "agent.env"
            config.write_text(
                "\n".join((
                    "transport=named_pipe",
                    r"endpoint=\\.\pipe\client-test",
                    "dosbox_executable=runtime/dosbox-x.exe",
                    "dosbox_workdir=runtime",
                    "profile=test",
                    "request_timeout_ms=50",
                    "max_message_bytes=64",
                    "max_memory_read_bytes=32",
                    "max_trace_events=16",
                )),
                encoding="utf-8",
            )
            client = AgentClient.from_config(config)
            self.assertTrue(client.config.test_profile)
            self.assertEqual((config.parent / "runtime" / "dosbox-x.exe").resolve(), client.config.dosbox_executable)
            self.assertEqual((config.parent / "runtime").resolve(), client.config.dosbox_workdir)
            client.close()

    def test_production_config_requires_absolute_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / "agent.env"
            config.write_text(
                "\n".join((
                    "transport=named_pipe",
                    r"endpoint=\\.\pipe\client-test",
                    "dosbox_executable=runtime\\dosbox-x.exe",
                    "dosbox_workdir=runtime",
                    "request_timeout_ms=50",
                    "max_message_bytes=64",
                    "max_memory_read_bytes=32",
                    "max_trace_events=16",
                )),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "absolute"):
                AgentClient.from_config(config)

    def test_unix_socket_config_requires_absolute_endpoint(self) -> None:
        def write_config(directory: str, endpoint: str) -> Path:
            config = Path(directory) / "agent.env"
            config.write_text(
                "\n".join((
                    "transport=unix_socket",
                    f"endpoint={endpoint}",
                    "dosbox_executable=runtime/dosbox-x",
                    "dosbox_workdir=runtime",
                    "profile=test",
                    "request_timeout_ms=50",
                    "max_message_bytes=64",
                    "max_memory_read_bytes=32",
                    "max_trace_events=16",
                )),
                encoding="utf-8",
            )
            return config

        with tempfile.TemporaryDirectory() as temporary:
            client = AgentClient.from_config(write_config(temporary, "/tmp/dosbox-agent.sock"))
            self.assertEqual("unix_socket", client.config.transport)
            self.assertEqual("/tmp/dosbox-agent.sock", client.config.endpoint)
            client.close()
            with self.assertRaisesRegex(ValueError, "absolute"):
                AgentClient.from_config(write_config(temporary, "dosbox-agent.sock"))

    def test_typed_methods_cover_v1_contract(self) -> None:
        address = {"space": "segmented", "segment": "0x0812", "offset": "0x00000106"}

        def handler(request: dict) -> dict:
            method = request["method"]
            if method == "agent.capabilities":
                return {"result": {"protocol_version": "1.0"}}
            if method == "session.start":
                return {"result": {"session_id": "ses-1", "state_revision": 1, "state": "stopped", "stop_reason": {"kind": "startup", "address": address}}}
            if method == "session.status":
                return {"result": {"session_id": "ses-1", "state_revision": 1, "state": "stopped", "last_stop": {"kind": "startup", "address": address}}}
            if method in ("execution.continue", "execution.pause", "session.stop"):
                return {"result": {"session_id": "ses-1", "state_revision": 2, "operation_id": "op-1"}}
            if method == "execution.wait":
                return {"result": {"session_id": "ses-1", "state_revision": 2, "state": "stopped", "stop_reason": {"kind": "breakpoint", "address": address}}}
            if method == "execution.step":
                result = registers_result(4)
                result["stop_reason"] = {"kind": "step", "address": address}
                result["registers"] = registers_result(4)
                return {"result": result}
            if method == "state.get_registers":
                return {"result": registers_result()}
            if method == "memory.read":
                return {"result": {"session_id": "ses-1", "state_revision": 3, "address": address, "byte_count": 3, "data_base64": "QUJD", "sha256": "a" * 64}}
            if method == "memory.write":
                return {"result": {"session_id": "ses-1", "state_revision": 4, "address": address, "byte_count": 3, "before_sha256": "b" * 64, "after_sha256": "c" * 64}}
            if method == "breakpoints.create":
                return {"result": {"session_id": "ses-1", "state_revision": 5, "breakpoint_id": "bp-1", "kind": "execution", "once": False, "address": address}}
            if method == "breakpoints.list":
                return {"result": {"session_id": "ses-1", "state_revision": 5, "breakpoints": [{"breakpoint_id": "bp-1", "kind": "execution", "enabled": True, "once": False, "address": address}]}}
            if method == "breakpoints.delete":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "breakpoint_id": "bp-1", "deleted": True}}
            if method == "debug.output.read":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "output": [{"sequence": 1, "timestamp": 1, "level": "info", "source": "debugger.command", "message": "CPU"}], "next_cursor": None}}
            if method == "debugger.execute_command":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "accepted": True, "raw_output": "CPU", "parse_status": "failed", "unparsed_output": "CPU"}}
            if method == "trace.start":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "active": True}}
            if method == "trace.read":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "active": False, "events": [{"sequence": 1, "address": address, "instruction": "MOV AX,1234", "register_changes": {"eax": "0x00001234"}}], "next_cursor": None}}
            if method == "trace.stop":
                return {"result": {"session_id": "ses-1", "state_revision": 6, "event_count": 1}}
            self.fail(f"unexpected method {method}")

        transport = FakeTransport(handler)
        client = AgentClient(make_config(), transport)
        self.assertEqual("1.0", client.capabilities()["protocol_version"])
        session = client.start("AGENTFIX.COM")
        self.assertEqual("0x0812", session.stop_reason.address.segment)
        self.assertEqual("0x00000106", client.status(session.id).stop_reason.address.offset)
        self.assertEqual("op-1", client.continue_(session.id).id)
        self.assertFalse(client.wait(session.id, "op-1", 1).running)
        stepped, stepped_registers = client.step(session.id)
        self.assertEqual("stopped", stepped.state)
        self.assertEqual("real", stepped_registers.cpu_mode)
        self.assertEqual("0x00000106", client.get_registers(session.id).instruction_pointer)
        self.assertEqual(b"ABC", client.read_memory(session.id, MemoryAddress.segmented(0x812, 0x106), 3).data)
        self.assertEqual("c" * 64, client.write_memory(session.id, MemoryAddress.segmented(0x812, 0x106), b"ABC").after_sha256)
        breakpoint = client.create_execution_breakpoint(session.id, 0x812, 0x106)
        self.assertEqual("bp-1", breakpoint.id)
        self.assertEqual((breakpoint,), client.list_breakpoints(session.id))
        self.assertEqual(6, client.delete_breakpoint(session.id, breakpoint.id))
        self.assertEqual("CPU", client.read_output(session.id, None, 1).records[0].message)
        self.assertTrue(client.execute_command(session.id, "CPU").accepted)
        self.assertTrue(client.start_trace(session.id, "normal", 1))
        self.assertEqual("MOV AX,1234", client.read_trace(session.id, None, 1).events[0].instruction)
        self.assertEqual(1, client.stop_trace(session.id))
        self.assertEqual("op-1", client.pause(session.id).id)
        self.assertEqual("op-1", client.stop(session.id).id)
        self.assertEqual(19, len(transport.requests))
        self.assertEqual(str(make_config().dosbox_workdir), transport.requests[1]["params"]["mounts"][0]["host_path"])

    def test_base64_request_error_mapping_and_explicit_request_id_retry(self) -> None:
        write_response = {"session_id": "ses-1", "state_revision": 2, "address": {"space": "segmented", "segment": "0x0812", "offset": "0x00000200"}, "byte_count": 2, "before_sha256": "a" * 64, "after_sha256": "b" * 64}

        def handler(request: dict) -> dict:
            if request["method"] == "state.get_registers":
                return {"error": {"code": -32012, "message": "Target is running", "data": {"reason": "TARGET_RUNNING"}}}
            return {"result": write_response}

        transport = FakeTransport(handler)
        client = AgentClient(make_config(), transport)
        with self.assertRaises(TargetRunningError):
            client.get_registers("ses-1")
        client.write_memory("ses-1", MemoryAddress.segmented(0x812, 0x200), b"\xDE\xAD", request_id="write-once")
        client.write_memory("ses-1", MemoryAddress.segmented(0x812, 0x200), b"\xDE\xAD", request_id="write-once")
        write_requests = [request for request in transport.requests if request["method"] == "memory.write"]
        self.assertEqual(["write-once", "write-once"], [request["id"] for request in write_requests])
        self.assertEqual(base64.b64encode(b"\xDE\xAD").decode("ascii"), write_requests[0]["params"]["data_base64"])


@unittest.skipUnless(hasattr(socket, "AF_UNIX"), "requires Unix domain sockets")
class UnixSocketTransportTests(unittest.TestCase):
    def setUp(self) -> None:
        # Keep the path short: sun_path is only 104 bytes on macOS.
        self.directory = tempfile.mkdtemp(prefix="dxa", dir="/tmp")
        self.endpoint = os.path.join(self.directory, "agent.sock")
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.endpoint)
        self.server.listen()
        self.threads: list[threading.Thread] = []

    def tearDown(self) -> None:
        for thread in self.threads:
            thread.join(5)
        self.server.close()
        shutil.rmtree(self.directory)

    def serve(self, replies) -> None:
        """Accept one connection per entry in replies. Each entry maps a request
        line to the chunks to send back, so a response can arrive in pieces."""
        def run() -> None:
            for reply in replies:
                connection, _ = self.server.accept()
                with connection:
                    pending = b""
                    while True:
                        chunk = connection.recv(4096)
                        if not chunk:
                            break
                        pending += chunk
                        while b"\n" in pending:
                            line, pending = pending.split(b"\n", 1)
                            for part in reply(line):
                                time.sleep(0.02)
                                try:
                                    connection.sendall(part)
                                except OSError:
                                    pass

        thread = threading.Thread(target=run)
        thread.start()
        self.threads.append(thread)

    def test_round_trip_reassembles_split_responses(self) -> None:
        self.serve([lambda line: (b'{"echo":', line + b"}\n")])
        transport = UnixSocketTransport(self.endpoint)
        try:
            self.assertEqual('{"echo":"first"}', transport.request('"first"', 1000, 1024))
            self.assertEqual('{"echo":"second"}', transport.request('"second"', 1000, 1024))
        finally:
            transport.close()

    def test_timeout_reconnects_instead_of_reading_a_late_response(self) -> None:
        self.serve([
            lambda line: [time.sleep(0.2) or b'"late"\n'],
            lambda line: [b'"fresh"\n'],
        ])
        transport = UnixSocketTransport(self.endpoint)
        try:
            with self.assertRaises(AgentConnectionError):
                transport.request('"slow"', 50, 1024)
            self.assertEqual('"fresh"', transport.request('"again"', 1000, 1024))
        finally:
            transport.close()

    def test_missing_endpoint_times_out(self) -> None:
        transport = UnixSocketTransport(os.path.join(self.directory, "missing.sock"))
        with self.assertRaises(AgentConnectionError):
            transport.request('"hello"', 50, 1024)


if __name__ == "__main__":
    unittest.main()
