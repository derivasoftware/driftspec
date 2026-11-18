"""driftspec.lsp, unit by unit: each handler asked directly, the editor stood in for by a server double
that holds the buffers and records what is published."""

import os
from types import SimpleNamespace

import pytest
from lsprotocol import types as lsp

import driftspec.lsp as server
from driftspec.lsp import (
    changed,
    completion,
    definition,
    draw,
    enclosing,
    entities_for,
    holds_ds,
    hover,
    location_of,
    main,
    open_in_browser,
    optional,
    path_of,
    references,
    refresh,
    subject_at,
    subject_of,
    symbols,
    uri_of,
    use,
    view,
    with_focus,
    word_at,
    workspace_files,
    workspace_of,
)

__covers__ = (
    path_of, uri_of, holds_ds, workspace_of, workspace_files, use, refresh, word_at, entities_for, location_of,
    changed, definition, references, hover, completion, symbols, subject_at, enclosing, subject_of, with_focus,
    optional, draw, open_in_browser, view, main,
)

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


class Editor:
    """What the handlers read of pygls' LanguageServer: the root, the buffers, and where diagnostics go."""

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


@pytest.fixture(autouse=True)
def fresh(monkeypatch):
    """Every test starts with no design loaded: the module keeps its models between requests."""
    monkeypatch.setattr(server, "models", {})
    monkeypatch.setattr(server, "workspaces", {})
    monkeypatch.setattr(server, "published", {})
    monkeypatch.setattr(server, "state", {"model": None})


@pytest.fixture
def project(tmp_path):
    root = os.path.realpath(tmp_path)
    design = os.path.join(root, "design")
    os.makedirs(os.path.join(design, "core"))
    with open(os.path.join(design, "p.ds"), "w") as fh:
        fh.write(DESIGN)
    with open(os.path.join(design, "core", "q.ds"), "w") as fh:
        fh.write("package q { }\n")
    return root, design, Editor(root)


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
    def test_a_file_belongs_to_the_folder_above_that_holds_ds_files_too(self, project):
        _, design, editor = project
        assert workspace_of(editor, os.path.join(design, "core", "q.ds")) == design
        assert workspace_of(editor, os.path.join(design, "p.ds")) == design

    def test_the_buffers_win_over_the_disk(self, project):
        _, design, editor = project
        path = os.path.join(design, "p.ds")
        editor.workspace.text_documents[uri_of(path)] = Doc("package unsaved { }\n")
        files = workspace_files(editor, design)
        assert files[path] == "package unsaved { }\n" and len(files) == 2


class TestRefresh:
    def test_each_file_of_the_design_gets_its_diagnostics_and_a_clean_one_is_cleared(self, project):
        _, design, editor = project
        path = os.path.join(design, "p.ds")
        editor.workspace.text_documents[uri_of(path)] = Doc("package p {\n")
        refresh(editor, design)
        wrong = [p for p in editor.published if p.uri == uri_of(path)]
        assert wrong and wrong[-1].diagnostics and wrong[-1].diagnostics[0].severity == lsp.DiagnosticSeverity.Error
        editor.workspace.text_documents[uri_of(path)] = Doc(DESIGN)
        editor.published.clear()
        changed(editor, at(path, 0, 0))
        assert [p.diagnostics for p in editor.published if p.uri == uri_of(path)] == [[]]

    def test_use_loads_the_design_of_a_document_once(self, project):
        _, design, editor = project
        assert use(editor, uri_of(os.path.join(design, "p.ds"))) == design
        assert server.state["model"] is server.models[design] and design in server.workspaces


class TestNames:
    def test_the_word_under_the_cursor_and_what_it_names(self, project):
        _, design, editor = project
        path = os.path.join(design, "p.ds")
        use(editor, uri_of(path))
        assert word_at(editor, uri_of(path), lsp.Position(6, 11)) == "C"
        assert word_at(editor, uri_of(path), lsp.Position(99, 0)) == ""
        (found,) = entities_for("C")
        assert found.id == "p@m::C" and entities_for("p@m::D")[0].id == "p@m::D" and entities_for("") == []
        where = location_of(found)
        assert where.uri == uri_of(path) and where.range.start == lsp.Position(2, 4)

    def test_definition_references_and_hover_answer_from_the_model(self, project):
        _, design, editor = project
        path = os.path.join(design, "p.ds")
        assert [d.range.start.line for d in definition(editor, at(path, 6, 11))] == [2]
        assert [r.range.start.line for r in references(editor, at(path, 2, 11))] == [6]
        said = hover(editor, at(path, 2, 11))
        assert "`p@m::C`" in said.contents.value and hover(editor, at(path, 0, 0)) is None

    def test_completion_offers_the_grammar_and_the_names_declared(self, project):
        _, design, editor = project
        labels = {i.label for i in completion(editor, at(os.path.join(design, "p.ds"), 0, 0)).items}
        assert "package" in labels and {"C", "D"} <= labels

    def test_the_outline_lists_the_declarations_of_the_file(self, project):
        _, design, editor = project
        names = [s.name for s in symbols(editor, at(os.path.join(design, "p.ds"), 0, 0))]
        assert {"p", "m", "C", "D", "n", "c"} <= set(names)


class TestSubject:
    def test_a_diagram_an_entity_or_what_the_cursor_is_written_in(self, project):
        _, design, editor = project
        path = os.path.join(design, "p.ds")
        uri = use(editor, uri_of(path)) and uri_of(path)
        assert subject_of("CL_P") == ("diagram", "CL_P") and subject_of("D") == ("entity", "p@m::D")
        assert subject_of("nothing") == ("", "")
        assert subject_at(editor, uri, 11, 10) == ("diagram", "CL_P")
        assert subject_at(editor, uri, 0, 0, "D") == ("entity", "p@m::D")
        assert subject_at(editor, uri, 7, 0) == ("entity", "p@m::D::c")  # below a member: the nearest declaration above
        assert enclosing(path, 2).id == "p@m::C" and enclosing("elsewhere.ds", 3) is None

    def test_a_member_is_drawn_through_the_class_that_holds_it(self, project):
        _, design, editor = project
        use(editor, uri_of(os.path.join(design, "p.ds")))
        files, name = with_focus({"p.ds": DESIGN}, "p@m::C::n")
        assert name == "__Focus" and "include: p@m::C\n" in files["__focus__.ds"] and files["p.ds"] == DESIGN

    def test_the_optional_arguments_come_through_as_given(self):
        assert optional(()) == (0, "") and optional((80, "C")) == (80, "C") and optional((None, None)) == (0, "")


class TestSeeing:
    def test_draw_gives_the_diagram_or_the_entity_as_text(self, project):
        _, design, editor = project
        uri = uri_of(os.path.join(design, "p.ds"))
        drawn = draw(editor, uri, 11, 10)
        assert drawn["title"] == "CL_P" and "C" in drawn["text"]
        assert draw(editor, uri, 2, 11, 60)["title"] == "p@m::C"
        blank = uri_of(os.path.join(design, "blank.ds"))
        editor.workspace.text_documents[blank] = Doc("\n")
        assert "nothing in" in draw(editor, blank, 0, 0, 0, "zzz")["text"]

    def test_view_writes_the_page_framed_where_asked_and_opens_it(self, project, monkeypatch):
        import shutil
        import subprocess

        root, design, editor = project
        opened = []
        monkeypatch.setenv("HOME", root)
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/xdg-open" if name == "xdg-open" else None)
        monkeypatch.setattr(subprocess, "Popen", lambda argv, **_: opened.append(argv[-1]))
        answer = view(editor, uri_of(os.path.join(design, "p.ds")), 11, 10)
        assert set(answer["focus"]) >= {"p@m::C", "p@m::D"} and opened == [answer["path"]]
        assert answer["path"].startswith(root) and os.path.getsize(answer["path"]) > 0
        assert view(editor, uri_of(os.path.join(design, "p.ds")), 2, 11)["focus"] == ["p@m::C"]

    def test_the_browser_is_windows_under_wsl_and_xdg_open_elsewhere(self, monkeypatch):
        import shutil
        import subprocess

        started = []
        monkeypatch.setattr(subprocess, "Popen", lambda argv, **_: started.append(argv))
        monkeypatch.setattr(subprocess, "run", lambda argv, **_: SimpleNamespace(stdout="C:\\x\\view.html\n"))
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/" + name)
        open_in_browser("/x/view.html")
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/xdg-open" if name == "xdg-open" else None)
        open_in_browser("/x/view.html")
        assert started == [["explorer.exe", "C:\\x\\view.html"], ["xdg-open", "/x/view.html"]]


def test_main_speaks_on_stdio(monkeypatch):
    spoke = []
    monkeypatch.setattr(server.server, "start_io", lambda: spoke.append(True))
    main()
    assert spoke == [True]
