"""The contract of the live session, declared once: each operation with its route, what the session
answers to it, the MCP tool an agent calls it by and the word of the command line that calls it.

The HTTP routes (live_http), the MCP tools (live_mcp) and the command line (live) are read from this
table, so a new operation is one row and the three cannot disagree. What the contract publishes
(GET /contract) is described from it too.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

PORT = 4747

# The fields of the state, in the order a client reads them: the contract says them and state() gives
# exactly these, so adding one is a new version a client can see (DS-123).
STATE = ["contract", "workspace", "title", "version", "boot", "findings", "coverage", "counts", "history", "selection",
         "error", "notes", "highlights", "rules", "comments", "isolated"]

VERSION = 5  # 5: the coverage, kept apart from the findings (DS-386); 4: what changed and the baseline left with ds diff
# (DS-277); 3: seen notes, comments, isolation

TONES = ["accent", "info", "ok", "bad", "req", "you"]


@dataclass(frozen=True)
class Tool:
    """An operation as an MCP tool: its name, what an agent reads of it, its arguments as a JSON schema, the
    body its arguments make, and what of the answer it gives back."""

    name: str
    description: str
    properties: dict
    required: tuple[str, ...]
    rank: int                                       # where tools/list puts it
    body: Callable[[dict], dict | None] = lambda a: None  # None: the route is a GET
    after: Callable[[dict], object] = lambda answer: answer

    def schema(self) -> dict:
        """The tool as tools/list describes it."""
        schema: dict = {"type": "object", "properties": self.properties}
        if self.required:
            schema["required"] = list(self.required)
        return {"name": self.name, "description": self.description, "inputSchema": schema}


@dataclass(frozen=True)
class Word:
    """An operation as a word of the command line: the body its words make, and how its answer is printed."""

    word: str
    body: Callable = lambda command: None          # None: the route is a GET
    indent: int | None = None
    show: Callable[[dict], object] = lambda answer: answer


@dataclass(frozen=True)
class Operation:
    """One operation of the session: its route, what the contract says of it, and what the session answers.
    A route the server writes itself (the page, the shell, the events) has no answer here."""

    verb: str
    path: str
    doc: str
    answer: Callable | None = None                 # GET: (live) -> object; POST: (live, body) -> object
    tool: Tool | None = None
    word: Word | None = None

    def route(self) -> str:
        return f"{self.verb} {self.path}"


S, B = {"type": "string"}, {"type": "boolean"}
LIST = {"type": "array", "items": {"type": "string"}}


class Contract:
    """The operations of the session in the order the contract lists them, and what each door reads of them."""

    def __init__(self, operations: tuple[Operation, ...]):
        self.operations = operations

    def describe(self) -> dict:
        """What GET /contract answers."""
        return {"version": VERSION, "state": STATE, "routes": {op.route(): op.doc for op in self.operations}, "tones": TONES}

    def gets(self) -> dict[str, Callable]:
        """What each GET answers as JSON, asked of the session."""
        return {op.path: op.answer for op in self.operations if op.verb == "GET" and op.answer}

    def posts(self) -> dict[str, Callable]:
        """What each POST does to the session with the body it sends, and what it answers."""
        return {op.path: op.answer for op in self.operations if op.verb == "POST" and op.answer}

    def tools(self) -> list[Operation]:
        """The operations an agent calls, in the order tools/list gives them."""
        return sorted((op for op in self.operations if op.tool), key=lambda op: op.tool.rank)

    def words(self) -> list[Operation]:
        """The operations the command line calls."""
        return [op for op in self.operations if op.word]


LIVE = Contract((
    Operation("GET", "/", "the page: the viewer that follows the session"),
    Operation("GET", "/page", "the viewer of the current model, one self-contained page"),
    Operation("GET", "/state", "the state: " + ", ".join(STATE), lambda live: live.state(),
              tool=Tool("live_state", "The live session: its version and the findings.", {}, (), 8,
                        after=lambda st: {**st, "findings": st["findings"][:30]}),
              word=Word("state", indent=1,
                        show=lambda st: {k: st[k] for k in ("version", "counts", "error")} | {"findings": st["findings"][:20]})),
    Operation("GET", "/events", "server-sent events: model (the state, on every change), focus, annotate, isolate, error"),
    Operation("GET", "/selection", "what the reader has selected", lambda live: live.selection,
              tool=Tool("live_selection", "What the reader has selected in the live viewer.", {}, (), 7),
              word=Word("selection")),
    Operation("GET", "/contract", "this contract", lambda live: LIVE.describe()),
    Operation("POST", "/selection", "{id, label}: the reader selected an identity",
              lambda live, d: live.select(d.get("id"), d.get("label", "")) or {}),
    Operation("POST", "/focus", "{id, also}: take the reader to an identity",
              lambda live, d: {"sent": d.get("id", ""), "listeners": live.focus(d.get("id", ""), d.get("also", []))},
              tool=Tool("live_focus", "Take the reader to an identity (a box, class, member, interface, requirement or sequence) in the live viewer: it is selected and framed.",
                        {"id": S, "also": LIST}, ("id",), 1, lambda a: {"id": a["id"], "also": a.get("also", [])}),
              word=Word("focus", lambda c: {"id": c.args[1], "also": c.args[2:]})),
    Operation("POST", "/note", "{id, text, tone}: a bubble on an identity; empty text removes it",
              lambda live, d: {"notes": live.note(d.get("id", ""), d.get("text", ""), d.get("tone", "accent"), d.get("frame", True))},
              tool=Tool("live_note", "Put a speech bubble on an identity in the live viewer; the same identity replaces its bubble, empty text removes it. tone: accent, info, ok, bad, req.",
                        {"id": S, "text": S, "tone": S}, ("id", "text"), 2,
                        lambda a: {"id": a["id"], "text": a["text"], "tone": a.get("tone", "accent")}),
              word=Word("note", lambda c: {"id": c.args[1], "text": " ".join(c.args[2:]), "tone": c.tone})),
    Operation("POST", "/highlight", "{ids, tone, add}: light identities; replaces the highlight unless add",
              lambda live, d: {"highlights": live.highlight(d.get("ids", []), d.get("tone", "accent"), d.get("add", False),
                                                            d.get("frame", True))},
              tool=Tool("live_highlight", "Highlight identities of any perspective in the live viewer with a tone; replaces the previous highlight unless add is true.",
                        {"ids": LIST, "tone": S, "add": B}, ("ids",), 3,
                        lambda a: {"ids": a["ids"], "tone": a.get("tone", "accent"), "add": a.get("add", False)}),
              word=Word("highlight", lambda c: {"ids": c.args[1:], "tone": c.tone, "add": c.add})),
    Operation("POST", "/clear", "{what}: notes, highlights or all", lambda live, d: live.clear(d.get("what", "all")) or {},
              tool=Tool("live_clear", "Clear the notes, the highlights or both (what: notes, highlights, all) in the live viewer.",
                        {"what": S}, (), 4, lambda a: {"what": a.get("what", "all")}),
              word=Word("clear", lambda c: {"what": c.word(1, "all")})),
    Operation("POST", "/seen", "{ids, seen}: the reader marks notes seen (or not)",
              lambda live, d: {"seen": live.seen(d.get("ids", []), d.get("seen", True))}),
    Operation("POST", "/comment", "{id, text}: the reader writes a comment on an identity for the agent",
              lambda live, d: {"comments": live.comment(d.get("id", ""), d.get("text", ""))}),
    Operation("POST", "/unread", "{mark}: the comments the agent has not read; marked read unless mark is false",
              lambda live, d: {"comments": live.unread(d.get("mark", True))},
              tool=Tool("live_comments", "What the reader wrote for the agent in the live viewer since the last call: each comment with the identity it is on; they are marked read unless peek is true. Answer with live_note on the same identity.",
                        {"peek": B}, (), 5, lambda a: {"mark": not a.get("peek", False)}),
              word=Word("comments", lambda c: {"mark": "--peek" not in c.args}, indent=1)),
    Operation("POST", "/isolate", "{ids, tell}: show only these boxes; empty shows everything",
              lambda live, d: {"isolated": live.isolate(d.get("ids", []), d.get("tell", True))},
              tool=Tool("live_isolate", "Show only these boxes in the live viewer, with what they use from outside as ghosts; an empty list shows everything.",
                        {"ids": LIST}, ("ids",), 6, lambda a: {"ids": a.get("ids", [])}),
              word=Word("isolate", lambda c: {"ids": c.args[1:]})),
))

CONTRACT = LIVE.describe()
