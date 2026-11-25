"""What a line is written in, whatever its kind (REQ-00264-1, DS-427): the editor reads a kind's category from the
metamodel and keeps no table of kinds of its own."""

import os
from types import SimpleNamespace

import pytest

from driftspec.lsp import DesignServer, path_of, uri_of

KINDS = """package q {
  module m {
    class V <<*>> {
      operator *(k: float) -> V
    }
    class Order {
      - open: bool = true
      + close() -> void
    }
    state Order {
      Open if open
      Closed if not open
      Open -> Closed on close
    }
    rule every-class-holds-something {
      title: a class holds something
      for class c in q@m
      holds m in c
    }
  }
  usecase UC-00001-1 {
    title: Close an order
    outcome default: it is closed
  }
}
"""


class Doc:
    def __init__(self, source: str):
        self.source = source
        self.lines = source.splitlines(True)


class Client:
    """What the editor reads of pygls' LanguageServer: the root, the buffers (read from the disk), where diagnostics go."""

    def __init__(self, root: str):
        self.workspace = SimpleNamespace(root_path=root, text_documents={}, get_text_document=self.document)

    def document(self, uri: str) -> Doc:
        if uri not in self.workspace.text_documents:
            with open(path_of(uri), encoding="utf-8") as fh:
                self.workspace.text_documents[uri] = Doc(fh.read())
        return self.workspace.text_documents[uri]

    def text_document_publish_diagnostics(self, params) -> None:
        pass


@pytest.mark.documents("happy")
def test_a_line_finds_what_it_is_written_in_whatever_its_kind(tmp_path) -> None:
    design = os.path.join(os.path.realpath(tmp_path), "design")
    os.makedirs(design)
    path = os.path.join(design, "q.ds")
    with open(path, "w") as fh:
        fh.write(KINDS)
    editor = DesignServer().editor_of(Client(os.path.realpath(tmp_path)), uri_of(path))
    found = {line + 1: editor.enclosing(path, line).id for line in (3, 10, 15, 21, 22)}
    assert found == {
        4: "q@m::V::operator*",  # an operator, as a method is
        11: "q@m::Order::state",  # a state line: the machine
        16: "q@m::every-class-holds-something",  # a rule line: the rule
        22: "UC-00001-1",  # the use case's own line
        23: "UC-00001-1::default",  # an outcome: itself
    }
    assert editor.typed().category_of("state") == "logic" and editor.typed().category_of("outcome") == "artefact"
