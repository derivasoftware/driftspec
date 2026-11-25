#include <gtest/gtest.h>

#include "ds/layout/OrthogonalRouter.hpp"
#include "ds/layout/Routing.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// Routes orthogonal through the layers (DS-185, DS-392): an edge from the bottom of one box to the top of the one
/// below, and a box's relation with itself out by its right side and back.
TEST(TestOrthogonalRouter, AnEdgeGoesDownBetweenTheBoxesAndALoopGoesOutOnTheRight) {
  ds::layout::Routing r;
  r.boxes = {{0, 0, 100, 40}, {0, 100, 100, 40}};
  r.ends = {{0, 1}, {0, 0}};
  const auto routes = ds::layout::OrthogonalRouter(ds::layout::Spacing{}).route(r);
  ASSERT_EQ(routes.count(0), 1u);
  const auto& down = routes.at(0).points;
  ASSERT_EQ(down.size(), 2u);
  EXPECT_DOUBLE_EQ(down.front().first, 50);
  EXPECT_DOUBLE_EQ(down.front().second, 40);  // the bottom of the upper box
  EXPECT_DOUBLE_EQ(down.back().second, 100);  // the top of the lower one
  ASSERT_EQ(routes.count(1), 1u);
  const auto& loop = routes.at(1).points;
  ASSERT_EQ(loop.size(), 4u);
  EXPECT_DOUBLE_EQ(loop[0].first, 100);  // out of the right side
  EXPECT_DOUBLE_EQ(loop[1].first, 136);  // three lanes of 12 out
  EXPECT_DOUBLE_EQ(loop[0].second, 10);  // a quarter down the box
  EXPECT_DOUBLE_EQ(loop[2].second, 30);  // and back a quarter from its bottom
}

}  // namespace
