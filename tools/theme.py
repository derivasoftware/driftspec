#!/usr/bin/env python3
"""The pages' theme regions, written from the theme (DS-393).

Each page DriftSpec writes keeps its colours between two markers,
`/* theme: <page>, written from the theme ... */` and `/* end theme: <page> */`; what lies between is what
the theme's writer for that page writes (ds_theme <page>). The values live once, in src/ds/theme/Theme.cpp.

    python3 tools/theme.py --ds-theme build/ds_theme --check    # every region as the theme writes it, or exit 1
    python3 tools/theme.py --ds-theme build/ds_theme --write    # rewrite every region from the theme
"""
from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGES = {
    "viewer": "viewer/scene.html",
    "viewer-night": "viewer/themes/night.css",
    "live": "python/driftspec/live_page.py",
    "playground": "playground/web/index.html",
    "design-page": "tools/design_page.py",
}


def region(text: str, page: str) -> tuple[int, int]:
    """Where a page's region starts and ends in its text: the line after its opening marker, and its closing marker."""
    begin = re.search(r"/\* theme: " + re.escape(page) + r", [^\n]*\*/\n", text)
    end = text.find("/* end theme: " + page + " */")
    if not begin or end < begin.end():
        raise SystemExit(f"{page}: no theme markers in {PAGES[page]}")
    return begin.end(), end


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ds-theme", required=True, help="the ds_theme the build made")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()
    stale = []
    for page, path in PAGES.items():
        file = ROOT / path
        text = file.read_text()
        start, end = region(text, page)
        written = subprocess.run([args.ds_theme, page], check=True, capture_output=True, text=True).stdout
        if text[start:end] == written:
            continue
        if args.write:
            file.write_text(text[:start] + written + text[end:])
            print(f"rewrote {path}")
        else:
            stale.append(path)
    for path in stale:
        print(f"{path}: its theme region differs from what the theme writes; run tools/theme.py --write", file=sys.stderr)
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
