#include <gtest/gtest.h>

#include "ds/layout/Spacing.hpp"

namespace {

/// The room the layered engine leaves (DS-392): by default the pixels of an SVG.
TEST(TestSpacing, TheDefaultRoomIsThePixelsOfAnSvg) {
  const ds::layout::Spacing s;
  EXPECT_EQ(s.channel, 8);
  EXPECT_EQ(s.gapX, 40);
  EXPECT_EQ(s.gapY, 56);
  EXPECT_EQ(s.lane, 12);
  EXPECT_EQ(s.lanePad, 20);
  EXPECT_EQ(s.containerPad, 18);
  EXPECT_EQ(s.containerLabelH, 22);
  EXPECT_DOUBLE_EQ(s.aspect, 1.6);
}

}  // namespace
