"""The playground's lessons, played to their end against the engine (DS-204).

Each lesson is replayed step by step with its solutions through the engine's one call for a page
(REQ-00113-1, built as libds_browser beside the Python face): every step must pass, no step the
reader does may pass before the reader acts, and every lesson must end with a clean model.
"""

from __future__ import annotations

import importlib.util
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
LESSONS = ROOT / "playground" / "lessons"


def _replay():
    os.environ.setdefault("DS_BROWSER_LIB", str(ROOT / "build-py" / "libds_browser.so"))
    spec = importlib.util.spec_from_file_location("replay", LESSONS / "replay.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TestPlayground:
    def test_every_lesson_plays_to_a_clean_end(self) -> None:
        replay = _replay()
        folders = sorted(p for p in LESSONS.iterdir() if (p / "lesson.json").exists())
        assert len(folders) == 15
        failed = [f.name for f in folders if not replay.replay(f, False)]
        assert failed == []

    def test_every_example_checks_clean_and_has_a_title_in_both_languages(self) -> None:
        # the whole workspaces offered after the lessons (DS-236): driftspec's own design among them
        replay = _replay()
        spec = importlib.util.spec_from_file_location("build", ROOT / "playground" / "build.py")
        build = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(build)
        offered = build.showcases()
        assert {e["id"] for e in offered} >= {"driftspec", "hola", "shop"}
        for example in offered:
            assert set(example["title"]) == {"en", "es"}, example["id"]
            out = replay.engine(example["files"])
            assert not [f for f in out["report"]["findings"] if f["severity"] != "info"], example["id"]
            assert out["diagrams"], example["id"]

    def test_every_lesson_speaks_english_and_spanish(self) -> None:
        import json

        for folder in sorted(p for p in LESSONS.iterdir() if (p / "lesson.json").exists()):
            lesson = json.loads((folder / "lesson.json").read_text())
            assert set(lesson["title"]) == {"en", "es"}, folder.name
            for step in lesson["steps"]:
                assert set(step["text"]) == {"en", "es"}, folder.name
