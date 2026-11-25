#include <gtest/gtest.h>

#include "ds/render/Cell.hpp"

namespace {

/// One character of the terminal drawing (DS-350, DS-371): a blank in the default colour that no edge drew.
TEST(TestCell, ACellStartsBlankInTheDefaultColourAndNoLongGlyphFitsItsSize) {
  const ds::render::Cell c;
  EXPECT_EQ(c.glyph[0], ' ');
  EXPECT_EQ(c.size, 1);
  EXPECT_EQ(c.colour, 0);
  EXPECT_EQ(c.line, 0);
  EXPECT_GT(ds::render::Cell::kLong, c.glyph.size());  // no glyph held inline is as long as the size that says long
}

}  // namespace
