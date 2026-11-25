#include <gtest/gtest.h>

#include "ds/layout/Edge.hpp"

namespace {

/// An edge between two boxes (DS-392), by their numbers in the order the graph is read, telling only whether it points up.
TEST(TestEdge, AnEdgeJoinsTwoBoxesByTheirNumbersAndSaysWhetherItPointsUp) {
  const ds::layout::Edge e{2, 1, true};
  EXPECT_EQ(e.from, 2u);
  EXPECT_EQ(e.to, 1u);
  EXPECT_TRUE(e.upward);  // the base sits above what inherits from it
  const ds::layout::Edge loose;
  EXPECT_EQ(loose.from, ds::layout::Edge::none);  // no box until told
  EXPECT_FALSE(loose.upward);
}

}  // namespace
