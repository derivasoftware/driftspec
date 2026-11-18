<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/assets/driftspec-lockup-h.svg">
  <img src="doc/assets/driftspec-lockup-h-light.svg" alt="driftspec" width="320">
</picture>

A workspace-level specification language for design, requirements and
behaviour, with its engine, language server and MCP surface in one C++ binary.

- `grammar/` — the tree-sitter grammar (`grammar.js`), the generated parser and
  the external scanner for indented text blocks.
- `include/ds/`, `src/ds/` — the engine (namespace root `ds`): parse, model, bind, check, render, query, axis, exporter.
- `app/ds/` — the `ds` command line, five commands (`ds <command> --help` for each):
  - `ds check <workspace> [--json]` — the findings, then the counts of the model.
  - `ds export <workspace>` — the model as JSON, with its metamodel.
  - `ds render <workspace|ir.json> [diagram] [-f html|json|svg|text|cells|ir|layout] [-w N] [-o file] [--embed [--height N]] [--css file]` — the viewer of the whole model (html, framed on the diagram when one is named), its scene (json), or one diagram drawn still or as its data.
  - `ds query <workspace> <question> [subject] [--json]` — diagrams, entities, relations, artefacts, behaviours, shows, findings, coupling.
  - `ds manual [topic] [--json]` — the manual ds carries: how each kind is written, with an example that checks clean; what each finding code means; the annotations; the commands and their exit codes.
- `doc/manual/` — the guides of the manual, Markdown (`NN-topic.md`): intent, structure, behaviour, logic and using. The build embeds them: `ds manual <topic>` prints one and the site's Docs tab draws it, each example beside what the engine makes of it. Every example is checked by the tests.
- `viewer/` — the viewer page compiled into ds (`scene.html`) and its themes (`themes/night.css`, for `--css`).
  The viewer reads one map in three perspectives (structure, behaviour, logic), each with its own lines and marks; touching a box lights up what it composes, aggregates, calls and uses, and docks its card on the right. Tours play along the bottom.
- `examples/` — designs to start from, each checking clean: `hola` (a greeting service in four boxes, the one the viewer is tried on: `ds render examples/hola -o hola.html`), `shop` (a shop from the promise to the code) and `states` (a connection, a machine that is checked).
- `tests/corpus/` — the tests' oracles, not examples: a small workspace per part of the language, each marking with `// !` the findings it must raise.
- `tests/unit/ds/` — GoogleTest suites, mirroring the namespaces; `tests/system/ds/` drives the binary from outside.

Build: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`.

Third-party code compiled into what DriftSpec ships, and its licences: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
Regenerate the parser after editing the grammar: `tree-sitter generate` in `grammar/`.

How a design is read in three perspectives, the outer contract of each and the rules: [`doc/perspectives.md`](doc/perspectives.md).
How we design DriftSpec itself, until each practice is a rule: [`doc/design-practices.md`](doc/design-practices.md).
How the engine runs in a browser, what its WebAssembly build needs and the traps it met: [`doc/webassembly.md`](doc/webassembly.md).

Governed by argos (ASIL NA). Rules of the language so far: `tests/corpus/diag/ds.package.ds`
and the R1–R9 notes in `tests/corpus/diag/*.ds`.

## Seeing a design

`tools/design_page.py --workspace ds=design --out atlas.html` writes one self-contained
page with every diagram beside the DriftSpec it comes from. Clicking a box goes to its text.

## Building and checking

```bash
cmake -S . -B build && cmake --build build -j8     # Debug: the engine, ds and every test
ctest --test-dir build                              # 12 unit suites + the system qualification
pre-commit run --all-files                          # clang-format, cmake-format, hygiene
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release --target ds
```

The performance budget (CST-00001-1, a hundred classes under a hundred
milliseconds per command) is asserted on the Release build. CI runs the same
three gates through deriva/infra/ci-templates (`.gitlab-ci.yml`) plus the argos
diagnose gate, which reads this repository's design through
argos-design-driftspec.

## From Python: pyds and the language server

`pyds` is the engine as a Python package. It runs in the same process, and one
native module (abi3) serves every Python from 3.10 on.

```bash
pip install ".[lsp]"            # builds pyds._native with scikit-build-core; lsp adds pygls
pip install "driftspec[lsp]" --index-url <the group's PyPI index>   # or the wheel main publishes (DS-181)
```

```python
import pyds
model = pyds.load("design")                 # a directory, or {path: text} as an editor holds it
model.find("driftspec@ds@api::Api").kind    # 'class'
pyds.check("design").errors                 # the findings of ds check, typed
pyds.render("design", "CL_Pyds", "text", 120)
```

`driftspec-lsp` is the language server, on stdio. Each file belongs to its own
design (its folder and the folders above it that hold `.ds` files), and that
design is checked on every change. The server also gives definition,
references, hover, completion and symbols. Two commands draw what is under the
cursor: `driftspec.draw` as text and `driftspec.view` in the viewer.

The Python tests run from a build configured with `-DDS_PYTHON=ON`. ctest
includes them, and the `pyds` job runs them in CI.

## A design seen live: driftspec-live

`driftspec-live` follows a workspace while it is written. Run `driftspec-live serve <workspace> --open`
and the viewer opens in the browser: on every save of a `.ds` file it is drawn again where you
were looking, with the findings beside it.

Whoever writes, a person or an agent, can point at the design. They can take the reader to an
identity, highlight identities and put notes on them, and they can read what the reader has
selected. Notes and highlights are kept in `<workspace>/.driftspec/live.json`, outside the design.
The contract is served at `GET /contract`: the routes a client calls and the events it receives.
A browser page and an editor's panel rely on that same contract.

For an agent, `driftspec-live mcp` serves the same contract as MCP tools over stdio (`live_open`,
`live_focus`, `live_note`, `live_highlight`, `live_clear`, `live_selection`, `live_state`):

```bash
claude mcp add driftspec-live -- driftspec-live mcp
```
