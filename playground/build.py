"""Assemble the playground as a static site (DS-204).

    python3 playground/build.py [--wasm build-wasm] [--out playground/site]

Writes the page, the engine (ds.js, ds.wasm from an Emscripten build of the tree, see
REQ-00113-1), the brand, and examples.json: the lessons, in order, then the examples (DS-236). Serve the folder as static files, for instance
`python3 -m http.server -d playground/site`.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent


def workspace(folder: pathlib.Path) -> dict[str, str]:
    return {str(p.relative_to(folder)): p.read_text() for p in sorted(folder.rglob("*.ds"))}


def whole_design() -> dict[str, str]:
    """DriftSpec's own design with the playground's, which is one of its packages kept beside its code."""
    return {**workspace(ROOT / "design"), **{f"playground/{k}": v for k, v in workspace(HERE / "design").items()}}


def lesson_files(folder: pathlib.Path, lesson: dict) -> dict[str, str]:
    """The files a lesson starts from: its own, over driftspec's whole design when it walks it (`"workspace": "driftspec"`)."""
    files = whole_design() if lesson.get("workspace") == "driftspec" else {}
    return {**files, **workspace(folder)}  # what the lesson adds of its own goes with it


def examples() -> list[dict]:
    """The lessons the page offers, in their order."""
    out = []
    for folder in sorted(p for p in (HERE / "lessons").iterdir() if (p / "lesson.json").exists()):
        lesson = json.loads((folder / "lesson.json").read_text())
        out.append({"name": lesson["title"]["en"], "id": folder.name, "files": lesson_files(folder, lesson), "lesson": lesson})
    return out


# whole workspaces offered after the lessons: a small system, the shop of the model audit, and driftspec's own design
EXAMPLES = [
    ("shop", {"en": "Example: a shop, from the promise to the code", "es": "Ejemplo: una tienda, de la promesa al código"},
     HERE / "examples" / "shop"),
    ("hola", {"en": "Example: hola, a greeting service in four boxes", "es": "Ejemplo: hola, un servicio de saludos en cuatro cajas"},
     ROOT / "examples" / "hola"),
    ("states", {"en": "Example: a connection, a machine that is checked", "es": "Ejemplo: una conexión, una máquina que se comprueba"},
     HERE / "examples" / "states"),
    ("driftspec", {"en": "Example: the design of DriftSpec itself", "es": "Ejemplo: el diseño del propio DriftSpec"}, ROOT / "design"),
]


def showcases() -> list[dict]:
    """The examples the page offers after the lessons: whole workspaces that check clean."""
    return [{"name": title["en"], "id": key, "title": title, "files": whole_design() if key == "driftspec" else workspace(folder), "group": "examples"}
            for key, title, folder in EXAMPLES]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wasm", default=str(ROOT / "build-wasm"), help="the Emscripten build that holds ds.js and ds.wasm")
    parser.add_argument("--out", default=str(HERE / "site"))
    args = parser.parse_args()
    out = pathlib.Path(args.out)
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(HERE / "web", out)
    for name in ("ds.js", "ds.wasm", "tree-sitter-driftspec.wasm"):
        shutil.copy(pathlib.Path(args.wasm) / name, out / name)
    # the editor is coloured by the grammar's own queries, the ones every other editor reads (DS-269)
    shutil.copy(ROOT / "grammar" / "queries" / "highlights.scm", out / "highlights.scm")
    # the engine compiled into the page carries third-party code: its notices go with it (DS-284)
    shutil.copy(ROOT / "THIRD_PARTY_NOTICES.md", out / "THIRD_PARTY_NOTICES.md")
    (out / "brand").mkdir()
    for name in ("driftspec-lockup-h.svg", "driftspec-lockup-h-light.svg", "driftspec-tile.svg", "driftspec-icon-180.png",
                 "deriva-lockup-h.svg", "deriva-lockup-h-light.svg"):
        shutil.copy(ROOT / "doc" / "assets" / name, out / "brand" / name)
    (out / "examples.json").write_text(json.dumps(examples() + showcases()))
    print(f"playground site in {out}")


if __name__ == "__main__":
    main()
