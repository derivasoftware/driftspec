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

/// The stylesheets written from the theme (DS-393), pinned to fixtures, so a value the theme holds wrong, or a token out
/// of its place, shows here. The SVG's moved with DS-490: the playground's look, its colours the playground's palette.
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
  EXPECT_EQ(writers.size(), 7u);
  for (const char* page : {"viewer", "viewer-night", "live", "playground", "design-page"}) EXPECT_TRUE(writers.count(page)) << page;
  EXPECT_EQ(writers.at("svg")->write(theme), ds::theme::SvgStyleSheet().write(theme));
  EXPECT_EQ(writers.at("sequence")->write(theme), ds::theme::SequenceStyleSheet().write(theme));
}

/// A page's writer puts each of its page's tokens where the page reads it (DS-393): every value of the page, light and
/// dark, is in what it writes, and nothing else of the theme is. The pages carry it between their theme markers
/// (tools/theme.py, the ctest theme_pages).
void writesItsPage(const ds::theme::IThemeWriter& writer, const char* page) {
  const ds::theme::Theme theme;
  const std::string written = writer.write(theme);
  for (const auto& t : theme.page(page)) {
    EXPECT_NE(written.find("--" + t.name), std::string::npos) << page << " " << t.name;
    if (!t.light.empty()) {
      EXPECT_NE(written.find(t.light), std::string::npos) << page << " " << t.name;
    }
    if (!t.dark.empty()) {
      EXPECT_NE(written.find(t.dark), std::string::npos) << page << " " << t.name << " dark";
    }
  }
  EXPECT_EQ(written.find("{{"), std::string::npos) << page << ": a placeholder left";
}

TEST(TestViewerVariables, ItWritesTheViewersTokensLightAndDark) { writesItsPage(ds::theme::ViewerVariables(), "viewer"); }

TEST(TestViewerNightVariables, ItWritesTheNightThemesTokens) { writesItsPage(ds::theme::ViewerNightVariables(), "viewer-night"); }

TEST(TestLivePageVariables, ItWritesTheLivePagesTokensLightAndDark) { writesItsPage(ds::theme::LivePageVariables(), "live"); }

TEST(TestPlaygroundVariables, ItWritesThePlaygroundsTokensDarkFirst) {
  writesItsPage(ds::theme::PlaygroundVariables(), "playground");
  const ds::theme::Theme theme;
  const std::string written = ds::theme::PlaygroundVariables().write(theme);
  // dark first: the page's own :root holds the dark values, the light ones come where a page sets data-theme
  EXPECT_LT(written.find(theme.page("playground").front().dark), written.find("data-theme=\"light\""));
}

TEST(TestDesignPageVariables, ItWritesTheDesignPagesTokensLightAndDark) { writesItsPage(ds::theme::DesignPageVariables(), "design-page"); }

}  // namespace

namespace {

/// A drawing's own stylesheet underlines a card's name in its kind's colour, two wide (REQ-00332-1, DS-490).
TEST(TestDrawnAsThePlayground, ACardsNameIsUnderlinedInItsKindsColour) {
  const ds::theme::Theme theme;
  const std::string css = ds::theme::SvgStyleSheet().write(theme);
  EXPECT_NE(css.find(".pr-box .pr-header + .pr-sep { stroke-width: 2; stroke: var(--pr-badge-class); }"), std::string::npos);
  EXPECT_NE(css.find(".pr-classifier-interface .pr-header + .pr-sep { stroke: var(--pr-badge-interface); }"), std::string::npos);
  EXPECT_NE(css.find("--pr-line-requirement: " + theme.token("line-requirement").light), std::string::npos);
  EXPECT_NE(css.find("--pr-box-fill: " + theme.page("playground")[2].light), std::string::npos);  // the panel
}

}  // namespace
