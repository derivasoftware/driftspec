---
id: BOK-00001-1
type: guide
title: What DriftSpec ships carries its licences and the notices of the code inside it
tags:
  - release
  - licence
---

# What DriftSpec ships carries its licences and the notices of the code inside it

A licence is a set of conditions on copies. The conditions travel with the
copy, not with the repository: a user who downloads a wheel, opens the site or
runs a binary attached to a release never sees the repository, so whatever the
licences ask for has to be inside what they got.

## Why Apache 2.0 for the tool

DriftSpec is held by a company and meant to be used inside other companies.
Two clauses of the Apache License 2.0 matter for that, and a shorter
permissive licence such as MIT has neither:

- **The patent grant.** Every contributor licenses the patents their
  contribution reads on, and the licence ends for anyone who sues over a
  patent in the work. A user does not have to wonder whether the holder could
  later claim a patent on how the tool checks a design.
- **The trademark clause.** The licence gives no right to the names. Anyone
  may fork the code; nobody may ship the fork as DriftSpec.

Apache 2.0 also asks that a NOTICE file, when the work has one, go with every
copy and every derived work. NOTICE is where the holder is named and where a
reader is sent to the third-party notices, so it is one of the files that
must ship.

## Why MIT for the grammar

The tree-sitter grammar is a different kind of artefact. Editors, language
servers and code hosts vendor grammars by the dozen and compile them into
their own binaries. Almost every grammar in that ecosystem is MIT, and the
projects that bundle them expect it: a grammar under a licence with more
conditions is one they leave out. The grammar carries its own LICENSE and its
manifest says MIT, so a tool that copies only the grammar copies the right
terms with it.

## Why the third-party notices ship

The engine is C++ that compiles other people's code into itself: the
tree-sitter runtime, JSON for Modern C++ and miniz (all MIT), CLI11
(BSD-3-Clause), and for the WebAssembly build the Emscripten runtime and its
system libraries. Statically linked code is a copy of that code. MIT and BSD
both allow it on one condition: the copyright notice and the licence text go
with every copy, binary ones included.

So each shipped thing that contains that code carries a file that names each
library, the version built and its licence. Naming the version matters: the
notice is a statement about what is inside this particular binary, and a
library that changes its licence between versions changes what the notice
has to say. Code that only builds the tests, such as GoogleTest, is in nothing
that ships and is listed only to say so.

## The rule in practice

- Every artefact a release produces (the wheel, the site, each attached
  binary) carries LICENSE, NOTICE, the grammar's LICENSE and the third-party
  notices.
- A new library fetched by the build is added to the notices in the same
  change that fetches it, with the version pinned by the build.
- A test that reads the packaging configuration keeps the list of shipped
  licence files from shrinking by accident.
