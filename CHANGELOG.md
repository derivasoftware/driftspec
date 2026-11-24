# Changelog

Every release of DriftSpec says here what it brings and, above all, what a design written for the
release before has to change. The version is the one `ds --version` prints.

While DriftSpec is below 1.0 a release may still rename a word of the language; when it does, the
entry says which word and what to write instead.

## 0.1.72

The one place that names the concrete classes of an interface is a factory, named so (DS-404). Steps is StepFactory
and Filters is FilterFactory; the node binders are made by NodeBinderFactory instead of the binder's constructor; and
the Api asks CheckerFactory for the checker and holds it as the IChecker it is, so the load names no concrete checker
(the viewer's tour of a load now reaches the check at IChecker::run). Nothing a design writes changes.

## 0.1.71

The language server and the live viewer show a finding's severity as the severity table gives it (DS-386). The engine
hands the table to Python (`driftspec.severities()`: each severity's name, whether it fails, whether it is kept apart as
coverage, and what an editor shows it as), and no severity is named in Python. The language server publishes each finding
with the table's editor mapping; what it publishes does not change. The live viewer colours each finding by its row,
errors red and warnings amber; its findings pill counts what fails, with the rest beside it; and the coverage, which
reached no Python door, has a folded section of its own in the information colour, fed by a new `coverage` field of the
state. The live contract is version 5.

## 0.1.70

A rule reads what an entity is, matches a value against a pattern and reads a member's flag (DS-403). `name(x)`,
`id(x)` and `kind(x)` are the name, the identity and the kind of the entity bound to x; they read it and never bind
it. `name(c) matches "^I[A-Z]"` holds when the value contains a match of the pattern, an ECMAScript regular
expression compiled once when the rule is checked. `m is abstract` holds when m carries the flag.

What a design written for the release before has to change: `name`, `id` and `kind` are now words of the rule
language. A definition of one of those names is reported (E-RULE-RESERVED) and can no longer be applied, and none of
the three can stand as a variable where a literal begins. A pattern that is no string or does not compile is reported
at the rule (E-RULE-PATTERN). No kind may declare a field named `name` or `id`. Every design that used none of them
checks, draws and exports as before.

## 0.1.69

The Owner approves the roadmap's drafts: check's finding order and architecture (REQ-00193-1..REQ-00195-1 with
VER-00213-1..VER-00215-1), query's (REQ-00200-1, VER-00220-1, VER-00003-7, VER-00037-8) and the live viewer's and files
rule's verifications (VER-00250-1..VER-00252-1). Nothing a design writes changes.

## 0.1.68

A workspace's files are read by one rule, the engine's, wherever Python reads them (DS-413). `driftspec.files()` lists
the .ds files of a folder as `ds` reads a workspace: every regular .ds file in every folder below it, dot-folders included;
a link to a file is followed and a link to a folder is not; a broken link, and a file named only `.ds`, are skipped; sorted.
`driftspec.read()` reads with it, so a broken link no longer stops it and `.ds` is no longer read. The live session's
watcher stamps with it, so a save inside a dot-folder now reaches a new version, as the engine already read it. The
language server finds a folder's own files with it.

## 0.1.67

The scene builds what it places from the model apart from how it is placed (DS-389). A projection reads the model
into nested frames, each package, module, interface, unit and row sized by what it shows, its children in the order
they are read and the room each kind of box asks for; a packer that knows no model turns those frames into boxes:
a frame keeps its size, fills shelves no wider than a target, or centres its first child over the row of the others.
Nothing a design writes changes: the viewer, its scene and every drawing are the same.

## 0.1.66

The check's architecture decisions are requirements held by rules (DS-383). The checker runs each step through IStep
and knows none of the concrete ones, no step knows another, and every operation of the check is a member of a class;
three rules of ds's own design hold them, each proven by a deliberate violation. SEQ_Load draws a load from the command
line two levels down. Nothing a design writes changes.

## 0.1.65

The contracts are checked in five steps, and the order of what the check reports no longer follows its steps
(DS-382). ContractCheck becomes InterfaceContracts, UseCaseContracts, DiagramCheck, ScenarioCheck and FailureCheck,
each run on its own and chosen on its own (`interfaces`, `usecases`, `diagrams`, `scenarios`, `failure-check`); the
findings each one says are the ones ContractCheck said.

An intended change of output: the findings and the coverage are both ordered by file, then line, and on one line by
code, then message. The coverage was never sorted and came out in the order the steps ran in, so a design whose
coverage spans several steps now reads it by where each entry is written. On the 28 workspaces of the comparison this
reorders the coverage of three (the audit and levels corpora and the repository) and two pairs of findings on one
line; nothing is added or removed, and the golden files do not move.

## 0.1.64

Every operation of the query is a member of its class (DS-385). A pattern of a diagram's include or exclude is read
once, by Pattern, which then takes or leaves each entity by comparing its identity and scope with what it read, with
no text built for each one. The one place that names the filter of each kind of diagram is the class Filters, where it
was a function, with a static of() as check's Steps has (DS-382), and the box a relation is drawn from, with the
helpers the filters hid, are members of the filter they serve. A rule of ds's design holds that the query writes no
function, refining what the query answers. Nothing changes for a design: every finding, export, drawing, viewer page,
answer and exit code is the same.

## 0.1.63

The golden ctest compares without pytest (DS-415). `ctest` ran the golden files through `python3 -m pytest`, so on a
machine whose python3 has no pytest the test failed for that reason alone. `tests/system/python/test_golden.py --check
--ds <ds>` makes the same comparison as a plain script and exits 1 when a document differs; ctest runs it that way and
needs only python3. CI's golden job still runs it under pytest, for its report.

## 0.1.62

A live event carries what it was sent with (DS-414). Each event shared the session's own notes, highlights and comments,
and a page serialised them when it took the event off its queue, so an event could show a change made after it was
sent (an annotate event for a clear, carrying the comment written next). The events now carry a copy taken when they
are sent, one for every page. Nothing else a page reads changes.

## 0.1.61

The live session is split into its parts, and its contract is declared once (DS-412). `Live` holds the version shown
and composes `Annotations` (the notes, highlights and comments, kept in `.driftspec/live.json`), `Events` (the pages
that follow it) and `Watcher` (the .ds files it watches). The contract is one table in `driftspec.live_contract`: each
operation with its route, what the session answers, its MCP tool and its command word. The HTTP routes, the MCP tools and
the command line are read from it, and a test checks that they agree. What a client sees does not change: GET /contract,
the state, the events, tools/list and the command line are the same, byte for byte (contract version 4).

## 0.1.60

The viewer orders the boxes of a package in one pass over the inheritances (DS-388). To put a module whose classes
are inherited from before the modules that inherit, the layout asked of every pair of boxes, again for each box it
placed, whether one inherits from the other, by walking every inheritance of the model; on a design of a few
thousand classes that was half of the layout and a sixth of `ds render -f json`. Each inheritance is now read once, as the boxes it
joins, and the boxes are taken in the same order as before: the first one written that waits for none of those
left, and the first one left when they wait for each other. Nothing a design writes changes: the viewer, its scene
and every drawing are the same.

## 0.1.59

The Owner approves the roadmap's drafts: the evaluator's requirement and its verification (REQ-00192-1,
VER-00212-1), the render steps' verification (VER-00094-3), the viewer's compression fixture (VER-00083-4) and the golden
outputs' system verification (SVER-00029-1). Nothing a design writes changes.

## 0.1.58

The check names its steps in a class, and its steps keep no hidden helpers (DS-382). The free function that made the
steps a selection asks for is `Steps::of`, the one place that names the concrete steps. What three steps did in
functions hidden in their files is now theirs: CallOrder says the states a call could come from, ContractCheck tells
whether a pattern names something declared, and LevelCoherence finds the operation a message calls, the behaviour below
an entry, the crossings below it and where each one said finds its place. The crossings and what lies below an entry are
written in the design. Nothing a design writes changes.

## 0.1.57

Every format of every document ds writes is pinned as a golden file (DS-395), before the formats are refactored
(DS-394): the scene as json and html, each diagram as ir, layout, svg, text and cells, the report and the answers as text
and json, and the export, over three frozen workspaces: a copy of ds's own design, a copy of the hola example with a table
and a tour so that every kind of diagram is drawn, and a small workspace with findings. ctest and a CI job of their own
compare them with what the ds just built writes; `python tests/system/python/test_golden.py --ds <ds>` writes them again,
a change reviewed like any other. Nothing a design writes changes.

## 0.1.56

The machine is written as requirements held by rules (DS-367): every operation of the machine is a member of the class
whose responsibility it is, and the machine reads the model it reads a state machine from and leaves it as it is. What
its source hid is now classes of the machine: the lists the binder writes into a field (`WrittenFields`), a condition
of a state's rule (`Literal`), the rules and the jumps of a machine (`StateRule`, `Jump`), what its subject holds
(`Subject`), what each name of a context may be worth (`Context`) and the walk of the behaviours for the calls of its
events (`Calls`). Nothing a design writes, no finding, export or drawing changes.

## 0.1.55

The viewer's test that a behaviour is written once and replayed by many tours runs on a fixture made for it (DS-377):
one behaviour called from four places, whose calls and steps are known, instead of a ratio of ds's own design that moved
with every change to it. ds's design keeps the tours that name its entities. Nothing a design writes changes.

## 0.1.54

The rules are evaluated with far less copying (DS-381). The solver bound each variable in a map of strings and copied
the whole map, strings included, for every candidate it tried, and climbing what holds what copied a string per step.
Each formula's variables now get fixed slots once, and a binding is a vector of pointers to texts that the evaluation
owns and that never move: the facts, which copy what holds what from the model's index once, and a few texts the solver
keeps itself. A candidate copies a few pointers. Subjects, witnesses and messages are ordered exactly as before.
Nothing a design writes changes: findings, breaches and tables read the same; `ds check` on a design with rules spends
roughly half the instructions it did in the evaluator.

## 0.1.53

The exporter is written as requirements held by rules (DS-367): every operation of the exporter is a member of the
class whose responsibility it is, and the exporter reads the model it writes and leaves it as it is. The helpers
JsonExport's source hid (a location, an entity, a relation, the names of kinds and the metamodel as JSON) are its
private members. Nothing a design writes, no finding, export or drawing changes.

## 0.1.52

The evaluator of the rules is five pieces (DS-380). `Facts` reads the model once into what a rule is evaluated over:
the entities of each kind, every relation both ways, the names a formula writes resolved, and what each definition holds
for; it is the pieces' one reader of the model. `RuleValidator` checks each formula before it is evaluated, `Strata`
orders the definitions, and `Solver` answers a body over the facts and never the model. `Evaluator` directs them: it
records into the facts what the validator resolved and what the definitions derive, and writes the breaches and the
tables into the model. The design of ds holds the solver to the facts with a rule. Nothing a design writes changes:
findings, breaches and tables read the same.

## 0.1.51

A drawing loads only the steps of the check it reads (DS-376). `ds render` to svg, text, cells, ir or layout, and the
engine's render that Python and an editor's preview call, run the linker, behaviour typing and derivation, and the rules
as well when the diagram is a table written as rows and columns. The viewer (html, json), check, export and query run
every step, as before. The Api chooses the steps from what the command gives, in one place, and a rule of ds's design
holds that decision there. Drawing CL_Ds takes about half the time: 243 to 111 ms on DriftSpec's design, 577 to 252 ms
on the whole repository. Nothing changes for a design: every drawing, finding, export, viewer page and exit code is the
same.

## 0.1.50

The check finds what each class declares through the index of its members (DS-375). To derive a class's
stereotypes (abstract, value and the operator families) it read every entity of the model, once per class, so the
time grew with the square of the design: a synthetic design of 3 200 classes spent most of its check there. Each
class now reads only its own members, the same ones in the same order. Nothing changes for a design: no finding,
export or drawing.

## 0.1.49

The Owner approves the requirements DS-379 left as drafts, with their verifications (every finding is made from the
catalogue; nothing else writes a severity: REQ-00190-1, REQ-00191-1), and the viewer-tours verification VER-00082-3.
Nothing a design writes changes.

## 0.1.48

A finding is made from its code (DS-379). Seventy places wrote a finding's code and severity as two strings, which
agreed with the catalogue only by care; each now asks the metamodel for the finding of a `Code`, and the catalogue
gives the code as it is printed and its severity, read from one slot. A code the catalogue does not hold is no `Code`
and does not compile. Severity is a value whose meaning is a row of the metamodel's severity table: its printed name,
whether it fails the check, whether it is kept apart as coverage and what an editor shows it as. The check, the model,
the export, the formats and the manual ask the row instead of naming a severity, and a check's report carries the
table, so a format asks it without the metamodel. A coverage finding is born coverage. The design of ds says so in two
rules: no class outside the model reads a code's row, and only the metamodel and the report hold a severity's row.
Nothing a design writes changes: findings, the JSON export and every drawing read the same.

## 0.1.47

The Owner approves the white-box requirements DS-374 left as drafts, with their verifications: parse knows no model and its
operations are class members (REQ-00176-1, REQ-00177-1); text depends on no other engine module and holds no state
(REQ-00178-1, REQ-00179-1). Nothing a design writes changes.

## 0.1.46

Parse and text keep their own rules (DS-374). Describing a syntax error is a class of its own, `SyntaxError`, and what
the grammar takes in a parse state is `Lookahead`, which the error's hint and an editor's completion now share instead
of each walking the grammar in its own copy; the parser only reads. A parsed file is data: the text a node spans is
read by the tree reader. Nothing reaches for the grammar through a static accessor any more: each file's tree carries
the grammar it was parsed with, the reader resolves its fields from it and the binder builds its table from the first
file. The design of ds says so in four rules, that parse knows no model and writes no free function, and that text
depends on no other module of the engine and holds no state. Nothing a design writes changes: findings, syntax errors,
completions and every drawing read the same.

## 0.1.45

The axis is written as requirements held by rules (DS-367): every operation of the axis is a member of the class
whose responsibility it is, and the axis reads the model it cuts and leaves it as it is. standsAmong and bondOf are
static members of Cut, and so are the helpers its source hid. Nothing a design writes, no finding, export or
drawing changes.

## 0.1.44

The architecture of the binder and the model is written in the design as requirements, each held by rules
(DS-365): the binder binds each construct through a binder of its own, reached through one interface; every binder
writes into the model through the bind context; the metamodel builds its own tables when it is created; and every
operation of the binder and of the model is a member of the class whose responsibility it is. A design that breaks
one is told so with E-RULE-BROKEN. The binder now holds its node binders as INodeBinder, and the scan of a type's
text is TypeToken::scan. Nothing a design writes, no finding, export or drawing changes.

VER-00037-7 (under REQ-00049-3) and VER-00003-6 (under REQ-00003-2), which DS-337 and DS-362 left as drafts, are approved.

## 0.1.43

A large `neighbours:` no longer keeps `ds render` busy for good (DS-362). The walk took every step a class diagram
asked for, each over every relation, so `neighbours: 2000000000` over three classes never ended while `ds check`
said nothing; it now ends at the first step that adds nothing, so a number past what the diagram reaches draws the
reach at once. A `neighbours:` that is no number, a word or one too large for an int, gets the new
`W-DIAGRAM-NEIGHBOURS` at the diagram, as such a depth gets `W-DIAGRAM-DEPTH`, and still adds no neighbours. A large
depth already ended where the calls end. Nothing a design writes draws differently.

## 0.1.42

The test of CST-00001-1, a hundred classes checked and drawn in under a hundred milliseconds, times each command
five times and holds the fastest to the budget (DS-363). Load on the machine, other pipelines on a shared runner, only
ever adds to a run's time, so the fastest run is the work's own; the budget itself does not change. Nothing a design
writes changes.

## 0.1.41

The walk the binder had is gone (DS-357, the end of DS-353): every construct of the language binds through its own
binder, found by the table, and the binder only builds them and binds each file. The requirements the walk still
answered to move to the binders that realise them: the high-level design as packages, a package as a box and its
nesting to the package binder, a module that reopens to the module binder. Nothing a design writes, no finding,
export or drawing changes.

## 0.1.40

Sequences get a binder of their own (DS-356, group four of four): a sequence is the behaviour of an operation, with
its steps, the frames that hold them and an alt in two branches, its messages kept for the checker. Every construct
of the language now binds through its own binder; the walk the binder had takes no kind any more, and DS-357 removes
it. Nothing a design writes, no finding, export or drawing changes.

## 0.1.39

Classes and functions get binders of their own (DS-356, group three of four). A class's members, its methods,
attributes and operators, are bound by a member part, and what a member or a function writes in its body by another,
both shared. The walk the binder had keeps only the sequences, until the last group. Nothing a design writes, no
finding, export or drawing changes.

## 0.1.38

Use cases and state machines get binders of their own (DS-356, group two of four). A use case keeps its signature and
its outcomes; a state machine is still the behaviour of its class, its events and actions kept for the checker. The
walk the binder had keeps classes, functions and sequences until the next groups. Nothing a design writes, no
finding, export or drawing changes.

## 0.1.37

The boxes and the plain constructs get binders of their own (DS-356, group one of four): packages, modules, interfaces,
enums, rules and definitions, and diagrams. What every construct shares is a part binder each binder is given by
reference: the fields, the annotations, an extension block, a type, a signature and a formula. The context goes back
to holding only what a binder writes through. The walk the binder had keeps use cases, classes, functions,
sequences and state machines until the next groups. Nothing a design writes, no finding, export or drawing changes.

## 0.1.36

A number too large for an int no longer stops ds (DS-337). A diagram that wrote `depth: 99999999999` passed
`ds check` and then made `ds render` stop with `ds: stoi`; it now gets `W-DIAGRAM-DEPTH`, the finding of a depth
that is no number, at the same place, and is drawn as one. A `neighbours:` too large for an int adds no neighbours,
as a word there does. A diagram reads such a field in one place, and nothing else a design writes changes.

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
