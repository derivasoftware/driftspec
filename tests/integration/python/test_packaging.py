"""REQ-00166-1: the wheel's build makes the ds command and installs it among the environment's scripts."""

import pathlib
import re
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[3]


class TestPackaging:
    def test_the_wheels_build_makes_ds_and_installs_it_as_a_script(self):
        build = tomllib.loads((ROOT / "pyproject.toml").read_text())["tool"]["scikit-build"]["build"]
        assert "ds" in build["targets"]  # built beside the extension
        cmake = (ROOT / "CMakeLists.txt").read_text()
        rule = re.search(r"install\(\s*TARGETS ds\s+RUNTIME DESTINATION \$\{SKBUILD_SCRIPTS_DIR\}\s+COMPONENT (\w+)\)", cmake)
        assert rule, "ds is installed where pip puts a package's commands"
        components = tomllib.loads((ROOT / "pyproject.toml").read_text())["tool"]["scikit-build"]["install"]["components"]
        assert rule.group(1) in components  # in a component the wheel installs
