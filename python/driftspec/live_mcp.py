"""The client side of the live session, the one who writes: its routes called from this machine, and
the same contract as MCP tools over stdio, for an agent."""

from __future__ import annotations

import json
import os
import sys
import urllib.request

from driftspec.live_http import browser_open, serve
from driftspec.live_session import CONTRACT


def call(port: int, path: str, data: dict | None = None) -> dict:
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=None if data is None else json.dumps(data).encode(),
                                 method="GET" if data is None else "POST", headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read() or b"{}")


# ---------------------------------------------------------------- MCP over stdio: the same contract, for an agent
TOOLS = [
    {"name": "live_open", "description": "Open a DriftSpec workspace in the live viewer (starts the server if needed) and open the browser.",
     "inputSchema": {"type": "object", "properties": {"workspace": {"type": "string"}}, "required": ["workspace"]}},
    {"name": "live_focus", "description": "Take the reader to an identity (a box, class, member, interface, requirement or sequence) in the live viewer: it is selected and framed.",
     "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "also": {"type": "array", "items": {"type": "string"}}}, "required": ["id"]}},
    {"name": "live_note", "description": "Put a speech bubble on an identity in the live viewer; the same identity replaces its bubble, empty text removes it. tone: accent, info, ok, bad, req.",
     "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "text": {"type": "string"}, "tone": {"type": "string"}}, "required": ["id", "text"]}},
    {"name": "live_highlight", "description": "Highlight identities of any perspective in the live viewer with a tone; replaces the previous highlight unless add is true.",
     "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}, "tone": {"type": "string"}, "add": {"type": "boolean"}}, "required": ["ids"]}},
    {"name": "live_clear", "description": "Clear the notes, the highlights or both (what: notes, highlights, all) in the live viewer.",
     "inputSchema": {"type": "object", "properties": {"what": {"type": "string"}}}},
    {"name": "live_comments", "description": "What the reader wrote for the agent in the live viewer since the last call: each comment with the identity it is on; they are marked read unless peek is true. Answer with live_note on the same identity.",
     "inputSchema": {"type": "object", "properties": {"peek": {"type": "boolean"}}}},
    {"name": "live_isolate", "description": "Show only these boxes in the live viewer, with what they use from outside as ghosts; an empty list shows everything.",
     "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}}, "required": ["ids"]}},
    {"name": "live_selection", "description": "What the reader has selected in the live viewer.", "inputSchema": {"type": "object", "properties": {}}},
    {"name": "live_state", "description": "The live session: its version and the findings.", "inputSchema": {"type": "object", "properties": {}}},
]


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
    st = call(port, "/state")
    st["findings"] = st["findings"][:30]
    return st


# Each tool, answered through the session's routes: port, arguments and what the call opened.
CALLS = {
    "live_open": open_workspace,
    "live_focus": lambda port, a, _: call(port, "/focus", {"id": a["id"], "also": a.get("also", [])}),
    "live_note": lambda port, a, _: call(port, "/note", {"id": a["id"], "text": a["text"], "tone": a.get("tone", "accent")}),
    "live_highlight": lambda port, a, _: call(port, "/highlight", {"ids": a["ids"], "tone": a.get("tone", "accent"),
                                                                   "add": a.get("add", False)}),
    "live_clear": lambda port, a, _: call(port, "/clear", {"what": a.get("what", "all")}),
    "live_comments": lambda port, a, _: call(port, "/unread", {"mark": not a.get("peek", False)}),
    "live_isolate": lambda port, a, _: call(port, "/isolate", {"ids": a.get("ids", [])}),
    "live_selection": lambda port, a, _: call(port, "/selection"),
    "live_state": lambda port, a, _: state(port),
}


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
