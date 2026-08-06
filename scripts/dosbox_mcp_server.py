#!/usr/bin/env python3
"""
Model Context Protocol (MCP) Server Bridge for DOSBox-X Agent Subsystem (A-TRES)
Copyright (C) 2026 Michael P. Burgus (https://github.com/NeuralDrifter)

Exposes DOSBox control and terminal streaming functions to LLMs (AGY, Claude, Codex).
"""

import sys
import os
import json
import urllib.request
import urllib.error

DOSBOX_AGENT_PORT = os.environ.get("DOSBOX_AGENT_PORT", "8090")
DOSBOX_AGENT_URL = f"http://127.0.0.1:{DOSBOX_AGENT_PORT}"
AUTH_TOKEN = os.environ.get("DOSBOX_AGENT_TOKEN", "dosbox-agent-secret")

def send_rpc_request(method, params=None):
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params or {},
        "id": 1,
        "token": AUTH_TOKEN
    }
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(
        DOSBOX_AGENT_URL, 
        data=data, 
        headers={"Content-Type": "application/json", "Authorization": f"Bearer {AUTH_TOKEN}"}
    )
    
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            res_body = response.read().decode('utf-8')
            return json.loads(res_body)
    except urllib.error.URLError as e:
        return {"error": f"Failed to connect to DOSBox-X A-TRES agent bridge: {e}"}

def handle_initialize(req_id):
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "dosbox-x-agent", "version": "1.0.0"}
        }
    }

def handle_tools_list(req_id):
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "tools": [
                {
                    "name": "dosbox_get_status",
                    "description": "Check DOSBox-X execution status, active command state, and exit code",
                    "inputSchema": {"type": "object", "properties": {}}
                },
                {
                    "name": "dosbox_read_output",
                    "description": "Read and flush the latest live output text/terminal stream from DOSBox-X",
                    "inputSchema": {"type": "object", "properties": {}}
                },
                {
                    "name": "dosbox_run_command",
                    "description": "Execute a DOS command inside DOSBox-X shell",
                    "inputSchema": {
                        "type": "object",
                        "properties": {
                            "command": {"type": "string", "description": "DOS command"}
                        },
                        "required": ["command"]
                    }
                }
            ]
        }
    }

def handle_tools_call(req_id, params):
    tool_name = params.get("name")
    tool_args = params.get("arguments", {})

    if tool_name == "dosbox_get_status":
        rpc_res = send_rpc_request("get_status")
    elif tool_name == "dosbox_read_output":
        rpc_res = send_rpc_request("read_output")
    elif tool_name == "dosbox_run_command":
        cmd = tool_args.get("command", "")
        rpc_res = send_rpc_request("run_command", {"command": cmd})
    else:
        rpc_res = {"error": f"Unknown tool: {tool_name}"}

    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "content": [{"type": "text", "text": json.dumps(rpc_res, indent=2)}]
        }
    }

def main():
    print("==========================================================", file=sys.stderr)
    print("DOSBox-X Agent Subsystem (A-TRES) MCP Bridge Initialized", file=sys.stderr)
    print("Listening for MCP JSON-RPC requests on stdio...", file=sys.stderr)
    print("==========================================================", file=sys.stderr)

    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            
            req = json.loads(line)
            method = req.get("method")
            req_id = req.get("id")

            if method == "initialize":
                res = handle_initialize(req_id)
            elif method == "tools/list":
                res = handle_tools_list(req_id)
            elif method == "tools/call":
                res = handle_tools_call(req_id, req.get("params", {}))
            else:
                res = {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32601, "message": "Method not found"}}

            sys.stdout.write(json.dumps(res) + "\n")
            sys.stdout.flush()

        except Exception as e:
            sys.stderr.write(f"Error handling MCP frame: {e}\n")
            sys.stderr.flush()

if __name__ == "__main__":
    main()
