"""driftspec.live_http, unit by unit: the handler served on a free port, the server and the browser
it opens, stood in for."""

import json
import threading
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import pytest

import driftspec.live_http as live_http
from driftspec.live_http import Handler, browser_open, handler_for, serve
from driftspec.live_mcp import call
from driftspec.live_session import CONTRACT, Live
from live_fixtures import no_browser, session, ws  # noqa: F401 (fixtures)

__covers__ = (Handler, handler_for, serve, browser_open)


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


class TestBrowserOpen:
    def test_wsl_opens_the_windows_browser_and_elsewhere_the_default_one(self, monkeypatch):
        import shutil
        import subprocess

        opened = []
        monkeypatch.setattr(subprocess, "Popen", lambda argv, **_: opened.append(argv))
        monkeypatch.setattr(live_http.webbrowser, "open", lambda url: opened.append(["webbrowser", url]))
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/explorer.exe")
        browser_open("http://localhost:1/")
        monkeypatch.setattr(shutil, "which", lambda name: None)
        browser_open("http://localhost:2/")
        assert opened == [["explorer.exe", "http://localhost:1/"], ["webbrowser", "http://localhost:2/"]]
