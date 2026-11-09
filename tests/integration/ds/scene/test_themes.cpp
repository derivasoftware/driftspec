#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {

/// A theme redefines the variables the viewer draws with (REQ-00071-2, DS-119): the night theme sets
/// every one of them, and the viewer's note to theme authors names every one.
class TestThemes : public ::testing::Test {
 protected:
  static std::string read(const std::string& path) {
    std::ifstream in(std::string(DRIFTSPEC_SOURCE_DIR) + "/" + path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
  }

  static std::set<std::string> defined(const std::string& css) {  // every --name: set in a block
    std::set<std::string> out;
    const std::regex var("(--[a-z][a-z0-9-]*)\\s*:");
    for (auto it = std::sregex_iterator(css.begin(), css.end(), var); it != std::sregex_iterator(); ++it) out.insert((*it)[1]);
    return out;
  }

  // the viewer's own colours: its first :root block, the light theme every other theme redefines
  std::set<std::string> viewer() const {
    const std::string page = read("viewer/scene.html");
    const auto at = page.find(":root{");
    return defined(page.substr(at, page.find('}', at) - at));
  }
};

TEST_F(TestThemes, TheNightThemeSetsEveryVariableTheViewerDrawsWith) {
  const std::set<std::string> night = defined(read("viewer/themes/night.css"));
  ASSERT_FALSE(viewer().empty());
  for (const auto& v : viewer()) EXPECT_TRUE(night.count(v)) << v << " is not set by the night theme";
}

TEST_F(TestThemes, TheNoteToThemeAuthorsNamesEveryVariable) {
  const std::string page = read("viewer/scene.html");
  const auto at = page.find("The viewer draws with these variables:");
  ASSERT_NE(at, std::string::npos);
  const std::string note = page.substr(at, page.find("*/", at) - at);
  for (const auto& v : viewer()) EXPECT_TRUE(std::regex_search(note, std::regex(v + "(\\s|$)"))) << v << " is not in the note";
}

}  // namespace
