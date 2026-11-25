#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "ds/render/BoxesSvgComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

ds::render::IrNode box(const std::string& id, const std::string& label) {
  ds::render::IrNode n;
  n.id = id;
  n.kind = "box";
  n.label = label;
  return n;
}

/// Two boxes, the second inheriting from the first.
ds::render::IrDocument twoBoxes() {
  ds::render::IrDocument doc;
  doc.nodes = {box("a", "Base"), box("b", "Derived")};
  ds::render::IrEdge e;
  e.from = "b";
  e.to = "a";
  e.kind = "inheritance";
  doc.edges = {e};
  return doc;
}

/// A box diagram composed for SVG (DS-400): every box placed in pixels, as the layout places it, and its edge routed.
TEST(TestBoxesSvgComposer, EveryBoxIsPlacedInPixelsAsTheLayoutPlacesIt) {
  const ds::render::Metrics m = ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg));
  const ds::render::IrDocument doc = twoBoxes();
  const ds::render::Figure figure = ds::render::BoxesSvgComposer(m).compose(doc, 0);
  ASSERT_EQ(figure.layout.placed.size(), 2u);
  EXPECT_EQ(figure.layout.routes.size(), 1u);
  const ds::render::IrDocument bundled = doc.bundled();  // what the layout's places point into
  const ds::render::LayoutResult direct = ds::render::Layout(m).place(bundled, 0, true);
  for (std::size_t i = 0; i < direct.placed.size(); ++i) {
    EXPECT_EQ(figure.layout.placed[i].node->id, direct.placed[i].node->id);
    EXPECT_EQ(figure.layout.placed[i].rect.x, direct.placed[i].rect.x);
    EXPECT_EQ(figure.layout.placed[i].rect.w, direct.placed[i].rect.w);
  }
  // where a box lands points into the figure's own document
  EXPECT_EQ(figure.layout.placed[0].node, &figure.document.nodes[0]);
}

}  // namespace
