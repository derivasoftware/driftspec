#include <gtest/gtest.h>

#include "ds/layout/SequenceLane.hpp"

namespace {

/// A lane of a sequence graph (DS-464): a box head with no label until it is given one.
TEST(TestSequenceLane, ALaneIsABoxHeadWithNoLabelUntilGivenOne) {
  const ds::layout::SequenceLane lane;
  EXPECT_EQ(lane.labelLength, 0u);
  EXPECT_FALSE(lane.actor);
  EXPECT_TRUE((ds::layout::SequenceLane{6, true}).actor);
}

}  // namespace
