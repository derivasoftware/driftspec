#include <gtest/gtest.h>

#include "ds/layout/TableSpacing.hpp"

namespace {

/// The distances a table is placed with (DS-464): the svg's 8 px and 18 px give exactly its pixels, a cell's the same
/// proportions in cells, and a cell wraps at 40 characters in both.
TEST(TestTableSpacing, TheSvgsMeasuresGiveItsPixelsAndACellsTheSameProportions) {
  const ds::layout::TableSpacing svg = ds::layout::TableSpacing::of(8, 18);
  EXPECT_EQ(svg.padX, 14);
  EXPECT_EQ(svg.headHeight, 38);
  EXPECT_EQ(svg.rowHeight, 34);
  EXPECT_EQ(svg.lineHeight, 17);
  EXPECT_EQ(svg.linePad, 17);
  EXPECT_EQ(svg.wrap, 40u);
  const ds::layout::TableSpacing cell = ds::layout::TableSpacing::of(1, 1);
  EXPECT_EQ(cell.padX, 2);
  EXPECT_EQ(cell.rowHeight, 2);
  EXPECT_EQ(cell.lineHeight, 1);
  EXPECT_EQ(cell.wrap, 40u);
}

}  // namespace
