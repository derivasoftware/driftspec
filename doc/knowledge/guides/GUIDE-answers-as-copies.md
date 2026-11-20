---
id: BOK-00003-1
type: guide
title: The engine answers with copies, never with views into its model
tags:
  - model
  - index
  - api
---

# The engine answers with copies, never with views into its model

The engine keeps one model of the workspace: entities, the relations between
them, and an index for finding relations fast. Many readers ask it questions,
inside the engine (the checks, the diff, the drawings, the exports) and outside
it (the command line, the Python package, the WebAssembly build, the language
server). The model knows none of them.

## Why a view is dangerous

A view is a pointer, a reference or an iterator into something the model
owns. It is cheap to hand out and it breaks in two ways.

- **It can be left pointing at nothing.** The model keeps its relations in a
  growing list. When a later relation makes the list reallocate, every pointer
  into the old storage is left dangling. Two readers inside the engine, the
  use case check and the diff, once kept pointers into that list; they keep
  copies now.
- **It can change the model.** A reader that holds a mutable view can write
  through it, and then the model says something nobody declared.

A copy has neither problem. It belongs to the reader, it stays what it was
when it was made, and nothing the reader does to it reaches the model.

## Why one contract and no exception

It is tempting to give fast readers a view and slow ones a copy. That makes
the rule depend on who is asking, and the model cannot know who is asking. A
rule with exceptions has to be checked case by case, and the case nobody
checked is the one that breaks. So every reader gets the same contract: the
relations are read only through the index, and the index answers with copies.

The copies cost little. A design is small next to the memory of the machine
it runs on, and the time budget is measured with the copies in place.

## Why the order is the model's

The index answers in the order the model states the relations. Drawings and
exports are written from those answers, so an answer in any other order would
change the text they produce, and a diff of two exports would show changes
nobody made. One order, the model's, keeps every output reproducible.

## Why the index holds nothing of its own

The index is a way into the model, not a second model. It holds no fact the
model does not hold, and it changes in the same step as the model: declaring,
relating and renaming update both together. An index built apart from the
model, or refreshed later, would be stale for part of a check, and a check
that reads a stale index reports findings about a model that no longer
exists. Without the index the model gives the same answers, only slower.

## At the edge of the engine

The same reasoning applies at the boundary. A caller in Python or in a browser
lives in another runtime with its own memory and its own lifetimes; a view
into the engine's memory would outlive the model it points into as soon as
the workspace reloads. So every answer that leaves the engine is a document
the engine writes, in JSON, with the same shape the command line prints. A
tool that reads the command line and a tool that calls the package see one
contract.
