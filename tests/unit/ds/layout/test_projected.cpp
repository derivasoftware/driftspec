#include <gtest/gtest.h>

#include "ds/layout/Projected.hpp"

namespace {

/// An edge projected is between two kids (DS-392).
TEST(TestProjected, ItIsEmptyUntilItIsTold) {
  const ds::layout::Projected v;
  EXPECT_EQ(v.upper + v.lower, 0);
  EXPECT_EQ(v.edge, 0u);
}

}  // namespace
