# driftspec

A workspace-level specification language for design, requirements and
behaviour, with its engine, language server and MCP surface in one C++ binary.

- `grammar/` — the tree-sitter grammar (`grammar.js`), the generated parser and
  the external scanner for indented text blocks.
- `include/ds/`, `src/ds/` — the engine (namespace root `ds`): parse, model, bind, check, render, diff, query, exporter.
- `app/ds/` — the `ds` command line, five commands (`ds <command> --help` for each):
  - `ds check <workspace> [--json]` — the findings, then the counts of the model.
  - `ds export <workspace>` — the model as JSON, with its metamodel.
  - `ds render <workspace|ir.json> [diagram] [-f html|json|svg|text|cells|ir|layout] [-w N] [-o file] [--embed [--height N]] [--css file]` — the viewer of the whole model (html, framed on the diagram when one is named), its scene (json), or one diagram drawn still or as its data.
  - `ds diff <A> <B> [path] [diagram] [-f text|svg|cells] [-w N]` — the changes in the model's terms, or a diagram with them marked.
  - `ds query <workspace> <question> [subject] [--json]` — diagrams, entities, relations, artefacts, behaviours, shows, findings, coupling.
- `viewer/` — the viewer page compiled into ds (`scene.html`) and its themes (`themes/night.css`, for `--css`).
- `examples/diag/` — the reference workspace, one entity per file.
- `tests/unit/ds/` — GoogleTest suites, mirroring the namespaces; `tests/system/ds/` drives the binary from outside.

Build: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`.
Regenerate the parser after editing the grammar: `tree-sitter generate` in `grammar/`.

Governed by argos (ASIL NA). Rules of the language so far: `examples/diag/ds.package.ds`
and the R1–R9 notes in `examples/diag/*.ds`.

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
