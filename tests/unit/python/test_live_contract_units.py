"""driftspec.live_contract, unit by unit: an operation, its tool and its word, and the contract that lists them."""

from driftspec.live_contract import Contract, Operation, Tool, Word

__covers__ = (Tool, Word, Operation, Contract)


class TestContract:
    def test_a_tool_describes_its_schema_and_leaves_out_required_when_nothing_is(self):
        t = Tool("t", "d", {"a": {"type": "string"}}, ("a",), 0)
        assert t.schema() == {"name": "t", "description": "d", "inputSchema": {"type": "object", "properties": {"a": {"type": "string"}}, "required": ["a"]}}
        assert "required" not in Tool("u", "d", {}, (), 0).schema()["inputSchema"]
        assert Word("w").show({"x": 1}) == {"x": 1} and Operation("GET", "/x", "doc").route() == "GET /x"
        assert Contract((Operation("GET", "/y", "y"),)).describe()["routes"] == {"GET /y": "y"}
