#include <gtest/gtest.h>

#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"

namespace {

/// What an engine is given to place (DS-392): it refers to the boxes and owns none, and asks for no width it is not
/// given.
TEST(TestGraph, AGraphRefersToItsBoxesAndHoldsNoShelfUntilTold) {
  ds::layout::Graph graph;
  EXPECT_EQ(graph.root, nullptr);
  EXPECT_TRUE(graph.edges.empty());
  EXPECT_DOUBLE_EQ(graph.maxWidth, 0);
  EXPECT_FALSE(graph.following);
  const ds::layout::Frame root;
  graph.root = &root;
  graph.edges.push_back({"a", "b", "inherits"});
  EXPECT_EQ(graph.root, &root);
  EXPECT_EQ(graph.edges.front().kind, "inherits");
}

}  // namespace
