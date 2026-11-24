"""Every format of every document ds writes, pinned as golden files (DS-395), so a change to how a
document is written (the format refactor, DS-394) shows in CI as a diff to review, not only in each
worker's own comparison.

Three workspaces under golden/workspaces/, each a frozen copy so the goldens move only when ds does:
  design  a copy of ds's own design (large and real: every construct ds itself uses)
  kinds   a copy of examples/hola with a table and a tour added: one diagram of every kind
  small   a synthetic one written for it: a behaviour called from two places, a scenario, findings
For each: the scene (json, html), every diagram in every format (ir, layout, svg, text, cells), the
report (ds check, text and json), the answers (ds query, text and json) and the export.

ds runs from golden/workspaces/ with the workspace's relative path, so no absolute path is written. What
still varies is masked, and only that: the version ds prints, which changes with every release.
A document over 256 KiB is pinned by its sha256 and size (NAME.sha256) instead of its text.

The test runs the ds that DS names: ctest passes the one it built, and CI's golden job the one
cpp-test built. Without DS it is skipped, as in the python job, whose build has no ds. Updating the
goldens is a change reviewed like any other; regenerate them with this same file:
  python tests/system/python/test_golden.py --ds <ds binary> [design kinds small]
"""

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import re
import subprocess
import difflib
import sys

HERE = pathlib.Path(__file__).resolve().parent / "golden"
WORKSPACES = HERE / "workspaces"
EXPECTED = HERE / "expected"

FORMATS = {"ir": "ir.json", "layout": "layout.json", "svg": "svg", "text": "txt", "cells": "cells.txt"}

# the questions asked of each workspace, with the subject each takes (None: none)
QUESTIONS = {
    "design": [
        ("diagrams", None),
        ("entities", "driftspec@ds@check"),
        ("relations", "driftspec@ds@check::Checker"),
        ("artefacts", "driftspec@ds@check::Checker"),
        ("behaviours", "driftspec@ds@check::Checker"),
        ("shows", "CL_CheckNamespace"),
        ("findings", None),
        ("coverage", None),
        ("coupling", None),
    ],
    "kinds": [
        ("diagrams", None),
        ("entities", "hola"),
        ("relations", "hola@core::Greeter"),
        ("artefacts", "hola@core::Greeter"),
        ("behaviours", "hola@core::Greeter"),
        ("shows", "CL_Hola"),
        ("findings", None),
        ("coverage", None),
        ("coupling", None),
        ("failures", "hola@core::Greeter::greet"),
    ],
    "small": [
        ("diagrams", None),
        ("entities", "p"),
        ("relations", "p@m::Leaf"),
        ("artefacts", "p@m::Leaf"),
        ("behaviours", "p@m::Leaf"),
        ("shows", "CL_Small"),
        ("findings", None),
        ("coverage", None),
        ("coupling", None),
        ("failures", "p@m::Leaf::work"),
    ],
}

# a document larger than this is pinned by its digest and size, not its text: the text and cells of
# ds's largest diagrams run to hundreds of MB. Its change still fails; the smaller workspaces carry
# the same formats whole, which is where a change is read.
WHOLE = 256 * 1024


def ds_binary() -> str:
    ds = os.environ.get("DS")
    if not ds:
        import pytest

        pytest.skip("DS names the ds binary the goldens are compared against (ctest and CI's golden job set it)")
    return ds


def run(ds: str, *args: str) -> tuple[int, str]:
    p = subprocess.run([ds, *args], cwd=WORKSPACES, capture_output=True, check=False)
    return p.returncode, p.stdout.decode()


def version_of(ds: str) -> str:
    return run(ds, "--version")[1].split()[-1]


def mask(text: str, version: str) -> str:
    return text.replace(version, "<version>")


def diagrams(ds: str, ws: str) -> list[str]:
    code, out = run(ds, "query", ws, "diagrams", "--json")
    assert code == 0, out
    return [d["id"] for d in json.loads(out)]


def cases(ds: str, ws: str) -> dict[str, list[str]]:
    """The file of each document of a workspace and the ds arguments that write it."""
    c = {
        "scene.json": ["render", ws, "-f", "json"],
        "scene.html": ["render", ws, "-f", "html"],
        "report.txt": ["check", ws],
        "report.json": ["check", ws, "--json"],
        "export.json": ["export", ws],
    }
    for d in diagrams(ds, ws):
        for fmt, ext in FORMATS.items():
            c[f"diagrams/{d}.{ext}"] = ["render", ws, d, "-f", fmt]
    for question, subject in QUESTIONS[ws]:
        name = question if subject is None else f"{question}-{re.sub(r'[^A-Za-z0-9_.-]+', '_', subject)}"
        args = ["query", ws, question, *([] if subject is None else [subject])]
        c[f"answers/{name}.txt"] = args
        c[f"answers/{name}.json"] = [*args, "--json"]
    return c


def written(ds: str, ws: str) -> dict[str, str]:
    """Every document of a workspace as ds writes it, masked, with the exit code of each command."""
    version = version_of(ds)
    docs: dict[str, str] = {}
    codes: dict[str, int] = {}
    for name, args in cases(ds, ws).items():
        codes[name], text = run(ds, *args)
        text = mask(text, version)
        if len(text.encode()) > WHOLE:
            data = text.encode()
            docs[f"{name}.sha256"] = f"{hashlib.sha256(data).hexdigest()}  {len(data)} bytes\n"
        else:
            docs[name] = text
    docs["exit-codes.json"] = json.dumps(codes, indent=2, sort_keys=True) + "\n"
    return docs


def expected(ws: str) -> dict[str, str]:
    root = EXPECTED / ws
    return {str(p.relative_to(root)): p.read_text() for p in sorted(root.rglob("*")) if p.is_file()}


def matches(ws: str) -> None:
    got = written(ds_binary(), ws)
    want = expected(ws)
    missing, extra = sorted(set(want) - set(got)), sorted(set(got) - set(want))
    assert not missing and not extra, f"{ws}: pinned but no longer written {missing}; written but not pinned {extra}"
    differ = [name for name in sorted(got) if got[name] != want[name]]
    first = "" if not differ else "".join(
        list(difflib.unified_diff(want[differ[0]].splitlines(True), got[differ[0]].splitlines(True), "golden", "ds"))[:60]
    )
    assert not differ, f"{len(differ)} of {len(got)} documents of {ws} differ from their golden file: {differ[:20]}\n{first}"


class TestGolden:
    def test_every_document_of_ds_s_design_matches_its_golden_file(self) -> None:
        matches("design")

    def test_every_document_of_a_diagram_of_every_kind_matches_its_golden_file(self) -> None:
        matches("kinds")

    def test_every_document_of_a_small_workspace_with_findings_matches_its_golden_file(self) -> None:
        matches("small")


def main(argv: list[str]) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="regenerate the golden files from a ds binary")
    parser.add_argument("--ds", default=os.environ.get("DS"), required=os.environ.get("DS") is None)
    parser.add_argument("workspaces", nargs="*", default=sorted(QUESTIONS))
    a = parser.parse_args(argv)
    for ws in a.workspaces:
        root = EXPECTED / ws
        for old in sorted(root.rglob("*"), reverse=True) if root.exists() else []:
            old.unlink() if old.is_file() else old.rmdir()
        docs = written(a.ds, ws)
        for name, text in docs.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        print(f"{ws}: {len(docs)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
