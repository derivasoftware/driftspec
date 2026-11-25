#include <gtest/gtest.h>

#include "ds/layout/Rect.hpp"

namespace {

/// A rectangle of the layered engine is whole (DS-392).
TEST(TestRect, ItIsEmptyUntilItIsTold) {
  const ds::layout::Rect v;
  EXPECT_EQ(v.x + v.y + v.w + v.h, 0);
}

}  // namespace
