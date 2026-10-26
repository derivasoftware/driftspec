# driftspec

A workspace-level specification language for design, requirements and
behaviour, with its engine, language server and MCP surface in one C++ binary.

- `grammar/` — the tree-sitter grammar (`grammar.js`), the generated parser and
  the external scanner for indented text blocks.
- `include/ds/`, `src/ds/` — the engine (namespace root `ds`): parse, model, bind, check, render, diff, query, exporter.
- `app/ds/` — the `ds` command line: `ds check`, `ds export`, `ds render`, `ds draw`, `ds coupling`, `ds diff`, `ds query` (run `ds` for the arguments).
- `examples/diag/` — the reference workspace, one entity per file.
- `tests/unit/ds/` — GoogleTest suites, mirroring the namespaces; `tests/system/ds/` drives the binary from outside.

Build: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`.
Regenerate the parser after editing the grammar: `tree-sitter generate` in `grammar/`.

Governed by argos (ASIL NA). Rules of the language so far: `examples/diag/ds.package.ds`
and the R1–R9 notes in `examples/diag/*.ds`.

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
