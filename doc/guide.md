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
  not a unit of meaning. `tests/corpus/diag`.
- A `package` is the whole scope, the black box. A `module` is a namespace
  and opens the white box. Both reopen across files (R1, R2, R9).
- Full identities name their package with `@` and their modules with `::`,
  like C++ namespaces: the class `Model` of module `ds` in package `ds` is
  `ds@ds::Model`; the interface `cli` of the package is `ds@cli`; process
  artefacts stay global (`REQ-00001-2`) (R4, decided 2026-10-25). What already
  encloses you is omitted: inside the package, `ds::Model`; inside the module,
  `Model`. No includes, no using: a bare name resolves from the innermost
  scope outward (module, then package), a qualified name crosses modules and a
  `pkg@` one crosses packages (R10). Diagrams live outside packages, so they
  name full ids: `include: ds@ds::Model`, a module `ds@ds::*`, a package `ds@*`.
- Identity: design artefacts (module, class, enum, interface, behaviour) by
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
  what binds it (HLD, decided 2026-10-26). Corpus: `tests/corpus/packages`.

## 2. Static design (`tests/corpus/static`)

- Kinds: `class`, `dataclass`, `enum`, and `function` for an operation of the
  module (below). Abstract is derived from the methods and never written; a
  class whose methods are all abstract is still a class (R14, DS-301).
- Any entity, a member too, may open a body with fields, `note:`,
  annotations, contracts and nested requirements or constraints; nesting
  means "about this" (R12, R13). Braces enclose, one thing per line, a field
  value runs to the end of its line and `//` always starts a comment.
- Contracts are rules in the member's body (`perspectives.md`, DS-94). The
  bracket contracts (`[requires: …]`) of R8 are gone: one syntax for one thing.
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
  expected node, `W-REF-UNDECLARED`. Corpus: `tests/corpus/packages/wiring.ds`.
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
  lifeline. Corpus: `tests/corpus/functions`.
- **Nothing is reserved (2026-10-26).** `abstract`, `static`, `const`,
  `owns`, `refs`, `shares` are modifiers only in front of a name; followed by
  `:` or `(` they are the name: `+ abstract: bool`, `+ refs: map<..>`. A code
  base designed in DriftSpec keeps its own names.

## 3. Behaviour (`tests/corpus/behaviour`, decided 2026-10-25)

- A behaviour is identified by its subject, written as any reference is
  (`Checker::run` beside the class, `ds@ds::Checker::run` from outside):
  `sequence Checker::run { }`, `state Checker { }`. Scenarios as
  `sequence Checker::run / timeout { }`; the unnamed one is the default, the
  one an expansion inlines. Scenarios may be written anywhere (the subject's
  module reopened, another module, a file of their own): the identity is the
  subject's. Two declarations of one scenario, wherever they are, are
  `E-DUPLICATE-IDENTITY`. A diagram asks as it is written:
  `ds@ds::Checker::run` the default, `ds@ds::Checker::run / timeout` that one,
  `ds@ds::Checker::run*` them all.
- Participants are the static context: `self`, the attributes of the class,
  the parameters of the method, by name; only what the method creates is
  written, `local parser: Parser`. The result of a message is one too, for the
  messages after it. `W-PARTICIPANT-UNDECLARED`.
- **Behaviour at any cut (decided 2026-11-16, DS-232, `tests/corpus/levels`).** A scenario is written
  in one block, between whatever takes part, without a sequence for every operation on the way:
  ```
  sequence UC-00001-1 {
    -> shell.buy(item)
    shell -> sales.sell(item)
    sales -> warehouse.take(item) -> held
    return 0
  }
  ```
  - The subject may be a use case, a package or a module, as well as a class, a method, a
    function or an operation: `sequence UC-00001-1 { }`, `sequence UC-00001-1 / sold-out { }`,
    `sequence sales { }`.
  - A participant may be named as what it is: a package, a module, an interface or a class the
    behaviour sees, resolved like any name. No `local` declares it.
  - The participant written before the arrow sends the message; with none, the subject does, and
    in a scenario of a use case whoever asks for it, from outside.
  - A message to a box is bound to the operation the box offers under that name (an operation of
    one of a package's interfaces, a function of a module): its arguments are checked, the sender
    has to declare its use, and it expands to the behaviour below. With no such operation it is a
    label, listed as coverage (`W-MESSAGE-NO-OPERATION`) and never a finding: that level is not
    written yet.
  A message written between two participants says that the sender uses the receiver (`sends`,
  DS-230): a design that has only its high level draws its boxes connected.
  Either level stands alone. Where both are written, the high level and the detail lifted onto
  its boxes (`close: package`) draw the same crossings.
- **The two levels are held to each other (2026-11-16, DS-233).** The level below a scenario of a
  use case is the behaviour of the operation it is asked through (`-> shell.buy(item)`), unfolded
  and lifted onto the scenario's participants. Write the high level first; as the detail is
  implemented it is matched against it.
  - The crossings the scenario writes have to be found below, in order. It may leave crossings
    out: an abstraction omits.
  - A crossing the level below never makes is `W-LEVEL-NO-CROSSING`; one it makes at another
    place is `W-LEVEL-ORDER`. Both need the sender to be all written below: every call made in
    it has a behaviour (an empty sequence says an operation calls nothing).
  - Otherwise the crossing is coverage, `W-LEVEL-NOT-BELOW`: `ds query <workspace> coverage`
    lists which steps of the high level have no detail beneath yet.
  - A scenario with no behaviour below it is held to nothing.
- A branch names a scenario, a path through one behaviour (decided 2026-11-05,
  `tests/corpus/branches`): `opt the account is frozen / frozen { ... return 3 }`, and a
  bare `return` ends a branch. What every scenario shares is written once. The path
  takes what is shared and its branch up to its return; the default path takes no
  named branch; of an alt with one named side, the other is taken; a branch it
  cannot tell stays a frame. Calls, diagrams and exit codes ask for it by name, as
  for a sequence of its own, which stays for paths that share nothing. A name
  given twice is `W-SCENARIO-TWICE`. A behaviour asked for by its subject is drawn
  whole, early returns included; an unnamed call follows the default path.
- Values have types (decided 2026-11-05, `tests/corpus/values`): a result has the
  type its operation returns, so a call on it is checked like any other. An
  argument is checked against its parameter (`W-ARG-TYPE`), and taking the
  result of an operation returning `none` is reported (`W-RESULT-NONE`). An
  argument may be a literal, `visit(file, root, "", 7)`, with the type it is written
  in. A value of unknown type has nothing to be checked against.
- What a behaviour gives back and where a call goes (decided 2026-11-05,
  `tests/corpus/scenarios`): `return code` or `return 2`, a name or a literal, never
  an expression, checked against the operation (`W-RETURN-TYPE`). A call names
  the scenario it enters as a declaration does, `vault.withdraw(amount) / refused`,
  and expands to it; one nobody declares is `W-SCENARIO-UNDECLARED`. A trace so
  goes down through its scenario to the value it ends in.
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
- The states of a machine as rules (`tests/corpus/states`, decided 2026-11-17,
  DS-249). A state is computed, not kept: `Paid if status = Paid, not
  cancelled` says when the subject of the machine is in it, over what the
  subject holds. The literals are joined by commas and all hold: an attribute
  compared with a value (`=`, `!=`), or a name alone, which is a boolean
  attribute, or a condition with a name when the subject holds nothing so
  called; `not` turns a literal round. A state may have several rules, each
  one way of being in it. The jumps are written beside the rules as before.
  - The rules of a machine exclude each other: whatever the subject holds, it
    is in one state. Two states that can hold at once are `W-STATE-OVERLAP`,
    and the finding says a context in which they do. What no rule covers is
    nothing to report.
  - A rule that cannot hold as written is a finding: a value the type of the
    attribute does not have (`W-STATE-VALUE`), something the subject does not
    hold (`W-STATE-UNDECLARED`), literals that exclude each other
    (`W-STATE-IMPOSSIBLE`).
  - The start is not written: the machine starts in the state the defaults of
    its subject are in, and a state no jump leads to from there is still told
    (`W-STATE-UNREACHABLE`). A state diagram draws each state with its rule.
- The calls of the behaviours, held to the machine (DS-250). The behaviours
  are read as they are. Along one, what it calls an event on may be in any
  state to begin with, and each call of an event moves it by the jumps on
  that event; a jump with a guard may leave it where it was. A frame may not
  happen, the sides of an alt are each one way, and a loop is read once. An
  operation that is no event moves nothing. A call the machine lets happen
  from no state its receiver may be in is `W-STATE-ORDER`, where it is made:
  *Desk::rush calls order.cancel() when order can only be Shipped: the machine
  of Order lets cancel happen from Open or Paid.* In a state diagram each jump
  says, when pointed at, the behaviours that call its event.

## 4. Requirements, constraints and verifications (`tests/corpus/artefacts`, decided 2026-10-25)

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
  (multiline), `status: draft | approved | obsolete` (draft when absent), `tags` (free,
  one line). No `asil`: the team classifies with tags.
- **Nesting and annotating are the same relation (Owner, 2026-11-15).** Two
  syntaxes that only change where an artefact is filed: nested in the same
  file, or written elsewhere and linked with an annotation. Today the engine
  keeps it for `refines` and `verifies`; a requirement nested in a class still
  binds `about` instead of `satisfies` (DS-217).
- **Parent.** A requirement nested in a requirement refines it; elsewhere,
  `@refines`. A `parent:` field is ignored with `W-PARENT-IGNORED`: one
  representation per concept.
- **Verification.** Nested in its requirement, or `@requirement` from
  elsewhere; `@constraint` from a verification verifies the constraint. `method: test | review | inspection | analysis`. `tests:` lists
  the ids as the runner names them, one per line; `evidence:` lists paths for
  the other methods. A test verification without tests is coverage (`W-VER-NO-TEST`).
  The outcome is never in the language.
- **Constraint.** `category:` (argos's subdirectory), `bok:` for the articles
  that ground it. Applies to entities through `@constraint`.
- **One-line form.** `requirement REQ-00002-1: title` and
  `constraint CST-00009-1: title`; a body can be added later.
- **Where.** A separate file that reopens the package is the normal place;
  nesting is for ad-hoc views.
- **Checks.** An approved requirement nothing verifies, and a verification by
  test with no tests, are coverage, not findings: what is not yet written
  (DS-216), answered by `ds query <workspace> coverage`.

## 3b. Visibility (`tests/corpus/visibility`, decided 2026-11-05)

- One rule at every level: a box connects to another through a public child. A
  package's public children are its interfaces and modules, a module's its classes
  and functions, a class's its methods and attributes. There is no port concept:
  a port is a public child, and an interface is one that crosses a process.
- A child is public unless it says `-`: `- module internal { }`, `- class Helper { }`,
  `- function round(n: int) -> int`, `- margin() -> int`. It is seen from inside its
  parent, so siblings see each other.
- A call or a type that crosses anything private on its way down is `W-NOT-VISIBLE`.
  A public member showing a private type is `W-PRIVATE-LEAK`. Dependencies between
  boxes stay derived from calls and types (`ds query <workspace> coupling`).

## 4b. Interfaces (`tests/corpus/interfaces`, decided 2026-11-05)

- An interface is an external interface of a process, or of a library when it
  is binary. It lives in a package, at any level: `interface shell { }` is
  `shop@shell`. It replaces `boundary`.
- `kind:` and the fields of an operation are data, not grammar: `command:`,
  `exit 2: refused`. A new kind changes no syntax. An operation has typed
  parameters and a return, and takes a body.
- Quality is not a field: `@requirement` and `@constraint` on the interface or
  on an operation.
- An interface behaves as a class does. `sequence shell::buy / refused { }` is
  the trace of an operation, going down through the scenario each call names:
  `W-OPERATION-NO-BEHAVIOUR` when it has none. `exit 2: refused` names a scenario
  (`W-SCENARIO-UNDECLARED`), and that trace has to end in that value
  (`W-OUTCOME-MISMATCH`); what cannot be told gives no finding.
- `state web { }` is optional. In a graphical interface it is the navigation and
  its states are `view`s: free text and what binds them, nothing that reinvents
  HTML. An event that names an operation goes down to it; any other is a label.
  `W-VIEW-UNDECLARED`, `W-VIEW-UNREACHABLE`, `W-VIEW-NO-WAY-OUT`.
- `@interface shell` says a class realises it. A sequence or a state may sit
  directly in a package.

## 5. Diagrams

- `diagram CL_X { kind: class | sequence | requirement | state | table | interface | tour; include: …; exclude: …; open: …; close: …; cut: …; unfold: …; fold: …; depth: n; hide: fields, methods; show: ownership, multiplicity, operators, notes, derived-stereotypes }`.
- `include` takes ids, a module (`ds@ds` or `ds@ds::*`) or a package
  (`ds@*`), read alike by every kind of diagram; extern entities only when
  named. A pattern that names what nothing declares is `W-DIAGRAM-UNDECLARED`.
- A diagram is a projection: it selects and decides what to print, never
  what to draw.
- **The cut (decided 2026-11-16, DS-229, `tests/corpus/cut`).** A cut is a set of boxes none of
  which holds another: what is above it stays open, and everything below it is drawn as the
  outermost closed box around it. It is written as what closes and what opens, never as its
  frontier, so whatever is written is a cut, and one branch may go down to its classes while
  another stays one box. There are no numbered levels.
  The words (decided 2026-11-16, DS-228), one for each thing: a **cut** is the flat layer; boxes
  **open** and **close**; calls **unfold** and **fold**; what lies below a cut is **lifted** onto
  it (a relation, a message, what answers to a requirement). The two ends of the axis are the
  **high level** and the **detail**, never a level with a number; and depth counts calls, so it
  is not said of boxes. In Spanish: corte, abrir y cerrar, desplegar y plegar, alzar, nivel alto
  y detalle.
  - `close:` names the boxes that close: a name (`close: shop@warehouse`), a prefix
    (`close: shop@sales@*`) or a kind of box (`close: module`, `close: package`), so a cut runs at
    one kind of box wherever each one is. Every other box stays open.
  - `open:` names the boxes that stay open, with the way to each, whatever `close:` says:
    `close: module` with `open: shop@sales@till` closes every module but the till.
  - `open:` written alone closes every other box: `open: shop@store@stock` opens that box and the
    way to it, and `open:` with nothing after it closes them all.
  - `cut: HLD_Till` takes the cut of that diagram, and what the diagram writes itself is added.
  - A diagram that writes none of them draws everything open. The outermost package is the
    drawing itself and never closes.
  A closed box shows only its public children, as a class shows its members (DS-244): a
  compartment for each kind of thing it offers, packages, modules, interfaces, classes,
  dataclasses, enums and functions, each by name and with the badge a box of its own would
  carry. An edge that loses an end to it is lifted, and
  between boxes it is one of two things (2026-11-16, DS-230): a structural bond, a solid arrow,
  when one owns, refers to, derives from or realises something of the other; a use, a dashed
  arrow, when it names a type of the other, calls it, sends it a message or declares its use.
  Each is drawn once between two boxes, and what stays inside a closed box is not drawn. A use
  between two boxes says the interfaces of the one it reaches that the other goes through, a line
  each with the operations asked (`engine: check, render`), as what the arrow says when pointed
  at (2026-11-17, DS-266); a use through no interface says nothing. A behaviour that crosses
  boxes counts too, so an interface reaches what handles it: the component view is derived,
  never written. `ds query <workspace> shows <diagram>` answers with the same cut. A pattern
  that names no box, or a `cut:` that names no diagram, is `W-DIAGRAM-UNDECLARED`.
- **A sequence and a requirement diagram stand on the same cut (2026-11-16, DS-231).** In a
  sequence diagram a closed box is one lifeline for everything inside it. A message between two
  things of one closed box is not drawn; one that crosses is drawn between the two lifelines,
  and into a closed box it says which child takes it (`Desk::sell(item)`). A frame of the detail
  is kept around what crosses inside it. In a requirement diagram what answers to a requirement
  is the box that stands for it. `cut: HLD_Shop` in either makes them agree with the boxes
  drawn. `level: module` is gone: it is written `close: module`.
- **A table (2026-11-17, DS-251)** draws a machine as a table:
  `diagram TB_Order { kind: table; include: shop@order::Order }`. Its events go down and its
  states across, in the order the machine writes them; a cell says where the event takes the state
  (`Paid`, `Paid [enough]`, `Paid / Held`), and is empty where the machine has no jump. Over it,
  what the behaviours written do: the heading of a row says who calls its event, and a cell in
  which a behaviour calls an event against the machine (`W-STATE-ORDER`) names that behaviour.
- **A table of anything (2026-11-17, DS-268)** says its rows as a rule says what it speaks about,
  and a column per question:
  `diagram TB_Reqs { kind: table; for requirement r in shop; where r.status = "approved";
  column requirement: r.id; column says: r.title; column kept: x where x satisfies r;
  column proved: holds v verifies r }`. A row per binding of the ranges that passes the filter.
  A column is a field of the row (`r.title`; `name` and `id` every entity has, `holds` says of a
  rule whether the model keeps it), what is related to it (`x where <literals>`: every `x` that
  answers, by name), or a yes or no (`holds <literals>`). The first column heads each row. A
  comparison reads a field of what a variable is bound to (`r.status = "draft"`), in a table and
  in a rule alike, and an artefact is in the box it is written in. A cell too long for a line
  goes on several.
  The state diagram shows the ways through; the table shows every pair the machine says nothing of.
- **A tour (2026-11-16, DS-245)** is one diagram that relates a behaviour to the boxes it crosses:
  `diagram TOUR_Buy { kind: tour; include: shop@shell::buy; close: module }`, with the fields of
  a sequence diagram. It draws two parts on one cut: who takes part, as boxes with the relations
  between them, and the sequence. Each lifeline is one box of the other part. The playground
  plays it step by step: the message in the sequence, and the two boxes it joins.
- **The calls a sequence follows (DS-231)** are a cut of their own, written the same way.
  - `fold:` names the calls that are not followed: `fold: shop@warehouse` every call that lands
    in that box, `fold: shop@sales@till::Till::charge` one operation. Every other call is followed.
  - `unfold:` names the calls that are followed, whatever `fold:` says. Written alone it folds
    every other call, and `unfold:` with nothing after it draws only what the behaviour writes.
  - `depth: 2` is the uniform case for the calls neither names, `depth: *` every level
    (`W-DIAGRAM-DEPTH` for anything else).
  - With none of the three, a diagram that writes a cut follows every call, since what a closed
    box does inside is seen only through what crosses out of it; one that writes no cut follows
    none, as before.
  A class diagram ignores them.
- `kind: state` draws a machine, `include: shop@web` naming its class or interface:
  from the initial state down, a jump with its event and guard, a jump to the same
  state as a loop, and of a view its text and what binds it.

## 6. Questions (`ds query`, SREQ-00011-1)

- `ds query <workspace> <question> [subject] [--json]`. Every answer is a line
  `kind  identity  detail`, so one answer feeds the next question or a drawing.
- `diagrams` (each with its kind), `entities <scope>` (the direct members of a
  package, module, class or behaviour; a package may be named plainly),
  `relations <id>` (every relation touching it, `->` out and `<-` in, with
  multiplicity), `artefacts <id>` (what a class realises, satisfies or is bound
  by, and what verifies each), `behaviours <id>` (the sequences and state
  machines of a method or class), `shows <diagram>` (what the drawing would
  show: the projection itself answers), `findings [file]`, `coupling`, and
  `failures <operation>` (what escapes it and the way it comes up), and
  `coverage [identity]`: what is not yet written, which the check does not
  report (an operation with no sequence, an outcome with no scenario, a use
  case nothing realises, a requirement nothing verifies, a verification with no
  tests, a rule with no requirement). An oracle corpus marks coverage `// ?`.
- An unknown question fails with the list of questions; `--json` gives the
  same answers as an array of `{kind, id, detail}`.
