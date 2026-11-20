---
id: BOK-00002-1
type: guide
title: Every command answers within a hundred milliseconds
tags:
  - performance
---

# Every command answers within a hundred milliseconds

DriftSpec is asked many times for each change a person makes. An editor
preview redraws the diagram on every save, the language server checks the
workspace as the text changes, and an agent loop runs a check after each edit
it makes. Each of those callers waits for the answer before doing anything
else.

## Why a hundred milliseconds

A tenth of a second is the usual threshold below which a response reads as
immediate: the person sees cause and effect, not a wait. Above it, a preview
lags behind the typing and a person starts batching changes to avoid the
pause, which is the opposite of what a live design is for. For an agent the
cost is plainer: a loop of a few hundred checks at one second each is minutes
of nothing.

The budget is stated for a workspace of a hundred classes with their
relations, behaviours and diagrams. That is a realistic size for one
component's design, larger than most, and it is the size the tool's own design
is measured against.

## Why it is a budget and not a measurement

The first measurements were far inside it: a few milliseconds for the tool's
own design and under ten for a synthetic hundred-class workspace. Speed of
that kind does not stay by itself. Each new check, export or drawing adds a
little, none of them is slow alone, and a tool that has no number to stay
under drifts past it without anyone deciding so.

A stated budget turns that drift into a decision. A change that would cross it
is either made faster or argued for, in the open.

## Why it binds every command

A budget on one command would let the cost move elsewhere: a fast check and a
slow diff give a preview that lags all the same. So the budget applies to
loading, checking, exporting, diffing and drawing alike. Because it applies to
everything the tool does rather than to one capability, it is a constraint and
not a requirement.

## What it rules out

- A background service or cache that the tool needs in order to be fast. If
  a cold run is within the budget, nothing has to be kept warm, and nothing
  warm can go stale.
- Work that grows faster than the workspace. A check that compares every
  entity with every other is fine at ten classes and fails the budget at a
  hundred.
