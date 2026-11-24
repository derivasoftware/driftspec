#include <gtest/gtest.h>

#include <string>

#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/LayeredEngine.hpp"
#include "ds/layout/Placement.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

using ds::layout::Box;
using ds::layout::Frame;
using ds::layout::Graph;
using ds::layout::LayeredEngine;
using ds::layout::Placement;
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
    return LayeredEngine(Spacing{}).place(graph);
  }
};

TEST_F(TestLayeredEngine, ABaseSitsAboveWhatInheritsFromIt) {
  Frame root;
  root.kids = {leaf("heir", 100, 40), leaf("base", 100, 40)};
  Graph g;
  g.edges = {{"heir", "base", true}};
  const Placement placed = place(root, g);
  EXPECT_LT(placed.boxes.at("base").y + placed.boxes.at("base").h, placed.boxes.at("heir").y);
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
  g.edges = {{"whole", "part", false}};
  const Placement placed = place(root, g);
  const Box& c = placed.boxes.at("box");
  for (const char* id : {"part", "whole"}) {
    const Box& b = placed.boxes.at(id);
    EXPECT_GE(b.x, c.x) << id;
    EXPECT_GE(b.y, c.y) << id;
    EXPECT_LE(b.x + b.w, c.x + c.w) << id;
    EXPECT_LE(b.y + b.h, c.y + c.h) << id;
  }
  EXPECT_LT(placed.boxes.at("whole").y, placed.boxes.at("part").y);
}

TEST_F(TestLayeredEngine, EveryPlaceIsAWholeNumberAndAnEmptyGraphPlacesNothing) {
  Frame root;
  root.kids = {leaf("a", 101, 33), leaf("b", 57, 21), leaf("c", 90, 40)};
  Graph g;
  g.edges = {{"a", "b", false}, {"a", "c", false}};
  const Placement placed = place(root, g);
  for (const auto& entry : placed.boxes) {
    EXPECT_DOUBLE_EQ(entry.second.x, static_cast<int>(entry.second.x)) << entry.first;
    EXPECT_DOUBLE_EQ(entry.second.y, static_cast<int>(entry.second.y)) << entry.first;
  }
  EXPECT_TRUE(LayeredEngine(Spacing{}).place(Graph{}).boxes.empty());
}

}  // namespace
