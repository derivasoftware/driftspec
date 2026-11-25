#include <gtest/gtest.h>

#include "ds/render/SequenceDivider.hpp"

namespace {

/// An else of a frame (DS-400): the message it comes before, its line and its condition.
TEST(TestSequenceDivider, ADividerSaysItsRowWhereItLandsAndItsCondition) {
  const ds::render::SequenceDivider d{3, 120, "else"};
  EXPECT_EQ(d.row, 3);
  EXPECT_EQ(d.y, 120);
  EXPECT_EQ(d.label, "else");
}

}  // namespace
