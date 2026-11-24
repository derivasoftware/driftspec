"""driftspec-lsp: a language server for .ds files, on the Python face (DS-85, a spike).

    python -m driftspec.lsp        # speaks LSP on stdin/stdout

The workspace is every .ds file under the root the editor opens, with the buffers it holds on
top: unsaved text is what is checked. On every change the workspace is loaded again in place and
each file gets its diagnostics. A word under the cursor leads to the entities so named; hover says
what one is; completion offers what the grammar takes at the cursor and the names the model declares.

The server is a door (DS-422): a DesignServer owns the protocol and one Editor per design, and every
feature is a method. Nothing is kept at the module's level but what never changes.
"""

from __future__ import annotations

import json
import os
import re
from urllib.parse import unquote, urlparse

from lsprotocol import types as lsp
from pygls.lsp.server import LanguageServer

import driftspec
from driftspec.browser import open_in_browser

WORD = re.compile(r"[A-Za-z_][\w-]*(?:(?:::|@)[A-Za-z_][\w-]*)*")

# What an editor shows a finding as, read from the severity table (DS-386): each severity by its editor mapping, so
# no severity is named here; the words on the right are the protocol's own.
EDITOR = {"error": lsp.DiagnosticSeverity.Error, "warning": lsp.DiagnosticSeverity.Warning,
          "information": lsp.DiagnosticSeverity.Information, "hint": lsp.DiagnosticSeverity.Hint}
SEVERITY = {s["name"]: EDITOR[s["editor"]] for s in driftspec.severities()}


class Kinds:
    """The kinds of the language the door names, in one place (DS-422). The metamodel says no kind's category yet
    (DS-427): until it does, these are written here once, and a test holds every kind named to the metamodel."""

    STRUCTURE = frozenset({"package", "module", "class", "enum", "function", "method", "attribute", "interface", "operation"})
    ARTEFACT = frozenset({"requirement", "constraint", "verification", "usecase"})
    DRAWN = frozenset({"diagram", "sequence"})  # what a line written in it draws as, beside the structure and the artefacts
    SYMBOL = {"package": lsp.SymbolKind.Package, "module": lsp.SymbolKind.Module, "class": lsp.SymbolKind.Class,
              "function": lsp.SymbolKind.Function, "method": lsp.SymbolKind.Method, "interface": lsp.SymbolKind.Interface,
              "requirement": lsp.SymbolKind.Key, "usecase": lsp.SymbolKind.Event}
    HOVER = ("title", "params", "returns", "type", "status")  # the fields hover says, in this order

    @classmethod
    def named(cls) -> set[str]:
        """Every kind the door names."""
        return set(cls.STRUCTURE | cls.ARTEFACT | cls.DRAWN | set(cls.SYMBOL))


def path_of(uri: str) -> str:
    return os.path.realpath(unquote(urlparse(uri).path))


def uri_of(path: str) -> str:
    return "file://" + os.path.abspath(path)


def holds_ds(directory: str) -> bool:
    """Whether a folder holds .ds files of its own, by the rule the engine reads them with (driftspec.files)."""
    return bool(driftspec.files(directory, nested=False))


def location_of(e: driftspec.Entity) -> lsp.Location:
    p = lsp.Position(max(e.location.line - 1, 0), max(e.location.column - 1, 0))
    return lsp.Location(uri=uri_of(e.location.file), range=lsp.Range(p, p))


def optional(rest: tuple) -> tuple[int, str]:
    """The width to draw at and the word the editor sees under the cursor, from what pygls passes on."""
    width = int(rest[0]) if len(rest) > 0 and rest[0] else 0
    typed = str(rest[1]) if len(rest) > 1 and rest[1] else ""
    return width, typed


class Editor:
    """One design as an editor works on it: its folder, the workspace kept loaded and reloaded in place (DS-420), the
    model read from it once per version and only when a request asks, and the files that carry diagnostics."""

    def __init__(self, root: str):
        self.root: str = root
        self.workspace: driftspec.Workspace | None = None
        self.model: driftspec.Model | None = None  # the version's model, read when a request asks
        self.published: set[str] = set()           # the files that carry diagnostics

    def files(self, ls: LanguageServer) -> dict[str, str]:
        """The design's files, the buffers on top: unsaved text is what is checked."""
        files = {os.path.realpath(p): text for p, text in driftspec.read(self.root).items()}
        for uri, doc in ls.workspace.text_documents.items():  # the buffers, saved or not, win over the disk
            path = path_of(uri)
            if path.endswith(".ds") and path.startswith(self.root + os.sep):
                files[path] = doc.source
        return files

    def refresh(self, ls: LanguageServer) -> None:
        """The workspace loaded again with the buffers on top, in place, and each file's diagnostics from its check:
        the model the requests read is read again only when one asks."""
        files = self.files(ls)
        if self.workspace is not None:
            self.workspace.reload(files)
        else:
            self.workspace = driftspec.Workspace(files)  # kept: the findings, the drawings and the viewer ask it
        self.model = None
        by_file: dict[str, list[lsp.Diagnostic]] = {path: [] for path in files}
        for f in self.workspace.report().findings:
            line, col = max(f.location.line - 1, 0), max(f.location.column - 1, 0)
            by_file.setdefault(f.location.file, []).append(
                lsp.Diagnostic(
                    range=lsp.Range(lsp.Position(line, col), lsp.Position(line, col + 1)),
                    message=f.message,
                    severity=SEVERITY[f.severity],
                    code=f.code,
                    source="ds",
                )
            )
        for path in set(by_file) | self.published:  # a file that is clean now is cleared
            ls.text_document_publish_diagnostics(lsp.PublishDiagnosticsParams(uri=uri_of(path), diagnostics=by_file.get(path, [])))
        self.published = {p for p, d in by_file.items() if d}

    def typed(self) -> driftspec.Model:
        """The version's model, read from the engine the first time a request asks for it."""
        if self.model is None:
            self.model = self.workspace.model()
        return self.model

    def entities_for(self, word: str) -> list[driftspec.Entity]:
        """The entities a word names: the one with that identity, or those whose name it ends with."""
        model = self.typed()
        if not word:
            return []
        exact = model.find(word)
        if exact:
            return [exact]
        tail = word.split("::")[-1].split("@")[-1]
        return [e for e in model.named(tail) if e.id.endswith(word) or e.name == word]

    def enclosing(self, path: str, line: int) -> driftspec.Entity | None:
        """The declaration a line of a file is written in: the nearest one that opens above it."""
        model = self.typed()
        candidates = [e for e in model.in_file(path) if e.kind in Kinds.STRUCTURE | Kinds.ARTEFACT | Kinds.DRAWN
                      and e.location.line - 1 <= line]
        if not candidates:
            return None
        best = max(candidates, key=lambda e: (e.location.line, e.location.column))
        if best.kind == "sequence":  # a sequence draws through its subject
            subject = model.find(best.fields.get("subject", "")) or model.find(best.scope)
            return subject or best
        return best

    def subject_of(self, word: str) -> tuple[str, str]:
        """('diagram', name) when the word names a diagram, ('entity', id) when it names an entity, or ('', '')."""
        model = self.typed()
        if model.find("diagram:" + word) or model.find(word) and model.find(word).kind == "diagram":
            return "diagram", word.removeprefix("diagram:")
        found = self.entities_for(word)
        return ("entity", found[0].id) if found else ("", "")

    def with_focus(self, files: dict[str, str], identity: str) -> tuple[dict[str, str], str]:
        """The workspace with a diagram of one entity and what it relates to, written on the fly; a member is drawn
        as the class it belongs to."""
        model = self.typed()
        entity = model.find(identity)
        if entity:  # a member, an outcome, a rule in a member: drawn as what holds it (DS-122)
            entity = model.owner(entity)
            identity = entity.id
        kind = "requirement" if entity and entity.kind in Kinds.ARTEFACT else "class"
        name = "__Focus"
        files = dict(files)
        files["__focus__.ds"] = f"diagram {name} {{\n  kind: {kind}\n  include: {identity}\n  neighbours: 1\n}}\n"
        return files, name


class DesignServer:
    """The language server: the protocol, and one Editor per design a document belongs to. Every feature is a method;
    each finds the Editor of its document and answers from it."""

    def __init__(self):
        self.ls = LanguageServer("driftspec-lsp", "0.0.1")
        self.editors: dict[str, Editor] = {}  # every design open, by its folder
        for feature in (lsp.TEXT_DOCUMENT_DID_OPEN, lsp.TEXT_DOCUMENT_DID_CHANGE, lsp.TEXT_DOCUMENT_DID_SAVE):
            self.ls.feature(feature)(self.handler(self.changed))
        self.ls.feature(lsp.TEXT_DOCUMENT_DEFINITION)(self.handler(self.definition))
        self.ls.feature(lsp.TEXT_DOCUMENT_REFERENCES)(self.handler(self.references))
        self.ls.feature(lsp.TEXT_DOCUMENT_HOVER)(self.handler(self.hover))
        self.ls.feature(lsp.TEXT_DOCUMENT_COMPLETION)(self.handler(self.completion))
        self.ls.feature(lsp.TEXT_DOCUMENT_DOCUMENT_SYMBOL)(self.handler(self.symbols))
        self.ls.command("driftspec.draw")(self.command(self.draw))
        self.ls.command("driftspec.view")(self.command(self.view))

    @staticmethod
    def handler(feature):
        """A feature as pygls registers it: a function of its own (pygls marks what it registers), taking the server
        and the request's parameters."""
        def handle(ls: LanguageServer, params):
            return feature(ls, params)

        return handle

    @staticmethod
    def command(feature):
        """A command as pygls registers it: a function of its own, taking the server and the command's arguments."""
        def run(ls: LanguageServer, *args):
            return feature(ls, *args)

        return run

    def workspace_of(self, ls: LanguageServer, path: str) -> str:
        """The design a file belongs to: from its folder up while the folder above holds .ds files of its own, never
        past the project root. design/ds/scene.ds is design/; examples/hola/core/x.ds is examples/hola/, apart from
        every other example."""
        top = os.path.realpath(ls.workspace.root_path or "/")
        here = os.path.dirname(path)
        while True:
            up = os.path.dirname(here)
            if up == here or not up.startswith(top) or not holds_ds(up):
                return here
            here = up

    def editor_of(self, ls: LanguageServer, uri: str) -> Editor:
        """The Editor of the design a document belongs to, loaded if it is not yet."""
        ws = self.workspace_of(ls, path_of(uri))
        if ws not in self.editors:
            self.editors[ws] = Editor(ws)
            self.editors[ws].refresh(ls)
        return self.editors[ws]

    def word_at(self, ls: LanguageServer, uri: str, position: lsp.Position) -> str:
        lines = ls.workspace.get_text_document(uri).lines
        line = lines[position.line] if position.line < len(lines) else ""
        for m in WORD.finditer(line):
            if m.start() <= position.character <= m.end():
                return m.group(0)
        return ""

    def subject_at(self, ls: LanguageServer, editor: Editor, uri: str, line: int, character: int, typed: str = "") -> tuple[str, str]:
        """('diagram', name) for a diagram under the cursor, ('entity', id) for an entity, or ('', word). The editor may
        send the word it sees under the cursor too: it is tried when the position finds nothing (a column counted in
        bytes against one counted in characters, past an accent or an arrow)."""
        for word in (self.word_at(ls, uri, lsp.Position(line, character)), WORD.search(typed or "") and WORD.search(typed).group(0) or ""):
            what, name = editor.subject_of(word)
            if what:
                return what, name
        inside = editor.enclosing(path_of(uri), line)  # no name under the cursor: what it is written in
        if inside:
            return ("diagram", inside.name) if inside.kind == "diagram" else ("entity", inside.id)
        return "", self.word_at(ls, uri, lsp.Position(line, character)) or typed

    # ---- the features
    def changed(self, ls: LanguageServer, params) -> None:
        ws = self.workspace_of(ls, path_of(params.text_document.uri))
        if ws not in self.editors:
            self.editors[ws] = Editor(ws)
        self.editors[ws].refresh(ls)

    def definition(self, ls: LanguageServer, params: lsp.DefinitionParams) -> list[lsp.Location]:
        editor = self.editor_of(ls, params.text_document.uri)
        return [location_of(e) for e in editor.entities_for(self.word_at(ls, params.text_document.uri, params.position))]

    def references(self, ls: LanguageServer, params: lsp.ReferenceParams) -> list[lsp.Location]:
        editor = self.editor_of(ls, params.text_document.uri)
        model = editor.typed()
        out = []
        for e in editor.entities_for(self.word_at(ls, params.text_document.uri, params.position)):
            for r in model.relations_of(e.id):
                if r.target == e.id and r.kind != "contains" and r.location.file:
                    p = lsp.Position(max(r.location.line - 1, 0), max(r.location.column - 1, 0))
                    out.append(lsp.Location(uri=uri_of(r.location.file), range=lsp.Range(p, p)))
        return out

    def hover(self, ls: LanguageServer, params: lsp.HoverParams) -> lsp.Hover | None:
        editor = self.editor_of(ls, params.text_document.uri)
        found = editor.entities_for(self.word_at(ls, params.text_document.uri, params.position))
        if not found:
            return None
        parts = []
        for e in found[:3]:
            lines = [f"**{e.kind}** `{e.id}`"]
            for key in Kinds.HOVER:
                if e.fields.get(key):
                    lines.append(f"{key}: {e.fields[key]}")
            parts.append("  \n".join(lines))
        return lsp.Hover(contents=lsp.MarkupContent(kind=lsp.MarkupKind.Markdown, value="\n\n---\n\n".join(parts)))

    def completion(self, ls: LanguageServer, params: lsp.CompletionParams) -> lsp.CompletionList:
        editor = self.editor_of(ls, params.text_document.uri)
        doc = ls.workspace.get_text_document(params.text_document.uri)
        items = [
            lsp.CompletionItem(label=w, kind=lsp.CompletionItemKind.Keyword)
            for w in driftspec.expected_at(doc.source, params.position.line, params.position.character)
            if w not in ("key: value",)
        ]
        model = editor.typed()  # the names the model declares that a reference can name, as the metamodel says (DS-122)
        seen = set()
        named = model.named_kinds()
        for e in model.entities.values():
            if e.kind in named and not e.expected and e.name not in seen:
                seen.add(e.name)
                items.append(lsp.CompletionItem(label=e.name, kind=lsp.CompletionItemKind.Class, detail=e.kind))
        return lsp.CompletionList(is_incomplete=False, items=items)

    def symbols(self, ls: LanguageServer, params: lsp.DocumentSymbolParams) -> list[lsp.SymbolInformation]:
        model = self.editor_of(ls, params.text_document.uri).typed()
        path = path_of(params.text_document.uri)
        # every declaration of the file; what a behaviour lays out (messages, frames) is no declaration (DS-122)
        return [lsp.SymbolInformation(name=e.name, kind=Kinds.SYMBOL.get(e.kind, lsp.SymbolKind.Object), location=location_of(e))
                for e in model.in_file(path) if not e.expected and not model.laid_out(e.kind)]

    def draw(self, ls: LanguageServer, uri: str, line: int, character: int, *rest) -> dict:
        """rest: the width to draw at and the word the editor sees under the cursor, both optional (pygls counts
        arguments strictly, so the optional ones come through *rest)."""
        width, typed = optional(rest)
        line, character = int(line), int(character)
        editor = self.editor_of(ls, uri)
        what, name = self.subject_at(ls, editor, uri, line, character, typed)
        if not what:
            return {"title": "", "text": f"nothing in {editor.root} is named '{name}': put the cursor on a diagram or an entity"}
        if what == "entity":  # a diagram written for the entity: a workspace of its own for this drawing
            files, diagram = editor.with_focus(editor.files(ls), name)
            text = driftspec.render(files, diagram, "text", width)
        else:
            text = editor.workspace.render(name, "text", width)
        return {"title": name, "text": text or f"{name} draws nothing"}

    def view(self, ls: LanguageServer, uri: str, line: int, character: int, *rest) -> dict:
        _, typed = optional(rest)
        line, character = int(line), int(character)
        editor = self.editor_of(ls, uri)
        what, name = self.subject_at(ls, editor, uri, line, character, typed)
        if what == "diagram":  # framed on what the diagram shows, as ds render <ws> <diagram> does
            ir = editor.workspace.render(name, "ir", 0)
            focus = [n["id"] for n in json.loads(ir).get("nodes", [])] if ir else []
        else:
            focus = [name] if name else []
        html = editor.workspace.page(os.path.basename(editor.root), focus)
        out = os.path.join(os.path.expanduser("~/.cache/driftspec"), "view.html")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as fh:
            fh.write(html)
        open_in_browser(out)
        return {"path": out, "focus": focus}


def main() -> None:
    DesignServer().ls.start_io()


if __name__ == "__main__":
    main()
