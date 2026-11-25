#include <gtest/gtest.h>

#include "ds/layout/SequenceSpacing.hpp"
#include "ds/layout/StackedEngine.hpp"

namespace {

ds::layout::StackedEngine svgEngine() { return ds::layout::StackedEngine(ds::layout::SequenceSpacing::of(8, 18, 7)); }

/// Two lanes are as far apart as their heads need; a label wider than the gap they leave pushes them apart.
TEST(TestStackedEngine, ALabelThatDoesNotFitPushesTheLanesApart) {
  ds::layout::SequenceGraph g;
  g.lanes = {{4, false}, {4, false}};
  g.messages = {{0, 1, 2, 0}};
  const ds::layout::SequencePlacement narrow = svgEngine().place(g);
  ASSERT_EQ(narrow.lanes.size(), 2u);
  const int gapNarrow = narrow.lanes[1].centre - narrow.lanes[0].centre;
  EXPECT_EQ(gapNarrow, 26 + 26 + 24);  // half of each 52 px head and the gap between heads
  g.messages[0].labelLength = 30;
  const ds::layout::SequencePlacement wide = svgEngine().place(g);
  EXPECT_EQ(wide.lanes[1].centre - wide.lanes[0].centre, 30 * 8 + 32);  // the label and its room
  ASSERT_EQ(wide.rows.size(), 1u);
  EXPECT_LT(wide.rows[0].top, wide.rows[0].line);
}

/// A label crossing two gaps spreads what it lacks over both, the rest on the last.
TEST(TestStackedEngine, WhatALabelLacksIsSpreadOverTheGapsItCrosses) {
  ds::layout::SequenceGraph g;
  g.lanes = {{4, false}, {4, false}, {4, false}};
  g.messages = {{0, 2, 40, 0}};
  const ds::layout::SequencePlacement p = svgEngine().place(g);
  const int first = p.lanes[1].centre - p.lanes[0].centre, second = p.lanes[2].centre - p.lanes[1].centre;
  EXPECT_EQ(first + second, 40 * 8 + 32);
  EXPECT_LE(second - first, 1);
}

/// A message to its own lane is taller than one between two, and a frame wraps its rows with its else inside.
TEST(TestStackedEngine, ASelfMessageIsTallerAndAFrameWrapsItsRowsAndItsElse) {
  ds::layout::SequenceGraph g;
  g.lanes = {{4, false}, {4, false}};
  g.messages = {{0, 1, 3, 0}, {1, 1, 3, 1}};
  g.blocks = {{0, 1, 3, 5, {1}}};
  const ds::layout::SequencePlacement p = svgEngine().place(g);
  ASSERT_EQ(p.rows.size(), 2u);
  EXPECT_GT(p.rows[1].bottom - p.rows[1].top, p.rows[0].bottom - p.rows[0].top);
  ASSERT_EQ(p.blocks.size(), 1u);
  EXPECT_LT(p.blocks[0].top, p.rows[0].line);
  EXPECT_GT(p.blocks[0].bottom, p.rows[1].line);
  EXPECT_LT(p.blocks[0].left, p.lanes[0].centre);
  ASSERT_EQ(p.blocks[0].dividers.size(), 1u);
  EXPECT_LT(p.blocks[0].dividers[0].y, p.rows[1].line);
  EXPECT_GE(p.width, p.blocks[0].right);
  EXPECT_GT(p.height, p.rows[1].bottom);
}

/// No lane, nothing placed.
TEST(TestStackedEngine, NoLanePlacesNothing) {
  const ds::layout::SequencePlacement p = svgEngine().place(ds::layout::SequenceGraph());
  EXPECT_TRUE(p.lanes.empty());
  EXPECT_EQ(p.width, 0);
}

}  // namespace
