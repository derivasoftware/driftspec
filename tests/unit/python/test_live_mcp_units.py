"""driftspec.live_mcp, unit by unit: the client side and the MCP tools, against a session served on a
free port."""

import io
import json

import pytest

from driftspec.live_mcp import answer, call, called, initialize, mcp, open_workspace, reply, state, tool
from live_fixtures import free_port, no_browser, session, ws  # noqa: F401 (fixtures)

__covers__ = (call, open_workspace, state, tool, initialize, called, reply, answer, mcp)


class TestTool:
    def test_every_tool_answers_through_the_sessions_routes(self, session, no_browser):
        port = session.port
        assert "live viewer at" in tool(port, "live_open", {"workspace": session.workspace}, {})
        assert no_browser == [f"http://localhost:{port}/"]
        assert tool(port, "live_focus", {"id": "x"}, {})["sent"] == "x"
        assert tool(port, "live_note", {"id": "x", "text": "t"}, {}) == {"notes": 1}
        assert tool(port, "live_highlight", {"ids": ["x", "y"]}, {}) == {"highlights": 2}
        assert tool(port, "live_clear", {}, {}) == {}
        assert tool(port, "live_comments", {"peek": True}, {}) == {"comments": []}
        assert tool(port, "live_isolate", {"ids": ["x"]}, {}) == {"isolated": ["x"]}
        assert tool(port, "live_selection", {}, {}) == {}
        assert tool(port, "live_state", {}, {})["version"] == 1
        with pytest.raises(KeyError):
            tool(port, "live_nothing", {}, {})

    def test_open_refuses_a_port_that_serves_another_workspace(self, session, tmp_path):
        assert tool(session.port, "live_open", {"workspace": str(tmp_path)}, {}).startswith("error: port")

    def test_open_starts_a_session_when_none_serves_the_port(self, ws, no_browser, free_port):
        port = free_port
        opened = {}
        assert "live viewer at" in tool(port, "live_open", {"workspace": ws}, opened)
        try:
            assert opened["live"].port == port and no_browser == [f"http://localhost:{port}/"]
            assert call(port, "/state")["version"] == 1
        finally:
            opened["live"].stop()


class TestMcp:
    def test_it_lists_and_calls_the_tools_over_stdio(self, session):
        lines = [
            {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-01-01"}},
            {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
            {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "live_state"}},
            {"jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": {"name": "live_note", "arguments": {"id": "x", "text": "t"}}},
            {"jsonrpc": "2.0", "id": 5, "method": "tools/call", "params": {"name": "live_nothing"}},
            {"jsonrpc": "2.0", "method": "notifications/initialized"},
            {"jsonrpc": "2.0", "id": 6, "method": "ping"},
        ]
        out = io.StringIO()
        mcp(session.port, io.StringIO("not json\n" + "".join(json.dumps(m) + "\n" for m in lines)), out)
        replies = {r["id"]: r for r in map(json.loads, out.getvalue().splitlines())}
        assert sorted(replies) == [1, 2, 3, 4, 5, 6]
        assert replies[1]["result"]["protocolVersion"] == "2025-01-01"
        assert {t["name"] for t in replies[2]["result"]["tools"]} >= {"live_open", "live_state", "live_comments"}
        assert json.loads(replies[3]["result"]["content"][0]["text"])["version"] == 1
        assert json.loads(replies[4]["result"]["content"][0]["text"]) == {"notes": 1} and session.notes[0]["id"] == "x"
        assert replies[5]["error"]["message"] == "no tool live_nothing" and replies[6]["result"] == {}

    def test_a_tool_that_fails_answers_an_error_result(self, free_port):
        out = io.StringIO()
        mcp(free_port, io.StringIO(json.dumps({"id": 1, "method": "tools/call", "params": {"name": "live_state"}}) + "\n"), out)
        reply = json.loads(out.getvalue())
        assert reply["result"]["isError"] and reply["result"]["content"][0]["text"].startswith("error: ")
