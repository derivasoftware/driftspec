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

/// A route is a polyline with no label of its own unless its router says otherwise (DS-443).
TEST(TestRoute, ARouteIsAPolylineWithNoLabelUnlessItsRouterSays) {
  ds::layout::Route r;
  EXPECT_EQ(r.shape, ds::layout::Route::Shape::polyline);
  EXPECT_FALSE(r.label.has_value());
  r.shape = ds::layout::Route::Shape::curve;
  r.label = std::make_pair(10.0, 20.0);
  EXPECT_DOUBLE_EQ(r.label->second, 20);
}

}  // namespace
