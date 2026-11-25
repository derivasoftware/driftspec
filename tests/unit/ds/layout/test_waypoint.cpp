#include <gtest/gtest.h>

#include "ds/layout/Waypoint.hpp"

namespace {

/// A waypoint is a point and the band of its layer (DS-392).
TEST(TestWaypoint, ItIsEmptyUntilItIsTold) {
  const ds::layout::Waypoint v;
  EXPECT_EQ(v.x + v.top + v.bottom + v.gapBelow, 0);
}

}  // namespace
