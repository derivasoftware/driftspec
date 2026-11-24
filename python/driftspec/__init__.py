"""driftspec, DriftSpec for Python: the engine in process, its model as typed objects (DS-84, a spike).

    import driftspec
    ws = driftspec.Workspace("design/")          # loaded once, kept in the engine (DS-173) ...
    ws.relations_from("p@m::C"); ws.tree("p@m::C::run::sequence"); ws.ask("coupling")
    ws.render("CL_Model", "text")
    model = ws.model()                       # ... and the whole of it, typed, when that is wanted
    model = driftspec.load("design/")            # the same, for one look: a directory or {path: text}
    report = driftspec.check("design/")
    driftspec.expected_at(text, line, column)     # what the grammar takes there
    driftspec.qualify("p@m", "C")                 # how an identity is written, asked of the engine

A workspace is the design's folder (e.g. design/): every .ds file below it is read, as ds reads it, so a
folder that holds more than the design is read whole, and is slower for it.

The engine answers in JSON it writes itself (the shapes of ds export, ds check --json and
ds query --json); this module only reads it into its own types, and computes nothing the engine
knows, so a tool sees the same thing whether it calls Python or the binary.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Iterator, Mapping, Union

from . import _native

__version__ = _native.version()  # the engine's, as ds --version says it (DS-278)

Source = Union[str, os.PathLike, Mapping[str, Union[str, bytes]]]

__all__ = [
    "Location", "Entity", "Relation", "Finding", "Model", "Report", "Answer", "Step", "Walked", "Frame", "Tree",
    "Workspace", "files", "read", "load", "check", "expected_at", "render", "page", "qualify", "outer", "box_of",
    "package_of", "behaviour_of", "subject_of", "scenario_of", "without_revision", "severities",
]


@dataclass(frozen=True)
class Location:
    file: str
    line: int
    column: int
    end_line: int = 0  # the place right after its last character; 0 when not known (DS-172)
    end_column: int = 0


@dataclass(frozen=True)
class Entity:
    id: str
    kind: str
    name: str
    scope: str
    location: Location
    fields: Mapping[str, str] = field(default_factory=dict)
    flags: tuple[str, ...] = ()
    expected: bool = False


@dataclass(frozen=True)
class Relation:
    kind: str
    source: str
    target: str
    location: Location
    multiplicity: str = ""  # how many a holding relation holds: "1", "*"; "" for the rest


@dataclass(frozen=True)
class Finding:
    code: str
    severity: str
    message: str
    location: Location


def _location(d: Mapping) -> Location:
    end = d.get("end", {})
    return Location(
        d.get("file", ""),
        int(d.get("line", 0)),
        int(d.get("column", 0)),
        int(end.get("line", 0)),
        int(end.get("column", 0)),
    )


def _finding(d: Mapping) -> Finding:
    return Finding(d["code"], d["severity"], d["message"], _location(d.get("location", {})))


@dataclass(frozen=True)
class Breach:
    """One binding that breaks a rule: its variables as bound and, for a never, the answer that breaks it."""

    bindings: tuple[tuple[str, str], ...]
    witness: tuple[tuple[str, str], ...]
    location: Location


@dataclass
class Model:
    """A workspace loaded: every entity by its identity, the relations, the findings, the metamodel,
    and what breaks each rule."""

    entities: dict[str, Entity]
    relations: list[Relation]
    findings: list[Finding]
    metamodel: dict
    breaches: dict[str, list[Breach]] = field(default_factory=dict)

    def find(self, identity: str) -> Entity | None:
        return self.entities.get(identity)

    def of_kind(self, kind: str) -> Iterator[Entity]:
        return (e for e in self.entities.values() if e.kind == kind)

    def named(self, name: str) -> list[Entity]:
        """The entities whose simple name is this one: what a reader means by a bare word."""
        return [e for e in self.entities.values() if e.name == name and not e.expected]

    def relations_of(self, identity: str) -> list[Relation]:
        return [r for r in self.relations if r.source == identity or r.target == identity]

    def in_file(self, path: str) -> list[Entity]:
        return [e for e in self.entities.values() if e.location.file == path]

    # what the metamodel says about a kind, read instead of listing kinds by hand (DS-122)
    def parents_of(self, kind: str) -> list[str]:
        """The kinds a kind may be written in, as the metamodel declares them."""
        return next((list(k.get("parents", [])) for k in self.metamodel.get("kinds", []) if k.get("kind") == kind), [])

    def in_a_box(self, kind: str) -> bool:
        """Whether a kind stands in a package or a module (a class, a function, an interface, a requirement),
        rather than inside another entity (a member, an outcome, an operation)."""
        return bool({"package", "module"} & set(self.parents_of(kind))) or kind in ("package", "module")

    def laid_out(self, kind: str) -> bool:
        """Whether a kind is laid out by a behaviour (a message, a frame) rather than declared: it stands
        only in a sequence or in a frame of one (DS-172)."""
        parents = set(self.parents_of(kind))
        return bool(parents) and parents <= {"sequence", "frame"}

    def named_kinds(self) -> set[str]:
        """The kinds a reference names: targets of a relation of the metamodel that stand in a box."""
        targets = {t for r in self.metamodel.get("relations", []) for t in r.get("to", [])}
        return {k for k in targets if self.in_a_box(k) and k not in ("package", "module")}

    def owner(self, entity: Entity) -> Entity:
        """The entity itself, or what it is drawn inside of when it does not stand in a box: a member's class,
        an outcome's use case, a rule's class through its method."""
        while not self.in_a_box(entity.kind) and entity.scope in self.entities:
            entity = self.entities[entity.scope]
        return entity


@dataclass(frozen=True)
class Report:
    findings: list[Finding]
    counts: Mapping[str, int]
    coverage: list[Finding] = field(default_factory=list)  # kept apart from the findings: what is not yet written (DS-216)

    @property
    def errors(self) -> int:
        return int(self.counts.get("errors", 0))


def files(root: Union[str, os.PathLike], nested: bool = True) -> list[str]:
    """The .ds files of the design's folder (e.g. design/), as the engine reads a workspace
    (Parser::parseDirectory): every regular file whose extension is .ds in every folder below it, dot-folders
    too; a link to a file is followed and a link to a folder is not; a broken link and a file named only ".ds"
    are no file of it; sorted. Only the folder's own files when nested is false."""
    found: list[str] = []
    for here, _, names in os.walk(os.fspath(root)):
        for name in names:
            path = os.path.join(here, name)
            if os.path.splitext(name)[1] == ".ds" and os.path.isfile(path):
                found.append(path)
        if not nested:
            break
    return sorted(found)


def read(source: Source) -> dict[str, str | bytes]:
    """The texts of a workspace: the .ds files of the design's folder (e.g. design/; files() says which) as their
    bytes, or a mapping given as it is, each text a str or bytes. The engine reads the bytes, so a file that is not
    UTF-8 is its finding, E-ENCODING, as it is on the command line, rather than a failure of Python's before the
    engine sees it (DS-411)."""
    if isinstance(source, Mapping):
        return {str(k): bytes(v) if isinstance(v, (bytes, bytearray)) else str(v) for k, v in source.items()}
    root = os.fspath(source)
    if not os.path.isdir(root):
        raise NotADirectoryError(root)
    texts: dict[str, str | bytes] = {}
    for path in files(root):
        with open(path, "rb") as fh:
            texts[path] = fh.read()
    return texts


def _entity(e: Mapping) -> Entity:
    return Entity(
        e["id"], e["kind"], e.get("name", ""), e.get("scope", ""), _location(e.get("location", {})),
        dict(e.get("fields", {})), tuple(e.get("flags", [])), bool(e.get("expected", False)),
    )


def _relation(r: Mapping) -> Relation:
    return Relation(r["kind"], r["from"], r["to"], _location(r.get("location", {})), r.get("multiplicity", ""))


def _model(data: Mapping) -> Model:
    entities = {e["id"]: _entity(e) for e in data["entities"]}
    relations = [_relation(r) for r in data["relations"]]
    breaches = {
        rule: [Breach(tuple(map(tuple, b["bindings"])), tuple(map(tuple, b["witness"])), _location(b.get("location", {}))) for b in list_]
        for rule, list_ in data.get("breaches", {}).items()
    }
    return Model(entities, relations, [_finding(f) for f in data["findings"]], data.get("metamodel", {}), breaches)


def _report(data: Mapping) -> Report:
    counts = {k: int(data[k]) for k in ("entities", "expected", "relations", "errors")}
    counts["findings"] = len(data["findings"])
    return Report([_finding(f) for f in data["findings"]], counts, [_finding(f) for f in data.get("coverage", [])])


# ---- what the engine answers about a loaded workspace (DS-173) -----------------------------------


@dataclass(frozen=True)
class Answer:
    """One line of an answer of ds query: an entity named by identity, with what the question asked."""

    kind: str
    id: str
    detail: str


@dataclass(frozen=True)
class Step:
    """One message a scenario's path takes, and the behaviour (and scenario) its call goes on into."""

    message: str
    next: str
    next_path: str


@dataclass(frozen=True)
class Walked:
    """One message of a behaviour walked into what it calls, at the depth it was reached."""

    message: str
    depth: int


@dataclass(frozen=True)
class Frame:
    """A loop, an alt or an opt of a behaviour's tree: where it stands and what each side holds."""

    id: str
    kind: str
    frame: str
    at: int
    first: str
    else_: str
    two_sides: bool
    scenario: str
    else_scenario: str


@dataclass(frozen=True)
class Tree:
    """A behaviour's tree as written: its messages in order with the frame around each, its frames."""

    sequence: str
    messages: tuple[tuple[str, str], ...]
    frames: tuple[Frame, ...]


class Workspace:
    """A workspace loaded once and kept in the engine (DS-173): asked as often as needed, each answer
    a copy. Several may be loaded at once; each keeps its own model."""

    def __init__(self, source: Source):
        self._native = _native.Workspace(read(source))

    def reload(self, source: Source) -> None:
        """Loaded again from the source given, in place (DS-420): a file whose text has not changed is not parsed
        again. What it answered before stays as it was; what it answers after reads the new files."""
        self._native.reload(read(source))

    def model(self) -> Model:
        """The whole model, typed: what ds export says."""
        return _model(json.loads(self._native.export()))

    def report(self) -> Report:
        """The check: what ds check --json says."""
        return _report(json.loads(self._native.report()))

    def find(self, identity: str) -> Entity | None:
        data = json.loads(self._native.find(identity))
        return _entity(data) if data else None

    def relations_from(self, identity: str) -> list[Relation]:
        return [_relation(r) for r in json.loads(self._native.relations_from(identity))]

    def relations_to(self, identity: str) -> list[Relation]:
        return [_relation(r) for r in json.loads(self._native.relations_to(identity))]

    def touching(self, identity: str) -> list[Relation]:
        return [_relation(r) for r in json.loads(self._native.touching(identity))]

    def members(self, scope: str) -> list[str]:
        return json.loads(self._native.members(scope))

    def parent(self, identity: str) -> str:
        return json.loads(self._native.parent(identity))

    def children(self, identity: str) -> list[str]:
        return json.loads(self._native.children(identity))

    def revision_of(self, artefact: str) -> str:
        return json.loads(self._native.revision_of(artefact))

    def tree(self, sequence: str) -> Tree:
        data = json.loads(self._native.tree(sequence))
        frames = tuple(
            Frame(f["id"], f["kind"], f["frame"], f["at"], f["first"], f["else"], f["two_sides"], f["scenario"], f["else_scenario"])
            for f in data["frames"]
        )
        return Tree(data["sequence"], tuple((m["id"], m["frame"]) for m in data["messages"]), frames)

    def steps(self, sequence: str, path: str = "") -> list[Step]:
        """The messages a scenario's path takes: "" the default path, "*" every branch."""
        return [Step(s["message"], s["next"], s["next_path"]) for s in json.loads(self._native.steps(sequence, path))]

    def walk(self, sequence: str, path: str = "", depth: int = 1) -> list[Walked]:
        return [Walked(w["message"], int(w["depth"])) for w in json.loads(self._native.walk(sequence, path, depth))]

    def ask(self, question: str, subject: str = "") -> list[Answer]:
        """A question of ds query, answered as ds query --json does."""
        return [Answer(a["kind"], a["id"], a["detail"]) for a in json.loads(self._native.ask(question, subject))]

    def shows(self, diagram: str) -> list[Answer]:
        return [Answer(a["kind"], a["id"], a["detail"]) for a in json.loads(self._native.shows(diagram))]

    def render(self, diagram: str, format: str = "text", width: int = 0) -> str:
        """A diagram drawn (svg, text, cells, ir, layout), wrapped to a width; "" when there is no such diagram."""
        return self._native.render(diagram, format, width)

    def page(self, title: str, focus: list[str] | tuple[str, ...] = ()) -> str:
        return self._native.page(title, list(focus))



def load(source: Source) -> Model:
    """A workspace loaded for one look at the whole of it."""
    return Workspace(source).model()


def check(source: Source) -> Report:
    return Workspace(source).report()  # what `ds check --json` prints (DS-120)


def render(source: Source, diagram: str, format: str = "text", width: int = 0) -> str:
    """A diagram drawn as text or svg, wrapped to a width (0: none); "" when there is no such diagram."""
    return _native.render(read(source), diagram, format, width)


def page(source: Source, title: str, focus: list[str] | tuple[str, ...] = ()) -> str:
    """The viewer as one self-contained page, framed on the identities given."""
    return _native.page(read(source), title, list(focus))


# ---- how an identity is written, asked of the engine rather than copied (DS-173) -----------------

qualify = _native.qualify
outer = _native.outer
box_of = _native.box_of
package_of = _native.package_of
behaviour_of = _native.behaviour_of
subject_of = _native.subject_of
scenario_of = _native.scenario_of
without_revision = _native.without_revision


def severities() -> list[dict]:
    """The severity table of the language, as the engine writes it (DS-386): each severity's name, whether a finding
    of it fails (fails), whether it is kept apart as coverage (keptApart), and what an editor shows it as (editor:
    error, warning, information or hint), in the engine's order. A door reads a severity here, never by its name."""
    return json.loads(_native.severities())


def expected_at(text: str, line: int, column: int) -> list[str]:
    """What the grammar takes at a place of a text (0-based line and column)."""
    return json.loads(_native.expected_at(text, line, column))
