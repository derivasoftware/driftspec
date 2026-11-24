#!/usr/bin/env python3
"""Move the released entries of changelog.d/ into CHANGELOG.md, each under its version.

A merge request writes its entry as changelog.d/<card>.md (see changelog.d/README.md). The version
goes up on merge: version-release pushes "Bump version to X.Y.Z" with a "Version-Bump-For: <sha>"
trailer naming the merge it was made for. For each entry this finds the merge that brought it to
main (the first-parent commit that added the file) and the version of that merge: the one its bump
commit set or, when the merge carried a hand bump and got no bump commit, the one the merge itself
set. An entry whose version is not on the history yet stays where it is.

Run it on an up-to-date main, in a merge request of its own:

    python3 tools/changelog.py            # rewrite CHANGELOG.md and delete the moved entries
    python3 tools/changelog.py --dry-run  # print what would move
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENTRIES = ROOT / "changelog.d"
CHANGELOG = ROOT / "CHANGELOG.md"
MANIFEST = "grammar/tree-sitter.json"
HEADING = re.compile(r"^## (\d+)\.(\d+)\.(\d+)\b.*$", re.M)


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True,
                          text=True).stdout


def version_at(sha: str) -> str | None:
    try:
        text = git("show", f"{sha}:{MANIFEST}")
    except subprocess.CalledProcessError:
        return None
    found = re.search(r'"version"\s*:\s*"([^"]+)"', text)
    return found.group(1) if found else None


def bumps(ref: str) -> dict[str, str]:
    """The merge each bump commit on ref was made for, and the version it set."""
    out: dict[str, str] = {}
    for record in git("log", "--format=%H%x1f%B%x1e", "--grep=^Version-Bump-For: ", ref).split("\x1e"):
        if not record.strip():
            continue
        sha, body = record.strip().split("\x1f", 1)
        marker = re.search(r"^Version-Bump-For:\s*([0-9a-f]{7,40})\s*$", body, re.M)
        version = version_at(sha)
        if marker and version:
            out[marker.group(1)] = version
    return out


def released_as(entry: Path, ref: str, bumped: dict[str, str]) -> str | None:
    added = git("log", "--first-parent", "--diff-filter=A", "--format=%H", ref, "--",
                str(entry.relative_to(ROOT))).split()
    if not added:
        return None  # not on ref's history yet
    merge = added[-1]
    for target, version in bumped.items():
        if merge.startswith(target) or target.startswith(merge):
            return version
    parent = git("rev-parse", f"{merge}^1").strip()
    mine, before = version_at(merge), version_at(parent)
    return mine if mine and mine != before else None


def key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in version.split("."))


def fold(text: str, moved: dict[str, list[str]]) -> str:
    """CHANGELOG.md with each version's entries at the top of its section, new sections in order."""
    for version in sorted(moved, key=key, reverse=True):
        body = "\n\n".join(moved[version]) + "\n"
        heading = re.search(rf"^## {re.escape(version)}\b.*\n\n?", text, re.M)
        if heading:
            text = text[:heading.end()] + body + "\n" + text[heading.end():]
            continue
        lower = next((m for m in HEADING.finditer(text)
                      if key(".".join(m.group(1, 2, 3))) < key(version)), None)
        at = lower.start() if lower else len(text)
        text = text[:at] + f"## {version}\n\n{body}\n" + text[at:]
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ref", default="HEAD", help="the history to read releases from")
    parser.add_argument("--dry-run", action="store_true", help="print what would move")
    args = parser.parse_args()
    bumped = bumps(args.ref)
    moved: dict[str, list[str]] = {}
    files: list[Path] = []
    for entry in sorted(ENTRIES.glob("*.md")):
        if entry.name == "README.md":
            continue
        version = released_as(entry, args.ref, bumped)
        print(f"{entry.name}: {version or 'not released yet, stays'}")
        if version:
            moved.setdefault(version, []).append(entry.read_text().strip())
            files.append(entry)
    if args.dry_run or not moved:
        return 0
    CHANGELOG.write_text(fold(CHANGELOG.read_text(), moved))
    for entry in files:
        entry.unlink()
    return 0


if __name__ == "__main__":
    sys.exit(main())
