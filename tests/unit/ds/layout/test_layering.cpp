#include <gtest/gtest.h>

#include "ds/layout/Layering.hpp"

namespace {

/// An empty layering holds no node and no layer (DS-392).
TEST(TestLayering, ItIsEmptyUntilItIsTold) {
  const ds::layout::Layering v;
  EXPECT_EQ(v.real + v.depth + v.total + v.minX, 0);
  EXPECT_TRUE(v.rows.empty() && v.chain.empty() && v.x.empty());
}

}  // namespace
