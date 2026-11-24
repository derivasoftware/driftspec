"""driftspec-lsp: a language server for .ds files, on the Python face (DS-85, a spike).

    python -m driftspec.lsp        # speaks LSP on stdin/stdout

The workspace is every .ds file under the root the editor opens, with the buffers it holds on
top: unsaved text is what is checked. On every change the whole workspace is checked again (the
engine takes some 30 ms on a design of 600 entities) and each file gets its diagnostics. A word
under the cursor leads to the entities so named; hover says what one is; completion offers what
the grammar takes at the cursor and the names the model declares.
"""

from __future__ import annotations

import os
import re
from urllib.parse import unquote, urlparse

from lsprotocol import types as lsp
from pygls.lsp.server import LanguageServer

import driftspec

server = LanguageServer("driftspec-lsp", "0.0.1")
WORD = re.compile(r"[A-Za-z_][\w-]*(?:(?:::|@)[A-Za-z_][\w-]*)*")
state: dict = {"model": None}       # the model of the workspace the request is about
models: dict[str, driftspec.Model] = {}  # every workspace open, by its directory
workspaces: dict[str, driftspec.Workspace] = {}  # the same, loaded once in the engine for drawing and the viewer (DS-173)
published: dict[str, set[str]] = {}       # the files of each workspace that carry diagnostics

# What an editor shows a finding as, read from the severity table (DS-386): each severity by its editor mapping, so
# no severity is named here; the words on the right are the protocol's own.
EDITOR = {"error": lsp.DiagnosticSeverity.Error, "warning": lsp.DiagnosticSeverity.Warning,
          "information": lsp.DiagnosticSeverity.Information, "hint": lsp.DiagnosticSeverity.Hint}
SEVERITY = {s["name"]: EDITOR[s["editor"]] for s in driftspec.severities()}


def path_of(uri: str) -> str:
    return os.path.realpath(unquote(urlparse(uri).path))


def uri_of(path: str) -> str:
    return "file://" + os.path.abspath(path)


def holds_ds(directory: str) -> bool:
    """Whether a folder holds .ds files of its own, by the rule the engine reads them with (driftspec.files)."""
    return bool(driftspec.files(directory, nested=False))


def workspace_of(ls: LanguageServer, path: str) -> str:
    """The design a file belongs to: from its folder up while the folder above holds .ds files of
    its own, never past the project root. design/ds/scene.ds is design/; examples/hola/core/x.ds is
    examples/hola/, apart from every other example."""
    top = os.path.realpath(ls.workspace.root_path or "/")
    here = os.path.dirname(path)
    while True:
        up = os.path.dirname(here)
        if up == here or not up.startswith(top) or not holds_ds(up):
            return here
        here = up


def workspace_files(ls: LanguageServer, ws: str) -> dict[str, str | bytes]:
    files = {os.path.realpath(p): text for p, text in driftspec.read(ws).items()}
    for uri, doc in ls.workspace.text_documents.items():  # the buffers, saved or not, win over the disk
        path = path_of(uri)
        if path.endswith(".ds") and path.startswith(ws + os.sep):
            files[path] = doc.source
    return files


def use(ls: LanguageServer, uri: str) -> str:
    """The workspace of a document, loaded if it is not yet: the model the request answers from."""
    ws = workspace_of(ls, path_of(uri))
    if ws not in models:
        refresh(ls, ws)
    state["model"] = models[ws]
    return ws


def refresh(ls: LanguageServer, ws: str) -> None:
    files = workspace_files(ls, ws)
    workspaces[ws] = driftspec.Workspace(files)  # loaded once: the findings, the drawings and the viewer ask it
    models[ws] = state["model"] = workspaces[ws].model()
    by_file: dict[str, list[lsp.Diagnostic]] = {path: [] for path in files}
    for f in state["model"].findings:
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
    for path in set(by_file) | published.get(ws, set()):  # a file that is clean now is cleared
        ls.text_document_publish_diagnostics(lsp.PublishDiagnosticsParams(uri=uri_of(path), diagnostics=by_file.get(path, [])))
    published[ws] = {p for p, d in by_file.items() if d}


def word_at(ls: LanguageServer, uri: str, position: lsp.Position) -> str:
    line = ls.workspace.get_text_document(uri).lines[position.line] if position.line < len(ls.workspace.get_text_document(uri).lines) else ""
    for m in WORD.finditer(line):
        if m.start() <= position.character <= m.end():
            return m.group(0)
    return ""


def entities_for(word: str) -> list[driftspec.Entity]:
    model: driftspec.Model = state["model"]
    if not model or not word:
        return []
    exact = model.find(word)
    if exact:
        return [exact]
    tail = word.split("::")[-1].split("@")[-1]
    return [e for e in model.named(tail) if e.id.endswith(word) or e.name == word]


def location_of(e: driftspec.Entity) -> lsp.Location:
    p = lsp.Position(max(e.location.line - 1, 0), max(e.location.column - 1, 0))
    return lsp.Location(uri=uri_of(e.location.file), range=lsp.Range(p, p))


@server.feature(lsp.TEXT_DOCUMENT_DID_OPEN)
@server.feature(lsp.TEXT_DOCUMENT_DID_CHANGE)
@server.feature(lsp.TEXT_DOCUMENT_DID_SAVE)
def changed(ls: LanguageServer, params) -> None:
    refresh(ls, workspace_of(ls, path_of(params.text_document.uri)))


@server.feature(lsp.TEXT_DOCUMENT_DEFINITION)
def definition(ls: LanguageServer, params: lsp.DefinitionParams) -> list[lsp.Location]:
    use(ls, params.text_document.uri)
    return [location_of(e) for e in entities_for(word_at(ls, params.text_document.uri, params.position))]


@server.feature(lsp.TEXT_DOCUMENT_REFERENCES)
def references(ls: LanguageServer, params: lsp.ReferenceParams) -> list[lsp.Location]:
    use(ls, params.text_document.uri)
    model: driftspec.Model = state["model"]
    out = []
    for e in entities_for(word_at(ls, params.text_document.uri, params.position)):
        for r in model.relations_of(e.id):
            if r.target == e.id and r.kind != "contains" and r.location.file:
                p = lsp.Position(max(r.location.line - 1, 0), max(r.location.column - 1, 0))
                out.append(lsp.Location(uri=uri_of(r.location.file), range=lsp.Range(p, p)))
    return out


@server.feature(lsp.TEXT_DOCUMENT_HOVER)
def hover(ls: LanguageServer, params: lsp.HoverParams) -> lsp.Hover | None:
    use(ls, params.text_document.uri)
    found = entities_for(word_at(ls, params.text_document.uri, params.position))
    if not found:
        return None
    parts = []
    for e in found[:3]:
        lines = [f"**{e.kind}** `{e.id}`"]
        for key in ("title", "params", "returns", "type", "status"):
            if e.fields.get(key):
                lines.append(f"{key}: {e.fields[key]}")
        parts.append("  \n".join(lines))
    return lsp.Hover(contents=lsp.MarkupContent(kind=lsp.MarkupKind.Markdown, value="\n\n---\n\n".join(parts)))


@server.feature(lsp.TEXT_DOCUMENT_COMPLETION)
def completion(ls: LanguageServer, params: lsp.CompletionParams) -> lsp.CompletionList:
    use(ls, params.text_document.uri)
    doc = ls.workspace.get_text_document(params.text_document.uri)
    items = [
        lsp.CompletionItem(label=w, kind=lsp.CompletionItemKind.Keyword)
        for w in driftspec.expected_at(doc.source, params.position.line, params.position.character)
        if w not in ("key: value",)
    ]
    model: driftspec.Model = state["model"]
    if model:  # the names the model declares that a reference can name, as the metamodel says (DS-122)
        seen = set()
        named = model.named_kinds()
        for e in model.entities.values():
            if e.kind in named and not e.expected and e.name not in seen:
                seen.add(e.name)
                items.append(lsp.CompletionItem(label=e.name, kind=lsp.CompletionItemKind.Class, detail=e.kind))
    return lsp.CompletionList(is_incomplete=False, items=items)


@server.feature(lsp.TEXT_DOCUMENT_DOCUMENT_SYMBOL)
def symbols(ls: LanguageServer, params: lsp.DocumentSymbolParams) -> list[lsp.SymbolInformation]:
    use(ls, params.text_document.uri)
    model: driftspec.Model = state["model"]
    kinds = {"package": lsp.SymbolKind.Package, "module": lsp.SymbolKind.Module, "class": lsp.SymbolKind.Class,
             "function": lsp.SymbolKind.Function, "method": lsp.SymbolKind.Method, "interface": lsp.SymbolKind.Interface,
             "requirement": lsp.SymbolKind.Key, "usecase": lsp.SymbolKind.Event}
    path = path_of(params.text_document.uri)
    # every declaration of the file; what a behaviour lays out (messages, frames) is no declaration (DS-122)
    return [lsp.SymbolInformation(name=e.name, kind=kinds.get(e.kind, lsp.SymbolKind.Object), location=location_of(e))
            for e in (model.in_file(path) if model else []) if not e.expected and not model.laid_out(e.kind)]


# ---- seeing it: the diagram or the entity under the cursor, drawn as text or in the viewer ----
STRUCTURE = {"package", "module", "class", "enum", "function", "method", "attribute", "interface", "operation"}
ARTEFACT = {"requirement", "constraint", "verification", "usecase"}


def subject_at(ls: LanguageServer, uri: str, line: int, character: int, typed: str = "") -> tuple[str, str]:
    """('diagram', name) for a diagram under the cursor, ('entity', id) for an entity, or ('', word).
    The editor may send the word it sees under the cursor too: it is tried when the position finds nothing
    (a column counted in bytes against one counted in characters, past an accent or an arrow)."""
    for word in (word_at(ls, uri, lsp.Position(line, character)), WORD.search(typed or "") and WORD.search(typed).group(0) or ""):
        what, name = subject_of(word)
        if what:
            return what, name
    inside = enclosing(path_of(uri), line)  # no name under the cursor: what it is written in
    if inside:
        return ("diagram", inside.name) if inside.kind == "diagram" else ("entity", inside.id)
    return "", word_at(ls, uri, lsp.Position(line, character)) or typed


def enclosing(path: str, line: int) -> driftspec.Entity | None:
    """The declaration a line of a file is written in: the nearest one that opens above it."""
    model: driftspec.Model = state["model"]
    if not model:
        return None
    candidates = [e for e in model.in_file(path) if e.kind in STRUCTURE | ARTEFACT | {"diagram", "sequence"}
                  and e.location.line - 1 <= line]
    if not candidates:
        return None
    best = max(candidates, key=lambda e: (e.location.line, e.location.column))
    if best.kind == "sequence":  # a sequence draws through its subject
        subject = model.find(best.fields.get("subject", "")) or model.find(best.scope)
        return subject or best
    return best


def subject_of(word: str) -> tuple[str, str]:
    model: driftspec.Model = state["model"]
    if model and (model.find("diagram:" + word) or model.find(word) and model.find(word).kind == "diagram"):
        return "diagram", word.removeprefix("diagram:")
    found = entities_for(word)
    return ("entity", found[0].id) if found else ("", "")


def with_focus(files: dict[str, str | bytes], identity: str) -> tuple[dict[str, str | bytes], str]:
    """The workspace with a diagram of one entity and what it relates to, written on the fly; a member
    is drawn as the class it belongs to."""
    entity = state["model"].find(identity)
    if entity:  # a member, an outcome, a rule in a member: drawn as what holds it (DS-122)
        entity = state["model"].owner(entity)
        identity = entity.id
    kind = "requirement" if entity and entity.kind in ARTEFACT else "class"
    name = "__Focus"
    files = dict(files)
    files["__focus__.ds"] = f"diagram {name} {{\n  kind: {kind}\n  include: {identity}\n  neighbours: 1\n}}\n"
    return files, name


def optional(rest: tuple) -> tuple[int, str]:
    width = int(rest[0]) if len(rest) > 0 and rest[0] else 0
    typed = str(rest[1]) if len(rest) > 1 and rest[1] else ""
    return width, typed


@server.command("driftspec.draw")
def draw(ls: LanguageServer, uri: str, line: int, character: int, *rest) -> dict:
    """rest: the width to draw at and the word the editor sees under the cursor, both optional
    (pygls counts arguments strictly, so the optional ones come through *rest)."""
    width, typed = optional(rest)
    line, character = int(line), int(character)
    ws = use(ls, uri)
    what, name = subject_at(ls, uri, line, character, typed)
    if not what:
        return {"title": "", "text": f"nothing in {ws} is named '{name}': put the cursor on a diagram or an entity"}
    if what == "entity":  # a diagram written for the entity: a workspace of its own for this drawing
        files, diagram = with_focus(workspace_files(ls, ws), name)
        text = driftspec.render(files, diagram, "text", width)
    else:
        text = workspaces[ws].render(name, "text", width)
    return {"title": name, "text": text or f"{name} draws nothing"}


def open_in_browser(path: str) -> None:
    import shutil
    import subprocess

    if shutil.which("wslpath") and shutil.which("explorer.exe"):  # WSL: the Windows browser
        windows = subprocess.run(["wslpath", "-w", path], capture_output=True, text=True).stdout.strip()
        subprocess.Popen(["explorer.exe", windows], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elif shutil.which("xdg-open"):
        subprocess.Popen(["xdg-open", path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


@server.command("driftspec.view")
def view(ls: LanguageServer, uri: str, line: int, character: int, *rest) -> dict:
    _, typed = optional(rest)
    import json as _json

    line, character = int(line), int(character)
    ws = use(ls, uri)
    what, name = subject_at(ls, uri, line, character, typed)
    if what == "diagram":  # framed on what the diagram shows, as ds render <ws> <diagram> does
        ir = workspaces[ws].render(name, "ir", 0)
        focus = [n["id"] for n in _json.loads(ir).get("nodes", [])] if ir else []
    else:
        focus = [name] if name else []
    html = workspaces[ws].page(os.path.basename(ws), focus)
    out = os.path.join(os.path.expanduser("~/.cache/driftspec"), "view.html")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write(html)
    open_in_browser(out)
    return {"path": out, "focus": focus}


def main() -> None:
    server.start_io()


if __name__ == "__main__":
    main()
