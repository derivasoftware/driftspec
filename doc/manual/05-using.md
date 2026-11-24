# Using DriftSpec

DriftSpec is one program, `ds`, written in C++. It reads a folder of `.ds` files as one
design and checks it, draws it, and answers questions about it.
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

The binary is `build/ds`. It needs a C++17 compiler and CMake, and nothing at run time.
`pip install .` from a checkout builds the same `ds` and puts it in the environment's scripts,
beside the Python package. On
Windows, from the Developer Command Prompt of Visual Studio 2022 (its Build Tools are enough):

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

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

It keeps each design loaded and loads it again on a change, reading again only the files that
changed: the diagnostics of an edit arrive in under 200 ms on DriftSpec's own design. The first
hover, definition or completion after a change takes longer, since it reads the whole model
again. An error shows as an error and a warning as a warning.

## Syntax: the tree-sitter grammar

The language is a tree-sitter grammar (`grammar/`), with its queries for highlighting and
folding (`grammar/queries/`). An editor with tree-sitter, such as Neovim, loads it as any other language, and this site colours
its editor with the same grammar, compiled to WebAssembly.

## From Python

`driftspec` is the engine as a Python package, in the same process (`import driftspec`).

```python
import driftspec

model = driftspec.load("design")
model.find("shop@store::Cart").kind       # 'class'
driftspec.check("design").errors               # the findings of ds check, typed
driftspec.render("design", "CL_Store", "text", 120)

ws = driftspec.Workspace("design")              # loaded once, asked as often as needed
ws.reload("design")                             # after a change: loaded again in place
driftspec.severities()                          # each severity: whether it fails, how an editor shows it
```

## A design seen live

`driftspec-live serve design --open` opens the viewer in the browser and draws the design
again on every save, where you were looking, with the findings beside it. Pass the design's
folder: everything below it is read and watched.

Errors are red and warnings amber, and the findings count at the top says how many fail, with
the rest beside it. Coverage, what is not written yet (a requirement nothing verifies, an
operation with no sequence), has a folded section of its own. The rules panel lists each rule
with what breaks it and the problems written in it.

An agent can point at the design through the same contract, served as MCP tools:

```bash
claude mcp add driftspec-live -- driftspec-live mcp
```

## This playground

The editor of this site is DriftSpec compiled to WebAssembly: it runs in your browser, with
no server behind it. Every example of these pages opens there with one click.

## What 0.1.0 does not do yet

DriftSpec is young. This is what a first release leaves out, so that nothing here surprises you.

- **The language may still change.** The grammar is not frozen and some words are under review
  before 1.0; a release that renames one says so in its changelog.
- **Some types of real code cannot be written yet.** In C++: pointers (`Node*`), the ownership a
  `std::unique_ptr` or a `std::shared_ptr` gives, a base class with its template arguments, and
  `long`. In Python: unions, `Callable`, `Literal` and tuples of any length. Write the nearest type
  the language has, or a class that stands for it.
- **No type aliases, and no deployment.** A name for a type that is not a class, and the
  libraries and binaries a system is built into, are not part of the language yet.
- **A sequence does not say that it creates an object**, and a message sent to a collection has
  no meaning of its own yet.
- **Two versions of a design are not compared.** An earlier `ds diff` was taken out before this
  release; it may come back.
- **Built and tested on Linux, Windows and macOS (Apple silicon); no Intel Mac wheel yet.** Every
  push is built and tested on the three; the release carries a wheel for each, and none for an
  Intel Mac until someone asks.
- **DriftSpec checks the design, not the code.** Whether the code does what the design says is
  outside this tool.
