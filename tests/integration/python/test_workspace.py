"""A workspace loaded once in the engine and asked from Python (REQ-00107-2, DS-173)."""

import json

import driftspec

SHOP = """package p {
  module m {
    class Vault {
      + take(n: int) -> int
    }
    class Desk {
      - vault: Vault
      + pay(n: int) -> int
    }
    sequence Desk::pay {
      alt there is enough {
        self -> vault.take(n) -> got
      } else / refused {
        return 0
      }
      return got
    }
  }
}
"""

SEQ = "p@m::Desk::pay::sequence"


class TestWorkspace:
    def test_one_load_answers_the_model_the_check_and_one_hop(self):
        ws = driftspec.Workspace({"shop.ds": SHOP})
        assert ws.model().find("p@m::Desk").kind == "class"
        assert ws.report().errors == 0
        assert ws.find("p@m::Nobody") is None
        assert any(r.kind == "owns" and r.target == "p@m::Vault" for r in ws.relations_from("p@m::Desk::vault"))
        assert ws.parent("p@m::Desk::vault") == "p@m::Desk"
        assert "p@m::Vault" in ws.members("p@m")

    def test_a_behaviour_is_read_as_its_tree_path_and_walk(self):
        ws = driftspec.Workspace({"shop.ds": SHOP})
        tree = ws.tree(SEQ)
        (alt,) = tree.frames
        assert alt.kind == "alt" and alt.else_scenario == "refused" and alt.two_sides
        assert len(tree.messages) == 3
        assert len(ws.steps(SEQ, "refused")) == 1
        assert len(ws.steps(SEQ, "*")) == 3
        assert ws.walk(SEQ, "", 1)

    def test_it_asks_as_ds_query_does_and_draws_as_ds_render_does(self):
        ws = driftspec.Workspace({"shop.ds": SHOP})
        assert any(a.id == "p@m::Desk" for a in ws.ask("entities", "p@m"))
        assert ws.render("CL_None", "svg") == ""
        assert "<html" in ws.page("shop", ["p@m::Desk"])

    def test_several_are_loaded_at_once_each_with_its_own_model(self):
        before = driftspec.Workspace({"shop.ds": SHOP})
        after = driftspec.Workspace({"shop.ds": SHOP.replace("+ take(n: int) -> int", "+ take(n: int) -> bool")})
        assert "bool" in after.find("p@m::Vault::take").fields.get("returns", "")
        assert "bool" not in before.find("p@m::Vault::take").fields.get("returns", "")  # the first is as it was

    def test_an_answer_is_a_copy_the_caller_may_change(self):
        ws = driftspec.Workspace({"shop.ds": SHOP})
        members = ws.members("p@m")
        members.clear()
        assert ws.members("p@m")  # the engine's model is as it was

    def test_the_package_says_the_version_of_its_engine(self):
        version = driftspec.__version__
        assert version.count(".") == 2 and all(part.isdigit() for part in version.split("."))
        assert json.loads(driftspec._native.export({"shop.ds": SHOP}))["version"] == version

    def test_an_identity_is_written_as_the_engine_writes_it(self):
        assert driftspec.qualify("p@m", "C") == "p@m::C"
        assert driftspec.outer("p@m::C") == "p@m"
        assert driftspec.behaviour_of("p@m::C::run::sequence/s::m2") == "p@m::C::run::sequence/s"
        assert driftspec.subject_of("p@m::C::run::sequence") == "p@m::C::run"
        assert driftspec.scenario_of("p@m::C::run::sequence/s") == "s"
        assert driftspec.without_revision("REQ-00001-2") == "REQ-00001"


class TestNotImplemented:
    """A pair the engine does not write, raised as driftspec.NotImplemented (DS-398)."""

    def test_a_diagram_asked_in_a_format_the_engine_does_not_write_raises_not_implemented(self):
        # a pair the engine does not write is said, never drawn as text instead (DS-398): a NotImplementedError that
        # names the document and the technology
        ws = driftspec.Workspace({"shop.ds": SHOP})
        try:
            ws.render("CL_Any", "yaml")
        except driftspec.NotImplemented as failure:
            assert isinstance(failure, NotImplementedError)
            assert (failure.document, failure.technology) == ("diagram", "yaml")
            assert str(failure) == "the diagram is not written in yaml; there is: cells ir layout svg text"
        else:
            raise AssertionError("a diagram was drawn in yaml")
        assert issubclass(driftspec.NotImplemented, NotImplementedError)


class TestReload:
    """DS-420: a workspace loaded again in place reads as one made anew from the same files, and what it answered
    before stays as it was."""

    def test_a_reload_answers_as_a_workspace_made_anew_from_the_same_files(self):
        changed = SHOP.replace("class Vault {", "class Vault {\n      + count() -> int")
        ws = driftspec.Workspace({"shop.ds": SHOP})
        before = ws.model()
        ws.reload({"shop.ds": changed, "more.ds": "package q { }\n"})
        anew = driftspec.Workspace({"shop.ds": changed, "more.ds": "package q { }\n"})
        assert ws._native.export() == anew._native.export() and ws._native.report() == anew._native.report()
        assert ws.find("p@m::Vault::count") is not None and before.find("p@m::Vault::count") is None

    def test_a_file_that_stops_parsing_and_parses_again_reloads_both_ways(self):
        ws = driftspec.Workspace({"shop.ds": SHOP})
        ws.reload({"shop.ds": SHOP.replace("class Desk {", "class Desk {{")})
        assert [f.code for f in ws.report().findings][:1] == ["E-SYNTAX"]
        ws.reload({"shop.ds": SHOP})
        assert ws._native.report() == driftspec.Workspace({"shop.ds": SHOP})._native.report()
