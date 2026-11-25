#include <gtest/gtest.h>

#include "ds/layout/TablePlacement.hpp"

namespace {

/// Where a table lands (DS-464): nothing until it is placed.
TEST(TestTablePlacement, APlacementStartsEmpty) {
  const ds::layout::TablePlacement p;
  EXPECT_TRUE(p.columnX.empty());
  EXPECT_TRUE(p.rowY.empty());
  EXPECT_TRUE(p.breaks.empty());
  EXPECT_EQ(p.width, 0);
}

}  // namespace
