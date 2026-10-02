# DOSBox-X Debugger Agent 使用手册

本文面向需要调用 DOSBox-X 调试后端的 AI Agent。目标是让 Agent 通过结构化 RPC 对 DOS 程序进行动态逆向：启动目标、停在入口、设置断点、运行到断点、读取寄存器和内存、单步、收集 trace，并在有明确前置条件时修改 guest memory。

Agent 不应依赖 DOSBox-X 窗口、stdout、`LOGCPU.TXT` 或 debugger 的人类可读文本来获得核心状态。正式数据必须来自 JSON-RPC；`debugger.execute_command` 只用于有限的只读诊断。

## 1. 运行模型

```text
Agent
  |
  | Python client 或 JSON-RPC 2.0
  v
DOSBox-X named pipe agent
  |
  | emulation-thread queue
  v
DOSBox-X debugger / emulator
  |
  v
DOS program
```

一次分析通常包含两个进程角色：

1. Agent 启动一个 DOSBox-X 进程，并传入 `--agent-config <path>`。
2. Agent 通过配置中的 Windows named pipe 发送 JSON-RPC 请求。

v1 的约束：

- 仅支持 Windows。
- 默认 transport 是当前用户可访问的 named pipe，不开放未认证 TCP。
- 一个 DOSBox-X 进程只有一个活动 session 和一个受控的 `C:` mount。
- `session.start` 固定在 `entry` 停止。
- target 必须是 DOS 安全文件名，例如 `GAME.COM`；不能传宿主机路径或 shell 命令。
- `session.start` 会等待入口停止后才返回；这不是一个只返回 operation id 的后台启动调用。
- `execution.continue`、`execution.pause`、`session.stop` 返回 `operation_id`，终态通过 `execution.wait` 获取。

## 2. 准备分析目录

建议为每次分析创建一个新的 runtime 目录，并只把待分析程序和必要的数据文件复制进去：

```text
reverse/
  configs/
    game.env
  runtime/
    GAME.COM
    DATA.DAT
```

`dosbox_workdir` 必须是受控目录。`session.start` 的 `mounts[0].host_path` 必须解析为同一个目录，不能临时切换到其他路径。目标程序和参数应使用 DOS 目录中的名字：

```python
session = agent.start(
    "GAME.COM",
    ["LEVEL1", "-DEBUG"],
    mount_path=workdir,
)
```

`target.command` 和 `target.arguments` 只允许字母、数字、`.`、`_`、`-`，因此应在启动前完成文件准备，不要把 `C:\...`、引号、重定向或 shell 元字符传给 RPC。

## 3. 配置文件

配置来自显式的 `.env` 文件。server 和 Python client 都不会从系统环境变量读取 endpoint、目标程序、工作目录或超时。

生产配置使用绝对路径：

```env
transport=named_pipe
endpoint=\\.\pipe\dosbox-agent-game-01
dosbox_executable=D:\workspace\dosbox-x\bin\x64\Agent Debug SDL2\dosbox-x.exe
dosbox_workdir=D:\workspace\reverse\game-01
profile=production
request_timeout_ms=5000
max_message_bytes=1048576
max_memory_read_bytes=65536
max_trace_events=10000
```

配置键说明：

| 键 | 说明 |
| --- | --- |
| `transport` | v1 使用 `named_pipe`。 |
| `endpoint` | Windows named pipe 名称；每个并行 DOSBox-X 进程应使用不同名称。 |
| `dosbox_executable` | 要启动的 DOSBox-X 可执行文件绝对路径。 |
| `dosbox_workdir` | 唯一受控的 `C:` mount 目录绝对路径。 |
| `profile` | `production` 或 `test`；正式 Agent 使用 `production`。 |
| `request_timeout_ms` | server 等待 emulation-thread 操作的上限。 |
| `max_message_bytes` | 单条 JSON request/response 的消息上限。 |
| `max_memory_read_bytes` | 单次 `memory.read` 和 `memory.write` 的最大字节数。 |
| `max_trace_events` | session trace store 的最大事件数。 |

`profile=test` 只用于仓库测试夹具。生产配置使用相对的 executable 或 workdir 会被拒绝；不要通过把路径放进 `tests` 目录来绕过校验。

## 4. 启动 DOSBox-X

PowerShell 示例：

```powershell
$config = 'D:\workspace\reverse\configs\game.env'
$process = Start-Process `
    -FilePath 'D:\workspace\dosbox-x\bin\x64\Agent Debug SDL2\dosbox-x.exe' `
    -ArgumentList @('--agent-config', $config) `
    -WindowStyle Hidden `
    -PassThru

# 等待进程完成 DOS shell 和 debugger 初始化。
Start-Sleep -Seconds 2
```

Agent 应通过进程句柄、named pipe readiness 或有限的启动等待确认 DOSBox-X 已经启动。不要无限重试 RPC，也不要把带副作用的请求自动重发。

Python Agent 通常在 `finally` 中关闭 client、停止 session，并回收 DOSBox-X 进程：

```python
process = subprocess.Popen(
    [str(config.dosbox_executable), "--agent-config", str(config.path)],
    cwd=repository_root,
    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
)
try:
    # 连接并进行 RPC 分析
    ...
finally:
    client.close()
    if process.poll() is None:
        process.kill()
    process.wait()
```

## 5. Python client 快速开始

仓库当前的 Python client 位于 `client/python`，还没有独立安装包。Agent 脚本可将该目录加入自身模块搜索路径：

```python
from pathlib import Path
import sys

repository_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repository_root / "client" / "python"))

from dosbox_agent import AgentClient, MemoryAddress

config_path = repository_root / "reverse" / "configs" / "game.env"
workdir = Path(r"D:\workspace\reverse\game-01")

with AgentClient.from_config(config_path) as agent:
    capabilities = agent.capabilities()
    session = agent.start("GAME.COM", mount_path=workdir)
    print(session.id, session.state, session.stop_reason)

    registers = agent.get_registers(session.id)
    code = agent.read_memory(
        session.id,
        MemoryAddress.segmented(registers.segments["cs"], registers.instruction_pointer),
        32,
    )
    print(registers)
    print(code.data.hex(), code.sha256)

    stop = agent.stop(session.id)
    stopped = agent.wait(session.id, stop.id, timeout_ms=5000)
    while stopped.running:
        stopped = agent.wait(session.id, stop.id, timeout_ms=5000)
```

关键返回对象是 typed model，不需要 Agent 手动解析 JSON 或 base64：

- `Session`：`id`、`state`、`state_revision`、`stop_reason`。
- `RegisterSnapshot`：`general`、`segments`、`instruction_pointer`、`flags`、`cpu_mode`。
- `MemoryRead`：`address`、`data: bytes`、`sha256`、`state_revision`。
- `MemoryWrite`：写入前后 SHA-256 和 `byte_count`。
- `Breakpoint`、`Operation`、`WaitResult`、`OutputPage`、`TracePage`。

## 6. 标准动态逆向流程

### 6.1 检查能力

先调用 `agent.capabilities()`，根据返回值决定是否使用 trace 或 memory-change breakpoint：

```python
capabilities = agent.capabilities()
if not capabilities["debugger"]:
    raise RuntimeError("current DOSBox-X build has no debugger capability")

trace_enabled = capabilities["trace"]["cpu"] is True
memory_breakpoint_enabled = capabilities["breakpoints"]["memory_change"] is True
```

不要把 capability 缺失当成普通断点或空 trace；server 会返回 `CapabilityUnavailableError`。

### 6.2 在入口建立基线

`agent.start()` 成功后 session 应为 `stopped`，`stop_reason.kind` 应为 `startup`。此时保存：

- `CS:IP` 和 `DS:SI/SS:SP` 等段寄存器组合。
- 通用寄存器和 FLAGS。
- 入口附近的 code bytes。
- 关键数据区的初始 bytes 和 SHA-256。

示例：

```python
session = agent.start("GAME.COM", mount_path=workdir)
assert session.state == "stopped"
assert session.stop_reason is not None
assert session.stop_reason.kind == "startup"

regs = agent.get_registers(session.id)
entry = MemoryAddress.segmented(regs.segments["cs"], regs.instruction_pointer)
entry_bytes = agent.read_memory(session.id, entry, 64)
```

不要假设 `CS`、`DS` 相同，也不要把 segmented 地址拼成字符串。使用 `MemoryAddress.segmented()`、`.linear()` 或 `.physical()` 明确指定 address space。

### 6.3 设置断点并运行

执行断点使用稳定的 `breakpoint_id`，不使用底层 debugger 的列表索引：

```python
breakpoint = agent.create_execution_breakpoint(
    session.id,
    segment=regs.segments["cs"],
    offset="0x00001234",
)

operation = agent.continue_(session.id)
result = agent.wait(session.id, operation.id, timeout_ms=5000)
while result.running:
    result = agent.wait(session.id, operation.id, timeout_ms=5000)

assert result.session.stop_reason is not None
print(result.session.stop_reason.kind, result.session.stop_reason.address)
```

执行断点当前要求 `segmented` 地址。`once=True` 可创建命中后自动删除的一次性断点：

```python
temporary = agent.create_execution_breakpoint(
    session.id, regs.segments["cs"], "0x00001400", once=True
)
```

### 6.4 读取寄存器和内存

寄存器和内存观察只能在 `stopped` 状态进行。运行时调用会抛出 `TargetRunningError`。

```python
regs = agent.get_registers(session.id)

stack = agent.read_memory(
    session.id,
    MemoryAddress.segmented(regs.segments["ss"], regs.general["esp"]),
    32,
)

linear_page = agent.read_memory(
    session.id,
    MemoryAddress.linear("0x00001000"),
    256,
)

physical = agent.read_memory(
    session.id,
    MemoryAddress.physical("0x000A0000"),
    64,
)
```

注意：`regs.general["esp"]` 是固定宽度十六进制字符串，适合直接传给 `MemoryAddress.segmented()`。如果地址未映射、selector 无效、超过 segment limit 或 page 不存在，client 会抛出 `AddressNotMappedError`；错误对象的 `data` 中包含失败地址和底层原因。

### 6.5 单步进入与越过

```python
stopped, after_step = agent.step(session.id, mode="into")
print(stopped.stop_reason.kind, after_step.instruction_pointer)

stopped, after_over = agent.step(session.id, mode="over")
```

`into` 进入当前指令的执行路径；`over` 使用 debugger 的现有 step-over 语义越过 call、interrupt 或重复指令的目标。每次返回都包含新的 register snapshot，不需要再次调用 `get_registers`，但可以再读内存验证副作用。

### 6.6 观察内存变化

优先使用已知代码位置的 execution breakpoint 和前后内存快照。heavy-debug build 还可以使用 memory-change breakpoint：

```python
if memory_breakpoint_enabled:
    watch = agent.create_breakpoint(
        session.id,
        "memory_change",
        MemoryAddress.segmented(regs.segments["ds"], "0x00000200"),
    )
```

不支持 memory-change breakpoint 时，必须改变分析策略，不能自动降级为 execution breakpoint。

### 6.7 修改 guest memory

写入前先读取并保留 SHA-256，然后用 `expected_sha256` 做乐观并发保护：

```python
address = MemoryAddress.segmented(regs.segments["ds"], "0x00000200")
before = agent.read_memory(session.id, address, 4)

written = agent.write_memory(
    session.id,
    address,
    b"\x90\x90\x90\x90",
    expected_sha256=before.sha256,
)
print(written.before_sha256, written.after_sha256)
```

如果目标在读取和写入之间发生变化，server 返回 `MemoryPreconditionFailedError`，本次写入不会发生。写入超时后不要自动重试；如果要由 Agent 明确重试，复用同一个 `request_id` 并确认请求 payload 完全相同。

## 7. Trace 和 debugger 输出

### 7.1 CPU trace

CPU trace 需要 `C_HEAVY_DEBUG=1`。trace 是 session-local 的结构化数据，不依赖 `LOGCPU.TXT`：

```python
if capabilities["trace"]["cpu"]:
    agent.start_trace(session.id, detail="normal", instruction_count=200)

    operation = agent.continue_(session.id)
    stopped = agent.wait(session.id, operation.id, timeout_ms=10000)
    while stopped.running:
        stopped = agent.wait(session.id, operation.id, timeout_ms=10000)

    cursor = None
    while True:
        page = agent.read_trace(session.id, cursor, limit=50)
        for event in page.events:
            print(event.sequence, event.address, event.instruction, event.register_changes)
        if page.next_cursor is None:
            break
        cursor = page.next_cursor

    agent.stop_trace(session.id)
```

`trace.read` 的 `cursor` 初始为 `None`，下一页使用上一次返回的 `next_cursor`。不要在 cursor 过期后从头拼接；应重新建立 trace 分析窗口。`instruction_count` 受配置的 `max_trace_events` 限制。

### 7.2 Debugger 输出

正式 API 不需要解析 debugger 文本。需要诊断时只能执行当前 adapter allowlist 中的只读命令：`HELP`、`CPU`、`PIC`。

```python
diagnostic = agent.execute_command(session.id, "CPU")
if diagnostic.accepted:
    print(diagnostic.raw_output)
```

输出记录来自 session-local ring buffer，可用 cursor 分页：

```python
cursor = None
while True:
    page = agent.read_output(session.id, cursor, limit=100)
    for record in page.records:
        print(record.sequence, record.source, record.message)
    if page.next_cursor is None:
        break
    cursor = page.next_cursor
```

`RUN`、`SM`、`SR`、`BP*`、`LOG*` 等会改变调试状态或 collector 状态的命令不属于稳定诊断 API，server 会拒绝它们。请使用对应的结构化 RPC 方法。

## 8. 会话状态和等待规则

| 状态 | Agent 可以做什么 |
| --- | --- |
| `starting` | 查询 `session.status`、读取输出；等待 `session.start` 完成。 |
| `stopped` | 读取/写入 memory、读取 registers、断点管理、continue、pause、step、trace 操作。 |
| `running` | `session.status`、`execution.pause`、`execution.wait`、`debug.output.read`。其他状态观察和修改请求返回 `TARGET_RUNNING`。 |
| `exited` | 查询最终状态、读取最后 stop reason、重复 stop；不能读取 guest registers/memory。 |
| `failed` | 查询状态、读取输出、执行 stop 清理；不要继续调用 target 操作。 |

每次成功响应都带 `state_revision`。Agent 应保存最后一次 revision，在断点命中、单步、memory.write 或断点集合变化后更新自己的观察缓存。旧 revision 的内存快照不能直接当成当前状态。

`execution.wait` 的超时不是失败：

```python
result = agent.wait(session.id, operation.id, timeout_ms=100)
if result.running:
    # 目标仍在运行；继续使用同一个 operation_id 轮询或先 pause。
    ...
```

不要把 `running=true` 当成程序退出，也不要为“确认停止”创建新的 continue 请求。

## 9. 错误处理和重试

Python client 会把 RPC business error 映射为 typed exception：

```python
from dosbox_agent import (
    AddressNotMappedError,
    AgentConnectionError,
    CapabilityUnavailableError,
    MemoryPreconditionFailedError,
    TargetRunningError,
)

try:
    registers = agent.get_registers(session.id)
except TargetRunningError:
    # 先 pause 或 wait，不要读取不一致状态。
    ...
except AddressNotMappedError as error:
    print(error.data)
except MemoryPreconditionFailedError:
    # 重新 read，确认目标状态后再决定是否写入。
    ...
except CapabilityUnavailableError:
    # 改用 capability 已声明支持的分析方法。
    ...
except AgentConnectionError:
    # 进程或 named pipe 已不可用；不要重发有副作用请求。
    ...
```

重要错误：

| reason | Agent 行为 |
| --- | --- |
| `SESSION_BUSY` | 当前进程已有 active session；先 stop 并 wait，或创建新的 DOSBox-X 进程。 |
| `SESSION_NOT_FOUND` | 丢弃本地 session 引用，不能继续使用旧 operation。 |
| `TARGET_RUNNING` | 先 pause 或等待停止。 |
| `TARGET_NOT_STOPPED` / `TARGET_EXITED` | 修正状态机，不要重试相同请求。 |
| `CAPABILITY_UNAVAILABLE` | 读取 capabilities，选择其他策略。 |
| `ADDRESS_NOT_MAPPED` | 检查 address space、segment、offset 和 CPU mode。 |
| `MEMORY_PRECONDITION_FAILED` | 重新读取并重新判断，不能盲写。 |
| `REQUEST_TOO_LARGE` | 减小 memory/trace/page 请求；server 不截断。 |
| `OPERATION_TIMEOUT` | 对 start 或 adapter 操作记录失败/不确定状态；不要自动重复有副作用调用。 |
| `CURSOR_EXPIRED` | 丢弃旧 cursor，重新开始一个明确的新读取窗口。 |
| `REQUEST_ID_CONFLICT` | 同一 request id 被用于不同 payload；生成新的逻辑请求 id。 |

server 会缓存 session 生命周期内最近完成的请求结果。只有 Agent 明确确认“同一个逻辑请求、同一个 payload”时，才可以用相同 `request_id` 重试。Python client 不会自动重试写内存、断点、单步、原始命令或其他有副作用调用。

## 10. 非 Python Agent 的最小 JSON-RPC 调用

传输是 UTF-8 JSON Lines，一行一个 request，一行一个 response。下面是入口启动请求：

```json
{"jsonrpc":"2.0","id":"start-1","method":"session.start","params":{"target":{"command":"GAME.COM","arguments":[]},"mounts":[{"drive":"C","host_path":"D:/workspace/reverse/game-01"}],"break_at":"entry"}}
```

入口成功响应的核心字段：

```json
{
  "jsonrpc": "2.0",
  "id": "start-1",
  "result": {
    "session_id": "ses-1",
    "state": "stopped",
    "state_revision": 1,
    "stop_reason": {
      "kind": "startup",
      "address": {
        "space": "segmented",
        "segment": "0x0812",
        "offset": "0x00000100"
      }
    }
  }
}
```

读取寄存器：

```json
{"jsonrpc":"2.0","id":"regs-1","method":"state.get_registers","params":{"session_id":"ses-1"}}
```

读取内存：

```json
{"jsonrpc":"2.0","id":"mem-1","method":"memory.read","params":{"session_id":"ses-1","address":{"space":"segmented","segment":"0x0812","offset":"0x00000100"},"length":16}}
```

## 11. 结束分析

正常结束必须停止 session 并等待终态。当前 server 的终止 deadline 使用配置中的 `request_timeout_ms`；`graceful_timeout_ms` 会被 client 发送，但目前不改变 server 的终止 deadline，因此不要依赖它延长等待：

```python
stop_operation = agent.stop(session.id, graceful_timeout_ms=5000)
result = agent.wait(session.id, stop_operation.id, timeout_ms=5000)
while result.running:
    result = agent.wait(session.id, stop_operation.id, timeout_ms=5000)

assert result.session.state == "exited"
```

无论分析成功还是异常，都应在 `finally` 中：

1. 对仍存在的 session 执行 stop/wait。
2. 关闭 `AgentClient`。
3. 回收 DOSBox-X 进程。
4. 为下一次分析使用新的 endpoint 或确认上一次进程已经完全退出。

不要复用上一次运行产生的 `LOGCPU.TXT`、`MEMDUMP.*`、trace 文件或未清理的 runtime 目录来推断本次分析结果。

## 12. 推荐的 Agent 分析策略

一个稳定的逆向 Agent 可以按以下顺序工作：

1. 创建干净 runtime，复制目标和输入数据。
2. 启动 DOSBox-X，调用 `agent.capabilities`。
3. `session.start` 停在入口，保存寄存器、入口 code bytes 和关键内存 SHA-256。
4. 根据静态分析或前一轮动态结果设置少量 execution breakpoint。
5. `continue_` + `wait` 到断点，记录 stop reason、register snapshot 和 `state_revision`。
6. 围绕当前 `CS:IP`、栈和数据段读取小范围内存，不要一次读取整个 guest memory。
7. 对可疑指令使用 `step("into")` 或 `step("over")`，每一步只做必要的观察。
8. 需要批量指令历史时，在 heavy-debug build 中使用 session-local CPU trace。
9. 需要验证 patch 时使用 `read -> expected_sha256 -> write -> read` 闭环。
10. 每轮实验结束后 stop/wait，保存结构化结果，再清理进程和 runtime。

这样可以把“静态假设”和“动态证据”绑定到明确的地址、寄存器、内存 hash、断点 id、trace sequence 和 state revision，而不是依赖窗口截图或文本日志。

## 13. 相关文件和验收命令

- 协议契约：[rpc.md](rpc.md)
- 开发和验收规范：[rpc-development.md](rpc-development.md)
- Python client：`client/python/dosbox_agent/`
- Python E2E：`client/python/tests/test_e2e.py`
- 测试配置：`tests/agent/agent-test.env`
- 测试夹具：`tests/agent/fixtures/`

开发者可用以下命令验证 client 和完整闭环：

```powershell
py -m unittest discover -s client\python\tests -t client\python -v
py client\python\tests\test_e2e.py --config tests\agent\agent-test.env
```

测试配置使用 `profile=test` 和仓库 runtime；生产 Agent 不应直接复用它，而应创建绝对路径的 `profile=production` 配置。
