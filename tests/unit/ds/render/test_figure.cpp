#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "ds/render/Figure.hpp"
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

/// A figure (DS-400): an empty one has no document and places nothing, and a moved one keeps where its nodes land.
TEST(TestFigure, AFigureIsMovedWithWhereItsNodesLand) {
  ds::render::Figure figure;
  EXPECT_TRUE(figure.document.nodes.empty());
  EXPECT_TRUE(figure.layout.placed.empty());
  figure.document = twoBoxes();
  figure.layout.placed.push_back({&figure.document.nodes[1], ds::render::Rect{1, 2, 3, 4}});
  const ds::render::Figure moved = std::move(figure);
  ASSERT_EQ(moved.layout.placed.size(), 1u);
  EXPECT_EQ(moved.layout.placed[0].node, &moved.document.nodes[1]);
  EXPECT_EQ(moved.layout.placed[0].rect.w, 3);
}

}  // namespace
