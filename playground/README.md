# Playground

A page that edits a DriftSpec workspace in the browser and draws every diagram beside the text
as it is typed, with twenty guided lessons in English and Spanish (SREQ-00019-1, DS-201). A diagram of
`kind: tour` is played step by step in its pane: the message in the sequence, and the two boxes
it joins (DS-245); `?play=<diagram>` in the address starts one.

| Path | What it is |
|---|---|
| `web/` | The page (`index.html`) and its engine loader (`engine.js`). |
| `lessons/NN-*/` | One lesson each: the workspace it starts from and `lesson.json`, its steps. `lessons/README.md` is the curriculum and the step format. |
| `build.py` | Assembles the static site in `site/`: the page, `ds.js` + `ds.wasm`, the brand from `doc/assets`, and `examples.json`: the lessons, then three whole examples (the shop of `examples/shop`, `examples/hola`, the design of this playground in `design/`, and driftspec's own design). |

The engine is `ds::api::Browser` (REQ-00113-1) compiled to WebAssembly by the tree itself:

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release && cmake --build build-wasm
python3 playground/build.py                      # -> playground/site
python3 -m http.server 8766 -d playground/site   # http://localhost:8766/  (?lang=es, ?example=06-sequence, ?theme=light)
```

CI builds both on every merge request: the `wasm` job gives `ds.js` and `ds.wasm`, the
`playground-site` job the whole site as an artefact.

## Checks

- `tests/system/python/test_playground.py` (SVER-00023-5, run by the `pyds` job) replays every lesson
  through the same engine built as `build-py/libds_browser.so` beside the Python face:
  `cmake -S . -B build-py -DDS_PYTHON=ON && cmake --build build-py --target driftspec_python ds_browser`.
- `lessons/replay.py` is that replay by hand, with `--print` for the final files; `lessons/chain.py --from NN --bump`
  carries a changed lesson into the starting files of the ones after it.
- `lessons/browser_check.sh` plays each lesson in headless Chrome with a scripted reader and compares the
  page's final files with the replay's (local tooling: Chrome on the Windows side of WSL, the site served
  on `$PORT`).
