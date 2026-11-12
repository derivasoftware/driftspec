# How we design

*Practices agreed with the Owner while cleaning up DriftSpec (2026-11-09, DS-111,
DS-125, DS-138). They are ours: the way this project is built, not checks DriftSpec
ships for anybody. What is C++ design in general lives in the C++ guide of argos,
`GUIDE-cpp-design` (argos-implementation-cpp14): interfaces as a module's contract,
the naming of interfaces (`I<Name>`) and abstract classes (`A<Name>`), abstract
classes as a justified exception, a step that changes state or reads it, generic
pieces only where they save our own work, ownership at the boundary. This page
keeps what is particular to DriftSpec. Each practice says whether a rule in the
project's design already checks it, or what the rule language still lacks.*

## A fact lives in one place

What the language is (kinds, relations, containers, which fields are derived)
lives in the metamodel; how an identity is written lives in `model/Identity`.
Everything that needs the fact reads it from there. A list of kinds or relations
written by hand in a consumer is a copy, and copies drift: most defects of the
first cleanup wave were two copies of one fact that no longer agreed. A test
holds the metamodel to the grammar, so the two cannot drift apart again.

## Requirements, constraints, rules

What is asked from outside is a requirement (what the system is or has), a use
case (what happens) or a constraint (what always or never holds), as
`perspectives.md` says. How we build the system is not a requirement: a
principle of construction is a constraint of the project, and a rule in the
design makes it real. *Rules answering to constraints wait for DS-110.*

Bug fixes, optimizations and refactors add no requirement: they go to the
requirement that already owns the behaviour, with the regression test it should
have had.

## A refactor proves it changed nothing

A refactor keeps the output identical, and we show it: export, scene, check and
every diagram of every example, of DriftSpec's design and of the sandbox, compared
byte for byte with `main`. A difference is a defect of the refactor until it is
explained. `tools/same.sh <ds-of-main> <ds-of-the-branch>` does the comparison over
the design and every example.

## Draw before building

A change to the architecture is drawn first as a `.ds` proposal in the live
viewer, discussed there, and its principles written as rules over the proposal.
Code comes after the design is settled.

## Rules that already check these practices

On the checker's proposal (`checkp`, the live viewer, DS-138): `checker-knows-no-step`
(the checker depends on `IStep`, never on a concrete step) and `steps-are-steps`.
They move into DriftSpec's own design when the checker is split. The naming of
interfaces and abstract classes waits for DS-107, which lets a rule read a
class's name and its derived stereotypes; it will be a rule of this project.
