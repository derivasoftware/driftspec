#include <gtest/gtest.h>

#include <string>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/theme/Theme.hpp"
#include "ds/theme/Writers.hpp"

namespace {

/// The renderers read their style from the theme handed to them (DS-393): the stylesheets an SVG carries are what the
/// theme's writers write, the terminal colours each role with the theme's code, and a box grows by the theme's advance.
class TestThemeDrawings : public ::testing::Test {
 protected:
  const ds::theme::Theme theme;

  static ds::render::IrDocument document(const std::string& json) { return ds::render::IrDocument::parse(json); }
};

TEST_F(TestThemeDrawings, ADrawingCarriesTheStyleSheetItsThemeWrites) {
  const std::string svg = ds::render::Svg(theme).render(document(
      R"J({"ir": 1, "title": "t", "nodes": [{"id": "A", "kind": "box", "label": "A"}, {"id": "B", "kind": "box", "label": "B"}],
          "edges": [{"from": "A", "to": "B", "kind": "dependency"}]})J"));
  EXPECT_NE(svg.find("<style>" + ds::theme::SvgStyleSheet().write(theme) + "</style>"), std::string::npos);
}

TEST_F(TestThemeDrawings, ASequenceCarriesWhatItsThemeAddsForSequences) {
  const std::string svg = ds::render::Svg(theme).render(document(
      R"J({"ir": 1, "title": "s", "nodes": [{"id": "a", "kind": "lifeline", "label": "a"}, {"id": "b", "kind": "lifeline", "label": "b"}],
          "edges": [{"from": "a", "to": "b", "kind": "message", "label": "run()", "order": 0}]})J"));
  EXPECT_NE(svg.find(ds::theme::SvgStyleSheet().write(theme) + ds::theme::SequenceStyleSheet().write(theme)), std::string::npos);
}

TEST_F(TestThemeDrawings, TheTerminalDrawingColoursEachRoleAsItsThemeSays) {
  const std::string drawn = ds::render::Text(theme).render(
      document(R"J({"ir": 1, "title": "t", "nodes": [{"id": "A", "kind": "box", "label": "A"}, {"id": "B", "kind": "box", "label": "B"}],
                   "edges": [{"from": "A", "to": "B", "kind": "dependency", "label": "uses", "refs": {"change": "changed"}}]})J"),
      true);
  EXPECT_NE(drawn.find("\x1b[" + theme.sgr(ds::theme::Role::changed) + "m"), std::string::npos);
}

TEST_F(TestThemeDrawings, ABoxGrowsByTheThemesAdvanceForEachCharacter) {
  for (const auto sink : {ds::theme::Sink::svg, ds::theme::Sink::text}) {
    const ds::theme::Measures m = theme.measures(sink);
    const ds::render::Layout layout(sink == ds::theme::Sink::svg ? ds::render::Metrics::svg(m) : ds::render::Metrics::text(m));
    const auto box = [](const std::string& label) {
      ds::render::IrNode n;
      n.id = label;
      n.kind = "box";
      n.label = label;
      return n;
    };
    EXPECT_EQ(layout.sizeOf(box("abcdefghijklmnopqrstuvwxyz")).w - layout.sizeOf(box("abcdefghijklmnopqrstuvwxy")).w, m.advance);
  }
}

}  // namespace
