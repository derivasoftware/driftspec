"""Rewrite each lesson's starting file from the end of the lesson before it.

Lessons build on one running example: lesson N starts exactly where lesson N-1 ends.
After changing a lesson, run this to carry the change through the ones that follow,
then `replay.py` to check them all. Every file carries over; the title line stays at the top of `shop.ds`.

    python3 playground/lessons/chain.py [--from NN] [--bump]

--bump raises the version of every lesson rewritten, so a reader's saved progress
of the old text is set aside.
"""

from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent


def final_files(folder: pathlib.Path) -> dict[str, str]:
    out = subprocess.run([sys.executable, str(HERE / "replay.py"), str(folder), "--print"], capture_output=True, text=True).stdout
    parts = re.split(r"^----- (.+)$", out, flags=re.M)
    return {parts[i]: parts[i + 1].lstrip("\n") for i in range(1, len(parts), 2)}


def main() -> None:
    start = sys.argv[sys.argv.index("--from") + 1] if "--from" in sys.argv else "02"
    bump = "--bump" in sys.argv
    lessons = sorted(p for p in HERE.iterdir() if (p / "lesson.json").exists())
    for prev, cur in zip(lessons, lessons[1:]):
        if cur.name < start:
            continue
        lesson = json.loads((cur / "lesson.json").read_text())
        number = int(cur.name[:2])
        title = f"// Lesson {number} · {lesson['title']['en'].split(' · ', 1)[1]}"
        files = final_files(prev)
        for old in cur.rglob("*.ds"):  # the lesson's files are exactly the previous lesson's end
            old.unlink()
        for name, text in files.items():
            if name == "shop.ds":
                body = "\n".join(line for line in text.split("\n") if not line.startswith("// Lesson "))
                text = f"{title}\n\n{body.lstrip(chr(10))}"
            path = cur / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text.rstrip("\n") + "\n")
        for d in sorted((p for p in cur.rglob("*") if p.is_dir()), reverse=True):  # folders left empty
            if not any(d.iterdir()):
                d.rmdir()
        if bump:
            lesson["version"] = lesson.get("version", 1) + 1
            (cur / "lesson.json").write_text(json.dumps(lesson, indent=2, ensure_ascii=False) + "\n")
        print(f"{cur.name}: starts where {prev.name} ends ({len(files)} file(s))")


if __name__ == "__main__":
    main()
