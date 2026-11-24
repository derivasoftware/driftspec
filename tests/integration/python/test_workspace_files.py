"""A workspace's files, by one rule (REQ-00089-2, REQ-00086-1): the facade, the live session's watcher and the
language server read the .ds files of a directory as the engine does (Parser::parseDirectory)."""

import json
import os
import subprocess

import pytest

import driftspec
from driftspec import lsp
from driftspec.live_session import Watcher


@pytest.fixture
def tree(tmp_path):
    """A workspace with every case the rule decides: a dot-folder, a file named only .ds, a link to a file,
    a link to a folder, a broken link."""
    ws, elsewhere = tmp_path / "ws", tmp_path / "elsewhere"
    for d in (ws / "sub", ws / ".hidden", elsewhere):
        d.mkdir(parents=True)
    for path in (ws / "a.ds", ws / "sub" / "b.ds", ws / ".hidden" / "c.ds", elsewhere / "d.ds", ws / ".ds"):
        path.write_text(f"package p_{path.parent.name.strip('.')}_{path.stem.strip('.') or 'dot'} {{\n}}\n")
    (ws / "linkfile.ds").symlink_to(elsewhere / "d.ds")
    (ws / "linkdir").symlink_to(elsewhere, target_is_directory=True)
    (ws / "broken.ds").symlink_to(tmp_path / "nothing.ds")
    return str(ws)


def expected(ws):
    return [os.path.join(ws, p) for p in (".hidden/c.ds", "a.ds", "linkfile.ds", "sub/b.ds")]


class TestWorkspaceFiles:
    def test_the_facade_reads_the_files_the_engine_reads_and_no_other(self, tree):
        assert driftspec.files(tree) == expected(tree)
        assert sorted(driftspec.read(tree)) == expected(tree)  # a broken link is no failure

    def test_the_watcher_stamps_the_same_files_and_sees_a_save_in_a_dot_folder(self, tree):
        w = Watcher(tree)
        assert [p for p, *_ in w.files_stamp()] == expected(tree) and w.changed()
        with open(os.path.join(tree, ".hidden", "c.ds"), "a") as fh:
            fh.write("// a save\n")
        assert w.changed()

    def test_the_language_server_finds_a_folder_s_own_files_by_the_same_rule(self, tree):
        assert driftspec.files(tree, nested=False) == [os.path.join(tree, "a.ds"), os.path.join(tree, "linkfile.ds")]
        assert lsp.holds_ds(tree) and lsp.holds_ds(os.path.join(tree, ".hidden"))
        assert not lsp.holds_ds(os.path.join(tree, "nothing")) and not lsp.holds_ds(os.path.join(tree, "sub", "b.ds"))

    def test_the_engine_reads_the_same_files_when_its_binary_is_at_hand(self, tree):
        ds = os.environ.get("DS")
        if not ds:
            pytest.skip("DS names a ds binary to compare with")
        export = json.loads(subprocess.run([ds, "export", tree], capture_output=True, check=True).stdout)
        assert sorted({e["location"]["file"] for e in export["entities"] if e["kind"] == "package"}) == expected(tree)
