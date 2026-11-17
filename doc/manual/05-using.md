# Using DriftSpec

DriftSpec is one program, `ds`, written in C++. It reads a folder of `.ds` files as one
design and checks it, draws it, compares two versions of it and answers questions about it.
Around it there is what an editor and a pipeline need: a language server, a tree-sitter
grammar, a Python package and a live viewer.

## A workspace

A workspace is a folder of `.ds` files, read together. There is no project file and no
import: a name is found wherever it is declared, and the same package may be reopened in as
many files as you like.

```
design/
  shop.ds
  store/Cart.ds
  store/Line.ds
  diagrams/structure.ds
```

## The commands

```bash
ds check design                     # the findings, then the counts of the model
ds render design                    # the whole design in the viewer, one HTML file
ds render design CL_Store -f svg    # one diagram drawn: svg, or text for a terminal
ds diff main HEAD design            # what changed between two versions, in the model's terms
ds query design entities shop@store # ask the model: entities, relations, coverage, findings
ds export design                    # the model as JSON, for another tool
ds manual                           # this manual; ds manual <kind> or <CODE> for one page
```

`ds check` exits with 0 when no finding is an error and with 1 when one is, so it fits in a
pipeline as it is. Every command takes `--help`, and `ds manual commands` lists what
each exit code means.

## Findings

A finding has a code, a place and a sentence: `E-…` is an error, `W-…` a warning. `ds manual
W-REF-UNDECLARED` says what one means and what to do about it; `ds manual codes` lists them
all.

```
store/Cart.ds:4:7  W-REF-UNDECLARED  class 'Line' is referenced by shop@store::Cart::lines but declared nowhere in the workspace
```

## Building it

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build
```

The binary is `build/ds`. It needs a C++ compiler and CMake, and nothing at run time.

## In your editor: the language server

`driftspec-lsp` is the language server, on stdio. It checks the design a file belongs to on
every change and gives diagnostics, go to definition, references, hover, completion and
symbols. Two commands draw what is under the cursor: `driftspec.draw` as text and
`driftspec.view` in the viewer.

```bash
pip install ".[lsp]"      # from a checkout of the repository; lsp adds the server
driftspec-lsp
```

Any editor that speaks the Language Server Protocol can start it for files ending in `.ds`.

## Syntax: the tree-sitter grammar

The language is a tree-sitter grammar (`grammar/`), with its queries for highlighting and
folding (`grammar/queries/`). Neovim loads it as any other language, and this site colours
its editor with the same grammar, compiled to WebAssembly.

## From Python

`pyds` is the engine as a Python package, in the same process.

```python
import pyds

model = pyds.load("design")
model.find("shop@store::Cart").kind       # 'class'
pyds.check("design").errors               # the findings of ds check, typed
pyds.render("design", "CL_Store", "text", 120)
```

## A design seen live

`driftspec-live serve design --open` opens the viewer in the browser and draws the design
again on every save, with what changed marked and the findings beside it. An agent can point
at the design through the same contract, served as MCP tools:

```bash
claude mcp add driftspec-live -- driftspec-live mcp
```

## This playground

The editor of this site is DriftSpec compiled to WebAssembly: it runs in your browser, with
no server behind it. Every example of these pages opens there with one click.
