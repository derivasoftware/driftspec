"""Every drawing and page DriftSpec writes takes its colours from one theme (CST-00012-1, DS-393).

A colour written by hand anywhere in what DriftSpec ships fails here: in the engine (src, include, app), the viewer,
the playground's page, the Python face and the tools that write pages. A colour lives in two places only: the
theme's values (src/ds/theme/Theme.cpp) and the region of a page between its theme markers, which the theme writes
(tools/theme.py, checked by the ctest theme_pages). A gradient's mask stop reads alpha, not colour, and is let be.

    python tests/system/python/test_theme.py      # the same check without pytest
"""
from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SHIPPED = ["src", "include", "app", "viewer", "playground/web", "python/driftspec", "tools"]
VALUES = "src/ds/theme/Theme.cpp"
COLOUR = re.compile(r"#[0-9a-fA-F]{3,8}\b|\b(?:rgb|rgba|hsl|hsla)\(")
REGION = re.compile(r"/\* theme: ([a-z-]+), [^\n]*\*/\n.*?/\* end theme: \1 \*/", re.S)
MASK = re.compile(r"mask-image\s*:")


def shipped_files() -> list[pathlib.Path]:
    listed = subprocess.run(["git", "ls-files", *SHIPPED], cwd=ROOT, check=True, capture_output=True, text=True).stdout
    out = []
    for name in listed.splitlines():
        path = ROOT / name
        if name == VALUES or not path.is_file():
            continue
        try:
            path.read_text()
        except UnicodeDecodeError:  # an image, a font: no text to hold a colour
            continue
        out.append(path)
    return out


def handwritten(text: str) -> list[tuple[int, str]]:
    """Each colour written outside the theme's regions: its line and the line itself."""
    kept = REGION.sub(lambda m: "\n" * m.group(0).count("\n"), text)  # a region keeps its lines, not its colours
    found = []
    for number, line in enumerate(kept.splitlines(), 1):
        if MASK.search(line):  # a mask stop reads alpha only
            continue
        if COLOUR.search(line):
            found.append((number, line.strip()[:120]))
    return found


def offenders() -> list[str]:
    out = []
    for path in shipped_files():
        for number, line in handwritten(path.read_text()):
            out.append(f"{path.relative_to(ROOT)}:{number}: {line}")
    return out


class TestTheme:
    def test_no_colour_is_written_outside_the_theme(self):
        found = offenders()
        assert not found, "colours written by hand; give each a token of the theme:\n" + "\n".join(found)

    def test_a_colour_written_by_hand_is_found_and_one_in_a_region_or_a_mask_is_not(self):
        page = (
            "a { color: #12ab34 }\n"
            "/* theme: viewer, written from the theme (DS-393): tools/theme.py --write */\n"
            ":root{--bg:#f6f4ef}\n"
            "/* end theme: viewer */\n"
            "b { mask-image: linear-gradient(#000 60%, transparent) }\n"
            "c { box-shadow: 0 1px rgba(0,0,0,.2) }\n"
        )
        assert [n for n, _ in handwritten(page)] == [1, 6]


if __name__ == "__main__":
    found = offenders()
    for line in found:
        print(line, file=sys.stderr)
    print(f"{len(found)} colour(s) written outside the theme" if found else "every colour is the theme's")
    sys.exit(1 if found else 0)
