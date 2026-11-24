"""driftspec-live: a design seen live while it is written.

    python -m driftspec.live serve <workspace> [--port 4747] [--open]   the viewer in the browser, following the files
    python -m driftspec.live focus <identity>                            take the reader to an identity
    python -m driftspec.live note <identity> <text> [--tone t]           a bubble on an identity (empty text removes it)
    python -m driftspec.live highlight <identity>... [--tone t] [--add]  light identities of any perspective
    python -m driftspec.live clear [notes|highlights|all]
    python -m driftspec.live selection                                   what the reader has selected
    python -m driftspec.live state                                       version, the findings
    python -m driftspec.live mcp [--port 4747]                           the same, as MCP tools over stdio

<workspace> is the design's folder (e.g. design/); the session reads and watches every .ds file below it.
The design stays text: whoever edits it edits the .ds files. The session watches them, loads the
model on every change and pushes the new state to every page
that follows it. Focus, notes, highlights and the reader's selection go both ways, so the one who
writes and the one who reads point at the same identities.

The contract (CONTRACT, in live_session) is what a client relies on, whether a browser page or an editor's
panel: the routes it calls, the events it receives and the shape of the state. Notes and highlights
are kept in <workspace>/.driftspec/live.json, beside the design and never in it.
"""

# The session is in four modules beside this one: live_session (the session and its contract), live_page
# (the page around the viewer), live_http (the routes and the server) and live_mcp (the client side and
# the MCP tools). This one is the command line, and it keeps every name a caller imports from driftspec.live.

from __future__ import annotations

import json
import sys

from driftspec.live_contract import LIVE, Operation
from driftspec.live_http import browser_open, handler_for, serve
from driftspec.live_mcp import TOOLS, call, mcp, tool
from driftspec.live_page import SHELL
from driftspec.live_session import CONTRACT, PORT, STATE, Live

__all__ = ["CONTRACT", "PORT", "SHELL", "STATE", "TOOLS", "Live", "browser_open", "call", "handler_for", "main", "mcp",
           "serve", "tool"]


class Command:
    """One command line: the command, its words, and the options it takes out of them."""

    def __init__(self, argv: list[str]):
        args = list(argv)
        self.port: int = int(self.option(args, "--port", PORT))
        self.tone: str = self.option(args, "--tone", "accent")
        self.add: bool = "--add" in args
        self.args: list[str] = [a for a in args if a != "--add"]
        self.name: str = self.args[0] if self.args else "serve"

    @staticmethod
    def option(args: list[str], flag: str, default):
        """The value after a flag, taken out of the words with the flag; the default when it is not there."""
        if flag not in args:
            return default
        k = args.index(flag)
        value = args[k + 1]
        del args[k:k + 2]
        return value

    def word(self, k: int, default: str) -> str:
        return self.args[k] if len(self.args) > k else default

    def ask(self, op: Operation) -> None:
        """The session's answer to an operation of the contract, printed as JSON."""
        word = op.word
        print(json.dumps(word.show(call(self.port, op.path, word.body(self))), indent=word.indent))


def asking(op: Operation):
    """The command of an operation: its answer asked of the session and printed."""
    return lambda c: c.ask(op)


# What each command does with its command line: serving and mcp are this door's own; every other
# command is an operation of the contract, read from it.
COMMANDS = {
    "serve": lambda c: serve(c.word(1, "."), c.port, open_browser="--open" in c.args),
    **{op.word.word: asking(op) for op in LIVE.words()},
    "mcp": lambda c: mcp(c.port),
}


def main(argv: list[str] | None = None) -> None:
    command = Command(sys.argv[1:] if argv is None else argv)
    if command.name in COMMANDS:
        COMMANDS[command.name](command)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
