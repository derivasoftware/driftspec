# DriftSpec language guide

*Since 2026-10-26 the rules below are also the requirement DAG of the
language, `design/grammar/language.ds` (SREQ-00012-1): concept, entity,
characteristic, with the universal rules as constraints and every leaf fixed
by an oracle test. This guide stays the prose; the DAG is the artefact.*

*The rules of the language as decided, in the order a reader needs them.
Each rule names the corpus that exercises it; the `!` comments in a corpus
file are the findings the tool emits for it, and the tests hold the tool to
them. The vision (`vision.md`) says why; this says what.*

## 1. Workspace and identity

- A workspace is a directory of `.ds` files. `ds` loads it whole; a file is
  not a unit of meaning. `examples/diag`.
- A `package` is the whole scope, the black box. A `module` is a namespace
  and opens the white box. Both reopen across files (R1, R2, R9).
- Full identities name their package with `@` and their modules with `::`,
  like C++ namespaces: the class `Model` of module `ds` in package `ds` is
  `ds@ds::Model`; the boundary `cli` of the package is `ds@cli`; process
  artefacts stay global (`REQ-00001-2`) (R4, decided 2026-10-25). What already
  encloses you is omitted: inside the package, `ds::Model`; inside the module,
  `Model`. No includes, no using: a bare name resolves from the innermost
  scope outward (module, then package), a qualified name crosses modules and a
  `pkg@` one crosses packages (R10). Diagrams live outside packages, so they
  name full ids: `include: ds@ds::Model`, a module `ds@ds::*`, a package `ds@*`.
- Identity: design artefacts (module, class, enum, boundary, behaviour) by
  qualified name; process artefacts (requirement, constraint, verification)
  by id **with its revision**, `REQ-00001-2`. Two declarations of one
  identity are `E-DUPLICATE-IDENTITY` (R5).
- A reference nobody declares creates an expected node and one
  `W-REF-UNDECLARED`; loading never fails on it, only on syntax (R6).
- Multiline text by indentation under the key (R7). A field value runs to the
  end of the line; `//` always starts a comment. Closing braces go on their
  own line.
- `extern module std { class vector<T> }` declares what lives outside the
  workspace, with no obligations (R11).

- **Nested packages (decided 2026-10-26).** A package holds packages: the
  HLD nests. Nested with braces, or reopened flat by its full path
  (`package app@render { }`), like a module reopens across files; the flat
  form needs the outer package declared somewhere (`W-REF-UNDECLARED`
  otherwise, and the outer package becomes an expected node). Every package
  level adds its name with `@`: `app`, `app@core`, `app@core@model::Entity`.
  Level by placement extends: the outermost package is the system; an inner
  package is its own level (argos: a REQ under the SREQ); a module its own.
  In a class diagram the nested packages are containers (the HLD boxes); the
  outermost one is the drawing itself. A package body takes annotations like
  a class body: `@requirement` for what the box realises, `@constraint` for
  what binds it (HLD, decided 2026-10-26). Boundaries of nested packages: not
  yet. Corpus: `examples/packages`.

## 2. Static design (`examples/static`)

- Kinds: `class`, `dataclass`, `enum`, and `function` for an operation of the
  module (below). Interface and abstract are derived
  from the methods and never written (R14).
- Any entity, a member too, may open a body with fields, `note:`,
  annotations, contracts and nested requirements or constraints; nesting
  means "about this" (R12, R13). Braces enclose, one thing per line, a field
  value runs to the end of its line and `//` always starts a comment.
- Contracts (`[requires: …]`, `[ensures: …]`, `[flags]`) are written only in
  the body and kept as text: semantic constraints for the reader and for
  argos, not interpreted by the tool (R8, decided 2026-10-25).
- `extern` declares names only, never signatures: what lives outside the
  workspace is not checked (R11).
- Members: visibility (`+ # -`), then `abstract | static | const` for
  methods or `owns | refs | shares` for attributes, then the name. A method
  named like its class is a constructor. `name: type = default` (R19).
- Ownership from the attribute: `owns` (default, composition), `refs` and
  `shares` (aggregation); multiplicity from the type (`T` 1,
  `optional<T>` 0..1, `list/set/map` *). Parameters and returns give `uses`.
  `E-OWNERSHIP-CYCLE`, `W-MODIFIER-ON-VALUE` (R16).
- Templates: `class Repository<T: Entity>`, identity name plus arity,
  `E-TEMPLATE-BOUND` (R15).
- Operators in the header as a set, `class Vector <<==, ordered, +, []>>`;
  the body may refine an operand; `E-OPERATOR-UNDECLARED` (R17).
- A characteristic of an entity is a requirement or a constraint, nested or
  linked; there is no other vocabulary (R18).

- **Definition and extension (2026-10-26).** An entity is defined once;
  a block with the same id that holds annotations only is an extension: it
  adds relations (`@refines`, `@requirement`, `@constraint`) to the entity
  from wherever the relation belongs, typically the consumer's file, and
  never touches the definition. A box writes its requirements without
  knowing who consumes them; the wiring to the system's SREQ lives in the
  consumer's file and leaves with it. A second definition is still
  `E-DUPLICATE-IDENTITY`; an extension of something defined nowhere is an
  expected node, `W-REF-UNDECLARED`. Corpus: `examples/packages/wiring.ds`.
- **The extern catalogue (2026-10-26).** `extern` modules reopen in any
  file, so the names the workspace borrows (std, a C API) grow into one
  catalogue. A bare name nobody declares in scope resolves to the one extern
  entity of that simple name; two externs with the same simple name make the
  bare reference `E-REF-AMBIGUOUS` (qualify it). The type text stays as
  written, which is what a code reader compares.
- **Enums are annotated like classes (2026-10-26)**: `@requirement` lines
  before the values.
- **Functions of a module (2026-10-26).** A module declares operations no
  class holds, as a Python module or a C++ namespace does:
  `function gather(board: Board, project: string) -> list<Task>`, with a
  member's visibility in front when it is not public (`- function _row_of(..)`).
  Its identity is its module's plus its name, like a class
  (`iris@tasks::gather`); two of one name are `E-DUPLICATE-IDENTITY`. Its
  signature and body are a method's: parameters and the return give `uses`,
  the body takes notes, `@requirement`, `@constraint`, contracts and nested
  artefacts. An extern module declares functions as names. Its behaviour is
  `sequence gather { }`: the participants are its parameters and locals, there
  is no self; a call with no target is a function resolved outward from the
  module, `-> tasks::gather(..)` one of another module, and a method reaches a
  function its class does not declare the same way. A class diagram draws each
  function as a box «function»; a sequence diagram runs it on its module's
  lifeline. Corpus: `examples/functions`.
- **Nothing is reserved (2026-10-26).** `abstract`, `static`, `const`,
  `owns`, `refs`, `shares` are modifiers only in front of a name; followed by
  `:` or `(` they are the name: `+ abstract: bool`, `+ refs: map<..>`. A code
  base designed in DriftSpec keeps its own names.

## 3. Behaviour (`examples/behaviour`, decided 2026-10-25)

- A behaviour is identified by its subject, written as any reference is
  (`Checker::run` beside the class, `ds@ds::Checker::run` from outside):
  `sequence Checker::run { }`, `state Checker { }`. Scenarios as
  `sequence Checker::run / timeout { }`; the unnamed one is the default, the
  one an expansion inlines. Scenarios may be written anywhere (the subject's
  module reopened, another module, a file of their own): the identity is the
  subject's. Two declarations of one scenario, wherever they are, are
  `E-DUPLICATE-IDENTITY`. A diagram names one scenario by id or all of them
  with `ds@ds::Checker::run*`.
- Participants are the static context: `self`, the attributes of the class,
  the parameters of the method, by name; only what the method creates is
  written, `local parser: Parser`. `W-PARTICIPANT-UNDECLARED`.
- Messages `self -> binder.references() -> refs`, `-> resolveReference(ref)`
  for self; arity is checked (`E-MSG-ARITY`); `loop`, `alt … else`, `opt`
  with prose conditions.
- A message to an operation with a behaviour expands to it (derived; a
  diagram inlines it with `depth` or links it). Coupling to another module is
  derived from what a behaviour touches.
- State machines, the behaviour of a class: `initial Idle`, then
  `Idle -> Running on run [guard] / report`: in `Idle`, a call of `run` whose
  guard (prose) holds moves to `Running` running `report`. Event and action
  are operations of the class (`W-EVENT-UNDECLARED`, `W-ACTION-UNDECLARED`);
  the guard is not interpreted. No nested states until a design asks for
  them. Activities come with the process domain.

## 4. Requirements, constraints and verifications (`examples/artefacts`, decided 2026-10-25)

- **Identity and links.** The revision is part of the id. Every link names a
  revision: `@refines SREQ-00001-2`, `@requirement REQ-00001-2`,
  `@constraint CST-00002-1`. An unpinned link is `E-REF-UNPINNED`. A link to a
  revision the workspace no longer declares is `W-REVISION-DRIFT`; it resolves
  to the declared one, so the graph stays whole and the drift is visible.
- **Level by placement.** `requirement` in a package is of the system (an
  SREQ for argos); in a module, of the software (a REQ). No keyword, no field.
  The id prefix follows the host's naming (`SREQ`/`SVER` at the system level,
  `REQ`/`VER` in a module), but the language reads the level from the
  placement, never from the prefix.
- **Fields.** `title` (the shall sentence), `text` and `rationale`
  (multiline), `status: draft | approved` (draft when absent), `tags` (free,
  one line). No `asil`: the team classifies with tags.
- **Parent.** A requirement nested in a requirement refines it; elsewhere,
  `@refines`. A `parent:` field is ignored with `W-PARENT-IGNORED`: one
  representation per concept.
- **Verification.** Nested in its requirement, or `@requirement` from
  elsewhere; `@constraint` from a verification verifies the constraint. `method: test | review | inspection | analysis`. `tests:` lists
  the ids as the runner names them, one per line; `evidence:` lists paths for
  the other methods. `W-VER-NO-TEST` for a test verification without tests.
  The outcome is never in the language.
- **Constraint.** `category:` (argos's subdirectory), `bok:` for the articles
  that ground it. Applies to entities through `@constraint`.
- **One-line form.** `requirement REQ-00002-1: title` and
  `constraint CST-00009-1: title`; a body can be added later.
- **Where.** A separate file that reopens the package is the normal place;
  nesting is for ad-hoc views.
- **Checks.** `W-REQ-NO-VERIFICATION` for an approved requirement nothing
  verifies. The semantic diff adds `W-REVISION-STALE` for an artefact whose
  text changed under the same revision.

## 5. Diagrams

- `diagram CL_X { kind: class | sequence; include: …; exclude: …; hide: fields, methods; show: ownership, multiplicity, operators, notes, derived-stereotypes; depth: n; level: module }`.
- `include` takes ids, a module (`ds` or `ds::*`) or a package
  (`package:ds`); extern entities only when named.
- A diagram is a projection: it selects and decides what to print, never
  what to draw.

## 6. Questions (`ds query`, SREQ-00011-1)

- `ds query <workspace> <question> [subject] [--json]`. Every answer is a line
  `kind  identity  detail`, so one answer feeds the next question or a drawing.
- `diagrams` (each with its kind), `entities <scope>` (the direct members of a
  package, module, class or behaviour; a package may be named plainly),
  `relations <id>` (every relation touching it, `->` out and `<-` in, with
  multiplicity), `artefacts <id>` (what a class realises, requires or is bound
  by, and what verifies each), `behaviours <id>` (the sequences and state
  machines of a method or class), `shows <diagram>` (what the drawing would
  show: the projection itself answers), `findings [file]`.
- An unknown question fails with the list of questions; `--json` gives the
  same answers as an array of `{kind, id, detail}`.
