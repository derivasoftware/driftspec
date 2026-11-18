"""A workspace loaded once in the engine and asked from Python (REQ-00107-2, DS-173)."""

import pyds

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
        ws = pyds.Workspace({"shop.ds": SHOP})
        assert ws.model().find("p@m::Desk").kind == "class"
        assert ws.report().errors == 0
        assert ws.find("p@m::Nobody") is None
        assert any(r.kind == "owns" and r.target == "p@m::Vault" for r in ws.relations_from("p@m::Desk::vault"))
        assert ws.parent("p@m::Desk::vault") == "p@m::Desk"
        assert "p@m::Vault" in ws.members("p@m")

    def test_a_behaviour_is_read_as_its_tree_path_and_walk(self):
        ws = pyds.Workspace({"shop.ds": SHOP})
        tree = ws.tree(SEQ)
        (alt,) = tree.frames
        assert alt.kind == "alt" and alt.else_scenario == "refused" and alt.two_sides
        assert len(tree.messages) == 3
        assert len(ws.steps(SEQ, "refused")) == 1
        assert len(ws.steps(SEQ, "*")) == 3
        assert ws.walk(SEQ, "", 1)

    def test_it_asks_as_ds_query_does_and_draws_as_ds_render_does(self):
        ws = pyds.Workspace({"shop.ds": SHOP})
        assert any(a.id == "p@m::Desk" for a in ws.ask("entities", "p@m"))
        assert ws.render("CL_None", "svg") == ""
        assert "<html" in ws.page("shop", ["p@m::Desk"])

    def test_several_are_loaded_at_once_each_with_its_own_model(self):
        before = pyds.Workspace({"shop.ds": SHOP})
        after = pyds.Workspace({"shop.ds": SHOP.replace("+ take(n: int) -> int", "+ take(n: int) -> bool")})
        assert "bool" in after.find("p@m::Vault::take").fields.get("returns", "")
        assert "bool" not in before.find("p@m::Vault::take").fields.get("returns", "")  # the first is as it was

    def test_an_answer_is_a_copy_the_caller_may_change(self):
        ws = pyds.Workspace({"shop.ds": SHOP})
        members = ws.members("p@m")
        members.clear()
        assert ws.members("p@m")  # the engine's model is as it was

    def test_an_identity_is_written_as_the_engine_writes_it(self):
        assert pyds.qualify("p@m", "C") == "p@m::C"
        assert pyds.outer("p@m::C") == "p@m"
        assert pyds.behaviour_of("p@m::C::run::sequence/s::m2") == "p@m::C::run::sequence/s"
        assert pyds.subject_of("p@m::C::run::sequence") == "p@m::C::run"
        assert pyds.scenario_of("p@m::C::run::sequence/s") == "s"
        assert pyds.without_revision("REQ-00001-2") == "REQ-00001"
