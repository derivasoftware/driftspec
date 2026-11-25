#include <gtest/gtest.h>

#include "ds/layout/GridEngine.hpp"
#include "ds/layout/TableSpacing.hpp"

namespace {

ds::layout::TableText words(std::initializer_list<std::size_t> lengths) { return ds::layout::TableText{lengths}; }

/// Each column is as wide as its widest line and its room each side, each row as tall as its tallest cell or its least.
TEST(TestGridEngine, ColumnsAreAsWideAsTheirWidestLineAndRowsAsTallAsTheirTallestCell) {
  ds::layout::TableGraph g;
  g.corner = words({5});
  g.columns = {words({4}), words({6})};
  g.rows = {{words({3}), {words({4}), words({1})}}, {words({6}), {words({9}), words({})}}};
  const ds::layout::TablePlacement p = ds::layout::GridEngine(ds::layout::TableSpacing::of(8, 18)).place(g);
  ASSERT_EQ(p.columnWidth.size(), 3u);
  EXPECT_EQ(p.columnWidth[0], 6 * 8 + 2 * 14);  // the widest of the corner and the labels
  EXPECT_EQ(p.columnWidth[1], 9 * 8 + 2 * 14);
  EXPECT_EQ(p.columnX[2], p.columnWidth[0] + p.columnWidth[1]);
  ASSERT_EQ(p.rowHeight.size(), 2u);
  EXPECT_EQ(p.rowHeight[0], 34);  // one line: the row's least
  EXPECT_EQ(p.rowY[0], 38);       // under the headings
  EXPECT_EQ(p.width, p.columnX[2] + p.columnWidth[2]);
  EXPECT_EQ(p.height, 38 + 34 + 34);
}

/// A text longer than the wrap breaks between words, and its row grows to the lines it takes.
TEST(TestGridEngine, ATextLongerThanTheWrapBreaksBetweenWords) {
  ds::layout::TableGraph g;
  g.columns = {words({4})};
  g.rows = {{words({3}), {words({20, 15, 10})}}};  // 20 + 1 + 15 fits 40; the 10 goes on a second line
  const ds::layout::TablePlacement p = ds::layout::GridEngine(ds::layout::TableSpacing::of(8, 18)).place(g);
  // the corner, the heading, the label, the cell
  ASSERT_EQ(p.breaks.size(), 4u);
  ASSERT_EQ(p.breaks[3].size(), 1u);
  EXPECT_EQ(p.breaks[3][0], 2u);
  EXPECT_EQ(p.columnWidth[1], 36 * 8 + 2 * 14);  // its widest line
  EXPECT_EQ(p.rowHeight[0], 2 * 17 + 17);
}

}  // namespace
