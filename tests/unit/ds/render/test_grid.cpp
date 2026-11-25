#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "ds/render/Grid.hpp"
#include "ds/render/Layout.hpp"

namespace {

/// The character grid the terminal drawing fills (DS-371): glyphs placed by cell, borders, blanks, lines that join,
/// and the grid written out as text.
TEST(TestGrid, TextTakesACellPerCodePointAndTheDumpDropsTrailingBlanks) {
  ds::render::Grid g(0, 0, 6, 2);
  g.text(1, 0,
         "a\xE2\x86\x92"
         "b");  // a, an arrow of three bytes, b: three cells
  EXPECT_EQ(g.dump(false),
            " a\xE2\x86\x92"
            "b\n\n");
  EXPECT_EQ(g.columns(), 6);
  EXPECT_EQ(g.rows(), 2);
  EXPECT_EQ(g.at(6, 0), nullptr);  // outside the grid: no cell
  EXPECT_NE(g.at(5, 1), nullptr);
}

TEST(TestGrid, AFrameDrawsItsCornersRoundedOrSquareAndClearBlanksWhatItCovers) {
  ds::render::Grid g(0, 0, 3, 3);
  g.frame(ds::render::Rect{0, 0, 3, 3}, true, "");
  EXPECT_EQ(g.dump(false),
            "\xE2\x95\xAD\xE2\x94\x80\xE2\x95\xAE\n\xE2\x94\x82 \xE2\x94\x82\n\xE2\x95\xB0\xE2\x94\x80\xE2\x95\xAF\n");  // ╭─╮ │ │ ╰─╯
  g.clear(ds::render::Rect{0, 0, 3, 3});
  EXPECT_EQ(g.dump(false), "\n\n\n");
}

TEST(TestGrid, TwoEdgesCrossingJoinInOneCross) {
  ds::render::Grid g(0, 0, 1, 1);
  g.line(0, 0, true, false, "");
  EXPECT_EQ(g.dump(false), "\xE2\x94\x80\n");  // ─
  g.line(0, 0, false, false, "");
  EXPECT_EQ(g.dump(false), "\xE2\x94\xBC\n");  // ┼
}

TEST(TestGrid, AGridPastedKeepsItsGlyphsAndColours) {
  ds::render::Grid part(0, 0, 2, 1);
  part.put(0, 0, "x", "31");
  ds::render::Grid all(0, 0, 2, 2);
  all.paste(part, 1);
  EXPECT_EQ(all.dump(false), "\nx\n");
  EXPECT_EQ(all.dump(true), "\n\x1b[31mx\x1b[0m\n");  // the colour is reset where it ends, the trailing blank dropped
  EXPECT_FALSE(all.encode({{"31", 0xFF0000u}}).empty());
}

TEST(TestGrid, AGridTooLargeToHoldAsTextIsRefused) { EXPECT_THROW(ds::render::Grid(0, 0, 100000, 100000), std::length_error); }

}  // namespace
