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
class TestThemeWriters : public ::testing::Test {
 protected:
  const ds::theme::Theme theme;
};

TEST_F(TestThemeWriters, TheSvgStyleSheetIsTheOneTheSvgCarried) {
  EXPECT_EQ(ds::theme::SvgStyleSheet().write(theme), fixture("style.css"));
}

TEST_F(TestThemeWriters, TheSequenceStyleSheetIsTheOneTheSvgCarried) {
  EXPECT_EQ(ds::theme::SequenceStyleSheet().write(theme), fixture("sequence.css"));
}

TEST_F(TestThemeWriters, TheFactoryNamesEveryWriter) {
  const auto writers = ds::theme::ThemeWriterFactory::of();
  EXPECT_EQ(writers.size(), 2u);
  EXPECT_EQ(writers.at("svg")->write(theme), ds::theme::SvgStyleSheet().write(theme));
  EXPECT_EQ(writers.at("sequence")->write(theme), ds::theme::SequenceStyleSheet().write(theme));
}

/// The colour a host that paints cells gives each terminal code: the light value of the first role that names it.
TEST_F(TestThemeWriters, EveryTerminalCodeHasTheColourOfItsFirstRole) {
  const auto& c = theme.cellColours();
  EXPECT_EQ(c.at("2"), 0x64748bu);
  EXPECT_EQ(c.at("34"), 0x2563ebu);
  EXPECT_EQ(c.at("32"), 0x16a34au);
  EXPECT_EQ(c.at("31"), 0xdc2626u);
  EXPECT_EQ(c.at("33"), 0xd97706u);
  EXPECT_EQ(c.at("35"), 0x7c3aedu);
  EXPECT_EQ(c.size(), 6u);
  EXPECT_EQ(theme.measures(ds::theme::Sink::svg).advance, 8);
  EXPECT_EQ(theme.measures(ds::theme::Sink::text).lineHeight, 1);
}

}  // namespace
