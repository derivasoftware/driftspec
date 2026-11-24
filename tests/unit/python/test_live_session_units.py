"""driftspec.live_session, unit by unit: the session asked directly, its model, its annotations and
its watch."""

import os

import driftspec.live_session as live_session
from driftspec.live_session import CONTRACT, Annotations, Events, Live, Watcher
from live_fixtures import ws  # noqa: F401 (fixtures)

__covers__ = (Live, Annotations, Events, Watcher)


class TestLive:
    def test_a_reload_reads_the_model_once_and_again_only_after_a_save(self, ws):
        live = Live(ws)
        assert live.title == "proj" and live.annotations.store == os.path.join(live.workspace, ".driftspec", "live.json")
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
        assert live.watcher.files_stamp() == live.watcher.stamp

    def test_a_half_written_file_is_an_error_not_an_end(self, ws, monkeypatch):
        live = Live(ws)
        monkeypatch.setattr(live_session.driftspec, "Workspace", lambda files: (_ for _ in ()).throw(RuntimeError("half")))
        assert live.reload() and live.error == "half" and live.version == 0

    def test_notes_highlights_and_comments_are_kept_beside_the_design(self, ws):
        live = Live(ws)
        heard = []
        live.events.listeners.append(type("Q", (), {"put": lambda self, m: heard.append(m)})())
        assert live.note("s@core::Broker", "look") == 1 and live.annotations.notes[0]["seen"] is False
        assert live.seen(["s@core::Broker"]) == 1
        assert live.note("s@core::Broker", "again") == 1 and live.annotations.notes[0]["seen"] is False
        assert live.highlight(["a", "b"], "ok") == 2 and live.highlight(["c"], add=True) == 3
        assert live.comment("s@core::Broker", "why?") == 1
        again = Live(ws)
        assert [n["text"] for n in again.annotations.notes] == ["again"] and len(again.annotations.highlights) == 3 and len(again.annotations.comments) == 1
        assert [c["text"] for c in live.unread()] == ["why?"] and live.unread() == []
        live.clear("highlights")
        assert live.annotations.highlights == [] and live.annotations.notes
        live.clear()
        assert live.annotations.notes == [] and live.note("x", "") == 0
        assert {event for event, _ in heard} == {"annotate"}

    def test_focus_selection_and_isolation_reach_the_pages(self, ws):
        live = Live(ws)
        heard = []
        live.events.listeners.append(type("Q", (), {"put": lambda self, m: heard.append(m)})())
        assert live.focus("s@core::Broker", ["s@core"]) == 1
        live.select("s@core::Broker", "Broker")
        assert live.selection["id"] == "s@core::Broker" and live.selection["label"] == "Broker"
        assert live.isolate(["s@core"]) == ["s@core"] and live.isolate([], tell=False) == []
        assert heard == [("focus", {"id": "s@core::Broker", "also": ["s@core"]}), ("isolate", {"ids": ["s@core"]})]
        live.events.broadcast("x", {})
        assert heard[-1] == ("x", {})

    def test_a_read_only_workspace_keeps_them_for_the_session(self, ws, monkeypatch):
        live = Live(ws)
        live.annotations.store = os.path.join(ws, "s.ds", "cannot", "live.json")
        live.note("a", "kept in memory")
        assert live.annotations.notes and live.annotations.load() is None

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


class TestParts:
    def test_annotations_change_only_what_they_are_asked_and_come_back_from_the_store(self, tmp_path):
        a = Annotations(str(tmp_path / ".driftspec" / "live.json"))
        assert a.note("x", "look", "ok") == 1 and a.highlight(["x", "y"], "bad", False) == 2 and a.comment("y", "why?") == 1
        assert a.seen(["x"], True) == 1 and a.unread() == a.comments and a.highlight(["z"], "ok", True) == 3
        a.save()
        kept = Annotations(a.store)
        assert kept.notes == a.notes and kept.highlights == a.highlights and kept.comments == a.comments
        a.mark_read()
        a.clear("notes")
        assert a.unread() == [] and a.notes == [] and a.highlights and a.told(["x"])["frame"] == ["x"]

    def test_events_reach_every_page_listening_and_none_that_left(self):
        e = Events()
        first, second = e.listen(), e.listen()
        e.leave(second)
        e.leave(second)  # leaving twice is no failure
        e.broadcast("focus", {"id": "x"})
        assert e.count() == 1 and first.get_nowait() == ("focus", {"id": "x"}) and second.empty()

    def test_the_watcher_sees_a_save_and_follows_until_stopped(self, ws):
        w = Watcher(ws)
        assert w.changed() and not w.changed()
        with open(os.path.join(ws, "t.ds"), "w") as fh:
            fh.write("package t { }\n")
        assert w.changed() and w.stamp == w.files_stamp()
        ticks = []
        w.follow(lambda: ticks.append(1), 0, lambda: len(ticks) >= 2)
        assert ticks == [1, 1]
