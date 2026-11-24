#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "ds/theme/Theme.hpp"

namespace {

/// The one source of style (DS-393): a token by its name, the font and how it measures on each sink, and the terminal's
/// palette with the colour a host that paints cells gives each code.
TEST(TestTheme, ATokenIsFoundByItsNameWithItsLightAndDarkValues) {
  const ds::theme::Theme theme;
  EXPECT_EQ(theme.token("text").light, "#1e293b");
  EXPECT_EQ(theme.token("text").dark, "#e2e8f0");
  EXPECT_EQ(theme.token("stroke-width").dark, "");  // the same in a dark page
  EXPECT_THROW(theme.token("no-such-token"), std::logic_error);
}

TEST(TestTheme, TheFontMeasuresInPixelsOnAPageAndInCellsInATerminal) {
  const ds::theme::Theme theme;
  EXPECT_EQ(theme.font().sizePx, 12);
  EXPECT_EQ(theme.measures(ds::theme::Sink::svg).advance, 8);
  EXPECT_EQ(theme.measures(ds::theme::Sink::svg).lineHeight, 18);
  EXPECT_EQ(theme.measures(ds::theme::Sink::text).advance, 1);
  EXPECT_EQ(theme.measures(ds::theme::Sink::text).lineHeight, 1);
}

/// The colour a host that paints cells gives each terminal code: the light value of the first role that names it.
TEST(TestTheme, EveryTerminalCodeHasTheColourOfItsFirstRole) {
  const ds::theme::Theme theme;
  const auto& c = theme.cellColours();
  EXPECT_EQ(c.at("2"), 0x64748bu);
  EXPECT_EQ(c.at("34"), 0x2563ebu);
  EXPECT_EQ(c.at("32"), 0x16a34au);
  EXPECT_EQ(c.at("31"), 0xdc2626u);
  EXPECT_EQ(c.at("33"), 0xd97706u);
  EXPECT_EQ(c.at("35"), 0x7c3aedu);
  EXPECT_EQ(c.size(), 6u);
  EXPECT_EQ(theme.sgr(ds::theme::Role::changed), "33");
}

}  // namespace
