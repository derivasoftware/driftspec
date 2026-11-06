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
state: dict = {"model": None, "published": set()}


def path_of(uri: str) -> str:
    return unquote(urlparse(uri).path)


def uri_of(path: str) -> str:
    return "file://" + os.path.abspath(path)


def workspace_files(ls: LanguageServer) -> dict[str, str]:
    files: dict[str, str] = {}
    root = ls.workspace.root_path
    if root and os.path.isdir(root):
        files.update(driftspec.read(root))
    for uri, doc in ls.workspace.text_documents.items():  # the buffers, saved or not, win over the disk
        if uri.endswith(".ds"):
            files[path_of(uri)] = doc.source
    return files


def refresh(ls: LanguageServer) -> None:
    files = workspace_files(ls)
    state["model"] = driftspec.load(files)
    by_file: dict[str, list[lsp.Diagnostic]] = {path: [] for path in files}
    for f in state["model"].findings:
        line, col = max(f.location.line - 1, 0), max(f.location.column - 1, 0)
        by_file.setdefault(f.location.file, []).append(
            lsp.Diagnostic(
                range=lsp.Range(lsp.Position(line, col), lsp.Position(line, col + 1)),
                message=f.message,
                severity=lsp.DiagnosticSeverity.Error if f.severity == "error" else lsp.DiagnosticSeverity.Warning,
                code=f.code,
                source="ds",
            )
        )
    for path in set(by_file) | state["published"]:  # a file that is clean now is cleared
        ls.text_document_publish_diagnostics(lsp.PublishDiagnosticsParams(uri=uri_of(path), diagnostics=by_file.get(path, [])))
    state["published"] = {p for p, d in by_file.items() if d}


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


@server.feature(lsp.INITIALIZED)
def initialized(ls: LanguageServer, _params) -> None:
    refresh(ls)


@server.feature(lsp.TEXT_DOCUMENT_DID_OPEN)
@server.feature(lsp.TEXT_DOCUMENT_DID_CHANGE)
@server.feature(lsp.TEXT_DOCUMENT_DID_SAVE)
def changed(ls: LanguageServer, _params) -> None:
    refresh(ls)


@server.feature(lsp.TEXT_DOCUMENT_DEFINITION)
def definition(ls: LanguageServer, params: lsp.DefinitionParams) -> list[lsp.Location]:
    return [location_of(e) for e in entities_for(word_at(ls, params.text_document.uri, params.position))]


@server.feature(lsp.TEXT_DOCUMENT_REFERENCES)
def references(ls: LanguageServer, params: lsp.ReferenceParams) -> list[lsp.Location]:
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
    doc = ls.workspace.get_text_document(params.text_document.uri)
    items = [
        lsp.CompletionItem(label=w, kind=lsp.CompletionItemKind.Keyword)
        for w in driftspec.expected_at(doc.source, params.position.line, params.position.character)
        if w not in ("key: value",)
    ]
    model: driftspec.Model = state["model"]
    if model:  # the names the model declares, for a type or a call
        seen = set()
        for e in model.entities.values():
            if e.kind in ("class", "enum", "function", "requirement", "usecase") and not e.expected and e.name not in seen:
                seen.add(e.name)
                items.append(lsp.CompletionItem(label=e.name, kind=lsp.CompletionItemKind.Class, detail=e.kind))
    return lsp.CompletionList(is_incomplete=False, items=items)


@server.feature(lsp.TEXT_DOCUMENT_DOCUMENT_SYMBOL)
def symbols(ls: LanguageServer, params: lsp.DocumentSymbolParams) -> list[lsp.SymbolInformation]:
    model: driftspec.Model = state["model"]
    kinds = {"package": lsp.SymbolKind.Package, "module": lsp.SymbolKind.Module, "class": lsp.SymbolKind.Class,
             "function": lsp.SymbolKind.Function, "method": lsp.SymbolKind.Method, "interface": lsp.SymbolKind.Interface,
             "requirement": lsp.SymbolKind.Key, "usecase": lsp.SymbolKind.Event}
    path = path_of(params.text_document.uri)
    return [lsp.SymbolInformation(name=e.name, kind=kinds.get(e.kind, lsp.SymbolKind.Object), location=location_of(e))
            for e in (model.in_file(path) if model else []) if e.kind in kinds]


def main() -> None:
    server.start_io()


if __name__ == "__main__":
    main()
