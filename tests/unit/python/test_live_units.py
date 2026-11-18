"""driftspec.live, unit by unit: the session asked directly, its handler served on a free port, the
client side, the MCP tools and the command line, with the browser stood in for."""

import io
import json
import os
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
import threading

import pytest

import driftspec.live as live_module
from driftspec.live import CONTRACT, Live, browser_open, call, handler_for, main, mcp, serve, tool

__covers__ = (Live, handler_for, browser_open, serve, call, tool, mcp, main)

DESIGN = (
    "package s {\n"
    "  requirement REQ-00009-1: Every class shall answer to a requirement\n"
    "  rule answered {\n"
    "    title: every class answers to a requirement\n"
    "    @requirement REQ-00009-1\n"
    "    for class c\n"
    "    holds c satisfies r\n"
    "  }\n"
    "  module core {\n"
    "    class Broker { }\n"
    "  }\n"
    "}\n"
)


@pytest.fixture
def ws(tmp_path):
    design = tmp_path / "proj" / "design"
    design.mkdir(parents=True)
    (design / "s.ds").write_text(DESIGN)
    return str(design)


@pytest.fixture
def no_browser(monkeypatch):
    """The browser the session would open: the default one, recorded instead of opened."""
    import shutil

    opened = []
    monkeypatch.setattr(shutil, "which", lambda name: None)
    monkeypatch.setattr(live_module.webbrowser, "open", opened.append)
    return opened


def free_port() -> int:
    import socket

    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.fixture
def session(ws, no_browser):
    live = serve(ws, 0, block=False)
    yield live
    live.stop()


class TestLive:
    def test_a_reload_reads_the_model_once_and_again_only_after_a_save(self, ws):
        live = Live(ws)
        assert live.title == "proj" and live.store == os.path.join(live.workspace, ".driftspec", "live.json")
        assert live.reload() and live.version == 1 and live.page and not live.error
        assert not live.reload() and live.version == 1
        with open(os.path.join(ws, "s.ds"), "a") as fh:
            fh.write("\n// saved\n")
        assert live.reload() and live.version == 2 and live.history[-1]["version"] == 2
        assert list(live.state()) == live_module.STATE and live.state()["contract"] == CONTRACT["version"]

    def test_the_rules_are_listed_as_written_with_what_breaks_them(self, ws):
        live = Live(ws)
        live.reload()
        (rule,) = live.rules
        assert rule["id"] == "s@answered" and rule["lines"] == [["for", "class c"], ["holds", "c satisfies r"]]
        assert rule["requirements"] == ["REQ-00009-1"] and rule["broken"] and rule["file"] == "s.ds"
        assert live.files_stamp() == live.stamp

    def test_a_half_written_file_is_an_error_not_an_end(self, ws, monkeypatch):
        live = Live(ws)
        monkeypatch.setattr(live_module.driftspec, "Workspace", lambda files: (_ for _ in ()).throw(RuntimeError("half")))
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


def request(port, path, data=None, host=None, origin=None):
    headers = {"Content-Type": "application/json"}
    if host:
        headers["Host"] = host
    if origin:
        headers["Origin"] = origin
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=None if data is None else json.dumps(data).encode(),
                                 method="GET" if data is None else "POST", headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


class TestHandler:
    @pytest.fixture
    def served(self, ws):
        live = Live(ws)
        live.reload()
        httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler_for(live))
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        yield live, httpd.server_address[1]
        live.stopped = True
        httpd.shutdown()
        httpd.server_close()

    def test_it_answers_the_routes_of_the_contract(self, served):
        live, port = served
        status, body = request(port, "/")
        assert status == 200 and b"proj" in body
        assert request(port, "/page")[1].decode() == live.page
        assert json.loads(request(port, "/contract")[1]) == CONTRACT
        assert json.loads(request(port, "/state")[1])["version"] == 1
        assert request(port, "/nowhere")[0] == 404
        for path, data in [("/selection", {"id": "s@core::Broker"}), ("/focus", {"id": "x"}), ("/note", {"id": "x", "text": "t"}),
                           ("/highlight", {"ids": ["x"]}), ("/seen", {"ids": ["x"]}), ("/comment", {"id": "x", "text": "c"}),
                           ("/unread", {"mark": False}), ("/isolate", {"ids": ["x"]}), ("/clear", {"what": "all"})]:
            assert request(port, path, data)[0] == 200, path
        assert json.loads(request(port, "/selection")[1])["id"] == "s@core::Broker"
        assert request(port, "/nowhere", {})[0] == 404

    def test_it_refuses_another_name_and_another_origin(self, served):
        live, port = served
        assert request(port, "/state", host="evil.example:80")[0] == 403
        assert request(port, "/note", {"id": "x", "text": "t"}, origin="http://evil.example")[0] == 403
        assert live.notes == []
        assert request(port, "/state", origin=f"http://localhost:{port}", host=f"localhost:{port}")[0] == 200


class TestServeAndCall:
    def test_a_session_serves_its_workspace_on_a_free_port_and_stops(self, session, no_browser):
        assert session.url == f"http://localhost:{session.port}/" and session.version == 1
        assert call(session.port, "/state")["workspace"] == session.workspace
        assert call(session.port, "/focus", {"id": "s@core::Broker"})["sent"] == "s@core::Broker"
        assert no_browser == []

    def test_open_takes_the_browser_to_it(self, ws, no_browser):
        live = serve(ws, 0, open_browser=True, block=False)
        try:
            assert no_browser == [live.url]
        finally:
            live.stop()


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

    def test_open_starts_a_session_when_none_serves_the_port(self, ws, no_browser):
        port = free_port()
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

    def test_a_tool_that_fails_answers_an_error_result(self):
        out = io.StringIO()
        mcp(free_port(), io.StringIO(json.dumps({"id": 1, "method": "tools/call", "params": {"name": "live_state"}}) + "\n"), out)
        reply = json.loads(out.getvalue())
        assert reply["result"]["isError"] and reply["result"]["content"][0]["text"].startswith("error: ")


class TestMain:
    def test_each_command_calls_its_route(self, session, capsys):
        port = ["--port", str(session.port)]
        main(["note", "s@core::Broker", "two", "words", "--tone", "ok", *port])
        main(["highlight", "a", "b", "--add", *port])
        main(["focus", "s@core::Broker", *port])
        main(["isolate", "s@core", *port])
        assert session.notes[0]["text"] == "two words" and session.notes[0]["tone"] == "ok"
        assert [h["id"] for h in session.highlights] == ["a", "b"] and session.isolated == ["s@core"]
        main(["clear", "highlights", *port])
        assert session.highlights == [] and session.notes
        session.comment("s@core::Broker", "why?")
        capsys.readouterr()
        main(["comments", "--peek", *port])
        assert "why?" in capsys.readouterr().out and session.comments[0]["read"] is False
        session.select("s@core::Broker", "Broker")
        main(["selection", *port])
        assert json.loads(capsys.readouterr().out)["label"] == "Broker"
        main(["state", *port])
        assert json.loads(capsys.readouterr().out)["version"] == 1

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

        monkeypatch.setattr(live_module, "ThreadingHTTPServer", Server)
        monkeypatch.setattr(live_module, "threading", type("T", (), {"Thread": Thread, "Lock": real.Lock}))
        main(["serve", ws, "--open", "--port", "4800"])
        assert served[-1] == "served" and no_browser == ["http://localhost:4800/"]
        assert threads[0].__self__.workspace == os.path.realpath(ws)


class TestBrowserOpen:
    def test_wsl_opens_the_windows_browser_and_elsewhere_the_default_one(self, monkeypatch):
        import shutil
        import subprocess

        opened = []
        monkeypatch.setattr(subprocess, "Popen", lambda argv, **_: opened.append(argv))
        monkeypatch.setattr(live_module.webbrowser, "open", lambda url: opened.append(["webbrowser", url]))
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/explorer.exe")
        browser_open("http://localhost:1/")
        monkeypatch.setattr(shutil, "which", lambda name: None)
        browser_open("http://localhost:2/")
        assert opened == [["explorer.exe", "http://localhost:1/"], ["webbrowser", "http://localhost:2/"]]
