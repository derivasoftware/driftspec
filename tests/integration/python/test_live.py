"""pyds.live, the live session (REQ-00089-1, REQ-00090-1, REQ-00091-2, REQ-00102-1..REQ-00105-1): a workspace
followed as it is written, and what the one who writes and the one who reads point at."""

import os
import subprocess
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

    def test_the_rules_reach_the_state_as_written_with_what_breaks_them(self, session):
        ws, live = session
        with open(os.path.join(ws, "rules.ds"), "w") as fh:
            fh.write("package sistema {\n"
                     "  requirement REQ-00009-1: Every class shall answer to a requirement\n"
                     "  rule answered {\n"
                     "    title: every class answers to a requirement\n"
                     "    @requirement REQ-00009-1\n"
                     "    for class c\n"
                     "    holds c requires r\n"
                     "  }\n"
                     "}\n")
        state = until(lambda: (s := get(live, "/state"))["version"] == 2 and s)
        (rule,) = [r for r in state["rules"] if r["id"] == "sistema@answered"]
        assert rule["kind"] == "rule" and rule["title"] == "every class answers to a requirement"
        assert rule["lines"] == [["for", "class c"], ["holds", "c requires r"]]
        assert rule["requirements"] == ["REQ-00009-1"] and rule["file"] == "rules.ds"
        assert rule["broken"] and all(b["bindings"][0][0] == "c" for b in rule["broken"])
        assert rule["problems"] == []

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
            (note,) = state["notes"]
            assert (note["id"], note["text"], note["tone"], note["seen"]) == ("sistema@core::Broker", "where the data comes in", "info", False)
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

    def test_the_page_goes_fullscreen_whole_when_the_viewer_asks(self, session):
        # the viewer's fullscreen button asks the page, so the side panel stays on screen (DS-141)
        ws, live = session
        import urllib.request
        with urllib.request.urlopen(live.url, timeout=5) as r:
            page = r.read().decode()
        assert "e.data.ds==='fullscreen'" in page and "document.documentElement.requestFullscreen()" in page

    def test_the_contract_says_its_routes_and_its_version(self, session):
        _, live = session
        contract = get(live, "/contract")
        state = get(live, "/state")
        assert contract["version"] == state["contract"] == 3
        assert list(state) == contract["state"]  # the state is exactly what the contract says (DS-123)
        assert ", ".join(contract["state"]) in contract["routes"]["GET /state"]
        for route in ("GET /state", "GET /events", "POST /focus", "POST /note", "POST /highlight", "POST /clear", "POST /selection",
                      "POST /seen", "POST /comment", "POST /unread", "POST /isolate", "POST /baseline"):
            assert route in contract["routes"]

    def test_an_agent_calls_the_contract_as_mcp_tools(self, session):
        _, live = session
        replies = agent(live.port, ("live_note", {"id": "sistema@core::Broker", "text": "here"}), ("live_state", {}), ("live_nothing", {}))
        assert replies[1]["result"]["content"][0]["text"] == '{\n "notes": 1\n}'
        assert '"version": 1' in replies[2]["result"]["content"][0]["text"]
        assert replies[3]["error"]["message"] == "no tool live_nothing"

    # ---- what the reader does with the notes (REQ-00102-1, DS-102)
    def test_the_reader_marks_a_note_seen_and_a_rewritten_note_comes_back_unseen(self, session):
        ws, live = session
        post(live, "/note", {"id": "sistema@core::Broker", "text": "first"})
        post(live, "/note", {"id": "sistema@core", "text": "second"})
        assert post(live, "/seen", {"ids": ["sistema@core::Broker"], "seen": True})["seen"] == 1
        seen = {n["id"]: n["seen"] for n in get(live, "/state")["notes"]}
        assert seen == {"sistema@core::Broker": True, "sistema@core": False}
        post(live, "/note", {"id": "sistema@core::Broker", "text": "first, said again"})  # the agent writes it again
        assert {n["id"]: n["seen"] for n in get(live, "/state")["notes"]}["sistema@core::Broker"] is False
        post(live, "/seen", {"ids": ["sistema@core"], "seen": True})
        live.stop()
        again = pyds.live.serve(ws, 0, block=False)  # what was seen is kept with the notes
        try:
            assert {n["id"]: n["seen"] for n in get(again, "/state")["notes"]} == {"sistema@core::Broker": False, "sistema@core": True}
        finally:
            again.stop()

    def test_the_page_folds_notes_marks_them_seen_and_keeps_them_apart(self, session):
        _, live = session
        import urllib.request
        with urllib.request.urlopen(live.url, timeout=5) as r:
            page = r.read().decode()
        for control in ('id="nfold"', 'id="nopen"', 'id="nseen"', 'id="nhide"', "e.data.ds==='note'", "fetch('/seen'"):
            assert control in page, control

    # ---- what the reader writes for the agent (REQ-00103-1, DS-147)
    def test_a_comment_reaches_the_agent_once_and_survives_a_restart(self, session):
        ws, live = session
        post(live, "/comment", {"id": "sistema@core::Broker", "text": "why does it publish ints?"})
        post(live, "/comment", {"id": "sistema@core", "text": "split this module"})
        assert [c["text"] for c in post(live, "/unread", {"mark": False})["comments"]] == ["why does it publish ints?", "split this module"]
        (first, second) = post(live, "/unread", {"mark": True})["comments"]
        assert (first["n"], first["id"], second["n"], second["id"]) == (1, "sistema@core::Broker", 2, "sistema@core")
        assert post(live, "/unread", {})["comments"] == []  # read once
        post(live, "/comment", {"id": "sistema@core", "text": "and rename it"})
        live.stop()
        again = pyds.live.serve(ws, 0, block=False)
        try:
            state = get(again, "/state")
            assert [(c["n"], c["read"]) for c in state["comments"]] == [(1, True), (2, True), (3, False)]
            assert [c["text"] for c in post(again, "/unread", {})["comments"]] == ["and rename it"]
            assert sorted(pyds.read(ws)) == [os.path.join(ws, "hld.ds")]  # beside the design, never in it
        finally:
            again.stop()

    def test_the_page_lets_the_reader_comment_on_what_is_selected(self, session):
        _, live = session
        import urllib.request
        with urllib.request.urlopen(live.url, timeout=5) as r:
            page = r.read().decode()
        assert 'id="ctext"' in page and "fetch('/comment'" in page and "body:JSON.stringify({id:SELID,text:t})" in page

    # ---- what the reader isolates (REQ-00104-1, DS-139)
    def test_the_isolation_is_kept_and_told_to_every_page(self, session):
        _, live = session
        reader = Reader(live)
        reader.next("model")
        assert post(live, "/isolate", {"ids": ["sistema@core"]})["isolated"] == ["sistema@core"]
        assert reader.next("isolate")["ids"] == ["sistema@core"]
        assert get(live, "/state")["isolated"] == ["sistema@core"]
        post(live, "/isolate", {"ids": [], "tell": False})  # a page that isolated it itself only tells the session
        assert get(live, "/state")["isolated"] == []

    # ---- what the design changes against a baseline (REQ-00105-1, DS-148)
    def test_the_design_is_compared_with_a_baseline_directory(self, session, tmp_path):
        ws, live = session
        base = tmp_path / "base"
        base.mkdir()
        (base / "hld.ds").write_text(SYSTEM.replace("    class Broker {", "    class Old { }\n    class Broker {").replace("(n: int)", "(n: string)"))
        assert post(live, "/baseline", {"baseline": str(base)})["loaded"] is True
        with open(os.path.join(ws, "hld.ds"), "a") as fh:
            fh.write("package extra { }\n")
        state = until(lambda: (s := get(live, "/state"))["diff"]["added"] and s)
        assert state["baseline"] == str(base)
        assert state["diff"]["added"] == ["extra"]
        assert "sistema@core::Broker::publish" in state["diff"]["changed"] and "sistema@core::Broker" in state["diff"]["changed"]
        assert state["diff"]["removed"] == ["sistema@core::Old"]
        post(live, "/baseline", {"baseline": ""})  # comparing stops
        assert until(lambda: (s := get(live, "/state"))["diff"] == {"added": [], "changed": [], "removed": []} and s)

    def test_the_design_is_compared_with_a_git_ref(self, session):
        ws, live = session
        root = os.path.dirname(ws)
        git = lambda *a: subprocess.run(["git", "-C", root, *a], check=True, capture_output=True)  # noqa: E731
        git("init", "-q")
        git("-c", "user.email=t@t", "-c", "user.name=t", "add", "-A")
        git("-c", "user.email=t@t", "-c", "user.name=t", "commit", "-qm", "base")
        with open(os.path.join(ws, "more.ds"), "w") as fh:
            fh.write("package more { }\n")
        assert post(live, "/baseline", {"baseline": "HEAD"})["loaded"] is True
        state = until(lambda: (s := get(live, "/state"))["diff"]["added"] and s)
        assert state["diff"] == {"added": ["more"], "changed": [], "removed": []}

    def test_an_agent_reads_comments_isolates_and_compares_as_mcp_tools(self, session, tmp_path):
        ws, live = session
        post(live, "/comment", {"id": "sistema@core", "text": "look here"})
        replies = agent(live.port, ("live_comments", {}), ("live_isolate", {"ids": ["sistema@core"]}), ("live_baseline", {"baseline": ws}))
        assert '"look here"' in replies[1]["result"]["content"][0]["text"]
        assert get(live, "/state")["isolated"] == ["sistema@core"]
        assert '"loaded": true' in replies[3]["result"]["content"][0]["text"]
