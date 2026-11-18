# Changelog

Every release of DriftSpec says here what it brings and, above all, what a design written for the
release before has to change. The version is the one `ds --version` prints.

While DriftSpec is below 1.0 a release may still rename a word of the language; when it does, the
entry says which word and what to write instead.

## 0.1.0 (not yet released)

The first public release.

### The language

- Requirements, verifications, constraints and use cases, with `@requirement`, `@constraint`,
  `@refines` and `@verification` linking them to the design. Nesting an artefact in what it is
  about says the same as annotating it.
- The structure: packages and modules with their high-level design, interfaces and the
  operations they offer (`@consumes` for a box that depends on another's interface), classes,
  dataclasses, enums, functions, templates, operators, and relations whose multiplicity comes from
  the type.
- The behaviour: sequences with messages, branches, loops, scenarios, attempts and the failures
  they handle, checked against the operations they call.
- The logic: rules (`for`, `where`, `holds`, `never`, `always`) over the model, definitions, and
  state machines whose states are rules over their subject.
- Diagrams as questions asked of the model: class, interface, sequence, state, requirement,
  table and tour views, each choosing what it shows and at which level.

### The tools

- `ds check`, `ds render`, `ds query`, `ds export` and `ds manual`; `ds --version`.
- A viewer of the whole model in one self-contained HTML page, in three perspectives
  (structure, behaviour, logic).
- The Python package `driftspec` (`import driftspec`), with the language server
  `driftspec-lsp` and the live session `driftspec-live` (also as MCP tools for an agent).
  `pip install driftspec` gives `ds` too.
- A tree-sitter grammar with highlight and fold queries, under the MIT licence.
- A site with an editor in the browser, the engine compiled to WebAssembly, twenty guided
  lessons and the guides.

### Known limits

What 0.1.0 does not do yet is listed in the
[using guide](doc/manual/05-using.md#what-010-does-not-do-yet).
