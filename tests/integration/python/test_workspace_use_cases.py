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
        outcome, built = native.run(name, request)
        assert same(built, json.loads(text)), name
        assert outcome in {"default", "unknown-question"}
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
        native.run("see-design", {"format": "svg", "diagram": "CL_Shop"})  # a writer until DS-399
    with pytest.raises(NotImplementedError):
        native.run("teleport", {})
    assert native.run("see-design", {"format": "layout", "diagram": "CL_Nope"}) == ("no-diagram", None)
