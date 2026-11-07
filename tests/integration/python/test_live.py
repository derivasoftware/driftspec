"""pyds.live, the live session (REQ-00089-1, REQ-00090-1, REQ-00091-1): a workspace followed as it is written, and
what the one who writes and the one who reads point at."""

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from live_helpers import SYSTEM, Reader, agent, get, post, until, workspace  # noqa: E402

import pyds.live  # noqa: E402


@pytest.fixture
def session(tmp_path):
    ws = workspace(str(tmp_path))
    live = pyds.live.serve(ws, 0, block=False)
    yield ws, live
    live.stop()


class TestLiveSession:
    def test_a_change_to_a_file_reaches_the_state_with_what_changed(self, session):
        ws, live = session
        assert get(live, "/state")["version"] == 1
        with open(os.path.join(ws, "hld.ds"), "w") as fh:
            fh.write(SYSTEM.replace("    class Broker {", "    class Queue {\n      + push(n: int) -> none\n    }\n    class Broker {"))
        state = until(lambda: (s := get(live, "/state"))["version"] == 2 and s)
        assert "sistema@core::Queue" in state["changed"] and "sistema@core::Queue::push" in state["changed"]
        assert "sistema@core::Broker" not in state["changed"]
        assert state["history"][-1]["changed"] == state["changed"]

    def test_a_reader_receives_the_model_and_is_taken_where_it_is_asked(self, session):
        _, live = session
        reader = Reader(live)
        assert reader.next("model")["version"] == 1
        post(live, "/focus", {"id": "sistema@core::Broker"})
        assert reader.next("focus")["id"] == "sistema@core::Broker"

    def test_the_readers_selection_reaches_the_one_who_writes(self, session):
        _, live = session
        post(live, "/selection", {"id": "sistema@core::Broker::publish", "label": "publish"})
        assert get(live, "/selection")["id"] == "sistema@core::Broker::publish"

    def test_notes_and_highlights_survive_a_restart_and_stay_out_of_the_design(self, session):
        ws, live = session
        post(live, "/note", {"id": "sistema@core::Broker", "text": "where the data comes in", "tone": "info"})
        post(live, "/highlight", {"ids": ["sistema@core::Broker::publish"], "tone": "ok"})
        live.stop()
        again = pyds.live.serve(ws, 0, block=False)
        try:
            state = get(again, "/state")
            assert state["notes"] == [{"id": "sistema@core::Broker", "text": "where the data comes in", "tone": "info"}]
            assert state["highlights"] == [{"id": "sistema@core::Broker::publish", "tone": "ok"}]
            assert sorted(pyds.read(ws)) == [os.path.join(ws, "hld.ds")]  # kept beside the design, not in it
        finally:
            again.stop()

    def test_a_note_with_no_text_removes_it_and_clear_removes_them_all(self, session):
        _, live = session
        post(live, "/note", {"id": "a", "text": "one"})
        post(live, "/note", {"id": "b", "text": "two"})
        assert post(live, "/note", {"id": "a", "text": ""})["notes"] == 1
        post(live, "/clear", {"what": "all"})
        assert get(live, "/state")["notes"] == [] and get(live, "/state")["highlights"] == []

    def test_the_contract_says_its_routes_and_its_version(self, session):
        _, live = session
        contract = get(live, "/contract")
        assert contract["version"] == get(live, "/state")["contract"] == 1
        for route in ("GET /state", "GET /events", "POST /focus", "POST /note", "POST /highlight", "POST /clear", "POST /selection"):
            assert route in contract["routes"]

    def test_an_agent_calls_the_contract_as_mcp_tools(self, session):
        _, live = session
        replies = agent(live.port, ("live_note", {"id": "sistema@core::Broker", "text": "here"}), ("live_state", {}), ("live_nothing", {}))
        assert replies[1]["result"]["content"][0]["text"] == '{\n "notes": 1\n}'
        assert '"version": 1' in replies[2]["result"]["content"][0]["text"]
        assert replies[3]["error"]["message"] == "no tool live_nothing"
