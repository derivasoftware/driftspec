# The engine in a browser

*What the WebAssembly spike of 2026-11-15 found (DS-203, DS-204), and what using it taught
since (DS-236). The spike's code was redone governed in `ds::api::Browser` and the
playground; this page keeps what is not in the code: why it is built so, and the traps.*

## The shape

- **One call in, one answer out.** The page sends one JSON request (the files, the width)
  and gets one JSON answer: the report, the entities, their locations, the artefacts and
  every diagram drawn as SVG (`ds::api::Browser::answer`, interface `driftspec@ds@browser`,
  REQ-00113-1). No state lives in the engine between calls, so the page never has to keep
  it in step: an edit is a new call with every file. A workspace of the size of driftspec's
  own design (21 files, 2837 entities) opens, checks and draws in the page (DS-236).
- **The same call, native, for the tests.** `ds_browser` is the same C entry point built as
  a shared library when `DS_PYTHON` is on. The lessons are replayed against it
  (`playground/lessons/replay.py`), so CI tests what the page runs without a browser.
- **No server.** The site is static: the page, `ds.js`, `ds.wasm` and `examples.json`
  (`playground/build.py`).

## How it is built

- Emscripten **7.0.0**, pinned in CI (`EMSDK_VERSION`), with the SDK cached by its version
  (`.emsdk/`, cache key `emsdk-$EMSDK_VERSION`).
- `-fwasm-exceptions` at **both** compile and link: the engine throws and catches; the JS
  exception emulation is slower and larger.
- Link options: an ES module (`-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createDs`), one
  exported function (`_ds_playground`) called through `ccall`, `-sALLOW_MEMORY_GROWTH=1`.
- Under `EMSCRIPTEN` the CMake build skips GoogleTest and CLI11 and builds only the
  `ds_wasm` target, then returns: nothing else of the tree is needed in a page.
- The tree-sitter runtime and the grammar compile as they are: no change to the parser.
- Sizes: `ds.wasm` about 1.5 MB, `ds.js` about 10 KB.

## Traps

- **The stack.** Emscripten gives 64 KB by default. The engine recurses (the binder over the
  syntax tree, the walks of the checker), and driftspec's own design overflowed it as
  "memory access out of bounds", which reads like a heap problem and is not one. The build
  links with `-sSTACK_SIZE=8MB` (DS-236).
- **What the page shows is not what `ds check` reports.** Coverage (what is not yet
  written) arrives apart from the findings and the page shows it as information, never as a
  problem (DS-216); a lesson that waits for "clean" must not wait for coverage.

## Open

Where the built site is served (DS-240), and whether these findings become a guide of
argos's BoK, which is the Owner's to decide (argos is QM).
