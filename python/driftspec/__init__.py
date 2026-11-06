"""DriftSpec for Python: the engine in process, its model as typed objects (DS-84, a spike).

    import driftspec
    model = driftspec.load("design/")            # a directory of .ds files ...
    model = driftspec.load({"a.ds": "package a { }"})  # ... or the texts an editor holds
    report = driftspec.check("design/")
    driftspec.expected_at(text, line, column)     # what the grammar takes there

The native module speaks the JSON of the command line (ds export, ds check --json); this
module only reads it, so a tool sees the same thing whether it calls Python or the binary.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Iterator, Mapping, Union

from . import _native

Source = Union[str, os.PathLike, Mapping[str, str]]

__all__ = ["Location", "Entity", "Relation", "Finding", "Model", "Report", "read", "load", "check", "expected_at", "render", "page"]


@dataclass(frozen=True)
class Location:
    file: str
    line: int
    column: int


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


@dataclass(frozen=True)
class Finding:
    code: str
    severity: str
    message: str
    location: Location


def _location(d: Mapping) -> Location:
    return Location(d.get("file", ""), int(d.get("line", 0)), int(d.get("column", 0)))


def _finding(d: Mapping) -> Finding:
    return Finding(d["code"], d["severity"], d["message"], _location(d.get("location", {})))


@dataclass
class Model:
    """A workspace loaded: every entity by its identity, the relations, the findings, the metamodel."""

    entities: dict[str, Entity]
    relations: list[Relation]
    findings: list[Finding]
    metamodel: dict

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


@dataclass(frozen=True)
class Report:
    findings: list[Finding]
    counts: Mapping[str, int]

    @property
    def errors(self) -> int:
        return int(self.counts.get("errors", 0))


def read(source: Source) -> dict[str, str]:
    """The texts of a workspace: a directory's .ds files, or a mapping given as it is."""
    if isinstance(source, Mapping):
        return {str(k): str(v) for k, v in source.items()}
    root = os.fspath(source)
    if not os.path.isdir(root):
        raise NotADirectoryError(root)
    files: dict[str, str] = {}
    for here, _, names in os.walk(root):
        for name in sorted(names):
            if name.endswith(".ds"):
                path = os.path.join(here, name)
                with open(path, encoding="utf-8") as fh:
                    files[path] = fh.read()
    return files


def load(source: Source) -> Model:
    data = json.loads(_native.export(read(source)))
    entities = {
        e["id"]: Entity(
            e["id"], e["kind"], e.get("name", ""), e.get("scope", ""), _location(e.get("location", {})),
            dict(e.get("fields", {})), tuple(e.get("flags", [])), bool(e.get("expected", False)),
        )
        for e in data["entities"]
    }
    relations = [Relation(r["kind"], r["from"], r["to"], _location(r.get("location", {}))) for r in data["relations"]]
    return Model(entities, relations, [_finding(f) for f in data["findings"]], data.get("metamodel", {}))


def check(source: Source) -> Report:
    data = json.loads(_native.check(read(source)))
    return Report([_finding(f) for f in data["findings"]], data["counts"])


def render(source: Source, diagram: str, format: str = "text", width: int = 0) -> str:
    """A diagram drawn as text or svg, wrapped to a width (0: none); "" when there is no such diagram."""
    return _native.render(read(source), diagram, format, width)


def page(source: Source, title: str, focus: list[str] | tuple[str, ...] = ()) -> str:
    """The viewer as one self-contained page, framed on the identities given."""
    return _native.page(read(source), title, list(focus))


def expected_at(text: str, line: int, column: int) -> list[str]:
    """What the grammar takes at a place of a text (0-based line and column)."""
    return json.loads(_native.expected_at(text, line, column))
