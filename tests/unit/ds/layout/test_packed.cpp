#include <gtest/gtest.h>

#include "ds/layout/Packed.hpp"

namespace {

/// A frame packed (DS-389): until the packer sizes it, it has no size and nothing sits in it.
TEST(TestPacked, APackedFrameHasNoSizeAndNothingInItUntilItIsPacked) {
  ds::layout::Packed p;
  EXPECT_DOUBLE_EQ(p.w + p.h, 0);
  EXPECT_TRUE(p.at.empty());
  EXPECT_TRUE(p.kids.empty());
  p.kids.push_back({120, 40, {}, {}});
  p.at.push_back({10, 20});
  EXPECT_DOUBLE_EQ(p.kids.front().w, 120);
  EXPECT_DOUBLE_EQ(p.at.front().second, 20);
}

}  // namespace
