<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/assets/banner-dark.svg">
  <img src="doc/assets/banner-light.svg" alt="driftspec, by deriva" width="480">
</picture>

DriftSpec is a language for writing the design of a piece of software as text, and `ds`, the
tool that reads it. You write requirements, classes, interfaces, sequences, state machines and
rules in `.ds` files; `ds` reads every file of a folder as one model, checks that the parts agree
with each other, and draws the diagrams from that same model. A diagram is never drawn by hand,
so it cannot drift from the design.

```
package shop {
  module store {
    requirement REQ-00001-1 {
      title: A cart shall charge its total to a card
    }

    class Cart {
      + total() -> int
      + checkout(card: Card) -> bool {
        @requirement REQ-00001-1
      }
    }
    class Card {
      + pay(amount: int) -> bool
    }

    sequence Cart::checkout {
      -> total() -> amount
      self -> card.pay() -> paid
      return paid
    }
  }
}
```

```console
$ ds check shop
shop/shop.ds:19:7  E-MSG-ARITY  shop@store::Card::pay takes 1 parameter, 0 given
12 entities · 0 expected · 16 relations · 1 findings
```

## What it does

- **Checks the design as a whole.** A message sent to a method that does not exist, or with the
  wrong arguments; a call that goes around the interface a box offers; a state that nothing
  leads to; a requirement that nothing verifies: each is a finding with a code, a place and an
  explanation (`ds manual <code>`).
- **Draws it.** Class, interface and component diagrams, sequences, state machines and tables
  are views of the model, as SVG, text or one self-contained HTML page with every diagram in it.
- **Reads it in three perspectives.** Structure (what the parts are), behaviour (what they do)
  and logic (the rules they must keep), on one map, so that touching a box shows what it holds,
  calls and uses.
- **Joins requirements, design and tests.** A requirement is satisfied by the design that names it,
  and verified by a verification that names the tests.

DriftSpec checks the design, not the code: whether the code does what the design says is outside
this tool.

## Getting it

DriftSpec 0.1.0 is not released yet; for now you install it from a checkout. One `pip install`
gives the `ds` command, the Python package, the language server and the live session. It
compiles the engine, so it needs a C++17 compiler and CMake 3.20 or later; everything else is
fetched by the build.

```bash
pip install ".[lsp]"
ds --version
```

To build only the command line, with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target ds
build/ds --version
```

It builds and is tested on Linux, and on Windows with Visual Studio 2022. It has not been tried
on macOS yet. The [using guide](doc/manual/05-using.md#building-it) has the details.

## First steps

```bash
ds check examples/hola                  # the findings, then the counts of the model
ds render examples/hola -o hola.html    # every diagram of the design in one page
ds render examples/hola SEQ_Greet -f svg -o greet.svg
ds manual                               # how each kind is written, what each finding means
```

`examples/` holds three designs that check clean: `hola`, a greeting service in four boxes;
`shop`, a shop from the requirement to the code; and `states`, a connection whose state machine
is checked.

## Learning the language

The manual comes with the tool (`ds manual <topic>`) and lives in [`doc/manual/`](doc/manual):

1. [Intent](doc/manual/01-intent.md): requirements, verifications, constraints and use cases.
2. [Structure](doc/manual/02-structure.md): packages, the high-level design, interfaces,
   classes and their relations.
3. [Behaviour](doc/manual/03-behaviour.md): sequences, branches, scenarios and failures.
4. [Logic](doc/manual/04-logic.md): rules and state machines.
5. [Using DriftSpec](doc/manual/05-using.md): the commands, editors, Python and what 0.1.0 does
   not do yet.

Every example in the manual is checked by the tests. The `playground/` folder builds a site with
an editor in the browser, the engine compiled to WebAssembly, and twenty guided lessons.

## Commands

| Command | What it gives |
|---|---|
| `ds check <workspace> [--json]` | The findings, then the counts of the model. Exits 1 when there is an error. |
| `ds render <workspace> [diagram] [-f html\|svg\|text\|json\|...] [-o file]` | The viewer of the whole model, or one diagram drawn. |
| `ds query <workspace> <question> [subject] [--json]` | Answers about the model: diagrams, entities, relations, behaviours, findings, coupling and more. |
| `ds export <workspace>` | The model as JSON, with its metamodel. |
| `ds manual [topic] [--json]` | The manual: each kind, each finding code, the commands and their exit codes. |

## Editors, Python and agents

- **Language server.** `driftspec-lsp` checks each design on every change, and gives
  definition, references, hover, completion and symbols, and draws what is under the cursor.
  Any editor that speaks LSP can run it on `.ds` files; the tree-sitter grammar in `grammar/`,
  with its highlight and fold queries, colours them.
- **Python.** The engine is also a Python package that runs in the same process:

  ```python
  import driftspec
  model = driftspec.load("examples/hola")
  driftspec.check("examples/hola").errors        # 0
  print(driftspec.render("examples/hola", "CL_Hola", "text", 120))
  ```

- **A design seen live.** `driftspec-live serve <workspace> --open` keeps the viewer open in the
  browser and draws the design again on every save. A person or an agent can point at it: take
  the reader to an element, highlight elements and leave notes. `driftspec-live mcp` serves the
  same as MCP tools, so an agent such as Claude Code can use it:

  ```bash
  claude mcp add driftspec-live -- driftspec-live mcp
  ```

## DriftSpec's own design

The design of DriftSpec is written in DriftSpec, in [`design/`](design): about 2,900 elements
that `ds check design` reads in a fraction of a second. It is the largest example there is.

## Repository

| Folder | What is in it |
|---|---|
| `grammar/` | The tree-sitter grammar and its generated parser. |
| `include/ds/`, `src/ds/` | The engine: parse, model, bind, check, render, query. |
| `app/ds/` | The `ds` command line. |
| `python/` | The Python package, the language server and the live session. |
| `viewer/` | The viewer page compiled into `ds`, and its themes. |
| `playground/` | The site: editor, lessons and docs, with the engine in WebAssembly. |
| `examples/` | Designs to start from. |
| `tests/` | Unit and system tests, and the corpora that hold each rule's expected findings. |

To build and run every test:

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build
```

After editing the grammar, regenerate the parser with `tree-sitter generate` in `grammar/`.

## Contributing

How to build, test and send a change: [`CONTRIBUTING.md`](CONTRIBUTING.md). What each release
brings: [`CHANGELOG.md`](CHANGELOG.md). A security problem is reported privately:
[`SECURITY.md`](SECURITY.md).

## Licence

Apache License 2.0 ([`LICENSE`](LICENSE)), copyright 2026 ARGOSQA SLU (deriva). The grammar in
`grammar/` is MIT ([`grammar/LICENSE`](grammar/LICENSE)), so that editors and other tools can
take it freely. The third-party code compiled into what DriftSpec ships, and its licences, are
listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
