# driftspec

A workspace-level specification language for design, requirements and
behaviour, with its engine, language server and MCP surface in one C++ binary.

- `grammar/` — the tree-sitter grammar (`grammar.js`), the generated parser and
  the external scanner for indented text blocks.
- `include/driftspec/`, `src/driftspec/` — the engine: parse, bind, check, export.
- `app/ds/` — the `ds` command line: `ds check <workspace>`, `ds export <workspace>`.
- `examples/diag/` — the reference workspace, one entity per file.
- `tests/unit/driftspec/` — GoogleTest suites, one per subsystem.

Build: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`.
Regenerate the parser after editing the grammar: `tree-sitter generate` in `grammar/`.

Governed by argos (ASIL NA). Rules of the language so far: `examples/diag/ds.package.ds`
and the R1–R9 notes in `examples/diag/*.ds`.
