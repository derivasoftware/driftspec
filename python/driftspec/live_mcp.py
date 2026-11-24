"""The client side of the live session, the one who writes: its routes called from this machine, and
the same contract as MCP tools over stdio, for an agent."""

from __future__ import annotations

import json
import os
import sys
import urllib.request

from driftspec.live_contract import CONTRACT, LIVE, Operation, Tool
from driftspec.live_http import browser_open, serve


def call(port: int, path: str, data: dict | None = None) -> dict:
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=None if data is None else json.dumps(data).encode(),
                                 method="GET" if data is None else "POST", headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read() or b"{}")


# ---------------------------------------------------------------- MCP over stdio: the same contract, for an agent
# live_open is this door's own: it starts a session rather than asking one. Every other tool is an
# operation of the contract, read from it.
OPEN = Tool("live_open", "Open a DriftSpec workspace in the live viewer (starts the server if needed) and open the browser.",
            {"workspace": {"type": "string"}}, ("workspace",), 0)

TOOLS = [OPEN.schema()] + [op.tool.schema() for op in LIVE.tools()]


def open_workspace(port: int, args: dict, opened: dict) -> str:
    """live_open: the browser taken to the session on the port, or a session started there for the workspace."""
    workspace = os.path.realpath(args["workspace"])
    try:
        st = call(port, "/state")
        if st.get("workspace") != workspace:
            return f"error: port {port} serves {st.get('workspace')}"
        browser_open(f"http://localhost:{port}/")
    except OSError:
        opened["live"] = serve(workspace, port, open_browser=True, block=False)
    return f"live viewer at http://localhost:{port}/ for {workspace}"


def state(port: int) -> dict:
    """live_state: the session's state, its first 30 findings only."""
    return asked(port, LIVE_STATE)


def asked(port: int, op: Operation, args: dict | None = None) -> object:
    """An operation of the contract asked of the session on the port, with the body the tool's arguments make."""
    return op.tool.after(call(port, op.path, op.tool.body(args or {})))


def answering(op: Operation):
    """The call of a tool: its operation asked of the session on the port with the tool's arguments."""
    return lambda port, a, _: asked(port, op, a)


LIVE_STATE = next(op for op in LIVE.tools() if op.tool.name == "live_state")

# Each tool, answered through the session's routes: port, arguments and what the call opened.
CALLS = {"live_open": open_workspace} | {op.tool.name: answering(op) for op in LIVE.tools()}


def tool(port: int, name: str, args: dict, opened: dict) -> object:
    """One MCP tool call, answered through the session's routes; a tool there is not raises KeyError."""
    if name not in CALLS:
        raise KeyError(name)
    return CALLS[name](port, args, opened)


def initialize(port: int, msg: dict, opened: dict) -> dict:
    return {"protocolVersion": msg.get("params", {}).get("protocolVersion", "2024-11-05"), "capabilities": {"tools": {}},
            "serverInfo": {"name": "driftspec-live", "version": str(CONTRACT["version"])}}


def called(port: int, msg: dict, opened: dict) -> dict:
    """tools/call: the tool's answer as text, or an error result when it fails; an unknown tool raises KeyError."""
    name, args = msg["params"]["name"], msg["params"].get("arguments", {})
    try:
        out = tool(port, name, args, opened)
    except KeyError:
        raise
    except Exception as exc:
        return {"content": [{"type": "text", "text": f"error: {exc}"}], "isError": True}
    return {"content": [{"type": "text", "text": out if isinstance(out, str) else json.dumps(out, indent=1)}]}


# What each JSON-RPC method answers; any other method with an id answers {}.
METHODS = {
    "initialize": initialize,
    "tools/list": lambda port, msg, opened: {"tools": TOOLS},
    "tools/call": called,
}


def reply(stdout, i, result=None, error=None) -> None:
    msg = {"jsonrpc": "2.0", "id": i}
    msg["result" if error is None else "error"] = result if error is None else {"code": -32000, "message": error}
    stdout.write(json.dumps(msg) + "\n")
    stdout.flush()


def answer(port: int, msg: dict, opened: dict, stdout) -> None:
    """One JSON-RPC message answered on stdout; a notification (no id) of an unknown method is not."""
    method, i = msg.get("method"), msg.get("id")
    if method in METHODS:
        try:
            reply(stdout, i, METHODS[method](port, msg, opened))
        except KeyError:
            reply(stdout, i, error=f"no tool {msg['params']['name']}")
    elif i is not None:
        reply(stdout, i, {})


def mcp(port: int, stdin=None, stdout=None) -> None:
    stdin, stdout = stdin or sys.stdin, stdout or sys.stdout
    opened: dict = {}
    for line in stdin:
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        answer(port, msg, opened, stdout)
