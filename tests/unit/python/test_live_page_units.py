"""driftspec.live_page, unit by unit: the shell a browser opens for a session."""

from driftspec.live_page import SHELL, shell

__covers__ = (shell,)


class TestShell:
    def test_the_shell_names_its_session_and_escapes_what_it_writes(self):
        page = shell("a<b", "/w&s")
        assert "a&lt;b" in page and "/w&amp;s" in page and "__TITLE__" not in page and "__WS__" not in page
        assert page.startswith(SHELL[:15]) and "new EventSource('/events')" in page
