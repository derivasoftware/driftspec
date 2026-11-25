#include <gtest/gtest.h>

#include "ds/layout/SequenceBlock.hpp"

namespace {

/// A frame of a sequence graph (DS-464): the rows it spans and where its elses come.
TEST(TestSequenceBlock, AFrameSpansItsRowsAndSaysItsElses) {
  const ds::layout::SequenceBlock alt{1, 4, 3, 9, {3}};
  EXPECT_LT(alt.first, alt.last);
  EXPECT_EQ(alt.classifierLength, 3u);
  ASSERT_EQ(alt.dividers.size(), 1u);
  EXPECT_EQ(alt.dividers[0], 3);
}

}  // namespace
