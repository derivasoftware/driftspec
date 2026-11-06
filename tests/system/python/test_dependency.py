"""SREQ-00015-1 from outside: a program that depends on pyds reads ds's own design in its own process."""

import pyds


class TestDependency:
    def test_a_program_depending_on_pyds_reads_checks_and_draws_the_design_of_ds(self, design):
        model = pyds.load(design)
        assert len(model.entities) > 1000 and model.find("driftspec@ds@api::Api").kind == "class"
        assert pyds.check(design).errors == 0
        assert "native" in pyds.render(design, "CL_Pyds", "text", 120)
