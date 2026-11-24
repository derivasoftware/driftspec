"""The live session itself: one workspace followed version by version, and what both sides point at.

The session (Live) holds the version shown and composes three parts, each with one job: the notes,
highlights and comments both sides keep (Annotations, in <workspace>/.driftspec/live.json, beside the
design and never in it), the pages that follow it (Events) and the .ds files it watches (Watcher). The
contract a client relies on is declared once, in live_contract.
"""

from __future__ import annotations

import json
import os
import queue
import threading
import time
from typing import Callable

import driftspec
from driftspec.live_contract import CONTRACT, PORT, STATE, VERSION

__all__ = ["CONTRACT", "PORT", "STATE", "Annotations", "Events", "Live", "Watcher"]


class Annotations:
    """What both sides point at and keep: the notes, the highlights and the comments, stored beside the design."""

    def __init__(self, store: str):
        self.store: str = store                 # <workspace>/.driftspec/live.json
        self.notes: list[dict] = []
        self.highlights: list[dict] = []
        self.comments: list[dict] = []          # what the reader writes for the agent (DS-147)
        self.load()

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

    def told(self, frame: list[str]) -> dict:
        """What a page is told when they change, with the identities it frames."""
        return {"notes": self.notes, "highlights": self.highlights, "comments": self.comments, "frame": frame}

    def note(self, identity: str, text: str, tone: str) -> int:
        # a note the agent writes again comes back unseen (DS-102)
        self.notes = [n for n in self.notes if n["id"] != identity] + (
            [{"id": identity, "text": text, "tone": tone, "seen": False, "at": time.strftime("%H:%M:%S")}] if text else [])
        return len(self.notes)

    def highlight(self, ids: list[str], tone: str, add: bool) -> int:
        self.highlights = ([h for h in self.highlights if h["id"] not in ids] if add else []) + [{"id": i, "tone": tone} for i in ids]
        return len(self.highlights)

    def clear(self, what: str) -> None:
        if what in ("all", "notes"):
            self.notes = []
        if what in ("all", "highlights"):
            self.highlights = []

    def seen(self, ids: list[str], seen: bool) -> int:
        for n in self.notes:
            if n["id"] in ids:
                n["seen"] = seen
        return sum(1 for n in self.notes if n.get("seen"))

    def comment(self, identity: str, text: str) -> int:
        self.comments.append({"n": len(self.comments) + 1, "id": identity, "text": text, "at": time.strftime("%H:%M:%S"), "read": False})
        return len(self.comments)

    def unread(self) -> list[dict]:
        return [c for c in self.comments if not c.get("read")]

    def mark_read(self) -> None:
        for c in self.comments:
            c["read"] = True


class Events:
    """The pages that follow the session: each one a queue that every event is put on."""

    def __init__(self):
        self.listeners: list[queue.Queue] = []  # one per page following the session

    def listen(self) -> queue.Queue:
        q: queue.Queue = queue.Queue()
        self.listeners.append(q)
        return q

    def leave(self, q: queue.Queue) -> None:
        if q in self.listeners:
            self.listeners.remove(q)

    def broadcast(self, event: str, data: dict) -> None:
        for q in list(self.listeners):
            q.put((event, data))

    def count(self) -> int:
        return len(self.listeners)


class Watcher:
    """The .ds files of a workspace, watched: what they were at the last look, and whether a save changed them."""

    def __init__(self, workspace: str):
        self.workspace: str = workspace
        self.stamp: tuple | None = None         # what the .ds files were at the last look; none before the first

    def files_stamp(self) -> tuple:
        """What the .ds files are now, the ones the engine reads (driftspec.files): a save changes it."""
        stamp = []
        for p in driftspec.files(self.workspace):
            try:
                st = os.stat(p)
                stamp.append((p, st.st_mtime_ns, st.st_size))
            except OSError:
                pass  # gone between the listing and the look
        return tuple(stamp)

    def changed(self) -> bool:
        """Whether the files changed since the last look; the first look is a change."""
        stamp = self.files_stamp()
        if stamp == self.stamp:
            return False
        self.stamp = stamp
        return True

    def follow(self, tick: Callable[[], None], every: float, stopped: Callable[[], bool]) -> None:
        """Call tick every so often until stopped."""
        while not stopped():
            tick()
            time.sleep(every)


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
        self.rules: list[dict] = []
        self.isolated: list[str] = []  # what the reader isolates, kept across reloads (DS-139)
        self.error = ""
        self.boot: str = str(time.time())  # a page from an older server reloads itself
        self.annotations = Annotations(os.path.join(self.workspace, ".driftspec", "live.json"))
        self.events = Events()
        self.watcher = Watcher(self.workspace)

    # ---- what both sides point at: changed, kept and told to every page
    def annotated(self, frame: list[str]) -> None:
        self.annotations.save()
        self.events.broadcast("annotate", self.annotations.told(frame))

    def focus(self, identity: str, also: list[str] | None = None) -> int:
        self.events.broadcast("focus", {"id": identity, "also": also or []})
        return self.events.count()

    def note(self, identity: str, text: str, tone: str = "accent", frame: bool = True) -> int:
        count = self.annotations.note(identity, text, tone)
        self.annotated([identity] if frame and text else [])
        return count

    def highlight(self, ids: list[str], tone: str = "accent", add: bool = False, frame: bool = True) -> int:
        count = self.annotations.highlight(ids, tone, add)
        self.annotated(list(ids) if frame else [])
        return count

    def clear(self, what: str = "all") -> None:
        self.annotations.clear(what)
        self.annotated([])

    def seen(self, ids: list[str], seen: bool = True) -> int:
        count = self.annotations.seen(ids, seen)
        self.annotated([])
        return count

    def comment(self, identity: str, text: str) -> int:
        count = self.annotations.comment(identity, text)
        self.annotated([])
        return count

    def unread(self, mark: bool = True) -> list[dict]:
        out = self.annotations.unread()
        if mark and out:
            self.annotations.mark_read()
            self.annotated([])
        return out

    def isolate(self, ids: list[str], tell: bool = True) -> list[str]:
        self.isolated = list(ids)
        if tell:
            self.events.broadcast("isolate", {"ids": self.isolated})
        return self.isolated

    def select(self, identity: str | None, label: str = "") -> None:
        self.selection = {"id": identity, "label": label, "at": time.strftime("%H:%M:%S")}

    # ---- the model
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
        if not self.watcher.changed():
            return False
        t0 = time.perf_counter()
        try:
            files = driftspec.read(self.workspace) if self.watcher.stamp else {}
            loaded = driftspec.Workspace(files)  # once: the model, the page and the check all ask it (DS-173)
            model = loaded.model()
            page = loaded.page(self.title, [])
            report = loaded.report()
        except Exception as exc:  # a half-written file must not stop the session
            self.error = str(exc)
            self.events.broadcast("error", {"message": self.error})
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
        self.events.broadcast("model", self.state())
        return True

    def state(self) -> dict:
        a = self.annotations
        return {"contract": VERSION, "workspace": self.workspace, "title": self.title, "version": self.version,
                "boot": self.boot, "findings": self.findings,
                "counts": self.counts, "history": self.history, "selection": self.selection, "error": self.error,
                "notes": a.notes, "highlights": a.highlights, "rules": self.rules, "comments": a.comments,
                "isolated": self.isolated}

    def tick(self) -> None:
        """One look at the files: a reload when they changed, an error kept whatever happens."""
        try:
            self.reload()
        except Exception as exc:  # keep watching whatever happens
            self.error = str(exc)

    def watch(self, every: float = 0.35) -> None:
        self.watcher.follow(self.tick, every, lambda: getattr(self, "stopped", False))
