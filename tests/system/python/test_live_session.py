"""SREQ-00017-1 from outside: an agent over MCP and a reader in the page share one live session."""

import io
import json
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from live_helpers import Reader, agent, get, post, workspace  # noqa: E402

import pyds.live  # noqa: E402


@pytest.mark.crosses_boundary  # the session's contract is HTTP on the machine itself: the system is that server
class TestLiveSessionFromOutside:
    def test_the_agent_points_and_annotates_and_the_reader_sees_it_and_answers(self, tmp_path):
        live = pyds.live.serve(workspace(str(tmp_path)), 0, block=False)
        try:
            reader = Reader(live)
            reader.next("model")
            replies = agent(live.port,
                            ("live_note", {"id": "sistema@core::Broker", "text": "the data comes in here", "tone": "info"}),
                            ("live_highlight", {"ids": ["sistema@core::Broker::publish"]}),
                            ("live_focus", {"id": "sistema@core::Broker"}))
            assert replies[0]["result"]["serverInfo"]["name"] == "driftspec-live"
            assert reader.next("annotate")["notes"][0]["text"] == "the data comes in here"
            assert reader.next("annotate")["highlights"][0]["id"] == "sistema@core::Broker::publish"
            assert reader.next("focus")["id"] == "sistema@core::Broker"
            post(live, "/selection", {"id": "sistema@core::Broker::publish", "label": "publish"})  # the reader points back
            got = agent(live.port, ("live_selection", {}), ("live_state", {}))
            assert json.loads(got[1]["result"]["content"][0]["text"])["id"] == "sistema@core::Broker::publish"
            assert json.loads(got[2]["result"]["content"][0]["text"])["notes"][0]["id"] == "sistema@core::Broker"
            assert get(live, "/state")["version"] == 1
        finally:
            live.stop()

    def test_the_agent_finds_every_tool_of_the_contract(self, tmp_path):
        replies = agent(0)
        lines = [{"jsonrpc": "2.0", "id": 1, "method": "tools/list"}]
        out = io.StringIO()
        pyds.live.mcp(0, io.StringIO(json.dumps(lines[0]) + "\n"), out)
        names = {t["name"] for t in json.loads(out.getvalue())["result"]["tools"]}
        assert names == {"live_open", "live_focus", "live_note", "live_highlight", "live_clear", "live_selection", "live_state"}
        assert replies[0]["result"]["capabilities"] == {"tools": {}}
