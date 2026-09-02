开发一个 DOSBox-X Debugger Agent Interface，将 DOSBox-X 现有 Debugger 能力封装成 Agent 可调用接口。

面向 AI Agent 的实际使用流程请先阅读：[DOSBox-X Debugger Agent 使用手册](rpc-agent-usage.md)。

协议契约与验收规范分别见：[rpc-development.md](rpc-development.md)。

目标：

让 AI Agent 能够通过结构化 API 控制 DOSBox-X 调试环境，实现：

\-  启动/停止 DOS 程序 

\-  控制程序运行 

\-  单步执行 

\-  获取 CPU 状态 

\-  读取/修改内存 

\-  设置/管理断点 

\-  获取调试输出 

\-  获取执行日志 

用于：

\-  DOS 程序逆向分析 

\-  老游戏行为分析 

\-  二进制重构辅助 

\-  动态验证静态分析结果 

不要重新实现：

\-  CPU 调试逻辑 

\-  内存访问逻辑 

\-  断点机制 

\-  单步机制 

利用 DOSBox-X 已有能力。

目标：

```

Agent

&#x20;|

RPC Interface

&#x20;|

Debugger Adapter

&#x20;|

DOSBox-X Debugger

&#x20;|

DOSBox-X Emulator

&#x20;|

DOS Program

```

第一阶段：

不要修改：

\-  CPU execution core 

\-  memory subsystem 

\-  DOS kernel simulation 

除非现有 debugger 能力不足。

不要简单暴露：

```

输入命令

返回文本

```

需要提供：

结构化接口：

例如：

```json

{

&#x20;"registers":{

&#x20;  "ax":123,

&#x20;  "cs":"1000",

&#x20;  "ip":"0020"

&#x20;}

}

```

而不是：

```ini

AX=0000 BX=0001...

```

```markdown



&#x20;                AI Agent



&#x20;                   |

&#x20;                   |



&#x20;         Agent API / RPC Layer



&#x20;                   |

&#x20;                   |



&#x20;         DOSBox Debugger Adapter



&#x20;                   |

&#x20;                   |



&#x20;         DOSBox-X Debugger



&#x20;                   |

&#x20;                   |



&#x20;            DOSBox-X Runtime



&#x20;                   |

&#x20;                   |



&#x20;             DOS Executable



```

负责：

\-  启动 DOSBox-X 

\-  管理调试会话 

\-  维护连接状态 

\-  发送 debugger command 

\-  获取 debugger response 

抽象接口：

```scss

start()



stop()



execute(command)



status()



```

要求：


\-  不绑定具体通信方式 

提供 Agent 调用入口。

要求：

支持：

\-  JSON RPC 

\-  或等价结构化接口 

API 设计保持稳定。

提供：

功能：

\-  加载 DOS 环境 

\-  启动目标程序 

继续运行程序。

对应 debugger：

```

G

```

单步执行。

对应：

```

T

```

暂停执行。

提供：

\-  通用寄存器 

\-  段寄存器 

\-  IP 

\-  FLAGS 

例如：

请求：

```

get\_registers

```

返回：

```json

{

&#x20;"ax": "...",

&#x20;"bx": "...",

&#x20;"cs": "...",

&#x20;"ip": "...",

&#x20;"flags": "..."

}

```

提供：

能力：

\-  指定地址读取 

\-  返回二进制数据 

例如：

```json

{

&#x20;"address":"segment:offset",

&#x20;"length":128

}

```

能力：

修改目标内存。

提供：

支持：

\-  地址断点 

\-  内存访问断点（如果 debugger 支持） 

保留底层能力。

提供：

```

execute\_debugger\_command

```

例如：

Agent 可以执行：

```

CPU

MEMDUMP

HELP

```

用途：

\-  调试 

\-  新功能验证 

\-  后续扩展 

利用 DOSBox-X 已有日志能力。

提供：

例如：

```

trace.start

```

返回结构化数据：

```json

\[

&#x20;{

&#x20;  "address":"",

&#x20;  "instruction":"",

&#x20;  "register\_change":{}

&#x20;}

]

```

如果当前 debugger 输出无法完全解析：

设计独立 parser 模块。

不要让 Agent 直接依赖 DOSBox-X 文本输出。

增加：

```

Debugger Output Parser

```

负责：

文本：

```yaml

AX=xxxx

CS:IP=xxxx

```

转换：

结构：

```json

{

&#x20;"ax":"",

&#x20;"cs":"",

&#x20;"ip":""

}

```

```

dosbox-agent-interface



├── server

│

├── debugger

│   ├── session

│   ├── command

│   └── parser

│

├── api

│

├── client

│

├── tests

│

└── docs



```

目标：

成功控制 DOSBox-X debugger。

实现：

\-  session管理 

\-  command发送 

\-  输出读取 

\-  基础解析 

实现：

\-  run 

\-  step 

\-  continue 

\-  registers 

\-  memory 

\-  breakpoint 

实现：

\-  debugger log采集 

\-  trace解析 

\-  结构化输出 

提供：

Python client：

让 Agent 可以：

```javascript

debugger.run()



debugger.get\_registers()



debugger.step()



debugger.read\_memory()

```

保持接口可扩展：

未来可以加入：

\-  CPU instrumentation 

\-  memory access tracing 

\-  interrupt tracing 

\-  file IO tracing 

\-  screenshot analysis 

\-  IDA/Ghidra integration 

但不要在当前项目阶段实现。

完成后，一个 Agent 可以：

1\.  启动 DOS 程序 

2\.  连接 debugger 

3\.  设置断点 

4\.  运行到断点 

5\.  获取寄存器状态 

6\.  读取内存 

7\.  单步执行 

8\.  收集执行日志 

9\.  根据结构化数据进行分析 

最终目标：

把 DOSBox-X Debugger 转换为一个 AI Agent 可以调用的动态分析后端，而不是重新开发一个调试器。

