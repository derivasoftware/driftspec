"""driftspec.live_session, unit by unit: the session asked directly, its model, its annotations and
its watch."""

import os

import driftspec.live_session as live_session
from driftspec.live_session import CONTRACT, Live
from live_fixtures import ws  # noqa: F401 (fixtures)

__covers__ = (Live,)


class TestLive:
    def test_a_reload_reads_the_model_once_and_again_only_after_a_save(self, ws):
        live = Live(ws)
        assert live.title == "proj" and live.store == os.path.join(live.workspace, ".driftspec", "live.json")
        assert live.reload() and live.version == 1 and live.page and not live.error
        assert not live.reload() and live.version == 1
        with open(os.path.join(ws, "s.ds"), "a") as fh:
            fh.write("\n// saved\n")
        assert live.reload() and live.version == 2 and live.history[-1]["version"] == 2
        assert list(live.state()) == live_session.STATE and live.state()["contract"] == CONTRACT["version"]

    def test_the_rules_are_listed_as_written_with_what_breaks_them(self, ws):
        live = Live(ws)
        live.reload()
        (rule,) = live.rules
        assert rule["id"] == "s@answered" and rule["lines"] == [["for", "class c"], ["holds", "c satisfies r"]]
        assert rule["requirements"] == ["REQ-00009-1"] and rule["broken"] and rule["file"] == "s.ds"
        assert live.files_stamp() == live.stamp

    def test_a_half_written_file_is_an_error_not_an_end(self, ws, monkeypatch):
        live = Live(ws)
        monkeypatch.setattr(live_session.driftspec, "Workspace", lambda files: (_ for _ in ()).throw(RuntimeError("half")))
        assert live.reload() and live.error == "half" and live.version == 0

    def test_notes_highlights_and_comments_are_kept_beside_the_design(self, ws):
        live = Live(ws)
        heard = []
        live.listeners.append(type("Q", (), {"put": lambda self, m: heard.append(m)})())
        assert live.note("s@core::Broker", "look") == 1 and live.notes[0]["seen"] is False
        assert live.seen(["s@core::Broker"]) == 1
        assert live.note("s@core::Broker", "again") == 1 and live.notes[0]["seen"] is False
        assert live.highlight(["a", "b"], "ok") == 2 and live.highlight(["c"], add=True) == 3
        assert live.comment("s@core::Broker", "why?") == 1
        again = Live(ws)
        assert [n["text"] for n in again.notes] == ["again"] and len(again.highlights) == 3 and len(again.comments) == 1
        assert [c["text"] for c in live.unread()] == ["why?"] and live.unread() == []
        live.clear("highlights")
        assert live.highlights == [] and live.notes
        live.clear()
        assert live.notes == [] and live.note("x", "") == 0
        assert {event for event, _ in heard} == {"annotate"}

    def test_focus_selection_and_isolation_reach_the_pages(self, ws):
        live = Live(ws)
        heard = []
        live.listeners.append(type("Q", (), {"put": lambda self, m: heard.append(m)})())
        assert live.focus("s@core::Broker", ["s@core"]) == 1
        live.select("s@core::Broker", "Broker")
        assert live.selection["id"] == "s@core::Broker" and live.selection["label"] == "Broker"
        assert live.isolate(["s@core"]) == ["s@core"] and live.isolate([], tell=False) == []
        assert heard == [("focus", {"id": "s@core::Broker", "also": ["s@core"]}), ("isolate", {"ids": ["s@core"]})]
        live.broadcast("x", {})
        assert heard[-1] == ("x", {})

    def test_a_read_only_workspace_keeps_them_for_the_session(self, ws, monkeypatch):
        live = Live(ws)
        live.store = os.path.join(ws, "s.ds", "cannot", "live.json")
        live.note("a", "kept in memory")
        assert live.notes and live.load() is None

    def test_watching_reloads_until_stopped(self, ws, monkeypatch):
        live = Live(ws)
        calls = []

        def reload():
            calls.append(1)
            if len(calls) == 2:
                raise RuntimeError("once")
            if len(calls) == 3:
                live.stopped = True
            return True

        monkeypatch.setattr(live, "reload", reload)
        live.watch(0)
        assert len(calls) == 3 and live.error == "once"
