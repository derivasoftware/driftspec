#include <gtest/gtest.h>

#include "ds/layout/SequenceMessage.hpp"

namespace {

/// A message of a sequence graph (DS-464): to its own lane when it runs from and to the same one.
TEST(TestSequenceMessage, AMessageRunsBetweenTwoLanesOnItsRow) {
  const ds::layout::SequenceMessage call{0, 1, 5, 0};
  EXPECT_NE(call.from, call.to);
  const ds::layout::SequenceMessage self{1, 1, 7, 3};
  EXPECT_EQ(self.from, self.to);
  EXPECT_EQ(self.row, 3);
}

}  // namespace
