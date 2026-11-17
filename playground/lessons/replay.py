"""Replay a lesson against the engine: every write, every solution, every check (DS-204).

A lesson is a test too: played with its solutions it must pass every step and end
clean. This mirrors how the page places text (end, below, into) so a lesson that
replays here plays in the page.

    python3 playground/lessons/replay.py [lesson-dir ...] [--print]
"""

from __future__ import annotations

import ctypes
import json
import os
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
# the engine's one call for a page (REQ-00113-1) as a shared library: built with the Python face
# (cmake -DDS_PYTHON=ON ... --target ds_browser), or named by DS_BROWSER_LIB
LIB = pathlib.Path(os.environ.get("DS_BROWSER_LIB", HERE.parent.parent / "build-py" / "libds_browser.so"))

_lib = ctypes.CDLL(str(LIB))
_lib.ds_playground.restype = ctypes.c_char_p
_lib.ds_playground.argtypes = [ctypes.c_char_p]


def engine(files: dict[str, str]) -> dict:
    out = json.loads(_lib.ds_playground(json.dumps({"files": files}).encode()))
    # what is not yet written is no problem (DS-216): read as information, as the page does
    report = out.get("report")
    if report:
        report["findings"] = report["findings"] + [{**f, "severity": "info"} for f in report.get("coverage", [])]
    return out


def indent_of(text: str) -> str:
    return re.match(r"\s*", text).group(0)


def locate(lines: list[str], at: dict) -> int:
    line = next((i for i, t in enumerate(lines) if at["find"] in t), -1)
    if line >= 0 and at.get("after"):
        head = indent_of(lines[line])
        line = next((i for i in range(line + 1, len(lines)) if lines[i].strip() == at["after"] and indent_of(lines[i]) == head), line)
    return line


def tree_order(path: str) -> list:
    """Folders before files, each alphabetical: the order of the page's tree."""
    parts = path.split("/")
    return [(0 if i < len(parts) - 1 else 1, p.casefold(), p) for i, p in enumerate(parts)]


def file_of(files: dict[str, str], o: dict) -> str | None:
    """A step may leave out the file: the one that holds its anchor, in the order of the tree."""
    if o.get("file"):
        return o["file"]
    text = o.get("replace", o.get("find")) or (o.get("below") or o.get("into") or {}).get("find")
    return next((f for f in sorted(files, key=tree_order) if text is not None and text in files[f]), None)


def write(files: dict[str, str], w: dict) -> None:
    w = {**w, "file": file_of(files, w)}
    if not w["file"]:
        raise SystemExit(f"cannot place {w!r}: no file holds its anchor")
    if w.get("remove"):
        files.pop(w["file"], None)
        return
    text = files.get(w["file"], "")
    if w.get("replace") is not None:
        if w["replace"] not in text:
            raise SystemExit(f"cannot replace {w['replace']!r}: not found")
        files[w["file"]] = text.replace(w["replace"], w["text"], 1)
        return
    lines = text.split("\n")
    body = w["text"].split("\n")
    if w.get("end"):
        while lines and lines[-1].strip() == "":
            lines.pop()
        lines += ([""] if lines else []) + body
        files[w["file"]] = "\n".join(lines) + "\n"
        return
    at = w.get("below") or w.get("into")
    line = locate(lines, at)
    if line < 0:
        raise SystemExit(f"cannot place {w!r}: {at['find']!r} not found")
    if w.get("into") and re.search(r"\{\s*\}\s*$", lines[line]):
        head = re.sub(r"\{\s*\}\s*$", "{", lines[line])
        lines[line : line + 1] = [head, indent_of(lines[line]) + "}"]
    if w.get("into") and not w["into"].get("first"):
        head = indent_of(lines[line])
        line = next((i - 1 for i in range(line + 1, len(lines)) if lines[i].strip() == "}" and indent_of(lines[i]) == head), line)
    anchor = locate(lines, {"find": at["find"]})
    indent = indent_of(lines[anchor]) + ("  " if w.get("into") else "")
    lines[line + 1 : line + 1] = [indent + b for b in body]
    files[w["file"]] = "\n".join(lines)


UI = ("opened", "selected", "solo")  # done on the page itself: only the browser check can see them


def passed(done: dict, out: dict) -> bool:
    if done.get("ack") or any(k in done for k in UI):
        return True
    fs = out["report"]["findings"]
    if done.get("entity") and done["entity"] not in out["entities"]:
        return False
    if done.get("kept"):
        row = next((a for a in out.get("artefacts", []) if a["id"] == done["kept"]["artefact"]), None)
        if not row or done["kept"]["by"] not in row["keptBy"]:
            return False
    if done.get("inFile") and (out.get("locations", {}).get(done["inFile"]["entity"]) or [None])[0] != done["inFile"]["file"]:
        return False
    if done.get("finding") and not any(f["code"] == done["finding"] for f in fs):
        return False
    if done.get("gone") and any(f["code"] == done["gone"] for f in fs):  # what is left to write is written (DS-216)
        return False
    if done.get("clean") and any(f["severity"] != "info" for f in fs):
        return False
    if done.get("drawnAs"):
        d = done["drawnAs"]
        svg = next((x["svg"] for x in out["diagrams"] if x["id"] == d["diagram"]), "")
        m = re.search(f'data-id="{re.escape(d["id"])}" class="([^"]*)"', svg)
        classes = m.group(1).split() if m else []
        has = d.get("has", []); has = [has] if isinstance(has, str) else has
        nots = d.get("not", []); nots = [nots] if isinstance(nots, str) else nots
        if not all(c in classes for c in has) or any(c in classes for c in nots):
            return False
    if done.get("draws"):
        d = done["draws"]
        if d["text"] not in next((x["svg"] for x in out["diagrams"] if x["id"] == d["diagram"]), ""):
            return False
    if done.get("hides"):  # the diagram is drawn and no longer shows it: what a cut closes, what a fold stops at
        d = done["hides"]
        svg = next((x["svg"] for x in out["diagrams"] if x["id"] == d["diagram"]), None)
        if svg is None or d["text"] in svg:
            return False
    if done.get("edge"):
        e = done["edge"]
        svg = next((d["svg"] for d in out["diagrams"] if d["id"] == e["diagram"]), "")
        a, b = f'data-from="{e["from"]}" data-to="{e["to"]}"', f'data-from="{e["to"]}" data-to="{e["from"]}"'
        if a not in svg and b not in svg:
            return False
    return True


def replay(folder: pathlib.Path, show: bool) -> bool:
    lesson = json.loads((folder / "lesson.json").read_text())
    files = {}
    root = pathlib.Path(__file__).resolve().parents[2]
    homes = [("", root / "design"), ("playground/", root / "playground" / "design")] if lesson.get("workspace") == "driftspec" else []
    for prefix, home in homes + [("", folder)]:  # its own files, over driftspec's whole design when the lesson walks it
        files.update({prefix + str(p.relative_to(home)): p.read_text() for p in sorted(home.rglob("*.ds"))})
    ok = True
    for i, st in enumerate(lesson["steps"], 1):
        for w in [st["write"]] if isinstance(st.get("write"), dict) else st.get("write", []):
            write(files, w)
        out = engine(files)
        done = st["done"]
        ui = any(k in done for k in UI)
        if not done.get("ack") and not ui and passed(done, out):
            print(f"  {i}: passes before the reader acts (a step that teaches nothing)")
            ok = False
        if not done.get("ack") and not ui:
            for w in [st["solution"]] if isinstance(st.get("solution"), dict) else st.get("solution", []):
                write(files, w)
            out = engine(files)
        good = passed(done, out)
        missing = [d for d in ([st["shows"]] if isinstance(st.get("shows"), str) else st.get("shows", [])) if d not in [x["id"] for x in out["diagrams"]]]
        if missing:  # the diagrams a step says it is about are drawn by the time it is done (DS-242)
            print(f"  {i}: shows {', '.join(missing)}, which is not drawn")
            good = False
        codes = sorted({f["code"] for f in out["report"]["findings"]})
        print(f"  {i}: {'ok ' if good else 'FAIL'} {', '.join(codes) or 'clean'}  diagrams={[d['id'] for d in out['diagrams']]}")
        ok &= good
    final = engine(files)
    clean = not any(f["severity"] != "info" for f in final["report"]["findings"])
    print(f"  end: {'clean' if clean else 'NOT clean'}")
    if show:
        for name, text in files.items():
            print(f"----- {name}\n{text}")
    return ok and clean


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    folders = [pathlib.Path(a) for a in args] or sorted(p for p in HERE.iterdir() if (p / "lesson.json").exists())
    results = []
    for f in folders:
        print(f.name)
        results.append(replay(f, "--print" in sys.argv))
    sys.exit(0 if all(results) else 1)
