#include <gtest/gtest.h>

#include "ds/layout/Point.hpp"

namespace {

/// A point of the layered engine is whole (DS-392).
TEST(TestPoint, ItIsEmptyUntilItIsTold) {
  const ds::layout::Point v;
  EXPECT_EQ(v.x + v.y, 0);
}

}  // namespace
