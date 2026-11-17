# Lessons

The default workspaces of the playground are lessons. A lesson is a small workspace
(`*.ds`) and a script (`lesson.json`) of steps. Each step is a card anchored to what
it talks about, and the code grows as the lesson goes:

- **watch** steps: the lesson writes code itself, typed in front of the reader
  (`write`), and the reader confirms (`done.ack`);
- **do** steps: the reader writes, and the engine says when it is done (`done.entity`,
  `done.edge`, `done.finding`, `done.clean`); `solution` lets the lesson write it
  for a reader who is stuck.

Lessons are short, so nothing is kept: choosing a lesson, from the list or with the card
that offers the next one, always starts it from the beginning, and so does reloading.

## Curriculum: bottom up

Each lesson builds on the one before and on one running example (a shop that grows
into a small system). A lesson teaches one idea, ends clean, and ends with a diagram
that shows what it taught.

| # | Lesson | What it teaches | Ends with |
|---|---|---|---|
| 0 | Where things are | the workspace and its files, a diagram per pane, relations lit on hover and click, a pane alone, problems | the page |
| **Static: the parts** ||||
| 1 | A class and its diagram | package, module, class, identity; a diagram is a question; `owns`; a reference to nothing is a finding | `CL_Store`: Cart, Line, Money |
| 2 | Members | attributes and methods, visibility `+ - #`, every form of default, `dataclass`, `enum`; interface and abstract are derived from the members | a richer `CL_Store` |
| 3 | Relations | `owns` / `refs` / `shares`, inheritance `class X : Base`, uses from parameters; an ownership cycle is an error | relations lit by kind |
| 4 | Modules and names | several modules, qualified names, bare names resolve outwards, private members, `extern module` | `CL_Shop`, the whole package |
| 5 | One class per file | a workspace is files: packages and modules reopen in every file, one class per file, diagrams in `diagrams/`; identity does not depend on the file | the workspace reorganised |
| **Dynamic: what it does** ||||
| 6 | A sequence | `sequence Class::method`, messages, results, `return`, checked against the classes (`W-MSG-UNDECLARED`, `E-MSG-ARITY`, `W-ARG-TYPE`) | `SEQ_Add`, `SEQ_Checkout` |
| 7 | Branches and scenarios | `opt` / `alt` / `loop` in prose, named scenarios, a scenario written apart, `local`, `depth` | scenarios side by side |
| 8 | State machines | `state Class`, `initial`, moves on operations, guards, actions | `ST_Cart` |
| 9 | Functions | free functions of a module, calling another module's function, static operations (`W-MSG-NOT-STATIC`, `W-RETURN-TYPE`) | `SEQ_Invoice` |
| **System: the black box** ||||
| 10 | Interfaces and the high-level design | the package as the black box, `interface` and operations, exit codes as scenarios, HLD closed and open | `HLD_Shop`, `HLD_Store` |
| 11 | Requirements | ids with revisions, `@refines`, `@requirement`, `constraint`, the requirements table, revising a requirement (`E-REF-UNPINNED`, `W-REVISION-DRIFT`), all of them moved to `requirements.ds` (level by placement, not by file) | `REQ_Shop`, the table |
| 12 | Verifications and use cases | `verification` nested, then in `verifications.ds` linked from outside with `@requirement`; `usecase` with outcomes, an operation that realises it | the chain from promise to test |
| **Further** ||||
| 13 | Failures | `fail`, `attempt` / `on`, `fails` promises, what escapes through a boundary | `fails nothing`, checked |
| 14 | Rules | `rule` with `for` / `never` / `holds`, `not`, containment; a rule about messages misses attributes; a library of generic rules (`rules/design.ds`, its own package, definitions included) adopted by linking to its requirement | one arrow left between store and billing, a library rule guarding it |
| 15 | Depth | `depth: 0`, `1` and `*` over one operation: a call to an operation with a sequence is a link at depth 0 and opens one level per step; nothing is written twice | `SEQ_Buy` from the shell down to the subtotal of each line |
| 16 | Levels | boxes hold boxes: a package inside the shop with two modules; `close: package`, `close: module`, and `open:` through it so one branch goes down to its classes; lifted onto a closed box a relation is a structural bond (solid) or a use (dashed) | `HLD_Levels`: the carrier open, every other module one box |
| 17 | The cut | depth counts calls, a cut counts boxes: `close: module` makes each module one lifeline and keeps only what crosses; `open:` goes through it; `fold:` stops following a call; `cut:` takes the cut of the boxes already drawn | `SEQ_Buy` with the store open, `SEQ_BuyBoxes` on the cut of `HLD_Shop` |
| 18 | Two levels | a scenario of the use case written in one block between the boxes: boxes named as participants, the sender before the arrow, a message bound to what a box offers or a label; the level below is held to it (`W-LEVEL-NO-CROSSING`), and what is not written yet is information; a `kind: tour` holds the boxes that take part and the sequence in one diagram, and the page plays it | `SEQ_Purchase` beside `SEQ_BuyBoxes`: the same crossings, and `TOUR_Purchase` played over the boxes |
| 19 | A machine that is checked | a state computed by a rule over what the class holds (`Paid if status = Paid, not cancelled`); the rules exclude each other (`W-STATE-OVERLAP`, with the cart that is in two); the start is derived from the defaults; the behaviours call the events in an order the machine allows (`W-STATE-ORDER`); the machine as a table, `kind: table` | `TB_Cart`, `ST_Cart` |

Notes for writing a lesson:

- The text teaches the language, never the page: no card explains a button.
- Every `do` step has a `solution`, so no reader is ever stuck.
- A lesson starts where the one before it ended: `shop.ds` is the previous lesson's
  final text with its own title line. After changing a lesson, `chain.py --from NN --bump`
  rewrites the starting files of the lessons after it (and sets aside readers' saved
  progress of the old text); never edit them by hand.
- A lesson is a test too. `replay.py` plays every step with its solutions against the
  engine (the native build of the playground's call) and requires each step to pass,
  no do-step to pass before the reader acts, and the end to be clean.
  `browser_check.sh` plays each lesson in headless Chrome with a scripted reader and
  requires the page's final files to equal the replay's. Both to run in CI when the
  playground is consolidated.

## Writing a step

| Key | What it does |
|---|---|
| `at` | what the card points at: `{file, find[, after]}` a line (`after: "}"` the end of that line's block), `{pane[, select]}` a diagram (and the class whose relations light up), `{panel: "problems"}` |
| `shows` | the diagrams the step is about, when they are not the pane the card points at or the one its check looks at: one or a list. The page shows the diagrams of a step alone while it lasts (DS-242), and plays the one that is a tour (DS-246); a lesson with a step done on the page itself moves nothing |
| `write` | what the lesson types before the card asks anything: one or a list of `{file, end}`, `{file, below: {find[, after]}}`, `{file, into: {find[, first]}}` (end of the block, or its first line), `{file, replace, text}` |
| `solution` | the same shapes, typed when the reader asks the lesson to write it |
| `open` (lesson) | the file the lesson opens first |
| `title`, `text`, `done.ack` | each `{"en": ..., "es": ...}`; the code between backquotes is the same in both |
| `done` | `{ack}` a button; `opened`, `selected` (a pane), `solo` (a pane or `false`) for steps done on the page, with a `solution` of `{ui: open|select|solo|unsolo}` behind "Show me"; otherwise every condition given must hold: `entity`, `edge {diagram, from, to}`, `draws {diagram, text}`, `hides {diagram, text}` (the diagram is drawn and no longer shows it), `finding`, `gone` (a code that must have left the problems: what was left to write is written), `clean`; what is not yet written (coverage) reads as information, never as a problem |

What the curriculum met while being written, kept for whoever writes the next lesson:
the requirement diagram draws the classes that realise a requirement but not the
interfaces; a diagram asking for a scenario nobody declares reports nothing; rules about
messages do not see attributes (lesson 13 turns that into a step).
