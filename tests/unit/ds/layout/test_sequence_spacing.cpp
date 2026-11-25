#include <gtest/gtest.h>

#include "ds/layout/SequenceSpacing.hpp"

namespace {

/// The distances a sequence is placed with (DS-464): the SVG's 8 px advance, 18 px line and 7 px small text give
/// exactly the pixels it always had; a terminal's cell, one by one, the same proportions in cells.
TEST(TestSequenceSpacing, TheSvgsMeasuresGiveItsPixelsAndACellsTheSameProportions) {
  const ds::layout::SequenceSpacing svg = ds::layout::SequenceSpacing::of(8, 18, 7);
  EXPECT_EQ(svg.laneGap, 24);
  EXPECT_EQ(svg.headPad, 20);
  EXPECT_EQ(svg.crossPad, 32);
  EXPECT_EQ(svg.selfWidth, 32);
  EXPECT_EQ(svg.frameSide, 44);
  EXPECT_EQ(svg.marginX, 20);
  EXPECT_EQ(svg.smallAdvance, 7);
  EXPECT_EQ(svg.marginY, 20);
  EXPECT_EQ(svg.headHeight, 38);
  EXPECT_EQ(svg.rowHeight, 30);
  EXPECT_EQ(svg.selfHeight, 42);
  EXPECT_EQ(svg.frameLead, 30);
  EXPECT_EQ(svg.dividerLead, 26);
  EXPECT_EQ(svg.dividerOffset, 6);
  const ds::layout::SequenceSpacing cell = ds::layout::SequenceSpacing::of(1, 1, 1);
  EXPECT_EQ(cell.laneGap, 3);
  EXPECT_EQ(cell.headHeight, 2);  // 38 px of 18 a line: two lines
  EXPECT_EQ(cell.rowHeight, 2);
  EXPECT_EQ(cell.advance, 1);
}

/// A sink's marks are given at least the room they need, and where the proportion gives more, the proportion stands.
TEST(TestSequenceSpacing, TheMarksGetAtLeastTheRoomTheSinkNeeds) {
  const ds::layout::SequenceSpacing cell = ds::layout::SequenceSpacing::of(1, 1, 1, {4, 3, 3, 1});
  EXPECT_EQ(cell.headPad, 4);     // 2.5 characters would leave a box no space before its border
  EXPECT_EQ(cell.actorPad, 4);    // a terminal boxes an actor too
  EXPECT_EQ(cell.headHeight, 3);  // a border, a name, a border
  EXPECT_EQ(cell.selfHeight, 3);
  EXPECT_EQ(cell.frameGap, 1);
  EXPECT_EQ(cell.rowHeight, 2);  // the proportion where no least is said
  const ds::layout::SequenceSpacing svg = ds::layout::SequenceSpacing::of(8, 18, 7, {4, 3, 3, 1});
  EXPECT_EQ(svg.headHeight, 38);  // the proportion is more than the least: it stands
  EXPECT_EQ(svg.headPad, 20);
}

}  // namespace
