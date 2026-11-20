"""The fixtures the unit tests of the live session share: a small workspace, the browser stood in
for, a session served on a free port and a port nothing serves."""

import webbrowser

import pytest

from driftspec.live_http import serve

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
    monkeypatch.setattr(webbrowser, "open", opened.append)
    return opened


@pytest.fixture
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
