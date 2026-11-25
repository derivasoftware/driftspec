#include <gtest/gtest.h>

#include "ds/layout/SequenceLeast.hpp"

namespace {

/// The least room a sink's marks need (DS-464): none until a sink says what its marks need.
TEST(TestSequenceLeast, ASinkNeedsNoLeastRoomUntilItSaysSo) {
  const ds::layout::SequenceLeast none;
  EXPECT_EQ(none.headPad, 0);
  EXPECT_EQ(none.headHeight, 0);
  EXPECT_EQ(none.selfHeight, 0);
  EXPECT_EQ(none.frameGap, 0);
  const ds::layout::SequenceLeast terminal{4, 3, 3, 1};
  EXPECT_EQ(terminal.headHeight, 3);
}

}  // namespace
