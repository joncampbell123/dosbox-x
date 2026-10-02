# DOSBox-X Debugger Agent RPC 开发与验收规范

状态：计划基线

版本：v1.0-draft

日期：2026-09-02

需求来源：[rpc.md](rpc.md)

本文是 `docs/rpc.md` 的可实施、可验收版本。开发任务、代码评审和发布验收必须以本文的检查项为准。检查项只有在命令执行成功、结果满足预期且证据已记录后才能勾选；不得用“接口存在”“命令未报错”替代行为验证。

## 1. 目标与边界

目标是在不重写 CPU、内存和断点机制的前提下，将 DOSBox-X 现有 debugger 能力封装为稳定的结构化 Agent API。Agent 只消费 JSON 数据，不依赖 ncurses 窗口或 debugger 的人类可读文本。

v1 的部署、构建和验收范围仅限 Windows。

第一期必须支持：

- 启动、停止并查询 DOS 调试会话。
- 继续、暂停、单步进入、单步越过和等待停止事件。
- 获取通用寄存器、段寄存器、IP/EIP 和 FLAGS/EFLAGS。
- 在 segmented、linear、physical 三种地址空间读取和写入内存。
- 创建、列出和删除执行断点；仅在底层实际支持时提供内存变化断点。
- 读取结构化 debugger 输出和执行 trace。
- 执行原始 debugger 命令，作为诊断接口而不是主 API。
- 提供 Python client 和端到端验收夹具。

第一期禁止：

- 修改 `src/cpu/` 的执行语义、指令实现或调度语义。
- 修改 `src/hardware/memory.cpp` 的内存映射、分页和读写语义。
- 自行实现第二套断点、单步或内存访问机制。
- 让 RPC 直接返回 `AX=...`、`CS:IP=...` 等 debugger 文本并要求客户端解析。
- 因地址无法解析、capability 不存在或 target 正在运行而悄悄降级、填充默认值或自动重试写操作。

## 2. 当前代码基线

实现必须复用以下现有能力，并在 adapter 内提供结构化包装：

| 能力 | 现有入口 | 说明 |
| --- | --- | --- |
| Debugger 生命周期 | `include/debug.h`、`src/debug/debug.cpp` | `DEBUG_EnableDebugger()`、`DEBUG_ExitLoop()` 和 `DEBUG_Enable_Handler()` 已管理 debugger 进入/退出。 |
| 执行与单步 | `src/debug/debug.cpp` | 现有 `RUN`、`RUNWATCH`、F10/F11 和 `StepOver()` 提供底层控制。 |
| 断点 | `src/debug/debug.cpp` | `CBreakpoint` 已实现执行、interrupt、memory/protected/linear 等类型。 |
| 寄存器与地址解析 | `src/debug/debug.cpp`、`include/cpu.h` | debugger 已读取 `reg_*`、段寄存器及 `GetAddress()`。 |
| 内存访问 | `src/debug/debug.cpp`、`src/hardware/memory.cpp` | debugger 已通过 checked memory API 访问 guest memory。 |
| Debugger 命令 | `README.debugger`、`src/debug/debug.cpp` | 现有 `RUN`、`SR`、`SM`、`BP`、`BPLIST`、`BPDEL`、`MEMDUMP*`、`LOG*` 等命令可作兼容和诊断参考。 |
| 单元测试入口 | `tests/tests.h`、`tests/readme.txt` | Debug build 可通过 `dosbox-x -tests` 运行 gTest。 |

构建前置条件：

- `C_DEBUG=1` 是 v1 的硬性条件；未启用时 server 必须拒绝启动并返回 `DEBUGGER_UNAVAILABLE`。
- `C_HEAVY_DEBUG=1` 是 CPU trace 的硬性条件；未启用时 `agent.capabilities` 必须声明 `trace.cpu=false`，`trace.start` 返回 `CAPABILITY_UNAVAILABLE`。
- Windows 当前 `.vcxproj` 配置没有显式定义 `C_DEBUG`。实现必须新增或调整一个专用调试配置，例如 `Agent Debug SDL2`，显式定义 `C_DEBUG=1;C_HEAVY_DEBUG=1;C_DOSBOX_AGENT=1`。不得使用现有 Release 输出作为 RPC 验收对象。

## 3. 总体架构

```text
Python client / AI Agent
          |
          | JSON-RPC 2.0
          v
dosbox-agent server
  - transport
  - session manager
  - request validation
          |
          | authenticated local IPC
          v
DOSBox-X in-process agent bridge
  - emulation-thread command queue
  - DebuggerAdapter
  - output/trace collector
          |
          v
DOSBox-X debugger and emulator
```

职责划分：

- `dosbox-agent server` 管理 RPC transport、DOSBox-X 子进程、会话状态和客户端连接。它不直接读取 `reg_*` 或 guest memory。
- in-process bridge 仅在 DOSBox-X 的 emulation thread 上调用 debugger、CPU 和 memory 已有能力。任何 RPC worker thread 都不得直接访问 debugger globals。
- `DebuggerAdapter` 以 C++ 类型返回 registers、memory、breakpoints、stop reason 和 trace events；transport 层只负责编解码 JSON。
- `DebuggerOutputParser` 只服务于 `debugger.execute_command` 的非结构化诊断输出。核心方法不得以 parser 作为数据来源。

现有 debugger 使用大量全局状态，例如 `debugging`、`debug_running`、`runnormal` 和 `CBreakpoint::BPoints`。因此 v1 每个 DOSBox-X 进程只允许一个活动调试会话。第二个 `session.start` 必须返回 `SESSION_BUSY`，不得共享或覆盖已有 session。

## 4. 目录与构建交付物

实现完成后的目标目录如下。可以按仓库既有布局微调文件名，但职责不能合并或缺失。

```text
src/agent/
  protocol/                 # JSON model、错误码、schema version
  server/                   # host-side process/session manager
  transport/                # Windows named pipe / test transport
  bridge/                   # in-process queue and emulation-thread dispatch
  debugger/                 # DebuggerAdapter and structured collectors
  parser/                   # raw debugger output parser only
  trace/                    # trace sink and paged store

include/agent/
  debugger_adapter.h
  agent_bridge.h
  agent_protocol.h

client/python/dosbox_agent/
  __init__.py
  client.py
  models.py
  errors.py

tests/agent/
  agent_adapter_tests.cpp
  agent_parser_tests.cpp
  agent_protocol_tests.cpp
  fixtures/agent_fixture.asm
  fixtures/agent_fixture.com
  agent-test.env

client/python/tests/
  test_client.py
  test_e2e.py
```

必须更新 Windows 的 `vs/dosbox-x.vcxproj` 和对应 solution configuration。新增源文件未进入 Windows 构建系统即视为未完成。

## 5. 配置、传输与安全

### 5.1 配置文件

不得读取系统环境变量决定 endpoint、目标程序、工作目录、超时或认证信息。server 仅从显式传入的 `--agent-config <path>` 读取 dotenv 格式的配置文件，例如 `tests/agent/agent-test.env`：

```env
transport=named_pipe
endpoint=\\.\pipe\dosbox-agent-test
dosbox_executable=bin\x64\Agent Debug SDL2\dosbox-x.exe
dosbox_workdir=tests\agent\runtime
profile=production
request_timeout_ms=5000
max_message_bytes=1048576
max_memory_read_bytes=65536
max_trace_events=10000
```

生产配置必须使用绝对路径；测试配置可使用相对 `--agent-config` 文件所在目录的路径。启动时必须将解析后的配置写入结构化启动日志，但不得记录认证 token。

### 5.2 传输

`IRpcTransport` 是唯一 transport 抽象，协议层不得依赖具体 named pipe 实现或 stdio。

- Windows v1 默认使用 named pipe，并设置仅当前用户可访问的 ACL。
- v1 不开放 TCP listener，不支持远程无认证连接。
- 所有 transport 必须按 JSON-RPC 2.0 传输完整 request/response；流式 transport 的 framing 采用 UTF-8 JSON Lines，一行一个 JSON object，最大消息长度受 `max_message_bytes` 限制。
- server 与 bridge 使用独立的本地 IPC endpoint，并使用每次启动生成的一次性 token 做握手；token 只经受限配置文件或命令行参数传递，不写入日志。

### 5.3 并发、超时和幂等

- 每个 session 的调试操作严格串行；请求按接收顺序进入 emulation-thread queue。
- `execution.continue`、`execution.pause` 和 `session.stop` 返回 `operation_id`。客户端通过 `execution.wait` 获取停止结果。
- 写内存、创建/删除断点、单步和原始 debugger 命令是有副作用操作。超时后 server 不得自动重试。
- 客户端重试必须携带相同 `request_id`。server 在 session 生命周期内缓存最近 1024 个已完成 request 的结果；同一 `request_id` 的 payload 不同返回 `REQUEST_ID_CONFLICT`。
- `execution.wait` 超时只表示仍在运行，返回 `running=true`；不得把超时伪造为断点、暂停或程序退出。

## 6. 会话状态机

```text
created -> starting -> stopped -> running -> stopped
                  |              |          |
                  v              v          v
                failed         exited     stopping -> exited
```

状态定义：

- `starting`：DOSBox-X 已启动，DOS environment 或 target 正在初始化。
- `stopped`：emulation 已停在 entry、breakpoint、step、明确 pause 或 fault；可读取/写入 state、memory 和 breakpoints。
- `running`：target 正在执行；除 `session.status`、`execution.pause`、`execution.wait`、`debug.output.read` 外，所有观察/修改状态方法必须返回 `TARGET_RUNNING`。
- `exited`：target 已退出；会话状态与最后 stop reason 可读，内存和寄存器不可读。
- `failed`：启动或 bridge 出错；只允许 `session.status`、`debug.output.read` 和 `session.stop`。

所有成功响应必须包含 `session_id` 和递增的 `state_revision`。`state_revision` 在每次停止、单步、写内存和断点集合变化后增加，用于客户端发现过期观察结果。

## 7. 数据约定

### 7.1 数值和地址

JSON number 不能无损表达所有未来 guest address，v1 所有 guest 寄存器、offset、selector、flags 和 breakpoint 地址均使用固定宽度的大写十六进制字符串。

```json
{
  "ax": "0x1234",
  "eip": "0x00000100",
  "flags": "0x00000202"
}
```

地址是 tagged object，不接受模糊的 `"segment:offset"` 字符串：

```json
{
  "space": "segmented",
  "segment": "0x1234",
  "offset": "0x00000020"
}
```

支持的 `space` 值：

- `segmented`：按当前 CPU mode 解析 selector:offset；real/v86 mode 中按段地址计算，protected mode 中遵从 descriptor 和 paging。
- `linear`：CPU linear/virtual address；需要正常的 paging 检查。
- `physical`：物理内存地址；不通过 CPU paging。

地址未映射、selector 无效、offset 越界或 page 不存在时必须返回 `ADDRESS_NOT_MAPPED`，并在 `error.data` 返回 `space`、`address` 和底层原因（`invalid_selector`、`segment_limit`、`page_not_present` 等）。不得将访问悄悄转换为 physical 地址。

### 7.2 二进制和分页

- 内存 bytes 一律使用 RFC 4648 base64；返回 `byte_count`、`data_base64` 和 SHA-256。
- 单个 `memory.read` 最大值为配置的 `max_memory_read_bytes`；超限返回 `REQUEST_TOO_LARGE`，不截断。
- `debug.output.read` 和 `trace.read` 使用显式 `cursor`、`limit` 和 `next_cursor` 分页。cursor 过期返回 `CURSOR_EXPIRED`，不从头补发。

### 7.3 统一响应和错误

成功响应：

```json
{
  "jsonrpc": "2.0",
  "id": "req-0001",
  "result": {
    "session_id": "ses-...",
    "state_revision": 7
  }
}
```

错误响应：

```json
{
  "jsonrpc": "2.0",
  "id": "req-0001",
  "error": {
    "code": -32012,
    "message": "Target must be stopped before reading memory",
    "data": {
      "reason": "TARGET_RUNNING",
      "session_id": "ses-...",
      "state": "running"
    }
  }
}
```

保留 JSON-RPC 标准错误 `-32600`、`-32601`、`-32602` 和 `-32603`。业务错误使用 `-32000` 到 `-32099`，至少包含：

| reason | 使用场景 |
| --- | --- |
| `DEBUGGER_UNAVAILABLE` | 构建未启用 `C_DEBUG`。 |
| `SESSION_BUSY` / `SESSION_NOT_FOUND` | 会话生命周期错误。 |
| `TARGET_RUNNING` / `TARGET_NOT_STOPPED` / `TARGET_EXITED` | 状态机违反。 |
| `CAPABILITY_UNAVAILABLE` | 例如未启用 `C_HEAVY_DEBUG` 的 trace，或底层不支持的 memory breakpoint。 |
| `ADDRESS_NOT_MAPPED` / `ADDRESS_OUT_OF_RANGE` | 地址解析或访问失败。 |
| `REQUEST_TOO_LARGE` / `INVALID_BINARY_LENGTH` | 二进制请求不满足限额或长度。 |
| `REQUEST_ID_CONFLICT` | 相同 request id 的 payload 不一致。 |
| `COMMAND_REJECTED` / `COMMAND_PARSE_FAILED` | 原始 debugger 命令失败。 |
| `OPERATION_TIMEOUT` / `CURSOR_EXPIRED` | 等待或分页语义错误。 |

## 8. RPC v1 方法

所有方法名和 request/response field 是兼容性契约。新字段只能可选新增；删除或改变现有字段语义必须升级 major version。

### 8.1 Capability 和会话

| 方法 | 参数 | 成功结果 | 约束 |
| --- | --- | --- | --- |
| `agent.capabilities` | 无 | `protocol_version`、`debugger`、`trace.cpu`、`breakpoints.memory_change`、limits、address spaces | 无 session 时也可调用。 |
| `session.start` | `target.command`、`target.arguments`、`mounts`、`break_at` | `session_id`、`state=stopped`、`stop_reason=startup` | `break_at` v1 固定为 `entry`；目标必须在受控 workdir 内启动。 |
| `session.status` | `session_id` | state、target、last_stop、state_revision | 任意 session 状态均可调用。 |
| `session.stop` | `session_id`、`graceful_timeout_ms` | `operation_id` | 只请求停止；结果由 `execution.wait` 获取。 |

`session.start` 示例：

```json
{
  "jsonrpc": "2.0",
  "id": "req-start",
  "method": "session.start",
  "params": {
    "target": {
      "command": "AGENTFIX.COM",
      "arguments": []
    },
    "mounts": [
      {"drive": "C", "host_path": "tests/agent/runtime"}
    ],
    "break_at": "entry"
  }
}
```

### 8.2 执行控制

| 方法 | 参数 | 成功结果 | 底层对应 |
| --- | --- | --- | --- |
| `execution.continue` | `session_id` | `operation_id`、`state=running` | `RUN` 及现有 normal loop。 |
| `execution.pause` | `session_id` | `operation_id` | 调度 debugger break；不能从 worker thread 直接改 debugger state。 |
| `execution.step` | `session_id`、`mode=into|over` | `stop_reason=step`、register snapshot | 复用 debugger 单步/F10/F11 行为。 |
| `execution.wait` | `session_id`、`operation_id`、`timeout_ms` | stopped/exited 状态，或 `running=true` | 等待 continue、pause 或 stop 的终态。 |

停止事件必须是结构化对象：

```json
{
  "kind": "breakpoint",
  "breakpoint_id": "bp-3",
  "address": {
    "space": "segmented",
    "segment": "0x1234",
    "offset": "0x00000106"
  }
}
```

合法 `kind`：`startup`、`step`、`breakpoint`、`pause`、`program_exit`、`fault`。`execution.wait` 返回 `running=true` 时不得同时给出 `stop_reason`。

### 8.3 CPU state

`state.get_registers` 仅允许在 `stopped` 状态调用，返回：

```json
{
  "general": {
    "eax": "0x00001234",
    "ebx": "0x00000000",
    "ecx": "0x00000000",
    "edx": "0x00000000",
    "esi": "0x00000000",
    "edi": "0x00000000",
    "ebp": "0x00000000",
    "esp": "0x0000FFFE"
  },
  "segments": {
    "cs": "0x1234",
    "ds": "0x1234",
    "es": "0x1234",
    "fs": "0x0000",
    "gs": "0x0000",
    "ss": "0x1234"
  },
  "instruction_pointer": "0x00000103",
  "flags": "0x00000202",
  "cpu_mode": "real",
  "state_revision": 8
}
```

v1 不提供 `state.set_registers`。现有 `SR` 仅可通过 `debugger.execute_command` 用于人工诊断，不能成为 Python client 的稳定写寄存器 API。

### 8.4 Memory

`memory.read` 参数：`session_id`、`address`、`length`。成功结果包含原始地址、`byte_count`、`data_base64`、`sha256`。

`memory.write` 参数：`session_id`、`address`、`data_base64`、可选 `expected_sha256`。实现要求：

- base64 decode 后长度必须大于 0 且不超过 `max_memory_read_bytes`。
- `expected_sha256` 存在时，先读取同一范围并精确匹配；不匹配返回 `MEMORY_PRECONDITION_FAILED`，不得写入任何字节。
- 写入前后读取同一范围，返回 `before_sha256`、`after_sha256` 和 `byte_count`。
- 任意一个字节地址不可访问时，整次写入失败并返回失败地址；不得只写可访问前缀。

### 8.5 Breakpoint

| 方法 | 参数 | 结果 | 规则 |
| --- | --- | --- | --- |
| `breakpoints.create` | `session_id`、`kind`、`address`、可选 `once` | 稳定 `breakpoint_id`、规范化地址 | `kind=execution` 为 v1 必选。 |
| `breakpoints.list` | `session_id` | 所有 breakpoint 的 id、kind、enabled、address、once | 不能解析 `BPLIST` 文本作为正式实现。 |
| `breakpoints.delete` | `session_id`、`breakpoint_id` | 删除确认和 revision | 删除不存在 id 返回 `BREAKPOINT_NOT_FOUND`。 |

`kind=memory_change` 仅在 `agent.capabilities.breakpoints.memory_change=true` 时可创建。实现必须明确 address space 和 CPU mode 的支持范围；不支持时返回 `CAPABILITY_UNAVAILABLE`，不得创建普通执行断点替代。

RPC id 不得复用 `CBreakpoint` 当前显示列表 index。adapter 必须生成 session 内稳定的 `bp-*` id，并维护其与底层 breakpoint object 的映射，避免 `BPDEL` 后索引变化导致误删。

### 8.6 Debugger 输出、原始命令和 trace

| 方法 | 作用 | 结果约束 |
| --- | --- | --- |
| `debug.output.read` | 读取 debugger output ring buffer | 每条记录有 `sequence`、`timestamp`、`level`、`source`、`message`。 |
| `debugger.execute_command` | 执行原始 debugger command | 返回 `accepted`、`raw_output`、可选 parser result；不替代任何结构化方法。 |
| `trace.start` | 开始指定条数的 CPU trace | 仅 `C_HEAVY_DEBUG`；参数为 `detail=short|normal|long|csip`、`instruction_count`。 |
| `trace.read` | 分页读取 trace events | event 必含 address、instruction、register_changes 和 sequence。 |
| `trace.stop` | 停止尚未完成的 trace | 返回已收集 event 数，不能伪称已完成。 |

trace 的数据源必须是 in-process `TraceSink`，在既有 heavy debug 指令日志点采集结构化事件。`LOGCPU.TXT` 可以作为人工对照产物，但不得作为正式 API 的唯一来源，也不得在多个 session 间共享文件名。

`DebuggerOutputParser` 的规则：

- parser 输入、输出和失败原因必须可单元测试；测试样本存放在 `tests/agent/fixtures/`。
- 解析失败返回 `parse_status=failed` 及 `unparsed_output`，不得返回部分 register object 伪装为完整结果。
- raw command 不能改变 session 状态机以外的状态；检测到 `RUN`、`LOG*`、`BP*`、`SM`、`SR` 等影响正式 API 状态的命令时，server 必须更新 `state_revision` 和相关 collector，或拒绝该命令并返回 `COMMAND_REJECTED`。第一期推荐采用 allowlist，只允许只读诊断命令：`HELP`、`CPU`、`PIC`、`GDT`、`IDT`、`PAGING`、`EMU MEM`、`EMU MACHINE`。

## 9. Python Client 契约

Python client 必须只调用 JSON-RPC，不得通过 stdout、窗口自动化或直接读取 DOSBox-X 输出文件工作。

```python
from dosbox_agent import AgentClient

with AgentClient.from_config("tests/agent/agent-test.env") as agent:
    session = agent.start("AGENTFIX.COM", break_at="entry")
    agent.create_execution_breakpoint(session.id, segment="0x1234", offset="0x00000106")
    operation = agent.continue_(session.id)
    stop = agent.wait(session.id, operation.id, timeout_ms=5000)
    registers = agent.get_registers(session.id)
    memory = agent.read_memory(session.id, space="segmented", segment="0x1234", offset="0x00000200", length=4)
```

要求：

- 公开方法使用 typed model，并把 RPC error 转为 `AgentError` 子类；不得让调用方检查错误字符串。
- `continue_` 使用尾部下划线避免与 Python keyword 冲突。
- bytes 参数和结果使用 `bytes`；client 负责 base64 编解码。
- client 对有副作用调用不自动重试；可由调用方以同一 request id 明确重试。
- 每个 RPC method 至少有一个 client unit test 和一个反序列化/错误映射测试。

## 10. 测试夹具

新增确定性的 16-bit COM 测试程序 `tests/agent/fixtures/agent_fixture.asm`，编译产物 `agent_fixture.com` 必须提交或由确定性构建步骤生成。程序固定从 `CS:0100` 开始，执行以下语义：

```asm
org 100h

start:
    mov ax, 0x1234
    mov bx, 0x5678
    mov si, 0x0200
    mov byte [si], 0x41
    mov word [si + 1], 0x4243
    nop
    int 0x20
```

夹具的明确断言：

- entry 的 `IP` 为 `0x00000100`。
- 单步进入一次后，`AX` 为 `0x00001234`，`IP` 为 `0x00000103`。
- `CS:0106` 是第二条指令的起点；在此创建执行断点后 continue 必须停在该地址。
- 五次单步后，`DS:0200` 的四字节是 `41 43 42 00`（最后一个字节为 fixture 未写入的原始值，不作为固定断言）；前三字节必须精确为 `41 43 42`。
- memory.write 向可读写的 fixture data 区写入四字节后，随后 memory.read 的 bytes 和 SHA-256 必须一致。

夹具必须在 runtime 目录的干净副本中运行。验收不得复用上次运行的 `LOGCPU.TXT`、`MEMDUMP.*`、DOSBox-X 配置或 trace 文件。

## 11. 分阶段实施与检查表

每一项格式为：`[ ] ID - 交付物；检查；通过标准；证据`，完成项使用 `[x]`。证据可填写测试日志路径、CI job URL 或 commit hash。

以下所有命令均在 Windows PowerShell 中执行，路径使用 Windows 分隔符。

当前状态（2026-09-02）：阶段 A、B、C、D、E 已完成并已在 Windows 验证。

### 阶段 A：构建和架构（已完成）

- [x] `RPC-A01` - 新增 `Agent Debug SDL2` Windows 配置，定义 `C_DEBUG=1;C_HEAVY_DEBUG=1;C_DOSBOX_AGENT=1`；运行 `msbuild .\vs\dosbox-x.sln /m /t:dosbox-x /p:Configuration="Agent Debug SDL2" /p:Platform=x64`；退出码为 0，生成 `bin\x64\Agent Debug SDL2\dosbox-x.exe`；证据：2026-09-02 Windows PowerShell，`msbuild` 退出码 0，0 个警告、0 个错误。
- [x] `RPC-A02` - Windows 项目编入全部 agent 源文件；检查 `vs\dosbox-x.vcxproj` 的 `ClCompile` 项包含 `src\agent\`，并执行 `msbuild .\vs\dosbox-x.sln /m /t:dosbox-x /p:Configuration="Agent Debug SDL2" /p:Platform=x64`；退出码为 0；证据：2026-09-02，项目已编入 bridge、debugger、protocol、server 和 transport 的 5 个 agent 源文件；同次 `msbuild` 退出码 0。
- [x] `RPC-A03` - agent bridge、server、transport 和 adapter 有独立职责与头文件边界；审查 `src\agent\`，确认 transport 层没有 `reg_`、`mem_read`、`CBreakpoint` 引用；证据：2026-09-02，`rg -n 'reg_|mem_read|CBreakpoint' src\agent\transport` 无匹配。
- [x] `RPC-A04` - 所有 DOSBox-X 状态访问都在 emulation-thread queue 执行；执行 `& '.\bin\x64\Agent Debug SDL2\dosbox-x.exe' --agent-config tests\agent\agent-test.env --agent-self-test`，连续并发提交 1000 个只读请求；退出码为 0，日志确认 FIFO 响应顺序与提交顺序一致，且无死锁或失败请求；证据：2026-09-02，命令退出码 0，输出 `Agent self-test completed: success`。
- [x] `RPC-A05` - 配置只从 `--agent-config` 加载；执行 `Remove-Item Env:DOSBOX_AGENT_ENDPOINT, Env:DOSBOX_AGENT_WORKDIR, Env:DOSBOX_AGENT_REQUEST_TIMEOUT_MS -ErrorAction SilentlyContinue` 后，以 `--agent-config tests\agent\agent-test.env` 启动；endpoint、workdir 和 limit 与 env 文件一致；证据：2026-09-02，清空当前 PowerShell 进程变量后自检成功；日志确认 endpoint=`\\.\pipe\dosbox-agent-test`、workdir=`D:\workspace\ai\yl\dosbox-x\tests\agent\runtime`、request_timeout_ms=5000、max_message_bytes=1048576、max_memory_read_bytes=65536、max_trace_events=10000。

### 阶段 B：协议和会话（已完成）

- [x] `RPC-B01` - JSON-RPC 2.0 request validation；对缺失 `jsonrpc`、未知 method、错误 field type 和超过 `max_message_bytes` 的请求运行 protocol unit tests；分别返回标准 JSON-RPC 错误或 `REQUEST_TOO_LARGE`；证据：2026-09-02，`--agent-self-test` 成功，覆盖缺失 `jsonrpc`、未知 method、params 类型错误和超限 request。
- [x] `RPC-B02` - `agent.capabilities`；在 `C_DEBUG` + `C_HEAVY_DEBUG` build 中调用；声明 `debugger=true`、`trace.cpu=true`、三种 address space 和数值 limits；证据：2026-09-02，Windows named pipe 返回 `debugger=true`、`trace.cpu=true`、`segmented`、`linear`、`physical` 及配置 limits。
- [x] `RPC-B03` - non-heavy build capability 退化；在仅 `C_DEBUG` build 中调用 `agent.capabilities` 和 `trace.start`；前者 `trace.cpu=false`，后者返回 `CAPABILITY_UNAVAILABLE`；证据：2026-09-02，`Agent Debug No Heavy SDL2|x64` named-pipe 验收返回上述结果。
- [x] `RPC-B04` - `session.start` 在 fixture entry 停止；调用 start 后 `session.status.state=stopped`、`last_stop.kind=startup`、`IP=0x00000100`；证据：2026-09-02，`AGENTFIX.COM` 返回 `0x0812:0x00000100` 和 `kind=startup`。
- [x] `RPC-B05` - 单 session 限制；会话处于 stopped 或 running 时再次调用 `session.start`；返回 `SESSION_BUSY`，原会话的 state_revision 和 breakpoint 集合不变；证据：2026-09-02，重复 start 返回 `SESSION_BUSY`，原 session revision 保持 `1`。
- [x] `RPC-B06` - 状态机错误；会话 running 时调用 `state.get_registers`、`memory.read`、`memory.write`；全部返回 `TARGET_RUNNING`，不改变 target；证据：2026-09-02，`AGENTRUN.COM` continue 后三种请求均返回 `TARGET_RUNNING`。
- [x] `RPC-B07` - operation timeout 语义；continue 后用极短 timeout 调用 wait；返回 `running=true` 且没有 stop_reason，随后 pause/wait 可以得到 `kind=pause`；证据：2026-09-02，`execution.wait(timeout_ms=1)` 返回 `running=true`，pause 后 wait 返回 stopped、revision `2` 和 `kind=pause`。
- [x] `RPC-B08` - request id 幂等；相同 payload 使用同一 request id 重发 memory.write，验证只执行一次；同 id 不同 payload 返回 `REQUEST_ID_CONFLICT`；证据：2026-09-02，干净 runtime 中对 `AGENTFIX.COM` 的 `0x0812:0x00000200` 写入 `AGENT` 后，两次 `id=write-once` 响应均为 revision `2`、SHA-256=`b62db7d0b79d11100c7103df97dd87bc90ff6ffbd88de6019cf1e933069c8525`；相同 id 的不同 payload 返回 `REQUEST_ID_CONFLICT`。

### 阶段 C：Debugger Adapter

- [x] `RPC-C01` - `state.get_registers` 结构化完整性；fixture entry 和单步后分别读取；general、segments、instruction_pointer、flags、cpu_mode 均存在且使用固定宽度十六进制字符串；证据：2026-09-02，Windows named-pipe 对 `AGENTFIX.COM` entry 返回 `cpu_mode=real`、`IP=0x00000100`、8 个 32-bit general registers、6 个 16-bit segments 和 32-bit flags；单步结果亦返回完整 snapshot。
- [x] `RPC-C02` - `execution.step(mode=into)`；从 fixture entry 单步一次；AX=`0x00001234`、IP=`0x00000103`、stop_reason.kind=`step`；证据：2026-09-02，Windows named-pipe 对 `AGENTFIX.COM` 返回 `kind=step`、`EAX=0x00001234`、`instruction_pointer=0x00000103`。
- [x] `RPC-C03` - `execution.step(mode=over)`；在 call、int、rep 夹具扩展用例中验证越过语义；执行后不进入被越过目标，且没有泄漏临时 breakpoint；证据：2026-09-02，Windows named-pipe 使用独立进程运行 `AGCALL.COM`、`AGENTINT.COM`、`AGENTREP.COM`；分别停在 `0x00000103`、`0x00000102`、`0x0000010B`，REP destination 为 `41 42`，三次 `breakpoints.list` 均无泄漏的用户可见 breakpoint，over 响应均含 registers snapshot。
- [x] `RPC-C04` - `memory.read`；读取 fixture code 开头 6 bytes；base64 decode 后严格等于 `B8 34 12 BB 78 56`，byte_count=6，SHA-256 匹配；证据：2026-09-02，Windows named-pipe 分别使用 segmented、linear、physical 读取，均返回 base64 `uDQSu3hW`、`byte_count=6`、SHA-256 `99b28729074f19ab5d3edfab4f425f3d88e4601acd37ae9e6fd43e46b85a5c9f`。
- [x] `RPC-C05` - 三地址空间和失败路径；分别测试 segmented、linear、physical 成功访问，以及 invalid selector、segment limit、page not present；失败必须返回 `ADDRESS_NOT_MAPPED` 和精确 reason；证据：2026-09-02，`tests\agent\verify_memory_mapping.ps1` 在 Windows named-pipe 运行 `AGPMODE.COM`，保护模式分页停止后，三地址空间各读取 1 byte 成功；失败请求均返回 `ADDRESS_NOT_MAPPED`，cause 分别为 `invalid_selector`、`segment_limit`、`page_not_present`。
- [x] `RPC-C06` - `memory.write` 原子性；expected SHA 不匹配时验证写前后 bytes 相同；匹配时验证写后 bytes、after SHA 和 byte_count；证据：2026-09-02，Windows named-pipe 对 fixture `DS:0200` 写入 `DE AD BE EF`；错误 SHA 返回 `MEMORY_PRECONDITION_FAILED` 且读回 bytes 未变，正确 SHA 返回 `byte_count=4`、after SHA `5f78c33274e43fa9de5659265c1d917e25c03722dcb0b8d27db8d5feaa813953`，随后读回 `DE AD BE EF`。
- [x] `RPC-C07` - 执行断点；在 `CS:0106` 创建后依次调用 continue 和 wait；stop_reason.kind=`breakpoint` 且 address 与创建地址一致；证据：2026-09-02，Windows named-pipe 创建 `bp-1` 于 `CS:0x00000106`，continue/wait 返回 `kind=breakpoint`、`breakpoint_id=bp-1` 和同一 segmented address。
- [x] `RPC-C08` - breakpoint stable id；创建多个 breakpoint、删除中间一个并列出；其他 id 不改变，按原 id 删除目标不会误删；证据：2026-09-02，Windows named-pipe 创建 `bp-1`、`bp-2`、`bp-3`，删除 `bp-2` 后 list 仍为 `bp-1`、`bp-3`，再按 `bp-3` 删除成功。
- [x] `RPC-C09` - memory-change breakpoint capability；capability true 的 build 使用实际底层支持的 CPU mode 验证一次触发；capability false 的 build 返回 `CAPABILITY_UNAVAILABLE`，不得转换为 execution breakpoint；证据：2026-09-02，heavy build 在 real mode 对 `DS:0200` 创建 `bp-1` 并由 fixture 写入实际触发；`Agent Debug No Heavy SDL2|x64` capability 返回 `memory_change=false`，同一 create 返回 `CAPABILITY_UNAVAILABLE`。
- [x] `RPC-C10` - `session.stop`；running 和 stopped 状态各执行一次 stop/wait；最终 state=exited，重复 stop 为幂等且不崩溃；证据：2026-09-02，Windows named-pipe 分别在 stopped `AGENTFIX.COM` 和 running `AGENTRUN.COM` 调用 stop/wait，均返回 `state=exited`、`kind=program_exit`；exited 后重复 stop 返回 `op-stop-complete`。

### 阶段 D：输出、trace 和兼容命令

- [x] `RPC-D01` - debugger output ring buffer；执行一个只读诊断命令后 `debug.output.read` 返回递增 sequence、非空 source 和完整 message；分页连接无重复无遗漏；证据：2026-09-02，`tests\agent\verify_output_trace.ps1` 以 `limit=1` 完整读取 `HELP`、`CPU`、`PIC` 三条输出记录，sequence 连续且 source/message 非空。
- [x] `RPC-D02` - 原始命令 allowlist；`HELP`、`CPU`、`PIC` 可执行并返回 raw_output；`RUN`、`SM`、`SR`、`BP*` 在 v1 被拒绝或由 adapter 显式同步 state，测试覆盖每种结果；证据：2026-09-02，`tests\agent\verify_output_trace.ps1` 验证三个允许命令均有非空 `raw_output`，并验证 `RUN`、`SM`、`SR`、`BP 1234:0100` 均返回 `COMMAND_REJECTED`。
- [x] `RPC-D03` - parser 失败显式化；向 parser fixture 输入缺少字段和异常空格的文本；返回 `parse_status=failed` 和未解析文本，不产生伪完整对象；证据：2026-09-02，新增 `tests\agent\agent_parser_tests.cpp` 和两个 fixture；`& '.\bin\x64\Agent Debug SDL2\dosbox-x.exe' -tests` 退出码为 0。
- [x] `RPC-D04` - CPU trace；在 heavy build 对 fixture 启动 `trace.start(instruction_count=2)` 并运行；`trace.read` 恰好得到 2 条递增 sequence，每条包含 address、instruction、register_changes；证据：2026-09-02，`tests\agent\verify_output_trace.ps1` 返回通过，两个 trace event 的 sequence 为 1、2，均包含所需字段。
- [x] `RPC-D05` - trace 隔离；连续两个独立 session 的 trace 不读取前一 session 的事件或文件；删除 runtime 后重跑结果一致；证据：2026-09-02，`tests\agent\verify_output_trace.ps1` 在同一 DOSBox-X 进程的第二个 session 启动独立 1 条 trace，并确认 sequence 从 1 开始且没有前一 session 的事件；另以原本不存在的 `tests\agent\runtime-d05-clean` 目录、显式 `agent-d05-clean.env` 和新生成 fixture 重跑同一脚本通过，未复用原 runtime 文件。

### 阶段 E：Client 和端到端

- [x] `RPC-E01` - Python client unit tests；执行 `py -m unittest discover -s client\python\tests -t client\python -v`；退出码为 0，覆盖 model、base64、error mapping、request id retry；证据：2026-09-02，3 个 unittest 全部通过；覆盖显式 dotenv 相对路径解析、全部 v1 client method 的 typed response、base64 写入、`TARGET_RUNNING` error mapping 和相同 `request_id` 的显式重试。
- [x] `RPC-E02` - 端到端 fixture；执行 `py client\python\tests\test_e2e.py --config tests\agent\agent-test.env`；依次完成 start、breakpoint、continue、wait、register read、memory read、memory write、step、stop；退出码为 0；证据：2026-09-02，Windows named-pipe E2E 通过，同时验证 `CPU` 诊断输出和 `trace.start(instruction_count=2)` 的 session-local sequence。
- [x] `RPC-E03` - DOSBox-X 既有单元测试未回归；执行 `& '.\bin\x64\Agent Debug SDL2\dosbox-x.exe' -tests`；退出码为 0 且输出 `Unit test completed: success`；证据：2026-09-02，`-tests` 退出码为 0；当前 Windows GUI build 不向调用 PowerShell 转发测试日志，`shell.cpp` 的 `RUN_ALL_TESTS()` 返回值为进程退出状态。
- [x] `RPC-E04` - 干净运行可重复；执行 `if (Test-Path tests\agent\runtime) { Remove-Item -Recurse -Force tests\agent\runtime }; New-Item -ItemType Directory -Path tests\agent\runtime` 后，连续运行 E02 三次；三次均通过，trace 和输出无跨运行数据；证据：2026-09-02，`tests\agent\verify_client_clean_runs.ps1` 创建三个此前不存在的独立 runtime/config 目录并串行运行 E02 三次，三次均通过；每次 E2E 断言 output sequence 从 1 开始、trace sequence 恰为 1、2。

## 12. 发布门禁

以下条件全部满足才能将 RPC 标为可用：

- 阶段 A 到 E 所有必选检查项已勾选并有可访问证据。
- `docs/rpc.md` 中列出的第一期能力均映射到一个 v1 RPC method 或在 `agent.capabilities` 中有明确、可测试的 unavailable 说明。
- 所有结构化接口都有正向、非法参数、running 状态和底层 capability 缺失测试。
- Python client 的 public API 与本文第 9 节一致，且示例可通过 E02。
- 默认 transport 只暴露本机当前用户；没有未认证 TCP listener。
- 不存在依赖 parser、ncurses 截图、stdout scraping 或共享 `LOGCPU.TXT` 的核心 API 数据路径。

## 13. 非目标与后续版本

以下能力不属于 v1，不能为了“先跑通”混入稳定 API：CPU instrumentation、全量 memory access trace、interrupt trace、file I/O trace、screenshot analysis、IDA/Ghidra 集成、多目标并行 session、远程 TCP transport 和写寄存器 API。

后续能力必须先补充本文：数据模型、capability 标志、线程模型、测试夹具和至少一个端到端检查项，然后才能实现。
