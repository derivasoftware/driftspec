"""driftspec.browser, unit by unit: the browser a door opens a page in, the system stood in for."""

from types import SimpleNamespace

from driftspec.browser import open_in_browser

__covers__ = (open_in_browser,)


class TestBrowser:
    def test_the_browser_is_windows_under_wsl_a_file_through_wslpath_and_the_default_one_elsewhere(self, monkeypatch):
        import shutil
        import subprocess
        import webbrowser

        started = []
        monkeypatch.setattr(subprocess, "Popen", lambda argv, **_: started.append(argv))
        monkeypatch.setattr(subprocess, "run", lambda argv, **_: SimpleNamespace(stdout="C:\\x\\view.html\n"))
        monkeypatch.setattr(webbrowser, "open", lambda target: started.append(["webbrowser", target]))
        monkeypatch.setattr(shutil, "which", lambda name: "/bin/" + name)
        open_in_browser("/x/view.html")
        open_in_browser("http://localhost:1/")
        monkeypatch.setattr(shutil, "which", lambda name: None)
        open_in_browser("/x/view.html")
        assert started == [["explorer.exe", "C:\\x\\view.html"], ["explorer.exe", "http://localhost:1/"], ["webbrowser", "/x/view.html"]]
