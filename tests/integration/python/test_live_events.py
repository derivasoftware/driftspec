"""driftspec.live, its events (REQ-00090-1, DS-414): a page takes an event off its queue later than it was sent,
and what it reads is what was sent, not the session's notes, highlights and comments as they are by then."""

import json

import pytest

from driftspec.live_session import Live

DESIGN = "package s {\n  module core {\n    class Broker { }\n  }\n}\n"


@pytest.fixture
def ws(tmp_path):
    design = tmp_path / "proj" / "design"
    design.mkdir(parents=True)
    (design / "s.ds").write_text(DESIGN)
    return str(design)


class TestEventsCarryWhatTheyWereSentWith:
    """DS-414: a page takes an event off its queue later than it was sent; what it reads is what was sent."""

    def test_an_annotate_event_keeps_the_annotations_it_was_sent_with(self, ws):
        live = Live(ws)
        q = live.events.listen()
        live.clear()
        live.comment("s@core::Broker", "sent after the clear")
        event, data = q.get_nowait()
        assert event == "annotate" and json.loads(json.dumps(data))["comments"] == []

    def test_a_model_event_keeps_the_state_it_was_sent_with(self, ws):
        live = Live(ws)
        live.note("s@core::Broker", "look")
        q = live.events.listen()
        live.reload()
        live.seen(["s@core::Broker"])                # changes the note in place
        live.comment("s@core::Broker", "written after the reload")  # appends to the list in place
        event, data = q.get_nowait()
        sent = json.loads(json.dumps(data))
        assert event == "model" and sent["comments"] == [] and sent["notes"][0]["seen"] is False
