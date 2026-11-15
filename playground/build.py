"""Assemble the playground as a static site (DS-204).

    python3 playground/build.py [--wasm build-wasm] [--out playground/site]

Writes the page, the engine (ds.js, ds.wasm from an Emscripten build of the tree, see
REQ-00113-1), the brand, and examples.json: the lessons first, then the starter and the
repository's example workspaces. Serve the folder as static files, for instance
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


def examples() -> list[dict]:
    out = []
    for folder in sorted(p for p in (HERE / "lessons").iterdir() if (p / "lesson.json").exists()):
        lesson = json.loads((folder / "lesson.json").read_text())
        out.append({"name": lesson["title"]["en"], "id": folder.name, "files": workspace(folder), "lesson": lesson})
    out.append({"name": "starter", "files": workspace(HERE / "starter")})
    for folder in sorted(p for p in (ROOT / "examples").iterdir() if p.is_dir()):
        files = workspace(folder)
        if files:
            out.append({"name": folder.name, "files": files})
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wasm", default=str(ROOT / "build-wasm"), help="the Emscripten build that holds ds.js and ds.wasm")
    parser.add_argument("--out", default=str(HERE / "site"))
    args = parser.parse_args()
    out = pathlib.Path(args.out)
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(HERE / "web", out)
    for name in ("ds.js", "ds.wasm"):
        shutil.copy(pathlib.Path(args.wasm) / name, out / name)
    (out / "brand").mkdir()
    for name in ("driftspec-lockup-h.svg", "driftspec-lockup-h-light.svg", "driftspec-tile.svg", "driftspec-icon-180.png"):
        shutil.copy(ROOT / "doc" / "assets" / name, out / "brand" / name)
    (out / "examples.json").write_text(json.dumps(examples()))
    print(f"playground site in {out}")


if __name__ == "__main__":
    main()
