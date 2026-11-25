#include <gtest/gtest.h>

#include "ds/render/SequenceFrame.hpp"

namespace {

/// Where a frame lands (DS-400): the messages it spans, its sides and its elses.
TEST(TestSequenceFrame, AFrameKnowsItsSpanItsSidesAndItsElses) {
  ds::render::IrNode alt;
  alt.id = "f0";
  ds::render::SequenceFrame f{&alt, 0, 2, 10, 90, 5, 300, {{1, 50, "else"}}};
  EXPECT_EQ(f.node->id, "f0");
  EXPECT_EQ(f.last - f.first, 2);
  EXPECT_LT(f.left, f.right);
  EXPECT_LT(f.top, f.bottom);
  ASSERT_EQ(f.dividers.size(), 1u);
  EXPECT_EQ(f.dividers[0].row, 1);
}

}  // namespace
