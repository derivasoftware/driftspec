#include <gtest/gtest.h>

#include "ds/layout/DividerPlace.hpp"

namespace {

/// Where an else lands (DS-464): which of its frame's it is, and its line.
TEST(TestDividerPlace, AnElseSaysWhichItIsAndWhereItsLineGoes) {
  const ds::layout::DividerPlace d{1, 150};
  EXPECT_EQ(d.index, 1u);
  EXPECT_EQ(d.y, 150);
}

}  // namespace
