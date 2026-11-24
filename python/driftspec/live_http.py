"""The live session served over HTTP: the routes of the contract, the events a page follows, and the
server that runs them on this machine only."""

from __future__ import annotations

import json
import queue
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from driftspec.browser import open_in_browser as browser_open
from driftspec.live_contract import LIVE, PORT
from driftspec.live_page import shell
from driftspec.live_session import Live

# What each GET answers as JSON, and what each POST does with the body it sends: read from the contract.
GETS = LIVE.gets()
POSTS = LIVE.posts()


class Handler(BaseHTTPRequestHandler):
    """One request to a session: handler_for gives each session its own subclass, bound to it."""

    live: Live

    def log_message(self, *a):  # quiet
        pass

    def local(self) -> bool:
        """Whether a request comes from this machine's own page or tools (DS-302).

        The session listens on 127.0.0.1, but any page the browser has open can still send to it:
        a POST from another origin is carried out even though the browser hides the answer, and a
        name that resolves to 127.0.0.1 (DNS rebinding) can read it. So the Host must be this
        session's own name, and an Origin, when the browser sends one, must be this session's.
        """
        port = self.server.server_address[1]
        own = {f"localhost:{port}", f"127.0.0.1:{port}", f"[::1]:{port}"}
        if (self.headers.get("Host") or "") not in own:
            return False
        origin = self.headers.get("Origin")
        return origin is None or origin in {f"http://{h}" for h in own}

    def send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def json(self, obj, code: int = 200) -> None:
        self.send(code, json.dumps(obj).encode(), "application/json")

    def do_GET(self):
        if not self.local():
            self.send(403, b"not this session's page", "text/plain")
            return
        path = self.path.split("?")[0]
        if path in GETS:
            self.json(GETS[path](self.live))
        elif path == "/":
            self.send(200, shell(self.live.title, self.live.workspace).encode(), "text/html; charset=utf-8")
        elif path == "/page":
            self.page()
        elif path == "/events":
            self.events()
        else:
            self.send(404, b"not found", "text/plain")

    def page(self) -> None:
        with self.live.lock:
            page = self.live.page
        self.send(200, page.encode(), "text/html; charset=utf-8")

    def events(self) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        live = self.live
        q = live.events.listen()
        q.put(("model", live.state()))
        try:
            while not getattr(live, "stopped", False):
                self.stream(q)
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            live.events.leave(q)

    def stream(self, q: queue.Queue) -> None:
        """The next event a page follows, or a keep-alive when none came for 15 seconds."""
        try:
            event, data = q.get(timeout=15)
            self.wfile.write(f"event: {event}\ndata: {json.dumps(data)}\n\n".encode())
        except queue.Empty:
            self.wfile.write(b": keep-alive\n\n")
        self.wfile.flush()

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        if not self.local():
            self.json({"error": "not this session's page"}, 403)
            return
        try:
            data = json.loads(body or b"{}")
        except ValueError:
            data = {}
        if self.path in POSTS:
            self.json(POSTS[self.path](self.live, data))
        else:
            self.json({}, 404)


def handler_for(live: Live) -> type[Handler]:
    """The HTTP handler class serving one session."""
    return type("Handler", (Handler,), {"live": live})


def serve(workspace: str, port: int = PORT, open_browser: bool = False, block: bool = True) -> Live:
    """A session on the workspace, served on the port (0: any free one); the session knows its url and how to stop."""
    live = Live(workspace)
    live.reload()
    server = ThreadingHTTPServer(("127.0.0.1", port), handler_for(live))
    server.daemon_threads = True
    live.port = server.server_address[1]
    live.url = f"http://localhost:{live.port}/"

    def stop() -> None:
        live.stopped = True
        server.shutdown()
        server.server_close()

    live.stop = stop
    threading.Thread(target=live.watch, daemon=True).start()
    print(f"driftspec live: {live.workspace} at {live.url}", file=sys.stderr, flush=True)
    if open_browser:
        browser_open(live.url)
    if block:
        server.serve_forever()
    else:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    return live
