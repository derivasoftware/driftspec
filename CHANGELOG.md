# Changelog

Every release of DriftSpec says here what it brings and, above all, what a design written for the
release before has to change. The version is the one `ds --version` prints.

While DriftSpec is below 1.0 a release may still rename a word of the language; when it does, the
entry says which word and what to write instead.

A merge request does not edit this file: it writes its entry as `changelog.d/<card>.md`, and
`tools/changelog.py` later moves each entry under the version its merge was released as (see
`changelog.d/README.md`).

## 0.2.0

What changed for a user since 0.1.0, released on 18 November 2026: walkthroughs, overloads and outcomes that name their
failure in the language; a check that keeps what is not yet written apart from what is wrong; drawings spaced the same in
SVG and in the terminal; one look for every drawing, page and the viewer, which also stays responsive on a large
design; a faster engine; a manual that teaches what first users stumbled on. The entries of each 0.1 release below say
the same in detail.

### Before you upgrade

What a design written for 0.1 has to change.

- Write types as your code writes them. `extern module` is gone and is now a syntax error: delete those blocks, since a
  name the design does not declare is text, with no relation and no finding. The neutral types are gone too: write
  `std::string` and `std::vector<Line>` in a C++ box, `str`, `list[Line]` and `Line | None` in a Python one.
  `tools/migrate_types.py` does most of the rewrite; check what it wrote against the code.
- Say an attribute's multiplicity in front of it, `many` or `optional`, beside `refs` or `shares`. The type no longer
  says it.
- Rename anything you called `final`: it is now the word for a state machine's final states.
- In a rule, `name`, `id` and `kind` are now words of the language. A definition of one of those names is reported
  (`E-RULE-RESERVED`), and no kind may declare a field named `name` or `id`.
- Expect warnings where the check was silent: a bare name two classes of its package share (`W-NAME-AMBIGUOUS`; qualify
  it, `store::Card`); a `show:`, `hide:` or interface `kind:` value nothing reads (`W-FIELD-VALUE`); a rule that reads
  `uses` off an attribute (`W-RULE-NEVER-MATCHES`; read it through `owns`, `refs` or `shares`); a piece that realises a
  requirement written outside the box that holds it (`W-SATISFIES-OUTSIDE-HOME`).
- If you style the viewer with a theme of your own, set its six new variables: `--you`, `--shadow`, `--shadow-pop`,
  `--shadow-soft`, `--k-interface` and `--k-abstract`.
- On macOS, the wheel needs macOS 13.3 or later.
- If you read the render IR or the export: every IR document says its diagram's kind; a state machine's start is a node
  of kind `initial`, its ends `final` and its states `state`; the export writes a machine's transitions and rules as
  records instead of text to split.
- In Python, `driftspec.render` and `Workspace.render` draw the format asked for, cells and layout included, and raise
  `driftspec.NotImplemented` for one they do not draw, where they used to hand back a text drawing.

### The language

- Take a reader through your design in steps with a walkthrough, a diagram of kind `walkthrough`. Each step shows a
  diagram, or one scenario of a behaviour, frames what it is about, marks what it talks to, points a callout and says
  its text. `ds check` tells what a step gets wrong, the viewer plays it, and `examples/shop` carries one from the
  shop's promise to its classes.
- Say which failure an outcome is the end of: `outcome bank-down fails BankUnreachable: nothing is paid`. A failure a
  scenario lets escape and no outcome names is told at the scenario.
- Declare two operations of one name with different parameters. Each is named by its parameter types,
  `Factory::of(Scene)` beside `Factory::of(Report, int)`; a behaviour names its overload the same way, and a call is
  related to the overload its arguments fit.
- Mark a state machine's final states, `final Paid, Cancelled`. A transition out of one is a warning.
- Write a dotted type as Python writes it: `store.Card` relates to the class `store::Card` names.
- Write pointers, template bases such as `class SceneJson : IFormat<Scene>`, methods with type parameters, and Python
  unions.
- In a rule, read an entity's `name(x)`, `id(x)` and `kind(x)`, match a value against a pattern
  (`name(c) matches "^I[A-Z]"`) and test a member's flag (`m is abstract`). A table's column reads them too:
  `column is: kind(r)`.
- When a rule breaks through a definition, read what the definition went through:
  `broken for c = Shop: x = Store (depends: m = Shop::store)`.
- A relation joins only the pairs of kinds it is declared for, so a class that inherits from a use case is an error.

### The check

- Read what is not yet written apart from what is wrong. An approved requirement that nothing satisfies and an approved
  constraint that nothing verifies are coverage, not findings. `ds check` ends its counts line with how many are open
  and says where they are listed; coverage never changes the exit code, and a draft owes nothing.
- Find the entity each finding is about: it is named as `subject` in `ds check --json` and the export, and as
  `Finding.subject` in Python.
- Find a file that is not UTF-8 at its file, line and column (`E-ENCODING`), while every other file checks as usual.
  Such a file used to check clean while export and render failed without naming it.
- Trust a drawing: `ds render` no longer draws from a file that does not parse. It writes the syntax error and exits 1,
  where it drew a blank and exited 0.
- Write any `depth:` or `neighbours:`: one that is no number or too large for one is a warning, and no longer stops
  `ds` or keeps it busy for good.
- Read findings and coverage in the order they are written: by file, then line.

### Drawings

- See a sequence spaced the same in SVG and in the terminal: one engine places its lanes, rows and frames, in pixels or
  in characters.
- See a table spaced the same way: a cell longer than forty characters wraps between words in both, and each column
  keeps room either side.
- Recognise each kind by its colour, the same in every drawing and page, light and dark: class indigo, interface rose,
  enum green, dataclass amber, function slate, abstract violet.
- See every drawing in the playground's look wherever it is shown: each SVG carries its own stylesheet, a dark or light
  card with its name underlined in its kind's colour, the kind's badge, members in the editor's colours and thin
  relations, so `ds render -f svg` draws on any page what the playground draws.
- See a diagram rest and light the same in the playground, in a walkthrough and on any page ds writes: relations quiet
  until asked, smaller tips tinted, the light following the pointer.
- Read a state diagram as UML draws one: a filled dot where the machine starts, a bullseye for each final state, states
  as capsules, transitions as curves with their labels clear of the states, and a transition back to its own state as a
  loop beside it.
- Fit more of a class diagram on a screen: a long edge running straight down no longer takes a row of its own.
- Draw large designs in the terminal: a text drawing takes a tenth of the memory it took, 380 MB instead of 3 GB for 200
  classes. One that would still take more than about 128 million characters is refused, with what it would take,
  instead of exhausting the machine.
- Read a table with symbols such as ≠ in it: a cell is measured in characters, so its column is no wider than it needs.
- Tell in a diff a changed relation (`~`, in its colour) and a renamed one (`>`) from one that was removed.

### Speed

Each figure was measured when its change landed, on the design as it was then.

- `ds check` on DriftSpec's own design went from about 330 ms to about 105 ms; on a generated design of 3,200 classes,
  from 5 s to under 1 s; on the whole repository, from 810 to 370 ms.
- Rules are evaluated with about half the instructions they took, and 5 to 7 % less again on designs with many rules.
- Drawing a diagram runs only the part of the check it reads: DriftSpec's main class diagram went from 243 to 111 ms.
- An editor gets its diagnostics in about half the time: 337 to 175 ms on DriftSpec's design, 484 to 184 ms on 3,200
  classes.
- `ds export` and every JSON document are written as they are made, faster and with less memory.
- Rules scoped to a box are checked faster: a scoped range reads only what its box holds, once per check. `ds check` on
  DriftSpec's own design went from 0.69 to 0.60 s, and on the playground's tour from 0.74 to 0.65 s.
- Rules are evaluated with each term read from a slot worked out once, and each constant's meaning looked up once:
  `ds check` spends 28% fewer instructions on DriftSpec's own design and on the playground's tour, every finding and
  table as before. A design with no rules is unchanged.
- Drawing a diagram that closes boxes finds what each box holds through the model's tree, instead of a pass over the
  whole model for every box.

### The manual

- Learn from the guides what five first users had to find out by trial: how every kind of link is written, a
  requirement's level, a constraint's verification, what a table's column may hold, the chain from a use case to its
  scenarios, what a `fails` clause promises, a class carrying out an interface of its module, what each relation of a
  rule sees, definitions, coverage and the counts line, asking the model with `ds query`, and adding to a design you did
  not write.
- Read the new walkthroughs guide, `ds manual walkthroughs`, with a worked example.
- Look up the values a field takes in `ds manual diagram` and `ds manual interface`, and the new pages
  `ds manual step` and `ds manual E-ENCODING`.
- See what each kind is part of, structure, artefact, behaviour, logic or view, in `ds manual <kind> --json` and the
  export.

### The command line

- Ask `ds query` the right question: its help says which subject each question takes, and a subject of the wrong kind
  gets an error that says what it takes, instead of an empty answer.
- List everything a scope holds at any depth: `ds query <workspace> entities shop@*`.
- Get each relation's two ends: `ds query <workspace> relations <id> --json`.
- List the walkthroughs and the steps of one with `walkthroughs`, and what escapes a use case with `failures`.
- Ask for the findings or the coverage of a file or of an entity and everything it holds.
- Play the walkthroughs in the page `ds render <workspace> -f html` writes.

### The viewer

- Read the map in the playground's look: its palette for each role, light and dark, Figtree and JetBrains Mono, each
  box's kind a badge with a line under its name in the kind's colour, members with their visibility and types in the
  editor's colours, relations in thin strokes. Boxes, layout and what the map shows are as they were.
- Open it dark, as the playground opens, and switch with the playground's toggle (◐); your choice is kept, and a
  stylesheet of your own (the night theme) still wins. The legend is gone, since the badges say each kind. A
  walkthrough's diagram cards stand in a column at the side, and in a strip along the top on a phone.
- See the map's structure at a glance: at rest it draws only inheritance, what inherits and what realises, and every
  other kind of relation is yours to turn on from a list beside the toggle, each with its stroke, your choice kept.
  On DriftSpec's own design the map at rest goes from 1,191 relations in structure and 605 in behaviour to its 97
  inheritances. Selecting a box or a class still lights everything it touches.
- Open and move about a large design without waiting: on DriftSpec's own design the pause on opening goes from about
  5 s to under half a second, a zoom's from 4.8 to 0.3 s, and with a class or a module selected each frame takes about
  2 ms instead of a third of a second to a second. The fonts no longer hold the first drawing while they load.

### The editor

- Keep a design open: the language server keeps it loaded, parses again only the files you change, and publishes the
  diagnostics of an edit in about half the time.
- Hover and outline any line, operators, states, rules and outcomes included: the editor knows which declaration each
  line is written in.
- Follow the live viewer: each finding in its severity's colour, a count of what fails with the rest beside it, and the
  coverage folded into a section of its own.

### Python

- Read a workspace as `ds` reads it with `driftspec.files()`: every `.ds` file below a folder, dot-folders included,
  links to files followed.
- Reload a workspace in place with `Workspace.reload(files)`, which parses again only what changed.
- Get the severity table with `driftspec.severities()`, a relation's ends as `Answer.source` and `Answer.target`, and a
  file that is not UTF-8 as `E-ENCODING`, since files are handed to the engine as bytes.

### The playground

- Start with a tour of DriftSpec's own design: a first visit to the editor opens on it once the editor shows, with an
  animation while the design is read, checked and drawn, and the coach then says how long that took, what it bought,
  and how long PlantUML takes only to draw as many diagrams. The small lessons come before the tour, and only an
  example you chose is remembered for your next visit.
- See the live viewer at work on the home page, beside an editor, in a short recorded clip; the home page itself loads
  no large design.
- Use the site on a phone: it fits the screen as the browser bar comes and goes.

### DriftSpec's own design

- Read DriftSpec's own design, which the playground's tour walks, one question at a time: how a file becomes a model,
  how a name binds, how a rule is checked, how a diagram is drawn, how the viewer gets its scene, and more, each a
  dozen boxes at most and named for what it shows, in place of diagrams that drew a whole namespace (one held 376
  boxes and 1,337 arrows). A diagram's name needs no `CL_` or `SEQ_` in front: any name ds reads is one.

### What 0.2 does not do yet

- The language may still change before 1.0; a release that renames a word says so.
- A type does not say who owns: ownership is what the attribute says, whatever the pointer.
- There are no type aliases and no deployment yet.
- A sequence does not say that it creates an object, and a message to a collection has no meaning of its own.
- Two versions of a design are not compared.
- There is no wheel for an Intel Mac, and DriftSpec is not on PyPI yet.
- DriftSpec checks the design, not the code.

## 0.1.178

Drawing a diagram with a cut finds each closed box's members through the model's tree, not by a pass over every
entity per box (DS-506). Nothing a design writes changes. The model's index answers three more questions: whether one
entity descends from another (by a pre-order interval), the chain of an id's holders, and a box's children of one
kind. All three are built when first asked and dropped when the tree changes. The resolver no longer walks a bare
name's scopes a second time: the first walk had already tried every candidate.

The tree and the holder read from an identity's text disagree in some places, and there the text is kept:
- an interface written directly in a package has no holder in the tree, so a closed package still finds it in its
  own scope;
- behaviours (sequences, their frames and messages, states) hang in the tree from where they were written and from
  their frames, and artefacts (requirements, verifications, constraints) from the box that holds them, while the text
  says otherwise. The resolver's scope walks and the cut's path to the top go on reading the text for that reason.

## 0.1.176

- The playground opens a first visit to the editor on the tour of DriftSpec's own design, and only once the editor shows (DS-510): an animation while the design is read, checked and drawn, then the coach says how long it took, what that time bought and what PlantUML takes only to draw as many diagrams. Only an example the reader chose is remembered, and the small lessons come before the tour. The home page shows the live viewer beside an editor in a recorded clip, which `playground/live/make_clip.sh` makes again; the home page alone loads no large design. (DS-510)

## 0.1.175

The rule evaluator looks up what a rule's constants stand for once, when it first solves each body, never at every step
(DS-507, second part). Each constant term's identity and the definition each call applies are resolved in the facts
once and kept with the body's compiled slots (`CompiledBody`). `ds check` spends a further 10% fewer instructions on
DriftSpec's own design and on the playground's tour (28% fewer than before DS-507), and the same on a design with no
rules; every finding, breach and table is as before, byte for byte.

The rule evaluator reads each term of a rule by its slot, compiled once, never by its name at every step (DS-507).
`Variables` compiles every body of its formula when it is made: for each literal, the slot of each term. The solver
reads them there, so `value`, `ready`, `unify` and `answers` look no name up. `ds check` spends 18% fewer
instructions on DriftSpec's own design and on the playground's tour, and the same on a design with no rules; every
finding, breach and table is as before, byte for byte.

## 0.1.174

DriftSpec's own design is drawn one question at a time (DS-505). Its 30 class, sequence and interface diagrams, most
of them a whole namespace (CL_Ds drew 376 boxes and 1,337 arrows, CL_BindNamespace 144 arrows), give way to 23 in
design/diagrams.ds, each answering one thing a reader asks, a dozen boxes and two dozen arrows at most, picked box by
box: engine-modules, doors-and-use-cases, how-a-file-becomes-a-model, how-a-name-binds, what-the-model-holds,
how-a-design-is-checked, how-a-rule-is-checked, how-a-failure-ends, how-a-state-machine-is-read,
how-a-diagram-picks-what-it-shows, how-a-diagram-is-drawn, how-a-diagram-is-laid-out, where-the-look-comes-from,
how-the-viewer-gets-its-scene, how-ds-query-answers, how-output-is-written, how-an-editor-gets-diagnostics,
how-the-live-viewer-follows-the-files, the-command-line, the-engine-in-process, how-a-file-is-parsed,
how-an-undeclared-operation-is-reported and how-a-syntax-error-is-reported. A diagram's name needs no CL_ or SEQ_
prefix; the requirement diagrams are unchanged.

## 0.1.173

- The viewer works out the relations of what is selected once per selection, not for every relation on every frame (DS-508). With a class selected on DriftSpec's design, a frame's work goes from hundreds of milliseconds to about 2 ms; what it draws is the same. (DS-508)

## 0.1.172

`ds check` evaluates scoped rules faster: a scoped range walks only the entities its scope holds, read once
per check, instead of every entity of its kind again for each binding of the ranges before it (DS-503).
Nothing a design writes changes.

## 0.1.171

- A diagram looks the same in a walkthrough as in the playground (DS-493): the walkthroughs rest and light their drawings with the playground's script (drawings.js) on the same grey stage, the playground drops its own copy of how a drawing looks (each drawing carries it since DS-490), and the viewer's palette gains the relation colours drawings.js lights with. (DS-493)

- The viewer's map wears the playground's look (DS-495): its colours are the playground's palette for each role, light and dark (and the theme a page sets), its fonts Figtree and JetBrains Mono, a box's kind a badge with a line under its name in the kind's colour, members with their visibility dots and their types and parameters in the editor's colours, relations in the rule with thin strokes and smaller tips. Boxes, layout, routing, interaction and what the map shows are as they were; the scene is untouched. (DS-495)
- The viewer opens dark, as the playground does, and carries the playground's light/dark toggle (◐), its choice kept by the viewer; a reader's stylesheet (the night theme) still wins. The legend is gone: the badges speak for themselves. A walkthrough's diagram cards stand in a column at the side; on a phone they stay a strip along the top, clear of the step's card at the bottom. (DS-495)

- The viewer's map draws at rest only inheritance (inherits and realises), in every perspective (DS-504). Every other relation kind (depends, owns, shares, refs, uses, calls, consumes, breaks) is turned on from a list next to the theme's toggle, each with its name and a swatch in its own stroke; the choice is kept for the reader. A selected box or class lights its relations as before. On DriftSpec's design, the map at rest goes from 1191 relations in structure and 605 in behaviour to the 97 inheritances. (DS-504)

## 0.1.170

A ds drawing rests and lights the same on every page (DS-491): the playground's rest-and-light code (each relation's
kind, the ones touching a class quiet until asked, tips shrunk and tinted, the light on pointing) is one script,
viewer/drawings.js, which the playground loads (build.py copies it) and ds puts into a viewer page beside the
walkthroughs' mode. The playground behaves as before; a viewer page with no walkthrough is written as it was.

## 0.1.169

- Every drawing ds writes carries the playground's look in its own stylesheet (DS-490): a dark or light card with its name underlined in its kind's colour, the kind's badge, members in the editor's colours, the rule's edges. Its colours are taken from the playground's palette, the one source, so `ds render -f svg` draws on any page what the playground draws. The playground, the viewer and the walkthroughs adopt it in DS-493. (DS-490)

## 0.1.168

A rule answers for a constraint, and is answered by it (DS-110): a rule that names a constraint with @constraint has
its reason, so W-RULE-NO-REQUIREMENT no longer lists it; and a rule that holds an approved constraint with one claim
is its verification, checked on every run, so W-CST-NO-VERIFICATION no longer lists the constraint. Both stay
coverage; nothing new is asked of a design.

## 0.1.167

The unit and integration tests are built as one executable per directory of tests (test_unit_ds_model,
test_integration_ds_check, …) instead of one per file: 261 executables, each linking the whole engine,
become 39, and the evidence build links and reads coverage for far fewer. Every test keeps its id, since
the evidence names each test by the file it is written in. tests/unit/ds/parse keeps one executable per
file: two of its files each define a fixture of their own under one name (DS-473).

## 0.1.166

Each relation joins exact pairs of kinds (DS-226): a class inherits a class and a use case a use case, a class
realises an interface and an operation a use case, a state machine is the behaviour of a class or an interface. A
class whose base is a use case, or a verification that names an interface, is now E-RELATION-KIND on its line, where
both were silent; a base written as an artefact's name is read whole. `ds manual` lists each relation's pairs.

## 0.1.165

- The playground answers through a use case of its own, `play-design` (UC-00008-1, DS-460): the design as one document (its artefacts, its diagrams drawn as render draws them, the identities and where each is written, the report) or a page of the manual, byte for byte what it received before. api's Browser is now only a door, held to the rule that doors know no model, and its exemption is gone. (DS-460)

## 0.1.164

see-design draws every diagram of a workspace (DS-474). It draws the svg and text drawings through render, describes the render-IR, and writes the viewer's page framed on a diagram. It also says, when asked whole, which diagrams did not read whole (DS-433). So `ds render`, the engine's render and page, and the Python face no longer draw anything themselves; their output is byte for byte what it was. A drawing reaches the sink lent to see-design as one verbatim text, which a sink of emit now accepts once per document. The text sink writes it as it is, the JSON sink fails NotImplemented, and the Python sink keeps it as a str, so `Workspace.run("see-design", {"format": "svg", ...})` returns the drawing. This is a bridge until a figure is described as marks (DS-463). FormatFactory keeps only the viewer's page: the svg and text writers (DiagramSvg, DiagramText) are gone, and so are the drawing options of FormatOptions (pretty, width, color). DS-470's Drawings moves from api to render (`render::Drawings`), and see-design draws every svg through it. The viewer's page gets a walkthrough's cards only when its scene has a walkthrough. This makes `ds render <ws> -f html` play walkthroughs again: since DS-470 merged over DS-409, only the engine's page carried them. The table of questions `ds query` answers now lives in the manual, and both the query and the command line's help read it there. Two rules hold the doors in the design. One is that the command line and the api know no model: the loading machinery (Workspace, Api, Bound) is exempted for good, and the playground's Browser until DS-460. The other is that the command line reaches the engine only through the workspace.

## 0.1.163

No anonymous namespace is left in the engine but App.cpp's, which DS-409 removes (DS-477): each hidden helper is a
member of the class it serves (Api, Request, IrDocument, render::Layout, CurvedRouter, scene::Projection, Theme and
the theme's writers), and DriftSpec's design names each. Nothing ds writes changes.

## 0.1.162

- The editor and the live viewer attach a finding to the entity it is about (DS-426, phase c). The language server publishes each diagnostic's subject as `data.subject`, and hover on an entity lists its own problems, wherever they are written. In the live viewer a finding frames its entity when clicked, and the selection filter keeps the findings about what is selected. The static `ds render` page is unchanged. (DS-488)

## 0.1.161

- The Python face's native `Workspace.run(name, request)` hands `(outcome, object, message)`: the message is what the outcome says (for instance "no diagram named CL_Nope"), "" for the default. `driftspec.Workspace` returns what it returned before. (DS-479)

## 0.1.159

A table is placed by one engine of the layout too (DS-464, step 4), as the Owner approved from the before/after
renders: `layout::GridEngine`, behind `ITableEngine` and named by `EngineFactory::table`, places a `TableGraph` (the
corner, the headings, the rows, each text as its words in characters) with the SVG's proportions, so a cell longer than
40 characters wraps between words in every sink, and a column takes its room each side. One `TableComposer` composes a
table for SVG and for a terminal; a terminal's marks ("! " against the machine, "·" empty) are measured as written.
SVG tables are the same, byte for byte; text and cells renders of tables change once, and the goldens are re-pinned.
The grid's colours are string views, so no cell builds a string to say its colour.

## 0.1.158

The test of the speed budget (a hundred classes checked and drawn in under a hundred milliseconds,
CST-00001-1) skips itself in the evidence build, which is unoptimised, instrumented and runs its tests
side by side: a time measured there says nothing of the product's, and a budget it missed failed the
evidence job. The tests' build still measures it (DS-480).

## 0.1.157

- Every check names the entity its finding is about, as the rules have since DS-426: the report (`ds check --json`) and the export write `subject` on findings of structure, links, behaviour typing, contracts, failures, dependencies, levels, diagrams, state machines and binding, only when there is one. A finding about a file or a word still names none. Messages and the text report are unchanged. (DS-478)

## 0.1.155

render's drawings place nothing, written as a requirement held by rules (DS-464, step 3): Svg and Text depend on no
composer, on neither render's box projection nor the sequence projection, and on nothing of the layout module, so they
reach what places a diagram only through IComposer, from ComposerFactory. Two new definitions, `draws(c)` and
`places(x)`, name them. Nothing a design writes changes.

## 0.1.154

The Python tests' waits for a server, an answer or an event bound a hang, not a speed: two minutes for the
language server's answer and the event stream, a minute for an HTTP answer or for what a test waits to come.
On a loaded machine (CI's tests side by side) the old bounds, from five to twenty seconds, tripped a server
that was only slow, and a passing test waits no longer for them (DS-481).

## 0.1.153

- `ds render <ws> -f html` carries a walkthrough's cards and the mode that plays them again, as the engine's page does; `ds render <ws> <D> -f svg` draws through the one path the cards take. Both were lost when DS-470 met the command line's move into its own module. (DS-483)

## 0.1.152

A state machine's transitions and rules are kept as records of the model (DS-384), one per transition and per rule,
read once from the syntax, not joined into text that machine split again. What a designer writes does not change. The
export writes them as lists of records in the state's fields (transitions: from, to, event, guard and action when
written; rules: state and its literals), and the metamodel declares both as records, so ds manual state says so; a
reader of the export gets data instead of a string to split.

## 0.1.151

Text sequences are placed by the layout's sequence engine too (DS-464, step 2), as the Owner chose: one
`SequenceComposer` composes a sequence for SVG and for a terminal, the same proportions in pixels or in cells, so a
terminal's sequences now space their lanes, rows and frames as the SVG does. A sink states the least room its marks
need (`layout::SequenceLeast`, from `render::Metrics`): a terminal's boxed head and its loop of a message to its own
lane take three rows, a head's label a border and a space each side, a frame's edge a row of its own. Text and cells
renders of sequences and tours change once, and nothing else does; the goldens are re-pinned.

## 0.1.150

- The viewer plays walkthroughs (`ds render <ws> -f html`, the "walkthroughs" button, or a page opened on a walkthrough). Every diagram of the workspace is a card drawn exactly as `ds render <ws> <D> -f svg` draws it, and the one a step shows grows calmly into the stage. The step's focus is framed and the rest dimmed, its highlight is marked, and the coach speaks beside what it points at without covering what is lit, or as a sheet on a phone. A sequence step hands its behaviour to the tour replay. A page whose workspace has no walkthrough is written as it was. The scene's walkthrough steps now give what they light by identity, so a requirement is lit too. (DS-470)

## 0.1.149

DriftSpec's own design now holds three of the service layer's decisions as rules (DS-476), and no engine code changes.
A use case knows no door: no class of `service` depends on or calls anything of `api`, `app` or the Python face
(`use-cases-know-no-door`). Only `UseCaseFactory` names a use case: a door's class that names one is told by the
convention of the families, and a use case that names another by `no-use-case-knows-another`. Neither `api` nor
`service` declares a function (`api-operations-belong-to-classes`, `service-operations-belong-to-classes`). The
requirements are REQ-00207-1, REQ-00208-1 and REQ-00209-1, at draft, and `test_service_principles` proves each rule
with a deliberate violation.

## 0.1.148

- The manual gains a walkthroughs guide (`ds manual walkthroughs`): a worked example, what a step says, what the check tells and how to read one; `examples/shop` holds `WALK_Buy`, from the shop's promise to its classes. A step that shows a scenario written apart (`sequence X / s`) is no longer told W-SCENARIO-UNDECLARED. (DS-472)

## 0.1.147

A sequence is placed by one engine of the layout (DS-464, step 1): `layout::StackedEngine`, behind
`ISequenceEngine` and named by `EngineFactory::sequence`, places a `SequenceGraph` (lanes, messages by row, frames)
into a `SequencePlacement` with a `SequenceSpacing` made from the sink's measures: proportions written once, across in
characters and down in lines, so the SVG's 8 px, 18 px and its small text's 7 px give exactly the pixels it always
had. render projects the IR into the graph (`SequenceProjection`) and the SVG composes its sequences through the
engine. The theme's measures say a character of small text. Every drawing is the same, byte for byte; text sequences
move to the engine in the next step.

## 0.1.146

The ds command line is a thin adapter over the use cases (DS-409), and nothing it prints or exits with changes.
`ds::app::CommandLine` owns the one CLI11 application, so the parse, the help, `--version` and the footers stay as
they were. Each verb is an `ICommand` (`CheckCommand`, `ExportCommand`, `RenderCommand`, `QueryCommand`,
`ManualCommand`), and `CommandFactory` names them. A verb opens a `Workspace` and runs its use case into the sink of
the technology asked for. `CommandLine` turns the outcome into the exit code and writes what it says on stderr.
`service::Outcome` now carries a `message`: `unknown-question`, `no-diagram`, `no-topic` and `errors` say what
happened. `ds manual` as text now goes through learn-language, whose page gains a presentation half. Check-design
names the workspace given, where its report says the open coverage is listed. Reading the disk is `app::Disk` and
writing a document is `app::Output`. `main` is app's only function, and a rule of the design holds it so. The free
functions of `ds/app/App.hpp` are gone.

## 0.1.145

A use case's outcome may say the failure it is the end of: `outcome bank-down fails BankUnreachable: nothing is paid`. A failure escaping the use case's scenario that an outcome names ends there, no longer a failure undeclared (DS-467). Two new warnings: W-OUTCOME-FAILURE-UNKNOWN for a failure the workspace names nowhere, W-OUTCOME-FAILURE-TWICE for two outcomes of one use case on the same failure.

## 0.1.144

- `ds check` tells what a walkthrough's steps get wrong, at the step: a name that resolves to nothing (W-WALK-UNDECLARED), an entity the shown diagram does not draw by its own filter (W-WALK-NOT-SHOWN), a callout pointing at what the step does not light (W-WALK-AT), a scenario the shown behaviour lacks (W-SCENARIO-UNDECLARED), and a walkthrough with no step or a step with no show or text (W-WALK-EMPTY). (DS-469)

- Walkthroughs reach every reader: `ds export` and `ds render -f json` (the scene) give each walkthrough with its steps in order, the export by identity and the scene by diagram name and node index, with the behaviour a sequence step replays; `ds query <ws> walkthroughs [WALK_X]` lists them and the steps of one. A workspace without walkthroughs is written as it was. (DS-471)

## 0.1.143

A class, an interface or a module may declare two operations of one name with different parameters (DS-455). Each is
then named by its parameter types as written, Factory::of(Scene) beside Factory::of(Report, int); one alone with its
name keeps its bare name. A behaviour names the overload it is of by its types: sequence Factory::of(Scene) { ... }. A
call is narrowed by its arguments to the overloads it could be a call of; a call two still match, or a behaviour that
names an overloaded operation bare, is W-NAME-AMBIGUOUS and relates nothing. `(` after a sequence's subject is new
syntax; nothing written before changes meaning.

## 0.1.142

A sequence is composed below its drawing too (DS-400): `SequenceSvgComposer` and `SequenceTextComposer` place its
lanes, rows and frames as each technology always has, into a `SequenceLayout` the figure carries, and the drawings
read it and place nothing. Every drawing is the same, byte for byte.

## 0.1.140

Each kind of the language says what it is part of (DS-427), in the Owner's five categories: structure, artefact, behaviour, logic and view. The export and ds manual <kind> --json write it with each kind, and the scene carries it for the viewer to group by. The editor reads it rather than tables of its own: what a line is written in is the nearest declaration above it whatever its kind, so an operator, a state, a rule and an outcome line find themselves.

## 0.1.139

The drawings take the render IR as a document, not as text they parse back (DS-458). The svg and text writers and the
layout and cells descriptions are handed the IrDocument; the IR itself is described by the door that asks for it, and
an IR file given to ds render is read once where it arrives. A use case no longer reads back the sink it is lent, so a
sink with no text to give back (a Python one) draws a layout and its cells too. Per pass over ds's own design, each
drawing format no longer describes the IR to text and reads it back: about 27 ms of work gone (25 ms of it the parse).

## 0.1.138

A diagram of kind walkthrough takes a reader through the design in steps (DS-468). Each step is written
`step <title> { ... }` and says which diagram it shows (`show: DIAGRAM`, or `DIAGRAM / scenario`), what it is about
(`focus:`), what it talks to (`highlight:`), where its callout points (`at:`) and what it says (`text:`). Each step is
an entity of the model, numbered in the order written, and what it shows and lights are relations (shows, focuses,
highlights, points-at). `ds manual step` describes it. The check, the page, the text and JSON forms and the manual
section come in the next releases.

## 0.1.137

ds's own design reaches a family rooted in an abstract class through it, as one rooted in a pure interface (DS-465):
format's descriptions, rooted in ADescription, are named only by their factory. Nothing a design writes changes.

## 0.1.136

The Python extension builds again: its sink takes the presentation half of ISink as the JSON sink does, skipping it, and says NotImplemented for a description that built nothing (DS-475).

## 0.1.135

An SVG table measures a cell by its characters, as the terminal's does (DS-466): a non-ASCII character such as ≠ is
one, not the three bytes UTF-8 writes it in, so its column is no wider than it needs and it wraps at forty characters.

## 0.1.134

The Python face takes what a use case describes as Python objects (DS-459): Workspace.model(), report(), ask() and shows() no longer receive JSON text to parse back, through a sink in the native module that builds the dicts and lists as the description writes them. Every caller gets what it got before. Api's check and export go through the use cases too.

## 0.1.133

A box diagram is composed below its drawing (DS-400): `render::Figure` holds the document as it is drawn and where
each node and route lands, made by a composer of its kind and technology behind `IComposer` (`BoxesSvgComposer`,
`BoxesTextComposer`), which `ComposerFactory` names. `Svg::draw` and `Text::draw` draw a figure and place nothing;
the svg, text, cells and layout outputs reach the layout only through their composer. Every drawing is the same, byte
for byte.

## 0.1.132

A text sink writes the report and the answers (DS-399), and nothing ds prints changes. A sink now takes two
halves of what a description says once: the structure (records, fields, lists, values) and the presentation
(paragraphs, and tables of rows). The JSON sink writes the structure and skips the presentation; the new
ds::emit::TextSink writes the presentation and skips the structure, a paragraph as its line and a table as its rows,
the cells joined by two spaces. The report and the answers describe both halves, so `ds check` and `ds query` as
text are their descriptions in the text sink, which emit::SinkFactory names "text"; ReportText and AnswersText are
gone. A description is now abstract, ds::format::ADescription (was IDescription): it says its document through a
private say() and then names it to the sink, and a sink that accepted nothing of its half fails with NotImplemented
("the scene is not written in text") rather than hand over an empty page.

A design written for the release before needs no change.

## 0.1.131

CI runs the tests with the JSON sink's checks on (DS-461): DS_JSON_CHECK=1 checks every sink its builder does not say
of, whatever the build, and cpp-test, golden and viewer-js set it. A Release build leaves the checks off by default,
so a broken sink contract passed CI while a debug build failed. Measured: the suite 67.4 s off, 67.6-69.3 s on.

## 0.1.130

The render IR is written by the streaming sink (DS-418). Each projector makes the diagram's IR as a typed document
(render::IrDocument), IrExport heads it, and one description, format::IrDescription, writes it into the sink its
door lends, every key in byte order with nothing kept back. The IR's bytes are unchanged; nlohmann stays only where an
IR is read back (Ir.cpp's parse).

## 0.1.129

In the Python face, driftspec.render draws the format it is asked for: cells and layout no longer come back as a
text drawing, and a format a diagram has no writer for raises driftspec.NotImplemented. The command line is
unchanged.

## 0.1.128

render's architecture is written as requirements held by rules (DS-372): every operation of render is a member of a
class; a projector is reached only through IProjector, from ProjectorFactory, by every class of render, the tour and
the export included; and the drawings read the render-IR alone, knowing nothing of the model, the query or the axis.
A new definition, `projects(c)`, names what projects a diagram. Nothing a design writes changes.

## 0.1.127

test_service passes in a Debug build too: its helper takes a use case's result only when the use case wrote one, as an outcome such as no-diagram is returned, not written (DS-462).

## 0.1.126

The workspace lives as long as its caller (DS-408): made over the use cases it serves, it takes files with reload, parsing again only what changed, and runs each use case by name, checking only the steps the use cases it ran need; one that needs more binds the files again, never parsing them again. The doors keep their workspace, now Workspace::madeFor, checked at once for what they read, until DS-409.

## 0.1.125

The SVG drawing keeps its helpers as members (DS-371): what drew nodes, edges, markers, members, sequences, tables
and the parts of a drawing are private members of `Svg`, the marker table and the sequence's script its private
constants; what an edge is drawn as and a sequence's lane are the dataclasses `EdgeMarkup` and `Lane`. render's design
names no free function any more. Every drawing is the same, byte for byte.

## 0.1.124

The Owner approves the drafts of format's descriptions, layout's rules, the finding's subject and the
curved routes (13 artefacts); VER-00131-2 traces the closed lists' test. The manual says that
`relations` answers carry their two ends in JSON. Nothing a design writes changes.

## 0.1.123

The terminal drawing keeps its helpers as members (DS-371): what drew boxes, containers, edges, members, sequences,
tables and the parts of a drawing are private members of `Text`; the character grid is the class `Grid`, with its
`frame` and `clear`, over the dataclasses `Cell` and `SeqLine`. A node's badge letter and whether a section holds
members are `IrNode::badgeOf` and `IrNode::holdsMembers`. Every drawing is the same, byte for byte.

## 0.1.122

The engine gains a service layer (DS-407): each use case of DriftSpec is a class behind one interface, IUseCase, made by UseCaseFactory, which describes its document into the sink a door lends it and ends in an outcome its use case declares. No door uses it yet (DS-409); what each describes is byte for byte what its door writes. REQ-00085-2: the use case a load serves chooses its check steps too.

## 0.1.121

Each document ds writes describes itself once, and each technology is a sink (DS-398). The scene, a check's report,
a query's answers and a diagram's layout and cells are descriptions (ds::format::IDescription), named by
DescriptionFactory alone; the sinks are named by emit::SinkFactory alone, by the technology's name; whoever asks for
an output pairs the two, so a new document or a new technology is one class and one maker. What is no description
yet (the report and the answers as text, the drawings, the IR and the viewer's page) is written by the writers
FormatFactory names. A pair ds does not write fails with NotImplemented, naming the document and the technology:
the command line says it as bad usage (exit 2), the browser answers it as its error. Rules of ds's own design keep
it so, each proven by a deliberate violation. No output of the command line changes.

In the Python face, Workspace.render with a format it does not know raises driftspec.NotImplemented (a
NotImplementedError naming the document and the technology) instead of drawing text.

A design written for the release before needs no change.

## 0.1.120

The viewer's tour test of ds's own design has the time it needs (DS-457): nearly all of it is ds loading and checking
its design (about 2.3 s idle), which went past vitest's 5 s under load; the test now allows 30 s and says why.

## 0.1.119

A curved edge's label clears the states by its whole text, not only the point it is set at (DS-454): a label that
fell just under a state no longer sits on its border.

## 0.1.118

Render's export draws each kind of diagram through a projector of its own behind one interface (DS-369):
`IProjector`, with `ClassProjector`, `SequenceProjector`, `TourProjector`, `StateProjector`, `TableProjector` and
`RequirementProjector`, which `ProjectorFactory` names by the diagram's kind. `IrExport` keeps the choice, the head
every document opens with and the writing; the sequence projector's builder is a class of its own. A tour composes
its two parts through the factory, without writing them out and reading them back. Every IR, SVG and text is the
same, byte for byte.

## 0.1.117

The grammar regenerates in place again (DS-450): `grammar/package.json` keeps `grammar.js` CommonJS, as
`tree-sitter generate` reads it, under a repository whose root package.json is an ES module. CONTRIBUTING.md says how.
Nothing ds does changes.

## 0.1.116

A finding names its subject (DS-426): the identity of the entity it is about. The report (`ds check --json`) and the
export write it as `subject`, only when there is one. A rule's or a definition's findings name the rule, a breach
included (the breach carries the entities it binds); a finding about a file or a word names none. The other checks
name their subject in later releases. Messages and the text report are unchanged. In Python, `Finding.subject`; the
live viewer's rules panel attaches a rule's problems by it, no longer by where they are written.

## 0.1.115

A state diagram's transitions are smooth curves (DS-443): through the points the layering leaves, from border to
border, two between the same states bowed apart, a transition back to the same state a loop beside it with its
label beside the loop, and every label on its own curve. Every render-IR document now says its diagram's kind
(`"kind": "state"`, `"class"`, ...), and the drawing routes by it; a reader of the IR gains one key and loses
nothing. Other diagrams, and every terminal drawing, keep their orthogonal routes.

## 0.1.114

The rule evaluator's solver backtracks in place: it binds each answer into one working binding, continues and undoes it, instead of copying the bindings for every candidate, and a negation stops at its first answer. ds check is 5–7% faster on designs with many rules; every finding is as before (DS-401).

## 0.1.113

`ds query <workspace> relations <id> --json` says each relation's two ends (DS-445): every answer gains `from` and
`to`, the identities as the model holds them, beside `id` (the other end) and `detail` (the arrow), which stay as they
were. No other question's answers change, and the text output is as it was. In Python, an Answer of a relation carries
them as `source` and `target`; every other Answer has them empty.

## 0.1.112

The Owner approves the drafts of the night's fixes and features (20 artefacts: format's JSON sink, final
states, coverage, query's subjects and deep listing, render on a file that does not parse, the viewer
page's size held on a fixture, table columns, the attribute page); VER-00218-2 traces the factory
exemption's test. The manual's "Asking the model" table says what entities, findings and coverage take
now. Nothing a design writes changes.

## 0.1.111

A diagram's show: and hide:, and an interface's kind:, take only the values ds knows, and any other is a warning, W-FIELD-VALUE,
naming the value and the ones it takes (DS-435): show: ownership, multiplicity, operators, notes, requirements,
derived-stereotypes, design or verifications; hide: fields or methods; an interface's kind: cli, gui, http, library,
lsp or mcp. Each name of a list is checked on its own. `ds manual diagram` and `ds manual interface` list them. The
counts line says one in the singular: 1 finding, 1 relation, 1 entity.

What a design written for the release before may see: a value no drawing ever read (show: contracts, show: returns)
is now said.

## 0.1.110

The layout module's architecture is written as requirements held by rules (DS-392): the layout depends on nothing
of render or the scene, as it already depended on nothing of the model, and whoever places reaches an engine only
through IEngine and a router only through IRouter, from the factories that name them: neither the scene nor render
holds an engine, a router or a phase of the layered engine. A new definition, `engine-part(x)`, names them. Nothing
a design writes changes.

## 0.1.109

`ds query <workspace> entities shop@*` lists everything a scope holds at any depth, one line each, as a diagram's `include: shop@*` takes it; `entities shop` still lists one level. A pattern that takes nothing is said, exit 1 (DS-444).

## 0.1.108

`ds manual attribute` no longer says an attribute uses its type (DS-439): an attribute owns, refs or shares it, as the
model records. A rule that reads `uses` off an attribute is now W-RULE-NEVER-MATCHES; read the field through `owns`,
`refs` or `shares`, or a definition such as `depends(c, x)`.

## 0.1.107

The layered engine's phases are classes of their own (DS-392, DS-370): the layerer assigns a component's layers,
the orderer orders them to cross less, the positioner places them, the framer lays out the frames and packs their
components, and the edges are routed by a router the engine is handed; the routers are named by a factory, the
orthogonal one today. Nothing a design writes changes: every drawing is the same.

## 0.1.106

A column of a table reads what the entity of its row is as a rule reads it (DS-437): `column is: kind(r)`, `name(r)`,
and `id(r)`. Such a line used to be refused with an E-SYNTAX.

## 0.1.105

ds render no longer draws a diagram from a file it reads that does not parse: a syntax error in the diagram's file, or in the file of what it shows, is written as ds check writes it and the render exits 1, where it drew a blank or an empty table and exited 0. A diagram that is missing or shows nothing while a file does not parse says that may be why. A broken file the diagram does not read stops nothing (DS-433).

## 0.1.104

A rule broken through a definition names what the definition went through: `never depends(c, x)` reports `broken for c = Shop: x = Store (depends: m = Shop::store)`, as a direct `m uses x` names m (DS-438).

## 0.1.103

A factory is exempt from reaching a family through its interface by its name, ...Factory, and not only by naming two
classes of the family (DS-451). The factories of one product (SinkFactory, QueryFactory) and EngineFactory now name
what they make in the design, as the code does. Nothing a design writes changes.

## 0.1.102

ds query says which subject each question takes (in --help, from the table its check reads) and answers a subject of another kind with an error that says what it takes, exit 1, instead of printing nothing: `artefacts REQ-…` says to ask `relations`. `findings` and `coverage` take a file or an identity, the entity and everything it holds (DS-436).

## 0.1.101

A use case whose scenario lets a failure escape gets W-FAILURE-UNDECLARED at the scenario, no longer W-FIELD-UNKNOWN for `escapes` and `escape.<Failure>`, fields the check writes itself. `ds query failures <use case>` answers what escapes it (DS-434).

## 0.1.100

What keeps the viewer's page small is held on a design made for it (DS-452): a behaviour called from many places costs
the page once, so a caller adds its own call and never the steps its tour replays. The bound of 250 KB on ds's own
design is gone, a number on a design that grows. Nothing a design writes changes.

## 0.1.99

The layout's engines number the boxes of a graph in the order it is read and give them back in that order (DS-392):
an edge names its ends by number, and whoever places reads every box back without looking it up by name, which
takes back what going through the engine interface had cost render's drawings. Nothing a design writes changes.

## 0.1.98

ds export is faster and lighter, and nothing changes in any output (DS-397). Every JSON document ds writes but the
render IR (the export, the scene, the report, the answers, the layout and cells of a diagram, the manual's page, the
engine's answers to the Python face and the browser) is now written by one streaming sink, ds::emit::JsonSink, in
place of nlohmann's tree: byte for byte the same, keys in byte order, written whole or not at all. The macOS wheel
needs macOS 13.3 or later: it is the first whose C++ library writes a double with std::to_chars.

A design written for the release before needs no change.

## 0.1.97

A state machine says its final states (DS-442): `final <State>, ...` inside a state block. A transition out of a final
state is W-STATE-LEAVES-FINAL; a state with no way out is not taken as final. A state diagram draws where the machine
starts as UML's filled dot, each final state with an arrow to a bullseye, and its states as capsules; the terminal
draws ●, ◉ and rounded boxes. The box that said "start" is gone: in the render-IR, a machine's start is a node of kind
initial, its ends nodes of kind final and its states nodes of kind state, so a reader of the IR that looked for the
classifier initial on a box reads the kind instead. `final` is a new word of the language: a design that used it as a
name renames it.

## 0.1.96

Nothing a design writes changes. CI keeps a compiler cache of 12 GB instead of 5, so a merge request
recompiles only what it touches, and each C++ job prints the disk the cache leaves (DS-449).

## 0.1.95

Render's drawings are placed by the layout module's layered engine (DS-392): render projects a diagram's IR into
nested boxes and the edges between them, and reads the places and the routes back. The engine is the one render
had, moved as it was. Nothing a design writes changes: every SVG, text, ir and layout drawing is the same.

## 0.1.94

The Owner approves the roadmap's drafts of 2026-11-24: the rule language, encoding, the theme and its
constraint, layout, the editor, the viewer's severities, coverage of what nothing realises or verifies,
and their verifications and the Owner's conventions as engine-wide rules (35 artefacts). REQ-00194 and REQ-00195 are revised to name StepFactory, the
name the steps' factory took. Nothing a design writes changes.

## 0.1.93

One palette for every drawing and page (DS-441), as the Owner chose it: each kind one colour, light and dark. Class
indigo, interface rose, enum green, dataclass amber, function slate and abstract violet, in the SVG's badges, the
viewer (day and night), the playground and the terminal (class blue, interface magenta, enum green, dataclass yellow,
function dim, abstract magenta). A dataclass has its own badge colour now, and a requirement, a verification and a
constraint keep theirs, apart from the kinds'. The viewer and its night theme set two more variables, --k-interface
and --k-abstract; a theme of your own that the viewer reads should set them too. What a design writes does not change;
what it draws changes colour on purpose.

## 0.1.92

Nothing a design writes changes. Main's tests build and pass again (DS-448): the test of the scene's placement builds
its checker the way the steps are built since DS-425, and the golden text report of ds's own design gains the open
coverage line that DS-431 writes, now that DS-429 and DS-430 hold two of its requirements as open coverage.

## 0.1.91

The Owner's design conventions are rules of ds's own design (DS-405): a pure interface is named I and a capital, an
abstract class A and a capital, and a factory its product and Factory; a class of a family is reached through the
family's interface by every class but its own family and the factories; and what a class refers to is given to its
constructor, itself or inside what it is given. Each rule is proven by a deliberate violation. Nothing a design
writes changes.

## 0.1.90

The manual teaches what five first-time users had to find out by trial (DS-440): how every kind of
link is written, the level of a requirement, the verification of a constraint, what a table's
column may hold, the full chain from a use case to its scenarios, what a `fails` clause promises,
a class carrying out an interface of its module, what each relation of a rule sees and the trap
of `uses` against an attribute, definitions, the findings that catch a rule speaking of nothing,
coverage and the counts line, asking the model with `ds query`, and adding to a design you did not
write. The list of what this release does not do no longer says that pointers, template bases and
Python unions cannot be written: they can.

## 0.1.89

An approved requirement that nothing satisfies is coverage (DS-429): `W-REQ-UNREALISED`, listed by `ds query <workspace>
coverage` and counted as open by `ds check`, never a finding. A requirement counts as satisfied when something names it
with `@requirement`, or when every requirement that refines it is satisfied, directly or through its own refinements. A
draft owes nothing.

An approved constraint that nothing verifies is coverage (DS-430): `W-CST-NO-VERIFICATION`, as an approved requirement
nothing verifies is `W-REQ-NO-VERIFICATION`. Obeying a constraint is no verification of it. A draft owes nothing.

## 0.1.88

Each step of the check is given what it reads when it is made (DS-425). The checker's context is made once, holds the
resolver and starts over with each run's notes; every step takes it at construction, and its run takes nothing.
CheckerFactory makes, for a selection, the context, the steps made with it and the checker that runs them, and hands
back that whole as the IChecker it is. Nothing a design writes changes.

## 0.1.87

Every page takes its colours from the one theme too (DS-393, second part): the viewer, its night theme, the live
page, the playground and the page of a whole design carry what the theme writes for them between theme markers, and
a colour written by hand anywhere in what DriftSpec ships fails the build. A shadow is a token now. The viewer's
night theme sets four more variables (--you, --shadow, --shadow-pop, --shadow-soft); a theme of your own that the
viewer reads should set them too. Every drawing draws as before.

## 0.1.86

The bind context is given what it refers to by a constructor, and the design declares it (DS-424). Nothing a design
writes changes.

## 0.1.85

The layout is a module of its own (DS-392), pure geometry: an engine is given nested boxes of given sizes and the
edges between them, and says where every box lands and how every edge runs, knowing nothing of what a box stands for.
The scene's packing of shelves, fixed places and trees is its first engine, which the scene reaches through the
engine interface the layout's factory hands out; render's layered engine follows. Nothing a design writes changes:
findings, the viewer, its scene and every drawing are the same.

## 0.1.84

`ds check` says how much coverage is open (DS-431): when a design has open items of coverage, what is not yet written
such as a requirement nothing verifies, the counts line ends with ` · N open`, and a line under it says where they are
listed (`ds query <workspace> coverage`). A design with none reads as before. The exit code never depends on coverage,
and `ds check --json` does not change.

## 0.1.83

The drawings take their style from one theme (DS-393, first part). Every colour of the SVG's stylesheet, light and
dark, the font and how it measures, and the terminal's palette are held once, in `ds::theme::Theme`; the stylesheet
every SVG carries and what a sequence adds to it are written from it, and the terminal drawing colours each role as
it says. Every drawing ds writes is the same, byte for byte. Nothing a design writes changes.

## 0.1.82

A design file that is not valid UTF-8 is now reported with its file, line and column (DS-411). Such a file, one Latin-1
é in a title for instance, used to check clean while `ds export`, `ds render -f json` and `ds query --json` exited with
2 and an empty output, naming no file. Each file's encoding is checked when it is read: a file that is not UTF-8 is the
new error E-ENCODING at its first byte that is not part of a character, on its line and at its column, both counted
from 1 and the column in bytes, as E-SYNTAX counts them (`ds manual E-ENCODING`). As with a file that does not parse,
it is reported once and read no further, the findings that only echo its absence are left out, and every other file is
checked as usual: `ds check` exits with 1, and export, render and query write. The Python face hands the engine each
file's bytes (`driftspec.read` gives a directory's files as bytes, and a mapping may hold bytes or str), so
`driftspec.check` gives the same finding instead of failing in Python first; a mapping of str loads as before.

Valid designs are unchanged: every finding, export, drawing, viewer page, answer and exit code is the same. The manual
gains the page of E-ENCODING and its row in `ds manual codes`.

## 0.1.81

The manual and the README say what DriftSpec's Python face does since today: the language server keeps a design
loaded and how fast its diagnostics arrive, `Workspace.reload` and `driftspec.severities()`, and the live viewer's
colours, findings count, coverage section and the problems in its rules panel.

## 0.1.80

The language server is a door of classes (DS-422): a `DesignServer` owns the protocol and one `Editor` per design, every
feature is a method, and the module keeps no state of its own. The kinds it names (what is structure, what is an
artefact, a kind in an outline, the fields hover says) are written in one place, `Kinds`, until the metamodel gives each
kind its category (DS-427). The language server and the live viewer open the browser the same way, `driftspec.browser`.
What an editor is told and answered does not change.

## 0.1.79

The live viewer's rules panel attaches each rule's problems by where they are written (DS-421): a rule or definition
finding written inside the rule is one of its problems, whatever its message says. They were matched by the start of
the message, which a rewording of the finding would have broken without a sound. What the panel shows does not change.

## 0.1.78

The exporter reaches the diagram filters through IFilter alone (DS-423). What a class diagram draws around what its
patterns take, which entities are boxes and the box a relation is drawn from are read by ClassBoxes, which the class
filter and the exporter both ask; every filter answers boxes(), what its patterns take without what is drawn around.
Nothing a design writes changes.

## 0.1.77

The scene's architecture is written as requirements held by rules (DS-391): every operation of the scene is a member
of the class whose responsibility it is; the scene reads the model it places and draws and leaves it as it is; no
layer knows the layout, so a layer reaches the boxes placed only through the map and the document; and the packer
places boxes knowing nothing of the model. A new definition, `lays-out(b)`, names what places the boxes. Nothing a
design writes changes.

## 0.1.76

The one place that names the concrete classes of an interface is a factory, named so (DS-404). Steps is StepFactory
and Filters is FilterFactory; the node binders are made by NodeBinderFactory instead of the binder's constructor; and
the Api asks CheckerFactory for the checker and holds it as the IChecker it is, so the load names no concrete checker
(the viewer's tour of a load now reaches the check at IChecker::run). Nothing a design writes changes.

## 0.1.75

The language server keeps its workspace loaded and reloads it on a change (DS-420). `Workspace.reload(files)` loads a
workspace again in place, for what it was loaded to give: a file whose text has not changed is not parsed again, and the
whole is bound and checked again. The language server reloads the workspace it keeps on each change and publishes the
diagnostics from its check, and reads the whole model only when a request asks for it. An editor sees the diagnostics of
an edit in about half the time (DriftSpec's design 337 → 175 ms, a design of 3,200 classes 484 → 184 ms); what it is
told and answered does not change.

## 0.1.74

The scene hides nothing (DS-390). The one place that names the concrete layers is a class, `LayerFactory`; the
count of a text's characters is the text module's, which render's cells use as well; and a type is read without the
names that qualify it by the model's `WrittenType`, with a scan instead of a regular expression, for the rows of a
box and for the types an interface's contract compares. Nothing a design writes changes: findings, the viewer, its
scene and every drawing are the same.

## 0.1.73

A diff's change on a relation or a message is drawn as on a box (DS-419). In the text drawing, a changed relation is
marked `~` in its colour and a renamed one `>`; before, every change but `added` was marked `-` in red, as removed. A
renamed message is marked `>` (it was `~`). In the SVG, a changed or renamed relation is drawn in its colour; the
stylesheet had no rule for them, so they looked unchanged. Every SVG ds writes carries the one new stylesheet line. Nothing
a design writes changes.

## 0.1.72

Nothing a design writes changes. From this release on the version goes up when a merge request is
merged, one release per merge, and each merge request writes its entry under `changelog.d/`
(DDEV-1548).

## 0.1.71

The language server and the live viewer show a finding's severity as the severity table gives it (DS-386). The engine
hands the table to Python (`driftspec.severities()`: each severity's name, whether it fails, whether it is kept apart as
coverage, and what an editor shows it as), and no severity is named in Python. The language server publishes each finding
with the table's editor mapping; what it publishes does not change. The live viewer colours each finding by its row,
errors red and warnings amber; its findings pill counts what fails, with the rest beside it; and the coverage, which
reached no Python door, has a folded section of its own in the information colour, fed by a new `coverage` field of the
state. The live contract is version 5.

## 0.1.70

A rule reads what an entity is, matches a value against a pattern and reads a member's flag (DS-403). `name(x)`,
`id(x)` and `kind(x)` are the name, the identity and the kind of the entity bound to x; they read it and never bind
it. `name(c) matches "^I[A-Z]"` holds when the value contains a match of the pattern, an ECMAScript regular
expression compiled once when the rule is checked. `m is abstract` holds when m carries the flag.

What a design written for the release before has to change: `name`, `id` and `kind` are now words of the rule
language. A definition of one of those names is reported (E-RULE-RESERVED) and can no longer be applied, and none of
the three can stand as a variable where a literal begins. A pattern that is no string or does not compile is reported
at the rule (E-RULE-PATTERN). No kind may declare a field named `name` or `id`. Every design that used none of them
checks, draws and exports as before.

## 0.1.69

The Owner approves the roadmap's drafts: check's finding order and architecture (REQ-00193-1..REQ-00195-1 with
VER-00213-1..VER-00215-1), query's (REQ-00200-1, VER-00220-1, VER-00003-7, VER-00037-8) and the live viewer's and files
rule's verifications (VER-00250-1..VER-00252-1). Nothing a design writes changes.

## 0.1.68

A workspace's files are read by one rule, the engine's, wherever Python reads them (DS-413). `driftspec.files()` lists
the .ds files of a folder as `ds` reads a workspace: every regular .ds file in every folder below it, dot-folders included;
a link to a file is followed and a link to a folder is not; a broken link, and a file named only `.ds`, are skipped; sorted.
`driftspec.read()` reads with it, so a broken link no longer stops it and `.ds` is no longer read. The live session's
watcher stamps with it, so a save inside a dot-folder now reaches a new version, as the engine already read it. The
language server finds a folder's own files with it.

## 0.1.67

The scene builds what it places from the model apart from how it is placed (DS-389). A projection reads the model
into nested frames, each package, module, interface, unit and row sized by what it shows, its children in the order
they are read and the room each kind of box asks for; a packer that knows no model turns those frames into boxes:
a frame keeps its size, fills shelves no wider than a target, or centres its first child over the row of the others.
Nothing a design writes changes: the viewer, its scene and every drawing are the same.

## 0.1.66

The check's architecture decisions are requirements held by rules (DS-383). The checker runs each step through IStep
and knows none of the concrete ones, no step knows another, and every operation of the check is a member of a class;
three rules of ds's own design hold them, each proven by a deliberate violation. SEQ_Load draws a load from the command
line two levels down. Nothing a design writes changes.

## 0.1.65

The contracts are checked in five steps, and the order of what the check reports no longer follows its steps
(DS-382). ContractCheck becomes InterfaceContracts, UseCaseContracts, DiagramCheck, ScenarioCheck and FailureCheck,
each run on its own and chosen on its own (`interfaces`, `usecases`, `diagrams`, `scenarios`, `failure-check`); the
findings each one says are the ones ContractCheck said.

An intended change of output: the findings and the coverage are both ordered by file, then line, and on one line by
code, then message. The coverage was never sorted and came out in the order the steps ran in, so a design whose
coverage spans several steps now reads it by where each entry is written. On the 28 workspaces of the comparison this
reorders the coverage of three (the audit and levels corpora and the repository) and two pairs of findings on one
line; nothing is added or removed, and the golden files do not move.

## 0.1.64

Every operation of the query is a member of its class (DS-385). A pattern of a diagram's include or exclude is read
once, by Pattern, which then takes or leaves each entity by comparing its identity and scope with what it read, with
no text built for each one. The one place that names the filter of each kind of diagram is the class Filters, where it
was a function, with a static of() as check's Steps has (DS-382), and the box a relation is drawn from, with the
helpers the filters hid, are members of the filter they serve. A rule of ds's design holds that the query writes no
function, refining what the query answers. Nothing changes for a design: every finding, export, drawing, viewer page,
answer and exit code is the same.

## 0.1.63

The golden ctest compares without pytest (DS-415). `ctest` ran the golden files through `python3 -m pytest`, so on a
machine whose python3 has no pytest the test failed for that reason alone. `tests/system/python/test_golden.py --check
--ds <ds>` makes the same comparison as a plain script and exits 1 when a document differs; ctest runs it that way and
needs only python3. CI's golden job still runs it under pytest, for its report.

## 0.1.62

A live event carries what it was sent with (DS-414). Each event shared the session's own notes, highlights and comments,
and a page serialised them when it took the event off its queue, so an event could show a change made after it was
sent (an annotate event for a clear, carrying the comment written next). The events now carry a copy taken when they
are sent, one for every page. Nothing else a page reads changes.

## 0.1.61

The live session is split into its parts, and its contract is declared once (DS-412). `Live` holds the version shown
and composes `Annotations` (the notes, highlights and comments, kept in `.driftspec/live.json`), `Events` (the pages
that follow it) and `Watcher` (the .ds files it watches). The contract is one table in `driftspec.live_contract`: each
operation with its route, what the session answers, its MCP tool and its command word. The HTTP routes, the MCP tools and
the command line are read from it, and a test checks that they agree. What a client sees does not change: GET /contract,
the state, the events, tools/list and the command line are the same, byte for byte (contract version 4).

## 0.1.60

The viewer orders the boxes of a package in one pass over the inheritances (DS-388). To put a module whose classes
are inherited from before the modules that inherit, the layout asked of every pair of boxes, again for each box it
placed, whether one inherits from the other, by walking every inheritance of the model; on a design of a few
thousand classes that was half of the layout and a sixth of `ds render -f json`. Each inheritance is now read once, as the boxes it
joins, and the boxes are taken in the same order as before: the first one written that waits for none of those
left, and the first one left when they wait for each other. Nothing a design writes changes: the viewer, its scene
and every drawing are the same.

## 0.1.59

The Owner approves the roadmap's drafts: the evaluator's requirement and its verification (REQ-00192-1,
VER-00212-1), the render steps' verification (VER-00094-3), the viewer's compression fixture (VER-00083-4) and the golden
outputs' system verification (SVER-00029-1). Nothing a design writes changes.

## 0.1.58

The check names its steps in a class, and its steps keep no hidden helpers (DS-382). The free function that made the
steps a selection asks for is `Steps::of`, the one place that names the concrete steps. What three steps did in
functions hidden in their files is now theirs: CallOrder says the states a call could come from, ContractCheck tells
whether a pattern names something declared, and LevelCoherence finds the operation a message calls, the behaviour below
an entry, the crossings below it and where each one said finds its place. The crossings and what lies below an entry are
written in the design. Nothing a design writes changes.

## 0.1.57

Every format of every document ds writes is pinned as a golden file (DS-395), before the formats are refactored
(DS-394): the scene as json and html, each diagram as ir, layout, svg, text and cells, the report and the answers as text
and json, and the export, over three frozen workspaces: a copy of ds's own design, a copy of the hola example with a table
and a tour so that every kind of diagram is drawn, and a small workspace with findings. ctest and a CI job of their own
compare them with what the ds just built writes; `python tests/system/python/test_golden.py --ds <ds>` writes them again,
a change reviewed like any other. Nothing a design writes changes.

## 0.1.56

The machine is written as requirements held by rules (DS-367): every operation of the machine is a member of the class
whose responsibility it is, and the machine reads the model it reads a state machine from and leaves it as it is. What
its source hid is now classes of the machine: the lists the binder writes into a field (`WrittenFields`), a condition
of a state's rule (`Literal`), the rules and the jumps of a machine (`StateRule`, `Jump`), what its subject holds
(`Subject`), what each name of a context may be worth (`Context`) and the walk of the behaviours for the calls of its
events (`Calls`). Nothing a design writes, no finding, export or drawing changes.

## 0.1.55

The viewer's test that a behaviour is written once and replayed by many tours runs on a fixture made for it (DS-377):
one behaviour called from four places, whose calls and steps are known, instead of a ratio of ds's own design that moved
with every change to it. ds's design keeps the tours that name its entities. Nothing a design writes changes.

## 0.1.54

The rules are evaluated with far less copying (DS-381). The solver bound each variable in a map of strings and copied
the whole map, strings included, for every candidate it tried, and climbing what holds what copied a string per step.
Each formula's variables now get fixed slots once, and a binding is a vector of pointers to texts that the evaluation
owns and that never move: the facts, which copy what holds what from the model's index once, and a few texts the solver
keeps itself. A candidate copies a few pointers. Subjects, witnesses and messages are ordered exactly as before.
Nothing a design writes changes: findings, breaches and tables read the same; `ds check` on a design with rules spends
roughly half the instructions it did in the evaluator.

## 0.1.53

The exporter is written as requirements held by rules (DS-367): every operation of the exporter is a member of the
class whose responsibility it is, and the exporter reads the model it writes and leaves it as it is. The helpers
JsonExport's source hid (a location, an entity, a relation, the names of kinds and the metamodel as JSON) are its
private members. Nothing a design writes, no finding, export or drawing changes.

## 0.1.52

The evaluator of the rules is five pieces (DS-380). `Facts` reads the model once into what a rule is evaluated over:
the entities of each kind, every relation both ways, the names a formula writes resolved, and what each definition holds
for; it is the pieces' one reader of the model. `RuleValidator` checks each formula before it is evaluated, `Strata`
orders the definitions, and `Solver` answers a body over the facts and never the model. `Evaluator` directs them: it
records into the facts what the validator resolved and what the definitions derive, and writes the breaches and the
tables into the model. The design of ds holds the solver to the facts with a rule. Nothing a design writes changes:
findings, breaches and tables read the same.

## 0.1.51

A drawing loads only the steps of the check it reads (DS-376). `ds render` to svg, text, cells, ir or layout, and the
engine's render that Python and an editor's preview call, run the linker, behaviour typing and derivation, and the rules
as well when the diagram is a table written as rows and columns. The viewer (html, json), check, export and query run
every step, as before. The Api chooses the steps from what the command gives, in one place, and a rule of ds's design
holds that decision there. Drawing CL_Ds takes about half the time: 243 to 111 ms on DriftSpec's design, 577 to 252 ms
on the whole repository. Nothing changes for a design: every drawing, finding, export, viewer page and exit code is the
same.

## 0.1.50

The check finds what each class declares through the index of its members (DS-375). To derive a class's
stereotypes (abstract, value and the operator families) it read every entity of the model, once per class, so the
time grew with the square of the design: a synthetic design of 3 200 classes spent most of its check there. Each
class now reads only its own members, the same ones in the same order. Nothing changes for a design: no finding,
export or drawing.

## 0.1.49

The Owner approves the requirements DS-379 left as drafts, with their verifications (every finding is made from the
catalogue; nothing else writes a severity: REQ-00190-1, REQ-00191-1), and the viewer-tours verification VER-00082-3.
Nothing a design writes changes.

## 0.1.48

A finding is made from its code (DS-379). Seventy places wrote a finding's code and severity as two strings, which
agreed with the catalogue only by care; each now asks the metamodel for the finding of a `Code`, and the catalogue
gives the code as it is printed and its severity, read from one slot. A code the catalogue does not hold is no `Code`
and does not compile. Severity is a value whose meaning is a row of the metamodel's severity table: its printed name,
whether it fails the check, whether it is kept apart as coverage and what an editor shows it as. The check, the model,
the export, the formats and the manual ask the row instead of naming a severity, and a check's report carries the
table, so a format asks it without the metamodel. A coverage finding is born coverage. The design of ds says so in two
rules: no class outside the model reads a code's row, and only the metamodel and the report hold a severity's row.
Nothing a design writes changes: findings, the JSON export and every drawing read the same.

## 0.1.47

The Owner approves the white-box requirements DS-374 left as drafts, with their verifications: parse knows no model and its
operations are class members (REQ-00176-1, REQ-00177-1); text depends on no other engine module and holds no state
(REQ-00178-1, REQ-00179-1). Nothing a design writes changes.

## 0.1.46

Parse and text keep their own rules (DS-374). Describing a syntax error is a class of its own, `SyntaxError`, and what
the grammar takes in a parse state is `Lookahead`, which the error's hint and an editor's completion now share instead
of each walking the grammar in its own copy; the parser only reads. A parsed file is data: the text a node spans is
read by the tree reader. Nothing reaches for the grammar through a static accessor any more: each file's tree carries
the grammar it was parsed with, the reader resolves its fields from it and the binder builds its table from the first
file. The design of ds says so in four rules, that parse knows no model and writes no free function, and that text
depends on no other module of the engine and holds no state. Nothing a design writes changes: findings, syntax errors,
completions and every drawing read the same.

## 0.1.45

The axis is written as requirements held by rules (DS-367): every operation of the axis is a member of the class
whose responsibility it is, and the axis reads the model it cuts and leaves it as it is. standsAmong and bondOf are
static members of Cut, and so are the helpers its source hid. Nothing a design writes, no finding, export or
drawing changes.

## 0.1.44

The architecture of the binder and the model is written in the design as requirements, each held by rules
(DS-365): the binder binds each construct through a binder of its own, reached through one interface; every binder
writes into the model through the bind context; the metamodel builds its own tables when it is created; and every
operation of the binder and of the model is a member of the class whose responsibility it is. A design that breaks
one is told so with E-RULE-BROKEN. The binder now holds its node binders as INodeBinder, and the scan of a type's
text is TypeToken::scan. Nothing a design writes, no finding, export or drawing changes.

VER-00037-7 (under REQ-00049-3) and VER-00003-6 (under REQ-00003-2), which DS-337 and DS-362 left as drafts, are approved.

## 0.1.43

A large `neighbours:` no longer keeps `ds render` busy for good (DS-362). The walk took every step a class diagram
asked for, each over every relation, so `neighbours: 2000000000` over three classes never ended while `ds check`
said nothing; it now ends at the first step that adds nothing, so a number past what the diagram reaches draws the
reach at once. A `neighbours:` that is no number, a word or one too large for an int, gets the new
`W-DIAGRAM-NEIGHBOURS` at the diagram, as such a depth gets `W-DIAGRAM-DEPTH`, and still adds no neighbours. A large
depth already ended where the calls end. Nothing a design writes draws differently.

## 0.1.42

The test of CST-00001-1, a hundred classes checked and drawn in under a hundred milliseconds, times each command
five times and holds the fastest to the budget (DS-363). Load on the machine, other pipelines on a shared runner, only
ever adds to a run's time, so the fastest run is the work's own; the budget itself does not change. Nothing a design
writes changes.

## 0.1.41

The walk the binder had is gone (DS-357, the end of DS-353): every construct of the language binds through its own
binder, found by the table, and the binder only builds them and binds each file. The requirements the walk still
answered to move to the binders that realise them: the high-level design as packages, a package as a box and its
nesting to the package binder, a module that reopens to the module binder. Nothing a design writes, no finding,
export or drawing changes.

## 0.1.40

Sequences get a binder of their own (DS-356, group four of four): a sequence is the behaviour of an operation, with
its steps, the frames that hold them and an alt in two branches, its messages kept for the checker. Every construct
of the language now binds through its own binder; the walk the binder had takes no kind any more, and DS-357 removes
it. Nothing a design writes, no finding, export or drawing changes.

## 0.1.39

Classes and functions get binders of their own (DS-356, group three of four). A class's members, its methods,
attributes and operators, are bound by a member part, and what a member or a function writes in its body by another,
both shared. The walk the binder had keeps only the sequences, until the last group. Nothing a design writes, no
finding, export or drawing changes.

## 0.1.38

Use cases and state machines get binders of their own (DS-356, group two of four). A use case keeps its signature and
its outcomes; a state machine is still the behaviour of its class, its events and actions kept for the checker. The
walk the binder had keeps classes, functions and sequences until the next groups. Nothing a design writes, no
finding, export or drawing changes.

## 0.1.37

The boxes and the plain constructs get binders of their own (DS-356, group one of four): packages, modules, interfaces,
enums, rules and definitions, and diagrams. What every construct shares is a part binder each binder is given by
reference: the fields, the annotations, an extension block, a type, a signature and a formula. The context goes back
to holding only what a binder writes through. The walk the binder had keeps use cases, classes, functions,
sequences and state machines until the next groups. Nothing a design writes, no finding, export or drawing changes.

## 0.1.36

A number too large for an int no longer stops ds (DS-337). A diagram that wrote `depth: 99999999999` passed
`ds check` and then made `ds render` stop with `ds: stoi`; it now gets `W-DIAGRAM-DEPTH`, the finding of a depth
that is no number, at the same place, and is drawn as one. A `neighbours:` too large for an int adds no neighbours,
as a word there does. A diagram reads such a field in one place, and nothing else a design writes changes.

## 0.1.35

The tree reader reads a field by its id (DS-360). Every field of the grammar is named once, in `parse::Field`, and the
reader turns each into the grammar's field id the first time a field is read; tree-sitter otherwise compares the
field's name with every name of the grammar on each read. A field written wrong is now a compile error, not a child
that is silently never found. Binding takes about a fifth fewer instructions, and nothing a design writes, no
finding, export or drawing changes.

## 0.1.34

Every reader of the model is handed the metamodel by const reference instead of asking the model for it (DS-351). The
checker puts it in the context every step of the check is given, and the evaluator of the rules takes it from there.
The query, the filters that read a cut, the cut itself, the scene with its layout and the layers that name kinds, the
drawing of a diagram and the workspace take it in their constructors from whoever holds it. `Model::metamodel()` is
gone: the model keeps its metamodel for what it declares, relates, renames and covers. The findings, the export and
every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.33

Each construct of the language gets its own binder (DS-353, step two of four, DS-355). A binder says which kinds of
node it takes and binds one of them; a table built once, one slot per symbol of the grammar, finds a node's binder
without comparing a name. The requirements, verifications and constraints are the first with a binder of their own;
the other constructs keep the walk the binder had until DS-356 splits it. Every write goes through the context, which
now also reads the fields and the annotations every construct shares. Binding takes a little less work than in
0.1.30, and nothing a design writes, no finding, export or drawing changes.

## 0.1.32

The model keeps its own lists (DS-346). Moving what is not yet written to the coverage, dropping what only echoes a file
that does not parse, sorting the findings and keeping a table are operations of `ds::model::Model`, and nobody else
changes those lists. An entity the model keeps is read only to everyone else: a field is written through
`Model::annotate`, and the body of a class refining an operator through `Model::refine`, so nothing can change what an
entity is indexed by. A rename moves each entity without copying it, and the metamodel says which fields hold an
identity it carries along. `Relation::change`, which nothing set, and `Identity::asScope`, which nothing called, are
gone. The findings, the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.31

The walks of a behaviour are a class, `ds::model::Paths` (DS-343), made over the model whoever asks already holds:
whether a behaviour names a scenario, the steps the path of one takes and who a behaviour runs on. How a path goes
at each frame is its own, in no header. The scope outside a name is `Scope`'s to tell, and the model's internal
helpers, `model::detail`, are gone. The findings, the export and every page of `ds manual` are the same. Nothing
changes for a design.

## 0.1.30

The binder starts its split into one binder per construct (DS-353, step one of four, DS-354). The binding code reads the
syntax tree through a reader of its own in parse, writes only through one context that holds the model and the notes, and
hands the checker those notes (the references, the messages and the events) instead of itself. Nothing a design writes
changes, and no finding, export or drawing changes either.

## 0.1.29

The Owner approves the two requirement revisions DS-342 left as drafts: REQ-00079-2 (the metamodel describes how each kind
is written, the codes and the annotations; the manual presents them with the commands) and REQ-00160-2 (the manual's guides).
Nothing a design writes changes.

## 0.1.28

A class diagram is drawn less tall (DS-350): the gap under a layer keeps a lane only for an edge that turns in it, so
a long edge running straight down its channel takes no row. Eight of DriftSpec's own thirty diagrams come out a
little shorter, as text and as svg; the 400-class synthetic as text goes from 75,227 rows and 2 GB to 20,679 rows
and 0.55 GB. A text drawing that would still take more than about 128 million characters is refused with what it
would take (draw it as svg, or select less) instead of exhausting the machine's memory. Nothing changes for a design.

## 0.1.27

The manual is a class, `ds::model::Manual` (DS-342), made from the metamodel whoever shows the manual already
holds: the command line and the page's engine. It writes every page (the index, the guides, the grammar, a kind, a
code, the commands, the annotations and the codes) and keeps the commands with their exit codes, which are the
manual's and no longer the metamodel's. The guides embedded from `doc/manual` are the Manual's too. The findings,
the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.26

The metamodel is a class, `ds::model::Metamodel` (DS-341): its tables are built once, when one is made where the
engine starts (the command line, the Python module, the page's engine), and it is handed by const reference to
the model, the binder, the checker and the manual; no global function reaches it any more. The annotations the
binder knows are a list of their own, apart from the two rows the manual adds to document any other word and the
use case an operation names. The findings, the export and every page of `ds manual` are the same. Nothing
changes for a design.

## 0.1.25

The text drawing takes a tenth of the memory it took (DS-350): a cell of the drawing holds its character inline
and its colour as an index into the drawing's palette, 7 bytes instead of some 72. A class diagram of 200 classes
drawn as text needs 380 MB instead of 3 GB, and one of 400 classes, which used to exhaust the machine's memory, 2 GB.
The drawing is the same, byte for byte. Nothing changes for a design.

## 0.1.24

The model answers with less copying (DS-345): the relations of one kind leaving or reaching an entity come
without copying the others, an entity's field is read without a copy, and the sides of an alt in a sequence are
told once when the sequence is walked instead of by a walk up for each question. The two sides of an alt are
named in one place, `Identity::branch`. Nothing changes for a design.

## 0.1.23

The metamodel's tables leave the manual's file (DS-340): the catalogue of finding codes moves to
`src/ds/model/Codes.cpp`, how each kind is written and its example to `src/ds/model/WrittenForms.cpp`, and
the annotations to `Metamodel.cpp`. The manual reads the metamodel and the metamodel no longer reads the
manual. The findings, the export and every page of `ds manual` are the same. Nothing changes for a design.

## 0.1.22

The engine's text helpers live in one module, `ds::text` (DS-344): splitting and joining a list, the lines of a
label, trimming, base64 and XML escaping, each once instead of a copy in each module. Joining a list whose
first item is empty keeps the separator after it (`"", a` joined with `,` is `,a`, not `a`). Nothing
changes for a design.

## 0.1.21

The rules evaluate faster (DS-338): applying a definition visits only the tuples that begin with its first
argument when that one is bound, and tests what the bindings fix before it copies them; a stratum of
definitions with no recursion is fixed in one pass, each definition after those it applies. On DriftSpec's
own design `ds check` goes from about 150 to 105 ms, and the whole repository from 810 to 370 ms. The
findings are the same. Nothing changes for a design.

## 0.1.20

A bare name no scope declares is looked up among the classes of its package through an index the model keeps
by package and name, instead of a pass over every entity for each reference (DS-336). On DriftSpec's own
design `ds check` goes from about 330 to 160 ms, and a generated workspace of 3,200 classes from 5 s to under
1 s. The findings are the same. Nothing changes for a design.

## 0.1.19

Artefact names are recognised by a hand-written scan instead of `std::regex` (DS-335), the regular
expressions a contract check and two number fields built on every call are built once or replaced by a
digit check, and six unused `#include <regex>` are gone. On DriftSpec's own design this saves about 5–6 %
of the instructions of `ds check`, `export` and `render`; the findings are the same. Nothing changes
for a design.

## 0.1.18

The model package has no anonymous namespaces (DS-332): each helper is a private member of the one
class it serves, or lives in `ds::model::detail`, declared in an internal header, and the design
draws them all. `holderOf` and `specNamed` are said once in the model for the check and the state
machines. Nothing changes for a design.

## 0.1.17

A file that does not parse quotes the unexpected text cut on a character boundary (DS-334): a
multi-byte character across the cut once left half of it, and `ds check --json` then aborted with no
findings. Nothing changes for a design.

## 0.1.16

The viewer's tours run on vitest (DS-329), so argos reads them as the tests their verifications
name. Nothing changes for a design.

## 0.1.15

Every constraint of DriftSpec's own design names the guide that says why it exists (DS-328): four
guides under `doc/knowledge/guides/` cover the licence and the third-party notices, the hundred
millisecond budget, the engine's answers as copies, and the rules of the language's text. Nothing
changes for a design.

## 0.1.14

The CI gate fails on code without design and on stale evidence (DDEV-1507). Nothing changes for a
design.

## 0.1.13

A bare name that two designed classes of its package share is an ambiguity, and it is said
(DS-325): `W-NAME-AMBIGUOUS` names the name and every candidate where the name is written. It
still relates nothing. A design that wrote such a name, silent until now, gets the warning; qualify
the name with its module (`store::Card`).

## 0.1.12

DriftSpec's own design no longer fakes two type aliases as entities (DS-324): an alias is text,
written where it is used as the code writes it. Nothing changes for a design.

## 0.1.11

The live session's code is split by what each part does (DS-322): `driftspec.live` keeps the command
line and every name a caller imports, beside `live_session`, `live_page`, `live_http` and `live_mcp`.
`driftspec-live` works as before. Nothing changes for a design.

## 0.1.10

The design names the native module `_native`, as Python imports it (DS-321), so the Python face's
design writes what its code writes. Nothing changes for a design.

## 0.1.9

The CI gate diagnoses with the current argos-design-driftspec (DDEV-1493). Nothing changes for a design.

## 0.1.8

argos compares DriftSpec's Python face with its code (DS-320): `python` is in the sources, and the
design of the Python face is written as its code writes it. Nothing changes for a design.

## 0.1.7

A dot in a type qualifies a name as `::` does (DS-319): `store.Card`, written as Python writes it,
relates to the class `store::Card` names, and the text stays as written; an undeclared dotted name
such as `typing.Any` is text, as before. A design written for 0.1.6 needs no change; one that
wrote `store::Card` in a Python type may now write `store.Card`.

## 0.1.6

Types are written as the code writes them (DS-18). A design written for 0.1.5 or earlier has to change:

- `extern module` is gone and is now a syntax error. Delete those blocks: a name the design does
  not declare, in a type, a base, a bound or a call, is text, with no relation and no finding.
- The neutral types are gone: write `std::string`, `void`, `std::vector<Line>` in a C++ box and
  `str`, `None`, `list[Line]`, `Line | None` in a Python one, as the code does. `*args` and
  `**kwargs` are parameter names.
- Multiplicity is no longer read from the type: write `many` (`*`) or `optional` (`0..1`) in front
  of the attribute, beside `refs` or `shares`. Two of a kind are `E-MODIFIER-CONFLICT`.
- A bare name that no scope declares is the one class of that name in its package.
- A base keeps its template arguments (`class SceneJson : IFormat<Scene>`), and a method may take
  its own (`+ each<F>(f: F) -> void`).
- A literal is checked only against a designed class; `W-RESULT-NONE`, `W-MODIFIER-ON-VALUE`,
  `W-PARTICIPANT-UNDECLARED` and `E-REF-AMBIGUOUS` are no longer reported.

`tools/migrate_types.py` does most of the rewrite; check what it wrote against the code.

## 0.1.0 (2026-11-18)

The first public release.

### The language

- Requirements, verifications, constraints and use cases, with `@requirement`, `@constraint`,
  `@refines` and `@verification` linking them to the design. Nesting an artefact in what it is
  about says the same as annotating it.
- The structure: packages and modules with their high-level design, interfaces and the
  operations they offer (`@consumes` for a box that depends on another's interface), classes,
  dataclasses, enums, functions, templates, operators, and relations whose multiplicity comes from
  the type.
- The behaviour: sequences with messages, branches, loops, scenarios, attempts and the failures
  they handle, checked against the operations they call.
- The logic: rules (`for`, `where`, `holds`, `never`, `always`) over the model, definitions, and
  state machines whose states are rules over their subject.
- Diagrams as questions asked of the model: class, interface, sequence, state, requirement,
  table and tour views, each choosing what it shows and at which level.

### The tools

- `ds check`, `ds render`, `ds query`, `ds export` and `ds manual`; `ds --version`.
- A viewer of the whole model in one self-contained HTML page, in three perspectives
  (structure, behaviour, logic).
- The Python package `driftspec` (`import driftspec`), with the language server
  `driftspec-lsp` and the live session `driftspec-live` (also as MCP tools for an agent).
  `pip install driftspec` gives `ds` too.
- A tree-sitter grammar with highlight and fold queries, under the MIT licence.
- Built and tested on Linux, Windows and macOS; the release carries a wheel and `ds` for each
  (Linux and Windows x86-64, macOS on Apple silicon). Not on PyPI yet.
- A site with an editor in the browser, the engine compiled to WebAssembly, twenty guided
  lessons and the guides.

### Known limits

What 0.1.0 does not do yet is listed in the
[using guide](doc/manual/05-using.md#what-010-does-not-do-yet).
