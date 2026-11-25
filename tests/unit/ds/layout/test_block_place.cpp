#include <gtest/gtest.h>

#include "ds/layout/BlockPlace.hpp"

namespace {

/// Where a frame lands (DS-464): four sides and its elses.
TEST(TestBlockPlace, AFrameLandsWithItsSidesAndElses) {
  const ds::layout::BlockPlace b{10, 90, 5, 200, {{0, 50}}};
  EXPECT_LT(b.top, b.bottom);
  EXPECT_LT(b.left, b.right);
  ASSERT_EQ(b.dividers.size(), 1u);
  EXPECT_EQ(b.dividers[0].y, 50);
}

}  // namespace
