"""driftspec.live, a finding on its subject (REQ-00331-1, DS-488): the state gives each finding the entity it is about,
and the page frames that entity when the finding is clicked and keeps, on the selection, the findings about it."""

import os
import shutil
import subprocess

from conftest import ROOT

import driftspec
from driftspec.live_page import shell
from driftspec.live_session import Live

AUDIT = os.path.join(ROOT, "tests", "corpus", "audit")


class TestAFindingOnItsSubject:
    def test_the_state_gives_each_finding_and_coverage_item_its_subject(self, tmp_path):
        ws = os.path.join(tmp_path, "audit")
        shutil.copytree(AUDIT, ws)
        live = Live(ws)
        live.reload()
        state, report = live.state(), driftspec.check(ws)
        assert [f["subject"] for f in state["findings"]] == [f.subject for f in report.findings]
        assert [f["subject"] for f in state["coverage"]] == [f.subject for f in report.coverage]
        assert any(f["subject"] for f in state["findings"] + state["coverage"])

    def test_a_finding_about_an_entity_is_drawn_to_frame_it_and_kept_on_its_selection(self):
        # the page's own code, run by node over the list it draws: a finding with a subject carries it as data-id,
        # which the side's click frames on the map; on the selection, only what is about it or what it holds stays
        page = shell("t", "/w")
        start = page.index("function about(")
        about = page[start:page.index("\n", start)]
        script = about + """
const f={subject:'p@m::A::run'};
console.log([about(f,'p@m::A'), about(f,'p@m::A::run'), about(f,'p@m::B'), about({subject:''},'p@m'), about(f,null)].join(','));"""
        out = subprocess.run(["node", "-e", script], capture_output=True, text=True, check=True).stdout.strip()
        assert out == "true,true,false,false,false"
        assert 'data-id="${esc(f.subject)}"' in page and "function finds()" in page
