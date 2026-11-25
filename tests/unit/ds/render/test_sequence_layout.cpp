#include <gtest/gtest.h>

#include "ds/render/SequenceLayout.hpp"

namespace {

/// Where a sequence lands (DS-400): empty until it is composed.
TEST(TestSequenceLayout, ALayoutStartsEmpty) {
  const ds::render::SequenceLayout s;
  EXPECT_TRUE(s.lanes.empty());
  EXPECT_TRUE(s.messages.empty());
  EXPECT_TRUE(s.rows.empty());
  EXPECT_TRUE(s.frames.empty());
  EXPECT_EQ(s.width, 0);
  EXPECT_EQ(s.height, 0);
}

}  // namespace
