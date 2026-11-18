# Contributing to DriftSpec

Thank you for wanting to help. Issues and pull requests are welcome: a design that DriftSpec
cannot write, a finding that is wrong, a message that does not say what to do, a guide that
leaves you guessing.

## Before you start

For anything larger than a fix, open an issue first and say what you want to change and why.
The language changes slowly on purpose: a new word or a new rule is decided before it is
written, so that nobody's design breaks for something that may not stay.

## Building and testing

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build                       # the engine, the command line and the grammar
pip install ".[lsp]" pytest && python -m pytest  # the Python package, the language server, the live session
pre-commit run --all-files                   # formatting: clang-format, cmake-format, the usual hygiene
```

A change is ready when all three pass.

## How the code is kept

- **The design comes first.** DriftSpec's own design is written in DriftSpec, in
  [`design/`](design): its requirements, its boxes and classes, and the tests that verify each
  requirement. A change to what the tool does changes the design with it, and `ds check design`
  stays clean.
- **Every requirement names its tests.** A new behaviour comes with a requirement or a change to
  one, and with tests whose names the verification lists.
- **Every example is checked.** The examples in `examples/`, in the guides and in the lessons
  are checked by the tests, so a change that breaks one shows up there.
- **One class per file**, named after it; tests mirror the source tree.
- **Plain words in messages.** A finding says what is wrong, where, and what to do.

## Licence

DriftSpec is under the [Apache License 2.0](LICENSE), and the grammar in `grammar/` under the
[MIT licence](grammar/LICENSE). By sending a contribution you agree that it is licensed under
the same terms, as section 5 of the Apache License says.

## Reporting a security problem

Do not open a public issue; see [`SECURITY.md`](SECURITY.md).
