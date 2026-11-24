---
id: BOK-00005-1
type: guide
title: Every drawing and page takes its colours, fonts and text measures from one theme
tags:
  - style
  - render
---

# Every drawing and page takes its colours, fonts and text measures from one theme

DriftSpec draws in many places: the SVG of a diagram, the same diagram in a
terminal, the viewer, the live page an agent and a reader share, the
playground and the page of a whole design. Each of them used to define its
own colours. The same kind of entity had a different colour in each, a dark
palette was written twice in one stylesheet, and the size of the SVG's font
was tied to the layout's measures only by both happening to agree. A change
of colour was a hunt through five files in four vocabularies.

## What the theme holds

`ds::theme::Theme`, in `src/ds/theme/Theme.cpp`, is the one place a value is
written:

- every token of the `--pr-*` contract that the SVG shares with
  plantuml-render, with its light value and its dark one;
- the font, and how it measures on each sink: the advance of a character and
  the height of a line, in pixels on a page and in cells in a terminal;
- the terminal's palette, a colour code per role, and the colour a host that
  paints cells gives each code;
- the variables of each page, in that page's own vocabulary.

One is made where the engine starts (the command line, a `Workspace`) and
handed by const reference to what draws. Geometry never reads the font: it
takes `Measures` and nothing else (DS-392).

## How a form gets it

A writer (`IThemeWriter`) knows where the values go in one form and holds no
value. `ThemeWriterFactory` is the only place that names the writers: the
SVG's stylesheet, what a sequence adds to it, and one per page.

The drawings ask the writers when they are made. The pages cannot: the viewer
is embedded into `ds`, the live page ships in the Python package and the
playground is copied as a static site. So each page carries what its writer
writes between two markers, `/* theme: <page>, written from the theme ... */`
and `/* end theme: <page> */`. `tools/theme.py --write` rewrites the regions
from the theme, and the ctest `theme_pages` fails when one differs.

## The rule

No colour is written anywhere else in what DriftSpec ships: not in the
engine, not in a page, not in a script that draws on a canvas. A shadow is a
colour too, so it is a token, and a dark page may want its own. A gradient's
mask stop is not: a mask reads alpha, and its stops stay where they are.

`tests/system/python/test_theme.py` holds the rule (CST-00012-1). It reads
every file the product ships and fails on a colour written outside the theme
and outside a page's region.

## Adding or changing a colour

- **A colour the drawings use:** a token in `Theme.cpp`, light and dark, and
  its place in the writer's stylesheet.
- **A colour a page uses:** a token in that page's table in `Theme.cpp`, and
  its place in the page's writer. Then run `tools/theme.py --write`, and the
  page uses `var(--name)`.
- **A script that draws on a canvas:** it reads the variable from the page,
  as the viewer reads `K` from its computed style, never a literal.

Changing a value changes every drawing that uses it, and the goldens show it.
That is intended: a palette is changed on purpose and reviewed as such.
