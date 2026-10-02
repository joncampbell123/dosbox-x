from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import time

CLIENT_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CLIENT_ROOT))

from dosbox_agent import AgentClient, MemoryAddress


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True)
    arguments = parser.parse_args()
    config_path = Path(arguments.config).resolve()
    client = AgentClient.from_config(config_path)
    if not client.config.dosbox_executable.is_file():
        raise RuntimeError(f"DOSBox-X executable was not found: {client.config.dosbox_executable}")

    process = subprocess.Popen(
        [str(client.config.dosbox_executable), "--agent-config", str(config_path)],
        cwd=REPOSITORY_ROOT,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    session_id: str | None = None
    try:
        # Popen returns before DOSBox-X has completed its shell bootstrap. This
        # is a process-readiness wait, not a retry of any stateful RPC.
        time.sleep(2)
        session = client.start("AGENTFIX.COM")
        session_id = session.id
        if session.state != "stopped" or session.stop_reason is None or session.stop_reason.kind != "startup":
            raise AssertionError("session.start did not stop at the fixture entry point")
        if session.stop_reason.address is None:
            raise AssertionError("session.start did not return the entry address")

        diagnostic = client.execute_command(session.id, "CPU")
        if not diagnostic.accepted or not diagnostic.raw_output:
            raise AssertionError("debugger.execute_command did not return diagnostic output")
        output = client.read_output(session.id, None, 1)
        if len(output.records) != 1 or output.records[0].sequence != 1 or not output.records[0].message:
            raise AssertionError("debug.output.read did not return the session-local diagnostic record")

        if not client.start_trace(session.id, "normal", 2):
            raise AssertionError("trace.start did not activate")
        trace_operation = client.continue_(session.id)
        trace_stop = client.wait(session.id, trace_operation.id, 10000)
        if trace_stop.running or trace_stop.session.state != "stopped":
            raise AssertionError("trace execution did not return to a stopped state")
        trace = client.read_trace(session.id, None, 2)
        if [event.sequence for event in trace.events] != [1, 2]:
            raise AssertionError("trace.read did not return exactly two session-local events")
        if client.stop_trace(session.id) != 2:
            raise AssertionError("trace.stop did not report two collected events")

        breakpoint = client.create_execution_breakpoint(
            session.id,
            session.stop_reason.address.segment,
            "0x00000106",
        )
        operation = client.continue_(session.id)
        stopped = client.wait(session.id, operation.id, 10000)
        if stopped.running or stopped.session.stop_reason is None or stopped.session.stop_reason.kind != "breakpoint":
            raise AssertionError("execution.continue did not stop on the created breakpoint")

        registers = client.get_registers(session.id)
        if registers.instruction_pointer != "0x00000106":
            raise AssertionError(f"unexpected breakpoint instruction pointer: {registers.instruction_pointer}")

        code = client.read_memory(session.id, MemoryAddress.segmented(registers.segments["cs"], "0x00000100"), 6)
        if code.data != bytes((0xB8, 0x34, 0x12, 0xBB, 0x78, 0x56)):
            raise AssertionError(f"fixture code mismatch: {code.data.hex()}")

        data_address = MemoryAddress.segmented(registers.segments["ds"], "0x00000200")
        before = client.read_memory(session.id, data_address, 4)
        written = client.write_memory(
            session.id,
            data_address,
            b"\xDE\xAD\xBE\xEF",
            expected_sha256=hashlib.sha256(before.data).hexdigest(),
        )
        after = client.read_memory(session.id, data_address, 4)
        if after.data != b"\xDE\xAD\xBE\xEF" or written.after_sha256 != hashlib.sha256(after.data).hexdigest():
            raise AssertionError("memory.write verification failed")

        stepped, step_registers = client.step(session.id, "into")
        if stepped.stop_reason is None or stepped.stop_reason.kind != "step" or not step_registers.instruction_pointer:
            raise AssertionError("execution.step did not return a stopped register snapshot")

        stop = client.stop(session.id)
        exited = client.wait(session.id, stop.id, 10000)
        if exited.running or exited.session.state != "exited" or exited.session.stop_reason is None or exited.session.stop_reason.kind != "program_exit":
            raise AssertionError("session.stop did not terminate the fixture")
        print("RPC-E02 passed: Python client completed fixture start, breakpoint, execution, memory, step, and stop.")
        return 0
    finally:
        if session_id is not None:
            try:
                status = client.status(session_id)
                if status.state != "exited":
                    stop = client.stop(session_id)
                    client.wait(session_id, stop.id, 1000)
            except Exception:
                pass
        client.close()
        if process.poll() is None:
            process.kill()
        process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
