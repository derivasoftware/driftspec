"""The live session itself: one workspace followed version by version, and what both sides point at.

The contract (CONTRACT) is what a client relies on, whether a browser page or an editor's panel: the
routes it calls, the events it receives and the shape of the state. Notes and highlights are kept in
<workspace>/.driftspec/live.json, beside the design and never in it.
"""

from __future__ import annotations

import json
import os
import queue
import threading
import time

import driftspec

PORT = 4747

# The fields of the state, in the order a client reads them: the contract says them and state() gives
# exactly these, so adding one is a new version a client can see (DS-123).
STATE = ["contract", "workspace", "title", "version", "boot", "findings", "counts", "history", "selection",
         "error", "notes", "highlights", "rules", "comments", "isolated"]

CONTRACT = {
    "version": 4,  # 4: what changed and the baseline left with ds diff (DS-277); 3: seen notes, comments, isolation
    "state": STATE,
    "routes": {
        "GET /": "the page: the viewer that follows the session",
        "GET /page": "the viewer of the current model, one self-contained page",
        "GET /state": "the state: " + ", ".join(STATE),
        "GET /events": "server-sent events: model (the state, on every change), focus, annotate, isolate, error",
        "GET /selection": "what the reader has selected",
        "GET /contract": "this contract",
        "POST /selection": "{id, label}: the reader selected an identity",
        "POST /focus": "{id, also}: take the reader to an identity",
        "POST /note": "{id, text, tone}: a bubble on an identity; empty text removes it",
        "POST /highlight": "{ids, tone, add}: light identities; replaces the highlight unless add",
        "POST /clear": "{what}: notes, highlights or all",
        "POST /seen": "{ids, seen}: the reader marks notes seen (or not)",
        "POST /comment": "{id, text}: the reader writes a comment on an identity for the agent",
        "POST /unread": "{mark}: the comments the agent has not read; marked read unless mark is false",
        "POST /isolate": "{ids, tell}: show only these boxes; empty shows everything",
    },
    "tones": ["accent", "info", "ok", "bad", "req", "you"],
}


class Live:
    """A live session over one workspace: its model version by version, and what both sides point at."""

    def __init__(self, workspace: str):
        self.workspace: str = os.path.realpath(workspace)
        base = os.path.basename(self.workspace.rstrip("/"))
        self.title: str = os.path.basename(os.path.dirname(self.workspace)) if base in ("design", "designs", "") else base
        self.lock = threading.Lock()
        self.version = 0
        self.page = ""
        self.loaded: driftspec.Workspace | None = None  # the version shown, loaded once in the engine (DS-173)
        self.history: list[dict] = []
        self.findings: list[dict] = []
        self.counts: dict = {}
        self.selection: dict = {}
        self.notes: list[dict] = []
        self.highlights: list[dict] = []
        self.rules: list[dict] = []
        self.comments: list[dict] = []  # what the reader writes for the agent (DS-147)
        self.isolated: list[str] = []  # what the reader isolates, kept across reloads (DS-139)
        self.listeners: list[queue.Queue] = []
        self.stamp = None
        self.error = ""
        self.boot: str = str(time.time())  # a page from an older server reloads itself
        self.store: str = os.path.join(self.workspace, ".driftspec", "live.json")
        self.load()

    # ---- what both sides point at, kept beside the design
    def load(self) -> None:
        try:
            with open(self.store) as fh:
                kept = json.load(fh)
            self.notes, self.highlights = list(kept.get("notes", [])), list(kept.get("highlights", []))
            self.comments = list(kept.get("comments", []))
        except (OSError, ValueError):
            pass

    def save(self) -> None:
        try:
            os.makedirs(os.path.dirname(self.store), exist_ok=True)
            with open(self.store, "w") as fh:
                json.dump({"notes": self.notes, "highlights": self.highlights, "comments": self.comments}, fh, indent=1)
        except OSError:
            pass  # a read-only workspace keeps them for the session only

    def annotations(self, frame: list[str]) -> None:
        self.save()
        self.broadcast("annotate", {"notes": self.notes, "highlights": self.highlights, "comments": self.comments, "frame": frame})

    def focus(self, identity: str, also: list[str] | None = None) -> int:
        self.broadcast("focus", {"id": identity, "also": also or []})
        return len(self.listeners)

    def note(self, identity: str, text: str, tone: str = "accent", frame: bool = True) -> int:
        # a note the agent writes again comes back unseen (DS-102)
        self.notes = [n for n in self.notes if n["id"] != identity] + (
            [{"id": identity, "text": text, "tone": tone, "seen": False, "at": time.strftime("%H:%M:%S")}] if text else [])
        self.annotations([identity] if frame and text else [])
        return len(self.notes)

    def highlight(self, ids: list[str], tone: str = "accent", add: bool = False, frame: bool = True) -> int:
        self.highlights = ([h for h in self.highlights if h["id"] not in ids] if add else []) + [{"id": i, "tone": tone} for i in ids]
        self.annotations(list(ids) if frame else [])
        return len(self.highlights)

    def clear(self, what: str = "all") -> None:
        if what in ("all", "notes"):
            self.notes = []
        if what in ("all", "highlights"):
            self.highlights = []
        self.annotations([])

    def seen(self, ids: list[str], seen: bool = True) -> int:
        for n in self.notes:
            if n["id"] in ids:
                n["seen"] = seen
        self.annotations([])
        return sum(1 for n in self.notes if n.get("seen"))

    def comment(self, identity: str, text: str) -> int:
        self.comments.append({"n": len(self.comments) + 1, "id": identity, "text": text, "at": time.strftime("%H:%M:%S"), "read": False})
        self.annotations([])
        return len(self.comments)

    def unread(self, mark: bool = True) -> list[dict]:
        out = [c for c in self.comments if not c.get("read")]
        if mark and out:
            for c in self.comments:
                c["read"] = True
            self.annotations([])
        return out

    def isolate(self, ids: list[str], tell: bool = True) -> list[str]:
        self.isolated = list(ids)
        if tell:
            self.broadcast("isolate", {"ids": self.isolated})
        return self.isolated

    def select(self, identity: str | None, label: str = "") -> None:
        self.selection = {"id": identity, "label": label, "at": time.strftime("%H:%M:%S")}

    # ---- the model
    def files_stamp(self):
        stamp = []
        for root, dirs, files in os.walk(self.workspace):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for f in files:
                if f.endswith(".ds"):
                    p = os.path.join(root, f)
                    try:
                        st = os.stat(p)
                        stamp.append((p, st.st_mtime_ns, st.st_size))
                    except OSError:
                        pass
        return tuple(sorted(stamp))

    def rules_of(self, model, findings) -> list[dict]:
        """The rules and definitions as written, what each answers to and what breaks it (DS-108).

        Read from the model: its entities, its relations and the breaches the evaluator recorded."""
        rel = lambda path: os.path.relpath(path, self.workspace) if path.startswith("/") else path
        rules = []
        for e in sorted(model.entities.values(), key=lambda x: x.id):
            if e.expected or e.kind not in ("rule", "definition"):
                continue
            lines = [[key, c.strip()] for key in ("for", "where", "holds", "never", "if")
                     for c in e.fields.get(key, "").split("; ") if c.strip()]
            breaches = [{"bindings": [list(p) for p in b.bindings], "witness": [list(p) for p in b.witness],
                         "file": rel(b.location.file), "line": b.location.line} for b in model.breaches.get(e.id, [])]
            problems = [{"code": f.code, "message": f.message[len(e.id) + 1:]} for f in findings
                        if f.message.startswith(e.id + " ") and f.code.startswith(("E-RULE", "W-RULE", "W-DEFINITION")) and f.code != "E-RULE-BROKEN"]
            rules.append({"id": e.id, "kind": e.kind, "name": e.name, "title": e.fields.get("title", ""), "scope": e.scope.rstrip("@"),
                          "lines": lines, "requirements": [r.target for r in model.relations if r.kind == "satisfies" and r.source == e.id],
                          "broken": breaches, "problems": problems, "file": rel(e.location.file), "line": e.location.line})
        return rules

    def reload(self) -> bool:
        stamp = self.files_stamp()
        if stamp == self.stamp:
            return False
        self.stamp = stamp
        t0 = time.perf_counter()
        try:
            files = driftspec.read(self.workspace) if stamp else {}
            loaded = driftspec.Workspace(files)  # once: the model, the page and the check all ask it (DS-173)
            model = loaded.model()
            page = loaded.page(self.title, [])
            report = loaded.report()
        except Exception as exc:  # a half-written file must not stop the session
            self.error = str(exc)
            self.broadcast("error", {"message": self.error})
            return True
        with self.lock:
            first = self.version == 0
            self.loaded, self.page, self.error = loaded, page, ""
            self.version += 1
            self.findings = [{"code": f.code, "severity": f.severity, "message": f.message,
                              "file": os.path.relpath(f.location.file, self.workspace) if f.location.file.startswith("/") else f.location.file,
                              "line": f.location.line} for f in report.findings]
            self.counts = dict(report.counts)
            self.rules = self.rules_of(model, report.findings)
            if not first:
                self.history.append({"version": self.version, "at": time.strftime("%H:%M:%S"), "ms": round((time.perf_counter() - t0) * 1000)})
                self.history = self.history[-30:]
        self.broadcast("model", self.state())
        return True

    def state(self) -> dict:
        return {"contract": CONTRACT["version"], "workspace": self.workspace, "title": self.title, "version": self.version,
                "boot": self.boot, "findings": self.findings,
                "counts": self.counts, "history": self.history, "selection": self.selection, "error": self.error,
                "notes": self.notes, "highlights": self.highlights, "rules": self.rules, "comments": self.comments,
                "isolated": self.isolated}

    def broadcast(self, event: str, data: dict) -> None:
        for q in list(self.listeners):
            q.put((event, data))

    def watch(self, every: float = 0.35) -> None:
        while not getattr(self, "stopped", False):
            try:
                self.reload()
            except Exception as exc:  # keep watching whatever happens
                self.error = str(exc)
            time.sleep(every)
