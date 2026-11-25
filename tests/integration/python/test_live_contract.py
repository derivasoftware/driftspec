"""driftspec.live, its contract declared once (REQ-00090-1, REQ-00091-3, REQ-00089-2): the HTTP routes, the MCP
tools and the command line are read from one table, agree with it, and publish what version 4 did."""

import json
import os
import sys
import urllib.request

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from live_helpers import ANSWER, workspace  # noqa: E402

import driftspec.live as live  # noqa: E402
import driftspec.live_http as live_http  # noqa: E402
import driftspec.live_mcp as live_mcp  # noqa: E402
from driftspec.live_contract import CONTRACT, LIVE  # noqa: E402

# what GET /contract publishes (version 5: the coverage, DS-386) and what tools/list said before the contract was a
# table (DS-322): a client reads these bytes
PUBLISHED = '{"version": 5, "state": ["contract", "workspace", "title", "version", "boot", "findings", "coverage", "counts", "history", "selection", "error", "notes", "highlights", "rules", "comments", "isolated"], "routes": {"GET /": "the page: the viewer that follows the session", "GET /page": "the viewer of the current model, one self-contained page", "GET /state": "the state: contract, workspace, title, version, boot, findings, coverage, counts, history, selection, error, notes, highlights, rules, comments, isolated", "GET /events": "server-sent events: model (the state, on every change), focus, annotate, isolate, error", "GET /selection": "what the reader has selected", "GET /contract": "this contract", "POST /selection": "{id, label}: the reader selected an identity", "POST /focus": "{id, also}: take the reader to an identity", "POST /note": "{id, text, tone}: a bubble on an identity; empty text removes it", "POST /highlight": "{ids, tone, add}: light identities; replaces the highlight unless add", "POST /clear": "{what}: notes, highlights or all", "POST /seen": "{ids, seen}: the reader marks notes seen (or not)", "POST /comment": "{id, text}: the reader writes a comment on an identity for the agent", "POST /unread": "{mark}: the comments the agent has not read; marked read unless mark is false", "POST /isolate": "{ids, tell}: show only these boxes; empty shows everything"}, "tones": ["accent", "info", "ok", "bad", "req", "you"]}'
TOOLS_V4 = '[{"name": "live_open", "description": "Open a DriftSpec workspace in the live viewer (starts the server if needed) and open the browser.", "inputSchema": {"type": "object", "properties": {"workspace": {"type": "string"}}, "required": ["workspace"]}}, {"name": "live_focus", "description": "Take the reader to an identity (a box, class, member, interface, requirement or sequence) in the live viewer: it is selected and framed.", "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "also": {"type": "array", "items": {"type": "string"}}}, "required": ["id"]}}, {"name": "live_note", "description": "Put a speech bubble on an identity in the live viewer; the same identity replaces its bubble, empty text removes it. tone: accent, info, ok, bad, req.", "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}, "text": {"type": "string"}, "tone": {"type": "string"}}, "required": ["id", "text"]}}, {"name": "live_highlight", "description": "Highlight identities of any perspective in the live viewer with a tone; replaces the previous highlight unless add is true.", "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}, "tone": {"type": "string"}, "add": {"type": "boolean"}}, "required": ["ids"]}}, {"name": "live_clear", "description": "Clear the notes, the highlights or both (what: notes, highlights, all) in the live viewer.", "inputSchema": {"type": "object", "properties": {"what": {"type": "string"}}}}, {"name": "live_comments", "description": "What the reader wrote for the agent in the live viewer since the last call: each comment with the identity it is on; they are marked read unless peek is true. Answer with live_note on the same identity.", "inputSchema": {"type": "object", "properties": {"peek": {"type": "boolean"}}}}, {"name": "live_isolate", "description": "Show only these boxes in the live viewer, with what they use from outside as ghosts; an empty list shows everything.", "inputSchema": {"type": "object", "properties": {"ids": {"type": "array", "items": {"type": "string"}}}, "required": ["ids"]}}, {"name": "live_selection", "description": "What the reader has selected in the live viewer.", "inputSchema": {"type": "object", "properties": {}}}, {"name": "live_state", "description": "The live session: its version and the findings.", "inputSchema": {"type": "object", "properties": {}}}]'

# what the server writes itself rather than answering as JSON
SERVED = {"/", "/page", "/events"}
DOOR_TOOLS = {"live_open"}             # the MCP door's own: it starts a session
DOOR_COMMANDS = {"serve", "mcp"}       # the command line's own



@pytest.fixture
def session(tmp_path):
    served = live.serve(workspace(str(tmp_path)), 0, block=False)
    yield served
    served.stop()


class TestContract:
    def test_the_contract_publishes_its_version_byte_for_byte(self):
        assert json.dumps(CONTRACT) == PUBLISHED and json.dumps(LIVE.describe()) == PUBLISHED

    def test_the_tools_an_agent_lists_are_what_version_4_listed_byte_for_byte(self):
        assert json.dumps(live_mcp.TOOLS) == TOOLS_V4

    def test_every_route_of_the_contract_is_served_and_nothing_else(self):
        routes = set(CONTRACT["routes"])
        json_routes = {f"GET {p}" for p in live_http.GETS} | {f"POST {p}" for p in live_http.POSTS}
        assert json_routes | {f"GET {p}" for p in SERVED} == routes

    def test_every_route_answers_on_a_session(self, session):
        for op in LIVE.operations:
            body = None if op.verb == "GET" else b"{}"
            req = urllib.request.Request(f"http://127.0.0.1:{session.port}{op.path}", data=body, method=op.verb,
                                         headers={"Host": f"localhost:{session.port}"})
            with urllib.request.urlopen(req, timeout=ANSWER) as r:
                assert r.status == 200, op.route()
                if op.path == "/events":
                    break  # a stream: it answers and stays open; the last one asked

    def test_every_tool_is_an_operation_of_the_contract_or_the_door_s_own(self):
        names = [t["name"] for t in live_mcp.TOOLS]
        of_contract = [op.tool.name for op in LIVE.tools()]
        assert set(names) == set(of_contract) | DOOR_TOOLS and len(names) == len(set(names))
        assert set(live_mcp.CALLS) == set(names)
        assert all(op.route() in CONTRACT["routes"] for op in LIVE.tools())

    def test_every_command_is_an_operation_of_the_contract_or_the_door_s_own(self):
        words = [op.word.word for op in LIVE.words()]
        assert set(live.COMMANDS) == set(words) | DOOR_COMMANDS and len(words) == len(set(words))
        assert all(op.route() in CONTRACT["routes"] for op in LIVE.words())

    def test_a_get_takes_no_body_from_a_tool_or_a_command(self):
        for op in LIVE.operations:
            if op.verb == "GET":
                assert (op.tool is None or op.tool.body({}) is None) and (op.word is None or op.word.body(None) is None), op.route()
