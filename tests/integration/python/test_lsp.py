"""driftspec.lsp, the language server (REQ-00087-1), driven over stdio as an editor drives it."""

import os
import shutil

import pytest

from conftest import ROOT, uri


@pytest.fixture
def server(hola, tmp_path, lsp):
    root = os.path.join(tmp_path, "hola")
    shutil.copytree(hola, root)
    return root, lsp(root)


def open_file(s, path):
    s.send("textDocument/didOpen", {"textDocument": {"uri": uri(path), "languageId": "driftspec", "version": 1,
                                                     "text": open(path).read()}})


def diagnostics_of(s, path, nonempty=False):
    return s.until(lambda m: m.get("method") == "textDocument/publishDiagnostics" and m["params"]["uri"] == uri(path)
                   and (m["params"]["diagnostics"] or not nonempty))["params"]["diagnostics"]


class TestLanguageServer:
    def test_a_buffer_typed_wrong_is_diagnosed_and_cleared_when_right(self, server):
        root, s = server
        path = os.path.join(root, "core", "Greeter.ds")
        text = open(path).read()
        open_file(s, path)
        s.send("textDocument/didChange", {"textDocument": {"uri": uri(path), "version": 2},
                                          "contentChanges": [{"text": text.replace("class Greeter", "clas Greeter")}]})
        assert diagnostics_of(s, path, nonempty=True)[0]["code"] == "E-SYNTAX"
        s.send("textDocument/didChange", {"textDocument": {"uri": uri(path), "version": 3}, "contentChanges": [{"text": text}]})
        assert diagnostics_of(s, path) == []

    def test_a_name_leads_to_its_declaration_and_draws(self, server):
        root, s = server
        path = os.path.join(root, "server", "Server.ds")
        open_file(s, path)
        lines = open(path).read().splitlines()
        line = next(i for i, l in enumerate(lines) if "core::Greeter" in l)
        character = lines[line].index("Greeter") + 2
        found = s.call("textDocument/definition", {"textDocument": {"uri": uri(path)}, "position": {"line": line, "character": character}})
        assert found and found[0]["uri"].endswith("core/Greeter.ds")
        drawn = s.call("workspace/executeCommand", {"command": "driftspec.draw", "arguments": [uri(path), line, character, 120]})
        assert drawn["title"] == "hola@core::Greeter" and "Greeter" in drawn["text"]

    def test_the_outline_lists_every_declaration_of_the_file(self, server):
        # every kind the file declares, attributes and outcomes included; no message or frame (DS-122)
        root, s = server
        for name, expected in (("core/Style.ds", {"Style", "apply", "emoji"}), ("usecases.ds", {"UC-00001-1", "default", "empty-name"})):
            path = os.path.join(root, name)
            open_file(s, path)
            names = {sym["name"] for sym in s.call("textDocument/documentSymbol", {"textDocument": {"uri": uri(path)}})}
            assert expected <= names, (name, names)

    def test_completion_offers_every_kind_a_reference_names(self, server):
        # interfaces too, as the metamodel says a type can be one (DS-122)
        root, s = server
        path = os.path.join(root, "server", "Server.ds")
        open_file(s, path)
        found = s.call("textDocument/completion", {"textDocument": {"uri": uri(path)}, "position": {"line": 0, "character": 0}})
        labels = {item["label"]: item.get("detail") for item in found["items"]}
        assert labels.get("shell") == "interface" and labels.get("Greeter") == "class"
        assert "apply" not in labels  # a member is named through its class, not offered alone

    def test_a_file_belongs_to_its_own_design_only(self, server, tmp_path):
        root, s = server
        other = os.path.join(tmp_path, "other")  # a second design beside: its identities never meet hola's
        os.makedirs(other)
        with open(os.path.join(other, "a.ds"), "w") as fh:
            fh.write("package hola {\n  requirement SREQ-00001-1: Another system requirement of the same number\n}\n")
        path = os.path.join(root, "requirements.ds")
        open_file(s, path)
        assert not any(d["code"] == "E-DUPLICATE-IDENTITY" for d in diagnostics_of(s, path))


class TestSeverityThroughTheTable:
    """DS-386: the language server shows each finding as the severity table's editor mapping says, and no severity by
    its name; it publishes the findings only, never the coverage."""

    def test_each_diagnostic_carries_the_editor_mapping_of_its_finding_s_severity(self, tmp_path, lsp):
        from collections import Counter

        import driftspec
        from driftspec.lsp import EDITOR

        root = os.path.join(tmp_path, "audit")
        shutil.copytree(os.path.join(ROOT, "tests", "corpus", "audit"), root)
        report = driftspec.check(root)
        editor = {s["name"]: EDITOR[s["editor"]] for s in driftspec.severities()}
        want = Counter((os.path.realpath(f.location.file), f.location.line, f.code, editor[f.severity]) for f in report.findings)
        s = lsp(root)
        first = driftspec.files(root)[0]
        open_file(s, first)
        published = []
        rid = s.send("textDocument/hover", {"textDocument": {"uri": uri(first)}, "position": {"line": 0, "character": 0}}, request=True)
        s.until(lambda m: (published.append(m["params"]) if m.get("method") == "textDocument/publishDiagnostics" else None) or m.get("id") == rid)
        got = Counter((p["uri"][len("file://"):], d["range"]["start"]["line"] + 1, d["code"], d["severity"]) for p in published for d in p["diagnostics"])
        assert got == want and sum(got.values()) == len(report.findings) and report.coverage  # coverage there, not published
