"""What the live session's tests share: a workspace to watch and a reader following its events."""

import io
import json
import os
import queue
import threading
import time
import urllib.request

SYSTEM = (
    "package sistema {\n"
    "  module core {\n"
    "    class Broker {\n"
    "      + publish(n: int) -> none\n"
    "    }\n"
    "  }\n"
    "}\n"
)


def workspace(root: str) -> str:
    ws = os.path.join(root, "design")
    os.makedirs(ws, exist_ok=True)
    with open(os.path.join(ws, "hld.ds"), "w") as fh:
        fh.write(SYSTEM)
    return ws


def get(live, path: str):
    with urllib.request.urlopen(live.url.rstrip("/") + path, timeout=5) as r:
        return json.loads(r.read())


def post(live, path: str, data: dict):
    req = urllib.request.Request(live.url.rstrip("/") + path, data=json.dumps(data).encode(), method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read())


def until(check, seconds: float = 8.0):
    end = time.time() + seconds
    while time.time() < end:
        got = check()
        if got:
            return got
        time.sleep(0.1)
    raise AssertionError("nothing came in time")


class Reader:
    """A page following the session: every event it receives, in order."""

    def __init__(self, live):
        self.events: queue.Queue = queue.Queue()
        self.stream = urllib.request.urlopen(live.url.rstrip("/") + "/events", timeout=20)
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        event = None
        try:
            for raw in self.stream:
                line = raw.decode().rstrip("\n")
                if line.startswith("event: "):
                    event = line[7:]
                elif line.startswith("data: ") and event:
                    self.events.put((event, json.loads(line[6:])))
                    event = None
        except Exception:
            pass

    def next(self, kind: str, seconds: float = 8.0):
        end = time.time() + seconds
        while time.time() < end:
            try:
                event, data = self.events.get(timeout=max(0.05, end - time.time()))
            except queue.Empty:
                break
            if event == kind:
                return data
        raise AssertionError(f"no {kind} event came")


def agent(port: int, *calls) -> list[dict]:
    """An MCP session over the module's own stdio loop: initialize, then each tool call; the replies in order."""
    import pyds.live

    lines = [{"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}}]
    lines += [{"jsonrpc": "2.0", "id": i + 1, "method": "tools/call", "params": {"name": n, "arguments": a}} for i, (n, a) in enumerate(calls)]
    out = io.StringIO()
    pyds.live.mcp(port, io.StringIO("".join(json.dumps(x) + "\n" for x in lines)), out)
    return [json.loads(x) for x in out.getvalue().splitlines()]
