"""driftspec, the Python face (REQ-00086-1): a workspace read into a typed model through the native module."""

import os

import pytest

import driftspec


class TestLoad:
    def test_a_directory_loads_into_a_typed_model(self, hola):
        model = driftspec.load(hola)
        greeter = model.find("hola@core::Greeter")
        assert isinstance(greeter, driftspec.Entity) and greeter.kind == "class"
        assert greeter.location.file.endswith("Greeter.ds")
        assert any(r.kind == "realises" and r.target == "UC-00001-1" for r in model.relations)
        held = {r.source: r.multiplicity for r in model.relations if r.kind == "owns"}
        assert held["hola@core::Greeter::style"] == "1" and held["hola@core::Greeter::history"] == "*"
        assert model.named("Greeter") == [greeter]
        assert not model.findings

    def test_the_texts_an_editor_holds_load_as_a_workspace(self):
        model = driftspec.load({"a.ds": "package p {\n  module m {\n    class C { }\n  }\n}\n"})
        assert model.find("p@m::C").kind == "class"

    def test_a_path_that_is_no_directory_is_refused(self, tmp_path):
        with pytest.raises(NotADirectoryError):
            driftspec.load(os.path.join(tmp_path, "nowhere"))


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
        model = driftspec.load(hola)
        assert model.owner(model.find("hola@core::Casual::emoji")).id == "hola@core::Casual"
        assert model.owner(model.find("hola@core::Style::apply")).id == "hola@core::Style"
        outcome = next(e for e in model.of_kind("outcome"))
        assert model.owner(outcome).kind == "usecase"
        assert model.owner(model.find("hola@core::Style")).id == "hola@core::Style"  # a class stands in its module

    def test_messages_are_laid_out_and_interfaces_are_named(self, hola):
        model = driftspec.load(hola)
        assert model.laid_out("message") and not model.laid_out("class")
        assert {"class", "enum", "interface", "function", "requirement", "usecase"} <= model.named_kinds()
        assert not {"method", "attribute", "message", "package"} & model.named_kinds()


class TestRules:
    def test_what_breaks_a_rule_comes_with_its_bindings_and_its_answer(self):
        model = driftspec.load({"a.ds": RULE})
        (breach,) = model.breaches["p@core::blind"]
        assert breach.bindings == (("c", "p@core::Model"),)
        assert breach.witness == (("x", "p@render::Canvas"),)
        assert breach.location.line == 7


class TestCheck:
    def test_a_clean_workspace_reports_no_error(self, hola):
        report = driftspec.check(hola)
        assert report.errors == 0 and report.counts["findings"] == 0

    def test_a_syntax_error_is_a_finding_where_it_is(self):
        report = driftspec.check({"x.ds": "package p {\n  modul m { }\n}\n"})
        assert report.errors == 1
        finding = report.findings[0]
        assert finding.code == "E-SYNTAX" and finding.location.file == "x.ds" and finding.location.line == 2

    def test_a_file_that_is_not_utf8_is_the_engines_finding_where_it_is(self, tmp_path):
        # DS-411: a Latin-1 é; Python hands the engine the bytes, and the engine says where they stop being UTF-8
        latin1 = b"package p {\n  requirement REQ-00001-1 {\n    status: draft\n    title: caf\xe9 latin1\n  }\n}\n"
        (tmp_path / "p.ds").write_bytes(latin1)
        for source in ({"p.ds": latin1}, tmp_path):
            report = driftspec.check(source)
            assert report.errors == 1
            (finding,) = report.findings
            assert finding.code == "E-ENCODING" and finding.location.file.endswith("p.ds")
            assert (finding.location.line, finding.location.column) == (4, 15)
        model = driftspec.load(tmp_path)  # the model and the export are written: nothing of the file reached them
        assert model.find("REQ-00001-1") is None and model.findings[0].code == "E-ENCODING"

    def test_bytes_and_str_load_alike(self):
        text = "package p {\n  requirement REQ-00001-1 {\n    status: draft\n    title: caf\u00e9\n  }\n}\n"
        as_str, as_bytes = driftspec.load({"a.ds": text}), driftspec.load({"a.ds": text.encode()})
        assert as_str.find("REQ-00001-1").fields["title"] == as_bytes.find("REQ-00001-1").fields["title"] == "caf\u00e9"
        assert set(as_str.entities) == set(as_bytes.entities) and not as_str.findings and not as_bytes.findings


class TestDrawing:
    def test_a_diagram_draws_as_text_and_none_draws_nothing(self, hola):
        assert "Greeter" in driftspec.render(hola, "CL_Hola", "text", 120)
        assert driftspec.render(hola, "NoSuchDiagram") == ""

    def test_a_diagram_draws_in_the_format_it_is_asked_for(self, hola):
        # each of the five as a workspace draws it, as ds render -f writes it: cells and layout are no text drawing
        # (DS-453)
        ws = driftspec.Workspace(hola)
        drawn = {format: driftspec.render(hola, "CL_Hola", format, 120) for format in ("cells", "ir", "layout", "svg", "text")}
        for format, drawing in drawn.items():
            assert drawing and drawing == ws.render("CL_Hola", format, 120), format
        assert len(set(drawn.values())) == 5

    def test_a_format_a_diagram_has_no_writer_for_raises_not_implemented(self, hola):
        # the engine's NotImplemented, as Workspace.render raises it (DS-398), never a text drawing instead (DS-453)
        with pytest.raises(driftspec.NotImplemented) as raised:
            driftspec.render(hola, "CL_Hola", "yaml")
        assert isinstance(raised.value, NotImplementedError)
        assert (raised.value.document, raised.value.technology) == ("diagram", "yaml")
        assert str(raised.value) == "the diagram is not written in yaml; there is: cells ir layout svg text"

    def test_the_viewer_opens_framed_where_it_is_asked(self, hola):
        page = driftspec.page(hola, "hola", ["hola@core::Greeter"])
        assert page.startswith("<!doctype html>") and 'const FOCUS=["hola@core::Greeter"];' in page

    def test_what_the_grammar_takes_in_a_module(self):
        words = driftspec.expected_at("package p {\n  module m {\n    \n  }\n}\n", 2, 4)
        assert "class" in words and "usecase" in words
