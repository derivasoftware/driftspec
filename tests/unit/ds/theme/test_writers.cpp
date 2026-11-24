#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "ds/theme/Theme.hpp"
#include "ds/theme/Writers.hpp"

namespace {

std::string fixture(const std::string& name) {
  std::ifstream in(std::string(DRIFTSPEC_SOURCE_DIR) + "/tests/unit/ds/theme/fixtures/" + name, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/// The stylesheets written from the theme (DS-393), pinned to the literals the SVG carried before the theme: the fixtures
/// are those literals as they were, so a value the theme holds wrong, or a token out of its place, shows here.
TEST(TestSvgStyleSheet, ItIsTheStyleSheetTheSvgCarried) {
  const ds::theme::Theme theme;
  EXPECT_EQ(ds::theme::SvgStyleSheet().write(theme), fixture("style.css"));
}

TEST(TestSequenceStyleSheet, ItIsTheStyleSheetASequenceCarried) {
  const ds::theme::Theme theme;
  EXPECT_EQ(ds::theme::SequenceStyleSheet().write(theme), fixture("sequence.css"));
}

TEST(TestThemeWriterFactory, ItNamesEveryWriterByTheFormItWrites) {
  const ds::theme::Theme theme;
  const auto writers = ds::theme::ThemeWriterFactory::of();
  EXPECT_EQ(writers.size(), 2u);
  EXPECT_EQ(writers.at("svg")->write(theme), ds::theme::SvgStyleSheet().write(theme));
  EXPECT_EQ(writers.at("sequence")->write(theme), ds::theme::SequenceStyleSheet().write(theme));
}

}  // namespace
