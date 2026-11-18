"""pyds, the old name of the driftspec package (DS-288): import driftspec.

Kept only until the argos plugin that reads DriftSpec imports driftspec; it goes before the first
release. Everything here is driftspec's, under its old name.
"""

import warnings

from driftspec import *  # noqa: F401,F403
from driftspec import __all__, __version__, _native  # noqa: F401

warnings.warn("pyds is now driftspec: import driftspec", DeprecationWarning, stacklevel=2)
