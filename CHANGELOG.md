# Changelog

Every release of DriftSpec says here what it brings and, above all, what a design written for the
release before has to change. The version is the one `ds --version` prints.

While DriftSpec is below 1.0 a release may still rename a word of the language; when it does, the
entry says which word and what to write instead.

## 0.1.36

The boxes and the plain constructs get binders of their own (DS-356, group one of four): packages, modules, interfaces,
enums, rules and definitions, and diagrams. What every construct shares is a part binder each binder is given by
reference: the fields, the annotations, an extension block, a type, a signature and a formula. The context goes back
to holding only what a binder writes through. The walk the binder had keeps use cases, classes, functions,
sequences and state machines until the next groups. Nothing a design writes, no finding, export or drawing changes.

## 0.1.35

The tree reader reads a field by its id (DS-360). Every field of the grammar is named once, in `parse::Field`, and the
reader turns each into the grammar's field id the first time a field is read; tree-sitter otherwise compares the
field's name with every name of the grammar on each read. A field written wrong is now a compile error, not a child
that is silently never found. Binding takes about a fifth fewer instructions, and nothing a design writes, no
finding, export or drawing changes.

## 0.1.34

Every reader of the model is handed the metamodel by const reference instead of asking the model for it (DS-351). The
checker puts it in the context every step of the check is given, and the evaluator of the rules takes it from there.
The query, the filters that read a cut, the cut itself, the scene with its layout and the layers that name kinds, the
drawing of a diagram and the workspace take it in their constructors from whoever holds it. `Model::metamodel()` is
gone: the model keeps its metamodel for what it declares, relates, renames and covers. The findings, the export and
every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.33

Each construct of the language gets its own binder (DS-353, step two of four, DS-355). A binder says which kinds of
node it takes and binds one of them; a table built once, one slot per symbol of the grammar, finds a node's binder
without comparing a name. The requirements, verifications and constraints are the first with a binder of their own;
the other constructs keep the walk the binder had until DS-356 splits it. Every write goes through the context, which
now also reads the fields and the annotations every construct shares. Binding takes a little less work than in
0.1.30, and nothing a design writes, no finding, export or drawing changes.

## 0.1.32

The model keeps its own lists (DS-346). Moving what is not yet written to the coverage, dropping what only echoes a file
that does not parse, sorting the findings and keeping a table are operations of `ds::model::Model`, and nobody else
changes those lists. An entity the model keeps is read only to everyone else: a field is written through
`Model::annotate`, and the body of a class refining an operator through `Model::refine`, so nothing can change what an
entity is indexed by. A rename moves each entity without copying it, and the metamodel says which fields hold an
identity it carries along. `Relation::change`, which nothing set, and `Identity::asScope`, which nothing called, are
gone. The findings, the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.31

The walks of a behaviour are a class, `ds::model::Paths` (DS-343), made over the model whoever asks already holds:
whether a behaviour names a scenario, the steps the path of one takes and who a behaviour runs on. How a path goes
at each frame is its own, in no header. The scope outside a name is `Scope`'s to tell, and the model's internal
helpers, `model::detail`, are gone. The findings, the export and every page of `ds manual` are the same. Nothing
changes for a design.

## 0.1.30

The binder starts its split into one binder per construct (DS-353, step one of four, DS-354). The binding code reads the
syntax tree through a reader of its own in parse, writes only through one context that holds the model and the notes, and
hands the checker those notes (the references, the messages and the events) instead of itself. Nothing a design writes
changes, and no finding, export or drawing changes either.

## 0.1.29

The Owner approves the two requirement revisions DS-342 left as drafts: REQ-00079-2 (the metamodel describes how each kind
is written, the codes and the annotations; the manual presents them with the commands) and REQ-00160-2 (the manual's guides).
Nothing a design writes changes.

## 0.1.28

A class diagram is drawn less tall (DS-350): the gap under a layer keeps a lane only for an edge that turns in it, so
a long edge running straight down its channel takes no row. Eight of DriftSpec's own thirty diagrams come out a
little shorter, as text and as svg; the 400-class synthetic as text goes from 75,227 rows and 2 GB to 20,679 rows
and 0.55 GB. A text drawing that would still take more than about 128 million characters is refused with what it
would take (draw it as svg, or select less) instead of exhausting the machine's memory. Nothing changes for a design.

## 0.1.27

The manual is a class, `ds::model::Manual` (DS-342), made from the metamodel whoever shows the manual already
holds: the command line and the page's engine. It writes every page (the index, the guides, the grammar, a kind, a
code, the commands, the annotations and the codes) and keeps the commands with their exit codes, which are the
manual's and no longer the metamodel's. The guides embedded from `doc/manual` are the Manual's too. The findings,
the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.26

The metamodel is a class, `ds::model::Metamodel` (DS-341): its tables are built once, when one is made where the
engine starts (the command line, the Python module, the page's engine), and it is handed by const reference to
the model, the binder, the checker and the manual; no global function reaches it any more. The annotations the
binder knows are a list of their own, apart from the two rows the manual adds to document any other word and the
use case an operation names. The findings, the export and every page of `ds manual` are the same. Nothing
changes for a design.

## 0.1.25

The text drawing takes a tenth of the memory it took (DS-350): a cell of the drawing holds its character inline
and its colour as an index into the drawing's palette, 7 bytes instead of some 72. A class diagram of 200 classes
drawn as text needs 380 MB instead of 3 GB, and one of 400 classes, which used to exhaust the machine's memory, 2 GB.
The drawing is the same, byte for byte. Nothing changes for a design.

## 0.1.24

The model answers with less copying (DS-345): the relations of one kind leaving or reaching an entity come
without copying the others, an entity's field is read without a copy, and the sides of an alt in a sequence are
told once when the sequence is walked instead of by a walk up for each question. The two sides of an alt are
named in one place, `Identity::branch`. Nothing changes for a design.

## 0.1.23

The metamodel's tables leave the manual's file (DS-340): the catalogue of finding codes moves to
`src/ds/model/Codes.cpp`, how each kind is written and its example to `src/ds/model/WrittenForms.cpp`, and
the annotations to `Metamodel.cpp`. The manual reads the metamodel and the metamodel no longer reads the
manual. The findings, the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.22

The engine's text helpers live in one module, `ds::text` (DS-344): splitting and joining a list, the lines of a
label, trimming, base64 and XML escaping, each once instead of a copy in each module. Joining a list whose
first item is empty keeps the separator after it (`"", a` joined with `,` is `,a`, not `a`). Nothing
changes for a design.

## 0.1.21

The rules evaluate faster (DS-338): applying a definition visits only the tuples that begin with its first
argument when that one is bound, and tests what the bindings fix before it copies them; a stratum of
definitions with no recursion is fixed in one pass, each definition after those it applies. On DriftSpec's
own design `ds check` goes from about 150 to 105 ms, and the whole repository from 810 to 370 ms. The
findings are the same. Nothing changes for a design.

## 0.1.20

A bare name no scope declares is looked up among the classes of its package through an index the model keeps
by package and name, instead of a pass over every entity for each reference (DS-336). On DriftSpec's own
design `ds check` goes from about 330 to 160 ms, and a generated workspace of 3,200 classes from 5 s to under
1 s. The findings are the same. Nothing changes for a design.

## 0.1.19

Artefact names are recognised by a hand-written scan instead of `std::regex` (DS-335), the regular
expressions a contract check and two number fields built on every call are built once or replaced by a
digit check, and six unused `#include <regex>` are gone. On DriftSpec's own design this saves about 5–6 %
of the instructions of `ds check`, `export` and `render`; the findings are the same. Nothing changes
for a design.

## 0.1.18

The model package has no anonymous namespaces (DS-332): each helper is a private member of the one
class it serves, or lives in `ds::model::detail`, declared in an internal header, and the design
draws them all. `holderOf` and `specNamed` are said once in the model for the check and the state
machines. Nothing changes for a design.

## 0.1.17

A file that does not parse quotes the unexpected text cut on a character boundary (DS-334): a
multi-byte character across the cut once left half of it, and `ds check --json` then aborted with no
findings. Nothing changes for a design.

## 0.1.16

The viewer's tours run on vitest (DS-329), so argos reads them as the tests their verifications
name. Nothing changes for a design.

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
