# DriftSpec — vision

*Status: working notes consolidated on 2026-10-24 and revised on 2026-11-13
with the Owner's decisions on the concept (§2a) and the roadmap by rings (§14).
Everything here is a decision or a direction already discussed; open points are
marked as such. This document is the memory of the project: update it when a
decision changes, do not let it drift from the code. The three perspectives,
their outer contracts and the rules are in [`perspectives.md`](perspectives.md).*

## 1. What it is

DriftSpec is a specification language and a single tool (`ds`) that reads a
whole workspace of `.ds` files into one model, checks it, and projects it into
diagrams. It is not a diagram language: the entities are specified once, with an
identity, and every diagram is a selection over the model. If the model changes,
the drawings follow; a drawing cannot drift from the specification, which is
where the name comes from.

It was born from the PlantUML family of tools (grammar, formatter, LSP, render,
editors): having built our own tools around PlantUML's syntax to avoid its core,
the question was what stops us from owning the syntax too, and designing it for
specifications instead of for drawing.

DriftSpec is a product of its own, not a PlantUML replacement for argos. Parity
with PlantUML is the floor it has to reach before argos can rely on it, not the
goal: where DriftSpec can say more than PlantUML, argos is extended to read it,
rather than DriftSpec being cut down to what PlantUML says.

## 2. Thesis, in the terms of the fleet

DriftSpec is a modelling tool based on text: it defines requirements,
verifications, design, behaviour and diagrams, and keeps them coherent with
each other. That is its whole job. It does not look at code, tests, runs or
data, and it does not judge whether the model matches reality: that is
argos's job, with gafas and prueba, on the model DriftSpec exports.

The boundary, stated once:

- **DriftSpec owns the declaration and its internal coherence.** Every
  identity declared once, every reference resolved, every relation typed,
  every behaviour bound to the static element it describes, every diagram a
  projection of the model. Incoherence inside the model is a finding.
- **DriftSpec exports.** The model, with its relations, in a form argos can
  read (`ds export`, and in process through the driftspec package).
- **argos owns reality.** Declared against implemented, declared against
  tested, declared against observed. If argos writes something back as `.ds`
  files, DriftSpec loads them like any other files; it does not know or care
  where they came from.

A domain earns its place in the tool when it has a coherence check that
catches something real inside the model; a domain that only draws is PlantUML
with another syntax.

## 2a. Decisions that set the direction (Owner, 2026-11-12 and 13)

Taken after an analysis of the concept and of where it was drifting. They are
not to be reopened by accident.

1. **A product with its own identity, without forgetting argos.** When a
   choice comes up between bending DriftSpec to what argos or PlantUML read
   today and extending argos, argos is extended (examples: verifications of
   constraints, argos REQ-01082-1; free functions projected as function
   sheets).
2. **The design is still written, member level included.** Design and code
   have different life cycles: a design is expanded ahead of the code, and
   argos's warnings are the list of what is left to implement. There is no
   autofix from design to code. The autofix goes the other way only, code to
   design, and only for what a machine can settle (a member the code grew, a
   signature it changed, a base, a module's functions).
3. **Design anything as code.** To design is to state intention: what and
   how, independent of the technology. No paradigm (object, functional,
   static, logic) and no domain is given up.
4. **The reader is a person and an agent.** The syntax is read like code; a
   merge request's diff shows what changed in the design, and the drawings
   exist for the rest. An agent has the queries, the code and argos. The
   manuals serve both: the grammar written down, skills for agents.
5. **Diagrams are perspectives.** The same idea as folio: different views
   over one model, wherever they help someone understand the project.

Decisions taken since, while using it:

- **Types are text, written as their language writes them** (DS-18, stage B).
  A box that says its language (`language: python`) will write
  `list[Sample]`, `Entity | None` or `const std::string&` as the code does,
  parsed with that language's tree-sitter grammar; that text is what argos
  compares. DriftSpec reads a type only to find the entities it names (the
  relations, their multiplicity, a template's arity), with a short table per
  language of its own names and of the marks that mean "one or none". There
  is no model of type concepts. The neutral types (`string`, `list<T>`) stay
  for boxes without a language and for interfaces between languages. Writing
  the concrete type commits the code to it (`std::array` is not
  `std::vector`); the neutral one commits it to the idea only.
- **Usage is stated in behaviour, never as a bare arrow** (DS-174). What a
  class or a free function uses is what its sequences call and declare
  locally; argos receives that as the subject's dependencies. A call the runs
  make and no sequence explains is design still to write, so there is no
  usage autofix.
- **A name a behaviour does not know stays prose** (DS-64, rule B4 kept). A
  loop's element or a value derived from a parameter is named without being
  declared; loose diagrams will often name things nothing declares, and the
  language stays flexible there.
- **A static operation is called on its class** (DS-60):
  `-> diff::Diff::compare(a, b)`, as `-> tasks::gather(x)` calls a function of
  another module. One that is not static needs an instance
  (`W-MSG-NOT-STATIC`).
- **A constraint may be verified, never has to be** (DS-36). Its
  verification reaches argos as one; a constraint verified otherwise (its BoK
  guide, a static diagnostic, a review) stays as it is.

## 2b. Decisions on the audit of the model (Owner, 2026-11-15)

Taken as comments on the audit of the model (DS-206); the work they lead to is
in Plane, epics DS-206 and DS-205.

1. **A partial specification is a complete one.** Every abstraction level
   stands on its own: drawn and checked with what it has, whether or not it has
   children. Nothing forces anyone to specify everything; a metaspecification
   represents what is pragmatic to represent. Only the high-level design, only
   the low-level one, or the sequences of two methods out of a thousand are
   valid designs.
2. **The mechanism punishes inconsistency, never missing information.** A
   finding needs two written facts that clash; one fact and a gap is coverage,
   asked for apart. The structure and the behaviour are written as far as it
   pays, and the rules sit on top giving consistency to what is there. What is
   not yet written is coverage, kept apart from the findings and answered by
   `ds query <workspace> coverage` (DS-216).
3. **Nesting and annotating are the same relation.** Two syntaxes that only
   change where an artefact is filed (DS-217).
4. **Relations are never merged.** Using, composing, aggregating and
   inheriting mean different things and do not weigh the same. What is missing
   is one rule to lift them from members to classes, modules and boxes, read by
   every view (DS-230). Decided on 2026-11-16: lifted onto a closed box, a
   relation is one of two bonds, structural or use, and nothing more; between
   classes each keeps its own kind.
5. **The abstraction axis is elastic.** No fixed number of levels: one part of
   a system may need one step down and another twenty, and a rigid frame grows
   sublevels where it does not fit. A flat layer is a cut through containment,
   named by its boxes or by the roles they carry, never by a number. Behaviour
   is written at any cut, and two cuts that are both written are checked for
   coherence (epic DS-205). The words, chosen on 2026-11-16 (DS-228): a cut,
   boxes open and close, calls unfold and fold, what is below a cut is lifted
   onto it, and the two ends are the high level and the detail, with no number.
   A role a box carries (system, service, library) waits for a real use
   (DS-247). A cut is written as
   what closes and what opens, `close:`, `open:` and `cut:` in a diagram, never
   as its frontier, so whatever is written is a cut (DS-229, decided on a
   sketch on 2026-11-16; guide §5). Boxes, sequences and requirements stand on
   the same cut, and the calls a sequence follows are written the same way,
   `fold:` and `unfold:` (DS-231). A tour is one diagram that holds both, the
   boxes that take part in a behaviour and its sequence, on one cut (DS-245). A scenario is written in one block at any
   cut, with a use case or a box as its subject, boxes as participants and the
   sender before the arrow; a message to a box is bound to the operation the
   box offers, or is a label until that level is written (DS-232). The scenario
   of a use case is held to the behaviour below it: the same crossings in the
   same order, omissions allowed, and what is not written yet is coverage
   (DS-233).

## 3. Principles

- **Workspace, not file.** `ds` loads a directory. A reference `ns::Name`
  collapses to one identity wherever it is declared. No includes, no using.
- **Entity and representation are separate.** Entities live in packages and
  modules; a `diagram` selects and decides what to print. Layout hints are
  the only thing a diagram owns.
- **One engine, C++.** Grammar (tree-sitter), engine, checks and renders in
  one repository; the command line, the Python face (the driftspec package) and through it the
  language server, the live session and argos's plugin all ask the same
  engine. Everything runs in milliseconds, which is what makes an editor
  preview and an agent loop viable.
- **Incoherence is a specification error.** A reference nobody declares is a
  finding, a duplicate identity is an error, a message to an operation that
  does not exist is a finding. Loading never fails on dangling references,
  only on syntax: the workspace stays usable while it is wrong.
- **Derived things are never written.** Interface, abstract, value, the
  operator families: the tool classifies, so a declaration cannot contradict
  what the members say.
- **Static and dynamic are one model.** Sequences and state machines name the
  classes and operations of the static model by identity and are checked
  against them.
- **Requirements and constraints are in scope.** They are first-class
  artefacts with ids, nested where they apply or related by annotation, and
  they travel into diagrams and into argos.
- **Durable names in English** (identifiers, codes, docs); the conversation
  can be in Spanish.

## 4. The language as decided so far (R1–R19)

Rules R1–R9 are in `tests/corpus/diag` and R10–R19 in `tests/corpus/static/static.ds`,
whose `!` comments are the oracle of the test suite. In short:

- **R1–R2** A `package` is the whole scope, the black box: requirements,
  boundaries and system verifications live there and see modules as black
  boxes. A `module` is a namespace and opens the white box.
- **R3** Nesting gives the default relation ("about", "verifies"); an
  `@annotation` adds a relation and never replaces it; a contradiction is an
  error.
- **R4** Full ids name the package with `@` and modules with `::`
  (`ds@ds::Model`, interface `ds@cli`); what encloses you is omitted; process
  artefact ids stay global. (Decided 2026-10-25; earlier the package gave no
  name.)
- **R5** Identity: process artefacts by id with revision (`DS-0001-0`), design
  artefacts by qualified name. Two declarations of one identity = error
  (`E-DUPLICATE-IDENTITY`); packages and modules are reopenable (R9).
- **R6** An undeclared reference creates an EXPECTED node and one
  `W-REF-UNDECLARED`; checks that depend on it wait for the declaration.
- **R7** Multiline text by indentation. Field values run to the end of the
  line; closing braces go on their own line.
- **R8** Contracts are rules in the member's body (DS-94, 2026-11-07); the
  bracket contracts of 2026-10-25 are gone.
- **R10** No includes, no using; bare names resolve innermost scope outward,
  qualified names cross modules; two extern entities of one simple name make a
  bare reference `E-REF-AMBIGUOUS`.
- **R11** `extern module` declares identities outside the workspace (std, Qt)
  with no obligations.
- **R12** Any entity, including a member, may open a body with fields, notes,
  annotations, contracts and nested requirements or constraints.
- **R13** Comments are not model; `note:` is.
- **R14** Kinds: `class`, `dataclass`, `enum`. Interface and abstract are
  derived from the methods.
- **R15** Templates: `class Repository<T: Entity>`; identity is name plus
  arity; a use `Repository<Finding>` is checked against the bound
  (`E-TEMPLATE-BOUND`). Built-in generics `list`, `set`, `map`, `optional`.
- **R16** Ownership from the attribute: `owns` (default, composition), `refs`
  and `shares` (aggregation). Multiplicity from the type (`1`, `0..1`, `*`).
  Parameters and returns give `uses`. `E-OWNERSHIP-CYCLE`,
  `W-MODIFIER-ON-VALUE`.
- **R17** Operators in the class header as a set, `class Vector <<==, +, []>>`;
  comparison symbols come as the family `ordered`; the body may refine an
  operand (`operator *(k: float) -> Vector`); `E-OPERATOR-UNDECLARED`.
- **R18** A characteristic of an entity is a claim, and a claim with
  traceability is a requirement or a constraint: no third vocabulary
  (no `feature`, no flags, no `alias`). `constraint ID: title` one-line form.
- **R19** Modifier order: visibility, then `abstract|static|const` or
  `owns|refs|shares`, then the name. A method named like its class is a
  constructor. Attributes may carry a default.

Decisions taken and why (so they are not reopened by accident):

- `owns` by default because a member by value in C++ is a composition.
- Operators in the header, not as signatures, because what matters is that the
  algebra exists; families are derived from symbols like everything else.
- No `feature`: a characteristic is a requirement or a constraint.
- No `alias`: it would hide multiplicity behind a name and the corpus did not
  need it.
- Boundaries are `pkg@name`; artefact ids global (decided 2026-10-25).

## 5. Architecture

The architecture is designed in DriftSpec itself (`design/hld.ds` and one
file per namespace under `design/ds/` and `design/python/`); `ds render design
HLD_Ds` draws it, and that design, not this section, is the reference. In
short:

```
.ds files ─ tree-sitter grammar ─► parse ─► bind ─► model ─► check (steps)
                                                      │
              query · axis · scene · render (svg, text, cells, ir, layout) · exporter
                                                      │
                                   api (Api, Workspace) ─► the driftspec package (CPython extension)
                                                      │
                                     ds (command line) · lsp · live viewer · argos plugin
```

- **model** is the only truth: entities, relations and findings. Relations are
  read only through its `Index` (one hop, by identity, copies, in the order
  stated); a behaviour is read through its `SequenceTree`; how an identity is
  written lives in `Identity`, once.
- **bind** declares; **check** runs the language's rules as ordered steps.
- **api** offers the engine in process: one-question functions (`interface
  engine`) and a `Workspace` loaded once and asked as often as needed
  (`interface workspace`), each answer the JSON the engine writes.
- **driftspec** (the Python package, `import driftspec`; `pyds` until DS-288) is the Python face: the
  native module, typed answers, the language server and the live session.
  argos-design-driftspec reads designs through it, in process.
- **Core must be domain-neutral.** The metamodel is already data, but adding
  a kind of entity still touches the binder, the checker and the drawing. The
  open core (a kind brings how it binds, checks and draws; languages and
  perspectives as packs) is stage C of the roadmap; until then no new kind of
  entity is added.

## 6. Domains

**Development** (the one that pays the trip): packages, modules, classes,
dataclasses, enums, templates, operators, boundaries with operations,
requirements, constraints, verifications, sequences, state machines,
diagrams. Checks: identity, references, templates, ownership, operators,
messages and events against the static model.

### Behaviour is addressable, and linked through the static model (decided 2026-10-25)

A sequence, a state machine or an activity is an entity with an identity and
a **subject** in the static model: the method, class or operation whose
behaviour it describes. `sequence Model::declare { ... }` is the behaviour of
that method; `state LspClient { ... }` the behaviour of that class. Behaviours
never reference other diagrams: they reference the static model, and the tool
derives the links.

- A message `client -> model.declare(e)` resolves to the method
  `ds::Model::declare` (relation `calls`). If a behaviour has that method as
  subject, the message also gets `expands-to` → that sequence. In a drawing
  the message is a link to the child diagram (SVG `href`, terminal `gd`); in a
  diagram it can be expanded inline (`depth: n`) or drawn as a `ref` frame.
  UML's `ref` is therefore derived, never written.
- The reverse links are the same relations read backwards: from a method in a
  class diagram to its behaviours, from a class to its state machine, from a
  sequence to the classes it touches (the static chapter of that behaviour).
- An event of a state machine (`on open`) is an operation, so it links to the
  sequence of that operation; an activity's actions may bind to operations
  (`action parse: Parser::parseFile`) and link the same way.
- One subject may have several scenarios (syntax open: `sequence
  Model::declare / duplicate`); the default one is the unnamed.
- Checks this enables: a message to an operation its target does not declare
  (`W-MSG-UNDECLARED`), arity and argument types, a behaviour whose subject
  nobody declares (expected node), recursion made visible.
- **The abstraction level is given by the subject, never declared.** A
  behaviour whose subject is a method is at method level; of a class, class
  level. A module or a package as subject is the direction (DS-232), not built:
  the engine rejects it today. A lower-level behaviour
  projects upward by lifting: a call between methods of two modules is, at
  module level, a message between the modules. One behaviour feeds every
  level above it; nothing is drawn twice.
- **Absence is not a finding.** A method without a declared behaviour means
  nobody wrote it, not that the design is wrong. Only contradictions are
  findings.
- **What DriftSpec does not do with behaviour**: compare it with code, tests
  or runs. Declared sequences are a design artefact; whether the code calls
  what the design says is argos's diff, outside this tool. The lifting by
  level above is what lets argos compare at the grain it has evidence for
  (system behaviour from boundaries, module behaviour from units), but the
  comparison is not here.
- **Participants are the static context.** In the behaviour of a method the
  participants are `self`, the attributes of the class and the parameters of
  the method, by name, without writing them; only what the method creates
  itself is declared (`local parser: Parser`). At class level the attributes,
  at module level the classes, at package level the modules and boundaries.
  A message to a name outside that context is `W-PARTICIPANT-UNDECLARED`.
- **Coupling is derived from behaviour (decided 2026-10-25).** Inside a
  module everything a behaviour touches is cohesion; a `local` of a type from
  another module, or a message through an attribute typed in another module,
  is coupling to that namespace. Because participants come from the static
  context, every behaviour yields its external dependencies without anyone
  listing them, and lifting them gives the module-to-module and
  package-to-package dependency graph: the component view is derived, like
  everything else. Two grains are kept apart: structural coupling (an
  attribute typed elsewhere) and transient coupling (a local created and
  dropped inside a method), because the second is the one that usually hides
  a missing injection. What a box may depend on is declared towards an
  interface, `@uses` (DS-96), and checked against what its behaviours call
  (`W-USES-UNDECLARED`, `W-USES-UNUSED`, `W-USES-BYPASS`); without the
  declaration coupling is a derived fact, reported, never a finding.
- **Observed sequences stay in argos (decided 2026-11-05, DS-16).** What the tests
  of a requirement did, with modules as participants, is reality, and reality is
  argos's. The design's trace from the top is the sequence of an operation of an
  interface, going down through the scenario each call names.
- **Proof of concept (2026-10-25, commit after 1ed159d)**: block 3 is built
  with the recommended syntax as *provisional*: `sequence Class::method { }`
  and `/ scenario`, participants from the static context plus `local`,
  `a -> b.op(args) -> result` and `-> op()` for self, `loop`/`alt … else`/`opt`
  with prose conditions, `on event [guard] / action` in states. Derived:
  `calls`, `expands-to`, `couples-structural`/`couples-transient`; checks
  `W-PARTICIPANT-UNDECLARED`, `E-MSG-ARITY`, `W-ACTION-UNDECLARED`.
  Diagrams `kind: sequence` with `depth` (inline or link) and `level: module`
  (lifting). `ds query <workspace> coupling` reports what every behaviour touches outside its
  module. Sequences are drawn by our own engine in SVG and in the terminal.
  The SVG carries its usability inside (no host code needed): tooltips on
  every element, links by identity (a lifeline to its class, a folded child
  to its behaviour), focus on hover, expansions that fold on click with the
  rows below moving up, and `data-ref-location` on every message for an
  editor to jump to the `.ds` line. The caller invokes the entry operation
  first, leftmost, so a drawing says what it shows. Corpus `tests/corpus/behaviour`. Activities are not
  in the POC. A `//` now always starts a comment, also after a field value.
- This is the input for syntax block 3; the behaviour-as-entity decision
  itself is taken, the syntax is provisional until chosen.

**Process** (useful now, with a consumer): PaC already exists in argos (PROC,
ACT, ROLE, protocols) and is the source of truth; titere edits it. DriftSpec
is its specification face: static checks outside argos (a role nobody
declares, an output nobody consumes, a gate without a criterion) and activity
diagrams as projections (the render-IR already has actions, decisions and
swimlanes). Short term: read PaC and draw/check it. Long term: write process
in `.ds` and let argos import it, as it will import design. Never a second
place where process is written.

**Analysis** (useful when traza has a study): causal DAGs as in dagitty 3.x.
Variables, the DAG as declared assumptions, claims with their method and
data. The algorithms (identifiability, adjustment sets, d-separation,
testable implications) are checks, not entities. Sketch:

```
study ouffroad-fatigue {
  variable Speed     { source: gps.speed }
  variable HeartRate { source: strap.bpm }
  variable Slope     { source: dem.slope }
  variable Fatigue   { latent }
  Slope -> Speed     Slope -> HeartRate
  Fatigue -> Speed   Fatigue -> HeartRate
  Speed -> HeartRate
  claim CL-0001 {
    text: Speed raises HeartRate on climbs
    effect: Speed -> HeartRate
    method: regression adjusting for Slope
    data: rides-2026-09.parquet
  }
}
```

The tool checks the method against the DAG (`E-EFFECT-NOT-IDENTIFIABLE`
when a needed adjustment is latent), lists the conditional independences the
DAG implies as the verifications of the study (running them against data is
traza's job, not this tool's), and projects the DAG with causal and biasing
paths and the umbrella of each claim. Export
to dagitty's text syntax is trivial and keeps interoperability. The pipeline
of steps is the process domain, not the DAG.

Priority: development, process, analysis.

## 7. Rendering and interaction

- **SVG for every DOM host** (browser, VS Code webview, folio docs, GitLab):
  stable ids (`data-id` = qualified name), `data-from`/`data-to` on edges, a
  12 px invisible hit path per relation, CSS tokens with light and dark
  palettes inside the file, focus classes (`pr-focusing`, `pr-focus`). Links,
  tooltips and `refs` exist in the IR; `ds render` does not fill them yet.
- **Layout JSON as the portable thing** (`ds render <dir> <diagram> -f
  layout`): placed rectangles and routes, so native hosts (Qt, a game engine,
  anything) draw their own interactive scene from the same positions.
- **Terminal** (`--text`, built): same layout on a character grid; Neovim can
  read it into a buffer today; a cell→id map will give `gd` from a box to its
  declaration.
- **VS Code**: webview with the SVG, click → extension reveals the
  declaration, cursor in the `.ds` → focus in the webview, save → redraw;
  plantuml-vscode is the precedent. With the LSP behind, the extension knows
  nothing about the language.
- **Live session** (`python -m driftspec.live serve <dir>`): the viewer follows
  the files as they are written, and the reader and an agent leave notes and
  comments on identities through it (its MCP tools).

### Semantic diff (built 2026-10-25, removed 2026-11-18)

*Taken out before the first release (DS-277): it was not used and had to be
maintained. Kept as the scope DS-276; the last version with it is 0.0.184.*

A design change is a difference between two models, never between two
texts: a moved file or a reordered member is no change. `ds diff A B [path]`
compares two directories or two git revisions of a path (git stays the only
history) and reports, by module, the entities and relations added, removed
and changed by identity, with what changed (`params: … -> …`,
`flags: owns -> shares`). A removed class and an added one of the same shape
in the same scope is a **rename** when at least 60 % of their members match;
the tie between look-alikes is broken by who now points at the candidate
(the attribute that used to be typed `Text` is now typed `Terminal`). Members
of a renamed class travel with it. Containment is implied and not reported.

Process: an artefact (requirement, constraint, verification) whose text
changed under the same id and revision raises `W-REVISION-STALE`: the silent
drift a merge request never shows.

The **diff diagram** is a projection of the after model marked with the diff
(`change` = added | removed | changed | renamed on entities, `kind+added` /
`kind+removed` on relations; removed entities come over as ghosts):
`ds diff A B path CL_X -f svg` / `-f text`. SVG: green added, red dashed
removed, amber changed, blue renamed, with the detail as tooltip; terminal:
`+ - ~ >` badges and colours. Meant for a merge request's description next to
argos's code quality report.

## 8. Performance (measured 2026-10-24, Release build, wall time per process)

| document | nodes / edges | ds (C++) | plantuml-render (Node + ELK) |
|---|---|---|---|
| CL_Package_ds | 21 / 24 | 2 ms | 389 ms |
| synthetic 100 | 104 / 140 | 8 ms | 975 ms |
| synthetic 400 | 410 / 568 | 24 ms | 33 s |
| synthetic 1600 | 1632 / 2270 | 114 ms | not run |

Whole pipeline `.ds` → SVG of the package: 5 ms. Small documents: Node
startup plus loading ELK is ~250 ms before drawing. Large: ELK is
superlinear and does more (exhaustive crossing minimisation, hierarchical
ports); ours is ~0.07 ms per node. Known weakness of our layout: an edge that
crosses a container border is routed straight between its ends.

## 9. What exists (2026-11-13)

The design of driftspec, written in DriftSpec, is the inventory: `ds query
design entities`, `ds render design HLD_Ds`. What it covers, in short:

- The language of §4 and §6 with its checks, the semantic diff (§7), the
  queries, the scene of the whole model for a viewer that zooms, and the
  drawings in SVG, terminal text and cells.
- The engine in process (`api`) and its Python face (`driftspec` on the
  deriva registry, import `driftspec`), with a language server and a live session:
  the viewer follows the files as they are written, with notes and comments
  for an agent.
- argos-design-driftspec reads a DriftSpec workspace into argos through the driftspec package:
  classes, free functions as function sheets, relations, usage from
  behaviour, the HLD, every sequence as a SEQ_, requirements, verifications
  and constraints, and the engine's findings. Its autofix writes code-ahead
  drift, a module's missing functions and a class's bases back into the
  `.ds` files.
- driftspec's own design is checked by argos on every merge request, with the
  merge request's own engine, and the C++ evidence of every test reaches the
  gate.

## 10. What is missing

The backlog is the roadmap of §14. Gaps found by use go there as cards, under
the stage where they appeared.

## 11. How we will know it works

1. **Expressiveness**: specify driftspec itself in `.ds` (dogfooding), then a
   foreign design (hyperstereo, iris modules from PlantUML). Every thing that
   cannot be said is a language gap.
2. **Checker precision**: mutation over the oracle corpus (remove each `!`
   line, exactly one finding disappears; inject a defect, exactly one
   appears); random edits never crash, at most `E-SYNTAX`.
3. **Fit for argos**: `ds export` of driftspec's own spec carries what argos
   needs to do its own comparison with the headers (classes, members,
   relations, requirements, verifications); argos does the comparison, this
   proves the export is complete.
4. **Usability**: someone else writes a small spec from the guide alone.

## 12. Open decisions to take together (syntax of the artefacts)

Work by blocks, each with a sketch file showing the real alternatives side by
side on real artefacts; the chosen form goes into the guide and the oracle
corpus the same day.

1. **Process artefacts**: DECIDED 2026-10-25, see `guide.md` §4: revision in
   the id and every link pinned to one (`E-REF-UNPINNED`, `W-REVISION-DRIFT`),
   level by placement, `status` in the language, no `asil` (tags), no
   `parent:` (nesting or `@refines`), verification nested or `@requirement`,
   tests as the runner names them, constraint `category`/`bok`, one-line
   forms; `applies:` on constraints deferred. Corpus `tests/corpus/artefacts`.
2. **Static leftovers**: DECIDED 2026-10-25: contracts as text in the body
   only; ids `pkg@mod::Class`, boundaries `pkg@name`, artefacts global;
   braces enclose and one thing per line; `extern` names only.
3. **Dynamic**: DECIDED 2026-10-25 as the proof of concept had it (subject
   as the name, `/ scenario` anywhere with the identity of the subject,
   inline return, `loop`/`alt`/`opt` in prose, `on event [guard] / action`);
   no nested states, activities with the process domain.
4. **Black box**: closed or free boundary `kind`; typed operation parameters;
   a class realising one operation rather than a whole boundary; system
   requirements that only see boundaries.
5. **Diagrams**: kinds (class, sequence, state, requirement, interface; the
   component view is a class diagram with `open:`);
   closed `include/exclude/show/hide`; minimal layout hints (direction,
   grouping, positions saved by an editor); diagrams inside modules or only
   at top level.

Recommended order: 1, 2, then 3 and 4 together, then 5.

## 13. Relationship to the fleet

- **argos**: reads DriftSpec through argos-design-driftspec, which loads the
  workspace through the driftspec package; the obligations DriftSpec derives become argos
  checks against code, tests and runs. argos is extended where DriftSpec says
  more than PlantUML (§2a).
- **plano**: the type package; DriftSpec's domains mirror its namespaces
  (core, dev, org, analysis).
- **plantuml-render**: the render-IR contract the drawings started from.
- **folio**: embeds the drawings in generated documents, and shares the idea
  of perspectives over one model.
- **PaC / titere**: the process domain's source of truth and editor.
- **traza / gafas / prueba**: the right side of the V; DriftSpec never calls
  them and never reads their evidence.

## 14. Roadmap by rings (applied to Plane on 2026-11-12)

The roadmap is planned by rings of use, not by features: a real repository is
designed in DriftSpec with PlantUML still the gate, and what that use finds is
the backlog. DriftSpec is not ready to replace PlantUML yet. One ring is
active at a time, and a language or viewer card is born from real use, under
the stage where it appeared.

- **A · Solid base and parity (DS-153).** Ring 0: driftspec and its argos
  plugin. The model offers its index, the behaviour is read as a tree, the
  engine's questions are asked from Python, the plugin reads through it and
  offers the reconcilers argos asks for. Python types wait for DS-18, so the
  Python parts of driftspec and the plugin are not compared with their code
  in this stage.
- **B · First repository outside (DS-166).** Ring 1: a Python repository
  (gafas) designed in DriftSpec beside PlantUML. `ds` writes DriftSpec
  (model to text, edits through the syntax tree) before any converter; types
  in their language's syntax (DS-18).
- **C · Open core (DS-167).** A kind brings how it binds, checks and draws;
  languages and perspectives become packs. After ring 1, with two languages'
  needs known.
- **D · A multi-language application (DS-168).** Ring 2 (ouffroad), and the
  perspectives (viewpoints, deployment, rules).
- **E · argos designed in DriftSpec (DS-169).** PlantUML retired, the grammar
  frozen, argos extended by DriftSpec.

Kept apart: viewer polish and layout (icebox, DS-170) and argos style debt in
driftspec (DS-111), done when the area is touched.
