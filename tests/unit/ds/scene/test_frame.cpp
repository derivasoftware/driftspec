#include <gtest/gtest.h>

#include "ds/scene/Frame.hpp"

namespace {

/// A box to pack (DS-389): a frame is fixed until it says how its children are arranged, and asks no room it is not
/// given.
TEST(TestFrame, AFrameIsFixedAndAsksNoRoomUntilItIsTold) {
  const ds::scene::Frame f;
  EXPECT_EQ(f.arrangement, ds::scene::Frame::Arrangement::Fixed);
  EXPECT_TRUE(f.kids.empty());
  EXPECT_TRUE(f.ports.empty());
  EXPECT_DOUBLE_EQ(f.gap + f.left + f.top + f.right + f.bottom + f.portTop + f.portGap + f.drop, 0);
  EXPECT_DOUBLE_EQ(f.factor, 1);
}

}  // namespace
