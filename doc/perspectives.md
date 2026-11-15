# Three perspectives

*Decided with the Owner on 2026-11-07 and 2026-11-09 (DS-94, DS-101, DS-108,
DS-110). Short on purpose: update it when one of these decisions changes.
"Built" means it is in `main`; "direction" means agreed and not built yet.*

## One map, three ways to read it

A design answers three questions, and DriftSpec keeps one model for all of
them:

- **Structure**: what there is. Packages, modules, interfaces, classes, their
  members, what holds and derives from what.
- **Behaviour**: what happens. Sequences, scenarios, state machines, who calls
  whom and through which interface.
- **Logic**: what holds. Rules over the model that are true or false, and
  what breaks them.

These are not three diagrams or three paradigms, one per box. They are three
layers over the same boxes. High-level design is drawn as objects, and inside
a box the code may be written in any paradigm. A perspective changes what is
drawn on top of the map, never where the boxes are.

## The boundary: an outer contract for each perspective

Whoever stands outside the project must be able to ask for something in each
of the three perspectives. Each perspective has its own form of request, a
part of the design that realises it, and evidence that it is met:

| Perspective | Outer contract | Realised inside by | Evidence |
|---|---|---|---|
| Structure | requirement: the system shall be or have X | boxes, classes, members (`@requirement`) | trace, tests |
| Behaviour | use case: when A happens the system does B and ends in one of its outcomes | interface operations and their sequences, one scenario per outcome | tours, tests per outcome |
| Logic | constraint: always, or never | rules | the evaluator, or tests when the design cannot decide |

The split is a guide, not a wall. A requirement may describe behaviour, and a
use case touches structure. A constraint is a requirement with a negative or
universal scope. What matters is that a request takes the form that states it
best: a capability is a requirement; an interaction in time, with who starts
it and how it may end, is a use case; "every", "always" or "never" is a
constraint. argos already sorts requests this way.

Built: requirements link to the design; a use case declares its contract and
outcomes and is realised by interface operations. Direction (DS-110): a rule
answers to a constraint as a class answers to a requirement; a constraint
with no rule and no verification is reported; the viewer shows requirements
under structure, use cases under behaviour and constraints under logic.

## Rules: the logic inside

A rule is a design piece with a name, written in the box it speaks about. It
makes an outer request real in a narrower context, and it may name the
entities of the design, which a request from outside cannot.

```
rule engine-knows-no-python {
  @requirement REQ-00005-1
  for class c in meteo@station@engine
  never depends(c, x), x in meteo@station@server
}

definition depends(a, b) {
  if m in a, m owns b
  if m in a, m uses b
}
```

- **Formal, never prose.** Rules are Datalog over the model: ranges, then
  literals joined by commas (a relation of the metamodel, a kind, containment,
  a comparison, a definition applied, `arg`), each one negated with `not`. A
  bare name is a variable; an entity is written qualified. `holds: <words>`
  does not parse. Nothing is inferred from keywords.
- **Always decided in the design.** The model is finite and negation is
  stratified, so every structural rule comes out true or false. A rule is
  broken by each binding with no answer to `holds`, or with an answer to
  `never`, and each breach is kept with its bindings, its answer and where it
  lands.
- **Contracts are rules.** What a member requires of its calls is a rule in its
  body. `arg(k, p, v)` is the literal a call passes to a parameter. A call that
  passes a name has no `arg`, so the rule's own `where` leaves it out: the
  design cannot decide it and does not pretend to. The bracket contracts
  (`[requires: ...]`) are gone: one syntax for one thing.
- **Never an autofix.** A rule says what is wrong and where. It does not change
  the design.
- Direction: rules the design cannot decide are backed by tests or recorded
  runs (DS-94 phase d); rules read fields, flags and names, and count (DS-107).

## In the viewer

Built: a switch picks the perspective over the same map.

- **Structure** is drawn at right angles, because it is built. What crosses
  two boxes is one dependency between them, as in a component diagram. Lines
  go around the boxes in their way, a shared corridor is split into lanes, and
  a line leaves the box that holds only one end early.
- **Behaviour** is drawn in curves, because it flows: who calls whom and which
  interfaces each box consumes, with the tours to replay.
- **Logic** marks each box with the rules that live in it, rings what breaks a
  rule and draws a broken `never` to its answer. The live session lists the
  rules as written and lights one when the reader picks it.

## Not yet

- Coverage per perspective is a question, not a finding (Owner, 2026-11-15):
  a constraint with no rule, or a use case nothing realises, is answered when
  asked (DS-110, DS-216).
- The functional perspective inside a box: the data that travels between
  steps, and a state lane. Waits for a real case that needs it.
- Relative identities: today an entity in a rule or a reference is written
  with its full path.
