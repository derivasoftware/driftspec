"""The browser this machine has, for the doors that open a page in it: the language server's view and the live
session (DS-422). On WSL, the Windows browser, reached through explorer.exe with a local file's path turned into a
Windows one; elsewhere, the default browser."""

from __future__ import annotations

import shutil
import subprocess
import webbrowser


def open_in_browser(target: str) -> None:
    """Open a URL or a local file in the browser."""
    if shutil.which("explorer.exe"):  # WSL: the Windows browser reaches localhost and, through wslpath, a local file
        if "://" not in target and shutil.which("wslpath"):  # a local file, not a URL
            target = subprocess.run(["wslpath", "-w", target], capture_output=True, text=True).stdout.strip()
        subprocess.Popen(["explorer.exe", target], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        webbrowser.open(target)
