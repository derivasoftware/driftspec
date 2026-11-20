# Changelog

Every release of DriftSpec says here what it brings and, above all, what a design written for the
release before has to change. The version is the one `ds --version` prints.

While DriftSpec is below 1.0 a release may still rename a word of the language; when it does, the
entry says which word and what to write instead.

## 0.1.15

Every constraint of DriftSpec's own design names the guide that says why it exists (DS-328): four
guides under `doc/knowledge/guides/` cover the licence and the third-party notices, the hundred
millisecond budget, the engine's answers as copies, and the rules of the language's text. Nothing
changes for a design.

## 0.1.14

The CI gate fails on code without design and on stale evidence (DDEV-1507). Nothing changes for a
design.

## 0.1.13

A bare name that two designed classes of its package share is an ambiguity, and it is said
(DS-325): `W-NAME-AMBIGUOUS` names the name and every candidate where the name is written. It
still relates nothing. A design that wrote such a name, silent until now, gets the warning; qualify
the name with its module (`store::Card`).

## 0.1.12

DriftSpec's own design no longer fakes two type aliases as entities (DS-324): an alias is text,
written where it is used as the code writes it. Nothing changes for a design.

## 0.1.11

The live session's code is split by what each part does (DS-322): `driftspec.live` keeps the command
line and every name a caller imports, beside `live_session`, `live_page`, `live_http` and `live_mcp`.
`driftspec-live` works as before. Nothing changes for a design.

## 0.1.10

The design names the native module `_native`, as Python imports it (DS-321), so the Python face's
design writes what its code writes. Nothing changes for a design.

## 0.1.9

The CI gate diagnoses with the current argos-design-driftspec (DDEV-1493). Nothing changes for a design.

## 0.1.8

argos compares DriftSpec's Python face with its code (DS-320): `python` is in the sources, and the
design of the Python face is written as its code writes it. Nothing changes for a design.

## 0.1.7

A dot in a type qualifies a name as `::` does (DS-319): `store.Card`, written as Python writes it,
relates to the class `store::Card` names, and the text stays as written; an undeclared dotted name
such as `typing.Any` is text, as before. A design written for 0.1.6 needs no change; one that
wrote `store::Card` in a Python type may now write `store.Card`.

## 0.1.6

Types are written as the code writes them (DS-18). A design written for 0.1.5 or earlier has to change:

- `extern module` is gone and is now a syntax error. Delete those blocks: a name the design does
  not declare, in a type, a base, a bound or a call, is text, with no relation and no finding.
- The neutral types are gone: write `std::string`, `void`, `std::vector<Line>` in a C++ box and
  `str`, `None`, `list[Line]`, `Line | None` in a Python one, as the code does. `*args` and
  `**kwargs` are parameter names.
- Multiplicity is no longer read from the type: write `many` (`*`) or `optional` (`0..1`) in front
  of the attribute, beside `refs` or `shares`. Two of a kind are `E-MODIFIER-CONFLICT`.
- A bare name that no scope declares is the one class of that name in its package.
- A base keeps its template arguments (`class SceneJson : IFormat<Scene>`), and a method may take
  its own (`+ each<F>(f: F) -> void`).
- A literal is checked only against a designed class; `W-RESULT-NONE`, `W-MODIFIER-ON-VALUE`,
  `W-PARTICIPANT-UNDECLARED` and `E-REF-AMBIGUOUS` are no longer reported.

`tools/migrate_types.py` does most of the rewrite; check what it wrote against the code.

## 0.1.0 (2026-11-18)

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
- Built and tested on Linux, Windows and macOS; the release carries a wheel and `ds` for each
  (Linux and Windows x86-64, macOS on Apple silicon). Not on PyPI yet.
- A site with an editor in the browser, the engine compiled to WebAssembly, twenty guided
  lessons and the guides.

### Known limits

What 0.1.0 does not do yet is listed in the
[using guide](doc/manual/05-using.md#what-010-does-not-do-yet).
