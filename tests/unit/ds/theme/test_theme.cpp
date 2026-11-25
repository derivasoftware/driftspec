#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ds/theme/Theme.hpp"

namespace {

/// The one source of style (DS-393): a token by its name, the font and how it measures on each sink, and the terminal's
/// palette with the colour a host that paints cells gives each code.
TEST(TestTheme, ATokenIsFoundByItsNameWithItsLightAndDarkValues) {
  const ds::theme::Theme theme;
  EXPECT_EQ(theme.token("text").light, "#1d1d1d");  // the playground's foreground (DS-490)
  EXPECT_EQ(theme.token("text").dark, "#e9e9e9");
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

/// The colour a host that paints cells gives each terminal code: the light value of the first role that names it, a
/// drawing token, so the cells wear the playground's palette as the drawings do (DS-490).
TEST(TestTheme, EveryTerminalCodeHasTheColourOfItsFirstRole) {
  const ds::theme::Theme theme;
  const auto& c = theme.cellColours();
  const auto rgb = [&theme](const char* token) {
    return static_cast<std::uint32_t>(std::stoul(theme.token(token).light.substr(1), nullptr, 16));
  };
  EXPECT_EQ(c.at("2"), rgb("muted"));
  EXPECT_EQ(c.at("34"), rgb("type"));
  EXPECT_EQ(c.at("32"), rgb("vis-public"));
  EXPECT_EQ(c.at("31"), rgb("vis-private"));
  EXPECT_EQ(c.at("33"), rgb("vis-protected"));
  EXPECT_EQ(c.at("35"), rgb("badge-abstract"));
  EXPECT_EQ(c.at("2"), 0x6a6a6au);  // the playground's muted
  EXPECT_EQ(c.size(), 6u);
  EXPECT_EQ(theme.sgr(ds::theme::Role::changed), "33");
}

}  // namespace

namespace {

/// A drawing looks as the playground draws it (REQ-00332-1, DS-490): each drawing token is the playground's colour its
/// rules gave that token, light and dark, and a container the panel 45% over the background.
TEST(TestDrawnAsThePlayground, EachDrawingTokenIsThePlaygroundsColour) {
  const ds::theme::Theme theme;
  const auto playground = [&theme](const std::string& name) {
    for (const auto& t : theme.page("playground"))
      if (t.name == name) return t;
    throw std::logic_error(name);
  };
  for (const auto& [drawn, from] : std::vector<std::pair<std::string, std::string>>{{"text", "fg"},
                                                                                    {"stroke", "rule"},
                                                                                    {"edge", "rule"},
                                                                                    {"box-fill", "panel"},
                                                                                    {"head-class", "panel"},
                                                                                    {"badge-class", "k-class"},
                                                                                    {"badge-function", "muted"},
                                                                                    {"type", "s-ty"},
                                                                                    {"param", "s-pn"},
                                                                                    {"punct", "muted"},
                                                                                    {"vis-public", "v-pub"},
                                                                                    {"container-stroke", "line"},
                                                                                    {"line-requirement", "c-req"}}) {
    EXPECT_EQ(theme.token(drawn).light, playground(from).light) << drawn;
    EXPECT_EQ(theme.token(drawn).dark, playground(from).dark) << drawn;
  }
  EXPECT_EQ(theme.token("container-fill").light, "#f6f6f6");  // #ffffff 45% over #eeeeee
  EXPECT_EQ(theme.token("container-fill").dark, "#1b1b1b");   // #202020 45% over #171717
}

}  // namespace
