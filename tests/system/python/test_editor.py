"""SREQ-00016-1 from outside: an editor's session with the language server over the design of ds, the
server run in this process on the session's bytes as it runs on an editor's pipe."""

import io
import json
import os
import shutil

import driftspec.lsp

from conftest import uri


def framed(*messages) -> bytes:
    out = b""
    for n, (method, params, request) in enumerate(messages):
        msg = {"jsonrpc": "2.0", "method": method, "params": params}
        if request:
            msg["id"] = n
        body = json.dumps(msg).encode()
        out += b"Content-Length: %d\r\n\r\n" % len(body) + body
    return out


def unframed(data: bytes) -> list[dict]:
    out = []
    while data:
        head, _, rest = data.partition(b"\r\n\r\n")
        length = int(next(l for l in head.decode().split("\r\n") if l.startswith("Content-Length")).split(": ")[1])
        out.append(json.loads(rest[:length]))
        data = rest[length:]
    return out


class Pipe(io.BytesIO):
    """The editor's end: the server closes it when the session ends, what it wrote stays readable."""

    def close(self) -> None:
        pass


class TestEditorSession:
    def test_an_editor_gets_what_it_types_wrong_and_the_drawing_of_what_it_points_at(self, design, tmp_path):
        root = os.path.join(tmp_path, "design")
        shutil.copytree(design, root)
        path = os.path.join(root, "ds", "api.ds")
        text = open(path).read()
        lines = text.splitlines()
        line = next(i for i, l in enumerate(lines) if "class Api {" in l)
        doc = {"uri": uri(path)}
        session = framed(
            ("initialize", {"processId": None, "rootUri": "file://" + root, "capabilities": {}}, True),
            ("initialized", {}, False),
            ("textDocument/didOpen", {"textDocument": {**doc, "languageId": "driftspec", "version": 1, "text": text}}, False),
            ("textDocument/didChange", {"textDocument": {**doc, "version": 2},
                                        "contentChanges": [{"text": text.replace("class Api {", "clas Api {")}]}, False),
            ("textDocument/didChange", {"textDocument": {**doc, "version": 3}, "contentChanges": [{"text": text}]}, False),
            ("workspace/executeCommand", {"command": "driftspec.draw",
                                          "arguments": [uri(path), line, lines[line].index("Api") + 1]}, True),
            ("shutdown", None, True),
            ("exit", None, False),
        )
        out = Pipe()
        try:
            driftspec.lsp.server.start_io(io.BytesIO(session), out)
        except SystemExit:
            pass
        replies = unframed(out.getvalue())
        mine = [m["params"]["diagnostics"] for m in replies
                if m.get("method") == "textDocument/publishDiagnostics" and m["params"]["uri"] == uri(path)]
        assert any(d["code"] == "E-SYNTAX" for d in mine[1]) and mine[-1] == []  # typed wrong, then right again
        drawn = next(m["result"] for m in replies if m.get("id") == 5)
        assert drawn["title"] == "driftspec@ds@api::Api" and "check" in drawn["text"]
