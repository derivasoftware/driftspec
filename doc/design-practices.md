# How we design

*Practices agreed with the Owner while cleaning up DriftSpec (2026-11-09, DS-111,
DS-125, DS-138). They are ours: the way this project and the Owner's projects are
built, not checks DriftSpec ships for anybody. Each one says whether a rule in the
project's design already checks it, or what the rule language still lacks. When a
practice becomes a rule, the rule is the reference and this text points to it.*

## Generic where it makes our own work easier

We add an abstraction when it saves us work or mistakes now: a shape every step
shares, an entry point tests can replace, a table read instead of a list copied.
We do not add one for a plugin, a third party or a second configuration nobody
has asked for. A configuration module comes when a second source of settings
exists; until then the one place that builds what a run needs is a function.

## A fact lives in one place

What the language is (kinds, relations, containers, which fields are derived)
lives in the metamodel; how an identity is written lives in `model/Identity`.
Everything that needs the fact reads it from there. A list of kinds or relations
written by hand in a consumer is a copy, and copies drift: most defects of the
first cleanup wave were two copies of one fact that no longer agreed.

## Interfaces are a module's contract

A pure interface says what the rest of the code relies on: the module's entry
point (`IChecker`), or the shape our own pieces implement (`IStep`). The rest of
the code depends on the interface, so the inside can be rebuilt without touching
it. A class that nobody outside uses needs no interface.

Naming (the Owner's convention): a pure interface is `I<Name>`, an abstract class
`A<Name>`, a concrete class a plain name. *Waits for DS-107: a rule must read a
class's name and its derived stereotypes.*

## An abstract class earns its place by what it implements

`ACheck` exists because its `run` hands a check the model to read only: the
compiler keeps every check from changing it. A base class that nobody uses as a
type and that implements nothing is a label, and labels go in comments.

## Change the model, or read it: never both in one step

A step that enriches the model (linking, derivation, typing behaviour) and a step
that only checks it are different things. A check reads and gives back what it
found; it never writes. Its name says so: a step that adds relations is not
called a check. The order of the steps comes from what each one says it needs,
not from a list someone keeps in order.

## Whoever builds a thing owns it; what is given is held by reference

The collections a component runs over are built in one place outside it and
given to it; the component holds them by reference. Smart pointers stay at the
top layers; low-level classes take their dependencies by reference; lifetime is
settled at the boundary, by RAII or a factory.

## Name what a thing does

A function named `check` does not change the model; a class named after a role
plays that role. A name that says less than the thing does, or something else,
is a defect to fix like any other.

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
explained.

## Draw before building

A change to the architecture is drawn first as a `.ds` proposal in the live
viewer, discussed there, and its principles written as rules over the proposal.
Code comes after the design is settled.

## Rules that already check these practices

On the checker's proposal (`checkp`, the live viewer, DS-138): `checker-knows-no-step`
(the checker depends on `IStep`, never on a concrete step) and `steps-are-steps`.
They move into DriftSpec's own design when the checker is split.
