#include <gtest/gtest.h>

#include "ds/layout/Frame.hpp"
#include "ds/layout/Framer.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/RouterFactory.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// Lays out the frames of one graph (DS-370): a box for every frame in the order the graph is read, a whole above
/// its part, and the edge between them routed.
TEST(TestFramer, EveryFrameGetsItsBoxAndTheEdgesAreRouted) {
  ds::layout::Frame a, b;
  a.id = "a", a.w = 100, a.h = 40;
  b.id = "b", b.w = 100, b.h = 40;
  ds::layout::Frame root;
  root.kids = {a, b};
  ds::layout::Graph graph;
  graph.root = &root;
  graph.edges = {{1, 2, false}};  // a above b
  const ds::layout::Spacing s;
  const auto placed = ds::layout::Framer(graph, s).run(*ds::layout::RouterFactory::orthogonal(s));
  ASSERT_EQ(placed.boxes.size(), 3u);
  EXPECT_LT(placed.boxes[1].y + placed.boxes[1].h, placed.boxes[2].y);
  ASSERT_EQ(placed.routes.count(0), 1u);
  EXPECT_GE(placed.routes.at(0).points.size(), 2u);
}

}  // namespace
