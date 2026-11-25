#include <gtest/gtest.h>

#include "ds/render/TableLayout.hpp"

namespace {

/// Where a table lands, composed (DS-464): nothing until it is composed.
TEST(TestTableLayout, ALayoutStartsEmpty) {
  const ds::render::TableLayout t;
  EXPECT_TRUE(t.columnX.empty());
  EXPECT_TRUE(t.cells.empty());
  EXPECT_EQ(t.height, 0);
}

}  // namespace
