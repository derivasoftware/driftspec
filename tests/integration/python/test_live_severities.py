"""driftspec.live, the severities on the page (REQ-00230-1, DS-386): the coverage reaches the state apart from the
findings, and the page colours each finding by the severity table, which it carries as the engine writes it."""

import json
import os
import shutil

from conftest import ROOT

import driftspec
from driftspec.live_page import shell
from driftspec.live_session import Live

AUDIT = os.path.join(ROOT, "tests", "corpus", "audit")


class TestSeveritiesOnThePage:
    def test_the_coverage_reaches_the_state_apart_from_the_findings(self, tmp_path):
        ws = os.path.join(tmp_path, "audit")
        shutil.copytree(AUDIT, ws)
        live = Live(ws)
        live.reload()
        state, report = live.state(), driftspec.check(ws)
        kept_apart = {s["name"] for s in driftspec.severities() if s["keptApart"]}
        assert [f["code"] for f in state["coverage"]] == [f.code for f in report.coverage] and state["coverage"]
        assert {f["severity"] for f in state["coverage"]} <= kept_apart
        assert [f["code"] for f in state["findings"]] == [f.code for f in report.findings]
        assert not {f["severity"] for f in state["findings"]} & kept_apart

    def test_the_page_carries_the_severity_table_it_colours_by(self):
        page = shell("t", "/w")
        start = page.index("const SEVERITIES=") + len("const SEVERITIES=")
        assert json.loads(page[start:page.index(";", start)]) == driftspec.severities() and "__SEVERITIES__" not in page
        assert 'id="coverage"' in page
