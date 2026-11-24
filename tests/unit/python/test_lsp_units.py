"""driftspec.lsp, unit by unit: each feature of the DesignServer and each question of an Editor asked directly, the
editor's side stood in for by a client double that holds the buffers and records what is published."""

import os
from types import SimpleNamespace

import pytest
from lsprotocol import types as lsp

import driftspec
import driftspec.lsp as server
from driftspec.lsp import DesignServer, Editor, Kinds, holds_ds, location_of, main, optional, path_of, uri_of

__covers__ = (DesignServer, Editor, Kinds, path_of, uri_of, holds_ds, location_of, optional, main)

DESIGN = (
    "package p {\n"
    "  module m {\n"
    "    class C {\n"
    "      + n: int\n"
    "    }\n"
    "    class D {\n"
    "      + c: C\n"
    "    }\n"
    "  }\n"
    "}\n"
    "\n"
    "diagram CL_P {\n"
    "  kind: class\n"
    "  include: p@m::*\n"
    "}\n"
)


class Doc:
    def __init__(self, source: str):
        self.source = source
        self.lines = source.splitlines(True)


class Client:
    """What the features read of pygls' LanguageServer: the root, the buffers, and where diagnostics go."""

    def __init__(self, root: str):
        self.workspace = SimpleNamespace(root_path=root, text_documents={}, get_text_document=self.document)
        self.published: list[lsp.PublishDiagnosticsParams] = []

    def document(self, uri: str) -> Doc:
        if uri not in self.workspace.text_documents:
            with open(path_of(uri), encoding="utf-8") as fh:
                self.workspace.text_documents[uri] = Doc(fh.read())
        return self.workspace.text_documents[uri]

    def text_document_publish_diagnostics(self, params) -> None:
        self.published.append(params)


@pytest.fixture
def srv():
    """A server of its own for each test: it keeps its designs between requests, and nothing else does."""
    return DesignServer()


@pytest.fixture
def project(tmp_path):
    root = os.path.realpath(tmp_path)
    design = os.path.join(root, "design")
    os.makedirs(os.path.join(design, "core"))
    with open(os.path.join(design, "p.ds"), "w") as fh:
        fh.write(DESIGN)
    with open(os.path.join(design, "core", "q.ds"), "w") as fh:
        fh.write("package q { }\n")
    return root, design, Client(root)


def at(path: str, line: int, character: int) -> SimpleNamespace:
    return SimpleNamespace(text_document=SimpleNamespace(uri=uri_of(path)), position=lsp.Position(line, character))


class TestPaths:
    def test_a_uri_and_a_path_say_the_same_file(self, tmp_path):
        path = os.path.join(os.path.realpath(tmp_path), "a b.ds")
        assert uri_of(path) == "file://" + path
        assert path_of("file://" + path.replace(" ", "%20")) == path

    def test_a_folder_holds_ds_when_a_file_in_it_ends_so(self, project):
        _, design, _ = project
        assert holds_ds(design) and not holds_ds(os.path.dirname(design)) and not holds_ds(os.path.join(design, "nowhere"))


class TestWorkspaceOf:
    def test_a_file_belongs_to_the_folder_above_that_holds_ds_files_too(self, project, srv):
        _, design, client = project
        assert srv.workspace_of(client, os.path.join(design, "core", "q.ds")) == design
        assert srv.workspace_of(client, os.path.join(design, "p.ds")) == design

    def test_the_buffers_win_over_the_disk(self, project):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        client.workspace.text_documents[uri_of(path)] = Doc("package unsaved { }\n")
        files = Editor(design).files(client)
        assert files[path] == "package unsaved { }\n" and len(files) == 2


class TestRefresh:
    def test_each_file_of_the_design_gets_its_diagnostics_and_a_clean_one_is_cleared(self, project, srv):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        client.workspace.text_documents[uri_of(path)] = Doc("package p {\n")
        srv.changed(client, at(path, 0, 0))
        wrong = [p for p in client.published if p.uri == uri_of(path)]
        assert wrong and wrong[-1].diagnostics and wrong[-1].diagnostics[0].severity == lsp.DiagnosticSeverity.Error
        client.workspace.text_documents[uri_of(path)] = Doc(DESIGN)
        client.published.clear()
        srv.changed(client, at(path, 0, 0))
        assert [p.diagnostics for p in client.published if p.uri == uri_of(path)] == [[]]

    def test_a_document_s_design_is_loaded_once(self, project, srv):
        _, design, client = project
        editor = srv.editor_of(client, uri_of(os.path.join(design, "p.ds")))
        assert editor.root == design and srv.editor_of(client, uri_of(os.path.join(design, "core", "q.ds"))) is editor
        assert editor.typed() is editor.typed()

    def test_a_change_reloads_the_workspace_it_keeps_and_reads_the_model_only_when_asked(self, project, srv):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        editor = srv.editor_of(client, uri_of(path))
        kept, editor.model = editor.workspace, editor.typed()
        client.workspace.text_documents[uri_of(path)] = Doc(DESIGN.replace("package p {", "package p {\n  // a comment"))
        srv.changed(client, at(path, 0, 0))
        assert editor.workspace is kept and editor.model is None  # reloaded in place, the model not read
        assert editor.typed() is editor.model  # read when a request asks


class TestNames:
    def test_the_word_under_the_cursor_and_what_it_names(self, project, srv):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        editor = srv.editor_of(client, uri_of(path))
        assert srv.word_at(client, uri_of(path), lsp.Position(6, 11)) == "C"
        assert srv.word_at(client, uri_of(path), lsp.Position(99, 0)) == ""
        (found,) = editor.entities_for("C")
        assert found.id == "p@m::C" and editor.entities_for("p@m::D")[0].id == "p@m::D" and editor.entities_for("") == []
        where = location_of(found)
        assert where.uri == uri_of(path) and where.range.start == lsp.Position(2, 4)

    def test_definition_references_and_hover_answer_from_the_model(self, project, srv):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        assert [d.range.start.line for d in srv.definition(client, at(path, 6, 11))] == [2]
        assert [r.range.start.line for r in srv.references(client, at(path, 2, 11))] == [6]
        said = srv.hover(client, at(path, 2, 11))
        assert "`p@m::C`" in said.contents.value and srv.hover(client, at(path, 0, 0)) is None

    def test_completion_offers_the_grammar_and_the_names_declared(self, project, srv):
        _, design, client = project
        labels = {i.label for i in srv.completion(client, at(os.path.join(design, "p.ds"), 0, 0)).items}
        assert "package" in labels and {"C", "D"} <= labels

    def test_the_outline_lists_the_declarations_of_the_file(self, project, srv):
        _, design, client = project
        names = [s.name for s in srv.symbols(client, at(os.path.join(design, "p.ds"), 0, 0))]
        assert {"p", "m", "C", "D", "n", "c"} <= set(names)


class TestSubject:
    def test_a_diagram_an_entity_or_what_the_cursor_is_written_in(self, project, srv):
        _, design, client = project
        path = os.path.join(design, "p.ds")
        uri = uri_of(path)
        editor = srv.editor_of(client, uri)
        assert editor.subject_of("CL_P") == ("diagram", "CL_P") and editor.subject_of("D") == ("entity", "p@m::D")
        assert editor.subject_of("nothing") == ("", "")
        assert srv.subject_at(client, editor, uri, 11, 10) == ("diagram", "CL_P")
        assert srv.subject_at(client, editor, uri, 0, 0, "D") == ("entity", "p@m::D")
        assert srv.subject_at(client, editor, uri, 7, 0) == ("entity", "p@m::D::c")  # below a member: the nearest declaration above
        assert editor.enclosing(path, 2).id == "p@m::C" and editor.enclosing("elsewhere.ds", 3) is None

    def test_a_member_is_drawn_through_the_class_that_holds_it(self, project, srv):
        _, design, client = project
        editor = srv.editor_of(client, uri_of(os.path.join(design, "p.ds")))
        files, name = editor.with_focus({"p.ds": DESIGN}, "p@m::C::n")
        assert name == "__Focus" and "include: p@m::C\n" in files["__focus__.ds"] and files["p.ds"] == DESIGN

    def test_the_optional_arguments_come_through_as_given(self):
        assert optional(()) == (0, "") and optional((80, "C")) == (80, "C") and optional((None, None)) == (0, "")


class TestSeeing:
    def test_draw_gives_the_diagram_or_the_entity_as_text(self, project, srv):
        _, design, client = project
        uri = uri_of(os.path.join(design, "p.ds"))
        drawn = srv.draw(client, uri, 11, 10)
        assert drawn["title"] == "CL_P" and "C" in drawn["text"]
        assert srv.draw(client, uri, 2, 11, 60)["title"] == "p@m::C"
        blank = uri_of(os.path.join(design, "blank.ds"))
        client.workspace.text_documents[blank] = Doc("\n")
        assert "nothing in" in srv.draw(client, blank, 0, 0, 0, "zzz")["text"]

    def test_view_writes_the_page_framed_where_asked_and_opens_it(self, project, srv, monkeypatch):
        import webbrowser

        root, design, client = project
        opened = []
        monkeypatch.setenv("HOME", root)
        monkeypatch.setattr("shutil.which", lambda name: None)
        monkeypatch.setattr(webbrowser, "open", opened.append)
        answer = srv.view(client, uri_of(os.path.join(design, "p.ds")), 11, 10)
        assert set(answer["focus"]) >= {"p@m::C", "p@m::D"} and opened == [answer["path"]]
        assert answer["path"].startswith(root) and os.path.getsize(answer["path"]) > 0
        assert srv.view(client, uri_of(os.path.join(design, "p.ds")), 2, 11)["focus"] == ["p@m::C"]


class TestKinds:
    def test_every_kind_the_door_names_is_a_kind_of_the_language(self):
        declared = {k["kind"] for k in driftspec.load({"p.ds": "package p { }\n"}).metamodel["kinds"]}
        assert Kinds.named() <= declared, Kinds.named() - declared

    def test_every_field_hover_says_is_a_field_of_some_kind(self):
        fields = {f["name"] for k in driftspec.load({"p.ds": "package p { }\n"}).metamodel["kinds"] for f in k.get("fields", [])}
        assert set(Kinds.HOVER) <= fields, set(Kinds.HOVER) - fields


def test_main_speaks_on_stdio(monkeypatch):
    spoke = []
    monkeypatch.setattr(server.LanguageServer, "start_io", lambda self: spoke.append(True))
    main()
    assert spoke == [True]
