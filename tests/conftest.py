"""What the Python tests share: the example they read and a language server session driven over
stdio as an editor drives it. pyds is imported from the build: PYTHONPATH carries the package beside
its native module (cmake -DDS_PYTHON=ON builds both into <build>/python)."""

import json
import os
import queue
import subprocess
import sys
import threading

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


@pytest.fixture
def hola() -> str:
    return os.path.join(ROOT, "examples", "hola")


@pytest.fixture
def design() -> str:
    return os.path.join(ROOT, "design")


def uri(path):
    return "file://" + os.path.realpath(path)


class Session:
    def __init__(self, root: str):
        log = open(os.environ.get("PYDS_LSP_LOG", os.devnull), "w")  # the server's own log, when asked for
        self.p = subprocess.Popen([sys.executable, "-m", "pyds.lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=log, env=os.environ)
        self.n = 0
        self.inbox: queue.Queue = queue.Queue()
        threading.Thread(target=self.read, daemon=True).start()
        self.call("initialize", {"processId": None, "rootUri": "file://" + root, "capabilities": {}})
        self.send("initialized", {})

    def send(self, method, params, request=False):
        msg = {"jsonrpc": "2.0", "method": method, "params": params}
        if request:
            self.n += 1
            msg["id"] = self.n
        body = json.dumps(msg).encode()
        self.p.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.p.stdin.flush()
        return msg.get("id")

    def read(self):  # on its own thread: one message after another into the queue, None at the end
        while True:
            headers = {}
            while True:
                line = self.p.stdout.readline()
                if not line:
                    self.inbox.put(None)
                    return
                line = line.decode().strip()
                if not line:
                    break
                key, value = line.split(": ", 1)
                headers[key] = value
            self.inbox.put(json.loads(self.p.stdout.read(int(headers["Content-Length"]))))

    def receive(self, timeout=20.0):
        try:
            m = self.inbox.get(timeout=timeout)
        except queue.Empty:
            raise TimeoutError("the server answered nothing in %.0f s" % timeout) from None
        if m is None:
            raise EOFError("the server closed its output")
        return m

    def until(self, wanted):
        seen = []
        while True:
            try:
                m = self.receive()
            except TimeoutError as e:
                raise TimeoutError(f"{e}; it sent {json.dumps(seen)[:3000]}") from None
            if wanted(m):
                return m
            seen.append(m)

    def call(self, method, params):
        rid = self.send(method, params, request=True)
        reply = self.until(lambda m: m.get("id") == rid)
        assert "error" not in reply, reply["error"]
        return reply.get("result")

    def close(self):
        self.call("shutdown", None)
        self.send("exit", None)
        self.p.wait(5)


@pytest.fixture
def lsp():
    """A factory of sessions, each closed at the end of the test."""
    opened = []

    def start(root: str) -> Session:
        opened.append(Session(root))
        return opened[-1]

    yield start
    for s in opened:
        s.close()
