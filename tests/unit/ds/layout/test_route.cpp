#include <gtest/gtest.h>

#include "ds/layout/Route.hpp"

namespace {

/// How an edge runs (DS-392): the corners of its polyline, in order.
TEST(TestRoute, ARouteKeepsItsCornersInOrder) {
  ds::layout::Route r;
  EXPECT_TRUE(r.points.empty());
  r.points = {{0, 0}, {0, 30}, {50, 30}};
  EXPECT_DOUBLE_EQ(r.points.back().first, 50);
  EXPECT_DOUBLE_EQ(r.points[1].second, 30);
}

}  // namespace
