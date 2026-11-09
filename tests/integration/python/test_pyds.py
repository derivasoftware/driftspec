"""pyds, the Python face (REQ-00086-1): a workspace read into a typed model through the native module."""

import os

import pytest

import pyds


class TestLoad:
    def test_a_directory_loads_into_a_typed_model(self, hola):
        model = pyds.load(hola)
        greeter = model.find("hola@core::Greeter")
        assert isinstance(greeter, pyds.Entity) and greeter.kind == "class"
        assert greeter.location.file.endswith("Greeter.ds")
        assert any(r.kind == "realises" and r.target == "UC-00001-1" for r in model.relations)
        assert model.named("Greeter") == [greeter]
        assert not model.findings

    def test_the_texts_an_editor_holds_load_as_a_workspace(self):
        model = pyds.load({"a.ds": "package p {\n  module m {\n    class C { }\n  }\n}\n"})
        assert model.find("p@m::C").kind == "class"

    def test_a_path_that_is_no_directory_is_refused(self, tmp_path):
        with pytest.raises(NotADirectoryError):
            pyds.load(os.path.join(tmp_path, "nowhere"))


RULE = """package p {
  requirement REQ-00001-1: The core shall own nothing of render
  module render {
    class Canvas { }
  }
  module core {
    class Model {
      + canvas: p@render::Canvas
    }
    definition owns-in(a, b) {
      if m in a, m owns b
    }
    rule blind {
      @requirement REQ-00001-1
      for class c in p@core
      never owns-in(c, x), x in p@render
    }
  }
}
"""


class TestModelReadsTheMetamodel:
    """What a kind is and where it stands, read from the metamodel instead of lists of kinds (DS-122)."""

    def test_a_member_an_outcome_and_a_rule_are_drawn_inside_what_holds_them(self, hola):
        model = pyds.load(hola)
        assert model.owner(model.find("hola@core::Casual::emoji")).id == "hola@core::Casual"
        assert model.owner(model.find("hola@core::Style::apply")).id == "hola@core::Style"
        outcome = next(e for e in model.of_kind("outcome"))
        assert model.owner(outcome).kind == "usecase"
        assert model.owner(model.find("hola@core::Style")).id == "hola@core::Style"  # a class stands in its module

    def test_messages_are_laid_out_and_interfaces_are_named(self, hola):
        model = pyds.load(hola)
        assert model.laid_out("message") and not model.laid_out("class")
        assert {"class", "enum", "interface", "function", "requirement", "usecase"} <= model.named_kinds()
        assert not {"method", "attribute", "message", "package"} & model.named_kinds()


class TestRules:
    def test_what_breaks_a_rule_comes_with_its_bindings_and_its_answer(self):
        model = pyds.load({"a.ds": RULE})
        (breach,) = model.breaches["p@core::blind"]
        assert breach.bindings == (("c", "p@core::Model"),)
        assert breach.witness == (("x", "p@render::Canvas"),)
        assert breach.location.line == 7


class TestCheck:
    def test_a_clean_workspace_reports_no_error(self, hola):
        report = pyds.check(hola)
        assert report.errors == 0 and report.counts["findings"] == 0

    def test_a_syntax_error_is_a_finding_where_it_is(self):
        report = pyds.check({"x.ds": "package p {\n  modul m { }\n}\n"})
        assert report.errors == 1
        finding = report.findings[0]
        assert finding.code == "E-SYNTAX" and finding.location.file == "x.ds" and finding.location.line == 2


class TestDrawing:
    def test_a_diagram_draws_as_text_and_none_draws_nothing(self, hola):
        assert "Greeter" in pyds.render(hola, "CL_Hola", "text", 120)
        assert pyds.render(hola, "NoSuchDiagram") == ""

    def test_the_viewer_opens_framed_where_it_is_asked(self, hola):
        page = pyds.page(hola, "hola", ["hola@core::Greeter"])
        assert page.startswith("<!doctype html>") and 'const FOCUS=["hola@core::Greeter"];' in page

    def test_what_the_grammar_takes_in_a_module(self):
        words = pyds.expected_at("package p {\n  module m {\n    \n  }\n}\n", 2, 4)
        assert "class" in words and "usecase" in words
