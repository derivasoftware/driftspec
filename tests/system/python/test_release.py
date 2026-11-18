"""CST-00010-1 from outside: what DriftSpec ships carries the notices of the third-party code in it."""

import pathlib
import re
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[3]
NOTICES = (ROOT / "THIRD_PARTY_NOTICES.md").read_text()


class TestRelease:
    def test_every_dependency_the_build_fetches_has_its_notice_with_the_version_pinned(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        pins = re.findall(r"FetchContent_Declare\(\s*(\w+)\s+(?:GIT_REPOSITORY \S+\s+GIT_TAG v?(\S+)|URL \S+/v?([0-9.]+)/)", cmake)
        assert len(pins) == 5
        names = {"tree_sitter": "tree-sitter", "nlohmann_json": "JSON for Modern C++", "googletest": "GoogleTest", "cli11": "CLI11", "miniz": "miniz"}
        for name, tag, url_version in pins:
            assert f"## {names[name]}" in NOTICES, name  # its licence text
            assert f"| {tag or url_version} |" in NOTICES, name  # the version that is built, in the table

    def test_the_wheel_and_the_site_ship_the_notices(self):
        wheel = tomllib.loads((ROOT / "pyproject.toml").read_text())["tool"]["scikit-build"]["wheel"]
        assert "THIRD_PARTY_NOTICES.md" in wheel["license-files"]
        assert "THIRD_PARTY_NOTICES.md" in (ROOT / "playground" / "build.py").read_text()
