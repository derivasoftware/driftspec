"""driftspec, the package, unit by unit: each function and the Workspace asked of a small design."""

import os

import pytest

import driftspec
from driftspec import Workspace, check, expected_at, files, load, page, read, render

__covers__ = (files, read, load, check, render, page, expected_at, Workspace)

DESIGN = (
    "package p {\n"
    "  module m {\n"
    "    class C {\n"
    "      + n: int\n"
    "      + run() -> int\n"
    "    }\n"
    "    function f() -> int\n"
    "  }\n"
    "}\n"
    "\n"
    "diagram CL_P {\n"
    "  kind: class\n"
    "  include: p@m::*\n"
    "}\n"
)
FILES = {"a.ds": DESIGN}
BEHAVIOUR = (
    "package p {\n"
    "  requirement REQ-00001-2: The C shall run\n"
    "  module m {\n"
    "    class C {\n"
    "      + run() -> int\n"
    "    }\n"
    "    function f() -> int\n"
    "    sequence C::run {\n"
    "      -> f() -> x\n"
    "      return x\n"
    "    }\n"
    "  }\n"
    "}\n"
)


@pytest.fixture
def directory(tmp_path):
    (tmp_path / "a.ds").write_text(DESIGN)
    (tmp_path / "notes.txt").write_text("not a design")
    (tmp_path / "sub").mkdir()
    (tmp_path / "sub" / "b.ds").write_text("package q { }\n")
    return str(tmp_path)


class TestFiles:
    def test_the_ds_files_below_a_folder_sorted_and_only_its_own_when_not_nested(self, tmp_path):
        (tmp_path / "in").mkdir()
        for name in ("b.ds", "a.ds", "notes.txt", "in/c.ds"):
            (tmp_path / name).write_text("package p { }\n")
        assert files(tmp_path) == [str(tmp_path / n) for n in ("a.ds", "b.ds", "in/c.ds")]
        assert files(tmp_path, nested=False) == [str(tmp_path / n) for n in ("a.ds", "b.ds")]
        assert files(tmp_path / "nowhere") == []


class TestRead:
    def test_a_directory_gives_its_ds_files_and_nothing_else(self, directory):
        files = read(directory)
        assert sorted(os.path.relpath(p, directory) for p in files) == ["a.ds", os.path.join("sub", "b.ds")]
        assert files[os.path.join(directory, "a.ds")] == DESIGN

    def test_a_mapping_is_given_as_it_is(self):
        assert read({"x.ds": "package x { }\n"}) == {"x.ds": "package x { }\n"}

    def test_a_path_that_is_no_directory_is_refused(self, tmp_path):
        with pytest.raises(NotADirectoryError):
            read(str(tmp_path / "nowhere"))


class TestLoad:
    def test_the_model_holds_every_entity_by_its_identity(self):
        model = load(FILES)
        assert model.find("p@m::C").kind == "class"
        assert model.find("p@m::f").kind == "function"
        assert not model.findings


class TestCheck:
    def test_a_clean_design_counts_no_error(self):
        report = check(FILES)
        assert report.errors == 0 and report.counts["findings"] == len(report.findings)

    def test_a_syntax_error_is_counted(self):
        report = check({"a.ds": "package p {\n"})
        assert report.errors >= 1 and report.findings[0].location.file == "a.ds"


class TestRender:
    def test_a_diagram_draws_and_one_the_design_lacks_draws_nothing(self):
        assert "C" in render(FILES, "CL_P", "text")
        assert render(FILES, "CL_None", "text") == ""


class TestPage:
    def test_the_viewer_is_one_page(self):
        assert page(FILES, "p", ["p@m::C"]).lstrip().lower().startswith("<!doctype html")


class TestExpectedAt:
    def test_the_grammar_names_what_it_takes_at_the_start_of_a_file(self):
        words = expected_at("", 0, 0)
        assert "package" in words


class TestWorkspace:
    def test_it_answers_about_the_design_it_holds(self):
        ws = Workspace(FILES)
        assert ws.model().find("p@m::C").name == "C"
        assert ws.report().errors == 0
        assert ws.find("p@m::C").kind == "class" and ws.find("p@nowhere") is None
        assert "p@m::C" in ws.members("p@m")
        assert ws.parent("p@m::C") == "p@m"
        assert "p@m::C::n" in ws.children("p@m::C")
        assert all(r.source == "p@m::C" for r in ws.relations_from("p@m::C"))
        assert all(r.target == "p@m::C" for r in ws.relations_to("p@m::C"))
        assert all("p@m::C" in (r.source, r.target) for r in ws.touching("p@m::C"))
        assert "C" in ws.render("CL_P", "text") and ws.render("CL_None") == ""
        assert {a.id for a in ws.shows("CL_P")} >= {"p@m::C"}
        assert ws.page("p", ("p@m::C",)).lstrip().lower().startswith("<!doctype html")

    def test_it_lays_out_a_behaviour_as_written_and_walks_it(self):
        ws = Workspace({"b.ds": BEHAVIOUR})
        seq = "p@m::C::run::sequence"
        tree = ws.tree(seq)
        assert tree.sequence == seq and [m for m, _ in tree.messages] == [seq + "::m0", seq + "::m1"] and tree.frames == ()
        assert [s.message for s in ws.steps(seq)] == [seq + "::m0", seq + "::m1"]
        assert [(w.message, w.depth) for w in ws.walk(seq)] == [(seq + "::m0", 1), (seq + "::m1", 1)]
        assert ws.revision_of("REQ-00001") == "REQ-00001-2"
        assert isinstance(ws.ask("diagrams"), list)

    def test_each_workspace_keeps_its_own_model(self):
        one, other = Workspace(FILES), Workspace({"b.ds": "package q { }\n"})
        assert one.find("p") and not other.find("p")
        assert other.find("q") and not one.find("q")


def test_the_package_says_the_engines_version():
    assert driftspec.__version__
