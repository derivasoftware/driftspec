#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

using ds::render::IrDocument;
using ds::render::IrNode;
using ds::render::Layout;
using ds::render::Metrics;

IrNode box(const std::string& id, const std::string& label, std::vector<std::vector<std::string>> sections = {}) {
  IrNode n;
  n.id = id;
  n.kind = "box";
  n.label = label;
  n.sections = std::move(sections);
  return n;
}

/// The layout of a document in one table of metrics (DS-185).

TEST(TestLayout, ABoxIsAsWideAsItsLongestLineAndAsTallAsItsLines) {
  const Metrics m = Metrics::text(kTheme.measures(ds::theme::Sink::text));
  const Layout layout(m);
  const auto narrow = layout.sizeOf(box("a", "A"));
  const auto wide = layout.sizeOf(box("b", "B", {{"+ aMuchLongerMemberThanTheName()"}}));
  EXPECT_GT(wide.w, narrow.w);
  EXPECT_EQ(wide.h - narrow.h, m.SECTION_GAP + m.LINE_H);  // one section of one line
}

TEST(TestLayout, EveryNodeIsPlacedAndTheSameDocumentPlacesTheSameWay) {
  IrDocument doc;
  doc.nodes = {box("a", "A"), box("b", "B")};
  const Layout layout(Metrics::svg(kTheme.measures(ds::theme::Sink::svg)));
  const auto first = layout.place(doc);
  const auto again = layout.place(doc);
  ASSERT_EQ(first.placed.size(), 2u);
  for (size_t i = 0; i < first.placed.size(); ++i) {
    EXPECT_EQ(first.placed[i].rect.x, again.placed[i].rect.x);
    EXPECT_EQ(first.placed[i].rect.y, again.placed[i].rect.y);
  }
}

}  // namespace
