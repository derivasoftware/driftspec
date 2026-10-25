# DriftSpec language guide

*The rules of the language as decided, in the order a reader needs them.
Each rule names the corpus that exercises it; the `!` comments in a corpus
file are the findings the tool emits for it, and the tests hold the tool to
them. The vision (`vision.md`) says why; this says what.*

## 1. Workspace and identity

- A workspace is a directory of `.ds` files. `ds` loads it whole; a file is
  not a unit of meaning. `examples/diag`.
- A `package` is the whole scope, the black box. A `module` is a namespace
  and opens the white box. Both reopen across files (R1, R2, R9).
- Packages do not contribute to qualified names; modules do: `ds::Model`
  wherever its file is (R4). No includes, no using: a bare name resolves from
  the innermost module outward, a qualified name crosses modules (R10).
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

## 2. Static design (`examples/static`)

- Kinds: `class`, `dataclass`, `enum`. Interface and abstract are derived
  from the methods and never written (R14).
- Any entity, a member too, may open a body with fields, `note:`,
  annotations, contracts and nested requirements or constraints; nesting
  means "about this" (R12, R13).
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

## 3. Behaviour (`examples/behaviour`, syntax provisional)

- A behaviour is identified by its subject: `sequence Checker::run { }`,
  `state Checker { }`; scenarios as `sequence Checker::run / duplicate { }`.
- Participants are the static context: `self`, the attributes of the class,
  the parameters of the method, by name; only what the method creates is
  written, `local parser: Parser`. `W-PARTICIPANT-UNDECLARED`.
- Messages `self -> binder.references() -> refs`, `-> resolveReference(ref)`
  for self; arity is checked (`E-MSG-ARITY`); `loop`, `alt … else`, `opt`
  with prose conditions.
- A message to an operation with a behaviour expands to it (derived; a
  diagram inlines it with `depth` or links it). Coupling to another module is
  derived from what a behaviour touches.
- State machines: `A -> B on event [guard] / action`; event and action are
  operations (`W-EVENT-UNDECLARED`, `W-ACTION-UNDECLARED`).

## 4. Requirements, constraints and verifications (`examples/artefacts`, decided 2026-10-25)

- **Identity and links.** The revision is part of the id. Every link names a
  revision: `@refines SREQ-00001-2`, `@requirement REQ-00001-2`,
  `@constraint CST-00002-1`. An unpinned link is `E-REF-UNPINNED`. A link to a
  revision the workspace no longer declares is `W-REVISION-DRIFT`; it resolves
  to the declared one, so the graph stays whole and the drift is visible.
- **Level by placement.** `requirement` in a package is of the system (an
  SREQ for argos); in a module, of the software (a REQ). No keyword, no field.
- **Fields.** `title` (the shall sentence), `text` and `rationale`
  (multiline), `status: draft | approved` (draft when absent), `tags` (free,
  one line). No `asil`: the team classifies with tags.
- **Parent.** A requirement nested in a requirement refines it; elsewhere,
  `@refines`. A `parent:` field is ignored with `W-PARENT-IGNORED`: one
  representation per concept.
- **Verification.** Nested in its requirement, or `@requirement` from
  elsewhere. `method: test | review | inspection | analysis`. `tests:` lists
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
