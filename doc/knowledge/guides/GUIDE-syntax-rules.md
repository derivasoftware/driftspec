---
id: BOK-00004-1
type: guide
title: Why the language's text is shaped as it is
tags:
  - language
  - grammar
  - workspace
  - artefacts
---

# Why the language's text is shaped as it is

A DriftSpec design is read by more than the parser: by people in a review, by
agents that edit it, by grep and by git. Every syntax rule below exists so that
one of those readers can do its job without knowing the whole grammar.

## One thing per line

Braces enclose a body, each line inside holds one thing, `//` always starts a
comment, a field value runs to the end of its line, and multiline text is
indented under its key.

- **Diffs and merges work by line.** One member, field or annotation per line
  means a change touches the lines it changes and nothing else, two people
  adding members to one class merge cleanly, and blame names who wrote each
  member.
- **No context is needed to read a line.** Because `//` is a comment
  everywhere and a value ends at the end of its line, there are no quotes to
  balance and no escapes to remember. A person or a script can tell what a line
  is by looking at that line alone.
- **One shape for every construct.** A class, a requirement and a diagram are
  all a keyword, a name and a body of lines. Learning one teaches the rest.

## Modifiers are words, and nothing is reserved

A modifier such as `abstract`, `static`, `owns` or `many` is a modifier only in
front of a name. Anywhere else it is an ordinary name: `+ abstract: bool` is an
attribute called `abstract`.

The design is compared with the code name for name. A code base already has
attributes called `state`, `static`, `default` or `abstract`, and a language
that reserved those words would force the design to rename them. A renamed
member is one the drift check can no longer match, so every reserved word is a
gap between design and code. Reading the word by position keeps the code's
names as they are.

## `@` for packages, `::` for namespaces

`app@core@model::Entity` names the boxes of the high-level design with `@` and
the namespaces of the code with `::`.

These are two different things. A package is organisation: a deployable unit,
a boundary, a level of requirements. A namespace is what the code writes.
Using one separator for both would make it impossible to tell, from the name,
where the design's organisation ends and the code begins. With two, the part
after `@` is exactly the code's qualified name, `::` as C++ writes it, which is
how readers of the code match a design entity to a class. Outer boxes that only
organise drop out of the qualified name, because the code never wrote them.

## One identity, and a second declaration is an error

Every entity has one identity across the whole workspace, and declaring it
twice is an error, not a merge.

References, diagrams and diffs all name entities by identity. If one identity
could mean two declarations, a reference would point at whichever the loader
met first, and a diagram drawn from one file would disagree with one drawn from
another. Merging the two silently, or letting the last one win, hides the fact
that two places say different things about one entity. An error makes the
author choose which one is true.

## Defined once, extended by annotations

An entity is defined in one block. Other blocks may extend it, but only with
annotations: relations to requirements, constraints and other entities. An
extension never changes the definition.

This lets a box state what it needs from an entity it does not own. A package
writes its requirements and annotates the classes that realise them without
editing the file where those classes are defined, and without the owner of
that file knowing who consumes them. Because extensions add only relations,
what an entity is stays in one place, and a reader looking for the definition
finds exactly one.

An extension of something nobody defines is an expected node, reported once,
so a typo does not silently create an entity. A class written with annotations
only and defined nowhere else is the one exception: it is the class, empty,
which is how a class that only carries requirements is written.

## Every link to an artefact names a revision

A link to a requirement, constraint or verification names the revision it was
written against: `REQ-00001-2`, never `REQ-00001`.

When an artefact changes, its revision moves. A link that named no revision
would follow it silently, and nobody could tell which designs had been
reviewed against the new text and which still assume the old one. A pinned
link records what its author read. When the artefact moves on, the tool lists
the links that stayed behind, and each one is a review someone still owes.
