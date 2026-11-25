#include <gtest/gtest.h>

#include "ds/layout/SequencePlacement.hpp"

namespace {

/// Where a sequence lands (DS-464): nothing until it is placed.
TEST(TestSequencePlacement, APlacementStartsEmpty) {
  const ds::layout::SequencePlacement p;
  EXPECT_TRUE(p.lanes.empty());
  EXPECT_TRUE(p.rows.empty());
  EXPECT_TRUE(p.blocks.empty());
  EXPECT_EQ(p.width, 0);
  EXPECT_EQ(p.height, 0);
}

}  // namespace
