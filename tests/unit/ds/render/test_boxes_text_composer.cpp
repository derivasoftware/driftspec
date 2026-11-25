#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "ds/render/BoxesTextComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
// built on first use, not when the executable starts: a test reaches the theme only if it uses it (DS-473)
const ds::theme::Theme& theme() {
  static const ds::theme::Theme built;
  return built;
}

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

/// A box diagram composed for a terminal (DS-400): every box placed in cells, as the layout places it, and its edge
/// routed; the figure moved keeps where its boxes land.
TEST(TestBoxesTextComposer, EveryBoxIsPlacedInCellsAndAMovedFigureKeepsItsPlaces) {
  const ds::render::Metrics m = ds::render::Metrics::text(theme().measures(ds::theme::Sink::text));
  const ds::render::IrDocument doc = twoBoxes();
  ds::render::Figure figure = ds::render::BoxesTextComposer(m).compose(doc, 40);
  ASSERT_EQ(figure.layout.placed.size(), 2u);
  EXPECT_EQ(figure.layout.routes.size(), 1u);
  const ds::render::IrDocument bundled = doc.bundled();  // what the layout's places point into
  const ds::render::LayoutResult direct = ds::render::Layout(m).place(bundled, 40);
  EXPECT_EQ(figure.layout.placed[1].rect.y, direct.placed[1].rect.y);
  const ds::render::Figure moved = std::move(figure);
  EXPECT_EQ(moved.layout.placed[0].node, &moved.document().nodes[0]);
  EXPECT_EQ(moved.layout.placed[0].node->label, "Base");
}

}  // namespace
