"""driftspec.live, the rules panel (REQ-00100-1, DS-421): a rule's problems are the findings written inside the rule,
by where they are and not by what their message says."""

import dataclasses

import pytest

from driftspec.live_session import Live

# every rule and definition finding the language has, each written in its own rule; one rule is written in a class
PROBE = """package p {
  requirement REQ-00001-1: Every class shall be tidy

  definition empty(a) {
    title: holds for nothing
  }

  definition loops(a) {
    title: negates itself
    if a is class, not loops(a)
  }

  definition name(a) {
    title: takes a reserved name
    if a is class
  }

  rule unknown-relation {
    title: names a relation nobody declares
    @requirement REQ-00001-1
    for class c
    never c usess x
  }

  rule wrong-arity {
    title: applies a definition to two
    @requirement REQ-00001-1
    for class c
    holds empty(c, c)
  }

  rule says-nothing {
    title: neither holds nor never
    @requirement REQ-00001-1
    for class c
  }

  rule never-joins {
    title: binds an end the relation never joins
    @requirement REQ-00001-1
    for class c, requirement r
    never r owns c
  }

  rule unsafe-var {
    title: a variable only negated
    @requirement REQ-00001-1
    for class c
    never not x in c
  }

  module m {
    class A {
      + go() -> none
      rule inside-a-class {
        title: written in a class
        @requirement REQ-00001-1
        for class c
        never c usess x
      }
    }
    class B { }
  }
}
"""


@pytest.fixture
def live(tmp_path):
    ws = tmp_path / "design"
    ws.mkdir()
    (ws / "p.ds").write_text(PROBE)
    session = Live(str(ws))
    session.reload()
    return session


class TestRuleProblems:
    def test_each_rule_has_the_problems_written_inside_it(self, live):
        problems = {r["id"]: [p["code"] for p in r["problems"]] for r in live.state()["rules"]}
        assert problems == {
            "p@empty": ["W-DEFINITION-NO-CASE"],
            "p@loops": ["E-RULE-UNSTRATIFIED"],
            "p@name": ["E-RULE-RESERVED"],
            "p@unknown-relation": ["E-RULE-UNKNOWN"],
            "p@wrong-arity": ["E-RULE-ARITY"],
            "p@says-nothing": ["W-RULE-NO-CLAIM"],
            "p@never-joins": ["W-RULE-NEVER-MATCHES"],
            "p@unsafe-var": ["E-RULE-UNSAFE"],
            "p@m::A::inside-a-class": ["E-RULE-UNKNOWN"],
        }

    def test_a_reworded_message_still_reaches_its_rule(self, live):
        model, report = live.loaded.model(), live.loaded.report()
        reworded = [dataclasses.replace(f, message="reworded: " + f.message) for f in report.findings]
        problems = {r["id"]: r["problems"] for r in live.rules_of(model, reworded)}
        assert problems["p@unknown-relation"][0]["code"] == "E-RULE-UNKNOWN"
        assert problems["p@unknown-relation"][0]["message"].startswith("reworded: ")
        assert all(problems[r] for r in problems)
