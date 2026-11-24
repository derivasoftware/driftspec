#include <gtest/gtest.h>

#include "ds/layout/Edge.hpp"

namespace {

/// An edge between two boxes (DS-392), by their identities, telling only whether it points up.
TEST(TestEdge, AnEdgeJoinsTwoBoxesByTheirIdentitiesAndSaysWhetherItPointsUp) {
  const ds::layout::Edge e{"p@m::Heir", "p@m::Base", true};
  EXPECT_EQ(e.from, "p@m::Heir");
  EXPECT_EQ(e.to, "p@m::Base");
  EXPECT_TRUE(e.upward);  // the base sits above what inherits from it
  EXPECT_FALSE(ds::layout::Edge{}.upward);
}

}  // namespace
