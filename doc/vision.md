# DriftSpec — vision

*Status: working notes consolidated on 2026-10-24, before the language is frozen.
Everything here is a decision or a direction already discussed; open points are
marked as such. This document is the memory of the project: update it when a
decision changes, do not let it drift from the code.*

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

## 2. Thesis, in the terms of the fleet

argos closes the V: declaration against reality. DriftSpec is the declaration
side done properly for design, with the same three layers everywhere:

1. **What we declare**: entities, relations, artefacts with identity.
2. **What follows from it**: derived facts and obligations the tool computes
   (an interface is derived, a composition cycle is an error, a template bound
   is checked, a testable implication is listed).
3. **What reality says**: evidence gathered by other tools (argos reading the
   code, gafas observing tests, traza running checks on data) against the
   obligations.

The tool owns 1 and 2. It emits 2 so that 3 can be done by whoever holds the
evidence. A domain earns its place in the tool when it has a checker that
catches something real; a domain that only draws is PlantUML with another
syntax.

## 3. Principles

- **Workspace, not file.** `ds` loads a directory. A reference `ns::Name`
  collapses to one identity wherever it is declared. No includes, no using.
- **Entity and representation are separate.** Entities live in packages and
  modules; a `diagram` selects and decides what to print. Layout hints are
  the only thing a diagram owns.
- **One tool, C++.** Grammar (tree-sitter), engine, checks, renders, and later
  LSP and MCP in one repository and one binary. Python bindings later, so
  argos can read a whole specification at once through a plugin. Everything
  runs in milliseconds, which is what makes an editor preview and an agent
  loop viable.
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

Rules R1–R9 are in `examples/diag` and R10–R19 in `examples/static/static.ds`,
whose `!` comments are the oracle of the test suite. In short:

- **R1–R2** A `package` is the whole scope, the black box: requirements,
  boundaries and system verifications live there and see modules as black
  boxes. A `module` is a namespace and opens the white box.
- **R3** Nesting gives the default relation ("about", "verifies"); an
  `@annotation` adds a relation and never replaces it; a contradiction is an
  error.
- **R4** Packages do not contribute to qualified names; modules do.
- **R5** Identity: process artefacts by id with revision (`DS-0001-0`), design
  artefacts by qualified name. Two declarations of one identity = error
  (`E-DUPLICATE-IDENTITY`); packages and modules are reopenable (R9).
- **R6** An undeclared reference creates an EXPECTED node and one
  `W-REF-UNDECLARED`; checks that depend on it wait for the declaration.
- **R7** Multiline text by indentation. Field values run to the end of the
  line; closing braces go on their own line.
- **R8** Contracts (`[requires: …]`, `[ensures: …]`, flags) are stored, not
  yet interpreted. **Open.**
- **R10** No includes, no using; bare names resolve innermost scope outward,
  qualified names cross modules; two candidates at the same depth should be
  `E-REF-AMBIGUOUS` (not implemented: the scope scheme makes it unreachable
  today).
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
- Boundaries and artefact ids currently have no package prefix. **Open**:
  proposal `ds/cli` from outside, `cli` inside.

## 5. Architecture

```
.ds files ─ tree-sitter grammar ─► Parser ─► Binder ─► Model ─► Checker
                                                         │
                               JsonExport ◄──────────────┤ (for argos, MCP)
                               IrExport (diagram → render-IR)
                                   │
                        ┌──────────┴──────────┐
                     Svg (DOM hosts)       Text (terminal)       plantuml-render (oracle)
```

- **Model** is the only truth: entities (kind, id, name, scope, fields, flags,
  location, expected), relations (kind, from, to, multiplicity), findings.
- **Binder** declares; **Checker** resolves references and runs the rules.
- **IrExport** projects a `diagram` onto plantuml-render's render-IR
  (contract v1, `schema/render-ir.schema.json` in that repo). The IR is the
  family's drawing contract; DriftSpec is its third producer. We do not
  invent a second one.
- **Two renders of our own, one layout.** `Layout` is a layered (Sugiyama)
  layout per container with fixed metrics: pixels for SVG (plantuml-render's
  table, so both drawings are themed and compared alike) and character cells
  for the terminal. The Text emitter is proof that the layout is independent
  of SVG.
- **Core must be domain-neutral.** Today `Kind` is an enum with the
  development vocabulary. Before consolidation it becomes an open registry so
  that process and analysis enter as domain packs, not as patches. This is a
  blocking refactor for consolidation.

## 6. Domains

**Development** (the one that pays the trip): packages, modules, classes,
dataclasses, enums, templates, operators, boundaries with operations,
requirements, constraints, verifications, sequences, state machines,
diagrams. Checks: identity, references, templates, ownership, operators,
messages and events against the static model.

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
DAG implies as verifications that traza runs against the data, and projects
the DAG with causal and biasing paths and the umbrella of each claim. Export
to dagitty's text syntax is trivial and keeps interoperability. The pipeline
of steps is the process domain, not the DAG.

Priority: development, process, analysis.

## 7. Rendering and interaction

- **SVG for every DOM host** (browser, VS Code webview, folio docs, GitLab):
  stable ids (`data-id` = qualified name), `data-from`/`data-to` on edges, a
  12 px invisible hit path per relation, CSS tokens with light and dark
  palettes inside the file, focus classes (`pr-focusing`, `pr-focus`). Links,
  tooltips and `refs` exist in the IR; `ds render` does not fill them yet.
- **Layout JSON as the portable thing** (`ds layout`, to do): placed
  rectangles and routes, so native hosts (Qt, a game engine, anything) draw
  their own interactive scene from the same positions.
- **Terminal** (`--text`, built): same layout on a character grid; Neovim can
  read it into a buffer today; a cell→id map will give `gd` from a box to its
  declaration.
- **VS Code**: webview with the SVG, click → extension reveals the
  declaration, cursor in the `.ds` → focus in the webview, save → redraw;
  plantuml-vscode is the precedent. With the LSP behind, the extension knows
  nothing about the language.
- **`ds serve`** (to do): HTTP preview with reload and focus script; the
  embryo of the LSP preview.

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

## 9. What exists (commits 12bf729 … 4bba1ec)

- Grammar with external scanner for indented text; Neovim plugin
  (`editors/nvim`, lazy spec in the user's config).
- Parser, Binder, Checker, JsonExport; `ds check`, `ds export`.
- IrExport with `include`/`exclude` (ids, modules, `package:x`), `hide`,
  `show`; `ds render <dir> [diagram] [--svg|--text]`, `ds draw <ir.json>`.
- Own SVG engine and terminal engine over one layered layout.
- Corpora: `examples/diag` (R1–R9), `examples/static` (R10–R19). 34 tests.
- `~/code/DriftSpec/` holds the user's original sketches (`CL_Diag.ds`,
  `CL_Diag.v2.ds`, `static.v3.ds`, `split/`).

## 10. What is missing

- Language: contracts, boundary/artefact prefixes, a single contract form,
  the language guide, `E-REF-AMBIGUOUS`, message arity and types,
  `W-MSG-NO-PATH` (a message with no static path between participants).
- Render: requirements and constraints on the drawing, component view
  (package, boundaries, modules), sequence and state drawings, the umbrella
  of a requirement (what realises it, what verifies it), cross-container
  routing, `ds layout`.
- Core: open `Kind` registry; domain packs.
- Governance: GitLab project (group to decide: `deriva/apps` vs
  `deriva/argos`), argos SREQ/REQ/VER/CL_, CI with ci-templates, version
  manifest, pre-commit; the repo is local only, no remote, nothing created
  in GitLab or Plane yet (needs explicit confirmation).
- Surfaces: LSP, MCP, `ds serve`, VS Code, Neovim navigation, Python
  bindings (deferred), wasm.

## 11. How we will know it works

1. **Expressiveness**: specify driftspec itself in `.ds` (dogfooding), then a
   foreign design (hyperstereo, iris modules from PlantUML). Every thing that
   cannot be said is a language gap.
2. **Checker precision**: mutation over the oracle corpus (remove each `!`
   line, exactly one finding disappears; inject a defect, exactly one
   appears); random edits never crash, at most `E-SYNTAX`.
3. **Reality**: `ds export` of driftspec's own spec against what
   argos-implementation-cpp14 extracts from the headers; the inventory must
   match except where declared.
4. **Usability**: someone else writes a small spec from the guide alone.

## 12. Open decisions to take together (syntax of the artefacts)

Work by blocks, each with a sketch file showing the real alternatives side by
side on real artefacts; the chosen form goes into the guide and the oracle
corpus the same day.

1. **Process artefacts**: mandatory fields; revision in the id or as a field;
   `title`/`text`; `parent` vs `@requirement`; verification levels and how the
   real test is referenced; whether status (draft/approved) is in the language
   or left to argos.
2. **Static leftovers**: contracts (free text vs a minimal checkable
   expression), one place for contracts, package prefix for boundaries/ids,
   closing-brace rule, `extern` with or without signatures.
3. **Dynamic**: messages with arguments and return, `alt`/`loop`/`opt` or not,
   guards and actions on transitions, nested states, meaning of
   `@verification` on a sequence.
4. **Black box**: closed or free boundary `kind`; typed operation parameters;
   a class realising one operation rather than a whole boundary; system
   requirements that only see boundaries.
5. **Diagrams**: kinds (class, component, sequence, state, requirement);
   closed `include/exclude/show/hide`; minimal layout hints (direction,
   grouping, positions saved by an editor); diagrams inside modules or only
   at top level.

Recommended order: 1, 2, then 3 and 4 together, then 5.

## 13. Relationship to the fleet

- **argos**: future plugin reads `ds export` (or the Python bindings) as a
  design source next to PlantUML; the obligations DriftSpec derives become
  argos checks against code and tests.
- **plano**: the type package; DriftSpec's domains mirror its namespaces
  (core, dev, org, analysis).
- **plantuml-render**: owner of the render-IR contract and the oracle for our
  engine; plantuml-vscode the precedent for the VS Code integration.
- **folio**: embeds the SVGs in generated documents.
- **PaC / titere**: the process domain's source of truth and editor.
- **traza / gafas**: the right side of the V for analysis and for runtime.
