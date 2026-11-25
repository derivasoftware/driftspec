#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>

#include "ds/layout/EngineFactory.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/LayeredEngine.hpp"
#include "ds/layout/Placement.hpp"
#include "ds/layout/RouterFactory.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// The boxes of a placement by the identities of their frames, which the engine numbers in the order the graph is read:
/// the root first, then each frame followed by its kids and its ports.
std::map<std::string, ds::layout::Box> byId(const ds::layout::Frame& root, const ds::layout::Placement& placed) {
  std::map<std::string, ds::layout::Box> out;
  std::size_t n = 0;
  std::function<void(const ds::layout::Frame&)> walk = [&](const ds::layout::Frame& f) {
    if (n < placed.boxes.size()) out[f.id] = placed.boxes[n];
    ++n;
    for (const ds::layout::Frame& k : f.kids) walk(k);
    for (const ds::layout::Frame& p : f.ports) walk(p);
  };
  walk(root);
  return out;
}

using ds::layout::Box;
using ds::layout::EngineFactory;
using ds::layout::Frame;
using ds::layout::Graph;
using ds::layout::LayeredEngine;
using ds::layout::Placement;
using ds::layout::RouterFactory;
using ds::layout::Spacing;

/// The engine that layers boxes along their edges (DS-392): bases above what inherits, every box in its container,
/// routes from one box to the other.
class TestLayeredEngine : public ::testing::Test {
 protected:
  static Frame leaf(const std::string& id, double w, double h) {
    Frame f;
    f.id = id;
    f.w = w;
    f.h = h;
    return f;
  }
  static Placement place(const Frame& root, const Graph& edges) {
    Graph graph = edges;
    graph.root = &root;
    return EngineFactory::layered(Spacing{})->place(graph);
  }
};

TEST_F(TestLayeredEngine, ABaseSitsAboveWhatInheritsFromIt) {
  Frame root;
  root.kids = {leaf("heir", 100, 40), leaf("base", 100, 40)};
  Graph g;
  g.edges = {{1, 2, true}};  // heir is box 1, base box 2
  const Placement placed = place(root, g);
  const auto b = byId(root, placed);
  EXPECT_LT(b.at("base").y + b.at("base").h, b.at("heir").y);
  ASSERT_EQ(placed.routes.count(0), 1u);
  EXPECT_GE(placed.routes.at(0).points.size(), 2u);
}

TEST_F(TestLayeredEngine, AWholeSitsAboveItsPartAndAContainerHoldsWhatItContains) {
  Frame box;
  box.id = "box";
  box.arrangement = Frame::Arrangement::Layered;
  box.label = 24;
  box.kids = {leaf("part", 80, 30), leaf("whole", 80, 30)};
  Frame root;
  root.kids = {box};
  Graph g;
  g.edges = {{3, 2, false}};  // box is 1, part 2, whole 3
  const Placement placed = place(root, g);
  const auto boxes = byId(root, placed);
  const Box& c = boxes.at("box");
  for (const char* id : {"part", "whole"}) {
    const Box& b = boxes.at(id);
    EXPECT_GE(b.x, c.x) << id;
    EXPECT_GE(b.y, c.y) << id;
    EXPECT_LE(b.x + b.w, c.x + c.w) << id;
    EXPECT_LE(b.y + b.h, c.y + c.h) << id;
  }
  EXPECT_LT(boxes.at("whole").y, boxes.at("part").y);
}

TEST_F(TestLayeredEngine, EveryPlaceIsAWholeNumberAndAnEmptyGraphPlacesNothing) {
  Frame root;
  root.kids = {leaf("a", 101, 33), leaf("b", 57, 21), leaf("c", 90, 40)};
  Graph g;
  g.edges = {{1, 2, false}, {1, 3, false}};
  const Placement placed = place(root, g);
  for (const Box& b : placed.boxes) {
    EXPECT_DOUBLE_EQ(b.x, static_cast<int>(b.x));
    EXPECT_DOUBLE_EQ(b.y, static_cast<int>(b.y));
  }
  EXPECT_TRUE(LayeredEngine(Spacing{}, RouterFactory::orthogonal(Spacing{})).place(Graph{}).boxes.empty());
}

}  // namespace
