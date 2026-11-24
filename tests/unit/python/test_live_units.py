"""driftspec.live, unit by unit: the command line, each command against a session served on a free
port."""

import io
import json
import os

import driftspec.live_http as live_http
from driftspec.live import Command, asking, main
from live_fixtures import no_browser, session, ws  # noqa: F401 (fixtures)

__covers__ = (Command, asking, main)


class TestMain:
    def test_each_command_calls_its_route(self, session, capsys):
        port = ["--port", str(session.port)]
        main(["note", "s@core::Broker", "two", "words", "--tone", "ok", *port])
        main(["highlight", "a", "b", "--add", *port])
        main(["focus", "s@core::Broker", *port])
        main(["isolate", "s@core", *port])
        assert session.annotations.notes[0]["text"] == "two words" and session.annotations.notes[0]["tone"] == "ok"
        assert [h["id"] for h in session.annotations.highlights] == ["a", "b"] and session.isolated == ["s@core"]
        main(["clear", "highlights", *port])
        assert session.annotations.highlights == [] and session.annotations.notes
        session.comment("s@core::Broker", "why?")
        capsys.readouterr()
        main(["comments", "--peek", *port])
        assert "why?" in capsys.readouterr().out and session.annotations.comments[0]["read"] is False
        session.select("s@core::Broker", "Broker")
        main(["selection", *port])
        assert json.loads(capsys.readouterr().out)["label"] == "Broker"
        main(["state", *port])
        assert json.loads(capsys.readouterr().out)["version"] == 1

    def test_a_command_of_the_contract_prints_what_its_word_shows(self, session, capsys):
        from driftspec.live_contract import LIVE

        state = next(op for op in LIVE.words() if op.word.word == "state")
        asking(state)(Command(["state", "--port", str(session.port)]))
        assert list(json.loads(capsys.readouterr().out)) == ["version", "counts", "error", "findings"]

    def test_mcp_speaks_on_stdio_and_anything_else_prints_the_usage(self, session, monkeypatch, capsys):
        monkeypatch.setattr("sys.stdin", io.StringIO(json.dumps({"id": 1, "method": "tools/list"}) + "\n"))
        main(["mcp", "--port", str(session.port)])
        assert json.loads(capsys.readouterr().out)["result"]["tools"]
        main(["what"])
        assert "python -m driftspec.live serve" in capsys.readouterr().out

    def test_serve_follows_the_workspace_on_the_port_given(self, ws, no_browser, monkeypatch):
        import threading as real

        served, threads = [], []

        class Server:  # the HTTP server, stood in for: it records what it would serve
            daemon_threads = False

            def __init__(self, address, handler):
                self.server_address = ("127.0.0.1", 4800)
                served.append(handler)

            def serve_forever(self):
                served.append("served")

        class Thread:  # the watcher is not started: the session would follow the files for ever
            def __init__(self, target, daemon):
                threads.append(target)

            def start(self):
                pass

        monkeypatch.setattr(live_http, "ThreadingHTTPServer", Server)
        monkeypatch.setattr(live_http, "threading", type("T", (), {"Thread": Thread, "Lock": real.Lock}))
        main(["serve", ws, "--open", "--port", "4800"])
        assert served[-1] == "served" and no_browser == ["http://localhost:4800/"]
        assert threads[0].__self__.workspace == os.path.realpath(ws)
