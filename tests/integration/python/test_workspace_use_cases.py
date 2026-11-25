"""The Python face through the use cases (REQ-00263-1, DS-459): what a use case describes reaches Python as the
objects the JSON text once parsed back to, with nothing written as text between."""

import json

import pytest

import driftspec
from driftspec import _native

SHOP = """package p {
  module m {
    class Vault {
      + take(n: int) -> int
    }
    class Desk {
      - vault: Vault
      + pay(n: int) -> int fails Broke
    }
  }
}

diagram CL_Shop {
  kind: class
  include: p@m::*
}
"""


def same(a, b) -> bool:
    """Equal, with the same types all the way down (an int is no float) and a record's fields in the same order."""
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return list(a) == list(b) and all(same(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b


@pytest.mark.documents("happy")
def test_each_use_case_reaches_python_as_the_objects_its_json_parsed_back_to() -> None:
    native = _native.Workspace({"shop.ds": SHOP})
    for name, request, text in [
        ("hand-model", {}, native.export()),
        ("check-design", {}, native.report()),
        ("ask-model", {"question": "coverage"}, native.ask("coverage", "")),
        ("ask-model", {"question": "weather"}, native.ask("weather", "")),
    ]:
        outcome, built, message = native.run(name, request)
        assert same(built, json.loads(text)), name
        assert outcome in {"default", "unknown-question"}
        assert message == "" or outcome != "default", name  # a default outcome says nothing more (DS-479)
    # the Python face reads them so: the same model, report and answers as before
    ws = driftspec.Workspace({"shop.ds": SHOP})
    assert ws.model() == driftspec._model(json.loads(native.export()))
    assert ws.report() == driftspec._report(json.loads(native.report()))
    assert ws.ask("weather")[0].kind == "error"  # an error is an answer, as before


def test_a_request_that_is_not_the_use_cases_is_a_value_error_and_an_unwritten_pair_not_implemented() -> None:
    native = _native.Workspace({"shop.ds": SHOP})
    with pytest.raises(ValueError, match="question"):
        native.run("ask-model", {})
    with pytest.raises(NotImplementedError):
        native.run("see-design", {"format": "yaml", "diagram": "CL_Shop"})  # a format see-design does not write
    with pytest.raises(NotImplementedError):
        native.run("teleport", {})
    assert native.run("see-design", {"format": "layout", "diagram": "CL_Nope"}) == ("no-diagram", None, "no diagram named CL_Nope")


def test_an_outcome_that_is_not_the_default_hands_what_it_says() -> None:
    # the message is the outcome's own, as the command line says it (DS-479): here, the manual's unknown topic
    native = _native.Workspace({"shop.ds": SHOP})
    outcome, built, message = native.run("learn-language", {"topic": "weather"})
    assert outcome == "no-topic" and message.startswith("no page 'weather' in the manual; there are:")


def test_a_drawing_reaches_python_as_the_text_ds_render_writes() -> None:
    """see-design hands a drawing and the page to the sink as one verbatim text (DS-474): the Python sink keeps it as the
    str it is, the drawing the workspace's render gives, as ds render writes it."""
    native = _native.Workspace({"shop.ds": SHOP})
    for format in ("svg", "text"):
        for width in (0, 60):
            outcome, drawn, _ = native.run("see-design", {"format": format, "diagram": "CL_Shop", "width": str(width)})
            assert outcome == "default"
            assert type(drawn) is str and drawn == native.render("CL_Shop", format, width), (format, width)
    assert driftspec.Workspace({"shop.ds": SHOP}).render("CL_Shop", "svg").startswith("<svg")
    outcome, page, _ = native.run("see-design", {"format": "html", "title": "shop", "focus": "p@m::Vault\n"})
    assert outcome == "default" and page == native.page("shop", ["p@m::Vault"])
    assert native.run("see-design", {"format": "svg", "diagram": "CL_Nope"}) == ("no-diagram", None, "no diagram named CL_Nope")
