"""SREQ-00015-1 from outside: a program that depends on driftspec reads ds's own design in its own process."""

import driftspec


class TestDependency:
    def test_a_program_depending_on_driftspec_reads_checks_and_draws_the_design_of_ds(self, design):
        model = driftspec.load(design)
        assert len(model.entities) > 1000 and model.find("driftspec@ds@api::Api").kind == "class"
        assert driftspec.check(design).errors == 0
        assert "native" in driftspec.render(design, "doors-and-use-cases", "text", 120)
